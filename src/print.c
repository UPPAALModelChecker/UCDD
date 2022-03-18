// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-
///////////////////////////////////////////////////////////////////////////////
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2004, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: print.c,v 1.2 2004/04/02 22:55:42 behrmann Exp $
//
///////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include "cdd/kernel.h"
#include "base/bitstring.h"

typedef struct conditionList_ conditionList;
typedef struct infor_ infor;
typedef enum child_ child_t;

enum child_ { HIGH, LOW };

struct conditionList_
{
    int level;
    child_t ch;  // currently explored child
    conditionList* next;
};

struct infor_
{
    ddNode* current;
    ddNode* other;
    uint32_t* mask;
    uint32_t* value;
    bool stringFound;
};

/* Print an interval.
 */
static void printInterval(FILE* ofile, raw_t lower, raw_t upper)
{
    if (lower == -dbm_LS_INFINITY) {
        fprintf(ofile, "]-INF;");
    } else {
        lower = bnd_l2u(lower);
        fprintf(ofile, "%s%d;", dbm_rawIsStrict(lower) ? "]" : "[", -dbm_raw2bound(lower));
    }

    if (upper == dbm_LS_INFINITY) {
        fprintf(ofile, "INF[");
    } else {
        fprintf(ofile, "%d%s", dbm_raw2bound(upper), dbm_rawIsStrict(upper) ? "[" : "]");
    }
}


/* Define a dynamic array of strings and its functions, to store pointers of nodes we have already visited
 */
typedef struct {

    size_t used;
    size_t size;
    char ( *array )[40];
} Array;


int initArray( Array *a, size_t initialSize)
{
    a->used = 0;

    a->array = malloc( initialSize * sizeof( char[40] ) );
    int success = a->array != NULL;

    if ( success )
    {
        a->size = initialSize;
    }
    else
    {
        a->size = 0;
    }

    return success;
}

int insertArray( Array *a, const char *element )
{
    int success = 1;

    if ( a->used == a->size )
    {
        char ( *tmp )[40] = realloc( a->array, 2 * a->size * sizeof( char[40] ) );
        if ( ( success = tmp != NULL ) )
        {
            a->array = tmp;
            a->size *= 2;
        }
    }

    if ( success )
    {
        strcpy( a->array[a->used++], element );
    }

    return success;
}

bool contains(Array *a, const char *element )
{
    //printf("%i",a->used);
    for (int i=0; i < a->used;i++)
    {
        //printf("%s : %s -> %i \n    ", a->array[i], element, strcmp(a->array[i], element));
        if (strcmp(a->array[i], element)==0)
            return true;
    }
    return false;
}

void freeArray(Array *a) {
    free(a->array);
    a->array = NULL;
    a->used = a->size = 0;
}


/* Recursive function to print the nodes and edges of a CDD.  A side
 * effect of this function is that all nodes will be marked. All
 * previously marked nodes are ignored.
 */
static void cdd_fprintdot_rec(FILE* ofile, ddNode* r, bool flip_negated, bool negated, Array *a)
{
    // we decided against regularizing because we need to know the bit to print the negation correctly
    //assert(cdd_rglr(r) == r);

    if (cdd_isterminal(r)) {
        return;
    }

    // this check was moved inside CDD part
    /*if (cdd_ismarked(r)) {
        printf("print.c: a marked node has been reached during printing\n");
        return;
    }*/

    // establish color for printing the node
    char *node_color = "black";
    if (cdd_is_negated(r))
        node_color = "red";

    if (cdd_info(r)->type == TYPE_BDD) {
        bddNode* node = bdd_node(r);

        // printf("bdd node");
        // we annotate each location in the dot file with a 0 if it was reached with an even number of negations,
        // and with a 1 if it was reached with an odd number of negations.
        char *current_neg_appendix = "0";
        char *high_neg_appendix = "0";
        char *low_neg_appendix = "0";

        // we do not care about whether the current node is negated, only about whether it was reached via a negation
        if (negated) current_neg_appendix = "1";
        // to see if its children are reached via negation, we do need to take the current negation into account
        if (cdd_is_negated(r) ^ negated)
        {
            high_neg_appendix = "1";
            low_neg_appendix = "1";
        }

        // terminal children nodes don't need the annotation
        if (cdd_isterminal((void *) node->high))
            high_neg_appendix="";
        if (cdd_isterminal((void *) node->low))
            low_neg_appendix="";


        //we chech whether we already reached this node via an array of strings keeping track of the pointers plus appendix
        char buf[40] = {0};
        snprintf(buf, sizeof buf, "%p%s\n", (void *) r, current_neg_appendix);


        if (!contains(a,buf)) {
            insertArray(a, buf);

            // print current node
            fprintf(ofile, "\"%p%s\" [shape=circle, color = %s, label=\"b%d\"];\n", (void *) r, current_neg_appendix,
                    node_color, node->level);

            // print arrow to high
            if (flip_negated && (negated ^ cdd_is_negated(r)) && cdd_isterminal((void *) node->high)) {
                // flip arrow to the negated terminal if we had negation
                fprintf(ofile, "\"%p%s\" -> \"%p\" [style=\"filled", (void *) r, current_neg_appendix,
                        cdd_neg((void *) node->high));
                fprintf(ofile, "\"];\n");
            } else {
                // print normal arrows with annotation for children
                fprintf(ofile, "\"%p%s\" -> \"%p%s\" [style=\"filled", (void *) r, current_neg_appendix,
                        (void *) node->high, high_neg_appendix);
                fprintf(ofile, "\"];\n");
            }
            // print arrow to low
            if (flip_negated && (negated ^ cdd_is_negated(r)) && cdd_isterminal((void *) node->low)) {
                // flip arrow to the negated terminal if we had negation
                fprintf(ofile, "\"%p%s\" -> \"%p\" [style=\"dashed", (void *) r, current_neg_appendix,
                        cdd_neg((void *) node->low));
                fprintf(ofile, "\"];\n");
            } else {
                // print normal arrows with annotation for children
                fprintf(ofile, "\"%p%s\" -> \"%p%s\" [style=\"dashed", (void *) r, current_neg_appendix,
                        (void *) node->low, low_neg_appendix);
                fprintf(ofile, "\"];\n");
            }

            cdd_fprintdot_rec(ofile, node->high, flip_negated, negated ^ cdd_is_negated(r), a);
            cdd_fprintdot_rec(ofile, node->low, flip_negated, negated ^ cdd_is_negated(r), a);
        }
    } else {

        // regularizing before checking whether a node is marked
        if (cdd_ismarked(cdd_rglr(r))) {
            //printf("print.c: a marked node has been reached during printing\n");
            return;
        }

        //printf("cdd node");
        raw_t bnd = -INF;
        cddNode *node = cdd_node(r);
        Elem *p = node->elem;

        char *current_neg_appendix = "0";
        char *child_neg_appendix = "0";

        // we do not care about whether the current node is negated, only about whether it was reached via a negation
        if (negated) current_neg_appendix = "1";
        // to see if its children are reached via negation, we do need to take the current one into account
        if (cdd_is_negated(r) ^ negated) {
            child_neg_appendix = "1";
        }

        // terminal children nodes don't need the annotation

        fprintf(ofile, "\"%p%s\" [shape=octagon, color = %s, label=\"x%d-x%d\"];\n", (void*)r, current_neg_appendix, node_color, cdd_info(node)->clock1,
                cdd_info(node)->clock2);

        do {
            ddNode* child = p->child;
            if (child != cddfalse) {
                if (child == cddfalse | child == cddtrue)
                {
                    child_neg_appendix = "";
                }

                fprintf(ofile, "\"%p%s\" -> \"%p%s\" [style=%s, label=\"", (void*)r, current_neg_appendix,
                        (void*)(child), child_neg_appendix, cdd_mask(child) ? "dashed" : "filled");
                printInterval(ofile, bnd, p->bnd);
                fprintf(ofile, "\"];\n");

                // TODO: here originally, the child node was regularized.. should I have kept that??
                cdd_fprintdot_rec(ofile, child, flip_negated, negated ^ cdd_is_negated(r),a);
            }
            bnd = p->bnd;
            p++;
        } while (bnd < INF);
    }

    cdd_setmark(r);
}
void cdd_print_terminal_node(FILE* ofile, ddNode* r, int label)
{
    fprintf(ofile,
            "\"%p\" [shape=box, label=\"%d\", style=filled, height=0.3, width=0.3];\n",
            (void *) r, label);
}


// main print function called from outside
void cdd_fprintdot(FILE* ofile, ddNode* r, bool push_negate)
{
    Array a;
    initArray(&a, 100);

    fprintf(ofile, "digraph G {\n");
    bool bit = cdd_is_negated(r);
    if (cdd_isterminal(r))
    {
        cdd_print_terminal_node(ofile, r, bit);
    } else {

        cdd_print_terminal_node(ofile,cddtrue, 1);
        cdd_print_terminal_node(ofile,cddfalse, 0);
        if (push_negate)
            cdd_fprintdot_rec(ofile, r, true, false,&a);
        else
            cdd_fprintdot_rec(ofile, r, false, false,&a );
    }
    fprintf(ofile, "}\n");
    cdd_unmark(r);

    freeArray(&a);
}

void cdd_printdot(ddNode* r, bool push_negate) { cdd_fprintdot(stdout, r, push_negate); }

/******** TIGA related extensions. *********/

static void print_node2label(FILE* ofile, ddNode* node)
{
    if (node == cddfalse) {
        fprintf(ofile, "_error");
    }
#ifdef MULTI_TERMINAL
    else if (cdd_is_extra_terminal(node)) {
        fprintf(ofile, "_action%d", cdd_get_tautology_id(node));
    }
#endif
    else {
        fprintf(ofile, "_%p", (void*)node);
    }
}

static void cdd_freduce_dump_rec(FILE* ofile, int maskSize, ddNode* r, infor* parentInfo,
                                 cdd_print_varloc_f labelPrinter,
                                 cdd_print_clockdiff_f clockPrinter, void* data, int dotFormat)
{
    // We mark only the states which are printed (ie not the ones which are reduced)
    // Maybe a problem when calling cdd_unmark ? ("Does not recurse into a node which is not
    // marked.")
    LevelInfo* info;
    info = cdd_info(r);

    // IF MASK SIZE IS TOO SHORT, KBOOM
    assert(maskSize * 32 >= cdd_get_level_count());

    /* Termination condition */

#ifdef MULTI_TERMINAL
    if (cdd_is_extra_terminal(r) && !cdd_ismarked(r)) {
        if (dotFormat) {
            /* Need to declare the node. */
            fprintf(ofile, "\"%p\" [label=\"_action%d\"];\n", (void*)r, cdd_get_tautology_id(r));
        }
        cdd_setmark(r);
        return;
    }
#endif

    if (cdd_is_tfterminal(r) || cdd_ismarked(r))
        return;

    if (info->type != TYPE_BDD) {
        cddNode* node = cdd_node(r);
        Elem* p;
        raw_t bnd;
        int ifstatement = 0;
        p = node->elem;
        bnd = -INF;
        const LevelInfo* levinf = cdd_get_levelinfo(node->level);

        if (dotFormat) {
            fprintf(ofile, "\"%p\" [label=\"", (void*)node);
            clockPrinter(ofile, levinf->clock1, levinf->clock2, data);
            fprintf(ofile, "\"];\n");
        }

        do {
            ddNode* child = p->child;
            if (child != cddfalse) {
                cdd_freduce_dump_rec(ofile, maskSize, cdd_rglr(child), NULL, labelPrinter,
                                     clockPrinter, data, dotFormat);
                if (dotFormat) {
                    fprintf(ofile, "\"%p\" -> \"%p\" [style=%s, label=\"", (void*)node,
                            (void*)cdd_rglr(child), cdd_mask(child) ? "dashed" : "filled");
                    printInterval(ofile, bnd, p->bnd);
                    fprintf(ofile, "\"];\n");
                } else {
                    raw_t lower = bnd_l2u(bnd);
                    fprintf(ofile, "_%p : if (", (void*)node);
                    ifstatement = 1;
                    clockPrinter(ofile, levinf->clock1, levinf->clock2, data);
                    fprintf(ofile, "%s%d", dbm_rawIsWeak(lower) ? ">=" : ">",
                            -dbm_raw2bound(lower));
                    if (p->bnd != dbm_LS_INFINITY) {
                        fprintf(ofile, " && ");
                        clockPrinter(ofile, levinf->clock1, levinf->clock2, data);
                        fprintf(ofile, "%s%d", dbm_rawIsWeak(p->bnd) ? "<=" : "<",
                                dbm_raw2bound(p->bnd));
                    }
                    fprintf(ofile, ") goto ");
                    print_node2label(ofile, cdd_rglr(child));
                    fprintf(ofile, ";\n");
                }
            }
            bnd = p->bnd;
            p++;
        } while (bnd < INF);

        if (ifstatement) {
            fprintf(ofile, "else goto _error;\n");
        }

        cdd_setmark(r);
    } else {
        bddNode* node = bdd_node(r);
        if (parentInfo == NULL ||
            (parentInfo->other != node->high && parentInfo->other != node->low)) {
            // No possible reduction, start exploration by low child
            infor myInfo;
            myInfo.current = node->low;
            myInfo.other = node->high;
            myInfo.mask = malloc(maskSize * sizeof(uint32_t));
            myInfo.value = malloc(maskSize * sizeof(uint32_t));
            int k;
            for (k = 0; k < maskSize; ++k) {
                myInfo.mask[k] = 0;
                myInfo.value[k] = 0;
            }
            base_setOneBit(myInfo.mask, node->level);
            myInfo.stringFound = false;
            cdd_freduce_dump_rec(ofile, maskSize, node->low, &myInfo, labelPrinter, clockPrinter,
                                 data, dotFormat);

            // Exploration of high child
            if (myInfo.stringFound) {
                // Do not search for any string in high child
                cdd_freduce_dump_rec(ofile, maskSize, node->high, NULL, labelPrinter, clockPrinter,
                                     data, dotFormat);
            } else {
                assert(*(myInfo.value) == 0);
                myInfo.current = node->high;
                myInfo.other = node->low;
                base_setOneBit(myInfo.value, node->level);
                cdd_freduce_dump_rec(ofile, maskSize, node->high, &myInfo, labelPrinter,
                                     clockPrinter, data, dotFormat);
            }

            // Print node, mask, value, and children
            if (dotFormat) {
                /*
                fprintf(ofile, "\"%x\" [label=\"", (int)node);
                labelPrinter(ofile, myInfo.mask, myInfo.value, data, 32*maskSize);
                fprintf(ofile, "\"];\n");
                */
                fprintf(ofile, "\"%p\" [label=\"_%p\"];\n", (void*)node, (void*)node);

                if (myInfo.current != cddfalse) {
                    fprintf(ofile, "\"%p\" -> \"%p\" [style=filled];\n", (void*)node,
                            (void*)myInfo.current);
                }
                if (myInfo.other != cddfalse) {
                    fprintf(ofile, "\"%p\" -> \"%p\" [style=dashed];\n", (void*)node,
                            (void*)myInfo.other);
                }
            } else {
                fprintf(ofile, "_%p: if ", (void*)node);
                labelPrinter(ofile, myInfo.mask, myInfo.value, data, 32 * maskSize);
                fprintf(ofile, " goto ");
                print_node2label(ofile, myInfo.current);
                fprintf(ofile, "; else goto ");
                print_node2label(ofile, myInfo.other);
                fprintf(ofile, ";\n");
            }

            free(myInfo.mask);
            free(myInfo.value);
            cdd_setmark(r);
        } else {
            parentInfo->stringFound = true;
            base_setOneBit(parentInfo->mask, node->level);
            if (parentInfo->other == node->high) {
                parentInfo->current = node->low;
                cdd_freduce_dump_rec(ofile, maskSize, node->low, parentInfo, labelPrinter,
                                     clockPrinter, data, dotFormat);
                cdd_freduce_dump_rec(ofile, maskSize, node->high, NULL, labelPrinter, clockPrinter,
                                     data, dotFormat);
            } else {
                assert(parentInfo->other == node->low);
                parentInfo->current = node->high;
                base_setOneBit(parentInfo->value, node->level);
                cdd_freduce_dump_rec(ofile, maskSize, node->high, parentInfo, labelPrinter,
                                     clockPrinter, data, dotFormat);
                cdd_freduce_dump_rec(ofile, maskSize, node->low, NULL, labelPrinter, clockPrinter,
                                     data, dotFormat);
            }
        }
    }
}

void cdd_fprint_code(FILE* ofile, ddNode* r, cdd_print_varloc_f labelPrinter,
                     cdd_print_clockdiff_f clockPrinter, void* data)
{
    assert(labelPrinter);
    assert(clockPrinter);

    fprintf(ofile, "goto _%p;\n", (void*)r);

    cdd_freduce_dump_rec(ofile,
                         cdd_get_level_count() / 32 + 1,  // mask size
                         cdd_rglr(r), NULL, labelPrinter, clockPrinter, data, 0);
    cdd_force_unmark(r);
}

void cdd_fprint_graph(FILE* ofile, ddNode* r, cdd_print_varloc_f labelPrinter,
                      cdd_print_clockdiff_f clockPrinter, void* data)
{
    assert(labelPrinter);
    assert(clockPrinter);

    fprintf(ofile, "digraph G {\n");
    // fprintf(ofile, "\"%x\" [shape=box, label=\"0\", style=filled, shape=box, height=0.3,
    // width=0.3];\n", (int)cddfalse);

    cdd_freduce_dump_rec(ofile,
                         cdd_get_level_count() / 32 + 1,  // mask size
                         cdd_rglr(r), NULL, labelPrinter, clockPrinter, data, 1);
    cdd_force_unmark(r);

    fprintf(ofile, "}\n");
}
