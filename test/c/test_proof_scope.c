/**
 * @file test_proof_scope.c
 * @brief 反证法作用域（assumption scope）契约测试（C-㉟）
 *
 * 批次 C-㉟：补齐 proof.h 4 个 scope API 的实现后，钉住其契约：
 * - proof_begin_assumption_scope：开启作用域，返回唯一 ID；NULL nav 失败
 * - proof_close_assumption_scope：关闭作用域；不存在/已关闭/全局 ID 失败
 * - proof_scope_is_active：活动查询；关闭后 false
 * - proof_has_global_proposition：命题不在激活作用域假设集合 → 全局
 *
 * 语义（对齐 proof_widget.c 消费与头文件契约）：
 * - scope_assumptions 为借用引用（不拥有命题）
 * - scope_id 从 1 起单调递增，0 为 lv_PROOF_SCOPE_GLOBAL 保留
 * - 关闭仅标记非激活，记录保留
 *
 * 按 test-authoring 三层：等价性 / 边界 / 性质。
 *
 * @author Lv-00 Project
 * @date 2026-08-20
 */

#include <stdio.h>
#include <string.h>

#include "lv/proof.h"
#include "lv.h"
#include "test_helpers.h"

/* ============================================================
 * 全局测试计数器
 * ============================================================ */
int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * 作用域生命周期：begin / is_active / close / has_global
 * ============================================================ */
static void test_scope_lifecycle(void) {
    Proposition *target = proposition_create(1, PROPOSITION_TYPE_ATOMIC);
    ProofNavigator *nav = proof_navigator_create(target, NULL);
    TEST_ASSERT_MSG(nav != NULL, "nav 创建");

    /* 假设命题（借用引用，导航器不拥有） */
    Proposition *assump = proposition_create(2, PROPOSITION_TYPE_ATOMIC);

    /* 初始：无作用域 */
    TEST_ASSERT_EQ(nav->scope_count, 0);

    /* 开启作用域 1 */
    lvProofScopeId s1 = proof_begin_assumption_scope(nav, assump);
    TEST_ASSERT_MSG(s1 >= lv_PROOF_SCOPE_GLOBAL + 1, "作用域 ID 从 1 起");
    TEST_ASSERT_EQ(nav->scope_count, 1);
    TEST_ASSERT_MSG(proof_scope_is_active(nav, s1), "新作用域激活");
    TEST_ASSERT_MSG(nav->scope_active[0] == true, "scope_active 标记");
    TEST_ASSERT_MSG(nav->scope_assumptions[0] == assump, "假设引用记录");

    /* 作用域内：命题不是全局（条件性成立） */
    TEST_ASSERT_MSG(!proof_has_global_proposition(nav, assump), "作用域假设非全局");

    /* 目标命题非假设 → 全局 */
    TEST_ASSERT_MSG(proof_has_global_proposition(nav, target), "目标命题全局");

    /* 开启第二个作用域：ID 单调递增 */
    Proposition *assump2 = proposition_create(3, PROPOSITION_TYPE_ATOMIC);
    lvProofScopeId s2 = proof_begin_assumption_scope(nav, assump2);
    TEST_ASSERT_MSG(s2 > s1, "作用域 ID 单调递增");
    TEST_ASSERT_EQ(nav->scope_count, 2);
    TEST_ASSERT_MSG(proof_scope_is_active(nav, s2), "第二作用域激活");

    /* 关闭第一个作用域 */
    TEST_ASSERT_MSG(proof_close_assumption_scope(nav, s1), "关闭作用域 1");
    TEST_ASSERT_MSG(!proof_scope_is_active(nav, s1), "关闭后非激活");
    TEST_ASSERT_MSG(proof_scope_is_active(nav, s2), "作用域 2 仍激活");
    TEST_ASSERT_MSG(nav->scope_count == 2, "关闭保留记录（仅标记）");

    /* 关闭后假设回归全局判定 */
    TEST_ASSERT_MSG(proof_has_global_proposition(nav, assump), "关闭后假设视为全局");
    TEST_ASSERT_MSG(!proof_has_global_proposition(nav, assump2), "作用域 2 假设仍非全局");

    /* 关闭作用域 2 */
    TEST_ASSERT_MSG(proof_close_assumption_scope(nav, s2), "关闭作用域 2");
    TEST_ASSERT_MSG(!proof_scope_is_active(nav, s2), "作用域 2 非激活");
    TEST_ASSERT_MSG(proof_has_global_proposition(nav, assump2), "全部关闭后假设全局");

    proposition_unref(assump);
    proposition_unref(assump2);
    proof_navigator_destroy(nav);
    proposition_unref(target);
}

/* ============================================================
 * 边界：NULL / 全局 ID / 不存在作用域 / NULL 假设
 * ============================================================ */
static void test_scope_boundaries(void) {
    Proposition *target = proposition_create(10, PROPOSITION_TYPE_ATOMIC);
    ProofNavigator *nav = proof_navigator_create(target, NULL);
    TEST_ASSERT_MSG(nav != NULL, "nav 创建");

    /* NULL nav */
    TEST_ASSERT_EQ(proof_begin_assumption_scope(NULL, target), lv_PROOF_SCOPE_INVALID);
    TEST_ASSERT_MSG(!proof_close_assumption_scope(NULL, 1), "NULL nav close 失败");
    TEST_ASSERT_MSG(!proof_scope_is_active(NULL, 1), "NULL nav is_active 失败");
    TEST_ASSERT_MSG(!proof_has_global_proposition(NULL, target), "NULL nav has_global 失败");
    TEST_ASSERT_MSG(!proof_has_global_proposition(nav, NULL), "NULL prop has_global 失败");

    /* 关闭不存在的 ID */
    TEST_ASSERT_MSG(!proof_close_assumption_scope(nav, 42), "不存在作用域 close 失败");
    TEST_ASSERT_MSG(!proof_scope_is_active(nav, 42), "不存在作用域 is_active false");

    /* 全局 ID 0 与负 ID */
    TEST_ASSERT_MSG(!proof_close_assumption_scope(nav, lv_PROOF_SCOPE_GLOBAL), "全局 ID close 失败");
    TEST_ASSERT_MSG(!proof_scope_is_active(nav, lv_PROOF_SCOPE_GLOBAL), "全局 ID is_active false");
    TEST_ASSERT_MSG(!proof_close_assumption_scope(nav, lv_PROOF_SCOPE_INVALID), "无效 ID close 失败");
    TEST_ASSERT_MSG(!proof_scope_is_active(nav, lv_PROOF_SCOPE_INVALID), "无效 ID is_active false");

    /* NULL 假设：允许开启（无条件作用域） */
    lvProofScopeId s = proof_begin_assumption_scope(nav, NULL);
    TEST_ASSERT_MSG(s >= 1, "NULL 假设可开启作用域");
    TEST_ASSERT_MSG(nav->scope_assumptions[nav->scope_count - 1] == NULL, "假设记录为 NULL");
    TEST_ASSERT_MSG(proof_has_global_proposition(nav, target), "无条件作用域不改变全局判定（target 仍全局）");
    TEST_ASSERT_MSG(proof_close_assumption_scope(nav, s), "关闭无条件作用域");

    /* 重复 close：幂等失败 */
    TEST_ASSERT_MSG(!proof_close_assumption_scope(nav, s), "重复 close 失败");

    /* 多作用域开启（验证倍增扩容路径，>8 个） */
    Proposition *dummy[12];
    for (int i = 0; i < 12; i++) {
        dummy[i] = proposition_create(20 + i, PROPOSITION_TYPE_ATOMIC);
        lvProofScopeId sid = proof_begin_assumption_scope(nav, dummy[i]);
        TEST_ASSERT_MSG(sid >= 1, "批量开启作用域");
    }
    TEST_ASSERT_MSG(nav->scope_count >= 12, "扩容后 count >= 12");
    /* 全部激活 */
    int active_count = 0;
    for (int i = 0; i < nav->scope_count; i++) {
        if (nav->scope_active[i])
            active_count++;
    }
    TEST_ASSERT_EQ(active_count, 12);
    /* 12 个假设均非全局 */
    TEST_ASSERT_MSG(!proof_has_global_proposition(nav, dummy[5]), "批量假设非全局");
    /* 关闭全部 */
    for (int i = 0; i < 12; i++) {
        /* 从 scope_count-1 倒序关闭全部激活记录 */
        for (int j = 0; j < nav->scope_count; j++) {
            if (nav->scope_active[j]) {
                proof_close_assumption_scope(nav, nav->scope_ids[j]);
            }
        }
    }
    int remaining = 0;
    for (int i = 0; i < nav->scope_count; i++) {
        if (nav->scope_active[i])
            remaining++;
    }
    TEST_ASSERT_EQ(remaining, 0);
    TEST_ASSERT_MSG(proof_has_global_proposition(nav, dummy[7]), "全部关闭后假设全局");
    for (int i = 0; i < 12; i++) {
        proposition_unref(dummy[i]);
    }

    proof_navigator_destroy(nav);
    proposition_unref(target);
}

/* ============================================================
 * 主入口
 * ============================================================ */
TEST_MAIN_BEGIN("Proof Assumption Scope")

    TEST_MAIN_RUN(test_scope_lifecycle);
    TEST_MAIN_RUN(test_scope_boundaries);

TEST_MAIN_END()
