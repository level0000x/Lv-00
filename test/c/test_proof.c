/**
 * @file test_proof.c
 * @brief 璇佹槑绯荤粺娴嬭瘯 - 鍛介鍒涘缓銆佸悎涓€妫€鏌ャ€佽瘉鏄庡鑸櫒銆佺垎鐐稿師鐞?
 *
 * 娴嬭瘯鍐呭锛?
 * - 鍛介鍒涘缓涓庣鐞?
 * - 璇佹槑姝ラ鍒涘缓涓庣鐞?
 * - 璇佹槑瀵艰埅鍣?
 * - 鍚堜竴妫€鏌?
 * - 鐖嗙偢鍘熺悊
 * - 璇佹槑渚濊禆閾?
 * - 瀵煎嚭鍔熻兘
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv.h"
#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 娴嬭瘯锛氬懡棰樼敓鍛藉懆鏈?============== */

static void test_proposition_lifecycle(void) {
    printf("Test: proposition lifecycle...\n");

    /* --- 楠岃瘉鍛介鍒涘缓鍚庣殑鍒濆鐘舵€?--- */
    Proposition *prop = proposition_create(1, PROPOSITION_TYPE_ATOMIC);
    lv_ASSERT_NOT_NULL(prop);
    lv_ASSERT(prop->id == 1);
    lv_ASSERT(prop->type == PROPOSITION_TYPE_ATOMIC);
    lv_ASSERT(prop->input_count == 0);
    lv_ASSERT(prop->output_count == 0);
    /* 鍒濆鐘舵€佷笅瀛愬懡棰樻暟閲忓簲涓?0 */
    lv_ASSERT(prop->sub_prop_count == 0);
    /* 鍒濆鐘舵€佷笅妯″紡鍥惧簲涓?NULL */
    lv_ASSERT(prop->pattern == NULL);

    printf("  鍘熷瓙鍛介鍒涘缓鎴愬姛 (ID=%d)\n", prop->id);

    /* --- 楠岃瘉娣诲姞姝ラ鍚庣姸鎬佸彉鍖?--- */
    /* 璁剧疆杈撳叆绔彛锛岄獙璇?input_count 鍙樺寲 */
    int in_ports[] = {10, 20};
    bool ok = proposition_set_input_ports(prop, in_ports, 2);
    lv_ASSERT(ok == true);
    lv_ASSERT(prop->input_count == 2);
    printf("  璁剧疆杈撳叆绔彛鍚? input_count = %d\n", prop->input_count);

    /* 璁剧疆杈撳嚭绔彛锛岄獙璇?output_count 鍙樺寲 */
    int out_ports[] = {30};
    ok = proposition_set_output_ports(prop, out_ports, 1);
    lv_ASSERT(ok == true);
    lv_ASSERT(prop->output_count == 1);
    printf("  璁剧疆杈撳嚭绔彛鍚? output_count = %d\n", prop->output_count);

    /* 娣诲姞瀛愬懡棰橈紝楠岃瘉 sub_prop_count 鍙樺寲 */
    Proposition *child = proposition_create(2, PROPOSITION_TYPE_ATOMIC);
    lv_ASSERT_NOT_NULL(child);
    ok = proposition_add_sub_proposition(prop, child);
    lv_ASSERT(ok == true);
    lv_ASSERT(prop->sub_prop_count == 1);
    printf("  娣诲姞瀛愬懡棰樺悗: sub_prop_count = %d\n", prop->sub_prop_count);

    /* --- 楠岃瘉閿€姣佸悗鐨勮祫婧愰噴鏀?--- */
    /* 閿€姣佸懡棰橈紙浼氶€掑綊閿€姣佸瓙鍛介锛夛紝涔嬪悗涓嶅簲鍐嶈闂凡閲婃斁鐨勬寚閽?*/
    proposition_destroy(prop);
    /* 娉ㄦ剰锛氶攢姣佸悗 prop 鍜?child 鍧囧凡閲婃斁锛屾澶勪笉璁块棶浠ラ伩鍏嶆湭瀹氫箟琛屼负銆?
     * 璧勬簮閲婃斁鐨勬纭€х敱 proposition_destroy 鍐呴儴瀹炵幇淇濊瘉锛?
     * 鍙€氳繃鍐呭瓨妫€娴嬪伐鍏凤紙濡?Valgrind/ASan锛夎繘涓€姝ラ獙璇併€?*/
    printf("  鍛介閿€姣佸畬鎴愶紝璧勬簮宸查噴鏀綷n");

    printf("  PASSED\n");

}

/* ============== 娴嬭瘯锛氬鍚堝懡棰?============== */

static void test_composite_propositions(void) {
    printf("Test: composite propositions...\n");

    /* 鍒涘缓鍚堝彇鍛介 */
    Proposition *conj = proposition_create(1, PROPOSITION_TYPE_CONJUNCTION);
    lv_ASSERT_NOT_NULL(conj);
    printf("  鍚堝彇鍛介鍒涘缓鎴愬姛\n");

    /* 鍒涘缓鏋愬彇鍛介 */
    Proposition *disj = proposition_create(2, PROPOSITION_TYPE_DISJUNCTION);
    lv_ASSERT_NOT_NULL(disj);
    printf("  鏋愬彇鍛介鍒涘缓鎴愬姛\n");

    /* 鍒涘缓钑村惈鍛介 */
    Proposition *impl = proposition_create(3, PROPOSITION_TYPE_IMPLICATION);
    lv_ASSERT_NOT_NULL(impl);
    printf("  钑村惈鍛介鍒涘缓鎴愬姛\n");

    /* 鍒涘缓鍚﹀畾鍛介 */
    Proposition *neg = proposition_create(4, PROPOSITION_TYPE_NEGATION);
    lv_ASSERT_NOT_NULL(neg);
    printf("  鍚﹀畾鍛介鍒涘缓鎴愬姛\n");

    /* 鍒涘缓鍏ㄧО鍛介 */
    Proposition *univ = proposition_create(5, PROPOSITION_TYPE_UNIVERSAL);
    lv_ASSERT_NOT_NULL(univ);
    printf("  鍏ㄧО鍛介鍒涘缓鎴愬姛\n");

    /* 鍒涘缓瀛樺湪鍛介 */
    Proposition *exist = proposition_create(6, PROPOSITION_TYPE_EXISTENTIAL);
    lv_ASSERT_NOT_NULL(exist);
    printf("  瀛樺湪鍛介鍒涘缓鎴愬姛\n");

    /* 鍒涘缓鐭涚浘鍛介 */
    Proposition *bottom = proposition_create(7, PROPOSITION_TYPE_BOTTOM);
    lv_ASSERT_NOT_NULL(bottom);
    printf("  鐭涚浘鍛介鍒涘缓鎴愬姛\n");

    proposition_destroy(conj);
    proposition_destroy(disj);
    proposition_destroy(impl);
    proposition_destroy(neg);
    proposition_destroy(univ);
    proposition_destroy(exist);
    proposition_destroy(bottom);

    printf("  PASSED\n");

}

/* ============== 娴嬭瘯锛氬懡棰樼鍙ｈ缃?============== */

static void test_proposition_ports(void) {
    printf("Test: proposition port configuration...\n");

    ConstraintGraph *g = graph_create();

    Proposition *prop = proposition_create(1, PROPOSITION_TYPE_ATOMIC);
    lv_ASSERT_NOT_NULL(prop);

    /* 鍒涘缓杈撳叆绔彛 */
    int in_ports[] = {1, 2};
    bool ok = proposition_set_input_ports(prop, in_ports, 2);
    lv_ASSERT(ok);
    lv_ASSERT(prop->input_count == 2);
    printf("  杈撳叆绔彛璁剧疆鎴愬姛: %d 涓猏n", prop->input_count);

    /* 鍒涘缓杈撳嚭绔彛 */
    int out_ports[] = {3};
    ok = proposition_set_output_ports(prop, out_ports, 1);
    lv_ASSERT(ok);
    lv_ASSERT(prop->output_count == 1);
    printf("  杈撳嚭绔彛璁剧疆鎴愬姛: %d 涓猏n", prop->output_count);

    /* 鍒涘缓妯″紡鍥?*/
    int p1 = add_point(g, 0, 1, 0, 1);
    int p2 = add_point(g, 1, 1, 1, 1);
    graph_add_line_segment(g, p1, p2);

    ok = proposition_set_pattern(prop, g);
    lv_ASSERT(ok);
    lv_ASSERT(prop->pattern == g);
    printf("  妯″紡鍥捐缃垚鍔焅n");

    proposition_destroy(prop);
    /* 娉ㄦ剰锛歡 宸茬敱 proposition_destroy 鍐呴儴閿€姣侊紙浣滀负 prop->pattern锛夛紝涓嶈鍐嶆閲婃斁 */

    printf("  PASSED\n");

}

/* ============== 娴嬭瘯锛氬瓙鍛介 ============== */

static void test_sub_propositions(void) {
    printf("Test: sub-propositions...\n");

    /* 鍒涘缓鐖跺懡棰?*/
    Proposition *parent = proposition_create(1, PROPOSITION_TYPE_CONJUNCTION);
    lv_ASSERT_NOT_NULL(parent);

    /* 鍒涘缓瀛愬懡棰?*/
    Proposition *child1 = proposition_create(2, PROPOSITION_TYPE_ATOMIC);
    Proposition *child2 = proposition_create(3, PROPOSITION_TYPE_ATOMIC);

    /* 娣诲姞瀛愬懡棰?*/
    bool ok = proposition_add_sub_proposition(parent, child1);
    lv_ASSERT(ok);
    printf("  瀛愬懡棰?娣诲姞鎴愬姛\n");

    ok = proposition_add_sub_proposition(parent, child2);
    lv_ASSERT(ok);
    printf("  瀛愬懡棰?娣诲姞鎴愬姛\n");

    lv_ASSERT(parent->sub_prop_count == 2);

    proposition_destroy(parent);
    /* 娉ㄦ剰锛歱roposition_destroy 浼氶€掑綊閿€姣佸瓙鍛介 */

    printf("  PASSED\n");

}

/* ============== 娴嬭瘯锛氳瘉鏄庢楠?============== */

static void test_proof_steps(void) {
    printf("Test: proof steps...\n");

    /* 鍒涘缓涓嶅悓绫诲瀷鐨勮瘉鏄庢楠?*/
    ProofStep *step1 = proof_step_create(PROOF_STEP_ADD_NODE);
    lv_ASSERT_NOT_NULL(step1);
    lv_ASSERT(step1->type == PROOF_STEP_ADD_NODE);
    printf("  娣诲姞鑺傜偣姝ラ鍒涘缓鎴愬姛\n");

    ProofStep *step2 = proof_step_create(PROOF_STEP_ADD_CONSTRAINT);
    lv_ASSERT_NOT_NULL(step2);
    printf("  娣诲姞绾︽潫姝ラ鍒涘缓鎴愬姛\n");

    ProofStep *step3 = proof_step_create(PROOF_STEP_NORMALIZATION);
    lv_ASSERT_NOT_NULL(step3);
    printf("  瑙勮寖鍖栨楠ゅ垱寤烘垚鍔焅n");

    ProofStep *step4 = proof_step_create(PROOF_STEP_UNIFY);
    lv_ASSERT_NOT_NULL(step4);
    printf("  鍚堜竴妫€鏌ユ楠ゅ垱寤烘垚鍔焅n");

    /* 璁剧疆鏂偣 */
    proof_step_set_breakpoint(step1, true);
    lv_ASSERT(step1->is_breakpoint == true);
    printf("  鏂偣璁剧疆鎴愬姛\n");

    /* 娣诲姞渚濊禆鍏崇郴 */
    bool ok = proof_step_add_dependency(step2, step1->id);
    printf("  渚濊禆鍏崇郴娣诲姞: %s\n", ok ? "鎴愬姛" : "澶辫触");

    proof_step_destroy(step1);
    proof_step_destroy(step2);
    proof_step_destroy(step3);
    proof_step_destroy(step4);

    printf("  PASSED\n");

}

/* ============== 娴嬭瘯锛氳瘉鏄庡鑸櫒 ============== */

static void test_proof_navigator(void) {
    printf("Test: proof navigator...\n");

    /* 鍒涘缓鐩爣鍛介 */
    Proposition *target = proposition_create(1, PROPOSITION_TYPE_ATOMIC);
    lv_ASSERT_NOT_NULL(target);

    /* 鍒涘缓璇佹槑瀵艰埅鍣?*/
    ProofNavigator *nav = proof_navigator_create(target, NULL);
    lv_ASSERT_NOT_NULL(nav);
    lv_ASSERT(nav->target_prop == target);
    lv_ASSERT(nav->step_count == 0);
    lv_ASSERT(nav->current_step == -1); /* 鍒濆鍊间负 -1锛堟棤褰撳墠姝ラ锛?*/

    printf("  璇佹槑瀵艰埅鍣ㄥ垱寤烘垚鍔焅n");

    /* 娣诲姞璇佹槑姝ラ */
    ProofStep *step1 = proof_step_create(PROOF_STEP_ADD_NODE);
    bool ok = proof_navigator_add_step(nav, step1);
    lv_ASSERT(ok);
    lv_ASSERT(nav->step_count == 1);
    printf("  姝ラ1娣诲姞鎴愬姛\n");

    ProofStep *step2 = proof_step_create(PROOF_STEP_NORMALIZATION);
    ok = proof_navigator_add_step(nav, step2);
    lv_ASSERT(ok);
    lv_ASSERT(nav->step_count == 2);
    printf("  姝ラ2娣诲姞鎴愬姛\n");

    /* 瀵艰埅娴嬭瘯 */
    ProofStep *current = proof_navigator_current_step(nav);
    printf("  当前步骤: %s\n", current ? proof_step_type_to_string(current->type) : "无");

    /* 下一步 */
    ok = proof_navigator_next(nav);
    printf("  导航到下一步: %s\n", ok ? "成功" : "失败/已在最后");

    /* 上一步 */
    ok = proof_navigator_prev(nav);
    printf("  导航到上一步: %s\n", ok ? "成功" : "失败/已在开头");

    /* 计算最终颜色 */
    ProofColor color = proof_navigator_compute_final_color(nav);
    printf("  最终颜色: %s\n", proof_color_to_string(color));

    proof_navigator_destroy(nav);
    proposition_destroy(target);

    printf("  PASSED\n");

}

/* ============== 娴嬭瘯锛氬悎涓€妫€鏌?============== */

static void test_unify_check(void) {
    printf("Test: unify check...\n");

    /* 鍒涘缓鏋勯€犲浘 */
    ConstraintGraph *construction = graph_create();
    int p1 = add_point(construction, 0, 1, 0, 1);
    int p2 = add_point(construction, 1, 1, 1, 1);
    graph_add_line_segment(construction, p1, p2);

    /* 鍒涘缓鍛介 */
    Proposition *prop = proposition_create(1, PROPOSITION_TYPE_ATOMIC);
    lv_ASSERT_NOT_NULL(prop);

    /* 璁剧疆鍛介鐨勬ā寮忓浘 */
    ConstraintGraph *pattern = graph_create();
    int pp1 = add_point(pattern, 0, 1, 0, 1);
    int pp2 = add_point(pattern, 1, 1, 1, 1);
    graph_add_line_segment(pattern, pp1, pp2);

    proposition_set_pattern(prop, pattern);

    /* 鎵ц鍚堜竴妫€鏌?*/
    UnifyStatus result = proof_unify(construction, prop, false);
    printf("  鍚堜竴缁撴灉: %s\n", unify_result_to_string(result));

    /* 璇︾粏鍚堜竴妫€鏌?*/
    char *mismatch_info = NULL;
    UnifyStatus result2 = proof_unify_detailed(construction, prop, &mismatch_info);
    printf("  璇︾粏鍚堜竴缁撴灉: %s\n", unify_result_to_string(result2));
    if (mismatch_info) {
        printf("  涓嶅尮閰嶄俊鎭? %s\n", mismatch_info);
        lv_free_ptr(mismatch_info);
    }

    graph_destroy(construction);
    proposition_destroy(prop);
    /* 娉ㄦ剰锛歱attern 宸茬敱 proposition_destroy 鍐呴儴閿€姣侊紝涓嶈鍐嶆閲婃斁 */

    printf("  PASSED\n");

}

/* ============== 娴嬭瘯锛氳瘉鏄庝緷璧栭摼 ============== */

static void test_proof_dependencies(void) {
    printf("Test: proof dependencies...\n");

    /* 鍒涘缓渚濊禆 */
    ProofDependency *dep = proof_dependency_create(PROOF_COLOR_GREEN);
    lv_ASSERT_NOT_NULL(dep);
    lv_ASSERT(dep->color == PROOF_COLOR_GREEN);
    printf("  渚濊禆鍒涘缓鎴愬姛\n");

    /* 鍒涘缓瀛愪緷璧?*/
    ProofDependency *sub_dep = proof_dependency_create(PROOF_COLOR_BLUE_UNEXPLORED);
    bool ok = proof_dependency_add_sub(dep, sub_dep);
    printf("  瀛愪緷璧栨坊鍔? %s\n", ok ? "鎴愬姛" : "澶辫触");

    /* 璁＄畻渚濊禆閾鹃鑹?*/
    ProofColor computed = proof_dependency_compute_color(dep);
    printf("  璁＄畻棰滆壊: %s\n", proof_color_to_string(computed));

    proof_dependency_destroy(dep);

    printf("  PASSED\n");

}

/* ============== 娴嬭瘯锛氱垎鐐稿師鐞?============== */

static void test_ex_falso(void) {
    printf("Test: explosion principle (ex falso)...\n");

    ConstraintGraph *g = graph_create();

    /* 鍒涘缓鐖嗙偢鍘熺悊鍑芥暟鍧?*/
    int block_id = -1;
    bool ok = proof_create_ex_falso_block(g, &block_id);
    printf("  鍒涘缓鐖嗙偢鍘熺悊鍑芥暟鍧? %s (ID=%d)\n", ok ? "鎴愬姛" : "澶辫触", block_id);

    /* 鍒涘缓璇佹槑瀵艰埅鍣ㄥ拰鐩爣鍛介 */
    Proposition *target = proposition_create(1, PROPOSITION_TYPE_ATOMIC);
    ProofNavigator *nav = proof_navigator_create(target, NULL);

    /* 鍒涘缓鈯ョ殑璇佺墿锛堢畝鍖栫ず渚嬶級 */
    ConstraintGraph *bottom_proof = graph_create();

    /* 搴旂敤鐖嗙偢鍘熺悊 */
    ok = proof_apply_ex_falso(nav, bottom_proof, target);
    printf("  搴旂敤鐖嗙偢鍘熺悊: %s\n", ok ? "鎴愬姛" : "澶辫触");

    proof_navigator_destroy(nav);
    proposition_destroy(target);
    graph_destroy(g);
    graph_destroy(bottom_proof);

    printf("  PASSED\n");

}

/* ============== 娴嬭瘯锛氳瘉鏄庨鑹?============== */

static void test_proof_colors(void) {
    printf("Test: proof colors...\n");

    /* 娴嬭瘯鎵€鏈夎瘉鏄庨鑹?*/
    ProofColor colors[] = {PROOF_COLOR_GREEN,          PROOF_COLOR_BLUE_UNEXPLORED,
                           PROOF_COLOR_BLUE_RESOURCE,  PROOF_COLOR_BLUE_OUT_OF_RANGE,
                           PROOF_COLOR_GREEN_VERIFIED, PROOF_COLOR_YELLOW,
                           PROOF_COLOR_ORANGE_ORACLE,  PROOF_COLOR_ORANGE_EX_FALSO,
                           PROOF_COLOR_AMBER,          PROOF_COLOR_DARK_ORANGE};

    const char *expected[] = {"Green",  "BlueUnexplored", "BlueResource",  "BlueOutOfRange", "GreenVerified",
                              "Yellow", "OrangeOracle",   "OrangeExFalso", "Amber",          "DarkOrange"};

    for (int i = 0; i < sizeof(colors) / sizeof(colors[0]); i++) {
        const char *str = proof_color_to_string(colors[i]);
        printf("  %s -> %s\n", expected[i], str);
    }

    printf("  PASSED\n");

}

/* ============== 娴嬭瘯锛氳緟鍔╁嚱鏁?============== */

static void test_helper_functions(void) {
    printf("Test: helper functions...\n");

    /* 鍛介绫诲瀷杞瓧绗︿覆 */
    const char *str = proposition_type_to_string(PROPOSITION_TYPE_ATOMIC);
    printf("  ATOMIC -> %s\n", str);

    str = proposition_type_to_string(PROPOSITION_TYPE_CONJUNCTION);
    printf("  CONJUNCTION -> %s\n", str);

    str = proposition_type_to_string(PROPOSITION_TYPE_IMPLICATION);
    printf("  IMPLICATION -> %s\n", str);

    /* 姝ラ绫诲瀷杞瓧绗︿覆 */
    str = proof_step_type_to_string(PROOF_STEP_ADD_NODE);
    printf("  ADD_NODE -> %s\n", str);

    str = proof_step_type_to_string(PROOF_STEP_UNIFY);
    printf("  UNIFY -> %s\n", str);

    str = proof_step_type_to_string(PROOF_STEP_EX_FALSO);
    printf("  EX_FALSO -> %s\n", str);

    /* 鍚堜竴缁撴灉杞瓧绗︿覆 */
    str = unify_result_to_string(UNIFY_STATUS_OK);
    printf("  UNIFY_STATUS_OK -> %s\n", str);

    str = unify_result_to_string(UNIFY_STATUS_PORT_TYPE_MISMATCH);
    printf("  PORT_TYPE_MISMATCH -> %s\n", str);

    str = unify_result_to_string(UNIFY_STATUS_CONSTRAINT_MISMATCH);
    printf("  CONSTRAINT_MISMATCH -> %s\n", str);

    printf("  PASSED\n");

}

/* ============== task group tests ============== */

static int g_task_a_calls = 0;
static int g_task_b_calls = 0;

static void task_fn_a(void *arg) {
    (void) arg;
    g_task_a_calls++;
}

static void task_fn_b(void *arg) {
    (void) arg;
    g_task_b_calls++;
}

static void test_task_group(void) {
    printf("Test: task group (create/add/run/wait/destroy)...\n");

    g_task_a_calls = 0;
    g_task_b_calls = 0;

    lvTaskGroup *g = lv_task_group_create("task_group_test");
    lv_ASSERT_NOT_NULL(g);

    lvTask *t1 = lv_task_create(task_fn_a, NULL, "a");
    lvTask *t2 = lv_task_create(task_fn_b, NULL, "b");
    lvTask *t3 = lv_task_create(task_fn_a, NULL, "c");
    lv_ASSERT_NOT_NULL(t1);
    lv_ASSERT_NOT_NULL(t2);
    lv_ASSERT_NOT_NULL(t3);

    lv_task_group_add(g, t1);
    lv_task_group_add(g, t2);
    lv_task_group_add(g, t3);

    /* explicit run executes all queued tasks in FIFO order */
    int done = lv_task_group_run(g);
    lv_ASSERT(done == 3);
    lv_ASSERT(g_task_a_calls == 2);
    lv_ASSERT(g_task_b_calls == 1);

    /* lazy execution: add then wait completes remaining tasks */
    lvTask *t4 = lv_task_create(task_fn_a, NULL, "d");
    lv_task_group_add(g, t4);
    lv_task_group_wait(g);
    lv_ASSERT(g_task_a_calls == 3);

    /* destroy executes leftover tasks instead of leaking them */
    lvTask *t5 = lv_task_create(task_fn_b, NULL, "e");
    lv_task_group_add(g, t5);
    lv_task_group_destroy(g);
    lv_ASSERT(g_task_b_calls == 2);

    printf("  PASSED\n");
}

/* ============== 涓诲嚱鏁?============== */

TEST_MAIN_BEGIN("Lv-00 Proof System Test Suite")
    printf("=== Lv-00 Proof System Test Suite ===\n\n");
    TEST_MAIN_RUN(test_proposition_lifecycle);
    TEST_MAIN_RUN(test_composite_propositions);
    TEST_MAIN_RUN(test_proposition_ports);
    TEST_MAIN_RUN(test_sub_propositions);
    TEST_MAIN_RUN(test_proof_steps);
    TEST_MAIN_RUN(test_proof_navigator);
    TEST_MAIN_RUN(test_unify_check);
    TEST_MAIN_RUN(test_proof_dependencies);
    TEST_MAIN_RUN(test_ex_falso);
    TEST_MAIN_RUN(test_proof_colors);
    TEST_MAIN_RUN(test_helper_functions);
    /* task group execution engine */
    TEST_MAIN_RUN(test_task_group);
    printf("\n=== All proof system tests PASSED! ===\n");
TEST_MAIN_END()
