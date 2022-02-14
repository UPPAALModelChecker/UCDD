/* -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/*********************************************************************
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * $Id: testcdd.cpp,v 1.12 2005/05/11 19:08:15 adavid Exp $
 *
 *********************************************************************/

/** @file testdbm
 *
 * Test of CDD module.
 */

#ifdef NDEBUG
#undef NDEBUG
#endif

#include <iostream>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include "base/Timer.h"
#include "cdd/cdd.h"
#include "cdd/kernel.h"
#include "cdd/debug.h"
#include "dbm/dbm.h"
#include "dbm/gen.h"
#include "dbm/print.h"
#include "debug/macros.h"

using std::endl;
using std::cerr;
using std::cout;
using base::Timer;

/* Inner repeat loop.
 */
#define LOOP 100

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

typedef void (*TestFunction)(size_t size);

/* test conversion between CDD and DBMs
 */
static void test_conversion(size_t size)
{
    ADBM(dbm1);
    ADBM(dbm2);

    /* Convert to CDD
     */
    DBM_GEN(dbm1);
    cdd cdd1(dbm1, size);

    /* Check conversion
     */
    ASSERT(cdd_contains(cdd1, dbm1, size), dbm_print(stdout, dbm1, size));

    /* Convert to DBM
     */
    cdd cdd2 = cdd_extract_dbm(cdd1, dbm2, size);

    /* Check conversion
     */
    DBM_EQUAL(dbm1, dbm2);
    assert(cdd_reduce(cdd2) == cdd_false());
}

/* test intersection of CDDs
 */
static void test_intersection(size_t size)
{
    cdd cdd1, cdd2, cdd3, cdd4;
    ADBM(dbm1);
    ADBM(dbm2);
    ADBM(dbm3);
    ADBM(dbm4);

    /* Generate DBMs
     */
    DBM_GEN(dbm1);
    DBM_GEN(dbm2);
    dbm_copy(dbm3, dbm2, size);

    /* Generate intersection
     */
    bool empty = !dbm_intersection(dbm3, dbm1, size);

    /* Do the same with CDDs.
     */
    cdd1 = cdd(dbm1, size);
    cdd2 = cdd(dbm2, size);
    cdd3 = cdd1 & cdd2;

    /* Check the result.
     */
    if (!empty) {
        assert(cdd_contains(cdd3, dbm3, size));

        /* Extract DBM.
         */
        cdd3 = cdd_reduce(cdd3);
        cdd4 = cdd_extract_dbm(cdd3, dbm4, size);

        /* Check result.
         */
        DBM_EQUAL(dbm3, dbm4);
    }
}

static double time_apply_and_reduce = 0;
static double time_apply_reduce = 0;

static void test_apply_reduce(size_t size)
{
    /* Generate 8 simple CDDs and then 'or' them together in a pair
     * wise/binary tree fashion.
     */
    cdd cdds[8];
    ADBM(dbm);

    for (uint32_t i = 0; i < 8; i++) {
        DBM_GEN(dbm);
        cdds[i] = cdd(dbm, size);
    }

    for (uint32_t j = 4; j > 0; j /= 2) {
        for (uint32_t i = 0; i < j; i++) {
            cdd a, b, c, e, f;

            a = cdds[2 * i];
            b = cdds[2 * i + 1];

            /* Fake run to ensure that the result has already been
             * created (timing is more fair).
             */
            c = !cdd_apply_reduce(!a, !b, cddop_and);

            /* Run (a|b) last so that the apply_reduce calls do not
             * gain from any cache lookups.
             */
            Timer timer;
            c = !cdd_apply_reduce(!a, !b, cddop_and);
            time_apply_reduce += timer.getElapsed();
            e = a | b;
            cdd_reduce(e);
            time_apply_and_reduce += timer.getElapsed();

            /* Check that c/d are actually reduced.
             */
            assert(c == cdd_reduce(c));

            /* Check that c/d and e describe the same federation.
             */
            assert(cdd_reduce(c ^ e) == cdd_false());

            cdds[i] = c;
        }
    }
}

static double time_reduce = 0;
static double time_bf = 0;

static void test_reduce(size_t size)
{
    cdd cdd1, cdd2, cdd3;
    ADBM(dbm);

    cdd1 = cdd_false();
    for (uint32_t j = 0; j < 5; j++) {
        DBM_GEN(dbm);
        cdd1 |= cdd(dbm, size);
    }

    cdd2 = cdd_reduce(cdd1);
    Timer timer;
    cdd2 = cdd_reduce(cdd1);
    time_reduce += timer.getElapsed();
    cdd3 = cdd(cdd_bf_reduce(cdd1.handle()));
    time_bf += timer.getElapsed();

    assert(cdd2 == cdd3);
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

    cdd_init(100000, 10000, 10000);
    cdd_add_clocks(n);

    for (uint32_t j = 1; j <= 10; ++j) {
        uint32_t DBM_sofar = allDBMs;
        uint32_t good_sofar = goodDBMs;
        uint32_t passDBMs, passGood;
        printf("*** Pass %d of 10 ***\n", j);
        for (uint32_t i = 1; i <= n; ++i) /* min dim = 1 */
        {
            test("test_conversion  ", test_conversion, i);
            test("test_intersection", test_intersection, i);
            test("test_apply_reduce", test_apply_reduce, i);
            test("test_reduce      ", test_reduce, i);
        }
        passDBMs = allDBMs - DBM_sofar;
        passGood = goodDBMs - good_sofar;
        printf("*** Passed(%d) for %d generated DBMs, %d (%d%%) non trivial\n", j, passDBMs,
               passGood, passDBMs ? (100 * passGood) / passDBMs : 0);
    }

    cdd_done();

    assert(n == 0 || allDBMs);
    printf("Total generated DBMs: %d, non trivial ones: %d (%d%%)\n", allDBMs, goodDBMs,
           allDBMs ? (100 * goodDBMs) / allDBMs : 0);
    printf("apply+reduce: %.3fs, apply_reduce: %.3fs\n", time_apply_and_reduce, time_apply_reduce);
    printf("reduce: %.3fs, bf_reduce: %.3fs\n", time_reduce, time_bf);

    printf("Passed\n");
}
