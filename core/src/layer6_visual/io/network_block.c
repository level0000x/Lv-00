#include "lv00/io_blocks.h"
#include "lv00/lv00_utils.h"

Lv00NetworkBlock *lv00_network_block_create(void) {
    Lv00NetworkBlock *block = lv00_calloc(1, sizeof(Lv00NetworkBlock));
    if (!block) return NULL;
    block->effect = LV00_EFFECT_NETWORK;
    block->url_port = -1;
    block->request_port = -1;
    block->response_port = -1;
    block->status_port = -1;
    return block;
}

void lv00_network_block_destroy(Lv00NetworkBlock *block) {
    lv00_free((void **)&block);
}
