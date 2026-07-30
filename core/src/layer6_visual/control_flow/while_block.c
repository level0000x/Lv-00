#include "lv/control_flow_blocks.h"
#include "lv/lv_internal.h"

lvWhileBlock *lv_while_block_create(void) {
    lvWhileBlock *block = lv_calloc(1, sizeof(lvWhileBlock));
    if (!block)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "failed to allocate while block");
    block->init_port = -1;
    block->condition_port = -1;
    block->output_port = -1;
    block->determinism = lv_DETERMINISM_LOOP_REQUIRES_PROOF;
    block->max_iterations = lv_DEFAULT_MAX_ITERATIONS;
    return block;
}

void lv_while_block_destroy(lvWhileBlock *block) {
    lv_free((void **) &block);
}

int lv_while_block_set_body(lvWhileBlock *block, void *body) {
    if (!block)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "NULL block");
    block->body = body;
    return 0;
}

int lv_while_block_set_invariant(lvWhileBlock *block, void *invariant) {
    if (!block)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "NULL block");
    block->invariant = invariant;
    if (invariant) {
        block->determinism = lv_DETERMINISM_VERIFIED;
    }
    return 0;
}
