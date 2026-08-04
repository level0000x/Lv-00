/**
 * @file lv_backend_plugin.h
 * @brief 统一后端插件系统抽象层 —— 统一 ATP、SMT、数值计算、
 *        调度器和通用注册表五套后端注册机制
 *
 * @details 设计目标：
 *          - 统一接口：所有后端共享 lvBackendPlugin 描述符头
 *          - 类型安全：通过 lvBackendPluginType 枚举区分后端类型
 *          - 能力声明：通过 lvBackendCapability 位标志组合描述后端能力
 *          - 线程安全：注册表操作使用 lvMutex 保护
 *          - 生命周期管理：插件式 init/cleanup，支持批量初始化/清理
 *          - 向后兼容：现有 ATPBackendRegistry/SMTBackendRegistry 等
 *            通过包装器适配到统一注册表
 *
 * @author Lv-00 Project
 * @version 1.0.0
 * @date 2026-08-04
 */
#ifndef lv_BACKEND_PLUGIN_H
#define lv_BACKEND_PLUGIN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lv_platform.h"  /* for lvMutex, lv_MUTEX_* */

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 后端类型枚举
 *
 * 注意：此枚举与 numerical_backend.h 中的 lvBackendType 不同。
 * lvBackendType 描述数值计算后端（SERIAL/OpenMP/CUDA 等），
 * 本枚举描述后端插件系统类型（ATP/SMT/Groebner/Solver 等）。
 * ======================================================================== */

/** @brief 后端插件类型 */
typedef enum {
    lv_PLUGIN_TYPE_NUMERIC = 0,   /**< 数值计算后端 */
    lv_PLUGIN_TYPE_ATP,           /**< ATP 自动定理证明 */
    lv_PLUGIN_TYPE_SMT,           /**< SMT 求解器 */
    lv_PLUGIN_TYPE_GROEBNER,      /**< Gröbner 基计算 */
    lv_PLUGIN_TYPE_SOLVER,        /**< 通用求解器 */
    lv_PLUGIN_TYPE_CUSTOM = 255   /**< 自定义后端 */
} lvBackendPluginType;

/* ========================================================================
 * 后端能力标志
 * ======================================================================== */

/** @brief 后端能力位标志 */
typedef enum {
    lv_PLUGIN_CAP_NONE         = 0,        /**< 无特殊能力 */
    lv_PLUGIN_CAP_PARALLEL     = 1 << 0,   /**< 支持并行计算 */
    lv_PLUGIN_CAP_INCREMENTAL  = 1 << 1,   /**< 支持增量求解 */
    lv_PLUGIN_CAP_PROOF_PROD   = 1 << 2,   /**< 支持生成证明 */
    lv_PLUGIN_CAP_UNSAT_CORE   = 1 << 3,   /**< 支持生成 unsat core */
    lv_PLUGIN_CAP_FLOAT        = 1 << 4,   /**< 支持浮点运算 */
    lv_PLUGIN_CAP_EXACT        = 1 << 5,   /**< 支持精确算术 */
} lvBackendPluginCapability;

/* ========================================================================
 * 后端插件描述符
 *
 * 所有后端共享的通用头部。具体后端可在此结构后附加额外字段，
 * 或通过 ops 指针指向类型特定的操作表（vtable）。
 * ======================================================================== */

/** @brief 后端插件描述符 */
typedef struct lvBackendPlugin {
    const char *name;                /**< 后端名称（如 "E Prover", "Z3"） */
    const char *version;             /**< 版本字符串 */
    lvBackendPluginType type;        /**< 后端类型 */
    uint32_t capabilities;           /**< 能力标志位（lvBackendPluginCapability 组合） */
    int priority;                    /**< 优先级（低=优先） */
    bool available;                  /**< 系统上是否可用 */

    /* 生命周期 */
    bool (*init)(void);              /**< 初始化 */
    void (*cleanup)(void);           /**< 清理 */

    /* 类型特定的操作表（由调用者转型为具体类型） */
    void *ops;                       /**< 指向具体 ops vtable 的指针 */
} lvBackendPlugin;

/* ========================================================================
 * 后端插件注册表（线程安全）
 *
 * 使用动态数组存储插件指针，支持按名称/类型查找。
 * ======================================================================== */

/** @brief 后端插件注册表 */
typedef struct lvBackendPluginRegistry {
    lvBackendPlugin **plugins;       /**< 插件指针数组 */
    int count;                       /**< 当前插件数 */
    int capacity;                    /**< 数组容量 */
    lvMutex mutex;                   /**< 互斥锁 */
} lvBackendPluginRegistry;

/* ========================================================================
 * 注册表生命周期 API
 * ======================================================================== */

/**
 * @brief 初始化后端插件注册表
 * @param reg 注册表指针（非 NULL）
 */
void lv_backend_plugin_registry_init(lvBackendPluginRegistry *reg);

/**
 * @brief 销毁后端插件注册表，释放所有资源
 * @param reg 注册表指针（非 NULL）
 */
void lv_backend_plugin_registry_cleanup(lvBackendPluginRegistry *reg);

/* ========================================================================
 * 插件注册与注销 API
 * ======================================================================== */

/**
 * @brief 注册一个后端插件
 * @param reg    注册表指针
 * @param plugin 插件描述符指针（调用者维护生命周期，注册表不接管所有权）
 * @return true 注册成功，false 名称重复或内存不足
 */
bool lv_backend_plugin_register(lvBackendPluginRegistry *reg, lvBackendPlugin *plugin);

/**
 * @brief 按名称注销一个后端插件
 * @param reg  注册表指针
 * @param name 插件名称
 * @return true 注销成功，false 未找到
 */
bool lv_backend_plugin_unregister(lvBackendPluginRegistry *reg, const char *name);

/* ========================================================================
 * 插件查找与遍历 API
 * ======================================================================== */

/**
 * @brief 按名称查找后端插件
 * @param reg  注册表指针
 * @param name 插件名称
 * @return 插件指针，未找到返回 NULL
 */
lvBackendPlugin *lv_backend_plugin_find(lvBackendPluginRegistry *reg, const char *name);

/**
 * @brief 按类型查找后端插件
 * @param[in]  reg       注册表指针
 * @param[in]  type      后端类型
 * @param[out] out       输出缓冲区（存放找到的插件指针）
 * @param[in]  max_count 输出缓冲区最大容量
 * @return 实际找到的插件数量
 */
int lv_backend_plugin_find_by_type(lvBackendPluginRegistry *reg, lvBackendPluginType type,
                                   lvBackendPlugin **out, int max_count);

/**
 * @brief 获取已注册的插件数量
 * @param reg 注册表指针
 * @return 插件数量
 */
int lv_backend_plugin_count(lvBackendPluginRegistry *reg);

/* ========================================================================
 * 批量生命周期管理 API
 * ======================================================================== */

/**
 * @brief 初始化所有已注册的插件（按优先级顺序）
 * @param reg 注册表指针
 */
void lv_backend_plugin_init_all(lvBackendPluginRegistry *reg);

/**
 * @brief 清理所有已注册的插件（按反向优先级顺序）
 * @param reg 注册表指针
 */
void lv_backend_plugin_cleanup_all(lvBackendPluginRegistry *reg);

/* ========================================================================
 * 全局单例注册表
 * ======================================================================== */

/**
 * @brief 获取全局后端插件注册表单例
 * @return 全局注册表指针（惰性初始化）
 */
lvBackendPluginRegistry *lv_backend_plugin_registry_global(void);

#ifdef __cplusplus
}
#endif

#endif /* lv_BACKEND_PLUGIN_H */