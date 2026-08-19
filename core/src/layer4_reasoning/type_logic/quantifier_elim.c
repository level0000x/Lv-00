/**
 * @file quantifier_elim.c
 * @brief 有限域量词消去与结果管理（由 quantifier.c 拆分子模块）
 *
 * @details 全称/存在/唯一存在消去、可消去性检查、满足计数、
 *          结果释放与枚举字符串映射。
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

/* ============== 有限域上的量词消去 ============== */

/**
 * @brief 有限域上的全称量词消去
 *
 * 在有限域 D = {d1, ..., dn} 上，将 ∀x∈D.P(x) 展开为 P(d1) ∧ ... ∧ P(dn)。
 * 返回消去后的合取命题。
 *
 * 算法流程：
 * 1. 验证表达式为全称量词且域有限
 * 2. 为每个域元素创建体命题的实例
 * 3. 将所有实例组合为合取命题
 * 4. 评估合取命题的真值
 *
 * @param expr         全称量化表达式
 * @param out_result   输出结果（含 status、truth_value 和 result_prop）
 * @return 操作结果状态码
 */
lvQuantResult lv_quant_eliminate_forall_finite(const lvQuantifiedExpr *expr, lvQuantifiedResult *out_result) {
    int i;
    char name_buf[RESULT_NAME_BUF_SIZE];
    lvTruthValue combined_truth;

    init_quant_result(out_result);

    if (!expr || !out_result) {
        out_result->status = lv_QUANT_ERROR;
        return lv_QUANT_ERROR;
    }

    if (expr->quantifier != lv_FORALL) {
        out_result->status = lv_QUANT_ERROR;
        out_result->error_message = lv_strdup("全称消去仅适用于全称量词(∀)");
        return lv_QUANT_ERROR;
    }

    if (!expr->domain || !expr->domain->is_finite) {
        out_result->status = lv_QUANT_DOMAIN_INFINITE;
        out_result->error_message = lv_strdup("域为无限域，无法消去全称量词");
        return lv_QUANT_DOMAIN_INFINITE;
    }

    /* 空域：∀ 在空域上为真 */
    if (expr->domain->element_count == 0) {
        out_result->status = lv_QUANT_DOMAIN_EMPTY;
        out_result->truth_value = lv_TRUE;
        (void) lv_snprintf(name_buf, RESULT_NAME_BUF_SIZE, "ForallElim_empty_%d", expr->id);
        out_result->result_prop = create_result_proposition(expr->id, name_buf);
        return lv_QUANT_DOMAIN_EMPTY;
    }

    /* 枚举所有元素，执行 AND 归约 */
    combined_truth = lv_TRUE;
    for (i = 0; i < expr->domain->element_count; i++) {
        lvTruthValue elem_truth = evaluate_body_for_element(expr, expr->domain->domain_elements[i]);
        combined_truth = lv_tvl_and(combined_truth, elem_truth);
        if (combined_truth == lv_FALSE) {
            break; /* 短路 */
        }
    }

    out_result->truth_value = combined_truth;

    /* 创建结果命题：P(d1) ∧ ... ∧ P(dn) */
    (void) lv_snprintf(name_buf, RESULT_NAME_BUF_SIZE, "ForallElim_%s_%d", expr->variable_name ? expr->variable_name : "x",
                    expr->id);
    out_result->result_prop = create_result_proposition(expr->id, name_buf);
    if (!out_result->result_prop) {
        out_result->status = lv_QUANT_ERROR;
        out_result->error_message = lv_strdup("创建结果命题失败");
        return lv_QUANT_ERROR;
    }

    out_result->status = lv_QUANT_OK;
    return lv_QUANT_OK;
}

/**
 * @brief 有限域上的存在量词消去
 *
 * 在有限域 D = {d1, ..., dn} 上，将 ∃x∈D.P(x) 展开为 P(d1) ∨ ... ∨ P(dn)。
 * 返回消去后的析取命题。
 *
 * 算法流程：
 * 1. 验证表达式为存在量词且域有限
 * 2. 枚举所有域元素，执行 OR 归约
 * 3. 记录第一个满足体命题的目击者
 * 4. 创建析取命题作为结果
 *
 * @param expr         存在量化表达式
 * @param out_result   输出结果（含 status、truth_value、witness_node_id 和 result_prop）
 * @return 操作结果状态码
 */
lvQuantResult lv_quant_eliminate_exists_finite(const lvQuantifiedExpr *expr, lvQuantifiedResult *out_result) {
    int i;
    char name_buf[RESULT_NAME_BUF_SIZE];
    lvTruthValue combined_truth;

    init_quant_result(out_result);

    if (!expr || !out_result) {
        out_result->status = lv_QUANT_ERROR;
        return lv_QUANT_ERROR;
    }

    if (expr->quantifier != lv_EXISTS) {
        out_result->status = lv_QUANT_ERROR;
        out_result->error_message = lv_strdup("存在消去仅适用于存在量词(∃)");
        return lv_QUANT_ERROR;
    }

    if (!expr->domain || !expr->domain->is_finite) {
        out_result->status = lv_QUANT_DOMAIN_INFINITE;
        out_result->error_message = lv_strdup("域为无限域，无法消去存在量词");
        return lv_QUANT_DOMAIN_INFINITE;
    }

    /* 空域：∃ 在空域上为假 */
    if (expr->domain->element_count == 0) {
        out_result->status = lv_QUANT_DOMAIN_EMPTY;
        out_result->truth_value = lv_FALSE;
        (void) lv_snprintf(name_buf, RESULT_NAME_BUF_SIZE, "ExistsElim_empty_%d", expr->id);
        out_result->result_prop = create_result_proposition(expr->id, name_buf);
        return lv_QUANT_DOMAIN_EMPTY;
    }

    /* 枚举所有元素，执行 OR 归约 */
    combined_truth = lv_FALSE;
    for (i = 0; i < expr->domain->element_count; i++) {
        lvTruthValue elem_truth = evaluate_body_for_element(expr, expr->domain->domain_elements[i]);
        combined_truth = lv_tvl_or(combined_truth, elem_truth);

        /* 记录第一个目击者 */
        if (elem_truth == lv_TRUE && out_result->witness_node_id < 0) {
            out_result->witness_node_id = expr->domain->domain_elements[i];
        }

        if (combined_truth == lv_TRUE) {
            break; /* 短路 */
        }
    }

    out_result->truth_value = combined_truth;

    /* 创建结果命题：P(d1) ∨ ... ∨ P(dn) */
    (void) lv_snprintf(name_buf, RESULT_NAME_BUF_SIZE, "ExistsElim_%s_%d", expr->variable_name ? expr->variable_name : "x",
                    expr->id);
    out_result->result_prop = create_result_proposition(expr->id, name_buf);
    if (!out_result->result_prop) {
        out_result->status = lv_QUANT_ERROR;
        out_result->error_message = lv_strdup("创建结果命题失败");
        return lv_QUANT_ERROR;
    }

    out_result->status = lv_QUANT_OK;
    return lv_QUANT_OK;
}

/**
 * @brief 有限域上的唯一存在量词消去
 *
 * 在有限域上，将 ∃!x.P(x) 展开。
 * 语义：恰好一个元素满足 P。
 *
 * 算法流程：
 * 1. 验证表达式为唯一存在量词且域有限
 * 2. 枚举所有域元素，统计满足体命题的元素数量
 * 3. 若恰好一个元素满足，返回 TRUE 并记录目击者
 * 4. 否则返回 FALSE
 *
 * 展开形式（D = {d1, ..., dn}）：
 *   (P(d1) ∧ ¬P(d2) ∧ ... ∧ ¬P(dn)) ∨
 *   (¬P(d1) ∧ P(d2) ∧ ... ∧ ¬P(dn)) ∨ ...
 *   即 n 个分支的析取，每个分支恰好一个元素满足 P。
 *
 * @param expr         唯一存在量化表达式
 * @param out_result   输出结果
 * @return 操作结果状态码
 */
lvQuantResult lv_quant_eliminate_exists_unique_finite(const lvQuantifiedExpr *expr, lvQuantifiedResult *out_result) {
    int i;
    char name_buf[RESULT_NAME_BUF_SIZE];
    int satisfying_count;
    int last_witness;
    bool has_unknown;

    init_quant_result(out_result);

    if (!expr || !out_result) {
        out_result->status = lv_QUANT_ERROR;
        return lv_QUANT_ERROR;
    }

    if (expr->quantifier != lv_EXISTS_UNIQUE) {
        out_result->status = lv_QUANT_ERROR;
        out_result->error_message = lv_strdup("唯一存在消去仅适用于唯一存在量词(∃!)");
        return lv_QUANT_ERROR;
    }

    if (!expr->domain || !expr->domain->is_finite) {
        out_result->status = lv_QUANT_DOMAIN_INFINITE;
        out_result->error_message = lv_strdup("域为无限域，无法消去唯一存在量词");
        return lv_QUANT_DOMAIN_INFINITE;
    }

    /* 空域：不存在唯一满足的元素 */
    if (expr->domain->element_count == 0) {
        out_result->status = lv_QUANT_DOMAIN_EMPTY;
        out_result->truth_value = lv_FALSE;
        (void) lv_snprintf(name_buf, RESULT_NAME_BUF_SIZE, "ExistsUniqueElim_empty_%d", expr->id);
        out_result->result_prop = create_result_proposition(expr->id, name_buf);
        return lv_QUANT_DOMAIN_EMPTY;
    }

    /* 枚举所有元素，统计满足体命题的元素数量 */
    satisfying_count = 0;
    last_witness = -1;
    has_unknown = false;

    for (i = 0; i < expr->domain->element_count; i++) {
        lvTruthValue elem_truth = evaluate_body_for_element(expr, expr->domain->domain_elements[i]);

        if (elem_truth == lv_TRUE) {
            satisfying_count++;
            last_witness = expr->domain->domain_elements[i];
        } else if (elem_truth == lv_UNKNOWN) {
            has_unknown = true;
        }
    }

    /* 确定真值 */
    if (has_unknown) {
        out_result->truth_value = lv_UNKNOWN;
    } else if (satisfying_count == 1) {
        out_result->truth_value = lv_TRUE;
        out_result->witness_node_id = last_witness;
    } else {
        out_result->truth_value = lv_FALSE;
    }

    /* 创建结果命题 */
    (void) lv_snprintf(name_buf, RESULT_NAME_BUF_SIZE, "ExistsUniqueElim_%s_%d",
                    expr->variable_name ? expr->variable_name : "x", expr->id);
    out_result->result_prop = create_result_proposition(expr->id, name_buf);
    if (!out_result->result_prop) {
        out_result->status = lv_QUANT_ERROR;
        out_result->error_message = lv_strdup("创建结果命题失败");
        return lv_QUANT_ERROR;
    }

    out_result->status = lv_QUANT_OK;
    return lv_QUANT_OK;
}

/* ============== 量词级别的最佳实践检查 ============== */

/**
 * @brief 检查量词在域上是否可消去
 *
 * 只有有限域上的量词才能通过枚举消去。
 * 判定条件：
 * - 域的 is_finite 标志为 true，或
 * - 域有约束图子图（可枚举子图节点）
 *
 * @param expr 量化表达式
 * @return true 可消去，false 不可
 */
bool lv_quant_is_eliminable(const lvQuantifiedExpr *expr) {
    if (!expr || !expr->domain) {
        return false;
    }

    /* 有限枚举域可直接消去 */
    if (expr->domain->is_finite) {
        return true;
    }

    /* 约束图子图域可枚举节点 */
    if (expr->domain->subgraph != NULL) {
        return true;
    }

    return false;
}

/**
 * @brief 获取给定域中满足体命题的元素数量
 *
 * 对于有限域，枚举检查每个元素并统计满足的数量。
 * 对于无限域，返回 -1（无法确定）。
 *
 * @param expr 量化表达式
 * @return 满足体命题的元素数量（-1 = 无法确定）
 */
int lv_quant_count_satisfying(const lvQuantifiedExpr *expr) {
    int i;
    int count;

    if (!expr || !expr->domain || !expr->body_proposition) {
        return -1;
    }

    /* 无限域：无法确定 */
    if (!expr->domain->is_finite && !expr->domain->subgraph) {
        return -1;
    }

    /* 有限域：枚举统计 */
    count = 0;
    for (i = 0; i < expr->domain->element_count; i++) {
        lvTruthValue elem_truth = evaluate_body_for_element(expr, expr->domain->domain_elements[i]);
        if (elem_truth == lv_TRUE) {
            count++;
        }
    }

    return count;
}

/* ============== 释放结果结构体 ============== */

/**
 * @brief 释放量词操作结果结构体
 *
 * 释放结果中拥有的动态资源：
 * - error_message 字符串
 * - result_prop 命题（包括其内部字段）
 *
 * 释放后，result 结构体本身不被释放（通常为栈分配），
 * 但其所有指针字段被置为 NULL。
 *
 * @param result 结果结构体指针
 */
void lv_quant_result_destroy(lvQuantifiedResult *result) {
    if (!result) {
        return;
    }

    lv_FREE_AND_NULL(result->error_message);

    if (result->result_prop) {
        /* result_prop 由 create_result_proposition 浅构造（仅 id + label），故用 kQuantBodyPropDestroyFields */
        lv_obj_destroy_fields(result->result_prop, kQuantBodyPropDestroyFields,
                              lv_ARRAY_SIZE(kQuantBodyPropDestroyFields));
        lv_free((void **) &(result->result_prop));
    }

    /* 重置字段 */
    result->status = lv_QUANT_OK;
    result->truth_value = lv_UNKNOWN;
    result->witness_node_id = -1;
}

/* ============== 辅助函数 ============== */

/**
 * @brief 量词类型转字符串
 *
 * 将量词枚举值转换为对应的数学符号字符串。
 * 返回静态字符串常量，调用者无需释放。
 *
 * @param q 量词类型
 * @return 静态字符串：
 *         - lv_FORALL        -> "∀"
 *         - lv_EXISTS        -> "∃"
 *         - lv_EXISTS_UNIQUE -> "∃!"
 *         - 其他               -> "?(未知)"
 */
/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch）
 * ================================================================ */

/** @brief lv_quant_to_string 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_lv_quant_to_string_entries[] = {
    {"\xe2\x88\x80", lv_FORALL},
    {"\xe2\x88\x83", lv_EXISTS},
    {"\xe2\x88\x83!", lv_EXISTS_UNIQUE},
};

const char *lv_quant_to_string(lvQuantifier q) {
    return lv_enum_to_str(s_lv_quant_to_string_entries, lv_ARRAY_SIZE(s_lv_quant_to_string_entries), (int) q, "?(unknown)");
}

/**
 * @brief 量词操作结果转字符串
 *
 * 将操作结果枚举值转换为中文描述字符串。
 * 返回静态字符串常量，调用者无需释放。
 *
 * @param result 操作结果
 * @return 静态字符串（中文描述）
 */
/** @brief lv_quant_result_to_string 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_lv_quant_result_to_string_entries[] = {
    {"操作成功", lv_QUANT_OK},
    {"域为空", lv_QUANT_DOMAIN_EMPTY},
    {"域为无限，消去不可能", lv_QUANT_DOMAIN_INFINITE},
    {"变量无效", lv_QUANT_INVALID_VARIABLE},
    {"体命题未定义", lv_QUANT_BODY_UNDEFINED},
    {"实例化失败", lv_QUANT_INSTANTIATE_FAILED},
    {"泛化失败", lv_QUANT_GENERALIZE_FAILED},
    {"找到反例", lv_QUANT_COUNTEREXAMPLE},
    {"一般性错误", lv_QUANT_ERROR},
};

const char *lv_quant_result_to_string(lvQuantResult result) {
    return lv_enum_to_str(s_lv_quant_result_to_string_entries, lv_ARRAY_SIZE(s_lv_quant_result_to_string_entries), (int) result, "未知结果");
}
