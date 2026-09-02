/**
 * @file lv_ringbuf.c
 * @brief 泛型环形缓冲区实现
 *
 * 从 debug.c 中的 lvLogRingBuffer 私有实现提取。
 * 原始实现（~150 行）被重构为通用的 lvRingBuf + 日志专用薄封装。
 */
#include "lv/lv_ringbuf.h"
#include "lv/lv_utils.h" /* lv_calloc / lv_free：统一分配器（不绕过） */

#include <stdlib.h>
#include <string.h>

bool lv_ringbuf_init(lvRingBuf *rb, size_t elem_size, int capacity) {
    if (!rb || elem_size == 0 || capacity < 1) {
        return false;
    }

    rb->buffer = (uint8_t *) lv_calloc((size_t) capacity, elem_size);
    if (!rb->buffer) {
        return false;
    }

    rb->elem_size = elem_size;
    rb->capacity = capacity;
    rb->head = 0;
    rb->count = 0;
    return true;
}

void lv_ringbuf_destroy(lvRingBuf *rb) {
    if (!rb) {
        return;
    }
    lv_free((void **) &rb->buffer); /* lv_free 置 NULL */
    rb->capacity = 0;
    rb->head = 0;
    rb->count = 0;
}

void lv_ringbuf_write(lvRingBuf *rb, const void *elem) {
    if (!rb || !rb->buffer || rb->capacity < 1 || !elem) {
        return;
    }

    /* 写入当前位置 */
    size_t offset = (size_t) rb->head * rb->elem_size;
    memcpy(rb->buffer + offset, elem, rb->elem_size);

    /* 推进写入位置 */
    rb->head = (rb->head + 1) % rb->capacity;
    if (rb->count < rb->capacity) {
        rb->count++;
    }
}

bool lv_ringbuf_read(const lvRingBuf *rb, int index, void *out) {
    if (!rb || !out || index < 0 || index >= rb->count) {
        return false;
    }

    void *src = lv_ringbuf_get(rb, index);
    if (!src) {
        return false;
    }

    memcpy(out, src, rb->elem_size);
    return true;
}

void *lv_ringbuf_get(const lvRingBuf *rb, int index) {
    if (!rb || !rb->buffer || index < 0 || index >= rb->count) {
        return NULL;
    }

    int actual_idx;
    if (rb->count < rb->capacity) {
        /* 未填满：元素从位置 0 开始连续排列 */
        actual_idx = index;
    } else {
        /* 已填满：最旧的元素在 head 位置 */
        actual_idx = (rb->head + index) % rb->capacity;
    }

    return rb->buffer + (size_t) actual_idx * rb->elem_size;
}

void lv_ringbuf_clear(lvRingBuf *rb) {
    if (!rb) {
        return;
    }
    rb->head = 0;
    rb->count = 0;
    if (rb->buffer && rb->capacity > 0 && rb->elem_size > 0) {
        memset(rb->buffer, 0, (size_t) rb->capacity * rb->elem_size);
    }
}

bool lv_ringbuf_resize(lvRingBuf *rb, int new_capacity) {
    if (!rb || new_capacity < 1) {
        return false;
    }
    if (new_capacity == rb->capacity) {
        return true;
    }

    uint8_t *new_buffer = (uint8_t *) lv_calloc((size_t) new_capacity, rb->elem_size);
    if (!new_buffer) {
        return false;
    }

    /* 计算要保留的条目数量（保留最新的） */
    int keep_count = (rb->count < new_capacity) ? rb->count : new_capacity;
    if (keep_count > 0) {
        /* 确定旧缓冲区中最旧元素的索引 */
        int start;
        if (rb->count < rb->capacity) {
            start = 0;
        } else {
            start = rb->head;
        }

        /* 如果 count > keep_count，跳过最旧的 (count - keep_count) 条 */
        if (rb->count > keep_count) {
            start = (start + (rb->count - keep_count)) % rb->capacity;
        }

        for (int i = 0; i < keep_count; i++) {
            int src_idx = (start + i) % rb->capacity;
            memcpy(new_buffer + (size_t) i * rb->elem_size,
                   rb->buffer + (size_t) src_idx * rb->elem_size,
                   rb->elem_size);
        }
    }

    /* 替换旧的缓冲区 */
    lv_free((void **) &rb->buffer);
    rb->buffer = new_buffer;
    rb->capacity = new_capacity;
    rb->head = keep_count % new_capacity;
    rb->count = keep_count;
    return true;
}
