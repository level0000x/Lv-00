#include "lv/data_structure_blocks.h"
#include "lv/lv_utils.h"
#include <string.h>

lvRecordBlock *lv_record_block_create(int field_count) {
    lvRecordBlock *block = lv_calloc(1, sizeof(lvRecordBlock));
    if (!block) return NULL;
    if (field_count > 0) {
        block->fields = lv_calloc(field_count, sizeof(block->fields[0]));
        if (!block->fields) {
            lv_free((void **)&block);
            return NULL;
        }
        block->field_count = field_count;
    }
    return block;
}

void lv_record_block_destroy(lvRecordBlock *block) {
    if (!block) return;
    for (int i = 0; i < block->field_count; i++) {
        lv_free((void **)&block->fields[i].field_name);
    }
    lv_free((void **)&block->fields);
    lv_free((void **)&block);
}

int lv_record_block_set_field(lvRecordBlock *block, int index, const char *name, void *type) {
    if (!block || index < 0 || index >= block->field_count) return -1;
    lv_free((void **)&block->fields[index].field_name);
    block->fields[index].field_name = name ? lv_strdup(name) : NULL;
    block->fields[index].field_type = type;
    return 0;
}
