#include <string.h>

#include "lv/control_flow_blocks.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"

lvMatchBlock *lv_match_block_create(int case_count) {
    lvMatchBlock *block = lv_calloc(1, sizeof(lvMatchBlock));
    if (!block)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "failed to allocate match block");
    block->input_port = -1;
    block->output_port = -1;
    if (case_count > 0) {
        block->cases = lv_calloc(case_count, sizeof(block->cases[0]));
        if (!block->cases) {
            lv_free((void **) &block);
            lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "failed to allocate cases array");
        }
        block->case_count = case_count;
    }
    return block;
}

void lv_match_block_destroy(lvMatchBlock *block) {
    if (!block)
        return;
    lv_free((void **) &block->cases);
    lv_free((void **) &block);
}

int lv_match_block_set_case(lvMatchBlock *block, int index, void *pattern, void *handler) {
    if (!block || index < 0 || index >= block->case_count)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "NULL block or invalid index");
    block->cases[index].pattern = pattern;
    block->cases[index].handler = handler;
    return 0;
}

int lv_match_block_set_default(lvMatchBlock *block, void *handler) {
    if (!block)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "NULL block");
    block->default_handler = handler;
    return 0;
}
