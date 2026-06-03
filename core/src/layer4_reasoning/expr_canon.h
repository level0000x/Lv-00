/**
 * @file expr_canon.h
 * @brief 代数表达式规范形式 (Canonical Form)
 *
 * @details 定义 Lv-00 代数表达式的规范形式及其排序规则。
 *
 *          规范形式定义：
 *          - 多项式项按总次数降序排列
 *          - 同次数内按字典序排列变量名
 *          - 合并同类项
 *          - 消除零系数项
 *          - 归一化符号：首项系数为正
 *
 *          此规范形式确保对于代数等价的两个不同表达式，
 *          它们的规范形式相同。这用于：
 *          - 约束求解中的方程标准化
 *          - Groebner 基前的项排序固定化
 *          - 等价性检查（快速判断两个表达式是否等价）
 *          - 缓存 - 相同规范形式可安全使用同一缓存键
 *
 * @author Lv-00 Project
 * @version v1.0.0
 * @date 2026-05-24
 */

#ifndef LV00_EXPR_CANON_H
#define LV00_EXPR_CANON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rational.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 项与表达式类型
 * ======================================================================== */

/**
 * @brief 单个多项式项: coeff * x^e_x * y^e_y * z^e_z * ...
 *
 * 所有次数为 >=0 的非负整数。总次数 = sum(exp[i])。
 */
typedef struct {
    Lv00Rational *coeff;    /**< 有理数系数 */
    int *exponents;         /**< 各变量的指数数组 */
    int var_count;          /**< 变量数（指数数组长度） */
} Lv00ExprTerm;

/**
 * @brief 规范多项式: 项的列表（按序排列）
 *
 * 规范形式保证:
 *   - 项按总次数降序排列
 *   - 同次数内按字典序（变量名）排列
 *   - 无零系数项
 *   - 首项系数为正
 */
typedef struct {
    Lv00ExprTerm *terms;    /**< 项数组 */
    int term_count;         /**< 项数量 */
    int term_capacity;      /**< 内部容量 */
    int var_count;          /**< 变量数 */
    char **var_names;       /**< 变量名数组（可为 NULL 表示使用默认名称） */
    bool canonicalized;     /**< 是否已规范化（add_term 后置 false，canonicalize 成功后置 true） */
} Lv00ExprCanonical;

/* ========================================================================
 * 生命周期函数
 * ======================================================================== */

/**
 * @brief 创建空的规范多项式
 *
 * @param var_count  变量数量
 * @param var_names  变量名数组（可为 NULL，则使用 x0, x1, ...）
 * @return 新分配的规范多项式，失败返回 NULL
 */
Lv00ExprCanonical *lv00_expr_canonical_create(int var_count, const char **var_names);

/**
 * @brief 销毁规范多项式并释放所有资源
 *
 * @param expr 多项式指针的指针
 */
void lv00_expr_canonical_destroy(Lv00ExprCanonical **expr);

/**
 * @brief 拷贝规范多项式
 *
 * @param src 源多项式
 * @return 新分配的拷贝，失败返回 NULL
 */
Lv00ExprCanonical *lv00_expr_canonical_clone(const Lv00ExprCanonical *src);

/* ========================================================================
 * 规范形式操作
 * ======================================================================== */

/**
 * @brief 向多项式中添加一项
 *
 * 添加后可能需要重新 canonicalize 以合并同类项和排序。
 *
 * @param expr      多项式
 * @param coeff     系数（内部会拷贝）
 * @param exponents 指数数组（内部会拷贝），长度必须等于 var_count
 * @return true 成功，false 失败
 */
bool lv00_expr_canonical_add_term(Lv00ExprCanonical *expr, const Lv00Rational *coeff, const int *exponents);

/**
 * @brief 执行规范化
 *
 * 将当前的多项式规范化：
 *   1. 合并同类项（相同指数组合）
 *   2. 消除零系数项
 *   3. 按总次数降序排序，同次数内按字典序
 *   4. 归一化符号：确保第一个非零项的系数为正
 *
 * @param expr 多项式
 * @return true 成功，false 失败
 */
bool lv00_expr_canonicalize(Lv00ExprCanonical *expr);

/**
 * @brief 判断多项式是否为规范形式
 *
 * 用于测试和断言：确认多项式已经满足所有规范不变式。
 *
 * @param expr 多项式
 * @return true 是规范形式，false 否
 */
bool lv00_expr_is_canonical(const Lv00ExprCanonical *expr);

/* ========================================================================
 * 排序规则
 * ======================================================================== */

/**
 * @brief 比较两个项在规范排序中的顺序
 *
 * 用于 qsort / bsearch 和规范化中的排序操作。
 *
 * 排序规则:
 *   1. 总次数降序: 次数大的项排前面
 *   2. 同次数: 按字典序比较指数数组（最后一个变量优先比较，
 *      形成 grlex/monomial ordering 的混合策略）
 *   3. 完全相同指数: 返回 0（应被合并）
 *
 * 【重要：返回值语义与标准 qsort 比较函数相反】
 * 本函数的返回值语义与 C 标准库 qsort 的比较函数约定相反：
 *   - 返回 < 0：表示 a 应排在 b 之后（即 a 的次数更小或字典序更后）
 *   - 返回 > 0：表示 a 应排在 b 之前（即 a 的次数更大或字典序更前）
 *   - 返回 0：  表示 a 和 b 完全相同（应合并同类项）
 *
 * 这意味着本函数实现的是降序排列（次数大的在前），而非标准 qsort
 * 比较函数的升序约定。直接将本函数传给 qsort 时，排序结果为降序。
 * 如果需要升序排列，需要将返回值取反后再传给 qsort。
 *
 * @param a 指数数组 a
 * @param b 指数数组 b
 * @param var_count 变量数
 * @return <0 a < b (a 应在 b 之后), 0 相等, >0 a > b (a 应在 b 之前)
 */
int lv00_canonical_compare_terms(const int *a, const int *b, int var_count);

/* ========================================================================
 * 算术操作（产生规范结果）
 * ======================================================================== */

/**
 * @brief 两个规范多项式的加法
 *
 * result = a + b，结果为规范形式。
 *
 * @param a 左操作数
 * @param b 右操作数
 * @return 新分配的规范多项式，失败返回 NULL
 */
Lv00ExprCanonical *lv00_expr_canonical_add(const Lv00ExprCanonical *a, const Lv00ExprCanonical *b);

/**
 * @brief 两个规范多项式的减法
 *
 * result = a - b
 */
Lv00ExprCanonical *lv00_expr_canonical_sub(const Lv00ExprCanonical *a, const Lv00ExprCanonical *b);

/**
 * @brief 两个规范多项式的乘法
 *
 * result = a * b
 */
Lv00ExprCanonical *lv00_expr_canonical_mul(const Lv00ExprCanonical *a, const Lv00ExprCanonical *b);

/**
 * @brief 多项式乘以标量
 *
 * result = coeff * a
 */
Lv00ExprCanonical *lv00_expr_canonical_scale(const Lv00ExprCanonical *a, const Lv00Rational *coeff);

/**
 * @brief 取反: result = -a
 */
Lv00ExprCanonical *lv00_expr_canonical_neg(const Lv00ExprCanonical *a);

/* ========================================================================
 * 比较与查询
 * ======================================================================== */

/**
 * @brief 判断两个规范多项式是否相等
 *
 * 对于规范形式，相等性检查是线性的（逐项比较）。
 *
 * @param a 多项式 a
 * @param b 多项式 b
 * @return true 相等
 */
bool lv00_expr_canonical_equal(const Lv00ExprCanonical *a, const Lv00ExprCanonical *b);

/**
 * @brief 判断多项式是否为 0
 */
bool lv00_expr_canonical_is_zero(const Lv00ExprCanonical *a);

/**
 * @brief 获取多项式的总次数
 *
 * @param expr 多项式
 * @return 总次数，空为 -1，零多项式为 -1
 */
int lv00_expr_canonical_degree(const Lv00ExprCanonical *expr);

/**
 * @brief 获取多项式的项数（合并同类项后）
 */
int lv00_expr_canonical_term_count(const Lv00ExprCanonical *expr);

/* ========================================================================
 * 字符串表示
 * ======================================================================== */

/**
 * @brief 将规范多项式序列化为人类可读字符串
 *
 * 格式: "3/2*x0^2 + x0*x1 - 5*x1^3 + 1"
 *
 * @param expr 多项式
 * @return 新分配的字符串（调用者负责 free），失败返回 NULL
 */
char *lv00_expr_canonical_to_string(const Lv00ExprCanonical *expr);

/**
 * @brief 从字符串解析多项式（基本格式）
 *
 * 支持格式: "3*x^2 + 2*x*y + y^2 - 1"
 * 基本实现 - 复杂解析应由 DSL 编译器处理。
 *
 * @param str       输入字符串
 * @param var_names 变量名数组
 * @param var_count 变量数
 * @return 新分配的规范多项式，解析失败返回 NULL
 */
Lv00ExprCanonical *lv00_expr_canonical_from_string(const char *str, const char **var_names, int var_count);

#ifdef __cplusplus
}
#endif

#endif /* LV00_EXPR_CANON_H */
