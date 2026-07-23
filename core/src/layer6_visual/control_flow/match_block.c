#include <string.h>

#include "lv/control_flow_blocks.h"
#include "lv/lv_utils.h"

lvMatchBlock *lv_match_block_create(int case_count) {
    lvMatchBlock *block = lv_calloc(1, sizeof(lvMatchBlock));
    if (!block)
        return NULL;
    block->input_port = -1;
    block->output_port = -1;
    if (case_count > 0) {
        block->cases = lv_calloc(case_count, sizeof(block->cases[0]));
        if (!block->cases) {
            lv_free((void **) &block);
            return NULL;
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
        return -1;
    block->cases[index].pattern = pattern;
    block->cases[index].handler = handler;
    return 0;
}

int lv_match_block_set_default(lvMatchBlock *block, void *handler) {
    if (!block)
        return -1;
    block->default_handler = handler;
    return 0;
}
