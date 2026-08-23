/**
 * @file test_modal_operators_ext.c
 * @brief 模态逻辑扩展契约测试（批次 C-㊺续17：modal_operators.h 20 个零覆盖 API）
 *
 * 覆盖零覆盖 API：
 *   世界族：world_create / world_destroy / world_assert / world_holds
 *   框架族：frame_create / frame_destroy / frame_add_world /
 *     frame_set_reachability / frame_is_reachable /
 *     frame_get_reachable_worlds
 *   公式族：formula_create_nested / formula_to_string
 *   评估族：evaluate / check_validity / eval_result_destroy
 *   几何辅助：frame_create_geometric_default /
 *     assert_point_must_on_line / assert_point_can_on_line
 *   辅助：op_to_string / reachability_type_to_string
 *
 * 契约要点（与头注释/实现核对）：
 *   - 世界自可达（from_idx == to_idx 视为可达）。
 *   - world_holds 按指针相等判断。
 *   - formula_destroy 不销毁 inner_prop（调用者管理）。
 *   - evaluate 返回 0 成功；result 含 explanation（调用者 destroy）。
 *   - 对偶转换恒 NULL（UNSUPPORTED，已有测试覆盖，不重复）。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/modal_operators.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：世界管理 ============== */

static void test_modal_world_api(void) {
    /* world_create / holds / assert / destroy */
    lvModalWorld *w = lv_modal_world_create(1, "世界1", NULL);
    TEST_ASSERT_NOT_NULL(w);
    TEST_ASSERT_EQ(w->id, 1);
    TEST_ASSERT(strcmp(w->world_name, "世界1") == 0, "世界名复制");

    /* 未断言的命题 holds = FALSE */
    Proposition *p = proposition_create(0, PROPOSITION_TYPE_ATOMIC);
    TEST_ASSERT(lv_modal_world_holds(w, p) == lv_FALSE, "未断言为假");

    /* assert 后 holds = TRUE */
    TEST_ASSERT(lv_modal_world_assert(w, p), "断言成功");
    TEST_ASSERT(lv_modal_world_holds(w, p) == lv_TRUE, "断言后为真");

    /* 另一个命题仍为假 */
    Proposition *q = proposition_create(1, PROPOSITION_TYPE_ATOMIC);
    TEST_ASSERT(lv_modal_world_holds(w, q) == lv_FALSE, "不同命题为假");

    /* NULL 契约 */
    TEST_ASSERT(!lv_modal_world_assert(NULL, p), "world NULL 断言失败");
    TEST_ASSERT(!lv_modal_world_assert(w, NULL), "prop NULL 断言失败");
    TEST_ASSERT(lv_modal_world_holds(NULL, p) == lv_UNKNOWN, "NULL world");
    TEST_ASSERT(lv_modal_world_holds(w, NULL) == lv_UNKNOWN, "NULL prop");
    lv_modal_world_destroy(NULL);

    /* 清理：world_destroy 不销毁 true_props 中的 prop，调用者管理 */
    proposition_destroy(p);
    proposition_destroy(q);
    lv_modal_world_destroy(w);
    printf("  test_modal_world_api: PASSED\n");
}

/* ============== 测试：框架管理 ============== */

static void test_modal_frame_api(void) {
    lvModalFrame *frame = lv_modal_frame_create();
    TEST_ASSERT_NOT_NULL(frame);
    TEST_ASSERT_EQ(frame->current_world_id, 1);

    /* 添加 3 个世界 */
    lvModalWorld *w1 = lv_modal_world_create(1, "W1", NULL);
    lvModalWorld *w2 = lv_modal_world_create(2, "W2", NULL);
    lvModalWorld *w3 = lv_modal_world_create(3, "W3", NULL);
    TEST_ASSERT(lv_modal_frame_add_world(frame, w1), "添加世界1");
    TEST_ASSERT(lv_modal_frame_add_world(frame, w2), "添加世界2");
    TEST_ASSERT(lv_modal_frame_add_world(frame, w3), "添加世界3");
    TEST_ASSERT_EQ(frame->worlds.count, 3);

    /* 默认：仅自可达 */
    TEST_ASSERT(lv_modal_frame_is_reachable(frame, 1, 1), "自可达");
    TEST_ASSERT(!lv_modal_frame_is_reachable(frame, 1, 2), "默认不可达");

    /* 设置可达 1→2 */
    TEST_ASSERT(lv_modal_frame_set_reachability(frame, 1, 2, lv_REACH_RIGID_TRANSFORM), "设置可达");
    TEST_ASSERT(lv_modal_frame_is_reachable(frame, 1, 2), "1→2 可达");
    TEST_ASSERT(!lv_modal_frame_is_reachable(frame, 2, 1), "2→1 仍不可达");

    /* get_reachable_worlds */
    int *ids = NULL;
    int count = 0;
    TEST_ASSERT(lv_modal_frame_get_reachable_worlds(frame, 1, &ids, &count), "获取可达世界");
    TEST_ASSERT_NOT_NULL(ids);
    TEST_ASSERT_EQ(count, 1);
    TEST_ASSERT_EQ(ids[0], 2);
    lv_free((void **) &ids);

    /* 无可达世界 */
    TEST_ASSERT(lv_modal_frame_get_reachable_worlds(frame, 3, &ids, &count), "获取可达世界3");
    TEST_ASSERT_NOT_NULL(ids);
    TEST_ASSERT_EQ(count, 0);
    lv_free((void **) &ids);

    /* 未知世界 */
    TEST_ASSERT(!lv_modal_frame_set_reachability(frame, 9, 1, lv_REACH_CUSTOM), "未知世界失败");
    TEST_ASSERT(!lv_modal_frame_is_reachable(frame, 9, 1), "未知世界不可达");
    TEST_ASSERT(!lv_modal_frame_get_reachable_worlds(frame, 9, &ids, &count), "未知世界获取失败");

    /* NULL 契约 */
    TEST_ASSERT(!lv_modal_frame_add_world(NULL, w1), "frame NULL");
    TEST_ASSERT(!lv_modal_frame_add_world(frame, NULL), "world NULL");
    TEST_ASSERT(!lv_modal_frame_set_reachability(NULL, 1, 2, lv_REACH_CUSTOM), "NULL frame");
    TEST_ASSERT(!lv_modal_frame_is_reachable(NULL, 1, 1), "NULL frame");
    TEST_ASSERT(!lv_modal_frame_get_reachable_worlds(NULL, 1, &ids, &count), "NULL frame");
    TEST_ASSERT(!lv_modal_frame_get_reachable_worlds(frame, 1, NULL, &count), "NULL out_ids");
    TEST_ASSERT(!lv_modal_frame_get_reachable_worlds(frame, 1, &ids, NULL), "NULL out_count");

    lv_modal_frame_destroy(frame);
    lv_modal_frame_destroy(NULL);
    printf("  test_modal_frame_api: PASSED\n");
}

/* ============== 测试：公式与评估 ============== */

static void test_modal_formula_eval_api(void) {
    /* 构造框架：W1 中断言 P 为真，W2 未断言 */
    lvModalFrame *frame = lv_modal_frame_create();
    lvModalWorld *w1 = lv_modal_world_create(1, "W1", NULL);
    lvModalWorld *w2 = lv_modal_world_create(2, "W2", NULL);
    lv_modal_frame_add_world(frame, w1);
    lv_modal_frame_add_world(frame, w2);

    Proposition *p = proposition_create(0, PROPOSITION_TYPE_ATOMIC);
    lv_modal_world_assert(w1, p);

    /* 可达：1→2 */
    lv_modal_frame_set_reachability(frame, 1, 2, lv_REACH_RIGID_TRANSFORM);

    /* ◇P 在世界 1 中：自世界 W1 中 P 为真 → TRUE（自可达 + W1 断言） */
    lvModalFormula *dia = lv_modal_formula_create(lv_MODALOP_POSSIBLE, p);
    lvModalEvalResult result;
    memset(&result, 0, sizeof(result));
    TEST_ASSERT_EQ(lv_modal_evaluate(frame, dia, 1, &result), 0);
    TEST_ASSERT(result.truth_value == lv_TRUE, "◇P 在 W1 为真（自世界 P 真）");
    TEST_ASSERT_NOT_NULL(result.explanation);
    lv_modal_eval_result_destroy(&result);

    /* ◇P 在世界 2 中：W2 自身 P 假，W1 不可达（2→1 未设置）→ FALSE */
    memset(&result, 0, sizeof(result));
    TEST_ASSERT_EQ(lv_modal_evaluate(frame, dia, 2, &result), 0);
    TEST_ASSERT(result.truth_value == lv_FALSE, "◇P 在 W2 为假");
    lv_modal_eval_result_destroy(&result);

    /* □P 在世界 1 中：W1（P 真）与可达 W2（P 假）→ FALSE（反例 W2） */
    lvModalFormula *box = lv_modal_formula_create(lv_MODALOP_NECESSARY, p);
    memset(&result, 0, sizeof(result));
    TEST_ASSERT_EQ(lv_modal_evaluate(frame, box, 1, &result), 0);
    TEST_ASSERT(result.truth_value == lv_FALSE, "□P 在 W1 为假（W2 反例）");
    lv_modal_eval_result_destroy(&result);

    /* □P 在世界 2 中：W2 自身 P 假 → FALSE */
    memset(&result, 0, sizeof(result));
    TEST_ASSERT_EQ(lv_modal_evaluate(frame, box, 2, &result), 0);
    TEST_ASSERT(result.truth_value == lv_FALSE, "□P 在 W2 为假");
    lv_modal_eval_result_destroy(&result);

    /* 嵌套公式：□◇P */
    lvModalFormula *nested = lv_modal_formula_create_nested(lv_MODALOP_NECESSARY, dia);
    TEST_ASSERT_NOT_NULL(nested);
    memset(&result, 0, sizeof(result));
    TEST_ASSERT_EQ(lv_modal_evaluate(frame, nested, 2, &result), 0);
    TEST_ASSERT(result.truth_value != lv_UNKNOWN, "嵌套评估有定值");
    lv_modal_eval_result_destroy(&result);
    /* nested 拥有 dia（sub），销毁 nested 递归销毁 dia；
     * dia 的 inner_prop p 不销毁（调用者管理）。 */
    lv_modal_formula_destroy(nested);

    /* formula_destroy 不销毁 inner_prop */
    lv_modal_formula_destroy(box);

    /* check_validity：□P 非有效（存在 W2 为假） */
    lvModalFormula *box2 = lv_modal_formula_create(lv_MODALOP_NECESSARY, p);
    lvTruthValue tv = lv_modal_check_validity(frame, box2);
    TEST_ASSERT(tv == lv_FALSE || tv == lv_UNKNOWN, "□P 非有效");
    lv_modal_formula_destroy(box2);

    /* evaluate NULL 契约 */
    TEST_ASSERT_EQ(lv_modal_evaluate(NULL, box, 1, &result), -1);
    TEST_ASSERT_EQ(lv_modal_evaluate(frame, NULL, 1, &result), -1);
    TEST_ASSERT_EQ(lv_modal_evaluate(frame, box, 1, NULL), -1);
    TEST_ASSERT(lv_modal_check_validity(NULL, box) == lv_UNKNOWN, "NULL frame");
    TEST_ASSERT(lv_modal_check_validity(frame, NULL) == lv_UNKNOWN, "NULL formula");

    lv_modal_eval_result_destroy(NULL);
    lv_modal_formula_destroy(NULL);
    proposition_destroy(p);
    lv_modal_frame_destroy(frame);
    printf("  test_modal_formula_eval_api: PASSED\n");
}

/* ============== 测试：几何辅助与字符串 ============== */

static void test_modal_geom_str_api(void) {
    /* frame_create_geometric_default */
    lvModalFrame *frame = lv_modal_frame_create_geometric_default();
    TEST_ASSERT_NOT_NULL(frame);
    TEST_ASSERT_EQ(frame->worlds.count, 1);
    TEST_ASSERT(lv_modal_frame_is_reachable(frame, 1, 1), "默认自可达");

    /* assert_point_must_on_line：□(onLine(p1, s2)) */
    lvModalFormula *must = lv_modal_assert_point_must_on_line(frame, 1, 2);
    TEST_ASSERT_NOT_NULL(must);
    TEST_ASSERT_EQ(must->op, lv_MODALOP_NECESSARY);
    TEST_ASSERT_NOT_NULL(must->inner_prop);
    TEST_ASSERT(strstr(must->inner_prop->name, "onLine") != NULL, "命题名含 onLine");

    /* assert_point_can_on_line：◇(onLine(p3, s4)) */
    lvModalFormula *can = lv_modal_assert_point_can_on_line(frame, 3, 4);
    TEST_ASSERT_NOT_NULL(can);
    TEST_ASSERT_EQ(can->op, lv_MODALOP_POSSIBLE);

    /* formula_to_string */
    char *s = lv_modal_formula_to_string(must);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT(strlen(s) > 0, "字符串非空");
    lv_free((void **) &s);
    s = lv_modal_formula_to_string(can);
    TEST_ASSERT_NOT_NULL(s);
    lv_free((void **) &s);
    /* 嵌套格式：□(◇(P)) */
    lvModalFormula *nested = lv_modal_formula_create_nested(lv_MODALOP_NECESSARY, can);
    s = lv_modal_formula_to_string(nested);
    TEST_ASSERT_NOT_NULL(s);
    lv_free((void **) &s);
    TEST_ASSERT_NULL(lv_modal_formula_to_string(NULL));

    /* op_to_string */
    TEST_ASSERT(strcmp(lv_modal_op_to_string(lv_MODALOP_NECESSARY), "\xe2\x96\xa1") == 0, "□ 符号");
    TEST_ASSERT(strcmp(lv_modal_op_to_string(lv_MODALOP_POSSIBLE), "\xe2\x9a\xa7") == 0, "◇ 符号");
    TEST_ASSERT(strcmp(lv_modal_op_to_string((lvModalOperator) 99), "?") == 0, "越界算子");

    /* reachability_type_to_string */
    TEST_ASSERT(strcmp(lv_reachability_type_to_string(lv_REACH_GEOMETRIC_IDENTITY), "identity") == 0, "identity");
    TEST_ASSERT(strcmp(lv_reachability_type_to_string(lv_REACH_RIGID_TRANSFORM), "rigid") == 0, "rigid");
    TEST_ASSERT(strcmp(lv_reachability_type_to_string(lv_REACH_CUSTOM), "custom") == 0, "custom");
    TEST_ASSERT(strcmp(lv_reachability_type_to_string((lvReachabilityType) 99), "unknown") == 0, "越界类型");

    /* 清理：nested 拥有 can，销毁 nested 递归销毁 can；
     * inner_prop 所有权在 formula_destroy 中不释放，调用者管理 */
    Proposition *p_can = can->inner_prop;
    lv_modal_formula_destroy(nested); /* 递归销毁 can */
    lv_modal_formula_destroy(must);
    proposition_destroy(p_can);
    lv_modal_frame_destroy(frame);
    printf("  test_modal_geom_str_api: PASSED\n");
}

/* ============== 测试：几何辅助内存修正 ============== */

static void test_modal_geom_cleanup_api(void) {
    lvModalFrame *frame = lv_modal_frame_create_geometric_default();
    lvModalFormula *must = lv_modal_assert_point_must_on_line(frame, 5, 6);
    lvModalFormula *can = lv_modal_assert_point_can_on_line(frame, 7, 8);

    /* 先取出 inner_prop（formula_destroy 不释放它们） */
    Proposition *pm = must->inner_prop;
    Proposition *pc = can->inner_prop;

    lv_modal_formula_destroy(must);
    lv_modal_formula_destroy(can);
    proposition_destroy(pm);
    proposition_destroy(pc);
    lv_modal_frame_destroy(frame);
    printf("  test_modal_geom_cleanup_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Modal Operators Ext Test Suite")
    printf("=== Lv-00 Modal Operators Ext Test Suite (batch C-㊺续17) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_modal_world_api);
    TEST_MAIN_RUN(test_modal_frame_api);
    TEST_MAIN_RUN(test_modal_formula_eval_api);
    TEST_MAIN_RUN(test_modal_geom_str_api);
    TEST_MAIN_RUN(test_modal_geom_cleanup_api);

    lv_cleanup();
TEST_MAIN_END()
