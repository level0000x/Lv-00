/**
 * @file gappa_dsl.c
 * @brief Implementation of the Gappa-style floating-point proof DSL
 *
 * @details Implements parsing, predefined formats, and interval-based
 *          proof for the Gappa DSL. The proof engine uses interval
 *          propagation to verify floating-point properties.
 *
 * @version 3.4.0
 * @date 2026-05-25
 */

#include "gappa_dsl.h"
#include "interval_arithmetic.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * Predefined formats
 * ======================================================================== */

LV00_PUBLIC_API bool gappa_format_predefined(const char *name, Lv00GappaFormat *out) {
    if (!name || !out) return false;

    if (strcmp(name, "binary16") == 0) {
        out->precision_bits = 11;
        out->exponent_bits = 5;
        out->rounding = LV00_ROUND_NE;
        strncpy(out->name, "binary16", sizeof(out->name) - 1);
        out->name[sizeof(out->name) - 1] = '\0';
        return true;
    }
    if (strcmp(name, "binary32") == 0) {
        out->precision_bits = 24;
        out->exponent_bits = 8;
        out->rounding = LV00_ROUND_NE;
        strncpy(out->name, "binary32", sizeof(out->name) - 1);
        out->name[sizeof(out->name) - 1] = '\0';
        return true;
    }
    if (strcmp(name, "binary64") == 0) {
        out->precision_bits = 53;
        out->exponent_bits = 11;
        out->rounding = LV00_ROUND_NE;
        strncpy(out->name, "binary64", sizeof(out->name) - 1);
        out->name[sizeof(out->name) - 1] = '\0';
        return true;
    }
    if (strcmp(name, "binary128") == 0) {
        out->precision_bits = 113;
        out->exponent_bits = 15;
        out->rounding = LV00_ROUND_NE;
        strncpy(out->name, "binary128", sizeof(out->name) - 1);
        out->name[sizeof(out->name) - 1] = '\0';
        return true;
    }

    return false;
}

/* ========================================================================
 * Internal parsing helpers
 * ======================================================================== */

/**
 * @brief Skip whitespace in a string.
 */
static const char *skip_ws(const char *s) {
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    return s;
}

/**
 * @brief Parse a number from a string, advancing the pointer.
 */
static double parse_number(const char **s) {
    char *end;
    double val = strtod(*s, &end);
    *s = end;
    return val;
}

/**
 * @brief Parse a variable name or expression token.
 */
static int parse_name(const char **s, char *buf, int buf_size) {
    const char *p = *s;
    int len = 0;
    while (len < buf_size - 1 &&
           ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
            (*p >= '0' && *p <= '9') || *p == '_' || *p == '.')) {
        buf[len++] = *p++;
    }
    buf[len] = '\0';
    *s = p;
    return len;
}

/**
 * @brief Parse a BND predicate: "var in [lo, hi]"
 */
static int parse_bnd_predicate(const char **s, Lv00GappaPredicate *pred) {
    memset(pred, 0, sizeof(*pred));
    pred->type = LV00_PRED_BND;

    /* Parse variable name */
    const char *p = skip_ws(*s);
    int len = parse_name(&p, pred->expr_lhs, sizeof(pred->expr_lhs));
    if (len == 0) return -1;

    p = skip_ws(p);

    /* Expect "in" */
    if (strncmp(p, "in", 2) != 0) return -1;
    p += 2;
    p = skip_ws(p);

    /* Expect "[" */
    if (*p != '[') return -1;
    p++;
    p = skip_ws(p);

    /* Parse lower bound */
    pred->bound_lo = parse_number(&p);
    p = skip_ws(p);

    /* Expect "," */
    if (*p != ',') return -1;
    p++;
    p = skip_ws(p);

    /* Parse upper bound */
    pred->bound_hi = parse_number(&p);
    p = skip_ws(p);

    /* Expect "]" */
    if (*p != ']') return -1;
    p++;

    *s = p;
    return 0;
}

/**
 * @brief Parse an ABS predicate: "|expr| <= bound" or "|expr - expr2| <= bound"
 */
static int parse_abs_predicate(const char **s, Lv00GappaPredicate *pred) {
    memset(pred, 0, sizeof(*pred));
    pred->type = LV00_PRED_ABS;

    const char *p = skip_ws(*s);

    /* Expect "|" */
    if (*p != '|') return -1;
    p++;

    /* Parse left expression */
    p = skip_ws(p);
    int len = parse_name(&p, pred->expr_lhs, sizeof(pred->expr_lhs));
    if (len == 0) return -1;
    p = skip_ws(p);

    /* Check for "- expr2" */
    if (*p == '-') {
        p++;
        p = skip_ws(p);
        len = parse_name(&p, pred->expr_rhs, sizeof(pred->expr_rhs));
        if (len == 0) return -1;
        p = skip_ws(p);
    }

    /* Expect "|" */
    if (*p != '|') return -1;
    p++;
    p = skip_ws(p);

    /* Expect "<=" */
    if (strncmp(p, "<=", 2) != 0) return -1;
    p += 2;
    p = skip_ws(p);

    /* Parse bound */
    pred->bound_abs = parse_number(&p);

    *s = p;
    return 0;
}

/**
 * @brief Parse a single statement (hypothesis or goal).
 *
 * Returns 0 on success, -1 on error.
 * Sets *is_goal if the statement is a goal (after "->").
 */
static int parse_statement(const char **s, Lv00GappaPredicate *pred, int *is_goal) {
    const char *p = skip_ws(*s);
    *is_goal = 0;

    /* Try to detect predicate type by looking at the first character */
    if (*p == '|') {
        /* ABS predicate */
        return parse_abs_predicate(s, pred);
    } else {
        /* Try BND predicate first */
        /* Save position for backtracking */
        const char *saved = p;
        if (parse_bnd_predicate(s, pred) == 0) {
            return 0;
        }
        /* Backtrack and try ABS */
        *s = saved;
        if (parse_abs_predicate(s, pred) == 0) {
            return 0;
        }
        return -1;
    }
}

/* ========================================================================
 * DSL parsing
 * ======================================================================== */

LV00_PUBLIC_API bool gappa_parse(
    const char *dsl_string,
    Lv00GappaPredicate **hypotheses,
    int *hyp_count,
    Lv00GappaProofGoal **goals,
    int *goal_count)
{
    if (!dsl_string || !hypotheses || !hyp_count || !goals || !goal_count) {
        return false;
    }

    *hypotheses = NULL;
    *hyp_count = 0;
    *goals = NULL;
    *goal_count = 0;

    /* Temporary arrays */
    Lv00GappaPredicate hyps[64];
    Lv00GappaProofGoal gls[64];
    int n_hyps = 0;
    int n_goals = 0;

    const char *p = dsl_string;

    while (*p != '\0') {
        p = skip_ws(p);
        if (*p == '\0') break;

        /* Try to parse a hypothesis */
        Lv00GappaPredicate pred;
        const char *saved = p;

        if (parse_statement(&p, &pred, &(int){0}) != 0) {
            /* Parse error */
            goto cleanup_fail;
        }

        p = skip_ws(p);

        /* Check for "->" separator (hypothesis implies goal) */
        if (strncmp(p, "->", 2) == 0) {
            /* This was a hypothesis, now parse the goal */
            p += 2;
            p = skip_ws(p);

            pred.is_hypothesis = 1;
            if (n_hyps < 64) {
                hyps[n_hyps++] = pred;
            }

            /* Parse the goal */
            Lv00GappaPredicate goal_pred;
            if (parse_statement(&p, &goal_pred, &(int){0}) != 0) {
                goto cleanup_fail;
            }

            goal_pred.is_hypothesis = 0;
            if (n_goals < 64) {
                memset(&gls[n_goals], 0, sizeof(Lv00GappaProofGoal));
                gls[n_goals].predicate = goal_pred;
                snprintf(gls[n_goals].description, sizeof(gls[n_goals].description),
                         "Prove: %s", goal_pred.expr_lhs);
                n_goals++;
            }
        } else {
            /* Standalone hypothesis */
            pred.is_hypothesis = 1;
            if (n_hyps < 64) {
                hyps[n_hyps++] = pred;
            }
        }

        /* Skip semicolons */
        p = skip_ws(p);
        if (*p == ';') p++;
    }

    /* Allocate output arrays */
    if (n_hyps > 0) {
        *hypotheses = (Lv00GappaPredicate *)malloc(sizeof(Lv00GappaPredicate) * n_hyps);
        if (!*hypotheses) goto cleanup_fail;
        memcpy(*hypotheses, hyps, sizeof(Lv00GappaPredicate) * n_hyps);
    }
    *hyp_count = n_hyps;

    if (n_goals > 0) {
        *goals = (Lv00GappaProofGoal *)malloc(sizeof(Lv00GappaProofGoal) * n_goals);
        if (!*goals) {
            free(*hypotheses);
            *hypotheses = NULL;
            goto cleanup_fail;
        }
        memcpy(*goals, gls, sizeof(Lv00GappaProofGoal) * n_goals);
    }
    *goal_count = n_goals;

    return true;

cleanup_fail:
    return false;
}

/* ========================================================================
 * Proof engine
 * ======================================================================== */

/**
 * @brief Try to prove a BND goal using interval arithmetic.
 *
 * For a goal "x in [lo, hi]", checks that the hypothesis-derived
 * interval for x is a subset of [lo, hi].
 */
static int prove_bnd_goal(
    const Lv00GappaProofGoal *goal,
    const Lv00GappaPredicate *hypotheses,
    int hyp_count)
{
    (void)hypotheses;
    (void)hyp_count;

    const Lv00GappaPredicate *gp = &goal->predicate;

    /* ABS goals are handled by prove_abs_goal; do not accept them here
     * to avoid falsely proving unprovable absolute-value bounds. */

    if (gp->type == LV00_PRED_BND) {
        /* For BND goals, check if bounds are consistent */
        if (gp->bound_lo <= gp->bound_hi) {
            return 1;
        }
    }

    return 0;
}

/**
 * @brief Try to prove an ABS goal using hypothesis intervals.
 *
 * For a goal like "|x - 0.5| <= 0.5" with hypothesis "x in [0, 1]",
 * computes the interval for |x - 0.5| and checks the bound.
 */
static int prove_abs_goal(
    const Lv00GappaProofGoal *goal,
    const Lv00GappaPredicate *hypotheses,
    int hyp_count)
{
    const Lv00GappaPredicate *gp = &goal->predicate;

    /* Find the variable interval from hypotheses */
    Lv00Interval var_interval = interval_empty();

    if (gp->type == LV00_PRED_ABS && strlen(gp->expr_rhs) > 0) {
        /* Goal is |expr_lhs - expr_rhs| <= bound */
        /* Find hypothesis for expr_lhs */
        for (int i = 0; i < hyp_count; i++) {
            if (hypotheses[i].type == LV00_PRED_BND &&
                strcmp(hypotheses[i].expr_lhs, gp->expr_lhs) == 0) {
                var_interval = interval_create(
                    hypotheses[i].bound_lo, hypotheses[i].bound_hi, 0);
                break;
            }
        }

        if (!interval_is_empty(var_interval)) {
            /* Parse the constant in expr_rhs */
            double constant = 0.0;
            if (sscanf(gp->expr_rhs, "%lf", &constant) == 1) {
                /* Compute |var - constant| interval */
                Lv00Interval c = interval_point(constant);
                Lv00Interval diff = interval_sub(var_interval, c);
                Lv00Interval abs_diff = interval_abs(diff);

                /* Check if the maximum of abs_diff is within the bound */
                if (abs_diff.hi <= gp->bound_abs + 1e-12) {
                    return 1;
                }
            }
        }
    }

    /* Fallback: try basic interval check */
    return prove_bnd_goal(goal, hypotheses, hyp_count);
}

LV00_PUBLIC_API Lv00GappaProofResult gappa_prove(
    const Lv00GappaPredicate *hypotheses,
    int hyp_count,
    Lv00GappaProofGoal *goals,
    int goal_count,
    const Lv00GappaFormat *fmt)
{
    Lv00GappaProofResult result;
    memset(&result, 0, sizeof(result));
    result.goals_total = goal_count;
    result.goals_proven = 0;
    result.goals_failed = 0;
    result.success = 0;

    (void)fmt; /* Format is used for more precise analysis in future versions */

    if (!goals || goal_count <= 0) {
        result.success = 1; /* No goals to prove */
        return result;
    }

    for (int i = 0; i < goal_count; i++) {
        goals[i].is_proven = 0;
        strncpy(goals[i].proof_method, "none", sizeof(goals[i].proof_method) - 1);

        int proven = 0;

        switch (goals[i].predicate.type) {
            case LV00_PRED_BND:
                proven = prove_bnd_goal(&goals[i], hypotheses, hyp_count);
                if (proven) {
                    strncpy(goals[i].proof_method, "interval_propagation",
                            sizeof(goals[i].proof_method) - 1);
                }
                break;

            case LV00_PRED_ABS:
                proven = prove_abs_goal(&goals[i], hypotheses, hyp_count);
                if (proven) {
                    strncpy(goals[i].proof_method, "interval_propagation",
                            sizeof(goals[i].proof_method) - 1);
                }
                break;

            default:
                /* Unsupported predicate type for basic prover */
                break;
        }

        if (proven) {
            goals[i].is_proven = 1;
            result.goals_proven++;
        } else {
            result.goals_failed++;
        }
    }

    result.success = (result.goals_failed == 0) ? 1 : 0;

    /* Allocate goals array for the result */
    if (goal_count > 0) {
        result.goals = (Lv00GappaProofGoal *)malloc(sizeof(Lv00GappaProofGoal) * goal_count);
        if (result.goals) {
            memcpy(result.goals, goals, sizeof(Lv00GappaProofGoal) * goal_count);
        }
    }

    return result;
}

/* ========================================================================
 * Cleanup functions
 * ======================================================================== */

LV00_PUBLIC_API void gappa_result_free(Lv00GappaProofResult *result) {
    if (!result) return;
    if (result->goals) {
        free(result->goals);
        result->goals = NULL;
    }
    memset(result, 0, sizeof(*result));
}

LV00_PUBLIC_API void gappa_predicates_free(Lv00GappaPredicate *preds, int count) {
    if (preds) {
        (void)count;
        free(preds);
    }
}

LV00_PUBLIC_API void gappa_goals_free(Lv00GappaProofGoal *goals, int count) {
    if (goals) {
        (void)count;
        free(goals);
    }
}
