// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-
////////////////////////////////////////////////////////////////////
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 2020, Uppsala University and Aalborg University.
// All right reserved.
//
///////////////////////////////////////////////////////////////////

#ifndef BASE_FLOATS_HPP
#define BASE_FLOATS_HPP

#include <algorithm>  // std::clamp

/**
 * Ensures that the value (probability) is no less than 0
 * @tparam T
 * @param prob to clamp
 * @param lowest limit to clamp against, 0 by default
 * @return 0 if value<0 and value otherwise
 */
template <typename T>
T clampp_low(T prob)
{
    return (prob >= 0) ? prob : 0;
}

/**
 * Ensures that the value (probability) is no greater than 1
 * @tparam T
 * @param prob to clamp
 * @return 1 if value>1 and value otherwise
 */
template <typename T>
T clampp_high(T prob)
{
    return (prob <= 1) ? prob : 1;
}

/**
 * Ensures that the value (probability) is within [0, 1]
 * @tparam T
 * @param prob to clamp
 * @return
 */
template <typename T>
T clampp(T prob)
{
    return std::clamp(prob, 0, 1);
}

#endif  // BASE_FLOATS_HPP
