/*
 * @file prop_verifier_analysis.c
 * @brief Proposition verifier module - inconstructibility analysis
 * @details Split from prop_verifier.c
 */

#include "lv/prop_verifier.h"
#include "lv/prop_formula_visitor.h"
#include "prop_verifier_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lv/lv_internal.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_utils.h"
#include "lv/lv_strbuf.h"
#include "lv/stream.h"
#include "lv/stream_context_util.h"

/* ============================================================
 * Visitor-based formula analysis
 * ============================================================ */

/* ---- CollectAtomsVisitor ---- */

typedef struct {
    char (*atoms)[PROP_ATOM_NAME_MAX_LEN];
    int max_atoms;
    int count;
} CollectAtomsCtx;

/* forward declarations for visitor vtables */
static const PropFormulaVisitor collect_atoms_visitor;
static const PropFormulaVisitor classical_pattern_visitor;

static void collect_atoms_visit_atom(const PropFormula *f, void *context) {
    CollectAtomsCtx *ctx = (CollectAtomsCtx *)context;
    /* dedup */
    for (int i = 0; i < ctx->count; i++) {
        if (lv_str_eq(ctx->atoms[i], f->data.atom.name))
            return;
    }
    if (ctx->count < ctx->max_atoms) {
        snprintf(ctx->atoms[ctx->count], PROP_ATOM_NAME_MAX_LEN, "%s", f->data.atom.name);
        ctx->count++;
    }
}

static void collect_atoms_visit_binary(const PropFormula *f, void *context) {
    CollectAtomsCtx *ctx = (CollectAtomsCtx *)context;
    CollectAtomsCtx left_ctx = {ctx->atoms, ctx->max_atoms, ctx->count};
    prop_formula_accept(f->data.binary.left, &collect_atoms_visitor, &left_ctx);
    ctx->count = left_ctx.count;
    CollectAtomsCtx right_ctx = {ctx->atoms, ctx->max_atoms, ctx->count};
    prop_formula_accept(f->data.binary.right, &collect_atoms_visitor, &right_ctx);
    ctx->count = right_ctx.count;
}

static void collect_atoms_visit_unary(const PropFormula *f, void *context) {
    CollectAtomsCtx *ctx = (CollectAtomsCtx *)context;
    prop_formula_accept(f->data.unary.operand, &collect_atoms_visitor, ctx);
}

static void collect_atoms_visit_constant(const PropFormula *f, void *context) {
    (void)f;
    (void)context;
    /* constants (BOTTOM/TRUE) have no atoms */
}

static const PropFormulaVisitor collect_atoms_visitor = {
    collect_atoms_visit_atom,
    collect_atoms_visit_binary,
    collect_atoms_visit_unary,
    collect_atoms_visit_constant
};

/**
 * @brief Collect atom names from goal formula
 *
 * Recursively traverses the formula AST and collects all unique atom names.
 * Used during proof failure analysis to identify which atoms lack construction.
 */
int collect_atoms(const PropFormula *f, char atoms[][PROP_ATOM_NAME_MAX_LEN], int max_atoms) {
    if (!f || !atoms || max_atoms <= 0)
        return 0;
    CollectAtomsCtx ctx = {atoms, max_atoms, 0};
    prop_formula_accept(f, &collect_atoms_visitor, &ctx);
    return ctx.count;
}

/* ---- ClassicalPatternVisitor ---- */

typedef struct {
    char *pattern_desc;
    size_t desc_size;
    bool found;
} ClassicalPatternCtx;

static void classical_visit_atom(const PropFormula *f, void *context) {
    (void)f;
    (void)context;
    /* atoms are not classical patterns */
}

static void classical_visit_constant(const PropFormula *f, void *context) {
    (void)f;
    (void)context;
    /* constants are not classical patterns */
}

static void classical_visit_unary(const PropFormula *f, void *context) {
    ClassicalPatternCtx *ctx = (ClassicalPatternCtx *)context;
    if (ctx->found)
        return;
    /* recurse into operand */
    prop_formula_accept(f->data.unary.operand, &classical_pattern_visitor, ctx);
}

static void classical_visit_binary(const PropFormula *f, void *context) {
    ClassicalPatternCtx *ctx = (ClassicalPatternCtx *)context;
    if (ctx->found)
        return;

    /* Check LEM: A \/ ~A  or  ~A \/ A */
    if (f->type == PROP_DISJUNCTION) {
        const PropFormula *left = f->data.binary.left;
        const PropFormula *right = f->data.binary.right;
        /* A \/ ~A */
        if (left->type == PROP_NEGATION && formula_equal(left->data.unary.operand, right)) {
            char *s = prop_formula_to_string(right);
            snprintf(ctx->pattern_desc, ctx->desc_size,
                     "排中律 (LEM): %s \\/ ~%s，直觉主义逻辑中不可证", s, s);
            lv_free((void **)&s);
            ctx->found = true;
            return;
        }
        /* ~A \/ A */
        if (right->type == PROP_NEGATION && formula_equal(right->data.unary.operand, left)) {
            char *s = prop_formula_to_string(left);
            snprintf(ctx->pattern_desc, ctx->desc_size,
                     "排中律 (LEM): ~%s \\/ %s，直觉主义逻辑中不可证", s, s);
            lv_free((void **)&s);
            ctx->found = true;
            return;
        }
    }

    /* Check DNE: ~~A -> A  and RAA: (~A -> _|_) -> A */
    if (f->type == PROP_IMPLICATION) {
        const PropFormula *antecedent = f->data.binary.left;
        const PropFormula *consequent = f->data.binary.right;
        /* Double negation elimination: ~~A -> A */
        if (antecedent->type == PROP_NEGATION &&
            antecedent->data.unary.operand->type == PROP_NEGATION &&
            formula_equal(antecedent->data.unary.operand->data.unary.operand, consequent)) {
            char *s = prop_formula_to_string(consequent);
            snprintf(ctx->pattern_desc, ctx->desc_size,
                     "双重否定消去: ~~%s -> %s，直觉主义逻辑中不可证", s, s);
            lv_free((void **)&s);
            ctx->found = true;
            return;
        }
        /* RAA: (~A -> _|_) -> A */
        if (antecedent->type == PROP_IMPLICATION &&
            antecedent->data.binary.left->type == PROP_NEGATION &&
            antecedent->data.binary.right->type == PROP_BOTTOM &&
            formula_equal(antecedent->data.binary.left->data.unary.operand, consequent)) {
            char *s = prop_formula_to_string(consequent);
            snprintf(ctx->pattern_desc, ctx->desc_size,
                     "反证法 (RAA): (~%s -> _|_) -> %s，直觉主义逻辑中不可证", s, s);
            lv_free((void **)&s);
            ctx->found = true;
            return;
        }
    }

    /* No pattern found at this level; recurse into children */
    prop_formula_accept(f->data.binary.left, &classical_pattern_visitor, ctx);
    if (ctx->found)
        return;
    prop_formula_accept(f->data.binary.right, &classical_pattern_visitor, ctx);
}

static const PropFormulaVisitor classical_pattern_visitor = {
    classical_visit_atom,
    classical_visit_binary,
    classical_visit_unary,
    classical_visit_constant
};

/**
 * @brief Check if goal formula contains classical-only patterns
 *
 * Identifies specific patterns that are not provable in intuitionistic logic:
 *   - Double negation elimination: ~~A -> A
 *   - Law of excluded middle (LEM): A \/ ~A
 *   - Reductio ad absurdum (RAA): (~A -> _|_) -> A
 */
bool has_classical_pattern(const PropFormula *f, char *pattern_desc, size_t desc_size) {
    if (!f || !pattern_desc || desc_size == 0)
        return false;
    ClassicalPatternCtx ctx = {pattern_desc, desc_size, false};
    prop_formula_accept(f, &classical_pattern_visitor, &ctx);
    return ctx.found;
}

InconstructibilityAnalysis prop_verifier_analyze_inconstructibility(const PropFormula **premises, int premise_count,
                                                                    const PropFormula *goal,
                                                                    const VerifierConfig *config) {
    InconstructibilityAnalysis analysis;
    memset(&analysis, 0, sizeof(analysis));

    VerifierConfig default_config = VERIFIER_CONFIG_DEFAULT;
    if (!config)
        config = &default_config;

    /* 先执行验证 */
    VerifyDetail detail = prop_verifier_verify(premises, premise_count, goal, config);

    if (detail.result == VERIFY_PROVEN) {
        analysis.is_inconstructible = false;
        snprintf(analysis.reason, sizeof(analysis.reason), "验证器报告为可构造，跳过不可构造性分析");
        return analysis;
    }

    analysis.is_inconstructible = true;

    /* 检查是否包含经典逻辑模式 */
    char pattern_desc[PROP_PATTERN_DESC_BUFSIZE] = {0};
    if (config->use_intuitionistic && has_classical_pattern(goal, pattern_desc, sizeof(pattern_desc))) {
        snprintf(analysis.reason, sizeof(analysis.reason),
                 "直觉主义限制: "
                 "%s，在直觉主义逻辑中，证明无法提供具体构造，"
                 "因为排中律或双重否定消除等经典原则不可用",
                 pattern_desc);
    } else if (detail.result == VERIFY_TIMEOUT) {
        snprintf(analysis.reason, sizeof(analysis.reason),
                 "验证超时: 证明搜索在 %d 毫秒内未完成。"
                 "可能需要更多步骤或更复杂的构造/证明组合。",
                 config->timeout_ms);
    } else {
        /* 分析缺少的前提/目标 */
        char goal_atoms[PROP_ATOM_COLLECT_MAX][PROP_ATOM_NAME_MAX_LEN];
        memset(goal_atoms, 0, sizeof(goal_atoms));
        int atom_count = collect_atoms(goal, goal_atoms, PROP_ATOM_COLLECT_MAX);

        /* 检查哪些目标原子不在前提中（lvStrBuf 累积，自动扩容，无 512 截断风险） */
        lvStrBuf missing_sb = {0};
        int missing_count = 0;
        for (int i = 0; i < atom_count; i++) {
            bool found = false;
            for (int j = 0; j < premise_count; j++) {
                if (premises[j]->type == PROP_ATOM && lv_str_eq(premises[j]->data.atom.name, goal_atoms[i])) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                lv_strbuf_printf(&missing_sb, "%s%s", missing_count > 0 ? ", " : "", goal_atoms[i]);
                missing_count++;
            }
        }

        if (missing_count > 0) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(analysis.reason, sizeof(analysis.reason),
                     "缺少构造: 目标需要原子命题 [%s] 的构造，"
                     "但当前前提未提供。在 BHK 解释下，"
                     "每个原子命题都需要一个构造证据（点、线段或图形）。",
                     lv_strbuf_cstr(&missing_sb));
        } else {
            snprintf(analysis.reason, sizeof(analysis.reason),
                     "构造缺失: "
                     "前提中包含所有目标原子命题，但无法通过"
                     "证明规则将其组合成目标。可能需要更长的蕴含前提"
                     "或更复杂的构造步骤。已使用 %d 步搜索",
                     detail.steps_used);
#pragma GCC diagnostic pop
        }
        lv_strbuf_destroy(&missing_sb);
    }

    /* 填写失败子目标信息 */
    analysis.failed_subgoals = 1;
    analysis.subgoal_descriptions = (char **) lv_malloc(sizeof(char *)); /* 分配内存 */
    if (analysis.subgoal_descriptions) {
        analysis.subgoal_descriptions[0] = (char *) lv_malloc(512); /* 分配内存 */
        if (analysis.subgoal_descriptions[0]) {
            snprintf(analysis.subgoal_descriptions[0], 512, "目标: %s | 状态: %s | 步骤: %d/%d",
                     prop_formula_to_string(goal), detail.result == VERIFY_TIMEOUT ? "超时" : "未找到证明",
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
            lv_free((void **) &analysis->subgoal_descriptions[i]); /* 释放非NULL */
        }
        lv_free((void **) &analysis->subgoal_descriptions); /* 释放非NULL */
    }
    analysis->subgoal_desc_count = 0;
}