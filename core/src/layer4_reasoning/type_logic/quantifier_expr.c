/**
 * @file quantifier_expr.c
 * @brief 量化表达式 API 与真值评估（由 quantifier.c 拆分子模块）
 *
 * @details 表达式销毁、空域/有限域真值处理、查找表分发与
 *          lv_quant_expr_evaluate。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/quantifier.h"
#include "lv/lv_xmacro.h"
#include "lv/lv_lifecycle.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/proof.h"

#include "lv/error_codes.h"
#include "lv/lv_utils.h"
#include "lv/three_valued_logic.h"
#include "quantifier_internal.h"

/* ============== 量化表达式 API ============== */

/**
 * @brief 创建量化表达式
 *
 * 构造一个完整的量化命题表达式：
 *   QUANTIFIER variable_name ∈ domain . body_proposition
 *
 * 所有权语义：domain 和 body_proposition 的所有权转移给新创建的表达式，
 * 表达式销毁时会一并释放它们。
 *
 * @param id               表达式ID
 * @param quantifier       量词类型
 * @param variable_name    变量名（内部复制）
 * @param variable_node_id 变量绑定的约束图节点ID
 * @param domain           量化域（所有权转移）
 * @param body_prop        体命题（所有权转移）
 * @return 新分配的量化表达式，失败返回 NULL
 */
lvQuantifiedExpr *lv_quant_expr_create(int id, lvQuantifier quantifier, const char *variable_name, int variable_node_id,
                                       lvDomain *domain, struct Proposition *body_prop) {
    lvQuantifiedExpr *expr;

    if (!domain) {
        return NULL;
    }

    expr = (lvQuantifiedExpr *) lv_calloc(1, sizeof(lvQuantifiedExpr));
    if (!expr) {
        return NULL;
    }

    expr->id = id;
    expr->quantifier = quantifier;
    expr->variable_name = variable_name ? lv_strdup(variable_name) : NULL;
    expr->variable_node_id = variable_node_id;
    expr->domain = domain;
    expr->body_proposition = body_prop;

    /* 实例化追踪初始化 */
    expr->instantiated_ids = NULL;
    expr->instantiated_count = 0;

    /* 真值缓存初始化为无效 */
    expr->cached_truth = lv_UNKNOWN;
    expr->truth_cache_valid = false;

    return expr;
}

/** @brief 结果命题（浅构造 Proposition）字段销毁表
 *  create_result_proposition 仅设置 id + label，其余字段为 calloc 零值，
 *  故只需释放 label 与 4 个端口数组；完整体命题走 proposition_destroy。 */
const lvFieldDesc kQuantBodyPropDestroyFields[] = {
    lv_FIELD_PLAIN(Proposition, label),
    lv_FIELD_PLAIN(Proposition, input_port_ids),
    lv_FIELD_PLAIN(Proposition, output_port_ids),
    lv_FIELD_PLAIN(Proposition, precondition_region_ids),
    lv_FIELD_PLAIN(Proposition, postcondition_constraint_ids),
};

/**
 * @brief 销毁量化表达式
 *
 * 释放表达式及其拥有的所有资源：
 * - 变量名字符串
 * - 量化域
 * - 体命题
 * - 实例化ID数组
 *
 * @param expr 量化表达式（可为 NULL，此时无操作）
 */
void lv_quant_expr_destroy(lvQuantifiedExpr *expr) {
    if (!expr) {
        return;
    }

    lv_FREE_AND_NULL(expr->variable_name);
    lv_quant_domain_destroy(expr->domain);
    expr->domain = NULL;

    /* 释放体命题：body_proposition 为完整 Proposition（所有权已随 lv_quant_expr_create 转移），
     * 委托 proposition_destroy 递归释放全部字段（含 name/description/pattern/sub_props/prop_type），
     * 避免仅释放外层字段造成 sub_props/pattern 泄漏。 */
    if (expr->body_proposition) {
        proposition_destroy(expr->body_proposition);
        expr->body_proposition = NULL;
    }

    lv_FREE_AND_NULL(expr->instantiated_ids);

    lv_free((void **) &expr);
}

/* ============== 空域真值处理函数 ============== */

/** 空域上 ∀x∈∅.P(x) 为 TRUE（空合取的恒等元） */
static lvTruthValue handle_forall_empty(void) {
    return lv_TRUE;
}

/** 空域上 ∃x∈∅.P(x) 为 FALSE（空析取的恒等元） */
static lvTruthValue handle_exists_empty(void) {
    return lv_FALSE;
}

/** 空域上 ∃!x∈∅.P(x) 为 FALSE（不存在唯一满足的元素） */
static lvTruthValue handle_exists_unique_empty(void) {
    return lv_FALSE;
}

/* ============== 有限域枚举评估函数 ============== */

/** 有限域全称量词评估：∀x∈D.P(x) → P(d1) ∧ ... ∧ P(dn) */
static lvTruthValue handle_forall_finite(lvQuantifiedExpr *expr) {
    int i;
    lvTruthValue elem_truth;
    lvTruthValue result = lv_TRUE;

    for (i = 0; i < expr->domain->element_count; i++) {
        elem_truth = evaluate_body_for_element(expr, expr->domain->domain_elements[i]);
        result = lv_tvl_and(result, elem_truth);
        /* 短路：遇到 FALSE 立即停止 */
        if (result == lv_FALSE) {
            break;
        }
    }
    return result;
}

/** 有限域存在量词评估：∃x∈D.P(x) → P(d1) ∨ ... ∨ P(dn) */
static lvTruthValue handle_exists_finite(lvQuantifiedExpr *expr) {
    int i;
    lvTruthValue elem_truth;
    lvTruthValue result = lv_FALSE;

    for (i = 0; i < expr->domain->element_count; i++) {
        elem_truth = evaluate_body_for_element(expr, expr->domain->domain_elements[i]);
        result = lv_tvl_or(result, elem_truth);
        /* 短路：遇到 TRUE 立即停止 */
        if (result == lv_TRUE) {
            break;
        }
    }
    return result;
}

/** 有限域唯一存在量词评估：∃!x∈D.P(x) → 恰好一个元素满足 */
static lvTruthValue handle_exists_unique_finite(lvQuantifiedExpr *expr) {
    int i;
    int satisfying_count = 0;
    lvTruthValue elem_truth;
    lvTruthValue result = lv_FALSE;

    for (i = 0; i < expr->domain->element_count; i++) {
        elem_truth = evaluate_body_for_element(expr, expr->domain->domain_elements[i]);
        if (elem_truth == lv_TRUE) {
            satisfying_count++;
        } else if (elem_truth == lv_UNKNOWN) {
            result = lv_UNKNOWN;
        }
    }
    if (result != lv_UNKNOWN) {
        result = (satisfying_count == 1) ? lv_TRUE : lv_FALSE;
    }
    return result;
}

/* ============== 查找表 ============== */

/* 空域真值处理函数指针类型 */
typedef lvTruthValue (*EmptyDomainHandler)(void);
/* 有限域枚举评估函数指针类型 */
typedef lvTruthValue (*FiniteDomainHandler)(lvQuantifiedExpr *expr);

/** 空域真值处理函数查找表 */
static const EmptyDomainHandler kEmptyDomainHandlers[] = {
    handle_forall_empty,       /* lv_FORALL = 0 */
    handle_exists_empty,       /* lv_EXISTS = 1 */
    handle_exists_unique_empty /* lv_EXISTS_UNIQUE = 2 */
};

/** 有限域枚举评估函数查找表 */
static const FiniteDomainHandler kFiniteDomainHandlers[] = {
    handle_forall_finite,       /* lv_FORALL = 0 */
    handle_exists_finite,       /* lv_EXISTS = 1 */
    handle_exists_unique_finite /* lv_EXISTS_UNIQUE = 2 */
};

/**
 * @brief 评估量化表达式的真值（三值逻辑）
 *
 * 评估策略：
 * - 有限域：枚举所有元素，逐一评估体命题
 *   - ∀：AND 归约（空域返回 TRUE）
 *   - ∃：OR 归约（空域返回 FALSE）
 *   - ∃!：统计满足元素数，恰好 1 个为 TRUE
 * - 无限域：返回 lv_UNKNOWN
 *
 * 评估结果会被缓存，后续调用直接返回缓存值。
 *
 * @param expr 量化表达式
 * @return 三值真值
 */
lvTruthValue lv_quant_expr_evaluate(lvQuantifiedExpr *expr) {
    int domain_size;

    if (!expr) {
        return lv_UNKNOWN;
    }

    /* 若缓存有效，直接返回 */
    if (expr->truth_cache_valid) {
        return expr->cached_truth;
    }

    domain_size = lv_quant_domain_size(expr->domain);

    /* 无限域：无法完全评估 */
    if (domain_size < 0) {
        expr->cached_truth = lv_UNKNOWN;
        expr->truth_cache_valid = true;
        return lv_UNKNOWN;
    }

    /* 空域处理 */
    if (domain_size == 0) {
        if (expr->quantifier >= lv_FORALL && expr->quantifier <= lv_EXISTS_UNIQUE) {
            expr->cached_truth = kEmptyDomainHandlers[expr->quantifier]();
        } else {
            expr->cached_truth = lv_UNKNOWN;
        }
        expr->truth_cache_valid = true;
        return expr->cached_truth;
    }

    /* 有限域：枚举评估 */
    if (expr->quantifier >= lv_FORALL && expr->quantifier <= lv_EXISTS_UNIQUE) {
        expr->cached_truth = kFiniteDomainHandlers[expr->quantifier](expr);
    } else {
        expr->cached_truth = lv_UNKNOWN;
    }

    expr->truth_cache_valid = true;
    return expr->cached_truth;
}

