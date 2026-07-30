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
#include "lv/lv_block_utils.h"

/**
 * @brief 创建映射块
 *
 * 分配并初始化一个映射块，指定操作类型。
 *
 * @param op 映射操作类型（如合并、过滤、转换等）
 * @return 成功返回映射块指针，失败返回NULL
 */
LV_SIMPLE_BLOCK_PARAM(lvMapBlock, lv_map_block, (lvMapOp op), ({
    block->operation = op;
}))

/**
 * @brief 销毁映射块
 *
 * 释放映射块占用的内存。
 *
 * @param block 映射块指针
 */
/* destroy 由 LV_SIMPLE_BLOCK_PARAM 宏自动生成 */
