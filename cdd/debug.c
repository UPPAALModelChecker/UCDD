
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "base/intutils.h"
#include "base/bitstring.h"

#include "cdd/kernel.h"
#include "cdd/debug.h"
#include "bellmanford.h"

///////////////////////////////////////////////////////////////////////////

static ddNode* cdd_bf_reduce_rec(ddNode* node, struct bellmanford* graph)
{
    raw_t bnd;
    int mask;
    Elem* top;
    cdd_iterator it;
    ddNode* prev;
    ddNode* n;
    ddNode* res;
    LevelInfo* info;

    /* Termination conditions */
    if (cdd_isterminal(node))
        return node;

    res = NULL;
    info = cdd_info(node);
    switch (info->type) {
    case TYPE_BDD:
        n = cdd_bf_reduce_rec(cdd_neg_cond(bdd_node(node)->low, cdd_mask(node)), graph);
        cdd_ref(n);
        res = cdd_make_bdd_node(
            cdd_rglr(node)->level, n,
            cdd_bf_reduce_rec(cdd_neg_cond(bdd_node(node)->high, cdd_mask(node)), graph));
        cdd_deref(n);
        break;

    case TYPE_CDD:
        /* Find first consistent child. Lower bounds do not matter
         * here since we know any edge to the left of the current one
         * is inconsistent.
         */
        cdd_it_init(it, node);
        cdd_bf_push(graph, info->clock1, info->clock2, cdd_it_upper(it));
        while (!cdd_bf_consistent(graph)) {
            cdd_bf_pop(graph);
            cdd_it_next(it);
            bnd = cdd_it_upper(it);
            if (bnd == dbm_LS_INFINITY) {
                /* We have reached the last child, hence all bounds
                 * except the last are inconsistent, hence this node
                 * has no effect at all. Reduce the last child and
                 * return it directly.
                 */
                return cdd_bf_reduce_rec(cdd_it_child(it), graph);
            }
            cdd_bf_push(graph, info->clock1, info->clock2, bnd);
        }

        /* Do recursion for the first consistent child we found above.
         */
        prev = cdd_bf_reduce_rec(cdd_it_child(it), graph);
        mask = cdd_mask(prev);
        cdd_ref(prev);

        /* Repeat until next inconsistent bound or the last bound.
         */
        top = cdd_refstacktop;
        for (cdd_it_next(it); !cdd_it_atend(it); cdd_it_next(it)) {
            cdd_bf_pop(graph);
            cdd_bf_push(graph, info->clock2, info->clock1, bnd_l2u(cdd_it_lower(it)));
            if (!cdd_bf_consistent(graph)) {
                break;
            }

            bnd = cdd_it_upper(it);
            if (bnd < dbm_LS_INFINITY) {
                cdd_bf_push(graph, info->clock1, info->clock2, bnd);
                n = cdd_bf_reduce_rec(cdd_it_child(it), graph);
                cdd_bf_pop(graph);
            } else {
                n = cdd_bf_reduce_rec(cdd_it_child(it), graph);
            }

            if (prev != n) {
                cdd_push(cdd_neg_cond(prev, mask), cdd_it_lower(it));
                prev = n;
                cdd_ref(prev);
            }
        }
        cdd_bf_pop(graph);
        cdd_push(cdd_neg_cond(prev, mask), INF);

        /* Create node */
        res = cdd_neg_cond(cdd_make_cdd_node(cdd_rglr(node)->level, top, cdd_refstacktop - top),
                           mask);

        /* Remove references */
        while (cdd_refstacktop > top) {
            cdd_refstacktop--;
            cdd_deref(cdd_refstacktop->child);
        }
    }
    return res;
}

ddNode* cdd_bf_reduce(ddNode* node)
{
    struct bellmanford graph;
    struct distance dist[cdd_clocknum];
    constraint_t edges[cdd_clocknum * cdd_clocknum];

    cdd_bf_init(&graph, cdd_clocknum, dist, edges);
    return cdd_bf_reduce_rec(node, &graph);
}
