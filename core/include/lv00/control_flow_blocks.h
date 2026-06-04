#ifndef LV00_CONTROL_FLOW_BLOCKS_H
#define LV00_CONTROL_FLOW_BLOCKS_H

#include "lv00/func_block.h"
#include "lv00/type_system.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Determinism states for control flow */
typedef enum {
    LV00_DETERMINISM_PURE,
    LV00_DETERMINISM_CONDITIONAL,
    LV00_DETERMINISM_LOOP_REQUIRES_PROOF,
    LV00_DETERMINISM_VERIFIED,
    LV00_DETERMINISM_NONDETERMINISTIC
} Lv00DeterminismState;

/* If block */
typedef struct Lv00IfBlock {
    void *base;  /* FuncBlock base */

    int condition_port;
    int then_output;
    int else_output;

    struct {
        void *then_branch;
        void *else_branch;
    } branches;

    Lv00DeterminismState determinism;
} Lv00IfBlock;

/* While block */
typedef struct Lv00WhileBlock {
    void *base;

    int init_port;
    int condition_port;
    int output_port;

    void *body;

    /* Loop invariant for proof */
    void *invariant;

    Lv00DeterminismState determinism;
    int max_iterations;
} Lv00WhileBlock;

/* Match block */
typedef struct Lv00MatchBlock {
    void *base;

    int input_port;
    int output_port;

    struct {
        void *pattern;
        void *handler;
        int output_port;
    } *cases;
    int case_count;

    void *default_handler;
} Lv00MatchBlock;

/* Factory functions */
Lv00IfBlock *lv00_if_block_create(void);
void lv00_if_block_destroy(Lv00IfBlock *block);
int lv00_if_block_set_branches(Lv00IfBlock *block, void *then_branch, void *else_branch);

Lv00WhileBlock *lv00_while_block_create(void);
void lv00_while_block_destroy(Lv00WhileBlock *block);
int lv00_while_block_set_body(Lv00WhileBlock *block, void *body);
int lv00_while_block_set_invariant(Lv00WhileBlock *block, void *invariant);

Lv00MatchBlock *lv00_match_block_create(int case_count);
void lv00_match_block_destroy(Lv00MatchBlock *block);
int lv00_match_block_set_case(Lv00MatchBlock *block, int index, void *pattern, void *handler);
int lv00_match_block_set_default(Lv00MatchBlock *block, void *handler);

#ifdef __cplusplus
}
#endif

#endif /* LV00_CONTROL_FLOW_BLOCKS_H */
