/**
 * @file quantifier_inst.c
 * @brief 量词实例化与存在量词运算（由 quantifier.c 拆分子模块）
 *
 * @details lv_quantifier_instantiate / generalize 与存在量词引入。
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

/* ============== 量词实例化 ============== */

/**
 * @brief 量词实例化（∀-消去）
 *
 * 从 ∀x.P(x) 推导出 P(t)，其中 t 必须在域中。
 * 这是全称量词的消去规则（∀E）。
 *
 * 实例化后：
 * - 将 instance_id 记录到 instantiated_ids 列表中
 * - 创建结果命题（简化为设置 id 和 name）
 * - 设置 truth_value 为体命题在 instance_id 上的评估值
 *
 * @param expr         量化表达式（须为 ∀ 类型）
 * @param instance_id  要代入的实例节点ID
 * @param out_result   输出结果
 * @return 操作结果状态码
 */
lvQuantResult lv_quantifier_instantiate(const lvQuantifiedExpr *expr, int instance_id, lvQuantifiedResult *out_result) {
    char name_buf[RESULT_NAME_BUF_SIZE];
    const char *quant_str;

    init_quant_result(out_result);

    /* 参数校验 */
    if (!expr || !out_result) {
        out_result->status = lv_QUANT_ERROR;
        return lv_QUANT_ERROR;
    }

    if (expr->quantifier != lv_FORALL) {
        out_result->status = lv_QUANT_INSTANTIATE_FAILED;
        out_result->error_message = lv_strdup("实例化仅适用于全称量词(∀)");
        return lv_QUANT_INSTANTIATE_FAILED;
    }

    if (!expr->domain) {
        out_result->status = lv_QUANT_DOMAIN_EMPTY;
        out_result->error_message = lv_strdup("域未定义");
        return lv_QUANT_DOMAIN_EMPTY;
    }

    /* 检查实例是否在域中 */
    if (!lv_quant_domain_contains(expr->domain, instance_id)) {
        out_result->status = lv_QUANT_INVALID_VARIABLE;
        out_result->error_message = lv_strdup("实例不在量化域中");
        return lv_QUANT_INVALID_VARIABLE;
    }

    if (!expr->body_proposition) {
        out_result->status = lv_QUANT_BODY_UNDEFINED;
        out_result->error_message = lv_strdup("体命题未定义");
        return lv_QUANT_BODY_UNDEFINED;
    }

    /* 记录实例化（需要修改 expr，但函数签名为 const，此处通过结果反映） */
    /* 注意：由于 expr 为 const，无法直接修改 instantiated_ids。
       调用者若需追踪，应在外部管理。此处仅生成结果。 */

    /* 评估体命题在实例上的真值 */
    out_result->truth_value = evaluate_body_for_element(expr, instance_id);

    /* 创建结果命题 */
    quant_str = lv_quant_to_string(expr->quantifier);
    (void) lv_snprintf(name_buf, RESULT_NAME_BUF_SIZE, "%s%s(%d).P(%d)", quant_str,
                    expr->variable_name ? expr->variable_name : "x", expr->id, instance_id);

    out_result->result_prop = create_result_proposition(expr->body_proposition->id, name_buf);
    if (!out_result->result_prop) {
        out_result->status = lv_QUANT_ERROR;
        out_result->error_message = lv_strdup("创建结果命题失败");
        return lv_QUANT_ERROR;
    }

    out_result->witness_node_id = instance_id;
    out_result->status = lv_QUANT_OK;

    return lv_QUANT_OK;
}

/**
 * @brief 量词泛化（∀-引入）
 *
 * 从 P(x) 对任意 x∈D 成立推导出 ∀x.P(x)。
 * 前提条件（特征变量条件）：
 * - x 不能在前提集中自由出现
 * - 域必须有限（或已验证所有元素）
 *
 * 泛化操作：
 * 1. 检查域是否为有限域
 * 2. 评估体命题在所有域元素上的真值
 * 3. 若全部为 TRUE，则泛化成功
 *
 * @param expr         量化表达式模板（须为 ∀ 类型）
 * @param out_result   输出结果
 * @return 操作结果状态码
 */
lvQuantResult lv_quantifier_generalize(const lvQuantifiedExpr *expr, lvQuantifiedResult *out_result) {
    char name_buf[RESULT_NAME_BUF_SIZE];
    const char *quant_str;
    lvTruthValue truth;

    init_quant_result(out_result);

    if (!expr || !out_result) {
        out_result->status = lv_QUANT_ERROR;
        return lv_QUANT_ERROR;
    }

    if (expr->quantifier != lv_FORALL) {
        out_result->status = lv_QUANT_GENERALIZE_FAILED;
        out_result->error_message = lv_strdup("泛化仅适用于全称量词(∀)");
        return lv_QUANT_GENERALIZE_FAILED;
    }

    if (!expr->body_proposition) {
        out_result->status = lv_QUANT_BODY_UNDEFINED;
        out_result->error_message = lv_strdup("体命题未定义");
        return lv_QUANT_BODY_UNDEFINED;
    }

    /* 检查域是否有限 */
    if (!expr->domain->is_finite && !expr->domain->subgraph) {
        out_result->status = lv_QUANT_DOMAIN_INFINITE;
        out_result->error_message = lv_strdup("无法在无限域上泛化");
        return lv_QUANT_DOMAIN_INFINITE;
    }

    /* 评估量化表达式的真值（通过有限域枚举评估） */
    truth = lv_UNKNOWN;

    /* 对有限域枚举评估 */
    if (expr->domain->is_finite && expr->domain->element_count >= 0) {
        int i;
        truth = lv_TRUE;
        for (i = 0; i < expr->domain->element_count; i++) {
            lvTruthValue elem_truth = evaluate_body_for_element(expr, expr->domain->domain_elements[i]);
            truth = lv_tvl_and(truth, elem_truth);
            if (truth == lv_FALSE) {
                break;
            }
        }
    }

    out_result->truth_value = truth;

    /* 创建结果命题 */
    quant_str = lv_quant_to_string(expr->quantifier);
    (void) lv_snprintf(name_buf, RESULT_NAME_BUF_SIZE, "%s%s∈D.P(%s)", quant_str,
                    expr->variable_name ? expr->variable_name : "x", expr->variable_name ? expr->variable_name : "x");

    out_result->result_prop = create_result_proposition(expr->id, name_buf);
    if (!out_result->result_prop) {
        out_result->status = lv_QUANT_ERROR;
        out_result->error_message = lv_strdup("创建结果命题失败");
        return lv_QUANT_ERROR;
    }

    if (truth == lv_TRUE) {
        out_result->status = lv_QUANT_OK;
    } else if (truth == lv_FALSE) {
        out_result->status = lv_QUANT_COUNTEREXAMPLE;
        out_result->error_message = lv_strdup("泛化失败：存在不满足体命题的元素");
    } else {
        out_result->status = lv_QUANT_GENERALIZE_FAILED;
        out_result->error_message = lv_strdup("泛化失败：无法确定所有元素是否满足体命题");
    }

    return out_result->status;
}

/* ============== 存在量词运算 ============== */

/**
 * @brief 存在量词引入（∃I）
 *
 * 从 P(t) 推导出 ∃x.P(x)，其中 t 必须在域中。
 * t 称为"目击者"（witness），证明存在性。
 *
 * @param expr         待填充的量化表达式（量词须为 ∃）
 * @param witness_id   目击者节点ID
 * @param out_result   输出结果
 * @return 操作结果状态码
 */
lvQuantResult lv_quant_exists_introduce(lvQuantifiedExpr *expr, int witness_id, lvQuantifiedResult *out_result) {
    char name_buf[RESULT_NAME_BUF_SIZE];
    const char *quant_str;

    init_quant_result(out_result);

    if (!expr || !out_result) {
        out_result->status = lv_QUANT_ERROR;
        return lv_QUANT_ERROR;
    }

    if (expr->quantifier != lv_EXISTS) {
        out_result->status = lv_QUANT_INSTANTIATE_FAILED;
        out_result->error_message = lv_strdup("存在引入仅适用于存在量词(∃)");
        return lv_QUANT_INSTANTIATE_FAILED;
    }

    if (!expr->domain) {
        out_result->status = lv_QUANT_DOMAIN_EMPTY;
        out_result->error_message = lv_strdup("域未定义");
        return lv_QUANT_DOMAIN_EMPTY;
    }

    /* 检查目击者是否在域中 */
    if (!lv_quant_domain_contains(expr->domain, witness_id)) {
        out_result->status = lv_QUANT_INVALID_VARIABLE;
        out_result->error_message = lv_strdup("目击者不在量化域中");
        return lv_QUANT_INVALID_VARIABLE;
    }

    if (!expr->body_proposition) {
        out_result->status = lv_QUANT_BODY_UNDEFINED;
        out_result->error_message = lv_strdup("体命题未定义");
        return lv_QUANT_BODY_UNDEFINED;
    }

    /* 评估体命题在目击者上的真值 */
    out_result->truth_value = evaluate_body_for_element(expr, witness_id);

    if (out_result->truth_value != lv_TRUE) {
        out_result->status = lv_QUANT_INSTANTIATE_FAILED;
        out_result->error_message = lv_strdup("目击者不满足体命题，无法引入存在量词");
        return lv_QUANT_INSTANTIATE_FAILED;
    }

    /* 记录目击者 */
    out_result->witness_node_id = witness_id;

    /* 创建结果命题 */
    quant_str = lv_quant_to_string(expr->quantifier);
    (void) lv_snprintf(name_buf, RESULT_NAME_BUF_SIZE, "%s%s∈D.P(%s)", quant_str,
                    expr->variable_name ? expr->variable_name : "x", expr->variable_name ? expr->variable_name : "x");

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
 * @brief 存在量词消去（∃E）
 *
 * 从 ∃x.P(x) 和 ∀y.(P(y)→Q) 推导出 Q（其中 y 不在 Q 中自由出现）。
 *
 * 实现：
 * 1. 验证存在量化表达式
 * 2. 在有限域上找到满足体命题的目击者（遍历搜索）
 * 3. 若找到目击者，构造实例化结果；否则报告失败
 *
 * @param exists_expr  存在量化表达式
 * @param target_prop  目标命题 Q
 * @param out_result   输出结果
 * @return 操作结果状态码
 */
lvQuantResult lv_quant_exists_eliminate(const lvQuantifiedExpr *exists_expr, struct Proposition *target_prop,
                                        lvQuantifiedResult *out_result) {
    int i;
    char name_buf[RESULT_NAME_BUF_SIZE];

    init_quant_result(out_result);

    if (!exists_expr || !out_result) {
        out_result->status = lv_QUANT_ERROR;
        return lv_QUANT_ERROR;
    }

    if (exists_expr->quantifier != lv_EXISTS) {
        out_result->status = lv_QUANT_INSTANTIATE_FAILED;
        out_result->error_message = lv_strdup("存在消去仅适用于存在量词(∃)");
        return lv_QUANT_INSTANTIATE_FAILED;
    }

    if (!exists_expr->body_proposition) {
        out_result->status = lv_QUANT_BODY_UNDEFINED;
        out_result->error_message = lv_strdup("体命题未定义");
        return lv_QUANT_BODY_UNDEFINED;
    }

    /* 在有限域上寻找目击者 */
    if (exists_expr->domain && exists_expr->domain->is_finite) {
        for (i = 0; i < exists_expr->domain->element_count; i++) {
            lvTruthValue elem_truth = evaluate_body_for_element(exists_expr, exists_expr->domain->domain_elements[i]);
            if (elem_truth == lv_TRUE) {
                out_result->witness_node_id = exists_expr->domain->domain_elements[i];
                break;
            }
        }
    }

    if (out_result->witness_node_id < 0) {
        out_result->status = lv_QUANT_INSTANTIATE_FAILED;
        out_result->error_message = lv_strdup("未找到满足体命题的目击者");
        out_result->truth_value = lv_FALSE;
        return lv_QUANT_INSTANTIATE_FAILED;
    }

    /* 使用目标命题作为结果 */
    if (target_prop) {
        (void) lv_snprintf(name_buf, RESULT_NAME_BUF_SIZE, "ElimE_%s_%d",
                        exists_expr->variable_name ? exists_expr->variable_name : "x", exists_expr->id);
        out_result->result_prop = create_result_proposition(target_prop->id, name_buf);
    }

    out_result->truth_value = lv_TRUE;
    out_result->status = lv_QUANT_OK;

    return lv_QUANT_OK;
}

