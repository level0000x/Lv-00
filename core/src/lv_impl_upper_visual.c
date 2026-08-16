/*
 * @file lv_impl_upper_visual.c
 * @brief Lv-00 upper unified impl - L6 visual layer
 * @details Split from lv_impl_upper.c
 */

#include <gmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/atp_backend.h"
#include "lv/conflict_detector.h"
#include "lv/engine.h"
#include "lv/func_block.h"
#include "lv/func_block_preset.h"
#include "lv/func_block_registry.h"
#include "lv/geom_evol.h"
#include "lv/interop.h"
#include "lv/lv_json.h"
#include "lv/lv_utils.h"
#include "lv/meta_verify.h"
#include "lv/orchestrator.h"
#include "lv/preset_algebraic.h"
#include "lv/preset_basic_geometry.h"
#include "lv/preset_measurements.h"
#include "lv/preset_polygons.h"
#include "lv/preset_transformations.h"
#include "lv/visual_editor.h"

#include "lv/lv_internal.h" /* lv_RETURN_ERROR / lv_RETURN_ERROR_NULL */
#include "lv/lv_strbuf.h"
#include "lv_impl_upper_internal.h"

/* ============================================================
 * 第8部分:L6 可视化层(visual_editor 5 + view_synchronizer 3 + text_code 3)
 * ============================================================ */

/* ---- visual_editor: 可视化编辑器(5函数)---- */

/** 创建可视化编辑器实例 */
int64_t visual_editor_create(lvEngine *ctx) {
    (void) ctx;
    if (s_upper_state.visual_editor_count >= MAX_VISUAL_EDITOR_TABLE)
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "visual_editor_create: editor table full");
    lvVisualEditor *editor = lv_visual_editor_create();
    if (!editor)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "visual_editor_create: lv_visual_editor_create failed");
    int slot = 0;
    for (; slot < MAX_VISUAL_EDITOR_TABLE; slot++) {
        if (!s_upper_state.visual_editor_table[slot])
            break;
    }
    editor->editor_id = (int) s_upper_state.upper_id;
    s_upper_state.visual_editor_table[slot] = editor;
    s_upper_state.visual_editor_count++;
    return s_upper_state.upper_id++;
}

/** 渲染当前约束图到画布（执行可视化编辑器） */
int64_t visual_editor_render(lvEngine *ctx, int64_t editor_id) {
    (void) ctx;
    if (editor_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "visual_editor_render: invalid editor_id");
    lvVisualEditor *editor = NULL;
    for (int i = 0; i < MAX_VISUAL_EDITOR_TABLE; i++) {
        if (s_upper_state.visual_editor_table[i] && s_upper_state.visual_editor_table[i]->editor_id == (int) editor_id) {
            editor = s_upper_state.visual_editor_table[i];
            break;
        }
    }
    if (!editor)
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "visual_editor_render: editor not found");
    return lv_visual_editor_execute(editor);
}

/** 更新编辑器中的节点位置（更新节点图坐标并重置执行） */
int64_t visual_editor_update(lvEngine *ctx, int64_t editor_id, int64_t node_id, int64_t x, int64_t y) {
    (void) ctx;
    if (editor_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "visual_editor_update: invalid editor_id");
    lvVisualEditor *editor = NULL;
    for (int i = 0; i < MAX_VISUAL_EDITOR_TABLE; i++) {
        if (s_upper_state.visual_editor_table[i] && s_upper_state.visual_editor_table[i]->editor_id == (int) editor_id) {
            editor = s_upper_state.visual_editor_table[i];
            break;
        }
    }
    if (!editor)
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "visual_editor_update: editor not found");
    /* 更新节点在节点图中的位置坐标 */
    if (editor->node_graph) {
        lv_node_graph_add_node(editor->node_graph, (int) node_id, NULL, (double) x, (double) y, 0);
    }
    return lv_visual_editor_reset(editor);
}

/** 缩放画布（通过 zoom_level 切换视图类型或适配画布） */
int64_t visual_editor_zoom(lvEngine *ctx, int64_t editor_id, int64_t zoom_level) {
    (void) ctx;
    if (editor_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "visual_editor_zoom: invalid editor_id");
    lvVisualEditor *editor = NULL;
    for (int i = 0; i < MAX_VISUAL_EDITOR_TABLE; i++) {
        if (s_upper_state.visual_editor_table[i] && s_upper_state.visual_editor_table[i]->editor_id == (int) editor_id) {
            editor = s_upper_state.visual_editor_table[i];
            break;
        }
    }
    if (!editor)
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "visual_editor_zoom: editor not found");
    /* zoom_level 0-3 映射到四种视图类型 */
    if (zoom_level >= 0 && zoom_level <= 3) {
        lv_visual_editor_switch_view(editor, (lvViewType)(int) zoom_level);
    }
    /* zoom_level > 3 则为适配画布操作 */
    if (zoom_level > 3 && editor->geometry_canvas) {
        lv_geometry_canvas_fit_view(editor->geometry_canvas);
    }
    return lv_visual_editor_execute_incremental(editor);
}

/** 销毁可视化编辑器 */
int64_t visual_editor_destroy(lvEngine *ctx, int64_t editor_id) {
    (void) ctx;
    if (editor_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "visual_editor_destroy: invalid editor_id");
    for (int i = 0; i < MAX_VISUAL_EDITOR_TABLE; i++) {
        lvVisualEditor *editor = s_upper_state.visual_editor_table[i];
        if (editor && editor->editor_id == (int) editor_id) {
            lv_visual_editor_destroy(editor);
            s_upper_state.visual_editor_table[i] = NULL;
            s_upper_state.visual_editor_count--;
            return 0;
        }
    }
    lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "visual_editor_destroy: editor not found");
}

/* ---- view_synchronizer: 视图同步器(3函数)---- */

/** 创建视图同步器 */
int64_t view_synchronizer_create(lvEngine *ctx) {
    (void) ctx;
    if (s_upper_state.view_sync_count >= MAX_VIEW_SYNC_TABLE)
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "view_synchronizer_create: sync table full");
    lvViewSynchronizer *sync = lv_view_sync_create();
    if (!sync)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "view_synchronizer_create: lv_view_sync_create failed");
    int slot = 0;
    for (; slot < MAX_VIEW_SYNC_TABLE; slot++) {
        if (!s_upper_state.view_sync_table[slot])
            break;
    }
    sync->sync_id = (int) s_upper_state.upper_id;
    s_upper_state.view_sync_table[slot] = sync;
    s_upper_state.view_sync_count++;
    return s_upper_state.upper_id++;
}

/** 同步两个视图(如文本视图与图形视图) */
int64_t view_synchronizer_sync(lvEngine *ctx, int64_t sync_id, int64_t src_view, int64_t dst_view) {
    (void) ctx;
    if (sync_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "view_synchronizer_sync: invalid sync_id");
    for (int i = 0; i < MAX_VIEW_SYNC_TABLE; i++) {
        if (s_upper_state.view_sync_table[i] && s_upper_state.view_sync_table[i]->sync_id == (int) sync_id) {
            lv_view_sync_propagate(s_upper_state.view_sync_table[i], (int) src_view, "sync_update");
            lv_view_sync_flush(s_upper_state.view_sync_table[i]);
            return 0;
        }
    }
    lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "view_synchronizer_sync: sync_id not found");
}

/** 销毁视图同步器 */
int64_t view_synchronizer_destroy(lvEngine *ctx, int64_t sync_id) {
    (void) ctx;
    if (sync_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "view_synchronizer_destroy: invalid sync_id");
    for (int i = 0; i < MAX_VIEW_SYNC_TABLE; i++) {
        lvViewSynchronizer *sync = s_upper_state.view_sync_table[i];
        if (sync && sync->sync_id == (int) sync_id) {
            lv_view_sync_destroy(sync);
            s_upper_state.view_sync_table[i] = NULL;
            s_upper_state.view_sync_count--;
            return 0;
        }
    }
    lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "view_synchronizer_destroy: sync_id not found");
}

/* ---- text_code: 文本代码视图(3函数)---- */

/** 创建文本代码视图 */
int64_t text_code_create(lvEngine *ctx) {
    (void) ctx;
    if (s_upper_state.text_code_count >= MAX_TEXT_CODE_TABLE)
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "text_code_create: text code table full");
    lvTextCodeView *view = lv_text_code_create();
    if (!view)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "text_code_create: lv_text_code_create failed");
    int slot = 0;
    for (; slot < MAX_TEXT_CODE_TABLE; slot++) {
        if (!s_upper_state.text_code_table[slot])
            break;
    }
    view->view_id = (int) s_upper_state.upper_id;
    s_upper_state.text_code_table[slot] = view;
    s_upper_state.text_code_count++;
    return s_upper_state.upper_id++;
}

/** 销毁文本代码视图 */
int64_t text_code_destroy(lvEngine *ctx, int64_t view_id) {
    (void) ctx;
    if (view_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "text_code_destroy: invalid view_id");
    for (int i = 0; i < MAX_TEXT_CODE_TABLE; i++) {
        lvTextCodeView *view = s_upper_state.text_code_table[i];
        if (view && view->view_id == (int) view_id) {
            lv_text_code_destroy(view);
            s_upper_state.text_code_table[i] = NULL;
            s_upper_state.text_code_count--;
            return 0;
        }
    }
    lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "text_code_destroy: view_id not found");
}

/** 设置文本代码视图内容 */
int64_t text_code_set_text(lvEngine *ctx, int64_t view_id, const char *text) {
    (void) ctx;
    if (view_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "text_code_set_text: invalid view_id");
    for (int i = 0; i < MAX_TEXT_CODE_TABLE; i++) {
        if (s_upper_state.text_code_table[i] && s_upper_state.text_code_table[i]->view_id == (int) view_id) {
            return lv_text_code_set_text(s_upper_state.text_code_table[i], text);
        }
    }
    lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "text_code_set_text: view_id not found");
}

/** 获取文本代码视图内容 */
const char *text_code_get_text(lvEngine *ctx, int64_t view_id) {
    (void) ctx;
    if (view_id < 0)
        return "";
    for (int i = 0; i < MAX_TEXT_CODE_TABLE; i++) {
        if (s_upper_state.text_code_table[i] && s_upper_state.text_code_table[i]->view_id == (int) view_id) {
            return lv_text_code_get_text(s_upper_state.text_code_table[i]);
        }
    }
    return "";
}
