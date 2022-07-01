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
#include "cdd/cdd.h"
#include "cdd/debug.h"
#include "cdd/kernel.h"
#include "base/Timer.h"
#include "debug/macros.h"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <iostream>
#include <cstdio>
#include <cstdlib>

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

using TestFunction = void (*)(size_t size);

/** Wraps raw dbm (array of raw_t) and provides pretty-printing of them. */
class dbm_wrap
{
    static inline uint32_t allDBMs = 0;
    static inline uint32_t goodDBMs = 0;
    size_t sz{};
    std::vector<raw_t> data;

public:
    static uint32_t get_allDBMs() { return allDBMs; }
    static uint32_t get_goodDBMs() { return goodDBMs; }
    static void resetCounters()
    {
        allDBMs = 0;
        goodDBMs = 0;
    }
    explicit dbm_wrap(size_t size): sz{size}, data(sz * sz)
    {
        REQUIRE(size <= std::numeric_limits<uint32_t>::max());
        REQUIRE(size * size <= std::numeric_limits<uint32_t>::max());
    }
    [[nodiscard]] uint32_t size() const { return static_cast<uint32_t>(sz); }
    [[nodiscard]] const raw_t* raw() const { return data.data(); }
    [[nodiscard]] raw_t* raw() { return data.data(); }
    bool operator==(const dbm_wrap& other) const
    {
        return size() == other.size() && dbm_areEqual(raw(), other.raw(), size());
    }

    /** Generate DBMs: track non trivial ones and count them all. */
    void generate()
    {
        bool good = dbm_generate(raw(), size(), RANGE());
        assert(allDBMs < std::numeric_limits<decltype(allDBMs)>::max());
        allDBMs++;
        assert(goodDBMs < std::numeric_limits<decltype(goodDBMs)>::max() - good);
        goodDBMs += good;
    }
};

std::ostream& operator<<(std::ostream& os, const dbm_wrap& d)
{
    os << "dbm{" << d.raw();
    for (auto i = 0u; i < d.size(); ++i) {
        os << ((i == 0) ? "{" : ",{");
        os << d.raw()[i * d.size()];
        for (auto j = 1u; j < d.size(); ++j)
            os << "," << d.raw()[i * d.size() + j];
        os << "}";
    }
    return os << "}";
}

/** test conversion between CDD and DBMs */
static void test_conversion(size_t size)
{
    auto dbm1 = dbm_wrap{size};
    auto dbm2 = dbm_wrap{size};

    // Convert to CDD
    dbm1.generate();
    auto cdd1 = cdd{dbm1.raw(), dbm1.size()};

    // Check conversion
    REQUIRE(cdd_contains(cdd1, dbm1.raw(), dbm1.size()));  // dbm_print(stdout, dbm1, size);

    // Convert to DBM
    auto cdd2 = cdd{cdd_extract_dbm(cdd1, dbm2.raw(), size)};

    /* Check conversion
     */
    REQUIRE(dbm1 == dbm2);  // DIFF(D1, D2)
    REQUIRE(cdd_reduce(cdd2) == cdd_false());
}

static void test_extract_bdd(size_t size)
{
    cdd cdd1, cdd2, cdd3, cdd4;
    auto dbm1 = dbm_wrap{size};
    auto dbm2 = dbm_wrap{size};

    // Create DBM and CDDs:
    dbm1.generate();
    cdd1 = cdd{dbm1.raw(), dbm1.size()};
    cdd2 = cdd_bddvarpp(bdd_start_level + size - 1);
    cdd3 = cdd1 & cdd2;

    // Extract the DBM and BDD part:
    cdd4 = cdd_extract_bdd(cdd_reduce(cdd3), size);

    // Check the result:
    REQUIRE(cdd_equiv(cdd4, cdd2));
}

static void test_extract_bdd_and_dbm(size_t size)
{
    cdd cdd1, cdd2, cdd3;
    auto dbm1 = dbm_wrap{size};

    // Create DBM and CDDs:
    dbm1.generate();
    cdd1 = cdd{dbm1.raw(), dbm1.size()};
    cdd2 = cdd_bddvarpp(bdd_start_level + size - 1);
    cdd3 = cdd1 & cdd2;

    // Extract the DBM and BDD part:
    REQUIRE(cdd_contains(cdd3, dbm1.raw(), dbm1.size()));
    extraction_result er = cdd_extract_bdd_and_dbm(cdd_reduce(cdd3));

    // Check the result:
    REQUIRE(dbm_areEqual(er.dbm, dbm1.raw(), size));
    REQUIRE(cdd_equiv(er.BDD_part, cdd2));
}

/* test intersection of CDDs
 */
static void test_intersection(size_t size)
{
    cdd cdd1, cdd2, cdd3, cdd4;
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
    cdd1 = cdd(dbm1.raw(), size);
    cdd2 = cdd(dbm2.raw(), size);
    cdd3 = cdd1 & cdd2;

    // Check the result:
    if (!empty) {
        REQUIRE(cdd_contains(cdd3, dbm3.raw(), size));

        // Extract DBM:
        cdd3 = cdd_reduce(cdd3);
        cdd4 = cdd_extract_dbm(cdd3, dbm4.raw(), size);

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
    cdd cdds[8];
    auto dbm = dbm_wrap{size};

    for (uint32_t i = 0; i < 8; i++) {
        dbm.generate();
        cdds[i] = cdd(dbm.raw(), dbm.size());
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
            REQUIRE(c == cdd_reduce(c));

            /* Check that c/d and e describe the same federation.
             */
            REQUIRE(cdd_reduce(c ^ e) == cdd_false());

            cdds[i] = c;
        }
    }
}

static double time_reduce = 0;
static double time_bf = 0;

std::ostream& operator<<(std::ostream& os, const cdd& d)
{
    static size_t cdd_log_counter = 0;
    auto cdd_log = std::string{"cdd_"} + std::to_string(++cdd_log_counter) + ".dot";
    auto file = std::fopen(cdd_log.c_str(), "w");
    cdd_fprintdot(file, d, false);
    os << "cdd@" << &d << "(" << cdd_log << ")";
    return os;
}

static void test_reduce(size_t size)
{
    cdd cdd1, cdd2, cdd3;
    auto dbm = dbm_wrap{size};

    cdd1 = cdd_false();
    for (uint32_t j = 0; j < 5; j++) {
        dbm.generate();
        cdd1 |= cdd(dbm.raw(), size);
    }

    cdd2 = cdd_reduce(cdd1);
    Timer timer;
    cdd2 = cdd_reduce(cdd1);
    time_reduce += timer.getElapsed();
    cdd3 = cdd(cdd_bf_reduce(cdd1.handle()));
    time_bf += timer.getElapsed();

    REQUIRE(cdd2 == cdd3);
}

static void test_remove_negative(size_t size)
{
    if (size == 0)
        return;

    // When cdd has size 1, it only contains the zero clock.
    // This clock should always remain zero.
    if (size == 1)
        return;

    cdd cdd1, cdd2, cdd3, cdd4, cdd5;
    auto dbm = dbm_wrap{size};

    // We create a CDD where only some range of negative values for the second clock are allowed.
    int bound1 = RANGE();
    int bound2 = RANGE();
    int low = ((bound1 >= bound2) ? bound1 : bound2) * -1;
    int up = ((bound1 >= bound2) ? bound2 : bound1) * -1;
    cdd1 = cdd_interval(1, 0, low, up);

    // Extracting a dbm triggers assert(isValid(dbm)) internally in cdd_extract_dbm.
    // cdd2 = cdd_extract_dbm(cdd1, dbm.raw(), size);

    cdd2 = cdd_remove_negative(cdd1);

    // cdd_extract_dbm has build-in assertions to verify that the extracted dbm is valid.
    // So successfully executing the line bellow means that the negative part of the cdd has
    // been removed successfully.
    cdd3 = cdd_extract_dbm(cdd2, dbm.raw(), size);

    // Additional test cases

    cdd1 = cdd_interval(1, 0, low, up);
    cdd2 = cdd_remove_negative(cdd1);
    REQUIRE(cdd2 == cdd_false());

    cdd3 = cdd_lower(1, 0, low);
    cdd4 = cdd_remove_negative(cdd3);
    cdd5 = cdd_remove_negative(cdd_true());
    REQUIRE(cdd4 == cdd5);
    REQUIRE(cdd4 != cdd3);
}

static void test_equiv(size_t size)
{
    cdd cdd1, cdd2, cdd3, cdd4;
    auto dbm1 = dbm_wrap{size};
    auto dbm2 = dbm_wrap{size};

    // Generate DBMs:
    dbm1.generate();
    dbm2.generate();

    // Create CDDs:
    cdd1 = cdd(dbm1.raw(), size);
    cdd2 = cdd(dbm2.raw(), size);
    cdd3 = cdd1 & cdd2;
    cdd4 = cdd2 & cdd1;

    // Check the result:
    REQUIRE(cdd_equiv(cdd3, cdd4));
}

static void test(const char* name, TestFunction f, size_t size)
{
    cout << name << " size = " << size << endl;
    for (uint32_t k = 0; k < 100; ++k) {
        PROGRESS();
        f(size);
    }
}

static void big_test(uint32_t n, uint32_t seed)
{
    cdd_init(100000, 10000, 10000);
    cdd_add_clocks(n);
    cdd_add_bddvar(n);

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
            test("test_equiv       ", test_equiv, i);
            test("test_extract_bdd ", test_extract_bdd, i);
            test("test_extract_bdd_and_dbm", test_extract_bdd_and_dbm, i);
        }
        test("test_remove_negative", test_remove_negative, n);
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

TEST_CASE("CDD intersection with size 3")
{
    cdd_init(100000, 10000, 10000);
    cdd_add_clocks(3);
    test_intersection(3);
}

// TODO: the bellow test case passes only on 32-bit, need to fix it
#if INTPTR_MAX == INT32_MAX
TEST_CASE("CDD reduce with size 3")
{
    cdd_init(100000, 10000, 10000);
    cdd_add_clocks(3);
    test_reduce(3);
}
#endif /* 32-bit */

TEST_CASE("Big CDD test")
{
    uint32_t seed{};
    srand(seed);
    dbm_wrap::resetCounters();
    SUBCASE("Size 0") { big_test(0, seed); }
    SUBCASE("Size 1") { big_test(1, seed); }
    SUBCASE("Size 2") { big_test(2, seed); }
    // TODO: the bellow cases pass only on 32-bit, need to fix it
#if INTPTR_MAX == INT32_MAX
    SUBCASE("Size 3") { big_test(3, seed); }
    SUBCASE("Size 10") { big_test(10, seed); }
#endif /* 32-bit */
}
