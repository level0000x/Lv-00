#include "lv/visual_editor.h"
#include "lv/lv_utils.h"
#include <stdlib.h>
#include <string.h>

lvTextCodeView *lv_text_code_create(void) {
    lvTextCodeView *view = lv_calloc(1, sizeof(lvTextCodeView));
    if (!view) return NULL;
    view->view_type = lv_VIEW_TEXT_CODE;
    view->buffer_size = 4096;
    view->code_buffer = lv_calloc(1, view->buffer_size);
    if (!view->code_buffer) {
        lv_free((void **)&view);
        return NULL;
    }
    return view;
}

void lv_text_code_destroy(lvTextCodeView *view) {
    if (!view) return;
    lv_free((void **)&view->code_buffer);
    lv_free((void **)&view);
}

/* 设置文本内容 */
int lv_text_code_set_text(lvTextCodeView *view, const char *text) {
    if (!view || !text) return -1;
    size_t len = strlen(text);
    if (len + 1 > (size_t)view->buffer_size) {
        int new_size = ((int)len + 1 + 4095) / 4096 * 4096;
        char *new_buf = lv_realloc(view->code_buffer, new_size);
        if (!new_buf) return -1;
        view->code_buffer = new_buf;
        view->buffer_size = new_size;
    }
    memcpy(view->code_buffer, text, len + 1);
    view->cursor_pos = (int)len;
    return 0;
}

/* 获取当前文本 */
const char *lv_text_code_get_text(const lvTextCodeView *view) {
    if (!view) return NULL;
    return view->code_buffer;
}

/* 在指定位置插入文本 */
int lv_text_code_insert(lvTextCodeView *view, int pos, const char *text) {
    if (!view || !text) return -1;
    int text_len = (int)strlen(text);
    int cur_len = view->code_buffer ? (int)strlen(view->code_buffer) : 0;
    if (pos < 0) pos = 0;
    if (pos > cur_len) pos = cur_len;
    int new_len = cur_len + text_len;
    if (new_len + 1 > view->buffer_size) {
        int new_size = ((new_len + 1 + 4095) / 4096) * 4096;
        char *new_buf = lv_realloc(view->code_buffer, new_size);
        if (!new_buf) return -1;
        view->code_buffer = new_buf;
        view->buffer_size = new_size;
    }
    /* 移动尾部数据 */
    if (pos < cur_len) {
        memmove(view->code_buffer + pos + text_len,
                view->code_buffer + pos,
                cur_len - pos);
    }
    memcpy(view->code_buffer + pos, text, text_len);
    view->code_buffer[new_len] = '\0';
    view->cursor_pos = pos + text_len;
    return 0;
}

/* 删除指定范围的文本 */
int lv_text_code_delete(lvTextCodeView *view, int pos, int len) {
    if (!view || pos < 0 || len <= 0) return -1;
    int cur_len = view->code_buffer ? (int)strlen(view->code_buffer) : 0;
    if (pos >= cur_len) return -1;
    int actual_len = len;
    if (pos + actual_len > cur_len) actual_len = cur_len - pos;
    /* 移动尾部数据覆盖被删除部分 */
    memmove(view->code_buffer + pos,
            view->code_buffer + pos + actual_len,
            cur_len - pos - actual_len);
    view->code_buffer[cur_len - actual_len] = '\0';
    view->cursor_pos = pos;
    return 0;
}

/* 渲染文本到缓冲区 */
int lv_text_code_render(const lvTextCodeView *view, char *buffer, size_t size) {
    if (!view || !buffer || size == 0) return -1;
    const char *src = view->code_buffer ? view->code_buffer : "";
    size_t len = strlen(src);
    if (len >= size) len = size - 1;
    memcpy(buffer, src, len);
    buffer[len] = '\0';
    return (int)len;
}
