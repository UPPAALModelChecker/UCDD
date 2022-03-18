// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-
///////////////////////////////////////////////////////////////////////////////
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: cddop.c,v 1.21 2005/05/14 16:57:35 behrmann Exp $
//
///////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "base/intutils.h"
#include "base/bitstring.h"
#include "dbm/dbm.h"
#include "dbm/mingraph.h"
#include "dbm/print.h"
#include "debug/malloc.h"

#include "cdd/kernel.h"
#include "cache.h"
#include "bellmanford.h"
#include "tarjan.h"

#define RELAXCACHE

#ifdef RELAXCACHE
#include "relax.h"
#endif

#define EX

#define P1 12582917
#define P2 4256249

#define COMPLHASH(r, op) (cdd_pair((uintptr_t)(r), (op)))
//#define APPLYHASH(l,r,op)    (cdd_triple((unsigned int)(l), (unsigned int)(r),(op)))
#define APPLYHASH(l, r, op) ((((uintptr_t)(op) + (uintptr_t)(l)) * P1 + (uintptr_t)(r)) * P2)
#define EXISTHASH(l)        ((uintptr_t)(l))
#define REPLACEHASH(r)      ((uintptr_t)r)

#ifdef RELAXCACHE
#define RELAXHASH(n, l, c1, c2, u) \
    (cdd_triple((uintptr_t)(node), cdd_pair((l), (c1)), cdd_pair((c2), (u))))
#endif

// #define cdd_and(l,r) cdd_apply_reduce((l), (r), cddop_and)
#define cdd_and(l, r) cdd_apply((l), (r), cddop_and)
#define cdd_xor(l, r) cdd_apply((l), (r), cddop_xor)
#define cdd_or(l, r)  cdd_neg(cdd_and(cdd_neg(l), cdd_neg(r)))

inline static int32_t maximum(int32_t a, int32_t b) __attribute__((const));
inline static int32_t minimum(int32_t a, int32_t b) __attribute__((const));

inline static int32_t maximum(int32_t a, int32_t b) { return a > b ? a : b; }

inline static int32_t minimum(int32_t a, int32_t b) { return a < b ? a : b; }

#ifdef MULTI_TERMINAL
#define IS_TRUE(node)  cdd_eval_true(node)
#define IS_FALSE(node) cdd_eval_false(node)
#else
#define IS_TRUE(node)  ((node) == cddtrue)
#define IS_FALSE(node) ((node) == cddfalse)
#endif

/*=== INTERNAL VARIABLES ===============================================*/
static CddCache applycache; /* Cache for apply results */
static CddCache quantcache;
static CddCache replacecache;
#ifdef RELAXCACHE
static CddRelaxCache relaxcache;
#endif
static int32_t applyop;
static int32_t opid;

/*=== TEMP EXTERNAL PROTOTYPE ==========================================*/
void cdd2Dot(char* fname, ddNode* node, char* name);

/*=== INTERNAL PROTOTYPES ==============================================*/
static int32_t cdd_contains_rec(ddNode*, raw_t*, int32_t dim);
static ddNode* cdd_apply_rec(ddNode*, ddNode*, bool forced);
#ifdef EX
static ddNode* cdd_exist_rec(ddNode* node, int32_t*, int32_t*, raw_t*);
#else
static ddNode* cdd_exist_rec(ddNode*, int32_t*, ddNode*);
#endif
static ddNode* cdd_replace_rec(ddNode*, int32_t*, int32_t*);

int32_t cdd_operator_init(size_t cachesize)
{
    if (CddCache_init(&applycache, cachesize) < 0) {
        return cdd_error(CDD_MEMORY);
    }
    if (CddCache_init(&quantcache, cachesize) < 0) {
        return cdd_error(CDD_MEMORY);
    }
    if (CddCache_init(&replacecache, cachesize) < 0) {
        return cdd_error(CDD_MEMORY);
    }
#ifdef RELAXCACHE
    if (CddRelaxCache_init(&relaxcache, cachesize) < 0) {
        return cdd_error(CDD_MEMORY);
    }
#endif

    return 0;
}

void cdd_operator_done()
{
    CddCache_done(&applycache);
    CddCache_done(&quantcache);
    CddCache_done(&replacecache);
#ifdef RELAXCACHE
    CddRelaxCache_done(&relaxcache);
#endif
}

void cdd_operator_reset()
{
    CddCache_reset(&applycache);
    CddCache_reset(&quantcache);
    CddCache_reset(&replacecache);
#ifdef RELAXCACHE
    CddRelaxCache_reset(&relaxcache);
#endif
}

void cdd_operator_flush()
{
    CddCache_flush(&applycache);
    CddCache_flush(&quantcache);
    CddCache_flush(&replacecache);
#ifdef RELAXCACHE
    CddRelaxCache_reset(&relaxcache);
#endif
}


ddNode* cdd_apply_forced(ddNode* l, ddNode* h, int32_t op)
{
    ddNode* res;
    applyop = op;
    res = cdd_apply_rec(l, h, true);
    if (cdd_errorcond) {
        cdd_error(cdd_errorcond);
        return NULL;
    }
    return res;
}

ddNode* cdd_push_negate(ddNode* r) {
    return cdd_apply_forced(r, r, cddop_and);
}


ddNode* cdd_apply(ddNode* l, ddNode* h, int32_t op)
{
    ddNode* res;
    applyop = op;
    res = cdd_apply_rec(l, h, false);
    if (cdd_errorcond) {
        cdd_error(cdd_errorcond);
        return NULL;
    }
    return res;
}

static ddNode* cdd_apply_rec(ddNode* l, ddNode* r, bool forced)
{
    CddCacheData* entry;
    int32_t lmask;
    int32_t rmask;
    int32_t mask;
    Elem* top;
    Elem* lp;
    Elem* rp;
    Elem* first;
    ddNode* ll;
    ddNode* lh;
    ddNode* rl;
    ddNode* rh;
    ddNode* n;
    ddNode* prev;
    raw_t bnd;

    /* Back off in case of error */
    if (cdd_errorcond) {
        return 0;
    }

    /* Termination conditons */
    if (!forced) {
        switch (applyop) {
            case cddop_and:
                if (l == r || r == cddtrue) {
                    return l;
                }
                if (l == cddfalse || r == cddfalse || l == cdd_neg(r)) {
                    return cddfalse;
                }
                if (l == cddtrue) {
                    return r;
                }
                break;
            case cddop_xor:
                if (l == r) {
                    return cddfalse;
                }
                if (l == cdd_neg(r)) {
                    return cddtrue;
                }
                if (l == cddfalse) {
                    return r;
                }
                if (r == cddfalse) {
                    return l;
                }
                if (l == cddtrue) {
                    return cdd_neg(r);
                }
                if (r == cddtrue) {
                    return cdd_neg(l);
                }
                break;
        }

    }
        /* The operation is symmetric; normalise for better cache performance */
        if (l > r) {
            n = l;
            l = r;
            r = n;
        }

        if (cdd_isterminal(l) && cdd_isterminal(r)) {
            /* This may happen only for extra terminals and it
             * is strange. We may return either of l or r.
             */
    #ifdef MULTI_TERMINAL
            assert(cdd_is_extra_terminal(l) && cdd_is_extra_terminal(r));
    #endif
            if (l != r) {
                fprintf(stderr, "Diagram is wrong: '%s' between extra terminal nodes.\n",
                        applyop == cddop_and ? "and" : "xor");
            }
            return l;
        }

    /* Do cache lookup */
    //    fprintf(stderr, "%u\n", APPLYHASH(l, r, applyop) % 10000);
    entry = CddCache_lookup(&applycache, APPLYHASH(l, r, applyop));
    if (entry->a == l && entry->b == r && entry->c == applyop) {
        if (cdd_rglr(entry->res)->ref == 0) {
            cdd_reclaim(entry->res);
        }
        return entry->res;
    }

    /* Generate masks to 'push down' the negation bit */
    lmask = cdd_mask(l);
    rmask = cdd_mask(r);
    l = cdd_rglr(l);
    r = cdd_rglr(r);

    switch (cdd_levelinfo[minimum(l->level, r->level)].type) {
    case TYPE_CDD:
        /* Prepare for recursion */
        top = cdd_refstacktop;
        if (l->level <= r->level) {
            lp = cdd_node(l)->elem;
        } else {
            lp = cdd_refstacktop;
            cdd_push(l, INF);
        }

        if (l->level >= r->level) {
            rp = cdd_node(r)->elem;
        } else {
            rp = cdd_refstacktop;
            cdd_push(r, INF);
        }

        /*
         * Do recursion
         */

        first = cdd_refstacktop;

        /* Do first recursion - check whether first edge is negated */
        prev = cdd_apply_rec(cdd_neg_cond(lp->child, lmask), cdd_neg_cond(rp->child, rmask), forced);
        cdd_ref(prev);
        mask = cdd_mask(prev);
        bnd = minimum(lp->bnd, rp->bnd);

        /* Continue */
        while (bnd < INF) {
            lp += (lp->bnd == bnd);
            rp += (rp->bnd == bnd);
            n = cdd_apply_rec(cdd_neg_cond(lp->child, lmask), cdd_neg_cond(rp->child, rmask), forced);
            if (n != prev) {
                cdd_push(cdd_neg_cond(prev, mask), bnd);
                prev = n;
                cdd_ref(prev);
            }
            bnd = minimum(lp->bnd, rp->bnd);
        }
        cdd_push(cdd_neg_cond(prev, mask), INF);

        /* Create node */
        entry->res = cdd_neg_cond(
            cdd_make_cdd_node(minimum(l->level, r->level), first, cdd_refstacktop - first), mask);

        /* Remove references */
        for (; first < cdd_refstacktop; first++) {
            cdd_deref(first->child);
        }

        /* Restore stacktop */
        cdd_refstacktop = top;

        break;
    case TYPE_BDD:
        if (l->level <= r->level) {
            ll = bdd_node(l)->low;
            lh = bdd_node(l)->high;
        } else {
            ll = lh = l;
        }

        if (l->level >= r->level) {
            rl = bdd_node(r)->low;
            rh = bdd_node(r)->high;
        } else {
            rl = rh = r;
        }

        n = cdd_apply_rec(cdd_neg_cond(ll, lmask), cdd_neg_cond(rl, rmask), forced);
        cdd_ref(n);
        entry->res =
            cdd_make_bdd_node(minimum(l->level, r->level), n,
                              cdd_apply_rec(cdd_neg_cond(lh, lmask), cdd_neg_cond(rh, rmask), forced));
        cdd_deref(n);
    }

    /* Update cache entry */
    entry->a = cdd_neg_cond(l, lmask);
    entry->b = cdd_neg_cond(r, rmask);
    entry->c = applyop;

    return entry->res;
}

///////////////////////////////////////////////////////////////////////////

static bool cdd_constrain2(raw_t* dbm, uint32_t dim, uint32_t i, uint32_t j, raw_t lower,
                           raw_t upper)
{
    constraint_t con[2] = {{j, i, bnd_l2u(lower)}, {i, j, upper}};
    return dbm_constrainN(dbm, dim, con, 2);
}

///////////////////////////////////////////////////////////////////////////

ddNode* cdd_ite(ddNode* f, ddNode* g, ddNode* h)
{
    g = cdd_and(f, g);
    cdd_ref(g);
    h = cdd_and(cdd_neg(f), h);
    cdd_ref(h);
    f = cdd_or(g, h);
    cdd_ref(f);
    cdd_rec_deref(g);
    cdd_rec_deref(h);
    cdd_deref(f);
    return f;
}

int32_t cdd_contains(ddNode* node, raw_t* dbm, int32_t dim)
{
    assert(dbm_isValid(dbm, dim));
    return cdd_contains_rec(node, dbm, dim);
}

static int32_t cdd_contains_rec(ddNode* node, raw_t* d, int32_t dim)
{
    raw_t* tmp;
    cdd_iterator it;
    LevelInfo* info;

    /* Check termination conditions */
    if (node == cddtrue)
        return 1;
    if (node == cddfalse)
        return 0;

#ifdef MULTI_TERMINAL
    if (cdd_is_extra_terminal(node)) {
        return cdd_mask(node) ? 0 : 1;
    }
#endif

    info = cdd_info(node);
    switch (info->type) {
    case TYPE_CDD:
        /* If the DBM has a lower dimension than the CDD, then the DBM
         * is a priori bigger than any CDD restricting these extra
         * dimensions. Thus the CDD does not contain the DBM.
         */
        if (info->clock1 >= dim || info->clock2 >= dim) {
            return 0;
        }

        /* Allocate space for copy */
        tmp = malloc(dim * dim * sizeof(raw_t));
        /* Iterate over children */
        for (cdd_it_init(it, node); !cdd_it_atend(it); cdd_it_next(it)) {
            if (!IS_TRUE(cdd_it_child(it))) {
                dbm_copy(tmp, d, dim);
                if (cdd_constrain2(tmp, dim, info->clock1, info->clock2, cdd_it_lower(it),
                                   cdd_it_upper(it)) &&
                    !cdd_contains_rec(cdd_it_child(it), tmp, dim)) {
                    free(tmp);
                    return 0;
                }
            }
        }
        free(tmp);
        break;
    case TYPE_BDD:
        if (!cdd_contains_rec(bdd_node(node)->low, d, dim))
            return 0;
        if (!cdd_contains_rec(bdd_node(node)->high, d, dim))
            return 0;
        break;
    }
    return 1;
}

int32_t cdd_edgecount(ddNode* node)
{
    int32_t num;

    if (node == NULL) {
        return cdd_error(CDD_ILLCDD);
    }
    num = 0;
    cdd_markedgecount(node, &num);
    cdd_unmark(node);
    return num;
}

/* Existentially quantify clocks in a CDD.
 */
#ifdef EX
ddNode* cdd_exist(ddNode* node, int32_t* levels, int32_t* clocks)
{
    int32_t i, j;
    raw_t removed_constraint[cdd_clocknum * cdd_clocknum];
    for (i = 0; i < cdd_clocknum; i++) {
        for (j = 0; j < cdd_clocknum; j++) {
            removed_constraint[i * cdd_clocknum + j] = INF;
        }
    }
    opid++;
    return cdd_exist_rec(node, levels, clocks, removed_constraint);
}
#else
ddNode* cdd_exist(ddNode* node, int32_t* levels)
{
    opid++;
    return cdd_exist_rec(node, levels, cddtrue);
}
#endif

/* // unused
static void cdd_check(ddNode *node)
{
    cdd_iterator it;
    if (cdd_isterminal(node)) {
        return;
    }
    switch (cdd_info(node)->type) {
    case TYPE_CDD:
        cdd_it_init(it, node);
        while (!cdd_it_atend(it)) {
            if (cdd_rglr(cdd_it_child(it))->ref == 0) {
                fprintf(stderr, "Invalid CDD\n");
                return;
            }
            cdd_check(cdd_it_child(it));
            cdd_it_next(it);
        }
        break;
    case TYPE_BDD:
        if (cdd_rglr(bdd_node(node)->low)->ref == 0) {
            fprintf(stderr, "Invalid CDD\n");
            return;
        }
        if (cdd_rglr(bdd_node(node)->high)->ref == 0) {
            fprintf(stderr, "Invalid CDD\n");
            return;
        }
        cdd_check(bdd_node(node)->low);
        cdd_check(bdd_node(node)->high);
    }
}
*/

#ifndef EX
static DBM cdd2dbm(ddNode* node)
{
    cdd_iterator it;
    DBM d = old_dbm_blank(old_dbm_alloc(cdd_clocknum));
    LevelInfo* info;

    while (!cdd_isterminal(node)) {
        cdd_it_init(it, node);
        while (cdd_it_child(it) == cddfalse) {
            cdd_it_next(it);
        }

        info = cdd_info(node);
        old_dbm_tighten_interval(d, info->clock1, info->clock2, cdd_it_lower(it), cdd_it_upper(it));
        node = cdd_it_child(it);
    }
    return d;
}

static ddNode* translate(ddNode* node, int32_t* levels)
{
    ddNode* res;
    int32_t i;
    int32_t j;
    DBM d;

    //     d = cdd2dbm(cdd_reduce(node));
    d = cdd2dbm(node);
    old_dbm_close(d);
    for (i = 1; i < old_dbm_get_size(d); i++) {
        for (j = 0; j < i; j++) {
            if (levels[cdd_diff2level[cdd_difference(i, j)]]) {
                old_dbm_set_bound(d, i, j, INF);
                old_dbm_set_bound(d, j, i, INF);
            }
        }
    }
    res = cdd_from_dbm(d);
    old_dbm_destroy(d);
    return res;
}

static ddNode* cdd_exist_rec(ddNode* node, int32_t* levels, ddNode* c)
{
    int32_t level;
    CddCacheData* entry;
    cdd_iterator it;
    ddNode* res;
    ddNode* tmp1;
    ddNode* tmp2;
    ddNode* tmp3;

    if (node == cddfalse)
        return cddfalse;
    if (node == cddtrue)
        return translate(c, levels);

#ifdef MULTI_TERMINAL
    if (cdd_is_extra_terminal(node)) {
        return cdd_mask(node) ? node : translate(c, levels);
    }
#endif

    entry = CddCache_lookup(&quantcache, EXISTHASH(node, c, opid));
    if (entry->a == node && entry->b == c && entry->c == opid) {
        if (cdd_rglr(entry->res)->ref == 0) {
            cdd_reclaim(entry->res);
        }
        return entry->res;
    }

    level = cdd_rglr(node)->level;
    res = NULL;
    if (levels[level]) {
        switch (cdd_info(node)->type) {
        case TYPE_CDD:
            res = cddfalse;
            for (cdd_it_init(it, node); !cdd_it_atend(it); cdd_it_next(it)) {
                tmp1 = cdd_interval_from_level(level, cdd_it_lower(it), cdd_it_upper(it));
                cdd_ref(tmp1);
                tmp2 = cdd_and(c, tmp1);
                cdd_ref(tmp2);
                cdd_rec_deref(tmp1);
                tmp1 = cdd_exist_rec(cdd_it_child(it), levels, tmp2);
                cdd_ref(tmp1);
                cdd_rec_deref(tmp2);
                tmp2 = cdd_or(tmp1, res);
                cdd_ref(tmp2);
                cdd_rec_deref(tmp1);
                cdd_rec_deref(res);
                res = tmp2;
            }
            cdd_deref(res);
            break;
        case TYPE_BDD:
            tmp1 = cdd_exist_rec(bdd_low(node), levels, c);
            cdd_ref(tmp1);
            tmp2 = cdd_exist_rec(bdd_high(node), levels, c);
            cdd_ref(tmp2);
            res = cdd_or(tmp1, tmp2);
            cdd_ref(res);
            cdd_rec_deref(tmp1);
            cdd_rec_deref(tmp2);
            cdd_deref(res);
        }
    } else {
        switch (cdd_info(node)->type) {
        case TYPE_CDD:
            res = cddfalse;
            for (cdd_it_init(it, node); !cdd_it_atend(it); cdd_it_next(it)) {
                tmp1 = cdd_interval_from_level(level, cdd_it_lower(it), cdd_it_upper(it));
                cdd_ref(tmp1);
                tmp2 = cdd_exist_rec(cdd_it_child(it), levels, c);
                cdd_ref(tmp2);
                tmp3 = cdd_and(tmp1, tmp2);
                cdd_ref(tmp3);
                cdd_rec_deref(tmp1);
                cdd_rec_deref(tmp2);
                tmp1 = cdd_or(res, tmp3);
                cdd_ref(tmp1);
                cdd_rec_deref(res);
                cdd_rec_deref(tmp3);
                res = tmp1;
            }
            cdd_deref(res);
            break;
        case TYPE_BDD:
            tmp1 = cdd_bddvar(level);
            cdd_ref(tmp1);
            tmp2 = cdd_exist_rec(bdd_low(node), levels, c);
            cdd_ref(tmp2);
            tmp3 = cdd_exist_rec(bdd_high(node), levels, c);
            cdd_ref(tmp3);
            res = cdd_ite(tmp1, tmp3, tmp2);
            cdd_ref(res);
            cdd_rec_deref(tmp1);
            cdd_rec_deref(tmp2);
            cdd_rec_deref(tmp3);
            cdd_deref(res);
        }
    }

    entry->a = node;
    entry->b = c;
    entry->c = opid;
    entry->res = res;

    return res;
}
#else
static ddNode* relax(ddNode* node, int32_t* clocks, raw_t lower, int32_t clock1, int32_t clock2,
                     raw_t upper, raw_t* rc)
{
    LevelInfo* info;
    cdd_iterator it;
    ddNode* res;
    ddNode* tmp1;
    ddNode* tmp2;
    ddNode* tmp3;
    ddNode* tmp4;
    ddNode* tmp5;
    int32_t pos;
    int32_t neg;
    raw_t l;
    raw_t u;
#ifdef RELAXCACHE
    CddRelaxCacheData* entry;
#endif

    if (cdd_isterminal(node)) {
        return node;
    }

#ifdef RELAXCACHE
    entry = CddCache_lookup(&relaxcache, RELAXHASH(node, lower, clock1, clock2, upper));
    if (entry->node == node && entry->lower == lower && entry->upper == upper &&
        entry->clock1 == clock1 && entry->clock2 == clock2 && entry->op == opid) {
        if (cdd_rglr(entry->res)->ref == 0) {
            cdd_reclaim(entry->res);
        }
        return entry->res;
    }
#endif

    info = cdd_info(node);

    res = cddfalse;
    switch (info->type) {
    case TYPE_CDD:
        res = cddfalse;
        cdd_it_init(it, node);
        while (!cdd_it_atend(it)) {
            // Detect consequences
            if (info->clock1 == clock1 && clocks[clock1]) {
                pos = info->clock2;
                neg = clock2;
                l = bnd_u2l(bnd_add(cdd_it_upper(it), bnd_l2u(lower)));
                u = bnd_add(upper, bnd_l2u(cdd_it_lower(it)));
            } else if (info->clock1 == clock2 && clocks[clock2]) {
                pos = clock1;
                neg = info->clock2;
                l = bnd_u2l(bnd_add(bnd_l2u(lower), bnd_l2u(cdd_it_lower(it))));
                u = bnd_add(upper, cdd_it_upper(it));
            } else if (info->clock2 == clock1 && clocks[clock1]) {
                pos = info->clock1;
                neg = clock2;
                l = bnd_u2l(bnd_add(bnd_l2u(lower), bnd_l2u(cdd_it_lower(it))));
                u = bnd_add(upper, cdd_it_upper(it));
            } else if (info->clock2 == clock2 && clocks[clock2]) {
                pos = info->clock1;
                neg = clock1;
                l = bnd_u2l(bnd_add(upper, bnd_l2u(cdd_it_lower(it))));
                u = bnd_add(cdd_it_upper(it), bnd_l2u(lower));
            } else {
                l = (u = (neg = (pos = -1)));
            }

            // Call relax recursively

            tmp2 = relax(cdd_it_child(it), clocks, lower, clock1, clock2, upper, rc);
            cdd_ref(tmp2);

            // Add consequence if tighter then those already removed
            if (pos > -1) {
                if ((l > bnd_u2l(rc[neg * cdd_clocknum + pos])) ||
                    (u < rc[pos * cdd_clocknum + neg])) {
                    tmp3 = cdd_interval(pos, neg, maximum(l, bnd_u2l(rc[neg * cdd_clocknum + pos])),
                                        minimum(u, rc[pos * cdd_clocknum + neg]));
                    cdd_ref(tmp3);

                    tmp4 = cdd_and(tmp2, tmp3);
                    cdd_ref(tmp4);

                    cdd_rec_deref(tmp2);
                    cdd_rec_deref(tmp3);

                    tmp2 = tmp4;
                }
            }

            // Rebuild CDD by adding constraints from node
            tmp3 =
                cdd_interval_from_level(cdd_rglr(node)->level, cdd_it_lower(it), cdd_it_upper(it));
            cdd_ref(tmp3);

            tmp4 = cdd_and(tmp2, tmp3);
            cdd_ref(tmp4);

            tmp5 = cdd_or(res, tmp4);
            cdd_ref(tmp5);

            cdd_rec_deref(tmp2);
            cdd_rec_deref(tmp3);
            cdd_rec_deref(tmp4);
            cdd_rec_deref(res);

            res = tmp5;
            cdd_it_next(it);
        }
        break;
    case TYPE_BDD:
        tmp1 = relax(bdd_low(node), clocks, lower, clock1, clock2, upper, rc);
        cdd_ref(tmp1);

        tmp2 = relax(bdd_high(node), clocks, lower, clock1, clock2, upper, rc);
        cdd_ref(tmp2);

        tmp3 = cdd_bddvar(cdd_rglr(node)->level);
        cdd_ref(tmp3);

        res = cdd_ite(tmp3, tmp2, tmp1);
        cdd_ref(res);
        cdd_rec_deref(tmp1);
        cdd_rec_deref(tmp2);
        cdd_rec_deref(tmp3);
        cdd_deref(res);
    }

#ifdef RELAXCACHE
    entry->node = node;
    entry->lower = lower;
    entry->upper = upper;
    entry->clock1 = clock1;
    entry->clock2 = clock2;
    entry->op = opid;
    entry->res = res;
#endif

    return res;
}

static ddNode* cdd_exist_rec(ddNode* node, int32_t* levels, int32_t* clocks, raw_t* rc)
{
    LevelInfo* info;
    CddCacheData* entry;
    cdd_iterator it;
    ddNode* res;
    ddNode* tmp1;
    ddNode* tmp2;
    ddNode* tmp3;
    ddNode* tmp4;
    raw_t old_lower, old_upper;

    if (cdd_isterminal(node)) {
        return node;
    }

    //    cdd2Dot("debug.dot", node, "InEx");
    entry = CddCache_lookup(&quantcache, EXISTHASH(node));
    if (entry->a == node && entry->c == opid) {
        if (cdd_rglr(entry->res)->ref == 0)
            cdd_reclaim(entry->res);
        return entry->res;
    }

    info = cdd_info(node);
    res = NULL;
    switch (info->type) {
    case TYPE_CDD:
        res = cddfalse;
        cdd_it_init(it, node);
        if (clocks[info->clock1] || clocks[info->clock2]) {
            while (!cdd_it_atend(it)) {
                // Here we add the constraint32_t to rc - we save the old
                // constraints so they can be restored.
                old_lower = rc[info->clock2 * cdd_clocknum + info->clock1];
                old_upper = rc[info->clock1 * cdd_clocknum + info->clock2];

                rc[info->clock2 * cdd_clocknum + info->clock1] = bnd_l2u(cdd_it_lower(it));
                rc[info->clock1 * cdd_clocknum + info->clock2] = cdd_it_upper(it);

                tmp1 = relax(cdd_it_child(it), clocks, cdd_it_lower(it), info->clock1, info->clock2,
                             cdd_it_upper(it), rc);

                cdd_ref(tmp1);

                tmp2 = cdd_exist_rec(tmp1, levels, clocks, rc);
                cdd_ref(tmp2);

                tmp3 = cdd_or(res, tmp2);
                cdd_ref(tmp3);

                cdd_rec_deref(res);
                cdd_rec_deref(tmp1);
                cdd_rec_deref(tmp2);
                res = tmp3;

                // Here we restore the constraint
                rc[info->clock2 * cdd_clocknum + info->clock1] = old_lower;
                rc[info->clock1 * cdd_clocknum + info->clock2] = old_upper;

                cdd_it_next(it);
            }
        } else {
            while (!cdd_it_atend(it)) {
                tmp1 = cdd_interval_from_level(cdd_rglr(node)->level, cdd_it_lower(it),
                                               cdd_it_upper(it));
                cdd_ref(tmp1);

                tmp2 = cdd_exist_rec(cdd_it_child(it), levels, clocks, rc);
                cdd_ref(tmp2);

                tmp3 = cdd_and(tmp1, tmp2);
                cdd_ref(tmp3);

                tmp4 = cdd_or(res, tmp3);
                cdd_ref(tmp4);

                cdd_rec_deref(res);
                cdd_rec_deref(tmp1);
                cdd_rec_deref(tmp2);
                cdd_rec_deref(tmp3);
                res = tmp4;
                cdd_it_next(it);
            }
        }
        cdd_deref(res);
        break;
    case TYPE_BDD:
        tmp1 = cdd_exist_rec(bdd_low(node), levels, clocks, rc);
        cdd_ref(tmp1);

        tmp2 = cdd_exist_rec(bdd_high(node), levels, clocks, rc);
        cdd_ref(tmp2);

        if (levels[cdd_rglr(node)->level]) {
            res = cdd_or(tmp1, tmp2);
            cdd_ref(res);
        } else {
            tmp3 = cdd_bddvar(cdd_rglr(node)->level);
            cdd_ref(tmp3);
            res = cdd_ite(tmp3, tmp2, tmp1);
            cdd_ref(res);
            cdd_rec_deref(tmp3);
        }

        cdd_rec_deref(tmp1);
        cdd_rec_deref(tmp2);
        cdd_deref(res);
    }

    entry->a = node;
    entry->c = opid;
    entry->res = res;

    return res;
}
#endif

ddNode* cdd_replace(ddNode* node, int32_t* levels, int32_t* clocks)
{
    opid++;
    return cdd_replace_rec(node, levels, clocks);
}

static ddNode* cdd_replace_rec(ddNode* node, int32_t* levels, int32_t* clocks)
{
    CddCacheData* entry;
    ddNode* res;
    ddNode* tmp1;
    ddNode* tmp2;
    ddNode* tmp3;
    cdd_iterator it;
    LevelInfo* info;

    if (cdd_isterminal(node)) {
        return node;
    }

    entry = CddCache_lookup(&replacecache, REPLACEHASH(node));
    if (entry->a == node && entry->c == opid) {
        if (cdd_rglr(entry->res)->ref == 0)
            cdd_reclaim(entry->res);
        return entry->res;
    }

    info = cdd_info(node);
    res = NULL;
    switch (info->type) {
    case TYPE_BDD:
        tmp1 = cdd_bddvar(levels[cdd_rglr(node)->level]);
        cdd_ref(tmp1);
        tmp2 = cdd_replace_rec(bdd_low(node), levels, clocks);
        cdd_ref(tmp2);
        tmp3 = cdd_replace_rec(bdd_high(node), levels, clocks);
        cdd_ref(tmp3);
        res = cdd_ite(tmp1, tmp3, tmp2);
        cdd_ref(res);
        cdd_rec_deref(tmp1);
        cdd_rec_deref(tmp2);
        cdd_rec_deref(tmp3);
        cdd_deref(res);
        break;
    case TYPE_CDD:
        res = cddfalse;
        for (cdd_it_init(it, node); !cdd_it_atend(it); cdd_it_next(it)) {
            tmp1 = cdd_interval(clocks[info->clock1], clocks[info->clock2], cdd_it_lower(it),
                                cdd_it_upper(it));
            cdd_ref(tmp1);
            tmp2 = cdd_replace_rec(cdd_it_child(it), levels, clocks);
            cdd_ref(tmp2);
            tmp3 = cdd_and(tmp1, tmp2);
            cdd_ref(tmp3);
            cdd_rec_deref(tmp1);
            cdd_rec_deref(tmp2);
            tmp1 = cdd_or(res, tmp3);
            cdd_ref(tmp1);
            cdd_rec_deref(res);
            cdd_rec_deref(tmp3);
            res = tmp1;
        }
        cdd_deref(res);
    }

    entry->a = node;
    entry->c = opid;
    entry->res = res;

    return res;
}
#if 1
ddNode* cdd_from_dbm(const raw_t* dbm, int32_t size)
{
    int32_t i;
    int32_t j;
    int32_t k;
    uint32_t ok[bits2intsize(size * size)];
    int32_t lo, hi;
    Elem* top;
    ddNode* c;
    ddNode* tmp;
    LevelInfo* info;

    dbm_analyzeForMinDBM(dbm, size, ok);

    /* Create CDD
     *
     * The idea is to build the CDD bottom-up by traversing all
     * declared levels in reverse order. We need to take care
     * of negated edges - this only matters if we don't have a
     * lower bound on the node.
     */
    c = cddtrue;
    for (k = cdd_levelcnt - 1; k >= 0; k--) {
        info = cdd_levelinfo + k;
        if (info->type != TYPE_CDD) {
            continue;
        }

        i = info->clock1;
        j = info->clock2;

        /* This is a hack! The problem arises when converting DBMs
         * smaller than the number of clocks declared in the CDD
         * package.
         */
        if (i >= size || j >= size) {
            continue;
        }

        //      lo = ok[j * size + i] && (old_dbm_get_bound(d, j, i) < ZERO || j > 0);
        //      hi = ok[i * size + j] && (old_dbm_get_bound(d, i, j) < ZERO || i > 0);

        lo = base_getOneBit(ok, j * size + i);
        hi = base_getOneBit(ok, i * size + j);

        if (lo || hi) {
            top = cdd_refstacktop;
            tmp = c;
            if (lo) {
                cdd_push(cddfalse, bnd_u2l(dbm[j * size + i]));
                if (hi) {
                    cdd_push(c, dbm[i * size + j]);
                    cdd_push(cddfalse, INF);
                } else {
                    cdd_push(c, INF);
                }

                c = cdd_make_cdd_node(k, top, cdd_refstacktop - top);
            } else {
                cdd_push(cdd_rglr(c), dbm[i * size + j]);
                cdd_push(cdd_neg_cond(cddfalse, cdd_mask(c)), INF);
                c = cdd_neg_cond(cdd_make_cdd_node(k, top, cdd_refstacktop - top), cdd_mask(c));
            }
            cdd_ref(c);
            cdd_deref(tmp);
            cdd_refstacktop = top;
        }
    }

    cdd_deref(c);
    return c;
}
#else
ddNode* cdd_from_dbm(const raw_t* dbm, int32_t size)
{
    int32_t i;
    int32_t j;
    int32_t k;
    int32_t lo, hi;
    Elem* top;
    ddNode* c;
    LevelInfo* info;

    /* Create CDD
     *
     * The idea is to build the CDD bottom-up by traversing all
     * declared levels in reverse order. We need to take care
     * of negated edges - this only matters if we don't have a
     * lower bound on the node.
     */
    c = cddtrue;
    for (k = cdd_levelcnt - 1; k >= 0; k--) {
        info = cdd_levelinfo + k;
        if (info->type != TYPE_CDD) {
            continue;
        }

        i = info->clock1;
        j = info->clock2;

        /* This is a hack! The problem arises when converting DBMs
         * smaller than the number of clocks declared in the CDD
         * package.
         */
        if (i >= size || j >= size) {
            continue;
        }

        lo = dbm[j * size + i] < dbm_LS_INFINITY;
        hi = dbm[i * size + j] < dbm_LS_INFINITY;

        if (lo || hi) {
            top = cdd_refstacktop;
            if (lo) {
                cdd_push(cddfalse, bnd_u2l(dbm[j * size + i]));
                if (hi) {
                    cdd_push(c, dbm[i * size + j]);
                    cdd_push(cddfalse, INF);
                } else {
                    cdd_push(c, INF);
                }

                c = cdd_make_cdd_node(k, top, cdd_refstacktop - top);
            } else {
                cdd_push(cdd_rglr(c), dbm[i * size + j]);
                cdd_push(cdd_neg_cond(cddfalse, cdd_mask(c)), INF);
                c = cdd_neg_cond(cdd_make_cdd_node(k, top, cdd_refstacktop - top), cdd_mask(c));
            }
            cdd_refstacktop = top;
        }
    }

    return c;
}
#endif

ddNode* cdd_extract_dbm(ddNode* cdd, raw_t* dbm, int32_t size)
{
    cdd_iterator it;
    LevelInfo* info;
    ddNode *node, *zone, *result;
    uint32_t touched[bits2intsize(size)];

    // cdd_printdot(cdd);

    node = cdd;

    dbm_init(dbm, size);
    // dbm_print(stdout, dbm, size);

    base_resetBits(touched, bits2intsize(size));

    while (!cdd_isterminal(node)) {
        info = cdd_info(node);

        assert(info->clock1 < size);
        assert(info->clock2 < size);

        cdd_it_init(it, node);
        if (IS_FALSE(cdd_it_child(it))) {
            cdd_it_next(it);
        }

        assert(cdd_it_child(it) != cddfalse);

        /*printf("%d %d %d %d\n",
               info->clock1, info->clock2,
               bnd_l2u(cdd_it_lower(it)), cdd_it_upper(it));*/

        dbm_constrain(dbm, size, info->clock2, info->clock1, bnd_l2u(cdd_it_lower(it)), touched);

        dbm_constrain(dbm, size, info->clock1, info->clock2, cdd_it_upper(it), touched);

        node = cdd_it_child(it);
    }

    dbm_closex(dbm, size, touched);
    assert(dbm_isValid(dbm, size));

    zone = cdd_from_dbm(dbm, size);
    cdd_ref(zone);

    result = cdd_and(cdd, cdd_neg(zone));
    cdd_deref(zone);

    return result;
}

void cdd_mark_clock(int32_t* vec, int32_t c)
{
    int32_t n;
    for (n = 0; n < cdd_clocknum; n++) {
        if (n < c) {
            vec[cdd_diff2level[cdd_difference(c, n)]] = 1;
        } else if (n > c) {
            vec[cdd_diff2level[cdd_difference(n, c)]] = 1;
        }
    }
}

int32_t cdd_nodecount(ddNode* node)
{
    int32_t cnt = 0;

    if (node == NULL) {
        return cdd_error(CDD_ILLCDD);
    }
    cdd_markcount(node, &cnt);
    cdd_unmark(node);
    return cnt;
}

static ddNode* add_bound(ddNode* c, int32_t level, raw_t low, raw_t up)
{
    ddNode *tmp1, *tmp2;
    if (low == -INF && up == INF) {
        return c;
    }
    tmp1 = cdd_interval_from_level(level, low, up);
    cdd_ref(tmp1);
    tmp2 = cdd_and(c, tmp1);
    cdd_ref(tmp2);
    cdd_rec_deref(tmp1);
    cdd_deref(tmp2);
    return tmp2;
}

static int32_t equiv(ddNode* c, ddNode* d)
{
    ddNode* tmp1;
    ddNode* tmp2;

    tmp1 = cdd_xor(c, d);
    cdd_ref(tmp1);

    tmp2 = cdd_reduce(tmp1);
    cdd_ref(tmp2);

    cdd_rec_deref(tmp1);
    cdd_rec_deref(tmp2);
    return tmp2 == cddfalse;
}

static ddNode* cdd_reduce2_rec(ddNode* node)
{
    ddNode* res = cddfalse;
    ddNode* tmp1;
    ddNode* tmp2;
    ddNode* tmp3;
    ddNode* prev;
    ddNode* split;
    ddNode* join;
    raw_t low;
    cdd_iterator it;
    LevelInfo* info;

    if (cdd_isterminal(node)) {
        return node;
    }

    info = cdd_info(node);
    switch (info->type) {
    case TYPE_CDD:
        res = cddfalse;

        cdd_it_init(it, node);
        low = cdd_it_lower(it);

        prev = cdd_it_child(it);
        cdd_ref(prev);

        cdd_it_next(it);
        while (!cdd_it_atend(it)) {
            /* Calculate split */
            tmp1 = add_bound(prev, cdd_rglr(node)->level, low, cdd_it_lower(it));
            cdd_ref(tmp1);
            tmp2 = add_bound(cdd_it_child(it), cdd_rglr(node)->level, cdd_it_lower(it),
                             cdd_it_upper(it));
            cdd_ref(tmp2);
            split = cdd_or(tmp1, tmp2);
            cdd_ref(split);
            cdd_rec_deref(tmp1);
            cdd_rec_deref(tmp2);

            /* Calculate join */
            tmp1 = cdd_or(prev, cdd_it_child(it));
            cdd_ref(tmp1);
            join = add_bound(tmp1, cdd_rglr(node)->level, low, cdd_it_upper(it));
            cdd_ref(join);

            /* Are they equivalent ? */
            if (equiv(split, join)) {
                /* Yes, use the union as the new prev */
                cdd_rec_deref(prev);
                prev = tmp1;
            } else {
                /* Nope */
                cdd_rec_deref(tmp1);

                /* First we reduce the previous */
                tmp1 = cdd_reduce2_rec(prev);
                cdd_ref(tmp1);

                /* And bind it */
                tmp2 = add_bound(tmp1, cdd_rglr(node)->level, low, cdd_it_lower(it));
                cdd_ref(tmp2);

                /* And add it to the result */
                tmp3 = cdd_or(res, tmp2);
                cdd_ref(tmp3);

                cdd_rec_deref(tmp1);
                cdd_rec_deref(tmp2);
                cdd_rec_deref(res);
                res = tmp3;

                /* Use current as prev */
                cdd_rec_deref(prev);
                prev = cdd_it_child(it);
                cdd_ref(prev);
                low = cdd_it_lower(it);
            }
            cdd_rec_deref(split);
            cdd_rec_deref(join);

            cdd_it_next(it);
        }

        tmp1 = cdd_reduce2_rec(prev);
        cdd_ref(tmp1);
        tmp2 = add_bound(tmp1, cdd_rglr(node)->level, low, INF);
        cdd_ref(tmp2);
        tmp3 = cdd_or(res, tmp2);
        cdd_ref(tmp3);
        cdd_rec_deref(tmp1);
        cdd_rec_deref(tmp2);
        cdd_rec_deref(res);
        cdd_rec_deref(prev);
        res = tmp3;
        cdd_deref(res);
        break;
    case TYPE_BDD:
        tmp1 = cdd_reduce2_rec(bdd_low(node));
        cdd_ref(tmp1);
        tmp2 = cdd_reduce2_rec(bdd_high(node));
        cdd_ref(tmp2);
        res = cdd_make_bdd_node(cdd_rglr(node)->level, tmp1, tmp2);
        cdd_deref(tmp1);
        cdd_deref(tmp2);
    }
    return res;
}

ddNode* cdd_reduce2(ddNode* node) { return cdd_reduce2_rec(node); }

///////////////////////////////////////////////////////////////////////////

static ddNode* cdd_tarjan_reduce_rec(ddNode* node, struct tarjan* graph)
{
    raw_t bnd;
    int32_t mask;
    int32_t modified;
    Elem* top;
    cdd_iterator it;
    ddNode* m;
    ddNode* n;
    LevelInfo* info;

    /* Termination conditions */
    if (cdd_isterminal(node))
        return node;

    info = cdd_info(node);
    switch (info->type) {
    case TYPE_BDD:
        n = cdd_tarjan_reduce_rec(cdd_neg_cond(bdd_node(node)->low, cdd_mask(node)), graph);
        cdd_ref(n);
        m = cdd_make_bdd_node(
            cdd_rglr(node)->level, n,
            cdd_tarjan_reduce_rec(cdd_neg_cond(bdd_node(node)->high, cdd_mask(node)), graph));
        cdd_deref(n);
        break;

    case TYPE_CDD:
        /* Find first consistent child. Lower bounds do not matter
         * here since we know any edge to the left of the current one
         * is inconsistent.
         */
        modified = 0;
        cdd_it_init(it, node);
        cdd_tarjan_push(graph, info->clock1, info->clock2, cdd_it_upper(it));
        while (!cdd_tarjan_consistent(graph)) {
            modified = 1;
            cdd_tarjan_pop(graph, info->clock1);
            cdd_it_next(it);
            bnd = cdd_it_upper(it);
            if (bnd == dbm_LS_INFINITY) {
                /* We have reached the last child, hence all bounds
                 * except the last are inconsistent, hence this node
                 * has no effect at all. Reduce the last child and
                 * return it directly.
                 */
                return cdd_tarjan_reduce_rec(cdd_it_child(it), graph);
            }
            cdd_tarjan_push(graph, info->clock1, info->clock2, bnd);
        }

        /* Do recursion for the first consistent child we found above.
         */
        m = cdd_tarjan_reduce_rec(cdd_it_child(it), graph);
        mask = cdd_mask(m);
        cdd_ref(m);
        cdd_tarjan_pop(graph, info->clock1);
        modified |= (m != cdd_it_child(it));

        /* Repeat until next inconsistent bound or the last bound.
         */
        top = cdd_refstacktop;
        for (cdd_it_next(it); !cdd_it_atend(it); cdd_it_next(it)) {
            cdd_tarjan_push(graph, info->clock2, info->clock1, bnd_l2u(cdd_it_lower(it)));
            if (!cdd_tarjan_consistent(graph)) {
                modified = 1;
                cdd_tarjan_pop(graph, info->clock2);
                break;
            }

            bnd = cdd_it_upper(it);
            if (bnd < dbm_LS_INFINITY) {
                cdd_tarjan_push(graph, info->clock1, info->clock2, bnd);
                n = cdd_tarjan_reduce_rec(cdd_it_child(it), graph);
                cdd_tarjan_pop(graph, info->clock1);
            } else {
                n = cdd_tarjan_reduce_rec(cdd_it_child(it), graph);
            }

            modified |= (n != cdd_it_child(it));
            if (m != n) {
                cdd_push(cdd_neg_cond(m, mask), cdd_it_lower(it));
                m = n;
                cdd_ref(m);
            }
            cdd_tarjan_pop(graph, info->clock2);
        }
        cdd_push(cdd_neg_cond(m, mask), INF);

        /* Create node */
        if (modified) {
            m = cdd_neg_cond(cdd_make_cdd_node(cdd_rglr(node)->level, top, cdd_refstacktop - top),
                             mask);
        } else {
            m = node;
        }

        /* Remove references */
        while (cdd_refstacktop > top) {
            cdd_refstacktop--;
            cdd_deref(cdd_refstacktop->child);
        }
        break;
    default: m = NULL;
    }
    return m;
}

ddNode* cdd_reduce(ddNode* node)
{
    struct tarjan graph;
    struct distance dist[cdd_clocknum];
    uint32_t count[cdd_clocknum];
    struct edge edges[cdd_clocknum * cdd_clocknum - cdd_clocknum];
    struct node fifo[cdd_clocknum + 1];
    uint32_t queued[bits2intsize(cdd_clocknum)];

    cdd_tarjan_init(&graph, cdd_clocknum, dist, count, edges, fifo, queued);
    return cdd_tarjan_reduce_rec(node, &graph);
}

///////////////////////////////////////////////////////////////////////////

static ddNode* cdd_apply_reduce_rec(ddNode* l, ddNode* r, struct tarjan* graph)
{
    assert(cdd_tarjan_consistent(graph));

    CddCacheData* entry;
    int32_t lmask;
    int32_t rmask;
    int32_t mask;
    Elem* top;
    Elem* lp;
    Elem* rp;
    Elem* first;
    ddNode* ll;
    ddNode* lh;
    ddNode* rl;
    ddNode* rh;
    ddNode* n;
    ddNode* prev;
    ddNode* res = NULL;
    raw_t bnd;
    raw_t lower;
    LevelInfo* info;

    /* Back off in case of error.
     */
    if (cdd_errorcond) {
        return 0;
    }

    /* Termination conditons.
     */
    switch (applyop) {
    case cddop_and:
        if (l == r || r == cddtrue) {
            return cdd_tarjan_reduce_rec(l, graph);
        }
        if (l == cddfalse || r == cddfalse || l == cdd_neg(r)) {
            return cddfalse;
        }
        if (l == cddtrue) {
            return cdd_tarjan_reduce_rec(r, graph);
        }
#ifdef MULTI_TERMINAL
        if (cdd_is_extra_terminal(l)) {
            return cdd_mask(l) ? l : cdd_tarjan_reduce_rec(r, graph);
        }
        if (cdd_is_extra_terminal(r)) {
            return cdd_mask(r) ? r : cdd_tarjan_reduce_rec(l, graph);
        }
#endif
        break;
    case cddop_xor:
        if (l == r) {
            return cddfalse;
        }
        if (l == cdd_neg(r)) {
            return cddtrue;
        }
        if (l == cddfalse) {
            return cdd_tarjan_reduce_rec(r, graph);
        }
        if (r == cddfalse) {
            return cdd_tarjan_reduce_rec(l, graph);
        }
        if (l == cddtrue) {
            return cdd_tarjan_reduce_rec(cdd_neg(r), graph);
        }
        if (r == cddtrue) {
            return cdd_tarjan_reduce_rec(cdd_neg(l), graph);
        }
        break;
#ifdef MULTI_TERMINAL
        if (cdd_is_extra_terminal(l)) {
            return cdd_mask(l) ? cdd_tarjan_reduce_rec(r, graph)
                               : cdd_tarjan_reduce_rec(cdd_neg(r), graph);
        }
        if (cdd_is_extra_terminal(r)) {
            return cdd_mask(r) ? cdd_tarjan_reduce_rec(l, graph)
                               : cdd_tarjan_reduce_rec(cdd_neg(l), graph);
        }
#endif
    }

    /* The operation is symmetric; normalise for better cache performance.
     */
    if (l > r) {
        n = l;
        l = r;
        r = n;
    }

    /* Do cache lookup.
     */
    entry = CddCache_lookup(&applycache, APPLYHASH(l, r, applyop));
    if (entry->a == l && entry->b == r && entry->c == applyop) {
        if (cdd_rglr(entry->res)->ref == 0) {
            cdd_reclaim(entry->res);
        }
        cdd_ref(entry->res);
        res = cdd_tarjan_reduce_rec(entry->res, graph);
        cdd_rec_deref(entry->res);
        return res;
    }

    /* Generate masks to 'push down' the negation bit.
     */
    lmask = cdd_mask(l);
    rmask = cdd_mask(r);
    l = cdd_rglr(l);
    r = cdd_rglr(r);

    info = cdd_levelinfo + minimum(l->level, r->level);
    switch (info->type) {
    case TYPE_CDD:
        /* Prepare for recursion: In case the two nodes do not have
         * the same level we create a fake intermediate node.
         */
        top = cdd_refstacktop;
        if (l->level <= r->level) {
            lp = cdd_node(l)->elem;
        } else {
            lp = cdd_refstacktop;
            cdd_push(l, INF);
        }

        if (l->level >= r->level) {
            rp = cdd_node(r)->elem;
        } else {
            rp = cdd_refstacktop;
            cdd_push(r, INF);
        }

        /*
         * Do recursion
         */

        first = cdd_refstacktop;

        /* Find first consistent child: We only need to check the
         * upper bound since we know that all children to the left of
         * the current child are on inconsistent paths.
         */
        bnd = minimum(lp->bnd, rp->bnd);
        cdd_tarjan_push(graph, info->clock1, info->clock2, bnd);
        while (!cdd_tarjan_consistent(graph)) {
            cdd_tarjan_pop(graph, info->clock1);
            lp += (lp->bnd == bnd);
            rp += (rp->bnd == bnd);
            bnd = minimum(lp->bnd, rp->bnd);
            if (bnd == dbm_LS_INFINITY) {
                cdd_refstacktop = top;
                return cdd_apply_reduce_rec(cdd_neg_cond(lp->child, lmask),
                                            cdd_neg_cond(rp->child, rmask), graph);
            }
            cdd_tarjan_push(graph, info->clock1, info->clock2, bnd);
        }

        /* Do first recursion - check whether first edge is negated.
         */
        prev = cdd_apply_reduce_rec(cdd_neg_cond(lp->child, lmask), cdd_neg_cond(rp->child, rmask),
                                    graph);
        cdd_ref(prev);
        mask = cdd_mask(prev);
        cdd_tarjan_pop(graph, info->clock1);

        /* Perform intermediate recursions: For each we need to apply
         * both the lower and upper bound. We are done when either we
         * have reached the last child (bnd is INF) or the path is
         * inconsistent (in which case all reamining children are
         * inconsistent as well).
         */
        lp += (lp->bnd == bnd);
        rp += (rp->bnd == bnd);
        lower = bnd;
        bnd = minimum(lp->bnd, rp->bnd);
        cdd_tarjan_push(graph, info->clock2, info->clock1, bnd_l2u(lower));
        while (bnd < INF && cdd_tarjan_consistent(graph)) {
            cdd_tarjan_push(graph, info->clock1, info->clock2, bnd);
            n = cdd_apply_reduce_rec(cdd_neg_cond(lp->child, lmask), cdd_neg_cond(rp->child, rmask),
                                     graph);
            cdd_tarjan_pop(graph, info->clock1);
            cdd_tarjan_pop(graph, info->clock2);

            if (n != prev) {
                cdd_push(cdd_neg_cond(prev, mask), lower);
                prev = n;
                cdd_ref(prev);
            }

            lp += (lp->bnd == bnd);
            rp += (rp->bnd == bnd);
            lower = bnd;
            bnd = minimum(lp->bnd, rp->bnd);

            cdd_tarjan_push(graph, info->clock2, info->clock1, bnd_l2u(lower));
        }

        /* We still need to do the recursion for the last child, but
         * only if the path is consistent.
         */
        if (bnd == INF && cdd_tarjan_consistent(graph)) {
            n = cdd_apply_reduce_rec(cdd_neg_cond(lp->child, lmask), cdd_neg_cond(rp->child, rmask),
                                     graph);
            if (n != prev) {
                cdd_push(cdd_neg_cond(prev, mask), lower);
                prev = n;
                cdd_ref(prev);
            }
        }

        /* Push the final new child to the stack.
         */
        cdd_tarjan_pop(graph, info->clock2);
        cdd_push(cdd_neg_cond(prev, mask), INF);

        /* Create node.
         */
        res = cdd_neg_cond(
            cdd_make_cdd_node(minimum(l->level, r->level), first, cdd_refstacktop - first), mask);

        /* Remove references.
         */
        do {
            cdd_refstacktop--;
            cdd_deref(cdd_refstacktop->child);
        } while (cdd_refstacktop > first);

        /* Restore stacktop.
         */
        cdd_refstacktop = top;
        break;
    case TYPE_BDD:
        if (l->level <= r->level) {
            ll = bdd_node(l)->low;
            lh = bdd_node(l)->high;
        } else {
            ll = lh = l;
        }

        if (l->level >= r->level) {
            rl = bdd_node(r)->low;
            rh = bdd_node(r)->high;
        } else {
            rl = rh = r;
        }

        n = cdd_apply_reduce_rec(cdd_neg_cond(ll, lmask), cdd_neg_cond(rl, rmask), graph);
        cdd_ref(n);
        res = cdd_make_bdd_node(
            minimum(l->level, r->level), n,
            cdd_apply_reduce_rec(cdd_neg_cond(lh, lmask), cdd_neg_cond(rh, rmask), graph));
        cdd_deref(n);
    }

    return res;
}

ddNode* cdd_apply_reduce(ddNode* l, ddNode* h, int32_t op)
{
    ddNode* res;

    /* Data structures needed for running Tarjans algoritm.
     */
    struct tarjan graph;
    struct distance dist[cdd_clocknum];
    uint32_t count[cdd_clocknum];
    struct edge edges[cdd_clocknum * cdd_clocknum - cdd_clocknum];
    struct node fifo[cdd_clocknum + 1];
    uint32_t queued[bits2intsize(cdd_clocknum)];

    cdd_tarjan_init(&graph, cdd_clocknum, dist, count, edges, fifo, queued);

    applyop = op;
    res = cdd_apply_reduce_rec(l, h, &graph);
    if (cdd_errorcond) {
        cdd_error(cdd_errorcond);
        return NULL;
    }
    return res;
}
