/**
 * @file expr_canonical.h
 * @brief 符号表达式规范表示 —— lvExpr 类型定义与构造/操作 API
 *
 * @details 定义符号表达式的树形结构，支持：
 *   - 变量 (EXPR_TYPE_VARIABLE)
 *   - 有理数常量 (EXPR_TYPE_RATIONAL)
 *   - 幂运算 (EXPR_TYPE_POWER): base^exponent
 *   - 乘积 (EXPR_TYPE_PRODUCT): a * b * ...
 *   - 和 (EXPR_TYPE_SUM): a + b + ...
 *   - 函数应用 (EXPR_TYPE_FUNCTION): f(x)
 *
 * @author Lv-00 Project
 * @version 1.1.0
 */

#ifndef lv_EXPR_CANONICAL_H
#define lv_EXPR_CANONICAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include <gmp.h>
#include <stdbool.h>
#include <stdint.h>

/* ============== 表达式类型枚举 ============== */

typedef enum {
    EXPR_TYPE_VARIABLE, /**< 变量，如 x, y, a */
    EXPR_TYPE_RATIONAL, /**< 有理数常量，如 3/4 */
    EXPR_TYPE_POWER,    /**< 幂运算: base^exponent */
    EXPR_TYPE_PRODUCT,  /**< 乘积: operand[0] * operand[1] * ... */
    EXPR_TYPE_SUM,      /**< 和: operand[0] + operand[1] + ... */
    EXPR_TYPE_FUNCTION  /**< 函数应用: f(args) */
} lvExprType;

/* ============== 表达式结构体 ============== */

typedef struct lvExpr {
    lvExprType type;
    union {
        /* EXPR_TYPE_POWER: base^exponent */
        struct {
            struct lvExpr *base;
            struct lvExpr *exponent;
        } power;
        /* EXPR_TYPE_PRODUCT / EXPR_TYPE_SUM: 不定长操作数列表 */
        struct {
            uint32_t count;
            struct lvExpr **operands;
        } composite;
        /* EXPR_TYPE_VARIABLE: 变量名 */
        struct {
            char *name;
        } variable;
        /* EXPR_TYPE_RATIONAL: 有理数值 */
        struct {
            mpq_t value;
        } rational;
        /* EXPR_TYPE_FUNCTION: 函数名和参数 */
        struct {
            char *func_name;
            struct lvExpr *argument;
        } function;
    } data;
    char *label; /**< 可选标签 */
} lvExpr;

/* ============== 表达式构造 ============== */

/**
 * @brief 创建变量表达式
 * @param name 变量名（内部复制）
 * @return 新表达式，失败返回 NULL
 */
lvExpr *lv_expr_create_variable(const char *name);

/**
 * @brief 创建有理数表达式
 * @param num 分子
 * @param den 分母（必须 > 0）
 * @return 新表达式，失败返回 NULL
 */
lvExpr *lv_expr_create_rational(int64_t num, uint64_t den);

/**
 * @brief 从 mpq_t 创建有理数表达式
 * @param value GMP 有理数（内部复制）
 * @return 新表达式，失败返回 NULL
 */
lvExpr *lv_expr_create_rational_mpq(const mpq_t value);

/**
 * @brief 创建幂表达式 base^exponent
 * @param base 底数
 * @param exponent 指数
 * @return 新表达式，失败返回 NULL
 */
lvExpr *lv_expr_power(lvExpr *base, lvExpr *exponent);

/**
 * @brief 创建乘积表达式
 * @param a 第一个因子
 * @param b 第二个因子
 * @return 新表达式，失败返回 NULL
 */
lvExpr *lv_expr_mul(lvExpr *a, lvExpr *b);

/**
 * @brief 创建加法表达式
 * @param a 被加数
 * @param b 加数
 * @return 新表达式，失败返回 NULL
 */
lvExpr *lv_expr_add(lvExpr *a, lvExpr *b);

/**
 * @brief 从表达式数组创建加法表达式（多参数版本）
 * @param exprs 表达式数组
 * @param count 数量
 * @return 新表达式（所有 exprs 的和），失败返回 NULL
 */
lvExpr *lv_expr_sum_n(lvExpr **exprs, uint32_t count);

/**
 * @brief 从表达式数组创建乘积表达式（多参数版本）
 * @param exprs 表达式数组
 * @param count 数量
 * @return 新表达式（所有 exprs 的乘积），失败返回 NULL
 */
lvExpr *lv_expr_product_n(lvExpr **exprs, uint32_t count);

/**
 * @brief 创建函数应用表达式 f(argument)
 * @param func_name 函数名
 * @param argument 参数
 * @return 新表达式，失败返回 NULL
 */
lvExpr *lv_expr_function(const char *func_name, lvExpr *argument);

/* ============== 表达式销毁/复制 ============== */

/**
 * @brief 销毁表达式
 * @param expr 表达式指针的地址（销毁后置 NULL）
 */
lv_PUBLIC_API void lv_expr_destroy(lvExpr **expr);

/* 兼容宏：旧的 lv_expr_free(x) 风格 */
#ifndef lv_expr_free
#define lv_expr_free(x) lv_expr_destroy(&(x))
#endif

/**
 * @brief 深拷贝表达式
 * @param expr 源表达式
 * @return 新表达式副本，失败返回 NULL
 */
lvExpr *lv_expr_copy(const lvExpr *expr);

/* ============== 表达式查询 ============== */

/**
 * @brief 检查表达式是否为常量（有理数）
 * @param expr 表达式
 * @return 是否为有理数常量
 */
lv_PUBLIC_API bool lv_expr_is_constant(const lvExpr *expr);

/**
 * @brief 获取有理数表达式的整数值（如果是有理数且分母为1）
 * @param expr 表达式
 * @param out_val 输出整数值
 * @return 是否成功
 */
lv_PUBLIC_API bool lv_expr_get_integer(const lvExpr *expr, int64_t *out_val);

#ifdef __cplusplus
}
#endif

#endif /* lv_EXPR_CANONICAL_H */
