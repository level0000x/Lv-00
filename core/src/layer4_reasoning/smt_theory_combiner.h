/**
 * @file smt_theory_combiner.h
 * @brief SMT theory combination dispatcher
 *
 * Provides a simple theory dispatcher that selects and serially invokes
 * theory solvers based on constraint types. Inspired by Alt-Ergo's
 * CDCL(T) theory combination architecture and Yices2's EF-solving
 * approach.
 *
 * The combiner maintains a priority queue of registered theories and
 * dispatches constraints to the most appropriate solver. Theories are
 * tried serially until one returns a definitive result (SAT/UNSAT).
 *
 * Design references:
 *   - Alt-Ergo: CDCL(T) with theory propagation and conflict-driven learning
 *   - Yices2: Equalities + Functions (EF) solving with eager splitting
 *   - STP: Bitvector theory with hierarchical encoding
 *
 * @version v3.4.2
 * @date 2026-05-25
 */

#ifndef LV00_SMT_THEORY_COMBINER_H
#define LV00_SMT_THEORY_COMBINER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "lv00.h"

/* ========================================================================
 * Theory identifiers
 * ======================================================================== */

/** Supported SMT theory identifiers */
typedef enum {
    THEORY_QF_LIA = 0,    /**< Quantifier-free Linear Integer Arithmetic */
    THEORY_QF_LRA = 1,    /**< Quantifier-free Linear Real Arithmetic */
    THEORY_QF_BV = 2,     /**< Quantifier-free Bitvectors */
    THEORY_QF_AUFNIA = 3, /**< Quantifier-free Arrays, Uninterpreted Functions,
                               Non-linear Integer Arithmetic */
    THEORY_QF_AX = 4,     /**< Quantifier-free Arrays with Extensionality */
    THEORY_COUNT = 5      /**< Number of supported theories (sentinel) */
} Lv00TheoryId;

/* ========================================================================
 * Theory result
 * ======================================================================== */

/** Result returned by a theory solver invocation */
typedef struct Lv00TheoryResult {
    bool satisfiable;     /**< true if the theory found the constraints satisfiable */
    bool timeout;         /**< true if the solver timed out */
    double solve_time_ms; /**< Wall-clock time spent solving, in milliseconds */
} Lv00TheoryResult;

/* ========================================================================
 * Theory solver interface
 * ======================================================================== */

/** Function pointer type for a theory solver */
typedef Lv00TheoryResult (*Lv00TheorySolverFn)(void *context, const void *constraints);

/** Per-theory registration entry in the combiner */
typedef struct Lv00TheoryEntry {
    Lv00TheoryId theory_id;       /**< Theory identifier */
    int priority;                 /**< Dispatch priority (lower = higher priority) */
    Lv00TheorySolverFn solver_fn; /**< Solver function pointer */
    void *solver_context;         /**< Opaque context passed to solver_fn */
    bool enabled;                 /**< Whether this theory is active */
} Lv00TheoryEntry;

/* ========================================================================
 * Theory combiner
 * ======================================================================== */

/**
 * Theory combination dispatcher.
 *
 * Maintains an ordered list of theory solvers and dispatches constraints
 * by trying each enabled theory in priority order until a definitive
 * result is obtained.
 */
typedef struct Lv00TheoryCombiner {
    Lv00TheoryEntry *entries; /**< Array of registered theory entries */
    int entry_count;          /**< Number of registered entries */
    int entry_capacity;       /**< Capacity of the entries array */
    double timeout_ms;        /**< Per-theory timeout in milliseconds */
} Lv00TheoryCombiner;

/* ========================================================================
 * Lifecycle
 * ======================================================================== */

/**
 * @brief Create a new theory combiner
 *
 * Allocates and initializes a theory combiner with the given initial
 * capacity and default timeout.
 *
 * @param[in] initial_capacity  Initial number of theory slots
 * @param[in] timeout_ms        Per-theory timeout in milliseconds (0 = no timeout)
 * @return New combiner instance, or NULL on allocation failure
 */
LV00_PUBLIC_API Lv00TheoryCombiner *smt_combiner_create(int initial_capacity, double timeout_ms);

/**
 * @brief Destroy a theory combiner and free all resources
 *
 * Does not free solver_context pointers; the caller is responsible
 * for managing those lifetimes.
 *
 * @param[in,out] combiner  The combiner to destroy (may be NULL)
 */
LV00_PUBLIC_API void smt_combiner_destroy(Lv00TheoryCombiner *combiner);

/* ========================================================================
 * Theory registration
 * ======================================================================== */

/**
 * @brief Register a theory solver with the combiner
 *
 * The theory is inserted in priority order. If a theory with the same
 * ID already exists, it is replaced.
 *
 * @param[in,out] combiner        The combiner to register with
 * @param[in]     theory_id       Theory identifier
 * @param[in]     priority        Dispatch priority (lower = tried first)
 * @param[in]     solver_fn       Solver function pointer
 * @param[in]     solver_context  Opaque context for the solver
 * @return true on success, false on error
 */
LV00_PUBLIC_API bool smt_combiner_add_theory(Lv00TheoryCombiner *combiner,
                                             Lv00TheoryId theory_id,
                                             int priority,
                                             Lv00TheorySolverFn solver_fn,
                                             void *solver_context);

/**
 * @brief Enable or disable a registered theory
 *
 * @param[in,out] combiner   The combiner
 * @param[in]     theory_id  Theory identifier to toggle
 * @param[in]     enabled    true to enable, false to disable
 * @return true if the theory was found and updated, false otherwise
 */
LV00_PUBLIC_API bool smt_combiner_set_enabled(Lv00TheoryCombiner *combiner,
                                              Lv00TheoryId theory_id,
                                              bool enabled);

/* ========================================================================
 * Solving
 * ======================================================================== */

/**
 * @brief Solve constraints using the registered theory solvers
 *
 * Dispatches constraints to each enabled theory in priority order.
 * Returns the first definitive (non-timeout) result. If all theories
 * time out, returns the last result with timeout=true.
 *
 * @param[in] combiner     The combiner
 * @param[in] constraints  Opaque constraint data (interpretation is solver-specific)
 * @return Theory result (satisfiable, timeout, solve_time_ms)
 */
LV00_PUBLIC_API Lv00TheoryResult smt_combiner_solve(const Lv00TheoryCombiner *combiner,
                                                    const void *constraints);

#ifdef __cplusplus
}
#endif

#endif /* LV00_SMT_THEORY_COMBINER_H */
