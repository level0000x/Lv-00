#ifndef lv_VISUAL_EDITOR_H
#define lv_VISUAL_EDITOR_H

#include "lv/func_block.h"
#include "lv/type_system.h"
#include "lv/lv_view.h"
#include "lv/lv_utils.h" /* lv_dirty_set */
#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */

#ifdef __cplusplus
extern "C" {
#endif

/* Layer 6 identifier */
#define lv_LAYER_VISUAL 6

/* lvViewType is now defined in lv/lv_view.h (includes lv_VIEW_COUNT) */

/* Editor state */
typedef enum { lv_EDITOR_IDLE, lv_EDITOR_EDITING, lv_EDITOR_EXECUTING, lv_EDITOR_ERROR } lvEditorState;

/* Forward declarations */
typedef struct lvVisualEditor lvVisualEditor;
typedef struct lvGeometryCanvas lvGeometryCanvas;
typedef struct lvNodeGraphView lvNodeGraphView;
typedef struct lvBlockCanvasView lvBlockCanvasView;
typedef struct lvTextCodeView lvTextCodeView;
typedef struct lvViewSynchronizer lvViewSynchronizer;
typedef struct lvGraphNode lvGraphNode;

/* View synchronizer - keeps all 4 views in sync */
struct lvViewSynchronizer {
    /* Unique sync ID for external API tracking */
    int sync_id;

    int sync_enabled;
    void *source_graph;
    int conflict_count;
    char last_conflict[512];

    /* 脏视图追踪 */
    lv_dirty_set dirty_views; /* 脏视图 ID 集合（去重、插入序、clear 保留容量） */

    /* 待处理的变更记录 */
    struct {
        int source_view_id;
        char change_type[128];
    } *pending_changes;
    int pending_count;
    int pending_capacity;
};

/* Text code view - bidirectional sync with function block graph */
struct lvTextCodeView {
    lvView base;        /* 视图基类（必须为第一个字段） */
    int view_id;        /* Unique view ID for external API tracking */

    char *code_buffer;
    int buffer_size;
    int cursor_pos;
};

/* Visual editor (top-level container) */
struct lvVisualEditor {
    /* Layer 6 identifier */
    int layer_id;

    /* Unique editor ID for external API tracking */
    int editor_id;

    /* Active view */
    lvViewType active_view;

    /* Editor state */
    lvEditorState state;

    /* Sub-views */
    lvGeometryCanvas *geometry_canvas;
    lvNodeGraphView *node_graph;
    lvBlockCanvasView *block_canvas;
    lvTextCodeView *text_code;

    /* View synchronization */
    lvViewSynchronizer *synchronizer;

    /* Function block graph (shared across views) */
    void *block_graph;

    /* Error state */
    char last_error[1024];
    int error_count;
};

/* Lifecycle */
lvVisualEditor *lv_visual_editor_create(void);
lv_PUBLIC_API void lv_visual_editor_destroy(lvVisualEditor *editor);
lv_PUBLIC_API int lv_visual_editor_reset(lvVisualEditor *editor);

/* View management */
lv_PUBLIC_API int lv_visual_editor_switch_view(lvVisualEditor *editor, lvViewType view);
lvViewType lv_visual_editor_active_view(const lvVisualEditor *editor);

/* Execution */
lv_PUBLIC_API int lv_visual_editor_execute(lvVisualEditor *editor);
lv_PUBLIC_API int lv_visual_editor_execute_incremental(lvVisualEditor *editor);

/* State query */
lvEditorState lv_visual_editor_state(const lvVisualEditor *editor);
lv_PUBLIC_API const char *lv_visual_editor_last_error(const lvVisualEditor *editor);

/* ---- View Synchronizer API ---- */
lvViewSynchronizer *lv_view_sync_create(void);
lv_PUBLIC_API void lv_view_sync_destroy(lvViewSynchronizer *sync);
lv_PUBLIC_API int lv_view_sync_enable(lvViewSynchronizer *sync);
lv_PUBLIC_API int lv_view_sync_disable(lvViewSynchronizer *sync);
lv_PUBLIC_API int lv_view_sync_conflicts(const lvViewSynchronizer *sync);
lv_PUBLIC_API int lv_view_sync_propagate(lvViewSynchronizer *sync, int source_view_id, const char *change_type);
lv_PUBLIC_API int lv_view_sync_flush(lvViewSynchronizer *sync);

/* ---- Text Code View API ---- */
lvTextCodeView *lv_text_code_create(void);
lv_PUBLIC_API void lv_text_code_destroy(lvTextCodeView *view);
lv_PUBLIC_API int lv_text_code_set_text(lvTextCodeView *view, const char *text);
lv_PUBLIC_API const char *lv_text_code_get_text(const lvTextCodeView *view);
lv_PUBLIC_API int lv_text_code_insert(lvTextCodeView *view, int pos, const char *text);
lv_PUBLIC_API int lv_text_code_delete(lvTextCodeView *view, int pos, int len);
lv_PUBLIC_API int lv_text_code_render(const lvTextCodeView *view, char *buffer, size_t size);

/* ---- Geometry Canvas API ---- */
lvGeometryCanvas *lv_geometry_canvas_create(void);
lv_PUBLIC_API void lv_geometry_canvas_destroy(lvGeometryCanvas *canvas);
lv_PUBLIC_API int lv_geometry_canvas_add_entity(lvGeometryCanvas *canvas, int type, const char *label, const double *coords, int coord_count);
lv_PUBLIC_API int lv_geometry_canvas_remove_entity(lvGeometryCanvas *canvas, int id);
lv_PUBLIC_API int lv_geometry_canvas_add_constraint(lvGeometryCanvas *canvas, int entity_a_id, int entity_b_id, const char *label);
lv_PUBLIC_API int lv_geometry_canvas_fit_view(lvGeometryCanvas *canvas);
lv_PUBLIC_API char *lv_geometry_canvas_render_svg(lvGeometryCanvas *canvas);

/* ---- Node Graph View API ---- */
lvNodeGraphView *lv_node_graph_create(void);
lv_PUBLIC_API void lv_node_graph_destroy(lvNodeGraphView *graph);
lv_PUBLIC_API int lv_node_graph_add_node(lvNodeGraphView *graph, int id, const char *label, double x, double y, int type);
lv_PUBLIC_API int lv_node_graph_remove_node(lvNodeGraphView *graph, int id);
lv_PUBLIC_API int lv_node_graph_add_connection(lvNodeGraphView *graph, int from_id, int to_id, const char *label);
lv_PUBLIC_API int lv_node_graph_remove_connection(lvNodeGraphView *graph, int conn_id);
lvGraphNode *lv_node_graph_find_node(lvNodeGraphView *graph, int id);
lv_PUBLIC_API int lv_node_graph_layout(lvNodeGraphView *graph);

/* ---- Block Canvas View API ---- */
lvBlockCanvasView *lv_block_canvas_create(void);
lv_PUBLIC_API void lv_block_canvas_destroy(lvBlockCanvasView *canvas);
lv_PUBLIC_API int lv_block_canvas_add_block(lvBlockCanvasView *canvas, const char *label, double x, double y, double width, double height, int type, int input_count, int output_count);
lv_PUBLIC_API int lv_block_canvas_remove_block(lvBlockCanvasView *canvas, int block_id);
lv_PUBLIC_API int lv_block_canvas_connect_blocks(lvBlockCanvasView *canvas, int from_block_id, int from_port_id, int to_block_id, int to_port_id);
lv_PUBLIC_API char *lv_block_canvas_render_svg(lvBlockCanvasView *canvas);

#ifdef __cplusplus
}
#endif

#endif /* lv_VISUAL_EDITOR_H */
