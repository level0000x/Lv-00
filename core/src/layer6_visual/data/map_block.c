#include "lv00/data_structure_blocks.h"
#include "lv00/lv00_utils.h"

Lv00MapBlock *lv00_map_block_create(Lv00MapOp op) {
    Lv00MapBlock *block = lv00_calloc(1, sizeof(Lv00MapBlock));
    if (!block) return NULL;
    block->operation = op;
    return block;
}

void lv00_map_block_destroy(Lv00MapBlock *block) {
    lv00_free((void **)&block);
}
