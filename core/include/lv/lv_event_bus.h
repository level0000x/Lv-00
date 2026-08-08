#ifndef lv_EVENT_BUS_H
#define lv_EVENT_BUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "lv/lv_callback_list.h"

/* 前向声明 —— 用于 StreamContext 桥接 */
struct StreamContext;

/**
 * @brief 事件订阅者回调函数类型
 * @param event_type  事件类型标识符（由调用方定义）
 * @param event_data  用户数据指针
 * @param user_data   注册时传入的用户自定义数据
 */
typedef void (*lvEventCallbackFn)(int event_type, void *event_data, void *user_data);

/**
 * @brief 事件总线配置
 */
typedef struct {
    int initial_capacity;   /**< 初始回调槽位数量（默认 16） */
    int max_callbacks;      /**< 最大回调数量（0 = 无限制，默认 0） */
} lvEventBusConfig;

#define lv_EVENT_BUS_DEFAULT_CONFIG { 16, 0 }

/** 事件总线 */
typedef struct lvEventBus {
    /** 订阅回调列表（基于公共设施 lvCallbackList 实现，
     *  条目 filter 字段存储订阅的事件类型，-1 = 监听所有） */
    lvCallbackList subscriptions;
    lvEventBusConfig config;

    /** 关联的 StreamContext（可选，非 NULL 时事件同时桥接到 Stream 系统） */
    struct StreamContext *stream_ctx;
} lvEventBus;

/**
 * @brief 初始化事件总线
 */
void lv_event_bus_init(lvEventBus *bus, const lvEventBusConfig *config);

/**
 * @brief 释放事件总线资源
 */
void lv_event_bus_cleanup(lvEventBus *bus);

/**
 * @brief 注册事件回调
 * @param bus        事件总线
 * @param event_type 监听的事件类型（-1 = 监听所有事件）
 * @param callback   回调函数（非 NULL）
 * @param user_data  用户自定义数据（透传给回调）
 * @return 订阅 ID（用于取消订阅），失败返回 -1
 */
int lv_event_subscribe(lvEventBus *bus, int event_type, lvEventCallbackFn callback, void *user_data);

/**
 * @brief 取消订阅
 * @param bus           事件总线
 * @param subscription_id lv_event_subscribe 返回的 ID
 * @return true 成功，false 未找到
 */
bool lv_event_unsubscribe(lvEventBus *bus, int subscription_id);

/**
 * @brief 发出事件
 * @param bus        事件总线
 * @param event_type 事件类型
 * @param event_data 事件数据指针（透传给回调）
 */
void lv_event_emit(lvEventBus *bus, int event_type, void *event_data);

/**
 * @brief 关联 StreamContext（可选）
 *
 * 设置后，所有 lv_event_emit() 发出的事件会自动以 STREAM_EVENT_BUS_EVENT
 * 类型投射到 Stream 系统，原始 event_type 存储在 StreamEvent.rule_id 中。
 * 传入 NULL 可解除关联。
 *
 * 调用方：runtime_monitor 的 lv_event_trace_set_stream_context() 在引擎
 * 初始化路径（stream_context 分发机制）调用一次；engine 销毁时以 NULL
 * 解除。可先于 lv_event_bus_init() 调用（init 保留预置的 stream_ctx）。
 *
 * @param bus        事件总线
 * @param stream_ctx StreamContext 指针（或 NULL）
 */
void lv_event_bus_set_stream(lvEventBus *bus, struct StreamContext *stream_ctx);

/**
 * @brief 获取关联的 StreamContext
 *
 * @param bus 事件总线
 * @return 关联的 StreamContext 指针，未关联时返回 NULL
 */
struct StreamContext *lv_event_bus_get_stream(const lvEventBus *bus);

#ifdef __cplusplus
}
#endif

#endif /* lv_EVENT_BUS_H */
