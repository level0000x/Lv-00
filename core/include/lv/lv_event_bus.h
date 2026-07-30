#ifndef lv_EVENT_BUS_H
#define lv_EVENT_BUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

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

/** 事件订阅记录 */
typedef struct lvEventSubscription {
    int id;                    /**< 唯一订阅 ID */
    lvEventCallbackFn callback; /**< 回调函数 */
    void *user_data;           /**< 用户数据 */
    int event_type;            /**< 监听的事件类型（-1 = 全部） */
    bool active;               /**< 是否激活 */
} lvEventSubscription;

/** 事件总线 */
typedef struct lvEventBus {
    lvEventSubscription *subscriptions;
    int subscription_count;
    int subscription_capacity;
    int next_id;
    lvEventBusConfig config;
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

#ifdef __cplusplus
}
#endif

#endif /* lv_EVENT_BUS_H */
