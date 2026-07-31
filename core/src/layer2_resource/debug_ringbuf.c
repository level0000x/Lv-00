/**
 * @file debug_ringbuf.c
 * @brief ring log buffer
 * @details Split from debug.c
 */

#include "lv/lv_file.h"
#include "lv/lv_platform.h"
#include "lv/lv_thread.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "lv/engine.h"
#include "lv/lv_json.h"

#include "context.h"
#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "stream.h"
#include "stream_context_util.h"
#include "type_system.h"
#include "lv/lv_xmacro.h"
#include "lv/lv_strbuf.h"
#include "debug_internal.h"

/* ============================================================
 * 环形日志缓冲区实现
 *
 * 时间戳通过 lv_get_time_us() 获取（平台无关，定义在 lv_utils.h）。
 * ============================================================ */

/**
 * @brief 创建环形日志缓冲区
 *
 * 分配 lvLogRingBuffer 并通过泛型 lvRingBuf 初始化。
 * capacity 必须 >= 1。
 *
 * @param capacity 缓冲区容量（条目数）
 * @return 新分配的缓冲区，失败返回 NULL
 */
lvLogRingBuffer *lv_log_ring_buffer_create(int capacity) {
    if (capacity < 1) {
        capacity = lv_LOG_RING_BUFFER_DEFAULT_CAPACITY;
    }

    lvLogRingBuffer *rb = (lvLogRingBuffer *) lv_calloc(1, sizeof(lvLogRingBuffer));
    if (!rb) {
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "分配环形缓冲区失败");
    }

    if (!lv_ringbuf_init(&rb->base, sizeof(lvLogEntry), capacity)) {
        lv_free((void **) &rb);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "初始化环形缓冲区失败");
    }

    return rb;
}

/**
 * @brief 销毁环形日志缓冲区
 * @param rb 缓冲区指针（可为 NULL）
 */
void lv_log_ring_buffer_destroy(lvLogRingBuffer *rb) {
    if (!rb) {
        return;
    }
    lv_ringbuf_destroy(&rb->base);
    lv_free((void **) &rb);
}

/**
 * @brief 向环形缓冲区写入一条结构化日志
 *
 * 固定大小的环形缓冲区：当 count == capacity 时，
 * 新条目覆盖最旧的条目。
 *
 * @note 此函数内部加锁（log_lock/log_unlock）以确保线程安全。
 *       如果在已持有 log_lock 的上下文中调用，请使用
 *       lv_log_ring_buffer_write_unlocked() 内部版本。
 */
void lv_log_ring_buffer_write(lvLogRingBuffer *rb, LogLevel level, const char *module_name, const char *function_name,
                              const char *file_name, int line_number, const char *fmt, ...) {
    if (!rb) {
        return;
    }

    /* 在栈上构造 lvLogEntry */
    lvLogEntry entry;
    entry.level = level;
    entry.timestamp_us = lv_get_time_us();
    entry.module_name = module_name;
    entry.function_name = function_name;
    entry.file_name = file_name;
    entry.line_number = line_number;
    entry.context_id = 0; /* 默认全局日志，lv_log_with_context 会覆盖 */

    /* 格式化消息（定长缓冲区，防止 OOM） */
    va_list args;
    va_start(args, fmt);
    vsnprintf(entry.message, sizeof(entry.message), fmt, args);
    va_end(args);

    log_lock();
    lv_ringbuf_write(&rb->base, &entry);
    log_unlock();
}

/**
 * @brief 导出环形缓冲区中的所有日志（按时间顺序）
 *
 * 以插入顺序导出所有条目（最旧的在前，最新的在后）。
 * 返回的数组由调用者负责释放（使用 lv_free）。
 *
 * @param rb        环形缓冲区（非 NULL）
 * @param out_count 输出：实际导出的条目数量
 * @return 日志条目数组（按插入时间排序），count == 0 时返回 NULL
 */
lvLogEntry *lv_log_ring_buffer_export(const lvLogRingBuffer *rb, int *out_count) {
    if (!rb || !out_count) {
        if (out_count)
            *out_count = 0;
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "环形缓冲区或输出参数为空");
    }

    log_lock();

    int cnt = lv_ringbuf_count(&rb->base);
    if (cnt == 0) {
        *out_count = 0;
        log_unlock();
        return NULL;
    }

    lvLogEntry *exported = (lvLogEntry *) lv_calloc((size_t) cnt, sizeof(lvLogEntry));
    if (!exported) {
        *out_count = 0;
        log_unlock();
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "分配导出缓冲区失败");
    }

    for (int i = 0; i < cnt; i++) {
        lv_ringbuf_read(&rb->base, i, &exported[i]);
    }

    *out_count = cnt;
    log_unlock();
    return exported;
}

/**
 * @brief 清空环形缓冲区中的所有日志条目
 * @param rb 环形缓冲区（非 NULL）
 */
void lv_log_ring_buffer_clear(lvLogRingBuffer *rb) {
    if (!rb) {
        return;
    }
    log_lock();
    lv_ringbuf_clear(&rb->base);
    log_unlock();
}

/**
 * @brief 调整环形缓冲区容量
 *
 * 分配新的缓冲区，复制最多 new_capacity 条最新日志。
 * 如果 new_capacity < 当前条目数，最旧的多余条目将被丢弃。
 *
 * @param rb       环形缓冲区（非 NULL）
 * @param capacity 新容量（>= 1）
 * @return true 成功，false 失败（内存不足）
 */
bool lv_log_ring_buffer_resize(lvLogRingBuffer *rb, int capacity) {
    if (!rb || capacity < 1) {
        return false;
    }

    log_lock();
    bool ok = lv_ringbuf_resize(&rb->base, capacity);
    log_unlock();
    return ok;
}
