/**
 * @file inequality_reasoning.h
 * @brief 不等式推理系统 —— 纯符号不等式证明
 *
 * @details 提供完全基于符号计算的不等式推理：
 *   1. 基本不等式：AM-GM、Cauchy-Schwarz、排序不等式
 *   2. 几何不等式：三角形不等式、面积不等式
 *   3. 代数不等式：多项式正定性、Schur 不等式
 *   4. 不等式链：传递性、同向合并
 *
 * 设计原则：
 *   - 所有数值计算使用 GMP 多精度有理数
 *   - 禁止使用浮点运算
 *   - 支持不等式的符号证明
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#ifndef LV00_INEQUALITY_REASONING_H
#define LV00_INEQUALITY_REASONING_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv00.h"           /* 必须先包含以获取 LV00_PUBLIC_API 定义 */
#include <gmp.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "expr_canonical.h"
#include "symbolic_coord.h"

/* ============== 前向声明 ============== */

typedef struct Lv00Inequality Lv00Inequality;
typedef struct Lv00InequalityProof Lv00InequalityProof;
typedef struct Lv00InequalitySystem Lv00InequalitySystem;

/* ============== 不等式类型 ============== */

/**
 * @brief 不等式关系类型
 */
typedef enum {
    INEQ_LESS_THAN,         /**< < */
    INEQ_LESS_EQUAL,        /**< ≤ */
    INEQ_GREATER_THAN,      /**< > */
    INEQ_GREATER_EQUAL,     /**< ≥ */
    INEQ_NOT_EQUAL          /**< ≠ */
} Lv00InequalityType;

/**
 * @brief 不等式证明状态
 */
typedef enum {
    INEQ_STATUS_UNPROVED,       /**< 未证明 */
    INEQ_STATUS_PROVED,         /**< 已证明 */
    INEQ_STATUS_DISPROVED,      /**< 已证伪 */
    INEQ_STATUS_CONDITIONAL,    /**< 条件成立 */
    INEQ_STATUS_UNKNOWN         /**< 无法判定 */
} Lv00InequalityStatus;

/**
 * @brief 不等式证明方法
 */
typedef enum {
    INEQ_METHOD_DIRECT,         /**< 直接证明 */
    INEQ_METHOD_AM_GM,          /**< AM-GM 不等式 */
    INEQ_METHOD_CAUCHY,         /**< Cauchy-Schwarz 不等式 */
    INEQ_METHOD_REARRANGEMENT,  /**< 排序不等式 */
    INEQ_METHOD_SCHUR,          /**< Schur 不等式 */
    INEQ_METHOD_JENSEN,         /**< Jensen 不等式 */
    INEQ_METHOD_HOLDER,         /**< Hölder 不等式 */
    INEQ_METHOD_MINKOWSKI,      /**< Minkowski 不等式 */
    INEQ_METHOD_TRIANGLE,       /**< 三角形不等式 */
    INEQ_METHOD_SUBSTITUTION,   /**< 变量替换 */
    INEQ_METHOD_INDUCTION,      /**< 数学归纳法 */
    INEQ_METHOD_CONTRADICTION,  /**< 反证法 */
    INEQ_METHOD_SMART_SUM,      /**< 智能求和（Muirhead） */
    INEQ_METHOD_SOS             /**< 平方和分解 */
} Lv00InequalityMethod;

/**
 * @brief 不等式结构
 */
struct Lv00Inequality {
    Lv00Expr *left;             /**< 左边表达式 */
    Lv00Expr *right;            /**< 右边表达式 */
    Lv00InequalityType type;    /**< 不等式类型 */
    Lv00InequalityStatus status;/**< 证明状态 */
    char *label;                /**< 标签（可选） */
};

/**
 * @brief 不等式证明步骤
 */
typedef struct {
    Lv00InequalityMethod method;    /**< 使用的方法 */
    Lv00Inequality *ineq;           /**< 此步骤的不等式 */
    char *justification;            /**< 证明理由 */
    int *premise_ids;               /**< 前提步骤 ID */
    int premise_count;              /**< 前提数量 */
} Lv00InequalityStep;

/**
 * @brief 不等式证明
 */
struct Lv00InequalityProof {
    Lv00Inequality *target;         /**< 目标不等式 */
    Lv00InequalityStep *steps;      /**< 证明步骤数组 */
    int step_count;                 /**< 步骤数量 */
    int step_capacity;              /**< 步骤容量 */
    Lv00InequalityStatus status;    /**< 最终状态 */
    char *error_message;            /**< 错误消息（失败时） */
};

/**
 * @brief 不等式系统
 */
struct Lv00InequalitySystem {
    Lv00Inequality **inequalities;  /**< 不等式数组 */
    uint32_t count;                 /**< 不等式数量 */
    uint32_t capacity;              /**< 数组容量 */
    Lv00Expr **variables;           /**< 变量数组 */
    uint32_t var_count;             /**< 变量数量 */
};

/* ============== 不等式创建/销毁 ============== */

/**
 * @brief 创建不等式
 * @param left 左边表达式
 * @param type 不等式类型
 * @param right 右边表达式
 * @return 新不等式
 */
Lv00Inequality *lv00_ineq_create(Lv00Expr *left, Lv00InequalityType type, Lv00Expr *right);

/**
 * @brief 销毁不等式
 * @param ineq 不等式指针
 */
void lv00_ineq_destroy(Lv00Inequality *ineq);

/**
 * @brief 复制不等式
 * @param ineq 源不等式
 * @return 新不等式副本
 */
Lv00Inequality *lv00_ineq_copy(const Lv00Inequality *ineq);

/**
 * @brief 创建不等式系统
 * @return 新系统
 */
Lv00InequalitySystem *lv00_ineq_system_create(void);

/**
 * @brief 销毁不等式系统
 * @param sys 系统指针
 */
void lv00_ineq_system_destroy(Lv00InequalitySystem *sys);

/**
 * @brief 添加不等式到系统
 * @param sys 系统
 * @param ineq 不等式
 * @return 是否成功
 */
bool lv00_ineq_system_add(Lv00InequalitySystem *sys, Lv00Inequality *ineq);

/**
 * @brief 添加变量约束（如 x > 0）
 * @param sys 系统
 * @param var 变量表达式
 * @param type 约束类型
 * @param value 约束值（通常为 0）
 * @return 是否成功
 */
bool lv00_ineq_system_add_var_constraint(Lv00InequalitySystem *sys,
                                          Lv00Expr *var,
                                          Lv00InequalityType type,
                                          const mpq_t value);

/* ============== 基本不等式证明 ============== */

/**
 * @brief 证明不等式
 * @param ineq 不等式
 * @param sys 不等式系统（提供前提条件）
 * @param proof 输出证明（可为 NULL）
 * @return 证明状态
 */
Lv00InequalityStatus lv00_ineq_prove(Lv00Inequality *ineq,
                                      const Lv00InequalitySystem *sys,
                                      Lv00InequalityProof **proof);

/**
 * @brief 销毁证明
 * @param proof 证明指针
 */
void lv00_ineq_proof_destroy(Lv00InequalityProof *proof);

/**
 * @brief 尝试使用指定方法证明不等式
 * @param ineq 不等式
 * @param method 证明方法
 * @param sys 不等式系统
 * @param proof 输出证明
 * @return 证明状态
 */
Lv00InequalityStatus lv00_ineq_prove_with_method(Lv00Inequality *ineq,
                                                  Lv00InequalityMethod method,
                                                  const Lv00InequalitySystem *sys,
                                                  Lv00InequalityProof **proof);

/* ============== 经典不等式 ============== */

/**
 * @brief 应用 AM-GM 不等式
 *
 * 对于非负实数 a1, a2, ..., an：
 * (a1 + a2 + ... + an) / n ≥ (a1 * a2 * ... * an)^(1/n)
 *
 * @param expressions 表达式数组
 * @param count 表达式数量
 * @param out_lower_bound 输出下界（算术平均）
 * @param out_upper_bound 输出上界（几何平均）
 * @return 是否成功
 */
bool lv00_ineq_am_gm(Lv00Expr **expressions, uint32_t count,
                     Lv00Expr **out_lower_bound, Lv00Expr **out_upper_bound);

/**
 * @brief 应用 Cauchy-Schwarz 不等式
 *
 * (∑ ai²)(∑ bi²) ≥ (∑ ai*bi)²
 *
 * @param a 第一个向量（表达式数组）
 * @param b 第二个向量
 * @param count 向量维度
 * @param out_ineq 输出不等式
 * @return 是否成功
 */
bool lv00_ineq_cauchy_schwarz(Lv00Expr **a, Lv00Expr **b, uint32_t count,
                               Lv00Inequality **out_ineq);

/**
 * @brief 应用排序不等式
 *
 * 对于两个递增序列 a1 ≤ a2 ≤ ... ≤ an 和 b1 ≤ b2 ≤ ... ≤ bn：
 * ∑ ai*b(n-i+1) ≤ ∑ ai*bσ(i) ≤ ∑ ai*bi
 *
 * @param a 第一个序列
 * @param b 第二个序列
 * @param count 序列长度
 * @param out_min 输出最小和
 * @param out_max 输出最大和
 * @return 是否成功
 */
bool lv00_ineq_rearrangement(Lv00Expr **a, Lv00Expr **b, uint32_t count,
                              Lv00Expr **out_min, Lv00Expr **out_max);

/**
 * @brief 应用 Schur 不等式
 *
 * 对于非负实数 a, b, c 和 r ≥ 0：
 * a^r(a-b)(a-c) + b^r(b-c)(b-a) + c^r(c-a)(c-b) ≥ 0
 *
 * @param a 第一个数
 * @param b 第二个数
 * @param c 第三个数
 * @param r 指数
 * @param out_ineq 输出不等式
 * @return 是否成功
 */
bool lv00_ineq_schur(Lv00Expr *a, Lv00Expr *b, Lv00Expr *c, uint32_t r,
                     Lv00Inequality **out_ineq);

/**
 * @brief 应用 Jensen 不等式
 *
 * 对于凸函数 f：f(∑ wi*xi) ≤ ∑ wi*f(xi)
 *
 * @param func 函数名
 * @param points 点数组
 * @param weights 权重数组（可为 NULL，表示等权重）
 * @param count 点数量
 * @param is_convex 函数是否为凸函数
 * @param out_ineq 输出不等式
 * @return 是否成功
 */
bool lv00_ineq_jensen(const char *func, Lv00Expr **points, mpq_t *weights,
                       uint32_t count, bool is_convex, Lv00Inequality **out_ineq);

/**
 * @brief 应用三角形不等式
 *
 * 对于三角形三边 a, b, c：
 * |a - b| < c < a + b
 *
 * @param a 边 a
 * @param b 边 b
 * @param c 边 c
 * @param out_inequalities 输出不等式数组
 * @param max_count 最大输出数量
 * @return 实际输出数量
 */
uint32_t lv00_ineq_triangle(Lv00Expr *a, Lv00Expr *b, Lv00Expr *c,
                             Lv00Inequality **out_inequalities, uint32_t max_count);

/* ============== 不等式变换 ============== */

/**
 * @brief 不等式两边加表达式
 * @param ineq 不等式
 * @param expr 表达式
 * @return 新不等式
 */
Lv00Inequality *lv00_ineq_add(Lv00Inequality *ineq, Lv00Expr *expr);

/**
 * @brief 不等式两边乘表达式
 * @param ineq 不等式
 * @param expr 表达式
 * @param expr_sign 表达式符号（正/负/未知）
 * @return 新不等式
 */
Lv00Inequality *lv00_ineq_mul(Lv00Inequality *ineq, Lv00Expr *expr, int expr_sign);

/**
 * @brief 不等式取反
 * @param ineq 不等式
 * @return 新不等式
 */
Lv00Inequality *lv00_ineq_negate(Lv00Inequality *ineq);

/**
 * @brief 不等式链传递
 * @param ineqs 不等式数组（按顺序）
 * @param count 不等式数量
 * @param out_result 输出结果不等式
 * @return 是否可以传递
 */
bool lv00_ineq_transitive(Lv00Inequality **ineqs, uint32_t count,
                          Lv00Inequality **out_result);

/**
 * @brief 合并同向不等式
 * @param ineqs 不等式数组
 * @param count 不等式数量
 * @param out_result 输出结果不等式
 * @return 是否可以合并
 */
bool lv00_ineq_merge(Lv00Inequality **ineqs, uint32_t count,
                     Lv00Inequality **out_result);

/* ============== 表达式符号判定 ============== */

/**
 * @brief 表达式符号判定结果
 */
typedef enum {
    SIGN_POSITIVE,      /**< 正数 */
    SIGN_NEGATIVE,      /**< 负数 */
    SIGN_ZERO,          /**< 零 */
    SIGN_NONNEGATIVE,   /**< 非负 */
    SIGN_NONPOSITIVE,   /**< 非正 */
    SIGN_UNKNOWN        /**< 未知 */
} Lv00Sign;

/**
 * @brief 判定表达式符号
 * @param expr 表达式
 * @param sys 不等式系统（提供变量约束）
 * @return 符号判定结果
 */
Lv00Sign lv00_expr_sign(Lv00Expr *expr, const Lv00InequalitySystem *sys);

/**
 * @brief 检查表达式是否为正
 * @param expr 表达式
 * @param sys 不等式系统
 * @return 是否为正
 */
bool lv00_expr_is_positive(Lv00Expr *expr, const Lv00InequalitySystem *sys);

/**
 * @brief 检查表达式是否为非负
 * @param expr 表达式
 * @param sys 不等式系统
 * @return 是否为非负
 */
bool lv00_expr_is_nonnegative(Lv00Expr *expr, const Lv00InequalitySystem *sys);

/* ============== 平方和分解 ============== */

/**
 * @brief 平方和分解结果
 */
typedef struct {
    Lv00Expr **squares;     /**< 平方项数组 */
    uint32_t count;         /**< 平方项数量 */
    Lv00Expr *remainder;    /**< 余项（可为 NULL） */
} Lv00SOSDecomposition;

/**
 * @brief 尝试平方和分解
 *
 * 将多项式分解为平方和形式。
 * 如果成功，则多项式非负。
 *
 * @param poly 多项式
 * @param out_sos 输出分解结果
 * @return 是否成功分解
 */
bool lv00_expr_sos_decompose(Lv00Expr *poly, Lv00SOSDecomposition **out_sos);

/**
 * @brief 销毁平方和分解
 * @param sos 分解结果
 */
void lv00_sos_destroy(Lv00SOSDecomposition *sos);

/* ============== 几何不等式 ============== */

/**
 * @brief 三角形面积不等式
 *
 * 对于三角形 ABC，面积 S：
 * S ≤ (1/4) * √3 * a²（等边三角形最大）
 *
 * @param a 边 a
 * @param b 边 b
 * @param c 边 c
 * @param area 面积
 * @param out_ineq 输出不等式
 * @return 是否成功
 */
bool lv00_ineq_triangle_area(Lv00Expr *a, Lv00Expr *b, Lv00Expr *c,
                              Lv00Expr *area, Lv00Inequality **out_ineq);

/**
 * @brief Weitzenböck 不等式
 *
 * 对于三角形三边 a, b, c 和面积 S：
 * a² + b² + c² ≥ 4√3 * S
 *
 * @param a 边 a
 * @param b 边 b
 * @param c 边 c
 * @param area 面积
 * @param out_ineq 输出不等式
 * @return 是否成功
 */
bool lv00_ineq_weitzenbock(Lv00Expr *a, Lv00Expr *b, Lv00Expr *c,
                            Lv00Expr *area, Lv00Inequality **out_ineq);

/**
 * @brief Erdős–Mordell 不等式
 *
 * 对于三角形 ABC 内点 P，设 P 到三边距离为 p, q, r：
 * PA + PB + PC ≥ 2(p + q + r)
 *
 * @param pa PA 距离
 * @param pb PB 距离
 * @param pc PC 距离
 * @param p 到边 a 距离
 * @param q 到边 b 距离
 * @param r 到边 c 距离
 * @param out_ineq 输出不等式
 * @return 是否成功
 */
bool lv00_ineq_erdos_mordell(Lv00Expr *pa, Lv00Expr *pb, Lv00Expr *pc,
                              Lv00Expr *p, Lv00Expr *q, Lv00Expr *r,
                              Lv00Inequality **out_ineq);

/* ============== 不等式序列化 ============== */

/**
 * @brief 不等式序列化为字符串
 * @param ineq 不等式
 * @return 字符串（调用者负责释放）
 */
char *lv00_ineq_to_string(const Lv00Inequality *ineq);

/**
 * @brief 证明序列化为字符串
 * @param proof 证明
 * @return 字符串（调用者负责释放）
 */
char *lv00_ineq_proof_to_string(const Lv00InequalityProof *proof);

/**
 * @brief 证明导出为 LaTeX
 * @param proof 证明
 * @return LaTeX 字符串（调用者负责释放）
 */
char *lv00_ineq_proof_to_latex(const Lv00InequalityProof *proof);

#ifdef __cplusplus
}
#endif

#endif /* LV00_INEQUALITY_REASONING_H */
