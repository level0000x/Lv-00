#include "lv00/control_flow_blocks.h"
#include <stdlib.h>
#include <string.h>

Lv00IfBlock *lv00_if_block_create(void) {
    Lv00IfBlock *block = calloc(1, sizeof(Lv00IfBlock));
    if (!block) return NULL;
    block->condition_port = -1;
    block->then_output = -1;
    block->else_output = -1;
    block->determinism = LV00_DETERMINISM_PURE;
    return block;
}

void lv00_if_block_destroy(Lv00IfBlock *block) {
    free(block);
}

int lv00_if_block_set_branches(Lv00IfBlock *block, void *then_branch, void *else_branch) {
    if (!block) return -1;
    block->branches.then_branch = then_branch;
    block->branches.else_branch = else_branch;
    return 0;
}
