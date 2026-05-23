/**
 * @file stream_advanced_demo.c
 * @brief Lv-00 流式输出高级演示 —— 四种发射模式 + 惰性求值
 *
 * 本示例演示 Lv-00 流式输出系统的完整功能：
 * 1. 四种发射模式对比（IMMEDIATE / BUFFERED / THROTTLED / LAZY）
 * 2. 事件类型过滤掩码的使用
 * 3. 惰性拉取（stream_lazy_next / stream_lazy_drain）
 * 4. JSON 和 JSON-RPC 序列化
 * 5. 事件统计和丢弃计数
 * 6. 回调 ID 管理和动态过滤更新
 *
 * 构建: cmake --build build --target example_stream_advanced
 * 运行: build\example_stream_advanced.exe
 */

#include "stream.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== 回调捕获结构 ==================== */

typedef struct {
    int call_count;
    int last_type;
    long last_timestamp;
    char last_description[256];
} CallbackCapture;

/* ==================== 回调函数 ==================== */

/** 通用回调：捕获事件信息 */
static void capture_callback(const StreamEvent *event, void *user_data) {
    if (!event || !user_data) return;
    CallbackCapture *cap = (CallbackCapture *)user_data;
    cap->call_count++;
    cap->last_type = (int)event->type;
    cap->last_timestamp = event->timestamp_ms;
    if (event->description) {
        snprintf(cap->last_description, sizeof(cap->last_description), "%s", event->description);
    }
}

/** JSON 输出回调：将事件以 JSON-RPC 格式输出到 stdout */
static void jsonrpc_callback(const StreamEvent *event, void *user_data) {
    (void)user_data;
    if (!event) return;

    char buf[STREAM_JSON_BUFFER_DEFAULT_SIZE + 256];
    int len = stream_event_to_jsonrpc(event, buf, sizeof(buf));
    if (len > 0) {
        printf("%s\n", buf);
        fflush(stdout);
    }
}

/** 错误专用回调：仅捕获 ERROR 和 WARNING 事件 */
static void error_only_callback(const StreamEvent *event, void *user_data) {
    (void)user_data;
    if (!event) return;
    fprintf(stderr, "  [错误监控] 类型=%s 描述=%s\n",
            stream_event_type_name(event->type),
            event->description ? event->description : "(null)");
}

/* ==================== 演示函数 ==================== */

/**
 * 演示1：四种发射模式对比
 */
static void demo_emit_modes(void) {
    fprintf(stderr, "\n========== 演示1: 四种发射模式对比 ==========\n");

    CallbackCapture cap;
    memset(&cap, 0, sizeof(cap));

    /* --- 1.1 IMMEDIATE 模式（默认） --- */
    fprintf(stderr, "\n  [1.1] IMMEDIATE 模式\n");
    {
        StreamContext *ctx = stream_context_create();
        stream_register_callback(ctx, capture_callback, &cap);

        /* 发射3个事件，应立即触发回调 */
        stream_emit_simple(ctx, STREAM_EVENT_ENGINE_START, "引擎启动", 0);
        stream_emit_simple(ctx, STREAM_EVENT_INFO, "正在处理...", 1);
        stream_emit_simple(ctx, STREAM_EVENT_ENGINE_DONE, "引擎完成", 2);

        fprintf(stderr, "    回调触发次数: %d (预期: 3)\n", cap.call_count);
        fprintf(stderr, "    最后事件类型: %s\n", stream_event_type_name((StreamEventType)cap.last_type));

        stream_context_destroy(ctx);
    }

    /* --- 1.2 BUFFERED 模式 --- */
    fprintf(stderr, "\n  [1.2] BUFFERED 模式\n");
    {
        memset(&cap, 0, sizeof(cap));
        StreamContext *ctx = stream_context_create();
        stream_register_callback(ctx, capture_callback, &cap);
        stream_set_emit_mode(ctx, STREAM_EMIT_BUFFERED, 0);

        /* 发射3个事件，回调不应触发 */
        stream_emit_simple(ctx, STREAM_EVENT_NORMALIZE_START, "归一化开始", 0);
        stream_emit_simple(ctx, STREAM_EVENT_NORMALIZE_MERGE, "节点合并", 1);
        stream_emit_simple(ctx, STREAM_EVENT_NORMALIZE_DONE, "归一化完成", 2);

        fprintf(stderr, "    flush 前回调次数: %d (预期: 0)\n", cap.call_count);
        fprintf(stderr, "    缓冲区事件数: %d (预期: 3)\n", stream_pending_count(ctx));

        /* 手动 flush */
        stream_flush(ctx);
        fprintf(stderr, "    flush 后回调次数: %d (预期: 3)\n", cap.call_count);
        fprintf(stderr, "    缓冲区事件数: %d (预期: 0)\n", stream_pending_count(ctx));

        stream_context_destroy(ctx);
    }

    /* --- 1.3 THROTTLED 模式 --- */
    fprintf(stderr, "\n  [1.3] THROTTLED 模式\n");
    {
        memset(&cap, 0, sizeof(cap));
        StreamContext *ctx = stream_context_create();
        stream_register_callback(ctx, capture_callback, &cap);
        /* 设置 100ms 节流间隔 */
        stream_set_emit_mode(ctx, STREAM_EMIT_THROTTLED, 100);

        /* 快速发射多个事件 */
        for (int i = 0; i < 10; i++) {
            stream_emit_simple(ctx, STREAM_EVENT_PROGRESS, "进度更新", i);
        }

        fprintf(stderr, "    发射10个事件后回调次数: %d\n", cap.call_count);
        fprintf(stderr, "    缓冲区事件数: %d\n", stream_pending_count(ctx));

        /* 等待节流间隔后 flush */
        stream_flush(ctx);
        fprintf(stderr, "    flush 后回调次数: %d\n", cap.call_count);

        stream_context_destroy(ctx);
    }

    /* --- 1.4 LAZY 模式 --- */
    fprintf(stderr, "\n  [1.4] LAZY 模式\n");
    {
        memset(&cap, 0, sizeof(cap));
        StreamContext *ctx = stream_context_create();
        stream_set_emit_mode(ctx, STREAM_EMIT_LAZY, 0);

        /* 发射事件，不触发任何回调 */
        stream_emit_simple(ctx, STREAM_EVENT_SOLVE_START, "求解开始", 0);
        stream_emit_simple(ctx, STREAM_EVENT_SOLVE_VARIABLE_RESOLVED, "变量x=3", 1);
        stream_emit_simple(ctx, STREAM_EVENT_SOLVE_DONE, "求解完成", 2);

        fprintf(stderr, "    惰性队列事件数: %d (预期: 3)\n", stream_lazy_pending(ctx));

        /* 使用 lazy_drain 批量拉取 */
        int drained = stream_lazy_drain(ctx, capture_callback, &cap, 0);
        fprintf(stderr, "    lazy_drain 拉取数: %d (预期: 3)\n", drained);
        fprintf(stderr, "    回调触发次数: %d (预期: 3)\n", cap.call_count);
        fprintf(stderr, "    惰性队列剩余: %d (预期: 0)\n", stream_lazy_pending(ctx));

        stream_context_destroy(ctx);
    }

    fprintf(stderr, "\n========== 演示1 完成 ==========\n");
}

/**
 * 演示2：事件类型过滤
 */
static void demo_event_filter(void) {
    fprintf(stderr, "\n========== 演示2: 事件类型过滤 ==========\n");

    CallbackCapture all_cap, error_cap, solve_cap;
    memset(&all_cap, 0, sizeof(all_cap));
    memset(&error_cap, 0, sizeof(error_cap));
    memset(&solve_cap, 0, sizeof(solve_cap));

    StreamContext *ctx = stream_context_create();

    /* 注册3个回调，使用不同的过滤掩码 */
    stream_register_callback(ctx, capture_callback, &all_cap);

    /* 仅接收 ERROR 和 WARNING */
    uint64_t error_mask = STREAM_EVENT_MASK(STREAM_EVENT_ERROR)
                        | STREAM_EVENT_MASK(STREAM_EVENT_WARNING);
    int error_cb_id = stream_register_callback_ex(ctx, error_only_callback, NULL, error_mask);

    /* 仅接收求解相关事件 */
    uint64_t solve_mask = STREAM_EVENT_MASK(STREAM_EVENT_SOLVE_START)
                        | STREAM_EVENT_MASK(STREAM_EVENT_SOLVE_VARIABLE_RESOLVED)
                        | STREAM_EVENT_MASK(STREAM_EVENT_SOLVE_DONE);
    stream_register_callback_ex(ctx, capture_callback, &solve_cap, solve_mask);

    /* 发射混合事件 */
    stream_emit_simple(ctx, STREAM_EVENT_ENGINE_START, "引擎启动", 0);
    stream_emit_simple(ctx, STREAM_EVENT_SOLVE_START, "求解开始", 1);
    stream_emit_simple(ctx, STREAM_EVENT_WARNING, "警告: 接近超时", 2);
    stream_emit_simple(ctx, STREAM_EVENT_SOLVE_VARIABLE_RESOLVED, "x=3", 3);
    stream_emit_simple(ctx, STREAM_EVENT_ERROR, "错误: 冲突", 4);
    stream_emit_simple(ctx, STREAM_EVENT_SOLVE_DONE, "求解完成", 5);

    fprintf(stderr, "  全量回调触发: %d (预期: 6)\n", all_cap.call_count);
    fprintf(stderr, "  求解回调触发: %d (预期: 3)\n", solve_cap.call_count);

    /* 动态更新过滤掩码：将错误回调改为仅接收 WARNING */
    stream_set_callback_filter(ctx, error_cb_id, STREAM_EVENT_MASK(STREAM_EVENT_WARNING));
    fprintf(stderr, "  已更新错误回调过滤为仅 WARNING\n");

    stream_context_destroy(ctx);

    fprintf(stderr, "========== 演示2 完成 ==========\n");
}

/**
 * 演示3：字符串过滤掩码解析
 */
static void demo_filter_parsing(void) {
    fprintf(stderr, "\n========== 演示3: 过滤掩码解析 ==========\n");

    struct { const char *input; const char *desc; } cases[] = {
        {"all",        "接收全部事件"},
        {"*",          "通配符接收全部"},
        {"none",       "不接收任何事件"},
        {"ENGINE_START", "仅引擎启动"},
        {"engine",     "引擎生命周期事件"},
        {"solve",      "求解相关事件"},
        {"engine,solve", "引擎+求解事件"},
        {"ENGINE_START,ENGINE_DONE,ERROR", "混合指定事件"},
        {"rewrite,proof", "重写+证明事件"},
        {"func_block", "函数块事件"},
        {"normalize",  "归一化事件"},
    };

    for (int i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); i++) {
        uint64_t mask = stream_parse_filter_mask(cases[i].input);
        fprintf(stderr, "  \"%-40s\" => mask=0x%016llX  (%s)\n",
                cases[i].input, (unsigned long long)mask, cases[i].desc);
    }

    fprintf(stderr, "========== 演示3 完成 ==========\n");
}

/**
 * 演示4：事件统计
 */
static void demo_event_stats(void) {
    fprintf(stderr, "\n========== 演示4: 事件统计 ==========\n");

    StreamContext *ctx = stream_context_create();
    stream_register_callback(ctx, capture_callback, NULL);

    /* 发射各种类型的事件 */
    stream_emit_simple(ctx, STREAM_EVENT_ENGINE_START, "启动", 0);
    stream_emit_simple(ctx, STREAM_EVENT_NODE_ADDED, "添加节点", 1);
    stream_emit_simple(ctx, STREAM_EVENT_NODE_ADDED, "添加节点", 2);
    stream_emit_simple(ctx, STREAM_EVENT_NODE_ADDED, "添加节点", 3);
    stream_emit_simple(ctx, STREAM_EVENT_CONSTRAINT_ADDED, "添加约束", 4);
    stream_emit_simple(ctx, STREAM_EVENT_CONSTRAINT_ADDED, "添加约束", 5);
    stream_emit_simple(ctx, STREAM_EVENT_NORMALIZE_START, "归一化开始", 6);
    stream_emit_simple(ctx, STREAM_EVENT_NORMALIZE_MERGE, "合并", 7);
    stream_emit_simple(ctx, STREAM_EVENT_NORMALIZE_DONE, "归一化完成", 8);
    stream_emit_simple(ctx, STREAM_EVENT_SOLVE_START, "求解开始", 9);
    stream_emit_simple(ctx, STREAM_EVENT_SOLVE_VARIABLE_RESOLVED, "x=3", 10);
    stream_emit_simple(ctx, STREAM_EVENT_SOLVE_DONE, "求解完成", 11);
    stream_emit_simple(ctx, STREAM_EVENT_ERROR, "错误", 12);
    stream_emit_simple(ctx, STREAM_EVENT_ENGINE_DONE, "完成", 13);

    fprintf(stderr, "  总事件数: %ld\n", stream_get_total_event_count(ctx));
    fprintf(stderr, "  丢弃事件数: %ld\n", stream_get_dropped_count(ctx));

    /* 按类型统计 */
    fprintf(stderr, "  按类型统计:\n");
    const StreamEventType stat_types[] = {
        STREAM_EVENT_ENGINE_START, STREAM_EVENT_ENGINE_DONE,
        STREAM_EVENT_NODE_ADDED, STREAM_EVENT_CONSTRAINT_ADDED,
        STREAM_EVENT_NORMALIZE_START, STREAM_EVENT_NORMALIZE_MERGE,
        STREAM_EVENT_NORMALIZE_DONE,
        STREAM_EVENT_SOLVE_START, STREAM_EVENT_SOLVE_VARIABLE_RESOLVED,
        STREAM_EVENT_SOLVE_DONE, STREAM_EVENT_ERROR
    };
    for (int i = 0; i < (int)(sizeof(stat_types) / sizeof(stat_types[0])); i++) {
        long count = stream_get_event_count(ctx, stat_types[i]);
        if (count > 0) {
            fprintf(stderr, "    %-30s: %ld\n",
                    stream_event_type_name(stat_types[i]), count);
        }
    }

    /* 重置统计 */
    stream_reset_stats(ctx);
    fprintf(stderr, "  重置后总事件数: %ld (预期: 0)\n", stream_get_total_event_count(ctx));

    stream_context_destroy(ctx);

    fprintf(stderr, "========== 演示4 完成 ==========\n");
}

/**
 * 演示5：JSON 序列化
 */
static void demo_json_serialization(void) {
    fprintf(stderr, "\n========== 演示5: JSON 序列化 ==========\n");

    StreamEvent event;
    memset(&event, 0, sizeof(event));
    event.type = STREAM_EVENT_SOLVE_VARIABLE_RESOLVED;
    event.timestamp_ms = stream_timestamp_ms();
    event.step_number = 5;
    event.total_steps = 10;
    event.var_id = 42;
    event.description = "变量 x 求解成功";
    event.numeric_value = 3.14159;
    event.progress = 0.5;

    /* JSON 序列化 */
    char json_buf[STREAM_JSON_BUFFER_DEFAULT_SIZE];
    int json_len = stream_event_to_json(&event, json_buf, sizeof(json_buf));
    fprintf(stderr, "  JSON 输出 (%d 字节):\n%s\n", json_len, json_buf);

    /* JSON-RPC 序列化 */
    char rpc_buf[STREAM_JSON_BUFFER_DEFAULT_SIZE + 128];
    int rpc_len = stream_event_to_jsonrpc(&event, rpc_buf, sizeof(rpc_buf));
    fprintf(stderr, "  JSON-RPC 输出 (%d 字节):\n%s\n", rpc_len, rpc_buf);

    /* 特殊字符转义测试 */
    StreamEvent escape_event;
    memset(&escape_event, 0, sizeof(escape_event));
    escape_event.type = STREAM_EVENT_INFO;
    escape_event.timestamp_ms = stream_timestamp_ms();
    escape_event.description = "包含\"引号\"和\\反斜杠\\及\n换行";

    char escape_buf[1024];
    stream_event_to_json(&escape_event, escape_buf, sizeof(escape_buf));
    fprintf(stderr, "  转义测试:\n%s\n", escape_buf);

    fprintf(stderr, "========== 演示5 完成 ==========\n");
}

/**
 * 演示6：惰性模式高级用法
 */
static void demo_lazy_advanced(void) {
    fprintf(stderr, "\n========== 演示6: 惰性模式高级用法 ==========\n");

    StreamContext *ctx = stream_context_create();
    stream_set_emit_mode(ctx, STREAM_EMIT_LAZY, 0);

    /* 设置自动刷新阈值为 100 */
    stream_set_lazy_threshold(ctx, 100);

    /* 发射大量事件 */
    for (int i = 0; i < 50; i++) {
        stream_emit_simple(ctx, STREAM_EVENT_PROGRESS, "进度", i);
    }

    fprintf(stderr, "  发射50个事件后:\n");
    fprintf(stderr, "    惰性队列: %d (预期: 50)\n", stream_lazy_pending(ctx));

    /* 逐个拉取前5个 */
    int pulled = 0;
    const StreamEvent *ev;
    while ((ev = stream_lazy_next(ctx)) != NULL && pulled < 5) {
        pulled++;
    }
    fprintf(stderr, "    拉取5个后队列: %d (预期: 45)\n", stream_lazy_pending(ctx));

    /* 批量拉取剩余 */
    CallbackCapture cap;
    memset(&cap, 0, sizeof(cap));
    int drained = stream_lazy_drain(ctx, capture_callback, &cap, 0);
    fprintf(stderr, "    批量拉取: %d (预期: 45)\n", drained);
    fprintf(stderr, "    回调触发: %d\n", cap.call_count);

    stream_context_destroy(ctx);

    fprintf(stderr, "========== 演示6 完成 ==========\n");
}

/**
 * 演示7：便捷发射函数
 */
static void demo_emit_helpers(void) {
    fprintf(stderr, "\n========== 演示7: 便捷发射函数 ==========\n");

    CallbackCapture cap;
    memset(&cap, 0, sizeof(cap));

    StreamContext *ctx = stream_context_create();
    stream_register_callback(ctx, capture_callback, &cap);

    /* 使用各种便捷函数 */
    stream_emit_node_event(ctx, STREAM_EVENT_NODE_ADDED, 42, "添加点节点", 1);
    stream_emit_constraint_event(ctx, STREAM_EVENT_CONSTRAINT_ADDED, 7, "添加线段约束", 2);
    stream_emit_progress(ctx, 0.75, "处理进度", 3, 4);
    stream_emit_numeric(ctx, STREAM_EVENT_SOLVE_VARIABLE_RESOLVED, 3.14, "x=3.14", 4);
    stream_emit_merge(ctx, 10, 20, 5);
    stream_emit_variable_resolved(ctx, 1, 42.0, "变量1=42", 6);
    stream_emit_error(ctx, "发生错误", 7);
    stream_emit_warning(ctx, "警告信息", 8);
    stream_emit_info(ctx, "一般信息", 9);

    fprintf(stderr, "  便捷函数回调触发次数: %d (预期: 9)\n", cap.call_count);

    /* 验证最后一个事件 */
    fprintf(stderr, "  最后事件: type=%s desc=\"%s\"\n",
            stream_event_type_name((StreamEventType)cap.last_type),
            cap.last_description);

    stream_context_destroy(ctx);

    fprintf(stderr, "========== 演示7 完成 ==========\n");
}

/**
 * 演示8：回调 ID 管理
 */
static void demo_callback_management(void) {
    fprintf(stderr, "\n========== 演示8: 回调 ID 管理 ==========\n");

    CallbackCapture cap1, cap2, cap3;
    memset(&cap1, 0, sizeof(cap1));
    memset(&cap2, 0, sizeof(cap2));
    memset(&cap3, 0, sizeof(cap3));

    StreamContext *ctx = stream_context_create();

    /* 注册3个回调，获取 ID */
    int id1 = stream_register_callback_ex(ctx, capture_callback, &cap1, STREAM_FILTER_ALL);
    int id2 = stream_register_callback_ex(ctx, capture_callback, &cap2, STREAM_FILTER_ALL);
    int id3 = stream_register_callback_ex(ctx, capture_callback, &cap3, STREAM_FILTER_ALL);

    fprintf(stderr, "  注册回调 ID: %d, %d, %d\n", id1, id2, id3);

    /* 发射事件 */
    stream_emit_simple(ctx, STREAM_EVENT_INFO, "测试事件", 0);
    fprintf(stderr, "  3个回调各触发1次: cap1=%d cap2=%d cap3=%d\n",
            cap1.call_count, cap2.call_count, cap3.call_count);

    /* 通过 ID 注销第2个回调 */
    stream_unregister_callback_by_id(ctx, id2);
    memset(&cap1, 0, sizeof(cap1));
    memset(&cap2, 0, sizeof(cap2));
    memset(&cap3, 0, sizeof(cap3));

    stream_emit_simple(ctx, STREAM_EVENT_INFO, "注销后测试", 1);
    fprintf(stderr, "  注销id2后: cap1=%d cap2=%d(预期:0) cap3=%d\n",
            cap1.call_count, cap2.call_count, cap3.call_count);

    /* 获取过滤掩码 */
    uint64_t mask = stream_get_callback_filter(ctx, id1);
    fprintf(stderr, "  id1 过滤掩码: 0x%016llX (预期: ALL)\n", (unsigned long long)mask);

    /* 获取不存在的 ID 的过滤掩码 */
    uint64_t invalid_mask = stream_get_callback_filter(ctx, 999);
    fprintf(stderr, "  无效ID过滤掩码: 0x%016llX (预期: NONE)\n", (unsigned long long)invalid_mask);

    stream_context_destroy(ctx);

    fprintf(stderr, "========== 演示8 完成 ==========\n");
}

/* ==================== 主程序 ==================== */

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    fprintf(stderr, "╔════════════════════════════════════════════════╗\n");
    fprintf(stderr, "║  Lv-00 流式输出高级演示 v3.1.0                 ║\n");
    fprintf(stderr, "║  四种发射模式 + 惰性求值 + 过滤 + 序列化       ║\n");
    fprintf(stderr, "╚════════════════════════════════════════════════╝\n");

    demo_emit_modes();
    demo_event_filter();
    demo_filter_parsing();
    demo_event_stats();
    demo_json_serialization();
    demo_lazy_advanced();
    demo_emit_helpers();
    demo_callback_management();

    fprintf(stderr, "\n所有演示完成。\n");
    return 0;
}
