#ifndef lv_REGISTRY_H
#define lv_REGISTRY_H

#include <stdbool.h>
#include <stddef.h>
#include "lv_platform.h"  /* for lvMutex, lv_MUTEX_* */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 通用注册表条目：名称 + 工厂函数/泛型值 */
typedef struct lvRegistryEntry {
    char *name;                /**< 条目名称（key，注册表内部拷贝并管理生命周期） */
    void *(*create)(void);     /**< 创建函数（name→factory 形态，返回 void*，调用者转型） */
    void *value;               /**< 泛型值（name→value 形态，任意指针） */
    void (*destroy)(void *);   /**< 值析构函数（remove/destroy 时调用，可为 NULL） */
} lvRegistryEntry;

/** @brief 通用注册表（线程安全） */
typedef struct lvRegistry {
    lvRegistryEntry *entries;  /**< 条目数组 */
    int count;                 /**< 当前条目数 */
    int capacity;              /**< 容量 */
    lvMutex mutex;             /**< 互斥锁 */
} lvRegistry;

/**
 * @brief 初始化注册表
 * @param reg      注册表指针
 * @param capacity 初始容量（0 则使用默认 16）
 */
void lv_registry_init(lvRegistry *reg, int capacity);

/**
 * @brief 销毁注册表
 *
 * 依次调用每个条目的 destroy 回调（若有）并释放内部拷贝的 name。
 */
void lv_registry_destroy(lvRegistry *reg);

/**
 * @brief 注册一个条目（name→factory 形态，向后兼容 API）
 * @param reg    注册表指针
 * @param name   条目名称（注册表内部拷贝）
 * @param create 创建函数指针
 * @return true 成功，false name 重复或内存不足
 */
bool lv_registry_register(lvRegistry *reg, const char *name, void *(*create)(void));

/**
 * @brief 按名称查找并创建后端
 * @param reg  注册表指针
 * @param name 后端名称
 * @return 创建的对象指针，未找到返回 NULL
 */
void *lv_registry_create(const lvRegistry *reg, const char *name);

/**
 * @brief 按名称查找条目索引
 * @return 索引，未找到返回 -1
 */
int lv_registry_find(const lvRegistry *reg, const char *name);

/* ============================================================
 * 泛型条目 API（name→value 形态）
 *
 * 与上面的 name→factory API 共用同一注册表结构与互斥锁：
 * 同一注册表内 name 唯一，put/register 重复注册均返回 false。
 * name 由注册表内部拷贝管理，value 的所有权由 destroy 回调决定。
 * ============================================================ */

/**
 * @brief 存入一个泛型条目（不带析构回调）
 * @param reg    注册表指针
 * @param name   条目名称（内部拷贝）
 * @param value  泛型值指针
 * @return true 成功，false name 重复或内存不足
 */
bool lv_registry_put(lvRegistry *reg, const char *name, void *value);

/**
 * @brief 存入一个泛型条目（带析构回调）
 * @param reg     注册表指针
 * @param name    条目名称（内部拷贝）
 * @param value   泛型值指针
 * @param destroy 值析构回调（remove/destroy 时调用，可为 NULL）
 * @return true 成功，false name 重复或内存不足
 */
bool lv_registry_put_ex(lvRegistry *reg, const char *name, void *value, void (*destroy)(void *));

/**
 * @brief 按名称获取泛型值
 * @param reg  注册表指针
 * @param name 条目名称
 * @return 值指针，未找到返回 NULL
 */
void *lv_registry_get(const lvRegistry *reg, const char *name);

/**
 * @brief 按名称删除条目
 *
 * 删除时先调用该条目的 destroy 回调（若有），再释放内部 name，
 * 并将后续条目前移紧凑（保持注册顺序）。
 *
 * @param reg  注册表指针
 * @param name 条目名称
 * @return true 删除成功，false 未找到
 */
bool lv_registry_remove(lvRegistry *reg, const char *name);

/**
 * @brief 获取当前条目数
 * @param reg 注册表指针
 * @return 条目数
 */
int lv_registry_count(const lvRegistry *reg);

/**
 * @brief 按索引获取条目（用于遍历）
 * @param reg   注册表指针
 * @param index 条目索引（0..count-1）
 * @param name  输出条目名称（可为 NULL）
 * @param value 输出条目值（可为 NULL）
 * @return true 成功，false 索引越界
 */
bool lv_registry_get_at(const lvRegistry *reg, int index, const char **name, void **value);

/* ============================================================
 * 模块生命周期管理（Module Lifecycle）
 *
 * 独立于上面的通用注册表，提供模块初始化/清理的集中管理。
 * 模块在 lv_module_register() 注册后，通过 lv_module_init_all()
 * 按优先级顺序批量初始化，通过 lv_module_cleanup_all()
 * 按反向优先级顺序批量清理。
 * ============================================================ */

/** @brief 模块初始化函数类型 */
typedef bool (*lvModuleInitFunc)(void);

/** @brief 模块清理函数类型 */
typedef void (*lvModuleCleanupFunc)(void);

/** @brief 模块优先级：决定初始化顺序 */
typedef enum {
    lv_MODULE_PRIO_CORE = 0,       /**< 核心基础设施（日志、内存等） */
    lv_MODULE_PRIO_RESOURCE,       /**< 资源层（配置、错误码等） */
    lv_MODULE_PRIO_GEOMETRY,       /**< 几何层 */
    lv_MODULE_PRIO_REASONING,      /**< 推理层 */
    lv_MODULE_PRIO_OUTPUT,         /**< 输出层 */
    lv_MODULE_PRIO_APPLICATION,    /**< 应用层 */
    lv_MODULE_PRIO_LATE = 999      /**< 最后初始化 */
} lvModulePriority;

/**
 * @brief 注册一个模块
 * @param name     模块名称
 * @param init_fn  初始化函数（可为 NULL）
 * @param cleanup_fn 清理函数（可为 NULL）
 * @param priority 初始化优先级
 * @return true 注册成功
 */
bool lv_module_register(const char *name, lvModuleInitFunc init_fn,
                         lvModuleCleanupFunc cleanup_fn, lvModulePriority priority);

/**
 * @brief 按优先级顺序初始化所有已注册的模块
 * @return true 全部成功，false 任一模块失败
 */
bool lv_module_init_all(void);

/**
 * @brief 按反向优先级顺序清理所有已注册的模块
 */
void lv_module_cleanup_all(void);

/**
 * @brief 获取已注册的模块数量
 * @return 已注册模块数
 */
int lv_module_count(void);

/**
 * @brief 在文件作用域自动注册模块
 *
 * 在模块的 .c 文件中使用此宏，可在初始化时自动注册。
 * 需要先调用 lv_module_init_all() 来触发初始化。
 *
 * 用法：
 *   LV_REGISTER_MODULE("my_module", my_init, my_cleanup, lv_MODULE_PRIO_RESOURCE);
 *
 * @note 仅在 GCC/Clang（__GNUC__ 或 __clang__）下生效，
 *       MSVC 下需手动调用 lv_module_register()。
 */
#if defined(__GNUC__) || defined(__clang__)
#define LV_REGISTER_MODULE(name, init_fn, cleanup_fn, priority) \
    __attribute__((constructor)) static void lv_auto_register_##name(void) { \
        lv_module_register(name, init_fn, cleanup_fn, priority); \
    }
#else
/* MSVC 及未知编译器：需要手动调用 lv_module_register() */
#define LV_REGISTER_MODULE(name, init_fn, cleanup_fn, priority) \
    static void lv_manual_register_##name(void) { \
        lv_module_register(name, init_fn, cleanup_fn, priority); \
    }
#endif

#ifdef __cplusplus
}
#endif

#endif /* lv_REGISTRY_H */
