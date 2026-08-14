/*
 * @file high_dim_zoom.c
 * @brief High-dim module - semantic zoom / perspective
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
#include "lv/stream_context_util.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_xmacro.h"

/* ==================== 语义缩放 ==================== */

int high_dim_enter_block_perspective(HighDimManager *manager, int block_id) {
    /**
     * 进入高维块内部透视（语义缩放）
     *
     * 切换画布上下文到指定高维块的局部坐标系。
     * 在C层实现基本的深度栈管理：将当前block_id压入栈顶，深度递增。
     * 完整的渲染语义（切换投影矩阵、更新视图层级）依赖UI层渲染引擎。
     *
     * 参数验证通过但UI层尚未集成时的行为：
     * - 深度栈push操作正常执行（C层状态正确）
     * - 设置warning提示UI层需要配合完成视觉切换
     *
     * @param manager 高维管理器指针
     * @param block_id 要进入透视的函数块ID
     * @return lv_OK 成功（深度栈已更新）
     *         lv_ERROR_INVALID_PARAM 参数无效
     *         lv_ERROR_NOT_FOUND 未找到对应的高维块
     *         lv_ERROR_UNSUPPORTED 深度栈已满
     */
    if (!manager)
        return lv_ERROR_INVALID_PARAM;

    HighDimAbstractBlock *block = high_dim_get_block(manager, block_id);
    if (!block) {
        lv_set_error(lv_ERROR_NOT_FOUND, "进入块透视失败：未找到block_id=%d对应的高维抽象块", block_id);
        return lv_ERROR_NOT_FOUND;
    }

    /* 检查深度栈是否已满 */
    if (manager->perspective_depth >= HIGH_DIM_MAX_DEPTH) {
        lv_set_error(lv_ERROR_UNSUPPORTED, "语义缩放深度栈已满（最大深度=%d），无法进入更深的透视层级",
                     HIGH_DIM_MAX_DEPTH);
        return lv_ERROR_UNSUPPORTED;
    }

    /* 将当前block_id压入深度栈 */
    manager->perspective_stack[manager->perspective_depth] = block_id;
    manager->perspective_depth++;

    if (high_dim_stream_ctx) {
        stream_emit_progress(high_dim_stream_ctx, 0.0, "语义缩放：进入块透视", block_id, -1);
    }

    /* DEBUG级别日志：提示UI层需要同步切换渲染管线 */
    LOG_DEBUG("high_dim", "已进入block_id=%d的内部透视，当前深度=%d。", block_id, manager->perspective_depth);

    return lv_OK;
}

int high_dim_exit_block_perspective(HighDimManager *manager) {
    /**
     * 退出高维块内部透视（语义缩放）
     *
     * 从深度栈pop顶部block_id，恢复到上一级透视的上下文。
     * C层负责深度栈的pop操作和状态管理。
     * 完整的视图恢复（切换渲染管线、还原投影矩阵）依赖UI层渲染引擎。
     *
     * @param manager 高维管理器指针
     * @return lv_OK 成功（深度栈已pop）
     *         lv_ERROR_INVALID_PARAM 参数无效
     *         lv_ERROR_UNSUPPORTED 深度栈已空（已在最外层）
     */
    if (!manager)
        return lv_ERROR_INVALID_PARAM;

    /* 检查深度栈是否已空 */
    if (manager->perspective_depth <= 0) {
        lv_set_error(lv_ERROR_UNSUPPORTED, "当前已在最外层透视，无法继续退出");
        return lv_ERROR_UNSUPPORTED;
    }

    /* 获取即将退出的block_id并pop栈 */
    int exited_block_id = manager->perspective_stack[manager->perspective_depth - 1];
    manager->perspective_stack[manager->perspective_depth - 1] = 0;
    manager->perspective_depth--;

    /* DEBUG级别日志：提示UI层需要同步恢复上层视图 */
    LOG_DEBUG("high_dim", "已退出block_id=%d的内部透视，恢复到深度=%d。", exited_block_id, manager->perspective_depth);

    return lv_OK;
}

int high_dim_get_current_depth(const HighDimManager *manager) {
    /**
     * 获取当前语义缩放透视深度
     *
     * 返回深度栈中当前记录的透视深度（即进入了几层块内部）。
     * 深度为0表示在最外层（无透视）。
     *
     * @param manager 高维管理器指针（const，只读操作）
     * @return 当前透视深度（>= 0），manager为NULL时返回-1
     */
    if (!manager)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "high_dim_get_current_depth: manager is NULL");

    /* 直接返回C层维护的深度计数值 */
    return manager->perspective_depth;
}

