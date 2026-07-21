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

#ifndef LV00_SMT_THEORY_COMBINER_H
#define LV00_SMT_THEORY_COMBINER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LV00_THEORY_UF = 0,
    LV00_THEORY_LRA,
    LV00_THEORY_LIA,
    LV00_THEORY_NRA,
    LV00_THEORY_NIA,
    LV00_THEORY_BV,
    LV00_THEORY_ARRAYS,
    LV00_THEORY_QUANTIFIERS,
    LV00_THEORY_DATATYPES,
    LV00_THEORY_SETS,
    LV00_THEORY_STRINGS,
    LV00_THEORY_COUNT
} Lv00TheoryId;

typedef struct {
    bool satisfiable;
    bool timeout;
    double solve_time_ms;
} Lv00TheoryResult;

typedef Lv00TheoryResult (*Lv00TheorySolverFn)(void *context, const void *constraints);

typedef struct {
    Lv00TheoryId theory_id;
    int priority;
    Lv00TheorySolverFn solver_fn;
    void *solver_context;
    bool enabled;
} Lv00TheoryEntry;

typedef struct {
    Lv00TheoryEntry *entries;
    int entry_count;
    int entry_capacity;
    double timeout_ms;
} Lv00TheoryCombiner;

Lv00TheoryCombiner *smt_combiner_create(int initial_capacity, double timeout_ms);
void smt_combiner_destroy(Lv00TheoryCombiner *combiner);

int smt_combiner_add_theory(Lv00TheoryCombiner *combiner,
                             Lv00TheoryId theory_id,
                             int priority,
                             Lv00TheorySolverFn solver_fn,
                             void *solver_context);

int smt_combiner_set_enabled(Lv00TheoryCombiner *combiner,
                              Lv00TheoryId theory_id,
                              bool enabled);

Lv00TheoryResult smt_combiner_solve(const Lv00TheoryCombiner *combiner,
                                    const void *constraints);

#ifdef __cplusplus
}
#endif

#endif /* LV00_SMT_THEORY_COMBINER_H */
