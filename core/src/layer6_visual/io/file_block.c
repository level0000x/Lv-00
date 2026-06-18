#include "lv00/io_blocks.h"
#include "lv00/lv00_utils.h"

Lv00FileBlock *lv00_file_block_create(Lv00EffectType effect) {
    Lv00FileBlock *block = lv00_calloc(1, sizeof(Lv00FileBlock));
    if (!block) return NULL;
    block->effect = effect;
    block->path_port = -1;
    block->data_port = -1;
    block->result_port = -1;
    block->status_port = -1;
    return block;
}

void lv00_file_block_destroy(Lv00FileBlock *block) {
    lv00_free((void **)&block);
}
