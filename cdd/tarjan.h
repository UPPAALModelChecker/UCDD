// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-
///////////////////////////////////////////////////////////////////////////////
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2004, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: tarjan.h,v 1.3 2004/11/24 15:42:12 adavid Exp $
//
///////////////////////////////////////////////////////////////////////////////

#ifndef OLD_CDD_TARJAN_H
#define OLD_CDD_TARJAN_H

#include <dbm/constraints.h>
#include "bellmanford.h"

#ifdef __cplusplus
extern "C" {
#endif

struct edge
{
    cindex_t v;
    raw_t value;
};

/**
 * Structure for array implementation of double linked lists.
 */
struct node
{
    uint32_t next;
    uint32_t prev;
};

/**
 * Structure used to represent data needed for Tarjan's algorithm.
 *
 * Besides the actual digraph, it also contains a distance vector \a
 * dist containing distances from a non-existing "virtual" vertice to
 * all other vertices. This vertice does not exist in the digraph (it
 * exists in the augmented graph, but we do not actually represent
 * this graph). The distance vector is reused between runs of the
 * algorithm, thus making the algorithm incremental.
 */
struct tarjan
{
    uint32_t dim;          /**< Number of vertices in graph. */
    uint32_t* count;       /**< Number of outgoing edges. */
    struct distance* dist; /**< Distance vector. */
    struct edge* edges;    /**< Edges in graph. */
    struct node* fifo;
    uint32_t* queued;
};

/**
 * Initialise a new tarjan structure. The function does not allocate
 * anything. The caller must allocate and provide a number of arrays
 * which is used by Tarjan's algorithm. After initialisation, the
 * digraph does not contain any edges.
 *
 * @param graph  the unitialised tarjan structure.
 * @param dim    number of vertices in graph.
 * @param dist   array of size \a dim.
 * @param count  array of size \a dim.
 * @param edges  array of size \a dim * (dim - 1).
 */
void cdd_tarjan_init(struct tarjan* graph, uint32_t dim, struct distance* dist, uint32_t* count,
                     struct edge* edges, struct node* fifo, uint32_t* queued);

/**
 * Add an edge to \a graph. It is an error to add an edge between vertices
 * which are already connected by an edge (in the same direction).
 */
void cdd_tarjan_push(struct tarjan* graph, cindex_t i, cindex_t j, raw_t c);

/**
 * Remove the last outgoing edge added.
 *
 * @param graph the graph.
 * @param i     a vertice from which to remove an outgoing edge.
 */
void cdd_tarjan_pop(struct tarjan* graph, cindex_t i);

/**
 * See if the constraint graph is consistent.
 * @return 1 if the constraint graph is consistent, 0 otherwise.
 */
int cdd_tarjan_consistent(struct tarjan* graph);

#ifdef __cplusplus
}
#endif

#endif
