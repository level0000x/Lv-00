/*
 * @file proof_navigator_misc.c
 * @brief Proof navigator module - bottom definition / lemma fold / axiom lock / contradiction check / ancestry
 * @details Split from proof_navigator.c
 */

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_platform.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_xmacro.h"
#include "lv/axiom_pkg.h"
#include "lv/constraint_graph.h"
#include "lv/engine.h"
#include "lv/proof.h"
#include "lv/proof_trace.h"
#include "lv/smt_backend.h"
#include "lv/trust_color.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "stream.h"
#include "stream_context_util.h"
#include "proof_navigator_internal.h"

/* ============== ⊥ 的公理包可定义性 ============== */

void proof_set_bottom_definition(ProofNavigator *nav, const BottomDefinition *def) {
    if (!nav || !def)
        return;

    if (!nav->bottom_def) {
        nav->bottom_def = lv_calloc(1, sizeof(BottomDefinition));
        if (!nav->bottom_def)
            return;
    }

    *nav->bottom_def = *def;
}

const BottomDefinition *proof_get_bottom_definition(const ProofNavigator *nav) {
    if (!nav)
        return NULL;
    return nav->bottom_def;
}

/* ============== 引理块折叠 ============== */

void proof_set_lemma_view_state(ProofNavigator *nav, int step_id, LemmaViewState state) {
    if (!nav || step_id < 0)
        return;

    /* 查找是否已存在该步骤的视图状态 */
    for (int i = 0; i < nav->lemma_view_step_ids.count; i++) {
        int *sid = (int *)lv_darray_get(&nav->lemma_view_step_ids, i);
        if (*sid == step_id) {
            LemmaViewState *st = (LemmaViewState *)lv_darray_get(&nav->lemma_view_states, i);
            *st = state;
            return;
        }
    }

    /* 添加新的视图状态（lv_darray_push 自动扩容） */
    lv_darray_push(&nav->lemma_view_step_ids, &step_id);
    lv_darray_push(&nav->lemma_view_states, &state);
}

LemmaViewState proof_get_lemma_view_state(const ProofNavigator *nav, int step_id) {
    if (!nav || step_id < 0)
        return LEMMA_VIEW_STATE_EXPANDED; /* 默认展开 */

    for (int i = 0; i < nav->lemma_view_step_ids.count; i++) {
        int *sid = (int *)lv_darray_get(&nav->lemma_view_step_ids, i);
        if (*sid == step_id) {
            LemmaViewState *st = (LemmaViewState *)lv_darray_get(&nav->lemma_view_states, i);
            return *st;
        }
    }

    return LEMMA_VIEW_STATE_EXPANDED; /* 未设置时默认展开 */
}

/* ============== 公理库权限保护 ============== */

/**
 * @brief 锁定公理库，禁止修改公理集合
 *
 * 锁定后，所有修改公理集合的操作（添加/删除/替换公理）
 * 将被拒绝。用于保护已验证的证明不因公理变化而失效。
 */
void proof_lock_axioms(void) {
    s_proof_state.axiom_locked = true;
    if (proof_stream_ctx) {
        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_INFO, "公理库已锁定：禁止修改公理集合", 0);
    }
}

/**
 * @brief 解锁公理库，允许修改公理集合
 */
void proof_unlock_axioms(void) {
    s_proof_state.axiom_locked = false;
    if (proof_stream_ctx) {
        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_INFO, "公理库已解锁：允许修改公理集合", 0);
    }
}

/**
 * @brief 查询公理库锁定状态
 *
 * @return true 表示公理库已锁定，禁止修改
 */
bool proof_axioms_is_locked(void) {
    return s_proof_state.axiom_locked;
}

/* ============== 逻辑互斥校验 ============== */

/**
 * @brief 检查两个命题是否逻辑互斥
 *
 * 通过比较命题的类型、模式图和约束关系判断是否构成矛盾。
 *
 * 判断规则：
 * 1. 一个为 BOTTOM（矛盾）类型的命题与任何命题互斥
 * 2. 一个为 NEGATION 类型的命题与被否定的原命题互斥
 * 3. 子命题中存在互斥对则整体互斥
 * 4. 通过比较命题类型对（如蕴含与反蕴含）判定语义矛盾
 *
 * @param a  命题 A
 * @param b  命题 B
 * @return true 表示两个命题互斥，false 表示不互斥或无法判断
 */
bool proposition_contradicts(const Proposition *a, const Proposition *b) {
    if (!a || !b)
        return false;

    /* 同一命题的引用——不矛盾 */
    if (a == b)
        return false;

    /* 规则 1：BOTTOM（矛盾）类型的命题与其他命题互斥 */
    if (a->type == PROPOSITION_TYPE_BOTTOM || b->type == PROPOSITION_TYPE_BOTTOM)
        return true;

    /* 规则 2：否定类型 NEGATION 与被否定命题互斥
     * 若 a 是否定，检查 a 的子命题中是否存在与 b 类型相同且模式相近的命题 */
    if (a->type == PROPOSITION_TYPE_NEGATION && a->sub_prop_count > 0) {
        for (int i = 0; i < a->sub_prop_count; i++) {
            if (proposition_contradicts(a->sub_props[i], b))
                return true;
        }
    }
    if (b->type == PROPOSITION_TYPE_NEGATION && b->sub_prop_count > 0) {
        for (int i = 0; i < b->sub_prop_count; i++) {
            if (proposition_contradicts(a, b->sub_props[i]))
                return true;
        }
    }

    /* 规则 3：对命题类型组合进行语义矛盾判定 */
    /* 蕴含与原蕴含反向 */
    if ((a->type == PROPOSITION_TYPE_IMPLICATION && b->type == PROPOSITION_TYPE_IMPLICATION)) {
        /* 两个蕴含命题，检查是否一个的前件等于另一个的后件且结论相反 */
        if (a->precondition_count == b->postcondition_count && a->postcondition_count == b->precondition_count) {
            /* 检查前提/后件 ID 集合的交集 */
            bool pre_post_overlap = false;
            for (int ap = 0; ap < a->precondition_count && !pre_post_overlap; ap++) {
                for (int bp = 0; bp < b->postcondition_count; bp++) {
                    if (a->precondition_region_ids[ap] == b->postcondition_constraint_ids[bp]) {
                        pre_post_overlap = true;
                        break;
                    }
                }
            }
            if (pre_post_overlap) {
                /* 可能存在 A->B 与 B->¬A 的变体冲突，标记为潜在矛盾 */
                return true;
            }
        }
    }

    /* 规则 4：通过命题 ID 和类型完全相同但颜色不同来检测重复声明矛盾
     * （如同一命题被同时标记为 GREEN 和 ORANGE_EX_FALSO，说明推导路径冲突） */
    if (a->id == b->id && a->type == b->type && a->color != PROOF_COLOR_BLUE_UNEXPLORED &&
        b->color != PROOF_COLOR_BLUE_UNEXPLORED) {
        /* 同一命题有两条不同信任颜色的推导路径，标记为潜在矛盾 */
        if ((a->color == PROOF_COLOR_GREEN && b->color == PROOF_COLOR_ORANGE_EX_FALSO) ||
            (a->color == PROOF_COLOR_ORANGE_EX_FALSO && b->color == PROOF_COLOR_GREEN)) {
            return true;
        }
    }

    return false;
}

/* ============== 证明步骤追溯 ============== */

/**
 * @brief 获取证明步骤的完整祖先链（推导链）
 *
 * 从指定步骤开始，沿 parent_step_id 向上追溯，
 * 返回所有祖先步骤的 ID 列表。结果按从近到远排序
 * （最近祖先在前，根步骤在最后）。
 *
 * @param nav              证明导航器
 * @param step_id          目标步骤 ID
 * @param out_ancestor_ids  输出：祖先步骤 ID 数组
 * @param out_count         输出：祖先数量
 * @return true 成功，false 失败
 */
bool proof_step_get_ancestors(const ProofNavigator *nav, int step_id, int **out_ancestor_ids, int *out_count) {
    if (!nav || !out_ancestor_ids || !out_count)
        return false;

    *out_ancestor_ids = NULL;
    *out_count = 0;

    /* 查找目标步骤 */
    ProofStep *current = NULL;
    for (int i = 0; i < nav->step_count; i++) {
        if (nav->steps[i] && nav->steps[i]->id == step_id) {
            current = nav->steps[i];
            break;
        }
    }
    if (!current)
        return false;

    /* 使用 lvDArray 存储祖先 ID（自动扩容） */
    lvDArray ancestors;
    lv_darray_init(&ancestors, sizeof(int));

    ProofStep *cursor = current;
    while (cursor->parent_step_id >= 0) {
        /* 查找父步骤 */
        ProofStep *parent = NULL;
        for (int i = 0; i < nav->step_count; i++) {
            if (nav->steps[i] && nav->steps[i]->id == cursor->parent_step_id) {
                parent = nav->steps[i];
                break;
            }
        }
        if (!parent)
            break;

        lv_darray_push(&ancestors, &parent->id);
        cursor = parent;
    }

    /* 转移所有权给调用者 */
    *out_ancestor_ids = (int *)ancestors.data;
    *out_count = ancestors.count;
    /* 防止 lv_darray_free 释放 data */
    ancestors.data = NULL;
    lv_darray_free(&ancestors);
    return true;
}
