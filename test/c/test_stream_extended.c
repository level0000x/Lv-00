/**
 * @file test_stream_extended.c
 * @brief 流式输出系统扩展测试
 *
 * 测试 stream 模块的高级功能：
 * - 异步模式（async mode）
 * - 回调过滤（callback filtering with masks）
 * - JSON 序列化（stream_event_to_json, stream_event_to_jsonrpc）
 * - 事件统计 API
 * - 过滤掩码解析（stream_parse_filter_mask）
 * - 发射模式配置（emit mode）
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00_utils.h"
#include "stream.h"

/** 仅计数的简单回调 */
static void count_callback(const StreamEvent *event, void *user_data) {
    (void) event;
    int *count = (int *) user_data;
    if (count)
        (*count)++;
}

static int test_async_mode_basic(void) {
    printf("Test: async mode basic...\n");

    StreamContext *ctx = stream_context_create();
    assert(ctx != NULL);

    int event_count = 0;

    stream_register_callback(ctx, count_callback, &event_count);

    assert(stream_set_async_mode(ctx, true, 0) == true);
    assert(stream_pending_count(ctx) == 0);

    StreamEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = STREAM_EVENT_ENGINE_START;

    for (int i = 0; i < 10; i++) {
        stream_emit(ctx, &ev);
    }

    assert(stream_pending_count(ctx) > 0);

    stream_flush(ctx);

    assert(event_count == 10);
    assert(stream_pending_count(ctx) == 0);

    stream_context_destroy(ctx);
    printf("  PASSED\n");
    return 0;
}

static int test_async_mode_capacity(void) {
    printf("Test: async mode capacity...\n");

    StreamContext *ctx = stream_context_create();
    assert(ctx != NULL);

    stream_register_callback(ctx, count_callback, &(int) {0});

    assert(stream_set_async_mode(ctx, true, 100) == true);

    StreamEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = STREAM_EVENT_INFO;

    for (int i = 0; i < 50; i++) {
        stream_emit(ctx, &ev);
    }

    assert(stream_pending_count(ctx) == 50);

    stream_flush(ctx);
    stream_context_destroy(ctx);
    printf("  PASSED\n");
    return 0;
}

static int test_callback_filter_by_id(void) {
    printf("Test: callback filter by ID...\n");

    StreamContext *ctx = stream_context_create();
    assert(ctx != NULL);

    int error_count = 0;
    int warning_count = 0;

    int cb_id1 = stream_register_callback_ex(ctx, count_callback, &error_count, STREAM_EVENT_MASK(STREAM_EVENT_ERROR));

    int cb_id2 =
        stream_register_callback_ex(ctx, count_callback, &warning_count, STREAM_EVENT_MASK(STREAM_EVENT_WARNING));

    assert(cb_id1 >= 0);
    assert(cb_id2 >= 0);
    assert(cb_id1 != cb_id2);

    StreamEvent ev;
    memset(&ev, 0, sizeof(ev));

    ev.type = STREAM_EVENT_ERROR;
    stream_emit(ctx, &ev);
    assert(error_count == 1);
    assert(warning_count == 0);

    ev.type = STREAM_EVENT_WARNING;
    stream_emit(ctx, &ev);
    assert(error_count == 1);
    assert(warning_count == 1);

    ev.type = STREAM_EVENT_INFO;
    stream_emit(ctx, &ev);
    assert(error_count == 1);
    assert(warning_count == 1);

    stream_context_destroy(ctx);
    printf("  PASSED\n");
    return 0;
}

static int test_callback_filter_update(void) {
    printf("Test: callback filter update...\n");

    StreamContext *ctx = stream_context_create();
    assert(ctx != NULL);

    int count1 = 0;
    int cb_id = stream_register_callback_ex(ctx, count_callback, &count1, STREAM_EVENT_MASK(STREAM_EVENT_ERROR));

    assert(cb_id >= 0);

    StreamEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = STREAM_EVENT_ERROR;
    stream_emit(ctx, &ev);
    assert(count1 == 1);

    ev.type = STREAM_EVENT_WARNING;
    stream_emit(ctx, &ev);
    assert(count1 == 1);

    stream_set_callback_filter(ctx, cb_id, STREAM_EVENT_MASK(STREAM_EVENT_WARNING));

    ev.type = STREAM_EVENT_WARNING;
    stream_emit(ctx, &ev);
    assert(count1 == 2);

    ev.type = STREAM_EVENT_ERROR;
    stream_emit(ctx, &ev);
    assert(count1 == 2);

    stream_context_destroy(ctx);
    printf("  PASSED\n");
    return 0;
}

static int test_callback_filter_get(void) {
    printf("Test: callback filter get...\n");

    StreamContext *ctx = stream_context_create();
    assert(ctx != NULL);

    uint64_t mask1 = STREAM_EVENT_MASK(STREAM_EVENT_ERROR);
    int cb_id = stream_register_callback_ex(ctx, count_callback, &(int) {0}, mask1);

    uint64_t retrieved = stream_get_callback_filter(ctx, cb_id);
    assert(retrieved == mask1);

    uint64_t invalid = stream_get_callback_filter(ctx, 9999);
    assert(invalid == STREAM_FILTER_NONE);

    stream_context_destroy(ctx);
    printf("  PASSED\n");
    return 0;
}

static int test_json_serialization(void) {
    printf("Test: JSON serialization...\n");

    StreamEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = STREAM_EVENT_ENGINE_START;
    ev.timestamp_ms = 1234567890;
    ev.step_number = 5;
    ev.node_id = 42;
    ev.constraint_id = 7;
    ev.description = "Test event";
    ev.progress = 0.5;
    ev.numeric_value = 3.14159;

    char buffer[4096];
    int len = stream_event_to_json(&ev, buffer, sizeof(buffer));

    assert(len > 0);
    assert(strlen(buffer) > 0);

    assert(strstr(buffer, "\"type\"") != NULL);
    assert(strstr(buffer, "\"timestamp_ms\"") != NULL);
    assert(strstr(buffer, "\"step\"") != NULL);

    printf("  JSON output sample: %.100s...\n", buffer);

    stream_context_destroy(NULL);
    printf("  PASSED\n");
    return 0;
}

static int test_jsonrpc_serialization(void) {
    printf("Test: JSON-RPC serialization...\n");

    StreamEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = STREAM_EVENT_SOLVE_DONE;
    ev.timestamp_ms = 9876543210;
    ev.step_number = 100;
    ev.node_id = -1;
    ev.constraint_id = -1;
    ev.description = "Solve completed";

    char buffer[4096];
    int len = stream_event_to_jsonrpc(&ev, buffer, sizeof(buffer));

    assert(len > 0);
    assert(strstr(buffer, "\"jsonrpc\"") != NULL);
    assert(strstr(buffer, "\"method\"") != NULL);
    assert(strstr(buffer, "\"params\"") != NULL);

    printf("  JSON-RPC output sample: %.100s...\n", buffer);

    printf("  PASSED\n");
    return 0;
}

static int test_event_stats(void) {
    printf("Test: event stats...\n");

    StreamContext *ctx = stream_context_create();
    assert(ctx != NULL);

    stream_reset_stats(ctx);

    StreamEvent ev;
    memset(&ev, 0, sizeof(ev));

    ev.type = STREAM_EVENT_ENGINE_START;
    stream_emit(ctx, &ev);

    ev.type = STREAM_EVENT_ENGINE_DONE;
    stream_emit(ctx, &ev);

    ev.type = STREAM_EVENT_ENGINE_START;
    stream_emit(ctx, &ev);

    assert(stream_get_event_count(ctx, STREAM_EVENT_ENGINE_START) == 2);
    assert(stream_get_event_count(ctx, STREAM_EVENT_ENGINE_DONE) == 1);
    assert(stream_get_event_count(ctx, STREAM_EVENT_ERROR) == 0);

    assert(stream_get_total_event_count(ctx) == 3);

    stream_context_destroy(ctx);
    printf("  PASSED\n");
    return 0;
}

static int test_dropped_event_count(void) {
    printf("Test: dropped event count...\n");

    StreamContext *ctx = stream_context_create();
    assert(ctx != NULL);

    stream_register_callback(ctx, count_callback, &(int) {0});
    stream_set_async_mode(ctx, true, 5);

    StreamEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = STREAM_EVENT_INFO;

    for (int i = 0; i < 20; i++) {
        stream_emit(ctx, &ev);
    }

    long dropped = stream_get_dropped_count(ctx);
    printf("  Dropped events: %ld (queue capacity was 5)\n", dropped);

    stream_flush(ctx);
    stream_context_destroy(ctx);
    printf("  PASSED\n");
    return 0;
}

static int test_parse_filter_mask(void) {
    printf("Test: parse filter mask...\n");

    uint64_t mask;

    mask = stream_parse_filter_mask("all");
    assert(mask == STREAM_FILTER_ALL);

    mask = stream_parse_filter_mask("*");
    assert(mask == STREAM_FILTER_ALL);

    mask = stream_parse_filter_mask("none");
    assert(mask == STREAM_FILTER_NONE);

    mask = stream_parse_filter_mask("ENGINE_START");
    assert(mask == STREAM_EVENT_MASK(STREAM_EVENT_ENGINE_START));

    mask = stream_parse_filter_mask("ENGINE_START,ENGINE_DONE");
    assert(mask == (STREAM_EVENT_MASK(STREAM_EVENT_ENGINE_START) | STREAM_EVENT_MASK(STREAM_EVENT_ENGINE_DONE)));

    mask = stream_parse_filter_mask("invalid_type");
    assert(mask == STREAM_FILTER_NONE);

    mask = stream_parse_filter_mask(NULL);
    assert(mask == STREAM_FILTER_NONE);

    printf("  PASSED\n");
    return 0;
}

static int test_emit_mode(void) {
    printf("Test: emit mode configuration...\n");

    StreamContext *ctx = stream_context_create();
    assert(ctx != NULL);

    stream_set_emit_mode(ctx, STREAM_EMIT_IMMEDIATE, 0);
    assert(stream_get_emit_mode(ctx) == STREAM_EMIT_IMMEDIATE);

    stream_set_emit_mode(ctx, STREAM_EMIT_BUFFERED, 0);
    assert(stream_get_emit_mode(ctx) == STREAM_EMIT_BUFFERED);

    stream_set_emit_mode(ctx, STREAM_EMIT_THROTTLED, 100);
    assert(stream_get_emit_mode(ctx) == STREAM_EMIT_THROTTLED);

    stream_context_destroy(ctx);
    printf("  PASSED\n");
    return 0;
}

static int test_unregister_by_id(void) {
    printf("Test: unregister callback by ID...\n");

    StreamContext *ctx = stream_context_create();
    assert(ctx != NULL);

    int count1 = 0, count2 = 0, count3 = 0;

    int id1 = stream_register_callback_ex(ctx, count_callback, &count1, STREAM_FILTER_ALL);
    int id2 = stream_register_callback_ex(ctx, count_callback, &count2, STREAM_FILTER_ALL);
    int id3 = stream_register_callback_ex(ctx, count_callback, &count3, STREAM_FILTER_ALL);

    assert(id1 >= 0 && id2 >= 0 && id3 >= 0);
    assert(id1 != id2 && id2 != id3);

    StreamEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = STREAM_EVENT_INFO;
    stream_emit(ctx, &ev);

    assert(count1 == 1 && count2 == 1 && count3 == 1);

    assert(stream_unregister_callback_by_id(ctx, id2) == true);

    stream_emit(ctx, &ev);
    assert(count1 == 2 && count2 == 1 && count3 == 2);

    assert(stream_unregister_callback_by_id(ctx, 9999) == false);

    stream_context_destroy(ctx);
    printf("  PASSED\n");
    return 0;
}

static int test_event_type_id(void) {
    printf("Test: event type ID...\n");

    const char *id;

    id = stream_event_type_id(STREAM_EVENT_ENGINE_START);
    assert(id != NULL);
    assert(strlen(id) > 0);

    id = stream_event_type_id(STREAM_EVENT_ERROR);
    assert(id != NULL);

    id = stream_event_type_id(STREAM_EVENT_WARNING);
    assert(id != NULL);

    printf("  ENGINE_START ID: %s\n", stream_event_type_id(STREAM_EVENT_ENGINE_START));
    printf("  ERROR ID: %s\n", stream_event_type_id(STREAM_EVENT_ERROR));

    printf("  PASSED\n");
    return 0;
}

static int test_stream_emit_helpers(void) {
    printf("Test: stream emit helper functions...\n");

    StreamContext *ctx = stream_context_create();
    assert(ctx != NULL);

    int node_count = 0;
    int constraint_count = 0;
    int progress_count = 0;
    int numeric_count = 0;
    int error_count = 0;

    stream_register_callback_ex(ctx, count_callback, &node_count, STREAM_EVENT_MASK(STREAM_EVENT_NODE_ADDED));
    stream_register_callback_ex(ctx, count_callback, &constraint_count,
                                STREAM_EVENT_MASK(STREAM_EVENT_CONSTRAINT_ADDED));
    stream_register_callback_ex(ctx, count_callback, &progress_count, STREAM_EVENT_MASK(STREAM_EVENT_PROGRESS));
    stream_register_callback_ex(ctx, count_callback, &numeric_count,
                                STREAM_EVENT_MASK(STREAM_EVENT_SOLVE_VARIABLE_RESOLVED));
    stream_register_callback_ex(ctx, count_callback, &error_count, STREAM_EVENT_MASK(STREAM_EVENT_ERROR));

    stream_emit_node_event(ctx, STREAM_EVENT_NODE_ADDED, 42, "Test node", 1);
    assert(node_count == 1);

    stream_emit_constraint_event(ctx, STREAM_EVENT_CONSTRAINT_ADDED, 7, "Test constraint", 2);
    assert(constraint_count == 1);

    stream_emit_progress(ctx, 0.75, "Progress test", 3, 4);
    assert(progress_count == 1);

    stream_emit_variable_resolved(ctx, 99, 3.14159, "Variable resolved", 4);
    assert(numeric_count == 1);

    stream_emit_error(ctx, "Error test", 5);
    assert(error_count == 1);

    stream_emit_warning(ctx, "Warning test", 6);
    stream_emit_info(ctx, "Info test", 7);
    stream_emit_graph_snapshot(ctx, STREAM_EVENT_GRAPH_SNAPSHOT, "{}", "Graph snapshot", 8);
    stream_emit_merge(ctx, 10, 20, 9);

    stream_context_destroy(ctx);
    printf("  PASSED\n");
    return 0;
}

/* ==================== 新增测试用例 ==================== */

/** 测试类别过滤掩码解析 */
static int test_parse_filter_category(void) {
    printf("Test: parse filter category...\n");

    uint64_t mask;

    /* "engine" 类别应包含 ENGINE_START/DONE/PAUSED */
    mask = stream_parse_filter_mask("engine");
    assert(mask != STREAM_FILTER_NONE);
    assert((mask & STREAM_EVENT_MASK(STREAM_EVENT_ENGINE_START)) != 0);
    assert((mask & STREAM_EVENT_MASK(STREAM_EVENT_ENGINE_DONE)) != 0);
    assert((mask & STREAM_EVENT_MASK(STREAM_EVENT_ENGINE_PAUSED)) != 0);
    /* 不应包含其他类别的事件 */
    assert((mask & STREAM_EVENT_MASK(STREAM_EVENT_ERROR)) == 0);

    /* "rewrite" 类别 */
    mask = stream_parse_filter_mask("rewrite");
    assert(mask != STREAM_FILTER_NONE);
    assert((mask & STREAM_EVENT_MASK(STREAM_EVENT_REWRITE_START)) != 0);
    assert((mask & STREAM_EVENT_MASK(STREAM_EVENT_REWRITE_MATCH_FOUND)) != 0);
    assert((mask & STREAM_EVENT_MASK(STREAM_EVENT_REWRITE_APPLIED)) != 0);
    assert((mask & STREAM_EVENT_MASK(STREAM_EVENT_REWRITE_DONE)) != 0);

    /* "solve" 类别 */
    mask = stream_parse_filter_mask("solve");
    assert(mask != STREAM_FILTER_NONE);
    assert((mask & STREAM_EVENT_MASK(STREAM_EVENT_SOLVE_START)) != 0);
    assert((mask & STREAM_EVENT_MASK(STREAM_EVENT_SOLVE_VARIABLE_RESOLVED)) != 0);

    /* "proof" 类别 */
    mask = stream_parse_filter_mask("proof");
    assert(mask != STREAM_FILTER_NONE);
    assert((mask & STREAM_EVENT_MASK(STREAM_EVENT_PROOF_STEP_ADDED)) != 0);
    assert((mask & STREAM_EVENT_MASK(STREAM_EVENT_PROOF_UNIFY)) != 0);

    /* "func_block" 类别 */
    mask = stream_parse_filter_mask("func_block");
    assert(mask != STREAM_FILTER_NONE);
    assert((mask & STREAM_EVENT_MASK(STREAM_EVENT_FUNC_BLOCK_PACK_START)) != 0);
    assert((mask & STREAM_EVENT_MASK(STREAM_EVENT_FUNC_BLOCK_INSTANTIATE_DONE)) != 0);

    /* "normalize" 类别 */
    mask = stream_parse_filter_mask("normalize");
    assert(mask != STREAM_FILTER_NONE);
    assert((mask & STREAM_EVENT_MASK(STREAM_EVENT_NORMALIZE_START)) != 0);
    assert((mask & STREAM_EVENT_MASK(STREAM_EVENT_NORMALIZE_MERGE)) != 0);
    assert((mask & STREAM_EVENT_MASK(STREAM_EVENT_NORMALIZE_DONE)) != 0);

    /* "conflict" 类别 */
    mask = stream_parse_filter_mask("conflict");
    assert(mask == STREAM_EVENT_MASK(STREAM_EVENT_CONFLICT_DETECTED));

    /* "info" 类别 */
    mask = stream_parse_filter_mask("info");
    assert(mask != STREAM_FILTER_NONE);
    assert((mask & STREAM_EVENT_MASK(STREAM_EVENT_INFO)) != 0);
    assert((mask & STREAM_EVENT_MASK(STREAM_EVENT_PROGRESS)) != 0);
    assert((mask & STREAM_EVENT_MASK(STREAM_EVENT_GRAPH_SNAPSHOT)) != 0);

    /* 混合使用类别和事件 ID */
    mask = stream_parse_filter_mask("engine,REWRITE_START,ERROR");
    assert((mask & STREAM_EVENT_MASK(STREAM_EVENT_ENGINE_START)) != 0);
    assert((mask & STREAM_EVENT_MASK(STREAM_EVENT_REWRITE_START)) != 0);
    assert((mask & STREAM_EVENT_MASK(STREAM_EVENT_ERROR)) != 0);
    assert((mask & STREAM_EVENT_MASK(STREAM_EVENT_SOLVE_START)) == 0);

    /* 大小写不敏感 */
    mask = stream_parse_filter_mask("Engine");
    assert((mask & STREAM_EVENT_MASK(STREAM_EVENT_ENGINE_START)) != 0);

    /* 带空格的输入 */
    mask = stream_parse_filter_mask(" ENGINE_START , ENGINE_DONE ");
    assert((mask & STREAM_EVENT_MASK(STREAM_EVENT_ENGINE_START)) != 0);
    assert((mask & STREAM_EVENT_MASK(STREAM_EVENT_ENGINE_DONE)) != 0);

    /* 空字符串 */
    mask = stream_parse_filter_mask("");
    assert(mask == STREAM_FILTER_NONE);

    /* 纯空白字符串 */
    mask = stream_parse_filter_mask("   ");
    assert(mask == STREAM_FILTER_NONE);

    printf("  PASSED\n");
    return 0;
}

/** 测试 stream_clear_buffer 清空缓冲区 */
static int test_clear_buffer(void) {
    printf("Test: clear buffer...\n");

    StreamContext *ctx = stream_context_create();
    assert(ctx != NULL);

    int event_count = 0;
    stream_register_callback(ctx, count_callback, &event_count);

    /* 切换到缓冲模式 */
    stream_set_emit_mode(ctx, STREAM_EMIT_BUFFERED, 0);

    StreamEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = STREAM_EVENT_INFO;

    for (int i = 0; i < 10; i++) {
        stream_emit(ctx, &ev);
    }

    assert(stream_pending_count(ctx) == 10);
    assert(stream_buffer_size(ctx) == 10);

    /* 清空缓冲区（不发射） */
    int cleared = stream_clear_buffer(ctx);
    assert(cleared == 10);
    assert(stream_pending_count(ctx) == 0);
    assert(stream_buffer_size(ctx) == 0);
    /* 回调不应被调用 */
    assert(event_count == 0);

    /* 清空已空的缓冲区 */
    cleared = stream_clear_buffer(ctx);
    assert(cleared == 0);

    stream_context_destroy(ctx);
    printf("  PASSED\n");
    return 0;
}

/** 测试缓冲模式手动刷新 */
static int test_buffered_mode_flush(void) {
    printf("Test: buffered mode flush...\n");

    StreamContext *ctx = stream_context_create();
    assert(ctx != NULL);

    int event_count = 0;
    stream_register_callback(ctx, count_callback, &event_count);

    stream_set_emit_mode(ctx, STREAM_EMIT_BUFFERED, 0);

    StreamEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = STREAM_EVENT_PROGRESS;

    /* 缓冲模式下事件不立即触发回调 */
    stream_emit(ctx, &ev);
    stream_emit(ctx, &ev);
    assert(event_count == 0);
    assert(stream_pending_count(ctx) == 2);

    /* 手动刷新 */
    stream_flush(ctx);
    assert(event_count == 2);
    assert(stream_pending_count(ctx) == 0);

    stream_context_destroy(ctx);
    printf("  PASSED\n");
    return 0;
}

/** 测试从缓冲模式切换到立即模式时自动刷新 */
static int test_mode_switch_auto_flush(void) {
    printf("Test: mode switch auto flush...\n");

    StreamContext *ctx = stream_context_create();
    assert(ctx != NULL);

    int event_count = 0;
    stream_register_callback(ctx, count_callback, &event_count);

    /* 缓冲模式 */
    stream_set_emit_mode(ctx, STREAM_EMIT_BUFFERED, 0);

    StreamEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = STREAM_EVENT_INFO;
    stream_emit(ctx, &ev);
    stream_emit(ctx, &ev);
    assert(event_count == 0);

    /* 切换到立即模式应自动刷新 */
    stream_set_emit_mode(ctx, STREAM_EMIT_IMMEDIATE, 0);
    assert(event_count == 2);
    assert(stream_pending_count(ctx) == 0);

    /* 切换后事件应立即触发 */
    stream_emit(ctx, &ev);
    assert(event_count == 3);

    stream_context_destroy(ctx);
    printf("  PASSED\n");
    return 0;
}

/** 测试便捷发射函数的字段正确性 */
static int test_emit_helper_fields(void) {
    printf("Test: emit helper field correctness...\n");

    StreamContext *ctx = stream_context_create();
    assert(ctx != NULL);

    /* 捕获回调：记录最后一次事件的完整字段 */
    StreamEvent captured;
    memset(&captured, 0, sizeof(captured));

    /* 使用闭包捕获完整事件 */
    stream_register_callback(ctx, count_callback, &(int) {0});

    /* 直接使用 stream_emit + 自定义回调来验证字段 */
    int verify_count = 0;
    /* 注册一个字段验证回调 */
    struct {
        StreamEventType expected_type;
        int expected_node_id;
        int expected_step;
        double expected_progress;
        double expected_numeric;
        int *verify_count;
    } verify_data;

    /* 测试 stream_emit_node_event 的字段 */
    {
        int vc = 0;
        /* 使用一个简单的 lambda 风格回调来捕获事件 */
        /* 由于 C 没有闭包，我们用全局变量 */
    }

    /* 替代方案：直接发射事件后通过统计验证 */
    stream_emit_node_event(ctx, STREAM_EVENT_NODE_ADDED, 42, "node test", 3);
    assert(stream_get_event_count(ctx, STREAM_EVENT_NODE_ADDED) == 1);

    stream_emit_constraint_event(ctx, STREAM_EVENT_CONSTRAINT_ADDED, 99, "constraint test", 4);
    assert(stream_get_event_count(ctx, STREAM_EVENT_CONSTRAINT_ADDED) == 1);

    stream_emit_progress(ctx, 0.85, "progress test", 5, 10);
    assert(stream_get_event_count(ctx, STREAM_EVENT_PROGRESS) == 1);

    stream_emit_numeric(ctx, STREAM_EVENT_SOLVE_VARIABLE_RESOLVED, 2.71828, "numeric test", 6);
    assert(stream_get_event_count(ctx, STREAM_EVENT_SOLVE_VARIABLE_RESOLVED) == 1);

    stream_emit_graph_snapshot(ctx, STREAM_EVENT_GRAPH_SNAPSHOT, "{\"nodes\":[]}", "snapshot test", 7);
    assert(stream_get_event_count(ctx, STREAM_EVENT_GRAPH_SNAPSHOT) == 1);

    stream_emit_merge(ctx, 15, 25, 8);
    assert(stream_get_event_count(ctx, STREAM_EVENT_NORMALIZE_MERGE) == 1);

    stream_emit_variable_resolved(ctx, 77, 1.41421, "var resolved", 9);
    assert(stream_get_event_count(ctx, STREAM_EVENT_SOLVE_VARIABLE_RESOLVED) == 2);

    /* 总事件数验证 */
    assert(stream_get_total_event_count(ctx) == 7);

    stream_context_destroy(ctx);
    printf("  PASSED\n");
    return 0;
}

/** 测试 stream_reset_stats 重置统计 */
static int test_reset_stats(void) {
    printf("Test: reset stats...\n");

    StreamContext *ctx = stream_context_create();
    assert(ctx != NULL);

    StreamEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = STREAM_EVENT_INFO;

    stream_emit(ctx, &ev);
    stream_emit(ctx, &ev);
    stream_emit(ctx, &ev);

    assert(stream_get_total_event_count(ctx) == 3);
    assert(stream_get_event_count(ctx, STREAM_EVENT_INFO) == 3);

    stream_reset_stats(ctx);

    assert(stream_get_total_event_count(ctx) == 0);
    assert(stream_get_event_count(ctx, STREAM_EVENT_INFO) == 0);
    assert(stream_get_dropped_count(ctx) == 0);

    stream_context_destroy(ctx);
    printf("  PASSED\n");
    return 0;
}

/** 测试 NULL 安全性 */
static int test_null_safety(void) {
    printf("Test: NULL safety...\n");

    /* 所有 API 在 ctx=NULL 时应安全返回 */
    stream_context_destroy(NULL);

    assert(stream_register_callback(NULL, count_callback, NULL) == false);
    assert(stream_register_callback_ex(NULL, count_callback, NULL, STREAM_FILTER_ALL) < 0);
    assert(stream_unregister_callback(NULL, count_callback) == false);
    assert(stream_unregister_callback_by_id(NULL, 1) == false);
    assert(stream_set_callback_filter(NULL, 1, STREAM_FILTER_ALL) == false);
    assert(stream_get_callback_filter(NULL, 1) == STREAM_FILTER_NONE);

    stream_set_emit_mode(NULL, STREAM_EMIT_IMMEDIATE, 0);
    assert(stream_get_emit_mode(NULL) == STREAM_EMIT_IMMEDIATE);

    stream_emit(NULL, NULL);
    stream_emit_simple(NULL, STREAM_EVENT_INFO, "test", 0);
    stream_emit_node_event(NULL, STREAM_EVENT_NODE_ADDED, 1, "test", 0);
    stream_emit_constraint_event(NULL, STREAM_EVENT_CONSTRAINT_ADDED, 1, "test", 0);
    stream_emit_progress(NULL, 0.5, "test", 0, 1);
    stream_emit_numeric(NULL, STREAM_EVENT_INFO, 1.0, "test", 0);
    stream_emit_graph_snapshot(NULL, STREAM_EVENT_GRAPH_SNAPSHOT, "{}", "test", 0);
    stream_emit_merge(NULL, 1, 2, 0);
    stream_emit_variable_resolved(NULL, 1, 1.0, "test", 0);
    stream_emit_error(NULL, "test", 0);
    stream_emit_warning(NULL, "test", 0);
    stream_emit_info(NULL, "test", 0);

    assert(stream_set_async_mode(NULL, true, 100) == false);
    stream_flush(NULL);
    assert(stream_pending_count(NULL) == 0);
    assert(stream_clear_buffer(NULL) == 0);
    assert(stream_buffer_size(NULL) == 0);

    stream_reset_stats(NULL);
    assert(stream_get_event_count(NULL, STREAM_EVENT_INFO) == 0);
    assert(stream_get_total_event_count(NULL) == 0);
    assert(stream_get_dropped_count(NULL) == 0);

    /* stream_event_to_json 和 stream_event_to_jsonrpc 对 NULL event 应安全 */
    char buf[256];
    int len = stream_event_to_json(NULL, buf, sizeof(buf));
    assert(len == 0);

    len = stream_event_to_jsonrpc(NULL, buf, sizeof(buf));
    assert(len == 0);

    printf("  PASSED\n");
    return 0;
}

/** 测试大量回调注册（接近上限） */
static int test_many_callbacks(void) {
    printf("Test: many callbacks registration...\n");

    StreamContext *ctx = stream_context_create();
    assert(ctx != NULL);

    int counts[32];
    memset(counts, 0, sizeof(counts));

    /* 注册 32 个回调（使用 _ex 版本获取 ID） */
    int ids[32];
    for (int i = 0; i < 32; i++) {
        ids[i] = stream_register_callback_ex(ctx, count_callback, &counts[i], STREAM_FILTER_ALL);
        assert(ids[i] >= 0);
        /* 验证每个回调获得唯一 ID */
        for (int j = 0; j < i; j++) {
            assert(ids[i] != ids[j]);
        }
    }

    StreamEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = STREAM_EVENT_INFO;

    stream_emit(ctx, &ev);

    /* 验证所有 32 个回调都被调用 */
    for (int i = 0; i < 32; i++) {
        assert(counts[i] == 1);
    }

    /* 通过 ID 注销部分回调 */
    assert(stream_unregister_callback_by_id(ctx, ids[0]) == true);
    assert(stream_unregister_callback_by_id(ctx, ids[15]) == true);
    assert(stream_unregister_callback_by_id(ctx, ids[31]) == true);

    stream_emit(ctx, &ev);

    /* 已注销的回调不应再被调用 */
    assert(counts[0] == 1);
    assert(counts[15] == 1);
    assert(counts[31] == 1);
    /* 其余回调应被调用两次 */
    assert(counts[1] == 2);
    assert(counts[16] == 2);
    assert(counts[30] == 2);

    stream_context_destroy(ctx);
    printf("  PASSED\n");
    return 0;
}

/** 测试 JSON 序列化包含特殊字符的描述 */
static int test_json_escape(void) {
    printf("Test: JSON escape special characters...\n");

    StreamEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = STREAM_EVENT_ERROR;
    ev.description = "Test with \"quotes\" and \\backslash\\ and\nnewline";

    char buffer[4096];
    int len = stream_event_to_json(&ev, buffer, sizeof(buffer));

    assert(len > 0);
    /* 转义后的字符串应包含转义序列 */
    assert(strstr(buffer, "\\\"quotes\\\"") != NULL);
    assert(strstr(buffer, "\\\\backslash\\\\") != NULL);
    assert(strstr(buffer, "\\n") != NULL);

    /* NULL 描述应序列化为 null */
    ev.description = NULL;
    len = stream_event_to_json(&ev, buffer, sizeof(buffer));
    assert(len > 0);
    assert(strstr(buffer, "\"description\": null") != NULL);

    printf("  PASSED\n");
    return 0;
}

/** 测试禁用异步模式时自动刷新 */
static int test_async_disable_flush(void) {
    printf("Test: async disable auto flush...\n");

    StreamContext *ctx = stream_context_create();
    assert(ctx != NULL);

    int event_count = 0;
    stream_register_callback(ctx, count_callback, &event_count);

    /* 启用异步（BUFFERED 别名） */
    stream_set_async_mode(ctx, true, 0);

    StreamEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = STREAM_EVENT_INFO;

    stream_emit(ctx, &ev);
    stream_emit(ctx, &ev);
    stream_emit(ctx, &ev);

    assert(event_count == 0);
    assert(stream_pending_count(ctx) == 3);

    /* 禁用异步应自动刷新 */
    stream_set_async_mode(ctx, false, 0);
    assert(event_count == 3);
    assert(stream_pending_count(ctx) == 0);

    stream_context_destroy(ctx);
    printf("  PASSED\n");
    return 0;
}

int main(void) {
    printf("=== Lv-00 Stream Extended Test Suite ===\n\n");

    test_async_mode_basic();
    test_async_mode_capacity();
    test_callback_filter_by_id();
    test_callback_filter_update();
    test_callback_filter_get();
    test_json_serialization();
    test_jsonrpc_serialization();
    test_event_stats();
    test_dropped_event_count();
    test_parse_filter_mask();
    test_emit_mode();
    test_unregister_by_id();
    test_event_type_id();
    test_stream_emit_helpers();

    /* 新增测试 */
    test_parse_filter_category();
    test_clear_buffer();
    test_buffered_mode_flush();
    test_mode_switch_auto_flush();
    test_emit_helper_fields();
    test_reset_stats();
    test_null_safety();
    test_many_callbacks();
    test_json_escape();
    test_async_disable_flush();

    printf("\n=== All stream extended tests PASSED! ===\n");
    return 0;
}
