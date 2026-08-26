#ifndef lv_EXPR_CANON_H
#define lv_EXPR_CANON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "lv/rational.h"

/* ============================================================
 * 类型定义
 * ============================================================ */

/**
 * @brief 多项式项结构
 *
 * 表示一个单项式：coeff * x0^e0 * x1^e1 * ... * xn^en
 */
typedef struct {
    lvRational *coeff; /**< 系数 */
    int *exponents;    /**< 各变量的指数数组，长度为 var_count */
    int var_count;     /**< 变量个数 */
} lvExprTerm;

/**
 * @brief 规范多项式表达式
 *
 * 以稀疏多项式形式存储的代数表达式规范表示。
 * 项按总次数降序排列，同次数按字典序排列。
 */
typedef struct {
    lvExprTerm *terms;  /**< 项数组 */
    int term_count;     /**< 当前项数 */
    int term_capacity;  /**< 项数组容量 */
    int var_count;      /**< 变量个数 */
    char **var_names;   /**< 变量名数组（可选，可为 NULL） */
    bool canonicalized; /**< 是否已规范化 */
} lvExprCanonical;

/* ============================================================
 * 排序规则
 * ============================================================ */

/**
 * @brief 按规范顺序比较两个指数数组
 * @param a 第一个指数数组
 * @param b 第二个指数数组
 * @param var_count 变量个数
 * @return 正数表示 a > b，负数表示 a < b，0 表示相等
 */
int lv_canonical_compare_terms(const int *a, const int *b, int var_count);

/* ============================================================
 * 生命周期管理
 * ============================================================ */

/**
 * @brief 创建空的规范多项式
 * @param var_count 变量个数
 * @param var_names 变量名数组（可选，可为 NULL）
 * @return 新的规范多项式，失败返回 NULL
 */
lvExprCanonical *lv_expr_canonical_create(int var_count, const char **var_names);

/**
 * @brief 销毁规范多项式
 * @param expr 指向多项式指针的指针（销毁后置 NULL）
 */
void lv_expr_canonical_destroy(lvExprCanonical **expr);

/**
 * @brief 克隆规范多项式
 * @param src 源多项式
 * @return 新的副本，失败返回 NULL
 */
lvExprCanonical *lv_expr_canonical_clone(const lvExprCanonical *src);

/* ============================================================
 * 项操作
 * ============================================================ */

/**
 * @brief 向多项式添加一项
 * @param expr 目标多项式
 * @param coeff 项的系数
 * @param exponents 项的指数数组
 * @return 是否成功
 */
bool lv_expr_canonical_add_term(lvExprCanonical *expr, const lvRational *coeff, const int *exponents);

/* ============================================================
 * 规范化
 * ============================================================ */

/**
 * @brief 将多项式规范化（合并同类项、排序）
 * @param expr 多项式
 * @return 是否成功
 */
bool lv_expr_canonicalize(lvExprCanonical *expr);

/**
 * @brief 检查多项式是否处于规范形式
 * @param expr 多项式
 * @return 是否规范
 */
bool lv_expr_is_canonical(const lvExprCanonical *expr);

/* ============================================================
 * 算术运算
 * ============================================================ */

/**
 * @brief 多项式加法
 * @param a 第一个多项式
 * @param b 第二个多项式
 * @return a + b，失败返回 NULL
 */
lvExprCanonical *lv_expr_canonical_add(const lvExprCanonical *a, const lvExprCanonical *b);

/**
 * @brief 多项式减法
 * @param a 被减多项式
 * @param b 减多项式
 * @return a - b，失败返回 NULL
 */
lvExprCanonical *lv_expr_canonical_sub(const lvExprCanonical *a, const lvExprCanonical *b);

/**
 * @brief 多项式乘法
 * @param a 第一个多项式
 * @param b 第二个多项式
 * @return a * b，失败返回 NULL
 */
lvExprCanonical *lv_expr_canonical_mul(const lvExprCanonical *a, const lvExprCanonical *b);

/**
 * @brief 多项式数乘
 * @param a 多项式
 * @param coeff 标量系数
 * @return coeff * a，失败返回 NULL
 */
lvExprCanonical *lv_expr_canonical_scale(const lvExprCanonical *a, const lvRational *coeff);

/**
 * @brief 多项式取负
 * @param a 多项式
 * @return -a，失败返回 NULL
 */
lvExprCanonical *lv_expr_canonical_neg(const lvExprCanonical *a);

/* ============================================================
 * 比较与查询
 * ============================================================ */

/**
 * @brief 比较两个多项式是否相等
 * @param a 第一个多项式
 * @param b 第二个多项式
 * @return 是否相等
 */
bool lv_expr_canonical_equal(const lvExprCanonical *a, const lvExprCanonical *b);

/**
 * @brief 检查多项式是否为零
 * @param a 多项式
 * @return 是否为零
 */
bool lv_expr_canonical_is_zero(const lvExprCanonical *a);

/**
 * @brief 获取多项式的总次数
 * @param expr 多项式
 * @return 总次数，零多项式返回 -1
 */
int lv_expr_canonical_degree(const lvExprCanonical *expr);

/**
 * @brief 获取多项式的项数
 * @param expr 多项式
 * @return 项数
 */
int lv_expr_canonical_term_count(const lvExprCanonical *expr);

/* ============================================================
 * 字符串表示
 * ============================================================ */

/**
 * @brief 将多项式转换为字符串
 * @param expr 多项式
 * @return 字符串表示，调用者负责用 free() 释放
 */
char *lv_expr_canonical_to_string(const lvExprCanonical *expr);

/**
 * @brief 从字符串解析多项式（完整实现，支持系数/变量幂次/± 分隔）
 * @param str 输入字符串
 * @param var_names 变量名数组
 * @param var_count 变量个数
 * @return 解析后的多项式，失败返回 NULL
 */
lvExprCanonical *lv_expr_canonical_from_string(const char *str, const char **var_names, int var_count);

/* ============================================================
 * 兼容性接口
 * ============================================================ */

/**
 * @brief 规范化表达式字符串（完整实现；内部经 lv_expr_canonical_from_string）
 * @param expr 表达式字符串
 * @return 规范化后的字符串，失败返回 NULL
 */
char *lv_expr_canon(const char *expr);

#ifdef __cplusplus
}
#endif

#endif /* lv_EXPR_CANON_H */
