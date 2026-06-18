#include "lv00/control_flow_blocks.h"
#include "lv00/lv00_utils.h"
#include <string.h>

Lv00MatchBlock *lv00_match_block_create(int case_count) {
    Lv00MatchBlock *block = lv00_calloc(1, sizeof(Lv00MatchBlock));
    if (!block) return NULL;
    block->input_port = -1;
    block->output_port = -1;
    if (case_count > 0) {
        block->cases = lv00_calloc(case_count, sizeof(block->cases[0]));
        if (!block->cases) {
            lv00_free((void **)&block);
            return NULL;
        }
        block->case_count = case_count;
    }
    return block;
}

void lv00_match_block_destroy(Lv00MatchBlock *block) {
    if (!block) return;
    lv00_free((void **)&block->cases);
    lv00_free((void **)&block);
}

int lv00_match_block_set_case(Lv00MatchBlock *block, int index, void *pattern, void *handler) {
    if (!block || index < 0 || index >= block->case_count) return -1;
    block->cases[index].pattern = pattern;
    block->cases[index].handler = handler;
    return 0;
}

int lv00_match_block_set_default(Lv00MatchBlock *block, void *handler) {
    if (!block) return -1;
    block->default_handler = handler;
    return 0;
}
