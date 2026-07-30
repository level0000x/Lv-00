#include "lv/lv_event_bus.h"
#include "lv/lv_utils.h"
#include "lv_internal.h"
#include <stdlib.h>
#include <string.h>

void lv_event_bus_init(lvEventBus *bus, const lvEventBusConfig *config) {
    memset(bus, 0, sizeof(*bus));
    if (config) {
        bus->config = *config;
    } else {
        bus->config = (lvEventBusConfig){ 16, 0 };
    }
    if (bus->config.initial_capacity <= 0)
        bus->config.initial_capacity = 16;
    bus->subscriptions = NULL;
    bus->subscription_count = 0;
    bus->subscription_capacity = 0;
    bus->next_id = 1;
}

void lv_event_bus_cleanup(lvEventBus *bus) {
    if (bus->subscriptions)
        lv_free((void **)&bus->subscriptions);
    memset(bus, 0, sizeof(*bus));
}

int lv_event_subscribe(lvEventBus *bus, int event_type, lvEventCallbackFn callback, void *user_data) {
    if (!bus || !callback)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "bus or callback is NULL");
    if (bus->config.max_callbacks > 0 && bus->subscription_count >= bus->config.max_callbacks)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "max callbacks reached");

    int idx = bus->subscription_count;
    if (!lv_ensure_capacity((void **)&bus->subscriptions, idx + 1, &bus->subscription_capacity,
                            sizeof(lvEventSubscription), 1))
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "failed to allocate subscription array");

    bus->subscriptions[idx].id = bus->next_id++;
    bus->subscriptions[idx].callback = callback;
    bus->subscriptions[idx].user_data = user_data;
    bus->subscriptions[idx].event_type = event_type;
    bus->subscriptions[idx].active = true;
    bus->subscription_count++;
    return bus->subscriptions[idx].id;
}

bool lv_event_unsubscribe(lvEventBus *bus, int subscription_id) {
    if (!bus || subscription_id <= 0)
        return false;
    for (int i = 0; i < bus->subscription_count; i++) {
        if (bus->subscriptions[i].id == subscription_id) {
            // Swap with last and decrement
            bus->subscriptions[i] = bus->subscriptions[--bus->subscription_count];
            // Clear the vacated slot
            memset(&bus->subscriptions[bus->subscription_count], 0, sizeof(lvEventSubscription));
            return true;
        }
    }
    return false;
}

void lv_event_emit(lvEventBus *bus, int event_type, void *event_data) {
    if (!bus)
        return;
    // Iterate all subscriptions. Use index-based loop since callbacks may unsubscribe.
    for (int i = 0; i < bus->subscription_count; ) {
        lvEventSubscription *sub = &bus->subscriptions[i];
        if (sub->active && (sub->event_type == -1 || sub->event_type == event_type)) {
            sub->callback(event_type, event_data, sub->user_data);
            // After callback, re-read since the array may have changed via unsubscription
            i = 0; // Reset scan - simple and safe
        } else {
            i++;
        }
    }
}
