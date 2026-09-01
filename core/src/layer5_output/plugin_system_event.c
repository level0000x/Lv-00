/**
 * @file plugin_system_event.c
 * @brief LV-00 模块化插件系统 —— 事件系统
 *
 * @details 由 plugin_system.c 按功能域拆分而来。
 *          共享内部数据结构与辅助函数见 plugin_system_internal.h。
 *
 * @author Lv-00 Project
 * @version 1.0
 */

#include "lv/plugin_system.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_check.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_utils.h"

#include "plugin_system_internal.h"

/* ============ 事件系统 ============ */

/**
 * @brief 发送事件到指定插件
 * @param plugin 目标插件指针
 * @param type 事件类型
 * @param data 事件数据
 * @param data_size 数据大小
 * @return 成功返回 0，失败返回 -1
 */
int lv_plugin_send_event(lvPlugin *plugin, lvPluginEventType type, void *data, size_t data_size) {
    lv_CHECK_NOT_NULL(plugin);
    lv_CHECK_ARG(plugin->context != NULL, lv_ERROR_NULL_POINTER, "plugin context is NULL");

    lvPluginEvent event = {.type = type,
                           .timestamp = lv_get_wallclock_ns(),
                           .source = plugin,
                           .data = data,
                           .data_size = data_size,
                           .target = NULL};

    if (plugin->on_event) {
        plugin->on_event(plugin->context, &event);
    }

    return 0;
}

/**
 * @brief 向系统中所有已加载的插件广播事件
 * @param system 插件系统指针
 * @param type 事件类型
 * @param data 事件数据
 * @param data_size 数据大小
 * @return 成功返回 0，失败返回 -1
 */
int lv_plugin_broadcast_event(lvPluginSystem *system, lvPluginEventType type, void *data, size_t data_size) {
    lv_CHECK_NOT_NULL(system);

    lvPluginEvent event = {.type = type,
                           .timestamp = lv_get_wallclock_ns(),
                           .source = NULL,
                           .data = data,
                           .data_size = data_size,
                           .target = NULL};

    for (size_t i = 0; i < system->plugin_count; i++) {
        event.source = system->plugins[i];
        if (!system->plugins[i]->context)
            continue;
        if (system->plugins[i]->on_event) {
            system->plugins[i]->on_event(system->plugins[i]->context, &event);
        }
    }

    return 0;
}

/**
 * @brief 设置插件系统的事件处理器
 * @param system 插件系统指针
 * @param handler 事件处理回调函数
 */
void lv_plugin_set_event_handler(lvPluginSystem *system, void (*handler)(lvPluginSystem *, const lvPluginEvent *)) {
    if (system) {
        system->event_handler = handler;
    }
}

