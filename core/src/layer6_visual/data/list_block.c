#include "lv00/data_structure_blocks.h"
#include "lv00/lv00_utils.h"

Lv00ListBlock *lv00_list_block_create(Lv00ListOp op) {
    Lv00ListBlock *block = lv00_calloc(1, sizeof(Lv00ListBlock));
    if (!block) return NULL;
    block->operation = op;
    return block;
}

void lv00_list_block_destroy(Lv00ListBlock *block) {
    lv00_free((void **)&block);
}
