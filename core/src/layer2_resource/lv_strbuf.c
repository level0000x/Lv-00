#include "lv/lv_strbuf.h"
#include "lv/lv_utils.h"

#include <stdarg.h>
#include <string.h>

void lv_strbuf_init(lvStrBuf *sb) {
    if (!sb) return;
    sb->data   = sb->stack;
    sb->stack[0] = '\0';
    sb->len    = 0;
    sb->cap    = sizeof(sb->stack);
}

static void lv_strbuf_grow(lvStrBuf *sb, size_t needed) {
    if (needed < sb->cap) return;
    size_t new_cap = sb->cap * 2;
    while (new_cap < needed) new_cap *= 2;
    char *new_data = (char *)lv_malloc(new_cap);
    if (!new_data) return;
    memcpy(new_data, sb->data, sb->len + 1);
    if (sb->data != sb->stack) lv_free((void **)&sb->data);
    sb->data = new_data;
    sb->cap  = new_cap;
}

void lv_strbuf_printf(lvStrBuf *sb, const char *fmt, ...) {
    if (!sb || !fmt) return;
    va_list args;
    va_start(args, fmt);
    int needed = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (needed < 0) return;
    size_t required = sb->len + (size_t)needed + 1;
    lv_strbuf_grow(sb, required);
    va_start(args, fmt);
    vsnprintf(sb->data + sb->len, sb->cap - sb->len, fmt, args);
    va_end(args);
    sb->len += (size_t)needed;
}

void lv_strbuf_reset(lvStrBuf *sb) {
    if (!sb) return;
    sb->len = 0;
    if (sb->data) sb->data[0] = '\0';
}

void lv_strbuf_destroy(lvStrBuf *sb) {
    if (!sb) return;
    if (sb->data && sb->data != sb->stack) {
        lv_free((void **)&sb->data);
    }
    sb->data = sb->stack;
    sb->stack[0] = '\0';
    sb->len = 0;
    sb->cap = sizeof(sb->stack);
}
