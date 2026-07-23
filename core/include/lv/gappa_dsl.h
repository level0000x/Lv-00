#ifndef lv_GAPPA_DSL_H
#define lv_GAPPA_DSL_H
#include <stdbool.h>
#include <stddef.h>

#include "lv/gappa_propagate.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Gappa format type. */
typedef struct {
    int format_id;
    const char *name;
    int precision_bits;
    int exponent_bits;
    int rounding;
} lvGappaFormat;

/** Predefined format helper */
bool gappa_format_predefined(const char *name, lvGappaFormat *out);

/** Rounding modes */
typedef enum { lv_ROUND_NE = 0, lv_ROUND_NA, lv_ROUND_NU, lv_ROUND_ND, lv_ROUND_ZR } lvRoundingMode;

/** Predicate types */
typedef enum { lv_PRED_BND = 0, lv_PRED_ABS, lv_PRED_REL } lvPredType;

/** Gappa predicate */
typedef struct lvGappaPredicate {
    lvPredType type;
    char expr_lhs[256];
    char expr_rhs[256];
    double bound_lo;
    double bound_hi;
    double bound_abs;
    bool is_hypothesis;
} lvGappaPredicate;

/** Gappa proof goal */
typedef struct {
    lvGappaPredicate predicate;
    char target_expr[256];
    double bound_lo;
    double bound_hi;
    bool proven;
} lvGappaProofGoal;

/** Gappa proof result */
typedef struct {
    bool success;
    int goals_total;
    int goals_proven;
    int goals_failed;
    lvGappaProofGoal *goals;
} lvGappaProofResult;

/** Rewrite rule */
typedef struct {
    char match_pattern[256];
    char replace_pattern[256];
    char description[256];
} lvGappaRewriteRule;

/** Parse Gappa DSL expression. */
int lv_gappa_parse(const char *input);

/** Parse with structured output */
bool gappa_parse(const char *input, lvGappaPredicate **hyp, int *hyp_count, lvGappaProofGoal **goals, int *goal_count);

/** Free parsed results */
void gappa_predicates_free(lvGappaPredicate *preds, int count);
void gappa_goals_free(lvGappaProofGoal *goals, int count);

/** Evaluate Gappa expression with interval bounds. */
int lv_gappa_eval(const char *expr, double *lo, double *hi);

/** Generate proof from Gappa script. */
char *lv_gappa_prove(const char *script);

/** Prove with structured API */
lvGappaProofResult gappa_prove(const lvGappaPredicate *hyp, int hyp_count, const lvGappaProofGoal *goals,
                               int goal_count, const void *config);

/** Free proof result */
void gappa_result_free(lvGappaProofResult *result);

/** Register rewrite rules */
bool gappa_register_rewrite_rules(const lvGappaRewriteRule *rules, int count);

#ifdef __cplusplus
}
#endif

#endif
