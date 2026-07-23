/**
 * @file smt_trigger_engine.c
 * @brief Quantifier instantiation engine based on pattern-matching triggers
 *
 * Implements E-matching based quantifier instantiation. The engine
 * maintains a set of triggers (patterns) and an instance cache to
 * avoid duplicate instantiations of quantified formulas.
 *
 * Matching strategy:
 *   - Each trigger is a conjunction of pattern IDs
 *   - When a ground term is presented, the engine checks all triggers
 *     for potential matches
 *   - New instantiations are recorded in the instance cache
 *   - A per-quantifier limit prevents resource exhaustion
 *
 * Design references:
 *   - Yices2: Multi-pattern triggers with relevance-based selection
 *   - Z3: E-matching with subterm sharing and instantiation caching
 *
 * @version v3.4.2
 * @date 2026-05-25
 */

#include "smt_trigger_engine.h"
#include "lv/lv_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * Internal helpers
 * ======================================================================== */

/** Default initial capacity for triggers */
#define DEFAULT_TRIGGER_CAPACITY 16

/** Default initial capacity for instance cache */
#define DEFAULT_CACHE_CAPACITY 64

/** Default maximum instances per quantifier */
#define DEFAULT_MAX_INSTANCES 1000

/**
 * @brief Simple hash combining for cache keys
 *
 * Combines a quantifier ID and a term hash into a single cache key.
 */
static uint64_t combine_cache_key(int quantifier_id, uint64_t term_hash) {
    uint64_t key = (uint64_t) quantifier_id;
    key = key * 31ULL + term_hash;
    return key;
}

/**
 * @brief Check if an instance is already in the cache
 *
 * @return true if the (quantifier_id, binding_hash) pair is cached
 */
static bool cache_contains(const lvTriggerEngine *engine,
                           int quantifier_id,
                           uint64_t binding_hash) {
    if (!engine)
        return false;

    uint64_t key = combine_cache_key(quantifier_id, binding_hash);
    for (int i = 0; i < engine->cache_count; i++) {
        if (engine->instance_cache[i].binding_hash == key)
            return true;
    }
    return false;
}

/**
 * @brief Count how many instances exist for a given quantifier
 */
static int count_instances_for_quantifier(const lvTriggerEngine *engine,
                                          int quantifier_id) {
    if (!engine)
        return 0;

    int count = 0;
    for (int i = 0; i < engine->cache_count; i++) {
        if (engine->instance_cache[i].quantifier_id == quantifier_id)
            count++;
    }
    return count;
}

/**
 * @brief Ensure the trigger array has room for at least one more trigger
 */
static bool ensure_trigger_capacity(lvTriggerEngine *engine) {
    if (!engine)
        return false;

    if (engine->trigger_count < engine->trigger_capacity)
        return true;

    int new_capacity = engine->trigger_capacity * 2;
    if (new_capacity < DEFAULT_TRIGGER_CAPACITY)
        new_capacity = DEFAULT_TRIGGER_CAPACITY;

    lvTrigger *new_triggers = (lvTrigger *) lv_realloc(
        engine->triggers, (size_t) new_capacity * sizeof(lvTrigger));
    if (!new_triggers)
        return false;

    engine->triggers = new_triggers;
    engine->trigger_capacity = new_capacity;
    return true;
}

/**
 * @brief Ensure the instance cache has room for at least one more entry
 */
static bool ensure_cache_capacity(lvTriggerEngine *engine) {
    if (!engine)
        return false;

    if (engine->cache_count < engine->cache_capacity)
        return true;

    int new_capacity = engine->cache_capacity * 2;
    if (new_capacity < DEFAULT_CACHE_CAPACITY)
        new_capacity = DEFAULT_CACHE_CAPACITY;

    lvInstanceEntry *new_cache = (lvInstanceEntry *) lv_realloc(
        engine->instance_cache, (size_t) new_capacity * sizeof(lvInstanceEntry));
    if (!new_cache)
        return false;

    engine->instance_cache = new_cache;
    engine->cache_capacity = new_capacity;
    return true;
}

/* ========================================================================
 * Lifecycle
 * ======================================================================== */

lvTriggerEngine *trigger_engine_create(int initial_trigger_count,
                                         int initial_cache_size,
                                         int max_instances) {
    if (initial_trigger_count <= 0)
        initial_trigger_count = DEFAULT_TRIGGER_CAPACITY;
    if (initial_cache_size <= 0)
        initial_cache_size = DEFAULT_CACHE_CAPACITY;
    if (max_instances <= 0)
        max_instances = DEFAULT_MAX_INSTANCES;

    lvTriggerEngine *engine =
        (lvTriggerEngine *) malloc(sizeof(lvTriggerEngine));
    if (!engine)
        return NULL;

    engine->triggers = (lvTrigger *) calloc(
        (size_t) initial_trigger_count, sizeof(lvTrigger));
    if (!engine->triggers) {
        free(engine);
        return NULL;
    }

    engine->instance_cache = (lvInstanceEntry *) calloc(
        (size_t) initial_cache_size, sizeof(lvInstanceEntry));
    if (!engine->instance_cache) {
        free(engine->triggers);
        free(engine);
        return NULL;
    }

    engine->trigger_count = 0;
    engine->trigger_capacity = initial_trigger_count;
    engine->cache_count = 0;
    engine->cache_capacity = initial_cache_size;
    engine->max_instances = max_instances;
    engine->total_instantiations = 0;

    return engine;
}

void trigger_engine_destroy(lvTriggerEngine *engine) {
    if (!engine)
        return;

    free(engine->triggers);
    engine->triggers = NULL;
    free(engine->instance_cache);
    engine->instance_cache = NULL;

    engine->trigger_count = 0;
    engine->trigger_capacity = 0;
    engine->cache_count = 0;
    engine->cache_capacity = 0;

    free(engine);
}

/* ========================================================================
 * Pattern registration
 * ======================================================================== */

int trigger_engine_add_pattern(lvTriggerEngine *engine,
                               const int *pattern_ids,
                               int pattern_size,
                               double weight) {
    if (!engine || !pattern_ids || pattern_size <= 0 || pattern_size > lv_TRIGGER_MAX_PATTERNS)
        return -1;

    if (!ensure_trigger_capacity(engine))
        return -1;

    lvTrigger *trigger = &engine->triggers[engine->trigger_count];
    memcpy(trigger->pattern_ids, pattern_ids, (size_t) pattern_size * sizeof(int));
    trigger->pattern_size = pattern_size;
    trigger->weight = weight;

    int index = engine->trigger_count;
    engine->trigger_count++;
    return index;
}

/* ========================================================================
 * Matching and instantiation
 * ======================================================================== */

bool trigger_engine_find_matches(lvTriggerEngine *engine,
                                 int quantifier_id,
                                 const void *ground_term,
                                 uint64_t term_hash,
                                 int *match_count) {
    if (!engine)
        return false;

    /* (void) ground_term: in this implementation, matching is hash-based.
     * A full implementation would perform structural E-matching on the
     * ground_term against each trigger's patterns. Here we use the
     * term_hash as a proxy for pattern matching. */
    (void) ground_term;

    int new_matches = 0;
    bool found_any = false;

    /* Check per-quantifier instantiation limit */
    int current_count = count_instances_for_quantifier(engine, quantifier_id);
    if (current_count >= engine->max_instances) {
        if (match_count)
            *match_count = 0;
        return false;
    }

    /*
     * Iterate over all triggers. In a full E-matching implementation,
     * each trigger would be checked against the ground term's subterms.
     * Here we simulate matching by checking if the term_hash could
     * correspond to any trigger's patterns.
     */
    for (int t = 0; t < engine->trigger_count; t++) {
        const lvTrigger *trigger = &engine->triggers[t];

        /*
         * Simulated match: we treat the term_hash as a potential match
         * for the first pattern in each trigger. A real implementation
         * would use a pattern index (e.g., discrimination tree) for
         * efficient lookup.
         */
        uint64_t trigger_key = (uint64_t) (trigger->pattern_ids[0] + 1);
        uint64_t combined = combine_cache_key(quantifier_id, term_hash ^ trigger_key);

        if (cache_contains(engine, quantifier_id, combined))
            continue;

        /* Check instantiation limit */
        if (current_count + new_matches >= engine->max_instances)
            break;

        /* Record the new instance */
        if (!ensure_cache_capacity(engine))
            break;

        engine->instance_cache[engine->cache_count].quantifier_id = quantifier_id;
        engine->instance_cache[engine->cache_count].binding_hash = combined;
        engine->cache_count++;
        engine->total_instantiations++;
        new_matches++;
        found_any = true;
    }

    if (match_count)
        *match_count = new_matches;

    return found_any;
}

void trigger_engine_clear_cache(lvTriggerEngine *engine) {
    if (!engine)
        return;

    engine->cache_count = 0;
    /* Note: total_instantiations is NOT reset; it tracks the lifetime total */
}

int trigger_engine_get_instantiation_count(const lvTriggerEngine *engine) {
    if (!engine)
        return 0;
    return engine->total_instantiations;
}
