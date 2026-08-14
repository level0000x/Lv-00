/**
 * @file text_code.c
 * @brief 文本代码视图实现
 *
 * @details 实现文本代码视图，支持文本的创建/销毁、设置、获取、插入、
 *          删除和渲染操作。缓冲区管理采用动态扩容策略，
 *          以 4KB 为对齐单位进行增长，减少频繁重分配。
 *
 * @author Lv-00 Project
 */

#include <stdlib.h>
#include <string.h>

#include "lv/lv_utils.h"
#include "lv/visual_editor.h"
#include "lv/lv_internal.h"

/* 文本缓冲区 4KB 对齐单位（初始大小与扩容步长共用） */
#define lv_TEXT_CODE_BUFFER_ALIGN 4096

/**
 * @brief 以 4KB 为单位对齐扩容文本缓冲区
 *
 * lv_text_code_set_text / lv_text_code_insert 共用（同文件双份收敛）：
 * 所需字节数超过当前容量时按 4KB 对齐扩大，超过 128MB 上限或
 * 分配失败时返回错误码。
 *
 * @param view    文本视图指针
 * @param needed  所需内容字节数（不含末尾 NUL）
 * @param oom_msg 分配失败错误消息（区分调用场景）
 * @return 成功返回 0，失败返回错误码
 */
static int text_code_grow_to_fit(lvTextCodeView *view, size_t needed, const char *oom_msg) {
    /* [安全] 使用 size_t 计算扩展大小，防止整数溢出 */
    size_t new_size = ((needed + 1 + (lv_TEXT_CODE_BUFFER_ALIGN - 1)) / lv_TEXT_CODE_BUFFER_ALIGN) * lv_TEXT_CODE_BUFFER_ALIGN;
    if (new_size > 128 * lv_MB_I)
        lv_RETURN_ERROR(lv_ERROR_BUFFER_TOO_SMALL, "text exceeds max buffer size 128MB");
    char *new_buf = lv_realloc(view->code_buffer, (int) new_size);
    if (!new_buf)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, oom_msg);
    view->code_buffer = new_buf;
    view->buffer_size = (int) new_size;
    return 0;
}

/**
 * @brief 创建文本代码视图
 *
 * 分配并初始化文本代码视图，默认缓冲区大小为 4096 字节。
 *
 * @return 成功返回文本视图指针，失败返回NULL
 */
lvTextCodeView *lv_text_code_create(void) {
    lvTextCodeView *view = lv_calloc(1, sizeof(lvTextCodeView));
    if (!view)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "failed to allocate text code view");
    view->base.type = lv_VIEW_TEXT_CODE;
    view->buffer_size = lv_TEXT_CODE_BUFFER_ALIGN;
    view->code_buffer = lv_calloc(1, view->buffer_size);
    if (!view->code_buffer) {
        lv_free((void **) &view);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "failed to allocate code buffer");
    }
    return view;
}

/**
 * @brief 销毁文本代码视图
 *
 * 释放代码缓冲区和视图结构体。
 *
 * @param view 文本视图指针
 */
void lv_text_code_destroy(lvTextCodeView *view) {
    if (!view)
        return;
    lv_free((void **) &view->code_buffer);
    lv_free((void **) &view);
}

/**
 * @brief 设置文本内容
 *
 * 将视图文本替换为指定字符串。如果文本过长，自动扩展缓冲区。
 *
 * @param view 文本视图指针
 * @param text 要设置的文本字符串
 * @return 成功返回0，失败返回-1
 */
int lv_text_code_set_text(lvTextCodeView *view, const char *text) {
    if (!view || !text)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "NULL view or text");
    size_t len = strlen(text);
    if (len + 1 > (size_t) view->buffer_size) {
        int rc = text_code_grow_to_fit(view, len, "failed to realloc code buffer");
        if (rc != 0)
            return rc;
    }
    memcpy(view->code_buffer, text, len + 1);
    view->cursor_pos = (int) len;
    return 0;
}

/**
 * @brief 获取当前文本
 *
 * @param view 文本视图指针（const）
 * @return 文本内容字符串，view为NULL时返回NULL
 */
const char *lv_text_code_get_text(const lvTextCodeView *view) {
    if (!view)
        return NULL;
    return view->code_buffer;
}

/**
 * @brief 在指定位置插入文本
 *
 * 在文本视图的指定位置插入字符串，自动扩展缓冲区。
 *
 * @param view 文本视图指针
 * @param pos  插入位置（负值或超界时自动修正到边界）
 * @param text 要插入的文本
 * @return 成功返回0，失败返回-1
 */
int lv_text_code_insert(lvTextCodeView *view, int pos, const char *text) {
    if (!view || !text)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "NULL view or text");
    size_t text_len = strlen(text);
    size_t cur_len = view->code_buffer ? strlen(view->code_buffer) : 0;
    if (pos < 0)
        pos = 0;
    if ((size_t) pos > cur_len)
        pos = (int) cur_len;
    size_t new_len = cur_len + text_len;
    /* [安全] 防止缓冲区扩展时整数溢出 */
    if (new_len + 1 > (size_t) view->buffer_size) {
        int rc = text_code_grow_to_fit(view, new_len, "failed to realloc code buffer for insert");
        if (rc != 0)
            return rc;
    }
    /* 移动尾部数据 */
    if ((size_t) pos < cur_len) {
        memmove(view->code_buffer + pos + text_len, view->code_buffer + pos, cur_len - pos);
    }
    memcpy(view->code_buffer + pos, text, text_len);
    view->code_buffer[new_len] = '\0';
    view->cursor_pos = (int) (pos + text_len);
    return 0;
}

/**
 * @brief 删除指定范围的文本
 *
 * 从文本视图中删除从 pos 开始的 len 个字符。
 *
 * @param view 文本视图指针
 * @param pos  起始位置
 * @param len  删除长度
 * @return 成功返回0，失败返回-1
 */
int lv_text_code_delete(lvTextCodeView *view, int pos, int len) {
    if (!view || pos < 0 || len <= 0)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "NULL view or invalid pos/len");
    size_t cur_len = view->code_buffer ? strlen(view->code_buffer) : 0;
    if ((size_t) pos >= cur_len)
        lv_RETURN_ERROR(lv_ERROR_INDEX_OUT_OF_RANGE, "position out of range");
    size_t actual_len = (size_t) len;
    if ((size_t) (pos + actual_len) > cur_len)
        actual_len = cur_len - (size_t) pos;
    /* 移动尾部数据覆盖被删除部分 */
    memmove(view->code_buffer + pos, view->code_buffer + pos + actual_len, cur_len - pos - actual_len);
    view->code_buffer[cur_len - actual_len] = '\0';
    view->cursor_pos = pos;
    return 0;
}

/**
 * @brief 渲染文本到缓冲区
 *
 * 将视图中的文本内容复制到指定的输出缓冲区。
 *
 * @param view   文本视图指针（const）
 * @param buffer 输出缓冲区
 * @param size   输出缓冲区大小
 * @return 成功返回写入的字符数（不含终止符），失败返回-1
 */
int lv_text_code_render(const lvTextCodeView *view, char *buffer, size_t size) {
    if (!view || !buffer || size == 0)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "NULL view, buffer, or zero size");
    const char *src = view->code_buffer ? view->code_buffer : "";
    size_t len = strlen(src);
    if (len >= size)
        len = size - 1;
    lv_strlcpy_n(buffer, size, src, (size_t) len);
    return (int) len;
}
