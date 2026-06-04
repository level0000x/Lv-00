#include "lv00/visual_editor.h"
#include <stdlib.h>
#include <string.h>

/* Text code view - bidirectional sync with function block graph */

typedef struct Lv00TextCodeView {
    int view_type;
    char *code_buffer;
    int buffer_size;
    int cursor_pos;
} Lv00TextCodeView;

Lv00TextCodeView *lv00_text_code_create(void) {
    Lv00TextCodeView *view = calloc(1, sizeof(Lv00TextCodeView));
    if (!view) return NULL;
    view->view_type = LV00_VIEW_TEXT_CODE;
    view->buffer_size = 4096;
    view->code_buffer = calloc(1, view->buffer_size);
    return view;
}

void lv00_text_code_destroy(Lv00TextCodeView *view) {
    if (!view) return;
    free(view->code_buffer);
    free(view);
}
