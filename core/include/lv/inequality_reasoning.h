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
 * @version 1.1.0
 */

#ifndef lv_INEQUALITY_REASONING_H
#define lv_INEQUALITY_REASONING_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include <gmp.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "expr_canonical.h"
#include "symbolic_coord.h"
#include "lv/lv_utils.h"

/* ============== 前向声明 ============== */

typedef struct lvInequality lvInequality;
typedef struct lvInequalityProof lvInequalityProof;
typedef struct lvInequalitySystem lvInequalitySystem;

/* ============== 不等式类型 ============== */

/**
 * @brief 不等式关系类型
 */
typedef enum {
    INEQ_LESS_THAN,     /**< < */
    INEQ_LESS_EQUAL,    /**< ≤ */
    INEQ_GREATER_THAN,  /**< > */
    INEQ_GREATER_EQUAL, /**< ≥ */
    INEQ_NOT_EQUAL      /**< ≠ */
} lvInequalityType;

/**
 * @brief 不等式证明状态
 */
typedef enum {
    INEQ_STATUS_UNPROVED,    /**< 未证明 */
    INEQ_STATUS_PROVED,      /**< 已证明 */
    INEQ_STATUS_DISPROVED,   /**< 已证伪 */
    INEQ_STATUS_CONDITIONAL, /**< 条件成立 */
    INEQ_STATUS_UNKNOWN      /**< 无法判定 */
} lvInequalityStatus;

/**
 * @brief 不等式证明方法
 */
typedef enum {
    INEQ_METHOD_DIRECT,        /**< 直接证明 */
    INEQ_METHOD_AM_GM,         /**< AM-GM 不等式 */
    INEQ_METHOD_CAUCHY,        /**< Cauchy-Schwarz 不等式 */
    INEQ_METHOD_REARRANGEMENT, /**< 排序不等式 */
    INEQ_METHOD_SCHUR,         /**< Schur 不等式 */
    INEQ_METHOD_JENSEN,        /**< Jensen 不等式 */
    INEQ_METHOD_HOLDER,        /**< Hölder 不等式 */
    INEQ_METHOD_MINKOWSKI,     /**< Minkowski 不等式 */
    INEQ_METHOD_TRIANGLE,      /**< 三角形不等式 */
    INEQ_METHOD_SUBSTITUTION,  /**< 变量替换 */
    INEQ_METHOD_INDUCTION,     /**< 数学归纳法 */
    INEQ_METHOD_CONTRADICTION, /**< 反证法 */
    INEQ_METHOD_SMART_SUM,     /**< 智能求和（Muirhead） */
    INEQ_METHOD_SOS            /**< 平方和分解 */
} lvInequalityMethod;

/**
 * @brief 不等式结构
 */
struct lvInequality {
    lvExpr *left;              /**< 左边表达式 */
    lvExpr *right;             /**< 右边表达式 */
    lvInequalityType type;     /**< 不等式类型 */
    lvInequalityStatus status; /**< 证明状态 */
    char *label;               /**< 标签（可选） */
};

/**
 * @brief 不等式证明步骤
 */
typedef struct {
    lvInequalityMethod method; /**< 使用的方法 */
    lvInequality *ineq;        /**< 此步骤的不等式 */
    char *justification;       /**< 证明理由 */
    int *premise_ids;          /**< 前提步骤 ID */
    int premise_count;         /**< 前提数量 */
} lvInequalityStep;

/**
 * @brief 不等式证明
 */
struct lvInequalityProof {
    lvInequality *target;      /**< 目标不等式 */
    lvInequalityStep *steps;   /**< 证明步骤数组 */
    int step_count;            /**< 步骤数量 */
    int step_capacity;         /**< 步骤容量 */
    lvInequalityStatus status; /**< 最终状态 */
    char *error_message;       /**< 错误消息（失败时） */
};

/**
 * @brief 不等式系统
 */
struct lvInequalitySystem {
    lvDArray inequalities; /**< 不等式数组（lvDArray of lvInequality*） */
    lvExpr **variables;    /**< 变量数组 */
    uint32_t var_count;    /**< 变量数量 */
};

/* ============== 不等式创建/销毁 ============== */

/**
 * @brief 创建不等式
 * @param left 左边表达式
 * @param type 不等式类型
 * @param right 右边表达式
 * @return 新不等式
 */
lvInequality *lv_ineq_create(lvExpr *left, lvInequalityType type, lvExpr *right);

/**
 * @brief 销毁不等式
 * @param ineq 不等式指针
 */
lv_PUBLIC_API void lv_ineq_destroy(lvInequality *ineq);

/**
 * @brief 复制不等式
 * @param ineq 源不等式
 * @return 新不等式副本
 */
lvInequality *lv_ineq_copy(const lvInequality *ineq);

/**
 * @brief 创建不等式系统
 * @return 新系统
 */
lvInequalitySystem *lv_ineq_system_create(void);

/**
 * @brief 销毁不等式系统
 * @param sys 系统指针
 */
lv_PUBLIC_API void lv_ineq_system_destroy(lvInequalitySystem *sys);

/**
 * @brief 添加不等式到系统
 * @param sys 系统
 * @param ineq 不等式
 * @return 是否成功
 */
lv_PUBLIC_API bool lv_ineq_system_add(lvInequalitySystem *sys, lvInequality *ineq);

/**
 * @brief 添加变量约束（如 x > 0）
 * @param sys 系统
 * @param var 变量表达式
 * @param type 约束类型
 * @param value 约束值（通常为 0）
 * @return 是否成功
 */
lv_PUBLIC_API bool lv_ineq_system_add_var_constraint(lvInequalitySystem *sys, lvExpr *var, lvInequalityType type, const mpq_t value);

/* ============== 基本不等式证明 ============== */

/**
 * @brief 证明不等式
 * @param ineq 不等式
 * @param sys 不等式系统（提供前提条件）
 * @param proof 输出证明（可为 NULL）
 * @return 证明状态
 */
lvInequalityStatus lv_ineq_prove(lvInequality *ineq, const lvInequalitySystem *sys, lvInequalityProof **proof);

/**
 * @brief 销毁证明
 * @param proof 证明指针
 */
lv_PUBLIC_API void lv_ineq_proof_destroy(lvInequalityProof *proof);

/**
 * @brief 尝试使用指定方法证明不等式
 * @param ineq 不等式
 * @param method 证明方法
 * @param sys 不等式系统
 * @param proof 输出证明
 * @return 证明状态
 */
lvInequalityStatus lv_ineq_prove_with_method(lvInequality *ineq, lvInequalityMethod method,
                                             const lvInequalitySystem *sys, lvInequalityProof **proof);

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
lv_PUBLIC_API bool lv_ineq_am_gm(lvExpr **expressions, uint32_t count, lvExpr **out_lower_bound, lvExpr **out_upper_bound);

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
lv_PUBLIC_API bool lv_ineq_cauchy_schwarz(lvExpr **a, lvExpr **b, uint32_t count, lvInequality **out_ineq);

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
lv_PUBLIC_API bool lv_ineq_rearrangement(lvExpr **a, lvExpr **b, uint32_t count, lvExpr **out_min, lvExpr **out_max);

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
lv_PUBLIC_API bool lv_ineq_schur(lvExpr *a, lvExpr *b, lvExpr *c, uint32_t r, lvInequality **out_ineq);

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
bool lv_ineq_jensen(const char *func, lvExpr **points, mpq_t *weights, uint32_t count, bool is_convex,
                    lvInequality **out_ineq);

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
lv_PUBLIC_API uint32_t lv_ineq_triangle(lvExpr *a, lvExpr *b, lvExpr *c, lvInequality **out_inequalities, uint32_t max_count);

/* ============== 不等式变换 ============== */

/**
 * @brief 不等式两边加表达式
 * @param ineq 不等式
 * @param expr 表达式
 * @return 新不等式
 */
lvInequality *lv_ineq_add(lvInequality *ineq, lvExpr *expr);

/**
 * @brief 不等式两边乘表达式
 * @param ineq 不等式
 * @param expr 表达式
 * @param expr_sign 表达式符号（正/负/未知）
 * @return 新不等式
 */
lvInequality *lv_ineq_mul(lvInequality *ineq, lvExpr *expr, int expr_sign);

/**
 * @brief 不等式取反（类型方向反转）
 * @param ineq 不等式
 * @return 新不等式
 *
 * @note 实现语义为方向反转（与 lv_ineq_mul 负乘共用 ineq_negate_type）：
 *   left < right => left > right
 *   left <= right => left >= right
 *   left > right => left < right
 *   left >= right => left <= right
 *   left != right => left != right
 */
lvInequality *lv_ineq_negate(lvInequality *ineq);

/**
 * @brief 不等式链传递
 * @param ineqs 不等式数组（按顺序）
 * @param count 不等式数量
 * @param out_result 输出结果不等式
 * @return 是否可以传递
 */
lv_PUBLIC_API bool lv_ineq_transitive(lvInequality **ineqs, uint32_t count, lvInequality **out_result);

/**
 * @brief 合并同向不等式
 * @param ineqs 不等式数组
 * @param count 不等式数量
 * @param out_result 输出结果不等式
 * @return 是否可以合并
 */
lv_PUBLIC_API bool lv_ineq_merge(lvInequality **ineqs, uint32_t count, lvInequality **out_result);

/* ============== 表达式符号判定 ============== */

/**
 * @brief 表达式符号判定结果
 */
typedef enum {
    SIGN_POSITIVE,    /**< 正数 */
    SIGN_NEGATIVE,    /**< 负数 */
    SIGN_ZERO,        /**< 零 */
    SIGN_NONNEGATIVE, /**< 非负 */
    SIGN_NONPOSITIVE, /**< 非正 */
    SIGN_UNKNOWN      /**< 未知 */
} lvSign;

/**
 * @brief 判定表达式符号
 * @param expr 表达式
 * @param sys 不等式系统（提供变量约束）
 * @return 符号判定结果
 */
lvSign lv_expr_sign(lvExpr *expr, const lvInequalitySystem *sys);

/**
 * @brief 检查表达式是否为正
 * @param expr 表达式
 * @param sys 不等式系统
 * @return 是否为正
 */
lv_PUBLIC_API bool lv_expr_is_positive(lvExpr *expr, const lvInequalitySystem *sys);

/**
 * @brief 检查表达式是否为非负
 * @param expr 表达式
 * @param sys 不等式系统
 * @return 是否为非负
 */
lv_PUBLIC_API bool lv_expr_is_nonnegative(lvExpr *expr, const lvInequalitySystem *sys);

/* ============== 平方和分解 ============== */

/**
 * @brief 平方和分解结果
 */
typedef struct {
    lvExpr **squares;     /**< 平方项数组 */
    uint32_t count;       /**< 平方项数量 */
    lvExpr *remainder;    /**< 余项（可为 NULL） */
    char *failure_reason; /**< 分解失败原因（成功时为 NULL） */
} lvSOSDecomposition;

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
lv_PUBLIC_API bool lv_expr_sos_decompose(lvExpr *poly, lvSOSDecomposition **out_sos);

/**
 * @brief 销毁平方和分解
 * @param sos 分解结果
 */
lv_PUBLIC_API void lv_sos_destroy(lvSOSDecomposition *sos);

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
lv_PUBLIC_API bool lv_ineq_triangle_area(lvExpr *a, lvExpr *b, lvExpr *c, lvExpr *area, lvInequality **out_ineq);

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
lv_PUBLIC_API bool lv_ineq_weitzenbock(lvExpr *a, lvExpr *b, lvExpr *c, lvExpr *area, lvInequality **out_ineq);

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
bool lv_ineq_erdos_mordell(lvExpr *pa, lvExpr *pb, lvExpr *pc, lvExpr *p, lvExpr *q, lvExpr *r,
                           lvInequality **out_ineq);

/* ============== 不等式序列化 ============== */

/**
 * @brief 不等式序列化为字符串
 * @param ineq 不等式
 * @return 字符串（[take] 调用者负责释放）
 */
lv_PUBLIC_API char *lv_ineq_to_string(const lvInequality *ineq);

/**
 * @brief 证明序列化为字符串
 * @param proof 证明
 * @return 字符串（[take] 调用者负责释放）
 */
lv_PUBLIC_API char *lv_ineq_proof_to_string(const lvInequalityProof *proof);

/**
 * @brief 证明导出为 LaTeX
 * @param proof 证明
 * @return LaTeX 字符串（[take] 调用者负责释放）
 */
lv_PUBLIC_API char *lv_ineq_proof_to_latex(const lvInequalityProof *proof);

#ifdef __cplusplus
}
#endif

#endif /* lv_INEQUALITY_REASONING_H */
