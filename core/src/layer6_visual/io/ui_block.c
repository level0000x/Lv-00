#include "lv00/io_blocks.h"
#include "lv00/lv00_utils.h"

Lv00UIEventBlock *lv00_ui_event_block_create(Lv00EffectType effect) {
    Lv00UIEventBlock *block = lv00_calloc(1, sizeof(Lv00UIEventBlock));
    if (!block) return NULL;
    block->effect = effect;
    block->event_port = -1;
    block->action_port = -1;
    return block;
}

void lv00_ui_event_block_destroy(Lv00UIEventBlock *block) {
    lv00_free((void **)&block);
}
