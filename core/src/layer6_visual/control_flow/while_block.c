#include "lv00/control_flow_blocks.h"
#include <stdlib.h>

Lv00WhileBlock *lv00_while_block_create(void) {
    Lv00WhileBlock *block = calloc(1, sizeof(Lv00WhileBlock));
    if (!block) return NULL;
    block->init_port = -1;
    block->condition_port = -1;
    block->output_port = -1;
    block->determinism = LV00_DETERMINISM_LOOP_REQUIRES_PROOF;
    block->max_iterations = 10000;
    return block;
}

void lv00_while_block_destroy(Lv00WhileBlock *block) {
    free(block);
}

int lv00_while_block_set_body(Lv00WhileBlock *block, void *body) {
    if (!block) return -1;
    block->body = body;
    return 0;
}

int lv00_while_block_set_invariant(Lv00WhileBlock *block, void *invariant) {
    if (!block) return -1;
    block->invariant = invariant;
    if (invariant) {
        block->determinism = LV00_DETERMINISM_VERIFIED;
    }
    return 0;
}
