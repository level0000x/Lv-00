/**
 * @file expr_canonical.h
 * @brief 符号表达式规范表示 —— Lv00Expr 类型定义与构造/操作 API
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
 * @version 3.3.0
 */

#ifndef LV00_EXPR_CANONICAL_H
#define LV00_EXPR_CANONICAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <gmp.h>
#include <stdbool.h>
#include <stdint.h>

/* ============== 表达式类型枚举 ============== */

typedef enum {
    EXPR_TYPE_VARIABLE,     /**< 变量，如 x, y, a */
    EXPR_TYPE_RATIONAL,     /**< 有理数常量，如 3/4 */
    EXPR_TYPE_POWER,        /**< 幂运算: base^exponent */
    EXPR_TYPE_PRODUCT,      /**< 乘积: operand[0] * operand[1] * ... */
    EXPR_TYPE_SUM,          /**< 和: operand[0] + operand[1] + ... */
    EXPR_TYPE_FUNCTION      /**< 函数应用: f(args) */
} Lv00ExprType;

/* ============== 表达式结构体 ============== */

typedef struct Lv00Expr {
    Lv00ExprType type;
    union {
        /* EXPR_TYPE_POWER: base^exponent */
        struct {
            struct Lv00Expr *base;
            struct Lv00Expr *exponent;
        } power;
        /* EXPR_TYPE_PRODUCT / EXPR_TYPE_SUM: 不定长操作数列表 */
        struct {
            uint32_t count;
            struct Lv00Expr **operands;
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
            struct Lv00Expr *argument;
        } function;
    } data;
    char *label;  /**< 可选标签 */
} Lv00Expr;

/* ============== 表达式构造 ============== */

/**
 * @brief 创建变量表达式
 * @param name 变量名（内部复制）
 * @return 新表达式，失败返回 NULL
 */
Lv00Expr *lv00_expr_create_variable(const char *name);

/**
 * @brief 创建有理数表达式
 * @param num 分子
 * @param den 分母（必须 > 0）
 * @return 新表达式，失败返回 NULL
 */
Lv00Expr *lv00_expr_create_rational(int64_t num, uint64_t den);

/**
 * @brief 从 mpq_t 创建有理数表达式
 * @param value GMP 有理数（内部复制）
 * @return 新表达式，失败返回 NULL
 */
Lv00Expr *lv00_expr_create_rational_mpq(const mpq_t value);

/**
 * @brief 创建幂表达式 base^exponent
 * @param base 底数
 * @param exponent 指数
 * @return 新表达式，失败返回 NULL
 */
Lv00Expr *lv00_expr_power(Lv00Expr *base, Lv00Expr *exponent);

/**
 * @brief 创建乘积表达式
 * @param a 第一个因子
 * @param b 第二个因子
 * @return 新表达式，失败返回 NULL
 */
Lv00Expr *lv00_expr_mul(Lv00Expr *a, Lv00Expr *b);

/**
 * @brief 创建加法表达式
 * @param a 被加数
 * @param b 加数
 * @return 新表达式，失败返回 NULL
 */
Lv00Expr *lv00_expr_add(Lv00Expr *a, Lv00Expr *b);

/**
 * @brief 从表达式数组创建加法表达式（多参数版本）
 * @param exprs 表达式数组
 * @param count 数量
 * @return 新表达式（所有 exprs 的和），失败返回 NULL
 */
Lv00Expr *lv00_expr_sum_n(Lv00Expr **exprs, uint32_t count);

/**
 * @brief 从表达式数组创建乘积表达式（多参数版本）
 * @param exprs 表达式数组
 * @param count 数量
 * @return 新表达式（所有 exprs 的乘积），失败返回 NULL
 */
Lv00Expr *lv00_expr_product_n(Lv00Expr **exprs, uint32_t count);

/**
 * @brief 创建函数应用表达式 f(argument)
 * @param func_name 函数名
 * @param argument 参数
 * @return 新表达式，失败返回 NULL
 */
Lv00Expr *lv00_expr_function(const char *func_name, Lv00Expr *argument);

/* ============== 表达式销毁/复制 ============== */

/**
 * @brief 销毁表达式
 * @param expr 表达式指针的地址（销毁后置 NULL）
 */
void lv00_expr_destroy(Lv00Expr **expr);

/**
 * @brief 深拷贝表达式
 * @param expr 源表达式
 * @return 新表达式副本，失败返回 NULL
 */
Lv00Expr *lv00_expr_copy(const Lv00Expr *expr);

/* ============== 表达式查询 ============== */

/**
 * @brief 检查表达式是否为常量（有理数）
 * @param expr 表达式
 * @return 是否为有理数常量
 */
bool lv00_expr_is_constant(const Lv00Expr *expr);

/**
 * @brief 获取有理数表达式的整数值（如果是有理数且分母为1）
 * @param expr 表达式
 * @param out_val 输出整数值
 * @return 是否成功
 */
bool lv00_expr_get_integer(const Lv00Expr *expr, int64_t *out_val);

#ifdef __cplusplus
}
#endif

#endif /* LV00_EXPR_CANONICAL_H */
