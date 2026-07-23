/**
 * @file record_block.c
 * @brief 记录块数据实现
 *
 * @details 实现记录块（RecordBlock）的创建、销毁和字段管理。
 *          记录块用于表示结构化数据记录，支持多个命名字段，
 *          每个字段可关联一个类型描述。
 *
 * @author Lv-00 Project
 */

#include "lv/data_structure_blocks.h"
#include "lv/lv_utils.h"
#include <string.h>

/**
 * @brief 创建记录块
 *
 * 分配并初始化一个记录块，预分配指定数量的字段槽位。
 *
 * @param field_count 字段数量（<=0时延迟分配）
 * @return 成功返回记录块指针，失败返回NULL
 */
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

/**
 * @brief 销毁记录块
 *
 * 释放所有字段的名称字符串、字段数组和记录块结构体。
 *
 * @param block 记录块指针
 */
void lv_record_block_destroy(lvRecordBlock *block) {
    if (!block) return;
    for (int i = 0; i < block->field_count; i++) {
        lv_free((void **)&block->fields[i].field_name);
    }
    lv_free((void **)&block->fields);
    lv_free((void **)&block);
}

/**
 * @brief 设置字段
 *
 * 设置记录块中指定索引的字段名称和类型。
 *
 * @param block 记录块指针
 * @param index 字段索引
 * @param name  字段名称（可为NULL，此时设为空）
 * @param type  字段类型指针
 * @return 成功返回0，失败返回-1
 */
int lv_record_block_set_field(lvRecordBlock *block, int index, const char *name, void *type) {
    if (!block || index < 0 || index >= block->field_count) return -1;
    lv_free((void **)&block->fields[index].field_name);
    block->fields[index].field_name = name ? lv_strdup(name) : NULL;
    block->fields[index].field_type = type;
    return 0;
}
