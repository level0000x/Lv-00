#ifndef LV00_GAPPA_DSL_H
#define LV00_GAPPA_DSL_H
#include "lv00/gappa_propagate.h"
#include <stdbool.h>
#include <stddef.h>

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
} Lv00GappaFormat;

/** Predefined format helper */
int gappa_format_predefined(const char *name, Lv00GappaFormat *out);

/** Rounding modes */
typedef enum {
    LV00_ROUND_NE = 0,
    LV00_ROUND_NA,
    LV00_ROUND_NU,
    LV00_ROUND_ND,
    LV00_ROUND_ZR
} Lv00RoundingMode;

/** Predicate types */
typedef enum {
    LV00_PRED_BND = 0,
    LV00_PRED_ABS,
    LV00_PRED_REL
} Lv00PredType;

/** Gappa predicate */
typedef struct Lv00GappaPredicate {
    Lv00PredType type;
    char expr_lhs[256];
    char expr_rhs[256];
    double bound_lo;
    double bound_hi;
    double bound_abs;
    bool is_hypothesis;
} Lv00GappaPredicate;

/** Gappa proof goal */
typedef struct {
    Lv00GappaPredicate predicate;
    char target_expr[256];
    double bound_lo;
    double bound_hi;
    bool proven;
} Lv00GappaProofGoal;

/** Gappa proof result */
typedef struct {
    bool success;
    int goals_total;
    int goals_proven;
    int goals_failed;
    Lv00GappaProofGoal *goals;
} Lv00GappaProofResult;

/** Rewrite rule */
typedef struct {
    char match_pattern[256];
    char replace_pattern[256];
    char description[256];
} Lv00GappaRewriteRule;

/** Parse Gappa DSL expression. */
int lv00_gappa_parse(const char *input);

/** Parse with structured output */
int gappa_parse(const char *input, Lv00GappaPredicate **hyp, int *hyp_count,
                 Lv00GappaProofGoal **goals, int *goal_count);

/** Free parsed results */
void gappa_predicates_free(Lv00GappaPredicate *preds, int count);
void gappa_goals_free(Lv00GappaProofGoal *goals, int count);

/** Evaluate Gappa expression with interval bounds. */
int lv00_gappa_eval(const char *expr, double *lo, double *hi);

/** Generate proof from Gappa script. */
char *lv00_gappa_prove(const char *script);

/** Prove with structured API */
Lv00GappaProofResult gappa_prove(const Lv00GappaPredicate *hyp, int hyp_count,
                                  const Lv00GappaProofGoal *goals, int goal_count,
                                  const void *config);

/** Free proof result */
void gappa_result_free(Lv00GappaProofResult *result);

/** Register rewrite rules */
int gappa_register_rewrite_rules(const Lv00GappaRewriteRule *rules, int count);

#ifdef __cplusplus
}
#endif

#endif
