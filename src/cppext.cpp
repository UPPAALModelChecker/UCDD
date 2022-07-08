// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-
///////////////////////////////////////////////////////////////////////////////
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: cppext.cpp,v 1.4 2004/04/02 22:53:55 behrmann Exp $
//
///////////////////////////////////////////////////////////////////////////////

#include <malloc.h>
#include "cdd/kernel.h"

#define ADBM(NAME) raw_t* NAME = allocDBM(size)

/* Allocate a DBM. */
static raw_t* allocDBM(uint32_t dim) { return (raw_t*)malloc(dim * dim * sizeof(raw_t)); }

cdd::cdd(const cdd& r)
{
    assert(cdd_isrunning());
    root = r.root;
    if (root)
        cdd_ref(root);
}

cdd::cdd(const raw_t* dbm, uint32_t dim)
{
    assert(cdd_isrunning());
    root = cdd_from_dbm(dbm, dim);
    cdd_ref(root);
}

cdd::cdd(ddNode* r)
{
    assert(cdd_isrunning() && r);
    root = r;
    cdd_ref(r);
}

cdd::~cdd() { cdd_rec_deref(root); }

cdd& cdd::operator=(const cdd& r)
{
    if (root != r.root) {
        cdd_rec_deref(root);
        root = r.root;
        cdd_ref(root);
    }
    return *this;
}

cdd cdd::operator=(ddNode* node)
{
    if (root != node) {
        cdd_rec_deref(root);
        root = node;
        cdd_ref(root);
    }
    return *this;
}

/**
 * Extract a BDD and DBM from a given CDD.
 * PRECONDITION: call CDD reduce first!!!
 * @param cdd a cdd
 * @return the extraction result containing the extracted BDD,
 *      extracted DBM ,and the remaining cdd.
 */
extraction_result cdd_extract_bdd_and_dbm(const cdd& state)
{
    uint32_t size = cdd_clocknum;
    ADBM(dbm);
    cdd bdd_part = cdd_extract_bdd(state, size);
    cdd cdd_part = cdd_extract_dbm(state, dbm, size);
    extraction_result res;
    res.BDD_part = bdd_part;
    res.CDD_part = cdd_part;
    res.dbm = dbm;
    return res;
}

/**
 * Perform the delay operation on a CDD.
 *
 * <p>The delay is performed by extracting all DBMs (and their
 * corresponding BDD part), delaying the DBMs individually,
 * and then compose the delayed DBMs back together with their
 * BDD parts.</p>
 *
 * @param cdd a CDD
 * @return the delayed CDD.
 */
cdd cdd_delay(const cdd& state)
{
    if (cdd_isterminal(state.root))
        return state;
    if (cdd_info(state.root)->type == TYPE_BDD)
        return state;
    uint32_t size = cdd_clocknum;
    cdd copy = state;
    cdd res = cdd_false();
    ADBM(dbm);
    while (!cdd_isterminal(copy.root) && cdd_info(copy.root)->type != TYPE_BDD) {
        copy = cdd_reduce(copy);
        cdd bottom = cdd_extract_bdd(copy, size);
        copy = cdd_extract_dbm(copy, dbm, size);
        copy = cdd_reduce(cdd_remove_negative(copy));
        dbm_up(dbm, size);
        cdd fixed_cdd = cdd(dbm, size);
        fixed_cdd &= bottom;
        res |= fixed_cdd;
    }
    return res;
}

/**
 * Perform the delay operation on a CDD taking an invariant into account.
 *
 * @param cdd the cdd to delay
 * @param cdd the invariant to be taken into account
 * @return the delayed CDD.
 * @see cdd_delay
 */
cdd cdd_delay_invariant(const cdd& state, const cdd& invar)
{
    cdd res = cdd_delay(state);
    res &= invar;
    return res;
}

/**
 * Perform the inverse delay operation on a CDD.
 *
 * <p>The inverse delay is performed by extracting all DBMs (and their
 * corresponding BDD part), delaying the DBMs individually,
 * and then compose the delayed DBMs back together with their
 * BDD parts.</p>
 *
 * @param cdd a CDD
 * @return the delayed CDD.
 */
cdd cdd_past(const cdd& state)
{
    if (cdd_isterminal(state.root))
        return state;
    if (cdd_info(state.root)->type == TYPE_BDD)
        return state;
    uint32_t size = cdd_clocknum;
    cdd copy = state;
    cdd res = cdd_false();
    ADBM(dbm);
    while (!cdd_isterminal(copy.handle()) && cdd_info(copy.handle())->type != TYPE_BDD) {
        copy = cdd_reduce(copy);
        cdd bottom = cdd_extract_bdd(copy, size);
        copy = cdd_extract_dbm(copy, dbm, size);
        copy = cdd_reduce(cdd_remove_negative(copy));
        dbm_down(dbm, size);
        res |= (cdd(dbm, size) & bottom);
    }
    return res;
}

cdd cdd_apply_reset(const cdd& state, int32_t* clock_resets, int32_t* clock_values, int32_t num_clock_resets, int32_t* bool_resets, int32_t* bool_values, int32_t num_bool_resets)
{
    uint32_t size = cdd_clocknum;
    //ADBM(dbm);
    cdd copy= state;
    copy = cdd_exist(copy, bool_resets, clock_resets, num_bool_resets,num_clock_resets);
    // Hint: if this quantifies a clock, the resulting CDD will include negative clock values

    // apply bool updates
    for (int i=bdd_start_level;i<bdd_start_level+cdd_varnum; i++)
    {
        if (bool_resets[i] == 1) { // TODO: FIX THIS TO THE NEW FORMAT OF LISTING RESETS
            if (bool_values[i]==1) {
                copy = cdd_apply(copy, cdd_bddvarpp(i), cddop_and);
            }
            else
            {
                copy = cdd_apply(copy, cdd_bddnvarpp(i), cddop_and);
            }
        }
    }
    copy = cdd_remove_negative(copy);
    copy = cdd_reduce(copy);
    // apply clock resets
    cdd res= cdd_true();
    for (int i = 0; i < num_clock_resets; i++) {

        ADBM(dbm_for_bounds);
        dbm_init(dbm_for_bounds, size);
        dbm_updateValue(dbm_for_bounds, size, clock_resets[i] , clock_values[i]);

        res &= (cdd(dbm_for_bounds,size));
    }
    res = res & copy;
/*    while (!cdd_isterminal(copy.root) && cdd_info(copy.root)->type != TYPE_BDD) {
        copy = cdd_remove_negative(copy);
        copy = cdd_reduce(copy);
        cdd bottom = cdd_extract_bdd(copy, size);
        copy = cdd_extract_dbm(copy, dbm, size);
        for (int i = 0; i < num_clock_resets; i++) {
                dbm_updateValue(dbm, size, clock_resets[i] , clock_values[i]);
        }
        res |= (cdd(dbm,size) & bottom);
    }*/

    return res;
}

cdd cdd_transition(const cdd& state, const cdd& guard, int32_t* clock_resets, int32_t* clock_values, int32_t num_clock_resets, int32_t* bool_resets, int32_t* bool_values,   int32_t num_bool_resets )
{
    uint32_t size = cdd_clocknum;
    ADBM(dbm);
    cdd copy= state;
    copy &= guard;
    copy = cdd_exist(copy, bool_resets, clock_resets, num_bool_resets,num_clock_resets);
    // Hint: if this quantifies a clock, the resulting CDD will include negative clock values

    for (int i=bdd_start_level;i<bdd_start_level+cdd_varnum; i++)
    {
        if (bool_resets[i] == 1) {
            if (bool_values[i]==1) {
                copy = cdd_apply(copy, cdd_bddvarpp(i), cddop_and);
            }
            else
            {
                copy = cdd_apply(copy, cdd_bddnvarpp(i), cddop_and);
            }
        }
    }

        cdd res= cdd_false();
        for (int i = 0; i < num_clock_resets; i++) {
            res = res & cdd_lowerpp(0, clock_resets[i], clock_values[i]);
            res = res & cdd_upperpp(0, clock_resets[i], clock_values[i]);
        }
        res = res & copy;
    /*cdd res= cdd_false();
    while (!cdd_isterminal(copy.root) && cdd_info(copy.root)->type != TYPE_BDD) {
        copy = cdd_remove_negative(copy);
        copy = cdd_reduce(copy);
        cdd bottom = cdd_extract_bdd(copy, size);
        copy = cdd_extract_dbm(copy, dbm, size);
        for (int i = 0; i < cdd_clocknum; i++) {
            for (int i = 0; i < num_clock_resets; i++) {
                dbm_updateValue(dbm, size, clock_resets[i] , clock_values[i]);
            }
        }
        res |= (cdd(dbm,size) & bottom);
    }*/
    return res;
}

cdd cdd_transition_back(const cdd&  state, const cdd& guard, const cdd& update, int32_t* clock_resets,  int32_t num_clock_resets, int32_t* bool_resets,  int32_t num_bool_resets)
{
    cdd copy= state;
    // TODO: sanity check: implement cdd_is_update();
    // assert(ccd_is_update(update));
    copy &= update;
    if (copy == cdd_false()) {
        return copy;
    }
    copy = cdd_exist(copy, bool_resets, clock_resets, num_bool_resets, num_clock_resets);
    copy = cdd_remove_negative(copy);
    copy &= guard;
    return copy;
}

cdd cdd_transition_back_past(const cdd&  state, const cdd& guard, const cdd& update, int32_t* clock_resets, int32_t num_clock_resets, int32_t* bool_resets, int32_t num_bool_resets)
{
    cdd result = cdd_transition_back(state,guard, update, clock_resets, num_clock_resets, bool_resets, num_bool_resets);
    return cdd_past(result);
}
