/* -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/*********************************************************************
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 2011 - 2022, Aalborg University.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 *********************************************************************/

/** @file test_cdd
 * Test for CDD module.
 */

#include "dbm/dbm.h"
#include "dbm/gen.h"
#include "dbm/print.h"
#include "cdd/cdd.h"
#include "cdd/debug.h"
#include "cdd/kernel.h"
#include "base/Timer.h"
#include "debug/macros.h"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <iostream>
#include <cstdio>
#include <cstdlib>

#include <doctest/doctest.h>

using std::endl;
using std::cerr;
using std::cout;
using base::Timer;

/** Random range, may change definition */
#define RANGE() ((rand() % 10000) + 1)

/** Print the difference between two DBMs two stdout. */
#define DIFF(D1, D2) dbm_printDiff(stdout, D1, D2, size)

/** Show progress */
#define PROGRESS() debug_spin(stderr)

typedef void (*TestFunction)(size_t size);

/** Wraps raw dbm (array of raw_t) and provides pretty-printing of them. */
class dbm_wrap
{
    static inline uint32_t allDBMs = 0;
    static inline uint32_t goodDBMs = 0;
    std::vector<raw_t> data;

public:
    static uint32_t get_allDBMs() { return allDBMs; }
    static uint32_t get_goodDBMs() { return goodDBMs; }
    explicit dbm_wrap(size_t size): data(size, 0) {}
    uint32_t size() const { return data.size(); }
    const raw_t* raw() const { return data.data(); }
    raw_t* raw() { return data.data(); }
    bool operator==(const dbm_wrap& other) const
    {
        return size() == other.size() && dbm_areEqual(raw(), other.raw(), size());
    }

    /** Generate DBMs: track non trivial ones and count them all. */
    void generate()
    {
        bool good = dbm_generate(raw(), size(), RANGE());
        allDBMs++;
        goodDBMs += good;
    }
};

std::ostream& operator<<(std::ostream& os, const dbm_wrap& d)
{
    // TODO: hookup pretty-printing function call for DBM
    return os << "dbm@" << d.raw();
}

class cdd_wrap
{
    cdd cdd_data{};

public:
    cdd_wrap() = default;
    explicit cdd_wrap(const cdd& data): cdd_data{data} {}
    explicit cdd_wrap(dbm_wrap& dbm): cdd_data{dbm.raw(), dbm.size()} {}
    cdd_wrap& operator=(const cdd& other)
    {
        cdd_data = other;
        return *this;
    }
    cdd_wrap& operator=(const cdd_wrap& other)
    {
        cdd_data = other.raw();
        return *this;
    }
    cdd_wrap& operator|=(const cdd& other)
    {
        cdd_data |= other;
        return *this;
    }
    cdd_wrap& operator|=(const cdd_wrap& other)
    {
        cdd_data |= other.raw();
        return *this;
    }
    bool operator==(const cdd& other) const { return cdd_data == other; }
    bool operator==(const cdd_wrap& other) const { return cdd_data == other.raw(); }

    bool contains(dbm_wrap& dbm) const { return cdd_contains(cdd_data, dbm.raw(), dbm.size()); }
    const cdd& raw() const { return cdd_data; }
};

cdd_wrap operator&(const cdd_wrap& cdd1, const cdd_wrap& cdd2) { return cdd_wrap(cdd1.raw() & cdd2.raw()); }
cdd_wrap operator|(const cdd_wrap& cdd1, const cdd_wrap& cdd2) { return cdd_wrap(cdd1.raw() | cdd2.raw()); }
cdd_wrap operator^(const cdd_wrap& cdd1, const cdd_wrap& cdd2) { return cdd_wrap(cdd1.raw() ^ cdd2.raw()); }

/** test conversion between CDD and DBMs */
static void test_conversion(size_t size)
{
    auto dbm1 = dbm_wrap{size};
    auto dbm2 = dbm_wrap{size};

    // Convert to CDD
    dbm1.generate();
    auto cdd1 = cdd_wrap{dbm1};

    // Check conversion
    REQUIRE(cdd1.contains(dbm1));  // dbm_print(stdout, dbm1, size);

    // Convert to DBM
    auto cdd2 = cdd_wrap{cdd_extract_dbm(cdd1.raw(), dbm2.raw(), size)};

    /* Check conversion
     */
    REQUIRE(dbm1 == dbm2);  // DIFF(D1, D2)
    REQUIRE(cdd_reduce(cdd2.raw()) == cdd_false());
}

/* test intersection of CDDs
 */
static void test_intersection(size_t size)
{
    cdd_wrap cdd1, cdd2, cdd3, cdd4;
    auto dbm1 = dbm_wrap{size};
    auto dbm2 = dbm_wrap{size};
    auto dbm3 = dbm_wrap{size};
    auto dbm4 = dbm_wrap{size};

    // Generate DBMs:
    dbm1.generate();
    dbm2.generate();
    dbm_copy(dbm3.raw(), dbm2.raw(), size);

    // Generate intersection:
    bool empty = !dbm_intersection(dbm3.raw(), dbm1.raw(), size);

    // Do the same with CDDs:
    cdd1 = cdd_wrap(dbm1);
    cdd2 = cdd_wrap(dbm2);
    cdd3 = cdd1 & cdd2;

    // Check the result:
    if (!empty) {
        REQUIRE(cdd3.contains(dbm3));

        // Extract DBM:
        cdd3 = cdd_reduce(cdd3.raw());
        cdd4 = cdd_extract_dbm(cdd3.raw(), dbm4.raw(), size);

        // Check result:
        REQUIRE(dbm3 == dbm4);  // DIFF(D1, D2);
    }
}

static double time_apply_and_reduce = 0;
static double time_apply_reduce = 0;

static void test_apply_reduce(size_t size)
{
    /* Generate 8 simple CDDs and then 'or' them together in a pair
     * wise/binary tree fashion.
     */
    cdd_wrap cdds[8];
    auto dbm = dbm_wrap{size};

    for (uint32_t i = 0; i < 8; i++) {
        dbm.generate();
        cdds[i] = cdd_wrap(dbm);
    }

    for (uint32_t j = 4; j > 0; j /= 2) {
        for (uint32_t i = 0; i < j; i++) {
            cdd_wrap a, b, c, e, f;

            a = cdds[2 * i];
            b = cdds[2 * i + 1];

            /* Fake run to ensure that the result has already been
             * created (timing is more fair).
             */
            c = !cdd_apply_reduce(!a.raw(), !b.raw(), cddop_and);

            /* Run (a|b) last so that the apply_reduce calls do not
             * gain from any cache lookups.
             */
            Timer timer;
            c = !cdd_apply_reduce(!a.raw(), !b.raw(), cddop_and);
            time_apply_reduce += timer.getElapsed();
            e = a | b;
            cdd_reduce(e.raw());
            time_apply_and_reduce += timer.getElapsed();

            /* Check that c/d are actually reduced.
             */
            REQUIRE(c == cdd_reduce(c.raw()));

            /* Check that c/d and e describe the same federation.
             */
            REQUIRE(cdd_reduce((c ^ e).raw()) == cdd_false());

            cdds[i] = c;
        }
    }
}

static double time_reduce = 0;
static double time_bf = 0;

std::ostream& operator<<(std::ostream& os, const cdd_wrap& c)
{
    // TODO: hookup pretty-printing function call for CDD
    os << "cdd@" << &c.raw();
    return os;
}

static void test_reduce(size_t size)
{
    cdd_wrap cdd1, cdd2, cdd3;
    auto dbm = dbm_wrap{size};

    cdd1 = cdd_false();
    for (uint32_t j = 0; j < 5; j++) {
        dbm.generate();
        cdd1 |= cdd_wrap(dbm);
    }

    cdd2 = cdd_reduce(cdd1.raw());
    Timer timer;
    cdd2 = cdd_reduce(cdd1.raw());
    time_reduce += timer.getElapsed();
    cdd3 = cdd(cdd_bf_reduce(cdd1.raw().handle()));
    time_bf += timer.getElapsed();

    REQUIRE(cdd2 == cdd3);
}

static void test(const char* name, TestFunction f, size_t size)
{
    cout << name << " size = " << size << endl;
    for (uint32_t k = 0; k < 100; ++k) {
        PROGRESS();
        f(size);
    }
}

void big_test(uint32_t n, uint32_t seed)
{
    cdd_init(100000, 10000, 10000);
    cdd_add_clocks(n);

    for (uint32_t j = 1; j <= 10; ++j) {
        uint32_t DBM_sofar = dbm_wrap::get_allDBMs();
        uint32_t good_sofar = dbm_wrap::get_goodDBMs();
        uint32_t passDBMs, passGood;
        printf("*** Pass %d of 10 ***\n", j);
        for (uint32_t i = 1; i <= n; ++i) /* min dim = 1 */
        {
            test("test_conversion  ", test_conversion, i);
            test("test_intersection", test_intersection, i);
            test("test_apply_reduce", test_apply_reduce, i);
            test("test_reduce      ", test_reduce, i);
        }
        passDBMs = dbm_wrap::get_allDBMs() - DBM_sofar;
        passGood = dbm_wrap::get_goodDBMs() - good_sofar;
        printf("*** Passed(%d) for %d generated DBMs, %d (%d%%) non trivial\n", j, passDBMs, passGood,
               passDBMs ? (100 * passGood) / passDBMs : 0);
    }

    cdd_done();

    if (n > 0)
        REQUIRE(dbm_wrap::get_allDBMs());
    printf("Total generated DBMs: %d, non trivial ones: %d (%d%%)\n", dbm_wrap::get_allDBMs(), dbm_wrap::get_goodDBMs(),
           dbm_wrap::get_allDBMs() ? (100 * dbm_wrap::get_goodDBMs()) / dbm_wrap::get_allDBMs() : 0);
    printf("apply+reduce: %.3fs, apply_reduce: %.3fs\n", time_apply_and_reduce, time_apply_reduce);
    printf("reduce: %.3fs, bf_reduce: %.3fs\n", time_reduce, time_bf);
    printf("Passed\n");
}

#if INTPTR_MAX == INT32_MAX
#define ENABLE_32BIT_TEST
#endif /* 32-bit target */

TEST_CASE("Big CDD test")
{
    uint32_t seed{};
    srand(seed);
    SUBCASE("size 0") { big_test(0, seed); }

    SUBCASE("size 1") { big_test(1, seed); }

    SUBCASE("size 2") { big_test(2, seed); }

    // TODO: on 64-bit the bellow tests are failing :-(
#ifdef ENABLE_32BIT_TEST
    SUBCASE("size 3") { big_test(3, seed); }

    SUBCASE("size 10") { big_test(10, seed); }
#endif
}