/**
 * @file visual_editor.c
 * @brief 可视化编辑器实现
 *
 * @details 实现可视化编辑器的生命周期管理（创建/销毁/重置）、视图切换、
 *          全量执行和增量执行等核心功能。编辑器支持多种视图类型（节点图、
 *          几何画布、块画布、文本代码），并通过调度器执行块图。
 *
 * @author Lv-00 Project
 */

#include "lv/visual_editor.h"

#include <stdlib.h>
#include <string.h>

#include "lv/block_scheduler.h"
#include "lv/lv_error.h"
#include "lv/lv_utils.h"
#include "lv/lv_internal.h"
#include "lv/lv_check.h"

/* ============ 实例错误缓冲统一写入口 ============ */

/** @brief 写错误消息到编辑器实例错误缓冲（公共写入口 lv_ERROR_SLOT_COPY） */
static void editor_set_error(lvVisualEditor *editor, const char *msg) {
    if (!editor)
        return;
    lv_ERROR_SLOT_COPY(editor->last_error, sizeof(editor->last_error), msg);
}

/** @brief 清空编辑器实例错误缓冲（公共写入口 lv_ERROR_SLOT_CLEAR） */
static void editor_clear_error(lvVisualEditor *editor) {
    if (!editor)
        return;
    lv_ERROR_SLOT_CLEAR(editor->last_error);
}

/**
 * @brief 创建可视化编辑器实例
 *
 * 分配并初始化编辑器，默认视图为节点图，状态为空闲。
 *
 * @return 成功返回编辑器指针，失败返回NULL
 */
lvVisualEditor *lv_visual_editor_create(void) {
    lvVisualEditor *editor = lv_calloc(1, sizeof(lvVisualEditor));
    if (!editor)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "failed to allocate visual editor");
    editor->layer_id = lv_LAYER_VISUAL;
    editor->active_view = lv_VIEW_NODE_GRAPH;
    editor->state = lv_EDITOR_IDLE;
    return editor;
}

/**
 * @brief 销毁可视化编辑器实例
 *
 * 释放编辑器占用的内存。子视图由各自的管理器负责销毁。
 *
 * @param editor 编辑器指针
 */
void lv_visual_editor_destroy(lvVisualEditor *editor) {
    if (!editor)
        return;
    /* Sub-views destroyed by their own managers */
    lv_free((void **) &editor);
}

/**
 * @brief 重置编辑器状态
 *
 * 将编辑器状态恢复为空闲，清除错误计数和错误信息。
 *
 * @param editor 编辑器指针
 * @return 成功返回0，失败返回-1
 */
int lv_visual_editor_reset(lvVisualEditor *editor) {
    lv_CHECK_NOT_NULL(editor);
    editor->state = lv_EDITOR_IDLE;
    editor->error_count = 0;
    editor_clear_error(editor);
    return 0;
}

/**
 * @brief 切换活动视图
 *
 * @param editor 编辑器指针
 * @param view   目标视图类型
 * @return 成功返回0，失败返回-1
 */
int lv_visual_editor_switch_view(lvVisualEditor *editor, lvViewType view) {
    lv_CHECK_NOT_NULL(editor);
    lv_CHECK_ARG(view >= lv_VIEW_BLOCK_CANVAS && view < lv_VIEW_COUNT, lv_ERROR_INVALID_PARAM,
                 "invalid view type %d", view);
    editor->active_view = view;
    return 0;
}

/**
 * @brief 获取当前活动视图类型
 *
 * @param editor 编辑器指针（const）
 * @return 当前视图类型，editor为NULL时返回默认值 lv_VIEW_NODE_GRAPH
 */
lvViewType lv_visual_editor_active_view(const lvVisualEditor *editor) {
    return editor ? editor->active_view : lv_VIEW_NODE_GRAPH;
}

/**
 * @brief 内部执行入口：按策略创建调度器并执行块图
 *
 * 全量/增量执行共用同一流程，差异仅参数化：
 *   - 调度策略（lv_SCHED_FULL / lv_SCHED_INCREMENTAL）
 *   - 是否先标记所有块为脏（增量执行）
 * 错误处理、编辑器状态切换与错误信息样板统一在此保留。
 *
 * @param editor   编辑器指针
 * @param strategy 调度策略
 * @param mark_all 是否先标记所有块为脏（增量执行）
 * @return 成功返回0，失败返回-1
 */
static int editor_run(lvVisualEditor *editor, lvSchedStrategy strategy, bool mark_all) {
    lv_CHECK_NOT_NULL(editor);
    if (!editor->block_graph) {
        editor->state = lv_EDITOR_ERROR;
        editor_set_error(editor, "no block graph loaded");
        editor->error_count++;
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "no block graph loaded");
    }

    /* 设置编辑器状态为执行中 */
    editor->state = lv_EDITOR_EXECUTING;

    /* 获取当前块图，创建调度器并执行 */
    lvBlockScheduler *sched = lv_block_scheduler_create(editor->block_graph);
    if (!sched) {
        editor->state = lv_EDITOR_ERROR;
        editor_set_error(editor, "failed to create scheduler");
        editor->error_count++;
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "failed to create scheduler");
    }

    /* 设置执行策略 */
    lv_block_scheduler_set_strategy(sched, strategy);

    /* 增量执行：标记所有块为脏（首次增量执行） */
    if (mark_all)
        lv_block_scheduler_mark_all_dirty(sched);

    /* 运行调度器（增量模式使用调度器内部脏块列表） */
    lvExecResult exec_result = mark_all ? lv_block_scheduler_run_incremental(sched, NULL, 0)
                                        : lv_block_scheduler_run(sched);

    /* 根据执行结果更新编辑器状态 */
    if (exec_result.success) {
        editor->state = lv_EDITOR_IDLE;
        /* 执行成功，清除错误状态 */
        editor->error_count = 0;
        editor_clear_error(editor);
    } else {
        editor->state = lv_EDITOR_ERROR;
        /* [安全] 防止 exec_result.error_msg 过长导致缓冲区问题 */
        editor_set_error(editor, exec_result.error_msg[0] ? exec_result.error_msg : "unknown error");
        editor->error_count++;
    }

    /* 销毁调度器 */
    lv_block_scheduler_destroy(sched);
    if (exec_result.success)
        return 0;
    lv_RETURN_ERROR(lv_ERROR_INTERNAL, mark_all ? "incremental execution failed" : "execution failed");
}

/**
 * @brief 全量执行块图
 *
 * 创建调度器并执行块图中所有块。执行结果会更新编辑器状态和错误信息。
 *
 * @param editor 编辑器指针
 * @return 成功返回0，失败返回-1
 */
int lv_visual_editor_execute(lvVisualEditor *editor) {
    return editor_run(editor, lv_SCHED_FULL, false);
}

/**
 * @brief 增量执行块图
 *
 * 创建增量调度器，仅执行被标记为脏的块。首次增量执行时会标记所有块为脏。
 *
 * @param editor 编辑器指针
 * @return 成功返回0，失败返回-1
 */
int lv_visual_editor_execute_incremental(lvVisualEditor *editor) {
    return editor_run(editor, lv_SCHED_INCREMENTAL, true);
}

/**
 * @brief 获取编辑器当前状态
 *
 * @param editor 编辑器指针（const）
 * @return 当前编辑器状态，editor为NULL时返回 lv_EDITOR_ERROR
 */
lvEditorState lv_visual_editor_state(const lvVisualEditor *editor) {
    return editor ? editor->state : lv_EDITOR_ERROR;
}

/**
 * @brief 获取编辑器最后错误信息
 *
 * @param editor 编辑器指针（const）
 * @return 错误信息字符串，editor为NULL时返回 "NULL editor"
 */
const char *lv_visual_editor_last_error(const lvVisualEditor *editor) {
    return editor ? editor->last_error : "NULL editor";
}
