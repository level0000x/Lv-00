/**
 * @file stream_emit.c
 * @brief 流式输出系统 —— 便捷发射与发射模式
 */

#include "stream_internal.h"


/* ==================== 便捷发射函数 ==================== */

/**
 * @brief 初始化 StreamEvent 的公共字段（内部辅助函数）
 *
 * 消除 7 个便捷发射函数中重复的 memset + 默认值设置代码。
 * 所有便捷发射函数共享相同的初始化模式：清零、设置时间戳、
 * 将未使用的整数字段设为 -1、将 progress 设为 -1.0。
 *
 * @param event       待初始化的事件指针（调用者负责分配）
 * @param type        事件类型
 * @param description 事件描述字符串
 * @param step_number 当前步骤编号
 */
static inline void stream_event_init(StreamEvent *event, StreamEventType type, const char *description,
                                     int step_number) {
    memset(event, 0, sizeof(*event));
    event->type = type;
    event->timestamp_ms = stream_timestamp_ms();
    event->step_number = step_number;
    event->description = description;
    /* 未使用的字段设为无效默认值，便于消费者区分"未设置"和"值为0" */
    event->node_id = -1;
    event->constraint_id = -1;
    event->rule_id = -1;
    event->var_id = -1;
    event->total_steps = -1;
    event->progress = -1.0;
}

void stream_emit_simple(StreamContext *ctx, StreamEventType type, const char *description, int step_number) {
    if (!ctx)
        return;
    StreamEvent event;
    stream_event_init(&event, type, description, step_number);
    stream_emit(ctx, &event);
}

/**
 * @brief 发射节点相关事件
 *
 * 便捷函数，用于发射与特定节点相关的事件（如节点添加、合并等）。
 *
 * @param ctx         流式上下文
 * @param type        事件类型
 * @param node_id     相关节点 ID
 * @param description 事件描述
 * @param step_number 步骤编号
 */
void stream_emit_node_event(StreamContext *ctx, StreamEventType type, int node_id, const char *description,
                            int step_number) {
    if (!ctx)
        return;
    StreamEvent event;
    stream_event_init(&event, type, description, step_number);
    event.node_id = node_id;
    stream_emit(ctx, &event);
}

/**
 * @brief 发射约束相关事件
 *
 * 便捷函数，用于发射与特定约束相关的事件。
 *
 * @param ctx           流式上下文
 * @param type          事件类型
 * @param constraint_id 相关约束 ID
 * @param description   事件描述
 * @param step_number   步骤编号
 */
void stream_emit_constraint_event(StreamContext *ctx, StreamEventType type, int constraint_id, const char *description,
                                  int step_number) {
    if (!ctx)
        return;
    StreamEvent event;
    stream_event_init(&event, type, description, step_number);
    event.constraint_id = constraint_id;
    stream_emit(ctx, &event);
}

/**
 * @brief 发射进度事件
 *
 * 便捷函数，用于发射进度更新事件。
 *
 * @param ctx         流式上下文
 * @param progress    进度值 (0.0 ~ 1.0)
 * @param description 事件描述
 * @param step_number 当前步骤
 * @param total_steps 总步骤数
 */
void stream_emit_progress(StreamContext *ctx, double progress, const char *description, int step_number,
                          int total_steps) {
    if (!ctx)
        return;
    StreamEvent event;
    stream_event_init(&event, STREAM_EVENT_PROGRESS, description, step_number);
    event.total_steps = total_steps;
    event.progress = progress;
    stream_emit(ctx, &event);
}

/**
 * @brief 发射带数值结果的事件
 *
 * 便捷函数，用于发射包含数值计算结果的事件（如求解结果）。
 *
 * @param ctx           流式上下文
 * @param type          事件类型
 * @param numeric_value 数值结果
 * @param description   事件描述
 * @param step_number   步骤编号
 */
void stream_emit_numeric(StreamContext *ctx, StreamEventType type, double numeric_value, const char *description,
                         int step_number) {
    if (!ctx)
        return;
    StreamEvent event;
    stream_event_init(&event, type, description, step_number);
    event.numeric_value = numeric_value;
    stream_emit(ctx, &event);
}

/**
 * @brief 发射带图快照的事件
 *
 * 便捷函数，用于发射包含图数据快照的事件（用于前端同步）。
 *
 * @param ctx         流式上下文
 * @param type        事件类型
 * @param graph_json  图数据的 JSON 字符串
 * @param description 事件描述
 * @param step_number 步骤编号
 */
void stream_emit_graph_snapshot(StreamContext *ctx, StreamEventType type, const char *graph_json,
                                const char *description, int step_number) {
    if (!ctx)
        return;
    StreamEvent event;
    stream_event_init(&event, type, description, step_number);
    event.graph_json = graph_json;
    stream_emit(ctx, &event);
}

/**
 * @brief 发射合并事件
 *
 * 便捷函数，用于发射节点合并事件（归一化阶段）。
 *
 * @param ctx         流式上下文
 * @param from_id     被合并的节点 ID
 * @param to_id       保留的节点 ID
 * @param step_number 步骤编号
 */
void stream_emit_merge(StreamContext *ctx, int from_id, int to_id, int step_number) {
    if (!ctx)
        return;

    StreamEvent event;
    stream_event_init(&event, STREAM_EVENT_NORMALIZE_MERGE, NULL, step_number);
    event.node_id = to_id;
    event.constraint_id = from_id; /* 复用字段存储 from_id */

    /* 构建描述字符串（使用线程局部 scratch 缓冲区，避免堆分配导致悬空指针风险）
     * 注意：之前使用 lv_malloc 分配堆内存，在 BUFFERED/THROTTLED/LAZY 模式下
     *       stream_emit 会缓冲事件引用，释放 desc_buf 后 description 变为悬空指针。
     *       改用 lv_fmt_tmp（lv_utils.h 的 TLS scratch 缓冲区），彻底消除此风险。 */
    event.description = lv_fmt_tmp("节点合并: %d → %d", from_id, to_id);

    stream_emit(ctx, &event);
}

/**
 * @brief 发射求解变量事件
 *
 * 便捷函数，用于发射变量求解成功事件。
 *
 * @param ctx         流式上下文
 * @param var_id      变量 ID
 * @param value       求解值
 * @param description 事件描述
 * @param step_number 步骤编号
 */
void stream_emit_variable_resolved(StreamContext *ctx, int var_id, double value, const char *description,
                                   int step_number) {
    if (!ctx)
        return;
    StreamEvent event;
    stream_event_init(&event, STREAM_EVENT_SOLVE_VARIABLE_RESOLVED, description, step_number);
    event.var_id = var_id;
    event.numeric_value = value;
    stream_emit(ctx, &event);
}

/**
 * @brief 发射错误事件
 *
 * 便捷函数，用于发射错误事件。
 *
 * @param ctx         流式上下文
 * @param description 错误描述
 * @param step_number 步骤编号
 */
void stream_emit_error(StreamContext *ctx, const char *description, int step_number) {
    stream_emit_simple(ctx, STREAM_EVENT_ERROR, description, step_number);
}

/**
 * @brief 发射警告事件
 *
 * 便捷函数，用于发射警告事件。
 *
 * @param ctx         流式上下文
 * @param description 警告描述
 * @param step_number 步骤编号
 */
void stream_emit_warning(StreamContext *ctx, const char *description, int step_number) {
    stream_emit_simple(ctx, STREAM_EVENT_WARNING, description, step_number);
}

/**
 * @brief 发射信息事件
 *
 * 便捷函数，用于发射一般信息事件。
 *
 * @param ctx         流式上下文
 * @param description 信息描述
 * @param step_number 步骤编号
 */
void stream_emit_info(StreamContext *ctx, const char *description, int step_number) {
    stream_emit_simple(ctx, STREAM_EVENT_INFO, description, step_number);
}

/* ==================== 预设函数块便捷发射 API ==================== */

/**
 * 发射预设注册事件。
 *
 * 根据注册结果自动选择 PRESET_REGISTER_DONE 或 PRESET_REGISTER_FAILED 事件类型，
 * 并在描述中包含预设名称和结果信息。
 */
void stream_emit_preset_register(StreamContext *ctx, const char *name, bool success, int step_number) {
    if (!ctx || !name)
        return;

    StreamEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = success ? STREAM_EVENT_PRESET_REGISTER_DONE : STREAM_EVENT_PRESET_REGISTER_FAILED;
    ev.timestamp_ms = stream_timestamp_ms();
    ev.step_number = step_number;

    /* 构造描述文本：使用线程局部 scratch 缓冲区，避免局部数组在 BUFFERED/THROTTLED/LAZY 模式下悬空 */
    ev.description = lv_fmt_tmp("预设 '%s' 注册%s", name, success ? "成功" : "失败");

    stream_emit(ctx, &ev);
}

/**
 * 发射预设实例化事件。
 *
 * 发射 PRESET_INSTANTIATE 事件，附带预设名称和实例 ID。
 */
void stream_emit_preset_instantiate(StreamContext *ctx, const char *name, int instance_id, int step_number) {
    if (!ctx || !name)
        return;

    StreamEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = STREAM_EVENT_PRESET_INSTANTIATE;
    ev.timestamp_ms = stream_timestamp_ms();
    ev.step_number = step_number;
    ev.node_id = instance_id; /* 复用 node_id 字段存储实例 ID */

    ev.description = lv_fmt_tmp("预设 '%s' 实例化 (ID=%d)", name, instance_id);

    stream_emit(ctx, &ev);
}

/**
 * 发射预设验证事件。
 *
 * 发射 PRESET_VALIDATE 事件，附带验证结果和详情。
 */
void stream_emit_preset_validate(StreamContext *ctx, const char *name, bool is_valid, const char *detail,
                                 int step_number) {
    if (!ctx || !name)
        return;

    StreamEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = STREAM_EVENT_PRESET_VALIDATE;
    ev.timestamp_ms = stream_timestamp_ms();
    ev.step_number = step_number;
    ev.detail_json = detail; /* 复用 detail_json 字段存储验证详情 */

    ev.description = lv_fmt_tmp("预设 '%s' 验证%s", name, is_valid ? "通过" : "失败");

    stream_emit(ctx, &ev);
}

/**
 * 发射预设模块加载完成事件。
 *
 * 发射 PRESET_MODULE_LOADED 事件，附带模块名称和注册数量。
 */
void stream_emit_preset_module_loaded(StreamContext *ctx, const char *module_name, int count, int step_number) {
    if (!ctx || !module_name)
        return;

    StreamEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = STREAM_EVENT_PRESET_MODULE_LOADED;
    ev.timestamp_ms = stream_timestamp_ms();
    ev.step_number = step_number;
    ev.numeric_value = (double) count; /* 复用 numeric_value 存储注册数量 */

    ev.description = lv_fmt_tmp("模块 '%s' 加载完成，共 %d 个预设", module_name, count);

    stream_emit(ctx, &ev);
}

/* ==================== 发射模式 API ==================== */

/**
 * 设置事件发射模式。
 *
 * @param ctx          流式上下文
 * @param mode         发射模式（IMMEDIATE / BUFFERED / THROTTLED）
 * @param throttle_ms  节流间隔毫秒数（仅 THROTTLED 模式生效，0 使用默认值 50ms）
 */
void stream_set_emit_mode(StreamContext *ctx, StreamEmitMode mode, long throttle_ms) {
    if (!ctx)
        return;

    /* 异步模式优先：如果已启用异步，不允许切换发射模式 */
    if (ctx->async_enabled) {
        /* 异步模式下忽略发射模式切换，保持 BUFFERED */
        return;
    }

    ctx->emit_mode = mode;
    if (throttle_ms > 0) {
        ctx->throttle_ms = throttle_ms;
    } else if (mode == STREAM_EMIT_THROTTLED) {
        ctx->throttle_ms = STREAM_DEFAULT_THROTTLE;
    }
    /* 切换模式时，如果从缓冲/节流切到立即模式，自动刷新残留事件 */
    if (mode == STREAM_EMIT_IMMEDIATE && ctx->buffer_count > 0) {
        stream_flush(ctx);
    }
    /* 从惰性模式切换出来时，清空惰性队列 */
    if (mode != STREAM_EMIT_LAZY && ctx->lazy_count > 0) {
        ctx->lazy_head = 0;
        ctx->lazy_count = 0;
    }
}

/**
 * 获取当前事件发射模式。
 *
 * @param ctx 流式上下文
 * @return 当前发射模式，ctx 为 NULL 时返回 STREAM_EMIT_IMMEDIATE
 */
StreamEmitMode stream_get_emit_mode(const StreamContext *ctx) {
    if (!ctx)
        return STREAM_EMIT_IMMEDIATE;
    return ctx->emit_mode;
}

