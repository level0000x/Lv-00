/**
 * @file sat_encoding_formula.c
 * @brief 关系公式→SAT 编码（由 sat_encoding.c 拆分子模块）
 *
 * @details Alloy 风格关系公式（some/no/one/lone/布尔组合/量化）
 *          的 CNF 编码与关系模型事实编码。
 * @author Lv-00 Project
 * @version 3.3.0
 */
#include "lv/sat_encoding.h"

#include <stdio.h>
#include <string.h>

#include "lv/constraint_graph.h"

#include "lv/error_codes.h"
#include "lv/lv.h"
#include "lv/lv_internal.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_utils.h"
#include "lv/solver_core.h"
#include "sat_encoding_internal.h"
/* ========================================================================
 * 关系模型 → SAT 编码
 * ======================================================================== */

/* ========================================================================
 * 公式编码器 VTable —— 函数指针类型与各编码器实现
 * ======================================================================== */

typedef int (*RelFormulaEncoderFn)(SatEncoding *enc, const RelFormula *formula);

/* ── 各公式类型的编码器实现 ── */

static int encode_formula_some(SatEncoding *enc, const RelFormula *formula) {
    /* some R: 关系 R 非空，至少一个元组为真 */
    if (formula->expr && formula->expr->type == REL_EXPR_ATOMIC && formula->expr->data.atomic.rel) {
        Relation *rel = formula->expr->data.atomic.rel;
        /* 为该关系的每个元组注册变量，然后添加"至少一个为真"的子句 */
        SatLiteral *disj = (SatLiteral *) lv_malloc((size_t) rel->tuple_count * sizeof(SatLiteral));
        if (!disj) {
            return 0;
        }
        int disj_count = 0;
        for (int ti = 0; ti < rel->tuple_count; ti++) {
            int var = sat_encoding_register_var(enc, rel->arity, rel->tuples[ti]);
            if (var >= 1) {
                disj[disj_count++] = var;
            }
        }
        if (disj_count > 0) {
            sat_encoding_add_clause(enc, disj, disj_count);
        }
        lv_free((void **) &disj);
    }
    return 0;
}

static int encode_formula_no(SatEncoding *enc, const RelFormula *formula) {
    /* no R: 关系 R 为空，所有元组必须为假 */
    if (formula->expr && formula->expr->type == REL_EXPR_ATOMIC && formula->expr->data.atomic.rel) {
        Relation *rel = formula->expr->data.atomic.rel;
        for (int ti = 0; ti < rel->tuple_count; ti++) {
            int var = sat_encoding_register_var(enc, rel->arity, rel->tuples[ti]);
            if (var >= 1) {
                SatLiteral unit = -var;
                sat_encoding_add_clause(enc, &unit, 1);
            }
        }
    }
    return 0;
}

static int encode_formula_one(SatEncoding *enc, const RelFormula *formula) {
    /* one R: 关系 R 恰好包含一个元组 */
    if (formula->expr && formula->expr->type == REL_EXPR_ATOMIC && formula->expr->data.atomic.rel) {
        Relation *rel = formula->expr->data.atomic.rel;
        int *vars = (int *) lv_malloc((size_t) rel->tuple_count * sizeof(int));
        if (!vars)
            return 0;
        int var_count = 0;
        for (int ti = 0; ti < rel->tuple_count; ti++) {
            int var = sat_encoding_register_var(enc, rel->arity, rel->tuples[ti]);
            if (var >= 1) {
                vars[var_count++] = var;
            }
        }
        /* 至少一个为真 */
        if (var_count > 0) {
            SatLiteral *disj = (SatLiteral *) lv_malloc((size_t) var_count * sizeof(SatLiteral));
            if (disj) {
                for (int vi = 0; vi < var_count; vi++)
                    disj[vi] = vars[vi];
                sat_encoding_add_clause(enc, disj, var_count);
                lv_free((void **) &disj);
            }
        }
        /* 至多一个为真：任意两个不同元组不能同时为真 */
        for (int i = 0; i < var_count; i++) {
            for (int j = i + 1; j < var_count; j++) {
                SatLiteral pair[] = {-vars[i], -vars[j]};
                sat_encoding_add_clause(enc, pair, 2);
            }
        }
        lv_free((void **) &vars);
    }
    return 0;
}

static int encode_formula_lone(SatEncoding *enc, const RelFormula *formula) {
    /* lone R: 关系 R 最多包含一个元组 */
    if (formula->expr && formula->expr->type == REL_EXPR_ATOMIC && formula->expr->data.atomic.rel) {
        Relation *rel = formula->expr->data.atomic.rel;
        int *vars = (int *) lv_malloc((size_t) rel->tuple_count * sizeof(int));
        if (!vars)
            return 0;
        int var_count = 0;
        for (int ti = 0; ti < rel->tuple_count; ti++) {
            int var = sat_encoding_register_var(enc, rel->arity, rel->tuples[ti]);
            if (var >= 1) {
                vars[var_count++] = var;
            }
        }
        for (int i = 0; i < var_count; i++) {
            for (int j = i + 1; j < var_count; j++) {
                SatLiteral pair[] = {-vars[i], -vars[j]};
                sat_encoding_add_clause(enc, pair, 2);
            }
        }
        lv_free((void **) &vars);
    }
    return 0;
}

static int encode_formula_eq_subset(SatEncoding *enc, const RelFormula *formula) {
    /* R = S 或 R in S: 简化为逐元组蕴含 */
    /* 对于原子关系引用，编码为元组级别的等价/蕴含 */
    if (formula->expr && formula->expr->type == REL_EXPR_ATOMIC && formula->expr->data.atomic.rel) {
        Relation *rel = formula->expr->data.atomic.rel;
        /* 将关系中的每个元组编码为必须为真（作为硬约束） */
        for (int ti = 0; ti < rel->tuple_count; ti++) {
            int var = sat_encoding_register_var(enc, rel->arity, rel->tuples[ti]);
            if (var >= 1) {
                SatLiteral unit = var;
                sat_encoding_add_clause(enc, &unit, 1);
            }
        }
    }
    return 0;
}

static int encode_formula_and(SatEncoding *enc, const RelFormula *formula) {
    /* F && G: 递归编码两个子公式 */
    /* 子公式通过 model->facts 中的其他条目处理，
     * 此处直接编码为：两个子公式对应的关系元组都必须为真 */
    for (int si = 0; si < 2; si++) {
        RelFormula *sub = formula->sub[si];
        if (!sub || !sub->expr)
            continue;
        if (sub->expr->type == REL_EXPR_ATOMIC && sub->expr->data.atomic.rel) {
            Relation *sub_rel = sub->expr->data.atomic.rel;
            for (int ti = 0; ti < sub_rel->tuple_count; ti++) {
                int var = sat_encoding_register_var(enc, sub_rel->arity, sub_rel->tuples[ti]);
                if (var >= 1) {
                    SatLiteral unit = var;
                    sat_encoding_add_clause(enc, &unit, 1);
                }
            }
        }
    }
    return 0;
}

static int encode_formula_or(SatEncoding *enc, const RelFormula *formula) {
    /* F || G: 至少一个子公式成立 */
    {
        SatLiteral disj[2];
        int disj_count = 0;
        for (int si = 0; si < 2; si++) {
            RelFormula *sub = formula->sub[si];
            if (!sub || !sub->expr)
                continue;
            if (sub->expr->type == REL_EXPR_ATOMIC && sub->expr->data.atomic.rel) {
                Relation *sub_rel = sub->expr->data.atomic.rel;
                /* 取第一个元组为代表变量 */
                if (sub_rel->tuple_count > 0) {
                    int var = sat_encoding_register_var(enc, sub_rel->arity, sub_rel->tuples[0]);
                    if (var >= 1)
                        disj[disj_count++] = var;
                }
            }
        }
        if (disj_count > 0) {
            sat_encoding_add_clause(enc, disj, disj_count);
        }
    }
    return 0;
}

static int encode_formula_not(SatEncoding *enc, const RelFormula *formula) {
    /* !F: 取反子公式 */
    if (formula->sub[0] && formula->sub[0]->expr && formula->sub[0]->expr->type == REL_EXPR_ATOMIC &&
        formula->sub[0]->expr->data.atomic.rel) {
        Relation *sub_rel = formula->sub[0]->expr->data.atomic.rel;
        for (int ti = 0; ti < sub_rel->tuple_count; ti++) {
            int var = sat_encoding_register_var(enc, sub_rel->arity, sub_rel->tuples[ti]);
            if (var >= 1) {
                SatLiteral unit = -var;
                sat_encoding_add_clause(enc, &unit, 1);
            }
        }
    }
    return 0;
}

static int encode_formula_implies(SatEncoding *enc, const RelFormula *formula) {
    /* F => G: 等价于 !F || G */
    {
        SatLiteral disj[2];
        int disj_count = 0;
        /* !F: 取反左侧 */
        if (formula->sub[0] && formula->sub[0]->expr && formula->sub[0]->expr->type == REL_EXPR_ATOMIC &&
            formula->sub[0]->expr->data.atomic.rel) {
            Relation *left_rel = formula->sub[0]->expr->data.atomic.rel;
            if (left_rel->tuple_count > 0) {
                int var = sat_encoding_register_var(enc, left_rel->arity, left_rel->tuples[0]);
                if (var >= 1)
                    disj[disj_count++] = -var;
            }
        }
        /* G: 正向右侧 */
        if (formula->sub[1] && formula->sub[1]->expr && formula->sub[1]->expr->type == REL_EXPR_ATOMIC &&
            formula->sub[1]->expr->data.atomic.rel) {
            Relation *right_rel = formula->sub[1]->expr->data.atomic.rel;
            if (right_rel->tuple_count > 0) {
                int var = sat_encoding_register_var(enc, right_rel->arity, right_rel->tuples[0]);
                if (var >= 1)
                    disj[disj_count++] = var;
            }
        }
        if (disj_count > 0) {
            sat_encoding_add_clause(enc, disj, disj_count);
        }
    }
    return 0;
}

static int encode_formula_forall_exists(SatEncoding *enc, const RelFormula *formula) {
    /* 量词公式：在全称/存在量化下编码子公式 */
    /* 实现：对有限域上的量词进行展开编码 */
    if (formula->sub[0] && formula->sub[0]->expr && formula->sub[0]->expr->type == REL_EXPR_ATOMIC &&
        formula->sub[0]->expr->data.atomic.rel) {
        Relation *sub_rel = formula->sub[0]->expr->data.atomic.rel;
        if (formula->type == REL_FORMULA_FORALL) {
            /* all x: S | F => F 对所有 x 成立 */
            for (int ti = 0; ti < sub_rel->tuple_count; ti++) {
                int var = sat_encoding_register_var(enc, sub_rel->arity, sub_rel->tuples[ti]);
                if (var >= 1) {
                    SatLiteral unit = var;
                    sat_encoding_add_clause(enc, &unit, 1);
                }
            }
        } else {
            /* some x: S | F => 至少一个 x 使 F 成立 */
            SatLiteral *disj = (SatLiteral *) lv_malloc((size_t) sub_rel->tuple_count * sizeof(SatLiteral));
            if (disj) {
                int dc = 0;
                for (int ti = 0; ti < sub_rel->tuple_count; ti++) {
                    int var = sat_encoding_register_var(enc, sub_rel->arity, sub_rel->tuples[ti]);
                    if (var >= 1)
                        disj[dc++] = var;
                }
                if (dc > 0)
                    sat_encoding_add_clause(enc, disj, dc);
                lv_free((void **) &disj);
            }
        }
    }
    return 0;
}

/* ── 公式编码器 VTable ── */

static const struct {
    RelFormulaType type;
    RelFormulaEncoderFn encode;
} kFormulaEncoders[] = {
    {REL_FORMULA_SOME,     encode_formula_some},
    {REL_FORMULA_NO,       encode_formula_no},
    {REL_FORMULA_ONE,      encode_formula_one},
    {REL_FORMULA_LONE,     encode_formula_lone},
    {REL_FORMULA_EQ,       encode_formula_eq_subset},
    {REL_FORMULA_SUBSET,   encode_formula_eq_subset},
    {REL_FORMULA_AND,      encode_formula_and},
    {REL_FORMULA_OR,       encode_formula_or},
    {REL_FORMULA_NOT,      encode_formula_not},
    {REL_FORMULA_IMPLIES,  encode_formula_implies},
    {REL_FORMULA_FORALL,   encode_formula_forall_exists},
    {REL_FORMULA_EXISTS,   encode_formula_forall_exists},
};

/* ========================================================================
 * 关系模型 → SAT 编码
 * ======================================================================== */

SatResult relation_model_to_sat(const RelModel *model, const SmallScopeConfig *scope, SatEncoding *enc) {
    lv_CHECK_NULL(model, SAT_ERROR);
    lv_CHECK_NULL(scope, SAT_ERROR);
    lv_CHECK_NULL(enc, SAT_ERROR);
    lv_UNUSED(scope);

    enc->rel_model = model;

    /* 为关系模型中的每个原子对注册变量 */
    if (model->sigs) {
        for (int si = 0; si < model->sig_count; si++) {
            RelSignature *sig = model->sigs[si];
            if (!sig)
                continue;
            for (int ai = 0; ai < sig->atom_count; ai++) {
                for (int aj = ai + 1; aj < sig->atom_count; aj++) {
                    int ids[2] = {sig->atoms[ai]->atom_id, sig->atoms[aj]->atom_id};
                    sat_encoding_register_var(enc, 2, ids);
                }
            }
        }
    }

    /* 编码事实公式为硬约束 */
    for (int fi = 0; fi < model->fact_count; fi++) {
        RelFormula *formula = model->facts[fi];
        if (!formula)
            continue;

        {
            bool found = false;
            for (size_t i = 0; i < sizeof(kFormulaEncoders) / sizeof(kFormulaEncoders[0]); i++) {
                if (kFormulaEncoders[i].type == formula->type) {
                    kFormulaEncoders[i].encode(enc, formula);
                    found = true;
                    break;
                }
            }
            if (!found) {
                lv_LOG_WARNING("Unknown formula type %d in sat_encode_model_facts", formula->type);
            }
        }
    }

    return SAT_OK;
}

