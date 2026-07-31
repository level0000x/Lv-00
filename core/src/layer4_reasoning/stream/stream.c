/**
 * @file stream.c
 * @brief 流式输出系统实现 —— 引擎事件回调与实时状态推送
 *
 * @details 提供流式事件发射、回调注册、事件过滤、JSON 序列化、事件统计等核心功能。
 *          支撑 Web 前端实时可视化和证明步骤动画渲染。
 *
 *          功能模块:
 *            - 生命周期管理: 创建/销毁流式上下文
 *            - 回调管理: 注册/注销回调，支持事件类型过滤掩码
 *            - 事件发射: 立即/缓冲/节流/惰性四种模式
 *            - 惰性求值: 消费者主动拉取模式，阈值自动刷新
 *            - 异步模式: 基于环形缓冲区的多线程消费者模式（互斥锁+条件变量）
 *            - JSON 序列化: 手工拼接 JSON / JSON-RPC 字符串
 *            - 事件统计: 按类型计数、总数、丢弃数
 *            - 工具函数: 时间戳、事件类型名称/颜色/标识符、过滤掩码解析
 *
 *          该文件为全量重构版本：原文件因编码损坏导致部分注释和逻辑丢失，
 *          于 2026-05-20 基于头文件声明和功能规格重新实现，并通过回归测试验证。
 *
 * @author Lv-00 Project
 * @version 3.3.0  (惰性求值完整实现 2026-05-23)
 *
 * @dependencies
 *   - stream.h              : 流式输出系统公共接口定义
 *   - lv_utils.h          : 统一内存分配器
 *
 * @note 本模块无外部依赖（除 lv_utils），仅依赖标准 C 库。
 *       所有平台相关代码通过 #ifdef 隔离（Windows: windows.h/timeGetTime/process.h，
 *       类 Unix: sys/time.h/strings.h/pthread.h）。异步模式使用平台原生线程原语。
 */

#include "lv/lv_platform.h"
#include "lv/lv_thread.h"
#include "lv/stream.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "lv.h"
#include "lv_utils.h"
#include "lv/lv_str_utils.h"
#ifdef _WIN32
#include <windows.h>
#define strcasecmp _stricmp
#else
#include <strings.h>  /* strcasecmp：不区分大小写的字符串比较 */
#endif

/* ── 平台线程支持 ── */
/* 使用统一的 lv/lv_thread.h 抽象 */

/* ==================== 内部常量 ==================== */

/* ── 回调容量配置 ── */
#define STREAM_INITIAL_CALLBACKS 16 /**< 初始回调容量 */
#define STREAM_MAX_CALLBACKS 64     /**< 硬上限：防止无限内存消耗 */

/* ── 事件缓冲区配置 ── */
#define STREAM_INITIAL_BUFFER 64   /**< 初始事件缓冲区容量 */
#define STREAM_MAX_BUFFER 4096     /**< 硬上限：缓冲区最大事件数 */
#define STREAM_MAX_LAZY 8192       /**< 惰性队列最大容量 */
#define STREAM_DEFAULT_THROTTLE 50 /**< 默认节流间隔（毫秒） */

/* ── JSON 序列化配置 ── */
#define STREAM_JSON_INT_BUF 64 /**< 整数转字符串的临时缓冲区大小 */

/* ==================== 事件颜色常量 ==================== */

#define STREAM_COLOR_GREEN "#3fb950"      /**< 绿色：成功/开始/完成 */
#define STREAM_COLOR_RED "#f85149"        /**< 红色：错误 */
#define STREAM_COLOR_YELLOW "#d29922"     /**< 黄色：警告 */
#define STREAM_COLOR_ORANGE "#f0883e"     /**< 橙色：位数熔断 */
#define STREAM_COLOR_BLUE "#58a6ff"       /**< 蓝色：进度 */
#define STREAM_COLOR_GRAY "#8b949e"       /**< 灰色：一般信息 */
#define STREAM_COLOR_LIGHT_GRAY "#c9d1d9" /**< 浅灰：图快照/默认 */
#define STREAM_COLOR_PURPLE "#a371f7"     /**< 紫色：重写/求解/证明步骤 */
#define STREAM_COLOR_TEAL "#39d353"       /**< 青绿色：函数块系统 */
#define STREAM_COLOR_CYAN "#56d4dd"       /**< 青色：递归系统 */
#define STREAM_COLOR_PINK "#f778ba"       /**< 粉色：选择器分支 */

/* ==================== 数据结构 ==================== */

/**
 * @brief 回调记录
 *
 * 每个注册的回调对应一条记录，包含回调函数指针、用户数据、
 * 自增回调 ID 和事件类型过滤掩码。
 */
typedef struct {
    StreamCallback callback; /**< 回调函数指针 */
    void *user_data;         /**< 回调透传数据 */
    int id;                  /**< 自增回调 ID（>= 1），用于按 ID 注销和更新过滤 */
    uint64_t filter_mask;    /**< 事件类型过滤掩码（位与运算） */
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
    CallbackEntry *callbacks; /**< 已注册回调数组（堆分配，支持动态扩容） */
    int callback_count;       /**< 当前回调数量 */
    int callback_capacity;    /**< 当前数组容量 */

    /* ── 事件缓冲 / 发射策略 ── */
    StreamEmitMode emit_mode; /**< 当前发射策略 */
    long throttle_ms;         /**< 节流间隔（毫秒） */
    StreamEvent *buffer;      /**< 事件缓冲区（环形队列） */
    int buffer_count;         /**< 缓冲区中当前事件数 */
    int buffer_capacity;      /**< 缓冲区容量 */
    int buffer_head;          /**< 缓冲区读头（flush 位置） */
    long last_emit_ms;        /**< 上次发射时间戳（节流用） */

    /* ── 回调 ID 管理 ── */
    int next_callback_id; /**< 下一个可用的回调 ID（自增，从 1 开始） */

    /* ── 事件统计 ── */
    long event_counts[STREAM_EVENT_TYPE_COUNT]; /**< 各事件类型发射计数 */
    long total_count;                           /**< 事件发射总数 */
    long dropped_count;                         /**< 丢弃的事件数（缓冲区满时） */

    /* ── 惰性队列（LAZY 模式专用） ── */
    StreamEvent *lazy_queue; /**< 惰性事件队列（环形缓冲区） */
    int lazy_count;          /**< 惰性队列中当前事件数 */
    int lazy_capacity;       /**< 惰性队列容量 */
    int lazy_head;           /**< 惰性队列读头 */
    int lazy_threshold;      /**< 惰性自动刷新阈值（0=禁用） */

    /* ── 异步模式（多线程） ── */
    bool async_enabled;         /**< 是否启用了真正的异步模式 */
    bool async_running;         /**< 消费者线程运行标志 */
    lv_thread_t async_thread;   /**< 消费者线程句柄 */
    lv_mutex_t async_mutex;     /**< 保护环形缓冲区的互斥锁 */
    lv_cond_t async_cond_not_empty; /**< 条件变量：缓冲区非空时通知消费者 */
    lv_cond_t async_cond_flushed;   /**< 条件变量：队列排空时通知 flush 等待者 */
    int async_flush_waiters;    /**< 等待 flush 完成的线程数 */
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
    StreamContext *ctx = (StreamContext *) lv_calloc(1, sizeof(StreamContext));
    if (!ctx)
        return NULL;
    memset(ctx, 0, sizeof(StreamContext));

    /* 预分配初始容量的回调数组 */
    ctx->callbacks = (CallbackEntry *) lv_calloc(1, sizeof(CallbackEntry) * STREAM_INITIAL_CALLBACKS);
    if (!ctx->callbacks) {
        lv_free((void **) &ctx);
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

    /* 初始化异步模式字段 */
    ctx->async_enabled = false;
    ctx->async_running = false;
    memset(&ctx->async_thread, 0, sizeof(ctx->async_thread));
    memset(&ctx->async_mutex, 0, sizeof(ctx->async_mutex));
    memset(&ctx->async_cond_not_empty, 0, sizeof(ctx->async_cond_not_empty));
    memset(&ctx->async_cond_flushed, 0, sizeof(ctx->async_cond_flushed));
    ctx->async_flush_waiters = 0;

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
    if (!ctx)
        return;

    /* 如果异步模式已启用，先停止消费者线程 */
    if (ctx->async_enabled && ctx->async_running) {
        stream_set_async_mode(ctx, false, 0);
    }

    /* 清理异步同步原语（防御性清理） */
    lv_mutex_destroy(&ctx->async_mutex);
    lv_cond_destroy(&ctx->async_cond_not_empty);
    lv_cond_destroy(&ctx->async_cond_flushed);

    /* 释放事件缓冲区 */
    if (ctx->buffer) {
        lv_free((void **) &ctx->buffer);
    }
    /* 释放惰性队列 */
    if (ctx->lazy_queue) {
        lv_free((void **) &ctx->lazy_queue);
    }
    lv_free((void **) &ctx->callbacks);
    lv_free((void **) &ctx);
}

/**
 * @brief 确保回调数组有足够容量（动态扩容）
 *
 * @param ctx        流式上下文
 * @param min_capacity 所需的最小容量
 * @return true 容量足够或扩容成功，false 扩容失败或在硬上限
 */
static bool stream_ensure_capacity(StreamContext *ctx, int min_capacity) {
    if (min_capacity <= ctx->callback_capacity)
        return true;
    if (min_capacity > STREAM_MAX_CALLBACKS)
        return false; /* 超过硬上限 */

    int new_cap = ctx->callback_capacity;
    while (new_cap < min_capacity) {
        if (new_cap > STREAM_MAX_CALLBACKS / 2) {
            new_cap = STREAM_MAX_CALLBACKS;
            break;
        }
        new_cap *= 2;
    }
    /* 钳制到硬上限 */
    if (new_cap > STREAM_MAX_CALLBACKS)
        new_cap = STREAM_MAX_CALLBACKS;

    CallbackEntry *new_arr = (CallbackEntry *) lv_realloc(ctx->callbacks, (size_t) new_cap * sizeof(CallbackEntry));
    if (!new_arr)
        return false;

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
    if (!ctx || !callback)
        return false;

    /* 动态扩容确保足够容量 */
    if (!stream_ensure_capacity(ctx, ctx->callback_count + 1)) {
        return false; /* 超过硬上限或内存分配失败 */
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
int stream_register_callback_ex(StreamContext *ctx, StreamCallback callback, void *user_data, uint64_t filter_mask) {
    if (!ctx || !callback)
        return -1;

    /* 动态扩容确保足够容量 */
    if (!stream_ensure_capacity(ctx, ctx->callback_count + 1)) {
        return -1; /* 超过硬上限或内存分配失败 */
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
    if (!ctx || !callback)
        return false;

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
    if (!ctx || callback_id <= 0)
        return false;

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
    if (!ctx || callback_id <= 0)
        return false;

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
    if (!ctx || callback_id <= 0)
        return STREAM_FILTER_NONE;

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
    if (ctx->buffer_count < ctx->buffer_capacity)
        return true;
    if (ctx->buffer_capacity >= STREAM_MAX_BUFFER)
        return false;

    int new_cap = ctx->buffer_capacity == 0 ? STREAM_INITIAL_BUFFER : ctx->buffer_capacity * 2;
    if (new_cap > STREAM_MAX_BUFFER)
        new_cap = STREAM_MAX_BUFFER;

    StreamEvent *new_buf = (StreamEvent *) lv_realloc(ctx->buffer, (size_t) new_cap * sizeof(StreamEvent));
    if (!new_buf)
        return false;

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
    /* 快照当前回调数量，防止回调函数中注册/注销回调导致迭代器失效 */
    int saved_count = ctx->callback_count;
    for (int i = 0; i < saved_count; i++) {
        /* 检查索引是否仍然有效（回调可能已被注销导致前移） */
        if (i >= ctx->callback_count)
            break;
        if (ctx->callbacks[i].callback && (ctx->callbacks[i].filter_mask & event_bit) != 0) {
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
    if (ctx->last_emit_ms == 0 || (now - ctx->last_emit_ms) >= ctx->throttle_ms) {
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
    if (!ctx)
        return;
    int type_idx = (int) event->type;
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
    if (!ctx || !event)
        return;

    /* 更新事件统计（无论哪种发射模式都计数） */
    stream_update_stats(ctx, event);

    /* 异步模式：加锁入队 + 条件变量通知消费者 */
    if (ctx->async_enabled && ctx->async_running) {
        lv_mutex_lock(&ctx->async_mutex);
        if (ctx->buffer_count < ctx->buffer_capacity) {
            int write_pos = (ctx->buffer_head + ctx->buffer_count) % ctx->buffer_capacity;
            ctx->buffer[write_pos] = *event;
            ctx->buffer_count++;
        } else {
            ctx->dropped_count++;
        }
        lv_cond_signal(&ctx->async_cond_not_empty);
        lv_mutex_unlock(&ctx->async_mutex);
        return;
    }

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
            if (ctx->lazy_threshold > 0 && ctx->lazy_count >= ctx->lazy_threshold) {
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

/* ==================== 异步模式 API ==================== */

/* ==================== 异步消费者线程 ==================== */

static void *async_consumer_thread(void *arg) {
    StreamContext *ctx = (StreamContext *) arg;

    while (true) {
        lv_mutex_lock(&ctx->async_mutex);

        /* 等待缓冲区非空或停止信号 */
        while (ctx->buffer_count == 0 && ctx->async_running) {
            lv_cond_wait(&ctx->async_cond_not_empty, &ctx->async_mutex);
        }

        /* 检查停止信号 */
        if (!ctx->async_running) {
            /* 排空剩余事件 */
            while (ctx->buffer_count > 0) {
                StreamEvent ev = ctx->buffer[ctx->buffer_head];
                ctx->buffer_head = (ctx->buffer_head + 1) % ctx->buffer_capacity;
                ctx->buffer_count--;
                lv_mutex_unlock(&ctx->async_mutex);
                stream_dispatch(ctx, &ev);
                lv_mutex_lock(&ctx->async_mutex);
            }
            ctx->buffer_head = 0;

            /* 通知等待 flush 的线程 */
            if (ctx->async_flush_waiters > 0) {
                lv_cond_signal(&ctx->async_cond_flushed);
            }

            lv_mutex_unlock(&ctx->async_mutex);
            break;
        }

        /* 取出一个事件 */
        StreamEvent ev = ctx->buffer[ctx->buffer_head];
        ctx->buffer_head = (ctx->buffer_head + 1) % ctx->buffer_capacity;
        ctx->buffer_count--;

        /* 如果队列已空，通知等待 flush 的线程 */
        if (ctx->buffer_count == 0 && ctx->async_flush_waiters > 0) {
            lv_cond_signal(&ctx->async_cond_flushed);
        }

        lv_mutex_unlock(&ctx->async_mutex);

        /* 在锁外执行回调（避免死锁） */
        stream_dispatch(ctx, &ev);
    }

    return NULL;
}

/**
 * 设置异步模式。
 *
 * 启用真正的多线程异步事件分发：
 * - 启用时：创建消费者线程，事件通过互斥锁保护的环形缓冲区传递，
 *           消费者线程在条件变量上等待，有事件时自动唤醒并分发。
 * - 禁用时：通知消费者线程排空剩余事件后退出，销毁同步原语，
 *           恢复为 IMMEDIATE 模式。
 *
 * @param ctx      流式上下文
 * @param enabled  true 启用异步，false 恢复同步
 * @param capacity 队列容量（仅在 enabled=true 时生效，0 使用默认值 1024）
 * @return true 成功，false 失败（ctx 为 NULL 或内存不足）
 */
bool stream_set_async_mode(StreamContext *ctx, bool enabled, int capacity) {
    if (!ctx)
        return false;

    if (enabled) {
        /* 如果已经启用，不重复创建 */
        if (ctx->async_enabled && ctx->async_running)
            return true;

        /* 确保缓冲区已分配 */
        int buf_cap = capacity > 0 ? capacity : STREAM_ASYNC_QUEUE_DEFAULT_CAPACITY;
        if (buf_cap > STREAM_MAX_BUFFER)
            buf_cap = STREAM_MAX_BUFFER;

        if (!ctx->buffer || ctx->buffer_capacity < buf_cap) {
            StreamEvent *new_buf = (StreamEvent *) lv_realloc(ctx->buffer, (size_t) buf_cap * sizeof(StreamEvent));
            if (!new_buf)
                return false;
            ctx->buffer = new_buf;
            ctx->buffer_capacity = buf_cap;
            ctx->buffer_count = 0;
            ctx->buffer_head = 0;
        }

        /* 创建同步原语（栈分配 + 初始化） */
        lv_mutex_init(&ctx->async_mutex);
        lv_cond_init(&ctx->async_cond_not_empty);
        lv_cond_init(&ctx->async_cond_flushed);

        /* 启动消费者线程 */
        ctx->async_running = true;
        ctx->async_flush_waiters = 0;

        if (lv_thread_create(&ctx->async_thread, async_consumer_thread, ctx) != 0) {
            ctx->async_running = false;
            lv_cond_destroy(&ctx->async_cond_flushed);
            lv_cond_destroy(&ctx->async_cond_not_empty);
            lv_mutex_destroy(&ctx->async_mutex);
            return false;
        }

        ctx->async_enabled = true;
        ctx->emit_mode = STREAM_EMIT_BUFFERED;

        return true;
    } else {
        /* 禁用异步模式 */
        if (!ctx->async_enabled)
            return true;

        /* 通知消费者线程停止 */
        lv_mutex_lock(&ctx->async_mutex);
        ctx->async_running = false;
        lv_cond_signal(&ctx->async_cond_not_empty);
        lv_mutex_unlock(&ctx->async_mutex);

        /* 等待消费者线程退出 */
        lv_thread_join(ctx->async_thread);

        /* 销毁同步原语 */
        lv_cond_destroy(&ctx->async_cond_flushed);
        lv_cond_destroy(&ctx->async_cond_not_empty);
        lv_mutex_destroy(&ctx->async_mutex);

        ctx->async_enabled = false;
        ctx->async_running = false;

        /* 恢复为 IMMEDIATE 模式 */
        ctx->emit_mode = STREAM_EMIT_IMMEDIATE;

        return true;
    }
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
    if (!ctx)
        return;

    /* 异步模式：阻塞等待消费者线程排空队列 */
    if (ctx->async_enabled && ctx->async_running) {
        lv_mutex_lock(&ctx->async_mutex);
        ctx->async_flush_waiters++;
        while (ctx->buffer_count > 0 && ctx->async_running) {
            lv_cond_wait(&ctx->async_cond_flushed, &ctx->async_mutex);
        }
        ctx->async_flush_waiters--;
        lv_mutex_unlock(&ctx->async_mutex);
        return;
    }

    /* 同步模式：直接分发 */
    if (ctx->buffer_count == 0)
        return;

    while (ctx->buffer_count > 0) {
        StreamEvent *ev = &ctx->buffer[ctx->buffer_head];
        stream_dispatch(ctx, ev);
        ctx->buffer_head = (ctx->buffer_head + 1) % ctx->buffer_capacity;
        ctx->buffer_count--;
    }
    ctx->buffer_head = 0;
}

/**
 * 获取缓冲区中待处理事件数量。
 *
 * @param ctx 流式上下文
 * @return 待处理事件数，ctx 为 NULL 时返回 0
 */
int stream_pending_count(StreamContext *ctx) {
    if (!ctx)
        return 0;
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
        switch (*src) {
            case '"':
                if (written + 2 >= dest_size)
                    goto done;
                dest[written++] = '\\';
                dest[written++] = '"';
                break;
            case '\\':
                if (written + 2 >= dest_size)
                    goto done;
                dest[written++] = '\\';
                dest[written++] = '\\';
                break;
            case '\n':
                if (written + 2 >= dest_size)
                    goto done;
                dest[written++] = '\\';
                dest[written++] = 'n';
                break;
            case '\r':
                if (written + 2 >= dest_size)
                    goto done;
                dest[written++] = '\\';
                dest[written++] = 'r';
                break;
            case '\t':
                if (written + 2 >= dest_size)
                    goto done;
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

/* ==================== 事件统计 API ==================== */

/**
 * 重置事件统计计数器。
 * 将所有事件类型计数、总计数和丢弃计数清零。
 *
 * @param ctx 流式上下文
 */
void stream_reset_stats(StreamContext *ctx) {
    if (!ctx)
        return;
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
int64_t stream_get_event_count(const StreamContext *ctx, StreamEventType type) {
    if (!ctx)
        return 0;
    int idx = (int) type;
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
int64_t stream_get_total_event_count(StreamContext *ctx) {
    if (!ctx)
        return 0;
    return ctx->total_count;
}

/**
 * 获取已丢弃的事件数（缓冲区满时）。
 *
 * @param ctx 流式上下文
 * @return 丢弃的事件数，ctx 为 NULL 时返回 0
 */
long stream_get_dropped_count(const StreamContext *ctx) {
    if (!ctx)
        return 0;
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
    return (long) (lv_get_time_ns() / 1000000);
}

/* ============================================================
 * 事件类型映射表（数据驱动，替代 switch 语句）
 *
 * 统一管理事件类型的中文名称、英文标识符和前端颜色。
 * 新增事件类型时只需在此表添加一行，无需修改 3 个函数。
 * ============================================================ */

/** 事件类型映射表条目 */
typedef struct {
    StreamEventType type;   /**< 事件类型枚举值 */
    const char *name;       /**< 中文名称 */
    const char *id;         /**< 英文标识符 */
    const char *color;      /**< 前端颜色（十六进制） */
} StreamEventTypeEntry;

/** 事件类型映射表（按枚举值顺序排列，支持 O(1) 直接索引） */
static const StreamEventTypeEntry s_event_type_table[STREAM_EVENT_TYPE_COUNT] = {
    {STREAM_EVENT_ENGINE_START,              "引擎启动",       "ENGINE_START",              STREAM_COLOR_GREEN},
    {STREAM_EVENT_ENGINE_DONE,               "引擎完成",       "ENGINE_DONE",               STREAM_COLOR_GREEN},
    {STREAM_EVENT_ENGINE_PAUSED,             "引擎暂停",       "ENGINE_PAUSED",             STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_NORMALIZE_START,           "归一化开始",     "NORMALIZE_START",           STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_NORMALIZE_MERGE,           "节点合并",       "NORMALIZE_MERGE",           STREAM_COLOR_PURPLE},
    {STREAM_EVENT_NORMALIZE_DONE,            "归一化完成",     "NORMALIZE_DONE",            STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_REWRITE_START,             "重写开始",       "REWRITE_START",             STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_REWRITE_RULE_LOADED,       "规则加载",       "REWRITE_RULE_LOADED",       STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_REWRITE_MATCH_FOUND,       "匹配找到",       "REWRITE_MATCH_FOUND",       STREAM_COLOR_PURPLE},
    {STREAM_EVENT_REWRITE_APPLIED,           "规则应用",       "REWRITE_APPLIED",           STREAM_COLOR_PURPLE},
    {STREAM_EVENT_REWRITE_ROLLBACK,          "规则回滚",       "REWRITE_ROLLBACK",          STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_REWRITE_DONE,              "重写完成",       "REWRITE_DONE",              STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_SOLVE_START,               "求解开始",       "SOLVE_START",               STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_SOLVE_EQUATION_EXTRACTED,  "方程提取",       "SOLVE_EQUATION_EXTRACTED",  STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_SOLVE_GROEBNER_STEP,       "Gröbner基步骤",  "SOLVE_GROEBNER_STEP",       STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_SOLVE_VARIABLE_RESOLVED,   "变量解得",       "SOLVE_VARIABLE_RESOLVED",   STREAM_COLOR_PURPLE},
    {STREAM_EVENT_SOLVE_DONE,                "求解完成",       "SOLVE_DONE",                STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_PROOF_STEP_ADDED,          "证明步骤添加",   "PROOF_STEP_ADDED",          STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_PROOF_STEP_APPLIED,        "证明步骤应用",   "PROOF_STEP_APPLIED",        STREAM_COLOR_PURPLE},
    {STREAM_EVENT_PROOF_UNIFY,               "合一检查",       "PROOF_UNIFY",               STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_PROOF_COLOR_UPDATE,        "颜色更新",       "PROOF_COLOR_UPDATE",        STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_PROOF_DEPENDENCY_CHANGE,   "依赖链变化",     "PROOF_DEPENDENCY_CHANGE",   STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_FUNC_BLOCK_PACK_START,     "函数打包开始",   "FUNC_BLOCK_PACK_START",     STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_FUNC_BLOCK_PACK_DONE,      "函数打包完成",   "FUNC_BLOCK_PACK_DONE",      STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_FUNC_BLOCK_INSTANTIATE_START,"函数实例化开始","FUNC_BLOCK_INSTANTIATE_START",STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_FUNC_BLOCK_INSTANTIATE_DONE,"函数实例化完成","FUNC_BLOCK_INSTANTIATE_DONE",STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_FUNC_BLOCK_PARTIAL_APPLY,  "部分应用",       "FUNC_BLOCK_PARTIAL_APPLY",  STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_FUNC_BLOCK_DETERMINISM_CHECK,"确定性检查",   "FUNC_BLOCK_DETERMINISM_CHECK",STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_FUNC_BLOCK_CAPTURE_AVOID,  "捕获避免",       "FUNC_BLOCK_CAPTURE_AVOID",  STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_FUNC_BLOCK_CROSS_BOUNDARY, "跨边界操作",     "FUNC_BLOCK_CROSS_BOUNDARY", STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_PRESET_REGISTER_START,     "预设注册开始",   "PRESET_REGISTER_START",     STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_PRESET_REGISTER_DONE,      "预设注册完成",   "PRESET_REGISTER_DONE",      STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_PRESET_REGISTER_FAILED,    "预设注册失败",   "PRESET_REGISTER_FAILED",    STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_PRESET_LOOKUP,             "预设查找",       "PRESET_LOOKUP",             STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_PRESET_INSTANTIATE,        "预设实例化",     "PRESET_INSTANTIATE",        STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_PRESET_VALIDATE,           "预设验证",       "PRESET_VALIDATE",           STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_PRESET_CATEGORY_LOADED,    "预设类别加载",   "PRESET_CATEGORY_LOADED",    STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_PRESET_MODULE_LOADED,      "预设模块加载",   "PRESET_MODULE_LOADED",      STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_CONFLICT_DETECTED,         "冲突检测",       "CONFLICT_DETECTED",         STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_CONSTRAINT_ADDED,          "约束添加",       "CONSTRAINT_ADDED",          STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_NODE_ADDED,                "节点添加",       "NODE_ADDED",                STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_CIRCUIT_TRIP,              "位数熔断",       "CIRCUIT_TRIP",              STREAM_COLOR_ORANGE},
    {STREAM_EVENT_ERROR,                     "错误",           "ERROR",                     STREAM_COLOR_RED},
    {STREAM_EVENT_WARNING,                   "警告",           "WARNING",                   STREAM_COLOR_YELLOW},
    {STREAM_EVENT_INFO,                      "信息",           "INFO",                      STREAM_COLOR_GRAY},
    {STREAM_EVENT_PROGRESS,                  "进度",           "PROGRESS",                  STREAM_COLOR_BLUE},
    {STREAM_EVENT_GRAPH_SNAPSHOT,            "图快照",         "GRAPH_SNAPSHOT",            STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_BUS_EVENT,                 "事件总线",       "BUS_EVENT",                 STREAM_COLOR_GRAY},
};

/**
 * 获取事件类型的中文名称。
 * 用于前端 UI 显示和日志输出，将枚举值映射为可读的中文字符串。
 * @param type 事件类型枚举值
 * @return 中文名称字符串（静态常量，无需释放）
 */
const char *stream_event_type_name(StreamEventType type) {
    if (type >= 0 && type < STREAM_EVENT_TYPE_COUNT) {
        return s_event_type_table[type].name;
    }
    return "未知事件";
}

/**
 * 获取事件类型的英文标识符。
 * 用于 JSON 序列化和前端事件路由，返回大写字母+下划线格式的字符串。
 * @param type 事件类型枚举值
 * @return 英文标识符字符串（静态常量，无需释放）
 */
const char *stream_event_type_id(StreamEventType type) {
    if (type >= 0 && type < STREAM_EVENT_TYPE_COUNT) {
        return s_event_type_table[type].id;
    }
    return "UNKNOWN_EVENT";
}

/**
 * 获取事件类型对应的前端显示颜色（十六进制格式）。
 * 根据事件类型返回对应的 CSS 颜色字符串，用于 Web 前端渲染事件节点。
 * 颜色常量统一定义在文件顶部的 STREAM_COLOR_* 宏中。
 * @param type 事件类型枚举值
 * @return 十六进制颜色字符串（如 "#3fb950"）
 */
const char *stream_event_color(StreamEventType type) {
    if (type >= 0 && type < STREAM_EVENT_TYPE_COUNT) {
        return s_event_type_table[type].color;
    }
    return STREAM_COLOR_LIGHT_GRAY;
}

/* ==================== 过滤掩码解析 ==================== */

/**
 * @brief 从英文标识符查找事件类型枚举值
 *
 * @param id_str 事件类型英文标识符（如 "ENGINE_START"）
 * @return 事件类型枚举值，未找到时返回 -1
 */
static int stream_find_event_type_by_id(const char *id_str) {
    if (!id_str)
        return -1;

    /* 遍历所有事件类型，通过 stream_event_type_id 反向查找 */
    for (int i = 0; i < STREAM_EVENT_TYPE_COUNT; i++) {
        const char *eid = stream_event_type_id((StreamEventType) i);
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
    if (!category)
        return STREAM_FILTER_NONE;

    /* 不区分大小写比较 */
    if (strcasecmp(category, "engine") == 0) {
        return STREAM_EVENT_MASK(STREAM_EVENT_ENGINE_START) | STREAM_EVENT_MASK(STREAM_EVENT_ENGINE_DONE) |
               STREAM_EVENT_MASK(STREAM_EVENT_ENGINE_PAUSED);
    }
    if (strcasecmp(category, "normalize") == 0) {
        return STREAM_EVENT_MASK(STREAM_EVENT_NORMALIZE_START) | STREAM_EVENT_MASK(STREAM_EVENT_NORMALIZE_MERGE) |
               STREAM_EVENT_MASK(STREAM_EVENT_NORMALIZE_DONE);
    }
    if (strcasecmp(category, "rewrite") == 0) {
        return STREAM_EVENT_MASK(STREAM_EVENT_REWRITE_START) | STREAM_EVENT_MASK(STREAM_EVENT_REWRITE_RULE_LOADED) |
               STREAM_EVENT_MASK(STREAM_EVENT_REWRITE_MATCH_FOUND) | STREAM_EVENT_MASK(STREAM_EVENT_REWRITE_APPLIED) |
               STREAM_EVENT_MASK(STREAM_EVENT_REWRITE_ROLLBACK) | STREAM_EVENT_MASK(STREAM_EVENT_REWRITE_DONE);
    }
    if (strcasecmp(category, "solve") == 0) {
        return STREAM_EVENT_MASK(STREAM_EVENT_SOLVE_START) | STREAM_EVENT_MASK(STREAM_EVENT_SOLVE_EQUATION_EXTRACTED) |
               STREAM_EVENT_MASK(STREAM_EVENT_SOLVE_GROEBNER_STEP) |
               STREAM_EVENT_MASK(STREAM_EVENT_SOLVE_VARIABLE_RESOLVED) | STREAM_EVENT_MASK(STREAM_EVENT_SOLVE_DONE);
    }
    if (strcasecmp(category, "proof") == 0) {
        return STREAM_EVENT_MASK(STREAM_EVENT_PROOF_STEP_ADDED) | STREAM_EVENT_MASK(STREAM_EVENT_PROOF_STEP_APPLIED) |
               STREAM_EVENT_MASK(STREAM_EVENT_PROOF_UNIFY) | STREAM_EVENT_MASK(STREAM_EVENT_PROOF_COLOR_UPDATE) |
               STREAM_EVENT_MASK(STREAM_EVENT_PROOF_DEPENDENCY_CHANGE);
    }
    if (strcasecmp(category, "func_block") == 0) {
        return STREAM_EVENT_MASK(STREAM_EVENT_FUNC_BLOCK_PACK_START) |
               STREAM_EVENT_MASK(STREAM_EVENT_FUNC_BLOCK_PACK_DONE) |
               STREAM_EVENT_MASK(STREAM_EVENT_FUNC_BLOCK_INSTANTIATE_START) |
               STREAM_EVENT_MASK(STREAM_EVENT_FUNC_BLOCK_INSTANTIATE_DONE) |
               STREAM_EVENT_MASK(STREAM_EVENT_FUNC_BLOCK_PARTIAL_APPLY) |
               STREAM_EVENT_MASK(STREAM_EVENT_FUNC_BLOCK_DETERMINISM_CHECK) |
               STREAM_EVENT_MASK(STREAM_EVENT_FUNC_BLOCK_CAPTURE_AVOID) |
               STREAM_EVENT_MASK(STREAM_EVENT_FUNC_BLOCK_CROSS_BOUNDARY);
    }
    if (strcasecmp(category, "conflict") == 0) {
        return STREAM_EVENT_MASK(STREAM_EVENT_CONFLICT_DETECTED);
    }
    if (strcasecmp(category, "info") == 0) {
        return STREAM_EVENT_MASK(STREAM_EVENT_INFO) | STREAM_EVENT_MASK(STREAM_EVENT_PROGRESS) |
               STREAM_EVENT_MASK(STREAM_EVENT_GRAPH_SNAPSHOT);
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
    if (!str)
        return STREAM_FILTER_NONE;

    /* 去除首尾空白 */
    while (*str == ' ' || *str == '\t' || *str == '\r' || *str == '\n')
        str++;
    if (*str == '\0')
        return STREAM_FILTER_NONE;

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
    char *buf = (char *) lv_malloc(len + 1);
    if (!buf)
        return STREAM_FILTER_NONE;
    lv_strlcpy(buf, str, len + 1);

    /* 按逗号分词 */
    char *saveptr = NULL;
    char *token = lv_strtok_r(buf, ",", &saveptr);

    while (token) {
        /* 去除 token 首尾空白 */
        while (*token == ' ' || *token == '\t')
            token++;
        if (*token == '\0') {
            token = lv_strtok_r(NULL, ",", &saveptr);
            continue; /* 空 token，跳过 */
        }
        size_t tok_len = strlen(token);
        if (tok_len == 0) {
            token = lv_strtok_r(NULL, ",", &saveptr);
            continue;
        }
        char *end = token + tok_len - 1;
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
                    mask |= STREAM_EVENT_MASK((StreamEventType) type_idx);
                }
                /* 无法识别的 token 静默忽略 */
            }
        }

        token = lv_strtok_r(NULL, ",", &saveptr);
    }

    lv_free((void **) &buf);
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
        ctx->lazy_queue = (StreamEvent *) lv_calloc(1, sizeof(StreamEvent) * (size_t) ctx->lazy_capacity);
        return ctx->lazy_queue != NULL;
    }
    if (ctx->lazy_count >= ctx->lazy_capacity) {
        int new_cap = ctx->lazy_capacity * 2;
        if (new_cap > STREAM_MAX_LAZY) {
            ctx->dropped_count++;
            return false;
        }
        StreamEvent *new_queue = (StreamEvent *) lv_calloc((size_t) new_cap, sizeof(StreamEvent));
        if (!new_queue)
            return false;
        /* 拷贝环形缓冲区到线性数组 */
        for (int i = 0; i < ctx->lazy_count; i++) {
            int src = (ctx->lazy_head + i) % ctx->lazy_capacity;
            memcpy(&new_queue[i], &ctx->lazy_queue[src], sizeof(StreamEvent));
        }
        lv_free((void **) &ctx->lazy_queue);
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
    if (!ctx || !event)
        return;
    if (!stream_lazy_ensure_capacity(ctx))
        return;
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
const StreamEvent *stream_lazy_next(StreamContext *ctx) {
    if (!ctx || ctx->lazy_count == 0)
        return NULL;
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
int stream_lazy_drain(StreamContext *ctx, StreamCallback callback, void *user_data, int max_count) {
    if (!ctx || !callback || ctx->lazy_count == 0)
        return 0;

    int limit = max_count > 0 ? max_count : ctx->lazy_count;
    if (limit > ctx->lazy_count)
        limit = ctx->lazy_count;

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
int stream_lazy_pending(const StreamContext *ctx) {
    if (!ctx)
        return 0;
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
void stream_set_lazy_threshold(StreamContext *ctx, int threshold) {
    if (!ctx)
        return;
    ctx->lazy_threshold = (threshold > 0) ? threshold : 0;
}
