#ifndef lv_DATA_STRUCTURE_BLOCKS_H
#define lv_DATA_STRUCTURE_BLOCKS_H

#include "lv/func_block.h"
#include "lv/type_system.h"
#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */

#ifdef __cplusplus
extern "C" {
#endif

/* List operations */
typedef enum { lv_LIST_CREATE, lv_LIST_APPEND, lv_LIST_GET, lv_LIST_MAP, lv_LIST_FILTER, lv_LIST_REDUCE } lvListOp;

/* Map operations */
typedef enum { lv_MAP_CREATE, lv_MAP_INSERT, lv_MAP_GET, lv_MAP_REMOVE, lv_MAP_KEYS, lv_MAP_VALUES } lvMapOp;

/* List block */
typedef struct lvListBlock {
    void *base;
    void *elem_type;
    lvListOp operation;
} lvListBlock;

/* Map block */
typedef struct lvMapBlock {
    void *base;
    void *key_type;
    void *value_type;
    lvMapOp operation;
} lvMapBlock;

/* Record block */
typedef struct lvRecordBlock {
    void *base;
    struct {
        char *field_name;
        void *field_type;
        int field_port;
    } *fields;
    int field_count;
} lvRecordBlock;

/* Factory */
lvListBlock *lv_list_block_create(lvListOp op);
lv_PUBLIC_API void lv_list_block_destroy(lvListBlock *block);

lvMapBlock *lv_map_block_create(lvMapOp op);
lv_PUBLIC_API void lv_map_block_destroy(lvMapBlock *block);

lvRecordBlock *lv_record_block_create(int field_count);
lv_PUBLIC_API void lv_record_block_destroy(lvRecordBlock *block);
lv_PUBLIC_API int lv_record_block_set_field(lvRecordBlock *block, int index, const char *name, void *type);

#ifdef __cplusplus
}
#endif

#endif /* lv_DATA_STRUCTURE_BLOCKS_H */
