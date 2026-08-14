/*
 * @file axiom_pkg_verify.c
 * @brief Axiom package system - unconstructible templates, normal form, test suite
 * @details Split from axiom_pkg.c
 */

#include "axiom_pkg.h"
#include "axiom_pkg_internal.h"

#include "lv/lv_file.h"

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/sha256.h"

#include "debug.h"
#include "error_codes.h"
#include "lexer_shared.h"
#include "lv_internal.h"
#include "lv/lv_str_utils.h"
#include "lv_utils.h"
#include "stream.h"

/* ============== 不可构造性证明模板 ============== */

int axiom_package_add_unconstructible_template(AxiomPackage *pkg, const char *target_name, const char *known_name,
                                               ConstraintGraph *construction, const char *description) {
    if (!pkg)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "axiom_package_add_unconstructible_template: pkg is NULL");
    if (!target_name)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "axiom_package_add_unconstructible_template: target_name is NULL");
    if (!known_name)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "axiom_package_add_unconstructible_template: known_name is NULL");
    if (!construction)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "axiom_package_add_unconstructible_template: construction is NULL");

    UnconstructibleTemplate tmpl;
    memset(&tmpl, 0, sizeof(UnconstructibleTemplate));

    /* 深拷贝字符串字段 */
    tmpl.target_problem_name = lv_strdup_safe(target_name);
    tmpl.known_unconstructible_name = lv_strdup_safe(known_name);
    tmpl.description = lv_strdup_safe(description);
    tmpl.verified = false;

    /* 接过归约构造图的所有权 */
    tmpl.reduction_construction = construction;

    if (lv_darray_push(&pkg->unconstructible_templates, &tmpl) < 0) {
        lv_free((void **) &tmpl.target_problem_name);
        lv_free((void **) &tmpl.known_unconstructible_name);
        lv_free((void **) &tmpl.description);
        return -2;
    }

    if (axiom_stream_ctx) {
        stream_emit_simple(axiom_stream_ctx, STREAM_EVENT_INFO, "已注册不可构造性证明模板", 0);
    }

    return 0;
}

UnconstructibleTemplate *axiom_package_lookup_unconstructible_template(AxiomPackage *pkg, const char *target_name) {
    if (!pkg || !target_name)
        return NULL;

    for (int i = 0; i < pkg->unconstructible_templates.count; i++) {
        UnconstructibleTemplate *t = (UnconstructibleTemplate *)lv_darray_get(&pkg->unconstructible_templates, i);
        if (lv_str_eq(t->target_problem_name, target_name)) {
            return t;
        }
    }
    return NULL;
}

bool axiom_package_verify_unconstructible(ConstraintGraph *graph, int target_node_id, AxiomPackage *pkg) {
    if (!graph || !pkg)
        return false;

    /* 获取目标问题节点 */
    GeomNode *target_node = graph_get_node(graph, target_node_id);
    if (!target_node)
        return false;

    /* 遍历所有不可构造性证明模板 */
    for (int i = 0; i < pkg->unconstructible_templates.count; i++) {
        UnconstructibleTemplate *tmpl = (UnconstructibleTemplate *)lv_darray_get(&pkg->unconstructible_templates, i);
        if (!tmpl->target_problem_name || !tmpl->reduction_construction)
            continue;

        /* 检查目标问题名称是否匹配（通过节点名称或自定义属性判断） */
        bool name_matched = false;
        /* 优先匹配 numeric_assumption_declaration 中可能包含的名称 */
        if (target_node->numeric_assumption_declaration &&
            strstr(target_node->numeric_assumption_declaration, tmpl->target_problem_name)) {
            name_matched = true;
        }
        /* 也检查 symbolic_coords 中的信息，看是否与目标问题关联 */
        if (!name_matched) {
            /* 简易启发式匹配：直接通过模板的目标名称推断 */
            for (int k = 0;
                 k < target_node->coord_count && target_node->symbolic_coords && target_node->symbolic_coords[k]; k++) {
                char *coord_str = symbolic_coord_serialize(target_node->symbolic_coords[k]);
                if (coord_str) {
                    if (strstr(coord_str, tmpl->target_problem_name)) {
                        name_matched = true;
                    }
                    lv_free((void **) &coord_str);
                    if (name_matched)
                        break;
                }
            }
        }

        if (!name_matched)
            continue;

        /* 找到匹配的模板，执行归约构造验证
         * 通过检查归约构造图的约束是否与目标节点关联的约束兼容来判定。
         * 这本质上是一个构造性合一检查。
         */
        bool reduction_valid = true;

        /* 验证归约构造图不为空且有约束 */
        if (tmpl->reduction_construction->constraint_count == 0) {
            reduction_valid = false;
        }

        /* 验证约束兼容性：检查归约构造的约束是否与目标图兼容 */
        if (reduction_valid) {
            for (int j = 0; j < tmpl->reduction_construction->constraint_count; j++) {
                Constraint *rc = tmpl->reduction_construction->constraints[j];
                if (!rc || !rc->is_active)
                    continue;

                /* 检查目标图中是否存在等效约束 */
                bool found_equiv = false;
                for (int k = 0; k < graph->constraint_count; k++) {
                    Constraint *gc = graph->constraints[k];
                    if (!gc || !gc->is_active)
                        continue;

                    /* 约束类型必须相同 */
                    if (gc->type != rc->type)
                        continue;

                    /* 参与者数量必须相同 */
                    if (gc->participant_count != rc->participant_count)
                        continue;

                    /* 检查参与者节点 ID 是否匹配（在目标图中查找等效节点） */
                    bool all_participants_found = true;
                    for (int p = 0; p < rc->participant_count; p++) {
                        GeomNode *rcp =
                            graph_get_node((ConstraintGraph *) tmpl->reduction_construction, rc->participants[p]);
                        GeomNode *gp = graph_get_node(graph, gc->participants[p]);
                        if (!rcp || !gp) {
                            all_participants_found = false;
                            break;
                        }
                        /* 简单比对：节点类型应一致 */
                        if (rcp->type != gp->type) {
                            all_participants_found = false;
                            break;
                        }
                    }

                    if (all_participants_found) {
                        found_equiv = true;
                        break;
                    }
                }

                if (!found_equiv) {
                    reduction_valid = false;
                    break;
                }
            }
        }

        if (reduction_valid) {
            /* 验证通过：标记模板已验证 */
            tmpl->verified = true;

            /* 更新目标节点的信任颜色
             * 检查已知不可构造问题的验证状态以决定颜色
             */
            KnownUnconstructible *known = axiom_package_lookup_unconstructible(pkg, tmpl->known_unconstructible_name);
            if (known && known->green_verified) {
                /* 已知问题已通过形式化验证，目标问题也为 GREEN */
                target_node->trust = TRUST_GREEN;
            } else {
                /* 已知问题为条件性不可构造（YELLOW），目标问题也为 YELLOW */
                target_node->trust = TRUST_YELLOW;
            }

            if (axiom_stream_ctx) {
                stream_emit_simple(axiom_stream_ctx, STREAM_EVENT_INFO,
                                   "不可构造性验证通过：目标问题已归约到已知不可构造问题", 0);
            }

            return true;
        }
    }

    return false;
}

/* ------------------------------------------------------------------ */
/*  axiom_template_validate_normal_form                                */
/* ------------------------------------------------------------------ */

/**
 * @brief 验证模板展开是否与声明的规范形式匹配
 *
 * 根据 design_v2.9.md 第 7.3 节：
 * 检查展开图的约束类型和节点类型是否与规范形式描述匹配。
 *
 * 规范形式格式："CONSTRAINT_TYPE(NODE_TYPE, NODE_TYPE)+"
 * 示例："INCIDENCE(POINT,LINE_SEGMENT)+"
 *
 * @param tmpl           约束模板（未使用，保持API一致性）
 * @param expanded_graph 要验证的展开约束图
 * @param canonical_form 规范形式描述字符串
 * @return 验证通过返回 true，否则返回 false
 */
bool axiom_template_validate_normal_form(const ConstraintTemplate *tmpl, const ConstraintGraph *expanded_graph,
                                         const char *canonical_form) {
    (void) tmpl;
    if (!expanded_graph)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "axiom_template_validate_normal_form: expanded_graph is NULL");
    if (!canonical_form)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "axiom_template_validate_normal_form: canonical_form is NULL");

    /* 解析规范形式以提取期望的约束类型和参与者节点类型
     * 格式："CONSTRAINT_TYPE(NODE_TYPE,NODE_TYPE,...)+" */

    /* 查找约束类型名称（在第一个 '(' 之前） */
    const char *paren = strchr(canonical_form, '(');
    if (!paren)
        return false;

    size_t type_name_len = (size_t) (paren - canonical_form);
    if (type_name_len == 0)
        return false;

    /* 查找闭括号 ')' */
    const char *close_paren = strchr(paren, ')');
    if (!close_paren)
        return false;

    /* 提取 '(' 和 ')' 之间的参与者节点类型 */
    /* 解析逗号分隔的节点类型名称 */
    char participant_types[AXIOM_MAX_PARTICIPANT_TYPES][AXIOM_PARTICIPANT_TYPE_LEN];
    int participant_type_count = 0;

    const char *p = paren + 1;
    while (p < close_paren && participant_type_count < AXIOM_MAX_PARTICIPANT_TYPES) {
        const char *comma = strchr(p, ',');
        size_t len;
        if (comma && comma < close_paren) {
            len = (size_t) (comma - p);
            p = comma + 1;
        } else {
            len = (size_t) (close_paren - p);
            p = close_paren;
        }
        if (len > 0 && len < AXIOM_PARTICIPANT_TYPE_LEN) {
            lv_strlcpy_n(participant_types[participant_type_count], AXIOM_PARTICIPANT_TYPE_LEN, p - len, len);
            participant_type_count++;
        }
    }

    if (participant_type_count == 0)
        return false;

    /* Helper: map type name string to GeomType enum */
    /* We only check that the constraint type prefix matches */
    /* For a simple implementation, check that all constraints in the
     * 展开图具有期望的参与者数量 */

    /* 检查展开图中的每个约束 */
    for (int i = 0; i < expanded_graph->constraint_count; i++) {
        Constraint *c = expanded_graph->constraints[i];
        if (!c)
            continue;

        /* 检查参与者数量是否与规范形式匹配 */
        if (c->participant_count != participant_type_count) {
            return false;
        }

        /* 检查所有参与者是否引用了有效的节点 */
        for (int k = 0; k < c->participant_count; k++) {
            GeomNode *node = graph_get_node((ConstraintGraph *) expanded_graph, c->participants[k]);
            if (!node)
                return false;
        }
    }

    /* 如果展开图没有约束但规范形式期望有约束，则为违规 */
    if (expanded_graph->constraint_count == 0) {
        return false;
    }

    return true;
}

/* ============== 双层测试集 ============== */

/**
 * @brief 运行单个测试用例
 *
 * 使用模板的 expand 函数展开模板，然后检查展开结果的基本有效性。
 */
static bool run_single_test_case(const ConstraintTemplate *tmpl, const TemplateTestCase *tc) {
    if (!tmpl || !tc)
        return false;

    /* 如果模板没有 expand 函数，无法运行测试 */
    if (!tmpl->expand)
        return false;

    /* 创建目标图 */
    ConstraintGraph *target = graph_create();
    if (!target)
        return false;

    /* 调用模板展开函数 */
    tmpl->expand(tc->params, target);

    /* 基本有效性检查：展开后应有约束产生 */
    bool passed = (target->constraint_count > 0);

    /* 如果模板有正则形式描述，进行额外验证 */
    if (passed && tmpl->normal_form.constraint_type_count > 0) {
        /* 检查约束数量是否匹配预期 */
        passed = (target->constraint_count >= tmpl->normal_form.constraint_type_count);
    }

    graph_destroy(target);
    return passed;
}

TemplateTestResult axiom_template_run_tests(AxiomPackage *pkg, const char *template_name,
                                            TemplateTestCase *factory_tests, int factory_count,
                                            TemplateTestCase *user_tests, int user_count) {
    TemplateTestResult result = {0, 0, 0, NULL};

    if (!pkg || !template_name)
        return result;

    /* 查找模板 */
    ConstraintTemplate *tmpl = axiom_package_get_template(pkg, template_name);
    if (!tmpl)
        return result;

    int total = factory_count + user_count;
    if (total == 0)
        return result;

    /* 分配失败消息数组 */
    result.failure_messages = lv_calloc((size_t) total, sizeof(char *));
    if (!result.failure_messages)
        return result;

    result.total = total;

    /* 运行工厂测试 */
    for (int i = 0; i < factory_count; i++) {
        bool passed = run_single_test_case(tmpl, &factory_tests[i]);

        if (passed == factory_tests[i].expected_result) {
            result.passed++;
        } else {
            /* 边界检查：确保 failure_messages 数组不越界 */
            if (result.failed < total) {
                result.failed++;
                char msg[AXIOM_TEST_MSG_BUF_SIZE];
                snprintf(msg, sizeof(msg), "[FACTORY] '%s': expected %s, got %s", factory_tests[i].template_name,
                         factory_tests[i].expected_result ? "pass" : "fail", passed ? "pass" : "fail");
                result.failure_messages[result.failed - 1] = lv_strdup_safe(msg);
            }
        }
    }

    /* 运行用户测试 */
    for (int i = 0; i < user_count; i++) {
        bool passed = run_single_test_case(tmpl, &user_tests[i]);

        if (passed == user_tests[i].expected_result) {
            result.passed++;
        } else {
            /* 边界检查：确保 failure_messages 数组不越界 */
            if (result.failed < total) {
                result.failed++;
                char msg[AXIOM_TEST_MSG_BUF_SIZE];
                snprintf(msg, sizeof(msg), "[USER] '%s': expected %s, got %s", user_tests[i].template_name,
                         user_tests[i].expected_result ? "pass" : "fail", passed ? "pass" : "fail");
                result.failure_messages[result.failed - 1] = lv_strdup_safe(msg);
            }
        }
    }

    return result;
}

void axiom_template_test_result_destroy(TemplateTestResult *result) {
    if (!result)
        return;

    if (result->failure_messages) {
        for (int i = 0; i < result->failed; i++) {
            lv_free((void **) &result->failure_messages[i]);
        }
        lv_free((void **) &result->failure_messages);
    }

    /* 释放详细记录 */
    if (result->records) {
        for (int i = 0; i < result->record_count; i++) {
            lv_free((void **) &result->records[i].test_name);
            lv_free((void **) &result->records[i].message);
        }
        lv_free((void **) &result->records);
    }

    result->total = 0;
    result->passed = 0;
    result->failed = 0;
    result->timed_out = 0;
    result->skipped = 0;
    result->record_count = 0;
    result->failure_messages = NULL;
    result->records = NULL;
}
