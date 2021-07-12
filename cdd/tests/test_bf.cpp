/* -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/*********************************************************************
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * $Id: testbf.cpp,v 1.5 2005/05/11 19:08:15 adavid Exp $
 *
 *********************************************************************/

/** @file testdbm
 *
 * Test of graph algorithms.
 */

#ifdef NDEBUG
#undef NDEBUG
#endif

#include <iostream>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include "base/Timer.h"
#include "base/bitstring.h"
#include "dbm/dbm.h"
#include "dbm/print.h"
#include "dbm/gen.h"
#include "debug/macros.h"
#include "../bellmanford.h"
#include "../tarjan.h"

using std::endl;
using std::cerr;
using std::cout;
using base::Timer;

/* Inner repeat loop.
 */
#define LOOP 1000

/* Allocation of DBM, vector, bitstring
 */
#define ADBM(NAME)  raw_t NAME[size * size];
#define AVECT(NAME) int32_t NAME[size];
#define ABITS(NAME) uint32_t NAME[bits2intsize(size)];

/* Random range
 * may change definition
 */
#define RANGE() ((rand() % 10000) + 1)

/* Print the difference between two DBMs two stdout.
 */
#define DIFF(D1, D2) dbm_printDiff(stdout, D1, D2, size)

/* Assert that two DBMs are equal. If not, prints the difference
 * between the two DBMs are exits the program.
 */
#define DBM_EQUAL(D1, D2) ASSERT(dbm_areEqual(D1, D2, size), DIFF(D1, D2))

/* Show progress
 */
#define PROGRESS() debug_spin(stderr)

/* Generation of DBMs: track non trivial ones
 * and count them all.
 */
#define DBM_GEN(D)                                  \
    do {                                            \
        bool good = dbm_generate(D, size, RANGE()); \
        allDBMs++;                                  \
        goodDBMs += good;                           \
    } while (0)

uint32_t allDBMs = 0;
uint32_t goodDBMs = 0;
uint32_t emptyDBMs = 0;

double time_bf = 0;
double time_tarjan = 0;

typedef void (*TestFunction)(size_t size);

static int bf(size_t size, raw_t* dbm)
{
    int result;
    struct bellmanford graph;
    struct distance dist[size];
    constraint_t edges[size * size];

    cdd_bf_init(&graph, size, dist, edges);
    for (size_t k = 0; k < size; k++) {
        for (size_t l = 0; l < size; l++) {
            if (k != l) {
                raw_t bound = dbm[k * size + l];
                if (bound < dbm_LS_INFINITY) {
                    cdd_bf_push(&graph, k, l, bound);
                }
            }
        }
    }

    Timer timer;
    result = cdd_bf_consistent(&graph);
    time_bf += timer.getElapsed();
    return result;
}

static int tarjan(size_t size, raw_t* dbm)
{
    int result;
    struct tarjan graph;
    struct distance dist[size];
    struct edge edges[size * size - size];
    struct node fifo[size + 1];
    uint32_t queued[bits2intsize(size)];
    uint32_t count[size];

    cdd_tarjan_init(&graph, size, dist, count, edges, fifo, queued);
    for (size_t k = 0; k < size; k++) {
        for (size_t l = 0; l < size; l++) {
            if (k != l) {
                raw_t bound = dbm[k * size + l];
                if (bound < dbm_LS_INFINITY) {
                    cdd_tarjan_push(&graph, k, l, bound);
                }
            }
        }
    }

    Timer timer;
    result = cdd_tarjan_consistent(&graph);
    time_tarjan += timer.getElapsed();
    return result;
}

static void test_shortestpath(size_t size)
{
    ADBM(dbm1);
    ADBM(dbm2);

    DBM_GEN(dbm1);
    DBM_GEN(dbm2);

    bool isEmpty = !dbm_intersection(dbm1, dbm2, size);
    assert(dbm_isEmpty(dbm1, size) == isEmpty);

    ASSERT(bf(size, dbm1) != isEmpty, dbm_print(stdout, dbm1, size));
    ASSERT(tarjan(size, dbm1) != isEmpty, dbm_print(stdout, dbm1, size));

    emptyDBMs += isEmpty;
}

static void test(const char* name, TestFunction f, size_t size)
{
    cout << name << " size = " << size << endl;
    for (uint32_t k = 0; k < LOOP; ++k) {
        PROGRESS();
        f(size);
    }
}

int main(int argc, char* argv[])
{
    uint32_t n;
    uint32_t seed;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s size [seed]\n", argv[0]);
        exit(1);
    }

    n = atoi(argv[1]);
    seed = argc > 2 ? atoi(argv[2]) : time(NULL);
    printf("Testing with seed=%d\n", seed);
    srand(seed);

    for (uint32_t j = 1; j <= 10; ++j) {
        uint32_t DBM_sofar = allDBMs;
        uint32_t good_sofar = goodDBMs;
        uint32_t empty_sofar = emptyDBMs;
        uint32_t passDBMs, passGood, passEmpty;
        printf("*** Pass %d of 10 ***\n", j);
        for (uint32_t i = 1; i <= n; ++i) /* min dim = 1 */
        {
            test("test_shortestpath", test_shortestpath, i);
        }
        passDBMs = allDBMs - DBM_sofar;
        passGood = goodDBMs - good_sofar;
        passEmpty = emptyDBMs - empty_sofar;

        printf("*** Passed %d generated DBMs, %d (%d%%) non trivial, %d non-empty\n", passDBMs,
               passGood, passDBMs ? (100 * passGood) / passDBMs : 0, passDBMs - passEmpty);
    }

    assert(n == 0 || allDBMs);
    printf("Total generated DBMs: %d, non trivial: %d (%d%%), empty: %d\n", allDBMs, goodDBMs,
           allDBMs ? (100 * goodDBMs) / allDBMs : 0, emptyDBMs);
    printf("Bellman Ford: %.3fs, Tarjan: %.3fs\n", time_bf, time_tarjan);

    printf("Passed\n");
}
