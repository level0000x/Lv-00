/**
 * @file stream_internal.h
 * @brief Internal shared definitions for the stream module.
 */

#ifndef lv_STREAM_INTERNAL_H
#define lv_STREAM_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "lv.h"
#include "lv_utils.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_thread.h"
#include "lv/lv_callback_list.h"

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
 * @brief 流式上下文
 *
 * 回调列表基于公共设施 lvCallbackList（初始容量 STREAM_INITIAL_CALLBACKS，
 * 最多扩容到 STREAM_MAX_CALLBACKS，超过硬上限后注册失败）。
 * 回调条目的 filter 字段存储事件类型过滤掩码（uint64_t 位掩码）。
 *
 * 事件缓冲区用于 BUFFERED 和 THROTTLED 模式：
 * - BUFFERED: 事件入队，等待 stream_flush() 手动刷新
 * - THROTTLED: 事件入队，按时间间隔自动刷新
 *
 * 事件统计数组记录每种事件类型的发射次数。
 */
struct StreamContext {
    lvCallbackList callback_list; /**< 已注册回调列表（公共设施，支持动态扩容） */

    /* ── 事件缓冲 / 发射策略 ── */
    StreamEmitMode emit_mode; /**< 当前发射策略 */
    long throttle_ms;         /**< 节流间隔（毫秒） */
    StreamEvent *buffer;      /**< 事件缓冲区（环形队列） */
    int buffer_count;         /**< 缓冲区中当前事件数 */
    int buffer_capacity;      /**< 缓冲区容量 */
    int buffer_head;          /**< 缓冲区读头（flush 位置） */
    long last_emit_ms;        /**< 上次发射时间戳（节流用） */

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


/* ---- shared helpers (defined in stream_buffer.c) ---- */
void stream_dispatch(StreamContext *ctx, const StreamEvent *event);
/* ---- shared helpers (defined in stream_lazy.c) ---- */
void stream_lazy_enqueue(StreamContext *ctx, const StreamEvent *event);

#ifdef __cplusplus
}
#endif

#endif /* lv_STREAM_INTERNAL_H */
