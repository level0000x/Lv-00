/**
 * @file smt_theory_combiner.c
 * @brief SMT theory combination dispatcher implementation
 *
 * Implements a simple serial theory dispatcher. Theories are stored in
 * a priority-sorted array and tried one at a time until a definitive
 * SAT/UNSAT result is obtained or all theories have been exhausted.
 *
 * Inspired by Alt-Ergo's CDCL(T) architecture where the SAT core
 * delegates theory checks to specialized solvers, and Yices2's
 * eager approach of trying simpler theories first.
 *
 * @version v3.4.2
 * @date 2026-05-25
 */

#include "smt_theory_combiner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_utils.h"

/* ========================================================================
 * Internal helpers
 * ======================================================================== */

/** Default initial capacity for the theory entry array */
#define DEFAULT_CAPACITY 8

/**
 * @brief Compare two theory entries by priority (for qsort)
 *
 * Lower priority value = higher dispatch precedence.
 */
static int compare_entries_by_priority(const void *a, const void *b) {
    const lvTheoryEntry *ea = (const lvTheoryEntry *) a;
    const lvTheoryEntry *eb = (const lvTheoryEntry *) b;
    int c = lv_cmp_int_asc(ea->priority, eb->priority);
    if (c != 0)
        return c;
    /* Tie-break by theory ID for deterministic ordering */
    return lv_cmp_int_asc((int) ea->theory_id, (int) eb->theory_id);
}

/**
 * @brief Find the index of a theory entry by theory ID
 *
 * @return Index of the entry, or -1 if not found
 */
static int find_entry_index(const lvTheoryCombiner *combiner, lvTheoryId theory_id) {
    if (!combiner)
        return -1;
    for (int i = 0; i < combiner->entry_count; i++) {
        if (combiner->entries[i].theory_id == theory_id)
            return i;
    }
    return -1;
}

/**
 * @brief Ensure the entries array has room for at least one more entry
 *
 * @return true on success, false on allocation failure
 */
static bool ensure_capacity(lvTheoryCombiner *combiner) {
    if (!combiner)
        return false;

    /* Unified growth via lv_ensure_capacity (overflow-checked doubling; initial capacity == DEFAULT_CAPACITY == 8) */
    return lv_ensure_capacity((void **) &combiner->entries, combiner->entry_count, &combiner->entry_capacity,
                              sizeof(lvTheoryEntry), 0);
}

/* ========================================================================
 * Lifecycle
 * ======================================================================== */

lvTheoryCombiner *smt_combiner_create(int initial_capacity, double timeout_ms) {
    if (initial_capacity <= 0)
        initial_capacity = DEFAULT_CAPACITY;

    lvTheoryCombiner *combiner = (lvTheoryCombiner *) lv_malloc(sizeof(lvTheoryCombiner));
    if (!combiner)
        return NULL;

    combiner->entries = (lvTheoryEntry *) lv_calloc((size_t) initial_capacity, sizeof(lvTheoryEntry));
    if (!combiner->entries) {
        lv_free((void **) &combiner);
        return NULL;
    }

    combiner->entry_count = 0;
    combiner->entry_capacity = initial_capacity;
    combiner->timeout_ms = timeout_ms;

    return combiner;
}

void smt_combiner_destroy(lvTheoryCombiner *combiner) {
    if (!combiner)
        return;

    lv_free((void **) &combiner->entries);
    combiner->entry_count = 0;
    combiner->entry_capacity = 0;

    lv_free((void **) &combiner);
}

/* ========================================================================
 * Theory registration
 * ======================================================================== */

bool smt_combiner_add_theory(lvTheoryCombiner *combiner, lvTheoryId theory_id, int priority, lvTheorySolverFn solver_fn,
                             void *solver_context) {
    if (!combiner || !solver_fn)
        return false;

    /* Check for existing entry with the same theory ID */
    int existing_idx = find_entry_index(combiner, theory_id);
    if (existing_idx >= 0) {
        /* Update the existing entry */
        combiner->entries[existing_idx].priority = priority;
        combiner->entries[existing_idx].solver_fn = solver_fn;
        combiner->entries[existing_idx].solver_context = solver_context;
        combiner->entries[existing_idx].enabled = true;

        /* Re-sort by priority */
        qsort(combiner->entries, (size_t) combiner->entry_count, sizeof(lvTheoryEntry), compare_entries_by_priority);
        return true;
    }

    /* Add a new entry */
    if (!ensure_capacity(combiner))
        return false;

    lvTheoryEntry *entry = &combiner->entries[combiner->entry_count];
    entry->theory_id = theory_id;
    entry->priority = priority;
    entry->solver_fn = solver_fn;
    entry->solver_context = solver_context;
    entry->enabled = true;
    combiner->entry_count++;

    /* Re-sort by priority */
    qsort(combiner->entries, (size_t) combiner->entry_count, sizeof(lvTheoryEntry), compare_entries_by_priority);

    return true;
}

bool smt_combiner_set_enabled(lvTheoryCombiner *combiner, lvTheoryId theory_id, bool enabled) {
    if (!combiner)
        return false;

    int idx = find_entry_index(combiner, theory_id);
    if (idx < 0)
        return false;

    combiner->entries[idx].enabled = enabled;
    return true;
}

/* ========================================================================
 * Solving
 * ======================================================================== */

lvTheoryResult smt_combiner_solve(const lvTheoryCombiner *combiner, const void *constraints) {
    lvTheoryResult result;
    result.satisfiable = false;
    result.timeout = true;
    result.solve_time_ms = 0.0;

    if (!combiner || !constraints)
        return result;

    /*
     * Serial dispatch: try each enabled theory in priority order.
     * Return the first definitive (non-timeout) result.
     * If all theories time out, return the last result with timeout=true.
     */
    for (int i = 0; i < combiner->entry_count; i++) {
        const lvTheoryEntry *entry = &combiner->entries[i];
        if (!entry->enabled || !entry->solver_fn)
            continue;

        result = entry->solver_fn(entry->solver_context, constraints);

        /* If this theory gave a definitive answer, return immediately */
        if (!result.timeout)
            return result;
    }

    /* All theories timed out or none were enabled */
    return result;
}
