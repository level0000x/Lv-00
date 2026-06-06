#include "lv00/visual_editor.h"
#include "lv00/block_scheduler.h"
#include "lv00/lv00_utils.h"
#include <stdlib.h>
#include <string.h>

Lv00VisualEditor *lv00_visual_editor_create(void) {
    Lv00VisualEditor *editor = lv00_calloc(1, sizeof(Lv00VisualEditor));
    if (!editor) return NULL;
    editor->layer_id = LV00_LAYER_VISUAL;
    editor->active_view = LV00_VIEW_NODE_GRAPH;
    editor->state = LV00_EDITOR_IDLE;
    return editor;
}

void lv00_visual_editor_destroy(Lv00VisualEditor *editor) {
    if (!editor) return;
    /* Sub-views destroyed by their own managers */
    lv00_free((void **)&editor);
}

int lv00_visual_editor_reset(Lv00VisualEditor *editor) {
    if (!editor) return -1;
    editor->state = LV00_EDITOR_IDLE;
    editor->error_count = 0;
    memset(editor->last_error, 0, sizeof(editor->last_error));
    return 0;
}

int lv00_visual_editor_switch_view(Lv00VisualEditor *editor, Lv00ViewType view) {
    if (!editor) return -1;
    if (view < LV00_VIEW_GEOMETRY_CANVAS || view > LV00_VIEW_TEXT_CODE) return -1;
    editor->active_view = view;
    return 0;
}

Lv00ViewType lv00_visual_editor_active_view(const Lv00VisualEditor *editor) {
    return editor ? editor->active_view : LV00_VIEW_NODE_GRAPH;
}

int lv00_visual_editor_execute(Lv00VisualEditor *editor) {
    if (!editor) return -1;
    if (!editor->block_graph) {
        editor->state = LV00_EDITOR_ERROR;
        strncpy(editor->last_error, "no block graph loaded", sizeof(editor->last_error));
        editor->error_count++;
        return -1;
    }

    /* 设置编辑器状态为执行中 */
    editor->state = LV00_EDITOR_EXECUTING;

    /* 获取当前块图，创建调度器并执行 */
    Lv00BlockScheduler *sched = lv00_block_scheduler_create(editor->block_graph);
    if (!sched) {
        editor->state = LV00_EDITOR_ERROR;
        strncpy(editor->last_error, "failed to create scheduler", sizeof(editor->last_error));
        editor->error_count++;
        return -1;
    }

    /* 设置全量执行策略 */
    lv00_block_scheduler_set_strategy(sched, LV00_SCHED_FULL);

    /* 运行调度器 */
    Lv00ExecResult exec_result = lv00_block_scheduler_run(sched);

    /* 根据执行结果更新编辑器状态 */
    if (exec_result.success) {
        editor->state = LV00_EDITOR_IDLE;
        /* 执行成功，清除错误状态 */
        editor->error_count = 0;
        memset(editor->last_error, 0, sizeof(editor->last_error));
    } else {
        editor->state = LV00_EDITOR_ERROR;
        strncpy(editor->last_error, exec_result.error_msg, sizeof(editor->last_error));
        editor->error_count++;
    }

    /* 销毁调度器 */
    lv00_block_scheduler_destroy(sched);

    return exec_result.success ? 0 : -1;
}

int lv00_visual_editor_execute_incremental(Lv00VisualEditor *editor) {
    if (!editor) return -1;
    if (!editor->block_graph) {
        editor->state = LV00_EDITOR_ERROR;
        strncpy(editor->last_error, "no block graph loaded", sizeof(editor->last_error));
        editor->error_count++;
        return -1;
    }

    /* 设置编辑器状态为执行中 */
    editor->state = LV00_EDITOR_EXECUTING;

    /* 创建增量调度器 */
    Lv00BlockScheduler *sched = lv00_block_scheduler_create(editor->block_graph);
    if (!sched) {
        editor->state = LV00_EDITOR_ERROR;
        strncpy(editor->last_error, "failed to create scheduler", sizeof(editor->last_error));
        editor->error_count++;
        return -1;
    }

    /* 设置增量执行策略 */
    lv00_block_scheduler_set_strategy(sched, LV00_SCHED_INCREMENTAL);

    /* 获取脏块列表（标记所有块为脏，首次增量执行） */
    lv00_block_scheduler_mark_all_dirty(sched);

    /* 获取脏块并执行增量调度 */
    /* 传入 NULL 表示使用调度器内部脏块列表 */
    Lv00ExecResult exec_result = lv00_block_scheduler_run_incremental(sched, NULL, 0);

    /* 根据执行结果更新编辑器状态 */
    if (exec_result.success) {
        editor->state = LV00_EDITOR_IDLE;
        editor->error_count = 0;
        memset(editor->last_error, 0, sizeof(editor->last_error));
    } else {
        editor->state = LV00_EDITOR_ERROR;
        strncpy(editor->last_error, exec_result.error_msg, sizeof(editor->last_error));
        editor->error_count++;
    }

    /* 销毁调度器 */
    lv00_block_scheduler_destroy(sched);

    return exec_result.success ? 0 : -1;
}

Lv00EditorState lv00_visual_editor_state(const Lv00VisualEditor *editor) {
    return editor ? editor->state : LV00_EDITOR_ERROR;
}

const char *lv00_visual_editor_last_error(const Lv00VisualEditor *editor) {
    return editor ? editor->last_error : "NULL editor";
}
