#include "lv/visual_editor.h"
#include "lv/block_scheduler.h"
#include "lv/lv_utils.h"
#include <stdlib.h>
#include <string.h>

lvVisualEditor *lv_visual_editor_create(void) {
    lvVisualEditor *editor = lv_calloc(1, sizeof(lvVisualEditor));
    if (!editor) return NULL;
    editor->layer_id = lv_LAYER_VISUAL;
    editor->active_view = lv_VIEW_NODE_GRAPH;
    editor->state = lv_EDITOR_IDLE;
    return editor;
}

void lv_visual_editor_destroy(lvVisualEditor *editor) {
    if (!editor) return;
    /* Sub-views destroyed by their own managers */
    lv_free((void **)&editor);
}

int lv_visual_editor_reset(lvVisualEditor *editor) {
    if (!editor) return -1;
    editor->state = lv_EDITOR_IDLE;
    editor->error_count = 0;
    memset(editor->last_error, 0, sizeof(editor->last_error));
    return 0;
}

int lv_visual_editor_switch_view(lvVisualEditor *editor, lvViewType view) {
    if (!editor) return -1;
    if (view < lv_VIEW_GEOMETRY_CANVAS || view > lv_VIEW_TEXT_CODE) return -1;
    editor->active_view = view;
    return 0;
}

lvViewType lv_visual_editor_active_view(const lvVisualEditor *editor) {
    return editor ? editor->active_view : lv_VIEW_NODE_GRAPH;
}

int lv_visual_editor_execute(lvVisualEditor *editor) {
    if (!editor) return -1;
    if (!editor->block_graph) {
        editor->state = lv_EDITOR_ERROR;
        strncpy(editor->last_error, "no block graph loaded", sizeof(editor->last_error));
        editor->error_count++;
        return -1;
    }

    /* 设置编辑器状态为执行中 */
    editor->state = lv_EDITOR_EXECUTING;

    /* 获取当前块图，创建调度器并执行 */
    lvBlockScheduler *sched = lv_block_scheduler_create(editor->block_graph);
    if (!sched) {
        editor->state = lv_EDITOR_ERROR;
        strncpy(editor->last_error, "failed to create scheduler", sizeof(editor->last_error));
        editor->error_count++;
        return -1;
    }

    /* 设置全量执行策略 */
    lv_block_scheduler_set_strategy(sched, lv_SCHED_FULL);

    /* 运行调度器 */
    lvExecResult exec_result = lv_block_scheduler_run(sched);

    /* 根据执行结果更新编辑器状态 */
    if (exec_result.success) {
        editor->state = lv_EDITOR_IDLE;
        /* 执行成功，清除错误状态 */
        editor->error_count = 0;
        memset(editor->last_error, 0, sizeof(editor->last_error));
    } else {
        editor->state = lv_EDITOR_ERROR;
        strncpy(editor->last_error, exec_result.error_msg, sizeof(editor->last_error));
        editor->error_count++;
    }

    /* 销毁调度器 */
    lv_block_scheduler_destroy(sched);

    return exec_result.success ? 0 : -1;
}

int lv_visual_editor_execute_incremental(lvVisualEditor *editor) {
    if (!editor) return -1;
    if (!editor->block_graph) {
        editor->state = lv_EDITOR_ERROR;
        strncpy(editor->last_error, "no block graph loaded", sizeof(editor->last_error));
        editor->error_count++;
        return -1;
    }

    /* 设置编辑器状态为执行中 */
    editor->state = lv_EDITOR_EXECUTING;

    /* 创建增量调度器 */
    lvBlockScheduler *sched = lv_block_scheduler_create(editor->block_graph);
    if (!sched) {
        editor->state = lv_EDITOR_ERROR;
        strncpy(editor->last_error, "failed to create scheduler", sizeof(editor->last_error));
        editor->error_count++;
        return -1;
    }

    /* 设置增量执行策略 */
    lv_block_scheduler_set_strategy(sched, lv_SCHED_INCREMENTAL);

    /* 获取脏块列表（标记所有块为脏，首次增量执行） */
    lv_block_scheduler_mark_all_dirty(sched);

    /* 获取脏块并执行增量调度 */
    /* 传入 NULL 表示使用调度器内部脏块列表 */
    lvExecResult exec_result = lv_block_scheduler_run_incremental(sched, NULL, 0);

    /* 根据执行结果更新编辑器状态 */
    if (exec_result.success) {
        editor->state = lv_EDITOR_IDLE;
        editor->error_count = 0;
        memset(editor->last_error, 0, sizeof(editor->last_error));
    } else {
        editor->state = lv_EDITOR_ERROR;
        strncpy(editor->last_error, exec_result.error_msg, sizeof(editor->last_error));
        editor->error_count++;
    }

    /* 销毁调度器 */
    lv_block_scheduler_destroy(sched);

    return exec_result.success ? 0 : -1;
}

lvEditorState lv_visual_editor_state(const lvVisualEditor *editor) {
    return editor ? editor->state : lv_EDITOR_ERROR;
}

const char *lv_visual_editor_last_error(const lvVisualEditor *editor) {
    return editor ? editor->last_error : "NULL editor";
}
