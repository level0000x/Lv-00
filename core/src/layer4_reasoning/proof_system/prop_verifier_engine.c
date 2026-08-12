/*
 * @file prop_verifier_engine.c
 * @brief Proposition verifier module - core proof engine
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
#include "lv/lv_xmacro.h" /* LV_DISPATCH / LV_DISPATCH_VOID */

/* ============================================================
 * 核心证明引擎（递归回溯，有剪枝）
 * ============================================================ */

/* 前置声明 */
bool prove(ProofContext *ctx, const PropFormula **premises, int premise_count, const PropFormula *goal);

/* 检查是否超时或超步数 */
static bool check_limits(ProofContext *ctx) {
    if (ctx->steps >= ctx->config->max_steps)
        return true;
    if (ctx->config->timeout_ms > 0) {
        uint64_t now = get_time_ms();
        if (now - ctx->start_time_ms >= (uint64_t) ctx->config->timeout_ms) {
            ctx->timed_out = true;
            return true;
        }
    }
    return false;
}

/* 尝试 modus ponens：在前提中找到 A→B 且 A，推导出 B */
static bool try_modus_ponens(ProofContext *ctx, const PropFormula **premises, int premise_count,
                             const PropFormula *goal) {
    for (int i = 0; i < premise_count; i++) {
        if (premises[i]->type == PROP_IMPLICATION) {
            const PropFormula *impl = premises[i];
            const PropFormula *antecedent = impl->data.binary.left;
            const PropFormula *consequent = impl->data.binary.right;

            /* 如果蕴含的后件与目标匹配 */
            if (formula_equal(consequent, goal)) {
                /* 检查前件是否在前提中 */
                if (premise_contains(premises, premise_count, antecedent)) {
                    ctx->steps++;
                    return true;
                }
                /* 递归证明前件 */
                ctx->steps++;
                if (prove(ctx, premises, premise_count, antecedent)) {
                    return true;
                }
            }
        }
    }
    return false;
}

/* ���Դ�ǰ����ֱ��ƥ��Ŀ�� */
static bool try_direct_match(const PropFormula **premises, int premise_count, const PropFormula *goal) {
    return premise_contains(premises, premise_count, goal);
}

/* ���� ?-��ȥ���� ?A �� A �Ƴ� �� */
static bool try_neg_elim(ProofContext *ctx, const PropFormula **premises, int premise_count) {
    /* Ŀ���� �ͣ�����Ƿ��� ?A �� A ͬʱ��Ϊǰ�� */
    for (int i = 0; i < premise_count; i++) {
        if (premises[i]->type == PROP_NEGATION) {
            const PropFormula *operand = premises[i]->data.unary.operand;
            if (premise_contains(premises, premise_count, operand)) {
                ctx->steps++;
                return true;
            }
        }
    }
    return false;
}

/* ============================================================
 * 目标公式类型的证明策略查找表（VTable）
 *
 * 将 prove() 中对 goal->type 的大型 switch 分发重构为函数指针查找表：
 * 每个目标公式类型对应一个独立的 static 证明策略函数，
 * 通过 designated initializer 建立「类型 → 函数」的映射。
 * ============================================================ */

/** 目标公式证明策略函数指针类型 */
typedef bool (*ProveGoalFn)(ProofContext *ctx, const PropFormula **premises, int premise_count,
                            const PropFormula *goal);

/** @brief 证明策略：目标为 TRUE，显然可证 */
static bool prove_true(ProofContext *ctx, const PropFormula **premises, int premise_count,
                       const PropFormula *goal) {
    (void) ctx;
    (void) premises;
    (void) premise_count;
    (void) goal;
    return true;
}

/** @brief 证明策略：目标为 BOTTOM（⊥），依次尝试前提匹配、¬-消去与蕴含前提推导矛盾 */
static bool prove_bottom(ProofContext *ctx, const PropFormula **premises, int premise_count,
                         const PropFormula *goal) {
    /* 目标为 ⊥：先检查前提中是否含 ⊥ */
    bool result = premise_contains(premises, premise_count, goal);
    /* 直接匹配不成功，尝试 ¬-消去 */
    if (!result) {
        result = try_neg_elim(ctx, premises, premise_count);
    }
    /* 仍未成功，尝试从蕴含前提推导矛盾 */
    if (!result) {
        for (int i = 0; i < premise_count && !result; i++) {
            if (premises[i]->type == PROP_IMPLICATION) {
                const PropFormula *impl = premises[i];
                if (impl->data.binary.right->type == PROP_BOTTOM) {
                    /* 若 A→⊥（即 ¬A），则递归证明 A */
                    ctx->steps++;
                    result = prove(ctx, premises, premise_count, impl->data.binary.left);
                }
            }
        }
    }
    /* 前提正向展开，获取更多蕴含关系后再次尝试 ¬-消去 */
    if (!result) {
        const PropFormula *expanded[MAX_PREMISES];
        int exp_count = forward_chain_conjunctions(premises, premise_count, expanded, MAX_PREMISES);
        /* 多步前提展开 */
        {
            bool changed = true;
            while (changed && exp_count < MAX_PREMISES) {
                changed = false;
                for (int i = 0; i < exp_count && !changed; i++) {
                    if (expanded[i]->type == PROP_IMPLICATION) {
                        const PropFormula *antecedent = expanded[i]->data.binary.left;
                        const PropFormula *consequent = expanded[i]->data.binary.right;
                        if (premise_contains(expanded, exp_count, antecedent) &&
                            !premise_contains(expanded, exp_count, consequent)) {
                            expanded[exp_count++] = consequent;
                            changed = true;
                            ctx->steps++;
                        }
                    }
                }
            }
            /* 对展开后的前提尝试 ¬-消去 */
            if (!result) {
                result = try_neg_elim(ctx, expanded, exp_count);
            }
            /* 检查 ⊥ 是否由展开前提推出 */
            if (!result) {
                result = premise_contains(expanded, exp_count, goal);
            }
        }
    }
    return result;
}

/** @brief 证明策略：目标为原子命题，直接匹配、modus ponens 或 ∨-消去 */
static bool prove_atom(ProofContext *ctx, const PropFormula **premises, int premise_count,
                       const PropFormula *goal) {
    /* 声明局部缓冲区数组，在函数作用域内使用，避免栈上生命周期问题 */
    const PropFormula *new_premises_l[MAX_PREMISES];
    const PropFormula *new_premises_r[MAX_PREMISES];
    const PropFormula *expanded[MAX_PREMISES];
    const PropFormula *fc_expanded[MAX_PREMISES];

    bool result = try_direct_match(premises, premise_count, goal);
    if (!result) {
        result = try_modus_ponens(ctx, premises, premise_count, goal);
    }
    /* 前提正向展开，获取更多可用于 modus ponens 的蕴含 */
    if (!result) {
        int exp_count = forward_chain_conjunctions(premises, premise_count, expanded, MAX_PREMISES);
        /* 多步前提展开：循环应用 modus ponens 直至无法推出新事实 */
        bool changed = true;
        while (changed && exp_count < MAX_PREMISES) {
            changed = false;
            for (int i = 0; i < exp_count && !changed; i++) {
                if (expanded[i]->type == PROP_IMPLICATION) {
                    const PropFormula *antecedent = expanded[i]->data.binary.left;
                    const PropFormula *consequent = expanded[i]->data.binary.right;
                    if (premise_contains(expanded, exp_count, antecedent) &&
                        !premise_contains(expanded, exp_count, consequent)) {
                        expanded[exp_count++] = consequent;
                        changed = true;
                        ctx->steps++;
                    }
                }
            }
            /* 检查目标是否在展开前提中 */
            result = try_direct_match(expanded, exp_count, goal);
        }
    }
    /* 最后尝试 ∨-消去：若展开前提含 A∨B，分别假设 A 和 B 证明 goal */
    if (!result) {
        int fc_count = forward_chain_conjunctions(premises, premise_count, fc_expanded, MAX_PREMISES);
        /* 多步前提展开 */
        {
            bool changed = true;
            while (changed && fc_count < MAX_PREMISES) {
                changed = false;
                for (int i = 0; i < fc_count && !changed; i++) {
                    if (fc_expanded[i]->type == PROP_IMPLICATION) {
                        const PropFormula *antecedent = fc_expanded[i]->data.binary.left;
                        const PropFormula *consequent = fc_expanded[i]->data.binary.right;
                        if (premise_contains(fc_expanded, fc_count, antecedent) &&
                            !premise_contains(fc_expanded, fc_count, consequent)) {
                            fc_expanded[fc_count++] = consequent;
                            changed = true;
                            ctx->steps++;
                        }
                    }
                }
            }
        }

        for (int i = 0; i < fc_count && !result; i++) {
            if (fc_expanded[i]->type == PROP_DISJUNCTION) {
                const PropFormula *disj = fc_expanded[i];
                /* 尝试左分支：假设 A 证明 goal */
                ctx->steps++;
                {
                    int new_count = fc_count;
                    memcpy((void *) new_premises_l, fc_expanded,
                           sizeof(const PropFormula *) * (size_t) fc_count);
                    if (new_count < MAX_PREMISES) {
                        new_premises_l[new_count++] = disj->data.binary.left;
                    }
                    if (prove(ctx, new_premises_l, new_count, goal)) {
                        result = true;
                    }
                }
                /* 尝试右分支：假设 B 证明 goal */
                if (!result) {
                    ctx->steps++;
                    {
                        int new_count = fc_count;
                        memcpy((void *) new_premises_r, fc_expanded,
                               sizeof(const PropFormula *) * (size_t) fc_count);
                        if (new_count < MAX_PREMISES) {
                            new_premises_r[new_count++] = disj->data.binary.right;
                        }
                        if (prove(ctx, new_premises_r, new_count, goal)) {
                            result = true;
                        }
                    }
                }
            }
        }
    }
    return result;
}

/** @brief 证明策略：目标为合取 A ∧ B，分别证明 A 和 B */
static bool prove_conjunction(ProofContext *ctx, const PropFormula **premises, int premise_count,
                              const PropFormula *goal) {
    const PropFormula *left = goal->data.binary.left;
    const PropFormula *right = goal->data.binary.right;
    ctx->steps++;
    bool result = false;
    bool left_ok = prove(ctx, premises, premise_count, left);
    if (left_ok) {
        ctx->steps++;
        result = prove(ctx, premises, premise_count, right);
    }
    return result;
}

/** @brief 证明策略：目标为析取 A ∨ B，先证明 A，失败则证明 B */
static bool prove_disjunction(ProofContext *ctx, const PropFormula **premises, int premise_count,
                              const PropFormula *goal) {
    const PropFormula *left = goal->data.binary.left;
    const PropFormula *right = goal->data.binary.right;

    /* 尝试左分支 */
    ctx->steps++;
    bool result = prove(ctx, premises, premise_count, left);
    if (!result) {
        /* 尝试右分支 */
        ctx->steps++;
        result = prove(ctx, premises, premise_count, right);
    }
    return result;
}

/** @brief 证明策略：目标为蕴含 A → B，将 A 加入前提后证明 B */
static bool prove_implication(ProofContext *ctx, const PropFormula **premises, int premise_count,
                              const PropFormula *goal) {
    const PropFormula *antecedent = goal->data.binary.left;
    const PropFormula *consequent = goal->data.binary.right;

    /* 将 A 加入前提 */
    const PropFormula *new_premises[MAX_PREMISES];
    int new_count = premise_count;
    if (new_count >= MAX_PREMISES) {
        return false;
    }
    memcpy(new_premises, premises, sizeof(const PropFormula *) * premise_count);
    new_premises[new_count++] = antecedent;

    ctx->steps++;
    return prove(ctx, new_premises, new_count, consequent);
}

/** @brief 证明策略：目标为否定 ¬A，将 A 加入前提后证明 ⊥ */
static bool prove_negation(ProofContext *ctx, const PropFormula **premises, int premise_count,
                           const PropFormula *goal) {
    const PropFormula *operand = goal->data.unary.operand;

    const PropFormula *new_premises[MAX_PREMISES];
    int new_count = premise_count;
    if (new_count >= MAX_PREMISES) {
        return false;
    }
    memcpy(new_premises, premises, sizeof(const PropFormula *) * premise_count);
    new_premises[new_count++] = operand;

    /* 以 ⊥ 作为新的目标 */
    PropFormula *bot = prop_formula_create_bottom();
    ctx->steps++;
    bool result = prove(ctx, new_premises, new_count, bot);
    prop_formula_destroy(bot);
    return result;
}

/** 目标公式类型 → 证明策略函数 查找表（designated initializer） */
static const ProveGoalFn kProveGoalHandlers[] = {
    [PROP_TRUE] = prove_true,
    [PROP_BOTTOM] = prove_bottom,
    [PROP_ATOM] = prove_atom,
    [PROP_CONJUNCTION] = prove_conjunction,
    [PROP_DISJUNCTION] = prove_disjunction,
    [PROP_IMPLICATION] = prove_implication,
    [PROP_NEGATION] = prove_negation,
};

/**
 * @brief 证明目标公式：递归回溯证明算法
 *
 * 使用带记忆化的递归回溯证明算法来验证目标：
 * 1. 检查步数与时间限制
 * 2. 查询记忆化表，避免重复推导
 * 3. 按目标公式类型分发（VTable）：
 *    - BOTTOM：目标为假，爆炸原理可用
 *    - TRUE：平凡成立
 *    - CONJUNCTION：分别证明两个子公式
 *    - DISJUNCTION：证明任一个分支
 *    - IMPLICATION：使用 Modus Ponens 递归证明目标
 *    - NEGATION：检查前提是否蕴含矛盾
 *    - ATOM：检查是否在前提取
 *
 * @param ctx           证明上下文（含步骤计数、超时限制等）
 * @param premises      前提公式数组
 * @param premise_count 前提数量
 * @param goal          待证明的目标公式
 * @return true 表示证明成功，false 表示证明失败或超时/超步数
 */
bool prove(ProofContext *ctx, const PropFormula **premises, int premise_count, const PropFormula *goal) {
    /* 递归深度保护，防止栈溢出 */
    ++ctx->recursion_depth;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wjump-misses-init"
    if (ctx->recursion_depth > PROP_MAX_RECURSION_DEPTH) { /* 递归过深，防止栈溢出 */
        goto prove_depth_exceeded;
    }

    /* 限制检查 */
    if (check_limits(ctx)) {
        goto prove_depth_exceeded;
    }
    ctx->steps++;

    /* 记忆化查询 */
    uint64_t phash = premises_hash(premises, premise_count);
    int midx = memo_find(ctx, goal, phash);
    if (midx >= 0 && ctx->memo[midx].searched) {
        bool r = ctx->memo[midx].proven;
        if (--ctx->recursion_depth == 0)
            memo_destroy(ctx);
        return r;
    }

    bool result = false;

    /* VTable 分发：根据目标公式类型调用对应的证明策略函数 */
    result = LV_DISPATCH(kProveGoalHandlers, goal->type, false, ctx, premises, premise_count, goal);
    /* 未知公式类型：保持 result = false（与原 default 分支行为一致） */

    /* 爆炸原理：如果前提包含爆炸原理（ex falso），即前提含 ⊥，任何目标都可证 */
    if (!result && ctx->config->enable_ex_falso) {
        /* 检查前提中是否含有 ⊥（矛盾常量"假"） */
        for (int i = 0; i < premise_count; i++) {
            if (premises[i]->type == PROP_BOTTOM) {
                result = true;
                break;
            }
        }
    }

    /* 额外尝试：使用前提展开，获取更多前提信息 */
    if (!result && goal->type == PROP_ATOM) {
        const PropFormula *expanded[MAX_PREMISES];
        int exp_count = forward_chain_conjunctions(premises, premise_count, expanded, MAX_PREMISES);
        if (exp_count > premise_count) {
            /* 对展开前提直接匹配 */
            result = try_direct_match(expanded, exp_count, goal);
            if (!result) {
                result = try_modus_ponens(ctx, expanded, exp_count, goal);
            }
        }
    }

    /* 记录记忆化结果 */
    memo_add(ctx, goal, phash, result);

    if (--ctx->recursion_depth == 0)
        memo_destroy(ctx);
    return result;

prove_depth_exceeded:
#pragma GCC diagnostic pop
    /* 递归深度超限或时间超时，统一在此递减递归深度 */
    if (--ctx->recursion_depth == 0)
        memo_destroy(ctx);
    return false;
}

