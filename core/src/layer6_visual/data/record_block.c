#include "lv00/data_structure_blocks.h"
#include "lv00/lv00_utils.h"
#include <string.h>

Lv00RecordBlock *lv00_record_block_create(int field_count) {
    Lv00RecordBlock *block = lv00_calloc(1, sizeof(Lv00RecordBlock));
    if (!block) return NULL;
    if (field_count > 0) {
        block->fields = lv00_calloc(field_count, sizeof(block->fields[0]));
        block->field_count = field_count;
    }
    return block;
}

void lv00_record_block_destroy(Lv00RecordBlock *block) {
    if (!block) return;
    for (int i = 0; i < block->field_count; i++) {
        lv00_free((void **)&block->fields[i].field_name);
    }
    lv00_free((void **)&block->fields);
    lv00_free((void **)&block);
}

int lv00_record_block_set_field(Lv00RecordBlock *block, int index, const char *name, void *type) {
    if (!block || index < 0 || index >= block->field_count) return -1;
    lv00_free((void **)&block->fields[index].field_name);
    block->fields[index].field_name = name ? lv00_strdup(name) : NULL;
    block->fields[index].field_type = type;
    return 0;
}
