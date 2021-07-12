// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-
///////////////////////////////////////////////////////////////////////////////
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2004, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: bellmanford.h,v 1.2 2004/11/24 15:42:12 adavid Exp $
//
///////////////////////////////////////////////////////////////////////////////

#ifndef OLD_CDD_BELLMANFORD_H
#define OLD_CDD_BELLMANFORD_H

#include <dbm/constraints.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file graph.h
 *
 * Support for checking consistency of a constraint graph. Uses a
 * variation of the Bellman Ford algorithm.
 */

/**
 * Structure measuring the "distance" between two vertices in a graph.
 *
 * In the Bellman Ford algorithm we cannot use a normal raw_t for
 * this, since the algorithm depends on the fact that it does not
 * stabilise if negative cycles exist. If we use the normal raw_t
 * addition operator, we get into the problem that some incosistent
 * cycles do not trigger this behaviour in the algorithm (this is true
 * for cycles which sum to (<, 0)).
 *
 * For this reason we use a different distance measure: a \a value
 * being the sum of all bounds on a path, and a \a strictness counting
 * the number of strict constraints on a path. With this measure,
 * Bellman Ford stabilizes if and only if there are no incosistent
 * cycles.
 */
struct distance
{
    int32_t value;
    int32_t strictness;
};

/**
 * Structure used to represent data needed for the Bellman Ford
 * algorithm.
 *
 * Besides the actual digraph, it also contains a distance vector \a
 * dist containing distances from a non-existing "virtual" vertice to
 * all other vertices. This vertice does not exist in the digraph (it
 * exists in the augmented graph, but we do not actually represent
 * this graph). The distance vector is reused between runs of the
 * algorithm, thus making the algorithm incremental.
 */
struct bellmanford
{
    uint32_t dim;          /**< Number of vertices in graph. */
    uint32_t count;        /**< Number of edges in graph. */
    struct distance* dist; /**< Distance vector. */
    constraint_t* edges;   /**< Edges in graph. */
};

/**
 * Initialise a new bellmanford structure. The function does not
 * allocate anything. The caller must allocate and provide a distance
 * vector and an edge vector. After initialisation, the digraph does
 * not contain any edges.
 *
 * @param graph pointer to a bellman ford structure.
 * @param dim   dimension of graph.
 * @param dist  a distance vector (expected size is \a dim).
 * @param edges an edge vector (expected size is \a dim * dim).
 */
void cdd_bf_init(struct bellmanford* graph, uint32_t dim, struct distance* dist,
                 constraint_t* edges);

/**
 * Add an edge to \a graph. It is an error to add an edge between vertices
 * which are already connected by an edge (in the same direction).
 */
void cdd_bf_push(struct bellmanford* graph, cindex_t i, cindex_t j, raw_t c);

/**
 * Remove an edge in LIFO order.
 */
void cdd_bf_pop(struct bellmanford* graph);

/**
 * See if the constraint graph is consistent.
 * @return 1 if the constraint graph is consistent, 0 otherwise.
 */
int cdd_bf_consistent(struct bellmanford* graph);

#ifdef __cplusplus
}
#endif

#endif
