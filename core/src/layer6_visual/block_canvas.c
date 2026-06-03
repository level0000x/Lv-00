#include "lv00/visual_editor.h"
#include <stdlib.h>

/* Block canvas view - placeholder for Scratch/Blockly-style editor */

typedef struct Lv00BlockCanvasView {
    int view_type;
    void *library;
    void *blocks;
    int block_count;
} Lv00BlockCanvasView;

Lv00BlockCanvasView *lv00_block_canvas_create(void) {
    Lv00BlockCanvasView *canvas = calloc(1, sizeof(Lv00BlockCanvasView));
    if (!canvas) return NULL;
    canvas->view_type = LV00_VIEW_BLOCK_CANVAS;
    return canvas;
}

void lv00_block_canvas_destroy(Lv00BlockCanvasView *canvas) {
    free(canvas);
}
