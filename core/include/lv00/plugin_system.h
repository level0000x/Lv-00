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
 * @version 1.1.0
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

/**
 * @brief 创建插件系统实例
 * @param ctx LV-00 上下文指针
 * @return 成功返回插件系统指针，失败返回 NULL
 */
Lv00PluginSystem* lv00_plugin_system_create(Lv00Context* ctx);

/**
 * @brief 销毁插件系统并释放所有资源
 * @param system 指向待销毁的插件系统的指针
 */
void lv00_plugin_system_destroy(Lv00PluginSystem* system);

/**
 * @brief 初始化插件系统，准备加载插件
 * @param system 指向插件系统的指针
 * @return 成功返回 0，失败返回非零错误码
 */
int lv00_plugin_system_init(Lv00PluginSystem* system);

/**
 * @brief 清理插件系统，卸载所有插件并释放内部资源
 * @param system 指向插件系统的指针
 */
void lv00_plugin_system_cleanup(Lv00PluginSystem* system);
/* ============ 插件加载与卸载 ============ */

/**
 * @brief 从指定路径加载插件到插件系统
 * @param system 指向插件系统的指针
 * @param path 插件文件的路径
 * @return 成功返回加载的插件指针，失败返回 NULL
 */
Lv00Plugin* lv00_plugin_load(
    Lv00PluginSystem* system,
    const char* path
);

/**
 * @brief 从插件系统中卸载指定插件
 * @param system 指向插件系统的指针
 * @param plugin 指向待卸载的插件的指针
 * @return 成功返回 0，失败返回非零错误码
 */
int lv00_plugin_unload(
    Lv00PluginSystem* system,
    Lv00Plugin* plugin
);

/**
 * @brief 重新加载指定插件（先卸载再加载）
 * @param system 指向插件系统的指针
 * @param plugin 指向待重载的插件的指针
 * @return 成功返回 0，失败返回非零错误码
 */
int lv00_plugin_reload(
    Lv00PluginSystem* system,
    Lv00Plugin* plugin
);
/* ============ 插件激活与停用 ============ */

/**
 * @brief 激活插件，使其进入工作状态
 * @param plugin 指向待激活的插件的指针
 * @return 成功返回 0，失败返回非零错误码
 */
int lv00_plugin_activate(Lv00Plugin* plugin);

/**
 * @brief 停用插件，使其进入非工作状态
 * @param plugin 指向待停用的插件的指针
 * @return 成功返回 0，失败返回非零错误码
 */
int lv00_plugin_deactivate(Lv00Plugin* plugin);

/**
 * @brief 检查插件是否处于激活状态
 * @param plugin 指向插件的指针
 * @return 非零表示已激活，0 表示未激活
 */
int lv00_plugin_is_active(const Lv00Plugin* plugin);

/**
 * @brief 获取插件的当前状态
 * @param plugin 指向插件的指针
 * @return 返回插件的当前 Lv00PluginState 枚举值
 */
Lv00PluginState lv00_plugin_get_state(const Lv00Plugin* plugin);
/* ============ 插件查询 ============ */

/**
 * @brief 按名称在插件系统中查找插件
 * @param system 指向插件系统的指针
 * @param name 要查找的插件名称
 * @return 成功返回找到的插件指针，未找到返回 NULL
 */
Lv00Plugin* lv00_plugin_find(
    Lv00PluginSystem* system,
    const char* name
);

/**
 * @brief 获取插件系统中的所有插件列表
 * @param system 指向插件系统的指针
 * @param count 输出参数，返回插件数量
 * @return 返回插件指针数组，调用者需负责释放
 */
Lv00Plugin** lv00_plugin_get_all(
    Lv00PluginSystem* system,
    size_t* count
);

/**
 * @brief 按类型筛选获取插件列表
 * @param system 指向插件系统的指针
 * @param type 要筛选的插件类型
 * @param count 输出参数，返回匹配的插件数量
 * @return 返回匹配类型的插件指针数组，调用者需负责释放
 */
Lv00Plugin** lv00_plugin_get_by_type(
    Lv00PluginSystem* system,
    Lv00PluginType type,
    size_t* count
);

/**
 * @brief 按状态筛选获取插件列表
 * @param system 指向插件系统的指针
 * @param state 要筛选的插件状态
 * @param count 输出参数，返回匹配的插件数量
 * @return 返回匹配状态的插件指针数组，调用者需负责释放
 */
Lv00Plugin** lv00_plugin_get_by_state(
    Lv00PluginSystem* system,
    Lv00PluginState state,
    size_t* count
);
/* ============ 接口注册与查询 ============ */

/**
 * @brief 为插件注册一个接口
 * @param plugin 指向插件的指针
 * @param interface 指向待注册接口的指针
 * @return 成功返回 0，失败返回非零错误码
 */
int lv00_plugin_register_interface(
    Lv00Plugin* plugin,
    Lv00PluginInterface* interface
);

/**
 * @brief 从插件注销指定名称的接口
 * @param plugin 指向插件的指针
 * @param name 要注销的接口名称
 * @return 成功返回 0，失败返回非零错误码
 */
int lv00_plugin_unregister_interface(
    Lv00Plugin* plugin,
    const char* name
);

/**
 * @brief 在插件系统中查询指定名称和版本的接口
 * @param system 指向插件系统的指针
 * @param name 要查询的接口名称
 * @param version 要求的接口版本号
 * @return 成功返回匹配的接口指针，未找到返回 NULL
 */
Lv00PluginInterface* lv00_plugin_query_interface(
    Lv00PluginSystem* system,
    const char* name,
    uint32_t version
);

/**
 * @brief 按名称模式模糊查询接口列表
 * @param system 指向插件系统的指针
 * @param pattern 接口名称匹配模式
 * @param count 输出参数，返回匹配的接口数量
 * @return 返回匹配的接口指针数组，调用者需负责释放
 */
Lv00PluginInterface** lv00_plugin_query_interfaces(
    Lv00PluginSystem* system,
    const char* pattern,
    size_t* count
);
/* ============ 插件配置 ============ */

/**
 * @brief 创建插件配置对象
 * @return 成功返回插件配置指针，失败返回 NULL
 */
Lv00PluginConfig* lv00_plugin_config_create(void);

/**
 * @brief 销毁插件配置对象并释放资源
 * @param config 指向待销毁的插件配置的指针
 */
void lv00_plugin_config_destroy(Lv00PluginConfig* config);

/**
 * @brief 从文件加载插件配置
 * @param config 指向插件配置的指针
 * @param filepath 配置文件路径
 * @return 成功返回 0，失败返回非零错误码
 */
int lv00_plugin_config_load(
    Lv00PluginConfig* config,
    const char* filepath
);

/**
 * @brief 将插件配置保存到文件
 * @param config 指向插件配置的指针
 * @param filepath 目标文件路径
 * @return 成功返回 0，失败返回非零错误码
 */
int lv00_plugin_config_save(
    const Lv00PluginConfig* config,
    const char* filepath
);

/**
 * @brief 设置插件配置项
 * @param config 指向插件配置的指针
 * @param key 配置键名
 * @param value 配置值
 * @param type 配置值类型（0=string, 1=int, 2=float, 3=bool, 4=json）
 * @return 成功返回 0，失败返回非零错误码
 */
int lv00_plugin_config_set(
    Lv00PluginConfig* config,
    const char* key,
    const char* value,
    int type
);

/**
 * @brief 获取插件配置项的值
 * @param config 指向插件配置的指针
 * @param key 配置键名
 * @param default_value 配置项不存在时返回的默认值
 * @return 返回配置值字符串，不存在时返回 default_value
 */
const char* lv00_plugin_config_get(
    const Lv00PluginConfig* config,
    const char* key,
    const char* default_value
);

/**
 * @brief 将配置应用到指定插件
 * @param plugin 指向插件的指针
 * @param config 指向要应用的插件配置的指针
 * @return 成功返回 0，失败返回非零错误码
 */
int lv00_plugin_apply_config(
    Lv00Plugin* plugin,
    const Lv00PluginConfig* config
);
/* ============ 事件系统 ============ */

/**
 * @brief 从插件发送事件到目标插件
 * @param plugin 事件发送方插件指针
 * @param type 事件类型
 * @param data 事件数据指针
 * @param data_size 事件数据大小（字节）
 * @return 成功返回 0，失败返回非零错误码
 */
int lv00_plugin_send_event(
    Lv00Plugin* plugin,
    Lv00PluginEventType type,
    void* data,
    size_t data_size
);

/**
 * @brief 向插件系统中所有插件广播事件
 * @param system 指向插件系统的指针
 * @param type 事件类型
 * @param data 事件数据指针
 * @param data_size 事件数据大小（字节）
 * @return 成功返回 0，失败返回非零错误码
 */
int lv00_plugin_broadcast_event(
    Lv00PluginSystem* system,
    Lv00PluginEventType type,
    void* data,
    size_t data_size
);

/**
 * @brief 设置插件系统级事件处理器
 * @param system 指向插件系统的指针
 * @param handler 事件处理回调函数，为 NULL 时清除处理器
 */
void lv00_plugin_set_event_handler(
    Lv00PluginSystem* system,
    void (*handler)(Lv00PluginSystem*, const Lv00PluginEvent*)
);
/* ============ 依赖管理 ============ */

/**
 * @brief 解析并加载插件的所有依赖项
 * @param system 指向插件系统的指针
 * @param plugin 指向需要解析依赖的插件的指针
 * @return 成功返回 0，失败返回非零错误码
 */
int lv00_plugin_resolve_dependencies(
    Lv00PluginSystem* system,
    Lv00Plugin* plugin
);

/**
 * @brief 检查插件的所有依赖是否满足
 * @param plugin 指向需要检查的插件的指针
 * @return 所有依赖满足返回 0，存在未满足的依赖返回非零错误码
 */
int lv00_plugin_check_dependencies(
    const Lv00Plugin* plugin
);

/**
 * @brief 获取依赖了指定插件的所有插件列表
 * @param system 指向插件系统的指针
 * @param plugin 指向被依赖的插件的指针
 * @param count 输出参数，返回依赖者数量
 * @return 返回依赖者插件指针数组，调用者需负责释放
 */
Lv00Plugin** lv00_plugin_get_dependents(
    Lv00PluginSystem* system,
    const Lv00Plugin* plugin,
    size_t* count
);
/* ============ 搜索路径管理 ============ */

/**
 * @brief 添加插件搜索路径
 * @param system 指向插件系统的指针
 * @param path 要添加的搜索路径
 * @return 成功返回 0，失败返回非零错误码
 */
int lv00_plugin_system_add_search_path(
    Lv00PluginSystem* system,
    const char* path
);

/**
 * @brief 移除插件搜索路径
 * @param system 指向插件系统的指针
 * @param path 要移除的搜索路径
 * @return 成功返回 0，失败返回非零错误码
 */
int lv00_plugin_system_remove_search_path(
    Lv00PluginSystem* system,
    const char* path
);

/**
 * @brief 获取当前所有插件搜索路径
 * @param system 指向插件系统的指针
 * @param count 输出参数，返回搜索路径数量
 * @return 返回搜索路径字符串数组，调用者需负责释放
 */
char** lv00_plugin_system_get_search_paths(
    Lv00PluginSystem* system,
    size_t* count
);
/* ============ 自动加载 ============ */

/**
 * @brief 自动加载指定目录下的所有插件
 * @param system 指向插件系统的指针
 * @param directory 要扫描的插件目录路径
 * @return 成功返回 0，失败返回非零错误码
 */
int lv00_plugin_system_autoload(
    Lv00PluginSystem* system,
    const char* directory
);

/**
 * @brief 自动加载所有搜索路径下的插件
 * @param system 指向插件系统的指针
 * @return 成功返回 0，失败返回非零错误码
 */
int lv00_plugin_system_autoload_all(Lv00PluginSystem* system);
/* ============ 版本兼容性 ============ */

/**
 * @brief 检查插件版本号是否满足所需版本约束
 * @param required 要求的版本约束字符串（如 ">=1.0.0"）
 * @param provided 实际提供的版本号字符串
 * @return 满足要求返回 0，不满足返回非零错误码
 */
int lv00_plugin_check_version(
    const char* required,
    const char* provided
);

/**
 * @brief 检查插件 API 版本兼容性
 * @param required 要求的 API 版本号
 * @param provided 实际提供的 API 版本号
 * @return 兼容返回 0，不兼容返回非零错误码
 */
int lv00_plugin_check_api_compatibility(
    uint32_t required,
    uint32_t provided
);
/* ============ 插件信息 ============ */

/**
 * @brief 获取插件信息的 JSON 格式字符串
 * @param plugin 指向插件的指针
 * @return 返回 JSON 格式的插件信息字符串，调用者需负责释放
 */
char* lv00_plugin_get_info_json(const Lv00Plugin* plugin);

/**
 * @brief 获取插件系统信息的 JSON 格式字符串
 * @param system 指向插件系统的指针
 * @return 返回 JSON 格式的系统信息字符串，调用者需负责释放
 */
char* lv00_plugin_system_get_info_json(const Lv00PluginSystem* system);
/* ============ 错误处理 ============ */

/**
 * @brief 获取插件的最后一次错误信息
 * @param plugin 指向插件的指针
 * @return 返回错误信息字符串，无错误时返回空字符串
 */
const char* lv00_plugin_get_last_error(const Lv00Plugin* plugin);

/**
 * @brief 获取插件系统的最后一次错误信息
 * @param system 指向插件系统的指针
 * @return 返回错误信息字符串，无错误时返回空字符串
 */
const char* lv00_plugin_system_get_last_error(const Lv00PluginSystem* system);

/**
 * @brief 清除插件的错误状态
 * @param plugin 指向插件的指针
 */
void lv00_plugin_clear_error(Lv00Plugin* plugin);

/**
 * @brief 清除插件系统的错误状态
 * @param system 指向插件系统的指针
 */
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
