/**
 * @file rewrite_strategy.c
 * @brief Implementation of the extended rewrite strategy engine.
 *
 * @details Implements innermost, outermost, parallel, and e-graph rewriting
 *          strategies. The engine performs string-based pattern matching and
 *          substitution, applying rules iteratively until a fixed point is
 *          reached or the iteration limit is exceeded.
 *
 *          Strategy details:
 *          - Innermost: Finds the rightmost, longest match (simulating innermost)
 *          - Outermost: Finds the leftmost match (simulating outermost)
 *          - Parallel: Applies all non-overlapping matches in one step
 *          - E-graph: Treats rules as equalities, accumulating all results
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date   2026-05-25
 */

#include "rewrite_strategy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * Internal helpers
 * ============================================================ */

/** Initial capacity for the rules array */
#define INITIAL_RULE_CAPACITY 16

/** Default maximum iterations if 0 is passed */
#define DEFAULT_MAX_ITERATIONS 1000

/**
 * @brief Duplicate a string using malloc.
 *
 * @param src  Source string
 * @return Newly allocated copy, or NULL on failure
 */
static char *str_dup(const char *src) {
    if (!src) return NULL;
    size_t len = strlen(src);
    char *dst = (char *)malloc(len + 1);
    if (dst) {
        memcpy(dst, src, len + 1);
    }
    return dst;
}

/**
 * @brief Find the first occurrence of pattern in text.
 *
 * @param text     The text to search in
 * @param pattern  The pattern to search for
 * @return Pointer to the first match, or NULL if not found
 */
static const char *find_first_match(const char *text, const char *pattern) {
    if (!text || !pattern || !*pattern) return NULL;
    size_t plen = strlen(pattern);
    size_t tlen = strlen(text);
    if (plen > tlen) return NULL;
    for (size_t i = 0; i <= tlen - plen; i++) {
        if (memcmp(text + i, pattern, plen) == 0) {
            return text + i;
        }
    }
    return NULL;
}

/**
 * @brief Find the last occurrence of pattern in text (innermost simulation).
 *
 * @param text     The text to search in
 * @param pattern  The pattern to search for
 * @return Pointer to the last match, or NULL if not found
 */
static const char *find_last_match(const char *text, const char *pattern) {
    if (!text || !pattern || !*pattern) return NULL;
    size_t plen = strlen(pattern);
    size_t tlen = strlen(text);
    if (plen > tlen) return NULL;
    const char *result = NULL;
    for (size_t i = 0; i <= tlen - plen; i++) {
        if (memcmp(text + i, pattern, plen) == 0) {
            result = text + i;
        }
    }
    return result;
}

/**
 * @brief Apply a single substitution to a string.
 *
 * Replaces the first (or last, depending on strategy) occurrence of pattern
 * with replacement in the text.
 *
 * @param text         The input text
 * @param pattern      The pattern to replace
 * @param replacement  The replacement string
 * @param use_last     If true, replace the last match; otherwise the first
 * @return Newly allocated string with the substitution applied, or NULL on failure
 */
static char *apply_substitution(const char *text, const char *pattern,
    const char *replacement, bool use_last) {
    if (!text || !pattern || !*pattern) return str_dup(text);

    const char *match = use_last
        ? find_last_match(text, pattern)
        : find_first_match(text, pattern);

    if (!match) return str_dup(text);

    size_t plen = strlen(pattern);
    size_t rlen = strlen(replacement);
    size_t prefix_len = (size_t)(match - text);
    size_t suffix_len = strlen(match + plen);
    size_t new_len = prefix_len + rlen + suffix_len;

    char *result = (char *)malloc(new_len + 1);
    if (!result) return NULL;

    memcpy(result, text, prefix_len);
    memcpy(result + prefix_len, replacement, rlen);
    memcpy(result + prefix_len + rlen, match + plen, suffix_len + 1);

    return result;
}

/**
 * @brief Apply a single rule to a term.
 *
 * @param term    The input term
 * @param rule    The rule to apply
 * @param use_last  If true, match innermost (last); otherwise outermost (first)
 * @return Newly allocated string with the rule applied, or NULL if no match
 */
static char *apply_single_rule(const char *term, const Lv00RewriteRuleEx *rule,
    bool use_last) {
    if (!term || !rule || !rule->pattern) return NULL;

    /* Check condition if present */
    if (rule->condition_fn && !rule->condition_fn(term)) {
        return NULL;
    }

    const char *match = use_last
        ? find_last_match(term, rule->pattern)
        : find_first_match(term, rule->pattern);

    if (!match) return NULL;

    size_t plen = strlen(rule->pattern);
    size_t rlen = strlen(rule->replacement);
    size_t prefix_len = (size_t)(match - term);
    size_t suffix_len = strlen(match + plen);
    size_t new_len = prefix_len + rlen + suffix_len;

    char *result = (char *)malloc(new_len + 1);
    if (!result) return NULL;

    memcpy(result, term, prefix_len);
    memcpy(result + prefix_len, rule->replacement, rlen);
    memcpy(result + prefix_len + rlen, match + plen, suffix_len + 1);

    return result;
}

/**
 * @brief Apply parallel rewriting: apply all non-overlapping matches in one step.
 *
 * @param term   The input term
 * @param rules  Array of rules
 * @param count  Number of rules
 * @return Newly allocated string with all applicable rules applied, or NULL
 */
static char *apply_parallel_rules(const char *term, const Lv00RewriteRuleEx *rules,
    size_t count) {
    if (!term) return NULL;

    char *current = str_dup(term);
    if (!current) return NULL;

    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t i = 0; i < count; i++) {
            if (!rules[i].pattern || !*rules[i].pattern) continue;
            if (rules[i].condition_fn && !rules[i].condition_fn(current)) continue;

            const char *match = find_first_match(current, rules[i].pattern);
            if (match) {
                char *next = apply_substitution(current, rules[i].pattern,
                    rules[i].replacement, false);
                if (next) {
                    if (strcmp(next, current) != 0) {
                        changed = true;
                    }
                    free(current);
                    current = next;
                }
            }
        }
    }

    return current;
}

/**
 * @brief Apply e-graph rewriting: treat rules as equalities, accumulate all variants.
 *
 * In this simplified implementation, e-graph mode applies all rules exhaustively
 * and returns the canonical (lexicographically smallest) result.
 *
 * @param term   The input term
 * @param rules  Array of rules
 * @param count  Number of rules
 * @param max_iter  Maximum iterations
 * @return Newly allocated string with the canonical form
 */
static char *apply_egraph_rules(const char *term, const Lv00RewriteRuleEx *rules,
    size_t count, int max_iter) {
    if (!term) return NULL;

    char *best = str_dup(term);
    if (!best) return NULL;

    for (int iter = 0; iter < max_iter; iter++) {
        bool any_change = false;
        for (size_t i = 0; i < count; i++) {
            if (!rules[i].pattern || !*rules[i].pattern) continue;
            if (rules[i].condition_fn && !rules[i].condition_fn(best)) continue;

            char *next = apply_substitution(best, rules[i].pattern,
                rules[i].replacement, false);
            if (next && strcmp(next, best) != 0) {
                any_change = true;
                /* Keep the lexicographically smallest variant */
                if (strcmp(next, best) < 0) {
                    free(best);
                    best = next;
                } else {
                    free(next);
                }
            } else if (next) {
                free(next);
            }
        }
        if (!any_change) break;
    }

    return best;
}

/**
 * @brief Sort rules by priority (ascending: lower priority value = higher precedence).
 *
 * @param rules  Array of rules
 * @param count  Number of rules
 */
static void sort_rules_by_priority(Lv00RewriteRuleEx *rules, size_t count) {
    for (size_t i = 1; i < count; i++) {
        Lv00RewriteRuleEx key = rules[i];
        size_t j = i;
        while (j > 0 && rules[j - 1].priority > key.priority) {
            rules[j] = rules[j - 1];
            j--;
        }
        rules[j] = key;
    }
}

/* ============================================================
 * API implementation: Engine lifecycle
 * ============================================================ */

Lv00RewriteEngineEx *rewrite_engine_ex_create(Lv00RewriteStrategyEx strategy,
    int max_iterations) {
    Lv00RewriteEngineEx *engine = (Lv00RewriteEngineEx *)malloc(sizeof(Lv00RewriteEngineEx));
    if (!engine) return NULL;

    engine->rules = (Lv00RewriteRuleEx *)malloc(
        INITIAL_RULE_CAPACITY * sizeof(Lv00RewriteRuleEx));
    if (!engine->rules) {
        free(engine);
        return NULL;
    }

    engine->rule_count = 0;
    engine->rule_capacity = INITIAL_RULE_CAPACITY;
    engine->strategy = strategy;
    engine->max_iterations = (max_iterations > 0) ? max_iterations : DEFAULT_MAX_ITERATIONS;

    return engine;
}

void rewrite_engine_ex_destroy(Lv00RewriteEngineEx *engine) {
    if (!engine) return;

    /* Free each rule's owned strings */
    for (size_t i = 0; i < engine->rule_count; i++) {
        free((char *)engine->rules[i].name);
        free((char *)engine->rules[i].pattern);
        free((char *)engine->rules[i].replacement);
    }

    free(engine->rules);
    free(engine);
}

/* ============================================================
 * API implementation: Rule management
 * ============================================================ */

bool rewrite_engine_ex_add_rule(Lv00RewriteEngineEx *engine,
    const char *name, const char *pattern, const char *replacement,
    int priority, Lv00RewriteConditionFn condition) {
    if (!engine || !name || !pattern || !replacement) return false;

    /* Grow array if needed */
    if (engine->rule_count >= engine->rule_capacity) {
        size_t new_cap = engine->rule_capacity * 2;
        Lv00RewriteRuleEx *new_rules = (Lv00RewriteRuleEx *)lv00_realloc(
            engine->rules, new_cap * sizeof(Lv00RewriteRuleEx));
        if (!new_rules) return false;
        engine->rules = new_rules;
        engine->rule_capacity = new_cap;
    }

    Lv00RewriteRuleEx *rule = &engine->rules[engine->rule_count];
    rule->name = str_dup(name);
    rule->pattern = str_dup(pattern);
    rule->replacement = str_dup(replacement);
    rule->priority = priority;
    rule->condition_fn = condition;

    if (!rule->name || !rule->pattern || !rule->replacement) {
        free((char *)rule->name);
        free((char *)rule->pattern);
        free((char *)rule->replacement);
        return false;
    }

    engine->rule_count++;

    /* Keep rules sorted by priority */
    sort_rules_by_priority(engine->rules, engine->rule_count);

    return true;
}

/* ============================================================
 * API implementation: Rewrite execution
 * ============================================================ */

bool rewrite_engine_ex_apply(Lv00RewriteEngineEx *engine,
    const char *input, Lv00RewriteResultEx *result) {
    if (!engine || !input || !result) return false;

    result->output = NULL;
    result->iterations = 0;
    result->converged = false;
    result->hit_limit = false;

    if (engine->rule_count == 0) {
        result->output = str_dup(input);
        result->converged = true;
        return true;
    }

    /* Sort rules by priority */
    sort_rules_by_priority(engine->rules, engine->rule_count);

    switch (engine->strategy) {
        case REWRITE_INNERMOST: {
            char *current = str_dup(input);
            if (!current) return false;

            for (int i = 0; i < engine->max_iterations; i++) {
                bool any_applied = false;
                for (size_t r = 0; r < engine->rule_count; r++) {
                    char *next = apply_single_rule(current, &engine->rules[r], true);
                    if (next) {
                        if (strcmp(next, current) != 0) {
                            any_applied = true;
                            free(current);
                            current = next;
                            break; /* Restart from highest priority rule */
                        }
                        free(next);
                    }
                }
                result->iterations = i + 1;
                if (!any_applied) {
                    result->converged = true;
                    break;
                }
            }
            if (!result->converged) {
                result->hit_limit = true;
            }
            result->output = current;
            return true;
        }

        case REWRITE_OUTERMOST: {
            char *current = str_dup(input);
            if (!current) return false;

            for (int i = 0; i < engine->max_iterations; i++) {
                bool any_applied = false;
                for (size_t r = 0; r < engine->rule_count; r++) {
                    char *next = apply_single_rule(current, &engine->rules[r], false);
                    if (next) {
                        if (strcmp(next, current) != 0) {
                            any_applied = true;
                            free(current);
                            current = next;
                            break; /* Restart from highest priority rule */
                        }
                        free(next);
                    }
                }
                result->iterations = i + 1;
                if (!any_applied) {
                    result->converged = true;
                    break;
                }
            }
            if (!result->converged) {
                result->hit_limit = true;
            }
            result->output = current;
            return true;
        }

        case REWRITE_PARALLEL: {
            char *current = str_dup(input);
            if (!current) return false;

            for (int i = 0; i < engine->max_iterations; i++) {
                char *next = apply_parallel_rules(current, engine->rules, engine->rule_count);
                if (!next) {
                    result->iterations = i + 1;
                    result->converged = true;
                    break;
                }
                if (strcmp(next, current) == 0) {
                    free(next);
                    result->iterations = i + 1;
                    result->converged = true;
                    break;
                }
                free(current);
                current = next;
                result->iterations = i + 1;
            }
            if (!result->converged) {
                result->hit_limit = true;
            }
            result->output = current;
            return true;
        }

        case REWRITE_EGRAPH: {
            char *current = apply_egraph_rules(input, engine->rules,
                engine->rule_count, engine->max_iterations);
            if (!current) return false;
            result->output = current;
            result->converged = true;
            result->iterations = 1;
            return true;
        }

        default:
            return false;
    }
}

void rewrite_engine_result_ex_destroy(Lv00RewriteResultEx *result) {
    if (!result) return;
    free(result->output);
    result->output = NULL;
}
