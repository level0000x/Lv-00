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

#include "lv_internal.h"

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
    if (!src)
        return NULL;
    size_t len = strlen(src);
    char *dst = (char *) lv_malloc(len + 1);
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
    if (!text || !pattern || !*pattern)
        return NULL;
    size_t plen = strlen(pattern);
    size_t tlen = strlen(text);
    if (plen > tlen)
        return NULL;
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
    if (!text || !pattern || !*pattern)
        return NULL;
    size_t plen = strlen(pattern);
    size_t tlen = strlen(text);
    if (plen > tlen)
        return NULL;
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
static char *apply_substitution(const char *text, const char *pattern, const char *replacement, bool use_last) {
    if (!text || !pattern || !*pattern)
        return str_dup(text);

    const char *match = use_last ? find_last_match(text, pattern) : find_first_match(text, pattern);

    if (!match)
        return str_dup(text);

    size_t plen = strlen(pattern);
    size_t rlen = strlen(replacement);
    size_t prefix_len = (size_t) (match - text);
    size_t suffix_len = strlen(match + plen);
    size_t new_len = prefix_len + rlen + suffix_len;

    char *result = (char *) lv_malloc(new_len + 1);
    if (!result)
        return NULL;

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
static char *apply_single_rule(const char *term, const lvRewriteRuleEx *rule, bool use_last) {
    if (!term || !rule || !rule->pattern)
        return NULL;

    /* Check condition if present */
    if (rule->condition_fn && !rule->condition_fn(term)) {
        return NULL;
    }

    const char *match = use_last ? find_last_match(term, rule->pattern) : find_first_match(term, rule->pattern);

    if (!match)
        return NULL;

    size_t plen = strlen(rule->pattern);
    size_t rlen = strlen(rule->replacement);
    size_t prefix_len = (size_t) (match - term);
    size_t suffix_len = strlen(match + plen);
    size_t new_len = prefix_len + rlen + suffix_len;

    char *result = (char *) lv_malloc(new_len + 1);
    if (!result)
        return NULL;

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
static char *apply_parallel_rules(const char *term, const lvRewriteRuleEx *rules, size_t count) {
    if (!term)
        return NULL;

    char *current = str_dup(term);
    if (!current)
        return NULL;

    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t i = 0; i < count; i++) {
            if (!rules[i].pattern || !*rules[i].pattern)
                continue;
            if (rules[i].condition_fn && !rules[i].condition_fn(current))
                continue;

            const char *match = find_first_match(current, rules[i].pattern);
            if (match) {
                char *next = apply_substitution(current, rules[i].pattern, rules[i].replacement, false);
                if (next) {
                    if (strcmp(next, current) != 0) {
                        changed = true;
                    }
                    lv_FREE_AND_NULL(current);
                    current = next;
                }
            }
        }
    }

    return current;
}

/* ============================================================
 * E-graph: simple hash set + union-find for e-class management
 * ============================================================ */

/** @brief FNV-1a hash for strings */
static size_t egraph_str_hash(const char *s) {
    size_t hash = 14695981039346656037ULL;
    while (*s) {
        hash ^= (unsigned char) *s++;
        hash *= 1099511628211ULL;
    }
    return hash;
}

/** @brief E-node table entry (hash set + e-class via union-find) */
typedef struct {
    char *expr;           /**< expression string */
    int eclass_parent;    /**< union-find parent in e-class (index into table) */
    int eclass_rank;      /**< union-find rank */
    bool occupied;
} EgraphEntry;

/** @brief Simple e-graph structure: hash set of e-nodes + e-class union-find */
typedef struct {
    EgraphEntry *entries;
    size_t capacity;
    size_t count;
} Egraph;

/**
 * @brief Create an e-graph with the given initial capacity.
 */
static Egraph *egraph_create(size_t capacity) {
    Egraph *g = (Egraph *) lv_malloc(sizeof(Egraph));
    if (!g)
        return NULL;
    g->capacity = (capacity < 64) ? 64 : capacity;
    g->count = 0;
    g->entries = (EgraphEntry *) lv_calloc(g->capacity, sizeof(EgraphEntry));
    if (!g->entries) {
        lv_FREE_AND_NULL(g);
        return NULL;
    }
    return g;
}

/**
 * @brief Destroy an e-graph, freeing all managed strings.
 */
static void egraph_destroy(Egraph *g) {
    if (!g)
        return;
    for (size_t i = 0; i < g->capacity; i++) {
        lv_FREE_AND_NULL(g->entries[i].expr);
    }
    lv_FREE_AND_NULL(g->entries);
    lv_FREE_AND_NULL(g);
}

/**
 * @brief Find or insert an expression in the e-node table.
 *
 * @param g       E-graph
 * @param expr    Expression string
 * @param is_new  [out] Set to true if this is a new insertion
 * @return Index into the entries array
 */
static size_t egraph_find_or_insert(Egraph *g, const char *expr, bool *is_new) {
    size_t hash = egraph_str_hash(expr);
    size_t idx = hash % g->capacity;

    while (g->entries[idx].occupied) {
        if (strcmp(g->entries[idx].expr, expr) == 0) {
            *is_new = false;
            return idx;
        }
        idx = (idx + 1) % g->capacity;
    }

    g->entries[idx].expr = str_dup(expr);
    g->entries[idx].occupied = true;
    g->entries[idx].eclass_parent = (int) g->count;
    g->entries[idx].eclass_rank = 0;
    g->count++;
    *is_new = true;
    return idx;
}

/**
 * @brief Union-find find with path compression.
 *
 * @param g    E-graph
 * @param idx  Index of the entry to find
 * @return Root e-class representative index
 */
static int egraph_eclass_find(Egraph *g, size_t idx) {
    int p = g->entries[idx].eclass_parent;
    if (p != (int) idx) {
        g->entries[idx].eclass_parent = egraph_eclass_find(g, (size_t) p);
    }
    return g->entries[idx].eclass_parent;
}

/**
 * @brief Union two e-classes.
 *
 * @param g     E-graph
 * @param idx_a Index of first entry
 * @param idx_b Index of second entry
 */
static void egraph_eclass_union(Egraph *g, size_t idx_a, size_t idx_b) {
    int ra = egraph_eclass_find(g, idx_a);
    int rb = egraph_eclass_find(g, idx_b);
    if (ra == rb)
        return;
    if (g->entries[ra].eclass_rank < g->entries[rb].eclass_rank) {
        g->entries[ra].eclass_parent = rb;
    } else if (g->entries[ra].eclass_rank > g->entries[rb].eclass_rank) {
        g->entries[rb].eclass_parent = ra;
    } else {
        g->entries[rb].eclass_parent = ra;
        g->entries[ra].eclass_rank++;
    }
}

/**
 * @brief Collect all expressions in the same e-class as the input entry,
 *        return the lexicographically smallest one.
 *
 * @param g          E-graph
 * @param input_idx  Index of the input expression's entry
 * @return Newly allocated string with the best expression
 */
static char *egraph_get_best(Egraph *g, size_t input_idx) {
    int target_eclass = egraph_eclass_find(g, input_idx);
    const char *best = NULL;
    for (size_t i = 0; i < g->capacity; i++) {
        if (!g->entries[i].occupied)
            continue;
        if (egraph_eclass_find(g, i) == target_eclass) {
            if (!best || strcmp(g->entries[i].expr, best) < 0) {
                best = g->entries[i].expr;
            }
        }
    }
    return best ? str_dup(best) : NULL;
}

/**
 * @brief Apply e-graph rewriting: treat rules as equalities, accumulate all variants.
 *
 * This implementation maintains a hash set of unique e-nodes and a union-find
 * structure to track e-class equivalence. After exhaustive application of rules,
 * all equivalent expressions belong to the same e-class. The result is the
 * lexicographically smallest expression from the input's e-class.
 *
 * @param term     The input term
 * @param rules    Array of rules
 * @param count    Number of rules
 * @param max_iter Maximum iterations
 * @return Newly allocated string with the canonical form
 */
static char *apply_egraph_rules(const char *term, const lvRewriteRuleEx *rules, size_t count, int max_iter) {
    if (!term)
        return NULL;

    /* Create e-graph with initial capacity */
    Egraph *g = egraph_create(256);
    if (!g)
        return str_dup(term);

    /* Insert initial term into the e-graph */
    bool is_new;
    size_t input_idx = egraph_find_or_insert(g, term, &is_new);

    /* Worklist: indices of newly discovered expressions to process */
    size_t worklist_cap = 1024;
    size_t *worklist = (size_t *) lv_malloc(worklist_cap * sizeof(size_t));
    if (!worklist) {
        egraph_destroy(g);
        return str_dup(term);
    }
    size_t work_count = 0;
    worklist[work_count++] = input_idx;

    for (int iter = 0; iter < max_iter && work_count > 0; iter++) {
        /* Snapshot current worklist; new additions go to next iteration */
        size_t current_count = work_count;
        work_count = 0;

        for (size_t wi = 0; wi < current_count; wi++) {
            size_t node_idx = worklist[wi];
            if (!g->entries[node_idx].occupied)
                continue;
            const char *current_expr = g->entries[node_idx].expr;

            for (size_t ri = 0; ri < count; ri++) {
                if (!rules[ri].pattern || !*rules[ri].pattern)
                    continue;
                if (rules[ri].condition_fn && !rules[ri].condition_fn(current_expr))
                    continue;

                char *result = apply_substitution(current_expr, rules[ri].pattern, rules[ri].replacement, false);
                if (!result)
                    continue;
                if (strcmp(result, current_expr) == 0) {
                    lv_FREE_AND_NULL(result);
                    continue;
                }

                /* Insert result into e-node table */
                bool result_new;
                size_t result_idx = egraph_find_or_insert(g, result, &result_new);

                /* Merge original and result into the same e-class */
                egraph_eclass_union(g, node_idx, result_idx);

                /* If result is a new e-node, add to next worklist */
                if (result_new) {
                    if (work_count >= worklist_cap) {
                        worklist_cap *= 2;
                        size_t *new_wl = (size_t *) lv_realloc(worklist, worklist_cap * sizeof(size_t));
                        if (!new_wl) {
                            lv_FREE_AND_NULL(result);
                            break;
                        }
                        worklist = new_wl;
                    }
                    worklist[work_count++] = result_idx;
                }
                lv_FREE_AND_NULL(result);
            }
        }
    }

    lv_FREE_AND_NULL(worklist);

    /* Retrieve the best (lexicographically smallest) expression from the
     * same e-class as the input term */
    char *best = egraph_get_best(g, input_idx);
    egraph_destroy(g);
    return best ? best : str_dup(term);
}

/**
 * @brief 按优先级升序比较两条规则（数值越小越优先）
 */
static int cmp_rewrite_rule_priority(const void *a, const void *b, void *ctx) {
    (void) ctx;
    const lvRewriteRuleEx *ra = (const lvRewriteRuleEx *) a;
    const lvRewriteRuleEx *rb = (const lvRewriteRuleEx *) b;
    return (ra->priority > rb->priority) - (ra->priority < rb->priority);
}

/**
 * @brief Sort rules by priority (ascending: lower priority value = higher precedence).
 *
 * @param rules  Array of rules
 * @param count  Number of rules
 */
static void sort_rules_by_priority(lvRewriteRuleEx *rules, size_t count) {
    lv_insertion_sort(rules, count, sizeof(lvRewriteRuleEx), cmp_rewrite_rule_priority, NULL);
}

/* ============================================================
 * API implementation: Engine lifecycle
 * ============================================================ */

lvRewriteEngineEx *rewrite_engine_ex_create(lvRewriteStrategyEx strategy, int max_iterations) {
    lvRewriteEngineEx *engine = (lvRewriteEngineEx *) lv_malloc(sizeof(lvRewriteEngineEx));
    if (!engine)
        return NULL;

    engine->rules = (lvRewriteRuleEx *) lv_malloc(INITIAL_RULE_CAPACITY * sizeof(lvRewriteRuleEx));
    if (!engine->rules) {
        lv_FREE_AND_NULL(engine);
        return NULL;
    }

    engine->rule_count = 0;
    engine->rule_capacity = INITIAL_RULE_CAPACITY;
    engine->strategy = strategy;
    engine->max_iterations = (max_iterations > 0) ? max_iterations : DEFAULT_MAX_ITERATIONS;

    return engine;
}

void rewrite_engine_ex_destroy(lvRewriteEngineEx *engine) {
    if (!engine)
        return;

    /* Free each rule's owned strings */
    for (size_t i = 0; i < engine->rule_count; i++) {
        lv_free_ptr((void *) engine->rules[i].name);
        lv_free_ptr((void *) engine->rules[i].pattern);
        lv_free_ptr((void *) engine->rules[i].replacement);
    }

    lv_FREE_AND_NULL(engine->rules);
    lv_FREE_AND_NULL(engine);
}

/* ============================================================
 * API implementation: Rule management
 * ============================================================ */

bool rewrite_engine_ex_add_rule(lvRewriteEngineEx *engine, const char *name, const char *pattern,
                                const char *replacement, int priority, lvRewriteConditionFn condition) {
    if (!engine || !name || !pattern || !replacement)
        return false;

    /* Grow array if needed */
    if (engine->rule_count >= engine->rule_capacity) {
        size_t new_cap = engine->rule_capacity * 2;
        lvRewriteRuleEx *new_rules = (lvRewriteRuleEx *) lv_realloc(engine->rules, new_cap * sizeof(lvRewriteRuleEx));
        if (!new_rules)
            return false;
        engine->rules = new_rules;
        engine->rule_capacity = new_cap;
    }

    lvRewriteRuleEx *rule = &engine->rules[engine->rule_count];
    rule->name = str_dup(name);
    rule->pattern = str_dup(pattern);
    rule->replacement = str_dup(replacement);
    rule->priority = priority;
    rule->condition_fn = condition;

    if (!rule->name || !rule->pattern || !rule->replacement) {
        lv_free_ptr((void *) rule->name);
        lv_free_ptr((void *) rule->pattern);
        lv_free_ptr((void *) rule->replacement);
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

bool rewrite_engine_ex_apply(lvRewriteEngineEx *engine, const char *input, lvRewriteResultEx *result) {
    if (!engine || !input || !result)
        return false;

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
            if (!current)
                return false;

            for (int i = 0; i < engine->max_iterations; i++) {
                bool any_applied = false;
                for (size_t r = 0; r < engine->rule_count; r++) {
                    char *next = apply_single_rule(current, &engine->rules[r], true);
                    if (next) {
                        if (strcmp(next, current) != 0) {
                            any_applied = true;
                            lv_FREE_AND_NULL(current);
                            current = next;
                            break; /* Restart from highest priority rule */
                        }
                        lv_FREE_AND_NULL(next);
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
            if (!current)
                return false;

            for (int i = 0; i < engine->max_iterations; i++) {
                bool any_applied = false;
                for (size_t r = 0; r < engine->rule_count; r++) {
                    char *next = apply_single_rule(current, &engine->rules[r], false);
                    if (next) {
                        if (strcmp(next, current) != 0) {
                            any_applied = true;
                            lv_FREE_AND_NULL(current);
                            current = next;
                            break; /* Restart from highest priority rule */
                        }
                        lv_FREE_AND_NULL(next);
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
            if (!current)
                return false;

            for (int i = 0; i < engine->max_iterations; i++) {
                char *next = apply_parallel_rules(current, engine->rules, engine->rule_count);
                if (!next) {
                    result->iterations = i + 1;
                    result->converged = true;
                    break;
                }
                if (strcmp(next, current) == 0) {
                    lv_FREE_AND_NULL(next);
                    result->iterations = i + 1;
                    result->converged = true;
                    break;
                }
                lv_FREE_AND_NULL(current);
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
            char *current = apply_egraph_rules(input, engine->rules, engine->rule_count, engine->max_iterations);
            if (!current)
                return false;
            result->output = current;
            result->converged = true;
            result->iterations = 1;
            return true;
        }

        default:
            return false;
    }
}

void rewrite_engine_result_ex_destroy(lvRewriteResultEx *result) {
    if (!result)
        return;
    lv_FREE_AND_NULL(result->output);
}

/* ============================================================
 * lv_rewrite_apply_strategy — 按策略类型应用重写
 * ============================================================ */

int lv_rewrite_apply_strategy(lvRewriteContext *ctx, lvRewriteStrategyType strategy) {
    if (!ctx) {
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "lv_rewrite_apply_strategy: ctx 为空");
    }

    if (ctx->impl) {
        /* 已有引擎，返回规则数（>0 表示可用） */
        lvRewriteEngineEx *engine = (lvRewriteEngineEx *) ctx->impl;
        return (int) engine->rule_count > 0 ? 0 : 1;
    }

    /* 创建新引擎并存入 context */
    lvRewriteEngineEx *engine = rewrite_engine_ex_create((lvRewriteStrategyEx) strategy, 100);
    if (!engine) {
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "lv_rewrite_apply_strategy: 创建引擎失败");
    }
    ctx->impl = engine;
    return 0;
}
