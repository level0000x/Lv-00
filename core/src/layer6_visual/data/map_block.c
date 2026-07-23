#include "lv/data_structure_blocks.h"
#include "lv/lv_utils.h"

lvMapBlock *lv_map_block_create(lvMapOp op) {
    lvMapBlock *block = lv_calloc(1, sizeof(lvMapBlock));
    if (!block) return NULL;
    block->operation = op;
    return block;
}

void lv_map_block_destroy(lvMapBlock *block) {
    lv_free((void **)&block);
}
