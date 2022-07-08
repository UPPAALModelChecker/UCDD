// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-
///////////////////////////////////////////////////////////////////////////////
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 2011 - 2018, Aalborg University.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: kernel.h,v 1.6 2004/05/19 10:58:36 behrmann Exp $
//
///////////////////////////////////////////////////////////////////////////////

#ifndef CDD_KERNEL_H_
#define CDD_KERNEL_H_

#include "cdd/cdd.h"
#include "cdd/config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file kernel.h Header file for internal things
 */

// Utility macros for dealing with bounds: Should be replaced with
// those defined in the dbm module.
#define INF                      0x7FFFFFFE
#define bnd_upper(limit, strict) (((limit) << 1) | ((strict) ? 0 : 1))
#define bnd_lower(limit, strict) (((limit) << 1) | ((strict) ? 1 : 0))
#define bnd_get_limit(b)         ((b) >> 1)
#define bnd_is_upper_strict(b)   (!((b)&0x1))
#define bnd_is_lower_strict(b)   ((b)&0x1)
#define bnd_add(a, b)            (((a) == INF || (b) == INF) ? INF : (((((a) & ~(0x1)) + ((b) & ~(0x1)))) | ((a) & (b)&0x1)))
#define bnd_l2u(b)               ((b) == -INF ? INF : ((-((b) & ~(0x1))) | ((b)&0x1)) ^ 0x1)
#define bnd_u2l(b)               ((b) == INF ? -INF : ((-((b) & ~(0x1))) | ((b)&0x1)) ^ 0x1)
#define bnd_flip_strict(b)       ((b) == ((-((b) & ~(0x1))) | ((b)&0x1)) ^ 0x1)


///////////////////////////////////////////////////////////////////////////
/// @defgroup kernel Internal interfaces
///
///
///
/// @{

/**
 * Version number.
 * @see \c cdd_versionnum()
 */
#define CDD_VERSION 03

///////////////////////////////////////////////////////////////////////////
/// @defgroup negation Negation bit
///
/// DDs are semantically negated by toggling the least significant bit
/// on the pointer to the root node of the DD. Semanticaly this is
/// equivalent interchanging all false and true terminales with each
/// other. A normal form is maintained by requiring that the first
/// child (the low child for BDD nodes and the left most child for CDD
/// nodes) is always a regular pointer, i.e. it is not negated.
///
/// @{
///

/** Returns the unmarked pointer to \a node */
#define cdd_rglr(node) ((ddNode*)(((uintptr_t)(node)) & ~0x1))

/** Returns the last bit (negated or true) of a \a node */
#define cdd_is_negated(node) ((((uintptr_t)(node)) & 1))

/** Returns a marked pointer to \a node */
#define cdd_mask(node) (((uintptr_t)(node)) & 0x1)

/** Conditionally negates \a node. If \a mask is 1, then \a node is
 *  negated otherwise not.
 */
#define cdd_neg_cond(node, mask) ((ddNode*)(((uintptr_t)(node)) ^ (mask)))

/** Returns the cddnode_ structure corresponding to \a node */
#define cdd_node(node) ((cddNode*)(cdd_rglr(node)))

/** Returns the bddnode_ structure corresponding to \a node */
#define bdd_node(node) ((bddNode*)(cdd_rglr(node)))

/** @} */

///////////////////////////////////////////////////////////////////////////
/// @defgroup refcount Reference counting
///
/// Each node has a 10-bit reference counter. Since this is a rather
/// limited size, the library detects when the maximum number of
/// references is reached and from that point onward the reference
/// count is never modified. As a consequence the node is never
/// deallocated (until \c cdd_done() is called). This does not happen
/// very often and thus the leakage is negligible.
///
/// @{
///

/** Max number of references */
#define MAXREF 0x3FF

/** Max number of levels.
 *  The allowed number of variables (BDD+CDD) must
 *  be < MAXLEVEL.
 */
#define MAXLEVEL ((1 << 20) - 1)

/** Increment references on \a node */
#define cdd_ref(node) (cdd_satinc(cdd_rglr(node)->ref))

/** Decrement reference on \a node */
#define cdd_deref(node) (cdd_satdec(cdd_rglr(node)->ref))

/** Increments \a ref if it is not equal to MAXREF */
#define cdd_satinc(ref) ((ref) += ((ref) != MAXREF))

/** Decrements \a ref if it is not equal to MAXREF */
#define cdd_satdec(ref) ((ref) -= ((ref) != MAXREF))

#ifdef MULTI_TERMINAL
int32_t cdd_isterminal(ddNode*);
int32_t cdd_is_extra_terminal(ddNode*);
#else
/** Returns true if \a node is a terminal */
#define cdd_isterminal(node) (cdd_rglr(node) == cddfalse)
#endif

/**
 * @return 1 if the node is a terminal true or false.
 * This is useful for the case of multi-terminals.
 */
#define cdd_is_tfterminal(node) (cdd_rglr(node) == cddfalse)

/**
 * Decrements the reference counter on \a node. If the counter hits
 * zero, the reference counter on the children of \a node is
 * recursively decremented.
 *
 * @param node a node
 */
extern void cdd_rec_deref(ddNode* node);

/** @} */

///////////////////////////////////////////////////////////////////////////
/// @defgroup Flags
///
/// Nodes can be flagged or marked. This is -- amongst other things --
/// used by the mark-and-sweep algorithm in the garbage collector.
///
/// @{
///

#define MARKON   0x1 /**< Bit used to mark a node (1) */
#define MARKOFF  0x2 /**< Mask used to unmark a node */
#define MARKHIDE 0x2

/** Mark node \a n */
#define cdd_setmark(n) (cdd_rglr(n)->flag) |= MARKON

/** Unmark node \a n */
#define cdd_resetmark(n) (cdd_rglr(n)->flag) &= MARKOFF

/** Returns true if \a n is marked. */
#define cdd_ismarked(n) ((cdd_rglr(n)->flag) & MARKON)

/**
 * Recursively marks all nodes of \a node. Does not recurse into a
 * node which is already marked.
 * @param node a DD node
 */
void cdd_mark(ddNode* node);

/**
 * Recursively marks all nodes of \a node. Increments \a *count for
 * each new node which is marked. Does not recurse into a node which
 * is already marked.
 * @param node a DD node
 * @param count pointer to a counter
 */
void cdd_markcount(ddNode* node, int32_t* count);

/**
 * Recursively marks all nodes of \a node. Increments \a *count for
 * each recursive call. Does not recurse into a node which is already
 * marked.
 * @param node a DD node
 * @param count pointer to a counter
 */
void cdd_markedgecount(ddNode* node, int32_t* count);

/**
 * Recursively unmarks all nodes of \a node. Does not recurse into a
 * node which is not marked.
 * @param node a DD node
 */
void cdd_unmark(ddNode* node);

/**
 * Recursively unmark all nodes. This function recurses into nodes
 * that are not marked, which allows us to unmark a partially marked
 * CDD. However, this may be quite expensive.
 */
void cdd_force_unmark(ddNode* node);

/** @} */

///////////////////////////////////////////////////////////////////////////
/// @defgroup node Node management
///
/// The library needs to maintain different types of nodes: fixed size
/// BDD nodes and variable size CDD nodes.
///
/// Each type/size of node is managed by a different node manager. A
/// node manager allocates memory in 64KB chunks which is divided into
/// equally sized nodes. It maintains a free list of unused nodes and
/// some statistical information about the nodes, which are used by
/// the garbage collector.
///
/// Each node manager maintains a number of subtables -- one for each
/// level. A subtable is basically a hash table containing nodes. The
/// hash table uses a collision list embedded into the nodes; each
/// node contains a \c next pointer which points to the next element
/// in the collision list. From time to time the hash table is resized
/// by doubling the size and rehashing all the elements. Notice that
/// since subtables are local to a node manager and tied to a specific
/// level, searching for existing nodes in a subtable is relatively
/// simple.
///
/// @{
///

typedef struct elem_ Elem;
typedef struct subtable_ SubTable;
typedef struct nodemanager_ NodeManager;
typedef struct cddnode_ cddNode;
typedef struct bddnode_ bddNode;
typedef struct xtermnode_ xtermNode;
typedef struct chunk_ Chunk;
typedef uint32_t (*NodeHashFunc)(NodeManager*, ddNode*);

/**
 * Base type for DD nodes.
 */
struct node_
{
    ddNode* next;         ///< Pointer to next element in hash table
    uint32_t level : 20;  ///< Level of the node
    uint32_t ref : 10;    ///< Reference count
    uint32_t flag : 2;    ///< Flag used when marking nodes
};

/**
 * Extra terminal node containing an ID.
 */
struct xtermnode_
{
    ddNode* next;         ///< Pointer to next element in hash table
    uint32_t level : 20;  ///< Level of the node
    uint32_t ref : 10;    ///< Reference count
    uint32_t flag : 2;    ///< Flag used when marking nodes
    int32_t id;
};

/**
 * An element is a pair containing a reference to a DD node and a
 * bound.  It is used in the definition of a CDD node.
 */

struct elem_
{
    ddNode* child;  ///< Pointer to a DD node
    raw_t bnd;      ///< Upper bound
};

/**
 * A CDD node. The first fields are identical to that of \c node_.
 * A CDD node has two or more children.
 */
struct cddnode_
{
    ddNode* next;         ///< Pointer to next element in hash table
    uint32_t level : 20;  ///< Level of the node
    uint32_t ref : 10;    ///< Reference count
    uint32_t flag : 2;    ///< Flag used when marking nodes
    Elem elem[];          ///< NULL terminated array of elements
};

/**
 * A BDD node. The first fields are identical to that of \c node_.
 * A BDD node has two children: a low node and a high node.
 */
struct bddnode_
{
    ddNode* next;         ///< Pointer to next element in hash table
    uint32_t level : 20;  ///< Level of the node
    uint32_t ref : 10;    ///< Reference count
    uint32_t flag : 2;    ///< Flag used when marking nodes
    ddNode* low;          ///< Low child node
    ddNode* high;         ///< High child node
};

/**
 * A 64KB chunk of memory. A chunk is allocated by a node manager and
 * is divided into nodes. Chunks are kept on a single linked
 * list. Notice that chunks are always allocated on 64KB boundaries;
 * this makes it easy to find the chunk in which a node is allocated.
 */
struct chunk_
{
    Chunk* next;       ///< Pointer to next chunk
    NodeManager* man;  ///< Pointer to owning node manager
    ddNode* nodes[];   ///< Array of nodes (64Kb - 8 byte)
};

/**
 * A subtable is basically a hash table with some statistical
 * information. A subtable is specific to a node manager and to a node
 * level.
 *
 * The hash table size is always a power of 2. This makes it easy to
 * map a 32-bit hash value to an entry in the table. It also makes
 * resizing much simpler, since each entry in the old table will
 * be placed in one of two entries in the new table.
 */
struct subtable_
{
    int32_t level;    ///< The level
    int32_t deadcnt;  ///< Number of dead nodes
    int32_t keys;     ///< Number of nodes in this sub table
    int32_t maxkeys;  ///< Max number of nodes before resizing occurs
    int32_t shift;    ///< Shift for hash
    int32_t buckets;  ///< Size of hash table
    ddNode** hash;    ///< Hash table
};

/**
 * A node manager maintains node allocation of a particular type/size
 * of node. It allocates memory as a number of chunks, maintains
 * subtables for each node level, maintains a free list of unused
 * nodes, and keeps statistics about used, dead and free nodes (used
 * by the garbage collector). Notice that a dead node is not free: A
 * dead node has been in used, but the reference count is now
 * zero. The node can be garbage collected (and thus become free) or
 * reclaimed as a consequence of a cache hit.
 *
 * @see \c cdd_reclaim()
 */
struct nodemanager_
{
    int32_t nodesize;  ///< Size of node in bytes
    int32_t freecnt;   ///< Number of nodes in free list
    int32_t chunkcnt;  ///< Number of chunks
    int32_t alloccnt;  ///< Number of allocated nodes
    int32_t deadcnt;   ///< Number of dead nodes
    int32_t usedcnt;   ///< Number of used nodes
    int32_t gbccnt;    ///< Number of garbage collection runs on this manager
    int32_t gbcclock;  ///< Time used for garbage collection
    // int32_t gbcwatch;      ///< True if scheduled for garbage collection
    ddNode* free;      ///< Free list
    Chunk* nodes;      ///< Chunk list
    ddNode* sentinel;  ///< "End of list" mark
    NodeHashFunc hashfunc;
    SubTable** subtables;
};

/**
 * Resurect a dead DD. When the reference count of a DD goes to zero,
 * this function can be used to bring it back from the dead. This is
 * possible as long as it has not yet been garbage
 * collected. Reclaiming a node involves incrementing the reference
 * count on all children of the node.  Nodes can be reclaimed as a
 * side effect of a cache hit or when one of the \c cdd_make_
 * functions is called.  Notice that this function does not actually
 * increment the reference count on the given node, only on the
 * children of the node.
 *
 * @param node a dead DD node
 */
void cdd_reclaim(ddNode*);

/**
 * Create BDD node for \a level with \a low and \a high children. The
 * reference counter on the node returned has not been incremented.
 * In case \a low is a negated node reference, this function also
 * takes care to normalise the node. As a consequence the pointer
 * returned might be marked.
 *
 * @param level a level (has to be a boolean variable) @param low low
 * child of new node @param high high child of new node @return new
 * BDD node
 */
ddNode* cdd_make_bdd_node(int32_t level, ddNode* low, ddNode* high);

/**
 * Creates a CDD node.
 * @see cdd_make_bdd_node()
 */
ddNode* cdd_make_cdd_node(int32_t level, Elem* children, int32_t len);

/** @} */

/**
 * Pairing function
 */
#define cdd_pair(a, b) ((uintptr_t)(((a) + (b)) * ((a) + (b) + 1) / 2 + (a)))

/**
 * Uses \c cdd_pair() to 'pair' three integers.
 */
#define cdd_triple(a, b, c) ((uintptr_t)(cdd_pair(cdd_pair(a, b), c)))

#define cdd_info(node) (cdd_levelinfo + cdd_rglr(node)->level)

extern int32_t cdd_errorcond;
extern int32_t* cdd_diff2level;
extern Elem* cdd_refstack;
extern Elem* cdd_refstacktop;  ///< Reference stack
extern size_t cdd_refstacksize;
extern int32_t bdd_start_level;  ///< BDD start level
extern int32_t cdd_clocknum;     ///< Number of clocks
extern int32_t cdd_varnum;       ///< Number of BDD variables
extern int32_t cdd_levelcnt;     ///< Number of levels
extern LevelInfo* cdd_levelinfo;

#define cdd_push(node, bound)            \
    do {                                 \
        cdd_refstacktop->child = (node); \
        cdd_refstacktop->bnd = (bound);  \
        cdd_refstacktop++;               \
    } while (0)

/* From kernel.c */

/**
 * Print error message for the given error.
 * @param error an error code
 * @return \a error
 */
int32_t cdd_error(int32_t error);

extern ddNode* cdd_upper_from_level(int32_t, raw_t);
extern ddNode* cdd_interval_from_level(int32_t, raw_t, raw_t);

/**
 * Initialise operator cache.
 * @param cachsize size of caches.
 * @return 0 if successful, an error code otherwise
 */
int32_t cdd_operator_init(size_t);

/**
 * Releases memory allocated by \c cdd_operator_init().
 */
void cdd_operator_done();

/**
 * Clears all operator caches.
 * @see CddCache_reset()
 */
void cdd_operator_reset();

/**
 * Flushes all operator caches.
 * @see CddCache_flush()
 */
void cdd_operator_flush();

/**
 * @name CDD Iterator
 * @{
 */
typedef struct
{
    raw_t low;
    uintptr_t neg;
    Elem* p;
} cdd_iterator;

#define cdd_it_init(it, node) (it).low = -INF, (it).neg = cdd_mask(node), (it).p = cdd_node(node)->elem
#define cdd_it_lower(it)      ((it).low)
#define cdd_it_child(it)      (cdd_neg_cond((it).p->child, (it).neg))
#define cdd_it_upper(it)      ((it).p->bnd)
#define cdd_it_atend(it)      ((it).low == INF)
#define cdd_it_next(it)       (it).low = cdd_it_upper(it), (it).p++

/** Returns the low child of a BDD node \a node */
#define bdd_low(node) (cdd_neg_cond(bdd_node(node)->low, cdd_mask(node)))

/** Returns the high child of a BDD node \a node */
#define bdd_high(node) (cdd_neg_cond(bdd_node(node)->high, cdd_mask(node)))

/** @} */

#ifdef __cplusplus
}
#endif

/** @} */

#endif
