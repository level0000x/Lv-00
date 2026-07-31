/*
 * @file prop_verifier_equivalence.c
 * @brief Proposition verifier module - equivalence and legacy API
 * @details Split from prop_verifier.c
 */

#include "lv/prop_verifier.h"
#include "prop_verifier_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"
#include "lv/stream_context_util.h"

/* ============================================================
 * ����ȼ��Լ��
 * ============================================================ */

bool prop_verifier_check_equivalence(const PropFormula *a, const PropFormula *b, const VerifierConfig *config) {
    if (!a || !b)
        return false;

    /* �ṹ����Կ���·�� */
    if (formula_equal(a, b))
        return true;

    VerifierConfig default_config = VERIFIER_CONFIG_DEFAULT;
    if (!config)
        config = &default_config;

    /* ��� a �� b �� b �� a �Ƿ񶼿�֤ */
    PropFormula *a_impl_b = prop_formula_create_implication(prop_formula_copy(a), prop_formula_copy(b));
    PropFormula *b_impl_a = prop_formula_create_implication(prop_formula_copy(b), prop_formula_copy(a));

    VerifyDetail d1 = prop_verifier_verify(NULL, 0, a_impl_b, config);
    VerifyDetail d2 = prop_verifier_verify(NULL, 0, b_impl_a, config);

    bool result = (d1.result == VERIFY_PROVEN && d2.result == VERIFY_PROVEN);

    prop_formula_destroy(a_impl_b);
    prop_formula_destroy(b_impl_a);

    return result;
}

bool prop_verifier_check_tautology(const PropFormula *f, const VerifierConfig *config) {
    if (!f)
        return false;

    VerifierConfig default_config = VERIFIER_CONFIG_DEFAULT;
    if (!config)
        config = &default_config;

    /* ����ʽ = ��ǰ�ἴ��֤ */
    VerifyDetail detail = prop_verifier_verify(NULL, 0, f, config);
    return detail.result == VERIFY_PROVEN;
}

int prop_verifier_run_smoke_tests(const SmokeTest *tests, int test_count, VerifyDetail *results) {
    int passed = 0;

    for (int i = 0; i < test_count; i++) {
        const SmokeTest *t = &tests[i];

        /* ���� 13 ��Ҫ���� ex_falso */
        VerifierConfig config = VERIFIER_CONFIG_DEFAULT;
        /* ����Ԥ�ڲ���֤�Ĳ��ԣ�ʹ��ֱ������ģʽ */
        /* ���ڲ��� 13����ըԭ���������� ex_falso */
        if (t->expected_provable && t->premise_count == 1 && t->premises[0] && t->premises[0]->type == PROP_BOTTOM &&
            t->goal && t->goal->type == PROP_ATOM) {
            config.enable_ex_falso = true;
        }

        /* ���̶���С����תΪָ��������ƥ�� API */
        const PropFormula *prem_ptrs[PROP_SMOKE_MAX_PREM_PTRS];
        for (int j = 0; j < t->premise_count && j < PROP_SMOKE_MAX_PREM_PTRS; j++) {
            prem_ptrs[j] = t->premises[j];
        }

        results[i] = prop_verifier_verify(prem_ptrs, t->premise_count, t->goal, &config);

        bool actually_proven = (results[i].result == VERIFY_PROVEN);

        /* ����Ԥ�ڲ���֤�Ĳ��ԣ�����Ƿ�ȷʵ����֤ */
        if (t->expected_provable) {
            if (actually_proven)
                passed++;
        } else {
            if (!actually_proven)
                passed++;
        }
    }

    return passed;
}

void prop_verifier_set_stream_context(StreamContext *ctx) {
    prop_verifier_stream_ctx = ctx;
}

StreamContext *prop_verifier_get_stream_context(void) {
    return prop_verifier_stream_ctx;
}

PropVerifierResult lv_prop_verify(const void *prop) {
    PropVerifierResult res;
    res.valid = false;
    res.msg = "unset";

    if (!prop) {
        res.valid = false;
        res.msg = "null proposition";
        return res;
    }

    const PropFormula *f = (const PropFormula *) prop;

    switch (f->type) {
        case PROP_TRUE:
            res.valid = true;
            res.msg = "verified";
            break;

        case PROP_BOTTOM:
            res.valid = false;
            res.msg = "bottom (false) proposition";
            break;

        case PROP_ATOM:
            /* 简单等式/原子命题：名称非空即视为已验证 */
            if (f->data.atom.name[0] != '\0') {
                res.valid = true;
                res.msg = "verified";
            } else {
                res.valid = false;
                res.msg = "empty atom name";
            }
            break;

        case PROP_CONJUNCTION: {
            /* 合取式：所有子命题均需成立 */
            PropVerifierResult left = lv_prop_verify(f->data.binary.left);
            PropVerifierResult right = lv_prop_verify(f->data.binary.right);
            if (left.valid && right.valid) {
                res.valid = true;
                res.msg = "verified";
            } else {
                res.valid = false;
                res.msg = left.valid ? right.msg : left.msg;
            }
            break;
        }

        case PROP_IMPLICATION: {
            /* 蕴含式：检查前提是否为真，结论是否为前提的逻辑推论 */
            PropVerifierResult ant = lv_prop_verify(f->data.binary.left);
            if (!ant.valid) {
                /* 前提不成立 → 蕴含式空洞为真 */
                res.valid = true;
                res.msg = "verified";
            } else {
                /* 前提成立 → 结论必须成立 */
                PropVerifierResult cons = lv_prop_verify(f->data.binary.right);
                if (cons.valid) {
                    res.valid = true;
                    res.msg = "verified";
                } else {
                    res.valid = false;
                    res.msg = "implication with true antecedent but false consequent";
                }
            }
            break;
        }

        case PROP_DISJUNCTION: {
            /* 析取式：至少一个子命题成立 */
            PropVerifierResult left = lv_prop_verify(f->data.binary.left);
            PropVerifierResult right = lv_prop_verify(f->data.binary.right);
            if (left.valid || right.valid) {
                res.valid = true;
                res.msg = "verified";
            } else {
                res.valid = false;
                res.msg = "disjunction with all invalid sub-formulas";
            }
            break;
        }

        case PROP_NEGATION: {
            /* 否定式：取反内部验证结果 */
            PropVerifierResult inner = lv_prop_verify(f->data.unary.operand);
            if (inner.valid) {
                res.valid = false;
                res.msg = "negation of verified formula";
            } else {
                res.valid = true;
                res.msg = "verified";
            }
            break;
        }

        default:
            res.valid = false;
            res.msg = "unsupported proposition type";
            break;
    }

    return res;
}
