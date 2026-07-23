#include "lv/data_structure_blocks.h"
#include "lv/lv_utils.h"

lvListBlock *lv_list_block_create(lvListOp op) {
    lvListBlock *block = lv_calloc(1, sizeof(lvListBlock));
    if (!block) return NULL;
    block->operation = op;
    return block;
}

void lv_list_block_destroy(lvListBlock *block) {
    lv_free((void **)&block);
}
