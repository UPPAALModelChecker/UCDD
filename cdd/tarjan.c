// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-
///////////////////////////////////////////////////////////////////////////////
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2004, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: tarjan.c,v 1.5 2004/11/24 15:42:12 adavid Exp $
//
///////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "base/intutils.h"
#include "base/bitstring.h"
#include "tarjan.h"

/**
 * Insert \a element after \a pos in linked list.
 *
 * @param list    array containing linked list.
 * @param pos     position after which to insert \a element.
 * @param element element to insert.
 */
inline static void insert(struct node* list, uint32_t pos, uint32_t element)
{
    uint32_t succ = list[pos].next;
    list[succ].prev = element;
    list[element].next = succ;
    list[element].prev = pos;
    list[pos].next = element;
}

/**
 * Remove \a pos from linked list.
 * @param list  array containing linked list.
 * @param pos   element to remove.
 */
inline static void remove(struct node* list, uint32_t pos)
{
    uint32_t next = list[pos].next;
    uint32_t prev = list[pos].prev;
    list[prev].next = next;
    list[next].prev = prev;
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

#ifndef NDEBUG
/**
 * Returns true if the graph does not contain the given edge, false
 * otherwise.
 */
static int unique(struct tarjan* graph, cindex_t i, cindex_t j)
{
    cindex_t idx;
    for (idx = 0; idx < graph->count[i]; idx++) {
        if (graph->edges[i * graph->dim - i + idx].v == j) {
            return 0;
        }
    }
    return 1;
}
#endif

void cdd_tarjan_init(struct tarjan* graph, uint32_t dim, struct distance* dist, uint32_t* count,
                     struct edge* edges, struct node* fifo, uint32_t* queued)
{
    assert(dim > 0);
    graph->dim = dim;
    graph->count = count;
    graph->dist = dist;
    graph->edges = edges;
    graph->fifo = fifo;
    graph->queued = queued;
    graph->fifo[dim].prev = graph->fifo[dim].next = dim;
    base_resetBits(queued, bits2intsize(dim));
    memset(graph->count, 0, dim * sizeof(uint32_t));
    memset(graph->dist, 0, dim * sizeof(struct distance));
}

void cdd_tarjan_push(struct tarjan* graph, cindex_t i, cindex_t j, raw_t value)
{
    assert(value < dbm_LS_INFINITY);
    assert(unique(graph, i, j));
    assert(i != j);

    /* Add edge from i to j.
     */
    uint32_t count = graph->count[i]++;
    uint32_t idx = i * graph->dim - i + count;
    graph->edges[idx].v = j;
    graph->edges[idx].value = value;

    /* Add i to queue if it is not already in the queue and
     * following the new edge improves the best known distance
     * to j.
     */
    if (!base_readOneBit(graph->queued, i) && less(add(graph->dist[i], value), graph->dist[j])) {
        base_setOneBit(graph->queued, i);
        insert(graph->fifo, graph->fifo[graph->dim].prev, i);
    }
}

void cdd_tarjan_pop(struct tarjan* graph, cindex_t i)
{
    assert(graph->count[i] > 0);
    graph->count[i]--;
}

/**
 * Traverse and disassemble a subtree rooted at \a root. If \a node is
 * not found in the tree, the tree is disassembled and 0 is
 * returned. Disassembling the tree means that all nodes on the
 * subtree are removed from the tree and from the queue.  If \a node
 * is found to be in the subtree, 1 is returned and the tree is left
 * in an inconsistent state.
 *
 * @param root     index of the root of the tree to disassemble.
 * @param node     index of the node to search for in the tree.
 * @param terminal index of the terminal node.
 * @param preorder preorder linked list of tree nodes.
 * @param depth    array containing the depth of a node in the tree.
 * @param queue    linked list of processing queue.
 * @param queued   booleans indicating whether a vertice is in \a queue.
 */
static int disassemble(uint32_t root, uint32_t node, uint32_t terminal, struct node* preorder,
                       uint32_t* depth, struct node* queue, uint32_t* queued)
{
    assert(root != node);
    uint32_t root_depth, current, tmp;

    /* Traverse in preorder and disassemble tree rooted at root.
     */
    root_depth = depth[root];
    current = preorder[root].next;
    while (depth[current] > root_depth) {
        if (current == node) {
            return 1;
        }

        /* Otherwise continue and remove current from queue if queued.
         */
        if (base_readOneBit(queued, current)) {
            base_toggleOneBit(queued, current);
            remove(queue, current);
        }

        /* Disassemble tree.
         */
        tmp = preorder[current].next;
        preorder[current].prev = preorder[current].next = terminal;
        depth[current] = 0;
        current = tmp;
    }

    /* Make current the next node after root.
     */
    preorder[root].next = current;
    preorder[current].prev = root;

    return 0;
}

/**
 * Make \a child a child of \a parent. The child must not
 * have children itself.
 *
 * @param parent    the node to which to add a child.
 * @param child     the node to add.
 * @param preorder  linked list of tree nodes sorted in preorder.
 * @param depth     array containing the depth of a node in the tree.
 */
static void link(uint32_t parent, uint32_t child, struct node* preorder, uint32_t* depth)
{
    /* Assert that the child does not itself have children.
     */
    assert(depth[preorder[child].next] <= depth[child]);

    /* Unlink child from its old tree.
     */
    remove(preorder, child);

    /* Insert child after parent in preorder.
     */
    depth[child] = depth[parent] + 1;
    insert(preorder, parent, child);
}

/**
 * Populate the queue with vertices to scan. Iterates over all edges
 * and adds the source of those that can improve the known distance.
 */
static void populateQueue(struct tarjan* graph)
{
    uint32_t u;
    uint32_t dim = graph->dim;
    struct distance* dist = graph->dist;
    struct node* fifo = graph->fifo;
    base_resetBits(graph->queued, bits2intsize(dim));
    fifo[dim].prev = fifo[dim].next = dim;
    for (u = 0; u < dim; u++) {
        struct edge* edge = graph->edges + u * dim - u;
        struct edge* last = edge + graph->count[u];
        while (edge < last) {
            if (less(add(dist[u], edge->value), dist[edge->v])) {
                insert(fifo, fifo[dim].prev, u);
                base_setOneBit(graph->queued, u);
                break;
            }
            edge++;
        }
    }
}

/* This is an implementation of Tarjan's algorithm (Bellman Ford with
 * FIFO order and subtree disassembly).
 */
int cdd_tarjan_consistent(struct tarjan* graph)
{
    assert(graph->dim > 0);

    /* For convenience we create local variables for some of the
     * graph elements.
     */
    struct distance* dist = graph->dist;
    uint32_t* count = graph->count;
    uint32_t dim = graph->dim;

    /* Variables for source, destination, and index.
     */
    uint32_t v, u, i;

    /* Spanning tree holding shortest paths. The last element is a
     * termination element.
     */
    uint32_t depth[dim + 1];
    struct node preorder[dim + 1];

    /* FIFO queue for vertices to scan.  The FIFO queue is
     * represented as a linked list of vertices encoded in an
     * array. The last element of the array is placed before the
     * first/after the last element. Thus the first element of the
     * queue is fifo[dim].next and the last element is fifo[dim].prev.
     */
    struct node* fifo = graph->fifo;
    uint32_t* queued = graph->queued;

    /* Initialise spanning tree: Make every vertex a little tree
     * containing only itself.
     */
    for (i = 0; i <= dim; i++) {
        depth[i] = 0;
        preorder[i].prev = preorder[i].next = dim;
    }

    /* While the queue is not empty, dequeue the first vertice and
     * scan it.
     */
    for (u = fifo[dim].next; u != dim; u = fifo[dim].next) {
        /* Remove u from the queue.
         */
        assert(base_readOneBit(queued, u));
        base_toggleOneBit(queued, u);
        remove(fifo, u);

        struct edge* edge = graph->edges + u * dim - u;
        struct edge* last = edge + count[u];
        while (edge < last) {
            /* We do not expect self-loops.
             */
            assert(u != edge->v);

            /* At this point u is the source and v becomes the
             * destination of the edge.
             */
            v = edge->v;

            /* What is the cost of going to u and following
             * edge to v?
             */
            struct distance sum = add(dist[u], edge->value);

            /* Is this cheaper than the previously known path to v?
             */
            if (less(sum, dist[v])) {
                /* Yes it was. Update distance to v.
                 */
                dist[v] = sum;

                /* See if u is a child of v by traversing the tree
                 * rooted at v. If so, return immediately.
                 */
                if (disassemble(v, u, dim, preorder, depth, fifo, queued)) {
                    /* Disassembly has removed elements from the queue
                     * which should not have been removed. Repopulate
                     * the queue.
                     */
                    populateQueue(graph);
                    return 0;
                }

                /* Make v a child of u.
                 */
                link(u, v, preorder, depth);

                /* Add v to the queue if not already there.
                 */
                if (!base_readOneBit(queued, v)) {
                    base_setOneBit(queued, v);
                    insert(fifo, fifo[dim].prev, v);
                }
            }
            ++edge;
        }
    }

    return 1;
}
