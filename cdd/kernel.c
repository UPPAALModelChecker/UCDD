// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-
///////////////////////////////////////////////////////////////////////////////
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 2011 - 2018, Aalborg University.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: kernel.c,v 1.10 2004/11/12 16:34:52 didier Exp $
//
///////////////////////////////////////////////////////////////////////////////

#include "hash/compute.h"
#include "cdd/kernel.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>

#ifdef __MINGW32__
#ifndef WIN32
#define WIN32
#endif
#endif

#ifdef WIN32
#include <windows.h>
#endif

#if defined(__APPLE__) && defined(__MACH__)
#define ARCH_APPLE_DARWIN
#endif

#define JIT_GBC

#define HASH_DENSITY  4  /**< Max. density of hash table. */
#define THRESHOLD     5  /**< Free nodes in percent for when to GBC. */
#define MINFREE       20 /**< Minimum free nodes in percent. */
#define SIZEOF_INT    4  /**< Size of integer in bytes. */
#define SIZEOF_VOID_P 4  /**< Size of void pointer in bytes. */

#if defined(WIN32)
#define CHUNKSIZE 0x10000 /* Size of chunk in bytes */
#elif defined(ARCH_APPLE_DARWIN)
#include <mach/vm_map.h>
#include <mach/mach_init.h>
#define CHUNKSIZE 0x1000 /* Size of chunk in bytes */
#else
#include <malloc.h>
#define CHUNKSIZE 0x10000 /* Size of chunk in bytes */
#endif

/** Returns a chunk in which \a node is allocated. */
#define cdd_node2chunk(node) ((Chunk*)((uintptr_t)(node) & ~(CHUNKSIZE - 1)))

/**
 * The terminal node. Since we can negate nodes by toggling a single
 * bit on the pointer to the node, we only need one terminal (the true
 * node).
 */
static ddNode cdd_terminal;

#ifdef MULTI_TERMINAL
static ddNode** extra_terminals = NULL;
static int32_t nb_extra_terminals = 0;
#endif

/*** KERNEL VARIABLES ***********************************************/
NodeManager* bddmanager;  /**< BDD Node manager. */
NodeManager** cddmanager; /**< Array of CDD Node managers. */
int32_t cdd_levelcnt;     /**< Number of levels allocated. */
int32_t cdd_gbcclock;     /**< Acc. time used for garbage collection. */
int32_t cdd_gbccnt;       /**< Number of times we have run GBC. */
int32_t cdd_rehashclock;  /**< Acc. time used for rehashing. */
int32_t cdd_rehashcnt;    /**< Number of times we have rehashed. */
int32_t cdd_maxcddsize;   /**< Max. arity of a node. */
int32_t cdd_maxcddused;   /**< Max. nodes the library may allocate. */
int32_t cdd_chunkcnt;     /**< Total number of chunks allocated. */
int32_t cdd_clocknum;     /**< Number of clocks allocated. */
int32_t cdd_varnum;       /**< Number of BDD variables allocated. */
LevelInfo* cdd_levelinfo;
int32_t* cdd_diff2level;
int32_t cdd_running;   /**< True if library has been initialised. */
int32_t cdd_errorcond; /**< Last error code. */
ddNode* cddfalse;      /**< True terminal. */
ddNode* cddtrue;       /**< False terminal (negated true). */

Elem* cdd_refstack;      /**< Base address of stack. */
Elem* cdd_refstacktop;   /**< Top of stack. */
size_t cdd_refstacksize; /**< Size of stack. */

/*** STATIC KERNEL VARIABLES ***************************************/
static void (*pregbc_handler)(void);               /**< Pre-gbc handler */
static void (*postgbc_handler)(CddGbcStat*);       /**< Post-gbc handler */
static void (*prerehash_handler)(void);            /**< Pre-rehash handler */
static void (*postrehash_handler)(CddRehashStat*); /**< Post-rehash handler */

/** Allocate a new subtable. */
static SubTable* cdd_alloc_subtable(NodeManager*, int);

/** Deallocate a subtable. */
static void cdd_dealloc_subtable(SubTable*);

/** Allocate a node manager. */
static NodeManager* cdd_alloc_nodemanager(int, NodeHashFunc);

/** Deallocate a node manager. */
static void cdd_dealloc_nodemanager(NodeManager*);

/** Garbage collect a node manager */
static void cdd_gbc_nodemanager(NodeManager*);

/** Allocate a chunk. */
static void cdd_alloc_chunk(NodeManager*);

/** Dealloate a chunk. */
static ddNode* cdd_alloc_node(NodeManager*);

/** Rehash a subtable, doubling the size of it. */
static void cdd_rehash(NodeManager*, SubTable*);

/**
 * @name Hash functions
 * @{
 */

#define DD_P1 12582917L   /**< Prime */
#define DD_P2 4256249L    /**< Prime */
#define DD_P3 741457L     /**< Prime */
#define DD_P4 1618033999L /**< Prime */

/**
 * Hash function used to pair two DD nodes.
 */
#define bddHash(f, g) ((uint32_t)(((uint32_t)(f)*DD_P1 + (uint32_t)(g)) * DD_P2))

/**
 * Hash function over an array of \a len Elem elements.
 */
#define cddHash(elem, len) (hash_computeU32((uint32_t*)(elem), (len) * (sizeof(Elem) >> 2), (len)))

static uint32_t cdd_hash_func(NodeManager*, ddNode*);
static uint32_t bdd_hash_func(NodeManager*, ddNode*);

/** @} */

int32_t cdd_error(int32_t e)
{
    fprintf(stderr, "CDD Error: %d\n", e);
    return e;
}

int32_t cdd_init(int32_t maxsize, int32_t cs, size_t stacksize)
{
    int32_t err;

    if (cdd_running) {
        return cdd_error(CDD_RUNNING);
    }

    cdd_terminal.next = NULL;
    cdd_terminal.ref = MAXREF;
    cdd_terminal.level = MAXLEVEL;
    cdd_terminal.flag = 0;
    cddfalse = &cdd_terminal;
    cddtrue = cdd_neg(cddfalse);

    cdd_maxcddsize = maxsize;
    cdd_maxcddused = 0;
    cdd_levelcnt = cdd_chunkcnt = 0;
    cdd_gbcclock = 0;
    cdd_gbccnt = 0;
    postgbc_handler = NULL;
    pregbc_handler = NULL;
    prerehash_handler = NULL;
    postrehash_handler = NULL;
    cdd_refstack = NULL;
    cddmanager = NULL;
    bddmanager = NULL;
    cdd_levelinfo = NULL;
    cdd_diff2level = NULL;
    cdd_clocknum = 0;
    cdd_varnum = 0;
    cdd_postgbc_hook(cdd_default_gbhandler);
    cdd_postrehash_hook(cdd_default_rehashhandler);

    if ((err = cdd_operator_init(cs)) < 0) {
        cdd_done();
        return err;
    }

    cdd_refstacksize = stacksize;
    cdd_refstack = cdd_refstacktop = (Elem*)malloc(sizeof(Elem) * stacksize);
    cddmanager = (NodeManager**)calloc(maxsize + 1, sizeof(NodeManager*));
    bddmanager = cdd_alloc_nodemanager(sizeof(bddNode), bdd_hash_func);

    if (cdd_refstack == NULL || cddmanager == NULL || bddmanager == NULL) {
        cdd_done();
        return cdd_error(CDD_MEMORY);
    }

    cdd_running = 1;

    return 0;
}

void cdd_ensure_running()
{
    if (!cdd_running) {
        cdd_init(64, 10000, 10000);
        cdd_pregbc_hook(NULL);
        cdd_postgbc_hook(NULL);
        cdd_prerehash_hook(NULL);
        cdd_postrehash_hook(NULL);
    }
}

#ifdef MULTI_TERMINAL

void cdd_add_tautologies(int32_t n)
{
    int32_t oldn = nb_extra_terminals;
    int32_t newn = oldn + n;
    int32_t i;
    assert(n >= 0);
    nb_extra_terminals = newn;
    extra_terminals = (ddNode**)realloc(extra_terminals, newn * sizeof(ddNode*));
    // FIXME: NULL.

    for (i = oldn; i < newn; ++i) {
        xtermNode* node = (xtermNode*)malloc(sizeof(xtermNode));
        // FIXME: NULL.
        node->next = NULL;
        node->ref = MAXREF;
        node->level = MAXLEVEL;
        node->flag = 0;
        node->id = i;
        extra_terminals[i] = (ddNode*)node;
    }
}

ddNode* cdd_apply_tautology(ddNode* node, int32_t t_id)
{
    assert(t_id >= 0 && t_id < nb_extra_terminals);
    return cdd_apply(node, extra_terminals[t_id], cddop_and);
}

int32_t cdd_isterminal(ddNode* node) { return cdd_rglr(node)->level == MAXLEVEL; }

int32_t cdd_is_extra_terminal(ddNode* node)
{
    return cdd_rglr(node) != cddfalse && cdd_isterminal(node);
}

int32_t cdd_get_tautology_id(ddNode* node)
{
    assert(cdd_is_extra_terminal(node));
    return ((xtermNode*)cdd_rglr(node))->id;
}

int32_t cdd_eval_true(ddNode* node)
{
    return ((node) == cddtrue || (cdd_is_extra_terminal(node) && !cdd_mask(node)));
}

int32_t cdd_eval_false(ddNode* node)
{
    return ((node) == cddfalse || (cdd_is_extra_terminal(node) && cdd_mask(node)));
}

int32_t cdd_get_number_of_tautologies() { return nb_extra_terminals; }

#endif

int32_t cdd_isrunning() { return cdd_running; }

void cdd_done()
{
    int32_t i;

    if (!cdd_running) {
        return;
    }

    cdd_operator_done();
    cdd_dealloc_nodemanager(bddmanager);
    for (i = 0; i <= cdd_maxcddsize; i++) {
        cdd_dealloc_nodemanager(cddmanager[i]);
    }
    free(cddmanager);
    free(cdd_refstack);
    free(cdd_levelinfo);
    free(cdd_diff2level);
#ifdef MULTI_TERMINAL
    for (i = 0; i < nb_extra_terminals; ++i) {
        free(extra_terminals[i]);
    }
    nb_extra_terminals = 0;
    free(extra_terminals);
    extra_terminals = NULL;
#endif /* MULTI_TERMINAL */
    cdd_running = 0;
}

static Chunk* cdd_allocate_chunk_from_os()
{
#if defined(WIN32)
    return (Chunk*)VirtualAlloc(0, CHUNKSIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#elif defined(ARCH_APPLE_DARWIN)
    vm_address_t address;
    vm_allocate(mach_task_self(), &address, CHUNKSIZE, true);
    return (Chunk*)address;
#else
    return (Chunk*)memalign(CHUNKSIZE, CHUNKSIZE);
#endif
}

static void cdd_deallocate_chunk_to_os(Chunk* chunk)
{
#if defined(WIN32)
    VirtualFree(chunk, CHUNKSIZE, MEM_DECOMMIT | MEM_RELEASE);
#elif defined(ARCH_APPLE_DARWIN)
    vm_deallocate(mach_task_self(), (vm_address_t)chunk, CHUNKSIZE);
#else
    free(chunk);
#endif
}

static SubTable* cdd_alloc_subtable(NodeManager* man, int32_t level)
{
    int32_t i;
    SubTable* tbl = (SubTable*)malloc(sizeof(SubTable));
    tbl->level = level;
    tbl->deadcnt = 0;
    tbl->shift = SIZEOF_INT * 8 - 8;
    tbl->buckets = 256;
    tbl->keys = 0;
    tbl->maxkeys = tbl->buckets * HASH_DENSITY;
    tbl->hash = (ddNode**)malloc(tbl->buckets * sizeof(ddNode*));
    for (i = 0; i < tbl->buckets; i++) {
        tbl->hash[i] = man->sentinel;
    }
    man->subtables[level] = tbl;
    return tbl;
}

static void cdd_dealloc_subtable(SubTable* tbl)
{
    if (tbl) {
        free(tbl->hash);
        free(tbl);
    }
}

static NodeManager* cdd_alloc_nodemanager(int32_t size, NodeHashFunc hashfunc)
{
    NodeManager* man;

    man = (NodeManager*)malloc(sizeof(NodeManager));
    man->nodesize = size;
    man->freecnt = 0;
    man->chunkcnt = 0;
    man->alloccnt = 0;
    man->usedcnt = 0;
    man->deadcnt = 0;
    man->gbccnt = 0;
    man->gbcclock = 0;
    man->free = NULL;
    man->nodes = NULL;
    man->hashfunc = hashfunc;
    man->subtables = calloc(cdd_levelcnt, sizeof(SubTable*));
    man->sentinel = cdd_alloc_node(man);
    memset(man->sentinel, 0, size);

    return man;
}

static void cdd_dealloc_nodemanager(NodeManager* man)
{
    int32_t i;
    Chunk *p, *q;

    if (man) {
        /* Free sub tables */
        for (i = 0; i < cdd_levelcnt; i++) {
            cdd_dealloc_subtable(man->subtables[i]);
        }

        /* Free chunks */
        p = man->nodes;
        while (p) {
            q = p->next;
            cdd_deallocate_chunk_to_os(p);
            p = q;
        }

        /* Free node manager */
        free(man->subtables);
        free(man);
    }
}

static void cdd_alloc_chunk(NodeManager* man)
{
    ddNode** p;
    int32_t size = man->nodesize / sizeof(ddNode*);
    int32_t nodes = (CHUNKSIZE - sizeof(Chunk)) / man->nodesize;
    Chunk* chunk = cdd_allocate_chunk_from_os();

    // Add node to chunk chain
    chunk->man = man;
    chunk->next = man->nodes;
    man->nodes = chunk;

    // Init nodes
    p = chunk->nodes + (nodes - 1) * size;
    *p = man->free;
    man->free = (ddNode*)(chunk->nodes);
    for (p -= size; p >= chunk->nodes; p -= size) {
        *p = (ddNode*)(p + size);
    }

    // Update counters
    man->freecnt += nodes;
    man->chunkcnt++;
    man->alloccnt += nodes;
    cdd_chunkcnt++;
}

static uint32_t cdd_hash_func(NodeManager* man, ddNode* node)
{
    return cddHash(cdd_node(node)->elem, (man->nodesize - sizeof(cddNode)) / sizeof(Elem));
}

static uint32_t bdd_hash_func(NodeManager* man, ddNode* node)
{
    return bddHash(bdd_node(node)->low, bdd_node(node)->high);
}

void cdd_rec_deref(ddNode* node)
{
    cdd_iterator it;
    ddNode** top = (ddNode**)cdd_refstacktop;
    *(top++) = cdd_rglr(node);

    do {
        node = cdd_rglr(*(--top));
        if (node->ref == 0) {
            cdd_error(CDD_DEREF);
            return;
        }
        cdd_deref(node);
        if (node->ref == 0) {
            cdd_node2chunk(node)->man->usedcnt--;
            cdd_node2chunk(node)->man->deadcnt++;
            cdd_node2chunk(node)->man->subtables[node->level]->deadcnt++;
            switch (cdd_info(node)->type) {
            case TYPE_BDD:
                *(top++) = bdd_node(node)->low;
                *(top++) = bdd_node(node)->high;
                break;
            case TYPE_CDD:
                cdd_it_init(it, node);
                while (!cdd_it_atend(it)) {
                    *(top++) = cdd_it_child(it);
                    cdd_it_next(it);
                }
            }
        }
    } while (top > (ddNode**)cdd_refstacktop);
}

void cdd_reclaim(ddNode* node)
{
    cdd_iterator it;
    ddNode** top = (ddNode**)cdd_refstacktop;
    *(top++) = cdd_rglr(node);

    do {
        node = cdd_rglr(*(--top));
        cdd_node2chunk(node)->man->usedcnt++;
        cdd_node2chunk(node)->man->deadcnt--;
        cdd_node2chunk(node)->man->subtables[node->level]->deadcnt--;
        switch (cdd_info(node)->type) {
        case TYPE_CDD:
            cdd_it_init(it, node);
            while (!cdd_it_atend(it)) {
                if (cdd_rglr(cdd_it_child(it))->ref == 0)
                    *(top++) = cdd_it_child(it);
                cdd_ref(cdd_it_child(it));
                cdd_it_next(it);
            }
            break;
        case TYPE_BDD:
            if (cdd_rglr(bdd_node(node)->low)->ref == 0) {
                *(top++) = bdd_node(node)->low;
            }
            if (cdd_rglr(bdd_node(node)->high)->ref == 0) {
                *(top++) = bdd_node(node)->high;
            }
            cdd_ref(bdd_node(node)->low);
            cdd_ref(bdd_node(node)->high);
        }
    } while (top > (ddNode**)cdd_refstacktop);
}

static void cdd_gbc_nodemanager(NodeManager* man)
{
    SubTable* tbl;
    ddNode *node, *next, **p;
    int64_t clk = clock();
    int32_t i;
    int32_t j;

    if (pregbc_handler != NULL) {
        pregbc_handler();
    }

    for (i = 0; i < cdd_levelcnt; i++) {
        tbl = man->subtables[i];
        if (tbl == NULL || tbl->deadcnt == 0) {
            continue;
        }
        for (j = 0; j < tbl->buckets; j++) {
            p = &(tbl->hash[j]);
            node = *p;
            while (node != man->sentinel) {
                next = node->next;
                if (node->ref == 0) {
                    node->next = man->free;
                    man->free = node;
                } else {
                    *p = node;
                    p = &node->next;
                }
                node = next;
            }
            *p = man->sentinel;
        }
        tbl->keys -= tbl->deadcnt;
        tbl->deadcnt = 0;
    }

    clk = clock() - clk;

    man->freecnt += man->deadcnt;
    man->deadcnt = 0;
    man->gbccnt++;
    man->gbcclock += clk;

    cdd_gbcclock += clk;
    cdd_gbccnt++;

    if (postgbc_handler != NULL) {
        CddGbcStat s;
        s.nodes = man->alloccnt;
        s.freenodes = man->freecnt;
        s.time = clk;
        s.sumtime = cdd_gbcclock;
        s.num = cdd_gbccnt;
        postgbc_handler(&s);
    }
}

void cdd_gbc()
{
    int32_t i;
    int64_t clk = clock();

    cdd_operator_flush();

    cdd_gbcclock += clock() - clk;

    // Check BDD manager
    if (THRESHOLD * bddmanager->alloccnt >= 100 * bddmanager->freecnt &&
        MINFREE * bddmanager->alloccnt < 100 * bddmanager->deadcnt) {
        cdd_gbc_nodemanager(bddmanager);
    }

    // Check CDD managers
    for (i = 2; i <= cdd_maxcddused; i++) {
        if (cddmanager[i] && THRESHOLD * cddmanager[i]->alloccnt >= 100 * cddmanager[i]->freecnt &&
            MINFREE * cddmanager[i]->alloccnt < 100 * cddmanager[i]->deadcnt) {
            cdd_gbc_nodemanager(cddmanager[i]);
        }
    }
}

static ddNode* cdd_alloc_node(NodeManager* man)
{
    ddNode* node;

    // Free nodes left?
    if (man->free == NULL) {
        if (MINFREE * man->alloccnt < 100 * man->deadcnt) {
#ifdef JIT_GBC
            cdd_operator_flush();
            cdd_gbc_nodemanager(man);
#else
            cdd_gbc();
#endif
        } else {
            cdd_alloc_chunk(man);
        }
    }

    // Get node from free list
    node = man->free;
    man->free = node->next;

    // Update counters
    man->usedcnt++;
    man->freecnt--;

    return node;
}

ddNode* cdd_make_bdd_node(int32_t level, ddNode* low, ddNode* high)
{
    bddNode* node;
    bddNode** p;
    int32_t bucket, cnt, mask;
    SubTable* tbl;

    // Eliminate redundant nodes
    if (low == high) {
        return low;
    }

    // Normalise
    mask = cdd_mask(low);
    low = cdd_rglr(low);
    high = cdd_neg_cond(high, mask);

    // Find sub table
    tbl = bddmanager->subtables[level];
    if (tbl == NULL) {
        tbl = cdd_alloc_subtable(bddmanager, level);
    }

    // Look for existing node
    bucket = bddHash(low, high) >> tbl->shift;
    p = (bddNode**)&(tbl->hash[bucket]);
    while (low < (*p)->low) {
        p = (bddNode**)&((*p)->next);
    }
    while (low == (*p)->low && high < (*p)->high) {
        p = (bddNode**)&((*p)->next);
    }
    if (low == (*p)->low && high == (*p)->high) {
        if ((*p)->ref == 0) {
            cdd_reclaim((ddNode*)*p);
        }
        return cdd_neg_cond((ddNode*)*p, mask);
    }

    // Increment references
    cdd_ref(low);
    cdd_ref(high);

    // Create new node
    cnt = cdd_gbccnt;
    node = (bddNode*)cdd_alloc_node(bddmanager);

    // If garbage collection has occured we need to recalc node pos
    if (cnt != cdd_gbccnt) {
        p = (bddNode**)&(tbl->hash[bucket]);
        while (low < (*p)->low) {
            p = (bddNode**)&((*p)->next);
        }
        while (low == (*p)->low && high < (*p)->high) {
            p = (bddNode**)&((*p)->next);
        }
    }

    // Add node to hash chain
    node->next = (ddNode*)*p;
    *p = node;

    // Initialise node
    node->ref = 0;
    node->level = level;
    node->low = low;
    node->high = high;

    // Check whether max keys has been reached
    tbl->keys++;
    if (tbl->keys > tbl->maxkeys) {
        cdd_rehash(bddmanager, tbl);
    }

    return cdd_neg_cond((ddNode*)node, mask);
}

ddNode* cdd_make_cdd_node(int32_t level, Elem* elem, int32_t len)
{
    SubTable* tbl;
    NodeManager* man;
    int32_t bucket, i, size;
    cddNode* node;
    cddNode** p;

    if (len > cdd_maxcddsize) {
        cdd_error(CDD_MAXSIZE);
        return NULL;
    }

    // Eliminate redundant nodes
    if (len == 1) {
        return elem[0].child;
    }

    // Find manager and subtable
    man = cddmanager[len];
    if (man == NULL) {
        size = sizeof(cddNode) + sizeof(Elem) * len;
        man = cddmanager[len] = cdd_alloc_nodemanager(size, cdd_hash_func);
        if (len > cdd_maxcddused) {
            cdd_maxcddused = len;
        }
    }
    tbl = man->subtables[level];
    if (tbl == NULL) {
        tbl = cdd_alloc_subtable(man, level);
    }

    // Look for existing node
    bucket = cddHash(elem, len) >> tbl->shift;
    p = (cddNode**)&(tbl->hash[bucket]);
    while ((i = memcmp(elem, (*p)->elem, len * sizeof(Elem))) < 0) {
        p = (cddNode**)&((*p)->next);
    }
    if (i == 0) {
        if ((*p)->ref == 0) {
            cdd_reclaim((ddNode*)*p);
        }
        return (ddNode*)*p;
    }

    // Increment references
    for (i = 0; i < len; i++) {
        cdd_ref(elem[i].child);
    }

    // Alloc node
    i = cdd_gbccnt;
    node = (cddNode*)cdd_alloc_node(man);

    // If garbage collection has occured we need to recalc the node pos
    if (i != cdd_gbccnt) {
        p = (cddNode**)&(tbl->hash[bucket]);
        while (memcmp(elem, (*p)->elem, len * sizeof(Elem)) < 0) {
            p = (cddNode**)&((*p)->next);
        }
    }

    // Add node to hash chain
    node->next = (ddNode*)*p;
    *p = node;

    // Initialise node
    node->level = level;
    node->ref = 0;
    memcpy(node->elem, elem, sizeof(Elem) * len);

    // Check whether max keys has been reached
    tbl->keys++;
    if (tbl->keys > tbl->maxkeys) {
        cdd_rehash(man, tbl);
    }

    return (ddNode*)node;
}

void cdd_pregbc_hook(void (*func)(void)) { pregbc_handler = func; }

void cdd_postgbc_hook(void (*func)(CddGbcStat*)) { postgbc_handler = func; }

void cdd_prerehash_hook(void (*func)(void)) { prerehash_handler = func; }

void cdd_postrehash_hook(void (*func)(CddRehashStat*)) { postrehash_handler = func; }

const char* cdd_versionstr()
{
    static char str[100];
    sprintf(str, "GB CDD package release %d.%d", CDD_VERSION / 10, CDD_VERSION % 10);
    return str;
}

int32_t cdd_versionnum() { return CDD_VERSION; }

void cdd_default_gbhandler(CddGbcStat* s)
{
    fprintf(stderr, "Garbage collection #%d: %d nodes / %d free", s->num, s->nodes, s->freenodes);
    fprintf(stderr, " / %.1fs / %.1fs total\n", ((double)s->time) / CLOCKS_PER_SEC,
            ((double)s->sumtime) / CLOCKS_PER_SEC);
}

void cdd_default_rehashhandler(CddRehashStat* s)
{
    fprintf(stderr, "Rehash #%d: level %d / %d buckets / %d keys / %d max / %.1fs / %.1fs total\n",
            s->num, s->level, s->buckets, s->keys, s->max, ((double)s->time) / CLOCKS_PER_SEC,
            ((double)s->sumtime) / CLOCKS_PER_SEC);
}

/* Doubles the size of the hash table */
static void cdd_rehash(NodeManager* man, SubTable* tbl)
{
    int32_t i, bucket;
    int32_t oldsize;
    ddNode **p, **q, *node, **oldhash;
    int64_t clk = clock();

    oldsize = tbl->buckets;
    oldhash = tbl->hash;
    tbl->buckets <<= 1;
    tbl->maxkeys <<= 1;
    tbl->shift -= 1;
    tbl->hash = malloc(tbl->buckets * sizeof(ddNode*));

    for (i = 0; i < oldsize; i++) {
        p = &(tbl->hash[i << 1]);
        q = p + 1;
        for (node = oldhash[i]; node != man->sentinel; node = node->next) {
            bucket = man->hashfunc(man, node) >> tbl->shift;
            if (bucket & 0x1) {
                *q = node;
                q = &(node->next);
            } else {
                *p = node;
                p = &(node->next);
            }
        }
        *p = *q = man->sentinel;
    }

    free(oldhash);

    clk = clock() - clk;
    cdd_rehashclock += clk;
    cdd_rehashcnt++;

    if (postgbc_handler != NULL) {
        CddRehashStat s;
        s.level = tbl->level;
        s.buckets = tbl->buckets;
        s.keys = tbl->keys;
        s.max = tbl->maxkeys;
        s.time = clk;
        s.sumtime = cdd_rehashclock;
        s.num = cdd_rehashcnt;
        postrehash_handler(&s);
    }
}

ddNode* cdd_bddvar(int32_t level) { return cdd_make_bdd_node(level, cddfalse, cddtrue); }

ddNode* cdd_interval_from_level(int32_t level, raw_t low, raw_t high)
{
    Elem* top = cdd_refstacktop;
    if (low > -INF) {
        cdd_push(cddfalse, low);
        cdd_push(cddtrue, high);
        if (high < INF)
            cdd_push(cddfalse, INF);
        cdd_refstacktop = top;
        return cdd_make_cdd_node(level, top, 2 + (high < INF));
    } else {
        cdd_push(cddfalse, high);
        cdd_push(cddtrue, INF);
        cdd_refstacktop = top;
        return cdd_neg(cdd_make_cdd_node(level, top, 2));
    }
}

ddNode* cdd_upper_from_level(int32_t level, raw_t bnd)
{
    Elem* top = cdd_refstacktop;
    if (bnd == INF) {
        return cddtrue;
    } else if (bnd == -INF) {
        return cddfalse;
    }
    cdd_push(cddfalse, bnd);
    cdd_push(cddtrue, INF);
    cdd_refstacktop = top;
    return cdd_neg(cdd_make_cdd_node(level, top, 2));
}

ddNode* cdd_interval(int32_t i, int32_t j, raw_t low, raw_t high)
{
    return (i > j) ? cdd_interval_from_level(cdd_diff2level[cdd_difference(i, j)], low, high)
                   : cdd_interval_from_level(cdd_diff2level[cdd_difference(j, i)], bnd_u2l(high),
                                             bnd_l2u(low));
}

ddNode* cdd_upper(int32_t i, int32_t j, raw_t bnd)
{
    if (i > j) {
        return cdd_upper_from_level(cdd_diff2level[cdd_difference(i, j)], bnd);
    } else {
        return cdd_neg(cdd_upper_from_level(cdd_diff2level[cdd_difference(j, i)], bnd_u2l(bnd)));
    }
}

void cdd_mark(ddNode* node)
{
    cdd_iterator it;

    if (cdd_isterminal(node) || cdd_ismarked(node)) {
        return;
    }

    cdd_setmark(node);
    switch (cdd_info(node)->type) {
    case TYPE_CDD:
        for (cdd_it_init(it, node); !cdd_it_atend(it); cdd_it_next(it))
            cdd_mark(cdd_it_child(it));
        break;
    case TYPE_BDD: cdd_mark(bdd_node(node)->low); cdd_mark(bdd_node(node)->high);
    }
}

void cdd_markcount(ddNode* node, int32_t* cnt)
{
    cdd_iterator it;

    if (cdd_isterminal(node) || cdd_ismarked(node)) {
        return;
    }

    (*cnt)++;
    cdd_setmark(node);
    switch (cdd_info(node)->type) {
    case TYPE_CDD:
        for (cdd_it_init(it, node); !cdd_it_atend(it); cdd_it_next(it))
            cdd_markcount(cdd_it_child(it), cnt);
        break;
    case TYPE_BDD:
        cdd_markcount(bdd_node(node)->low, cnt);
        cdd_markcount(bdd_node(node)->high, cnt);
    }
}

void cdd_markedgecount(ddNode* node, int32_t* cnt)
{
    cdd_iterator it;

    if (cdd_isterminal(node) || cdd_ismarked(node)) {
        return;
    }

    cdd_setmark(node);
    switch (cdd_info(node)->type) {
    case TYPE_CDD:
        for (cdd_it_init(it, node); !cdd_it_atend(it); cdd_it_next(it)) {
            (*cnt)++;
            cdd_markedgecount(cdd_it_child(it), cnt);
        }
        break;
    case TYPE_BDD:
        (*cnt) += 2;
        cdd_markedgecount(bdd_node(node)->low, cnt);
        cdd_markedgecount(bdd_node(node)->high, cnt);
    }
}

void cdd_unmark(ddNode* node)
{
    cdd_iterator it;

    if (cdd_is_tfterminal(node) || !cdd_ismarked(node))
        return;

    cdd_resetmark(node);

#ifdef MULTI_TERMINAL
    /* For some reason extra terminals can be marked. */
    if (cdd_is_extra_terminal(node))
        return;
#endif

    switch (cdd_info(node)->type) {
    case TYPE_CDD:
        for (cdd_it_init(it, node); !cdd_it_atend(it); cdd_it_next(it))
            cdd_unmark(cdd_it_child(it));
        break;
    case TYPE_BDD: cdd_unmark(bdd_node(node)->low); cdd_unmark(bdd_node(node)->high);
    }
}

/* See API for why we have this one too.
 */
void cdd_force_unmark(ddNode* node)
{
    cdd_iterator it;

    if (cdd_is_tfterminal(node))
        return;

    cdd_resetmark(node);

#ifdef MULTI_TERMINAL
    /* For some reason extra terminals can be marked. */
    if (cdd_is_extra_terminal(node))
        return;
#endif

    switch (cdd_info(node)->type) {
    case TYPE_CDD:
        for (cdd_it_init(it, node); !cdd_it_atend(it); cdd_it_next(it))
            cdd_force_unmark(cdd_it_child(it));
        break;
    case TYPE_BDD: cdd_force_unmark(bdd_node(node)->low); cdd_force_unmark(bdd_node(node)->high);
    }
}

/**
 * Adds \a n addition to \a man. This will reallocate the
 * array of subtables of \a man.
 * @param man a node manager
 * @param n new number of additional levels to add
 */
static void add_levels_to_nodemanager(NodeManager* man, int32_t n)
{
    int32_t i;
    man->subtables = realloc(man->subtables, (cdd_levelcnt + n) * sizeof(SubTable*));
    for (i = cdd_levelcnt; i < cdd_levelcnt + n; i++) {
        man->subtables[i] = NULL;
    }
}

/**
 * Updates node managers to handle \a n additional levels (compared
 * to the current value of \c cdd_levelcnt).
 * @param n the number of levels to add
 */
static void add_levels(int32_t n)
{
    int32_t i;
    add_levels_to_nodemanager(bddmanager, n);
    for (i = 0; i < cdd_maxcddsize; i++) {
        if (cddmanager[i]) {
            add_levels_to_nodemanager(cddmanager[i], n);
        }
    }
}

void cdd_add_clocks(int32_t n)
{
    int32_t i;
    int32_t j;
    int32_t diffs;  // Number of new levels
    LevelInfo* info;

    diffs = cdd_difference_count(cdd_clocknum + n) - cdd_difference_count(cdd_clocknum);
    add_levels(diffs);
    cdd_diff2level =
        realloc(cdd_diff2level, cdd_difference_count(cdd_clocknum + n) * sizeof(LevelInfo*));
    cdd_levelinfo = realloc(cdd_levelinfo, (cdd_levelcnt + diffs) * sizeof(LevelInfo));

    info = cdd_levelinfo + cdd_levelcnt;
    for (i = cdd_clocknum; i < cdd_clocknum + n; i++) {
        for (j = 0; j < i; j++) {
            info->type = TYPE_CDD;
            info->clock1 = i;
            info->clock2 = j;
            info->diff = cdd_difference(i, j);
            cdd_diff2level[info->diff] = cdd_levelcnt;
            info++;
            cdd_levelcnt++;
        }
    }
    cdd_clocknum += n;
}

int32_t cdd_getclocks() { return cdd_clocknum; }

int32_t cdd_add_bddvar(int32_t n)
{
    int32_t offset = cdd_levelcnt;
    LevelInfo* info;

    add_levels(n);
    cdd_levelcnt += n;
    cdd_varnum += n;
    cdd_levelinfo = realloc(cdd_levelinfo, cdd_levelcnt * sizeof(LevelInfo));
    if (cdd_levelinfo == NULL) {
        return cdd_error(CDD_MEMORY);
    }

    info = cdd_levelinfo + cdd_levelcnt - n;
    while (n) {
        info->type = TYPE_BDD;
        info++;
        n--;
    }
    return offset;
}

const LevelInfo* cdd_get_levelinfo(int32_t level) { return cdd_levelinfo + level; }

int32_t cdd_get_level_count() { return cdd_levelcnt; }

int32_t cdd_get_bdd_level_count() { return cdd_varnum; }

void cdd_dump_nodes()
{
    SubTable* tbl;
    ddNode *node, **p;
    int32_t i, j, k;

    fprintf(stdout, "\"%p\" [true]\n", cddfalse);

    for (i = 0; i < cdd_levelcnt; i++) {
        tbl = bddmanager->subtables[i];
        if (tbl) {
            for (j = 0; j < tbl->buckets; j++) {
                p = &(tbl->hash[j]);
                node = *p;
                while (node != bddmanager->sentinel) {
                    if (node->ref != 0) {
                        fprintf(stdout, "\"%p\" [level %d]\n", node, node->level);
                    }
                    node = node->next;
                }
            }
        }

        for (k = 0; k < cdd_maxcddsize; k++) {
            if (cddmanager[k]) {
                tbl = cddmanager[k]->subtables[i];
                if (tbl) {
                    for (j = 0; j < tbl->buckets; j++) {
                        p = &(tbl->hash[j]);
                        node = *p;
                        while (node != cddmanager[k]->sentinel) {
                            if (node->ref != 0) {
                                fprintf(stdout, "\"%p\" [level %d : %d-%d]\n", node, node->level,
                                        cdd_info(node)->clock1, cdd_info(node)->clock2);
                            }
                            node = node->next;
                        }
                    }
                }
            }
        }
    }
}
