/**
 * @file expr_vtable.h
 * @brief lvExpr 符号表达式虚函数表（VTable）—— 按表达式类型分发
 *
 * @details 为 lvExpr 的每种类型（EXPR_TYPE_*）提供统一的操作接口，
 *          消除 switch-on-type 的分发模式。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#ifndef lv_EXPR_VTABLE_H
#define lv_EXPR_VTABLE_H

#include "expr_canonical.h"
#include "inequality_reasoning.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============== 函数指针类型 ============== */

/**
 * @brief 结构性比较两个表达式
 * @param a 第一个表达式
 * @param b 第二个表达式
 * @return 是否结构相等
 */
typedef bool (*lvExprEqualFn)(const lvExpr *a, const lvExpr *b);

/**
 * @brief 判定表达式符号（基于表达式自身结构，不依赖约束系统）
 * @param expr 表达式
 * @param sys 不等式系统（可为 NULL）
 * @return 符号判定结果
 */
typedef lvSign (*lvExprSignFn)(const lvExpr *expr, const lvInequalitySystem *sys);

/* ============== 虚函数表 ============== */

/**
 * @brief lvExpr 虚函数表
 *
 * 每种表达式类型（EXPR_TYPE_*）对应一个静态实例，
 * 通过 lv_expr_get_ops() 获取。
 */
typedef struct {
    lvExprEqualFn structurally_equal; /**< 结构性比较 */
    lvExprSignFn sign;                /**< 符号判定 */
} lvExprOps;

/**
 * @brief 获取指定表达式类型的虚函数表
 * @param type 表达式类型
 * @return 对应的 vtable 指针，未知类型返回 NULL
 */
extern const lvExprOps *lv_expr_get_ops(lvExprType type);

#ifdef __cplusplus
}
#endif

#endif /* lv_EXPR_VTABLE_H */