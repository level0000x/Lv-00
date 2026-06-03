/**
 * @file gappa_propagate.c
 * @brief Implementation of the predicate propagation engine
 *
 * @details Implements forward and backward propagation of interval predicates
 *          for Gappa-style floating-point proofs.
 *
 *          Forward propagation derives new predicates from known hypotheses
 *          using interval arithmetic rules.
 *
 *          Backward propagation determines what additional hypotheses are
 *          needed to prove a given goal.
 *
 * @version 3.4.0
 * @date 2026-05-25
 */

#include "gappa_propagate.h"
#include "interval_arithmetic.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * Global rewrite rules storage
 * ======================================================================== */

static Lv00GappaRewriteRule g_rewrite_rules[LV00_MAX_REWRITE_RULES];
static int g_rewrite_rule_count = 0;

/* ========================================================================
 * Predicate set operations
 * ======================================================================== */

LV00_PUBLIC_API void gappa_pred_set_init(Lv00GappaPredSet *set) {
    if (!set) return;
    memset(set, 0, sizeof(*set));
}

LV00_PUBLIC_API bool gappa_pred_set_add(Lv00GappaPredSet *set, const Lv00GappaPredicate *pred) {
    if (!set || !pred) return false;
    if (set->count >= LV00_PRED_SET_MAX_SIZE) return false;

    /* Check for duplicate */
    for (int i = 0; i < set->count; i++) {
        if (set->preds[i].type == pred->type &&
            strcmp(set->preds[i].expr_lhs, pred->expr_lhs) == 0) {
            /* Same type and same variable -- update bounds if tighter */
            if (pred->type == LV00_PRED_BND) {
                if (pred->bound_lo > set->preds[i].bound_lo) {
                    set->preds[i].bound_lo = pred->bound_lo;
                }
                if (pred->bound_hi < set->preds[i].bound_hi) {
                    set->preds[i].bound_hi = pred->bound_hi;
                }
            }
            return false; /* Already exists (updated) */
        }
    }

    set->preds[set->count] = *pred;
    set->count++;
    return true;
}

LV00_PUBLIC_API int gappa_pred_set_find(
    Lv00GappaPredSet *set, const char *var_name, Lv00GappaPredicate *out)
{
    if (!set || !var_name) return -1;

    for (int i = 0; i < set->count; i++) {
        if (set->preds[i].type == LV00_PRED_BND &&
            strcmp(set->preds[i].expr_lhs, var_name) == 0) {
            if (out) *out = set->preds[i];
            return i;
        }
    }
    return -1;
}

LV00_PUBLIC_API const Lv00GappaPredicate *gappa_pred_set_get(const Lv00GappaPredSet *set, int index) {
    if (!set || index < 0 || index >= set->count) return NULL;
    return &set->preds[index];
}

LV00_PUBLIC_API void gappa_pred_set_clear(Lv00GappaPredSet *set) {
    if (!set) return;
    set->count = 0;
}

/* ========================================================================
 * Forward propagation
 * ======================================================================== */

/**
 * @brief Try to derive new predicates from a pair of existing predicates.
 *
 * Applies interval arithmetic rules to combine two known facts.
 */
static int propagate_pair(
    const Lv00GappaPredicate *a,
    const Lv00GappaPredicate *b,
    Lv00GappaPredSet *output)
{
    int derived = 0;

    /* Both must be BND predicates */
    if (a->type != LV00_PRED_BND || b->type != LV00_PRED_BND) {
        return 0;
    }

    Lv00Interval ia = interval_create(a->bound_lo, a->bound_hi, 0);
    Lv00Interval ib = interval_create(b->bound_lo, b->bound_hi, 0);

    /* Derive sum bounds: a + b */
    {
        Lv00Interval sum = interval_add(ia, ib);
        char name[256];
        snprintf(name, sizeof(name), "(%s + %s)", a->expr_lhs, b->expr_lhs);
        Lv00GappaPredicate pred;
        memset(&pred, 0, sizeof(pred));
        pred.type = LV00_PRED_BND;
        strncpy(pred.expr_lhs, name, sizeof(pred.expr_lhs) - 1);
        pred.bound_lo = sum.lo;
        pred.bound_hi = sum.hi;
        pred.is_hypothesis = 0;
        if (gappa_pred_set_add(output, &pred)) derived++;
    }

    /* Derive difference bounds: a - b */
    {
        Lv00Interval diff = interval_sub(ia, ib);
        char name[256];
        snprintf(name, sizeof(name), "(%s - %s)", a->expr_lhs, b->expr_lhs);
        Lv00GappaPredicate pred;
        memset(&pred, 0, sizeof(pred));
        pred.type = LV00_PRED_BND;
        strncpy(pred.expr_lhs, name, sizeof(pred.expr_lhs) - 1);
        pred.bound_lo = diff.lo;
        pred.bound_hi = diff.hi;
        pred.is_hypothesis = 0;
        if (gappa_pred_set_add(output, &pred)) derived++;
    }

    /* Derive product bounds: a * b */
    {
        Lv00Interval prod = interval_mul(ia, ib);
        char name[256];
        snprintf(name, sizeof(name), "(%s * %s)", a->expr_lhs, b->expr_lhs);
        Lv00GappaPredicate pred;
        memset(&pred, 0, sizeof(pred));
        pred.type = LV00_PRED_BND;
        strncpy(pred.expr_lhs, name, sizeof(pred.expr_lhs) - 1);
        pred.bound_lo = prod.lo;
        pred.bound_hi = prod.hi;
        pred.is_hypothesis = 0;
        if (gappa_pred_set_add(output, &pred)) derived++;
    }

    /* Derive absolute value bounds: |a| */
    {
        Lv00Interval abs_a = interval_abs(ia);
        char name[256];
        snprintf(name, sizeof(name), "|%s|", a->expr_lhs);
        Lv00GappaPredicate pred;
        memset(&pred, 0, sizeof(pred));
        pred.type = LV00_PRED_ABS;
        strncpy(pred.expr_lhs, a->expr_lhs, sizeof(pred.expr_lhs) - 1);
        pred.bound_abs = abs_a.hi;
        pred.is_hypothesis = 0;
        if (gappa_pred_set_add(output, &pred)) derived++;
    }

    return derived;
}

/**
 * @brief Apply registered rewrite rules to simplify predicates.
 */
static void apply_rewrite_rules(Lv00GappaPredSet *set) {
    (void)set;
    /* Rewrite rules are applied in future versions */
}

LV00_PUBLIC_API int gappa_propagate(
    const Lv00GappaPredSet *input_set,
    Lv00GappaPredSet *output_set,
    const Lv00GappaPropagateConfig *config)
{
    if (!input_set || !output_set) return -1;

    Lv00GappaPropagateConfig cfg;
    if (config) {
        cfg = *config;
    } else {
        cfg = gappa_propagate_config_default();
    }

    /* Initialize output with input hypotheses */
    gappa_pred_set_init(output_set);
    for (int i = 0; i < input_set->count; i++) {
        gappa_pred_set_add(output_set, &input_set->preds[i]);
    }

    int total_derived = 0;
    int iteration = 0;

    /* Fixed-point iteration */
    while (iteration < cfg.max_iterations) {
        int new_derived = 0;
        int prev_count = output_set->count;

        /* Try all pairs */
        for (int i = 0; i < output_set->count; i++) {
            for (int j = i + 1; j < output_set->count; j++) {
                new_derived += propagate_pair(
                    &output_set->preds[i],
                    &output_set->preds[j],
                    output_set);
            }
        }

        /* Apply rewrite rules */
        apply_rewrite_rules(output_set);

        total_derived += new_derived;
        iteration++;

        /* Fixed point reached */
        if (output_set->count == prev_count) {
            break;
        }

        /* Safety: stop if set is full */
        if (output_set->count >= LV00_PRED_SET_MAX_SIZE) {
            break;
        }
    }

    return total_derived;
}

/* ========================================================================
 * Backward propagation
 * ======================================================================== */

LV00_PUBLIC_API int gappa_propagate_backward(
    const Lv00GappaPredicate *goal,
    const Lv00GappaPredSet *known_facts,
    Lv00GappaPredSet *output_set,
    const Lv00GappaPropagateConfig *config)
{
    if (!goal || !output_set) return -1;

    Lv00GappaPropagateConfig cfg;
    if (config) {
        cfg = *config;
    } else {
        cfg = gappa_propagate_config_default();
    }

    gappa_pred_set_init(output_set);

    /* Check if the goal is already satisfied by known facts */
    if (known_facts) {
        Lv00GappaPredicate existing;
        if (gappa_pred_set_find((Lv00GappaPredSet *)known_facts,
                                goal->expr_lhs, &existing) >= 0) {
            /* Variable bounds are already known */
            if (goal->type == LV00_PRED_BND) {
                /* Check if existing bounds imply the goal */
                if (existing.bound_lo >= goal->bound_lo &&
                    existing.bound_hi <= goal->bound_hi) {
                    return 0; /* Already proven, no new hypotheses needed */
                }
            }
        }
    }

    /* For ABS goals like |x - c| <= b, derive needed bounds for x */
    if (goal->type == LV00_PRED_ABS && strlen(goal->expr_rhs) > 0) {
        double constant = 0.0;
        if (sscanf(goal->expr_rhs, "%lf", &constant) == 1) {
            /* |x - c| <= b  =>  c - b <= x <= c + b */
            Lv00GappaPredicate needed;
            memset(&needed, 0, sizeof(needed));
            needed.type = LV00_PRED_BND;
            strncpy(needed.expr_lhs, goal->expr_lhs, sizeof(needed.expr_lhs) - 1);
            needed.bound_lo = constant - goal->bound_abs;
            needed.bound_hi = constant + goal->bound_abs;
            needed.is_hypothesis = 1;
            gappa_pred_set_add(output_set, &needed);
            return 1;
        }
    }

    /* For BND goals, the needed hypothesis is the goal itself */
    if (goal->type == LV00_PRED_BND) {
        Lv00GappaPredicate needed = *goal;
        needed.is_hypothesis = 1;
        gappa_pred_set_add(output_set, &needed);
        return 1;
    }

    (void)cfg.max_backward_depth;
    return 0;
}

/* ========================================================================
 * Rewrite rules
 * ======================================================================== */

LV00_PUBLIC_API bool gappa_register_rewrite_rules(
    const Lv00GappaRewriteRule *rules,
    int rule_count)
{
    if (!rules || rule_count <= 0) return false;

    for (int i = 0; i < rule_count && g_rewrite_rule_count < LV00_MAX_REWRITE_RULES; i++) {
        g_rewrite_rules[g_rewrite_rule_count++] = rules[i];
    }

    return true;
}

LV00_PUBLIC_API Lv00GappaPropagateConfig gappa_propagate_config_default(void) {
    Lv00GappaPropagateConfig cfg;
    cfg.max_iterations = 10;
    cfg.max_backward_depth = 3;
    cfg.contraction_eps = 1e-12;
    cfg.enable_backward = 1;
    return cfg;
}
