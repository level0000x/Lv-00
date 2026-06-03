/**
 * @file gappa_propagate.h
 * @brief Predicate propagation engine for Gappa-style proofs
 *
 * @details Implements forward and backward propagation of interval predicates
 *          to automatically derive new facts from hypotheses and goals.
 *
 *          Forward propagation: from known hypotheses, derive new predicates
 *          that follow by interval arithmetic.
 *
 *          Backward propagation: from a goal, determine what additional
 *          hypotheses would be needed to prove it.
 *
 *          Design references:
 *          - Gappa (gappa.gitlabpages.inria.fr) -- predicate propagation
 *          - Abstract interpretation -- forward/backward dataflow analysis
 *
 * @version 3.4.0
 * @date 2026-05-25
 */

#ifndef LV00_GAPPA_PROPAGATE_H
#define LV00_GAPPA_PROPAGATE_H

#include <stdbool.h>
#include <stddef.h>
#include "lv00.h"
#include "gappa_dsl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Predicate set
 * ======================================================================== */

/** Maximum number of predicates in a set */
#define LV00_PRED_SET_MAX_SIZE 256

/**
 * @brief A set of predicates representing known facts.
 */
typedef struct {
    Lv00GappaPredicate preds[LV00_PRED_SET_MAX_SIZE]; /**< Predicate array */
    int count;                                         /**< Number of predicates */
} Lv00GappaPredSet;

/* ========================================================================
 * Propagation configuration
 * ======================================================================== */

/**
 * @brief Configuration for the propagation engine.
 */
typedef struct {
    int max_iterations;       /**< Maximum forward propagation iterations */
    int max_backward_depth;   /**< Maximum backward reasoning depth */
    double contraction_eps;   /**< Epsilon for interval contraction */
    int enable_backward;      /**< Enable backward reasoning */
} Lv00GappaPropagateConfig;

/* ========================================================================
 * Predicate set operations
 * ======================================================================== */

/**
 * @brief Initialize an empty predicate set.
 */
LV00_PUBLIC_API void gappa_pred_set_init(Lv00GappaPredSet *set);

/**
 * @brief Add a predicate to the set.
 *
 * If an equivalent predicate already exists, the set is unchanged.
 *
 * @return true if added, false if set is full or predicate already exists.
 */
LV00_PUBLIC_API bool gappa_pred_set_add(Lv00GappaPredSet *set, const Lv00GappaPredicate *pred);

/**
 * @brief Find a predicate for a given variable name in the set.
 *
 * Searches for BND predicates whose expr_lhs matches the given name.
 *
 * @param set      Predicate set
 * @param var_name Variable name to search for
 * @param out      Output predicate (if found)
 * @return Index of the found predicate, or -1 if not found.
 */
LV00_PUBLIC_API int gappa_pred_set_find(Lv00GappaPredSet *set, const char *var_name, Lv00GappaPredicate *out);

/**
 * @brief Get predicate at a given index.
 *
 * @return Pointer to the predicate, or NULL if index is out of range.
 */
LV00_PUBLIC_API const Lv00GappaPredicate *gappa_pred_set_get(const Lv00GappaPredSet *set, int index);

/**
 * @brief Clear all predicates from the set.
 */
LV00_PUBLIC_API void gappa_pred_set_clear(Lv00GappaPredSet *set);

/* ========================================================================
 * Propagation engine
 * ======================================================================== */

/**
 * @brief Run forward propagation on a predicate set.
 *
 * Starting from the initial hypotheses in the input set, derives new
 * predicates by applying interval arithmetic rules. Iterates until
 * no new predicates can be derived or max_iterations is reached.
 *
 * @param input_set   Initial set of hypotheses
 * @param output_set  Output set with all derived predicates
 * @param config      Propagation configuration (NULL for defaults)
 * @return Number of new predicates derived, or -1 on error.
 */
LV00_PUBLIC_API int gappa_propagate(
    const Lv00GappaPredSet *input_set,
    Lv00GappaPredSet *output_set,
    const Lv00GappaPropagateConfig *config);

/**
 * @brief Run backward propagation from a goal.
 *
 * Given a goal predicate, attempts to determine what additional
 * hypotheses would be needed to prove it. The required hypotheses
 * are added to the output set.
 *
 * @param goal         The goal predicate to prove
 * @param known_facts  Currently known facts
 * @param output_set   Output set with required additional hypotheses
 * @param config       Propagation configuration (NULL for defaults)
 * @return Number of new hypotheses generated, or -1 on error.
 */
LV00_PUBLIC_API int gappa_propagate_backward(
    const Lv00GappaPredicate *goal,
    const Lv00GappaPredSet *known_facts,
    Lv00GappaPredSet *output_set,
    const Lv00GappaPropagateConfig *config);

/* ========================================================================
 * Rewrite rules
 * ======================================================================== */

/** Maximum number of rewrite rules */
#define LV00_MAX_REWRITE_RULES 64

/**
 * @brief A rewrite rule for predicate simplification.
 */
typedef struct {
    char match_pattern[256];    /**< Pattern to match (simplified) */
    char replace_pattern[256];  /**< Replacement pattern */
    char description[128];      /**< Human-readable description */
} Lv00GappaRewriteRule;

/**
 * @brief Register rewrite rules for predicate simplification.
 *
 * Rewrite rules are applied during propagation to simplify predicates
 * and enable more deductions.
 *
 * @param rules     Array of rewrite rules
 * @param rule_count Number of rules
 * @return true if all rules were registered, false on error.
 */
LV00_PUBLIC_API bool gappa_register_rewrite_rules(
    const Lv00GappaRewriteRule *rules,
    int rule_count);

/**
 * @brief Get default propagation configuration.
 */
LV00_PUBLIC_API Lv00GappaPropagateConfig gappa_propagate_config_default(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_GAPPA_PROPAGATE_H */
