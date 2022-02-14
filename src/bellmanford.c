// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-
///////////////////////////////////////////////////////////////////////////////
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2004, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: bellmanford.c,v 1.2 2004/11/24 15:42:12 adavid Exp $
//
///////////////////////////////////////////////////////////////////////////////

#include "bellmanford.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

void cdd_bf_init(struct bellmanford* graph, uint32_t dim, struct distance* dist, constraint_t* edges)
{
    assert(dim > 0);
    graph->dim = dim;
    graph->count = 0;
    graph->dist = dist;
    graph->edges = edges;
    memset(graph->dist, 0, dim * sizeof(struct distance));
}

#ifndef NDEBUG
/**
 * Returns true if the graph does not contain the given edge, false
 * otherwise.
 */
static int unique(struct bellmanford* graph, cindex_t i, cindex_t j)
{
    cindex_t idx;
    for (idx = 0; idx < graph->count; idx++) {
        if (graph->edges[idx].i == i && graph->edges[idx].j == j) {
            return 0;
        }
    }
    return 1;
}
#endif

void cdd_bf_push(struct bellmanford* graph, cindex_t i, cindex_t j, raw_t c)
{
    assert(c < dbm_LS_INFINITY);
    assert(unique(graph, i, j));
    assert(i != j);
    int count = graph->count++;
    graph->edges[count].i = i;
    graph->edges[count].j = j;
    graph->edges[count].value = c;
}

void cdd_bf_pop(struct bellmanford* graph)
{
    assert(graph->count > 0);
    graph->count--;
}

/**
 * Sum operator for adding a distance and a raw_t bound.
 */
inline static struct distance add(struct distance i, raw_t e)
{
    i.value += dbm_raw2bound(e);
    i.strictness += dbm_rawIsStrict(e);
    return i;
}

/**
 * Less-than operator for distance structures.
 */
inline static int less(struct distance i, struct distance j)
{
    return (i.value < j.value || (i.value == j.value && i.strictness > j.strictness));
}

/* This is a very naive implementation of Bellman Ford:
 *
 * - We do not use FIFO ordering.
 * - The negative cycle detection is run after the main algorithm (by
 *   assuming an upper bound on the stabilisation time and then
 *   detecting absence of stabilisation). Thus for incosistent graphs
 *   the runtime equals the worst case which is O(nm).
 */
int cdd_bf_consistent(struct bellmanford* graph)
{
    assert(graph->dim > 0);

    constraint_t* edges = graph->edges;
    struct distance* dist = graph->dist;
    uint32_t count = graph->count;
    uint32_t v = graph->dim, e;
    int found;

    /* Compute single-source-shortest path.
     */
    do {
        found = 0;
        for (e = 0; e < count; e++) {
            struct distance sum = add(dist[edges[e].i], edges[e].value);
            if (less(sum, dist[edges[e].j])) {
                dist[edges[e].j] = sum;
                found = 1;
            }
        }
    } while (--v && found);

    /* Detect negative cycles.
     */
    if (found) {
        for (e = 0; e < count; e++) {
            if (less(add(dist[edges[e].i], edges[e].value), dist[edges[e].j])) {
                return 0;
            }
        }
    }
    return 1;
}
