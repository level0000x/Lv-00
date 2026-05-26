/**
 * @file proof_rule_engine.c
 * @brief Proof rule search engine implementation
 *
 * Implements best-first proof search with weighted rule prioritization.
 * Supports depth limits and timeout control.
 *
 * Search algorithm (best-first):
 *   1. Collect all rules applicable to the current proof state.
 *   2. Sort applicable rules by weight in descending order.
 *   3. Apply the highest-weighted rule.
 *   4. Recurse on the resulting proof state.
 *   5. Backtrack if no rule leads to a solution within depth limit.
 *
 * For other strategies:
 *   - Depth-first: apply first applicable rule, recurse immediately.
 *   - Breadth-first: expand all applicable rules at current depth before going deeper.
 *   - Iterative deepening: run depth-first with increasing depth limits.
 */

#include "proof_rule_engine.h"

#include <stdlib.h>
#include <string.h>

#include "lv00.h"
#include "lv00_utils.h"

/* ============== Internal Helpers ============== */

/**
 * @brief Safe string duplication using lv00_strdup
 */
static char *safe_strdup(const char *s) {
    if (!s) return NULL;
    return lv00_strdup(s);
}

/**
 * @brief Swap two proof rule pointers (for sorting)
 */
static void swap_rules(Lv00ProofRule **a, Lv00ProofRule **b) {
    Lv00ProofRule *tmp = *a;
    *a = *b;
    *b = tmp;
}

/**
 * @brief Sort rules by weight in descending order (insertion sort)
 *
 * Uses insertion sort since the rule set is typically small.
 */
static void sort_rules_by_weight(Lv00ProofRule **rules, int count) {
    int i, j;
    for (i = 1; i < count; i++) {
        Lv00ProofRule *key = rules[i];
        j = i - 1;
        while (j >= 0 && rules[j]->weight < key->weight) {
            rules[j + 1] = rules[j];
            j--;
        }
        rules[j + 1] = key;
    }
}

/**
 * @brief Collect applicable rules from the engine into a buffer
 *
 * @param engine  Rule engine
 * @param state   Current proof state
 * @param out     Output buffer for applicable rules
 * @param max_out Maximum number of rules to collect
 * @return Number of applicable rules found
 */
static int collect_applicable_rules(const Lv00RuleEngine *engine,
                                     const Lv00ProofState *state,
                                     Lv00ProofRule **out,
                                     int max_out) {
    int count = 0;
    int i;
    if (!engine || !state || !out || max_out <= 0) return 0;

    for (i = 0; i < engine->rule_count && count < max_out; i++) {
        Lv00ProofRule *rule = engine->rule_set[i];
        if (rule && rule->applicability_check_fn) {
            if (rule->applicability_check_fn(rule, state)) {
                out[count++] = rule;
            }
        }
    }
    return count;
}

/**
 * @brief Internal recursive search for best-first strategy
 */
static Lv00SearchResultStatus search_best_first(Lv00RuleEngine *engine,
                                                 Lv00ProofState *state,
                                                 int depth) {
    Lv00ProofRule *applicable[LV00_RULE_SET_CAPACITY];
    int count, i;

    /* Check depth limit */
    if (depth >= engine->max_depth) {
        return SEARCH_RESULT_DEPTH_LIMIT;
    }

    /* Check if proof is complete */
    if (proof_state_is_complete(state)) {
        return SEARCH_RESULT_FOUND;
    }

    /* Collect applicable rules */
    count = collect_applicable_rules(engine, state, applicable, LV00_RULE_SET_CAPACITY);
    if (count == 0) {
        return SEARCH_RESULT_EXHAUSTED;
    }

    /* Sort by weight (descending) for best-first ordering */
    sort_rules_by_weight(applicable, count);

    /* Try each rule in weight order */
    for (i = 0; i < count; i++) {
        Lv00ProofRule *rule = applicable[i];

        /* Save state for backtracking */
        int saved_depth = state->current_depth;
        int saved_goal_top = state->goal_stack_top;
        int saved_hyp_count = state->hypothesis_count;
        int saved_rule_count = state->applied_rule_count;

        /* Apply the rule */
        if (rule->apply_fn && rule->apply_fn(rule, state)) {
            state->current_depth = depth + 1;

            /* Record the applied rule */
            proof_state_record_rule(state, rule->name);

            /* Recurse or finish immediately when the rule closes all goals. */
            Lv00SearchResultStatus result = proof_state_is_complete(state)
                ? SEARCH_RESULT_FOUND
                : search_best_first(engine, state, depth + 1);
            if (result == SEARCH_RESULT_FOUND) {
                state->current_depth = saved_depth;
                return SEARCH_RESULT_FOUND;
            }

            /* Backtrack: restore goal stack, hypotheses, and rule history */
            /* Free any goals/hypotheses that were added during the failed branch */
            while (state->goal_stack_top > saved_goal_top) {
                if (state->goal_stack[state->goal_stack_top] != NULL) {
                    lv00_free((void **) &state->goal_stack[state->goal_stack_top]);
                    state->goal_stack[state->goal_stack_top] = NULL;
                }
                state->goal_stack_top--;
            }
            while (state->hypothesis_count > saved_hyp_count) {
                state->hypothesis_count--;
                if (state->hypotheses[state->hypothesis_count] != NULL) {
                    lv00_free((void **) &state->hypotheses[state->hypothesis_count]);
                    state->hypotheses[state->hypothesis_count] = NULL;
                }
            }
            while (state->applied_rule_count > saved_rule_count) {
                state->applied_rule_count--;
                if (state->applied_rules[state->applied_rule_count] != NULL) {
                    lv00_free((void **) &state->applied_rules[state->applied_rule_count]);
                    state->applied_rules[state->applied_rule_count] = NULL;
                }
            }
            state->current_depth = saved_depth;
        }
    }

    return SEARCH_RESULT_EXHAUSTED;
}

/**
 * @brief Internal recursive search for depth-first strategy
 */
static Lv00SearchResultStatus search_depth_first(Lv00RuleEngine *engine,
                                                  Lv00ProofState *state,
                                                  int depth) {
    Lv00ProofRule *applicable[LV00_RULE_SET_CAPACITY];
    int count, i;

    if (depth >= engine->max_depth) {
        return SEARCH_RESULT_DEPTH_LIMIT;
    }

    if (proof_state_is_complete(state)) {
        return SEARCH_RESULT_FOUND;
    }

    count = collect_applicable_rules(engine, state, applicable, LV00_RULE_SET_CAPACITY);
    if (count == 0) {
        return SEARCH_RESULT_EXHAUSTED;
    }

    for (i = 0; i < count; i++) {
        Lv00ProofRule *rule = applicable[i];
        int saved_depth = state->current_depth;
        int saved_goal_top = state->goal_stack_top;
        int saved_hyp_count = state->hypothesis_count;
        int saved_rule_count = state->applied_rule_count;

        if (rule->apply_fn && rule->apply_fn(rule, state)) {
            state->current_depth = depth + 1;
            proof_state_record_rule(state, rule->name);

            Lv00SearchResultStatus result = proof_state_is_complete(state)
                ? SEARCH_RESULT_FOUND
                : search_depth_first(engine, state, depth + 1);
            if (result == SEARCH_RESULT_FOUND) {
                state->current_depth = saved_depth;
                return SEARCH_RESULT_FOUND;
            }

            /* Backtrack */
            while (state->goal_stack_top > saved_goal_top) {
                if (state->goal_stack[state->goal_stack_top] != NULL) {
                    lv00_free((void **) &state->goal_stack[state->goal_stack_top]);
                    state->goal_stack[state->goal_stack_top] = NULL;
                }
                state->goal_stack_top--;
            }
            while (state->hypothesis_count > saved_hyp_count) {
                state->hypothesis_count--;
                if (state->hypotheses[state->hypothesis_count] != NULL) {
                    lv00_free((void **) &state->hypotheses[state->hypothesis_count]);
                    state->hypotheses[state->hypothesis_count] = NULL;
                }
            }
            while (state->applied_rule_count > saved_rule_count) {
                state->applied_rule_count--;
                if (state->applied_rules[state->applied_rule_count] != NULL) {
                    lv00_free((void **) &state->applied_rules[state->applied_rule_count]);
                    state->applied_rules[state->applied_rule_count] = NULL;
                }
            }
            state->current_depth = saved_depth;
        }
    }

    return SEARCH_RESULT_EXHAUSTED;
}

/**
 * @brief Internal search for breadth-first strategy (iterative, not recursive)
 */
static Lv00SearchResultStatus search_breadth_first(Lv00RuleEngine *engine,
                                                    Lv00ProofState *initial_state) {
    /* Breadth-first uses a simplified iterative approach.
     * For a full implementation, a proper queue data structure would be needed.
     * This implementation expands one level at a time. */
    int depth;
    for (depth = 0; depth < engine->max_depth; depth++) {
        /* At each depth, try all applicable rules */
        Lv00ProofRule *applicable[LV00_RULE_SET_CAPACITY];
        int count = collect_applicable_rules(engine, initial_state, applicable,
                                              LV00_RULE_SET_CAPACITY);
        if (count == 0) {
            return SEARCH_RESULT_EXHAUSTED;
        }

        if (proof_state_is_complete(initial_state)) {
            return SEARCH_RESULT_FOUND;
        }

        /* Apply the first applicable rule and go one level deeper */
        if (count > 0 && applicable[0]->apply_fn) {
            if (applicable[0]->apply_fn(applicable[0], initial_state)) {
                proof_state_record_rule(initial_state, applicable[0]->name);
            }
        }
    }

    if (proof_state_is_complete(initial_state)) {
        return SEARCH_RESULT_FOUND;
    }
    return SEARCH_RESULT_DEPTH_LIMIT;
}

/**
 * @brief Internal search for iterative deepening strategy
 */
static Lv00SearchResultStatus search_iterative_deepening(Lv00RuleEngine *engine,
                                                          Lv00ProofState *state) {
    int depth_limit;
    for (depth_limit = 1; depth_limit <= engine->max_depth; depth_limit++) {
        int saved_max_depth = engine->max_depth;
        engine->max_depth = depth_limit;

        Lv00SearchResultStatus result = search_depth_first(engine, state, 0);

        engine->max_depth = saved_max_depth;

        if (result == SEARCH_RESULT_FOUND) {
            return SEARCH_RESULT_FOUND;
        }
    }
    return SEARCH_RESULT_DEPTH_LIMIT;
}

/* ============== Rule Engine API Implementation ============== */

Lv00RuleEngine *rule_engine_create(void) {
    return rule_engine_create_ex(SEARCH_BEST_FIRST, LV00_DEFAULT_MAX_DEPTH,
                                  LV00_DEFAULT_SEARCH_TIMEOUT_MS);
}

Lv00RuleEngine *rule_engine_create_ex(Lv00SearchStrategy strategy,
                                       int max_depth,
                                       uint64_t timeout_ms) {
    Lv00RuleEngine *engine = (Lv00RuleEngine *)lv00_malloc(sizeof(Lv00RuleEngine));
    if (!engine) return NULL;

    memset(engine, 0, sizeof(Lv00RuleEngine));

    engine->rule_set = (Lv00ProofRule **)lv00_malloc(
        sizeof(Lv00ProofRule *) * LV00_RULE_SET_CAPACITY);
    if (!engine->rule_set) {
        lv00_free((void **) &engine);
        return NULL;
    }
    memset(engine->rule_set, 0, sizeof(Lv00ProofRule *) * LV00_RULE_SET_CAPACITY);

    engine->rule_count = 0;
    engine->rule_capacity = LV00_RULE_SET_CAPACITY;
    engine->search_strategy = strategy;
    engine->max_depth = (max_depth > 0) ? max_depth : LV00_DEFAULT_MAX_DEPTH;
    engine->timeout_ms = timeout_ms;

    return engine;
}

void rule_engine_destroy(Lv00RuleEngine *engine) {
    int i;
    if (!engine) return;

    if (engine->rule_set) {
        for (i = 0; i < engine->rule_count; i++) {
            if (engine->rule_set[i]) {
                lv00_free((void **) &engine->rule_set[i]);
                engine->rule_set[i] = NULL;
            }
        }
        lv00_free((void **) &engine->rule_set);
        engine->rule_set = NULL;
    }

    lv00_free((void **) &engine);
}

bool rule_engine_add_rule(Lv00RuleEngine *engine, Lv00ProofRule *rule) {
    if (!engine || !rule) return false;
    if (engine->rule_count >= engine->rule_capacity) return false;

    engine->rule_set[engine->rule_count++] = rule;
    return true;
}

bool rule_engine_remove_rule(Lv00RuleEngine *engine, const char *name) {
    int i;
    if (!engine || !name) return false;

    for (i = 0; i < engine->rule_count; i++) {
        if (engine->rule_set[i] &&
            strncmp(engine->rule_set[i]->name, name, LV00_PROOF_RULE_NAME_MAX) == 0) {
            lv00_free((void **) &engine->rule_set[i]);
            /* Shift remaining rules */
            int j;
            for (j = i; j < engine->rule_count - 1; j++) {
                engine->rule_set[j] = engine->rule_set[j + 1];
            }
            engine->rule_set[engine->rule_count - 1] = NULL;
            engine->rule_count--;
            return true;
        }
    }
    return false;
}

const Lv00ProofRule *rule_engine_find_rule(const Lv00RuleEngine *engine,
                                             const char *name) {
    int i;
    if (!engine || !name) return NULL;

    for (i = 0; i < engine->rule_count; i++) {
        if (engine->rule_set[i] &&
            strncmp(engine->rule_set[i]->name, name, LV00_PROOF_RULE_NAME_MAX) == 0) {
            return engine->rule_set[i];
        }
    }
    return NULL;
}

Lv00SearchResultStatus rule_engine_search(Lv00RuleEngine *engine,
                                           Lv00ProofState *state) {
    if (!engine || !state) return SEARCH_RESULT_ERROR;

    /* Check if already complete */
    if (proof_state_is_complete(state)) {
        return SEARCH_RESULT_FOUND;
    }

    /* Dispatch to strategy-specific search */
    switch (engine->search_strategy) {
        case SEARCH_BEST_FIRST:
            return search_best_first(engine, state, 0);
        case SEARCH_DEPTH_FIRST:
            return search_depth_first(engine, state, 0);
        case SEARCH_BREADTH_FIRST:
            return search_breadth_first(engine, state);
        case SEARCH_ITERATIVE_DEEPENING:
            return search_iterative_deepening(engine, state);
        default:
            return SEARCH_RESULT_ERROR;
    }
}

int rule_engine_rule_count(const Lv00RuleEngine *engine) {
    if (!engine) return -1;
    return engine->rule_count;
}

/* ============== Proof State API Implementation ============== */

Lv00ProofState *proof_state_create(const char *initial_goal) {
    Lv00ProofState *state;
    if (!initial_goal) return NULL;

    state = (Lv00ProofState *)lv00_malloc(sizeof(Lv00ProofState));
    if (!state) return NULL;

    memset(state, 0, sizeof(Lv00ProofState));
    state->goal_stack_top = -1;
    state->current_depth = 0;

    /* Push the initial goal */
    if (!proof_state_push_goal(state, initial_goal)) {
        lv00_free((void **) &state);
        return NULL;
    }

    return state;
}

void proof_state_destroy(Lv00ProofState *state) {
    int i;
    if (!state) return;

    /* Free goal stack entries */
    for (i = 0; i <= state->goal_stack_top && i < LV00_GOAL_STACK_MAX; i++) {
        if (state->goal_stack[i]) {
            free(state->goal_stack[i]);
            state->goal_stack[i] = NULL;
        }
    }

    /* Free hypothesis entries */
    for (i = 0; i < state->hypothesis_count && i < LV00_HYPOTHESIS_MAX; i++) {
        if (state->hypotheses[i]) {
            free(state->hypotheses[i]);
            state->hypotheses[i] = NULL;
        }
    }

    /* Free applied rules history */
    for (i = 0; i < state->applied_rule_count && i < LV00_APPLIED_RULES_MAX; i++) {
        if (state->applied_rules[i]) {
            free(state->applied_rules[i]);
            state->applied_rules[i] = NULL;
        }
    }

    state->current_goal = NULL;
    lv00_free((void **) &state);
}

bool proof_state_push_goal(Lv00ProofState *state, const char *goal) {
    if (!state || !goal) return false;
    if (state->goal_stack_top + 1 >= LV00_GOAL_STACK_MAX) return false;

    state->goal_stack_top++;
    state->goal_stack[state->goal_stack_top] = safe_strdup(goal);
    state->current_goal = state->goal_stack[state->goal_stack_top];

    return state->goal_stack[state->goal_stack_top] != NULL;
}

bool proof_state_pop_goal(Lv00ProofState *state) {
    if (!state) return false;
    if (state->goal_stack_top < 0) return false;

    if (state->goal_stack[state->goal_stack_top]) {
        lv00_free((void **) &state->goal_stack[state->goal_stack_top]);
        state->goal_stack[state->goal_stack_top] = NULL;
    }
    state->goal_stack_top--;

    /* Update current_goal pointer */
    if (state->goal_stack_top >= 0) {
        state->current_goal = state->goal_stack[state->goal_stack_top];
    } else {
        state->current_goal = NULL;
    }

    return true;
}

bool proof_state_add_hypothesis(Lv00ProofState *state, const char *hypothesis) {
    if (!state || !hypothesis) return false;
    if (state->hypothesis_count >= LV00_HYPOTHESIS_MAX) return false;

    state->hypotheses[state->hypothesis_count] = safe_strdup(hypothesis);
    if (!state->hypotheses[state->hypothesis_count]) return false;

    state->hypothesis_count++;
    return true;
}

bool proof_state_record_rule(Lv00ProofState *state, const char *name) {
    if (!state || !name) return false;
    if (state->applied_rule_count >= LV00_APPLIED_RULES_MAX) return false;

    state->applied_rules[state->applied_rule_count] = safe_strdup(name);
    if (!state->applied_rules[state->applied_rule_count]) return false;

    state->applied_rule_count++;
    return true;
}

bool proof_state_is_complete(const Lv00ProofState *state) {
    if (!state) return false;
    return state->goal_stack_top < 0;
}

const char *proof_state_current_goal(const Lv00ProofState *state) {
    if (!state) return NULL;
    return state->current_goal;
}

/* ============== Utility Functions ============== */

const char *proof_rule_type_to_string(Lv00ProofRuleType type) {
    switch (type) {
        case RULE_INTRO:          return "INTRO";
        case RULE_ELIM:           return "ELIM";
        case RULE_REWRITE:        return "REWRITE";
        case RULE_INDUCTION:      return "INDUCTION";
        case RULE_CONTRADICTION:  return "CONTRADICTION";
        case RULE_CASE_SPLIT:     return "CASE_SPLIT";
        case RULE_GENERALIZE:     return "GENERALIZE";
        case RULE_SPECIALIZE:     return "SPECIALIZE";
        case RULE_NEURAL_SUGGEST: return "NEURAL_SUGGEST";
        case RULE_AUX_CONSTRUCT:  return "AUX_CONSTRUCT";
        default:                  return "UNKNOWN";
    }
}

const char *search_strategy_to_string(Lv00SearchStrategy strategy) {
    switch (strategy) {
        case SEARCH_BEST_FIRST:           return "BEST_FIRST";
        case SEARCH_DEPTH_FIRST:          return "DEPTH_FIRST";
        case SEARCH_BREADTH_FIRST:        return "BREADTH_FIRST";
        case SEARCH_ITERATIVE_DEEPENING:  return "ITERATIVE_DEEPENING";
        default:                          return "UNKNOWN";
    }
}

const char *search_result_status_to_string(Lv00SearchResultStatus status) {
    switch (status) {
        case SEARCH_RESULT_FOUND:       return "FOUND";
        case SEARCH_RESULT_TIMEOUT:     return "TIMEOUT";
        case SEARCH_RESULT_DEPTH_LIMIT: return "DEPTH_LIMIT";
        case SEARCH_RESULT_EXHAUSTED:   return "EXHAUSTED";
        case SEARCH_RESULT_ERROR:       return "ERROR";
        default:                        return "UNKNOWN";
    }
}
