/**
 * @file plugin_system.c
 * @brief LV-00 模块化插件系统实现
 *
 * 实现插件加载、卸载、接口注册机制和插件间通信
 *
 * @author Lv-00 Project
 * @version 1.0
 */

#include "lv00/plugin_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

/* ============ 内部数据结构 ============ */

typedef struct {
    char last_error[1024];
} PluginSystemInternal;

/* ============ 辅助函数 ============ */

static uint64_t get_timestamp(void) {
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    return ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
#endif
}

static void* load_library(const char* path) {
#ifdef _WIN32
    return LoadLibraryA(path);
#else
    return dlopen(path, RTLD_LAZY);
#endif
}

static void unload_library(void* handle) {
    if (!handle) return;
#ifdef _WIN32
    FreeLibrary((HMODULE)handle);
#else
    dlclose(handle);
#endif
}

static void* get_symbol(void* handle, const char* name) {
    if (!handle || !name) return NULL;
#ifdef _WIN32
    return (void*)GetProcAddress((HMODULE)handle, name);
#else
    return dlsym(handle, name);
#endif
}

static void set_error(Lv00PluginSystem* system, const char* format, ...) {
    if (!system) return;
    
    PluginSystemInternal* internal = (PluginSystemInternal*)system->mutex;
    if (!internal) return;
    
    va_list args;
    va_start(args, format);
    vsnprintf(internal->last_error, sizeof(internal->last_error), format, args);
    va_end(args);
}

/* ============ 生命周期管理 ============ */

Lv00PluginSystem* lv00_plugin_system_create(Lv00Context* ctx) {
    Lv00PluginSystem* system = (Lv00PluginSystem*)malloc(sizeof(Lv00PluginSystem));
    if (!system) return NULL;
    
    memset(system, 0, sizeof(Lv00PluginSystem));
    
    system->lv00_context = ctx;
    system->version = (LV00_PLUGIN_SYSTEM_VERSION_MAJOR << 16) |
                      (LV00_PLUGIN_SYSTEM_VERSION_MINOR << 8) |
                      LV00_PLUGIN_SYSTEM_VERSION_PATCH;
    
    system->plugin_capacity = LV00_MAX_PLUGINS;
    system->plugins = (Lv00Plugin**)malloc(sizeof(Lv00Plugin*) * system->plugin_capacity);
    if (!system->plugins) {
        free(system);
        return NULL;
    }
    
    system->interface_capacity = LV00_MAX_INTERFACES;
    system->interfaces = (Lv00PluginInterface**)malloc(
        sizeof(Lv00PluginInterface*) * system->interface_capacity);
    if (!system->interfaces) {
        free(system->plugins);
        free(system);
        return NULL;
    }
    
    system->search_path_count = 0;
    system->search_paths = NULL;
    
    PluginSystemInternal* internal = (PluginSystemInternal*)malloc(sizeof(PluginSystemInternal));
    if (!internal) {
        free(system->interfaces);
        free(system->plugins);
        free(system);
        return NULL;
    }
    
    memset(internal, 0, sizeof(PluginSystemInternal));
    system->mutex = internal;
    
    return system;
}

void lv00_plugin_system_destroy(Lv00PluginSystem* system) {
    if (!system) return;
    
    lv00_plugin_system_cleanup(system);
    
    if (system->plugins) free(system->plugins);
    if (system->interfaces) free(system->interfaces);
    
    for (size_t i = 0; i < system->search_path_count; i++) {
        if (system->search_paths[i]) free(system->search_paths[i]);
    }
    if (system->search_paths) free(system->search_paths);
    
    if (system->mutex) free(system->mutex);
    free(system);
}

int lv00_plugin_system_init(Lv00PluginSystem* system) {
    if (!system) return -1;
    
    system->initialized = 1;
    return 0;
}

void lv00_plugin_system_cleanup(Lv00PluginSystem* system) {
    if (!system) return;
    
    /* 卸载所有插件 */
    while (system->plugin_count > 0) {
        lv00_plugin_unload(system, system->plugins[0]);
    }
    
    system->initialized = 0;
}

/* ============ 插件加载与卸载 ============ */

Lv00Plugin* lv00_plugin_load(Lv00PluginSystem* system, const char* path) {
    if (!system || !path) return NULL;
    if (system->plugin_count >= system->plugin_capacity) {
        set_error(system, "Plugin capacity exceeded");
        return NULL;
    }
    
    /* 检查是否已加载 */
    for (size_t i = 0; i < system->plugin_count; i++) {
        if (strcmp(system->plugins[i]->path, path) == 0) {
            set_error(system, "Plugin already loaded: %s", path);
            return NULL;
        }
    }
    
    /* 加载动态库 */
    void* handle = load_library(path);
    if (!handle) {
        set_error(system, "Failed to load library: %s", path);
        return NULL;
    }
    
    /* 创建插件对象 */
    Lv00Plugin* plugin = (Lv00Plugin*)malloc(sizeof(Lv00Plugin));
    if (!plugin) {
        unload_library(handle);
        return NULL;
    }
    
    memset(plugin, 0, sizeof(Lv00Plugin));
    strncpy(plugin->path, path, sizeof(plugin->path) - 1);
    plugin->handle = handle;
    plugin->state = LV00_PLUGIN_STATE_LOADING;
    plugin->load_time = get_timestamp();
    
    /* 获取入口函数 */
    typedef int (*PluginEntryFunc)(Lv00PluginContext* ctx);
    PluginEntryFunc entry = (PluginEntryFunc)get_symbol(handle, "lv00_plugin_load_entry");
    
    if (!entry) {
        set_error(system, "Plugin entry point not found: %s", path);
        unload_library(handle);
        free(plugin);
        return NULL;
    }
    
    /* 创建插件上下文 */
    plugin->context = (Lv00PluginContext*)malloc(sizeof(Lv00PluginContext));
    if (!plugin->context) {
        unload_library(handle);
        free(plugin);
        return NULL;
    }
    
    memset(plugin->context, 0, sizeof(Lv00PluginContext));
    plugin->context->plugin = plugin;
    plugin->context->system = system;
    plugin->context->lv00_context = system->lv00_context;
    
    /* 调用入口函数 */
    if (entry(plugin->context) != 0) {
        set_error(system, "Plugin entry function failed: %s", path);
        free(plugin->context);
        unload_library(handle);
        free(plugin);
        return NULL;
    }
    
    /* 调用 on_load 回调 */
    if (plugin->on_load) {
        if (plugin->on_load(plugin->context) != 0) {
            set_error(system, "Plugin on_load failed: %s", path);
            free(plugin->context);
            unload_library(handle);
            free(plugin);
            return NULL;
        }
    }
    
    plugin->state = LV00_PLUGIN_STATE_LOADED;
    system->plugins[system->plugin_count++] = plugin;
    
    /* 发送加载事件 */
    Lv00PluginEvent event = {
        .type = LV00_PLUGIN_EVENT_LOAD,
        .timestamp = get_timestamp(),
        .source = plugin,
        .target = NULL
    };
    
    if (system->event_handler) {
        system->event_handler(system, &event);
    }
    
    return plugin;
}

int lv00_plugin_unload(Lv00PluginSystem* system, Lv00Plugin* plugin) {
    if (!system || !plugin) return -1;
    
    /* 停用插件 */
    if (plugin->state == LV00_PLUGIN_STATE_ACTIVE) {
        lv00_plugin_deactivate(plugin);
    }
    
    /* 注销所有接口 */
    for (size_t i = 0; i < plugin->registered_interface_count; i++) {
        lv00_plugin_unregister_interface(plugin, plugin->registered_interfaces[i]->name);
    }
    
    /* 发送卸载事件 */
    Lv00PluginEvent event = {
        .type = LV00_PLUGIN_EVENT_UNLOAD,
        .timestamp = get_timestamp(),
        .source = plugin,
        .target = NULL
    };
    
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
            lv00_plugin_config_destroy(plugin->context->config);
        }
        free(plugin->context);
    }
    
    if (plugin->registered_interfaces) {
        free(plugin->registered_interfaces);
    }
    
    if (plugin->resolved_dependencies) {
        free(plugin->resolved_dependencies);
    }
    
    plugin->state = LV00_PLUGIN_STATE_UNLOADED;
    
    /* 卸载动态库 */
    if (plugin->handle) {
        unload_library(plugin->handle);
    }
    
    free(plugin);
    return 0;
}

int lv00_plugin_reload(Lv00PluginSystem* system, Lv00Plugin* plugin) {
    if (!system || !plugin) return -1;
    
    char path[LV00_PLUGIN_PATH_MAX];
    strncpy(path, plugin->path, sizeof(path) - 1);
    
    if (lv00_plugin_unload(system, plugin) != 0) {
        return -1;
    }
    
    return lv00_plugin_load(system, path) ? 0 : -1;
}

/* ============ 插件激活与停用 ============ */

int lv00_plugin_activate(Lv00Plugin* plugin) {
    if (!plugin) return -1;
    if (plugin->state != LV00_PLUGIN_STATE_LOADED) return -1;
    
    plugin->state = LV00_PLUGIN_STATE_INITIALIZING;
    
    /* 解析依赖 */
    if (lv00_plugin_resolve_dependencies(plugin->context->system, plugin) != 0) {
        plugin->state = LV00_PLUGIN_STATE_ERROR;
        return -1;
    }
    
    /* 调用 on_activate 回调 */
    if (plugin->on_activate) {
        if (plugin->on_activate(plugin->context) != 0) {
            plugin->state = LV00_PLUGIN_STATE_ERROR;
            return -1;
        }
    }
    
    plugin->state = LV00_PLUGIN_STATE_ACTIVE;
    plugin->activate_time = get_timestamp();
    
    /* 发送激活事件 */
    Lv00PluginEvent event = {
        .type = LV00_PLUGIN_EVENT_ACTIVATE,
        .timestamp = get_timestamp(),
        .source = plugin,
        .target = NULL
    };
    
    if (plugin->context->system->event_handler) {
        plugin->context->system->event_handler(plugin->context->system, &event);
    }
    
    return 0;
}

int lv00_plugin_deactivate(Lv00Plugin* plugin) {
    if (!plugin) return -1;
    if (plugin->state != LV00_PLUGIN_STATE_ACTIVE) return -1;
    
    plugin->state = LV00_PLUGIN_STATE_DEACTIVATING;
    
    /* 调用 on_deactivate 回调 */
    if (plugin->on_deactivate) {
        plugin->on_deactivate(plugin->context);
    }
    
    plugin->state = LV00_PLUGIN_STATE_LOADED;
    
    /* 发送停用事件 */
    Lv00PluginEvent event = {
        .type = LV00_PLUGIN_EVENT_DEACTIVATE,
        .timestamp = get_timestamp(),
        .source = plugin,
        .target = NULL
    };
    
    if (plugin->context->system->event_handler) {
        plugin->context->system->event_handler(plugin->context->system, &event);
    }
    
    return 0;
}

int lv00_plugin_is_active(const Lv00Plugin* plugin) {
    return plugin && plugin->state == LV00_PLUGIN_STATE_ACTIVE;
}

Lv00PluginState lv00_plugin_get_state(const Lv00Plugin* plugin) {
    return plugin ? plugin->state : LV00_PLUGIN_STATE_UNLOADED;
}

/* ============ 插件查询 ============ */

Lv00Plugin* lv00_plugin_find(Lv00PluginSystem* system, const char* name) {
    if (!system || !name) return NULL;
    
    for (size_t i = 0; i < system->plugin_count; i++) {
        if (strcmp(system->plugins[i]->info.name, name) == 0) {
            return system->plugins[i];
        }
    }
    return NULL;
}

Lv00Plugin** lv00_plugin_get_all(Lv00PluginSystem* system, size_t* count) {
    if (!system || !count) return NULL;
    
    *count = system->plugin_count;
    return system->plugins;
}

Lv00Plugin** lv00_plugin_get_by_type(Lv00PluginSystem* system, Lv00PluginType type, size_t* count) {
    if (!system || !count) return NULL;
    
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
    Lv00Plugin** result = (Lv00Plugin**)malloc(sizeof(Lv00Plugin*) * match_count);
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

Lv00Plugin** lv00_plugin_get_by_state(Lv00PluginSystem* system, Lv00PluginState state, size_t* count) {
    if (!system || !count) return NULL;
    
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
    Lv00Plugin** result = (Lv00Plugin**)malloc(sizeof(Lv00Plugin*) * match_count);
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

int lv00_plugin_register_interface(Lv00Plugin* plugin, Lv00PluginInterface* interface) {
    if (!plugin || !interface) return -1;
    if (plugin->registered_interface_count >= LV00_MAX_INTERFACES) return -1;
    
    /* 检查是否已注册 */
    for (size_t i = 0; i < plugin->registered_interface_count; i++) {
        if (strcmp(plugin->registered_interfaces[i]->name, interface->name) == 0) {
            return -1;
        }
    }
    
    /* 添加到插件注册表 */
    if (!plugin->registered_interfaces) {
        plugin->registered_interfaces = (Lv00PluginInterface**)malloc(
            sizeof(Lv00PluginInterface*) * LV00_MAX_INTERFACES);
        if (!plugin->registered_interfaces) return -1;
    }
    
    interface->owner = plugin;
    plugin->registered_interfaces[plugin->registered_interface_count++] = interface;
    
    /* 添加到系统注册表 */
    Lv00PluginSystem* system = plugin->context->system;
    if (system->interface_count < system->interface_capacity) {
        system->interfaces[system->interface_count++] = interface;
    }
    
    return 0;
}

int lv00_plugin_unregister_interface(Lv00Plugin* plugin, const char* name) {
    if (!plugin || !name) return -1;
    
    /* 从插件注册表中移除 */
    for (size_t i = 0; i < plugin->registered_interface_count; i++) {
        if (strcmp(plugin->registered_interfaces[i]->name, name) == 0) {
            /* 从系统注册表中移除 */
            Lv00PluginSystem* system = plugin->context->system;
            for (size_t j = 0; j < system->interface_count; j++) {
                if (system->interfaces[j] == plugin->registered_interfaces[i]) {
                    system->interfaces[j] = system->interfaces[--system->interface_count];
                    break;
                }
            }
            
            plugin->registered_interfaces[i] = 
                plugin->registered_interfaces[--plugin->registered_interface_count];
            return 0;
        }
    }
    
    return -1;
}

Lv00PluginInterface* lv00_plugin_query_interface(Lv00PluginSystem* system, const char* name, uint32_t version) {
    if (!system || !name) return NULL;
    
    for (size_t i = 0; i < system->interface_count; i++) {
        if (strcmp(system->interfaces[i]->name, name) == 0 &&
            system->interfaces[i]->version == version) {
            return system->interfaces[i];
        }
    }
    return NULL;
}

/* 通配符模式匹配：支持 '*' glob */
static int wildcard_match(const char* pattern, const char* str) {
    if (!pattern || !str) return 0;

    const char* p = pattern;
    const char* s = str;
    const char* star_p = NULL;
    const char* star_s = NULL;

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
    while (*p == '*') p++;

    return *p == '\0';
}

Lv00PluginInterface** lv00_plugin_query_interfaces(Lv00PluginSystem* system, const char* pattern, size_t* count) {
    if (!system || !pattern || !count) return NULL;

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
    Lv00PluginInterface** result = (Lv00PluginInterface**)malloc(sizeof(Lv00PluginInterface*) * match_count);
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

Lv00PluginConfig* lv00_plugin_config_create(void) {
    Lv00PluginConfig* config = (Lv00PluginConfig*)malloc(sizeof(Lv00PluginConfig));
    if (!config) return NULL;
    
    memset(config, 0, sizeof(Lv00PluginConfig));
    config->entry_capacity = 256;
    config->entries = (Lv00PluginConfigEntry*)malloc(
        sizeof(Lv00PluginConfigEntry) * config->entry_capacity);
    
    if (!config->entries) {
        free(config);
        return NULL;
    }
    
    return config;
}

void lv00_plugin_config_destroy(Lv00PluginConfig* config) {
    if (!config) return;
    if (config->entries) free(config->entries);
    free(config);
}

int lv00_plugin_config_load(Lv00PluginConfig* config, const char* filepath) {
    if (!config || !filepath) return -1;
    
    FILE* fp = fopen(filepath, "r");
    if (!fp) return -1;
    
    /* 当前节名称，NULL 表示全局节 */
    char current_section[256] = {0};
    char line[2048];
    
    while (fgets(line, sizeof(line), fp)) {
        /* 去除行尾换行符 */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }
        
        /* 跳过空行 */
        if (len == 0) continue;
        
        /* 跳过注释行（# 或 // 开头） */
        if (line[0] == '#' || (line[0] == '/' && line[1] == '/')) continue;
        
        /* 跳过行首空白后的注释 */
        {
            const char* trimmed = line;
            while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
            if (*trimmed == '#' || (*trimmed == '/' && *(trimmed+1) == '/')) continue;
            if (*trimmed == '\0') continue; /* 全空白行 */
        }
        
        /* 检查节标题 [section] */
        if (line[0] == '[') {
            char* end = strchr(line, ']');
            if (end) {
                size_t slen = (size_t)(end - line - 1);
                if (slen < sizeof(current_section)) {
                    memcpy(current_section, line + 1, slen);
                    current_section[slen] = '\0';
                }
            }
            continue;
        }
        
        /* 解析 key=value */
        char* eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            const char* key = line;
            const char* value = eq + 1;
            
            /* 去除 key 首尾空白 */
            while (*key == ' ' || *key == '\t') key++;
            char* key_end = (char*)(key + strlen(key) - 1);
            while (key_end > key && (*key_end == ' ' || *key_end == '\t')) *key_end-- = '\0';
            
            /* 如果有节名，添加节前缀: "section.key" */
            if (current_section[0] != '\0') {
                char full_key[512];
                snprintf(full_key, sizeof(full_key), "%s.%s", current_section, key);
                lv00_plugin_config_set(config, full_key, value, 0);
            } else {
                lv00_plugin_config_set(config, key, value, 0);
            }
        }
    }
    
    fclose(fp);
    strncpy(config->config_file, filepath, sizeof(config->config_file) - 1);
    return 0;
}

int lv00_plugin_config_save(const Lv00PluginConfig* config, const char* filepath) {
    if (!config || !filepath) return -1;
    
    FILE* fp = fopen(filepath, "w");
    if (!fp) return -1;
    
    for (size_t i = 0; i < config->entry_count; i++) {
        fprintf(fp, "%s=%s\n", config->entries[i].key, config->entries[i].value);
    }
    
    fclose(fp);
    return 0;
}

int lv00_plugin_config_set(Lv00PluginConfig* config, const char* key, const char* value, int type) {
    if (!config || !key || !value) return -1;
    if (config->entry_count >= config->entry_capacity) return -1;
    
    /* 检查是否已存在 */
    for (size_t i = 0; i < config->entry_count; i++) {
        if (strcmp(config->entries[i].key, key) == 0) {
            strncpy(config->entries[i].value, value, sizeof(config->entries[i].value) - 1);
            config->entries[i].type = type;
            return 0;
        }
    }
    
    /* 添加新条目 */
    Lv00PluginConfigEntry* entry = &config->entries[config->entry_count++];
    strncpy(entry->key, key, sizeof(entry->key) - 1);
    strncpy(entry->value, value, sizeof(entry->value) - 1);
    entry->type = type;
    
    return 0;
}

const char* lv00_plugin_config_get(const Lv00PluginConfig* config, const char* key, const char* default_value) {
    if (!config || !key) return default_value;
    
    for (size_t i = 0; i < config->entry_count; i++) {
        if (strcmp(config->entries[i].key, key) == 0) {
            return config->entries[i].value;
        }
    }
    
    return default_value;
}

int lv00_plugin_apply_config(Lv00Plugin* plugin, const Lv00PluginConfig* config) {
    if (!plugin || !config) return -1;
    
    if (plugin->on_configure) {
        return plugin->on_configure(plugin->context, config);
    }
    
    return 0;
}

/* ============ 事件系统 ============ */

int lv00_plugin_send_event(Lv00Plugin* plugin, Lv00PluginEventType type, void* data, size_t data_size) {
    if (!plugin) return -1;
    
    Lv00PluginEvent event = {
        .type = type,
        .timestamp = get_timestamp(),
        .source = plugin,
        .data = data,
        .data_size = data_size,
        .target = NULL
    };
    
    if (plugin->on_event) {
        plugin->on_event(plugin->context, &event);
    }
    
    return 0;
}

int lv00_plugin_broadcast_event(Lv00PluginSystem* system, Lv00PluginEventType type, void* data, size_t data_size) {
    if (!system) return -1;
    
    Lv00PluginEvent event = {
        .type = type,
        .timestamp = get_timestamp(),
        .source = NULL,
        .data = data,
        .data_size = data_size,
        .target = NULL
    };
    
    for (size_t i = 0; i < system->plugin_count; i++) {
        event.source = system->plugins[i];
        if (system->plugins[i]->on_event) {
            system->plugins[i]->on_event(system->plugins[i]->context, &event);
        }
    }
    
    return 0;
}

void lv00_plugin_set_event_handler(Lv00PluginSystem* system, void (*handler)(Lv00PluginSystem*, const Lv00PluginEvent*)) {
    if (system) {
        system->event_handler = handler;
    }
}

/* ============ 依赖管理 ============ */

int lv00_plugin_resolve_dependencies(Lv00PluginSystem* system, Lv00Plugin* plugin) {
    if (!system || !plugin) return -1;
    
    for (size_t i = 0; i < plugin->info.dependency_count; i++) {
        Lv00PluginDependency* dep = plugin->info.dependencies[i];
        Lv00Plugin* dep_plugin = lv00_plugin_find(system, dep->name);
        
        if (!dep_plugin) {
            if (!dep->optional) {
                set_error(system, "Required dependency not found: %s", dep->name);
                return -1;
            }
            continue;
        }
        
        /* 检查版本兼容性 */
        if (!lv00_plugin_check_version(dep->version_constraint, dep_plugin->info.version)) {
            if (!dep->optional) {
                set_error(system, "Dependency version mismatch: %s", dep->name);
                return -1;
            }
        }
        
        /* 激活依赖 */
        if (dep_plugin->state != LV00_PLUGIN_STATE_ACTIVE) {
            if (lv00_plugin_activate(dep_plugin) != 0) {
                if (!dep->optional) {
                    set_error(system, "Failed to activate dependency: %s", dep->name);
                    return -1;
                }
            }
        }
    }
    
    return 0;
}

int lv00_plugin_check_dependencies(const Lv00Plugin* plugin) {
    if (!plugin) return -1;
    
    for (size_t i = 0; i < plugin->info.dependency_count; i++) {
        if (!plugin->info.dependencies[i]->optional) {
            return 0; /* 至少有一个非可选依赖 */
        }
    }
    
    return 1; /* 没有非可选依赖 */
}

Lv00Plugin** lv00_plugin_get_dependents(Lv00PluginSystem* system, const Lv00Plugin* plugin, size_t* count) {
    if (!system || !plugin || !count) return NULL;
    
    /* 统计依赖此插件的插件数量 */
    size_t dependent_count = 0;
    for (size_t i = 0; i < system->plugin_count; i++) {
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
    Lv00Plugin** result = (Lv00Plugin**)malloc(sizeof(Lv00Plugin*) * dependent_count);
    if (!result) {
        *count = 0;
        return NULL;
    }
    
    /* 填充结果 */
    size_t idx = 0;
    for (size_t i = 0; i < system->plugin_count; i++) {
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

int lv00_plugin_system_add_search_path(Lv00PluginSystem* system, const char* path) {
    if (!system || !path) return -1;
    
    /* 检查是否已存在 */
    for (size_t i = 0; i < system->search_path_count; i++) {
        if (strcmp(system->search_paths[i], path) == 0) {
            return 0;
        }
    }
    
    /* 添加新路径 */
    char** new_paths = (char**)realloc(system->search_paths, 
        sizeof(char*) * (system->search_path_count + 1));
    if (!new_paths) return -1;
    
    system->search_paths = new_paths;
    system->search_paths[system->search_path_count] = (char*)malloc(strlen(path) + 1);
    if (!system->search_paths[system->search_path_count]) return -1;
    
    strcpy(system->search_paths[system->search_path_count], path);
    system->search_path_count++;
    
    return 0;
}

int lv00_plugin_system_remove_search_path(Lv00PluginSystem* system, const char* path) {
    if (!system || !path) return -1;
    
    for (size_t i = 0; i < system->search_path_count; i++) {
        if (strcmp(system->search_paths[i], path) == 0) {
            free(system->search_paths[i]);
            system->search_paths[i] = system->search_paths[--system->search_path_count];
            return 0;
        }
    }
    
    return -1;
}

char** lv00_plugin_system_get_search_paths(Lv00PluginSystem* system, size_t* count) {
    if (!system || !count) return NULL;
    
    *count = system->search_path_count;
    return system->search_paths;
}

/* ============ 自动加载 ============ */

int lv00_plugin_system_autoload(Lv00PluginSystem* system, const char* directory) {
    if (!system || !directory) return -1;

    /* 添加搜索路径 */
    lv00_plugin_system_add_search_path(system, directory);

    /* 扫描目录中的 .dll 文件（Windows）或 .so 文件（Linux） */
#ifdef _WIN32
    char search_pattern[MAX_PATH];
    snprintf(search_pattern, sizeof(search_pattern), "%s\\*.dll", directory);

    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(search_pattern, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        return 0; /* 目录为空或不存在，不算错误 */
    }

    do {
        /* 跳过 . 和 .. 目录 */
        if (strcmp(find_data.cFileName, ".") == 0 ||
            strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }

        /* 构造完整路径 */
        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s\\%s", directory, find_data.cFileName);

        /* 尝试加载为插件 */
        Lv00Plugin* plugin = lv00_plugin_load(system, full_path);
        if (plugin) {
            /* 版本兼容性检查：验证插件版本是否与系统版本兼容 */
            if (plugin->info.version[0] != '\0') {
                if (!lv00_plugin_check_api_compatibility(
                        system->version,
                        (LV00_PLUGIN_SYSTEM_VERSION_MAJOR << 16) |
                        (LV00_PLUGIN_SYSTEM_VERSION_MINOR << 8))) {
                    /* 插件 API 版本不兼容，记录警告并跳过激活 */
                    set_error(system,
                              "Plugin '%s' version '%s' may be incompatible with "
                              "system API version %d.%d.%d. Loading but not activating.",
                              plugin->info.name,
                              plugin->info.version,
                              LV00_PLUGIN_SYSTEM_VERSION_MAJOR,
                              LV00_PLUGIN_SYSTEM_VERSION_MINOR,
                              LV00_PLUGIN_SYSTEM_VERSION_PATCH);
                    /* 不卸载插件，但也不自动激活，让用户决定 */
                }
            }
        }

    } while (FindNextFileA(hFind, &find_data));

    FindClose(hFind);
#else
    /* Linux/macOS: 使用 opendir/readdir 扫描 .so 文件 */
    #include <dirent.h>
    DIR* dir = opendir(directory);
    if (!dir) return 0;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        /* 跳过 . 和 .. */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        /* 检查是否为 .so 文件 */
        size_t name_len = strlen(entry->d_name);
        if (name_len > 3 && strcmp(entry->d_name + name_len - 3, ".so") == 0) {
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", directory, entry->d_name);

            /* 尝试加载为插件 */
            Lv00Plugin* plugin = lv00_plugin_load(system, full_path);
            if (plugin) {
                /* 版本兼容性检查：验证插件版本是否与系统版本兼容 */
                if (plugin->info.version[0] != '\0') {
                    if (!lv00_plugin_check_api_compatibility(
                            system->version,
                            (LV00_PLUGIN_SYSTEM_VERSION_MAJOR << 16) |
                            (LV00_PLUGIN_SYSTEM_VERSION_MINOR << 8))) {
                        set_error(system,
                                  "Plugin '%s' version '%s' may be incompatible with "
                                  "system API version %d.%d.%d. Loading but not activating.",
                                  plugin->info.name,
                                  plugin->info.version,
                                  LV00_PLUGIN_SYSTEM_VERSION_MAJOR,
                                  LV00_PLUGIN_SYSTEM_VERSION_MINOR,
                                  LV00_PLUGIN_SYSTEM_VERSION_PATCH);
                    }
                }
            }
        }
    }

    closedir(dir);
#endif

    return 0;
}

int lv00_plugin_system_autoload_all(Lv00PluginSystem* system) {
    if (!system) return -1;
    
    /* 遍历所有搜索路径，自动加载其中的插件（含版本兼容性检查） */
    for (size_t i = 0; i < system->search_path_count; i++) {
        lv00_plugin_system_autoload(system, system->search_paths[i]);
    }
    
    return 0;
}

/* ============ 版本兼容性 ============ */

/* 版本兼容性常量 */
#define LV00_PLUGIN_VERSION_OK       1
#define LV00_PLUGIN_VERSION_MISMATCH 0

/* 解析语义版本字符串 "major.minor.patch" */
static int parse_semver(const char* ver_str, int* major, int* minor, int* patch) {
    if (!ver_str) return -1;
    *major = *minor = *patch = 0;
    return sscanf(ver_str, "%d.%d.%d", major, minor, patch);
}

int lv00_plugin_check_version(const char* required, const char* provided) {
    if (!required || !provided) return LV00_PLUGIN_VERSION_MISMATCH;

    /* 解析 required 版本 */
    int req_major, req_minor, req_patch;
    if (parse_semver(required, &req_major, &req_minor, &req_patch) < 1) {
        return LV00_PLUGIN_VERSION_MISMATCH;
    }

    /* 解析 provided 版本 */
    int prov_major, prov_minor, prov_patch;
    if (parse_semver(provided, &prov_major, &prov_minor, &prov_patch) < 1) {
        return LV00_PLUGIN_VERSION_MISMATCH;
    }

    /* 语义版本比较：逐级比较 major -> minor -> patch */
    if (prov_major > req_major) return LV00_PLUGIN_VERSION_OK;
    if (prov_major < req_major) return LV00_PLUGIN_VERSION_MISMATCH;

    /* major 相同，比较 minor */
    if (prov_minor > req_minor) return LV00_PLUGIN_VERSION_OK;
    if (prov_minor < req_minor) return LV00_PLUGIN_VERSION_MISMATCH;

    /* minor 相同，比较 patch */
    if (prov_patch >= req_patch) return LV00_PLUGIN_VERSION_OK;

    return LV00_PLUGIN_VERSION_MISMATCH;
}

int lv00_plugin_check_api_compatibility(uint32_t required, uint32_t provided) {
    return provided >= required;
}

/* ============ 插件信息 ============ */

char* lv00_plugin_get_info_json(const Lv00Plugin* plugin) {
    if (!plugin) return NULL;
    
    size_t size = 1024;
    char* json = (char*)malloc(size);
    if (!json) return NULL;
    
    snprintf(json, size,
        "{"
        "\"name\":\"%s\","
        "\"version\":\"%s\","
        "\"author\":\"%s\","
        "\"description\":\"%s\","
        "\"state\":%d,"
        "\"type\":%d"
        "}",
        plugin->info.name,
        plugin->info.version,
        plugin->info.author,
        plugin->info.description,
        plugin->state,
        plugin->info.type
    );
    
    return json;
}

char* lv00_plugin_system_get_info_json(const Lv00PluginSystem* system) {
    if (!system) return NULL;
    
    size_t size = 2048 + system->plugin_count * 512;
    char* json = (char*)malloc(size);
    if (!json) return NULL;
    
    char* ptr = json;
    ptr += sprintf(ptr, "{"
        "\"version\":%u,"
        "\"plugin_count\":%zu,"
        "\"interface_count\":%zu,"
        "\"plugins\":[",
        system->version,
        system->plugin_count,
        system->interface_count
    );
    
    for (size_t i = 0; i < system->plugin_count; i++) {
        ptr += sprintf(ptr, "{"
            "\"name\":\"%s\","
            "\"version\":\"%s\","
            "\"state\":%d"
            "}%s",
            system->plugins[i]->info.name,
            system->plugins[i]->info.version,
            system->plugins[i]->state,
            (i < system->plugin_count - 1) ? "," : ""
        );
    }
    
    ptr += sprintf(ptr, "]}");
    
    return json;
}

/* ============ 错误处理 ============ */

const char* lv00_plugin_get_last_error(const Lv00Plugin* plugin) {
    if (!plugin || !plugin->context) return NULL;
    
    PluginSystemInternal* internal = 
        (PluginSystemInternal*)plugin->context->system->mutex;
    return internal->last_error;
}

const char* lv00_plugin_system_get_last_error(const Lv00PluginSystem* system) {
    if (!system) return NULL;
    
    PluginSystemInternal* internal = (PluginSystemInternal*)system->mutex;
    return internal->last_error;
}

void lv00_plugin_clear_error(Lv00Plugin* plugin) {
    if (!plugin || !plugin->context) return;
    
    PluginSystemInternal* internal = 
        (PluginSystemInternal*)plugin->context->system->mutex;
    internal->last_error[0] = '\0';
}

void lv00_plugin_system_clear_error(Lv00PluginSystem* system) {
    if (!system) return;
    
    PluginSystemInternal* internal = (PluginSystemInternal*)system->mutex;
    internal->last_error[0] = '\0';
}
