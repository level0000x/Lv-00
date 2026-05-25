/**
 * @file rewrite_strategy.h
 * @brief Extended rewrite strategy engine with multiple strategies
 *
 * @details Provides an extended rewrite engine supporting innermost, outermost,
 *          parallel, and e-graph based rewriting strategies. Inspired by:
 *          - egg (e-graph rewriting in Rust)
 *          - Maude (rewriting logic)
 *          - K Framework (matching logic)
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date   2026-05-25
 */

#ifndef LV00_REWRITE_STRATEGY_H
#define LV00_REWRITE_STRATEGY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lv00.h"

/* ============================================================
 * Rewrite strategy enumeration
 * ============================================================ */

/**
 * @brief Extended rewrite strategies for term/graph rewriting.
 *
 * - REWRITE_INNERMOST: Apply rules at the innermost (deepest) redex first.
 *   Guarantees termination for non-overlapping left-linear rules.
 *
 * - REWRITE_OUTERMOST: Apply rules at the outermost redex first.
 *   Often more efficient for non-terminating or large term spaces.
 *
 * - REWRITE_PARALLEL: Apply all matching rules simultaneously in each step.
 *   Inspired by Maude's parallel rewriting semantics.
 *
 * - REWRITE_EGRAPH: Maintain an e-graph (equivalence graph) and apply
 *   rewrite rules as equalities, merging equivalent nodes.
 *   Inspired by the egg e-graph rewriting library.
 */
typedef enum Lv00RewriteStrategyEx {
    REWRITE_INNERMOST = 0,
    REWRITE_OUTERMOST = 1,
    REWRITE_PARALLEL  = 2,
    REWRITE_EGRAPH    = 3
} Lv00RewriteStrategyEx;

/* ============================================================
 * Rewrite rule (extended)
 * ============================================================ */

/**
 * @brief Condition function type for conditional rewrite rules.
 *
 * A condition function receives the current term string and returns
 * true if the rule should be applied, false otherwise.
 *
 * @param term  The current term being matched (null-terminated string)
 * @return true if the condition is satisfied
 */
typedef bool (*Lv00RewriteConditionFn)(const char *term);

/**
 * @brief Extended rewrite rule with priority and condition support.
 *
 * Each rule has a name, a pattern string, a replacement string,
 * a numeric priority (lower = higher priority), and an optional
 * condition function.
 */
typedef struct Lv00RewriteRuleEx {
    const char            *name;          /**< Rule name (owned reference, freed on destroy) */
    const char            *pattern;       /**< Pattern string to match (owned) */
    const char            *replacement;   /**< Replacement string (owned) */
    int                    priority;      /**< Priority: lower value = higher priority */
    Lv00RewriteConditionFn condition_fn;  /**< Optional condition; NULL means unconditional */
} Lv00RewriteRuleEx;

/* ============================================================
 * Rewrite engine (extended)
 * ============================================================ */

/**
 * @brief Extended rewrite engine with configurable strategy.
 *
 * Holds a collection of rewrite rules and a strategy selector.
 * Supports innermost, outermost, parallel, and e-graph rewriting.
 */
typedef struct Lv00RewriteEngineEx {
    Lv00RewriteRuleEx    *rules;          /**< Array of rewrite rules (dynamically allocated) */
    size_t                rule_count;     /**< Number of rules currently registered */
    size_t                rule_capacity;  /**< Allocated capacity for rules */
    Lv00RewriteStrategyEx strategy;       /**< Active rewriting strategy */
    int                   max_iterations; /**< Maximum rewrite iterations (safety limit) */
} Lv00RewriteEngineEx;

/* ============================================================
 * Rewrite result
 * ============================================================ */

/**
 * @brief Result of a rewrite engine application.
 */
typedef struct Lv00RewriteResultEx {
    char   *output;         /**< Resulting term after rewriting (caller must free) */
    int     iterations;     /**< Number of iterations performed */
    bool    converged;      /**< true if a fixed point was reached */
    bool    hit_limit;      /**< true if max_iterations was reached without convergence */
} Lv00RewriteResultEx;

/* ============================================================
 * API: Engine lifecycle
 * ============================================================ */

/**
 * @brief Create a new extended rewrite engine.
 *
 * @param strategy       The rewriting strategy to use
 * @param max_iterations Maximum number of rewrite iterations (safety limit, 0 = 1000)
 * @return Pointer to the new engine, or NULL on allocation failure
 */
LV00_PUBLIC_API Lv00RewriteEngineEx *rewrite_engine_ex_create(
    Lv00RewriteStrategyEx strategy, int max_iterations);

/**
 * @brief Destroy an extended rewrite engine and free all resources.
 *
 * @param engine The engine to destroy (may be NULL)
 */
LV00_PUBLIC_API void rewrite_engine_ex_destroy(Lv00RewriteEngineEx *engine);

/* ============================================================
 * API: Rule management
 * ============================================================ */

/**
 * @brief Add a rewrite rule to the engine.
 *
 * The engine takes ownership of the name, pattern, and replacement strings.
 * These strings will be freed when the engine is destroyed.
 *
 * @param engine      The rewrite engine
 * @param name        Rule name (will be duplicated internally)
 * @param pattern     Pattern string (will be duplicated internally)
 * @param replacement Replacement string (will be duplicated internally)
 * @param priority    Rule priority (lower = higher priority)
 * @param condition   Optional condition function (NULL for unconditional)
 * @return true on success, false on failure (NULL engine or allocation failure)
 */
LV00_PUBLIC_API bool rewrite_engine_ex_add_rule(Lv00RewriteEngineEx *engine,
    const char *name, const char *pattern, const char *replacement,
    int priority, Lv00RewriteConditionFn condition);

/* ============================================================
 * API: Rewrite execution
 * ============================================================ */

/**
 * @brief Apply the rewrite engine to a term.
 *
 * Iteratively applies matching rules to the input term according to the
 * configured strategy until either a fixed point is reached or the
 * maximum iteration count is exceeded.
 *
 * @param engine  The rewrite engine
 * @param input   The input term string (null-terminated)
 * @param result  Pointer to a result structure to populate (caller must free result->output)
 * @return true on success, false on failure (NULL engine or NULL input)
 */
LV00_PUBLIC_API bool rewrite_engine_ex_apply(Lv00RewriteEngineEx *engine,
    const char *input, Lv00RewriteResultEx *result);

/**
 * @brief Free resources held by a rewrite result.
 *
 * @param result The result to free (may be NULL)
 */
LV00_PUBLIC_API void rewrite_engine_result_ex_destroy(Lv00RewriteResultEx *result);

#ifdef __cplusplus
}
#endif

#endif /* LV00_REWRITE_STRATEGY_H */
