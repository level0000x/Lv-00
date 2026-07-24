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
    TEST_ASSERT_FALSE(command_log_append(NULL, NULL));
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
    TEST_ASSERT(e->params.normalize_graph.scope_aware);
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
    TEST_ASSERT_FALSE(command_log_serialize_json(NULL, "test.json"));
    TEST_ASSERT_FALSE(command_log_serialize_json(log, NULL));
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
    TEST_ASSERT_FALSE(command_log_replay(NULL, replay_engine));
    TEST_ASSERT_FALSE(command_log_replay_from(NULL, replay_engine, 0));
    TEST_ASSERT_FALSE(command_log_replay(log, NULL));
    TEST_ASSERT_FALSE(command_log_execute(NULL, NULL, NULL));

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
    TEST_ASSERT_FALSE(proof_navigator_next(nav), "next beyond end");

    /* 回退 */
    TEST_ASSERT(proof_navigator_prev(nav), "prev");
    TEST_ASSERT(proof_navigator_prev(nav), "prev again");
    TEST_ASSERT_FALSE(proof_navigator_prev(nav), "prev before start");

    /* 跳转 */
    TEST_ASSERT(proof_navigator_goto(nav, 1), "goto 1");
    TEST_ASSERT_EQ(proof_navigator_current_step(nav), nav->steps[1]);

    /* 越界跳转 */
    TEST_ASSERT_FALSE(proof_navigator_goto(nav, -1));
    TEST_ASSERT_FALSE(proof_navigator_goto(nav, 100));

    /* NULL 安全 */
    TEST_ASSERT_FALSE(proof_navigator_add_step(NULL, NULL));
    TEST_ASSERT_FALSE(proof_navigator_next(NULL));
    TEST_ASSERT_FALSE(proof_navigator_prev(NULL));
    TEST_ASSERT_FALSE(proof_navigator_goto(NULL, 0));
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
    TEST_ASSERT_FALSE(proof_navigator_next_breakpoint(nav));

    /* NULL 安全 */
    TEST_ASSERT_FALSE(proof_navigator_next_breakpoint(NULL));

    proof_navigator_destroy(nav);
    proposition_unref(prop);
}

static void test_pro