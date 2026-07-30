/**
 * @file plugin_system.c
 * @brief LV-00 模块化插件系统实现
 *
 * 实现插件加载、卸载、接口注册机制和插件间通信
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

/* ============ 内部数据结构 ============ */

typedef struct {
    char last_error[1024];
} PluginSystemInternal;

/* ============ 辅助函数 ============ */

/* 设置系统错误消息（支持 printf 风格格式化） */
static void set_error(lvPluginSystem *system, const char *format, ...) {
    if (!system)
        return;

    PluginSystemInternal *internal = (PluginSystemInternal *) system->mutex;
    if (!internal)
        return;

    va_list args;
    va_start(args, format);
    vsnprintf(internal->last_error, sizeof(internal->last_error), format, args);
    va_end(args);
}

/* ============ 生命周期管理 ============ */

/**
 * @brief 创建插件系统实例
 * @param ctx LV-00 上下文指针
 * @return 成功返回插件系统指针，失败返回 NULL
 */
lvPluginSystem *lv_plugin_system_create(lvContext *ctx) {
    lvPluginSystem *system = (lvPluginSystem *) lv_calloc(1, sizeof(lvPluginSystem));
    if (!system)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_plugin_system_create: lv_calloc failed");

    memset(system, 0, sizeof(lvPluginSystem));

    system->lv_context = ctx;
    system->version =
        (lv_PLUGIN_SYSTEM_VERSION_MAJOR << 16) | (lv_PLUGIN_SYSTEM_VERSION_MINOR << 8) | lv_PLUGIN_SYSTEM_VERSION_PATCH;

    system->plugin_capacity = lv_MAX_PLUGINS;
    system->plugins = (lvPlugin **) lv_malloc(sizeof(lvPlugin *) * system->plugin_capacity);
    if (!system->plugins) {
        lv_free((void **) &system);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_plugin_system_create: plugins malloc failed");
    }

    system->interface_capacity = lv_MAX_INTERFACES;
    system->interfaces = (lvPluginInterface **) lv_malloc(sizeof(lvPluginInterface *) * system->interface_capacity);
    if (!system->interfaces) {
        lv_free((void **) &system->plugins);
        lv_free((void **) &system);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_plugin_system_create: interfaces malloc failed");
    }

    lv_darray_init(&system->search_paths, sizeof(char *));

    PluginSystemInternal *internal = (PluginSystemInternal *) lv_calloc(1, sizeof(PluginSystemInternal));
    if (!internal) {
        lv_free((void **) &system->interfaces);
        lv_free((void **) &system->plugins);
        lv_free((void **) &system);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_plugin_system_create: internal calloc failed");
    }

    memset(internal, 0, sizeof(PluginSystemInternal));
    system->mutex = internal;

    return system;
}

/**
 * @brief 销毁插件系统实例，释放所有相关资源
 * @param system 插件系统指针
 */
void lv_plugin_system_destroy(lvPluginSystem *system) {
    if (!system)
        return;

    lv_plugin_system_cleanup(system);

    if (system->plugins)
        lv_free((void **) &system->plugins);
    if (system->interfaces)
        lv_free((void **) &system->interfaces);

    for (int i = 0; i < system->search_paths.count; i++) {
        lv_free((void **) lv_darray_get(&system->search_paths, i));
    }
    lv_darray_free(&system->search_paths);

    if (system->mutex)
        lv_free((void **) &system->mutex);
    lv_free((void **) &system);
}

/**
 * @brief 初始化插件系统
 * @param system 插件系统指针
 * @return 成功返回 0，失败返回 -1
 */
int lv_plugin_system_init(lvPluginSystem *system) {
    lv_CHECK_NOT_NULL(system);

    system->initialized = 1;
    return 0;
}

/**
 * @brief 清理插件系统，卸载所有已加载的插件
 * @param system 插件系统指针
 */
void lv_plugin_system_cleanup(lvPluginSystem *system) {
    if (!system)
        return;
    if (!system->plugins || system->plugin_count <= 0)
        return;

    /* 卸载所有插件 */
    while (system->plugin_count > 0) {
        lv_plugin_unload(system, system->plugins[0]);
    }

    system->initialized = 0;
}

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

/* ============ 插件查询 ============ */

/**
 * @brief 按名称查找已加载的插件
 * @param system 插件系统指针
 * @param name 插件名称
 * @return 找到返回插件指针，未找到返回 NULL
 */
lvPlugin *lv_plugin_find(lvPluginSystem *system, const char *name) {
    if (!system)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_plugin_find: system is NULL");
    if (!name)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_plugin_find: name is NULL");

    for (size_t i = 0; i < system->plugin_count; i++) {
        if (strcmp(system->plugins[i]->info.name, name) == 0) {
            return system->plugins[i];
        }
    }
    return NULL;
}

/**
 * @brief 获取所有已加载插件的数组
 * @param system 插件系统指针
 * @param count 输出参数，插件数量
 * @return 返回插件指针数组，失败返回 NULL
 */
lvPlugin **lv_plugin_get_all(lvPluginSystem *system, size_t *count) {
    if (!system)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_plugin_get_all: system is NULL");
    if (!count)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_plugin_get_all: count is NULL");

    *count = system->plugin_count;
    return system->plugins;
}

/**
 * @brief 按类型筛选已加载的插件
 * @param system 插件系统指针
 * @param type 插件类型
 * @param count 输出参数，匹配的插件数量
 * @return 成功返回匹配的插件指针数组（需调用者释放），失败返回 NULL
 */
lvPlugin **lv_plugin_get_by_type(lvPluginSystem *system, lvPluginType type, size_t *count) {
    if (!system || !count)
        return NULL;

    /* 统计匹配数量 */
    size_t match_count = 0;
    for (size_t i = 0; i < system->plugin_count; i++) {
        if (system->plugins[i]->info.type == type) {
            match_count++;
        }
    }

    if (match_count == 0) {
        *count = 0;
        return NULL;
    }

    /* 分配结果数组 */
    lvPlugin **result = (lvPlugin **) lv_malloc(sizeof(lvPlugin *) * match_count);
    if (!result) {
        *count = 0;
        return NULL;
    }

    /* 填充结果 */
    size_t idx = 0;
    for (size_t i = 0; i < system->plugin_count; i++) {
        if (system->plugins[i]->info.type == type) {
            result[idx++] = system->plugins[i];
        }
    }

    *count = match_count;
    return result;
}

/**
 * @brief 按状态筛选已加载的插件
 * @param system 插件系统指针
 * @param state 插件状态
 * @param count 输出参数，匹配的插件数量
 * @return 成功返回匹配的插件指针数组（需调用者释放），失败返回 NULL
 */
lvPlugin **lv_plugin_get_by_state(lvPluginSystem *system, lvPluginState state, size_t *count) {
    if (!system || !count)
        return NULL;

    /* 统计匹配数量 */
    size_t match_count = 0;
    for (size_t i = 0; i < system->plugin_count; i++) {
        if (system->plugins[i]->state == state) {
            match_count++;
        }
    }

    if (match_count == 0) {
        *count = 0;
        return NULL;
    }

    /* 分配结果数组 */
    lvPlugin **result = (lvPlugin **) lv_malloc(sizeof(lvPlugin *) * match_count);
    if (!result) {
        *count = 0;
        return NULL;
    }

    /* 填充结果 */
    size_t idx = 0;
    for (size_t i = 0; i < system->plugin_count; i++) {
        if (system->plugins[i]->state == state) {
            result[idx++] = system->plugins[i];
        }
    }

    *count = match_count;
    return result;
}

/* ============ 接口注册与查询 ============ */

/**
 * @brief 注册插件接口到系统和插件注册表
 * @param plugin 注册接口的插件指针
 * @param iface 待注册的接口指针
 * @return 成功返回 0，失败返回 -1
 */
int lv_plugin_register_interface(lvPlugin *plugin, lvPluginInterface *iface) {
    lv_CHECK_NOT_NULL(plugin);
    lv_CHECK_ARG(plugin->context != NULL, lv_ERROR_NULL_POINTER, "plugin context is NULL");
    lv_CHECK_NOT_NULL(iface);
    lv_CHECK_ARG(plugin->registered_interface_count < lv_MAX_INTERFACES, lv_ERROR_RESOURCE_EXHAUSTED,
                 "max interfaces reached");

    /* 检查是否已注册 */
    for (size_t i = 0; i < plugin->registered_interface_count; i++) {
        if (strcmp(plugin->registered_interfaces[i]->name, iface->name) == 0) {
            lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "lv_plugin_register_interface: interface already registered");
        }
    }

    /* 添加到插件注册表 */
    if (!plugin->registered_interfaces) {
        plugin->registered_interfaces =
            (lvPluginInterface **) lv_malloc(sizeof(lvPluginInterface *) * lv_MAX_INTERFACES);
        if (!plugin->registered_interfaces)
            lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_plugin_register_interface: malloc failed");
    }

    iface->owner = plugin;
    plugin->registered_interfaces[plugin->registered_interface_count++] = iface;

    /* 添加到系统注册表 */
    lvPluginSystem *system = plugin->context->system;
    if (system->interface_count < system->interface_capacity) {
        system->interfaces[system->interface_count++] = iface;
    }

    return 0;
}

/**
 * @brief 从插件和系统中注销指定名称的接口
 * @param plugin 注销接口的插件指针
 * @param name 接口名称
 * @return 成功返回 0，失败返回 -1
 */
int lv_plugin_unregister_interface(lvPlugin *plugin, const char *name) {
    lv_CHECK_NOT_NULL(plugin);
    lv_CHECK_ARG(plugin->context != NULL, lv_ERROR_NULL_POINTER, "plugin context is NULL");
    lv_CHECK_NOT_NULL(name);

    /* 从插件注册表中移除 */
    for (size_t i = 0; i < plugin->registered_interface_count; i++) {
        if (strcmp(plugin->registered_interfaces[i]->name, name) == 0) {
            /* 从系统注册表中移除 */
            lvPluginSystem *system = plugin->context->system;
            for (size_t j = 0; j < system->interface_count; j++) {
                if (system->interfaces[j] == plugin->registered_interfaces[i]) {
                    system->interfaces[j] = system->interfaces[--system->interface_count];
                    break;
                }
            }

            plugin->registered_interfaces[i] = plugin->registered_interfaces[--plugin->registered_interface_count];
            return 0;
        }
    }

    lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "lv_plugin_unregister_interface: interface not found");
}

/**
 * @brief 按名称和版本精确查询已注册的接口
 * @param system 插件系统指针
 * @param name 接口名称
 * @param version 接口版本号
 * @return 找到返回接口指针，未找到返回 NULL
 */
lvPluginInterface *lv_plugin_query_interface(lvPluginSystem *system, const char *name, uint32_t version) {
    if (!system || !name)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_plugin_query_interface: system or name is NULL");

    for (size_t i = 0; i < system->interface_count; i++) {
        if (strcmp(system->interfaces[i]->name, name) == 0 && system->interfaces[i]->version == version) {
            return system->interfaces[i];
        }
    }
    return NULL;
}

/* 通配符模式匹配：支持 '*' 和 '?' glob 通配符 */
static int wildcard_match(const char *pattern, const char *str) {
    if (!pattern || !str)
        return 0;

    const char *p = pattern;
    const char *s = str;
    const char *star_p = NULL;
    const char *star_s = NULL;

    while (*s) {
        if (*p == '*') {
            /* 记录星号位置，跳过连续星号 */
            star_p = p++;
            star_s = s;
        } else if (*p == *s || *p == '?') {
            p++;
            s++;
        } else if (star_p) {
            /* 回溯到上一个星号，多匹配一个字符 */
            p = star_p + 1;
            s = ++star_s;
        } else {
            return 0;
        }
    }

    /* 跳过 pattern 末尾的星号 */
    while (*p == '*')
        p++;

    return *p == '\0';
}

/**
 * @brief 按通配符模式查询已注册的接口列表
 * @param system 插件系统指针
 * @param pattern 通配符匹配模式（支持 '*' 和 '?'）
 * @param count 输出参数，匹配的接口数量
 * @return 成功返回匹配的接口指针数组（需调用者释放），失败返回 NULL
 */
lvPluginInterface **lv_plugin_query_interfaces(lvPluginSystem *system, const char *pattern, size_t *count) {
    if (!system || !pattern || !count)
        return NULL;

    /* 第一遍：统计匹配数量 */
    size_t match_count = 0;
    for (size_t i = 0; i < system->interface_count; i++) {
        if (wildcard_match(pattern, system->interfaces[i]->name)) {
            match_count++;
        }
    }

    if (match_count == 0) {
        *count = 0;
        return NULL;
    }

    /* 分配结果数组 */
    lvPluginInterface **result = (lvPluginInterface **) lv_malloc(sizeof(lvPluginInterface *) * match_count);
    if (!result) {
        *count = 0;
        return NULL;
    }

    /* 第二遍：填充匹配结果 */
    size_t idx = 0;
    for (size_t i = 0; i < system->interface_count; i++) {
        if (wildcard_match(pattern, system->interfaces[i]->name)) {
            result[idx++] = system->interfaces[i];
        }
    }

    *count = match_count;
    return result;
}

/* ============ 插件配置 ============ */

/**
 * @brief 创建插件配置对象
 * @return 成功返回配置指针，失败返回 NULL
 */
lvPluginConfig *lv_plugin_config_create(void) {
    lvPluginConfig *config = (lvPluginConfig *) lv_calloc(1, sizeof(lvPluginConfig));
    if (!config)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_plugin_config_create: config calloc failed");
    config->entry_capacity = 256;
    config->entries = (lvPluginConfigEntry *) lv_calloc(config->entry_capacity, sizeof(lvPluginConfigEntry));

    if (!config->entries) {
        lv_free((void **) &config);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_plugin_config_create: entries calloc failed");
    }

    return config;
}

/**
 * @brief 销毁插件配置对象，释放资源
 * @param config 插件配置指针
 */
void lv_plugin_config_destroy(lvPluginConfig *config) {
    if (!config)
        return;
    if (config->entries)
        lv_free((void **) &config->entries);
    lv_free((void **) &config);
}

/**
 * @brief 从 INI 格式文件加载配置
 * @param config 插件配置指针
 * @param filepath 配置文件路径
 * @return 成功返回 0，失败返回 -1
 */
int lv_plugin_config_load(lvPluginConfig *config, const char *filepath) {
    lv_CHECK_NOT_NULL(config);
    lv_CHECK_NOT_NULL(filepath);

    FILE *fp = fopen(filepath, "r");
    if (!fp)
        lv_RETURN_ERROR(lv_ERROR_IO, "lv_plugin_config_load: fopen failed");

    /* 当前节名称，NULL 表示全局节 */
    char current_section[256] = {0};
    char line[2048];

    while (fgets(line, sizeof(line), fp)) {
        /* 去除行尾换行符 */
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }

        /* 跳过空行 */
        if (len == 0)
            continue;

        /* 跳过注释行（# 或 // 开头） */
        if (line[0] == '#' || (line[0] == '/' && line[1] == '/'))
            continue;

        /* 跳过行首空白后的注释 */
        {
            const char *trimmed = line;
            while (*trimmed == ' ' || *trimmed == '\t')
                trimmed++;
            if (*trimmed == '#' || (*trimmed == '/' && *(trimmed + 1) == '/'))
                continue;
            if (*trimmed == '\0')
                continue; /* 全空白行 */
        }

        /* 检查节标题 [section] */
        if (line[0] == '[') {
            char *end = strchr(line, ']');
            if (end) {
                size_t slen = (size_t) (end - line - 1);
                if (slen < sizeof(current_section)) {
                    memcpy(current_section, line + 1, slen);
                    current_section[slen] = '\0';
                }
            }
            continue;
        }

        /* 解析 key=value */
        char *eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            const char *key = line;
            const char *value = eq + 1;

            /* 去除 key 首尾空白 */
            while (*key == ' ' || *key == '\t')
                key++;
            char *key_end = (char *) (key + strlen(key) - 1);
            while (key_end > key && (*key_end == ' ' || *key_end == '\t'))
                *key_end-- = '\0';

            /* 如果有节名，添加节前缀: "section.key" */
            if (current_section[0] != '\0') {
                lvStrBuf sb = {0};
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                lv_strbuf_printf(&sb, "%s.%s", current_section, key);
#pragma GCC diagnostic pop
                lv_plugin_config_set(config, sb.data, value, 0);
                lv_strbuf_destroy(&sb);
            } else {
                lv_plugin_config_set(config, key, value, 0);
            }
        }
    }

    fclose(fp);
    strncpy(config->config_file, filepath, sizeof(config->config_file) - 1);
    return 0;
}

/**
 * @brief 将配置保存到文件（key=value 格式）
 * @param config 插件配置指针
 * @param filepath 保存路径
 * @return 成功返回 0，失败返回 -1
 */
int lv_plugin_config_save(const lvPluginConfig *config, const char *filepath) {
    lv_CHECK_NOT_NULL(config);
    lv_CHECK_NOT_NULL(filepath);

    FILE *fp = fopen(filepath, "w");
    if (!fp)
        lv_RETURN_ERROR(lv_ERROR_IO, "lv_plugin_config_save: fopen failed");

    for (size_t i = 0; i < config->entry_count; i++) {
        fprintf(fp, "%s=%s\n", config->entries[i].key, config->entries[i].value);
    }

    fclose(fp);
    return 0;
}

/**
 * @brief 设置配置项的值（若 key 已存在则覆盖）
 * @param config 插件配置指针
 * @param key 配置键名
 * @param value 配置值
 * @param type 配置值类型标识
 * @return 成功返回 0，失败返回 -1
 */
int lv_plugin_config_set(lvPluginConfig *config, const char *key, const char *value, int type) {
    lv_CHECK_NOT_NULL(config);
    lv_CHECK_NOT_NULL(key);
    lv_CHECK_NOT_NULL(value);
    lv_CHECK_ARG(config->entries != NULL, lv_ERROR_INTERNAL, "config entries is NULL");
    if (config->entry_count >= config->entry_capacity)
        lv_RETURN_ERROR(lv_ERROR_RESOURCE_EXHAUSTED, "lv_plugin_config_set: entry_capacity exhausted");

    /* 检查是否已存在 */
    for (size_t i = 0; i < config->entry_count; i++) {
        if (strcmp(config->entries[i].key, key) == 0) {
            strncpy(config->entries[i].value, value, sizeof(config->entries[i].value) - 1);
            config->entries[i].type = type;
            return 0;
        }
    }

    /* 添加新条目 */
    lvPluginConfigEntry *entry = &config->entries[config->entry_count++];
    strncpy(entry->key, key, sizeof(entry->key));
    entry->key[sizeof(entry->key) - 1] = '\0';
    strncpy(entry->value, value, sizeof(entry->value));
    entry->value[sizeof(entry->value) - 1] = '\0';
    entry->type = type;

    return 0;
}

/**
 * @brief 获取配置项的值，不存在则返回默认值
 * @param config 插件配置指针
 * @param key 配置键名
 * @param default_value 默认值
 * @return 配置值或默认值
 */
const char *lv_plugin_config_get(const lvPluginConfig *config, const char *key, const char *default_value) {
    if (!config || !key)
        return default_value;

    for (size_t i = 0; i < config->entry_count; i++) {
        if (strcmp(config->entries[i].key, key) == 0) {
            return config->entries[i].value;
        }
    }

    return default_value;
}

/**
 * @brief 将配置应用到指定插件（调用 on_configure 回调）
 * @param plugin 插件指针
 * @param config 配置指针
 * @return 成功返回 0，失败返回 -1
 */
int lv_plugin_apply_config(lvPlugin *plugin, const lvPluginConfig *config) {
    lv_CHECK_NOT_NULL(plugin);
    lv_CHECK_NOT_NULL(config);

    if (plugin->on_configure) {
        return plugin->on_configure(plugin->context, config);
    }

    return 0;
}

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

/* ============ 依赖管理 ============ */

/**
 * @brief 解析并激活指定插件的所有依赖
 * @param system 插件系统指针
 * @param plugin 待解析依赖的插件指针
 * @return 成功返回 0，缺失必需依赖时返回 -1
 */
int lv_plugin_resolve_dependencies(lvPluginSystem *system, lvPlugin *plugin) {
    if (!system || !plugin)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "lv_plugin_resolve_dependencies: system or plugin is NULL");
    if (!plugin->info.dependencies && plugin->info.dependency_count > 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "lv_plugin_resolve_dependencies: deps array NULL but count > 0");

    for (size_t i = 0; i < plugin->info.dependency_count; i++) {
        lvPluginDependency *dep = plugin->info.dependencies[i];
        lvPlugin *dep_plugin = lv_plugin_find(system, dep->name);

        if (!dep_plugin) {
            if (!dep->optional) {
                set_error(system, "Required dependency not found: %s", dep->name);
                lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "lv_plugin_resolve_dependencies: required dependency not found");
            }
            continue;
        }

        /* 检查版本兼容性 */
        if (!lv_plugin_check_version(dep->version_constraint, dep_plugin->info.version)) {
            if (!dep->optional) {
                set_error(system, "Dependency version mismatch: %s", dep->name);
                lv_RETURN_ERROR(lv_ERROR_UNSUPPORTED, "lv_plugin_resolve_dependencies: version mismatch");
            }
        }

        /* 激活依赖 */
        if (dep_plugin->state != lv_PLUGIN_STATE_ACTIVE) {
            if (lv_plugin_activate(dep_plugin) != 0) {
                if (!dep->optional) {
                    set_error(system, "Failed to activate dependency: %s", dep->name);
                    lv_RETURN_ERROR(lv_ERROR_INTERNAL, "lv_plugin_resolve_dependencies: activation failed");
                }
            }
        }
    }

    return 0;
}

/**
 * @brief 检查插件是否包含非可选的必需依赖
 * @param plugin 插件指针
 * @return 有非可选依赖返回 0，无非可选依赖返回 1，出错返回 -1
 */
int lv_plugin_check_dependencies(const lvPlugin *plugin) {
    lv_CHECK_NOT_NULL(plugin);
    lv_CHECK_ARG(plugin->info.dependencies != NULL || plugin->info.dependency_count == 0, lv_ERROR_INTERNAL,
                 "deps array NULL but count > 0");

    for (size_t i = 0; i < plugin->info.dependency_count; i++) {
        if (!plugin->info.dependencies[i]->optional) {
            return 0; /* 至少有一个非可选依赖 */
        }
    }

    return 1; /* 没有非可选依赖 */
}

/**
 * @brief 获取所有依赖指定插件的插件列表
 * @param system 插件系统指针
 * @param plugin 被依赖的插件指针
 * @param count 输出参数，依赖者数量
 * @return 成功返回依赖者数组（需调用者释放），失败返回 NULL
 */
lvPlugin **lv_plugin_get_dependents(lvPluginSystem *system, const lvPlugin *plugin, size_t *count) {
    if (!system || !plugin || !count)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_plugin_get_dependents: invalid parameters");

    /* 统计依赖此插件的插件数量 */
    size_t dependent_count = 0;
    for (size_t i = 0; i < system->plugin_count; i++) {
        if (!system->plugins[i]->info.dependencies)
            continue;
        for (size_t j = 0; j < system->plugins[i]->info.dependency_count; j++) {
            if (strcmp(system->plugins[i]->info.dependencies[j]->name, plugin->info.name) == 0) {
                dependent_count++;
                break;
            }
        }
    }

    if (dependent_count == 0) {
        *count = 0;
        return NULL;
    }

    /* 分配结果数组 */
    lvPlugin **result = (lvPlugin **) lv_malloc(sizeof(lvPlugin *) * dependent_count);
    if (!result) {
        *count = 0;
        return NULL;
    }

    /* 填充结果 */
    size_t idx = 0;
    for (size_t i = 0; i < system->plugin_count; i++) {
        if (!system->plugins[i]->info.dependencies)
            continue;
        for (size_t j = 0; j < system->plugins[i]->info.dependency_count; j++) {
            if (strcmp(system->plugins[i]->info.dependencies[j]->name, plugin->info.name) == 0) {
                result[idx++] = system->plugins[i];
                break;
            }
        }
    }

    *count = dependent_count;
    return result;
}

/* ============ 搜索路径管理 ============ */

/**
 * @brief 添加插件搜索路径
 * @param system 插件系统指针
 * @param path 搜索路径
 * @return 成功返回 0，失败返回 -1
 */
int lv_plugin_system_add_search_path(lvPluginSystem *system, const char *path) {
    if (!system || !path)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "lv_plugin_system_add_search_path: system or path is NULL");

    /* 检查是否已存在 */
    for (int i = 0; i < system->search_paths.count; i++) {
        if (strcmp(*(char **)lv_darray_get(&system->search_paths, i), path) == 0) {
            return 0;
        }
    }

    /* 添加新路径 */
    char *copy = lv_strdup_safe(path);
    if (!copy)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_plugin_system_add_search_path: strdup failed");

    if (lv_darray_push(&system->search_paths, &copy) < 0) {
        lv_free((void **) &copy);
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_plugin_system_add_search_path: darray_push failed");
    }

    return 0;
}

/**
 * @brief 移除插件搜索路径
 * @param system 插件系统指针
 * @param path 待移除的搜索路径
 * @return 成功返回 0，未找到返回 -1
 */
int lv_plugin_system_remove_search_path(lvPluginSystem *system, const char *path) {
    if (!system || !path)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "lv_plugin_system_remove_search_path: system or path is NULL");

    for (int i = 0; i < system->search_paths.count; i++) {
        if (strcmp(*(char **)lv_darray_get(&system->search_paths, i), path) == 0) {
            lv_free((void **) lv_darray_get(&system->search_paths, i));
            /* 将最后一个元素移到当前位置 */
            char **last = (char **)lv_darray_get(&system->search_paths, system->search_paths.count - 1);
            char **cur = (char **)lv_darray_get(&system->search_paths, i);
            *cur = *last;
            lv_darray_pop(&system->search_paths);
            return 0;
        }
    }

    lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "lv_plugin_system_remove_search_path: path not found");
}

/**
 * @brief 获取所有已注册的搜索路径
 * @param system 插件系统指针
 * @param count 输出参数，路径数量
 * @return 返回搜索路径数组
 */
char **lv_plugin_system_get_search_paths(lvPluginSystem *system, size_t *count) {
    if (!system || !count)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_plugin_system_get_search_paths: system or count is NULL");

    *count = (size_t)system->search_paths.count;
    return (char **)system->search_paths.data;
}

/* ============ 自动加载 ============ */

/**
 * @brief 从指定目录自动扫描并加载插件
 * @param system 插件系统指针
 * @param directory 待扫描的目录路径
 * @return 成功返回 0，失败返回 -1
 */
int lv_plugin_system_autoload(lvPluginSystem *system, const char *directory) {
    if (!system || !directory)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "lv_plugin_system_autoload: system or directory is NULL");

    /* 添加搜索路径 */
    lv_plugin_system_add_search_path(system, directory);

    /* 扫描目录中的 .dll 文件（Windows）或 .so 文件（Linux） */
#ifdef _WIN32
    lvStrBuf sb_2 = {0};
    lv_strbuf_printf(&sb_2, "%s\\*.dll", directory);

    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(sb_2.data, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        lv_strbuf_destroy(&sb_2);
        return 0; /* 目录为空或不存在，不算错误 */
    }

    do {
        /* 跳过 . 和 .. 目录 */
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }

        /* 构造完整路径 */
        lvStrBuf sb_3 = {0};
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        lv_strbuf_printf(&sb_3, "%s\\%s", directory, find_data.cFileName);
#pragma GCC diagnostic pop

        /* 尝试加载为插件 */
        lvPlugin *plugin = lv_plugin_load(system, sb_3.data);
        if (plugin) {
            /* 版本兼容性检查：验证插件版本是否与系统版本兼容 */
            if (plugin->info.version[0] != '\0') {
                if (!lv_plugin_check_api_compatibility(system->version, (lv_PLUGIN_SYSTEM_VERSION_MAJOR << 16) |
                                                                            (lv_PLUGIN_SYSTEM_VERSION_MINOR << 8))) {
                    /* 插件 API 版本不兼容，记录警告并跳过激活 */
                    set_error(system,
                              "Plugin '%s' version '%s' may be incompatible with "
                              "system API version %d.%d.%d. Loading but not activating.",
                              plugin->info.name, plugin->info.version, lv_PLUGIN_SYSTEM_VERSION_MAJOR,
                              lv_PLUGIN_SYSTEM_VERSION_MINOR, lv_PLUGIN_SYSTEM_VERSION_PATCH);
                    /* 不卸载插件，但也不自动激活，让用户决定 */
                }
            }
        }
        lv_strbuf_destroy(&sb_3);

    } while (FindNextFileA(hFind, &find_data));

    FindClose(hFind);
    lv_strbuf_destroy(&sb_2);
#else
    /* Linux/macOS: 使用 opendir/readdir 扫描 .so 文件 */
    DIR *dir = opendir(directory);
    if (!dir)
        return 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        /* 跳过 . 和 .. */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        /* 检查是否为 .so 文件 */
        size_t name_len = strlen(entry->d_name);
        if (name_len > 3 && strcmp(entry->d_name + name_len - 3, ".so") == 0) {
            lvStrBuf sb_4 = {0};
            lv_strbuf_printf(&sb_4, "%s/%s", directory, entry->d_name);

            /* 尝试加载为插件 */
            lvPlugin *plugin = lv_plugin_load(system, sb_4.data);
            if (plugin) {
                /* 版本兼容性检查：验证插件版本是否与系统版本兼容 */
                if (plugin->info.version[0] != '\0') {
                    if (!lv_plugin_check_api_compatibility(
                            system->version,
                            (lv_PLUGIN_SYSTEM_VERSION_MAJOR << 16) | (lv_PLUGIN_SYSTEM_VERSION_MINOR << 8))) {
                        set_error(system,
                                  "Plugin '%s' version '%s' may be incompatible with "
                                  "system API version %d.%d.%d. Loading but not activating.",
                                  plugin->info.name, plugin->info.version, lv_PLUGIN_SYSTEM_VERSION_MAJOR,
                                  lv_PLUGIN_SYSTEM_VERSION_MINOR, lv_PLUGIN_SYSTEM_VERSION_PATCH);
                    }
                }
            }
            lv_strbuf_destroy(&sb_4);
        }
    }

    closedir(dir);
#endif

    return 0;
}

/**
 * @brief 自动加载所有搜索路径下的插件
 * @param system 插件系统指针
 * @return 成功返回 0，失败返回 -1
 */
int lv_plugin_system_autoload_all(lvPluginSystem *system) {
    lv_CHECK_NOT_NULL(system);

    /* 遍历所有搜索路径，自动加载其中的插件（含版本兼容性检查） */
    for (int i = 0; i < system->search_paths.count; i++) {
        lv_plugin_system_autoload(system, *(char **)lv_darray_get(&system->search_paths, i));
    }

    return 0;
}

/* ============ 版本兼容性 ============ */

/* 版本兼容性常量 */
#define lv_PLUGIN_VERSION_OK 1
#define lv_PLUGIN_VERSION_MISMATCH 0

/* 解析语义版本字符串 "major.minor.patch"，返回 sscanf 匹配项数 */
static int parse_semver(const char *ver_str, int *major, int *minor, int *patch) {
    if (!ver_str)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "parse_semver: ver_str is NULL");
    *major = *minor = *patch = 0;
    return sscanf(ver_str, "%d.%d.%d", major, minor, patch);
}

/**
 * @brief 检查版本号是否满足要求（语义版本比较）
 * @param required 要求的版本字符串
 * @param provided 提供的版本字符串
 * @return 兼容返回 lv_PLUGIN_VERSION_OK (1)，不兼容返回 lv_PLUGIN_VERSION_MISMATCH (0)
 */
int lv_plugin_check_version(const char *required, const char *provided) {
    if (!required || !provided)
        return lv_PLUGIN_VERSION_MISMATCH;

    /* 解析 required 版本 */
    int req_major, req_minor, req_patch;
    if (parse_semver(required, &req_major, &req_minor, &req_patch) < 1) {
        return lv_PLUGIN_VERSION_MISMATCH;
    }

    /* 解析 provided 版本 */
    int prov_major, prov_minor, prov_patch;
    if (parse_semver(provided, &prov_major, &prov_minor, &prov_patch) < 1) {
        return lv_PLUGIN_VERSION_MISMATCH;
    }

    /* 语义版本比较：逐级比较 major -> minor -> patch */
    if (prov_major > req_major)
        return lv_PLUGIN_VERSION_OK;
    if (prov_major < req_major)
        return lv_PLUGIN_VERSION_MISMATCH;

    /* major 相同，比较 minor */
    if (prov_minor > req_minor)
        return lv_PLUGIN_VERSION_OK;
    if (prov_minor < req_minor)
        return lv_PLUGIN_VERSION_MISMATCH;

    /* minor 相同，比较 patch */
    if (prov_patch >= req_patch)
        return lv_PLUGIN_VERSION_OK;

    return lv_PLUGIN_VERSION_MISMATCH;
}

/**
 * @brief 检查 API 版本兼容性（provided >= required）
 * @param required 要求的 API 版本
 * @param provided 提供的 API 版本
 * @return 兼容返回 1，不兼容返回 0
 */
int lv_plugin_check_api_compatibility(uint32_t required, uint32_t provided) {
    return provided >= required;
}

/* ============ 插件信息 ============ */

/**
 * @brief 获取插件信息的 JSON 字符串
 * @param plugin 插件指针
 * @return 成功返回 JSON 字符串（需调用者释放），失败返回 NULL
 */
char *lv_plugin_get_info_json(const lvPlugin *plugin) {
    if (!plugin)
        return NULL;

    size_t size = 1024;
    char *json = (char *) lv_malloc(size);
    if (!json)
        return NULL;

    snprintf(json, size,
             "{"
             "\"name\":\"%s\","
             "\"version\":\"%s\","
             "\"author\":\"%s\","
             "\"description\":\"%s\","
             "\"state\":%d,"
             "\"type\":%d"
             "}",
             plugin->info.name, plugin->info.version, plugin->info.author, plugin->info.description, plugin->state,
             plugin->info.type);

    return json;
}

/**
 * @brief 获取插件系统完整信息的 JSON 字符串
 * @param system 插件系统指针
 * @return 成功返回 JSON 字符串（需调用者释放），失败返回 NULL
 */
char *lv_plugin_system_get_info_json(const lvPluginSystem *system) {
    if (!system)
        return NULL;

    /* 防止整数溢出 */
    size_t plugin_size;
    if (system->plugin_count > (SIZE_MAX - 2048) / 512) {
        return NULL; /* overflow */
    }
    size_t size = 2048 + system->plugin_count * 512;
    char *json = (char *) lv_malloc(size);
    if (!json)
        return NULL;

    char *ptr = json;
    size_t remaining = size;
    int written = snprintf(ptr, remaining,
                           "{"
                           "\"version\":%u,"
                           "\"plugin_count\":%zu,"
                           "\"interface_count\":%zu,"
                           "\"plugins\":[",
                           system->version, system->plugin_count, system->interface_count);
    if (written > 0) {
        ptr += written;
        remaining -= written;
    }

    for (size_t i = 0; i < system->plugin_count; i++) {
        if (remaining <= 0)
            break;
        written = snprintf(ptr, (size_t) remaining,
                           "{"
                           "\"name\":\"%s\","
                           "\"version\":\"%s\","
                           "\"state\":%d"
                           "}%s",
                           system->plugins[i]->info.name, system->plugins[i]->info.version, system->plugins[i]->state,
                           (i < system->plugin_count - 1) ? "," : "");
        if (written > 0) {
            ptr += written;
            remaining -= written;
        }
    }

    if (remaining > 0) {
        snprintf(ptr, (size_t) remaining, "]}");
    }

    return json;
}

/* ============ 错误处理 ============ */

/**
 * @brief 获取指定插件最近一次的错误消息
 * @param plugin 插件指针
 * @return 错误字符串，无错误返回空字符串
 */
const char *lv_plugin_get_last_error(const lvPlugin *plugin) {
    if (!plugin || !plugin->context)
        return NULL;
    if (!plugin->context->system)
        return NULL;

    PluginSystemInternal *internal = (PluginSystemInternal *) plugin->context->system->mutex;
    return internal->last_error;
}

/**
 * @brief 获取插件系统的最近一次错误消息
 * @param system 插件系统指针
 * @return 错误字符串，无错误返回空字符串
 */
const char *lv_plugin_system_get_last_error(const lvPluginSystem *system) {
    if (!system)
        return NULL;

    PluginSystemInternal *internal = (PluginSystemInternal *) system->mutex;
    return internal->last_error;
}

/**
 * @brief 清除指定插件的错误消息
 * @param plugin 插件指针
 */
void lv_plugin_clear_error(lvPlugin *plugin) {
    if (!plugin || !plugin->context)
        return;
    if (!plugin->context->system)
        return;

    PluginSystemInternal *internal = (PluginSystemInternal *) plugin->context->system->mutex;
    internal->last_error[0] = '\0';
}

/**
 * @brief 清除插件系统的错误消息
 * @param system 插件系统指针
 */
void lv_plugin_system_clear_error(lvPluginSystem *system) {
    if (!system)
        return;

    PluginSystemInternal *internal = (PluginSystemInternal *) system->mutex;
    internal->last_error[0] = '\0';
}
