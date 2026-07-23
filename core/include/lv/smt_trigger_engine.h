/**
 * @file smt_trigger_engine.h
 * @brief Quantifier instantiation engine based on pattern-matching triggers
 *
 * Implements E-matching based quantifier instantiation. The engine
 * maintains a set of triggers (patterns) and an instance cache to
 * avoid duplicate instantiations of quantified formulas.
 *
 * @version 1.1.0
 * @date 2026-05-25
 */

#ifndef lv_SMT_TRIGGER_ENGINE_H
#define lv_SMT_TRIGGER_ENGINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define lv_TRIGGER_MAX_PATTERNS 16

typedef struct {
    int pattern_ids[lv_TRIGGER_MAX_PATTERNS];
    int pattern_size;
    double weight;
} lvTrigger;

typedef struct {
    int quantifier_id;
    uint64_t binding_hash;
} lvInstanceEntry;

typedef struct {
    lvTrigger *triggers;
    int trigger_count;
    int trigger_capacity;

    lvInstanceEntry *instance_cache;
    int cache_count;
    int cache_capacity;

    int max_instances;
    int total_instantiations;
} lvTriggerEngine;

lvTriggerEngine *trigger_engine_create(int initial_trigger_count,
                                         int initial_cache_size,
                                         int max_instances);

void trigger_engine_destroy(lvTriggerEngine *engine);

int trigger_engine_add_pattern(lvTriggerEngine *engine,
                               const int *pattern_ids,
                               int pattern_size,
                               double weight);

bool trigger_engine_find_matches(lvTriggerEngine *engine,
                                 int quantifier_id,
                                 const void *ground_term,
                                 uint64_t term_hash,
                                 int *match_count);

void trigger_engine_clear_cache(lvTriggerEngine *engine);

int trigger_engine_get_instantiation_count(const lvTriggerEngine *engine);

#ifdef __cplusplus
}
#endif

#endif /* lv_SMT_TRIGGER_ENGINE_H */
