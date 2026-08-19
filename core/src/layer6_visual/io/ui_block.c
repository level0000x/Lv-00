/**
 * @file ui_block.c
 * @brief UI事件块实现
 *
 * @details 实现 UI 事件块的创建和销毁。
 *          UI 事件块用于处理用户界面交互事件，
 *          通过事件端口和动作端口与 UI 系统通信。
 *
 * @author Lv-00 Project
 */

#include "lv/io_blocks.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/lv_block_utils.h"

/**
 * @brief 创建 UI 事件块
 *
 * 分配并初始化一个 UI 事件块，指定效果类型。
 * 初始事件端口和动作端口均设为 -1（未连接）。
 *
 * @param effect 效果类型
 * @return 成功返回 UI 事件块指针，失败返回NULL
 */
lvUIEventBlock *lv_ui_event_block_create(lvEffectType effect) {
    lvUIEventBlock *block = lv_calloc(1, sizeof(lvUIEventBlock));
    if (!block)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "failed to allocate lvUIEventBlock");
    block->effect = effect;
    block->event_port = -1;
    block->action_port = -1;
    return block;
}

void lv_ui_event_block_destroy(lvUIEventBlock *block) {
    lv_free((void **)&block);
}

/**
 * @brief 销毁 UI 事件块
 *
 * 释放 UI 事件块占用的内存。
 *
 * @param block UI 事件块指针
 */
/* destroy 已在上方手写实现 */
