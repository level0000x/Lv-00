/**
 * @file plugin_system_state.c
 * @brief LV-00 模块化插件系统 —— 插件激活与停用
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

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include "lv/lv_strbuf.h"
#endif

#include "plugin_system_internal.h"

/* ============ 插件激活与停用 ============ */

/**
 * @brief 激活插件，解析依赖并调用激活回调
 * @param plugin 待激活的插件指针
 * @return 成功返回 0，失败返回 -1
 */
int lv_plugin_activate(lvPlugin *plugin) {
    lv_CHECK_NOT_NULL(plugin);
    lv_CHECK_ARG(plugin->state == lv_PLUGIN_STATE_LOADED, lv_ERROR_INVALID_STATE,
                 "plugin state is not LOADED (state=%d)", plugin->state);

    plugin->state = lv_PLUGIN_STATE_INITIALIZING;

    /* 解析依赖 */
    if (lv_plugin_resolve_dependencies(plugin->context->system, plugin) != 0) {
        plugin->state = lv_PLUGIN_STATE_ERROR;
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "lv_plugin_activate: resolve_dependencies failed");
    }

    /* 调用 on_activate 回调 */
    if (plugin->on_activate) {
        if (plugin->on_activate(plugin->context) != 0) {
            plugin->state = lv_PLUGIN_STATE_ERROR;
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "lv_plugin_activate: on_activate callback failed");
        }
    }

    plugin->state = lv_PLUGIN_STATE_ACTIVE;
    plugin->activate_time = lv_get_wallclock_ns();

    /* 发送激活事件 */
    lvPluginEvent event = {
        .type = lv_PLUGIN_EVENT_ACTIVATE, .timestamp = lv_get_wallclock_ns(), .source = plugin, .target = NULL};

    if (plugin->context->system->event_handler) {
        plugin->context->system->event_handler(plugin->context->system, &event);
    }

    return 0;
}

/**
 * @brief 停用插件，调用停用回调并发送停用事件
 * @param plugin 待停用的插件指针
 * @return 成功返回 0，失败返回 -1
 */
int lv_plugin_deactivate(lvPlugin *plugin) {
    lv_CHECK_NOT_NULL(plugin);
    lv_CHECK_ARG(plugin->state == lv_PLUGIN_STATE_ACTIVE, lv_ERROR_INVALID_STATE,
                 "plugin state is not ACTIVE (state=%d)", plugin->state);

    plugin->state = lv_PLUGIN_STATE_DEACTIVATING;

    /* 调用 on_deactivate 回调 */
    if (plugin->on_deactivate) {
        plugin->on_deactivate(plugin->context);
    }

    plugin->state = lv_PLUGIN_STATE_LOADED;

    /* 发送停用事件 */
    lvPluginEvent event = {
        .type = lv_PLUGIN_EVENT_DEACTIVATE, .timestamp = lv_get_wallclock_ns(), .source = plugin, .target = NULL};

    if (plugin->context->system->event_handler) {
        plugin->context->system->event_handler(plugin->context->system, &event);
    }

    return 0;
}

/**
 * @brief 检查插件是否处于激活状态
 * @param plugin 插件指针
 * @return 激活返回 1，否则返回 0
 */
int lv_plugin_is_active(const lvPlugin *plugin) {
    return plugin && plugin->state == lv_PLUGIN_STATE_ACTIVE;
}

/**
 * @brief 获取插件的当前状态
 * @param plugin 插件指针
 * @return 插件状态枚举值
 */
lvPluginState lv_plugin_get_state(const lvPlugin *plugin) {
    return plugin ? plugin->state : lv_PLUGIN_STATE_UNLOADED;
}

