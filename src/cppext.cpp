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

#include "cdd/kernel.h"

#include <dbm/fed.h>
#include <dbm/print.h>

#define ADBM(NAME, DIM) raw_t* NAME = allocDBM(DIM)

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
    ADBM(dbm, cdd_clocknum);
    cdd bdd_part = cdd_extract_bdd(state, cdd_clocknum);
    cdd cdd_part = cdd_extract_dbm(state, dbm, cdd_clocknum);
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
    // First some trivial cases.
    if (cdd_isterminal(state.handle()))
        return state;
    if (cdd_info(state.handle())->type == TYPE_BDD)
        return state;

    cdd copy = state;
    cdd res = cdd_false();
    ADBM(dbm, cdd_clocknum);
    while (!cdd_isterminal(copy.handle()) && cdd_info(copy.handle())->type != TYPE_BDD) {
        copy = cdd_reduce(copy);
        cdd bottom = cdd_extract_bdd(copy, cdd_clocknum);
        copy = cdd_extract_dbm(copy, dbm, cdd_clocknum);
        copy = cdd_reduce(cdd_remove_negative(copy));
        dbm_up(dbm, cdd_clocknum);
        cdd fixed_cdd = cdd(dbm, cdd_clocknum);
        fixed_cdd &= bottom;
        res |= fixed_cdd;
    }
    return res;
}

/**
 * Construct a cdd from a federation.
 * @param fed a federation
 * @return the cdd representing \a fed.
 */
cdd cdd_from_fed(const dbm::fed_t& fed)
{
    dbm::fed_t copy = dbm::fed_t(fed);
    cdd res = cdd_false();
    for (auto& zone : fed) {
        res |= cdd(zone.const_dbm(), cdd_clocknum);
    }
    return res;
}

/**
 * Get the timed predecessor of the given (bad) target dbm and target bdd that cannot be
 * saved (i.e. reached) by delaying into \a safe.
 *
 * <p>If there is an overlap between \a target and \a safe, the overlap is considered
 * to be saved.</p>
 * @param dbm_target the target dbm
 * @param bdd_target the target bdd
 * @param safe the safe cdd
 * @return a cdd containing states that can delay into \a target without reaching \a safe.
 */
cdd cdd_predt_dbm(raw_t* dbm_target, cdd bdd_target, const cdd& safe)
{
    cdd result = cdd_false();

    // Check whether the safe has an overlapping BDD part with the target.
    cdd good_part_with_fitting_bools = bdd_target & safe;
    if (good_part_with_fitting_bools != cdd_false()) {
        // For each possible combination of boolean valuations,
        // compute the part of safe that overlaps with it.
        for (auto i = 0u; i < (1u << cdd_varnum); ++i) {
            cdd all_booleans = cdd_true();
            for (auto j = 0u; j < cdd_varnum; ++j) {
                bool current = (i & 1 << j) != 0;
                if (current) {
                    all_booleans &= cdd_bddvarpp(bdd_start_level + j);
                } else {
                    all_booleans &= cdd_bddnvarpp(bdd_start_level + j);
                }
            }

            // No need to test combinations that don't satisfy the bad part.
            if (cdd_equiv(all_booleans & bdd_target, cdd_false())) {
                continue;
            }

            // Paranoia check.
            assert(!cdd_eval_false(all_booleans & bdd_target));

            auto bad_fed = dbm::fed_t{dbm_target, (uint32_t)cdd_clocknum};
            ADBM(dbm_good, cdd_clocknum);
            cdd good_copy = good_part_with_fitting_bools & all_booleans;

            if (!cdd_eval_false(good_copy)) {
                auto good_fed = dbm::fed_t{(uint32_t)cdd_clocknum};
                while (!cdd_isterminal(good_copy.handle()) && cdd_info(good_copy.handle())->type != TYPE_BDD) {
                    extraction_result res_good = cdd_extract_bdd_and_dbm(good_copy);
                    good_copy = cdd_reduce(cdd_remove_negative(res_good.CDD_part));
                    free(dbm_good);
                    dbm_good = res_good.dbm;
                    cdd bdd_good = res_good.BDD_part;
                    good_fed.add(dbm_good, cdd_clocknum);
                }

                // If good_fed is empty, good_copy did not contain any DBM. This has the interpretation
                // of an unbounded DBM.
                if (good_fed.isEmpty()) {
                    dbm_init(dbm_good, cdd_clocknum);
                    good_fed.add(dbm_good, cdd_clocknum);
                }

                auto pred_fed = bad_fed.predt(good_fed);
                result |= cdd_from_fed(pred_fed) & all_booleans;

            } else {
                // For all boolean valuations we did not reach with our safe CDD, we take the past of the
                // current target DBM.
                ADBM(local, cdd_clocknum);
                dbm_copy(local, dbm_target, cdd_clocknum);
                dbm_down(local, cdd_clocknum);
                cdd past = cdd(local, cdd_clocknum) & all_booleans;
                result |= past;
                free(local);
            }
            free(dbm_good);
        }

    } else {
        // Safe does not have an overlapping part with the target BDD.
        // So the complete past of this DBM is bad.
        dbm_down(dbm_target, cdd_clocknum);
        cdd past = cdd(dbm_target, cdd_clocknum) & bdd_target;
        result |= past;
    }

    return result;
}

/**
 * Get the timed predecessor of the given (bad) \a target state that cannot be
 * saved (i.e. reached) by delaying into \a safe.
 *
 * <p>If there is an overlap between \a target and \a safe, the overlap is considered
 * to be saved.</p>
 * @param target the target cdd
 * @param safe the safe cdd
 * @return a cdd containing states that can delay into \a target without reaching \a safe.
 */
cdd cdd_predt(const cdd& target, const cdd& safe)
{
    // First some trivial cases.
    if (target == cdd_false())
        return target;
    if (safe == cdd_true())
        return cdd_false();

    cdd allThatKillsUs = cdd_false();
    cdd copy = target;
    ADBM(dbm_target, cdd_clocknum);

    if (cdd_isterminal(target.handle()) || cdd_info(target.handle())->type == TYPE_BDD) {
        dbm_init(dbm_target, cdd_clocknum);
        cdd result = cdd_predt_dbm(dbm_target, target, safe);
        free(dbm_target);
        return result;
    }

    // Split target into DBMs.
    while (!cdd_isterminal(copy.handle()) && cdd_info(copy.handle())->type != TYPE_BDD) {
        extraction_result res = cdd_extract_bdd_and_dbm(copy);
        copy = cdd_reduce(cdd_remove_negative(res.CDD_part));
        free(dbm_target);
        dbm_target = res.dbm;
        cdd bdd_target = res.BDD_part;

        allThatKillsUs |= cdd_predt_dbm(dbm_target, bdd_target, safe);
    }
    free(dbm_target);
    return allThatKillsUs;
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
    // First some trivial cases.
    if (cdd_isterminal(state.handle()))
        return state;
    if (cdd_info(state.handle())->type == TYPE_BDD)
        return state;

    cdd copy = state;
    cdd res = cdd_false();
    ADBM(dbm, cdd_clocknum);
    while (!cdd_isterminal(copy.handle()) && cdd_info(copy.handle())->type != TYPE_BDD) {
        copy = cdd_reduce(copy);
        cdd bottom = cdd_extract_bdd(copy, cdd_clocknum);
        copy = cdd_extract_dbm(copy, dbm, cdd_clocknum);
        copy = cdd_reduce(cdd_remove_negative(copy));
        dbm_down(dbm, cdd_clocknum);
        res |= (cdd(dbm, cdd_clocknum) & bottom);
    }
    return res;
}

/**
 * Apply clock and boolean variable resets.
 *
 * @param state cdd to be reset
 * @param clock_resets array of clock numbers that have to be reset
 * @param clock_values array of clock reset values, should match the size of \a clocks_resets
 * @param num_clock_resets the number of clock variables that have to be reset,
 *      should match the size of \a clocks_resets
 * @param bool_resets array of boolean node levels that have to be reset
 * @param bool_values array of boolean reset values, should match the size of \a bool_resets
 * @param num_bool_resets the number of boolean variables that have to be reset,
 *      should match the size of \a bool_resets
 * @return cdd where the supplied reset has been applied
 */
cdd cdd_apply_reset(const cdd& state, int32_t* clock_resets, int32_t* clock_values, int32_t num_clock_resets,
                    int32_t* bool_resets, int32_t* bool_values, int32_t num_bool_resets)
{
    cdd copy = state;

    // Apply bool resets.
    if (num_bool_resets > 0)
        copy = cdd_exist(copy, bool_resets, nullptr, num_bool_resets, 0);

    for (int i = 0; i < num_bool_resets; i++) {
        if (bool_values[i] == 1) {
            copy = cdd_apply(copy, cdd_bddvarpp(bool_resets[i]), cddop_and);
        } else {
            copy = cdd_apply(copy, cdd_bddnvarpp(bool_resets[i]), cddop_and);
        }
    }
    copy = cdd_remove_negative(copy);

    // Special cases when we are already done.
    if (num_clock_resets == 0)
        return copy;
    if (cdd_isterminal(copy.handle()))
        return copy;
    if (cdd_info(copy.handle())->type == TYPE_BDD)
        return copy;

    // Apply the clock resets.
    cdd res = cdd_false();
    while (!cdd_isterminal(copy.handle()) && cdd_info(copy.handle())->type != TYPE_BDD) {
        copy = cdd_reduce(copy);
        extraction_result exres = cdd_extract_bdd_and_dbm(copy);
        cdd bottom = exres.BDD_part;
        copy = cdd_reduce(cdd_remove_negative(exres.CDD_part));
        for (int i = 0; i < num_clock_resets; i++) {
            dbm_updateValue(exres.dbm, cdd_clocknum, clock_resets[i], clock_values[i]);
        }
        res |= (cdd(exres.dbm, cdd_clocknum) & bottom);
    }
    return res;
}

/**
 * Perform the execution of a transition.
 *
 * @param state cdd of the transition's source state
 * @param guard cdd of the guard
 * @param clock_resets array of clock numbers that have to be reset
 * @param clock_values array of clock reset values, should match the size of \a clocks_resets
 * @param num_clock_resets the number of clock variables that have to be reset,
 *      should match the size of \a clocks_resets
 * @param bool_resets array of boolean node levels that have to be reset
 * @param bool_values array of boolean reset values, should match the size of \a bool_resets
 * @param num_bool_resets the number of boolean variables that have to be reset,
 *      should match the size of \a bool_resets
 * @return cdd of the target state after taking the transition
 */
cdd cdd_transition(const cdd& state, const cdd& guard, int32_t* clock_resets, int32_t* clock_values,
                   int32_t num_clock_resets, int32_t* bool_resets, int32_t* bool_values, int32_t num_bool_resets)
{
    cdd copy = state;
    copy &= guard;
    return cdd_apply_reset(copy, clock_resets, clock_values, num_clock_resets, bool_resets, bool_values,
                           num_bool_resets);
}

/**
 * Perform the execution of a transition backwards.
 *
 * <p>The update cdd contains the strict conditions for the clock and boolean values
 * assignment. For example, `x == 0 and b == true` represents the update setting clock x
 * to 0 and boolean b to true.</p>
 *
 * @param state cdd of the transition's target state
 * @param guard cdd of the guard
 * @param update cdd of the update
 * @param clock_resets array of clock numbers that have to be reset
 * @param num_clock_resets the number of clock variables that have to be reset,
 *      should match the size of \a clocks_resets
 * @param bool_resets array of boolean node levels that have to be reset
 * @param num_bool_resets the number of boolean variables that have to be reset,
 *      should match the size of \a bool_resets
 * @return cdd of the immediate source state after taking the transition backwards
 */
cdd cdd_transition_back(const cdd& state, const cdd& guard, const cdd& update, int32_t* clock_resets,
                        int32_t num_clock_resets, int32_t* bool_resets, int32_t num_bool_resets)
{
    cdd copy = state;
    // TODO: sanity check: implement cdd_is_update();
    // assert(ccd_is_update(update));
    copy &= update;
    if (copy == cdd_false()) {
        return copy;
    }

    // Apply bool resets.
    if (num_bool_resets > 0)
        copy = cdd_exist(copy, bool_resets, nullptr, num_bool_resets, 0);

    // Special cases when we are already done.
    if (num_clock_resets == 0)
        return copy & guard;
    if (!cdd_isterminal(copy.handle()) && cdd_info(copy.handle())->type == TYPE_BDD)
        return copy & guard;

    // Apply the clock resets.
    cdd res = cdd_false();
    copy = cdd_remove_negative(copy);
    while (!cdd_isterminal(copy.handle()) && cdd_info(copy.handle())->type != TYPE_BDD) {
        copy = cdd_reduce(copy);
        extraction_result exres = cdd_extract_bdd_and_dbm(copy);
        cdd bottom = exres.BDD_part;
        copy = cdd_reduce(cdd_remove_negative(exres.CDD_part));
        for (int i = 0; i < num_clock_resets; i++) {
            dbm_freeClock(exres.dbm, cdd_clocknum, clock_resets[i]);
        }
        res |= (cdd(exres.dbm, cdd_clocknum) & bottom);
    }
    return res & guard;
}

/**
 * Perform the execution of a transition backwards and delay in the past. Thus the
 * resulting cdd represents the state that can either take the transition immediately
 * or delay first before taking the transition and then end up in the supplied target
 * state.
 *
 * <p>The update cdd contains the strict conditions for the clock and boolean values
 * assignment. For example, `x == 0 and b == true` represents the update setting clock x
 * to 0 and boolean b to true.</p>
 *
 * @param state cdd of the transition's target state
 * @param guard cdd of the guard
 * @param update cdd of the update
 * @param clock_resets array of clock numbers that have to be reset
 * @param num_clock_resets the number of clock variables that have to be reset,
 *      should match the size of \a clocks_resets
 * @param bool_resets array of boolean node levels that have to be reset
 * @param num_bool_resets the number of boolean variables that have to be reset,
 *      should match the size of \a bool_resets
 * @return cdd of the target state after taking the transition
 */
cdd cdd_transition_back_past(const cdd& state, const cdd& guard, const cdd& update, int32_t* clock_resets,
                             int32_t num_clock_resets, int32_t* bool_resets, int32_t num_bool_resets)
{
    cdd result =
        cdd_transition_back(state, guard, update, clock_resets, num_clock_resets, bool_resets, num_bool_resets);
    return cdd_past(result);
}
