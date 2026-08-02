/**
 * @file plugin_system_load.c
 * @brief LV-00 模块化插件系统 —— 插件加载与卸载
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

/* ============ 插件加载与卸载 ============ */

/**
 * @brief 从指定路径加载插件
 * @param system 插件系统指针
 * @param path 插件动态库文件路径
 * @return 成功返回插件指针，失败返回 NULL
 */
lvPlugin *lv_plugin_load(lvPluginSystem *system, const char *path) {
    if (!system)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_plugin_load: system is NULL");
    if (!path)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_plugin_load: path is NULL");
    if (system->plugin_count >= system->plugin_capacity) {
        set_error(system, "Plugin capacity exceeded");
        lv_RETURN_ERROR_NULL(lv_ERROR_RESOURCE_EXHAUSTED, "lv_plugin_load: plugin capacity exceeded");
    }

    /* 检查是否已加载 */
    for (size_t i = 0; i < system->plugin_count; i++) {
        if (system->plugins[i]->path[0] == '\0')
            continue;
        if (strcmp(system->plugins[i]->path, path) == 0) {
            set_error(system, "Plugin already loaded: %s", path);
            return NULL;
        }
    }

    /* 加载动态库 */
    void *handle = lv_dlopen(path);
    if (!handle) {
        set_error(system, "Failed to load library: %s", path);
        return NULL;
    }

    /* 创建插件对象 */
    lvPlugin *plugin = (lvPlugin *) lv_calloc(1, sizeof(lvPlugin));
    if (!plugin) {
        lv_dlclose(handle);
        return NULL;
    }
    strncpy(plugin->path, path, sizeof(plugin->path));
    plugin->path[sizeof(plugin->path) - 1] = '\0';
    plugin->handle = handle;
    plugin->state = lv_PLUGIN_STATE_LOADING;
    plugin->load_time = lv_get_wallclock_ns();

    /* 获取入口函数 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    typedef int (*PluginEntryFunc)(lvPluginContext *ctx);
    PluginEntryFunc entry = (PluginEntryFunc) lv_dlsym(handle, "lv_plugin_load_entry");
#pragma GCC diagnostic pop

    if (!entry) {
        set_error(system, "Plugin entry point not found: %s", path);
        lv_dlclose(handle);
        lv_free((void **) &plugin);
        return NULL;
    }

    /* 创建插件上下文 */
    plugin->context = (lvPluginContext *) lv_calloc(1, sizeof(lvPluginContext));
    if (!plugin->context) {
        lv_dlclose(handle);
        lv_free((void **) &plugin);
        return NULL;
    }
    plugin->context->plugin = plugin;
    plugin->context->system = system;
    plugin->context->lv_context = system->lv_context;

    /* 调用入口函数 */
    if (entry(plugin->context) != 0) {
        set_error(system, "Plugin entry function failed: %s", path);
        lv_free((void **) &plugin->context);
        lv_dlclose(handle);
        lv_free((void **) &plugin);
        return NULL;
    }

    /* 调用 on_load 回调 */
    if (plugin->on_load) {
        if (plugin->on_load(plugin->context) != 0) {
            set_error(system, "Plugin on_load failed: %s", path);
            lv_free((void **) &plugin->context);
            lv_dlclose(handle);
            lv_free((void **) &plugin);
            return NULL;
        }
    }

    plugin->state = lv_PLUGIN_STATE_LOADED;
    system->plugins[system->plugin_count++] = plugin;

    /* 发送加载事件 */
    lvPluginEvent event = {
        .type = lv_PLUGIN_EVENT_LOAD, .timestamp = lv_get_wallclock_ns(), .source = plugin, .target = NULL};

    if (system->event_handler) {
        system->event_handler(system, &event);
    }

    return plugin;
}

/**
 * @brief 卸载指定插件，释放其占用的资源
 * @param system 插件系统指针
 * @param plugin 待卸载的插件指针
 * @return 成功返回 0，失败返回 -1
 */
int lv_plugin_unload(lvPluginSystem *system, lvPlugin *plugin) {
    lv_CHECK_NOT_NULL(system);
    lv_CHECK_NOT_NULL(plugin);

    /* 停用插件 */
    if (plugin->state == lv_PLUGIN_STATE_ACTIVE) {
        lv_plugin_deactivate(plugin);
    }

    /* 注销所有接口 */
    if (plugin->registered_interfaces) {
        for (size_t i = 0; i < plugin->registered_interface_count; i++) {
            lv_plugin_unregister_interface(plugin, plugin->registered_interfaces[i]->name);
        }
    }

    /* 发送卸载事件 */
    lvPluginEvent event = {
        .type = lv_PLUGIN_EVENT_UNLOAD, .timestamp = lv_get_wallclock_ns(), .source = plugin, .target = NULL};

    if (system->event_handler) {
        system->event_handler(system, &event);
    }

    /* 调用 on_unload 回调 */
    if (plugin->on_unload) {
        plugin->on_unload(plugin->context);
    }

    /* 从系统列表中移除 */
    for (size_t i = 0; i < system->plugin_count; i++) {
        if (system->plugins[i] == plugin) {
            system->plugins[i] = system->plugins[--system->plugin_count];
            break;
        }
    }

    /* 清理资源 */
    if (plugin->context) {
        if (plugin->context->config) {
            lv_plugin_config_destroy(plugin->context->config);
        }
        lv_free((void **) &plugin->context);
    }

    if (plugin->registered_interfaces) {
        lv_free((void **) &plugin->registered_interfaces);
    }

    if (plugin->resolved_dependencies) {
        lv_free((void **) &plugin->resolved_dependencies);
    }

    plugin->state = lv_PLUGIN_STATE_UNLOADED;

    /* 卸载动态库 */
    if (plugin->handle) {
        lv_dlclose(plugin->handle);
    }

    lv_free((void **) &plugin);
    return 0;
}

/**
 * @brief 重新加载指定插件（卸载后重新加载）
 * @param system 插件系统指针
 * @param plugin 待重新加载的插件指针
 * @return 成功返回 0，失败返回 -1
 */
int lv_plugin_reload(lvPluginSystem *system, lvPlugin *plugin) {
    lv_CHECK_NOT_NULL(system);
    lv_CHECK_NOT_NULL(plugin);
    lv_CHECK_ARG(plugin->path[0] != '\0', lv_ERROR_INVALID_PARAM, "plugin path is empty");

    char path[lv_PLUGIN_PATH_MAX];
    strncpy(path, plugin->path, sizeof(path) - 1);

    if (lv_plugin_unload(system, plugin) != 0) {
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "lv_plugin_reload: unload failed");
    }

    if (!lv_plugin_load(system, path)) {
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "lv_plugin_reload: load failed");
    }
    return 0;
}

