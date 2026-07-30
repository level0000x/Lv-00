/**
 * @file map_block.c
 * @brief 映射块数据实现
 *
 * @details 实现映射块（MapBlock）的创建和销毁。
 *          映射块用于表示键值对操作（如合并、过滤键、取值等），
 *          是数据流图中处理字典/映射数据的节点类型。
 *
 * @author Lv-00 Project
 */

#include "lv/data_structure_blocks.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"

/**
 * @brief 创建映射块
 *
 * 分配并初始化一个映射块，指定操作类型。
 *
 * @param op 映射操作类型（如合并、过滤、转换等）
 * @return 成功返回映射块指针，失败返回NULL
 */
lvMapBlock *lv_map_block_create(lvMapOp op) {
    lvMapBlock *block = lv_calloc(1, sizeof(lvMapBlock));
    if (!block)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "failed to allocate map block");
    block->operation = op;
    return block;
}

/**
 * @brief 销毁映射块
 *
 * 释放映射块占用的内存。
 *
 * @param block 映射块指针
 */
void lv_map_block_destroy(lvMapBlock *block) {
    lv_free((void **) &block);
}
