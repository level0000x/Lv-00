/**
 * @file expr_canonical.h
 * @brief 表达式规范化系统 —— 纯整数符号运算
 *
 * @details 提供完全基于整数运算的表达式规范化系统：
 *   1. 多项式规范化：统一变量排序、合并同类项
 *   2. 有理表达式规范化：消除分母、通分
 *   3. 根式规范化：嵌套根式展开、有理化分母
 *   4. 三角恒等式规范化：基本恒等式应用
 *
 * 设计原则：
 *   - 所有数值计算使用 GMP 多精度整数/有理数
 *   - 禁止使用浮点运算（double/float）
 *   - 表达式规范化为唯一规范形式
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
#include <stddef.h>
#include <stdint.h>

/* ============== 前向声明 ============== */

typedef struct Lv00Expr Lv00Expr;
typedef struct Lv00PolyTerm Lv00PolyTerm;
typedef struct Lv00RationalExpr Lv00RationalExpr;
typedef struct Lv00RadicalExpr Lv00RadicalExpr;
typedef struct Lv00CanonicalContext Lv00CanonicalContext;

/* ============== 表达式类型 ============== */

/**
 * @brief 表达式类型枚举
 */
typedef enum {
    EXPR_TYPE_INTEGER,      /**< 整数常量 */
    EXPR_TYPE_RATIONAL,     /**< 有理数常量 */
    EXPR_TYPE_VARIABLE,     /**< 变量 */
    EXPR_TYPE_POLYNOMIAL,   /**< 多项式 */
    EXPR_TYPE_RATIONAL_EXPR,/**< 有理表达式（分式） */
    EXPR_TYPE_RADICAL,      /**< 根式 */
    EXPR_TYPE_SUM,          /**< 和 */
    EXPR_TYPE_PRODUCT,      /**< 积 */
    EXPR_TYPE_POWER,        /**< 幂 */
    EXPR_TYPE_FUNCTION,     /**< 函数调用 */
    EXPR_TYPE_INVALID       /**< 无效表达式 */
} Lv00ExprType;

/**
 * @brief 变量标识符
 */
typedef struct {
    uint32_t id;        /**< 变量 ID */
    char name[32];      /**< 变量名（可选） */
} Lv00VarId;

/**
 * @brief 单项式项：系数 * x1^e1 * x2^e2 * ...
 */
struct Lv00PolyTerm {
    mpq_t coeff;            /**< 有理数系数 */
    Lv00VarId *vars;        /**< 变量数组（按 ID 排序） */
    uint32_t *exponents;    /**< 指数数组 */
    uint32_t var_count;     /**< 变量数量 */
    uint32_t capacity;      /**< 数组容量 */
};

/**
 * @brief 多项式：单项式项的和
 */
typedef struct {
    Lv00PolyTerm *terms;    /**< 项数组（按字典序排序） */
    uint32_t term_count;    /**< 项数量 */
    uint32_t capacity;      /**< 数组容量 */
} Lv00Polynomial;

/**
 * @brief 有理表达式：分子/分母
 */
struct Lv00RationalExpr {
    Lv00Polynomial *numerator;   /**< 分子 */
    Lv00Polynomial *denominator; /**< 分母 */
};

/**
 * @brief 根式表达式：coeff * sqrt(radicand)
 */
struct Lv00RadicalExpr {
    mpq_t coeff;            /**< 有理数系数 */
    mpz_t radicand;         /**< 被开方数（整数） */
    uint32_t index;         /**< 根指数（2=平方根，3=立方根，...） */
};

/**
 * @brief 通用表达式结构
 */
struct Lv00Expr {
    Lv00ExprType type;
    union {
        mpz_t int_val;              /**< 整数值 */
        mpq_t rational_val;         /**< 有理数值 */
        Lv00VarId var;              /**< 变量 */
        Lv00Polynomial *poly;       /**< 多项式 */
        Lv00RationalExpr *rat_expr; /**< 有理表达式 */
        Lv00RadicalExpr *radical;   /**< 根式 */
        struct {                    /**< 和/积 */
            Lv00Expr **operands;
            uint32_t count;
            uint32_t capacity;
        } composite;
        struct {                    /**< 幂 */
            Lv00Expr *base;
            Lv00Expr *exponent;
        } power;
        struct {                    /**< 函数 */
            char name[32];
            Lv00Expr **args;
            uint32_t arg_count;
        } func;
    } data;
};

/* ============== 规范化上下文 ============== */

/**
 * @brief 规范化选项
 */
typedef struct {
    bool expand_products;       /**< 展开乘积 */
    bool merge_like_terms;      /**< 合并同类项 */
    bool order_terms;           /**< 项排序（字典序） */
    bool rationalize_denom;     /**< 分母有理化 */
    bool expand_nested_radicals;/**< 展开嵌套根式 */
    bool simplify_fractions;    /**< 约分 */
    int max_recursion_depth;    /**< 最大递归深度 */
} Lv00CanonicalOptions;

/**
 * @brief 规范化上下文
 */
struct Lv00CanonicalContext {
    Lv00CanonicalOptions options;
    uint32_t next_var_id;       /**< 下一个变量 ID */
    int recursion_depth;        /**< 当前递归深度 */
};

/* ============== 多项式操作 ============== */

/**
 * @brief 创建空多项式
 * @return 新多项式，失败返回 NULL
 */
Lv00Polynomial *lv00_poly_create(void);

/**
 * @brief 销毁多项式
 * @param poly 多项式指针
 */
void lv00_poly_destroy(Lv00Polynomial *poly);

/**
 * @brief 复制多项式
 * @param src 源多项式
 * @return 新多项式副本
 */
Lv00Polynomial *lv00_poly_copy(const Lv00Polynomial *src);

/**
 * @brief 添加单项式项
 * @param poly 多项式
 * @param coeff 系数（会被复制）
 * @param vars 变量数组（可为 NULL）
 * @param exponents 指数数组（可为 NULL）
 * @param var_count 变量数量
 * @return 是否成功
 */
bool lv00_poly_add_term(Lv00Polynomial *poly, const mpq_t coeff,
                        const Lv00VarId *vars, const uint32_t *exponents,
                        uint32_t var_count);

/**
 * @brief 合并同类项
 * @param poly 多项式（原地修改）
 */
void lv00_poly_merge_like_terms(Lv00Polynomial *poly);

/**
 * @brief 按字典序排序项
 * @param poly 多项式（原地修改）
 */
void lv00_poly_order_terms(Lv00Polynomial *poly);

/**
 * @brief 规范化多项式（合并 + 排序）
 * @param poly 多项式（原地修改）
 */
void lv00_poly_normalize(Lv00Polynomial *poly);

/**
 * @brief 多项式加法
 * @param a 第一个多项式
 * @param b 第二个多项式
 * @return a + b
 */
Lv00Polynomial *lv00_poly_add(const Lv00Polynomial *a, const Lv00Polynomial *b);

/**
 * @brief 多项式减法
 * @param a 第一个多项式
 * @param b 第二个多项式
 * @return a - b
 */
Lv00Polynomial *lv00_poly_sub(const Lv00Polynomial *a, const Lv00Polynomial *b);

/**
 * @brief 多项式乘法
 * @param a 第一个多项式
 * @param b 第二个多项式
 * @return a * b
 */
Lv00Polynomial *lv00_poly_mul(const Lv00Polynomial *a, const Lv00Polynomial *b);

/**
 * @brief 多项式除法（带余）
 * @param a 被除数
 * @param b 除数
 * @param out_quotient 输出商（可为 NULL）
 * @param out_remainder 输出余数（可为 NULL）
 * @return 是否整除
 */
bool lv00_poly_div(const Lv00Polynomial *a, const Lv00Polynomial *b,
                   Lv00Polynomial **out_quotient, Lv00Polynomial **out_remainder);

/**
 * @brief 计算多项式次数
 * @param poly 多项式
 * @param var_id 变量 ID（0 表示总次数）
 * @return 次数
 */
uint32_t lv00_poly_degree(const Lv00Polynomial *poly, Lv00VarId var_id);

/**
 * @brief 提取主系数
 * @param poly 多项式
 * @param var_id 主变量
 * @param out_coeff 输出系数
 * @return 是否成功
 */
bool lv00_poly_leading_coeff(const Lv00Polynomial *poly, Lv00VarId var_id, mpq_t out_coeff);

/**
 * @brief 多项式求值（整数点）
 * @param poly 多项式
 * @param var_values 变量值数组（按变量 ID 索引）
 * @param var_count 变量数量
 * @param result 输出结果
 * @return 是否成功
 */
bool lv00_poly_eval_int(const Lv00Polynomial *poly, const mpz_t *var_values,
                        uint32_t var_count, mpq_t result);

/**
 * @brief 多项式序列化为字符串
 * @param poly 多项式
 * @param var_names 变量名数组（可为 NULL）
 * @return 字符串（调用者负责释放）
 */
char *lv00_poly_to_string(const Lv00Polynomial *poly, const char **var_names);

/* ============== 有理表达式操作 ============== */

/**
 * @brief 创建有理表达式
 * @param numerator 分子
 * @param denominator 分母
 * @return 新有理表达式
 */
Lv00RationalExpr *lv00_rat_expr_create(Lv00Polynomial *numerator,
                                        Lv00Polynomial *denominator);

/**
 * @brief 销毁有理表达式
 * @param expr 有理表达式
 */
void lv00_rat_expr_destroy(Lv00RationalExpr *expr);

/**
 * @brief 有理表达式约分
 * @param expr 有理表达式（原地修改）
 */
void lv00_rat_expr_simplify(Lv00RationalExpr *expr);

/**
 * @brief 有理表达式加法
 * @param a 第一个表达式
 * @param b 第二个表达式
 * @return a + b
 */
Lv00RationalExpr *lv00_rat_expr_add(const Lv00RationalExpr *a,
                                     const Lv00RationalExpr *b);

/**
 * @brief 有理表达式乘法
 * @param a 第一个表达式
 * @param b 第二个表达式
 * @return a * b
 */
Lv00RationalExpr *lv00_rat_expr_mul(const Lv00RationalExpr *a,
                                     const Lv00RationalExpr *b);

/* ============== 根式操作 ============== */

/**
 * @brief 创建根式
 * @param coeff 系数
 * @param radicand 被开方数
 * @param index 根指数
 * @return 新根式
 */
Lv00RadicalExpr *lv00_radical_create(const mpq_t coeff, const mpz_t radicand,
                                      uint32_t index);

/**
 * @brief 销毁根式
 * @param rad 根式
 */
void lv00_radical_destroy(Lv00RadicalExpr *rad);

/**
 * @brief 尝试展开嵌套根式
 *
 * 检查 sqrt(a + b*sqrt(c)) 是否可展开为 sqrt(p) + sqrt(q) 形式。
 * 条件：a^2 - b^2*c 必须为完全平方数。
 *
 * @param rad 根式
 * @param out_expanded 输出展开后的表达式（成功时）
 * @return 是否可以展开
 */
bool lv00_radical_try_expand(const Lv00RadicalExpr *rad, Lv00Expr **out_expanded);

/**
 * @brief 根式有理化
 *
 * 将 a + b*sqrt(n) 的倒数有理化：
 * 1/(a + b*sqrt(n)) = (a - b*sqrt(n)) / (a^2 - b^2*n)
 *
 * @param rad 根式
 * @return 有理化后的表达式
 */
Lv00RationalExpr *lv00_radical_rationalize(const Lv00RadicalExpr *rad);

/**
 * @brief 检查是否为完全平方数
 * @param n 整数
 * @param out_root 输出平方根（成功时）
 * @return 是否为完全平方数
 */
bool lv00_is_perfect_square(const mpz_t n, mpz_t out_root);

/**
 * @brief 检查是否为完全立方数
 * @param n 整数
 * @param out_root 输出立方根（成功时）
 * @return 是否为完全立方数
 */
bool lv00_is_perfect_cube(const mpz_t n, mpz_t out_root);

/* ============== 通用表达式操作 ============== */

/**
 * @brief 创建整数表达式
 * @param val 整数值
 * @return 新表达式
 */
Lv00Expr *lv00_expr_create_int(const mpz_t val);

/**
 * @brief 创建有理数表达式
 * @param val 有理数值
 * @return 新表达式
 */
Lv00Expr *lv00_expr_create_rational(const mpq_t val);

/**
 * @brief 创建变量表达式
 * @param var_id 变量 ID
 * @param name 变量名（可为 NULL）
 * @return 新表达式
 */
Lv00Expr *lv00_expr_create_var(Lv00VarId var_id, const char *name);

/**
 * @brief 创建和表达式
 * @param operands 操作数数组
 * @param count 操作数数量
 * @return 新表达式
 */
Lv00Expr *lv00_expr_create_sum(Lv00Expr **operands, uint32_t count);

/**
 * @brief 创建积表达式
 * @param operands 操作数数组
 * @param count 操作数数量
 * @return 新表达式
 */
Lv00Expr *lv00_expr_create_product(Lv00Expr **operands, uint32_t count);

/**
 * @brief 销毁表达式
 * @param expr 表达式
 */
void lv00_expr_destroy(Lv00Expr *expr);

/**
 * @brief 复制表达式
 * @param src 源表达式
 * @return 新表达式副本
 */
Lv00Expr *lv00_expr_copy(const Lv00Expr *src);

/**
 * @brief 规范化表达式
 * @param expr 表达式（原地修改）
 * @param ctx 规范化上下文
 * @return 是否成功
 */
bool lv00_expr_normalize(Lv00Expr *expr, Lv00CanonicalContext *ctx);

/**
 * @brief 比较两个表达式是否等价
 * @param a 第一个表达式
 * @param b 第二个表达式
 * @return 是否等价
 */
bool lv00_expr_equivalent(const Lv00Expr *a, const Lv00Expr *b);

/**
 * @brief 表达式序列化为字符串
 * @param expr 表达式
 * @return 字符串（调用者负责释放）
 */
char *lv00_expr_to_string(const Lv00Expr *expr);

/* ============== 规范化上下文操作 ============== */

/**
 * @brief 创建规范化上下文（默认选项）
 * @return 新上下文
 */
Lv00CanonicalContext *lv00_canonical_ctx_create(void);

/**
 * @brief 销毁规范化上下文
 * @param ctx 上下文
 */
void lv00_canonical_ctx_destroy(Lv00CanonicalContext *ctx);

/**
 * @brief 获取默认规范化选项
 * @param options 输出选项
 */
void lv00_get_default_options(Lv00CanonicalOptions *options);

/**
 * @brief 分配新变量 ID
 * @param ctx 上下文
 * @return 新变量 ID
 */
Lv00VarId lv00_alloc_var_id(Lv00CanonicalContext *ctx);

/* ============== Groebner 基相关 ============== */

/**
 * @brief 计算理想基的 Groebner 基
 *
 * 使用 Buchberger 算法计算多项式理想的 Groebner 基。
 * 仅处理度数 <= 2 的多项式系统。
 *
 * @param polys 多项式数组
 * @param poly_count 多项式数量
 * @param out_basis 输出 Groebner 基数组
 * @param out_basis_count 输出基元素数量
 * @return 是否成功
 */
bool lv00_compute_groebner_basis(Lv00Polynomial **polys, uint32_t poly_count,
                                  Lv00Polynomial ***out_basis, uint32_t *out_basis_count);

/**
 * @brief 计算 S-多项式
 * @param f 第一个多项式
 * @param g 第二个多项式
 * @return S(f, g)
 */
Lv00Polynomial *lv00_s_polynomial(const Lv00Polynomial *f, const Lv00Polynomial *g);

/**
 * @brief 多项式约化
 * @param f 被约化多项式
 * @param g 约化多项式
 * @return f mod g
 */
Lv00Polynomial *lv00_poly_reduce(const Lv00Polynomial *f, const Lv00Polynomial *g);

/* ============== 连分数近似（纯整数实现） ============== */

/**
 * @brief 连分数近似
 *
 * 使用连分数算法将有理数近似为更简单的有理数。
 * 完全基于整数运算，不使用浮点数。
 *
 * @param num 分子
 * @param denom 分母
 * @param max_denom 最大允许分母
 * @param out_num 输出近似分子
 * @param out_denom 输出近似分母
 * @return 是否成功
 */
bool lv00_continued_fraction_approx(const mpz_t num, const mpz_t denom,
                                     mpz_t max_denom, mpz_t out_num, mpz_t out_denom);

/**
 * @brief 最佳有理近似
 *
 * 找到分母不超过 max_denom 的最佳有理近似。
 * 使用 Stern-Brocot 树搜索，完全基于整数运算。
 *
 * @param num 分子
 * @param denom 分母
 * @param max_denom 最大允许分母
 * @param out_num 输出近似分子
 * @param out_denom 输出近似分母
 */
void lv00_best_rational_approx(const mpz_t num, const mpz_t denom,
                                const mpz_t max_denom, mpz_t out_num, mpz_t out_denom);

#ifdef __cplusplus
}
#endif

#endif /* LV00_EXPR_CANONICAL_H */
