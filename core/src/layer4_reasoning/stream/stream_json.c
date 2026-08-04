/**
 * @file stream_json.c
 * @brief 流式输出系统 —— 缓冲区管理 API 与 JSON 序列化
 */

#include "stream_internal.h"


/* ==================== 缓冲区管理 API（已有） ==================== */

/**
 * @brief 清空缓冲区（不发射事件）
 *
 * @param ctx 流式上下文
 * @return 被清除的事件数
 */
int stream_clear_buffer(StreamContext *ctx) {
    if (!ctx)
        return 0;
    int cleared = ctx->buffer_count;
    ctx->buffer_count = 0;
    ctx->buffer_head = 0;
    return cleared;
}

/**
 * @brief 获取缓冲区中当前事件数
 *
 * @param ctx 流式上下文
 * @return 缓冲区事件数
 */
int stream_buffer_size(const StreamContext *ctx) {
    if (!ctx)
        return 0;
    return ctx->buffer_count;
}

/* ==================== JSON 序列化 API ==================== */

/** @brief JSON 字符转义查找表（按 ASCII 下标，NULL 表示无需转义） */
static const char *const s_json_escape_table[256] = {
    ['"'] = "\\\"",
    ['\\'] = "\\\\",
    ['\n'] = "\\n",
    ['\r'] = "\\r",
    ['\t'] = "\\t",
};

/**
 * @brief JSON 字符串转义辅助函数
 *
 * 将需要转义的字符（双引号、反斜杠、换行、回车、制表符）写入输出缓冲区。
 * 其他字符直接写入。
 *
 * @param dest     输出缓冲区
 * @param src      源字符串
 * @param dest_size 输出缓冲区剩余大小
 * @return 写入的字符数（不含终止符）
 */
static int stream_json_escape(char *dest, const char *src, size_t dest_size) {
    if (!src || dest_size == 0)
        return 0;

    size_t written = 0;
    while (*src && written + 1 < dest_size) {
        const char *esc = s_json_escape_table[(unsigned char) *src];
        if (esc) {
            size_t esc_len = strlen(esc);
            if (written + esc_len >= dest_size)
                goto done;
            memcpy(dest + written, esc, esc_len);
            written += esc_len;
        } else {
            dest[written++] = *src;
        }
        src++;
    }
done:
    if (written < dest_size)
        dest[written] = '\0';
    return (int) written;
}

/**
 * @brief 向缓冲区追加格式化字符串（安全版本）
 *
 * 类似 snprintf，但返回追加后的总偏移量，并防止缓冲区溢出。
 *
 * @param buf      输出缓冲区
 * @param size     缓冲区总大小
 * @param offset   当前写入偏移量
 * @param fmt      printf 格式字符串
 * @param ...      可变参数
 * @return 新的偏移量（可能超过 size，表示截断）
 */
static int stream_buf_append(char *buf, size_t size, int offset, const char *fmt, ...) {
    if (offset < 0)
        return offset;

    va_list args;
    va_start(args, fmt);
    int written =
        vsnprintf(buf ? buf + offset : NULL, (buf && (size_t) offset < size) ? size - (size_t) offset : 0, fmt, args);
    va_end(args);

    /* vsnprintf 返回期望写入的字符数（不含终止符），可能超过剩余空间 */
    if (written < 0)
        written = 0;
    return offset + written;
}

/**
 * 将流式事件序列化为 JSON 字符串。
 *
 * 手工拼接 JSON，不依赖外部 JSON 库。输出包含以下字段：
 * type, type_name, color, timestamp_ms, step, total_steps,
 * node_id, constraint_id, rule_id, var_id, description, progress, numeric_value
 *
 * @param event   流式事件
 * @param buffer  输出缓冲区
 * @param size    缓冲区大小
 * @return 写入的字节数（不含终止符），缓冲区不足时返回所需大小
 */
int stream_event_to_json(const StreamEvent *event, char *buffer, size_t size) {
    if (!event) {
        if (buffer && size > 0)
            buffer[0] = '\0';
        return 0;
    }

    const char *type_id = stream_event_type_id(event->type);
    const char *type_name = stream_event_type_name(event->type);
    const char *color = stream_event_color(event->type);

    /* Adaptive buffer for escaped description field.
     *
     * Fast path: use a 2048-byte stack buffer for typical descriptions
     * (< 200 bytes raw, < 400 bytes escaped). This covers all built-in
     * event descriptions without any heap allocation.
     *
     * Slow path: if the raw description exceeds 1024 bytes (which after
     * JSON escaping could approach or exceed the 2048-byte stack buffer),
     * allocate a heap buffer of 2 * strlen(description) + 1 bytes to
     * guarantee sufficient space for worst-case escaping (every character
     * becomes 2 bytes). The heap buffer is freed before the function
     * returns.
     *
     * desc_buf / desc_buf_size point to whichever buffer is active,
     * and desc_heap tracks whether free() is needed. */
    char desc_stack[2048];
    char *desc_buf = desc_stack;
    size_t desc_buf_size = sizeof(desc_stack);
    bool desc_heap = false;

    desc_buf[0] = '\0';
    if (event->description) {
        size_t raw_len = strlen(event->description);
        if (raw_len > 1024) {
            /* Slow path: allocate heap buffer sized for worst-case escaping */
            size_t heap_size = raw_len * 2 + 1;
            char *heap_buf = (char *) lv_malloc(heap_size);
            if (heap_buf) {
                desc_buf = heap_buf;
                desc_buf_size = heap_size;
                desc_heap = true;
            }
            /* If malloc fails, fall through with the stack buffer
             * (truncation is safe — stream_json_escape handles it) */
        }
        stream_json_escape(desc_buf, event->description, desc_buf_size);
    }

    /* 先用 NULL buffer 计算所需大小 */
    int needed = 0;
    needed = stream_buf_append(NULL, 0, needed, "{\n");
    needed = stream_buf_append(NULL, 0, needed, "  \"type\": \"%s\",\n", type_id);
    needed = stream_buf_append(NULL, 0, needed, "  \"type_name\": \"%s\",\n", type_name);
    needed = stream_buf_append(NULL, 0, needed, "  \"color\": \"%s\",\n", color);
    needed = stream_buf_append(NULL, 0, needed, "  \"timestamp_ms\": %ld,\n", event->timestamp_ms);
    needed = stream_buf_append(NULL, 0, needed, "  \"step\": %d,\n", event->step_number);
    needed = stream_buf_append(NULL, 0, needed, "  \"total_steps\": %d,\n", event->total_steps);
    needed = stream_buf_append(NULL, 0, needed, "  \"node_id\": %d,\n", event->node_id);
    needed = stream_buf_append(NULL, 0, needed, "  \"constraint_id\": %d,\n", event->constraint_id);
    needed = stream_buf_append(NULL, 0, needed, "  \"rule_id\": %d,\n", event->rule_id);
    needed = stream_buf_append(NULL, 0, needed, "  \"var_id\": %d,\n", event->var_id);
    if (event->description) {
        needed = stream_buf_append(NULL, 0, needed, "  \"description\": \"%s\",\n", desc_buf);
    } else {
        needed = stream_buf_append(NULL, 0, needed, "  \"description\": null,\n");
    }
    needed = stream_buf_append(NULL, 0, needed, "  \"progress\": %.6g,\n", event->progress);
    needed = stream_buf_append(NULL, 0, needed, "  \"numeric_value\": %.6g\n", event->numeric_value);
    needed = stream_buf_append(NULL, 0, needed, "}");

    /* 如果没有提供缓冲区或缓冲区太小，返回所需大小 */
    if (!buffer || size == 0) {
        if (desc_heap)
            lv_free((void **) &desc_buf);
        return needed;
    }

    /* 写入实际数据 */
    int pos = 0;
    pos = stream_buf_append(buffer, size, pos, "{\n");
    pos = stream_buf_append(buffer, size, pos, "  \"type\": \"%s\",\n", type_id);
    pos = stream_buf_append(buffer, size, pos, "  \"type_name\": \"%s\",\n", type_name);
    pos = stream_buf_append(buffer, size, pos, "  \"color\": \"%s\",\n", color);
    pos = stream_buf_append(buffer, size, pos, "  \"timestamp_ms\": %ld,\n", event->timestamp_ms);
    pos = stream_buf_append(buffer, size, pos, "  \"step\": %d,\n", event->step_number);
    pos = stream_buf_append(buffer, size, pos, "  \"total_steps\": %d,\n", event->total_steps);
    pos = stream_buf_append(buffer, size, pos, "  \"node_id\": %d,\n", event->node_id);
    pos = stream_buf_append(buffer, size, pos, "  \"constraint_id\": %d,\n", event->constraint_id);
    pos = stream_buf_append(buffer, size, pos, "  \"rule_id\": %d,\n", event->rule_id);
    pos = stream_buf_append(buffer, size, pos, "  \"var_id\": %d,\n", event->var_id);
    if (event->description) {
        pos = stream_buf_append(buffer, size, pos, "  \"description\": \"%s\",\n", desc_buf);
    } else {
        pos = stream_buf_append(buffer, size, pos, "  \"description\": null,\n");
    }
    pos = stream_buf_append(buffer, size, pos, "  \"progress\": %.6g,\n", event->progress);
    pos = stream_buf_append(buffer, size, pos, "  \"numeric_value\": %.6g\n", event->numeric_value);
    pos = stream_buf_append(buffer, size, pos, "}");

    /* 确保终止符 */
    if ((size_t) pos < size) {
        buffer[pos] = '\0';
    } else if (size > 0) {
        buffer[size - 1] = '\0';
    }

    /* Free heap-allocated description buffer if used */
    if (desc_heap)
        lv_free((void **) &desc_buf);

    return pos;
}

/**
 * 将流式事件序列化为 JSON-RPC notification 字符串。
 *
 * 输出格式: {"jsonrpc":"2.0","method":"stream.event","params":{...event_json...}}
 * 适用于 interop STDIO/WebSocket 模式的实时事件推送。
 *
 * @param event   流式事件
 * @param buffer  输出缓冲区
 * @param size    缓冲区大小
 * @return 写入的字节数（不含终止符），缓冲区不足时返回所需大小
 */
int stream_event_to_jsonrpc(const StreamEvent *event, char *buffer, size_t size) {
    if (!event) {
        if (buffer && size > 0)
            buffer[0] = '\0';
        return 0;
    }

    /* 先将事件序列化为 JSON 到临时缓冲区 */
    char event_json[STREAM_JSON_BUFFER_DEFAULT_SIZE];
    int event_json_len = stream_event_to_json(event, event_json, sizeof(event_json));

    /* 如果事件 JSON 太长，使用动态分配 */
    char *event_json_ptr = event_json;
    if (event_json_len >= (int) sizeof(event_json)) {
        /* 需要更大的缓冲区 */
        size_t needed = (size_t) event_json_len + 1;
        event_json_ptr = (char *) lv_malloc(needed);
        if (!event_json_ptr) {
            if (buffer && size > 0)
                buffer[0] = '\0';
            return 0;
        }
        stream_event_to_json(event, event_json_ptr, needed);
    }

    /* 构建 JSON-RPC 外壳 */
    int pos = 0;
    pos = stream_buf_append(buffer, size, pos, "{\"jsonrpc\":\"2.0\",\"method\":\"stream.event\",\"params\":");
    pos = stream_buf_append(buffer, size, pos, "%s", event_json_ptr);
    pos = stream_buf_append(buffer, size, pos, "}");

    /* 确保终止符 */
    if ((size_t) pos < size) {
        buffer[pos] = '\0';
    } else if (size > 0) {
        buffer[size - 1] = '\0';
    }

    /* 释放动态分配的临时缓冲区 */
    if (event_json_ptr != event_json) {
        lv_free((void **) &event_json_ptr);
    }

    return pos;
}

