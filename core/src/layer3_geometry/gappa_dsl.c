/**
 * @file gappa_dsl.c
 * @brief Gappa DSL parsing and proof generation (stub implementations)
 */

#include "lv00/gappa_dsl.h"
#include "lv00/lv00_utils.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── Original API ── */

int lv00_gappa_parse(const char *input) {
    if (!input) return -1;
    return 0;
}

int lv00_gappa_eval(const char *expr, double *lo, double *hi) {
    if (!expr || !lo || !hi) return -1;
    *lo = -1.0;
    *hi =  1.0;
    return 0;
}

char *lv00_gappa_prove(const char *script) {
    if (!script) return NULL;
    return lv00_strdup("proof placeholder");
}

/* ── Structured API ── */

bool gappa_format_predefined(const char *name, Lv00GappaFormat *out) {
    if (!out) return false;
    memset(out, 0, sizeof(Lv00GappaFormat));
    if (name) {
        if (strcmp(name, "binary32") == 0) {
            out->format_id = 1;
            out->name = "binary32";
            out->precision_bits = 24;
            out->exponent_bits = 8;
        } else if (strcmp(name, "binary64") == 0) {
            out->format_id = 2;
            out->name = "binary64";
            out->precision_bits = 53;
            out->exponent_bits = 11;
        } else {
            out->format_id = 0;
            out->name = "default";
            out->precision_bits = 53;
            out->exponent_bits = 11;
        }
    } else {
        out->format_id = 0;
        out->name = "default";
        out->precision_bits = 53;
        out->exponent_bits = 11;
    }
    out->rounding = LV00_ROUND_NE;
    return true;
}

bool gappa_parse(const char *input, Lv00GappaPredicate **hyp, int *hyp_count,
                 Lv00GappaProofGoal **goals, int *goal_count) {
    (void)input;
    if (hyp) *hyp = NULL;
    if (hyp_count) *hyp_count = 0;
    if (goals) *goals = NULL;
    if (goal_count) *goal_count = 0;
    return true;
}

void gappa_predicates_free(Lv00GappaPredicate *preds, int count) {
    (void)count;
    free(preds);
}

void gappa_goals_free(Lv00GappaProofGoal *goals, int count) {
    (void)count;
    free(goals);
}

Lv00GappaProofResult gappa_prove(const Lv00GappaPredicate *hyp, int hyp_count,
                                  const Lv00GappaProofGoal *goals, int goal_count,
                                  const void *config) {
    (void)hyp; (void)hyp_count; (void)goals; (void)goal_count; (void)config;
    Lv00GappaProofResult result;
    memset(&result, 0, sizeof(result));
    result.success = true;
    result.goals_total = goal_count;
    result.goals_proven = goal_count;
    result.goals_failed = 0;
    if (goal_count > 0) {
        result.goals = (Lv00GappaProofGoal *)calloc((size_t)goal_count, sizeof(Lv00GappaProofGoal));
        if (result.goals) {
            for (int i = 0; i < goal_count; i++) {
                result.goals[i] = goals[i];
                result.goals[i].proven = true;
            }
        }
    }
    return result;
}

void gappa_result_free(Lv00GappaProofResult *result) {
    if (result) {
        free(result->goals);
        result->goals = NULL;
    }
}

bool gappa_register_rewrite_rules(const Lv00GappaRewriteRule *rules, int count) {
    (void)rules; (void)count;
    return true;
}
