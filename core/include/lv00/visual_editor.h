#ifndef LV00_VISUAL_EDITOR_H
#define LV00_VISUAL_EDITOR_H

#include "lv00/func_block.h"
#include "lv00/type_system.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Layer 6 identifier */
#define LV00_LAYER_VISUAL 6

/* View types */
typedef enum {
    LV00_VIEW_GEOMETRY_CANVAS,
    LV00_VIEW_NODE_GRAPH,
    LV00_VIEW_BLOCK_CANVAS,
    LV00_VIEW_TEXT_CODE
} Lv00ViewType;

/* Editor state */
typedef enum {
    LV00_EDITOR_IDLE,
    LV00_EDITOR_EDITING,
    LV00_EDITOR_EXECUTING,
    LV00_EDITOR_ERROR
} Lv00EditorState;

/* Forward declarations */
typedef struct Lv00VisualEditor Lv00VisualEditor;
typedef struct Lv00GeometryCanvas Lv00GeometryCanvas;
typedef struct Lv00NodeGraphView Lv00NodeGraphView;
typedef struct Lv00BlockCanvasView Lv00BlockCanvasView;
typedef struct Lv00TextCodeView Lv00TextCodeView;
typedef struct Lv00ViewSynchronizer Lv00ViewSynchronizer;

/* Visual editor (top-level container) */
struct Lv00VisualEditor {
    /* Layer 6 identifier */
    int layer_id;

    /* Active view */
    Lv00ViewType active_view;

    /* Editor state */
    Lv00EditorState state;

    /* Sub-views */
    Lv00GeometryCanvas *geometry_canvas;
    Lv00NodeGraphView *node_graph;
    Lv00BlockCanvasView *block_canvas;
    Lv00TextCodeView *text_code;

    /* View synchronization */
    Lv00ViewSynchronizer *synchronizer;

    /* Function block graph (shared across views) */
    void *block_graph;

    /* Error state */
    char last_error[1024];
    int error_count;
};

/* Lifecycle */
Lv00VisualEditor *lv00_visual_editor_create(void);
void lv00_visual_editor_destroy(Lv00VisualEditor *editor);
int lv00_visual_editor_reset(Lv00VisualEditor *editor);

/* View management */
int lv00_visual_editor_switch_view(Lv00VisualEditor *editor, Lv00ViewType view);
Lv00ViewType lv00_visual_editor_active_view(const Lv00VisualEditor *editor);

/* Execution */
int lv00_visual_editor_execute(Lv00VisualEditor *editor);
int lv00_visual_editor_execute_incremental(Lv00VisualEditor *editor);

/* State query */
Lv00EditorState lv00_visual_editor_state(const Lv00VisualEditor *editor);
const char *lv00_visual_editor_last_error(const Lv00VisualEditor *editor);

#ifdef __cplusplus
}
#endif

#endif /* LV00_VISUAL_EDITOR_H */
