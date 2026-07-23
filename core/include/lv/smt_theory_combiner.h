/**
 * @file smt_theory_combiner.h
 * @brief SMT theory combination dispatcher
 *
 * Implements a simple serial theory dispatcher. Theories are stored in
 * a priority-sorted array and tried one at a time until a definitive
 * SAT/UNSAT result is obtained or all theories have been exhausted.
 *
 * @version 1.1.0
 * @date 2026-05-25
 */

#ifndef lv_SMT_THEORY_COMBINER_H
#define lv_SMT_THEORY_COMBINER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    lv_THEORY_UF = 0,
    lv_THEORY_LRA,
    lv_THEORY_LIA,
    lv_THEORY_NRA,
    lv_THEORY_NIA,
    lv_THEORY_BV,
    lv_THEORY_ARRAYS,
    lv_THEORY_QUANTIFIERS,
    lv_THEORY_DATATYPES,
    lv_THEORY_SETS,
    lv_THEORY_STRINGS,
    lv_THEORY_COUNT
} lvTheoryId;

typedef struct {
    bool satisfiable;
    bool timeout;
    double solve_time_ms;
} lvTheoryResult;

typedef lvTheoryResult (*lvTheorySolverFn)(void *context, const void *constraints);

typedef struct {
    lvTheoryId theory_id;
    int priority;
    lvTheorySolverFn solver_fn;
    void *solver_context;
    bool enabled;
} lvTheoryEntry;

typedef struct {
    lvTheoryEntry *entries;
    int entry_count;
    int entry_capacity;
    double timeout_ms;
} lvTheoryCombiner;

lvTheoryCombiner *smt_combiner_create(int initial_capacity, double timeout_ms);
void smt_combiner_destroy(lvTheoryCombiner *combiner);

bool smt_combiner_add_theory(lvTheoryCombiner *combiner, lvTheoryId theory_id, int priority, lvTheorySolverFn solver_fn,
                             void *solver_context);

bool smt_combiner_set_enabled(lvTheoryCombiner *combiner, lvTheoryId theory_id, bool enabled);

lvTheoryResult smt_combiner_solve(const lvTheoryCombiner *combiner, const void *constraints);

#ifdef __cplusplus
}
#endif

#endif /* lv_SMT_THEORY_COMBINER_H */
