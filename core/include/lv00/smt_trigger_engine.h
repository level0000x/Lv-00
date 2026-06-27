/**
 * @file smt_trigger_engine.h
 * @brief Quantifier instantiation engine based on pattern-matching triggers
 *
 * Implements E-matching based quantifier instantiation. The engine
 * maintains a set of triggers (patterns) and an instance cache to
 * avoid duplicate instantiations of quantified formulas.
 *
 * @version v3.4.2
 * @date 2026-05-25
 */

#ifndef LV00_SMT_TRIGGER_ENGINE_H
#define LV00_SMT_TRIGGER_ENGINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LV00_TRIGGER_MAX_PATTERNS 16

typedef struct {
    int pattern_ids[LV00_TRIGGER_MAX_PATTERNS];
    int pattern_size;
    double weight;
} Lv00Trigger;

typedef struct {
    int quantifier_id;
    uint64_t binding_hash;
} Lv00InstanceEntry;

typedef struct {
    Lv00Trigger *triggers;
    int trigger_count;
    int trigger_capacity;

    Lv00InstanceEntry *instance_cache;
    int cache_count;
    int cache_capacity;

    int max_instances;
    int total_instantiations;
} Lv00TriggerEngine;

Lv00TriggerEngine *trigger_engine_create(int initial_trigger_count,
                                         int initial_cache_size,
                                         int max_instances);

void trigger_engine_destroy(Lv00TriggerEngine *engine);

int trigger_engine_add_pattern(Lv00TriggerEngine *engine,
                               const int *pattern_ids,
                               int pattern_size,
                               double weight);

bool trigger_engine_find_matches(Lv00TriggerEngine *engine,
                                 int quantifier_id,
                                 const void *ground_term,
                                 uint64_t term_hash,
                                 int *match_count);

void trigger_engine_clear_cache(Lv00TriggerEngine *engine);

int trigger_engine_get_instantiation_count(const Lv00TriggerEngine *engine);

#ifdef __cplusplus
}
#endif

#endif /* LV00_SMT_TRIGGER_ENGINE_H */
