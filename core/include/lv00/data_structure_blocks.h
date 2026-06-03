#ifndef LV00_DATA_STRUCTURE_BLOCKS_H
#define LV00_DATA_STRUCTURE_BLOCKS_H

#include "lv00/func_block.h"
#include "lv00/type_system.h"

#ifdef __cplusplus
extern "C" {
#endif

/* List operations */
typedef enum {
    LV00_LIST_CREATE,
    LV00_LIST_APPEND,
    LV00_LIST_GET,
    LV00_LIST_MAP,
    LV00_LIST_FILTER,
    LV00_LIST_REDUCE
} Lv00ListOp;

/* Map operations */
typedef enum {
    LV00_MAP_CREATE,
    LV00_MAP_INSERT,
    LV00_MAP_GET,
    LV00_MAP_REMOVE,
    LV00_MAP_KEYS,
    LV00_MAP_VALUES
} Lv00MapOp;

/* List block */
typedef struct Lv00ListBlock {
    void *base;
    void *elem_type;
    Lv00ListOp operation;
} Lv00ListBlock;

/* Map block */
typedef struct Lv00MapBlock {
    void *base;
    void *key_type;
    void *value_type;
    Lv00MapOp operation;
} Lv00MapBlock;

/* Record block */
typedef struct Lv00RecordBlock {
    void *base;
    struct {
        char *field_name;
        void *field_type;
        int field_port;
    } *fields;
    int field_count;
} Lv00RecordBlock;

/* Factory */
Lv00ListBlock *lv00_list_block_create(Lv00ListOp op);
void lv00_list_block_destroy(Lv00ListBlock *block);

Lv00MapBlock *lv00_map_block_create(Lv00MapOp op);
void lv00_map_block_destroy(Lv00MapBlock *block);

Lv00RecordBlock *lv00_record_block_create(int field_count);
void lv00_record_block_destroy(Lv00RecordBlock *block);
int lv00_record_block_set_field(Lv00RecordBlock *block, int index, const char *name, void *type);

#ifdef __cplusplus
}
#endif

#endif /* LV00_DATA_STRUCTURE_BLOCKS_H */
