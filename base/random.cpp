// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-
///////////////////////////////////////////////////////////////////////////////
//
// This file is a part of UPPAAL.
// Copyright (c) 2011-2020, Aalborg University.
// All rights reserved.
// Author: Marius Mikucionis marius@cs.aau.dk
//
///////////////////////////////////////////////////////////////////////////////

#include "base/random.h"

#include <random>
#include <cassert>

struct RandomGenerator::internalstate
{
    std::mt19937 rnd;  // randomness generator
};

static uint32_t sharedseed = 42;

RandomGenerator::RandomGenerator(): s{new internalstate()} { seed(sharedseed++); }

RandomGenerator::~RandomGenerator() {}

void RandomGenerator::set_seed(const uint32_t seed) { sharedseed = seed; }

void RandomGenerator::seed(const uint32_t seed) { s->rnd.seed(seed); }

uint32_t RandomGenerator::uni(const uint32_t max)
{
    return std::uniform_int_distribution<uint32_t>{0, max}(s->rnd);
}

uint32_t RandomGenerator::uni(const uint32_t from, const uint32_t till)
{
    assert(from <= till);
    return std::uniform_int_distribution<uint32_t>{from, till}(s->rnd);
}

int32_t RandomGenerator::uni(const int32_t from, const int32_t till)
{
    assert(from <= till);
    return std::uniform_int_distribution<int32_t>{from, till}(s->rnd);
}

double RandomGenerator::uni_1() { return std::uniform_real_distribution<double>{0.0, 1.0}(s->rnd); }

double RandomGenerator::uni_r(const double max)
{
    return std::uniform_real_distribution<double>{0.0, max}(s->rnd);
}

double RandomGenerator::uni_r(const double from, const double till)
{
    assert(till - from >= 0);
    return std::uniform_real_distribution<double>{from, till}(s->rnd);
}

double RandomGenerator::exp(const double rate)
{
    assert(rate != 0);
    return std::exponential_distribution<double>{rate}(s->rnd);
}

double RandomGenerator::normal(const double mean, const double stddev)
{
    return std::normal_distribution<double>{mean, stddev}(s->rnd);
}
