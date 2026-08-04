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

#include "lv_internal.h" /* lv_RETURN_ERROR / lv_RETURN_ERROR_NULL */
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
    int64_t id = s_upper_state.upper_id++;
    editor->editor_id = (int) id; /* 保留对象内嵌 ID,与槽位 id 保持一致 */
    lv_obj_table_add(s_upper_state.visual_editor_table, &s_upper_state.visual_editor_count,
                     MAX_VISUAL_EDITOR_TABLE, id, editor);
    return id;
}

/** 渲染当前约束图到画布（执行可视化编辑器） */
int64_t visual_editor_render(lvEngine *ctx, int64_t editor_id) {
    (void) ctx;
    if (editor_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "visual_editor_render: invalid editor_id");
    lvObjSlot *slot = lv_obj_table_find(s_upper_state.visual_editor_table, MAX_VISUAL_EDITOR_TABLE, editor_id);
    if (!slot)
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "visual_editor_render: editor not found");
    return lv_visual_editor_execute((lvVisualEditor *) slot->ptr);
}

/** 更新编辑器中的节点位置（更新节点图坐标并重置执行） */
int64_t visual_editor_update(lvEngine *ctx, int64_t editor_id, int64_t node_id, int64_t x, int64_t y) {
    (void) ctx;
    if (editor_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "visual_editor_update: invalid editor_id");
    lvObjSlot *slot = lv_obj_table_find(s_upper_state.visual_editor_table, MAX_VISUAL_EDITOR_TABLE, editor_id);
    if (!slot)
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "visual_editor_update: editor not found");
    lvVisualEditor *editor = (lvVisualEditor *) slot->ptr;
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
    lvObjSlot *slot = lv_obj_table_find(s_upper_state.visual_editor_table, MAX_VISUAL_EDITOR_TABLE, editor_id);
    if (!slot)
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "visual_editor_zoom: editor not found");
    lvVisualEditor *editor = (lvVisualEditor *) slot->ptr;
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
    lvObjSlot *slot = lv_obj_table_find(s_upper_state.visual_editor_table, MAX_VISUAL_EDITOR_TABLE, editor_id);
    if (!slot)
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "visual_editor_destroy: editor not found");
    lv_visual_editor_destroy((lvVisualEditor *) slot->ptr);
    lv_obj_table_remove(s_upper_state.visual_editor_table, &s_upper_state.visual_editor_count,
                        MAX_VISUAL_EDITOR_TABLE, editor_id);
    return 0;
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
    int64_t id = s_upper_state.upper_id++;
    sync->sync_id = (int) id; /* 保留对象内嵌 ID,与槽位 id 保持一致 */
    lv_obj_table_add(s_upper_state.view_sync_table, &s_upper_state.view_sync_count,
                     MAX_VIEW_SYNC_TABLE, id, sync);
    return id;
}

/** 同步两个视图(如文本视图与图形视图) */
int64_t view_synchronizer_sync(lvEngine *ctx, int64_t sync_id, int64_t src_view, int64_t dst_view) {
    (void) ctx;
    (void) dst_view;
    if (sync_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "view_synchronizer_sync: invalid sync_id");
    lvObjSlot *slot = lv_obj_table_find(s_upper_state.view_sync_table, MAX_VIEW_SYNC_TABLE, sync_id);
    if (!slot)
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "view_synchronizer_sync: sync_id not found");
    lv_view_sync_propagate((lvViewSynchronizer *) slot->ptr, (int) src_view, "sync_update");
    lv_view_sync_flush((lvViewSynchronizer *) slot->ptr);
    return 0;
}

/** 销毁视图同步器 */
int64_t view_synchronizer_destroy(lvEngine *ctx, int64_t sync_id) {
    (void) ctx;
    if (sync_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "view_synchronizer_destroy: invalid sync_id");
    lvObjSlot *slot = lv_obj_table_find(s_upper_state.view_sync_table, MAX_VIEW_SYNC_TABLE, sync_id);
    if (!slot)
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "view_synchronizer_destroy: sync_id not found");
    lv_view_sync_destroy((lvViewSynchronizer *) slot->ptr);
    lv_obj_table_remove(s_upper_state.view_sync_table, &s_upper_state.view_sync_count,
                        MAX_VIEW_SYNC_TABLE, sync_id);
    return 0;
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
    int64_t id = s_upper_state.upper_id++;
    view->view_id = (int) id; /* 保留对象内嵌 ID,与槽位 id 保持一致 */
    lv_obj_table_add(s_upper_state.text_code_table, &s_upper_state.text_code_count,
                     MAX_TEXT_CODE_TABLE, id, view);
    return id;
}

/** 设置文本代码视图内容 */
int64_t text_code_set_text(lvEngine *ctx, int64_t view_id, const char *text) {
    (void) ctx;
    if (view_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "text_code_set_text: invalid view_id");
    lvObjSlot *slot = lv_obj_table_find(s_upper_state.text_code_table, MAX_TEXT_CODE_TABLE, view_id);
    if (!slot)
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "text_code_set_text: view_id not found");
    return lv_text_code_set_text((lvTextCodeView *) slot->ptr, text);
}

/** 获取文本代码视图内容并写入缓冲区(统一错误码约定,与 proof_tptp_export 同风格) */
int64_t text_code_get_text(lvEngine *ctx, int64_t view_id, char *buf, int64_t buf_size) {
    (void) ctx;
    if (view_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "text_code_get_text: invalid view_id");
    if (!buf || buf_size <= 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "text_code_get_text: NULL buf or small buf_size");
    lvObjSlot *slot = lv_obj_table_find(s_upper_state.text_code_table, MAX_TEXT_CODE_TABLE, view_id);
    if (!slot)
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "text_code_get_text: view_id not found");
    const char *text = lv_text_code_get_text((lvTextCodeView *) slot->ptr);
    if (!text)
        return 0;
    int n = snprintf(buf, (size_t) buf_size, "%s", text);
    return (int64_t) (n >= 0 ? n : -1);
}