/**
 * @file lv00.h
 * @brief Lv-00 几何元语言系统主头文件
 *
 * @details Lv-00 是一门以几何为唯一载体的双模数学元语言。
 *          几何体本身是计算的执行者、数据的承载者、证明的见证者。
 *
 * @note   本文件是 Lv-00 的公共 API 入口，仅暴露对外接口。
 *         内部实现细节（如共享数据结构、JSON 序列化工具、内部宏等）
 *         集中定义在 lv00_internal.h 中，该头文件不应被外部使用者直接引用。
 *
 * @author Lv-00 Project
 * @version 3.0.1
 */

#ifndef LV00_MAIN_H
#define LV00_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ============================================================
 * 第零层：平台兼容性宏（必须在所有头文件之前定义）
 * ============================================================
 * 【设计原则】所有平台差异集中处理，模块代码只使用统一宏接口：
 *   - 线程局部存储：LV00_THREAD_LOCAL（跨编译器统一）
 *   - 内存分配追踪：lv00_malloc / lv00_free（在lv00_utils.h中定义）
 *   - 字符串安全操作：lv00_strlcpy / lv00_strdup
 *   - 线程安全时间：LV00_LOCALTIME
 * ============================================================ */

/* ====================================================================
 * 线程局部存储宏（跨平台统一，避免各模块各自定义）
 * ==================================================================== */
#if defined(_MSC_VER)
    #define LV00_THREAD_LOCAL __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
    #define LV00_THREAD_LOCAL __thread
#else
    #if __STDC_VERSION__ >= 201112L
        #define LV00_THREAD_LOCAL _Thread_local
    #else
        #define LV00_THREAD_LOCAL /* 不支持：回退到全局变量 */
        #warning "Thread-local storage not supported on this compiler"
    #endif
#endif

/* strdup 兼容性（非标准C函数） */
#if defined(_MSC_VER)
    #define lv00_strdup _strdup
#else
    #define lv00_strdup strdup
#endif

/* localtime 线程安全包装 */
#if defined(_WIN32)
    #define LV00_LOCALTIME(time_ptr, tm_buf) localtime_s(tm_buf, time_ptr)
#else
    #define LV00_LOCALTIME(time_ptr, tm_buf) localtime_r(time_ptr, tm_buf)
#endif

/* 函数废弃标记（跨编译器统一） */
#ifndef LV00_DEPRECATED
    #if defined(__GNUC__) || defined(__clang__)
        #define LV00_DEPRECATED(msg) __attribute__((deprecated(msg)))
    #elif defined(_MSC_VER)
        #define LV00_DEPRECATED(msg) __declspec(deprecated(msg))
    #else
        #define LV00_DEPRECATED(msg) /* 不支持废弃标记 */
    #endif
#endif

/* ── 路径分隔符（跨平台） ── */
#ifdef _WIN32
    #define LV00_PATH_SEPARATOR_CHAR '\\'
    #define LV00_PATH_SEPARATOR_STR  "\\"
#else
    #define LV00_PATH_SEPARATOR_CHAR '/'
    #define LV00_PATH_SEPARATOR_STR  "/"
#endif
/* 向后兼容：旧宏名 LV00_PATH_SEPARATOR 保留 */
#define LV00_PATH_SEPARATOR LV00_PATH_SEPARATOR_CHAR

/* ---- 版本信息（统一版本号 v3.0.1，所有模块引用此宏） ---- */
#define LV00_VERSION_MAJOR 3
#define LV00_VERSION_MINOR 0
#define LV00_VERSION_PATCH 1
#define LV00_VERSION_STRING_EXPAND(maj, min, pat) #maj "." #min "." #pat
#define LV00_VERSION_STRING_MACRO(maj, min, pat) LV00_VERSION_STRING_EXPAND(maj, min, pat)
#ifndef LV00_VERSION_STRING
#define LV00_VERSION_STRING LV00_VERSION_STRING_MACRO(LV00_VERSION_MAJOR, LV00_VERSION_MINOR, LV00_VERSION_PATCH)
#endif

/* 基础模块（必须在其他模块之前） */
#include "error_codes.h"      /* 统一错误码系统 */

/* 核心模块 */
#include "symbolic_coord.h"   /* 符号坐标系统 */
#include "constraint_graph.h" /* 约束图核心 */
#include "normalization.h"    /* 图规范化遍引擎 */
#include "graph_hash.h"        /* 图结构哈希 */
#include "solver.h"           /* 符号代数求解器 */
#include "rewrite.h"          /* 图重写引擎 */
#include "unify.h"            /* 合一检查 */

/* 公理系统 */
#include "axiom_pkg.h"        /* 公理系统包 */
#include "module.h"           /* 模块系统 */

/* 高级功能 */
#include "func_block.h"           /* 函数块系统 */
#include "func_block_registry.h"  /* 预设函数块注册系统 */
#include "func_block_preset.h"    /* 预设函数块库 */

/* 模块化预设函数块系统 */
#include "preset_blocks.h"        /* 模块化预设函数块主系统 */
#include "preset_basic_geometry.h"  /* 基础几何构造模块 */
#include "preset_transformations.h" /* 几何变换模块 */
#include "preset_measurements.h"    /* 度量计算模块 */
#include "preset_polygons.h"        /* 多边形构造模块 */
#include "preset_algebraic.h"       /* 代数运算模块 */

#include "type_system.h"          /* 类型系统 */
#include "proof.h"                /* 命题与证明系统 */
#include "recursion.h"            /* 递归与条件 */

/* 引擎 */
#include "engine.h"           /* 主引擎 */

/* 调试 */
#include "debug.h"            /* 调试工具 */

/* 工具函数库 */
#include "lv00_utils.h"       /* 通用工具函数 */

/* 流式输出 */
#include "stream.h"           /* 流式事件系统 */
#include "stream_context_util.h"  /* 流式上下文工具宏 */

/* ============================================================
 * 系统生命周期管理
 * ============================================================ */

/**
 * @brief 获取版本字符串
 * @return 版本字符串，格式为 "major.minor.patch"
 */
static inline const char *lv00_get_version(void) {
    return LV00_VERSION_STRING;
}

/**
 * @brief 初始化 Lv-00 系统
 * @return 成功返回 true，失败返回 false
 */
bool lv00_init(void);

/**
 * @brief 清理 Lv-00 系统，释放所有资源
 */
void lv00_cleanup(void);

/**
 * @brief 获取 Lv-00 系统状态信息
 * @param[out] info 输出缓冲区
 * @param[in]  size 缓冲区大小（字节数）
 * @return 实际写入的字符数（不含终止符），参数无效时返回 0
 */
int lv00_get_system_info(char *info, size_t size);

/**
 * @brief 检查系统健康状况
 * @return 健康状态评分（0~100分制），未初始化时为 0
 */
int lv00_health_check(void);

/**
 * @brief 检查系统是否已初始化
 * @return true 已初始化，false 未初始化
 */
bool lv00_is_initialized(void);

/* ============================================================
 * 便捷 API —— 快速构造与操作的封装函数
 * ============================================================ */

/**
 * @brief 创建并初始化引擎实例（便捷函数）
 * @return 引擎指针，失败返回 NULL（自动设置错误码）
 */
LV00Engine *lv00_engine_create(void);

/**
 * @brief 销毁引擎实例（便捷函数）
 * @param engine 引擎指针（可为 NULL）
 */
void lv00_engine_destroy(LV00Engine *engine);

/**
 * @brief 快速创建一个有理数坐标的点（便捷函数）
 * @param engine 引擎实例
 * @param x_num  X 坐标分子
 * @param x_den  X 坐标分母
 * @param y_num  Y 坐标分子
 * @param y_den  Y 坐标分母
 * @return 新节点的 ID（>= 0），失败返回 -1
 */
int lv00_add_point(LV00Engine *engine, int64_t x_num, uint64_t x_den,
                   int64_t y_num, uint64_t y_den);

/**
 * @brief 快速创建线段（便捷函数）
 * @param engine      引擎实例
 * @param point1_id   端点1节点ID
 * @param point2_id   端点2节点ID
 * @return 新节点的 ID（>= 0），失败返回 -1
 */
int lv00_add_line_segment(LV00Engine *engine, int point1_id, int point2_id);

/**
 * @brief 快速添加关联约束（便捷函数）
 * @param engine    引擎实例
 * @param point_id  点节点ID
 * @param line_id   线段/区域节点ID
 * @return true 成功，false 失败
 */
bool lv00_add_constraint_incidence(LV00Engine *engine, int point_id, int line_id);

/**
 * @brief 执行图归一化（便捷函数）
 * @param engine       引擎实例
 * @param scope_aware  是否考虑命名空间范围
 * @return 归一化结果（调用者负责释放），失败返回 NULL
 */
NormalizationResult *lv00_normalize(LV00Engine *engine, bool scope_aware);

/**
 * @brief 执行求解流水线（便捷函数）
 * @param engine 引擎实例
 * @return 求解结果状态码
 */
EngineSolveResult lv00_solve(LV00Engine *engine);

/* ============================================================
 * 配置管理便捷 API
 * ============================================================ */

/**
 * @brief 获取整数配置值（便捷函数）
 * @param key         配置键名
 * @param default_val 默认值
 * @return 配置值
 */
int lv00_config_get_int(const char *key, int default_val);

/**
 * @brief 获取布尔配置值（便捷函数）
 * @param key         配置键名
 * @param default_val 默认值
 * @return 配置值
 */
bool lv00_config_get_bool(const char *key, bool default_val);

/**
 * @brief 获取浮点配置值（便捷函数）
 * @param key         配置键名
 * @param default_val 默认值
 * @return 配置值
 */
double lv00_config_get_double(const char *key, double default_val);

/**
 * @brief 获取字符串配置值（便捷函数）
 * @param key         配置键名
 * @param default_val 默认值
 * @return 配置值（内部存储，勿释放）
 */
const char *lv00_config_get_string(const char *key, const char *default_val);

/**
 * @brief 设置整数配置值
 * @param key   配置键名
 * @param value 配置值
 * @return true 成功，false 失败
 */
bool lv00_config_set_int(const char *key, int value);

/**
 * @brief 设置布尔配置值
 * @param key   配置键名
 * @param value 配置值
 * @return true 成功，false 失败
 */
bool lv00_config_set_bool(const char *key, bool value);

/**
 * @brief 设置浮点配置值
 * @param key   配置键名
 * @param value 配置值
 * @return true 成功，false 失败
 */
bool lv00_config_set_double(const char *key, double value);

/**
 * @brief 设置字符串配置值
 * @param key   配置键名
 * @param value 配置值
 * @return true 成功，false 失败
 */
bool lv00_config_set_string(const char *key, const char *value);

/* ============================================================
 * 内存管理便捷 API
 * ============================================================ */

/**
 * @brief 获取当前内存使用统计
 * @param stats 输出统计信息结构体
 * @return true 成功，false 失败
 */
bool lv00_get_memory_stats_ex(MemoryStats *stats);

/**
 * @brief 设置内存使用上限
 * @param limit_bytes 内存上限（字节），0表示无限制
 */
void lv00_set_memory_limit_ex(size_t limit_bytes);

/**
 * @brief 获取当前内存使用上限
 * @return 内存上限（字节），0表示无限制
 */
size_t lv00_get_memory_limit_ex(void);

/* ============================================================
 * 调试和日志便捷 API
 * ============================================================ */

/**
 * @brief 设置日志级别
 * @param level 日志级别（0=禁用，1=错误，2=警告，3=信息，4=调试）
 */
void lv00_set_log_level(int level);

/**
 * @brief 获取当前日志级别
 * @return 当前日志级别
 */
int lv00_get_log_level(void);

/**
 * @brief 启用/禁用断言
 * @param enabled true 启用，false 禁用
 */
void lv00_set_assertions_enabled(bool enabled);

/**
 * @brief 检查断言是否启用
 * @return true 启用，false 禁用
 */
bool lv00_are_assertions_enabled(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_MAIN_H */
