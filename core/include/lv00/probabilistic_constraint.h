#ifndef LV00_PROBABILISTIC_CONSTRAINT_H
#define LV00_PROBABILISTIC_CONSTRAINT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "constraint_graph.h"

/* ── Prob Distribution Type ── */
typedef enum {
    PROB_DIST_UNIFORM  = 0,
    PROB_DIST_NORMAL,
    PROB_DIST_BETA,
    PROB_DIST_DISCRETE,
    PROB_DIST_CUSTOM
} ProbDistType;

/* ── Prob Distribution ── */
typedef double (*ProbDistFunc)(double x, double *params, int param_count);

typedef struct ProbDistribution {
    ProbDistType type;
    int          param_count;
    double      *params;
    ProbDistFunc pdf;
    ProbDistFunc cdf;
    double       support_lo;
    double       support_hi;
} ProbDistribution;

/* ── PCTL Formula Type ── */
typedef enum {
    PCTL_PROB_BOUND   = 0,
    PCTL_EVENTUALLY,
    PCTL_ALWAYS,
    PCTL_UNTIL,
    PCTL_NEXT,
    PCTL_STEADY_STATE,
    PCTL_ATOMIC
} PCTLFormulaType;

struct PCTLFormula {
    PCTLFormulaType type;
    double          p_bound;
    bool            upper_bound;      /* true = P≤p, false = P≥p */
    const char     *state_predicate;
    const char     *path_predicate;
    struct PCTLFormula *sub_formula;
};
typedef struct PCTLFormula PCTLFormula;

/* ── Prob Constraint Node ── */
typedef struct ProbConstraintNode {
    int               node_id;
    int               base_node_id;
    ProbDistribution *coord_dist;
    ProbDistribution *dist;         /* alias */
    bool              is_soft;
    bool              is_verified;
    double            probability;
    double            confidence;
    int               sample_count;
    double           *samples;
    PCTLFormula      *pctl_formula;
} ProbConstraintNode;

/* ── API ── */
ProbDistribution *prob_dist_create(ProbDistType type, double *params, int param_count);
void prob_dist_destroy(ProbDistribution *dist);
double prob_dist_pdf(ProbDistribution *dist, double x);
double prob_dist_cdf(ProbDistribution *dist, double x);
int prob_dist_sample(ProbDistribution *dist, int n_samples, double **out_samples);

ProbConstraintNode *prob_constraint_create(int node_id, ProbDistribution *dist);
void prob_constraint_destroy(ProbConstraintNode *node);
int prob_constraint_sample(ProbConstraintNode *node, int n_samples, double **out_samples);

bool prob_constraint_infer(const ConstraintGraph *graph, int target_var,
                            ProbConstraintNode **constraints, int n, double *out_prob);

#ifdef __cplusplus
}
#endif
#endif
