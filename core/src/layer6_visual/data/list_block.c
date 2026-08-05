/**
 * @file list_block.c
 * @brief 列表块数据实现
 *
 * @details 实现列表块（ListBlock）的创建和销毁。
 *          列表块用于表示列表操作（如映射、过滤、归约等），
 *          是数据流图中处理序列数据的节点类型。
 *
 * @author Lv-00 Project
 */

#include "lv/data_structure_blocks.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/lv_block_utils.h"

/**
 * @brief 创建列表块
 *
 * 分配并初始化一个列表块，指定操作类型。
 *
 * @param op 列表操作类型（如映射、过滤、归约等）
 * @return 成功返回列表块指针，失败返回NULL
 */
lvListBlock *lv_list_block_create(lvListOp op) {
    lvListBlock *block = lv_calloc(1, sizeof(lvListBlock));
    if (!block)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "failed to allocate lvListBlock");
    block->operation = op;
    return block;
}

void lv_list_block_destroy(lvListBlock *block) {
    lv_free((void **)&block);
}

/**
 * @brief 销毁列表块
 *
 * 释放列表块占用的内存。
 *
 * @param block 列表块指针
 */
/* destroy 已在上方手写实现 */
