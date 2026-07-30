/**
 * @file lv_events.h
 * @brief 统一事件系统 —— 整合 lvEventBus 与 Stream 事件体系
 *
 * @details 本文件提供 lvEventBus（模块间消息）和 Stream 系统
 *          （引擎前端实时推送）的统一包含入口。
 *
 *           统一策略：
 *            - Stream 系统作为主要事件骨干，提供 4 种发射模式
 *              （立即/缓冲/节流/惰性）、异步消费者线程、JSON 序列化
 *              和 64 位位掩码过滤。
 *            - lvEventBus 保持向后兼容的订阅/发布 API，
 *              内部通过 StreamContext 将事件同步桥接到 Stream 系统。
 *            - 通过 lv_event_bus_set_stream() 关联 StreamContext 后，
 *              所有 lv_event_emit() 发出的事件自动以 STREAM_EVENT_BUS_EVENT
 *              类型出现在 Stream 系统中，Stream 消费者可通过
 *              event->rule_id 获取原始的 event_type。
 *
 *           使用方式：
 *            - 新代码可直接使用 stream_emit() / stream_register_callback()
 *              获得完整功能
 *            - 旧代码继续使用 lv_event_emit() / lv_event_subscribe()，
 *              无需任何修改
 *            - 需要桥接时调用 lv_event_bus_set_stream(bus, ctx)
 *              即可将事件总线事件投射到 Stream 系统
 *            - 包含本文件等价于同时包含 stream.h 和 lv_event_bus.h
 *
 * @author Lv-00 Project
 */

#ifndef lv_EVENTS_H
#define lv_EVENTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv/stream.h"
#include "lv/lv_event_bus.h"

#ifdef __cplusplus
}
#endif

#endif /* lv_EVENTS_H */
