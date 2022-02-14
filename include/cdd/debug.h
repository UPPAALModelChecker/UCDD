// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-
///////////////////////////////////////////////////////////////////////////////
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2004, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: debug.h,v 1.1 2004/08/18 09:25:43 behrmann Exp $
//
///////////////////////////////////////////////////////////////////////////////

#ifndef CDD_DEBUG_H
#define CDD_DEBUG_H

#include <cdd/cdd.h>
#include "cdd/config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Brings a CDD into reduced form. Like cdd_reduce(), uses the
 * Bellman-Ford algorithm rather than Tarjan's.
 *
 * We only keep this implementation for comparison with the "real"
 * cdd_reduce() implementation.
 *
 * @param cdd a cdd
 * @return a reduced cdd equivalent to \a cdd
 * @see cdd_reduce()
 */
extern ddNode* cdd_bf_reduce(ddNode* cdd);

#ifdef __cplusplus
}
#endif

#endif
