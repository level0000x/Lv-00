/* ========================================================================
 * 模块名称：量词系统 (quantifier)
 * 功能概述：提供全称量词（forall）、存在量词（exists）和唯一存在量词
 *          （exists_unique）的形式化处理。支持量词实例化/泛化操作、
 *          有限域上的量词消去、与约束图的双向映射，以及三值逻辑
 *          真值评估。遵循构造性/BHK 解释语义。
 *
 * 主要 API：
 *   - lv_quant_domain_create / destroy      — 域管理
 *   - lv_quant_expr_create / destroy        — 量化表达式管理
 *   - lv_quant_expr_evaluate                — 三值逻辑评估
 *   - lv_quantifier_instantiate              — 全称量词消去（forall-E）
 *   - lv_quantifier_generalize              — 全称量词引入（forall-I）
 *   - lv_quant_exists_introduce / eliminate — 存在量词引入/消去
 *   - lv_quant_eliminate_forall_finite      — 有限域全称消去
 *   - lv_quant_count_satisfying             — 统计满足元素数
 *
 * 使用示例：
 lv_PUBLIC_API *   lvDomain *d = lv_quant_domain_create_finite(1, elements, n);
 *   lvQuantifiedExpr *expr = lv_quant_expr_create(
 *       1, lv_FORALL, "p", var_id, d, body_prop);
 lv_PUBLIC_API *   lvTruthValue tv = lv_quant_expr_evaluate(expr);
 *
 * @version 1.1.0
 * ======================================================================== */

/**
 * @file quantifier.h
 * @brief 量词系统 —— 全称/存在/唯一存在量词的形式化处理
 */

#ifndef lv_QUANTIFIER_H
#define lv_QUANTIFIER_H

#include <stdbool.h>

#include "constraint_graph.h"
#include "three_valued_logic.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============== 前向声明 ============== */
typedef struct lvQuantifiedExpr lvQuantifiedExpr;
typedef struct lvDomain lvDomain;
typedef struct lvQuantifiedResult lvQuantifiedResult;

/* ============== 量词类型 ============== */

/**
 * @brief 量词枚举
 *
 * lv_FORALL       : ∀  全称量词  "对所有...都成立"
 * lv_EXISTS       : ∃  存在量词  "存在一个...使得..."
 * lv_EXISTS_UNIQUE: ∃! 唯一存在  "存在唯一一个...使得..."
 */
typedef enum { lv_FORALL = 0, lv_EXISTS = 1, lv_EXISTS_UNIQUE = 2 } lvQuantifier;

/* ============== 域定义 ============== */

/**
 * @brief 量化域（变量取值范围）
 *
 * 支持三种域定义方式：
 * - 有限枚举：domain_elements 中列出所有元素
 * - 约束图子图：subgraph 指定一个约束图子图作为域
 * - 命名域：domain_name 引用预定义的域（如实数域 R）
 */
struct lvDomain {
    int id;            /**< 域ID */
    char *domain_name; /**< 域名（如 "R", "Triangle", "Point"） */

    /* 有限枚举域 */
    int *domain_elements; /**< 域元素列表（节点ID数组） */
    int element_count;    /**< 元素数量 */
    int element_capacity; /**< 元素数组容量 */

    /* 约束图子图域 */
    ConstraintGraph *subgraph; /**< 约束图子图（可为 NULL） */

    /* 域基数信息 */
    bool is_finite;            /**< 是否为有限域 */
    int estimated_cardinality; /**< 估计基数（-1 = 未知/无限） */
};

/* ============== 量化表达式 ============== */

/**
 * @brief 量化命题表达式
 *
 * 结构: QUANTIFIER variable ∈ domain . body_proposition
 * 例如: ∀p ∈ Points . collinear(p, A, B) → onSegment(p, AB)
 */
struct lvQuantifiedExpr {
    int id;                  /**< 表达式ID */
    lvQuantifier quantifier; /**< 量词类型 */
    char *variable_name;     /**< 绑定变量名（如 "x", "p"） */
    int variable_node_id;    /**< 绑定变量对应的约束图节点ID */
    lvDomain *domain;        /**< 量化域 */

    /* 体命题：量词作用域内的命题公式 */
    struct Proposition *body_proposition; /**< 体命题（所有权属于本表达式） */

    /* 实例化追踪 */
    int *instantiated_ids;     /**< 已实例化的变量ID列表 */
    int instantiated_count;    /**< 已实例化变量数量 */
    int instantiated_capacity; /**< 已实例化ID数组的分配容量 */

    /* 真值缓存（三值逻辑） */
    lvTruthValue cached_truth; /**< 缓存的真值 */
    bool truth_cache_valid;    /**< 真值缓存是否有效 */
};

/* ============== 量词运算结果 ============== */

/**
 * @brief 量词操作结果
 */
typedef enum {
    lv_QUANT_OK,                 /**< 操作成功 */
    lv_QUANT_DOMAIN_EMPTY,       /**< 域为空（∀ 为真，∃ 为伪） */
    lv_QUANT_DOMAIN_INFINITE,    /**< 域为无限，消去不可能 */
    lv_QUANT_INVALID_VARIABLE,   /**< 变量无效 */
    lv_QUANT_BODY_UNDEFINED,     /**< 体命题未定义 */
    lv_QUANT_INSTANTIATE_FAILED, /**< 实例化失败 */
    lv_QUANT_GENERALIZE_FAILED,  /**< 泛化失败 */
    lv_QUANT_COUNTEREXAMPLE,     /**< 找到反例 */
    lv_QUANT_ERROR               /**< 一般性错误 */
} lvQuantResult;

/**
 * @brief 实例化结果
 */
struct lvQuantifiedResult {
    lvQuantResult status;            /**< 操作结果状态 */
    lvTruthValue truth_value;        /**< 量词表达式的真值 */
    int witness_node_id;             /**< 目击者节点ID（存在量词） */
    char *error_message;             /**< 错误消息（失败时） */
    struct Proposition *result_prop; /**< 结果命题（实例化/消去后） */
};

/* ============== 域管理 API ============== */

/**
 * @brief 创建命名域
 *
 * @param id          域ID
 * @param domain_name 域名称（内部复制）
 * @return 新分配的域，失败返回 NULL
 */
lv_PUBLIC_API lvDomain *lv_quant_domain_create(int id, const char *domain_name);

/**
 * @brief 创建有限枚举域
 *
 * @param id       域ID
 * @param elements 元素节点ID数组
 * @param count    元素数量
 * @return 新分配的域，失败返回 NULL
 */
lv_PUBLIC_API lvDomain *lv_quant_domain_create_finite(int id, const int *elements, int count);

/**
 * @brief 向域中添加元素
 *
 * @param domain   域
 * @param element  元素节点ID
 * @return true 成功，false 失败
 */
lv_PUBLIC_API bool lv_quant_domain_add_element(lvDomain *domain, int element);

/**
 * @brief 向域中批量添加元素
 *
 * @param domain   域
 * @param elements 元素节点ID数组
 * @param count    元素数量
 * @return true 成功，false 失败
 */
lv_PUBLIC_API bool lv_quant_domain_add_elements(lvDomain *domain, const int *elements, int count);

/**
 * @brief 检查元素是否属于域
 *
 * @param domain    域
 * @param element   元素ID
 * @return true 属于，false 不属于
 */
lv_PUBLIC_API bool lv_quant_domain_contains(const lvDomain *domain, int element);

/**
 * @brief 获取域大小
 *
 * @param domain 域
 * @return 元素数量（-1 表示无限）
 */
lv_PUBLIC_API int lv_quant_domain_size(const lvDomain *domain);

/**
 * @brief 销毁域
 *
 * @param domain 域（可为 NULL）
 */
lv_PUBLIC_API void lv_quant_domain_destroy(lvDomain *domain);

/* ============== 量化表达式 API ============== */

/**
 * @brief 创建量化表达式
 *
 * @param id              表达式ID
 * @param quantifier      量词类型
 * @param variable_name   变量名（内部复制）
 * @param variable_node_id 变量绑定的约束图节点ID
 * @param domain          量化域（所有权转移）
 * @param body_prop       体命题（所有权转移）
 * @return 新分配的量化表达式，失败返回 NULL
 */
lv_PUBLIC_API lvQuantifiedExpr *lv_quant_expr_create(int id, lvQuantifier quantifier, const char *variable_name,
                                                     int variable_node_id, lvDomain *domain,
                                                     struct Proposition *body_prop);

/**
 * @brief 销毁量化表达式
 *
 * @param expr 量化表达式（可为 NULL）
 */
lv_PUBLIC_API void lv_quant_expr_destroy(lvQuantifiedExpr *expr);

/**
 * @brief 评估量化表达式的真值（三值逻辑）
 *
 * 对有限域尝试完全评估；对无限域返回 lv_UNKNOWN。
 *
 * @param expr 量化表达式
 * @return 三值真值
 */
lv_PUBLIC_API lvTruthValue lv_quant_expr_evaluate(lvQuantifiedExpr *expr);

/* ============== 量词实例化 ============== */

/**
 * @brief 量词实例化（∀-消去）
 *
 * 从 ∀x.P(x) 推导出 P(t)，其中 t 在域中。
 * 这是全称量词的消去规则（∀E）。
 *
 * 例如: 从 "∀p∈Points.collinear(p,A,B)→onSegment(p,AB)"
 *       和 t=Midpoint(A,B) 推导出
 *       "collinear(Midpoint(A,B),A,B)→onSegment(Midpoint(A,B),AB)"
 *
 * @param expr         量化表达式（∀...）
 * @param instance_id  要代入的实例节点ID
 * @param out_result   输出结果
 * @return 操作结果
 */
lv_PUBLIC_API lvQuantResult lv_quantifier_instantiate(const lvQuantifiedExpr *expr, int instance_id,
                                                      lvQuantifiedResult *out_result);

/**
 * @brief 量词泛化（∀-引入）
 *
 * 从 P(x) 对任意 x∈D 成立推导出 ∀x.P(x)。
 * 前提：x 不能在前提集中自由出现（特征变量条件）。
 *
 * @param expr         量化表达式模板
 * @param out_result   输出结果
 * @return 操作结果
 */
lv_PUBLIC_API lvQuantResult lv_quantifier_generalize(const lvQuantifiedExpr *expr, lvQuantifiedResult *out_result);

/* ============== 存在量词运算 ============== */

/**
 * @brief 存在量词引入（∃I）
 *
 * 从 P(t) 推导出 ∃x.P(x)，其中 t 在域中。
 *
 * @param expr         待填充的量化表达式（量词为 ∃）
 * @param witness_id   目击者节点ID
 * @param out_result   输出结果
 * @return 操作结果
 */
lv_PUBLIC_API lvQuantResult lv_quant_exists_introduce(lvQuantifiedExpr *expr, int witness_id,
                                                      lvQuantifiedResult *out_result);

/**
 * @brief 存在量词消去（∃E）
 *
 * 从 ∃x.P(x) 和 ∀y.(P(y)→Q) 推导出 Q（其中 y 不在 Q 中自由）。
 *
 * @param exists_expr  存在量化表达式
 * @param target_prop  目标命题 Q
 * @param out_result   输出结果
 * @return 操作结果
 */
lv_PUBLIC_API lvQuantResult lv_quant_exists_eliminate(const lvQuantifiedExpr *exists_expr,
                                                      struct Proposition *target_prop, lvQuantifiedResult *out_result);

/* ============== 有限域上的量词消去 ============== */

/**
 * @brief 有限域上的全称量词消去
 *
 * 在有限域 D = {d1, ..., dn} 上，将 ∀x∈D.P(x) 展开为 P(d1) ∧ ... ∧ P(dn)。
 * 返回消去后的合取命题。
 *
 * @param expr         全称量化表达式
 * @param out_result   输出结果（含 status 和 result_prop）
 * @return 操作结果
 */
lv_PUBLIC_API lvQuantResult lv_quant_eliminate_forall_finite(const lvQuantifiedExpr *expr,
                                                             lvQuantifiedResult *out_result);

/**
 * @brief 有限域上的存在量词消去
 *
 * 在有限域 D = {d1, ..., dn} 上，将 ∃x∈D.P(x) 展开为 P(d1) ∨ ... ∨ P(dn)。
 * 返回消去后的析取命题。
 *
 * @param expr         存在量化表达式
 * @param out_result   输出结果
 * @return 操作结果
 */
lv_PUBLIC_API lvQuantResult lv_quant_eliminate_exists_finite(const lvQuantifiedExpr *expr,
                                                             lvQuantifiedResult *out_result);

/**
 * @brief 有限域上的唯一存在量词消去
 *
 * 在有限域上，将 ∃!x.P(x) 展开为：
 *  (P(d1) ∧ ¬P(d2) ∧ ... ∧ ¬P(dn)) ∨ (¬P(d1) ∧ P(d2) ∧ ... ∧ ¬P(dn)) ∨ ...
 * 即恰好一个元素满足 P。
 *
 * @param expr         唯一存在量化表达式
 * @param out_result   输出结果
 * @return 操作结果
 */
lv_PUBLIC_API lvQuantResult lv_quant_eliminate_exists_unique_finite(const lvQuantifiedExpr *expr,
                                                                    lvQuantifiedResult *out_result);

/* ============== 量词级别的最佳实践检查 ============== */

/**
 * @brief 检查量词在域上是否可消去
 *
 * 只有有限域上的量词才能通过枚举消去。
 * 对无限域返回 false。
 *
 * @param expr 量化表达式
 * @return true 可消去，false 不可
 */
lv_PUBLIC_API bool lv_quant_is_eliminable(const lvQuantifiedExpr *expr);

/**
 * @brief 获取给定域中满足体命题的元素数量
 *
 * 对于有限域，枚举检查每个元素。
 * 对于无限域，返回 -1。
 *
 * @param expr 量化表达式
 * @return 满足体命题的元素数量（-1 = 无法确定）
 */
lv_PUBLIC_API int lv_quant_count_satisfying(const lvQuantifiedExpr *expr);

/* ============== 释放结果结构体 ============== */

/**
 * @brief 释放量词操作结果结构体
 *
 * 注意：result_prop 的所有权在释放后不再属于调用者。
 *
 * @param result 结果结构体指针
 */
lv_PUBLIC_API void lv_quant_result_destroy(lvQuantifiedResult *result);

/* ============== 辅助函数 ============== */

/**
 * @brief 量词类型转字符串
 *
 * @param q 量词类型
 * @return 静态字符串（"∀" / "∃" / "∃!"），请勿释放
 */
lv_PUBLIC_API const char *lv_quant_to_string(lvQuantifier q);

/**
 * @brief 量词操作结果转字符串
 *
 * @param result 操作结果
 * @return 静态字符串，请勿释放
 */
lv_PUBLIC_API const char *lv_quant_result_to_string(lvQuantResult result);

#ifdef __cplusplus
}
#endif

#endif /* lv_QUANTIFIER_H */
