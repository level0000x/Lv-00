/**
 * @file test_stream.c
 * @brief 流式输出系统模块测试
 *
 * 测试 stream 模块的核心功能：
 * - 上下文创建与销毁
 * - 回调注册与注销
 * - 事件发射与回调触发
 * - 事件类型名称映射
 * - 事件颜色映射
 * - 简化事件发射
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00_utils.h"
#include "stream.h"

/* ============== 测试辅助结构 ============== */

/** 用于捕获最后一次回调触发的事件数据 */
typedef struct {
    StreamEventType type;
    int node_id;
    int constraint_id;
    int step_number;
    char description[256];
    int call_count; /* 回调触发计数器 */
} CallbackCapture;

/* ============== 测试回调函数 ============== */

/** 捕获事件数据的回调 */
static void capture_callback(const StreamEvent *event, void *user_data) {
    CallbackCapture *cap = (CallbackCapture *) user_data;
    if (!event || !cap)
        return;

    cap->type = event->type;
    cap->node_id = event->node_id;
    cap->constraint_id = event->constraint_id;
    cap->step_number = event->step_number;
    if (event->description) {
        lv00_strlcpy(cap->description, event->description, sizeof(cap->description));
    }
    cap->call_count++;
}

/** 仅计数的简单回调 */
static void count_callback(const StreamEvent *event, void *user_data) {
    (void) event;
    int *count = (int *) user_data;
    if (count)
        (*count)++;
}

/* ============== 测试用例 ============== */

/** 测试上下文创建与销毁 */
static int test_context_lifecycle(void) {
    StreamContext *ctx = stream_context_create();
    if (!ctx) {
        printf("  失败: 上下文创建返回 NULL\n");
        return 1;
    }
    stream_context_destroy(ctx);
    /* NULL 销毁应安全 */
    stream_context_destroy(NULL);
    printf("  通过: 上下文创建/销毁正常\n");
    return 0;
}

/** 测试回调注册与注销 */
static int test_callback_register(void) {
    StreamContext *ctx = stream_context_create();
    if (!ctx) {
        printf("  失败: 无法创建上下文\n");
        return 1;
    }

    CallbackCapture cap;
    memset(&cap, 0, sizeof(cap));

    /* 注册回调 */
    if (!stream_register_callback(ctx, capture_callback, &cap)) {
        printf("  失败: 注册回调返回 false\n");
        stream_context_destroy(ctx);
        return 1;
    }

    /* 重复注册应失败（容量限制方面：同一函数可多次注册） */
    if (!stream_register_callback(ctx, capture_callback, &cap)) {
        printf("  失败: 第二次注册回调返回 false (超出容量?)\n");
        stream_context_destroy(ctx);
        return 1;
    }

    /* NULL 参数应安全返回 */
    if (stream_register_callback(NULL, capture_callback, NULL)) {
        printf("  失败: NULL ctx 注册应返回 false\n");
        stream_context_destroy(ctx);
        return 1;
    }

    /* 注销回调 */
    if (!stream_unregister_callback(ctx, capture_callback)) {
        printf("  失败: 注销已注册回调返回 false\n");
        stream_context_destroy(ctx);
        return 1;
    }

    /* 同一函数注册了两次，第二次注销也应成功 */
    if (!stream_unregister_callback(ctx, capture_callback)) {
        printf("  失败: 注销第二个注册应返回 true（函数被注册了两次）\n");
        stream_context_destroy(ctx);
        return 1;
    }

    /* 现在所有注册都已注销，再次注销应返回 false */
    if (stream_unregister_callback(ctx, capture_callback)) {
        printf("  失败: 注销已删除的回调应返回 false\n");
        stream_context_destroy(ctx);
        return 1;
    }

    /* NULL 参数应安全 */
    if (stream_unregister_callback(NULL, NULL)) {
        printf("  失败: NULL 参数注销应返回 false\n");
        stream_context_destroy(ctx);
        return 1;
    }

    stream_context_destroy(ctx);
    printf("  通过: 回调注册/注销正常\n");
    return 0;
}

/** 测试事件发射 */
static int test_event_emit(void) {
    StreamContext *ctx = stream_context_create();
    if (!ctx)
        return 1;

    CallbackCapture cap;
    memset(&cap, 0, sizeof(cap));

    stream_register_callback(ctx, capture_callback, &cap);

    StreamEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = STREAM_EVENT_ENGINE_START;
    ev.node_id = 42;
    ev.constraint_id = 7;
    ev.step_number = 3;
    ev.description = "测试事件";

    stream_emit(ctx, &ev);

    /* 验证捕获的事件数据 */
    int errors = 0;
    if (cap.call_count != 1) {
        printf("  失败: 回调调用次数=%d, 期望=1\n", cap.call_count);
        errors++;
    }
    if (cap.type != STREAM_EVENT_ENGINE_START) {
        printf("  失败: 事件类型=%d, 期望=%d\n", cap.type, STREAM_EVENT_ENGINE_START);
        errors++;
    }
    if (cap.node_id != 42) {
        printf("  失败: node_id=%d, 期望=42\n", cap.node_id);
        errors++;
    }
    if (cap.step_number != 3) {
        printf("  失败: step_number=%d, 期望=3\n", cap.step_number);
        errors++;
    }

    /* 发射到 NULL 上下文应安全 */
    stream_emit(NULL, &ev);
    stream_emit(ctx, NULL);

    stream_context_destroy(ctx);

    if (errors == 0) {
        printf("  通过: 事件发射正常\n");
    }
    return errors;
}

/** 测试简化事件发射 */
static int test_emit_simple(void) {
    StreamContext *ctx = stream_context_create();
    if (!ctx)
        return 1;

    CallbackCapture cap;
    memset(&cap, 0, sizeof(cap));

    stream_register_callback(ctx, capture_callback, &cap);

    stream_emit_simple(ctx, STREAM_EVENT_WARNING, "简化事件测试", 5);

    if (cap.call_count != 1) {
        printf("  失败: 简化发射后 call_count=%d, 期望=1\n", cap.call_count);
        stream_context_destroy(ctx);
        return 1;
    }
    if (cap.type != STREAM_EVENT_WARNING) {
        printf("  失败: 简化发射后 type=%d, 期望=%d\n", cap.type, STREAM_EVENT_WARNING);
        stream_context_destroy(ctx);
        return 1;
    }
    if (cap.step_number != 5) {
        printf("  失败: 简化发射后 step_number=%d, 期望=5\n", cap.step_number);
        stream_context_destroy(ctx);
        return 1;
    }
    /* 简化发射的 node_id/constraint_id 应为 -1 */
    if (cap.node_id != -1) {
        printf("  失败: 简化发射后 node_id=%d, 期望=-1\n", cap.node_id);
        stream_context_destroy(ctx);
        return 1;
    }

    /* NULL ctx 应安全 */
    stream_emit_simple(NULL, STREAM_EVENT_INFO, "test", 0);

    stream_context_destroy(ctx);
    printf("  通过: 简化事件发射正常\n");
    return 0;
}

/** 测试事件类型名称映射 */
static int test_event_type_names(void) {
    int errors = 0;

    const char *name;
    name = stream_event_type_name(STREAM_EVENT_ENGINE_START);
    if (!name || strlen(name) == 0) {
        printf("  失败: ENGINE_START 名称为空\n");
        errors++;
    }

    name = stream_event_type_name(STREAM_EVENT_NORMALIZE_MERGE);
    if (!name || strlen(name) == 0) {
        printf("  失败: NORMALIZE_MERGE 名称为空\n");
        errors++;
    }

    name = stream_event_type_name(STREAM_EVENT_ERROR);
    if (!name || strlen(name) == 0) {
        printf("  失败: ERROR 名称为空\n");
        errors++;
    }

    /* 未知事件类型的回退 */
    name = stream_event_type_name((StreamEventType) 999);
    if (!name || strcmp(name, "未知事件") != 0) {
        printf("  失败: 未知类型应返回'未知事件', 实际='%s'\n", name ? name : "NULL");
        errors++;
    }

    if (errors == 0)
        printf("  通过: 事件类型名称映射正常\n");
    return errors;
}

/** 测试事件颜色映射 */
static int test_event_colors(void) {
    int errors = 0;

    const char *color;
    color = stream_event_color(STREAM_EVENT_ENGINE_START);
    if (!color || color[0] != '#') {
        printf("  失败: ENGINE_START 颜色格式错误: %s\n", color ? color : "NULL");
        errors++;
    }

    color = stream_event_color(STREAM_EVENT_ERROR);
    if (!color || color[0] != '#') {
        printf("  失败: ERROR 颜色格式错误: %s\n", color ? color : "NULL");
        errors++;
    }

    color = stream_event_color(STREAM_EVENT_WARNING);
    if (!color || color[0] != '#') {
        printf("  失败: WARNING 颜色格式错误: %s\n", color ? color : "NULL");
        errors++;
    }

    /* 未知类型应返回默认颜色 */
    color = stream_event_color((StreamEventType) 999);
    if (!color || color[0] != '#') {
        printf("  失败: 未知类型应返回默认颜色: %s\n", color ? color : "NULL");
        errors++;
    }

    if (errors == 0)
        printf("  通过: 事件颜色映射正常\n");
    return errors;
}

/** 测试多个回调同时触发 */
static int test_multiple_callbacks(void) {
    StreamContext *ctx = stream_context_create();
    if (!ctx)
        return 1;

    CallbackCapture cap1, cap2;
    memset(&cap1, 0, sizeof(cap1));
    memset(&cap2, 0, sizeof(cap2));

    stream_register_callback(ctx, capture_callback, &cap1);
    stream_register_callback(ctx, capture_callback, &cap2);

    StreamEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = STREAM_EVENT_SOLVE_DONE;
    ev.description = "多回调测试";

    stream_emit(ctx, &ev);

    int errors = 0;
    if (cap1.call_count != 1) {
        printf("  失败: 回调1 call_count=%d\n", cap1.call_count);
        errors++;
    }
    if (cap2.call_count != 1) {
        printf("  失败: 回调2 call_count=%d\n", cap2.call_count);
        errors++;
    }

    stream_context_destroy(ctx);

    if (errors == 0)
        printf("  通过: 多回调同时触发正常\n");
    return errors;
}

/** 测试时间戳生成 */
static int test_timestamp(void) {
    long t1 = stream_timestamp_ms();
    if (t1 < 0) {
        printf("  失败: 时间戳为负值: %ld\n", t1);
        return 1;
    }

    /* 验证时间戳单调性（至少不倒退） */
    long t2 = stream_timestamp_ms();
    if (t2 < t1) {
        printf("  失败: 时间戳倒退: t1=%ld, t2=%ld\n", t1, t2);
        return 1;
    }

    printf("  通过: 时间戳生成正常 (t=%ld ms)\n", t2);
    return 0;
}

/* ============== 入口 ============== */

int main(void) {
    printf("=== Lv-00 流式输出系统测试 ===\n\n");

    int total_errors = 0;

    total_errors += test_context_lifecycle();
    total_errors += test_callback_register();
    total_errors += test_event_emit();
    total_errors += test_emit_simple();
    total_errors += test_event_type_names();
    total_errors += test_event_colors();
    total_errors += test_multiple_callbacks();
    total_errors += test_timestamp();

    printf("\n=== 测试结果: %d 个错误 ===\n", total_errors);
    return total_errors > 0 ? 1 : 0;
}
