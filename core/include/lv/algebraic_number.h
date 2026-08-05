/* 内部模块 API，非公共导出 */

/**
 * @file algebraic_number.h
 * @brief 代数数域封装 —— 有理数、二次代数数、区间运算、多项式系统
 *
 * 本模块提供三层代数数域的表示与运算，为 Lv-00 几何元语言系统提供
 * 精确的数值计算能力。所有实现基于 int64_t，不依赖 GMP 等外部库。
 *
 * 三层数域体系：
 *   - 第一层：有理数域 Q（int64_t 分子/分母，自动约分）
 *   - 第二层：二次代数数域 Q(sqrt(d))（a + b*sqrt(d) 形式）
 *   - 第三层：区间运算（隔离区间 + 多项式系统）
 *
 * 设计原则：
 *   - 无外部依赖：仅使用 int64_t 实现有理数运算
 *   - 精确计算：所有运算保持数学精确性，不引入浮点误差
 *   - 溢出检测：关键运算检测 int64_t 溢出并返回错误
 *   - 内存安全：使用 lv_malloc/lv_free 管理内存
 *
 * @note   本模块不引用 Proposition 类型，避免与 proof.h 的循环依赖。
 *
 * @version 1.1.0
 * @copyright Copyright (c) 2024-2026 Lv-00 Project
 */

#ifndef lv_ALGEBRAIC_NUMBER_H
#define lv_ALGEBRAIC_NUMBER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* lv_PUBLIC_API 由 lv.h 统一定义，此处仅做守卫检查。
 * 若未包含 lv.h，则定义为空（支持独立编译测试）。 */
#ifndef lv_PUBLIC_API
#define lv_PUBLIC_API
#endif

/* ============================================================
 * 第一层：有理数域 Q
 * ============================================================
 * 使用 int64_t 分子/分母表示有理数，分母始终为正数。
 * 自动约分确保最简形式。
 * ============================================================ */

/**
 * @brief 有理数结构体（第一层数域）
 *
 * 表示一个有理数 p/q，其中：
 *   - p 为分子（int64_t，可为负数）
 *   - q 为分母（int64_t，始终 > 0）
 *   - gcd(p, q) = 1（自动约分后的最简形式）
 *
 * 不变量：
 *   - q > 0
 *   - gcd(|p|, q) = 1
 *   - 特殊值 0 表示为 p=0, q=1
 */
typedef struct {
    int64_t num; /**< 分子 */
    int64_t den; /**< 分母（始终 > 0） */
} AlgRational;

/**
 * @brief 有理数运算错误码
 */
typedef enum {
    lv_alg_rational_OK = 0,       /**< 成功 */
    lv_alg_rational_ERR_ZERO_DEN, /**< 分母为零 */
    lv_alg_rational_ERR_OVERFLOW, /**< int64_t 溢出 */
    lv_alg_rational_ERR_NULL,     /**< 空指针 */
    lv_alg_rational_ERR_INVALID   /**< 无效参数 */
} AlgRationalError;

/**
 * @brief 创建有理数（自动约分）
 *
 * 将 p/q 约分为最简形式。若 q 为 0，返回 0/1 并设置错误码。
 *
 * @param[in]  p 分子
 * @param[in]  q 分母
 * @param[out] err 错误码（可为 NULL）
 * @return 约分后的有理数
 */
AlgRational lv_alg_rational_create(int64_t p, int64_t q, AlgRationalError *err);

/**
 * @brief 创建零值有理数
 * @return 0/1
 */
AlgRational lv_alg_rational_zero(void);

/**
 * @brief 创建单位有理数
 * @return 1/1
 */
AlgRational lv_alg_rational_one(void);

/**
 * @brief 从整数创建有理数
 * @param[in] n 整数值
 * @return n/1
 */
AlgRational lv_alg_rational_from_int(int64_t n);

/**
 * @brief 有理数加法
 *
 * 计算 a + b，结果自动约分。
 *
 * @param[in]  a 加数
 * @param[in]  b 加数
 * @param[out] err 错误码（可为 NULL）
 * @return a + b 的约分结果
 */
AlgRational lv_alg_rational_add(const AlgRational *a, const AlgRational *b, AlgRationalError *err);

/**
 * @brief 有理数减法
 *
 * 计算 a - b，结果自动约分。
 *
 * @param[in]  a 被减数
 * @param[in]  b 减数
 * @param[out] err 错误码（可为 NULL）
 * @return a - b 的约分结果
 */
AlgRational lv_alg_rational_sub(const AlgRational *a, const AlgRational *b, AlgRationalError *err);

/**
 * @brief 有理数乘法
 *
 * 计算 a * b，结果自动约分。
 *
 * @param[in]  a 乘数
 * @param[in]  b 乘数
 * @param[out] err 错误码（可为 NULL）
 * @return a * b 的约分结果
 */
AlgRational lv_alg_rational_mul(const AlgRational *a, const AlgRational *b, AlgRationalError *err);

/**
 * @brief 有理数除法
 *
 * 计算 a / b，结果自动约分。若 b 为零，返回 0/1 并设置错误码。
 *
 * @param[in]  a 被除数
 * @param[in]  b 除数
 * @param[out] err 错误码（可为 NULL）
 * @return a / b 的约分结果
 */
AlgRational lv_alg_rational_div(const AlgRational *a, const AlgRational *b, AlgRationalError *err);

/**
 * @brief 有理数取负
 *
 * 计算 -a。
 *
 * @param[in] a 有理数
 * @return -a
 */
AlgRational lv_alg_rational_neg(const AlgRational *a);

/**
 * @brief 有理数绝对值
 *
 * 计算 |a|。
 *
 * @param[in] a 有理数
 * @return |a|
 */
AlgRational lv_alg_rational_abs(const AlgRational *a);

/**
 * @brief 有理数倒数
 *
 * 计算 1/a。若 a 为零，返回 0/1 并设置错误码。
 *
 * @param[in]  a 有理数
 * @param[out] err 错误码（可为 NULL）
 * @return 1/a
 */
AlgRational lv_alg_rational_inv(const AlgRational *a, AlgRationalError *err);

/**
 * @brief 有理数乘方
 *
 * 计算 a^n（n 为非负整数）。
 *
 * @param[in]  a 底数
 * @param[in]  n 指数（非负整数）
 * @param[out] err 错误码（可为 NULL）
 * @return a^n
 */
AlgRational lv_alg_rational_pow(const AlgRational *a, int n, AlgRationalError *err);

/**
 * @brief 有理数比较
 *
 * 比较两个有理数的大小。
 *
 * @param[in] a 有理数
 * @param[in] b 有理数
 * @return <0 表示 a < b，0 表示 a == b，>0 表示 a > b
 */
int lv_alg_rational_cmp(const AlgRational *a, const AlgRational *b);

/**
 * @brief 有理数相等判断
 *
 * @param[in] a 有理数
 * @param[in] b 有理数
 * @return true 相等，false 不等
 */
bool lv_alg_rational_eq(const AlgRational *a, const AlgRational *b);

/**
 * @brief 有理数转 double（近似值）
 *
 * @param[in] r 有理数
 * @return 近似浮点值
 */
double lv_alg_rational_to_double(const AlgRational *r);

/**
 * @brief 有理数转字符串
 *
 * 格式化为 "p/q" 或 "p"（当 q=1 时）。
 *
 * @param[in]  r   有理数
 * @param[out] buf 输出缓冲区
 * @param[in]  size 缓冲区大小
 * @return 写入的字符数（不含终止符），缓冲区不足返回所需大小
 */
int lv_alg_rational_to_string(const AlgRational *r, char *buf, size_t size);

/**
 * @brief 判断有理数是否为零
 *
 * @param[in] r 有理数
 * @return true 为零，false 非零
 */
bool lv_alg_rational_is_zero(const AlgRational *r);

/**
 * @brief 判断有理数是否为正
 *
 * @param[in] r 有理数
 * @return true 为正，false 非正
 */
bool lv_alg_rational_is_positive(const AlgRational *r);

/**
 * @brief 判断有理数是否为负
 *
 * @param[in] r 有理数
 * @return true 为负，false 非负
 */
bool lv_alg_rational_is_negative(const AlgRational *r);

/**
 * @brief 错误码转字符串
 *
 * @param[in] err 错误码
 * @return 错误描述字符串（静态内存，勿释放）
 */
const char *lv_alg_rational_error_string(AlgRationalError err);

/* ============================================================
 * 第二层：二次代数数域 Q(sqrt(d))
 * ============================================================
 * 表示形如 a + b*sqrt(d) 的数，其中 a, b 为有理数，d 为无平方因子的整数。
 * 支持四则运算、共轭、范数等操作。
 * ============================================================ */

/**
 * @brief 二次代数数结构体（第二层数域）
 *
 * 表示 Q(sqrt(d)) 中的元素 a + b*sqrt(d)。
 *
 * 不变量：
 *   - d 为无平方因子的非负整数（d=0 时退化为有理数域）
 *   - a, b 为已约分的有理数
 *   - d=1 时表示有理数（sqrt(1)=1）
 */
typedef struct {
    AlgRational a; /**< 有理部分 a */
    AlgRational b; /**< sqrt(d) 的系数 b */
    int64_t d;     /**< 根号内的整数 d（>= 0，无平方因子） */
} AlgQuadratic;

/**
 * @brief 二次数运算错误码
 */
typedef enum {
    lv_alg_quadratic_OK = 0,       /**< 成功 */
    lv_alg_quadratic_ERR_DOMAIN,   /**< 域不匹配（d 不同） */
    lv_alg_quadratic_ERR_OVERFLOW, /**< int64_t 溢出 */
    lv_alg_quadratic_ERR_NULL,     /**< 空指针 */
    lv_alg_quadratic_ERR_INVALID   /**< 无效参数 */
} AlgQuadraticError;

/**
 * @brief 创建二次代数数
 *
 * 创建 a + b*sqrt(d) 形式的二次代数数。
 * d 应为非负整数（通常无平方因子）。
 *
 * @param[in] a_val 有理部分 a 的分子
 * @param[in] a_den 有理部分 a 的分母
 * @param[in] b_val sqrt(d) 系数 b 的分子
 * @param[in] b_den sqrt(d) 系数 b 的分母
 * @param[in] d     根号内的整数（>= 0）
 * @param[out] err  错误码（可为 NULL）
 * @return 二次代数数
 */
AlgQuadratic lv_alg_quadratic_create(int64_t a_val, int64_t a_den, int64_t b_val, int64_t b_den, int64_t d,
                                                AlgQuadraticError *err);

/**
 * @brief 从有理数创建二次代数数（b=0）
 *
 * @param[in] r 有理数
 * @param[in] d 域参数（sqrt(d) 的 d）
 * @return 二次代数数 r + 0*sqrt(d)
 */
AlgQuadratic lv_alg_quadratic_from_rational(const AlgRational *r, int64_t d);

/**
 * @brief 创建纯 sqrt(d) 倍数
 *
 * 创建 0 + (b_val/b_den)*sqrt(d)。
 *
 * @param[in] b_val 系数分子
 * @param[in] b_den 系数分母
 * @param[in] d     根号内的整数
 * @param[out] err  错误码（可为 NULL）
 * @return 二次代数数
 */
AlgQuadratic lv_alg_quadratic_sqrt(int64_t b_val, int64_t b_den, int64_t d, AlgQuadraticError *err);

/**
 * @brief 二次代数数加法
 *
 * 要求两个操作数的 d 相同（同一数域）。
 *
 * @param[in]  x 加数
 * @param[in]  y 加数
 * @param[out] err 错误码（可为 NULL）
 * @return x + y
 */
AlgQuadratic lv_alg_quadratic_add(const AlgQuadratic *x, const AlgQuadratic *y, AlgQuadraticError *err);

/**
 * @brief 二次代数数减法
 *
 * @param[in]  x 被减数
 * @param[in]  y 减数
 * @param[out] err 错误码（可为 NULL）
 * @return x - y
 */
AlgQuadratic lv_alg_quadratic_sub(const AlgQuadratic *x, const AlgQuadratic *y, AlgQuadraticError *err);

/**
 * @brief 二次代数数乘法
 *
 * (a1 + b1*sqrt(d)) * (a2 + b2*sqrt(d))
 *   = (a1*a2 + b1*b2*d) + (a1*b2 + a2*b1)*sqrt(d)
 *
 * @param[in]  x 乘数
 * @param[in]  y 乘数
 * @param[out] err 错误码（可为 NULL）
 * @return x * y
 */
AlgQuadratic lv_alg_quadratic_mul(const AlgQuadratic *x, const AlgQuadratic *y, AlgQuadraticError *err);

/**
 * @brief 二次代数数除法
 *
 * 通过乘以共轭的逆实现：
 *   x / y = x * conj(y) / norm(y)
 *
 * @param[in]  x 被除数
 * @param[in]  y 除数
 * @param[out] err 错误码（可为 NULL）
 * @return x / y
 */
AlgQuadratic lv_alg_quadratic_div(const AlgQuadratic *x, const AlgQuadratic *y, AlgQuadraticError *err);

/**
 * @brief 二次代数数取负
 *
 * @param[in] x 二次代数数
 * @return -x
 */
AlgQuadratic lv_alg_quadratic_neg(const AlgQuadratic *x);

/**
 * @brief 二次代数数共轭
 *
 * 计算 a - b*sqrt(d)（即 sqrt(d) -> -sqrt(d)）。
 *
 * @param[in] x 二次代数数
 * @return 共轭数
 */
AlgQuadratic lv_alg_quadratic_conj(const AlgQuadratic *x);

/**
 * @brief 二次代数数范数
 *
 * 计算 N(x) = x * conj(x) = a^2 - b^2*d。
 * 范数始终是有理数。
 *
 * @param[in]  x 二次代数数
 * @param[out] err 错误码（可为 NULL）
 * @return 范数（有理数）
 */
AlgRational lv_alg_quadratic_norm(const AlgQuadratic *x, AlgQuadraticError *err);

/**
 * @brief 二次代数数比较
 *
 * 通过转换为 double 近似值进行比较。
 * 对于精确比较，建议使用 lv_alg_quadratic_cmp_exact。
 *
 * @param[in] x 二次代数数
 * @param[in] y 二次代数数
 * @return <0 表示 x < y，0 表示 x == y，>0 表示 x > y
 */
int lv_alg_quadratic_cmp(const AlgQuadratic *x, const AlgQuadratic *y);

/**
 * @brief 二次代数数精确比较
 *
 * 通过比较差值是否为零进行精确判断。
 *
 * @param[in]  x 二次代数数
 * @param[in]  y 二次代数数
 * @param[out] err 错误码（可为 NULL）
 * @return <0 表示 x < y，0 表示 x == y，>0 表示 x > y
 */
int lv_alg_quadratic_cmp_exact(const AlgQuadratic *x, const AlgQuadratic *y, AlgQuadraticError *err);

/**
 * @brief 二次代数数转 double（近似值）
 *
 * @param[in] x 二次代数数
 * @return 近似浮点值
 */
double lv_alg_quadratic_to_double(const AlgQuadratic *x);

/**
 * @brief 二次代数数转字符串
 *
 * 格式化为 "a + b*sqrt(d)" 或 "a - |b|*sqrt(d)" 等形式。
 *
 * @param[in]  x    二次代数数
 * @param[out] buf  输出缓冲区
 * @param[in]  size 缓冲区大小
 * @return 写入的字符数（不含终止符）
 */
int lv_alg_quadratic_to_string(const AlgQuadratic *x, char *buf, size_t size);

/**
 * @brief 判断二次代数数是否为有理数（b=0）
 *
 * @param[in] x 二次代数数
 * @return true 是有理数，false 含 sqrt(d) 分量
 */
bool lv_alg_quadratic_is_rational(const AlgQuadratic *x);

/**
 * @brief 获取二次代数数的有理部分
 *
 * @param[in] x 二次代数数
 * @return 有理部分 a 的副本
 */
AlgRational lv_alg_quadratic_rational_part(const AlgQuadratic *x);

/**
 * @brief 错误码转字符串
 *
 * @param[in] err 错误码
 * @return 错误描述字符串（静态内存，勿释放）
 */
const char *lv_alg_quadratic_error_string(AlgQuadraticError err);

/* ============================================================
 * 第三层：区间运算（隔离区间）
 * ============================================================
 * 使用有理数区间 [lo, hi] 隔离代数数。
 * 支持区间四则运算、交并、包含判断等。
 * ============================================================ */

/**
 * @brief 有理数隔离区间结构体（第三层数域）
 *
 * 表示一个有理数闭区间 [lo, hi]，用于隔离代数数的精确值。
 *
 * 不变量：
 *   - lo <= hi
 *   - lo, hi 为有效有理数
 */
typedef struct {
    AlgRational lo; /**< 区间下界 */
    AlgRational hi; /**< 区间上界 */
} AlgInterval;

/**
 * @brief 区间运算错误码
 */
typedef enum {
    lv_alg_interval_OK = 0,         /**< 成功 */
    lv_alg_interval_ERR_EMPTY,      /**< 空区间 */
    lv_alg_interval_ERR_OVERFLOW,   /**< int64_t 溢出 */
    lv_alg_interval_ERR_NULL,       /**< 空指针 */
    lv_alg_interval_ERR_INVALID,    /**< 无效参数（lo > hi） */
    lv_alg_interval_ERR_DIV_BY_ZERO /**< 除以包含零的区间 */
} AlgIntervalError;

/**
 * @brief 创建隔离区间
 *
 * 创建 [lo_val/lo_den, hi_val/hi_den]。
 * 若 lo > hi，交换 lo 和 hi。
 *
 * @param[in] lo_val 下界分子
 * @param[in] lo_den 下界分母
 * @param[in] hi_val 上界分子
 * @param[in] hi_den 上界分母
 * @param[out] err   错误码（可为 NULL）
 * @return 隔离区间
 */
AlgInterval lv_alg_interval_create(int64_t lo_val, int64_t lo_den, int64_t hi_val, int64_t hi_den,
                                              AlgIntervalError *err);

/**
 * @brief 从单个有理数创建点区间 [r, r]
 *
 * @param[in] r 有理数
 * @return 点区间
 */
AlgInterval lv_alg_interval_point(const AlgRational *r);

/**
 * @brief 从二次代数数创建隔离区间
 *
 * 通过计算 a - |b|*sqrt(d) 和 a + |b|*sqrt(d) 创建包围区间。
 * 若 d 为完全平方数，则创建精确点区间。
 *
 * @param[in]  x   二次代数数
 * @param[out] err 错误码（可为 NULL）
 * @return 隔离区间
 */
AlgInterval lv_alg_interval_from_quadratic(const AlgQuadratic *x, AlgIntervalError *err);

/**
 * @brief 区间加法
 *
 * [a, b] + [c, d] = [a+c, b+d]
 *
 * @param[in]  x 区间
 * @param[in]  y 区间
 * @param[out] err 错误码（可为 NULL）
 * @return 和区间
 */
AlgInterval lv_alg_interval_add(const AlgInterval *x, const AlgInterval *y, AlgIntervalError *err);

/**
 * @brief 区间减法
 *
 * [a, b] - [c, d] = [a-d, b-c]
 *
 * @param[in]  x 区间
 * @param[in]  y 区间
 * @param[out] err 错误码（可为 NULL）
 * @return 差区间
 */
AlgInterval lv_alg_interval_sub(const AlgInterval *x, const AlgInterval *y, AlgIntervalError *err);

/**
 * @brief 区间乘法
 *
 * [a, b] * [c, d] 的结果为四个端点乘积的最小值和最大值构成的区间。
 *
 * @param[in]  x 区间
 * @param[in]  y 区间
 * @param[out] err 错误码（可为 NULL）
 * @return 积区间
 */
AlgInterval lv_alg_interval_mul(const AlgInterval *x, const AlgInterval *y, AlgIntervalError *err);

/**
 * @brief 区间除法
 *
 * [a, b] / [c, d]，要求 [c, d] 不包含零。
 *
 * @param[in]  x 区间
 * @param[in]  y 区间（不含零）
 * @param[out] err 错误码（可为 NULL）
 * @return 商区间
 */
AlgInterval lv_alg_interval_div(const AlgInterval *x, const AlgInterval *y, AlgIntervalError *err);

/**
 * @brief 区间取负
 *
 * -[a, b] = [-b, -a]
 *
 * @param[in] x 区间
 * @return 负区间
 */
AlgInterval lv_alg_interval_neg(const AlgInterval *x);

/**
 * @brief 区间交集
 *
 * 计算两个区间的交集。若交集为空，返回空区间并设置错误码。
 *
 * @param[in]  x 区间
 * @param[in]  y 区间
 * @param[out] err 错误码（可为 NULL）
 * @return 交集区间（可能为空）
 */
AlgInterval lv_alg_interval_intersect(const AlgInterval *x, const AlgInterval *y, AlgIntervalError *err);

/**
 * @brief 区间并集
 *
 * 计算两个区间的凸包（最小包含区间）。
 *
 * @param[in]  x 区间
 * @param[in]  y 区间
 * @param[out] err 错误码（可为 NULL）
 * @return 并集的凸包区间
 */
AlgInterval lv_alg_interval_hull(const AlgInterval *x, const AlgInterval *y, AlgIntervalError *err);

/**
 * @brief 区间包含判断
 *
 * 判断区间 x 是否包含区间 y。
 *
 * @param[in] x 外区间
 * @param[in] y 内区间
 * @return true x 包含 y，false 不包含
 */
bool lv_alg_interval_contains(const AlgInterval *x, const AlgInterval *y);

/**
 * @brief 判断区间是否包含某个有理数
 *
 * @param[in] x 区间
 * @param[in] r 有理数
 * @return true 区间包含 r，false 不包含
 */
bool lv_alg_interval_contains_rational(const AlgInterval *x, const AlgRational *r);

/**
 * @brief 判断区间是否为空
 *
 * @param[in] x 区间
 * @return true 为空（lo > hi），false 非空
 */
bool lv_alg_interval_is_empty(const AlgInterval *x);

/**
 * @brief 判断区间是否为点区间（lo == hi）
 *
 * @param[in] x 区间
 * @return true 为点区间，false 非点区间
 */
bool lv_alg_interval_is_point(const AlgInterval *x);

/**
 * @brief 区间宽度
 *
 * 计算 hi - lo。
 *
 * @param[in]  x   区间
 * @param[out] err 错误码（可为 NULL）
 * @return 区间宽度（有理数）
 */
AlgRational lv_alg_interval_width(const AlgInterval *x, AlgIntervalError *err);

/**
 * @brief 区间中点
 *
 * 计算 (lo + hi) / 2。
 *
 * @param[in]  x   区间
 * @param[out] err 错误码（可为 NULL）
 * @return 中点（有理数）
 */
AlgRational lv_alg_interval_midpoint(const AlgInterval *x, AlgIntervalError *err);

/**
 * @brief 区间二分
 *
 * 将区间 [lo, hi] 分为 [lo, mid] 和 [mid, hi]。
 *
 * @param[in]  x     区间
 * @param[out] lower 下半区间（可为 NULL）
 * @param[out] upper 上半区间（可为 NULL）
 * @param[out] err   错误码（可为 NULL）
 */
void lv_alg_interval_bisect(const AlgInterval *x, AlgInterval *lower, AlgInterval *upper,
                                       AlgIntervalError *err);

/**
 * @brief 区间转字符串
 *
 * 格式化为 "[lo, hi]"。
 *
 * @param[in]  x    区间
 * @param[out] buf  输出缓冲区
 * @param[in]  size 缓冲区大小
 * @return 写入的字符数（不含终止符）
 */
int lv_alg_interval_to_string(const AlgInterval *x, char *buf, size_t size);

/**
 * @brief 错误码转字符串
 *
 * @param[in] err 错误码
 * @return 错误描述字符串（静态内存，勿释放）
 */
const char *lv_alg_interval_error_string(AlgIntervalError err);

/* ============================================================
 * 第四层：多项式系统（简化版）
 * ============================================================
 * 使用 int64_t 系数的多项式，支持求值、加减乘、
 * 判别式计算、有理根检测等基本操作。
 * ============================================================ */

/**
 * @brief 多项式最大度数限制
 */
#define lv_alg_poly_MAX_DEGREE 16

/**
 * @brief 多项式结构体（简化版）
 *
 * 表示一个整系数多项式：
 *   p(x) = coef[0] + coef[1]*x + coef[2]*x^2 + ... + coef[degree]*x^degree
 *
 * 不变量：
 *   - degree >= 0
 *   - coef[degree] != 0（首项系数非零）
 *   - degree <= lv_alg_poly_MAX_DEGREE
 */
typedef struct {
    int64_t coef[lv_alg_poly_MAX_DEGREE + 1]; /**< 系数数组（coef[i] 为 x^i 的系数） */
    int degree;                            /**< 多项式次数 */
} AlgPoly;

/**
 * @brief 多项式运算错误码
 */
typedef enum {
    lv_alg_poly_OK = 0,         /**< 成功 */
    lv_alg_poly_ERR_DEGREE,     /**< 次数超限 */
    lv_alg_poly_ERR_OVERFLOW,   /**< int64_t 溢出 */
    lv_alg_poly_ERR_NULL,       /**< 空指针 */
    lv_alg_poly_ERR_INVALID,    /**< 无效参数 */
    lv_alg_poly_ERR_DIV_BY_ZERO /**< 除以零多项式 */
} AlgPolyError;

/**
 * @brief 创建零多项式
 *
 * @return p(x) = 0
 */
AlgPoly lv_alg_poly_zero(void);

/**
 * @brief 创建常数多项式
 *
 * @param[in] c 常数值
 * @return p(x) = c
 */
AlgPoly lv_alg_poly_const(int64_t c);

/**
 * @brief 创建一次多项式
 *
 * p(x) = a*x + b
 *
 * @param[in] a 一次项系数
 * @param[in] b 常数项
 * @return 一次多项式
 */
AlgPoly lv_alg_poly_linear(int64_t a, int64_t b);

/**
 * @brief 创建二次多项式
 *
 * p(x) = a*x^2 + b*x + c
 *
 * @param[in] a 二次项系数
 * @param[in] b 一次项系数
 * @param[in] c 常数项
 * @return 二次多项式
 */
AlgPoly lv_alg_poly_quadratic(int64_t a, int64_t b, int64_t c);

/**
 * @brief 创建单位多项式 p(x) = x
 *
 * @return p(x) = x
 */
AlgPoly lv_alg_poly_x(void);

/**
 * @brief 多项式求值（代入整数）
 *
 * 使用 Horner 方法计算 p(n)。
 *
 * @param[in]  p   多项式
 * @param[in]  n   代入值
 * @param[out] err 错误码（可为 NULL）
 * @return p(n) 的值
 */
int64_t lv_alg_poly_eval_int(const AlgPoly *p, int64_t n, AlgPolyError *err);

/**
 * @brief 多项式求值（代入有理数）
 *
 * 计算 p(r)，其中 r 为有理数。
 *
 * @param[in]  p   多项式
 * @param[in]  r   有理数值
 * @param[out] err 错误码（可为 NULL）
 * @return p(r) 的值（有理数）
 */
AlgRational lv_alg_poly_eval_rational(const AlgPoly *p, const AlgRational *r, AlgPolyError *err);

/**
 * @brief 多项式加法
 *
 * @param[in]  p   多项式
 * @param[in]  q   多项式
 * @param[out] err 错误码（可为 NULL）
 * @return p + q
 */
AlgPoly lv_alg_poly_add(const AlgPoly *p, const AlgPoly *q, AlgPolyError *err);

/**
 * @brief 多项式减法
 *
 * @param[in]  p   多项式
 * @param[in]  q   多项式
 * @param[out] err 错误码（可为 NULL）
 * @return p - q
 */
AlgPoly lv_alg_poly_sub(const AlgPoly *p, const AlgPoly *q, AlgPolyError *err);

/**
 * @brief 多项式乘法
 *
 * @param[in]  p   多项式
 * @param[in]  q   多项式
 * @param[out] err 错误码（可为 NULL）
 * @return p * q
 */
AlgPoly lv_alg_poly_mul(const AlgPoly *p, const AlgPoly *q, AlgPolyError *err);

/**
 * @brief 多项式取负
 *
 * @param[in] p 多项式
 * @return -p
 */
AlgPoly lv_alg_poly_neg(const AlgPoly *p);

/**
 * @brief 多项式首项系数
 *
 * @param[in] p 多项式
 * @return 首项系数（coef[degree]）
 */
int64_t lv_alg_poly_lead_coef(const AlgPoly *p);

/**
 * @brief 多项式常数项
 *
 * @param[in] p 多项式
 * @return 常数项（coef[0]）
 */
int64_t lv_alg_poly_const_coef(const AlgPoly *p);

/**
 * @brief 判断多项式是否为零多项式
 *
 * @param[in] p 多项式
 * @return true 为零多项式，false 非零
 */
bool lv_alg_poly_is_zero(const AlgPoly *p);

/**
 * @brief 判断多项式是否为常数多项式
 *
 * @param[in] p 多项式
 * @return true 为常数多项式（degree == 0），false 非常数
 */
bool lv_alg_poly_is_const(const AlgPoly *p);

/**
 * @brief 多项式判别式（简化版，仅支持 degree <= 2）
 *
 * 对于二次多项式 ax^2+bx+c，判别式为 b^2-4ac。
 * 对于一次多项式，判别式为 1。
 * 对于常数多项式，判别式为 0。
 *
 * @param[in]  p   多项式
 * @param[out] err 错误码（可为 NULL）
 * @return 判别式值
 */
int64_t lv_alg_poly_discriminant(const AlgPoly *p, AlgPolyError *err);

/**
 * @brief 多项式有理根检测（有理根定理）
 *
 * 根据有理根定理，p/q 是多项式 f(x) = a_n*x^n + ... + a_0 的有理根，
 * 则 p | a_0 且 q | a_n。
 *
 * 返回找到的有理根数量（最多 max_roots 个）。
 *
 * @param[in]  p          多项式
 * @param[out] roots      找到的有理根数组
 * @param[in]  max_roots  最大返回根数
 * @param[out] err        错误码（可为 NULL）
 * @return 实际找到的有理根数量
 */
int lv_alg_poly_rational_roots(const AlgPoly *p, AlgRational *roots, int max_roots, AlgPolyError *err);

/**
 * @brief 多项式导数
 *
 * 计算多项式的形式导数。
 *
 * @param[in]  p   多项式
 * @param[out] err 错误码（可为 NULL）
 * @return 导数多项式
 */
AlgPoly lv_alg_poly_derivative(const AlgPoly *p, AlgPolyError *err);

/**
 * @brief 多项式转字符串
 *
 * 格式化为 "a_n*x^n + ... + a_1*x + a_0" 形式。
 *
 * @param[in]  p    多项式
 * @param[out] buf  输出缓冲区
 * @param[in]  size 缓冲区大小
 * @return 写入的字符数（不含终止符）
 */
int lv_alg_poly_to_string(const AlgPoly *p, char *buf, size_t size);

/**
 * @brief 错误码转字符串
 *
 * @param[in] err 错误码
 * @return 错误描述字符串（静态内存，勿释放）
 */
const char *lv_alg_poly_error_string(AlgPolyError err);

/* ============================================================
 * 跨层数域转换工具
 * ============================================================ */

/**
 * @brief 二次代数数转隔离区间
 *
 * @param[in]  x   二次代数数
 * @param[out] err 错误码（可为 NULL）
 * @return 隔离区间
 */
AlgInterval lv_alg_quadratic_to_interval(const AlgQuadratic *x, AlgIntervalError *err);

/**
 * @brief 有理数转隔离区间（点区间）
 *
 * @param[in] r 有理数
 * @return 点区间 [r, r]
 */
AlgInterval lv_alg_rational_to_interval(const AlgRational *r);

/**
 * @brief 判断二次多项式是否有实根
 *
 * 通过判别式判断 ax^2+bx+c 是否有实根。
 *
 * @param[in] a 二次项系数
 * @param[in] b 一次项系数
 * @param[in] c 常数项
 * @return true 有实根，false 无实根
 */
bool lv_alg_has_real_roots(int64_t a, int64_t b, int64_t c);

/* ============================================================
 * 向后兼容别名（旧名称 → lv_ 前缀新名称）
 *
 * v3.5.0 起公共符号统一使用 lv_ 前缀（lv_alg_*）。
 * 以下 #define 仅为既有外部代码提供向后兼容，新代码请直接使用 lv_alg_*。
 * ============================================================ */
#define alg_rational_create lv_alg_rational_create
#define alg_rational_zero lv_alg_rational_zero
#define alg_rational_one lv_alg_rational_one
#define alg_rational_from_int lv_alg_rational_from_int
#define alg_rational_add lv_alg_rational_add
#define alg_rational_sub lv_alg_rational_sub
#define alg_rational_mul lv_alg_rational_mul
#define alg_rational_div lv_alg_rational_div
#define alg_rational_neg lv_alg_rational_neg
#define alg_rational_abs lv_alg_rational_abs
#define alg_rational_inv lv_alg_rational_inv
#define alg_rational_pow lv_alg_rational_pow
#define alg_rational_cmp lv_alg_rational_cmp
#define alg_rational_eq lv_alg_rational_eq
#define alg_rational_to_double lv_alg_rational_to_double
#define alg_rational_to_string lv_alg_rational_to_string
#define alg_rational_is_zero lv_alg_rational_is_zero
#define alg_rational_is_positive lv_alg_rational_is_positive
#define alg_rational_is_negative lv_alg_rational_is_negative
#define alg_rational_error_string lv_alg_rational_error_string
#define alg_quadratic_create lv_alg_quadratic_create
#define alg_quadratic_from_rational lv_alg_quadratic_from_rational
#define alg_quadratic_sqrt lv_alg_quadratic_sqrt
#define alg_quadratic_add lv_alg_quadratic_add
#define alg_quadratic_sub lv_alg_quadratic_sub
#define alg_quadratic_mul lv_alg_quadratic_mul
#define alg_quadratic_div lv_alg_quadratic_div
#define alg_quadratic_neg lv_alg_quadratic_neg
#define alg_quadratic_conj lv_alg_quadratic_conj
#define alg_quadratic_norm lv_alg_quadratic_norm
#define alg_quadratic_cmp lv_alg_quadratic_cmp
#define alg_quadratic_cmp_exact lv_alg_quadratic_cmp_exact
#define alg_quadratic_to_double lv_alg_quadratic_to_double
#define alg_quadratic_to_string lv_alg_quadratic_to_string
#define alg_quadratic_is_rational lv_alg_quadratic_is_rational
#define alg_quadratic_rational_part lv_alg_quadratic_rational_part
#define alg_quadratic_error_string lv_alg_quadratic_error_string
#define alg_interval_create lv_alg_interval_create
#define alg_interval_point lv_alg_interval_point
#define alg_interval_from_quadratic lv_alg_interval_from_quadratic
#define alg_interval_add lv_alg_interval_add
#define alg_interval_sub lv_alg_interval_sub
#define alg_interval_mul lv_alg_interval_mul
#define alg_interval_div lv_alg_interval_div
#define alg_interval_neg lv_alg_interval_neg
#define alg_interval_intersect lv_alg_interval_intersect
#define alg_interval_hull lv_alg_interval_hull
#define alg_interval_contains lv_alg_interval_contains
#define alg_interval_contains_rational lv_alg_interval_contains_rational
#define alg_interval_is_empty lv_alg_interval_is_empty
#define alg_interval_is_point lv_alg_interval_is_point
#define alg_interval_width lv_alg_interval_width
#define alg_interval_midpoint lv_alg_interval_midpoint
#define alg_interval_bisect lv_alg_interval_bisect
#define alg_interval_to_string lv_alg_interval_to_string
#define alg_interval_error_string lv_alg_interval_error_string
#define alg_poly_zero lv_alg_poly_zero
#define alg_poly_const lv_alg_poly_const
#define alg_poly_linear lv_alg_poly_linear
#define alg_poly_quadratic lv_alg_poly_quadratic
#define alg_poly_x lv_alg_poly_x
#define alg_poly_eval_int lv_alg_poly_eval_int
#define alg_poly_eval_rational lv_alg_poly_eval_rational
#define alg_poly_add lv_alg_poly_add
#define alg_poly_sub lv_alg_poly_sub
#define alg_poly_mul lv_alg_poly_mul
#define alg_poly_neg lv_alg_poly_neg
#define alg_poly_lead_coef lv_alg_poly_lead_coef
#define alg_poly_const_coef lv_alg_poly_const_coef
#define alg_poly_is_zero lv_alg_poly_is_zero
#define alg_poly_is_const lv_alg_poly_is_const
#define alg_poly_discriminant lv_alg_poly_discriminant
#define alg_poly_rational_roots lv_alg_poly_rational_roots
#define alg_poly_derivative lv_alg_poly_derivative
#define alg_poly_to_string lv_alg_poly_to_string
#define alg_poly_error_string lv_alg_poly_error_string
#define alg_quadratic_to_interval lv_alg_quadratic_to_interval
#define alg_rational_to_interval lv_alg_rational_to_interval
#define alg_has_real_roots lv_alg_has_real_roots

#ifdef __cplusplus
}
#endif

/* ============== 向后兼容别名 ==============
 * v3.x 前枚举名为 ALG_*_*（无 lv_ 前缀），现已统一为 lv_alg_*_*。
 * 以下别名保持旧代码可编译。 */
#ifndef ALG_RATIONAL_OK
#define ALG_RATIONAL_OK lv_alg_rational_OK
#define ALG_RATIONAL_ERR_ZERO_DEN lv_alg_rational_ERR_ZERO_DEN
#define ALG_RATIONAL_ERR_OVERFLOW lv_alg_rational_ERR_OVERFLOW
#define ALG_RATIONAL_ERR_NULL lv_alg_rational_ERR_NULL
#define ALG_RATIONAL_ERR_INVALID lv_alg_rational_ERR_INVALID
#endif
#ifndef ALG_QUADRATIC_OK
#define ALG_QUADRATIC_OK lv_alg_quadratic_OK
#define ALG_QUADRATIC_ERR_DOMAIN lv_alg_quadratic_ERR_DOMAIN
#define ALG_QUADRATIC_ERR_OVERFLOW lv_alg_quadratic_ERR_OVERFLOW
#define ALG_QUADRATIC_ERR_NULL lv_alg_quadratic_ERR_NULL
#define ALG_QUADRATIC_ERR_INVALID lv_alg_quadratic_ERR_INVALID
#endif
#ifndef ALG_INTERVAL_OK
#define ALG_INTERVAL_OK lv_alg_interval_OK
#define ALG_INTERVAL_ERR_EMPTY lv_alg_interval_ERR_EMPTY
#define ALG_INTERVAL_ERR_OVERFLOW lv_alg_interval_ERR_OVERFLOW
#define ALG_INTERVAL_ERR_NULL lv_alg_interval_ERR_NULL
#define ALG_INTERVAL_ERR_INVALID lv_alg_interval_ERR_INVALID
#define ALG_INTERVAL_ERR_DIV_BY_ZERO lv_alg_interval_ERR_DIV_BY_ZERO
#endif
#ifndef ALG_POLY_OK
#define ALG_POLY_OK lv_alg_poly_OK
#define ALG_POLY_ERR_DEGREE lv_alg_poly_ERR_DEGREE
#define ALG_POLY_ERR_OVERFLOW lv_alg_poly_ERR_OVERFLOW
#define ALG_POLY_ERR_NULL lv_alg_poly_ERR_NULL
#define ALG_POLY_ERR_INVALID lv_alg_poly_ERR_INVALID
#define ALG_POLY_ERR_DIV_BY_ZERO lv_alg_poly_ERR_DIV_BY_ZERO
#endif

#endif /* lv_ALGEBRAIC_NUMBER_H */
