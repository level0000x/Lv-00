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
    /* 防倍增溢出：容量超过可表示上限的一半时不再扩容（返回保持原状，
     * 上层 vsnprintf 会按剩余容量截断），避免 new_cap *= 2 回绕成小值导致越界写 */
    if (sb->cap > SIZE_MAX / 2)
        return;
    size_t new_cap = sb->cap ? sb->cap * 2 : lv_STRBUF_SSO_SIZE;
    while (new_cap < needed) {
        if (new_cap > SIZE_MAX / 2)
            return; /* 已无法继续倍增（再倍将回绕），放弃扩容 */
        new_cap *= 2;
    }
    char *new_data = (char *)lv_malloc(new_cap);
    if (!new_data) return;
    if (sb->len > 0 && sb->data) {
        memcpy(new_data, sb->data, sb->len + 1);
    } else {
        new_data[0] = '\0';
    }
    if (sb->data && sb->data != sb->stack) lv_free((void **)&sb->data);
    sb->data = new_data;
    sb->cap  = new_cap;
}

void lv_strbuf_vprintf(lvStrBuf *sb, const char *fmt, va_list args) {
    if (!sb || !fmt) return;
    va_list probe;
    va_copy(probe, args);
    int needed = vsnprintf(NULL, 0, fmt, probe);
    va_end(probe);
    if (needed < 0) return;
    size_t required = sb->len + (size_t)needed + 1;
    lv_strbuf_grow(sb, required);
    vsnprintf(sb->data + sb->len, sb->cap - sb->len, fmt, args);
    sb->len += (size_t)needed;
}

void lv_strbuf_printf(lvStrBuf *sb, const char *fmt, ...) {
    if (!sb || !fmt) return;
    va_list args;
    va_start(args, fmt);
    lv_strbuf_vprintf(sb, fmt, args);
    va_end(args);
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

char *lv_strbuf_to_string(lvStrBuf *sb) {
    if (!sb) return NULL;
    char *result = (char *)lv_malloc(sb->len + 1);
    if (result) {
        if (sb->len > 0) {
            memcpy(result, sb->data, sb->len);
        }
        result[sb->len] = '\0';
    }
    lv_strbuf_destroy(sb);
    return result;
}

void lv_strbuf_append_n(lvStrBuf *sb, char ch, size_t count) {
    if (!sb || count == 0) return;
    for (size_t i = 0; i < count; i++) {
        lv_strbuf_printf(sb, "%c", ch);
    }
}
