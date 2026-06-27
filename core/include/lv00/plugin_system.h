/**
 * @file plugin_system.h
 * @brief LV-00 模块化插件系统
 *
 * 提供完整的插件架构，支持：
 * - 动态插件加载与卸载
 * - 插件接口注册与发现
 * - 插件间通信
 * - 版本兼容性检查
 * - 热插拔支持
 *
 * @author Lv-00 Project
 * @version 1.0
 */
#ifndef LV00_PLUGIN_SYSTEM_H
#define LV00_PLUGIN_SYSTEM_H
#define LV00_PLUGIN_FULL_TYPE 1
#include <lv00.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* ============ 版本与常量 ============ */
#define LV00_PLUGIN_SYSTEM_VERSION_MAJOR 1
#define LV00_PLUGIN_SYSTEM_VERSION_MINOR 0
#define LV00_PLUGIN_SYSTEM_VERSION_PATCH 0
#define LV00_PLUGIN_API_VERSION 1
#define LV00_MAX_PLUGINS 256
#define LV00_MAX_INTERFACES 128
#define LV00_PLUGIN_NAME_MAX 64
#define LV00_PLUGIN_DESC_MAX 256
#define LV00_PLUGIN_AUTHOR_MAX 128
#define LV00_PLUGIN_PATH_MAX 512
/* ============ 前向声明 ============ */
typedef struct Lv00PluginSystem Lv00PluginSystem;
typedef struct Lv00Plugin Lv00Plugin;
typedef struct Lv00PluginInterface Lv00PluginInterface;
typedef struct Lv00PluginContext Lv00PluginContext;
typedef struct Lv00PluginConfig Lv00PluginConfig;
typedef struct Lv00PluginEvent Lv00PluginEvent;
typedef struct Lv00PluginDependency Lv00PluginDependency;
/* ============ 插件状态枚举 ============ */
typedef enum {
    LV00_PLUGIN_STATE_UNLOADED = 0,
    LV00_PLUGIN_STATE_LOADING,
    LV00_PLUGIN_STATE_LOADED,
    LV00_PLUGIN_STATE_INITIALIZING,
    LV00_PLUGIN_STATE_ACTIVE,
    LV00_PLUGIN_STATE_DEACTIVATING,
    LV00_PLUGIN_STATE_UNLOADING,
    LV00_PLUGIN_STATE_ERROR
} Lv00PluginState;
/* ============ 插件类型枚举 ============ */
typedef enum {
    LV00_PLUGIN_TYPE_NATIVE = 0,
    LV00_PLUGIN_TYPE_SCRIPT,
    LV00_PLUGIN_TYPE_EXTENSION,
    LV00_PLUGIN_TYPE_BUNDLE
} Lv00PluginType;
/* ============ 事件类型枚举 ============ */
typedef enum {
    LV00_PLUGIN_EVENT_LOAD = 0,
    LV00_PLUGIN_EVENT_UNLOAD,
    LV00_PLUGIN_EVENT_ACTIVATE,
    LV00_PLUGIN_EVENT_DEACTIVATE,
    LV00_PLUGIN_EVENT_CONFIG_CHANGE,
    LV00_PLUGIN_EVENT_DEPENDENCY_ADD,
    LV00_PLUGIN_EVENT_DEPENDENCY_REMOVE,
    LV00_PLUGIN_EVENT_MESSAGE,
    LV00_PLUGIN_EVENT_SHUTDOWN
} Lv00PluginEventType;
/* ============ 插件信息 ============ */
typedef struct {
    char name[LV00_PLUGIN_NAME_MAX];
    char description[LV00_PLUGIN_DESC_MAX];
    char author[LV00_PLUGIN_AUTHOR_MAX];
    char version[32];
    uint32_t api_version;
    Lv00PluginType type;

    /* 依赖信息 */
    Lv00PluginDependency** dependencies;
    size_t dependency_count;

    /* 导出接口 */
    Lv00PluginInterface** interfaces;
    size_t interface_count;

    /* 元数据 */
    char license[64];
    char homepage[256];
    uint64_t build_timestamp;
} Lv00PluginInfo;
/* ============ 插件依赖 ============ */
struct Lv00PluginDependency {
    char name[LV00_PLUGIN_NAME_MAX];
    char version_constraint[64];
    int optional;
};
/* ============ 插件接口 ============ */
struct Lv00PluginInterface {
    char name[LV00_PLUGIN_NAME_MAX];
    uint32_t version;

    /* 接口函数表 */
    void** functions;
    size_t function_count;

    /* 接口元数据 */
    char description[LV00_PLUGIN_DESC_MAX];

    /* 所属插件 */
    Lv00Plugin* owner;
};
/* ============ 插件事件 ============ */
struct Lv00PluginEvent {
    Lv00PluginEventType type;
    uint64_t timestamp;

    /* 事件源 */
    Lv00Plugin* source;

    /* 事件数据 */
    void* data;
    size_t data_size;

    /* 目标插件（可为NULL表示广播） */
    Lv00Plugin* target;
};
/* ============ 插件配置 ============ */
typedef struct {
    char key[128];
    char value[1024];
    int type;  /* 0=string, 1=int, 2=float, 3=bool, 4=json */
} Lv00PluginConfigEntry;
struct Lv00PluginConfig {
    Lv00PluginConfigEntry* entries;
    size_t entry_count;
    size_t entry_capacity;

    /* 配置文件路径 */
    char config_file[LV00_PLUGIN_PATH_MAX];
};
/* ============ 插件上下文 ============ */
struct Lv00PluginContext {
    /* 所属插件 */
    Lv00Plugin* plugin;

    /* 系统上下文 */
    Lv00PluginSystem* system;
    Lv00Context* lv00_context;

    /* 配置 */
    Lv00PluginConfig* config;

    /* 用户数据 */
    void* user_data;

    /* 日志回调 */
    void (*log)(Lv00PluginContext* ctx, int level, const char* message);

    /* 事件回调 */
    void (*on_event)(Lv00PluginContext* ctx, const Lv00PluginEvent* event);

    /* 接口查找 */
    Lv00PluginInterface* (*query_interface)(
        Lv00PluginContext* ctx,
        const char* name,
        uint32_t version
    );

    /* 发送事件 */
    int (*send_event)(
        Lv00PluginContext* ctx,
        Lv00PluginEventType type,
        void* data,
        size_t data_size,
        Lv00Plugin* target
    );
};
/* ============ 插件结构 ============ */
struct Lv00Plugin {
    /* 基本信息 */
    Lv00PluginInfo info;
    Lv00PluginState state;

    /* 文件路径 */
    char path[LV00_PLUGIN_PATH_MAX];

    /* 动态库句柄 */
    void* handle;

    /* 插件上下文 */
    Lv00PluginContext* context;

    /* 生命周期回调 */
    int (*on_load)(Lv00PluginContext* ctx);
    int (*on_unload)(Lv00PluginContext* ctx);
    int (*on_activate)(Lv00PluginContext* ctx);
    int (*on_deactivate)(Lv00PluginContext* ctx);
    int (*on_configure)(Lv00PluginContext* ctx, const Lv00PluginConfig* config);
    int (*on_event)(Lv00PluginContext* ctx, const Lv00PluginEvent* event);

    /* 注册表 */
    Lv00PluginInterface** registered_interfaces;
    size_t registered_interface_count;

    /* 依赖解析 */
    Lv00Plugin** resolved_dependencies;
    size_t resolved_dependency_count;

    /* 加载时间 */
    uint64_t load_time;
    uint64_t activate_time;
};
/* ============ 插件系统 ============ */
struct Lv00PluginSystem {
    /* 版本信息 */
    uint32_t version;

    /* 插件列表 */
    Lv00Plugin** plugins;
    size_t plugin_count;
    size_t plugin_capacity;

    /* 接口注册表 */
    Lv00PluginInterface** interfaces;
    size_t interface_count;
    size_t interface_capacity;

    /* 搜索路径 */
    char** search_paths;
    size_t search_path_count;

    /* 系统上下文 */
    Lv00Context* lv00_context;

    /* 事件处理器 */
    void (*event_handler)(Lv00PluginSystem* system, const Lv00PluginEvent* event);

    /* 互斥锁 */
    void* mutex;

    /* 初始化标志 */
    int initialized;
};
/* ============ 生命周期管理 ============ */
Lv00PluginSystem* lv00_plugin_system_create(Lv00Context* ctx);
void lv00_plugin_system_destroy(Lv00PluginSystem* system);
int lv00_plugin_system_init(Lv00PluginSystem* system);
void lv00_plugin_system_cleanup(Lv00PluginSystem* system);
/* ============ 插件加载与卸载 ============ */
Lv00Plugin* lv00_plugin_load(
    Lv00PluginSystem* system,
    const char* path
);
int lv00_plugin_unload(
    Lv00PluginSystem* system,
    Lv00Plugin* plugin
);
int lv00_plugin_reload(
    Lv00PluginSystem* system,
    Lv00Plugin* plugin
);
/* ============ 插件激活与停用 ============ */
int lv00_plugin_activate(Lv00Plugin* plugin);
int lv00_plugin_deactivate(Lv00Plugin* plugin);
int lv00_plugin_is_active(const Lv00Plugin* plugin);
Lv00PluginState lv00_plugin_get_state(const Lv00Plugin* plugin);
/* ============ 插件查询 ============ */
Lv00Plugin* lv00_plugin_find(
    Lv00PluginSystem* system,
    const char* name
);
Lv00Plugin** lv00_plugin_get_all(
    Lv00PluginSystem* system,
    size_t* count
);
Lv00Plugin** lv00_plugin_get_by_type(
    Lv00PluginSystem* system,
    Lv00PluginType type,
    size_t* count
);
Lv00Plugin** lv00_plugin_get_by_state(
    Lv00PluginSystem* system,
    Lv00PluginState state,
    size_t* count
);
/* ============ 接口注册与查询 ============ */
int lv00_plugin_register_interface(
    Lv00Plugin* plugin,
    Lv00PluginInterface* interface
);
int lv00_plugin_unregister_interface(
    Lv00Plugin* plugin,
    const char* name
);
Lv00PluginInterface* lv00_plugin_query_interface(
    Lv00PluginSystem* system,
    const char* name,
    uint32_t version
);
Lv00PluginInterface** lv00_plugin_query_interfaces(
    Lv00PluginSystem* system,
    const char* pattern,
    size_t* count
);
/* ============ 插件配置 ============ */
Lv00PluginConfig* lv00_plugin_config_create(void);
void lv00_plugin_config_destroy(Lv00PluginConfig* config);
int lv00_plugin_config_load(
    Lv00PluginConfig* config,
    const char* filepath
);
int lv00_plugin_config_save(
    const Lv00PluginConfig* config,
    const char* filepath
);
int lv00_plugin_config_set(
    Lv00PluginConfig* config,
    const char* key,
    const char* value,
    int type
);
const char* lv00_plugin_config_get(
    const Lv00PluginConfig* config,
    const char* key,
    const char* default_value
);
int lv00_plugin_apply_config(
    Lv00Plugin* plugin,
    const Lv00PluginConfig* config
);
/* ============ 事件系统 ============ */
int lv00_plugin_send_event(
    Lv00Plugin* plugin,
    Lv00PluginEventType type,
    void* data,
    size_t data_size
);
int lv00_plugin_broadcast_event(
    Lv00PluginSystem* system,
    Lv00PluginEventType type,
    void* data,
    size_t data_size
);
void lv00_plugin_set_event_handler(
    Lv00PluginSystem* system,
    void (*handler)(Lv00PluginSystem*, const Lv00PluginEvent*)
);
/* ============ 依赖管理 ============ */
int lv00_plugin_resolve_dependencies(
    Lv00PluginSystem* system,
    Lv00Plugin* plugin
);
int lv00_plugin_check_dependencies(
    const Lv00Plugin* plugin
);
Lv00Plugin** lv00_plugin_get_dependents(
    Lv00PluginSystem* system,
    const Lv00Plugin* plugin,
    size_t* count
);
/* ============ 搜索路径管理 ============ */
int lv00_plugin_system_add_search_path(
    Lv00PluginSystem* system,
    const char* path
);
int lv00_plugin_system_remove_search_path(
    Lv00PluginSystem* system,
    const char* path
);
char** lv00_plugin_system_get_search_paths(
    Lv00PluginSystem* system,
    size_t* count
);
/* ============ 自动加载 ============ */
int lv00_plugin_system_autoload(
    Lv00PluginSystem* system,
    const char* directory
);
int lv00_plugin_system_autoload_all(Lv00PluginSystem* system);
/* ============ 版本兼容性 ============ */
int lv00_plugin_check_version(
    const char* required,
    const char* provided
);
int lv00_plugin_check_api_compatibility(
    uint32_t required,
    uint32_t provided
);
/* ============ 插件信息 ============ */
char* lv00_plugin_get_info_json(const Lv00Plugin* plugin);
char* lv00_plugin_system_get_info_json(const Lv00PluginSystem* system);
/* ============ 错误处理 ============ */
const char* lv00_plugin_get_last_error(const Lv00Plugin* plugin);
const char* lv00_plugin_system_get_last_error(const Lv00PluginSystem* system);
void lv00_plugin_clear_error(Lv00Plugin* plugin);
void lv00_plugin_system_clear_error(Lv00PluginSystem* system);
/* ============ 插件开发辅助宏 ============ */
#define LV00_PLUGIN_EXPORT __attribute__((visibility("default")))
#define LV00_PLUGIN_DECLARE(name) \
    LV00_PLUGIN_EXPORT const char* lv00_plugin_name = name; \
    LV00_PLUGIN_EXPORT const uint32_t lv00_plugin_version = LV00_PLUGIN_API_VERSION;
#define LV00_PLUGIN_ENTRY() \
    LV00_PLUGIN_EXPORT int lv00_plugin_load_entry(Lv00PluginContext* ctx) \
    { \
        ctx->plugin->on_load = lv00_plugin_on_load; \
        ctx->plugin->on_unload = lv00_plugin_on_unload; \
        ctx->plugin->on_activate = lv00_plugin_on_activate; \
        ctx->plugin->on_deactivate = lv00_plugin_on_deactivate; \
        ctx->plugin->on_configure = lv00_plugin_on_configure; \
        ctx->plugin->on_event = lv00_plugin_on_event; \
        return 0; \
    }
/* 插件必须实现的回调函数声明 */
extern int lv00_plugin_on_load(Lv00PluginContext* ctx);
extern int lv00_plugin_on_unload(Lv00PluginContext* ctx);
extern int lv00_plugin_on_activate(Lv00PluginContext* ctx);
extern int lv00_plugin_on_deactivate(Lv00PluginContext* ctx);
extern int lv00_plugin_on_configure(Lv00PluginContext* ctx, const Lv00PluginConfig* config);
extern int lv00_plugin_on_event(Lv00PluginContext* ctx, const Lv00PluginEvent* event);
#ifdef __cplusplus
}
#endif
#endif /* LV00_PLUGIN_SYSTEM_H */
