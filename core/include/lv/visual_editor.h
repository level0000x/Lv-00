#ifndef lv_VISUAL_EDITOR_H
#define lv_VISUAL_EDITOR_H

#include "lv/func_block.h"
#include "lv/type_system.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Layer 6 identifier */
#define lv_LAYER_VISUAL 6

/* View types */
typedef enum { lv_VIEW_GEOMETRY_CANVAS, lv_VIEW_NODE_GRAPH, lv_VIEW_BLOCK_CANVAS, lv_VIEW_TEXT_CODE } lvViewType;

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
    int *dirty_views;
    int dirty_count;
    int dirty_capacity;

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
    /* Unique view ID for external API tracking */
    int view_id;

    int view_type;
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
void lv_visual_editor_destroy(lvVisualEditor *editor);
int lv_visual_editor_reset(lvVisualEditor *editor);

/* View management */
int lv_visual_editor_switch_view(lvVisualEditor *editor, lvViewType view);
lvViewType lv_visual_editor_active_view(const lvVisualEditor *editor);

/* Execution */
int lv_visual_editor_execute(lvVisualEditor *editor);
int lv_visual_editor_execute_incremental(lvVisualEditor *editor);

/* State query */
lvEditorState lv_visual_editor_state(const lvVisualEditor *editor);
const char *lv_visual_editor_last_error(const lvVisualEditor *editor);

/* ---- View Synchronizer API ---- */
lvViewSynchronizer *lv_view_sync_create(void);
void lv_view_sync_destroy(lvViewSynchronizer *sync);
int lv_view_sync_enable(lvViewSynchronizer *sync);
int lv_view_sync_disable(lvViewSynchronizer *sync);
int lv_view_sync_conflicts(const lvViewSynchronizer *sync);
int lv_view_sync_propagate(lvViewSynchronizer *sync, int source_view_id, const char *change_type);
int lv_view_sync_flush(lvViewSynchronizer *sync);

/* ---- Text Code View API ---- */
lvTextCodeView *lv_text_code_create(void);
void lv_text_code_destroy(lvTextCodeView *view);
int lv_text_code_set_text(lvTextCodeView *view, const char *text);
const char *lv_text_code_get_text(const lvTextCodeView *view);
int lv_text_code_insert(lvTextCodeView *view, int pos, const char *text);
int lv_text_code_delete(lvTextCodeView *view, int pos, int len);
int lv_text_code_render(const lvTextCodeView *view, char *buffer, size_t size);

/* ---- Node Graph View API ---- */
lvNodeGraphView *lv_node_graph_create(void);
void lv_node_graph_destroy(lvNodeGraphView *graph);
int lv_node_graph_add_node(lvNodeGraphView *graph, int id, const char *label, double x, double y, int type);
int lv_node_graph_remove_node(lvNodeGraphView *graph, int id);
int lv_node_graph_add_connection(lvNodeGraphView *graph, int from_id, int to_id, const char *label);
int lv_node_graph_remove_connection(lvNodeGraphView *graph, int conn_id);
lvGraphNode *lv_node_graph_find_node(lvNodeGraphView *graph, int id);
int lv_node_graph_layout(lvNodeGraphView *graph);

#ifdef __cplusplus
}
#endif

#endif /* lv_VISUAL_EDITOR_H */
