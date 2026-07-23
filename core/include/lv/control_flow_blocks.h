#ifndef lv_CONTROL_FLOW_BLOCKS_H
#define lv_CONTROL_FLOW_BLOCKS_H

#include "lv/func_block.h"
#include "lv/type_system.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Determinism states for control flow */
typedef enum {
    lv_DETERMINISM_PURE,
    lv_DETERMINISM_CONDITIONAL,
    lv_DETERMINISM_LOOP_REQUIRES_PROOF,
    lv_DETERMINISM_VERIFIED,
    lv_DETERMINISM_NONDETERMINISTIC
} lvDeterminismState;

/* If block */
typedef struct lvIfBlock {
    void *base;  /* FuncBlock base */

    int condition_port;
    int then_output;
    int else_output;

    struct {
        void *then_branch;
        void *else_branch;
    } branches;

    lvDeterminismState determinism;
} lvIfBlock;

/* While block */
typedef struct lvWhileBlock {
    void *base;

    int init_port;
    int condition_port;
    int output_port;

    void *body;

    /* Loop invariant for proof */
    void *invariant;

    lvDeterminismState determinism;
    int max_iterations;
} lvWhileBlock;

/* Match block */
typedef struct lvMatchBlock {
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
} lvMatchBlock;

/* Factory functions */
lvIfBlock *lv_if_block_create(void);
void lv_if_block_destroy(lvIfBlock *block);
int lv_if_block_set_branches(lvIfBlock *block, void *then_branch, void *else_branch);

lvWhileBlock *lv_while_block_create(void);
void lv_while_block_destroy(lvWhileBlock *block);
int lv_while_block_set_body(lvWhileBlock *block, void *body);
int lv_while_block_set_invariant(lvWhileBlock *block, void *invariant);

lvMatchBlock *lv_match_block_create(int case_count);
void lv_match_block_destroy(lvMatchBlock *block);
int lv_match_block_set_case(lvMatchBlock *block, int index, void *pattern, void *handler);
int lv_match_block_set_default(lvMatchBlock *block, void *handler);

#ifdef __cplusplus
}
#endif

#endif /* lv_CONTROL_FLOW_BLOCKS_H */
