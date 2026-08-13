/*
 * @file prop_verifier_bhk.c
 * @brief Proposition verifier module - BHK semantics
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
#include "lv/lv_str_utils.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"
#include "lv/stream_context_util.h"
#include "lv/lv_xmacro.h" /* LV_DISPATCH / LV_DISPATCH_VOID */

/* ============================================================
 * BHK ���ι�����֤�Ž�
 * ============================================================ */

/** @brief BHK 描述函数指针类型 */
typedef void (*BHKDescFunc)(const PropFormula *f, char *buf, size_t size);

/* 前向声明 */
static void bhk_desc_atom(const PropFormula *f, char *buf, size_t size);
static void bhk_desc_conjunction(const PropFormula *f, char *buf, size_t size);
static void bhk_desc_disjunction(const PropFormula *f, char *buf, size_t size);
static void bhk_desc_implication(const PropFormula *f, char *buf, size_t size);
static void bhk_desc_negation(const PropFormula *f, char *buf, size_t size);
static void bhk_desc_bottom(const PropFormula *f, char *buf, size_t size);
static void bhk_desc_true(const PropFormula *f, char *buf, size_t size);

/** @brief 公式类型→BHK 描述函数查找表 */
static const BHKDescFunc s_bhk_desc_funcs[] = {
    [PROP_ATOM]         = bhk_desc_atom,
    [PROP_CONJUNCTION]  = bhk_desc_conjunction,
    [PROP_DISJUNCTION]  = bhk_desc_disjunction,
    [PROP_IMPLICATION]  = bhk_desc_implication,
    [PROP_NEGATION]     = bhk_desc_negation,
    [PROP_BOTTOM]       = bhk_desc_bottom,
    [PROP_TRUE]         = bhk_desc_true,
};

static void bhk_desc_atom(const PropFormula *f, char *buf, size_t size) {
    snprintf(buf, size,
             "原子命题 %s 需要一个具体证物（点、线段或圆）",
             f->data.atom.name);
}

static void bhk_desc_conjunction(const PropFormula *f, char *buf, size_t size) {
    snprintf(buf, size,
             "合取 %s 的证明是一个证明对 (a, b)，"
             "对应几何中的复合函数块（双投影端口）",
             prop_formula_to_string(f));
}

static void bhk_desc_disjunction(const PropFormula *f, char *buf, size_t size) {
    snprintf(buf, size,
             "析取 %s 的证明是一个带标签的证物（左/右），"
             "对应几何中的和类型函数块（带标签的取证件）",
             prop_formula_to_string(f));
}

static void bhk_desc_implication(const PropFormula *f, char *buf, size_t size) {
    snprintf(buf, size,
             "蕴含 %s 的证明是一个构造性函数，"
             "将前件的证明转换为后件的证物，"
             "对应几何中的标准函数块（输入端口、输出端口）",
             prop_formula_to_string(f));
}

static void bhk_desc_negation(const PropFormula *f, char *buf, size_t size) {
    snprintf(buf, size,
             "否定 %s 的证明是一个将 %s 的证明转换为 ⊥ 的构造，"
             "对应几何中的函数块（反演输入端口）",
             prop_formula_to_string(f), prop_formula_to_string(f->data.unary.operand));
}

static void bhk_desc_bottom(const PropFormula *f, char *buf, size_t size) {
    (void)f;
    snprintf(buf, size,
             "矛盾 ⊥ 没有证物（不可构造），"
             "对应几何中的空模式（无开放端口）");
}

static void bhk_desc_true(const PropFormula *f, char *buf, size_t size) {
    (void)f;
    snprintf(buf, size,
             "真 ⊤ 的证明是平凡构造（单位类型），"
             "对应几何中的单位对象");
}

/**
 * @brief 获取公式类型的 BHK 描述信息
 */
static void get_bhk_description(const PropFormula *f, char *buf, size_t size) {
    if (!f || size == 0)
        return;
    LV_DISPATCH_VOID(s_bhk_desc_funcs, f->type, f, buf, size);
}

/**
 * @brief ��ȡ��ʽ���͵ļ���ӳ������
 */
/** @brief 公式类型→几何映射描述静态查找表 */
static const char *s_geometric_mapping_table[] = {
    [PROP_ATOM]         = "GEOM_POINT / GEOM_REGION（证伪节点）",
    [PROP_CONJUNCTION]  = "FuncBlock[Product]（复合函数块，双投影端口）",
    [PROP_DISJUNCTION]  = "FuncBlock[Sum]（复合函数块，双射端口）",
    [PROP_IMPLICATION]  = "FuncBlock[Arrow]（标准函数块，输入输出端口）",
    [PROP_NEGATION]     = "FuncBlock[Neg]（否定函数块，反演端口）",
    [PROP_BOTTOM]       = "空模式/无端口（矛盾空间）",
    [PROP_TRUE]         = "平凡构造（单位类型证明）",
};

static void get_geometric_mapping(const PropFormula *f, char *buf, size_t size) {
    if (!f || size == 0)
        return;
    if ((unsigned)f->type < sizeof(s_geometric_mapping_table) / sizeof(s_geometric_mapping_table[0])
        && s_geometric_mapping_table[f->type]) {
        lv_strlcpy(buf, s_geometric_mapping_table[f->type], size);
    }
}

BHKVerificationResult prop_verifier_bhk_verify(const PropFormula **premises, int premise_count, const PropFormula *goal,
                                               const VerifierConfig *config) {
    BHKVerificationResult result;
    memset(&result, 0, sizeof(result));

    VerifierConfig default_config = VERIFIER_CONFIG_DEFAULT;
    if (!config)
        config = &default_config;

    /* ��ִ�������߼���֤ */
    VerifyDetail detail = prop_verifier_verify(premises, premise_count, goal, config);
    result.verified = (detail.result == VERIFY_PROVEN);

    /* ���� BHK ���� */
    get_bhk_description(goal, result.bhk_interpretation, sizeof(result.bhk_interpretation));

    /* ���ɼ���ӳ�� */
    get_geometric_mapping(goal, result.geometric_mapping, sizeof(result.geometric_mapping));

    if (result.verified) {
        /* ��֤�ɹ�����鹹�������� */
        result.missing_constructions = 0;
        result.missing_descriptions = NULL;
        result.missing_count = 0;
    } else {
        /* ��֤ʧ�ܣ�����ȱ�ٵĹ��� */
        char goal_atoms[32][64];
        memset(goal_atoms, 0, sizeof(goal_atoms));
        int atom_count = collect_atoms(goal, goal_atoms, 32);

        /* ͳ��ȱ�ٹ����ԭ������ */
        int missing = 0;
        for (int i = 0; i < atom_count; i++) {
            bool found = false;
            for (int j = 0; j < premise_count; j++) {
                if (premises[j]->type == PROP_ATOM && lv_str_eq(premises[j]->data.atom.name, goal_atoms[i])) {
                    found = true;
                    break;
                }
            }
            if (!found)
                missing++;
        }

        result.missing_constructions = missing;
        result.missing_count = missing;

        if (missing > 0) {
            result.missing_descriptions = (char **) lv_malloc(sizeof(char *) * (size_t) missing); /* �����ڴ� */
            if (result.missing_descriptions) {
                int idx = 0;
                for (int i = 0; i < atom_count && idx < missing; i++) {
                    bool found = false;
                    for (int j = 0; j < premise_count; j++) {
                        if (premises[j]->type == PROP_ATOM && lv_str_eq(premises[j]->data.atom.name, goal_atoms[i])) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        lvStrBuf sb_2 = {0};
                        lv_strbuf_printf(&sb_2,
                                 "ȱ��ԭ������ '%s' "
                                 "�ļ���֤���Ҫ��Ӧ�ĵ㡢�߶λ�����ڵ㣩",
                                 goal_atoms[i]);
                        result.missing_descriptions[idx] = lv_strdup_safe(sb_2.data); /* �����ַ��� */
                        lv_strbuf_destroy(&sb_2);
                        idx++;
                    }
                }
            }
        } else {
            result.missing_descriptions = NULL;
            /* ��ԭ��ǰ�ᵫ�޷����죺��������������������� */
            result.missing_count = 1;
            result.missing_descriptions = (char **) lv_malloc(sizeof(char *)); /* �����ڴ� */
            if (result.missing_descriptions) {
                lvStrBuf sb_3 = {0};
                lv_strbuf_printf(&sb_3,
                         "�޷�ͨ������ǰ�����Ϲ���Ŀ�꣨����������������"
                         "�"
                         "�");
                result.missing_descriptions[0] = lv_strdup_safe(sb_3.data); /* �����ַ��� */
                lv_strbuf_destroy(&sb_3);
            }
        }
    }

    return result;
}

void prop_verifier_free_bhk_result(BHKVerificationResult *result) {
    if (!result)
        return;
    if (result->missing_descriptions) {
        for (int i = 0; i < result->missing_count; i++) {
            lv_free((void **) &result->missing_descriptions[i]); /* �ͷŲ���NULL */
        }
        lv_free((void **) &result->missing_descriptions); /* �ͷŲ���NULL */
    }
    result->missing_count = 0;
}

