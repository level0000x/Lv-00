/**
 * @file smt_trigger_engine.h
 * @brief Quantifier instantiation engine based on pattern-matching triggers
 *
 * Implements E-matching based quantifier instantiation, inspired by the
 * trigger mechanisms in modern SMT solvers. When a quantified formula
 * contains patterns (triggers), the engine monitors ground terms and
 * instantiates the quantifier whenever a matching substitution is found.
 *
 * Design references:
 *   - Yices2: Multi-pattern triggers with relevance-based selection
 *   - Z3: E-matching with subterm sharing and caching
 *   - CVC5: Trigger selection heuristics and instantiation limits
 *
 * The engine maintains:
 *   - A set of registered triggers (patterns)
 *   - An instance cache to avoid duplicate instantiations
 *   - Weight-based priority for trigger selection
 *
 * @version v3.4.2
 * @date 2026-05-25
 */

#ifndef LV00_SMT_TRIGGER_ENGINE_H
#define LV00_SMT_TRIGGER_ENGINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "lv00.h"

/* ========================================================================
 * Trigger types
 * ======================================================================== */

/** Maximum number of pattern IDs per trigger */
#define LV00_TRIGGER_MAX_PATTERNS 16

/**
 * A single trigger (pattern) for quantifier instantiation.
 *
 * A trigger consists of a set of pattern IDs that must all be matched
 * simultaneously (conjunction). The weight determines selection priority
 * when multiple triggers could fire -- lower weight triggers are preferred.
 */
typedef struct Lv00Trigger {
    int pattern_ids[LV00_TRIGGER_MAX_PATTERNS]; /**< IDs of sub-patterns in this trigger */
    int pattern_size;                            /**< Number of active entries in pattern_ids */
    double weight;                               /**< Selection weight (lower = preferred) */
} Lv00Trigger;

/* ========================================================================
 * Instance cache entry
 * ======================================================================== */

/**
 * A cached instantiation to prevent duplicates.
 *
 * Each entry records the quantifier ID and the ground term substitution
 * that was used, identified by a hash of the binding.
 */
typedef struct Lv00InstanceEntry {
    int quantifier_id; /**< ID of the quantified formula */
    uint64_t binding_hash; /**< Hash of the ground term substitution */
} Lv00InstanceEntry;

/* ========================================================================
 * Trigger engine
 * ======================================================================== */

/**
 * Quantifier trigger engine.
 *
 * Manages a collection of triggers and an instance cache. When new ground
 * terms are added, the engine checks all triggers for potential matches
 * and generates instantiations for quantified formulas.
 */
typedef struct Lv00TriggerEngine {
    Lv00Trigger *triggers;           /**< Array of registered triggers */
    int trigger_count;               /**< Number of registered triggers */
    int trigger_capacity;            /**< Capacity of the triggers array */

    Lv00InstanceEntry *instance_cache; /**< Cache of already-generated instances */
    int cache_count;                 /**< Number of cached instances */
    int cache_capacity;              /**< Capacity of the instance cache */

    int max_instances;               /**< Maximum number of instances per quantifier */
    int total_instantiations;        /**< Running count of total instantiations generated */
} Lv00TriggerEngine;

/* ========================================================================
 * Lifecycle
 * ======================================================================== */

/**
 * @brief Create a new trigger engine
 *
 * @param[in] initial_trigger_count  Initial capacity for triggers
 * @param[in] initial_cache_size     Initial capacity for instance cache
 * @param[in] max_instances          Per-quantifier instantiation limit
 * @return New engine instance, or NULL on allocation failure
 */
LV00_PUBLIC_API Lv00TriggerEngine *trigger_engine_create(int initial_trigger_count,
                                                         int initial_cache_size,
                                                         int max_instances);

/**
 * @brief Destroy a trigger engine and free all resources
 *
 * @param[in,out] engine  The engine to destroy (may be NULL)
 */
LV00_PUBLIC_API void trigger_engine_destroy(Lv00TriggerEngine *engine);

/* ========================================================================
 * Pattern registration
 * ======================================================================== */

/**
 * @brief Add a pattern (trigger) to the engine
 *
 * Registers a new trigger with the given pattern IDs and weight.
 * The pattern_ids array is copied; the caller may free it afterwards.
 *
 * @param[in,out] engine       The trigger engine
 * @param[in]     pattern_ids  Array of pattern term IDs (non-NULL)
 * @param[in]     pattern_size Number of entries in pattern_ids (1..MAX_PATTERNS)
 * @param[in]     weight       Selection weight (lower = preferred)
 * @return Index of the newly added trigger (>= 0), or -1 on error
 */
LV00_PUBLIC_API int trigger_engine_add_pattern(Lv00TriggerEngine *engine,
                                               const int *pattern_ids,
                                               int pattern_size,
                                               double weight);

/* ========================================================================
 * Matching and instantiation
 * ======================================================================== */

/**
 * @brief Find all matching instantiations for a given ground term
 *
 * Scans all registered triggers and checks if any trigger's patterns
 * match the provided ground term. For each match, checks the instance
 * cache to avoid duplicates, then records the new instantiation.
 *
 * @param[in,out] engine        The trigger engine
 * @param[in]     quantifier_id ID of the quantified formula to instantiate
 * @param[in]     ground_term   Opaque handle to the ground term being matched
 * @param[in]     term_hash     Hash of the ground term for cache lookup
 * @param[out]    match_count   Number of new instantiations generated (may be NULL)
 * @return true if at least one new instantiation was found, false otherwise
 */
LV00_PUBLIC_API bool trigger_engine_find_matches(Lv00TriggerEngine *engine,
                                                 int quantifier_id,
                                                 const void *ground_term,
                                                 uint64_t term_hash,
                                                 int *match_count);

/**
 * @brief Clear the instance cache
 *
 * Removes all cached instances, allowing previously generated
 * instantiations to be reconsidered.
 *
 * @param[in,out] engine  The trigger engine
 */
LV00_PUBLIC_API void trigger_engine_clear_cache(Lv00TriggerEngine *engine);

/**
 * @brief Get the total number of instantiations generated
 *
 * @param[in] engine  The trigger engine (non-NULL)
 * @return Total instantiation count
 */
LV00_PUBLIC_API int trigger_engine_get_instantiation_count(const Lv00TriggerEngine *engine);

#ifdef __cplusplus
}
#endif

#endif /* LV00_SMT_TRIGGER_ENGINE_H */
