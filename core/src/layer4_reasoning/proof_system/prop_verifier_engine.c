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

/**
 * @brief ����֤���������ݹ���������㷨��
 *
 * ʹ�ô��м��仯�ĵݹ���������㷨����֤����
 * 1. ��鲽����ʱ������
 * 2. ��ѯ���仯���������ظ�����
 * 3. ����Ŀ�깫ʽ���ͷ��ɣ�
 *    - BOTTOM����ȻΪ�٣���ըԭ�����ã�
 *    - TRUE��ƽ������
 *    - CONJUNCTION���ֱ�֤�������ӹ�ʽ
 *    - DISJUNCTION������֤����һ����
 *    - IMPLICATION������ Modus Ponens ����Ŀ��֤��
 *    - NEGATION�����ǰ���Ƿ��̺�ì��
 *    - ATOM������Ƿ���ǰ�Ἧ��
 *
 * @param ctx           ֤�������ģ��������á����仯���ȣ�
 * @param premises      ǰ�ṫʽ����
 * @param premise_count ǰ������
 * @param goal          ��֤����Ŀ�깫ʽ
 * @return true ��ʾ֤���ɹ���false ��ʾ֤��ʧ�ܻ�ʱ/������
 */
bool prove(ProofContext *ctx, const PropFormula **premises, int premise_count, const PropFormula *goal) {
    /* ���ݹ�������ƣ���ֹջ��� */
    ++ctx->recursion_depth;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wjump-misses-init"
    if (ctx->recursion_depth > MAX_MEMO_ENTRIES) { /* ���ݹ���� = ���仯������ */
        goto prove_depth_exceeded;
    }

    /* ������� */
    if (check_limits(ctx)) {
        goto prove_depth_exceeded;
    }
    ctx->steps++;

    /* ���仯��� */
    uint64_t phash = premises_hash(premises, premise_count);
    int midx = memo_find(ctx, goal, phash);
    if (midx >= 0 && ctx->memo[midx].searched) {
        bool r = ctx->memo[midx].proven;
        ctx->recursion_depth--;
        return r;
    }

    bool result = false;

    switch (goal->type) {
        case PROP_TRUE:
            /* ? ���ǿ�֤�� */
            result = true;
            break;

        case PROP_BOTTOM:
            /* Ŀ���� �ͣ����ȼ��ǰ�����Ƿ��� �� */
            result = premise_contains(premises, premise_count, goal);
            /* ������ɹ������� ?-��ȥ */
            if (!result) {
                result = try_neg_elim(ctx, premises, premise_count);
            }
            /* ������ɹ������Դ��̺�ǰ���Ƶ�ì�� */
            if (!result) {
                for (int i = 0; i < premise_count && !result; i++) {
                    if (premises[i]->type == PROP_IMPLICATION) {
                        const PropFormula *impl = premises[i];
                        if (impl->data.binary.right->type == PROP_BOTTOM) {
                            /* �� A���� = ?A������֤�� A */
                            ctx->steps++;
                            result = prove(ctx, premises, premise_count, impl->data.binary.left);
                        }
                    }
                }
            }
            /* ǰ������չ����ȡ��Ӧ�� modus ponens��Ȼ������ ?-��ȥ */
            if (!result) {
                const PropFormula *expanded[MAX_PREMISES];
                int exp_count = forward_chain_conjunctions(premises, premise_count, expanded, MAX_PREMISES);
                /* �ಽǰ������ */
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
                    /* ����չ���ǰ������ ?-��ȥ */
                    if (!result) {
                        result = try_neg_elim(ctx, expanded, exp_count);
                    }
                    /* ��� �� �Ƿ��Ƶ����� */
                    if (!result) {
                        result = premise_contains(expanded, exp_count, goal);
                    }
                }
            }
            break;

        case PROP_ATOM: {
            /* Ŀ����ԭ�����⣺ֱ��ƥ��� modus ponens */
            /* ���оֲ����������� case �����򶥲������� stack-use-after-scope */
            const PropFormula *new_premises_l[MAX_PREMISES];
            const PropFormula *new_premises_r[MAX_PREMISES];
            const PropFormula *expanded[MAX_PREMISES];
            const PropFormula *fc_expanded[MAX_PREMISES];

            result = try_direct_match(premises, premise_count, goal);
            if (!result) {
                result = try_modus_ponens(ctx, premises, premise_count, goal);
            }
            /* ǰ������չ����ȡ������ modus ponens �� */
            if (!result) {
                int exp_count = forward_chain_conjunctions(premises, premise_count, expanded, MAX_PREMISES);
                /* �ಽǰ������������Ӧ�� modus ponens ֱ���޷��Ƶ�����ʵ */
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
                    /* ���Ŀ���Ƿ�����չǰ���� */
                    result = try_direct_match(expanded, exp_count, goal);
                }
            }
            /* ���� ��-��ȥ������� A��B���� A��goal, B��goal */
            if (!result) {
                int fc_count = forward_chain_conjunctions(premises, premise_count, fc_expanded, MAX_PREMISES);
                /* �ಽǰ������ */
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
                        /* �������֧������ A��֤�� goal */
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
                        /* �����ҷ�֧������ B��֤�� goal */
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
            break;
        }

        case PROP_CONJUNCTION: {
            /* Ŀ���� A �� B���ֱ�֤�� A �� B */
            const PropFormula *left = goal->data.binary.left;
            const PropFormula *right = goal->data.binary.right;
            ctx->steps++;
            bool left_ok = prove(ctx, premises, premise_count, left);
            if (left_ok) {
                ctx->steps++;
                result = prove(ctx, premises, premise_count, right);
            }
            break;
        }

        case PROP_DISJUNCTION: {
            /* Ŀ���� A �� B������֤�� A ��֤�� B */
            const PropFormula *left = goal->data.binary.left;
            const PropFormula *right = goal->data.binary.right;

            /* �������֧ */
            ctx->steps++;
            result = prove(ctx, premises, premise_count, left);
            if (!result) {
                /* �����ҷ�֧ */
                ctx->steps++;
                result = prove(ctx, premises, premise_count, right);
            }
            break;
        }

        case PROP_IMPLICATION: {
            /* Ŀ���� A �� B������ A��֤�� B */
            const PropFormula *antecedent = goal->data.binary.left;
            const PropFormula *consequent = goal->data.binary.right;

            /* �� A ����ǰ�� */
            const PropFormula *new_premises[MAX_PREMISES];
            int new_count = premise_count;
            if (new_count >= MAX_PREMISES) {
                result = false;
                break;
            }
            memcpy(new_premises, premises, sizeof(const PropFormula *) * premise_count);
            new_premises[new_count++] = antecedent;

            ctx->steps++;
            result = prove(ctx, new_premises, new_count, consequent);
            break;
        }

        case PROP_NEGATION: {
            /* Ŀ���� ?A = A �� �ͣ����� A��֤�� �� */
            const PropFormula *operand = goal->data.unary.operand;

            const PropFormula *new_premises[MAX_PREMISES];
            int new_count = premise_count;
            if (new_count >= MAX_PREMISES) {
                result = false;
                break;
            }
            memcpy(new_premises, premises, sizeof(const PropFormula *) * premise_count);
            new_premises[new_count++] = operand;

            /* ���� �� ��Ϊ��Ŀ�� */
            PropFormula *bot = prop_formula_create_bottom();
            ctx->steps++;
            result = prove(ctx, new_premises, new_count, bot);
            prop_formula_destroy(bot);
            break;
        }
        default:
            break;
    }

    /* 爆炸原理：如果前提包含��ըԭ�������ǰ������ �ͣ��κ�Ŀ�궼��֤ */
    if (!result && ctx->config->enable_ex_falso) {
        /* ���ǰ�����Ƿ���� �ͣ����ⳣ��"��"�� */
        for (int i = 0; i < premise_count; i++) {
            if (premises[i]->type == PROP_BOTTOM) {
                result = true;
                break;
            }
        }
    }

    /* ���Ⳣ�ԣ�ʹ��ǰ����չ����ȡǰ������� */
    if (!result && goal->type == PROP_ATOM) {
        const PropFormula *expanded[MAX_PREMISES];
        int exp_count = forward_chain_conjunctions(premises, premise_count, expanded, MAX_PREMISES);
        if (exp_count > premise_count) {
            /* ���µ�ǰ�ᱻ��ȡ */
            result = try_direct_match(expanded, exp_count, goal);
            if (!result) {
                result = try_modus_ponens(ctx, expanded, exp_count, goal);
            }
        }
    }

    /* ��¼���仯��� */
    memo_add(ctx, goal, phash, result);

    ctx->recursion_depth--;
    return result;

prove_depth_exceeded:
#pragma GCC diagnostic pop
    /* �ݹ���ȳ��޻���/ʱ�䳬�ޣ�ͳһ�ڴ˵ݼ������� */
    ctx->recursion_depth--;
    return false;
}

