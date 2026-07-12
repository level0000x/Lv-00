#ifndef LV00_PROOF_TRACE_H
#define LV00_PROOF_TRACE_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/* 不透明类型：完整定义仅在 proof_trace.c 中 */
typedef struct ProofTrace ProofTrace;
ProofTrace *lv00_proof_trace_create(void);
void lv00_proof_trace_destroy(ProofTrace *t);
int lv00_proof_trace_add_step(ProofTrace *t, const char *rule, const void *state);
/* 访问器函数 */
int lv00_proof_trace_get_step_count(const ProofTrace *t);
bool lv00_proof_trace_is_complete(const ProofTrace *t);

/* ── Proof Tree API (used by test_proof_trace.c) ── */
typedef struct Lv00ProofPremise {
    int premise_id;
    char description[256];
    bool is_axiom;
} Lv00ProofPremise;

typedef struct Lv00ProofTreeNode {
    int id;
    char description[256];
    char detail[512];
    int step_type;
    bool is_contradiction;
    bool is_contradiction_branch;
    struct Lv00ProofTreeNode *parent;
    struct Lv00ProofTreeNode **children;
    int child_count;
    int child_capacity;
    int depth;
    int step_index;
    Lv00ProofPremise *premises;
    int premise_count;
    int premise_capacity;
    char *axiom_used;
    char *conclusion;
} Lv00ProofTreeNode;

typedef struct Lv00ProofTree {
    char name[256];
    char strategy[128];
    char *theorem_name;
    char *proof_strategy;
    Lv00ProofTreeNode *root;
    Lv00ProofTreeNode **all_nodes;
    int node_count;
    int node_capacity;
    int next_id;
    int total_steps;
    int max_depth;
    bool is_complete;
} Lv00ProofTree;

Lv00ProofTree *lv00_proof_tree_create(const char *name, const char *strategy);
void lv00_proof_tree_destroy(Lv00ProofTree *tree);
Lv00ProofTreeNode *lv00_proof_tree_add_step(Lv00ProofTree *tree, Lv00ProofTreeNode *parent,
                                             const char *desc, const char *detail, int id);
bool lv00_proof_tree_mark_contradiction(Lv00ProofTree *tree, Lv00ProofTreeNode *node);
char *lv00_proof_tree_export_text(const Lv00ProofTree *tree, const char *opts);

#ifdef __cplusplus
}
#endif
#endif
