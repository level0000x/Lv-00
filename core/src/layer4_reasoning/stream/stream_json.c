/**
 * @file stream_json.c
 * @brief 流式输出系统 —— 缓冲区管理 API 与 JSON 序列化
 */

#include "stream_internal.h"

#include "lv/lv_json.h"

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

/**
 * 将流式事件序列化为 JSON 字符串。
 *
 * 统一使用公共写入器 lvJsonBuf；字符串字段经 append_string 自动 JSON 转义
 * （此前自建的 stream_buf_append / stream_json_escape 已删除）。
 * 输出包含以下字段：
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

    lvJsonBuf jb;
    if (!lv_json_buf_init(&jb, 512)) {
        if (buffer && size > 0)
            buffer[0] = '\0';
        return 0;
    }

    lv_json_buf_append_raw(&jb, "{\n");
    lv_json_buf_append_raw(&jb, "  \"type\": ");
    lv_json_buf_append_string(&jb, type_id);
    lv_json_buf_append_raw(&jb, ",\n  \"type_name\": ");
    lv_json_buf_append_string(&jb, type_name);
    lv_json_buf_append_raw(&jb, ",\n  \"color\": ");
    lv_json_buf_append_string(&jb, color);
    lv_json_buf_append_fmt(&jb, ",\n  \"timestamp_ms\": %ld", event->timestamp_ms);
    lv_json_buf_append_fmt(&jb, ",\n  \"step\": %d", event->step_number);
    lv_json_buf_append_fmt(&jb, ",\n  \"total_steps\": %d", event->total_steps);
    lv_json_buf_append_fmt(&jb, ",\n  \"node_id\": %d", event->node_id);
    lv_json_buf_append_fmt(&jb, ",\n  \"constraint_id\": %d", event->constraint_id);
    lv_json_buf_append_fmt(&jb, ",\n  \"rule_id\": %d", event->rule_id);
    lv_json_buf_append_fmt(&jb, ",\n  \"var_id\": %d", event->var_id);
    if (event->description) {
        lv_json_buf_append_raw(&jb, ",\n  \"description\": ");
        lv_json_buf_append_string(&jb, event->description);
    } else {
        lv_json_buf_append_raw(&jb, ",\n  \"description\": null");
    }
    /* 流式输出：progress/numeric_value 为进度类数值，%.6g 有意为之
     * （流式事件不需要 15 位精度，6 位有效数字足以表达进度，输出更紧凑；
     * 与 lv_json_buf_append_double 的 %.15g 数据序列化口径不冲突）。 */
    lv_json_buf_append_fmt(&jb, ",\n  \"progress\": %.6g", event->progress);
    lv_json_buf_append_fmt(&jb, ",\n  \"numeric_value\": %.6g\n", event->numeric_value);
    lv_json_buf_append_raw(&jb, "}");

    char *json = lv_json_buf_finalize(&jb);
    if (!json) {
        if (buffer && size > 0)
            buffer[0] = '\0';
        return 0;
    }
    size_t len = strlen(json);
    /* 截断语义：缓冲区不足时写满 size-1 字节 + NUL，返回所需大小 */
    if (buffer && size > 0) {
        lv_strlcpy(buffer, json, size);
    }
    lv_free((void **) &json);
    return (int) len;
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

    /* 构建 JSON-RPC 外壳（lvJsonBuf 拼接，event_json 本身是合法 JSON 片段） */
    lvJsonBuf jb;
    if (!lv_json_buf_init(&jb, (size_t) event_json_len + 128)) {
        if (event_json_ptr != event_json)
            lv_free((void **) &event_json_ptr);
        if (buffer && size > 0)
            buffer[0] = '\0';
        return 0;
    }
    lv_json_buf_append_raw(&jb, "{\"jsonrpc\":\"2.0\",\"method\":\"stream.event\",\"params\":");
    lv_json_buf_append_raw(&jb, event_json_ptr);
    lv_json_buf_append_raw(&jb, "}");
    char *shell = lv_json_buf_finalize(&jb);
    if (!shell) {
        if (event_json_ptr != event_json)
            lv_free((void **) &event_json_ptr);
        if (buffer && size > 0)
            buffer[0] = '\0';
        return 0;
    }
    size_t slen = strlen(shell);
    if (buffer && size > 0) {
        lv_strlcpy(buffer, shell, size);
    }
    lv_free((void **) &shell);
    if (event_json_ptr != event_json)
        lv_free((void **) &event_json_ptr);
    return (int) slen;
}
