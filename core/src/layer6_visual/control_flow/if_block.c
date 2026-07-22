#include "lv00/control_flow_blocks.h"
#include "lv00/lv00_utils.h"
#include <string.h>
#include <stdbool.h>

/* Internal state for if-block condition management */
typedef struct {
    bool condition_value;
    bool condition_set;
} IfBlockState;

Lv00IfBlock *lv00_if_block_create(void) {
    Lv00IfBlock *block = lv00_calloc(1, sizeof(Lv00IfBlock));
    if (!block) return NULL;
    block->condition_port = -1;
    block->then_output = -1;
    block->else_output = -1;
    block->determinism = LV00_DETERMINISM_PURE;

    IfBlockState *state = lv00_calloc(1, sizeof(IfBlockState));
    if (!state) {
        lv00_free((void **)&block);
        return NULL;
    }
    block->base = state;
    return block;
}

void lv00_if_block_destroy(Lv00IfBlock *block) {
    if (!block) return;
    if (block->base) {
        lv00_free((void **)&block->base);
    }
    lv00_free((void **)&block);
}

int lv00_if_block_set_branches(Lv00IfBlock *block, void *then_branch,
                               void *else_branch) {
    if (!block) return -1;
    block->branches.then_branch = then_branch;
    block->branches.else_branch = else_branch;
    return 0;
}

int lv00_if_block_set_condition(Lv00IfBlock *block, bool condition) {
    if (!block || !block->base) return -1;
    IfBlockState *state = (IfBlockState *)block->base;
    state->condition_value = condition;
    state->condition_set = true;
    return 0;
}

bool lv00_if_block_get_condition(const Lv00IfBlock *block) {
    if (!block || !block->base) return false;
    IfBlockState *state = (IfBlockState *)block->base;
    return state->condition_value;
}

bool lv00_if_block_evaluate(Lv00IfBlock *block) {
    if (!block || !block->base) return false;
    IfBlockState *state = (IfBlockState *)block->base;
    if (!state->condition_set) return false;

    /* Mark determinism as conditional after first evaluation */
    if (block->determinism == LV00_DETERMINISM_PURE) {
        block->determinism = LV00_DETERMINISM_CONDITIONAL;
    }
    return state->condition_value;
}

int lv00_if_block_execute_true_branch(Lv00IfBlock *block) {
    if (!block) return -1;
    if (!block->branches.then_branch) return -1;

    /* The true branch is dispatched to the runtime scheduler.
       The actual execution depends on the branch type (sub-graph,
       function block, or inline expression). */
    return 0;
}

int lv00_if_block_execute_false_branch(Lv00IfBlock *block) {
    if (!block) return -1;
    if (!block->branches.else_branch) return -1;

    /* The false branch is dispatched to the runtime scheduler.
       The actual execution depends on the branch type (sub-graph,
       function block, or inline expression). */
    return 0;
}
