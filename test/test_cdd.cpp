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
#include "base/random.h"
#include "debug/macros.h"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <iostream>
#include <random>
#include <cstdio>
#include <cstdlib>

using std::endl;
using std::cerr;
using std::cout;
using base::Timer;

/** Random helper functions. */
static int uniform(uint32_t a, uint32_t b)
{
    RandomGenerator* rg = new RandomGenerator();
    return rg->uni(a, b);
}

static bool random_bool()
{
    RandomGenerator* rg = new RandomGenerator();
    return rg->uni(0, 1);
}

#define RANGE() (uniform(1, 10000))

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

/** Generate a random cdd with only BDD nodes. */
static cdd generate_bdd(size_t size)
{
    cdd bdd_part = cdd_true();
    for (uint32_t i = 0; i < size; i++) {
        cdd bdd_node = cdd_bddvarpp(bdd_start_level + i);
        if (random_bool())
            bdd_node = !bdd_node;
        if (random_bool()) {
            bdd_part &= bdd_node;
        } else {
            bdd_part |= bdd_node;
        }
    }
    return bdd_part;
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

int strict(int inputNumber) { return inputNumber * 2; }

int nstrict(int inputNumber) { return inputNumber * 2 + 1; }

void test_delay(size_t size)
{
    // First some trivial cases.
    REQUIRE(cdd_delay(cdd_true()) == cdd_true());
    REQUIRE(cdd_delay(cdd_false()) == cdd_false());

    // Delay CDDs consisting of only clock differences.
    // Delaying them before disjunction should be the same as after disjunction
    cdd result1 = cdd_false();
    cdd result2 = cdd_false();
    auto dbm = dbm_wrap{size};
    int n_dbms = 8;

    for (uint32_t i = 0; i < n_dbms; i++) {
        dbm.generate();
        cdd cdd1 = cdd(dbm.raw(), dbm.size());
        result1 |= cdd1;
        result2 |= cdd_delay(cdd1);
    }

    REQUIRE(cdd_equiv(cdd_delay(result1), result2));

    // Create a random BDD.
    cdd bdd_part = generate_bdd(size);
    cdd result3 = result1 & bdd_part;

    // The delay operator should not influence the BDD part.
    REQUIRE(cdd_equiv(cdd_delay(result3), result2 & bdd_part));
}

void test_delay_invariant(size_t size)
{
    // First some trivial cases.
    REQUIRE(cdd_delay_invariant(cdd_true(), cdd_true()) == cdd_true());
    REQUIRE(cdd_delay_invariant(cdd_false(), cdd_true()) == cdd_false());
    REQUIRE(cdd_delay_invariant(cdd_true(), cdd_false()) == cdd_false());
    REQUIRE(cdd_delay_invariant(cdd_false(), cdd_false()) == cdd_false());

    // Delay CDDs consisting of only clock differences.
    // Delaying them before disjunction should be the same as after disjunction
    cdd result1 = cdd_false();
    cdd result2 = cdd_false();
    auto dbm = dbm_wrap{size};
    int n_dbms = 8;

    for (uint32_t i = 0; i < n_dbms; i++) {
        dbm.generate();
        cdd cdd1 = cdd(dbm.raw(), dbm.size());
        result1 |= cdd1;
        result2 |= cdd_delay_invariant(cdd1, cdd_true());
    }

    REQUIRE(cdd_equiv(cdd_delay_invariant(result1, cdd_true()), result2));
    REQUIRE(cdd_equiv(cdd_delay_invariant(result1, cdd_true()), cdd_delay(result1)));

    // Create a random BDD.
    cdd bdd_part = generate_bdd(size);
    cdd result3 = result1 & bdd_part;

    // The delay operator should not influence the BDD part.
    REQUIRE(cdd_equiv(cdd_delay_invariant(result3, cdd_true()), result2 & bdd_part));
}

void test_past(size_t size)
{
    // First some trivial cases.
    REQUIRE(cdd_past(cdd_true()) == cdd_true());
    REQUIRE(cdd_past(cdd_false()) == cdd_false());

    // Delay CDDs consisting of only clock differences.
    // Delaying them before disjunction should be the same as after disjunction
    cdd result1 = cdd_false();
    cdd result2 = cdd_false();
    auto dbm = dbm_wrap{size};
    int n_dbms = 8;

    for (uint32_t i = 0; i < n_dbms; i++) {
        dbm.generate();
        cdd cdd1 = cdd(dbm.raw(), dbm.size());
        result1 |= cdd1;
        result2 |= cdd_past(cdd1);
    }

    REQUIRE(cdd_equiv(cdd_past(result1), result2));

    // Create a random BDD.
    cdd bdd_part = generate_bdd(size);
    cdd result3 = result1 & bdd_part;

    // The delay operator should not influence the BDD part.
    REQUIRE(cdd_equiv(cdd_past(result3), result2 & bdd_part));
}

void test_exist(size_t size)
{
    // First some trivial cases.
    REQUIRE(cdd_exist(cdd_true(), NULL, NULL, size, size) == cdd_true());
    REQUIRE(cdd_exist(cdd_false(), NULL, NULL, size, size) == cdd_false());

    // Create a CDD containing random DBMs.
    cdd cdd_part;
    auto dbm = dbm_wrap{size};
    int n_dbms = 8;

    for (uint32_t i = 0; i < n_dbms; i++) {
        dbm.generate();
        cdd_part |= cdd(dbm.raw(), dbm.size());
    }

    // Create a random BDD.
    cdd bdd_part = generate_bdd(size);
    cdd result = cdd_part & bdd_part;

    // Select randomly a clock and a boolean variable
    const int num_bools = 1;
    const int num_clocks = 1;
    int arr[num_clocks] = {uniform(0, size - 1)};
    int* clockPtr = arr;
    int arr1[num_bools] = {uniform(0, size - 1) + bdd_start_level};
    int* boolPtr = arr1;

    // Perform the existential quantification for the chosen clock and boolean variables
    cdd result1 = cdd_exist(result, boolPtr, clockPtr, num_bools, num_clocks);
    result1 = cdd_reduce(result1);

    if (size == 1)
        REQUIRE(cdd_equiv(result1, cdd_true()));
    // TODO how to check correct result for size > 1?

    // Performing the identical operation multiple times should not change the result
    cdd result2 = cdd_exist(result1, boolPtr, clockPtr, num_bools, num_clocks);
    result2 = cdd_reduce(result2);
    REQUIRE(cdd_equiv(result1, result2));

    // The order of existential quantification should not matter
    cdd result3 = cdd_exist(result, boolPtr, NULL, num_bools, 0);
    result3 = cdd_reduce(result3);
    cdd result4 = cdd_exist(result3, NULL, clockPtr, 0, num_clocks);
    result4 = cdd_reduce(result4);

    cdd result5 = cdd_exist(result, NULL, clockPtr, 0, num_clocks);
    result5 = cdd_reduce(result5);
    cdd result6 = cdd_exist(result5, boolPtr, NULL, num_bools, 0);
    result6 = cdd_reduce(result6);

    REQUIRE(cdd_equiv(result1, result4));
    REQUIRE(cdd_equiv(result1, result6));
    REQUIRE(cdd_equiv(result4, result6));
}

void test_apply_reset(size_t size)
{
    // First some trivial cases.
    REQUIRE(cdd_equiv(cdd_apply_reset(cdd_true(), NULL, NULL, 0, NULL, NULL, 0), cdd_remove_negative(cdd_true())));
    REQUIRE(cdd_equiv(cdd_apply_reset(cdd_false(), NULL, NULL, 0, NULL, NULL, 0), cdd_remove_negative(cdd_false())));

    // Create a CDD containing random DBMs.
    cdd cdd_part;
    auto dbm = dbm_wrap{size};
    int n_dbms = 8;

    for (uint32_t i = 0; i < n_dbms; i++) {
        dbm.generate();
        cdd_part |= cdd(dbm.raw(), dbm.size());
    }

    // Create a random BDD.
    cdd bdd_part = generate_bdd(size);
    cdd cdd1 = cdd_part & bdd_part;

    // Resetting nothing should not change the CDDs.
    REQUIRE(cdd_equiv(cdd_apply_reset(cdd_part, NULL, NULL, 0, NULL, NULL, 0), cdd_remove_negative(cdd_part)));
    REQUIRE(cdd_equiv(cdd_apply_reset(bdd_part, NULL, NULL, 0, NULL, NULL, 0), cdd_remove_negative(bdd_part)));
    REQUIRE(cdd_equiv(cdd_apply_reset(cdd1, NULL, NULL, 0, NULL, NULL, 0), cdd_remove_negative(cdd1)));

    // The rest of the test only applies when there are 2 or more clocks.
    if (size <= 1)
        return;

    // Select randomly a clock and a boolean variable to be reset.
    const int num_bools = 1;
    const int num_clocks = 1;
    int clock_num = (uniform(1, size - 1));  // So we don't select the zero clock
    int arr[num_clocks] = {clock_num};
    int* clockPtr = arr;
    int arr1[num_clocks] = {0};  // Always reset to 0 in this test case.
    int* clock_values = arr1;
    int arr2[num_bools] = {uniform(0, size - 1) + bdd_start_level};
    int* boolPtr = arr2;
    int arr3[num_bools] = {0};  // Always reset to false in this test case.
    int* bool_values = arr3;

    // Perform the reset to the 0 value.
    cdd result1 = cdd_apply_reset(cdd1, clockPtr, clock_values, num_clocks, boolPtr, bool_values, num_bools);
    result1 = cdd_reduce(result1);

    // Create a CDD containing just the updated values.
    cdd update = cdd_intervalpp(clockPtr[0], 0, 0, 1) & cdd_bddnvarpp(boolPtr[0]);

    // Check the result.
    REQUIRE(cdd_equiv(result1, result1 & update));
    REQUIRE(cdd_equiv(cdd_false(), result1 & !update));
}

void test_transition(size_t size)
{
    // First some trivial cases.
    REQUIRE(cdd_equiv(cdd_transition(cdd_true(), cdd_true(), NULL, NULL, 0, NULL, NULL, 0),
                      cdd_remove_negative(cdd_true())));
    REQUIRE(cdd_equiv(cdd_transition(cdd_true(), cdd_false(), NULL, NULL, 0, NULL, NULL, 0),
                      cdd_remove_negative(cdd_false())));
    REQUIRE(cdd_equiv(cdd_transition(cdd_false(), cdd_true(), NULL, NULL, 0, NULL, NULL, 0),
                      cdd_remove_negative(cdd_false())));

    // Create a CDD containing random DBMs.
    cdd cdd_part;
    auto dbm = dbm_wrap{size};
    int n_dbms = 8;

    for (uint32_t i = 0; i < n_dbms; i++) {
        dbm.generate();
        cdd_part |= cdd(dbm.raw(), dbm.size());
    }

    // Create a random BDD.
    cdd bdd_part = generate_bdd(size);
    cdd cdd1 = cdd_part & bdd_part;

    // Taking a transition with true guard and no update should not alter the state.
    REQUIRE(
        cdd_equiv(cdd_transition(cdd_part, cdd_true(), NULL, NULL, 0, NULL, NULL, 0), cdd_remove_negative(cdd_part)));
    REQUIRE(
        cdd_equiv(cdd_transition(bdd_part, cdd_true(), NULL, NULL, 0, NULL, NULL, 0), cdd_remove_negative(bdd_part)));
    REQUIRE(cdd_equiv(cdd_transition(cdd1, cdd_true(), NULL, NULL, 0, NULL, NULL, 0), cdd_remove_negative(cdd1)));

    // The rest of the test only applies when there are 2 or more clocks.
    if (size <= 1)
        return;

    cdd guard = generate_bdd(size);

    // Select randomly a clock and a boolean variable to be reset.
    const int num_bools = 1;
    const int num_clocks = 1;
    int clock_num = (uniform(1, size - 1));  // So we don't select the zero clock
    int arr[num_clocks] = {clock_num};
    int* clockPtr = arr;
    int arr1[num_clocks] = {0};  // Always reset to 0 in this test case.
    int* clock_values = arr1;
    int arr2[num_bools] = {uniform(0, size - 1) + bdd_start_level};
    int* boolPtr = arr2;
    int arr3[num_bools] = {0};  // Always reset to false in this test case.
    int* bool_values = arr3;

    // Take the transition.
    cdd result1 = cdd_transition(cdd1, guard, clockPtr, clock_values, num_clocks, boolPtr, bool_values, num_bools);
    result1 = cdd_reduce(result1);

    // Create a CDD containing just the updated values.
    cdd update = cdd_intervalpp(clockPtr[0], 0, 0, 1) & cdd_bddnvarpp(boolPtr[0]);

    // Check the result.
    REQUIRE(cdd_equiv(result1, result1 & update));
    REQUIRE(cdd_equiv(cdd_false(), result1 & !update));

    // Performing transition manually.
    cdd after_guard = cdd1 & guard;
    cdd result2 = cdd_apply_reset(after_guard, clockPtr, clock_values, num_clocks, boolPtr, bool_values, num_bools);
    REQUIRE(cdd_equiv(result1, result2));
}

void test_transition_back(size_t size)
{
    // First some trivial cases.
    REQUIRE(cdd_equiv(cdd_transition_back(cdd_true(), cdd_true(), cdd_true(), NULL, 0, NULL, 0), cdd_true()));
    REQUIRE(cdd_equiv(cdd_transition_back(cdd_true(), cdd_false(), cdd_true(), NULL, 0, NULL, 0), cdd_false()));
    REQUIRE(cdd_equiv(cdd_transition_back(cdd_false(), cdd_true(), cdd_true(), NULL, 0, NULL, 0), cdd_false()));

    // Create a CDD containing random DBMs.
    cdd cdd_part;
    auto dbm = dbm_wrap{size};
    int n_dbms = 8;

    for (uint32_t i = 0; i < n_dbms; i++) {
        dbm.generate();
        cdd_part |= cdd(dbm.raw(), dbm.size());
    }

    // Create a random BDD.
    cdd bdd_part = generate_bdd(size);
    cdd cdd1 = cdd_part & bdd_part;

    // Taking a transition with true guard and no update should not alter the state.
    REQUIRE(cdd_equiv(cdd_transition_back(cdd_part, cdd_true(), cdd_true(), NULL, 0, NULL, 0), cdd_part));
    REQUIRE(cdd_equiv(cdd_transition_back(bdd_part, cdd_true(), cdd_true(), NULL, 0, NULL, 0), bdd_part));
    REQUIRE(cdd_equiv(cdd_transition_back(cdd1, cdd_true(), cdd_true(), NULL, 0, NULL, 0), cdd1));

    // The rest of the test only applies when there are 2 or more clocks.
    if (size <= 1)
        return;

    cdd guard = generate_bdd(size);

    // Select randomly a clock and a boolean variable to be reset.
    const int num_bools = 1;
    const int num_clocks = 1;
    int clock_num = (uniform(1, size - 1));  // So we don't select the zero clock
    int arr[num_clocks] = {clock_num};
    int* clockPtr = arr;
    int arr1[num_clocks] = {0};  // Always reset to 0 in this test case.
    int* clock_values = arr1;
    int arr2[num_bools] = {uniform(0, size - 1) + bdd_start_level};
    int* boolPtr = arr2;
    int arr3[num_bools] = {0};  // Always reset to false in this test case.
    int* bool_values = arr3;
    cdd update = cdd_intervalpp(clockPtr[0], 0, 0, 1) & cdd_bddnvarpp(boolPtr[0]);

    // Take the transition.
    cdd result1 = cdd_transition_back(cdd1, guard, update, clockPtr, num_clocks, boolPtr, num_bools);
    result1 = cdd_reduce(result1);

    // Check the result.
    REQUIRE(cdd_equiv(result1, result1 & guard));
    REQUIRE(cdd_equiv(cdd_false(), result1 & !guard));

    // Transitioning forward again should be included in the original start CDD.
    // Remember that cdd1 \subset cdd2 <==> cdd1 & !cdd2 == false
    REQUIRE((!cdd1 & cdd_transition(result1, guard, clockPtr, clock_values, num_clocks, boolPtr, bool_values,
                                    num_bools)) == cdd_false());
}

void test_transition_back_past(size_t size)
{
    // First some trivial cases.
    REQUIRE(cdd_equiv(cdd_transition_back_past(cdd_true(), cdd_true(), cdd_true(), NULL, 0, NULL, 0), cdd_true()));
    REQUIRE(cdd_equiv(cdd_transition_back_past(cdd_true(), cdd_false(), cdd_true(), NULL, 0, NULL, 0), cdd_false()));
    REQUIRE(cdd_equiv(cdd_transition_back_past(cdd_false(), cdd_true(), cdd_true(), NULL, 0, NULL, 0), cdd_false()));

    // Create a CDD containing random DBMs.
    cdd cdd_part;
    auto dbm = dbm_wrap{size};
    int n_dbms = 8;

    for (uint32_t i = 0; i < n_dbms; i++) {
        dbm.generate();
        cdd_part |= cdd(dbm.raw(), dbm.size());
    }

    // Create a random BDD.
    cdd bdd_part = generate_bdd(size);
    cdd cdd1 = cdd_part & bdd_part;

    // Taking a transition with true guard and no update should not alter the state. So only the past delay operator is
    // applied.
    REQUIRE(
        cdd_equiv(cdd_transition_back_past(cdd_part, cdd_true(), cdd_true(), NULL, 0, NULL, 0), cdd_past(cdd_part)));
    REQUIRE(
        cdd_equiv(cdd_transition_back_past(bdd_part, cdd_true(), cdd_true(), NULL, 0, NULL, 0), cdd_past(bdd_part)));
    REQUIRE(cdd_equiv(cdd_transition_back_past(cdd1, cdd_true(), cdd_true(), NULL, 0, NULL, 0), cdd_past(cdd1)));

    // The rest of the test only applies when there are 2 or more clocks.
    if (size <= 1)
        return;

    cdd guard = generate_bdd(size);

    // Select randomly a clock and a boolean variable to be reset.
    const int num_bools = 1;
    const int num_clocks = 1;
    int clock_num = (uniform(1, size - 1));  // So we don't select the zero clock
    int arr[num_clocks] = {clock_num};
    int* clockPtr = arr;
    int arr1[num_clocks] = {0};  // Always reset to 0 in this test case.
    int* clock_values = arr1;
    int arr2[num_bools] = {uniform(0, size - 1) + bdd_start_level};
    int* boolPtr = arr2;
    int arr3[num_bools] = {0};  // Always reset to false in this test case.
    int* bool_values = arr3;
    cdd update = cdd_intervalpp(clockPtr[0], 0, 0, 1) & cdd_bddnvarpp(boolPtr[0]);

    // Take the transition.
    cdd result1 = cdd_transition_back_past(cdd1, guard, update, clockPtr, num_clocks, boolPtr, num_bools);
    result1 = cdd_reduce(result1);

    // Check the result.
    REQUIRE(cdd_equiv(result1, result1 & guard));
    REQUIRE(cdd_equiv(cdd_false(), result1 & !guard));
    REQUIRE(cdd_equiv(result1,
                      cdd_past(cdd_transition_back(cdd1, guard, update, clockPtr, num_clocks, boolPtr, num_bools))));

    // Transitioning forward again should be included in the original start CDD.
    // Remember that cdd1 \subset cdd2 <==> cdd1 & !cdd2 == false
    REQUIRE((!cdd1 & cdd_transition(result1, guard, clockPtr, clock_values, num_clocks, boolPtr, bool_values,
                                    num_bools)) == cdd_false());
}

void test_predt(size_t size)
{
    // First some trivial cases.
    REQUIRE(cdd_equiv(cdd_predt(cdd_true(), cdd_true()), cdd_false()));
    REQUIRE(cdd_equiv(cdd_predt(cdd_true(), cdd_false()), cdd_remove_negative(cdd_true())));
    REQUIRE(cdd_equiv(cdd_predt(cdd_false(), cdd_true()), cdd_false()));
    REQUIRE(cdd_equiv(cdd_predt(cdd_false(), cdd_false()), cdd_false()));

    // Create a CDD containing random DBMs.
    cdd cdd_part;
    auto dbm = dbm_wrap{size};
    int n_dbms = 8;

    for (uint32_t i = 0; i < n_dbms; i++) {
        dbm.generate();
        cdd_part |= cdd(dbm.raw(), dbm.size());
    }

    // Create a random BDD.
    cdd bdd_part = generate_bdd(size);
    cdd cdd1 = cdd_part & bdd_part;

    // First some checks when nothing can save us.
    // cdd_part \subset cdd_predt(cdd_part) <==> cdd_part & !cdd_predt(cdd_part) == false
    REQUIRE((!cdd_predt(cdd_part, cdd_false()) & cdd_remove_negative(cdd_part)) == cdd_false());
    // cdd_predt(cdd_part) \subset cdd_past(cdd_part) <==> cdd_predt(cdd_part) & !cdd_past(cdd_part) == false
    REQUIRE((!cdd_past(cdd_part) & cdd_predt(cdd_part, cdd_false())) == cdd_false());
    REQUIRE(cdd_equiv(cdd_predt(bdd_part, cdd_false()), cdd_remove_negative(bdd_part)));
    // cdd_predt(cdd1) \subset cdd_past(cdd1) <==> cdd_predt(cdd1) & !cdd_past(cdd1) == false
    REQUIRE((!cdd_past(cdd1) & cdd_predt(cdd1, cdd_false())) == cdd_false());
    REQUIRE(cdd_equiv(cdd_predt(cdd1, cdd_true()), cdd_false()));

    // Check timed predecessor for random, but non-overlapping safe cdd.
    if (size == 0)
        return;
    cdd b1 = cdd_bddvarpp(bdd_start_level);
    cdd cdd2_part;
    for (uint32_t i = 0; i < n_dbms; i++) {
        dbm.generate();
        cdd_part |= cdd(dbm.raw(), dbm.size());
    }

    cdd left = cdd_part & b1;
    cdd right = cdd2_part & !b1;
    cdd test = left | right;
    REQUIRE(cdd_equiv(cdd_predt(test, right), cdd_remove_negative(cdd_past(left))));
}

static void test(const char* name, TestFunction f, size_t size)
{
    cout << name << " size = " << size << endl;
    for (uint32_t k = 0; k < 100; ++k) {
        PROGRESS();
        f(size);
    }
}

static void big_test(uint32_t n)
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
            test("test_delay       ", test_delay, i);
            test("test_delay_invariant", test_delay_invariant, i);
            test("test_past        ", test_past, i);
            test("test_exist       ", test_exist, i);
            test("test_apply_reset ", test_apply_reset, i);
            test("test_transition  ", test_transition, i);
            test("test_transition_back", test_transition_back, i);
            test("test_transition_back_past", test_transition_back_past, i);
            test("test_predt       ", test_predt, i);
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
    cdd_add_bddvar(3);
    test_intersection(3);
    cdd_done();
}

TEST_CASE("CDD timed predecessor static test")
{
    cdd_init(100000, 10000, 10000);
    cdd_add_clocks(4);
    cdd_add_bddvar(3);

    cdd b6 = cdd_bddvarpp(bdd_start_level + 0);
    cdd b7 = cdd_bddvarpp(bdd_start_level + 1);
    cdd b8 = cdd_bddvarpp(bdd_start_level + 2);

    // Create input cdd.
    cdd input = cdd_intervalpp(1, 0, 6, 10);
    input &= cdd_intervalpp(2, 0, 5, dbm_LS_INFINITY);
    input &= cdd_intervalpp(3, 0, 8, dbm_LS_INFINITY);
    input &= b6;
    cdd_reduce(input);

    // Create safe cdd, which will be safe1 | safe2.
    cdd safe1 = cdd_intervalpp(1, 0, 0, 4);
    safe1 &= cdd_intervalpp(2, 0, 0, dbm_LS_INFINITY);
    safe1 &= cdd_intervalpp(3, 0, 0, dbm_LS_INFINITY);
    safe1 &= b7;

    cdd safe2 = cdd_intervalpp(1, 0, 7, 9);
    safe2 &= cdd_intervalpp(2, 0, 0, 4);
    safe2 &= cdd_intervalpp(3, 0, 0, 3);
    safe2 &= b8;
    cdd safe = safe1 | safe2;
    cdd_reduce(safe);

    // Perform the timed predecessor operator and remove any negative clock values.
    cdd result = cdd_remove_negative(cdd_predt(input, safe));

    // Create the expected cdd.
    cdd expected1 = cdd_intervalpp(1, 0, 0, 4);
    expected1 &= cdd_intervalpp(2, 1, -5, dbm_LS_INFINITY);
    expected1 &= cdd_intervalpp(3, 1, -1, dbm_LS_INFINITY);
    expected1 &= b6;
    expected1 &= !b7;
    cdd expected2 = cdd_intervalpp(1, 0, 4, 10);
    expected2 &= cdd_intervalpp(2, 1, -5, dbm_LS_INFINITY);
    expected2 &= cdd_intervalpp(3, 1, -1, dbm_LS_INFINITY);
    expected2 &= b6;
    cdd expected = cdd_remove_negative(expected1 | expected2);

    // Check whether the result matches the expected one.
    REQUIRE(cdd_equiv(result, expected));
}

// TODO: the bellow test case passes only on 32-bit, need to fix it
#if INTPTR_MAX == INT32_MAX
TEST_CASE("CDD reduce with size 3")
{
    cdd_init(100000, 10000, 10000);
    cdd_add_clocks(3);
    cdd_add_bddvar(3);
    test_reduce(3);
}
#endif /* 32-bit */

TEST_CASE("Big CDD test")
{
    RandomGenerator* rg = new RandomGenerator();
    uint32_t seed{};
    rg->set_seed(seed);
    dbm_wrap::resetCounters();
    SUBCASE("Size 0") { big_test(0); }
    SUBCASE("Size 1") { big_test(1); }
    SUBCASE("Size 2") { big_test(2); }
    // TODO: the bellow cases pass only on 32-bit, need to fix it
#if INTPTR_MAX == INT32_MAX
    SUBCASE("Size 3") { big_test(3, seed); }
    SUBCASE("Size 10") { big_test(10, seed); }
#endif /* 32-bit */
}
