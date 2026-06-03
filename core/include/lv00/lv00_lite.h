/**
 * @file lv00_lite.h
 * @brief Lv-00 轻量版 API 头文件
 *
 * @details 轻量版 Lv-00 内核提供核心几何推理功能，适用于资源受限环境：
 *          - 嵌入式系统
 *          - 移动设备 (Android/iOS)
 *          - WebAssembly
 *          - 微控制器
 *
 * 特性:
 *   - 模块化设计：按需启用功能
 *   - 小内存占用：默认 < 1MB 堆内存
 *   - 无动态依赖：纯 C 标准库实现
 *   - 可选线程安全：可禁用以减小开销
 *
 * 使用方法:
 *   #include <lv00/lv00_lite.h>
 *
 *   // 初始化轻量版内核
 *   Lv00LiteConfig config = lv00_lite_default_config();
 *   config.max_memory = 512 * 1024;  // 512KB
 *   lv00_lite_init(&config);
 *
 *   // 使用 API
 *   Lv00Point* p = lv00_point_create(1.0, 2.0);
 *   ...
 *
 *   // 清理
 *   lv00_lite_cleanup();
 *
 * @version 3.5.0
 * @date   2026-05-29
 */

#ifndef LV00_LITE_H
#define LV00_LITE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cross_platform.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* ====================================================================
 * 版本信息
 * ==================================================================== */

#define LV00_LITE_VERSION_MAJOR 3
#define LV00_LITE_VERSION_MINOR 5
#define LV00_LITE_VERSION_PATCH 0
#define LV00_LITE_VERSION_STRING "3.5.0-lite"

/* ====================================================================
 * 配置结构
 * ==================================================================== */

/**
 * @brief 轻量版配置结构
 */
typedef struct {
    /* 内存限制 */
    size_t max_memory;              /**< 最大内存限制 (字节, 0 = 无限制) */
    size_t initial_pool_size;       /**< 初始内存池大小 */
    
    /* 容量限制 */
    uint32_t max_symbols;           /**< 最大符号数 */
    uint32_t max_constraints;       /**< 最大约束数 */
    uint32_t max_cache_entries;     /**< 最大缓存条目数 */
    
    /* 特性开关 */
    bool enable_memory_pool;        /**< 启用内存池 */
    bool enable_memory_monitor;     /**< 启用内存监控 */
    bool enable_thread_safety;      /**< 启用线程安全 */
    bool enable_unicode;            /**< 启用 Unicode 支持 */
    
    /* 精度设置 */
    bool use_float32;               /**< 使用 32 位浮点数 (默认 64 位) */
    
    /* 日志回调 */
    void (*log_callback)(const char* msg);  /**< 日志回调函数 */
} Lv00LiteConfig;

/**
 * @brief 获取默认配置
 * @return 默认配置结构
 */
Lv00LiteConfig lv00_lite_default_config(void);

/**
 * @brief 初始化轻量版内核
 * @param config 配置指针 (NULL 使用默认配置)
 * @return 成功返回 true
 */
bool lv00_lite_init(const Lv00LiteConfig* config);

/**
 * @brief 清理轻量版内核
 */
void lv00_lite_cleanup(void);

/**
 * @brief 检查是否已初始化
 * @return 已初始化返回 true
 */
bool lv00_lite_is_initialized(void);

/**
 * @brief 获取版本字符串
 * @return 版本字符串
 */
const char* lv00_lite_version(void);

/* ====================================================================
 * 内存管理
 * ==================================================================== */

/**
 * @brief 内存统计信息
 */
typedef struct {
    size_t total_allocated;     /**< 总分配内存 */
    size_t total_used;          /**< 已使用内存 */
    size_t peak_usage;          /**< 峰值使用 */
    size_t pool_size;           /**< 内存池大小 */
    uint32_t allocation_count;  /**< 分配次数 */
} Lv00LiteMemoryStats;

/**
 * @brief 获取内存统计
 * @param stats 输出统计信息
 */
void lv00_lite_get_memory_stats(Lv00LiteMemoryStats* stats);

/**
 * @brief 执行内存压缩
 * @return 回收的字节数
 */
size_t lv00_lite_compact_memory(void);

/**
 * @brief 强制垃圾回收
 * @return 回收的字节数
 */
size_t lv00_lite_gc(void);

/* ====================================================================
 * 基本几何类型
 * ==================================================================== */

#ifdef LV00_USE_FLOAT32
typedef float lv00_real_t;
#define LV00_REAL_FMT "%.6f"
#else
typedef double lv00_real_t;
#define LV00_REAL_FMT "%.12f"
#endif

/**
 * @brief 2D 点
 */
typedef struct {
    lv00_real_t x;
    lv00_real_t y;
} Lv00Point;

/**
 * @brief 2D 向量
 */
typedef struct {
    lv00_real_t x;
    lv00_real_t y;
} Lv00Vector;

/**
 * @brief 圆
 */
typedef struct {
    Lv00Point center;
    lv00_real_t radius;
} Lv00Circle;

/**
 * @brief 线段
 */
typedef struct {
    Lv00Point start;
    Lv00Point end;
} Lv00LineSegment;

/**
 * @brief 矩形 (AABB)
 */
typedef struct {
    lv00_real_t x, y;       /**< 左上角 */
    lv00_real_t width;      /**< 宽度 */
    lv00_real_t height;     /**< 高度 */
} Lv00Rectangle;

/* ====================================================================
 * 点操作
 * ==================================================================== */

/**
 * @brief 创建点
 * @param x X 坐标
 * @param y Y 坐标
 * @return 点结构
 */
Lv00Point lv00_point_create(lv00_real_t x, lv00_real_t y);

/**
 * @brief 计算两点距离
 * @param a 点 A
 * @param b 点 B
 * @return 距离
 */
lv00_real_t lv00_point_distance(Lv00Point a, Lv00Point b);

/**
 * @brief 计算两点距离平方 (避免开方)
 * @param a 点 A
 * @param b 点 B
 * @return 距离平方
 */
lv00_real_t lv00_point_distance_sq(Lv00Point a, Lv00Point b);

/**
 * @brief 点相加
 * @param a 点 A
 * @param b 点 B
 * @return 结果点
 */
Lv00Point lv00_point_add(Lv00Point a, Lv00Point b);

/**
 * @brief 点相减
 * @param a 点 A
 * @param b 点 B
 * @return 结果向量
 */
Lv00Vector lv00_point_sub(Lv00Point a, Lv00Point b);

/**
 * @brief 判断两点是否相等 (带容差)
 * @param a 点 A
 * @param b 点 B
 * @param epsilon 容差
 * @return 相等返回 true
 */
bool lv00_point_equals(Lv00Point a, Lv00Point b, lv00_real_t epsilon);

/* ====================================================================
 * 向量操作
 * ==================================================================== */

/**
 * @brief 创建向量
 * @param x X 分量
 * @param y Y 分量
 * @return 向量结构
 */
Lv00Vector lv00_vector_create(lv00_real_t x, lv00_real_t y);

/**
 * @brief 向量长度
 * @param v 向量
 * @return 长度
 */
lv00_real_t lv00_vector_length(Lv00Vector v);

/**
 * @brief 向量长度平方
 * @param v 向量
 * @return 长度平方
 */
lv00_real_t lv00_vector_length_sq(Lv00Vector v);

/**
 * @brief 向量归一化
 * @param v 向量
 * @return 归一化向量
 */
Lv00Vector lv00_vector_normalize(Lv00Vector v);

/**
 * @brief 向量点积
 * @param a 向量 A
 * @param b 向量 B
 * @return 点积
 */
lv00_real_t lv00_vector_dot(Lv00Vector a, Lv00Vector b);

/**
 * @brief 向量叉积 (2D 标量)
 * @param a 向量 A
 * @param b 向量 B
 * @return 叉积 (z 分量)
 */
lv00_real_t lv00_vector_cross(Lv00Vector a, Lv00Vector b);

/**
 * @brief 向量数乘
 * @param v 向量
 * @param scalar 标量
 * @return 结果向量
 */
Lv00Vector lv00_vector_scale(Lv00Vector v, lv00_real_t scalar);

/**
 * @brief 向量相加
 * @param a 向量 A
 * @param b 向量 B
 * @return 结果向量
 */
Lv00Vector lv00_vector_add(Lv00Vector a, Lv00Vector b);

/* ====================================================================
 * 圆操作
 * ==================================================================== */

/**
 * @brief 创建圆
 * @param center 圆心
 * @param radius 半径
 * @return 圆结构
 */
Lv00Circle lv00_circle_create(Lv00Point center, lv00_real_t radius);

/**
 * @brief 计算圆周长
 * @param c 圆
 * @return 周长
 */
lv00_real_t lv00_circle_circumference(Lv00Circle c);

/**
 * @brief 计算圆面积
 * @param c 圆
 * @return 面积
 */
lv00_real_t lv00_circle_area(Lv00Circle c);

/**
 * @brief 判断点是否在圆内
 * @param c 圆
 * @param p 点
 * @return 在内返回 true
 */
bool lv00_circle_contains_point(Lv00Circle c, Lv00Point p);

/**
 * @brief 计算两圆交点
 * @param c1 圆 1
 * @param c2 圆 2
 * @param out_p1 输出交点 1
 * @param out_p2 输出交点 2
 * @return 交点数量 (0, 1, 或 2)
 */
int lv00_circle_intersect(Lv00Circle c1, Lv00Circle c2, 
                           Lv00Point* out_p1, Lv00Point* out_p2);

/* ====================================================================
 * 线段操作
 * ==================================================================== */

/**
 * @brief 创建线段
 * @param start 起点
 * @param end 终点
 * @return 线段结构
 */
Lv00LineSegment lv00_line_segment_create(Lv00Point start, Lv00Point end);

/**
 * @brief 计算线段长度
 * @param ls 线段
 * @return 长度
 */
lv00_real_t lv00_line_segment_length(Lv00LineSegment ls);

/**
 * @brief 计算线段中点
 * @param ls 线段
 * @return 中点
 */
Lv00Point lv00_line_segment_midpoint(Lv00LineSegment ls);

/**
 * @brief 判断点是否在线段上 (带容差)
 * @param ls 线段
 * @param p 点
 * @param epsilon 容差
 * @return 在线上返回 true
 */
bool lv00_line_segment_contains_point(Lv00LineSegment ls, Lv00Point p, 
                                       lv00_real_t epsilon);

/**
 * @brief 计算两线段交点
 * @param ls1 线段 1
 * @param ls2 线段 2
 * @param out_p 输出交点
 * @return 相交返回 true
 */
bool lv00_line_segment_intersect(Lv00LineSegment ls1, Lv00LineSegment ls2,
                                  Lv00Point* out_p);

/* ====================================================================
 * 约束系统 (简化版)
 * ==================================================================== */

/**
 * @brief 约束类型
 */
typedef enum {
    LV00_CONSTRAINT_DISTANCE,       /**< 距离约束 */
    LV00_CONSTRAINT_ANGLE,          /**< 角度约束 */
    LV00_CONSTRAINT_PARALLEL,       /**< 平行约束 */
    LV00_CONSTRAINT_PERPENDICULAR,  /**< 垂直约束 */
    LV00_CONSTRAINT_COINCIDENT,     /**< 重合约束 */
    LV00_CONSTRAINT_FIXED           /**< 固定约束 */
} Lv00ConstraintType;

/**
 * @brief 约束句柄
 */
typedef struct Lv00Constraint* Lv00ConstraintHandle;

/**
 * @brief 创建距离约束
 * @param p1 点 1
 * @param p2 点 2
 * @param distance 目标距离
 * @return 约束句柄
 */
Lv00ConstraintHandle lv00_constraint_distance(Lv00Point* p1, Lv00Point* p2,
                                               lv00_real_t distance);

/**
 * @brief 创建角度约束
 * @param center 顶点
 * @param p1 点 1
 * @param p2 点 2
 * @param angle 目标角度 (弧度)
 * @return 约束句柄
 */
Lv00ConstraintHandle lv00_constraint_angle(Lv00Point* center,
                                            Lv00Point* p1, Lv00Point* p2,
                                            lv00_real_t angle);

/**
 * @brief 创建平行约束
 * @param ls1 线段 1
 * @param ls2 线段 2
 * @return 约束句柄
 */
Lv00ConstraintHandle lv00_constraint_parallel(Lv00LineSegment* ls1,
                                               Lv00LineSegment* ls2);

/**
 * @brief 创建垂直约束
 * @param ls1 线段 1
 * @param ls2 线段 2
 * @return 约束句柄
 */
Lv00ConstraintHandle lv00_constraint_perpendicular(Lv00LineSegment* ls1,
                                                    Lv00LineSegment* ls2);

/**
 * @brief 销毁约束
 * @param handle 约束句柄
 */
void lv00_constraint_destroy(Lv00ConstraintHandle handle);

/* ====================================================================
 * 求解器 (简化版)
 * ==================================================================== */

/**
 * @brief 求解器配置
 */
typedef struct {
    uint32_t max_iterations;        /**< 最大迭代次数 */
    lv00_real_t tolerance;          /**< 收敛容差 */
    bool use_damping;               /**< 使用阻尼 */
} Lv00SolverConfig;

/**
 * @brief 求解器状态
 */
typedef enum {
    LV00_SOLVER_IDLE,           /**< 空闲 */
    LV00_SOLVER_RUNNING,        /**< 运行中 */
    LV00_SOLVER_CONVERGED,      /**< 已收敛 */
    LV00_SOLVER_DIVERGED,       /**< 发散 */
    LV00_SOLVER_MAX_ITER        /**< 达到最大迭代 */
} Lv00SolverStatus;

/**
 * @brief 获取默认求解器配置
 * @return 默认配置
 */
Lv00SolverConfig lv00_solver_default_config(void);

/**
 * @brief 求解约束系统
 * @param constraints 约束数组
 * @param count 约束数量
 * @param config 求解器配置
 * @return 求解器状态
 */
Lv00SolverStatus lv00_solver_solve(Lv00ConstraintHandle* constraints,
                                    uint32_t count,
                                    const Lv00SolverConfig* config);

/**
 * @brief 单步求解 (用于迭代显示)
 * @param constraints 约束数组
 * @param count 约束数量
 * @param config 求解器配置
 * @return 求解器状态
 */
Lv00SolverStatus lv00_solver_step(Lv00ConstraintHandle* constraints,
                                   uint32_t count,
                                   const Lv00SolverConfig* config);

/* ====================================================================
 * 实用函数
 * ==================================================================== */

/**
 * @brief 将角度转换为弧度
 * @param degrees 角度
 * @return 弧度
 */
lv00_real_t lv00_degrees_to_radians(lv00_real_t degrees);

/**
 * @brief 将弧度转换为角度
 * @param radians 弧度
 * @return 角度
 */
lv00_real_t lv00_radians_to_degrees(lv00_real_t radians);

/**
 * @brief 限制值在范围内
 * @param value 值
 * @param min 最小值
 * @param max 最大值
 * @return 限制后的值
 */
lv00_real_t lv00_clamp(lv00_real_t value, lv00_real_t min, lv00_real_t max);

/**
 * @brief 线性插值
 * @param a 起始值
 * @param b 结束值
 * @param t 插值因子 (0-1)
 * @return 插值结果
 */
lv00_real_t lv00_lerp(lv00_real_t a, lv00_real_t b, lv00_real_t t);

/**
 * @brief 比较两个浮点数是否相等 (带容差)
 * @param a 值 A
 * @param b 值 B
 * @param epsilon 容差
 * @return 相等返回 true
 */
bool lv00_real_equals(lv00_real_t a, lv00_real_t b, lv00_real_t epsilon);

/* ====================================================================
 * 错误处理
 * ==================================================================== */

/**
 * @brief 错误码
 */
typedef enum {
    LV00_OK = 0,                    /**< 成功 */
    LV00_ERROR_NOMEM = -1,          /**< 内存不足 */
    LV00_ERROR_INVALID = -2,        /**< 无效参数 */
    LV00_ERROR_OVERFLOW = -3,       /**< 溢出 */
    LV00_ERROR_NOT_INITIALIZED = -4,/**< 未初始化 */
    LV00_ERROR_NOT_IMPLEMENTED = -5,/**< 未实现 */
    LV00_ERROR_SOLVER_FAIL = -6,    /**< 求解器失败 */
    LV00_ERROR_CONSTRAINT = -7      /**< 约束错误 */
} Lv00ErrorCode;

/**
 * @brief 获取最后错误码
 * @return 错误码
 */
Lv00ErrorCode lv00_get_last_error(void);

/**
 * @brief 获取错误描述
 * @param code 错误码
 * @return 错误描述字符串
 */
const char* lv00_error_string(Lv00ErrorCode code);

/**
 * @brief 清除错误状态
 */
void lv00_clear_error(void);

/* ====================================================================
 * 平台特性查询
 * ==================================================================== */

/**
 * @brief 轻量版能力信息
 */
typedef struct {
    bool has_memory_pool;       /**< 支持内存池 */
    bool has_memory_monitor;    /**< 支持内存监控 */
    bool has_thread_safety;     /**< 支持线程安全 */
    bool has_unicode;           /**< 支持 Unicode */
    bool has_solver;            /**< 支持求解器 */
    bool uses_float32;          /**< 使用 32 位浮点 */
    uint32_t max_symbols;       /**< 最大符号数 */
    uint32_t max_constraints;   /**< 最大约束数 */
} Lv00LiteCapabilities;

/**
 * @brief 获取轻量版能力信息
 * @return 能力信息结构
 */
Lv00LiteCapabilities lv00_lite_get_capabilities(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_LITE_H */
