/**
 * @file stream.c
 * @brief 流式输出系统实现 —— 引擎事件回调与实时状态推送
 *
 * 提供流式事件发射、回调注册、事件过滤、JSON 序列化、事件统计等核心功能。
 * 支撑 Web 前端实时可视化和证明步骤动画渲染。
 *
 * 功能模块:
 *   - 生命周期管理: 创建/销毁流式上下文
 *   - 回调管理: 注册/注销回调，支持事件类型过滤掩码
 *   - 事件发射: 立即/缓冲/节流/惰性四种模式
 *   - 惰性求值: 消费者主动拉取模式，阈值自动刷新
 *   - 异步模式: 基于环形缓冲区的缓冲队列（纯 C 无线程依赖）
 *   - JSON 序列化: 手工拼接 JSON / JSON-RPC 字符串
 *   - 事件统计: 按类型计数、总数、丢弃数
 *   - 工具函数: 时间戳、事件类型名称/颜色/标识符、过滤掩码解析
 *
 * 该文件为全量重构版本：原文件因编码损坏导致部分注释和逻辑丢失，
 * 于 2026-05-20 基于头文件声明和功能规格重新实现，并通过回归测试验证。
 *
 * @author Lv-00 Project
 * @version 3.2.0  (惰性求值完整实现 2026-05-23)
 *
 * @dependencies
 *   - stream.h              : 流式输出系统公共接口定义
 *   - lv00_utils.h          : 统一内存分配器
 *
 * @note 本模块无外部依赖（除 lv00_utils），仅依赖标准 C 库。
 *       所有平台相关代码通过 #ifdef 隔离（Windows: windows.h/timeGetTime，
 *       类 Unix: sys/time.h/strings.h）。
 */

#include "stream.h"
#include "lv00_utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#ifdef _WIN32
#include <windows.h>
#define strcasecmp _stricmp
#define strtok_r strtok_s
#else
#include <sys/time.h>   /* gettimeofday：高精度墙上时钟（非处理器时间） */
#include <strings.h>    /* strcasecmp：不区分大小写的字符串比较 */
#endif

/* ==================== 内部常量 ==================== */

/* ── 回调容量配置 ── */
#define STREAM_INITIAL_CALLBACKS 16   /**< 初始回调容量 */
#define STREAM_MAX_CALLBACKS     64   /**< 硬上限：防止无限内存消耗 */

/* ── 事件缓冲区配置 ── */
#define STREAM_INITIAL_BUFFER    64   /**< 初始事件缓冲区容量 */
#define STREAM_MAX_BUFFER       4096  /**< 硬上限：缓冲区最大事件数 */
#define STREAM_MAX_LAZY         8192  /**< 惰性队列最大容量 */
#define STREAM_DEFAULT_THROTTLE  50   /**< 默认节流间隔（毫秒） */

/* ── JSON 序列化配置 ── */
#define STREAM_JSON_INT_BUF      64   /**< 整数转字符串的临时缓冲区大小 */

/* ==================== 事件颜色常量 ==================== */

#define STREAM_COLOR_GREEN      "#3fb950"  /**< 绿色：成功/开始/完成 */
#define STREAM_COLOR_RED        "#f85149"  /**< 红色：错误 */
#define STREAM_COLOR_YELLOW     "#d29922"  /**< 黄色：警告 */
#define STREAM_COLOR_ORANGE     "#f0883e"  /**< 橙色：位数熔断 */
#define STREAM_COLOR_BLUE       "#58a6ff"  /**< 蓝色：进度 */
#define STREAM_COLOR_GRAY       "#8b949e"  /**< 灰色：一般信息 */
#define STREAM_COLOR_LIGHT_GRAY "#c9d1d9"  /**< 浅灰：图快照/默认 */
#define STREAM_COLOR_PURPLE     "#a371f7"  /**< 紫色：重写/求解/证明步骤 */
#define STREAM_COLOR_TEAL       "#39d353"  /**< 青绿色：函数块系统 */
#define STREAM_COLOR_CYAN       "#56d4dd"  /**< 青色：递归系统 */
#define STREAM_COLOR_PINK       "#f778ba"  /**< 粉色：选择器分支 */

/* ==================== 数据结构 ==================== */

/**
 * @brief 回调记录
 *
 * 每个注册的回调对应一条记录，包含回调函数指针、用户数据、
 * 自增回调 ID 和事件类型过滤掩码。
 */
typedef struct {
    StreamCallback callback;  /**< 回调函数指针 */
    void *user_data;          /**< 回调透传数据 */
    int id;                   /**< 自增回调 ID（>= 1），用于按 ID 注销和更新过滤 */
    uint64_t filter_mask;     /**< 事件类型过滤掩码（位与运算） */
} CallbackEntry;

/**
 * @brief 流式上下文
 *
 * 回调数组支持动态扩容：初始容量 STREAM_INITIAL_CALLBACKS，
 * 最多扩容到 STREAM_MAX_CALLBACKS。超过硬上限后注册会失败。
 *
 * 事件缓冲区用于 BUFFERED 和 THROTTLED 模式：
 * - BUFFERED: 事件入队，等待 stream_flush() 手动刷新
 * - THROTTLED: 事件入队，按时间间隔自动刷新
 *
 * 事件统计数组记录每种事件类型的发射次数。
 */
struct StreamContext {
    CallbackEntry *callbacks;   /**< 已注册回调数组（堆分配，支持动态扩容） */
    int callback_count;         /**< 当前回调数量 */
    int callback_capacity;      /**< 当前数组容量 */

    /* ── 事件缓冲 / 发射策略 ── */
    StreamEmitMode emit_mode;   /**< 当前发射策略 */
    long throttle_ms;           /**< 节流间隔（毫秒） */
    StreamEvent *buffer;        /**< 事件缓冲区（环形队列） */
    int buffer_count;           /**< 缓冲区中当前事件数 */
    int buffer_capacity;        /**< 缓冲区容量 */
    int buffer_head;            /**< 缓冲区读头（flush 位置） */
    long last_emit_ms;          /**< 上次发射时间戳（节流用） */

    /* ── 回调 ID 管理 ── */
    int next_callback_id;       /**< 下一个可用的回调 ID（自增，从 1 开始） */

    /* ── 事件统计 ── */
    long event_counts[STREAM_EVENT_TYPE_COUNT]; /**< 各事件类型发射计数 */
    long total_count;           /**< 事件发射总数 */
    long dropped_count;         /**< 丢弃的事件数（缓冲区满时） */

    /* ── 惰性队列（LAZY 模式专用） ── */
    StreamEvent *lazy_queue;    /**< 惰性事件队列（环形缓冲区） */
    int lazy_count;             /**< 惰性队列中当前事件数 */
    int lazy_capacity;          /**< 惰性队列容量 */
    int lazy_head;              /**< 惰性队列读头 */
    int lazy_threshold;         /**< 惰性自动刷新阈值（0=禁用） */
};

/* ==================== 内部辅助函数（前向声明） ==================== */

static bool stream_ensure_capacity(StreamContext *ctx, int min_capacity);
static bool stream_ensure_buffer(StreamContext *ctx);
static void stream_buffer_push(StreamContext *ctx, const StreamEvent *event);
static void stream_dispatch(StreamContext *ctx, const StreamEvent *event);
static bool stream_throttle_expired(StreamContext *ctx);
static void stream_update_stats(StreamContext *ctx, const StreamEvent *event);
static bool stream_lazy_ensure_capacity(StreamContext *ctx);
static void stream_lazy_enqueue(StreamContext *ctx, const StreamEvent *event);

/* ==================== 生命周期 ==================== */

/**
 * @brief 创建流式上下文
 *
 * 分配并初始化 StreamContext，预分配 STREAM_INITIAL_CALLBACKS 容量的回调数组。
 * @return 新上下文指针，内存不足返回 NULL
 */
StreamContext *stream_context_create(void) {
    StreamContext *ctx = (StreamContext *)lv00_malloc(sizeof(StreamContext));
    if (!ctx) return NULL;
    memset(ctx, 0, sizeof(StreamContext));

    /* 预分配初始容量的回调数组 */
    ctx->callbacks = (CallbackEntry *)lv00_malloc(
        sizeof(CallbackEntry) * STREAM_INITIAL_CALLBACKS);
    if (!ctx->callbacks) {
        lv00_free((void **)&ctx);
        return NULL;
    }
    ctx->callback_capacity = STREAM_INITIAL_CALLBACKS;

    /* 初始化发射策略：默认立即发射模式 */
    ctx->emit_mode = STREAM_EMIT_IMMEDIATE;
    ctx->throttle_ms = STREAM_DEFAULT_THROTTLE;
    ctx->buffer = NULL;
    ctx->buffer_count = 0;
    ctx->buffer_capacity = 0;
    ctx->buffer_head = 0;
    ctx->last_emit_ms = 0;

    /* 初始化回调 ID 计数器 */
    ctx->next_callback_id = 1;

    /* 初始化事件统计 */
    memset(ctx->event_counts, 0, sizeof(ctx->event_counts));
    ctx->total_count = 0;
    ctx->dropped_count = 0;

    /* 初始化惰性队列 */
    ctx->lazy_queue = NULL;
    ctx->lazy_count = 0;
    ctx->lazy_capacity = 0;
    ctx->lazy_head = 0;
    ctx->lazy_threshold = 0;

    return ctx;
}

/**
 * @brief 销毁流式上下文
 *
 * 释放 StreamContext 及其所有资源（包括动态分配的回调数组和事件缓冲区）。
 * 传入 NULL 安全返回。
 * @param ctx 流式上下文指针
 */
void stream_context_destroy(StreamContext *ctx) {
    if (!ctx) return;
    /* 释放事件缓冲区 */
    if (ctx->buffer) {
        /* 释放缓冲区中事件的动态内存（description/detail_json 为外部指针，不释放） */
        lv00_free((void **)&ctx->buffer);
    }
    /* 释放惰性队列 */
    if (ctx->lazy_queue) {
        lv00_free((void **)&ctx->lazy_queue);
    }
    lv00_free((void **)&ctx->callbacks);
    lv00_free((void **)&ctx);
}

/**
 * @brief 确保回调数组有足够容量（动态扩容）
 *
 * @param ctx        流式上下文
 * @param min_capacity 所需的最小容量
 * @return true 容量足够或扩容成功，false 扩容失败或在硬上限
 */
static bool stream_ensure_capacity(StreamContext *ctx, int min_capacity) {
    if (min_capacity <= ctx->callback_capacity) return true;
    if (min_capacity > STREAM_MAX_CALLBACKS) return false;  /* 超过硬上限 */

    int new_cap = ctx->callback_capacity;
    while (new_cap < min_capacity) {
        if (new_cap > STREAM_MAX_CALLBACKS / 2) {
            new_cap = STREAM_MAX_CALLBACKS;
            break;
        }
        new_cap *= 2;
    }
    /* 钳制到硬上限 */
    if (new_cap > STREAM_MAX_CALLBACKS) new_cap = STREAM_MAX_CALLBACKS;

    CallbackEntry *new_arr = (CallbackEntry *)lv00_realloc(
        ctx->callbacks, (size_t)new_cap * sizeof(CallbackEntry));
    if (!new_arr) return false;

    ctx->callbacks = new_arr;
    ctx->callback_capacity = new_cap;
    return true;
}

/* ==================== 回调管理 ==================== */

/**
 * @brief 注册流式事件回调（无过滤）
 *
 * 将回调函数添加到上下文的回调列表中，过滤掩码设为 STREAM_FILTER_ALL。
 * 最多支持 STREAM_MAX_CALLBACKS 个回调。
 * @param ctx       流式上下文
 * @param callback  回调函数指针
 * @param user_data 回调透传数据
 * @return true 注册成功，false 参数无效或回调已满
 */
bool stream_register_callback(StreamContext *ctx, StreamCallback callback, void *user_data) {
    if (!ctx || !callback) return false;

    /* 动态扩容确保足够容量 */
    if (!stream_ensure_capacity(ctx, ctx->callback_count + 1)) {
        return false;  /* 超过硬上限或内存分配失败 */
    }

    ctx->callbacks[ctx->callback_count].callback = callback;
    ctx->callbacks[ctx->callback_count].user_data = user_data;
    ctx->callbacks[ctx->callback_count].id = ctx->next_callback_id++;
    ctx->callbacks[ctx->callback_count].filter_mask = STREAM_FILTER_ALL;
    ctx->callback_count++;

    return true;
}

/**
 * 注册流式事件回调（带事件类型过滤掩码）。
 * 仅当事件类型匹配 filter_mask 中的位时，回调才会被调用。
 * @param ctx          流式上下文
 * @param callback     回调函数指针
 * @param user_data    回调透传数据
 * @param filter_mask  事件类型位掩码（STREAM_FILTER_ALL 表示接收全部）
 * @return >=0 成功，返回回调 ID；<0 失败
 */
int stream_register_callback_ex(StreamContext *ctx, StreamCallback callback,
                                 void *user_data, uint64_t filter_mask) {
    if (!ctx || !callback) return -1;

    /* 动态扩容确保足够容量 */
    if (!stream_ensure_capacity(ctx, ctx->callback_count + 1)) {
        return -1;  /* 超过硬上限或内存分配失败 */
    }

    int assigned_id = ctx->next_callback_id++;
    ctx->callbacks[ctx->callback_count].callback = callback;
    ctx->callbacks[ctx->callback_count].user_data = user_data;
    ctx->callbacks[ctx->callback_count].id = assigned_id;
    ctx->callbacks[ctx->callback_count].filter_mask = filter_mask;
    ctx->callback_count++;

    return assigned_id;
}

/**
 * 取消注册流式事件回调（按函数指针）。
 * 从回调列表中移除指定回调函数，后续回调前移一位以保持数组紧凑。
 * @param ctx      流式上下文
 * @param callback 要移除的回调函数指针
 * @return true 移除成功，false 未找到或参数无效
 */
bool stream_unregister_callback(StreamContext *ctx, StreamCallback callback) {
    if (!ctx || !callback) return false;

    for (int i = 0; i < ctx->callback_count; i++) {
        if (ctx->callbacks[i].callback == callback) {
            /* 将后续回调前移一位 */
            for (int j = i; j < ctx->callback_count - 1; j++) {
                ctx->callbacks[j] = ctx->callbacks[j + 1];
            }
            ctx->callback_count--;
            return true;
        }
    }
    return false;
}

/**
 * 通过回调 ID 注销流式事件回调。
 * 从回调列表中查找指定 ID 的回调并移除，后续回调前移以保持数组紧凑。
 * @param ctx          流式上下文
 * @param callback_id  注册时返回的回调 ID
 * @return true 成功，false 未找到或 ctx 为 NULL
 */
bool stream_unregister_callback_by_id(StreamContext *ctx, int callback_id) {
    if (!ctx || callback_id <= 0) return false;

    for (int i = 0; i < ctx->callback_count; i++) {
        if (ctx->callbacks[i].id == callback_id) {
            /* 将后续回调前移一位 */
            for (int j = i; j < ctx->callback_count - 1; j++) {
                ctx->callbacks[j] = ctx->callbacks[j + 1];
            }
            ctx->callback_count--;
            return true;
        }
    }
    return false;
}

/**
 * 更新回调的事件过滤掩码。
 * 通过回调 ID 查找回调并更新其 filter_mask 字段。
 * @param ctx          流式上下文
 * @param callback_id  注册时返回的回调 ID
 * @param filter_mask  新的事件类型位掩码
 * @return true 成功，false 未找到对应回调
 */
bool stream_set_callback_filter(StreamContext *ctx, int callback_id, uint64_t filter_mask) {
    if (!ctx || callback_id <= 0) return false;

    for (int i = 0; i < ctx->callback_count; i++) {
        if (ctx->callbacks[i].id == callback_id) {
            ctx->callbacks[i].filter_mask = filter_mask;
            return true;
        }
    }
    return false;
}

/**
 * 获取回调的事件过滤掩码。
 * @param ctx          流式上下文
 * @param callback_id  注册时返回的回调 ID
 * @return 过滤掩码，未找到时返回 STREAM_FILTER_NONE
 */
uint64_t stream_get_callback_filter(StreamContext *ctx, int callback_id) {
    if (!ctx || callback_id <= 0) return STREAM_FILTER_NONE;

    for (int i = 0; i < ctx->callback_count; i++) {
        if (ctx->callbacks[i].id == callback_id) {
            return ctx->callbacks[i].filter_mask;
        }
    }
    return STREAM_FILTER_NONE;
}

/* ==================== 事件缓冲区管理（内部） ==================== */

/**
 * @brief 确保事件缓冲区有足够容量
 *
 * 如果缓冲区未分配，则首次分配 STREAM_INITIAL_BUFFER 容量。
 * 如果已满，则倍增扩容，上限为 STREAM_MAX_BUFFER。
 *
 * @param ctx 流式上下文
 * @return true 容量足够或扩容成功，false 缓冲区已满或分配失败
 */
static bool stream_ensure_buffer(StreamContext *ctx) {
    if (ctx->buffer_count < ctx->buffer_capacity) return true;
    if (ctx->buffer_capacity >= STREAM_MAX_BUFFER) return false;

    int new_cap = ctx->buffer_capacity == 0
        ? STREAM_INITIAL_BUFFER
        : ctx->buffer_capacity * 2;
    if (new_cap > STREAM_MAX_BUFFER) new_cap = STREAM_MAX_BUFFER;

    StreamEvent *new_buf = (StreamEvent *)lv00_realloc(
        ctx->buffer, (size_t)new_cap * sizeof(StreamEvent));
    if (!new_buf) return false;

    ctx->buffer = new_buf;
    ctx->buffer_capacity = new_cap;
    return true;
}

/**
 * @brief 将事件追加到缓冲区（环形队列尾部）
 *
 * 如果缓冲区已满（达到 STREAM_MAX_BUFFER），事件被丢弃并增加 dropped_count。
 *
 * @param ctx   流式上下文
 * @param event 待缓冲的事件
 */
static void stream_buffer_push(StreamContext *ctx, const StreamEvent *event) {
    if (!stream_ensure_buffer(ctx)) {
        /* 缓冲区满，丢弃事件并计数 */
        ctx->dropped_count++;
        return;
    }

    /* 计算写入位置（环形队列） */
    int write_pos = (ctx->buffer_head + ctx->buffer_count) % ctx->buffer_capacity;
    ctx->buffer[write_pos] = *event;
    ctx->buffer_count++;
}

/**
 * @brief 内部发射函数：分发事件到所有过滤匹配的回调
 *
 * 遍历回调数组，对每个回调检查其 filter_mask 是否与事件类型匹配。
 * 仅当 (filter_mask & STREAM_EVENT_MASK(event->type)) != 0 时才调用回调。
 *
 * @param ctx   流式上下文
 * @param event 事件数据
 */
static void stream_dispatch(StreamContext *ctx, const StreamEvent *event) {
    uint64_t event_bit = STREAM_EVENT_MASK(event->type);
    for (int i = 0; i < ctx->callback_count; i++) {
        if (ctx->callbacks[i].callback &&
            (ctx->callbacks[i].filter_mask & event_bit) != 0) {
            ctx->callbacks[i].callback(event, ctx->callbacks[i].user_data);
        }
    }
}

/**
 * @brief 检查节流间隔是否已过
 *
 * 如果距上次发射已超过 throttle_ms 毫秒，则返回 true 并更新时间戳。
 * 首次调用（last_emit_ms == 0）时直接返回 true。
 *
 * @param ctx 流式上下文
 * @return true 节流间隔已过，false 尚未到期
 */
static bool stream_throttle_expired(StreamContext *ctx) {
    long now = stream_timestamp_ms();
    if (ctx->last_emit_ms == 0 ||
        (now - ctx->last_emit_ms) >= ctx->throttle_ms) {
        ctx->last_emit_ms = now;
        return true;
    }
    return false;
}

/**
 * @brief 更新事件统计计数器
 *
 * 在每次事件发射时调用，累加对应类型的计数和总数。
 *
 * @param ctx   流式上下文
 * @param event 事件数据
 */
static void stream_update_stats(StreamContext *ctx, const StreamEvent *event) {
    if (!ctx) return;
    int type_idx = (int)event->type;
    if (type_idx >= 0 && type_idx < STREAM_EVENT_TYPE_COUNT) {
        ctx->event_counts[type_idx]++;
    }
    ctx->total_count++;
}

/* ==================== 事件发射 ==================== */

/**
 * @brief 发射流式事件
 *
 * 根据当前发射策略采取不同行为：
 * - IMMEDIATE: 立即分发到所有过滤匹配的回调，并更新统计
 * - BUFFERED: 事件入队缓冲区，等待 stream_flush()
 * - THROTTLED: 事件入队，间隔到期时自动刷新缓冲区
 *
 * 无论哪种模式，都会更新事件统计计数器。
 *
 * @param ctx   流式上下文
 * @param event 事件数据指针
 */
void stream_emit(StreamContext *ctx, const StreamEvent *event) {
    if (!ctx || !event) return;

    /* 更新事件统计（无论哪种发射模式都计数） */
    stream_update_stats(ctx, event);

    switch (ctx->emit_mode) {
        case STREAM_EMIT_IMMEDIATE:
            stream_dispatch(ctx, event);
            break;

        case STREAM_EMIT_BUFFERED:
            stream_buffer_push(ctx, event);
            break;

        case STREAM_EMIT_THROTTLED:
            stream_buffer_push(ctx, event);
            if (stream_throttle_expired(ctx)) {
                stream_flush(ctx);
            }
            break;

        case STREAM_EMIT_LAZY:
            /* 惰性模式：事件仅入队到 lazy_queue，
             * 由消费者通过 stream_lazy_next / stream_lazy_drain 主动拉取。
             * 当队列达到阈值时自动触发刷新。 */
            stream_lazy_enqueue(ctx, event);
            if (ctx->lazy_threshold > 0 &&
                ctx->lazy_count >= ctx->lazy_threshold) {
                stream_flush(ctx);
            }
            break;
    }
}

/**
 * 发射简化流式事件。
 * 封装 StreamEvent 结构体的构造过程，用基本参数填充事件字段，
 * 一次性完成事件组装和发射。不需要的字段（如 node_id、constraint_id 等）
 * 自动设为 -1。
 * @param ctx         流式上下文
 * @param type        事件类型
 * @param description 事件描述字符串
 * @param step_number 当前步骤编号
 */
void stream_emit_simple(StreamContext *ctx, StreamEventType type,
                         const char *description, int step_number) {
    if (!ctx) return;

    StreamEvent event;
    memset(&event, 0, sizeof(event));

    event.type = type;
    event.timestamp_ms = stream_timestamp_ms();
    event.step_number = step_number;
    event.description = description;
    event.node_id = -1;
    event.constraint_id = -1;
    event.rule_id = -1;
    event.var_id = -1;
    event.total_steps = -1;
    event.progress = -1.0;

    stream_emit(ctx, &event);
}

/* ==================== 便捷发射函数 ==================== */

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
void stream_emit_node_event(StreamContext *ctx, StreamEventType type,
                             int node_id, const char *description, int step_number) {
    if (!ctx) return;

    StreamEvent event;
    memset(&event, 0, sizeof(event));

    event.type = type;
    event.timestamp_ms = stream_timestamp_ms();
    event.step_number = step_number;
    event.description = description;
    event.node_id = node_id;
    event.constraint_id = -1;
    event.rule_id = -1;
    event.var_id = -1;
    event.total_steps = -1;
    event.progress = -1.0;

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
void stream_emit_constraint_event(StreamContext *ctx, StreamEventType type,
                                   int constraint_id, const char *description, int step_number) {
    if (!ctx) return;

    StreamEvent event;
    memset(&event, 0, sizeof(event));

    event.type = type;
    event.timestamp_ms = stream_timestamp_ms();
    event.step_number = step_number;
    event.description = description;
    event.node_id = -1;
    event.constraint_id = constraint_id;
    event.rule_id = -1;
    event.var_id = -1;
    event.total_steps = -1;
    event.progress = -1.0;

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
void stream_emit_progress(StreamContext *ctx, double progress,
                           const char *description, int step_number, int total_steps) {
    if (!ctx) return;

    StreamEvent event;
    memset(&event, 0, sizeof(event));

    event.type = STREAM_EVENT_PROGRESS;
    event.timestamp_ms = stream_timestamp_ms();
    event.step_number = step_number;
    event.total_steps = total_steps;
    event.description = description;
    event.node_id = -1;
    event.constraint_id = -1;
    event.rule_id = -1;
    event.var_id = -1;
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
void stream_emit_numeric(StreamContext *ctx, StreamEventType type,
                          double numeric_value, const char *description, int step_number) {
    if (!ctx) return;

    StreamEvent event;
    memset(&event, 0, sizeof(event));

    event.type = type;
    event.timestamp_ms = stream_timestamp_ms();
    event.step_number = step_number;
    event.description = description;
    event.node_id = -1;
    event.constraint_id = -1;
    event.rule_id = -1;
    event.var_id = -1;
    event.total_steps = -1;
    event.progress = -1.0;
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
void stream_emit_graph_snapshot(StreamContext *ctx, StreamEventType type,
                                 const char *graph_json, const char *description, int step_number) {
    if (!ctx) return;

    StreamEvent event;
    memset(&event, 0, sizeof(event));

    event.type = type;
    event.timestamp_ms = stream_timestamp_ms();
    event.step_number = step_number;
    event.description = description;
    event.node_id = -1;
    event.constraint_id = -1;
    event.rule_id = -1;
    event.var_id = -1;
    event.total_steps = -1;
    event.progress = -1.0;
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
    if (!ctx) return;

    StreamEvent event;
    memset(&event, 0, sizeof(event));

    event.type = STREAM_EVENT_NORMALIZE_MERGE;
    event.timestamp_ms = stream_timestamp_ms();
    event.step_number = step_number;
    event.node_id = to_id;
    event.constraint_id = from_id;  /* 复用字段存储 from_id */
    event.rule_id = -1;
    event.var_id = -1;
    event.total_steps = -1;
    event.progress = -1.0;

    /* 构建描述字符串（堆分配，避免静态缓冲区线程安全问题） */
    char *desc_buf = (char *)lv00_malloc(128);
    if (desc_buf) {
        snprintf(desc_buf, 128, "节点合并: %d → %d", from_id, to_id);
        event.description = desc_buf;
        stream_emit(ctx, &event);
        lv00_free((void **)&desc_buf);
    } else {
        /* 内存不足时使用简单描述 */
        event.description = "节点合并";
        stream_emit(ctx, &event);
    }
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
void stream_emit_variable_resolved(StreamContext *ctx, int var_id,
                                    double value, const char *description, int step_number) {
    if (!ctx) return;

    StreamEvent event;
    memset(&event, 0, sizeof(event));

    event.type = STREAM_EVENT_SOLVE_VARIABLE_RESOLVED;
    event.timestamp_ms = stream_timestamp_ms();
    event.step_number = step_number;
    event.description = description;
    event.node_id = -1;
    event.constraint_id = -1;
    event.rule_id = -1;
    event.var_id = var_id;
    event.total_steps = -1;
    event.progress = -1.0;
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

/* ==================== 发射模式 API ==================== */

/**
 * 设置事件发射模式。
 *
 * @param ctx          流式上下文
 * @param mode         发射模式（IMMEDIATE / BUFFERED / THROTTLED）
 * @param throttle_ms  节流间隔毫秒数（仅 THROTTLED 模式生效，0 使用默认值 50ms）
 */
void stream_set_emit_mode(StreamContext *ctx, StreamEmitMode mode, long throttle_ms) {
    if (!ctx) return;
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
    if (!ctx) return STREAM_EMIT_IMMEDIATE;
    return ctx->emit_mode;
}

/* ==================== 异步模式 API ==================== */

/**
 * 设置异步模式。
 *
 * 由于本项目是纯 C 无线程库依赖，异步模式实现为 BUFFERED 模式的别名：
 * - 启用时：切换到 BUFFERED 模式，并按 capacity 参数分配缓冲区
 * - 禁用时：恢复为 IMMEDIATE 模式，自动刷新残留事件
 *
 * @param ctx      流式上下文
 * @param enabled  true 启用异步，false 恢复同步
 * @param capacity 队列容量（仅在 enabled=true 时生效，0 使用默认值 1024）
 * @return true 成功，false 失败（ctx 为 NULL 或内存不足）
 */
bool stream_set_async_mode(StreamContext *ctx, bool enabled, int capacity) {
    if (!ctx) return false;

    if (enabled) {
        /* 启用异步：切换到 BUFFERED 模式 */
        ctx->emit_mode = STREAM_EMIT_BUFFERED;

        /* 如果指定了容量且大于当前容量，尝试扩容 */
        if (capacity > 0 && capacity > ctx->buffer_capacity) {
            /* 钳制到硬上限 */
            if (capacity > STREAM_MAX_BUFFER) capacity = STREAM_MAX_BUFFER;
            StreamEvent *new_buf = (StreamEvent *)lv00_realloc(
                ctx->buffer, (size_t)capacity * sizeof(StreamEvent));
            if (!new_buf) return false;
            ctx->buffer = new_buf;
            ctx->buffer_capacity = capacity;
        }
        /* 如果缓冲区尚未分配，使用默认容量 */
        if (!ctx->buffer) {
            int init_cap = (capacity > 0 && capacity <= STREAM_MAX_BUFFER)
                ? capacity
                : STREAM_ASYNC_QUEUE_DEFAULT_CAPACITY;
            if (init_cap > STREAM_MAX_BUFFER) init_cap = STREAM_MAX_BUFFER;
            ctx->buffer = (StreamEvent *)lv00_malloc(
                (size_t)init_cap * sizeof(StreamEvent));
            if (!ctx->buffer) return false;
            ctx->buffer_capacity = init_cap;
            ctx->buffer_count = 0;
            ctx->buffer_head = 0;
        }
    } else {
        /* 禁用异步：恢复为 IMMEDIATE 模式，自动刷新残留事件 */
        ctx->emit_mode = STREAM_EMIT_IMMEDIATE;
        if (ctx->buffer_count > 0) {
            stream_flush(ctx);
        }
    }

    return true;
}

/**
 * 刷新缓冲区中的事件。
 *
 * 将缓冲区中所有待处理事件按 FIFO 顺序分发到所有过滤匹配的回调。
 * 刷新完成后重置读写指针。
 *
 * @param ctx 流式上下文
 */
void stream_flush(StreamContext *ctx) {
    if (!ctx || ctx->buffer_count == 0) return;

    while (ctx->buffer_count > 0) {
        /* 从 buffer_head 位置读取事件 */
        StreamEvent *ev = &ctx->buffer[ctx->buffer_head];
        stream_dispatch(ctx, ev);
        ctx->buffer_head = (ctx->buffer_head + 1) % ctx->buffer_capacity;
        ctx->buffer_count--;
    }
    /* flush 完成后重置读写指针 */
    ctx->buffer_head = 0;
}

/**
 * 获取缓冲区中待处理事件数量。
 *
 * @param ctx 流式上下文
 * @return 待处理事件数，ctx 为 NULL 时返回 0
 */
int stream_pending_count(StreamContext *ctx) {
    if (!ctx) return 0;
    return ctx->buffer_count;
}

/* ==================== 缓冲区管理 API（已有） ==================== */

/**
 * @brief 清空缓冲区（不发射事件）
 *
 * @param ctx 流式上下文
 * @return 被清除的事件数
 */
int stream_clear_buffer(StreamContext *ctx) {
    if (!ctx) return 0;
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
    if (!ctx) return 0;
    return ctx->buffer_count;
}

/* ==================== JSON 序列化 API ==================== */

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
    if (!src || dest_size == 0) return 0;

    size_t written = 0;
    while (*src && written + 1 < dest_size) {
        switch (*src) {
            case '"':
                if (written + 2 >= dest_size) goto done;
                dest[written++] = '\\';
                dest[written++] = '"';
                break;
            case '\\':
                if (written + 2 >= dest_size) goto done;
                dest[written++] = '\\';
                dest[written++] = '\\';
                break;
            case '\n':
                if (written + 2 >= dest_size) goto done;
                dest[written++] = '\\';
                dest[written++] = 'n';
                break;
            case '\r':
                if (written + 2 >= dest_size) goto done;
                dest[written++] = '\\';
                dest[written++] = 'r';
                break;
            case '\t':
                if (written + 2 >= dest_size) goto done;
                dest[written++] = '\\';
                dest[written++] = 't';
                break;
            default:
                dest[written++] = *src;
                break;
        }
        src++;
    }
done:
    if (written < dest_size) dest[written] = '\0';
    return (int)written;
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
    if (offset < 0) return offset;

    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(buf ? buf + offset : NULL,
                            (buf && (size_t)offset < size) ? size - (size_t)offset : 0,
                            fmt, args);
    va_end(args);

    /* vsnprintf 返回期望写入的字符数（不含终止符），可能超过剩余空间 */
    if (written < 0) written = 0;
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
        if (buffer && size > 0) buffer[0] = '\0';
        return 0;
    }

    const char *type_id = stream_event_type_id(event->type);
    const char *type_name = stream_event_type_name(event->type);
    const char *color = stream_event_color(event->type);

    /* 转义 description 字段 */
    char desc_escaped[2048];
    desc_escaped[0] = '\0';
    if (event->description) {
        stream_json_escape(desc_escaped, event->description, sizeof(desc_escaped));
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
        needed = stream_buf_append(NULL, 0, needed, "  \"description\": \"%s\",\n", desc_escaped);
    } else {
        needed = stream_buf_append(NULL, 0, needed, "  \"description\": null,\n");
    }
    needed = stream_buf_append(NULL, 0, needed, "  \"progress\": %.6g,\n", event->progress);
    needed = stream_buf_append(NULL, 0, needed, "  \"numeric_value\": %.6g\n", event->numeric_value);
    needed = stream_buf_append(NULL, 0, needed, "}");

    /* 如果没有提供缓冲区或缓冲区太小，返回所需大小 */
    if (!buffer || size == 0) return needed;

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
        pos = stream_buf_append(buffer, size, pos, "  \"description\": \"%s\",\n", desc_escaped);
    } else {
        pos = stream_buf_append(buffer, size, pos, "  \"description\": null,\n");
    }
    pos = stream_buf_append(buffer, size, pos, "  \"progress\": %.6g,\n", event->progress);
    pos = stream_buf_append(buffer, size, pos, "  \"numeric_value\": %.6g\n", event->numeric_value);
    pos = stream_buf_append(buffer, size, pos, "}");

    /* 确保终止符 */
    if ((size_t)pos < size) {
        buffer[pos] = '\0';
    } else if (size > 0) {
        buffer[size - 1] = '\0';
    }

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
        if (buffer && size > 0) buffer[0] = '\0';
        return 0;
    }

    /* 先将事件序列化为 JSON 到临时缓冲区 */
    char event_json[STREAM_JSON_BUFFER_DEFAULT_SIZE];
    int event_json_len = stream_event_to_json(event, event_json, sizeof(event_json));

    /* 如果事件 JSON 太长，使用动态分配 */
    char *event_json_ptr = event_json;
    if (event_json_len >= (int)sizeof(event_json)) {
        /* 需要更大的缓冲区 */
        size_t needed = (size_t)event_json_len + 1;
        event_json_ptr = (char *)lv00_malloc(needed);
        if (!event_json_ptr) {
            if (buffer && size > 0) buffer[0] = '\0';
            return 0;
        }
        stream_event_to_json(event, event_json_ptr, needed);
    }

    /* 构建 JSON-RPC 外壳 */
    int pos = 0;
    pos = stream_buf_append(buffer, size, pos,
        "{\"jsonrpc\":\"2.0\",\"method\":\"stream.event\",\"params\":");
    pos = stream_buf_append(buffer, size, pos, "%s", event_json_ptr);
    pos = stream_buf_append(buffer, size, pos, "}");

    /* 确保终止符 */
    if ((size_t)pos < size) {
        buffer[pos] = '\0';
    } else if (size > 0) {
        buffer[size - 1] = '\0';
    }

    /* 释放动态分配的临时缓冲区 */
    if (event_json_ptr != event_json) {
        lv00_free((void **)&event_json_ptr);
    }

    return pos;
}

/* ==================== 事件统计 API ==================== */

/**
 * 重置事件统计计数器。
 * 将所有事件类型计数、总计数和丢弃计数清零。
 *
 * @param ctx 流式上下文
 */
void stream_reset_stats(StreamContext *ctx) {
    if (!ctx) return;
    memset(ctx->event_counts, 0, sizeof(ctx->event_counts));
    ctx->total_count = 0;
    ctx->dropped_count = 0;
}

/**
 * 获取指定事件类型的发射次数。
 *
 * @param ctx  流式上下文
 * @param type 事件类型
 * @return 发射次数，ctx 为 NULL 或类型越界时返回 0
 */
long stream_get_event_count(StreamContext *ctx, StreamEventType type) {
    if (!ctx) return 0;
    int idx = (int)type;
    if (idx >= 0 && idx < STREAM_EVENT_TYPE_COUNT) {
        return ctx->event_counts[idx];
    }
    return 0;
}

/**
 * 获取事件发射总数。
 *
 * @param ctx 流式上下文
 * @return 总发射次数，ctx 为 NULL 时返回 0
 */
long stream_get_total_event_count(StreamContext *ctx) {
    if (!ctx) return 0;
    return ctx->total_count;
}

/**
 * 获取已丢弃的事件数（缓冲区满时）。
 *
 * @param ctx 流式上下文
 * @return 丢弃的事件数，ctx 为 NULL 时返回 0
 */
long stream_get_dropped_count(StreamContext *ctx) {
    if (!ctx) return 0;
    return ctx->dropped_count;
}

/* ==================== 工具函数 ==================== */

/**
 * 获取高精度时间戳（毫秒）。
 * Windows 平台使用 QueryPerformanceCounter 获取高精度时间，
 * 其他平台使用 gettimeofday。返回值为自某参考点以来的毫秒数，
 * 仅用于计算相对时间差，绝对值无意义。
 * @return 毫秒级时间戳
 */
long stream_timestamp_ms(void) {
    /* 使用平台最优时钟获取墙上时钟时间（非处理器时间）：
     * - Windows: QueryPerformanceCounter 高精度墙上时钟
     * - 其他平台：gettimeofday 微秒精度墙上时钟
     *
     * 避免使用 clock()，因为它在多线程环境下测量的是处理器时间
     * 而非墙上时钟时间，导致时间戳不准确。 */
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    if (QueryPerformanceFrequency(&freq) && QueryPerformanceCounter(&counter)) {
        return (long)((counter.QuadPart * 1000) / freq.QuadPart);
    }
#else
    struct timeval tv;
    if (gettimeofday(&tv, NULL) == 0) {
        return (long)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
    }
#endif
    /* 终极回退：使用 time(NULL) */
    return (long)(time(NULL) * 1000);
}

/**
 * 获取事件类型的中文名称。
 * 用于前端 UI 显示和日志输出，将枚举值映射为可读的中文字符串。
 * @param type 事件类型枚举值
 * @return 中文名称字符串（静态常量，无需释放）
 */
const char *stream_event_type_name(StreamEventType type) {
    switch (type) {
        case STREAM_EVENT_ENGINE_START:          return "引擎启动";
        case STREAM_EVENT_ENGINE_DONE:           return "引擎完成";
        case STREAM_EVENT_ENGINE_PAUSED:         return "引擎暂停";
        case STREAM_EVENT_NORMALIZE_START:       return "归一化开始";
        case STREAM_EVENT_NORMALIZE_MERGE:       return "节点合并";
        case STREAM_EVENT_NORMALIZE_DONE:        return "归一化完成";
        case STREAM_EVENT_REWRITE_START:         return "重写开始";
        case STREAM_EVENT_REWRITE_RULE_LOADED:   return "规则加载";
        case STREAM_EVENT_REWRITE_MATCH_FOUND:   return "匹配找到";
        case STREAM_EVENT_REWRITE_APPLIED:       return "规则应用";
        case STREAM_EVENT_REWRITE_ROLLBACK:      return "规则回滚";
        case STREAM_EVENT_REWRITE_DONE:          return "重写完成";
        case STREAM_EVENT_SOLVE_START:           return "求解开始";
        case STREAM_EVENT_SOLVE_EQUATION_EXTRACTED: return "方程提取";
        case STREAM_EVENT_SOLVE_GROEBNER_STEP:   return "Gröbner基步骤";
        case STREAM_EVENT_SOLVE_VARIABLE_RESOLVED: return "变量解得";
        case STREAM_EVENT_SOLVE_DONE:            return "求解完成";
        case STREAM_EVENT_PROOF_STEP_ADDED:      return "证明步骤添加";
        case STREAM_EVENT_PROOF_STEP_APPLIED:    return "证明步骤应用";
        case STREAM_EVENT_PROOF_UNIFY:           return "合一检查";
        case STREAM_EVENT_PROOF_COLOR_UPDATE:    return "颜色更新";
        case STREAM_EVENT_PROOF_DEPENDENCY_CHANGE: return "依赖链变化";
        case STREAM_EVENT_FUNC_BLOCK_PACK_START:      return "函数打包开始";
        case STREAM_EVENT_FUNC_BLOCK_PACK_DONE:       return "函数打包完成";
        case STREAM_EVENT_FUNC_BLOCK_INSTANTIATE_START: return "函数实例化开始";
        case STREAM_EVENT_FUNC_BLOCK_INSTANTIATE_DONE:  return "函数实例化完成";
        case STREAM_EVENT_FUNC_BLOCK_PARTIAL_APPLY:   return "部分应用";
        case STREAM_EVENT_FUNC_BLOCK_DETERMINISM_CHECK: return "确定性检查";
        case STREAM_EVENT_FUNC_BLOCK_CAPTURE_AVOID:    return "捕获避免";
        case STREAM_EVENT_FUNC_BLOCK_CROSS_BOUNDARY:   return "跨边界操作";
        case STREAM_EVENT_CONFLICT_DETECTED:     return "冲突检测";
        case STREAM_EVENT_CONSTRAINT_ADDED:      return "约束添加";
        case STREAM_EVENT_NODE_ADDED:            return "节点添加";
        case STREAM_EVENT_CIRCUIT_TRIP:          return "位数熔断";
        case STREAM_EVENT_ERROR:                 return "错误";
        case STREAM_EVENT_WARNING:               return "警告";
        case STREAM_EVENT_INFO:                  return "信息";
        case STREAM_EVENT_PROGRESS:              return "进度";
        case STREAM_EVENT_GRAPH_SNAPSHOT:        return "图快照";
        default:                                 return "未知事件";
    }
}

/**
 * 获取事件类型的英文标识符。
 * 用于 JSON 序列化和前端事件路由，返回大写字母+下划线格式的字符串。
 * @param type 事件类型枚举值
 * @return 英文标识符字符串（静态常量，无需释放）
 */
const char *stream_event_type_id(StreamEventType type) {
    switch (type) {
        case STREAM_EVENT_ENGINE_START:          return "ENGINE_START";
        case STREAM_EVENT_ENGINE_DONE:           return "ENGINE_DONE";
        case STREAM_EVENT_ENGINE_PAUSED:         return "ENGINE_PAUSED";
        case STREAM_EVENT_NORMALIZE_START:       return "NORMALIZE_START";
        case STREAM_EVENT_NORMALIZE_MERGE:       return "NORMALIZE_MERGE";
        case STREAM_EVENT_NORMALIZE_DONE:        return "NORMALIZE_DONE";
        case STREAM_EVENT_REWRITE_START:         return "REWRITE_START";
        case STREAM_EVENT_REWRITE_RULE_LOADED:   return "REWRITE_RULE_LOADED";
        case STREAM_EVENT_REWRITE_MATCH_FOUND:   return "REWRITE_MATCH_FOUND";
        case STREAM_EVENT_REWRITE_APPLIED:       return "REWRITE_APPLIED";
        case STREAM_EVENT_REWRITE_ROLLBACK:      return "REWRITE_ROLLBACK";
        case STREAM_EVENT_REWRITE_DONE:          return "REWRITE_DONE";
        case STREAM_EVENT_SOLVE_START:           return "SOLVE_START";
        case STREAM_EVENT_SOLVE_EQUATION_EXTRACTED: return "SOLVE_EQUATION_EXTRACTED";
        case STREAM_EVENT_SOLVE_GROEBNER_STEP:   return "SOLVE_GROEBNER_STEP";
        case STREAM_EVENT_SOLVE_VARIABLE_RESOLVED: return "SOLVE_VARIABLE_RESOLVED";
        case STREAM_EVENT_SOLVE_DONE:            return "SOLVE_DONE";
        case STREAM_EVENT_PROOF_STEP_ADDED:      return "PROOF_STEP_ADDED";
        case STREAM_EVENT_PROOF_STEP_APPLIED:    return "PROOF_STEP_APPLIED";
        case STREAM_EVENT_PROOF_UNIFY:           return "PROOF_UNIFY";
        case STREAM_EVENT_PROOF_COLOR_UPDATE:    return "PROOF_COLOR_UPDATE";
        case STREAM_EVENT_PROOF_DEPENDENCY_CHANGE: return "PROOF_DEPENDENCY_CHANGE";
        case STREAM_EVENT_FUNC_BLOCK_PACK_START:      return "FUNC_BLOCK_PACK_START";
        case STREAM_EVENT_FUNC_BLOCK_PACK_DONE:       return "FUNC_BLOCK_PACK_DONE";
        case STREAM_EVENT_FUNC_BLOCK_INSTANTIATE_START: return "FUNC_BLOCK_INSTANTIATE_START";
        case STREAM_EVENT_FUNC_BLOCK_INSTANTIATE_DONE:  return "FUNC_BLOCK_INSTANTIATE_DONE";
        case STREAM_EVENT_FUNC_BLOCK_PARTIAL_APPLY:   return "FUNC_BLOCK_PARTIAL_APPLY";
        case STREAM_EVENT_FUNC_BLOCK_DETERMINISM_CHECK: return "FUNC_BLOCK_DETERMINISM_CHECK";
        case STREAM_EVENT_FUNC_BLOCK_CAPTURE_AVOID:    return "FUNC_BLOCK_CAPTURE_AVOID";
        case STREAM_EVENT_FUNC_BLOCK_CROSS_BOUNDARY:   return "FUNC_BLOCK_CROSS_BOUNDARY";
        case STREAM_EVENT_CONFLICT_DETECTED:     return "CONFLICT_DETECTED";
        case STREAM_EVENT_CONSTRAINT_ADDED:      return "CONSTRAINT_ADDED";
        case STREAM_EVENT_NODE_ADDED:            return "NODE_ADDED";
        case STREAM_EVENT_CIRCUIT_TRIP:          return "CIRCUIT_TRIP";
        case STREAM_EVENT_ERROR:                 return "ERROR";
        case STREAM_EVENT_WARNING:               return "WARNING";
        case STREAM_EVENT_INFO:                  return "INFO";
        case STREAM_EVENT_PROGRESS:              return "PROGRESS";
        case STREAM_EVENT_GRAPH_SNAPSHOT:        return "GRAPH_SNAPSHOT";
        default:                                 return "UNKNOWN_EVENT";
    }
}

/**
 * 获取事件类型对应的前端显示颜色（十六进制格式）。
 * 根据事件类型返回对应的 CSS 颜色字符串，用于 Web 前端渲染事件节点。
 * 颜色常量统一定义在文件顶部的 STREAM_COLOR_* 宏中。
 * @param type 事件类型枚举值
 * @return 十六进制颜色字符串（如 "#3fb950"）
 */
const char *stream_event_color(StreamEventType type) {
    switch (type) {
        case STREAM_EVENT_ENGINE_START:
        case STREAM_EVENT_ENGINE_DONE:
            return STREAM_COLOR_GREEN;  /* 绿色 */
        case STREAM_EVENT_ERROR:
            return STREAM_COLOR_RED;  /* 红色 */
        case STREAM_EVENT_WARNING:
            return STREAM_COLOR_YELLOW;  /* 黄色 */
        case STREAM_EVENT_CIRCUIT_TRIP:
            return STREAM_COLOR_ORANGE;  /* 橙色 */
        case STREAM_EVENT_PROGRESS:
            return STREAM_COLOR_BLUE;  /* 蓝色 */
        case STREAM_EVENT_INFO:
            return STREAM_COLOR_GRAY;  /* 灰色 */
        case STREAM_EVENT_GRAPH_SNAPSHOT:
            return STREAM_COLOR_LIGHT_GRAY;  /* 浅灰 */
        case STREAM_EVENT_NORMALIZE_MERGE:
        case STREAM_EVENT_REWRITE_MATCH_FOUND:
        case STREAM_EVENT_REWRITE_APPLIED:
        case STREAM_EVENT_SOLVE_VARIABLE_RESOLVED:
        case STREAM_EVENT_PROOF_STEP_APPLIED:
            return STREAM_COLOR_PURPLE;  /* 紫色 */
        default:
            return STREAM_COLOR_LIGHT_GRAY;  /* 默认浅灰 */
    }
}

/* ==================== 过滤掩码解析 ==================== */

/**
 * @brief 从英文标识符查找事件类型枚举值
 *
 * @param id_str 事件类型英文标识符（如 "ENGINE_START"）
 * @return 事件类型枚举值，未找到时返回 -1
 */
static int stream_find_event_type_by_id(const char *id_str) {
    if (!id_str) return -1;

    /* 遍历所有事件类型，通过 stream_event_type_id 反向查找 */
    for (int i = 0; i < STREAM_EVENT_TYPE_COUNT; i++) {
        const char *eid = stream_event_type_id((StreamEventType)i);
        if (eid && strcmp(eid, id_str) == 0) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief 解析类别名为事件类型掩码
 *
 * 支持以下类别名（不区分大小写）：
 *   - "engine":    引擎生命周期事件（ENGINE_START/DONE/PAUSED）
 *   - "normalize": 归一化事件（NORMALIZE_START/MERGE/DONE）
 *   - "rewrite":   重写事件（REWRITE_START/RULE_LOADED/MATCH_FOUND/APPLIED/ROLLBACK/DONE）
 *   - "solve":     求解事件（SOLVE_START/EQUATION_EXTRACTED/GROEBNER_STEP/VARIABLE_RESOLVED/DONE）
 *   - "proof":     证明事件（PROOF_STEP_ADDED/APPLIED/UNIFY/COLOR_UPDATE/DEPENDENCY_CHANGE）
 *   - "conflict":  冲突事件（CONFLICT_DETECTED）
 *   - "info":      信息事件（INFO/PROGRESS/GRAPH_SNAPSHOT）
 *
 * @param category 类别名
 * @return 对应的事件类型位掩码，未识别时返回 STREAM_FILTER_NONE
 */
static uint64_t stream_parse_category(const char *category) {
    if (!category) return STREAM_FILTER_NONE;

    /* 不区分大小写比较 */
    if (strcasecmp(category, "engine") == 0) {
        return STREAM_EVENT_MASK(STREAM_EVENT_ENGINE_START)
             | STREAM_EVENT_MASK(STREAM_EVENT_ENGINE_DONE)
             | STREAM_EVENT_MASK(STREAM_EVENT_ENGINE_PAUSED);
    }
    if (strcasecmp(category, "normalize") == 0) {
        return STREAM_EVENT_MASK(STREAM_EVENT_NORMALIZE_START)
             | STREAM_EVENT_MASK(STREAM_EVENT_NORMALIZE_MERGE)
             | STREAM_EVENT_MASK(STREAM_EVENT_NORMALIZE_DONE);
    }
    if (strcasecmp(category, "rewrite") == 0) {
        return STREAM_EVENT_MASK(STREAM_EVENT_REWRITE_START)
             | STREAM_EVENT_MASK(STREAM_EVENT_REWRITE_RULE_LOADED)
             | STREAM_EVENT_MASK(STREAM_EVENT_REWRITE_MATCH_FOUND)
             | STREAM_EVENT_MASK(STREAM_EVENT_REWRITE_APPLIED)
             | STREAM_EVENT_MASK(STREAM_EVENT_REWRITE_ROLLBACK)
             | STREAM_EVENT_MASK(STREAM_EVENT_REWRITE_DONE);
    }
    if (strcasecmp(category, "solve") == 0) {
        return STREAM_EVENT_MASK(STREAM_EVENT_SOLVE_START)
             | STREAM_EVENT_MASK(STREAM_EVENT_SOLVE_EQUATION_EXTRACTED)
             | STREAM_EVENT_MASK(STREAM_EVENT_SOLVE_GROEBNER_STEP)
             | STREAM_EVENT_MASK(STREAM_EVENT_SOLVE_VARIABLE_RESOLVED)
             | STREAM_EVENT_MASK(STREAM_EVENT_SOLVE_DONE);
    }
    if (strcasecmp(category, "proof") == 0) {
        return STREAM_EVENT_MASK(STREAM_EVENT_PROOF_STEP_ADDED)
             | STREAM_EVENT_MASK(STREAM_EVENT_PROOF_STEP_APPLIED)
             | STREAM_EVENT_MASK(STREAM_EVENT_PROOF_UNIFY)
             | STREAM_EVENT_MASK(STREAM_EVENT_PROOF_COLOR_UPDATE)
             | STREAM_EVENT_MASK(STREAM_EVENT_PROOF_DEPENDENCY_CHANGE);
    }
    if (strcasecmp(category, "func_block") == 0) {
        return STREAM_EVENT_MASK(STREAM_EVENT_FUNC_BLOCK_PACK_START)
             | STREAM_EVENT_MASK(STREAM_EVENT_FUNC_BLOCK_PACK_DONE)
             | STREAM_EVENT_MASK(STREAM_EVENT_FUNC_BLOCK_INSTANTIATE_START)
             | STREAM_EVENT_MASK(STREAM_EVENT_FUNC_BLOCK_INSTANTIATE_DONE)
             | STREAM_EVENT_MASK(STREAM_EVENT_FUNC_BLOCK_PARTIAL_APPLY)
             | STREAM_EVENT_MASK(STREAM_EVENT_FUNC_BLOCK_DETERMINISM_CHECK)
             | STREAM_EVENT_MASK(STREAM_EVENT_FUNC_BLOCK_CAPTURE_AVOID)
             | STREAM_EVENT_MASK(STREAM_EVENT_FUNC_BLOCK_CROSS_BOUNDARY);
    }
    if (strcasecmp(category, "conflict") == 0) {
        return STREAM_EVENT_MASK(STREAM_EVENT_CONFLICT_DETECTED);
    }
    if (strcasecmp(category, "info") == 0) {
        return STREAM_EVENT_MASK(STREAM_EVENT_INFO)
             | STREAM_EVENT_MASK(STREAM_EVENT_PROGRESS)
             | STREAM_EVENT_MASK(STREAM_EVENT_GRAPH_SNAPSHOT);
    }

    return STREAM_FILTER_NONE;
}

/**
 * 从字符串解析事件类型掩码。
 *
 * 支持以下格式：
 *   - "all" / "*" → STREAM_FILTER_ALL（接收所有事件）
 *   - "none" → STREAM_FILTER_NONE（不接收任何事件）
 *   - "ENGINE_START" → 单个事件类型
 *   - "ENGINE_START,ENGINE_DONE" → 多个事件类型用逗号分隔
 *   - "engine" → 按类别匹配（引擎生命周期事件）
 *   - "engine,rewrite" → 多个类别用逗号分隔
 *   - "ENGINE_START,engine" → 混合使用事件 ID 和类别名
 *
 * @param str 输入字符串
 * @return 解析后的位掩码，解析失败返回 STREAM_FILTER_NONE
 */
uint64_t stream_parse_filter_mask(const char *str) {
    if (!str) return STREAM_FILTER_NONE;

    /* 去除首尾空白 */
    while (*str == ' ' || *str == '\t' || *str == '\r' || *str == '\n') str++;
    if (*str == '\0') return STREAM_FILTER_NONE;

    /* 检查特殊值 "all" 或 "*" */
    if (strcmp(str, "all") == 0 || strcmp(str, "*") == 0) {
        return STREAM_FILTER_ALL;
    }
    /* 检查特殊值 "none" */
    if (strcmp(str, "none") == 0) {
        return STREAM_FILTER_NONE;
    }

    uint64_t mask = STREAM_FILTER_NONE;

    /* 复制字符串用于分词（避免修改原始字符串） */
    size_t len = strlen(str);
    char *buf = (char *)lv00_malloc(len + 1);
    if (!buf) return STREAM_FILTER_NONE;
    lv00_strlcpy(buf, str, len + 1);

    /* 按逗号分词 */
    char *saveptr = NULL;
    char *token = strtok_r(buf, ",", &saveptr);

    while (token) {
        /* 去除 token 首尾空白 */
        while (*token == ' ' || *token == '\t') token++;
        char *end = token + strlen(token) - 1;
        while (end > token && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
            *end = '\0';
            end--;
        }

        if (*token != '\0') {
            /* 先尝试按类别名解析 */
            uint64_t cat_mask = stream_parse_category(token);
            if (cat_mask != STREAM_FILTER_NONE) {
                mask |= cat_mask;
            } else {
                /* 再尝试按事件 ID 解析 */
                int type_idx = stream_find_event_type_by_id(token);
                if (type_idx >= 0 && type_idx < STREAM_EVENT_TYPE_COUNT) {
                    mask |= STREAM_EVENT_MASK((StreamEventType)type_idx);
                }
                /* 无法识别的 token 静默忽略 */
            }
        }

        token = strtok_r(NULL, ",", &saveptr);
    }

    lv00_free((void **)&buf);
    return mask;
}

/* ============================================================
 * 惰性拉取模式（完整实现 —— LZ/2026-05-23）
 *
 * 惰性模式让事件排队到 lazy_queue，消费者通过
 * stream_lazy_next / stream_lazy_drain 主动拉取。
 * 与 BUFFERED/THROTTLED 不同，惰性模式不自动调用 stream_dispatch，
 * 而是由消费者控制何时分发。
 *
 * 当 lazy_threshold > 0 且队列达到阈值时，
 * stream_emit 会自动触发 stream_flush 刷新。
 * ============================================================ */

/**
 * @brief 确保惰性队列有足够容量
 */
static bool stream_lazy_ensure_capacity(StreamContext *ctx) {
    if (ctx->lazy_capacity == 0) {
        ctx->lazy_capacity = 64;
        ctx->lazy_queue = (StreamEvent *)lv00_malloc(
            sizeof(StreamEvent) * (size_t)ctx->lazy_capacity);
        return ctx->lazy_queue != NULL;
    }
    if (ctx->lazy_count >= ctx->lazy_capacity) {
        int new_cap = ctx->lazy_capacity * 2;
        if (new_cap > STREAM_MAX_LAZY) {
            ctx->dropped_count++;
            return false;
        }
        StreamEvent *new_queue = (StreamEvent *)lv00_malloc(
            sizeof(StreamEvent) * (size_t)new_cap);
        if (!new_queue) return false;
        /* 拷贝环形缓冲区到线性数组 */
        for (int i = 0; i < ctx->lazy_count; i++) {
            int src = (ctx->lazy_head + i) % ctx->lazy_capacity;
            memcpy(&new_queue[i], &ctx->lazy_queue[src], sizeof(StreamEvent));
        }
        lv00_free((void **)&ctx->lazy_queue);
        ctx->lazy_queue = new_queue;
        ctx->lazy_capacity = new_cap;
        ctx->lazy_head = 0;
    }
    return true;
}

/**
 * @brief 事件入队到惰性队列
 */
static void stream_lazy_enqueue(StreamContext *ctx, const StreamEvent *event) {
    if (!ctx || !event) return;
    if (!stream_lazy_ensure_capacity(ctx)) return;
    int tail = (ctx->lazy_head + ctx->lazy_count) % ctx->lazy_capacity;
    memcpy(&ctx->lazy_queue[tail], event, sizeof(StreamEvent));
    ctx->lazy_count++;
}

/**
 * @brief 惰性拉取下一个待处理事件
 *
 * 从惰性队列头部取出第一个事件返回。
 * 调用者应当在使用完后通过 stream_lazy_drain 或 stream_flush 清理。
 *
 * @param ctx 流式上下文
 * @return 队列头部事件指针（属于队列内部内存），队列为空返回NULL
 */
const StreamEvent *stream_lazy_next(StreamContext *ctx)
{
    if (!ctx || ctx->lazy_count == 0) return NULL;
    return &ctx->lazy_queue[ctx->lazy_head];
}

/**
 * @brief 批量惰性拉取事件
 *
 * 从惰性队列头部开始，按 FIFO 顺序逐个调用 callback。
 * 最多处理 max_count 个事件。每处理一个，从队列中移除。
 *
 * @param ctx       流式上下文
 * @param callback  处理每个事件的回调
 * @param user_data 回调用户数据
 * @param max_count 最大处理事件数（<=0 表示无限制，但不推荐超过队列大小）
 * @return 实际处理的事件数
 */
int stream_lazy_drain(StreamContext *ctx, StreamCallback callback,
                      void *user_data, int max_count)
{
    if (!ctx || !callback || ctx->lazy_count == 0) return 0;

    int limit = max_count > 0 ? max_count : ctx->lazy_count;
    if (limit > ctx->lazy_count) limit = ctx->lazy_count;

    int drained = 0;
    for (int i = 0; i < limit; i++) {
        int idx = (ctx->lazy_head + i) % ctx->lazy_capacity;
        callback(&ctx->lazy_queue[idx], user_data);
        drained++;

        /* 更新 dispatch 统计 */
        ctx->total_count++;
    }

    /* 将剩余事件前移 */
    ctx->lazy_head = (ctx->lazy_head + drained) % ctx->lazy_capacity;
    ctx->lazy_count -= drained;

    return drained;
}

/**
 * @brief 获取惰性队列中的待处理事件数
 */
int stream_lazy_pending(const StreamContext *ctx)
{
    if (!ctx) return 0;
    return ctx->lazy_count;
}

/**
 * @brief 设置惰性模式的自动刷新阈值
 *
 * 当 lazy_threshold > 0 时，stream_emit 在懒队列达到阈值
 * 时自动调用 stream_flush() 刷新事件。
 *
 * @param ctx       流式上下文
 * @param threshold 事件数阈值（0 禁用自动刷新）
 */
void stream_set_lazy_threshold(StreamContext *ctx, int threshold)
{
    if (!ctx) return;
    ctx->lazy_threshold = (threshold > 0) ? threshold : 0;
}
