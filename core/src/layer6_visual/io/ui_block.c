#include "lv/io_blocks.h"
#include "lv/lv_utils.h"

lvUIEventBlock *lv_ui_event_block_create(lvEffectType effect) {
    lvUIEventBlock *block = lv_calloc(1, sizeof(lvUIEventBlock));
    if (!block) return NULL;
    block->effect = effect;
    block->event_port = -1;
    block->action_port = -1;
    return block;
}

void lv_ui_event_block_destroy(lvUIEventBlock *block) {
    lv_free((void **)&block);
}
