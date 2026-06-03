/**
 * @file global_state.h
 * @brief Lv-00 全局状态管理器
 * @details 统一管理所有全局参数和状态，提供版本迭代后的参数清理机制，
 *          避免状态残留。所有分散的全局变量应迁移到此统一管理结构中。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 所有全局状态集中管理，禁止分散定义全局变量
 * - 支持上下文隔离，每个上下文拥有独立的状态实例
 * - 版本迭代时自动清理过期参数，避免状态残留
 * - 线程安全：状态访问通过上下文隔离实现
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被所有上层模块依赖。
 * 下层模块（Layer 1）不应访问全局状态。
 */

#ifndef LV00_GLOBAL_STATE_H
#define LV00_GLOBAL_STATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 版本与兼容性定义
 * ============================================================ */

/** @brief 全局状态结构版本号（用于版本兼容性检查） */
#define LV00_GLOBAL_STATE_VERSION 1

/** @brief 全局状态魔法数（用于内存完整性校验） */
#define LV00_GLOBAL_STATE_MAGIC 0x4C5630304753ULL /* "LV00GS" */

/** @brief 最大参数数量 */
#define LV00_GLOBAL_STATE_MAX_PARAMS 256

/** @brief 参数字符串最大长度 */
#define LV00_GLOBAL_STATE_MAX_PARAM_NAME_LEN 64

/** @brief 参数字符串值最大长度 */
#define LV00_GLOBAL_STATE_MAX_PARAM_VALUE_LEN 256

/* ============================================================
 * 参数类型枚举
 * ============================================================ */

typedef enum {
    GS_PARAM_TYPE_INVALID = 0,   /**< 无效类型 */
    GS_PARAM_TYPE_BOOL,          /**< 布尔值 */
    GS_PARAM_TYPE_INT,           /**< 整数 */
    GS_PARAM_TYPE_UINT,          /**< 无符号整数 */
    GS_PARAM_TYPE_FLOAT,         /**< 浮点数 */
    GS_PARAM_TYPE_DOUBLE,        /**< 双精度浮点数 */
    GS_PARAM_TYPE_STRING,        /**< 字符串 */
    GS_PARAM_TYPE_PTR,           /**< 指针 */
    GS_PARAM_TYPE_BLOB,          /**< 二进制数据 */
    GS_PARAM_TYPE_COUNT          /**< 类型数量 */
} Lv00GlobalParamType;

/* ============================================================
 * 参数值联合体
 * ============================================================ */

typedef union {
    bool b;
    int i;
    unsigned int u;
    float f;
    double d;
    char s[LV00_GLOBAL_STATE_MAX_PARAM_VALUE_LEN];
    void *p;
    struct {
        uint8_t *data;
        size_t size;
    } blob;
} Lv00GlobalParamValue;

/* ============================================================
 * 参数描述结构
 * ============================================================ */

typedef struct {
    char name[LV00_GLOBAL_STATE_MAX_PARAM_NAME_LEN]; /**< 参数名 */
    Lv00GlobalParamType type;                         /**< 参数类型 */
    Lv00GlobalParamValue value;                       /**< 参数值 */
    uint32_t version_introduced;                      /**< 引入版本 */
    uint32_t version_deprecated;                      /**< 废弃版本（0表示未废弃） */
    bool is_deprecated;                               /**< 是否已废弃 */
    bool is_dirty;                                    /**< 脏标记 */
} Lv00GlobalParam;

/* ============================================================
 * 全局状态结构
 * ============================================================ */

typedef struct Lv00GlobalState {
    uint64_t magic;              /**< 魔法数（完整性校验） */
    uint32_t version;            /**< 结构版本号 */
    uint32_t param_count;        /**< 当前参数数量 */
    uint32_t current_system_version; /**< 当前系统版本 */
    
    /* 参数存储 */
    Lv00GlobalParam params[LV00_GLOBAL_STATE_MAX_PARAMS];
    
    /* 统计信息 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t modification_count; /**< 修改计数 */
    
    /* 上下文关联 */
    void *context;               /**< 关联的上下文指针 */
    
    /* 清理回调 */
    void (*cleanup_callback)(struct Lv00GlobalState *);
} Lv00GlobalState;

/* ============================================================
 * 生命周期管理 API
 * ============================================================ */

/**
 * @brief 创建全局状态实例
 *
 * @param system_version 当前系统版本号
 * @return 新创建的全局状态实例，失败返回 NULL
 */
LV00_PUBLIC_API Lv00GlobalState *lv00_global_state_create(uint32_t system_version);

/**
 * @brief 销毁全局状态实例
 *
 * @param state 全局状态实例（可为 NULL）
 */
LV00_PUBLIC_API void lv00_global_state_destroy(Lv00GlobalState *state);

/**
 * @brief 验证全局状态实例有效性
 *
 * @param state 全局状态实例
 * @return true 有效，false 无效
 */
LV00_PUBLIC_API bool lv00_global_state_is_valid(const Lv00GlobalState *state);

/**
 * @brief 清理过期参数
 *
 * 根据当前系统版本，清理所有已废弃的参数。
 *
 * @param state 全局状态实例
 * @return LV00_OK 成功，其他错误码表示失败
 */
LV00_PUBLIC_API Lv00ErrorCode lv00_global_state_cleanup_deprecated(Lv00GlobalState *state);

/**
 * @brief 重置全局状态为默认值
 *
 * @param state 全局状态实例
 * @return LV00_OK 成功，其他错误码表示失败
 */
LV00_PUBLIC_API Lv00ErrorCode lv00_global_state_reset(Lv00GlobalState *state);

/* ============================================================
 * 参数操作 API
 * ============================================================ */

/**
 * @brief 设置布尔参数
 *
 * @param state 全局状态实例
 * @param name 参数名
 * @param value 参数值
 * @param version_introduced 引入版本
 * @return LV00_OK 成功，其他错误码表示失败
 */
LV00_PUBLIC_API Lv00ErrorCode lv00_global_state_set_bool(
    Lv00GlobalState *state, 
    const char *name, 
    bool value,
    uint32_t version_introduced
);

/**
 * @brief 获取布尔参数
 *
 * @param state 全局状态实例
 * @param name 参数名
 * @param default_value 默认值
 * @return 参数值，不存在则返回默认值
 */
LV00_PUBLIC_API bool lv00_global_state_get_bool(
    const Lv00GlobalState *state, 
    const char *name, 
    bool default_value
);

/**
 * @brief 设置整数参数
 *
 * @param state 全局状态实例
 * @param name 参数名
 * @param value 参数值
 * @param version_introduced 引入版本
 * @return LV00_OK 成功，其他错误码表示失败
 */
LV00_PUBLIC_API Lv00ErrorCode lv00_global_state_set_int(
    Lv00GlobalState *state, 
    const char *name, 
    int value,
    uint32_t version_introduced
);

/**
 * @brief 获取整数参数
 *
 * @param state 全局状态实例
 * @param name 参数名
 * @param default_value 默认值
 * @return 参数值，不存在则返回默认值
 */
LV00_PUBLIC_API int lv00_global_state_get_int(
    const Lv00GlobalState *state, 
    const char *name, 
    int default_value
);

/**
 * @brief 设置无符号整数参数
 *
 * @param state 全局状态实例
 * @param name 参数名
 * @param value 参数值
 * @param version_introduced 引入版本
 * @return LV00_OK 成功，其他错误码表示失败
 */
LV00_PUBLIC_API Lv00ErrorCode lv00_global_state_set_uint(
    Lv00GlobalState *state, 
    const char *name, 
    unsigned int value,
    uint32_t version_introduced
);

/**
 * @brief 获取无符号整数参数
 *
 * @param state 全局状态实例
 * @param name 参数名
 * @param default_value 默认值
 * @return 参数值，不存在则返回默认值
 */
LV00_PUBLIC_API unsigned int lv00_global_state_get_uint(
    const Lv00GlobalState *state, 
    const char *name, 
    unsigned int default_value
);

/**
 * @brief 设置浮点参数
 *
 * @param state 全局状态实例
 * @param name 参数名
 * @param value 参数值
 * @param version_introduced 引入版本
 * @return LV00_OK 成功，其他错误码表示失败
 */
LV00_PUBLIC_API Lv00ErrorCode lv00_global_state_set_double(
    Lv00GlobalState *state, 
    const char *name, 
    double value,
    uint32_t version_introduced
);

/**
 * @brief 获取浮点参数
 *
 * @param state 全局状态实例
 * @param name 参数名
 * @param default_value 默认值
 * @return 参数值，不存在则返回默认值
 */
LV00_PUBLIC_API double lv00_global_state_get_double(
    const Lv00GlobalState *state, 
    const char *name, 
    double default_value
);

/**
 * @brief 设置字符串参数
 *
 * @param state 全局状态实例
 * @param name 参数名
 * @param value 参数值
 * @param version_introduced 引入版本
 * @return LV00_OK 成功，其他错误码表示失败
 */
LV00_PUBLIC_API Lv00ErrorCode lv00_global_state_set_string(
    Lv00GlobalState *state, 
    const char *name, 
    const char *value,
    uint32_t version_introduced
);

/**
 * @brief 获取字符串参数
 *
 * @param state 全局状态实例
 * @param name 参数名
 * @param default_value 默认值
 * @param out_buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return LV00_OK 成功，其他错误码表示失败
 */
LV00_PUBLIC_API Lv00ErrorCode lv00_global_state_get_string(
    const Lv00GlobalState *state, 
    const char *name, 
    const char *default_value,
    char *out_buffer,
    size_t buffer_size
);

/**
 * @brief 设置指针参数
 *
 * @param state 全局状态实例
 * @param name 参数名
 * @param value 参数值
 * @param version_introduced 引入版本
 * @return LV00_OK 成功，其他错误码表示失败
 */
LV00_PUBLIC_API Lv00ErrorCode lv00_global_state_set_ptr(
    Lv00GlobalState *state, 
    const char *name, 
    void *value,
    uint32_t version_introduced
);

/**
 * @brief 获取指针参数
 *
 * @param state 全局状态实例
 * @param name 参数名
 * @param default_value 默认值
 * @return 参数值，不存在则返回默认值
 */
LV00_PUBLIC_API void *lv00_global_state_get_ptr(
    const Lv00GlobalState *state, 
    const char *name, 
    void *default_value
);

/**
 * @brief 删除参数
 *
 * @param state 全局状态实例
 * @param name 参数名
 * @return LV00_OK 成功，其他错误码表示失败
 */
LV00_PUBLIC_API Lv00ErrorCode lv00_global_state_remove_param(
    Lv00GlobalState *state, 
    const char *name
);

/**
 * @brief 检查参数是否存在
 *
 * @param state 全局状态实例
 * @param name 参数名
 * @return true 存在，false 不存在
 */
LV00_PUBLIC_API bool lv00_global_state_has_param(
    const Lv00GlobalState *state, 
    const char *name
);

/**
 * @brief 标记参数为废弃
 *
 * @param state 全局状态实例
 * @param name 参数名
 * @param version_deprecated 废弃版本
 * @return LV00_OK 成功，其他错误码表示失败
 */
LV00_PUBLIC_API Lv00ErrorCode lv00_global_state_deprecate_param(
    Lv00GlobalState *state, 
    const char *name,
    uint32_t version_deprecated
);

/* ============================================================
 * 批量操作 API
 * ============================================================ */

/**
 * @brief 从配置字符串批量加载参数
 *
 * 配置格式：key=value，每行一个参数
 *
 * @param state 全局状态实例
 * @param config 配置字符串
 * @return LV00_OK 成功，其他错误码表示失败
 */
LV00_PUBLIC_API Lv00ErrorCode lv00_global_state_load_from_string(
    Lv00GlobalState *state, 
    const char *config
);

/**
 * @brief 导出所有参数为配置字符串
 *
 * @param state 全局状态实例
 * @param out_buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return LV00_OK 成功，其他错误码表示失败
 */
LV00_PUBLIC_API Lv00ErrorCode lv00_global_state_export_to_string(
    const Lv00GlobalState *state, 
    char *out_buffer,
    size_t buffer_size
);

/**
 * @brief 复制全局状态
 *
 * @param src 源状态
 * @return 新创建的副本，失败返回 NULL
 */
LV00_PUBLIC_API Lv00GlobalState *lv00_global_state_clone(const Lv00GlobalState *src);

/* ============================================================
 * 统计与诊断 API
 * ============================================================ */

/**
 * @brief 获取参数数量
 *
 * @param state 全局状态实例
 * @return 参数数量
 */
LV00_PUBLIC_API uint32_t lv00_global_state_get_param_count(const Lv00GlobalState *state);

/**
 * @brief 获取废弃参数数量
 *
 * @param state 全局状态实例
 * @return 废弃参数数量
 */
LV00_PUBLIC_API uint32_t lv00_global_state_get_deprecated_count(const Lv00GlobalState *state);

/**
 * @brief 获取访问计数
 *
 * @param state 全局状态实例
 * @return 访问计数
 */
LV00_PUBLIC_API uint64_t lv00_global_state_get_access_count(const Lv00GlobalState *state);

/**
 * @brief 获取修改计数
 *
 * @param state 全局状态实例
 * @return 修改计数
 */
LV00_PUBLIC_API uint64_t lv00_global_state_get_modification_count(const Lv00GlobalState *state);

/* ============================================================
 * 预定义参数名常量
 * ============================================================ */

/* --- 几何计算参数 --- */
#define LV00_GS_GEOMETRY_PRECISION_BITS "geometry.precision_bits"
#define LV00_GS_GEOMETRY_EPSILON "geometry.epsilon"
#define LV00_GS_GEOMETRY_MAX_ITERATIONS "geometry.max_iterations"
#define LV00_GS_GEOMETRY_ENABLE_CACHE "geometry.enable_cache"

/* --- 求解器参数 --- */
#define LV00_GS_SOLVER_MAX_ITERATIONS "solver.max_iterations"
#define LV00_GS_SOLVER_CONVERGENCE_THRESHOLD "solver.convergence_threshold"
#define LV00_GS_SOLVER_ENABLE_PARALLEL "solver.enable_parallel"

/* --- 证明引擎参数 --- */
#define LV00_GS_PROOF_MAX_DEPTH "proof.max_depth"
#define LV00_GS_PROOF_ENABLE_TRACE "proof.enable_trace"
#define LV00_GS_PROOF_AUTO_EXPORT "proof.auto_export"

/* --- 内存管理参数 --- */
#define LV00_GS_MEMORY_POOL_SIZE "memory.pool_size"
#define LV00_GS_MEMORY_ENABLE_TRACKING "memory.enable_tracking"
#define LV00_GS_MEMORY_GC_THRESHOLD "memory.gc_threshold"

/* --- 日志参数 --- */
#define LV00_GS_LOG_LEVEL "log.level"
#define LV00_GS_LOG_ENABLE_FILE "log.enable_file"
#define LV00_GS_LOG_FILE_PATH "log.file_path"

/* --- 性能参数 --- */
#define LV00_GS_PERF_ENABLE_PROFILING "perf.enable_profiling"
#define LV00_GS_PERF_SAMPLE_RATE "perf.sample_rate"

/* --- 缓存参数 --- */
#define LV00_GS_CACHE_MAX_SIZE "cache.max_size"
#define LV00_GS_CACHE_TTL_SECONDS "cache.ttl_seconds"
#define LV00_GS_CACHE_ENABLE_LRU "cache.enable_lru"

#ifdef __cplusplus
}
#endif

#endif /* LV00_GLOBAL_STATE_H */
