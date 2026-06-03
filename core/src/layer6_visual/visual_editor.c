#include "lv00/visual_editor.h"
#include <stdlib.h>
#include <string.h>

Lv00VisualEditor *lv00_visual_editor_create(void) {
    Lv00VisualEditor *editor = calloc(1, sizeof(Lv00VisualEditor));
    if (!editor) return NULL;
    editor->layer_id = LV00_LAYER_VISUAL;
    editor->active_view = LV00_VIEW_NODE_GRAPH;
    editor->state = LV00_EDITOR_IDLE;
    return editor;
}

void lv00_visual_editor_destroy(Lv00VisualEditor *editor) {
    if (!editor) return;
    /* Sub-views destroyed by their own managers */
    free(editor);
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
    editor->state = LV00_EDITOR_EXECUTING;
    /* TODO: invoke block scheduler */
    editor->state = LV00_EDITOR_IDLE;
    return 0;
}

int lv00_visual_editor_execute_incremental(Lv00VisualEditor *editor) {
    if (!editor) return -1;
    editor->state = LV00_EDITOR_EXECUTING;
    /* TODO: invoke incremental scheduler */
    editor->state = LV00_EDITOR_IDLE;
    return 0;
}

Lv00EditorState lv00_visual_editor_state(const Lv00VisualEditor *editor) {
    return editor ? editor->state : LV00_EDITOR_ERROR;
}

const char *lv00_visual_editor_last_error(const Lv00VisualEditor *editor) {
    return editor ? editor->last_error : "NULL editor";
}
