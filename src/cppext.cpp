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
    free(dbm);
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
    free(dbm);
    return res;
}
/**
 * Checks if a CDD is a BDD.
 * @param state: The CDD to check.
 * @return <code>true</code> if the CDD is a BDD and <code>false</code> if it is not.
 */
bool cdd_isBDD(const cdd& state) { return cdd_isterminal(state.root) || cdd_info(state.handle())->type == TYPE_BDD; }

/**
 * Class for a 2D int32_t matrix where the number of rows is dynamic.
 */
class dynamic_two_dim_matrix
{
public:
    /**
     * Constructor for the \a dynamic_two_dim_matrix class.
     * @param num_cols the fixed number of columns.
     */
    dynamic_two_dim_matrix(int num_cols): num_cols{num_cols}, matrix(1), rows_to_be_ignored(1, false) {}

    /**
     * Add a new value at the end the current row as long as there is place left.
     * @param value the value to add.
     */
    void add_value_to_row(int32_t value)
    {
        assert(matrix.back().size() < num_cols);
        matrix.back().push_back(value);
    }

    /**
     * Change the current row to the next row. Resizing is taking place when necessary. The values
     * of the current row are copied to the next row up to and including the provided column.
     * @param copy_to_column copy the previous values up to this column index (including this column).
     */
    void next_row(int copy_to_column)
    {
        assert(copy_to_column < num_cols);
        assert(!matrix.empty());

        matrix.emplace_back(matrix.back().begin(), matrix.back().begin() + copy_to_column + 1);
        rows_to_be_ignored.push_back(false);
    }

    /**
     * Indicate that the current row needs to be ignored.
     */
    void ignore_current_row() { rows_to_be_ignored.back() = true; }

    /**
     * Delete the ignored rows from the matrix and shift the remaining rows upwards, i.e.,
     * if row \a is to be ignored, the new row \i of the matrix is row \i + 1;
     */
    void delete_ignored_rows()
    {
        int i = 0;
        matrix.erase(std::remove_if(matrix.begin(), matrix.end(),
                                    [&i, this](const auto& value) { return rows_to_be_ignored.at(i++); }),
                     matrix.end());
    }

    /**
     * Get the current matrix as a single dimensional array of size \a num_cols * (\a current_row + 1).
     * Entries that were not added will get the default value.
     * @return the matrix as a single row array.
     */
    int32_t* get_array(int32_t default_value)
    {
        auto* array = new int32_t[num_cols * (matrix.size())];

        for (size_t i = 0; i < matrix.size(); ++i) {
            const auto& row = matrix[i];         // Reference: just an alias, no copy
            auto target = array + i * num_cols;  // Common pointer value for writing to
            std::copy(row.begin(), row.end(), target);
            std::fill(target + row.size(), target + num_cols, default_value);  // Pad if necessary
        }

        return array;
    }

    /**
     * Get the current row size of the matrix.
     * @return the current row size.
     */
    uint32_t get_current_row_size() { return matrix.size(); }

private:
    std::vector<std::vector<int32_t>> matrix;
    std::vector<bool> rows_to_be_ignored;
    int num_cols;
};

/**
 * Transform a BDD recursively into an matrix representation.
 *
 * @param r a cdd node from the bdd.
 * @param varsMatrix the matrix containing the traces with the boolean variable id numbers.
 * @param valuesMatrix the matrix containing the traces with the boolean variable values.
 * @param current_step the current step number (= depth of the trace from the top BDD node).
 * @param negated Whether this node is reached by (an odd number of) negated node(s).
 * @param num_bools the number of boolean variables.
 */
void cdd_bdd_to_matrix_rec(ddNode* r, dynamic_two_dim_matrix& varsMatrix, dynamic_two_dim_matrix& valuesMatrix,
                           int32_t current_step, bool negated)
{
    // The four terminating cases.
    if (r == cddtrue && !negated) {
        // Nothing special has to be done.
        return;
    }
    if (r == cddtrue && negated) {
        // We cannot delete the row here already, as we might still copy part of the
        // row (trace) in the depth-first search. Therefore, we only mark the current row
        // as to be ignored.
        varsMatrix.ignore_current_row();
        valuesMatrix.ignore_current_row();
        return;
    }
    if (r == cddfalse && negated) {
        // Nothing special has to be done.
        return;
    }
    if (r == cddfalse && !negated) {
        // We cannot delete the row here already, as we might still copy part of the
        // row (trace) in the depth-first search. Therefore, we only mark the current row
        // as to be ignored.
        varsMatrix.ignore_current_row();
        valuesMatrix.ignore_current_row();
        return;
    }

    // We have not reached the end of a trace.
    assert(!cdd_isterminal(r));
    if (cdd_info(r)->type == TYPE_BDD) {
        bddNode* node = bdd_node(r);

        // First follow the true child of the BDD node.
        varsMatrix.add_value_to_row(node->level);
        valuesMatrix.add_value_to_row(1);
        cdd_bdd_to_matrix_rec(node->high, varsMatrix, valuesMatrix, current_step + 1, negated ^ cdd_is_negated(r));

        // Now follow the false child of the BDD node.
        varsMatrix.next_row(current_step);
        valuesMatrix.next_row(current_step - 1);
        valuesMatrix.add_value_to_row(0);
        cdd_bdd_to_matrix_rec(node->low, varsMatrix, valuesMatrix, current_step + 1, negated ^ cdd_is_negated(r));
    } else {
        printf("not called with a BDD node");
    }
}

/**
 * Transform a BDD into an array representation.
 *
 * @param state a cdd containing only BDD or terminal nodes
 * @return an array representation of the BDD.
 */
bdd_arrays cdd_bdd_to_array(const cdd& state)
{
    auto varsMatrix = dynamic_two_dim_matrix(cdd_varnum);
    auto valuesMatrix = dynamic_two_dim_matrix(cdd_varnum);

    // Perform the actual depth-first search.
    cdd_bdd_to_matrix_rec(state.handle(), varsMatrix, valuesMatrix, 0, false);

    // Delete ignored rows from the result.
    assert(varsMatrix.get_current_row_size() == valuesMatrix.get_current_row_size());
    varsMatrix.delete_ignored_rows();
    valuesMatrix.delete_ignored_rows();
    assert(varsMatrix.get_current_row_size() == valuesMatrix.get_current_row_size());

    int32_t default_value = -1;
    bdd_arrays arrays;
    arrays.vars = varsMatrix.get_array(default_value);
    arrays.values = valuesMatrix.get_array(default_value);
    arrays.numBools = cdd_varnum;
    arrays.numTraces = varsMatrix.get_current_row_size();

    // Check for the special case when no trace was effectively generated (or all traces ended in
    // the false terminal)
    if (arrays.vars[0] == default_value) {
        arrays.numTraces = 0;
    }

    return arrays;
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
        free(exres.dbm);
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
        free(exres.dbm);
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
