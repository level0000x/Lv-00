/**
 * @file test_proof_infra.c
 * @brief 证明基础设施测试
 *
 * 覆盖模块：
 * - command_log.c: 命令日志生命周期、条目创建、执行、重放、序列化
 * - proof_navigator.c: 证明导航器创建、步骤管理、导航、颜色计算
 * - proof_dependency.c: 依赖链创建、子依赖、颜色计算、传播
 *
 * @author Lv-00 Project
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/command_log.h"
#include "lv/proof.h"
#include "lv/proof_version_internal.h" /* proof_ghost_set_navigator / clear（ghost 冲突检查绑定） */

#include "lv.h"
#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * 命令日志模块测试
 * ============================================================ */

static void test_command_log_lifecycle(void) {
    CommandLog *log = command_log_create(64);
    TEST_ASSERT_NOT_NULL(log);
    TEST_ASSERT_EQ(command_log_count(log), 0);
    TEST_ASSERT_EQ(command_log_current_seq(log), (int64_t) 0);

    command_log_destroy(log);
    command_log_destroy(NULL);
}

static void test_command_log_append(void) {
    CommandLog *log = command_log_create(4);
    TEST_ASSERT_NOT_NULL(log);

    /* 追加各种命令 */
    {
        int nodes[] = {1, 2};
        int ports[] = {10, 11};
        uint64_t dens[] = {1, 1};
        double nums[] = {1.0, 2.0};

        CommandEntry *e1 = command_entry_create_add_node(0, -1, 2, nums, dens);
        TEST_ASSERT_NOT_NULL(e1);
        TEST_ASSERT(command_log_append(log, e1), "append add_node");

        int parts1[] = {0, 1};
        CommandEntry *e2 = command_entry_create_add_constraint(0, -1, parts1, 2);
        TEST_ASSERT_NOT_NULL(e2);
        TEST_ASSERT(command_log_append(log, e2), "append add_constraint");

        CommandEntry *e3 = command_entry_create_remove_node(5);
        TEST_ASSERT_NOT_NULL(e3);
        TEST_ASSERT(command_log_append(log, e3), "append remove_node");

        CommandEntry *e4 = command_entry_create_remove_constraint(0);
        TEST_ASSERT_NOT_NULL(e4);
        TEST_ASSERT(command_log_append(log, e4), "append remove_constraint");

        CommandEntry *e5 = command_entry_create_normalize_graph(true, 100);
        TEST_ASSERT_NOT_NULL(e5);
        TEST_ASSERT(command_log_append(log, e5), "append normalize");

        CommandEntry *e6 = command_entry_create_unify(0, 1);
        TEST_ASSERT_NOT_NULL(e6);
        TEST_ASSERT(command_log_append(log, e6), "append unify");

        CommandEntry *e7 = command_entry_create_set_numeric_assumption(3, 0.01, "test assumption");
        TEST_ASSERT_NOT_NULL(e7);
        TEST_ASSERT(command_log_append(log, e7), "append numeric_assumption");

        CommandEntry *e8 = command_entry_create_pack_function(2, nodes, 1, ports, 1, ports + 1);
        TEST_ASSERT_NOT_NULL(e8);
        TEST_ASSERT(command_log_append(log, e8), "append pack_function");
    }

    TEST_ASSERT_EQ(command_log_count(log), 8);
    TEST_ASSERT_EQ(command_log_current_seq(log), (int64_t) 8);

    /* 获取条目验证 */
    const CommandEntry *get = command_log_get(log, 0);
    TEST_ASSERT_NOT_NULL(get);
    TEST_ASSERT_EQ(get->type, CMD_ADD_NODE);
    TEST_ASSERT_EQ(get->seq, (int64_t) 0);

    get = command_log_get(log, 7);
    TEST_ASSERT_NOT_NULL(get);
    TEST_ASSERT_EQ(get->type, CMD_PACK_FUNCTION);

    /* 越界访问 */
    TEST_ASSERT_NULL(command_log_get(log, -1));
    TEST_ASSERT_NULL(command_log_get(log, 100));

    /* NULL 安全 */
    TEST_ASSERT(!command_log_append(NULL, NULL), "command_log_append should fail for NULL");
    TEST_ASSERT_EQ(command_log_count(NULL), 0);
    TEST_ASSERT_EQ(command_log_current_seq(NULL), (int64_t) 0);
    TEST_ASSERT_NULL(command_log_get(NULL, 0));

    command_log_destroy(log);
}

static void test_command_entry_create_all_types(void) {
    /* 测试所有便利构造函数 */

    /* add_node */
    double nums[] = {1.0, 2.0};
    uint64_t dens[] = {1, 1};
    CommandEntry *e = command_entry_create_add_node(0, -1, 2, nums, dens);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQ(e->type, CMD_ADD_NODE);
    TEST_ASSERT_EQ(e->params.add_node.geom_type, 0);
    command_entry_destroy(e);

    /* add_constraint */
    int parts[] = {1, 2, 3};
    e = command_entry_create_add_constraint(1, -1, parts, 3);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQ(e->type, CMD_ADD_CONSTRAINT);
    TEST_ASSERT_EQ(e->params.add_constraint.participant_count, 3);
    command_entry_destroy(e);

    /* remove_node */
    e = command_entry_create_remove_node(42);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQ(e->type, CMD_REMOVE_NODE);
    TEST_ASSERT_EQ(e->params.remove_node.node_id, 42);
    command_entry_destroy(e);

    /* remove_constraint */
    e = command_entry_create_remove_constraint(5);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQ(e->type, CMD_REMOVE_CONSTRAINT);
    TEST_ASSERT_EQ(e->params.remove_constraint.constraint_index, 5);
    command_entry_destroy(e);

    /* normalize_graph */
    e = command_entry_create_normalize_graph(true, 50);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQ(e->type, CMD_NORMALIZE_GRAPH);
    TEST_ASSERT(e->params.normalize_graph.scope_aware, "e- should be valid");
    TEST_ASSERT_EQ(e->params.normalize_graph.max_iterations, 50);
    command_entry_destroy(e);

    /* unify */
    e = command_entry_create_unify(0, 1);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQ(e->type, CMD_UNIFY);
    command_entry_destroy(e);

    /* set_numeric_assumption */
    e = command_entry_create_set_numeric_assumption(7, 0.001, "approx");
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQ(e->type, CMD_SET_NUMERIC_ASSUMPTION);
    TEST_ASSERT_EQ(e->params.set_numeric_assumption.node_id, 7);
    command_entry_destroy(e);

    /* pack_function */
    int internal[] = {1, 2};
    int in_ports[] = {10};
    int out_ports[] = {11};
    e = command_entry_create_pack_function(2, internal, 1, in_ports, 1, out_ports);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQ(e->type, CMD_PACK_FUNCTION);
    command_entry_destroy(e);

    /* NULL/空参数 */
    e = command_entry_create_add_node(0, -1, 0, NULL, NULL);
    TEST_ASSERT_NOT_NULL(e);
    command_entry_destroy(e);

    command_entry_destroy(NULL);
}

static void test_command_log_clear(void) {
    CommandLog *log = command_log_create(4);

    double nums[] = {1.0, 2.0};
    uint64_t dens[] = {1, 1};
    CommandEntry *e = command_entry_create_add_node(0, -1, 2, nums, dens);
    command_log_append(log, e);
    TEST_ASSERT_EQ(command_log_count(log), 1);

    command_log_clear(log);
    TEST_ASSERT_EQ(command_log_count(log), 0);
    TEST_ASSERT_EQ(command_log_current_seq(log), (int64_t) 0);

    /* NULL 安全 */
    command_log_clear(NULL);

    command_log_destroy(log);
}

static void test_command_entry_serialize_json(void) {
    CommandLog *log = command_log_create(4);

    double nums[] = {1.0, 2.0};
    uint64_t dens[] = {1, 1};
    CommandEntry *e = command_entry_create_add_node(0, -1, 2, nums, dens);
    command_log_append(log, e);

    int parts[] = {0, 1};
    e = command_entry_create_add_constraint(0, -1, parts, 2);
    command_log_append(log, e);

    /* 序列化到 JSON 文件 */
    TEST_ASSERT(command_log_serialize_json(log, "test_command_log.json"), "serialize to json");

    /* 反序列化 */
    CommandLog *restored = command_log_deserialize_json("test_command_log.json");
    TEST_ASSERT_NOT_NULL(restored);
    TEST_ASSERT_EQ(command_log_count(restored), 2);
    command_log_destroy(restored);

    /* NULL 安全 */
    TEST_ASSERT(!command_log_serialize_json(NULL, "test.json"), "serialize with NULL log should fail");
    TEST_ASSERT(!command_log_serialize_json(log, NULL), "serialize with NULL path should fail");
    TEST_ASSERT_NULL(command_log_deserialize_json(NULL));

    command_log_destroy(log);
}

static void test_command_log_execute_replay(void) {
    lv_init();
    lvEngine *engine = lv_engine_create();
    TEST_ASSERT_NOT_NULL(engine);

    CommandLog *log = command_log_create(16);

    /* 执行命令并记录 */
    {
        double nums[] = {0.0, 0.0};
        uint64_t dens[] = {1, 1};
        CommandEntry *e = command_entry_create_add_node(0, -1, 2, nums, dens);
        TEST_ASSERT(command_log_execute(log, e, engine), "execute add_node");
    }
    TEST_ASSERT_EQ(command_log_count(log), 1);

    /* 创建新引擎并回放 */
    lvEngine *replay_engine = lv_engine_create();
    TEST_ASSERT_NOT_NULL(replay_engine);

    TEST_ASSERT(command_log_replay(log, replay_engine), "replay all");

    /* 从指定 seq 回放 */
    TEST_ASSERT(command_log_replay_from(log, replay_engine, -1), "replay from -1 (all)");
    TEST_ASSERT(command_log_replay_from(log, replay_engine, 0), "replay from 0");

    /* NULL 安全 */
    TEST_ASSERT(!command_log_replay(NULL, replay_engine), "replay with NULL log should fail");
    TEST_ASSERT(!command_log_replay_from(NULL, replay_engine, 0), "replay_from with NULL log should fail");
    TEST_ASSERT(!command_log_replay(log, NULL), "replay with NULL engine should fail");
    TEST_ASSERT(!command_log_execute(NULL, NULL, NULL), "execute with NULL should fail");

    command_log_destroy(log);
    lv_engine_destroy(replay_engine);
    lv_engine_destroy(engine);
    lv_cleanup();
}

/* ============================================================
 * ProofNavigator 测试
 * ============================================================ */

static void test_proof_navigator_create_destroy(void) {
    Proposition *prop = proposition_create(1, PROPOSITION_TYPE_ATOMIC);
    TEST_ASSERT_NOT_NULL(prop);

    ProofNavigator *nav = proof_navigator_create(prop, NULL);
    TEST_ASSERT_NOT_NULL(nav);
    TEST_ASSERT_EQ(nav->step_count, 0);
    TEST_ASSERT(nav->construction != NULL || nav->step_count == 0, "navigator created");

    proof_navigator_destroy(nav);
    proof_navigator_destroy(NULL);

    proposition_unref(prop);
}

static void test_proof_navigator_steps(void) {
    Proposition *prop = proposition_create(2, PROPOSITION_TYPE_ATOMIC);
    ProofNavigator *nav = proof_navigator_create(prop, NULL);
    TEST_ASSERT_NOT_NULL(nav);

    /* 添加步骤 */
    ProofStep *s1 = proof_step_create(PROOF_STEP_ADD_NODE);
    TEST_ASSERT_NOT_NULL(s1);
    s1->color = PROOF_COLOR_GREEN;
    TEST_ASSERT(proof_navigator_add_step(nav, s1), "add step 1");

    ProofStep *s2 = proof_step_create(PROOF_STEP_ADD_CONSTRAINT);
    TEST_ASSERT_NOT_NULL(s2);
    s2->color = PROOF_COLOR_YELLOW;
    s2->parent_step_id = 0;
    TEST_ASSERT(proof_navigator_add_step(nav, s2), "add step 2");

    ProofStep *s3 = proof_step_create(PROOF_STEP_UNIFY);
    TEST_ASSERT_NOT_NULL(s3);
    s3->color = PROOF_COLOR_GREEN;
    s3->parent_step_id = 0;
    TEST_ASSERT(proof_navigator_add_step(nav, s3), "add step 3");

    TEST_ASSERT_EQ(nav->step_count, 3);

    /* 当前步骤 */
    ProofStep *cur = proof_navigator_current_step(nav);
    TEST_ASSERT_NOT_NULL(cur);

    /* 导航 */
    TEST_ASSERT(proof_navigator_next(nav), "next");
    TEST_ASSERT(proof_navigator_next(nav), "next again");

    /* 超出范围 */
    TEST_ASSERT(!proof_navigator_next(nav), "next beyond end");

    /* 回退 */
    TEST_ASSERT(proof_navigator_prev(nav), "prev");
    TEST_ASSERT(proof_navigator_prev(nav), "prev again");
    TEST_ASSERT(!proof_navigator_prev(nav), "prev before start");

    /* 跳转 */
    TEST_ASSERT(proof_navigator_goto(nav, 1), "goto 1");
    TEST_ASSERT_EQ(proof_navigator_current_step(nav), nav->steps[1]);

    /* 越界跳转 */
    TEST_ASSERT(!proof_navigator_goto(nav, -1), "goto -1 should fail");
    TEST_ASSERT(!proof_navigator_goto(nav, 100), "goto 100 should fail");

    /* NULL 安全 */
    TEST_ASSERT(!proof_navigator_add_step(NULL, NULL), "add step NULL should fail");
    TEST_ASSERT(!proof_navigator_next(NULL), "next NULL should fail");
    TEST_ASSERT(!proof_navigator_prev(NULL), "prev NULL should fail");
    TEST_ASSERT(!proof_navigator_goto(NULL, 0), "goto NULL should fail");
    TEST_ASSERT_NULL(proof_navigator_current_step(NULL));

    proof_navigator_destroy(nav);
    proposition_unref(prop);
}

static void test_proof_navigator_breakpoints(void) {
    Proposition *prop = proposition_create(3, PROPOSITION_TYPE_ATOMIC);
    ProofNavigator *nav = proof_navigator_create(prop, NULL);
    TEST_ASSERT_NOT_NULL(nav);

    /* 添加带有断点的步骤 */
    for (int i = 0; i < 5; i++) {
        ProofStep *s = proof_step_create(PROOF_STEP_ADD_NODE);
        TEST_ASSERT_NOT_NULL(s);
        s->is_breakpoint = (i == 1 || i == 3);
        proof_navigator_add_step(nav, s);
    }

    /* 跳转到下一个断点 */
    TEST_ASSERT(proof_navigator_next_breakpoint(nav), "next bp 1");
    TEST_ASSERT_EQ(nav->current_step, 1);

    TEST_ASSERT(proof_navigator_next_breakpoint(nav), "next bp 2");
    TEST_ASSERT_EQ(nav->current_step, 3);

    /* 没有更多断点 */
    TEST_ASSERT(!proof_navigator_next_breakpoint(nav), "no more breakpoints");

    /* NULL 安全 */
    TEST_ASSERT(!proof_navigator_next_breakpoint(NULL), "next_breakpoint with NULL should fail");

    proof_navigator_destroy(nav);
    proposition_unref(prop);
}


/* ============================================================
 * 批次 C-㉙：proof.h 零覆盖设施契约测试
 * ============================================================ */

/* 流式上下文 setter/getter（宏生成 setter + 手写 getter） */
static void test_proof_stream_context(void) {
    StreamContext *ctx = stream_context_create();
    TEST_ASSERT_MSG(ctx != NULL, "stream_context_create 应成功");

    proof_set_stream_context(ctx);
    TEST_ASSERT_MSG(proof_get_stream_context() == ctx, "set 后 get 应返回同一上下文");

    proof_set_stream_context(NULL);
    TEST_ASSERT_MSG(proof_get_stream_context() == NULL, "set NULL 后 get 应返回 NULL");

    stream_context_destroy(ctx);
}

/* 公理库锁定状态 */
static void test_proof_axiom_lock(void) {
    proof_unlock_axioms(); /* 复位：避免残留影响断言 */
    TEST_ASSERT_MSG(!proof_axioms_is_locked(), "初始应未锁定");

    proof_lock_axioms();
    TEST_ASSERT_MSG(proof_axioms_is_locked(), "锁定后应报告锁定");

    proof_unlock_axioms();
    TEST_ASSERT_MSG(!proof_axioms_is_locked(), "解锁后应报告未锁定");
}

/* ⊥ 定义 set/get */
static void test_proof_bottom_definition(void) {
    Proposition *prop = proposition_create(10, PROPOSITION_TYPE_ATOMIC);
    ProofNavigator *nav = proof_navigator_create(prop, NULL);
    TEST_ASSERT_MSG(nav != NULL, "nav 创建应成功");

    TEST_ASSERT_MSG(proof_get_bottom_definition(nav) == NULL, "初始无 ⊥ 定义");

    BottomDefinition def;
    def.has_input_ports = true;
    def.input_port_count = 2;
    def.allow_explosion = false;
    proof_set_bottom_definition(nav, &def);
    const BottomDefinition *got = proof_get_bottom_definition(nav);
    TEST_ASSERT_MSG(got != NULL, "set 后应可读取");
    TEST_ASSERT_MSG(got->has_input_ports == true, "has_input_ports 应保留");
    TEST_ASSERT_MSG(got->input_port_count == 2, "input_port_count 应保留");
    TEST_ASSERT_MSG(got->allow_explosion == false, "allow_explosion 应保留");

    /* 覆盖更新 */
    def.allow_explosion = true;
    proof_set_bottom_definition(nav, &def);
    got = proof_get_bottom_definition(nav);
    TEST_ASSERT_MSG(got->allow_explosion == true, "allow_explosion 覆盖更新");

    /* NULL 安全 */
    TEST_ASSERT_MSG(proof_get_bottom_definition(NULL) == NULL, "NULL nav get 返回 NULL");
    proof_set_bottom_definition(NULL, &def);
    proof_set_bottom_definition(nav, NULL);

    proof_navigator_destroy(nav);
    proposition_unref(prop);
}

/* 引理视图状态 set/get（默认展开、更新已存在） */
static void test_proof_lemma_view_state(void) {
    Proposition *prop = proposition_create(11, PROPOSITION_TYPE_ATOMIC);
    ProofNavigator *nav = proof_navigator_create(prop, NULL);
    TEST_ASSERT_MSG(nav != NULL, "nav 创建应成功");

    /* 未设置 → 默认 EXPANDED */
    TEST_ASSERT_MSG(proof_get_lemma_view_state(nav, 5) == LEMMA_VIEW_STATE_EXPANDED, "未设置默认展开");

    /* 设置折叠 */
    proof_set_lemma_view_state(nav, 5, LEMMA_VIEW_STATE_COLLAPSED);
    TEST_ASSERT_MSG(proof_get_lemma_view_state(nav, 5) == LEMMA_VIEW_STATE_COLLAPSED, "设置折叠");

    /* 更新已存在的条目 */
    proof_set_lemma_view_state(nav, 5, LEMMA_VIEW_STATE_EXPANDED);
    TEST_ASSERT_MSG(proof_get_lemma_view_state(nav, 5) == LEMMA_VIEW_STATE_EXPANDED, "更新为展开");

    /* 多个步骤互不干扰 */
    proof_set_lemma_view_state(nav, 7, LEMMA_VIEW_STATE_COLLAPSED);
    TEST_ASSERT_MSG(proof_get_lemma_view_state(nav, 5) == LEMMA_VIEW_STATE_EXPANDED, "步骤5不受步骤7影响");
    TEST_ASSERT_MSG(proof_get_lemma_view_state(nav, 7) == LEMMA_VIEW_STATE_COLLAPSED, "步骤7折叠");

    /* NULL / 负 ID 安全 */
    TEST_ASSERT_MSG(proof_get_lemma_view_state(NULL, 5) == LEMMA_VIEW_STATE_EXPANDED, "NULL nav 返回默认");
    TEST_ASSERT_MSG(proof_get_lemma_view_state(nav, -1) == LEMMA_VIEW_STATE_EXPANDED, "负 ID 返回默认");
    proof_set_lemma_view_state(NULL, 5, LEMMA_VIEW_STATE_COLLAPSED);
    proof_set_lemma_view_state(nav, -1, LEMMA_VIEW_STATE_COLLAPSED);

    proof_navigator_destroy(nav);
    proposition_unref(prop);
}

/* 命题等价声明/查找（去重、双向） */
static void test_proof_equivalence(void) {
    Proposition *prop = proposition_create(12, PROPOSITION_TYPE_ATOMIC);
    ProofNavigator *nav = proof_navigator_create(prop, NULL);
    TEST_ASSERT_MSG(nav != NULL, "nav 创建应成功");

    /* 空表：查找无结果 */
    int ids[8];
    TEST_ASSERT_MSG(proof_find_equivalent_proposition(nav, 1, ids, 8) == 0, "空表查找为 0");

    /* 声明等价并双向查找 */
    proof_declare_proposition_equivalence(nav, 1, 2);
    TEST_ASSERT_MSG(proof_find_equivalent_proposition(nav, 1, ids, 8) == 1, "正向查找 1 个");
    TEST_ASSERT_MSG(ids[0] == 2, "1 的等价为 2");
    TEST_ASSERT_MSG(proof_find_equivalent_proposition(nav, 2, ids, 8) == 1, "反向查找 1 个");
    TEST_ASSERT_MSG(ids[0] == 1, "2 的等价为 1");

    /* 重复声明（同序/反序）不重复添加 */
    proof_declare_proposition_equivalence(nav, 1, 2);
    proof_declare_proposition_equivalence(nav, 2, 1);
    TEST_ASSERT_MSG(proof_find_equivalent_proposition(nav, 1, ids, 8) == 1, "重复声明不新增");

    /* 多个等价关系 */
    proof_declare_proposition_equivalence(nav, 1, 3);
    TEST_ASSERT_MSG(proof_find_equivalent_proposition(nav, 1, ids, 8) == 2, "1 有两个等价");
    TEST_ASSERT_MSG(proof_find_equivalent_proposition(nav, 3, ids, 8) == 1, "3 的等价为 1");
    TEST_ASSERT_MSG(ids[0] == 1, "3 的等价 id 为 1");

    /* NULL 安全 */
    TEST_ASSERT_MSG(proof_find_equivalent_proposition(NULL, 1, ids, 8) == 0, "NULL nav 返回 0");
    TEST_ASSERT_MSG(proof_find_equivalent_proposition(nav, 1, NULL, 8) == 0, "NULL 输出返回 0");
    TEST_ASSERT_MSG(proof_find_equivalent_proposition(nav, 1, ids, 0) == 0, "max_count 0 返回 0");
    proof_declare_proposition_equivalence(NULL, 1, 2);

    proof_navigator_destroy(nav);
    proposition_unref(prop);
}

/* 策略注释 set/get（LeanGeo 风格） */
static void test_proof_strategy_note(void) {
    Proposition *prop = proposition_create(13, PROPOSITION_TYPE_ATOMIC);
    ProofNavigator *nav = proof_navigator_create(prop, NULL);
    TEST_ASSERT_MSG(nav != NULL, "nav 创建应成功");

    TEST_ASSERT_MSG(proof_navigator_get_strategy_note(nav) == NULL, "初始无策略注释");

    TEST_ASSERT_MSG(proof_navigator_set_strategy_note(nav, "通过作辅助线构造相似三角形"), "设置策略注释");
    TEST_ASSERT_MSG(strcmp(proof_navigator_get_strategy_note(nav), "通过作辅助线构造相似三角形") == 0,
                    "策略注释往返");

    /* 覆盖 */
    TEST_ASSERT_MSG(proof_navigator_set_strategy_note(nav, "策略B"), "覆盖策略注释");
    TEST_ASSERT_MSG(strcmp(proof_navigator_get_strategy_note(nav), "策略B") == 0, "覆盖往返");

    /* 清除（空串 → NULL） */
    TEST_ASSERT_MSG(proof_navigator_set_strategy_note(nav, ""), "空串清除");
    TEST_ASSERT_MSG(proof_navigator_get_strategy_note(nav) == NULL, "清除后为 NULL");

    /* NULL 安全 */
    TEST_ASSERT_MSG(!proof_navigator_set_strategy_note(NULL, "x"), "NULL nav 应失败");
    TEST_ASSERT_MSG(proof_navigator_get_strategy_note(NULL) == NULL, "NULL nav get 返回 NULL");

    proof_navigator_destroy(nav);
    proposition_unref(prop);
}

/* 证明步骤注释 set */
static void test_proof_step_note(void) {
    ProofStep *step = proof_step_create(PROOF_STEP_ADD_NODE);
    TEST_ASSERT_MSG(step != NULL, "步骤创建应成功");

    TEST_ASSERT_MSG(proof_step_set_note(step, "辅助构造：连接点A与点B"), "设置注释");
    TEST_ASSERT_MSG(step->note != NULL, "注释已设置");
    TEST_ASSERT_MSG(strcmp(step->note, "辅助构造：连接点A与点B") == 0, "注释往返");

    /* 覆盖 */
    TEST_ASSERT_MSG(proof_step_set_note(step, "新注释"), "覆盖注释");
    TEST_ASSERT_MSG(strcmp(step->note, "新注释") == 0, "覆盖往返");

    /* 清除 */
    TEST_ASSERT_MSG(proof_step_set_note(step, ""), "空串清除");
    TEST_ASSERT_MSG(step->note == NULL, "清除后为 NULL");

    /* NULL 安全 */
    TEST_ASSERT_MSG(!proof_step_set_note(NULL, "x"), "NULL step 应失败");

    proof_step_destroy(step);
}

/* 命题前后置条件 set（数组内部拷贝） */
static void test_proposition_pre_post_conditions(void) {
    Proposition *prop = proposition_create(14, PROPOSITION_TYPE_IMPLICATION);
    TEST_ASSERT_MSG(prop != NULL, "命题创建应成功");

    int pre[] = {100, 101, 102};
    int post[] = {200, 201};

    TEST_ASSERT_MSG(proposition_set_preconditions(prop, pre, 3), "设置前置条件");
    TEST_ASSERT_MSG(prop->precondition_count == 3, "前置条件数量");
    TEST_ASSERT_MSG(prop->precondition_region_ids != NULL, "前置条件数组");
    TEST_ASSERT_MSG(prop->precondition_region_ids[0] == 100, "前置条件首元素");
    TEST_ASSERT_MSG(prop->precondition_region_ids[2] == 102, "前置条件尾元素");

    TEST_ASSERT_MSG(proposition_set_postconditions(prop, post, 2), "设置后置条件");
    TEST_ASSERT_MSG(prop->postcondition_count == 2, "后置条件数量");
    TEST_ASSERT_MSG(prop->postcondition_constraint_ids != NULL, "后置条件数组");
    TEST_ASSERT_MSG(prop->postcondition_constraint_ids[1] == 201, "后置条件尾元素");

    /* 覆盖为更短列表 */
    int pre2[] = {7};
    TEST_ASSERT_MSG(proposition_set_preconditions(prop, pre2, 1), "覆盖前置条件");
    TEST_ASSERT_MSG(prop->precondition_count == 1, "覆盖后数量");
    TEST_ASSERT_MSG(prop->precondition_region_ids[0] == 7, "覆盖后首元素");

    /* 清空 */
    TEST_ASSERT_MSG(proposition_set_preconditions(prop, NULL, 0), "清空前置条件");
    TEST_ASSERT_MSG(prop->precondition_count == 0, "清空后数量 0");

    /* NULL 安全 */
    TEST_ASSERT_MSG(!proposition_set_preconditions(NULL, pre, 1), "NULL prop 应失败");
    TEST_ASSERT_MSG(!proposition_set_postconditions(NULL, post, 1), "NULL prop 应失败");

    proposition_unref(prop);
}

/* 命题互斥判定 */
static void test_proposition_contradicts(void) {
    /* BOTTOM 与任意命题互斥 */
    Proposition *bottom = proposition_create(20, PROPOSITION_TYPE_BOTTOM);
    Proposition *atom = proposition_create(21, PROPOSITION_TYPE_ATOMIC);
    TEST_ASSERT_MSG(proposition_contradicts(bottom, atom), "BOTTOM 与任意命题互斥");

    /* 同引用不互斥 */
    TEST_ASSERT_MSG(!proposition_contradicts(atom, atom), "同引用不互斥");

    /* 普通原子命题之间不互斥 */
    Proposition *atom2 = proposition_create(22, PROPOSITION_TYPE_ATOMIC);
    TEST_ASSERT_MSG(!proposition_contradicts(atom, atom2), "无关原子命题不互斥");

    /* NEGATION 与子命题互斥：子命题须为与 b 不同引用且可判互斥的对象
     * （实现规则 2 递归 contradicts(子命题, b)；同引用 a==b 提前返回 false），
     * 故用 BOTTOM 子命题钉住递归路径。 */
    Proposition *neg = proposition_create(23, PROPOSITION_TYPE_NEGATION);
    TEST_ASSERT_MSG(proposition_add_sub_proposition(neg, bottom), "添加 BOTTOM 子命题");
    TEST_ASSERT_MSG(proposition_contradicts(neg, atom), "NEGATION(BOTTOM) 与任意命题互斥");

    /* 同一 ID + 不同信任颜色（GREEN vs ORANGE_EX_FALSO）互斥 */
    Proposition *c1 = proposition_create(30, PROPOSITION_TYPE_ATOMIC);
    Proposition *c2 = proposition_create(30, PROPOSITION_TYPE_ATOMIC);
    c1->color = PROOF_COLOR_GREEN;
    c2->color = PROOF_COLOR_ORANGE_EX_FALSO;
    TEST_ASSERT_MSG(proposition_contradicts(c1, c2), "同 ID 不同色（GREEN/EX_FALSO）互斥");

    /* 同 ID 同类型同蓝色（未探索）不互斥 */
    Proposition *b1 = proposition_create(31, PROPOSITION_TYPE_ATOMIC);
    Proposition *b2 = proposition_create(31, PROPOSITION_TYPE_ATOMIC);
    b1->color = PROOF_COLOR_BLUE_UNEXPLORED;
    b2->color = PROOF_COLOR_BLUE_UNEXPLORED;
    TEST_ASSERT_MSG(!proposition_contradicts(b1, b2), "双蓝未探索不互斥");

    /* NULL 安全 */
    TEST_ASSERT_MSG(!proposition_contradicts(NULL, atom), "NULL a 不互斥");
    TEST_ASSERT_MSG(!proposition_contradicts(atom, NULL), "NULL b 不互斥");

    proposition_unref(bottom);
    proposition_unref(atom);
    proposition_unref(atom2);
    proposition_unref(neg);
    proposition_unref(c1);
    proposition_unref(c2);
    proposition_unref(b1);
    proposition_unref(b2);
}

/* 证明步骤祖先链 */
static void test_proof_step_ancestors(void) {
    Proposition *prop = proposition_create(15, PROPOSITION_TYPE_ATOMIC);
    ProofNavigator *nav = proof_navigator_create(prop, NULL);
    TEST_ASSERT_MSG(nav != NULL, "nav 创建应成功");

    /* 链：1(根) → 2 → 3（add_step 自动分配 id，故 add 后覆写 id/parent） */
    ProofStep *s1 = proof_step_create(PROOF_STEP_ADD_NODE);
    TEST_ASSERT_MSG(proof_navigator_add_step(nav, s1), "添加步骤1");
    s1->id = 1;
    s1->parent_step_id = -1;

    ProofStep *s2 = proof_step_create(PROOF_STEP_ADD_CONSTRAINT);
    TEST_ASSERT_MSG(proof_navigator_add_step(nav, s2), "添加步骤2");
    s2->id = 2;
    s2->parent_step_id = 1;

    ProofStep *s3 = proof_step_create(PROOF_STEP_REWRITE);
    TEST_ASSERT_MSG(proof_navigator_add_step(nav, s3), "添加步骤3");
    s3->id = 3;
    s3->parent_step_id = 2;

    /* 步骤3 的祖先：[2, 1]（近→远） */
    int *anc = NULL;
    int anc_count = 0;
    TEST_ASSERT_MSG(proof_step_get_ancestors(nav, 3, &anc, &anc_count), "获取祖先");
    TEST_ASSERT_MSG(anc_count == 2, "祖先数量 2");
    TEST_ASSERT_MSG(anc != NULL, "祖先数组非空");
    TEST_ASSERT_MSG(anc[0] == 2, "最近祖先为 2");
    TEST_ASSERT_MSG(anc[1] == 1, "根祖先为 1");
    lv_free((void **) &anc);

    /* 根步骤：无祖先（count=0，但成功） */
    anc = NULL;
    anc_count = -1;
    TEST_ASSERT_MSG(proof_step_get_ancestors(nav, 1, &anc, &anc_count), "根步骤祖先获取");
    TEST_ASSERT_MSG(anc_count == 0, "根步骤祖先数 0");

    /* 不存在的步骤 → false */
    anc = NULL;
    anc_count = 0;
    TEST_ASSERT_MSG(!proof_step_get_ancestors(nav, 999, &anc, &anc_count), "不存在步骤应失败");

    /* NULL 安全 */
    TEST_ASSERT_MSG(!proof_step_get_ancestors(NULL, 1, &anc, &anc_count), "NULL nav 应失败");
    TEST_ASSERT_MSG(!proof_step_get_ancestors(nav, 1, NULL, &anc_count), "NULL 输出应失败");
    TEST_ASSERT_MSG(!proof_step_get_ancestors(nav, 1, &anc, NULL), "NULL count 应失败");

    proof_navigator_destroy(nav);
    proposition_unref(prop);
}

/* 断点保存/恢复 */
static void test_proof_breakpoint_save_restore(void) {
    Proposition *prop = proposition_create(16, PROPOSITION_TYPE_ATOMIC);
    ProofNavigator *nav = proof_navigator_create(prop, NULL);
    TEST_ASSERT_MSG(nav != NULL, "nav 创建应成功");

    /* 构造 3 步并导航到第 2 步 */
    for (int i = 0; i < 3; i++) {
        ProofStep *s = proof_step_create(PROOF_STEP_ADD_NODE);
        s->id = i + 1;
        proof_navigator_add_step(nav, s);
    }
    TEST_ASSERT_MSG(proof_navigator_goto(nav, 1), "goto 1");

    /* 保存断点（ID 9001 避免与其他测试冲突） */
    TEST_ASSERT_MSG(proof_save_breakpoint(nav, 9001), "保存断点");

    /* 修改导航状态 */
    TEST_ASSERT_MSG(proof_navigator_goto(nav, 2), "goto 2");
    nav->is_complete = true;

    /* 恢复断点 */
    TEST_ASSERT_MSG(proof_restore_breakpoint(nav, 9001), "恢复断点");
    TEST_ASSERT_MSG(nav->current_step == 1, "恢复后 current_step 还原");
    TEST_ASSERT_MSG(!nav->is_complete, "恢复后 is_complete 还原");

    /* 恢复不存在的断点 → false */
    TEST_ASSERT_MSG(!proof_restore_breakpoint(nav, 9999), "不存在断点应失败");

    /* 保存非法 ID → false */
    TEST_ASSERT_MSG(!proof_save_breakpoint(nav, -1), "负 ID 保存应失败");

    /* NULL 安全 */
    TEST_ASSERT_MSG(!proof_save_breakpoint(NULL, 9002), "NULL nav 保存应失败");
    TEST_ASSERT_MSG(!proof_restore_breakpoint(NULL, 9001), "NULL nav 恢复应失败");

    proof_navigator_destroy(nav);
    proposition_unref(prop);
}

/* 搜索树：创建/添加/标记/策略注册/设置 */
static void test_proof_search_tree(void) {
    ProofSearchTree *tree = proof_search_tree_create();
    TEST_ASSERT_MSG(tree != NULL, "搜索树创建应成功");
    TEST_ASSERT_MSG(tree->root == NULL, "初始无根");
    TEST_ASSERT_MSG(tree->node_count == 0, "初始节点数 0");

    /* 根节点 */
    BacktrackNode *root = backtrack_node_create(BACKTRACK_CHOICE_POINT, "根");
    TEST_ASSERT_MSG(root != NULL, "根节点创建");
    TEST_ASSERT_MSG(proof_search_tree_add_child(tree, NULL, root), "添加根节点");
    TEST_ASSERT_MSG(tree->node_count == 1, "节点数 1");
    TEST_ASSERT_MSG(tree->root == root, "根指针");
    TEST_ASSERT_MSG(root->id == 0, "根节点 id 0");

    /* 第二个根 → 失败 */
    BacktrackNode *root2 = backtrack_node_create(BACKTRACK_CHOICE_POINT, "根2");
    TEST_ASSERT_MSG(!proof_search_tree_add_child(tree, NULL, root2), "重复根应失败");

    /* 子节点 */
    BacktrackNode *child = backtrack_node_create(BACKTRACK_SUCCESS, "成功");
    TEST_ASSERT_MSG(proof_search_tree_add_child(tree, root, child), "添加子节点");
    TEST_ASSERT_MSG(tree->node_count == 2, "节点数 2");
    TEST_ASSERT_MSG(root->child_count == 1, "子节点数 1");
    TEST_ASSERT_MSG(child->parent == root, "父子指针");
    TEST_ASSERT_MSG(child->id == 1, "子节点 id 1");
    TEST_ASSERT_MSG(tree->success_paths == 1, "成功路径 1");
    TEST_ASSERT_MSG(tree->max_depth == 1, "最大深度 1");

    /* 失败/剪枝统计 */
    BacktrackNode *fail = backtrack_node_create(BACKTRACK_FAILURE, "失败");
    BacktrackNode *prune = backtrack_node_create(BACKTRACK_PRUNE, "剪枝");
    TEST_ASSERT_MSG(proof_search_tree_add_child(tree, root, fail), "添加失败节点");
    TEST_ASSERT_MSG(proof_search_tree_add_child(tree, root, prune), "添加剪枝节点");
    TEST_ASSERT_MSG(tree->failure_paths == 1, "失败路径 1");
    TEST_ASSERT_MSG(tree->pruned_branches == 1, "剪枝分支 1");

    /* 标记回溯点 */
    backtrack_node_mark_backtrack(fail, "面积法");
    TEST_ASSERT_MSG(fail->is_backtrack_point == true, "回溯点标记");
    TEST_ASSERT_MSG(fail->strategy_name != NULL && strcmp(fail->strategy_name, "面积法") == 0, "回溯策略名");

    /* 策略注册（去重） */
    proof_search_tree_register_strategy(tree, "面积法");
    proof_search_tree_register_strategy(tree, "面积法");
    proof_search_tree_register_strategy(tree, "向量法");
    TEST_ASSERT_MSG(tree->strategy_count == 2, "策略去重后 2 个");

    /* 设置当前策略 */
    proof_search_tree_set_strategy(tree, "面积法");
    TEST_ASSERT_MSG(tree->current_strategy != NULL && strcmp(tree->current_strategy, "面积法") == 0, "当前策略");
    proof_search_tree_set_strategy(tree, "");
    TEST_ASSERT_MSG(tree->current_strategy == NULL, "空串清除当前策略");

    /* NULL 安全 */
    BacktrackNode *nulllabel = backtrack_node_create(BACKTRACK_CHOICE_POINT, NULL);
    TEST_ASSERT_MSG(nulllabel != NULL, "NULL label 创建应成功");
    TEST_ASSERT_MSG(nulllabel->label == NULL, "NULL label 保持 NULL");
    lv_free((void **) &nulllabel);
    TEST_ASSERT_MSG(!proof_search_tree_add_child(NULL, NULL, root), "NULL tree 应失败");
    TEST_ASSERT_MSG(!proof_search_tree_add_child(tree, NULL, NULL), "NULL child 应失败");
    backtrack_node_mark_backtrack(NULL, "x");
    proof_search_tree_register_strategy(NULL, "x");
    proof_search_tree_register_strategy(tree, NULL);
    proof_search_tree_set_strategy(NULL, "x");

    proof_search_tree_destroy(tree);
    proof_search_tree_destroy(NULL);
}

/* 依赖链验证与颜色降级 */
static void test_proof_validate_dependencies(void) {
    Proposition *prop = proposition_create(17, PROPOSITION_TYPE_ATOMIC);
    ProofNavigator *nav = proof_navigator_create(prop, NULL);
    TEST_ASSERT_MSG(nav != NULL, "nav 创建应成功");

    /* 步骤：add_step 后 id 由导航器分配（=0），以实际 id 建立依赖 */
    ProofStep *s = proof_step_create(PROOF_STEP_ADD_NODE);
    s->color = PROOF_COLOR_GREEN;
    TEST_ASSERT_MSG(proof_navigator_add_step(nav, s), "添加步骤");
    TEST_ASSERT_MSG(s->id == 0, "add_step 分配 id 0");

    /* 依赖树：内容哈希为 NULL → 视为哈希变化 → GREEN 降级 YELLOW */
    ProofDependency *dep = proof_dependency_create(PROOF_COLOR_GREEN);
    dep->id = s->id;
    dep->content_hash = NULL;
    nav->dep_tree = dep;

    DependencyUpdateResult results[4];
    int n = proof_validate_dependencies(nav, results, 4);
    TEST_ASSERT_MSG(n == 1, "应产生 1 个更新");
    TEST_ASSERT_MSG(results[0].dependency_id == s->id, "更新依赖 id");
    TEST_ASSERT_MSG(results[0].old_color == PROOF_COLOR_GREEN, "旧颜色 GREEN");
    TEST_ASSERT_MSG(results[0].hash_changed == true, "NULL 哈希视为变化");
    TEST_ASSERT_MSG(results[0].new_color == PROOF_COLOR_YELLOW, "降级为 YELLOW");
    TEST_ASSERT_MSG(s->color == PROOF_COLOR_YELLOW, "步骤颜色降级");

    /* 有哈希 → 无更新 */
    dep->content_hash = lv_strdup_safe("abc123");
    n = proof_validate_dependencies(nav, results, 4);
    TEST_ASSERT_MSG(n == 0, "有哈希无更新");

    /* NULL 安全 */
    TEST_ASSERT_MSG(proof_validate_dependencies(NULL, results, 4) == 0, "NULL nav 返回 0");
    TEST_ASSERT_MSG(proof_validate_dependencies(nav, NULL, 4) == 0, "NULL results 返回 0");
    TEST_ASSERT_MSG(proof_validate_dependencies(nav, results, 0) == 0, "max 0 返回 0");

    proof_navigator_destroy(nav); /* 释放 dep_tree */
    proposition_unref(prop);
}

/* ghost 标记与冲突检查 */
static void test_proof_ghost(void) {
    /* 越界标记失败 */
    TEST_ASSERT_MSG(!proof_mark_ghost(-1, PROOF_QTT_ERASED), "负 ID 标记应失败");
    TEST_ASSERT_MSG(!proof_mark_ghost(4096, PROOF_QTT_ERASED), "越界 ID 标记应失败");

    /* 正常标记 */
    TEST_ASSERT_MSG(proof_mark_ghost(101, PROOF_QTT_ERASED), "标记 ERASED");
    TEST_ASSERT_MSG(proof_mark_ghost(102, PROOF_QTT_LINEAR), "标记 LINEAR");

    /* 无绑定导航器：无冲突 */
    TEST_ASSERT_MSG(proof_check_ghost_conflicts() == 0, "无绑定导航器无冲突");

    /* 绑定导航器：runtime 步骤依赖 ERASED 步骤 → 冲突 */
    Proposition *prop = proposition_create(18, PROPOSITION_TYPE_ATOMIC);
    ProofNavigator *nav = proof_navigator_create(prop, NULL);
    TEST_ASSERT_MSG(nav != NULL, "nav 创建应成功");
    proof_ghost_set_navigator(nav);

    /* 步骤 103（runtime，非 ERASED）依赖步骤 101（ERASED）→ 1 冲突 */
    ProofStep *s = proof_step_create(PROOF_STEP_ADD_NODE);
    s->id = 103;
    TEST_ASSERT_MSG(proof_step_add_dependency(s, 101), "添加依赖 ERASED 步骤");
    TEST_ASSERT_MSG(proof_navigator_add_step(nav, s), "添加 runtime 步骤");
    TEST_ASSERT_MSG(proof_check_ghost_conflicts() == 1, "1 个冲突");

    /* 步骤 102 为 LINEAR（runtime）：若依赖 ERASED 同样冲突（计数增加） */
    ProofStep *s2 = proof_step_create(PROOF_STEP_ADD_NODE);
    s2->id = 104;
    TEST_ASSERT_MSG(proof_step_add_dependency(s2, 102), "添加依赖");
    TEST_ASSERT_MSG(proof_navigator_add_step(nav, s2), "添加步骤");
    TEST_ASSERT_MSG(proof_check_ghost_conflicts() == 1, "LINEAR 依赖不冲突（仅 ERASED 计冲突）");

    /* 第二个 runtime 步骤也依赖 101 → 冲突计数 +1 = 2 */
    ProofStep *s3 = proof_step_create(PROOF_STEP_ADD_NODE);
    s3->id = 105;
    TEST_ASSERT_MSG(proof_step_add_dependency(s3, 101), "添加第二个依赖");
    TEST_ASSERT_MSG(proof_navigator_add_step(nav, s3), "添加第二个 runtime 步骤");
    TEST_ASSERT_MSG(proof_check_ghost_conflicts() == 2, "两个 runtime 依赖同一 ERASED → 2 冲突");

    proof_navigator_destroy(nav); /* 自动解除 ghost 导航器绑定 */
    proposition_unref(prop);
}

/* 多策略引擎管理 API（注册/激活/切换/统计/适用性/流水线） */
static bool stub_applicability(const ProofMultiStrategy *mse, const ConstraintGraph *graph, const Proposition *prop) {
    (void) mse;
    (void) graph;
    (void) prop;
    return true;
}

static bool stub_execute(ProofMultiStrategy *mse, ProofNavigator *nav) {
    (void) mse;
    (void) nav;
    return true;
}

static void test_proof_multi_strategy_manage(void) {
    ProofMultiStrategy *mse = proof_multi_strategy_create(NULL);
    TEST_ASSERT_MSG(mse != NULL, "mse 创建应成功");
    TEST_ASSERT_MSG(proof_multi_strategy_get_active(mse) == NULL, "初始无激活策略");

    /* 自定义描述符注册（DIRECT_CONSTRUCTION 为 AVAILABLE 默认域） */
    ProofStrategyDescriptor desc;
    memset(&desc, 0, sizeof(desc));
    desc.type = PROOF_STRATEGY_DIRECT_CONSTRUCTION;
    desc.status = PROOF_STRATEGY_AVAILABLE;
    desc.name = (char *) "自定义直接构造";
    desc.description = (char *) "测试描述";
    desc.applicability_check = stub_applicability;
    desc.execute = stub_execute;
    desc.search_algorithm = PROOF_SEARCH_DFS;
    TEST_ASSERT_MSG(proof_multi_strategy_register(mse, &desc), "注册策略");

    /* 激活并读取 */
    TEST_ASSERT_MSG(proof_multi_strategy_activate(mse, PROOF_STRATEGY_DIRECT_CONSTRUCTION), "激活策略");
    const ProofStrategyDescriptor *active = proof_multi_strategy_get_active(mse);
    TEST_ASSERT_MSG(active != NULL, "激活后应有活动策略");
    TEST_ASSERT_MSG(active->type == PROOF_STRATEGY_DIRECT_CONSTRUCTION, "活动策略类型");

    /* 适用性评估：stub 恒 true → 至少包含已注册策略 */
    ProofStrategyType types[8];
    int n = proof_multi_strategy_evaluate_applicability(mse, NULL, NULL, types, 8);
    TEST_ASSERT_MSG(n > 0, "适用性评估应返回正数");
    bool found = false;
    for (int i = 0; i < n; i++) {
        if (types[i] == PROOF_STRATEGY_DIRECT_CONSTRUCTION)
            found = true;
    }
    TEST_ASSERT_MSG(found, "已注册策略应出现在适用列表中");

    /* 执行 */
    TEST_ASSERT_MSG(proof_multi_strategy_execute(mse), "执行策略");

    /* 统计 */
    int attempts = -1, success = -1;
    proof_multi_strategy_get_stats(mse, &attempts, &success);
    TEST_ASSERT_MSG(attempts == 1, "尝试次数 1");
    TEST_ASSERT_MSG(success == 1, "成功次数 1");

    /* 流水线：单策略管道 */
    ProofStrategyType pipeline[] = {PROOF_STRATEGY_DIRECT_CONSTRUCTION};
    TEST_ASSERT_MSG(proof_multi_strategy_pipeline(mse, pipeline, 1), "流水线执行");

    /* 切换：LAMBDA_UNIFY 默认无公理包依赖（default_strategy_table 未声明），
     * 可直接激活；COORDINATE 默认依赖 field_theory（未挂载引擎时不可激活）。 */
    TEST_ASSERT_MSG(proof_multi_strategy_switch(mse, PROOF_STRATEGY_LAMBDA_UNIFY), "切换策略");
    active = proof_multi_strategy_get_active(mse);
    TEST_ASSERT_MSG(active != NULL, "切换后有活动策略");
    TEST_ASSERT_MSG(active->type == PROOF_STRATEGY_LAMBDA_UNIFY, "切换后类型为 LAMBDA_UNIFY");

    /* NULL 安全 */
    TEST_ASSERT_MSG(!proof_multi_strategy_register(NULL, &desc), "NULL mse 注册失败");
    TEST_ASSERT_MSG(!proof_multi_strategy_register(mse, NULL), "NULL desc 注册失败");
    TEST_ASSERT_MSG(!proof_multi_strategy_activate(NULL, PROOF_STRATEGY_DIRECT_CONSTRUCTION), "NULL mse 激活失败");
    TEST_ASSERT_MSG(proof_multi_strategy_get_active(NULL) == NULL, "NULL mse 无活动策略");
    TEST_ASSERT_MSG(proof_multi_strategy_evaluate_applicability(NULL, NULL, NULL, types, 8) == 0, "NULL 评估 0");
    TEST_ASSERT_MSG(!proof_multi_strategy_execute(NULL), "NULL mse 执行失败");
    TEST_ASSERT_MSG(!proof_multi_strategy_pipeline(NULL, pipeline, 1), "NULL mse 流水线失败");
    TEST_ASSERT_MSG(!proof_multi_strategy_pipeline(mse, NULL, 1), "NULL pipeline 失败");
    TEST_ASSERT_MSG(!proof_multi_strategy_pipeline(mse, pipeline, 0), "空流水线失败");
    TEST_ASSERT_MSG(!proof_multi_strategy_switch(NULL, PROOF_STRATEGY_COORDINATE), "NULL mse 切换失败");
    proof_multi_strategy_get_stats(NULL, NULL, NULL);

    proof_multi_strategy_destroy(mse);
}

/* 类型变量检测 / 命题实例化 */
static void test_proof_instantiate(void) {
    /* NULL 安全 */
    TEST_ASSERT_MSG(!proof_has_type_variables(NULL), "NULL prop 无类型变量");

    /* 无模式、无类型信息的原子命题 → 无类型变量 */
    Proposition *prop = proposition_create(40, PROPOSITION_TYPE_ATOMIC);
    TEST_ASSERT_MSG(!proof_has_type_variables(prop), "无模式命题无类型变量");

    /* 实例化：无映射 → 深拷贝 */
    int ports[] = {1, 2};
    int posts[] = {3};
    proposition_set_input_ports(prop, ports, 2);
    proposition_set_output_ports(prop, ports, 2);
    proposition_set_preconditions(prop, ports, 1);
    proposition_set_postconditions(prop, posts, 1);

    Proposition *inst = proof_instantiate_proposition(prop, NULL, 0);
    TEST_ASSERT_MSG(inst != NULL, "实例化应成功");
    TEST_ASSERT_MSG(inst->id == prop->id, "实例 id 保留");
    TEST_ASSERT_MSG(inst->type == prop->type, "实例类型保留");
    TEST_ASSERT_MSG(inst->input_count == 2, "实例输入端口数");
    TEST_ASSERT_MSG(inst->precondition_count == 1, "实例前置条件数");
    TEST_ASSERT_MSG(inst->postcondition_count == 1, "实例后置条件数");

    /* 原命题未被修改 */
    TEST_ASSERT_MSG(prop->input_count == 2, "原命题输入端口数不变");

    /* NULL 安全 */
    TEST_ASSERT_MSG(proof_instantiate_proposition(NULL, NULL, 0) == NULL, "NULL prop 实例化返回 NULL");

    proposition_unref(inst);
    proposition_unref(prop);
}

/* 不可构造性检查：NULL 契约 + 未匹配路径 */
static void test_proof_unconstructibility(void) {
    /* NULL 参数 → UNCONSTRUCT_ERROR */
    UnconstructInfo info;
    memset(&info, 0, sizeof(info));
    TEST_ASSERT_MSG(proof_check_unconstructibility(NULL, NULL, NULL, &info) == UNCONSTRUCT_ERROR, "NULL 参数报错");
    TEST_ASSERT_MSG(info.result == UNCONSTRUCT_ERROR, "NULL 参数 info.result 置 ERROR");

    /* 合法 nav + 空图（无 engine）：未匹配 → MAYBE_POSSIBLE */
    Proposition *prop = proposition_create(41, PROPOSITION_TYPE_ATOMIC);
    ProofNavigator *nav = proof_navigator_create(prop, NULL);
    TEST_ASSERT_MSG(nav != NULL, "nav 创建应成功");
    ConstraintGraph *graph = graph_create();
    TEST_ASSERT_MSG(graph != NULL, "graph 创建应成功");

    memset(&info, 0, sizeof(info));
    UnconstructResult r = proof_check_unconstructibility(nav, graph, NULL, &info);
    TEST_ASSERT_MSG(r == UNCONSTRUCT_MAYBE_POSSIBLE, "未匹配返回 MAYBE_POSSIBLE");
    TEST_ASSERT_MSG(info.result == UNCONSTRUCT_MAYBE_POSSIBLE, "info.result 同步");

    unconstruct_info_destroy(&info);
    unconstruct_info_destroy(NULL);

    graph_destroy(graph);
    proof_navigator_destroy(nav);
    proposition_unref(prop);
}

/* 自然语言步骤描述 */
static void test_proof_step_natural_language(void) {
    ProofStep *step = proof_step_create(PROOF_STEP_ADD_NODE);
    step->id = 1;
    step->color = PROOF_COLOR_GREEN;

    char *zh = proof_step_get_natural_language(step, PROOF_NL_LANG_ZH_CN);
    TEST_ASSERT_MSG(zh != NULL, "中文描述非空");
    TEST_ASSERT_MSG(strstr(zh, "步骤") != NULL, "中文描述应含步骤字样");
    lv_free((void **) &zh);

    char *en = proof_step_get_natural_language(step, PROOF_NL_LANG_EN_US);
    TEST_ASSERT_MSG(en != NULL, "英文描述非空");
    TEST_ASSERT_MSG(strstr(en, "Step") != NULL, "英文描述应含 Step 字样");
    lv_free((void **) &en);

    /* NULL step */
    TEST_ASSERT_MSG(proof_step_get_natural_language(NULL, PROOF_NL_LANG_ZH_CN) == NULL, "NULL step 返回 NULL");

    proof_step_destroy(step);
}

TEST_MAIN_BEGIN("Proof Infrastructure")

    /* ── Command Log ── */
    printf("\n--- Command Log ---\n");
    TEST_MAIN_RUN(test_command_log_lifecycle);
    TEST_MAIN_RUN(test_command_log_append);
    TEST_MAIN_RUN(test_command_entry_create_all_types);
    TEST_MAIN_RUN(test_command_log_clear);
    TEST_MAIN_RUN(test_command_entry_serialize_json);
    TEST_MAIN_RUN(test_command_log_execute_replay);

    /* ── Proof Navigator ── */
    printf("\n--- Proof Navigator ---\n");
    TEST_MAIN_RUN(test_proof_navigator_create_destroy);
    TEST_MAIN_RUN(test_proof_navigator_steps);
    TEST_MAIN_RUN(test_proof_navigator_breakpoints);

    /* ── C-㉙: proof.h 零覆盖设施 ── */
    printf("\n--- Proof Facilities (C-㉙) ---\n");
    TEST_MAIN_RUN(test_proof_stream_context);
    TEST_MAIN_RUN(test_proof_axiom_lock);
    TEST_MAIN_RUN(test_proof_bottom_definition);
    TEST_MAIN_RUN(test_proof_lemma_view_state);
    TEST_MAIN_RUN(test_proof_equivalence);
    TEST_MAIN_RUN(test_proof_strategy_note);
    TEST_MAIN_RUN(test_proof_step_note);
    TEST_MAIN_RUN(test_proposition_pre_post_conditions);
    TEST_MAIN_RUN(test_proposition_contradicts);
    TEST_MAIN_RUN(test_proof_step_ancestors);
    TEST_MAIN_RUN(test_proof_breakpoint_save_restore);
    TEST_MAIN_RUN(test_proof_search_tree);
    TEST_MAIN_RUN(test_proof_validate_dependencies);
    TEST_MAIN_RUN(test_proof_ghost);
    TEST_MAIN_RUN(test_proof_multi_strategy_manage);
    TEST_MAIN_RUN(test_proof_instantiate);
    TEST_MAIN_RUN(test_proof_unconstructibility);
    TEST_MAIN_RUN(test_proof_step_natural_language);

TEST_MAIN_END()
