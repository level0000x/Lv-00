#include "lv/lv_event_bus.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"
#include "lv_internal.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * 分发参数包装：event_type + event_data
 * ============================================================ */
typedef struct {
    int event_type;
    void *event_data;
} EventBusDispatchArgs;

/** @brief 过滤：事件类型匹配（订阅 event_type 为 -1 = 监听所有） */
static bool ev_bus_filter(const lvCallbackEntry *entry, const void *arg) {
    const EventBusDispatchArgs *d = (const EventBusDispatchArgs *) arg;
    uint64_t et = (uint64_t) d->event_type;
    return entry->filter == (uint64_t) -1 || entry->filter == et;
}

/** @brief 调用：将泛型回调转回 lvEventCallbackFn 签名后调用 */
static void ev_bus_invoke(const lvCallbackEntry *entry, const void *arg) {
    const EventBusDispatchArgs *d = (const EventBusDispatchArgs *) arg;
    lvEventCallbackFn cb = (lvEventCallbackFn) entry->callback;
    cb(d->event_type, d->event_data, entry->user_data);
}

void lv_event_bus_init(lvEventBus *bus, const lvEventBusConfig *config) {
    memset(bus, 0, sizeof(*bus));
    if (config) {
        bus->config = *config;
    } else {
        bus->config = (lvEventBusConfig){ 16, 0 };
    }
    if (bus->config.initial_capacity <= 0)
        bus->config.initial_capacity = 16;
    /* 订阅列表委托公共设施：初始容量 initial_capacity，硬上限 max_callbacks（0 = 无限制） */
    lv_callback_list_init(&bus->subscriptions, bus->config.initial_capacity, bus->config.max_callbacks);
}

void lv_event_bus_cleanup(lvEventBus *bus) {
    if (!bus)
        return;
    lv_callback_list_cleanup(&bus->subscriptions);
    memset(bus, 0, sizeof(*bus));
}

int lv_event_subscribe(lvEventBus *bus, int event_type, lvEventCallbackFn callback, void *user_data) {
    if (!bus || !callback)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "bus or callback is NULL");
    if (bus->config.max_callbacks > 0 &&
        lv_callback_list_count(&bus->subscriptions) >= bus->config.max_callbacks)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "max callbacks reached");

    /* 订阅 ID 由公共设施自增分配（>= 1），event_type 存入 filter 字段 */
    return lv_callback_list_add(&bus->subscriptions, (lvCallbackFn) callback, user_data,
                                (uint64_t) event_type);
}

bool lv_event_unsubscribe(lvEventBus *bus, int subscription_id) {
    if (!bus || subscription_id <= 0)
        return false;
    return lv_callback_list_remove_by_id(&bus->subscriptions, subscription_id);
}

void lv_event_bus_set_stream(lvEventBus *bus, struct StreamContext *stream_ctx) {
    if (!bus)
        return;
    bus->stream_ctx = stream_ctx;
}

struct StreamContext *lv_event_bus_get_stream(const lvEventBus *bus) {
    if (!bus)
        return NULL;
    return bus->stream_ctx;
}

void lv_event_emit(lvEventBus *bus, int event_type, void *event_data) {
    if (!bus)
        return;

    /* ---- 原有分发逻辑：通知所有 lvEventBus 订阅者 ----
     * 委托公共设施分发（迭代安全：遍历中注销/注册安全），
     * 订阅者按注册顺序调用，与原有语义一致。 */
    EventBusDispatchArgs args;
    args.event_type = event_type;
    args.event_data = event_data;
    lv_callback_list_dispatch(&bus->subscriptions, &args, ev_bus_filter, ev_bus_invoke);

    /* ---- Stream 桥接：若关联了 StreamContext，同步投射到 Stream 系统 ---- */
    if (bus->stream_ctx) {
        StreamEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = STREAM_EVENT_BUS_EVENT;
        ev.timestamp_ms = stream_timestamp_ms();
        ev.rule_id = event_type;          /* 原始 event_type 存储在 rule_id 中 */
        ev.step_number = -1;
        ev.node_id = -1;
        ev.constraint_id = -1;
        ev.var_id = -1;
        ev.total_steps = -1;
        ev.progress = -1.0;
        /* 注意：event_data (void*) 无法安全通过 StreamEvent 传递
         * （void* 在异步/缓冲模式下会导致悬空指针），
         * Stream 消费者如需获取事件数据应使用 lv_event_subscribe() 直接订阅。 */
        stream_emit(bus->stream_ctx, &ev);
    }
}
