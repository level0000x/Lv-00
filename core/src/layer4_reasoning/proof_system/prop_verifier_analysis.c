/*
 * @file prop_verifier_analysis.c
 * @brief Proposition verifier module - inconstructibility analysis
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
 * ���ɹ����Է���
 * ============================================================ */

/**
 * @brief �ռ�Ŀ�깫ʽ������ԭ���ӹ�ʽ
 *
 * �ݹ������ʽ AST���ռ�����ԭ���������ơ�
 * ���ڷ���֤��ʧ��ʱ��Щԭ������ȱ�ٹ��졣
 */
int collect_atoms(const PropFormula *f, char atoms[][PROP_ATOM_NAME_MAX_LEN], int max_atoms) {
    if (!f)
        return 0;
    switch (f->type) {
        case PROP_ATOM: {
            /* ȥ�ؼ�� */
            for (int i = 0; i < max_atoms; i++) {
                if (atoms[i][0] == '\0')
                    break;
                if (strcmp(atoms[i], f->data.atom.name) == 0)
                    return 0;
            }
            for (int i = 0; i < max_atoms; i++) {
                if (atoms[i][0] == '\0') {
                    snprintf(atoms[i], PROP_ATOM_NAME_MAX_LEN, "%s", f->data.atom.name);
                    return 1;
                }
            }
            return 0;
        }
        case PROP_CONJUNCTION:
        case PROP_DISJUNCTION:
        case PROP_IMPLICATION:
            return collect_atoms(f->data.binary.left, atoms, max_atoms) +
                   collect_atoms(f->data.binary.right, atoms, max_atoms);
        case PROP_NEGATION:
            return collect_atoms(f->data.unary.operand, atoms, max_atoms);
        case PROP_BOTTOM:
        case PROP_TRUE:
            return 0;
        default:
            break;
    }
    return 0;
}

/**
 * @brief ���Ŀ�깫ʽ�Ƿ���������߼����е�ģʽ
 *
 * ʶ������ֱ�����岻��֤�ľ���ģʽ��
 *   - ˫�ط���ȥ��~~A �� A
 *   - �����ɣ�A �� ~A
 *   - ��֤����RAA����(~A �� ��) �� A
 */
bool has_classical_pattern(const PropFormula *f, char *pattern_desc, size_t desc_size) {
    if (!f)
        return false;

    /* ��������ɣ�A �� ~A �� ~A �� A */
    if (f->type == PROP_DISJUNCTION) {
        const PropFormula *left = f->data.binary.left;
        const PropFormula *right = f->data.binary.right;
        /* A �� ~A */
        if (left->type == PROP_NEGATION && formula_equal(left->data.unary.operand, right)) {
            char *s = prop_formula_to_string(right);
            snprintf(pattern_desc, desc_size,
                     "������ (LEM): %s \\/ ~%s��ֱ�������߼��в���֤��", s,
                     s);
            lv_free((void **) &s);
            return true;
        }
        /* ~A �� A */
        if (right->type == PROP_NEGATION && formula_equal(right->data.unary.operand, left)) {
            char *s = prop_formula_to_string(left);
            snprintf(pattern_desc, desc_size,
                     "������ (LEM): ~%s \\/ %s��ֱ�������߼��в���֤��", s,
                     s);
            lv_free((void **) &s);
            return true;
        }
    }

    /* ���˫�ط���ȥ��~~A �� A ��ǰ�� ~~A ? A */
    if (f->type == PROP_IMPLICATION) {
        const PropFormula *antecedent = f->data.binary.left;
        const PropFormula *consequent = f->data.binary.right;
        if (antecedent->type == PROP_NEGATION && antecedent->data.unary.operand->type == PROP_NEGATION &&
            formula_equal(antecedent->data.unary.operand->data.unary.operand, consequent)) {
            char *s = prop_formula_to_string(consequent);
            snprintf(pattern_desc, desc_size,
                     "˫�ط���ȥ: ~~%s �� %s��ֱ�������߼��в���֤��", s,
                     s);
            lv_free((void **) &s);
            return true;
        }
        /* ��֤�� (RAA): (~A �� ��) �� A */
        if (antecedent->type == PROP_IMPLICATION && antecedent->data.binary.left->type == PROP_NEGATION &&
            antecedent->data.binary.right->type == PROP_BOTTOM &&
            formula_equal(antecedent->data.binary.left->data.unary.operand, consequent)) {
            char *s = prop_formula_to_string(consequent);
            snprintf(pattern_desc, desc_size,
                     "��֤�� (RAA): (~%s �� _|_) �� "
                     "%s��ֱ�������߼��в���֤��",
                     s, s);
            lv_free((void **) &s);
            return true;
        }
    }

    /* �ݹ����ӹ�ʽ */
    char sub_desc[PROP_PATTERN_DESC_BUFSIZE];
    switch (f->type) {
        case PROP_CONJUNCTION:
        case PROP_DISJUNCTION:
        case PROP_IMPLICATION:
            if (has_classical_pattern(f->data.binary.left, sub_desc, sizeof(sub_desc))) {
                snprintf(pattern_desc, desc_size, "%s", sub_desc);
                return true;
            }
            if (has_classical_pattern(f->data.binary.right, sub_desc, sizeof(sub_desc))) {
                snprintf(pattern_desc, desc_size, "%s", sub_desc);
                return true;
            }
            break;
        case PROP_NEGATION:
            if (has_classical_pattern(f->data.unary.operand, sub_desc, sizeof(sub_desc))) {
                snprintf(pattern_desc, desc_size, "%s", sub_desc);
                return true;
            }
            break;
        default:
            break;
    }
    return false;
}

InconstructibilityAnalysis prop_verifier_analyze_inconstructibility(const PropFormula **premises, int premise_count,
                                                                    const PropFormula *goal,
                                                                    const VerifierConfig *config) {
    InconstructibilityAnalysis analysis;
    memset(&analysis, 0, sizeof(analysis));

    VerifierConfig default_config = VERIFIER_CONFIG_DEFAULT;
    if (!config)
        config = &default_config;

    /* ��ִ����֤ */
    VerifyDetail detail = prop_verifier_verify(premises, premise_count, goal, config);

    if (detail.result == VERIFY_PROVEN) {
        analysis.is_inconstructible = false;
        snprintf(analysis.reason, sizeof(analysis.reason), "������֤��Ϊ�ɹ��죬���費�ɹ����Է���");
        return analysis;
    }

    analysis.is_inconstructible = true;

    /* ����Ƿ���������߼�ģʽ */
    char pattern_desc[PROP_PATTERN_DESC_BUFSIZE] = {0};
    if (config->use_intuitionistic && has_classical_pattern(goal, pattern_desc, sizeof(pattern_desc))) {
        snprintf(analysis.reason, sizeof(analysis.reason),
                 "ֱ����������: "
                 "%s����ֱ�������߼��У�֤�������ṩ��ʽ���죬"
                 "�������������ɻ�˫�ط���ȥ�Ⱦ�����������",
                 pattern_desc);
    } else if (detail.result == VERIFY_TIMEOUT) {
        snprintf(analysis.reason, sizeof(analysis.reason),
                 "������ʱ: ֤�������� %d ������δ��ɡ�"
                 "������Ҫ���ಽ�����ڸ��ӵ���Ŀ��������ϵ��",
                 config->timeout_ms);
    } else {
        /* ����ȱ�ٵ�ǰ�����Ŀ�� */
        char goal_atoms[PROP_ATOM_COLLECT_MAX][PROP_ATOM_NAME_MAX_LEN];
        memset(goal_atoms, 0, sizeof(goal_atoms));
        int atom_count = collect_atoms(goal, goal_atoms, PROP_ATOM_COLLECT_MAX);

        /* �����ЩĿ��ԭ�Ӳ���ǰ���� */
        char missing[512] = {0};
        int missing_count = 0;
        for (int i = 0; i < atom_count; i++) {
            bool found = false;
            for (int j = 0; j < premise_count; j++) {
                if (premises[j]->type == PROP_ATOM && strcmp(premises[j]->data.atom.name, goal_atoms[i]) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                lvStrBuf sb = {0};
                lv_strbuf_printf(&sb, "%s%s", missing_count > 0 ? ", " : "", goal_atoms[i]);
                lv_strncat(missing, sb.data, sizeof(missing));
                missing_count++;
                lv_strbuf_destroy(&sb);
            }
        }

        if (missing_count > 0) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(analysis.reason, sizeof(analysis.reason),
                     "ȱ�ٹ���: Ŀ����Ҫԭ������ [%s] �Ĺ��죬"
                     "����ǰǰ����δ�ṩ���� BHK �����£�"
                     "ÿ��ԭ��������Ҫһ������֤��㡢�߶λ����򣩡�",
                     missing);
        } else {
            snprintf(analysis.reason, sizeof(analysis.reason),
                     "����ȱ��: "
                     "ǰ���а�������Ŀ��ԭ�����⣬���޷�ͨ��"
                     "��������������ϳ�Ŀ�ꡣ������Ҫ������̺�ǰ��"
                     "������ӵĹ��첽�衣��ʹ�� %d ��������",
                     detail.steps_used);
#pragma GCC diagnostic pop
        }
    }

    /* ����ʧ����Ŀ������ */
    analysis.failed_subgoals = 1;
    analysis.subgoal_descriptions = (char **) lv_malloc(sizeof(char *)); /* �����ڴ� */
    if (analysis.subgoal_descriptions) {
        analysis.subgoal_descriptions[0] = (char *) lv_malloc(512); /* �����ڴ� */
        if (analysis.subgoal_descriptions[0]) {
            snprintf(analysis.subgoal_descriptions[0], 512, "Ŀ��: %s | ״̬: %s | ����: %d/%d",
                     prop_formula_to_string(goal), detail.result == VERIFY_TIMEOUT ? "��ʱ" : "�����ռ�ľ�",
                     detail.steps_used, detail.max_steps);
        }
        analysis.subgoal_desc_count = 1;
    }

    return analysis;
}

void prop_verifier_free_analysis(InconstructibilityAnalysis *analysis) {
    if (!analysis)
        return;
    if (analysis->subgoal_descriptions) {
        for (int i = 0; i < analysis->subgoal_desc_count; i++) {
            lv_free((void **) &analysis->subgoal_descriptions[i]); /* �ͷŲ���NULL */
        }
        lv_free((void **) &analysis->subgoal_descriptions); /* �ͷŲ���NULL */
    }
    analysis->subgoal_desc_count = 0;
}

