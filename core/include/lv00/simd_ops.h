/**
 * @file simd_ops.h
 * @brief SIMD向量运算库 —— 跨平台SIMD抽象层
 *
 * @details 提供统一的SIMD接口，支持：
 *   - SSE/AVX (x86/x64)
 *   - NEON (ARM)
 *   - 标量回退（无SIMD支持时）
 *
 * 核心功能：
 *   1. 向量算术运算（加、减、乘、除）
 *   2. 向量比较与选择
 *   3. 向量归约（求和、最大、最小）
 *   4. 向量加载/存储
 *   5. 矩阵运算加速
 *
 * 设计目标：
 *   - 单指令多数据并行计算
 *   - 几何坐标批量处理
 *   - 约束求解加速
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#ifndef LV00_SIMD_OPS_H
#define LV00_SIMD_OPS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ============== SIMD 能力检测 ============== */

/**
 * @brief SIMD能力标志
 */
typedef enum {
    LV00_SIMD_NONE     = 0,       /**< 无SIMD支持 */
    LV00_SIMD_SSE2     = 1 << 0,  /**< SSE2 */
    LV00_SIMD_SSE41    = 1 << 1,  /**< SSE4.1 */
    LV00_SIMD_AVX      = 1 << 2,  /**< AVX (256-bit) */
    LV00_SIMD_AVX2     = 1 << 3,  /**< AVX2 */
    LV00_SIMD_AVX512F  = 1 << 4,  /**< AVX-512F */
    LV00_SIMD_NEON     = 1 << 5,  /**< ARM NEON */
} Lv00SimdCapability;

/**
 * @brief 检测运行时SIMD能力
 *
 * @return SIMD能力标志位掩码
 */
uint32_t lv00_simd_detect_capabilities(void);

/**
 * @brief 检查是否支持指定SIMD能力
 *
 * @param cap 能力标志
 * @return 是否支持
 */
bool lv00_simd_has_capability(Lv00SimdCapability cap);

/**
 * @brief 获取SIMD能力描述字符串
 *
 * @param cap 能力标志
 * @return 描述字符串
 */
const char *lv00_simd_capability_name(Lv00SimdCapability cap);

/* ============== 向量类型定义 ============== */

/**
 * @brief 4元素双精度向量
 */
typedef struct {
    double v[4];
} Lv00Vec4d;

/**
 * @brief 4元素单精度向量
 */
typedef struct {
    float v[4];
} Lv00Vec4f;

/**
 * @brief 8元素单精度向量
 */
typedef struct {
    float v[8];
} Lv00Vec8f;

/**
 * @brief 4元素整数向量
 */
typedef struct {
    int32_t v[4];
} Lv00Vec4i;

/* ============== 向量创建 ============== */

/**
 * @brief 创建全零向量 (4x double)
 */
Lv00Vec4d lv00_vec4d_zero(void);

/**
 * @brief 创建全1向量 (4x double)
 */
Lv00Vec4d lv00_vec4d_one(void);

/**
 * @brief 从标量创建广播向量 (4x double)
 */
Lv00Vec4d lv00_vec4d_set1(double val);

/**
 * @brief 从4个值创建向量 (4x double)
 */
Lv00Vec4d lv00_vec4d_set(double x, double y, double z, double w);

/**
 * @brief 从内存加载向量 (4x double)
 */
Lv00Vec4d lv00_vec4d_load(const double *ptr);

/**
 * @brief 从内存加载向量（非对齐）(4x double)
 */
Lv00Vec4d lv00_vec4d_loadu(const double *ptr);

/**
 * @brief 存储向量到内存 (4x double)
 */
void lv00_vec4d_store(double *ptr, Lv00Vec4d vec);

/**
 * @brief 存储向量到内存（非对齐）(4x double)
 */
void lv00_vec4d_storeu(double *ptr, Lv00Vec4d vec);

/* ============== 向量算术运算 (4x double) ============== */

/**
 * @brief 向量加法
 */
Lv00Vec4d lv00_vec4d_add(Lv00Vec4d a, Lv00Vec4d b);

/**
 * @brief 向量减法
 */
Lv00Vec4d lv00_vec4d_sub(Lv00Vec4d a, Lv00Vec4d b);

/**
 * @brief 向量乘法（逐元素）
 */
Lv00Vec4d lv00_vec4d_mul(Lv00Vec4d a, Lv00Vec4d b);

/**
 * @brief 向量除法（逐元素）
 */
Lv00Vec4d lv00_vec4d_div(Lv00Vec4d a, Lv00Vec4d b);

/**
 * @brief 向量取负
 */
Lv00Vec4d lv00_vec4d_neg(Lv00Vec4d a);

/**
 * @brief 向量平方根
 */
Lv00Vec4d lv00_vec4d_sqrt(Lv00Vec4d a);

/**
 * @brief 向量绝对值
 */
Lv00Vec4d lv00_vec4d_abs(Lv00Vec4d a);

/**
 * @brief 向量最大值（逐元素）
 */
Lv00Vec4d lv00_vec4d_max(Lv00Vec4d a, Lv00Vec4d b);

/**
 * @brief 向量最小值（逐元素）
 */
Lv00Vec4d lv00_vec4d_min(Lv00Vec4d a, Lv00Vec4d b);

/**
 * @brief 向量线性组合: a * x + y
 */
Lv00Vec4d lv00_vec4d_fmadd(Lv00Vec4d a, Lv00Vec4d x, Lv00Vec4d y);

/* ============== 向量比较 (4x double) ============== */

/**
 * @brief 向量相等比较
 * @return 掩码向量（全1表示相等，全0表示不等）
 */
Lv00Vec4d lv00_vec4d_cmpeq(Lv00Vec4d a, Lv00Vec4d b);

/**
 * @brief 向量小于比较
 */
Lv00Vec4d lv00_vec4d_cmplt(Lv00Vec4d a, Lv00Vec4d b);

/**
 * @brief 向量小于等于比较
 */
Lv00Vec4d lv00_vec4d_cmple(Lv00Vec4d a, Lv00Vec4d b);

/**
 * @brief 向量大于比较
 */
Lv00Vec4d lv00_vec4d_cmpgt(Lv00Vec4d a, Lv00Vec4d b);

/**
 * @brief 向量大于等于比较
 */
Lv00Vec4d lv00_vec4d_cmpge(Lv00Vec4d a, Lv00Vec4d b);

/**
 * @brief 向量条件选择
 * @param mask 条件掩码
 * @param a 为真时选择
 * @param b 为假时选择
 */
Lv00Vec4d lv00_vec4d_select(Lv00Vec4d mask, Lv00Vec4d a, Lv00Vec4d b);

/* ============== 向量归约 (4x double) ============== */

/**
 * @brief 向量水平求和
 */
double lv00_vec4d_hsum(Lv00Vec4d a);

/**
 * @brief 向量水平最大值
 */
double lv00_vec4d_hmax(Lv00Vec4d a);

/**
 * @brief 向量水平最小值
 */
double lv00_vec4d_hmin(Lv00Vec4d a);

/**
 * @brief 向量点积
 */
double lv00_vec4d_dot(Lv00Vec4d a, Lv00Vec4d b);

/* ============== 单精度向量运算 (4x float) ============== */

Lv00Vec4f lv00_vec4f_zero(void);
Lv00Vec4f lv00_vec4f_set1(float val);
Lv00Vec4f lv00_vec4f_load(const float *ptr);
void lv00_vec4f_store(float *ptr, Lv00Vec4f vec);

Lv00Vec4f lv00_vec4f_add(Lv00Vec4f a, Lv00Vec4f b);
Lv00Vec4f lv00_vec4f_sub(Lv00Vec4f a, Lv00Vec4f b);
Lv00Vec4f lv00_vec4f_mul(Lv00Vec4f a, Lv00Vec4f b);
Lv00Vec4f lv00_vec4f_div(Lv00Vec4f a, Lv00Vec4f b);
Lv00Vec4f lv00_vec4f_sqrt(Lv00Vec4f a);

float lv00_vec4f_hsum(Lv00Vec4f a);
float lv00_vec4f_dot(Lv00Vec4f a, Lv00Vec4f b);

/* ============== 8元素单精度向量运算 ============== */

Lv00Vec8f lv00_vec8f_zero(void);
Lv00Vec8f lv00_vec8f_set1(float val);
Lv00Vec8f lv00_vec8f_load(const float *ptr);
void lv00_vec8f_store(float *ptr, Lv00Vec8f vec);

Lv00Vec8f lv00_vec8f_add(Lv00Vec8f a, Lv00Vec8f b);
Lv00Vec8f lv00_vec8f_sub(Lv00Vec8f a, Lv00Vec8f b);
Lv00Vec8f lv00_vec8f_mul(Lv00Vec8f a, Lv00Vec8f b);
Lv00Vec8f lv00_vec8f_div(Lv00Vec8f a, Lv00Vec8f b);

float lv00_vec8f_hsum(Lv00Vec8f a);

/* ============== 批量运算 ============== */

/**
 * @brief 批量向量加法
 *
 * @param a 输入数组a
 * @param b 输入数组b
 * @param out 输出数组
 * @param count 元素数量
 */
void lv00_simd_add_array_d(const double *a, const double *b, double *out, size_t count);

/**
 * @brief 批量向量乘法
 */
void lv00_simd_mul_array_d(const double *a, const double *b, double *out, size_t count);

/**
 * @brief 批量向量乘加: out = a * b + c
 */
void lv00_simd_fmadd_array_d(const double *a, const double *b, const double *c,
                              double *out, size_t count);

/**
 * @brief 批量向量求和
 */
double lv00_simd_sum_array_d(const double *arr, size_t count);

/**
 * @brief 批量向量点积
 */
double lv00_simd_dot_array_d(const double *a, const double *b, size_t count);

/**
 * @brief 批量向量缩放
 */
void lv00_simd_scale_array_d(const double *in, double scale, double *out, size_t count);

/**
 * @brief 批量向量最大值
 */
double lv00_simd_max_array_d(const double *arr, size_t count);

/**
 * @brief 批量向量最小值
 */
double lv00_simd_min_array_d(const double *arr, size_t count);

/* ============== 几何运算加速 ============== */

/**
 * @brief 批量计算点到点距离
 *
 * @param x1 第一个点的x坐标数组
 * @param y1 第一个点的y坐标数组
 * @param x2 第二个点的x坐标数组
 * @param y2 第二个点的y坐标数组
 * @param out 输出距离数组
 * @param count 点对数量
 */
void lv00_simd_distance_array(const double *x1, const double *y1,
                               const double *x2, const double *y2,
                               double *out, size_t count);

/**
 * @brief 批量计算点到线段距离
 *
 * @param px 点x坐标数组
 * @param py 点y坐标数组
 * @param x1 线段起点x
 * @param y1 线段起点y
 * @param x2 线段终点x
 * @param y2 线段终点y
 * @param out 输出距离数组
 * @param count 点数量
 */
void lv00_simd_point_line_distance_array(const double *px, const double *py,
                                          double x1, double y1,
                                          double x2, double y2,
                                          double *out, size_t count);

/**
 * @brief 批量计算向量叉积（2D）
 *
 * @param ax 向量a的x分量数组
 * @param ay 向量a的y分量数组
 * @param bx 向量b的x分量数组
 * @param by 向量b的y分量数组
 * @param out 输出叉积数组
 * @param count 向量数量
 */
void lv00_simd_cross2d_array(const double *ax, const double *ay,
                              const double *bx, const double *by,
                              double *out, size_t count);

/**
 * @brief 批量判断点是否在圆内
 *
 * @param px 点x坐标数组
 * @param py 点y坐标数组
 * @param cx 圆心x
 * @param cy 圆心y
 * @param r 圆半径
 * @param out 输出布尔数组（1=在圆内，0=不在）
 * @param count 点数量
 */
void lv00_simd_point_in_circle_array(const double *px, const double *py,
                                      double cx, double cy, double r,
                                      int *out, size_t count);

/* ============== 矩阵运算加速 ============== */

/**
 * @brief 4x4矩阵向量乘法
 *
 * @param mat 4x4矩阵（行优先）
 * @param vec 输入向量
 * @return 输出向量
 */
Lv00Vec4d lv00_simd_mat4x4_vec4_mul(const double mat[16], Lv00Vec4d vec);

/**
 * @brief 批量4x4矩阵向量乘法
 *
 * @param mat 4x4矩阵
 * @param vecs 输入向量数组
 * @param out 输出向量数组
 * @param count 向量数量
 */
void lv00_simd_mat4x4_vec4_array_mul(const double mat[16],
                                      const double *vecs,
                                      double *out,
                                      size_t count);

/**
 * @brief 3x3矩阵向量乘法（用于2D变换）
 *
 * @param mat 3x3矩阵（行优先）
 * @param x 输入x坐标
 * @param y 输入y坐标
 * @param out_x 输出x坐标
 * @param out_y 输出y坐标
 */
void lv00_simd_mat3x3_vec2_mul(const double mat[9],
                                double x, double y,
                                double *out_x, double *out_y);

/* ============== 性能统计 ============== */

/**
 * @brief SIMD性能统计
 */
typedef struct {
    uint64_t vec4_ops;       /**< 4元素向量操作次数 */
    uint64_t vec8_ops;       /**< 8元素向量操作次数 */
    uint64_t array_ops;      /**< 数组操作次数 */
    uint64_t elements_processed; /**< 处理的元素总数 */
    uint64_t simd_time_us;   /**< SIMD操作总耗时 */
    uint64_t scalar_fallbacks; /**< 标量回退次数 */
} Lv00SimdStats;

/**
 * @brief 获取SIMD统计信息
 */
void lv00_simd_get_stats(Lv00SimdStats *stats);

/**
 * @brief 重置SIMD统计信息
 */
void lv00_simd_reset_stats(void);

/**
 * @brief 打印SIMD诊断信息
 */
void lv00_simd_print_diag(void *stream);

#ifdef __cplusplus
}
#endif

#endif /* LV00_SIMD_OPS_H */
