#ifndef lv_PROOF_TRACE_H
#define lv_PROOF_TRACE_H
#include <stdbool.h>
#include "lv/lv_api_spec.h"
#include "lv/lv_utils.h"
#ifdef __cplusplus
extern "C" {
#endif
/* 不透明类型：完整定义仅在 proof_trace.c 中 */
typedef struct ProofTrace ProofTrace;

/* 不透明类型：证明优化器，完整定义在 proof_optimize.c 中 */
typedef struct ProofOptimizer ProofOptimizer;
int lv_proof_trace_add_step(ProofTrace *t, const char *rule, const void *state);
/* 访问器函数 */
int lv_proof_trace_get_step_count(const ProofTrace *t);
const char *lv_proof_trace_get_rule(const ProofTrace *t, int step_index);
bool lv_proof_trace_is_complete(const ProofTrace *t);

/* ── Proof Tree API (used by test_proof_trace.c) ── */
typedef struct lvProofPremise {
    int premise_id;
    char description[256];
    bool is_axiom;
} lvProofPremise;

typedef struct lvProofTreeNode {
    int id;
    char description[256];
    char detail[512];
    int step_type;
    bool is_contradiction;
    bool is_contradiction_branch;
    struct lvProofTreeNode *parent;
    lvDArray children;       /**< lvProofTreeNode * 指针数组 */
    int depth;
    int step_index;
    lvDArray premises;       /**< lvProofPremise 元素数组 */
    char *axiom_used;
    char *conclusion;
} lvProofTreeNode;

typedef struct lvProofTree {
    char name[256];
    char strategy[128];
    char *theorem_name;
    char *proof_strategy;
    lvProofTreeNode *root;
    lvDArray all_nodes;      /**< lvProofTreeNode * 指针数组 */
    int next_id;
    int total_steps;
    int max_depth;
    bool is_complete;
} lvProofTree;

lvProofTree *lv_proof_tree_create(const char *name, const char *strategy);
void lv_proof_tree_destroy(lvProofTree *tree);
lvProofTreeNode *lv_proof_tree_add_step(lvProofTree *tree, lvProofTreeNode *parent, const char *desc,
                                        const char *detail, int id);
bool lv_proof_tree_mark_contradiction(lvProofTreeNode *node);
char *lv_proof_tree_export_text(const lvProofTree *tree, const char *opts);

/* ── Proof Optimizer API（内部模块 API，非公共导出）── */
ProofOptimizer *lv_proof_opt_create(void);
void lv_proof_opt_destroy(ProofOptimizer *opt);
int lv_proof_opt_add_step(ProofOptimizer *opt, const char *rule, const int *deps, int dep_count);
int lv_proof_opt_dead_step_elimination(ProofOptimizer *opt, int final_step);
int lv_proof_opt_merge_steps(ProofOptimizer *opt);
int lv_proof_opt_active_count(const ProofOptimizer *opt);

#ifdef __cplusplus
}
#endif
#endif
