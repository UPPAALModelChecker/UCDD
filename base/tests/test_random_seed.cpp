// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-

#include "base/random.h"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <random>

using namespace std;

TEST_CASE("Random seed")
{
    auto rdev = std::random_device{};
    auto dist = std::uniform_int_distribution<uint32_t>{std::numeric_limits<uint32_t>::min(),
                                                        std::numeric_limits<uint32_t>::max()};
    const uint32_t myseed = dist(rdev);
    const auto p = std::vector<uint32_t>{1,
                                         2,
                                         7,
                                         10,
                                         17,
                                         19,
                                         100,
                                         1000,
                                         10001,
                                         (1 << 15) - 1,
                                         1 << 15,
                                         (1 << 15) + 1,
                                         (1 << 15) + 2,
                                         (1 << 16) - 1,
                                         1 << 16,
                                         (1 << 16) + 1,
                                         (1UL << 31) - 1,
                                         1UL << 31,
                                         (1UL << 31) + 1,
                                         (1UL << 31) + 2};
    const uint32_t trials = 1000000;
    auto uni = std::vector<uint32_t>(trials);
    auto exp = std::vector<uint32_t>(trials);
    uint32_t n;

    // Prepare random number sequences
    RandomGenerator gen;
    gen.seed(myseed);
    for (auto i = 0u; i < trials; ++i) {
        uni[i] = gen.uni(p[i % p.size()]);
    }
    for (auto i = 0u; i < trials; ++i) {
        exp[i] = gen.exp(p[i % p.size()]);
    }
    // Replicate the sequences
    gen.seed(myseed);
    for (auto i = 0u; i < trials; ++i) {
        n = gen.uni(p[i % p.size()]);
        CHECK(uni[i] == n);
    }
    for (auto i = 0u; i < trials; ++i) {
        n = gen.exp(p[i % p.size()]);
        CHECK(exp[i] == n);
    }
}
