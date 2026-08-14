/*
 * @file high_dim_core.c
 * @brief High-dim module - lifecycle and block operations
 * @details Split from high_dim.c
 */

#include "lv/high_dim.h"
#include "high_dim_internal.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "lv/config.h"
#include "lv/lv_json.h"
#include "lv/lv_parse_utils.h"

#include "lv/debug.h"
#include "lv/error_codes.h"
#include "lv/lv_internal.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"
#include "lv/stream.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_xmacro.h"

/* ==================== 生命周期管理 ==================== */

/**
 * @brief 创建高维管理器
 *
 * 分配并初始化 HighDimManager，调用 high_dim_manager_init 完成内部状态设置。
 *
 * @return 新分配的管理器指针，失败返回 NULL
 */
HighDimManager *high_dim_manager_create(void) {
    HighDimManager *manager = (HighDimManager *) lv_malloc(sizeof(HighDimManager));
    if (!manager)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "high_dim_manager_create: malloc failed");

    if (high_dim_manager_init(manager) != 0) {
        lv_free((void **) &manager);
        lv_ERROR_SET(lv_ERROR_INTERNAL, "high_dim_manager_create: init failed");
        return NULL;
    }

    return manager;
}

/**
 * @brief 销毁高维管理器
 *
 * 释放高维块数组和管管理器本身。HighDimAbstractBlock 仅含标量和固定大小数组，
 * 无需逐个释放。
 *
 * @param manager 管理器指针（可为 NULL）
 */
void high_dim_manager_destroy(HighDimManager *manager) {
    if (!manager)
        return;

    /* HighDimAbstractBlock 仅含标量和固定大小数组，无动态资源需要释放；
     * 已移除空 for 循环（迭代无副作用）。 */
    lv_darray_free(&manager->blocks);

    lv_free((void **) &manager);
}

/**
 * @brief 初始化高维管理器
 *
 * 分配初始容量为 HIGH_DIM_INITIAL_CAPACITY 的高维块数组。
 *
 * @param manager 管理器指针
 * @return lv_OK 成功，错误码表示失败原因
 */
int high_dim_manager_init(HighDimManager *manager) {
    if (!manager)
        return lv_ERROR_INVALID_PARAM;

    lv_darray_init(&manager->blocks, sizeof(HighDimAbstractBlock));
    if (!lv_darray_reserve(&manager->blocks, HIGH_DIM_INITIAL_CAPACITY)) {
        return lv_ERROR_OUT_OF_MEMORY;
    }

    /* 初始化语义缩放深度栈 */
    manager->perspective_depth = 0;
    memset(manager->perspective_stack, 0, sizeof(manager->perspective_stack));

    return lv_OK;
}


/* ==================== 高维块操作 ==================== */

/**
 * @brief 注册高维块
 *
 * 向管理器添加一个新的高维抽象块，指定维度数量。
 *
 * @param manager         管理器指针
 * @param block_id        块 ID
 * @param dimension_count 维度数量
 * @return lv_OK 成功，错误码表示失败原因
 */
int high_dim_register_block(HighDimManager *manager, int block_id, int dimension_count) {
    if (!manager || dimension_count < 4 || dimension_count > HIGH_DIM_MAX_DIMENSIONS) {
        return lv_ERROR_INVALID_PARAM;
    }

    /* 检查是否已存在 */
    HighDimAbstractBlock *blocks_arr = (HighDimAbstractBlock *) manager->blocks.data;
    for (int i = 0; i < manager->blocks.count; i++) {
        if (blocks_arr[i].block_id == block_id) {
            return lv_ERROR_ALREADY_EXISTS;
        }
    }

    /* 确保容量（lv_darray_reserve 替代手写扩容） */
    if (!lv_darray_reserve(&manager->blocks, manager->blocks.count + 1)) {
        return lv_ERROR_OUT_OF_MEMORY;
    }

    /* 初始化新块 */
    blocks_arr = (HighDimAbstractBlock *) manager->blocks.data; /* 扩容后重取指针 */
    HighDimAbstractBlock *block = &blocks_arr[manager->blocks.count];
    memset(block, 0, sizeof(HighDimAbstractBlock));

    block->block_id = block_id;
    block->dimension_count = dimension_count;
    block->preset_count = 0;
    block->current_preset_index = -1;
    block->fidelity_ratio = 1.0;

    /* 创建默认投影预设 */
    HighDimProjectionPreset default_preset;
    int result = high_dim_create_default_preset(dimension_count, &default_preset);
    if (result != lv_OK) {
        return result;
    }

    memcpy(&block->presets[0], &default_preset, sizeof(HighDimProjectionPreset));
    block->preset_count = 1;
    block->current_preset_index = 0;

    manager->blocks.count++;

    return lv_OK;
}

/**
 * @brief 注销高维块
 *
 * 从管理器中移除指定 ID 的高维块，将最后一个块移到被删除位置以保持数组紧凑。
 *
 * @param manager  管理器指针
 * @param block_id 块 ID
 * @return lv_OK 成功，错误码表示失败原因
 */
int high_dim_unregister_block(HighDimManager *manager, int block_id) {
    if (!manager)
        return lv_ERROR_INVALID_PARAM;

    HighDimAbstractBlock *blocks_arr = (HighDimAbstractBlock *) manager->blocks.data;
    int index = -1;
    for (int i = 0; i < manager->blocks.count; i++) {
        if (blocks_arr[i].block_id == block_id) {
            index = i;
            break;
        }
    }

    if (index < 0) {
        return lv_ERROR_NOT_FOUND;
    }

    /* 移动后续元素：统一走 lv_shift_left（单次 memmove 替代循环） */
    lv_shift_left(blocks_arr, sizeof(blocks_arr[0]), (size_t) index, (size_t) manager->blocks.count);

    manager->blocks.count--;

    return lv_OK;
}

/**
 * @brief 根据块 ID 查找高维块
 *
 * 在管理器的高维块数组中线性搜索指定 ID 的块。
 *
 * @param manager  管理器指针
 * @param block_id 块 ID
 * @return 高维块指针，未找到或 manager 为 NULL 时返回 NULL
 */
HighDimAbstractBlock *high_dim_get_block(HighDimManager *manager, int block_id) {
    if (!manager)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "high_dim_get_block: manager is NULL");

    HighDimAbstractBlock *blocks_arr = (HighDimAbstractBlock *) manager->blocks.data;
    for (int i = 0; i < manager->blocks.count; i++) {
        if (blocks_arr[i].block_id == block_id) {
            return &blocks_arr[i];
        }
    }

    return NULL;
}

