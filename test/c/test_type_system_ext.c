/**
 * @file test_type_system_ext.c
 * @brief 类型系统扩展契约测试（批次 C-㊳：type_system.h 零覆盖 API）
 *
 * 补充 test_type_system.c 未覆盖的 25+ 个零覆盖 API：
 *   - 深拷贝族：type_region_deep_copy / deep_free / foreach_child / destroy
 *   - 谓词子类型族：type_create_predicate_subtype / get_base /
 *     check_predicate_subtype_value
 *   - 端口/推断族：type_check_port_compatibility / type_infer_port /
 *     type_substitute_variable / type_normalize
 *   - 推断规则表：type_system_register_inference_rule / get_inference_rules /
 *     clear_inference_rules / type_infer_by_rules
 *   - 重写路径族：type_rewrite_path_create / destroy / record / replay /
 *     type_system_get_rewrite_path / type_system_set_stream_context
 *   - 路径探索器：path_explorer_* 全家族 11 个 API
 *   - 打印：type_print
 *
 * 契约要点（与 type_system.h 注释 / 实现核对）：
 *   - type_region_destroy 不递归销毁子类型、不修改 TypeSystem 注册表，
 *     测试仅对其 NULL 契约断言（对已注册 region 手动 destroy 会与
 *     type_system_destroy 双释放，故不做）。
 *   - type_region_deep_copy 产物必须用 type_region_deep_free 释放（配对）。
 *   - type_normalize 对简单类型可能返回原类型指针（非新分配），调用者
 *     不得销毁 out_normalized。
 *   - type_substitute_variable 对匹配变量直接赋值 replacement 指针（引用
 *     语义），测试只断言指针关系，不销毁 replacement。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== foreach_child 回调统计 ============== */

typedef struct {
    int calls; /* 回调调用次数 */
    int nulls; /* child 为 NULL 的次数 */
} ChildStat;

static void count_child_cb(TypeRegion *child, void *ctx) {
    ChildStat *s = (ChildStat *) ctx;
    if (s) {
        s->calls++;
        if (!child)
            s->nulls++;
    }
}

/* ============== 测试：深拷贝族 ============== */

static void test_region_deep_copy_api(void) {
    /* NULL 契约 */
    TEST_ASSERT_NULL(type_region_deep_copy(NULL));
    type_region_deep_free(NULL); /* 不崩溃即通过 */
    type_region_destroy(NULL);   /* 不崩溃即通过 */

    TypeSystem *ts = type_system_create();
    TEST_ASSERT_NOT_NULL(ts);

    /* 简单类型深拷贝：kind/level/别名一致，指针不同 */
    TypeRegion *point = type_create_point(ts);
    TEST_ASSERT_NOT_NULL(point);
    TEST_ASSERT(type_add_alias(point, "Pt"), "设置别名");

    TypeRegion *copy = type_region_deep_copy(point);
    TEST_ASSERT_NOT_NULL(copy);
    TEST_ASSERT(copy != point, "深拷贝产生新对象");
    TEST_ASSERT_EQ(copy->kind, point->kind);
    TEST_ASSERT_EQ(copy->level, point->level);
    TEST_ASSERT(copy->alias_name && strcmp(copy->alias_name, "Pt") == 0, "别名已复制");
    type_region_deep_free(copy);

    /* 复合类型（函数）深拷贝：递归复制子类型 */
    TypeRegion *seg = type_create_line_segment(ts);
    TypeRegion *fn = type_create_function(ts, point, seg);
    TypeRegion *fn_copy = type_region_deep_copy(fn);
    TEST_ASSERT_NOT_NULL(fn_copy);
    TEST_ASSERT_EQ(fn_copy->kind, TYPE_KIND_FUNCTION);
    TEST_ASSERT_NOT_NULL(fn_copy->input_type);
    TEST_ASSERT_NOT_NULL(fn_copy->output_type);
    TEST_ASSERT_EQ(fn_copy->input_type->kind, TYPE_KIND_POINT);
    TEST_ASSERT_EQ(fn_copy->output_type->kind, TYPE_KIND_LINE_SEGMENT);
    TEST_ASSERT(fn_copy->input_type != fn->input_type, "子类型也深拷贝");
    type_region_deep_free(fn_copy);

    /* foreach_child：NULL tr / NULL cb 不调用回调 */
    ChildStat stat = {0, 0};
    type_region_foreach_child(NULL, count_child_cb, &stat);
    TEST_ASSERT_EQ(stat.calls, 0);
    /* 简单点类型：9 个字段全 NULL，回调被调用 9 次且全为 NULL */
    stat.calls = 0;
    stat.nulls = 0;
    type_region_foreach_child(point, count_child_cb, &stat);
    TEST_ASSERT_EQ(stat.calls, 9);
    TEST_ASSERT_EQ(stat.nulls, 9);
    /* 函数类型：input/output 两个非 NULL 子节点 */
    stat.calls = 0;
    stat.nulls = 0;
    type_region_foreach_child(fn, count_child_cb, &stat);
    TEST_ASSERT_EQ(stat.calls, 9);
    TEST_ASSERT_EQ(stat.nulls, 7);

    type_system_destroy(ts);
    printf("  test_region_deep_copy_api: PASSED\n");
}

/* ============== 测试：谓词子类型族 ============== */

static void test_predicate_subtype_api(void) {
    TypeSystem *ts = type_system_create();
    TEST_ASSERT_NOT_NULL(ts);

    /* NULL 契约 */
    TEST_ASSERT_NULL(type_create_predicate_subtype(NULL, NULL, "P", NULL));
    TypeRegion *base = type_create_point(ts);
    TEST_ASSERT_NULL(type_create_predicate_subtype(ts, NULL, "P", NULL));
    TEST_ASSERT_NULL(type_create_predicate_subtype(ts, base, NULL, NULL));

    /* 正常创建 */
    TypeRegion *sub = type_create_predicate_subtype(ts, base, "IsOrigin", "x == 0");
    TEST_ASSERT_NOT_NULL(sub);
    TEST_ASSERT_EQ(sub->kind, TYPE_KIND_PREDICATE_SUBTYPE);
    TEST_ASSERT(sub->base_type == base, "基类型引用");
    TEST_ASSERT(sub->predicate_name && strcmp(sub->predicate_name, "IsOrigin") == 0, "谓词名复制");
    TEST_ASSERT(sub->predicate_expr && strcmp(sub->predicate_expr, "x == 0") == 0, "谓词表达式复制");
    TEST_ASSERT_EQ(sub->level, base->level);
    TEST_ASSERT_EQ(sub->predicate_constraint_id, -1);

    /* get_base：NULL / 非谓词类型 / 谓词类型 */
    TEST_ASSERT_NULL(type_predicate_subtype_get_base(NULL));
    TEST_ASSERT_NULL(type_predicate_subtype_get_base(base));
    TEST_ASSERT(type_predicate_subtype_get_base(sub) == base, "谓词基类型");

    /* check_predicate_subtype_value：NULL 契约 + 未附加节点 */
    TEST_ASSERT(!type_check_predicate_subtype_value(NULL, sub, 1), "NULL ts");
    TEST_ASSERT(!type_check_predicate_subtype_value(ts, NULL, 1), "NULL subtype");
    TEST_ASSERT(!type_check_predicate_subtype_value(ts, base, 1), "非谓词子类型");
    TEST_ASSERT(!type_check_predicate_subtype_value(ts, sub, 1), "节点无附加类型");

    /* 正路径：附加基类型后，值满足谓词约束（constraint_id=-1 基类型兼容即可） */
    TEST_ASSERT(type_attach_to_node(ts, 1, base), "附加基类型");
    TEST_ASSERT(type_check_predicate_subtype_value(ts, sub, 1), "基类型兼容 → 满足");

    type_system_destroy(ts);
    printf("  test_predicate_subtype_api: PASSED\n");
}

/* ============== 测试：端口兼容 / 端口推断 / 变量替换 / 规范化 ============== */

static void test_port_infer_api(void) {
    TypeSystem *ts = type_system_create();
    TEST_ASSERT_NOT_NULL(ts);

    /* type_check_port_compatibility：NULL 契约 */
    TEST_ASSERT_EQ(type_check_port_compatibility(NULL, NULL, NULL), TYPE_CHECK_ERROR);
    TypeRegion *point = type_create_point(ts);
    TypeRegion *seg = type_create_line_segment(ts);
    TEST_ASSERT_EQ(type_check_port_compatibility(ts, NULL, seg), TYPE_CHECK_ERROR);
    TEST_ASSERT_EQ(type_check_port_compatibility(ts, point, NULL), TYPE_CHECK_ERROR);

    /* 等价 / 不等价 */
    TEST_ASSERT_EQ(type_check_port_compatibility(ts, point, point), TYPE_CHECK_OK);
    TEST_ASSERT_EQ(type_check_port_compatibility(ts, point, seg), TYPE_CHECK_MISMATCH);

    /* type_infer_port：NULL 契约 / 无端口节点 / 无连接端口 → 类型变量 */
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT(!type_infer_port(NULL, g, 1, NULL), "NULL ts");
    TEST_ASSERT(!type_infer_port(ts, NULL, 1, NULL), "NULL graph");
    TEST_ASSERT(!type_infer_port(ts, g, 1, NULL), "NULL out_type");
    TEST_ASSERT(!type_infer_port(ts, g, 999, NULL), "节点不存在");

    TypeRegion *out = NULL;
    TEST_ASSERT(!type_infer_port(ts, g, 1, &out), "无端口节点推断失败");
    TEST_ASSERT_NULL(out);

    AddNodeResult pr = graph_add_port(g, PORT_INPUT, 0, 0);
    TEST_ASSERT(pr >= 0, "添加端口节点成功");
    out = NULL;
    TEST_ASSERT(type_infer_port(ts, g, pr, &out), "无连接端口推断成功");
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_EQ(out->kind, TYPE_KIND_VARIABLE);

    graph_destroy(g);

    /* type_substitute_variable：NULL 契约 */
    TypeRegion *var = type_create_variable(ts, "A");
    TEST_ASSERT_NOT_NULL(var);
    TEST_ASSERT(!type_substitute_variable(NULL, var, 1, point, NULL), "NULL ts");
    TEST_ASSERT(!type_substitute_variable(ts, NULL, 1, point, NULL), "NULL type");
    TEST_ASSERT(!type_substitute_variable(ts, var, 1, NULL, NULL), "NULL replacement");
    TEST_ASSERT(!type_substitute_variable(ts, var, 1, point, NULL), "NULL out_result");

    /* 变量匹配 → 直接引用 replacement */
    TypeRegion *subst = NULL;
    TEST_ASSERT(type_substitute_variable(ts, var, var->variable_id, point, &subst), "变量替换成功");
    TEST_ASSERT(subst == point, "匹配变量直接返回 replacement");

    /* 变量不匹配 → 原样返回 */
    subst = NULL;
    TEST_ASSERT(type_substitute_variable(ts, var, var->variable_id + 1, point, &subst), "不匹配替换成功");
    TEST_ASSERT(subst == var, "不匹配原样返回");

    /* 函数类型替换：递归替换 input（变量） */
    TypeRegion *fn = type_create_function(ts, var, point);
    subst = NULL;
    TEST_ASSERT(type_substitute_variable(ts, fn, var->variable_id, point, &subst), "函数替换成功");
    TEST_ASSERT_NOT_NULL(subst);
    TEST_ASSERT_EQ(subst->kind, TYPE_KIND_FUNCTION);
    TEST_ASSERT(subst->input_type == point, "输入类型已替换");

    /* type_normalize：NULL 契约 / 简单类型恒等 / 别名展开 */
    TypeRegion *norm = NULL;
    TEST_ASSERT(!type_normalize(NULL, point, &norm), "NULL ts");
    TEST_ASSERT(!type_normalize(ts, NULL, &norm), "NULL type");
    TEST_ASSERT(!type_normalize(ts, point, NULL), "NULL out");
    TEST_ASSERT(type_normalize(ts, point, &norm), "简单类型规范化");
    TEST_ASSERT_NOT_NULL(norm);
    TEST_ASSERT_EQ(norm->kind, TYPE_KIND_POINT);

    /* 别名展开：alias_name + aliased_type 均设置时，规范化返回被别名类型 */
    TypeRegion *alias = type_create_region(ts, NULL, 0);
    TEST_ASSERT(type_add_alias(alias, "SegAlias"), "别名设置");
    alias->aliased_type = seg; /* TypeRegion 公开结构体，直接挂接被别名类型 */
    norm = NULL;
    TEST_ASSERT(type_normalize(ts, alias, &norm), "别名展开");
    TEST_ASSERT_NOT_NULL(norm);
    TEST_ASSERT_EQ(norm->kind, TYPE_KIND_LINE_SEGMENT);

    type_system_destroy(ts);
    printf("  test_port_infer_api: PASSED\n");
}

/* ============== 测试：推断规则表 ============== */

static void test_inference_rules_api(void) {
    /* register：NULL ts → -1 */
    TEST_ASSERT_EQ(type_system_register_inference_rule(NULL, GEOM_POINT, TYPE_KIND_POINT, 0, "d"), -1);

    TypeSystem *ts = type_system_create();
    TEST_ASSERT_NOT_NULL(ts);

    /* 默认规则集：create 注册 5 条基本几何规则 */
    int rule_count = -1;
    const TypeInferenceRule *rules = type_system_get_inference_rules(ts, &rule_count);
    TEST_ASSERT_NOT_NULL(rules);
    TEST_ASSERT(rule_count >= 5, "默认规则 >= 5");

    /* register：正常 → 0，规则数 +1 */
    TEST_ASSERT_EQ(type_system_register_inference_rule(ts, GEOM_POINT, TYPE_KIND_POINT, 5, "priority5"), 0);
    rule_count = -1;
    rules = type_system_get_inference_rules(ts, &rule_count);
    TEST_ASSERT_NOT_NULL(rules);
    TEST_ASSERT(rule_count >= 6, "注册后规则数 +1");

    /* clear：NULL 安全 / 清空后 count == 0（数组保留，指针不保证 NULL） */
    type_system_clear_inference_rules(NULL); /* 不崩溃即通过 */
    type_system_clear_inference_rules(ts);
    rule_count = -1;
    rules = type_system_get_inference_rules(ts, &rule_count);
    TEST_ASSERT_EQ(rule_count, 0);

    /* get_inference_rules：NULL ts → NULL 且 count 置 0 */
    rule_count = 123;
    TEST_ASSERT_NULL(type_system_get_inference_rules(NULL, &rule_count));
    TEST_ASSERT_EQ(rule_count, 0);

    /* type_infer_by_rules：NULL 契约 + 无规则匹配 */
    TEST_ASSERT_EQ(type_infer_by_rules(NULL, NULL, 1), TYPE_EQUIV_NOT_EQUIV);
    TEST_ASSERT_EQ(type_infer_by_rules(ts, NULL, 1), TYPE_EQUIV_NOT_EQUIV);
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_EQ(type_infer_by_rules(ts, g, 1), TYPE_EQUIV_NOT_EQUIV); /* 节点不存在 */

    /* 重新注册规则并正路径推断：点节点 → POINT 类型并附加。
     * 注意：type_attach_to_node 要求 node_id > 0（实现约束），
     * 第一个 add_point 节点 id 为 0，故用第二个节点做推断。 */
    TEST_ASSERT_EQ(type_system_register_inference_rule(ts, GEOM_POINT, TYPE_KIND_POINT, 0, "Point -> Point"), 0);
    int pid0 = add_point(g, 0, 1, 0, 1);
    TEST_ASSERT(pid0 >= 0, "添加点节点");
    int pid1 = add_point(g, 1, 1, 0, 1);
    TEST_ASSERT(pid1 >= 0, "添加第二个点节点");
    TEST_ASSERT_EQ(type_infer_by_rules(ts, g, pid1), TYPE_EQUIV_OK);
    TypeRegion *attached = type_get_node_type(ts, pid1);
    TEST_ASSERT_NOT_NULL(attached);
    TEST_ASSERT_EQ(attached->kind, TYPE_KIND_POINT);

    graph_destroy(g);
    type_system_destroy(ts);
    printf("  test_inference_rules_api: PASSED\n");
}

/* ============== 测试：重写路径族 ============== */

static void test_rewrite_path_api(void) {
    /* 流式上下文：NULL 设置安全（禁用流式输出） */
    type_system_set_stream_context(NULL);

    /* create / destroy(NULL) */
    TypeRewritePath *path = type_rewrite_path_create();
    TEST_ASSERT_NOT_NULL(path);
    type_rewrite_path_destroy(NULL); /* 不崩溃即通过 */

    /* 空路径：任何回放目标都失败 */
    TEST_ASSERT(!type_rewrite_path_replay(path, 0), "空路径回放失败");
    TEST_ASSERT(!type_rewrite_path_replay(path, -1), "负目标失败");

    /* record：NULL path 安全 */
    type_rewrite_path_record(NULL, "r", NULL, NULL); /* 不崩溃即通过 */

    /* 连续链：3 步 A→B→C→D，回放第 2 步成功 */
    TypeSystem *ts = type_system_create();
    TypeRegion *a = type_create_point(ts);
    TypeRegion *b = type_create_line_segment(ts);
    TypeRegion *c = type_create_region(ts, NULL, 0);
    TypeRegion *d = type_create_bottom(ts);
    type_rewrite_path_record(path, "r1", a, b);
    type_rewrite_path_record(path, "r2", b, c);
    type_rewrite_path_record(path, "r3", c, d);
    TEST_ASSERT(type_rewrite_path_replay(path, 2), "连续链回放成功");
    TEST_ASSERT(type_rewrite_path_replay(path, 1), "部分回放成功");
    TEST_ASSERT(type_rewrite_path_replay(path, 0), "单步回放成功");
    TEST_ASSERT(!type_rewrite_path_replay(path, 3), "越界目标失败");

    /* 断链：步骤间 before/after 不连续 → 回放失败 */
    TypeRewritePath *bad = type_rewrite_path_create();
    TEST_ASSERT_NOT_NULL(bad);
    type_rewrite_path_record(bad, "x1", a, b);
    type_rewrite_path_record(bad, "x2", d, c); /* d(bottom) != b(segment) */
    TEST_ASSERT(!type_rewrite_path_replay(bad, 1), "断链回放失败");
    type_rewrite_path_destroy(bad);

    /* type_system_get_rewrite_path：NULL ts → NULL；create 后非 NULL */
    TEST_ASSERT_NULL(type_system_get_rewrite_path(NULL));
    TEST_ASSERT_NOT_NULL(type_system_get_rewrite_path(ts));

    type_rewrite_path_destroy(path);
    type_system_destroy(ts);
    printf("  test_rewrite_path_api: PASSED\n");
}

/* ============== 测试：路径探索器（PathExplorer 全家族） ============== */

static void test_path_explorer_api(void) {
    TypeSystem *ts = type_system_create();
    TEST_ASSERT_NOT_NULL(ts);
    TypeRegion *point = type_create_point(ts);
    TypeRegion *seg = type_create_line_segment(ts);

    /* create：NULL 契约 */
    TEST_ASSERT_NULL(path_explorer_create(NULL, point, seg));
    TEST_ASSERT_NULL(path_explorer_create(ts, NULL, seg));
    TEST_ASSERT_NULL(path_explorer_create(ts, point, NULL));

    /* create 正常 */
    PathExplorer *ex = path_explorer_create(ts, point, seg);
    TEST_ASSERT_NOT_NULL(ex);

    /* get_current / get_step_count / get_steps 初值 */
    TEST_ASSERT_NULL(path_explorer_get_current(NULL));
    TEST_ASSERT_NOT_NULL(path_explorer_get_current(ex));
    TEST_ASSERT_EQ(path_explorer_get_current(ex)->kind, TYPE_KIND_POINT);
    TEST_ASSERT_EQ(path_explorer_get_step_count(NULL), 0);
    TEST_ASSERT_EQ(path_explorer_get_step_count(ex), 0);
    TEST_ASSERT_NULL(path_explorer_get_steps(NULL));
    TEST_ASSERT_NULL(path_explorer_get_steps(ex)); /* 无步骤 → NULL */

    /* get_applicable_rules：NULL 契约；无重写规则 → NO_RULES */
    int *indices = NULL;
    int count = -1;
    TEST_ASSERT_EQ(path_explorer_get_applicable_rules(NULL, &indices, &count), EXPLORER_ERROR);
    TEST_ASSERT_EQ(path_explorer_get_applicable_rules(ex, NULL, &count), EXPLORER_ERROR);
    TEST_ASSERT_EQ(path_explorer_get_applicable_rules(ex, &indices, NULL), EXPLORER_ERROR);
    TEST_ASSERT_EQ(path_explorer_get_applicable_rules(ex, &indices, &count), EXPLORER_NO_RULES);

    /* preview_rule：NULL / 越界 / 无规则 → INVALID_RULE */
    TypeRegion *preview = NULL;
    TEST_ASSERT_EQ(path_explorer_preview_rule(NULL, 0, &preview), EXPLORER_ERROR);
    TEST_ASSERT_EQ(path_explorer_preview_rule(ex, 0, NULL), EXPLORER_ERROR);
    TEST_ASSERT_EQ(path_explorer_preview_rule(ex, -1, &preview), EXPLORER_INVALID_RULE);
    TEST_ASSERT_EQ(path_explorer_preview_rule(ex, 0, &preview), EXPLORER_INVALID_RULE);

    /* apply_rule：NULL / 越界 / 无规则 → INVALID_RULE */
    TEST_ASSERT_EQ(path_explorer_apply_rule(NULL, 0), EXPLORER_ERROR);
    TEST_ASSERT_EQ(path_explorer_apply_rule(ex, 0), EXPLORER_INVALID_RULE);
    TEST_ASSERT_EQ(path_explorer_apply_rule(ex, -1), EXPLORER_INVALID_RULE);

    /* undo：NULL / 空历史 → UNDO_EMPTY */
    TEST_ASSERT_EQ(path_explorer_undo(NULL), EXPLORER_ERROR);
    TEST_ASSERT_EQ(path_explorer_undo(ex), EXPLORER_UNDO_EMPTY);

    /* check_goal：NULL 契约；不同类型 → reached=false */
    bool reached = true;
    TEST_ASSERT_EQ(path_explorer_check_goal(NULL, &reached), EXPLORER_ERROR);
    TEST_ASSERT_EQ(path_explorer_check_goal(ex, NULL), EXPLORER_ERROR);
    TEST_ASSERT_EQ(path_explorer_check_goal(ex, &reached), EXPLORER_OK);
    TEST_ASSERT(!reached, "点 vs 线段未达目标");

    /* save_path：NULL 契约；空探索 → OK + 空路径 */
    TypeRewritePath *saved = NULL;
    TEST_ASSERT_EQ(path_explorer_save_path(NULL, &saved), EXPLORER_ERROR);
    TEST_ASSERT_EQ(path_explorer_save_path(ex, NULL), EXPLORER_ERROR);
    TEST_ASSERT_EQ(path_explorer_save_path(ex, &saved), EXPLORER_OK);
    TEST_ASSERT_NOT_NULL(saved);
    TEST_ASSERT(!type_rewrite_path_replay(saved, 0), "空路径不可回放");
    type_rewrite_path_destroy(saved);

    path_explorer_destroy(ex);

    /* 同类型 current == target：check_goal → reached=true；applicable → GOAL_REACHED */
    PathExplorer *ex2 = path_explorer_create(ts, point, point);
    TEST_ASSERT_NOT_NULL(ex2);
    reached = false;
    TEST_ASSERT_EQ(path_explorer_check_goal(ex2, &reached), EXPLORER_OK);
    TEST_ASSERT(reached, "同类型已达目标");
    count = -1;
    indices = NULL;
    TEST_ASSERT_EQ(path_explorer_get_applicable_rules(ex2, &indices, &count), EXPLORER_GOAL_REACHED);
    TEST_ASSERT_NULL(indices);
    path_explorer_destroy(ex2);

    /* destroy(NULL) 安全 */
    path_explorer_destroy(NULL);

    type_system_destroy(ts);
    printf("  test_path_explorer_api: PASSED\n");
}

/* ============== 测试：类型打印 ============== */

static void test_type_print_api(void) {
    type_print(NULL, 0); /* 不崩溃即通过 */

    TypeSystem *ts = type_system_create();
    TEST_ASSERT_NOT_NULL(ts);
    TypeRegion *point = type_create_point(ts);
    TypeRegion *seg = type_create_line_segment(ts);
    TypeRegion *fn = type_create_function(ts, point, seg);

    /* 简单类型 / 复合类型打印不崩溃（输出到 stdout） */
    type_print(point, 0);
    type_print(fn, 2);

    type_system_destroy(ts);
    printf("  test_type_print_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Type System Ext Test Suite")
    printf("=== Lv-00 Type System Ext Test Suite (batch C-㊳) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_region_deep_copy_api);
    TEST_MAIN_RUN(test_predicate_subtype_api);
    TEST_MAIN_RUN(test_port_infer_api);
    TEST_MAIN_RUN(test_inference_rules_api);
    TEST_MAIN_RUN(test_rewrite_path_api);
    TEST_MAIN_RUN(test_path_explorer_api);
    TEST_MAIN_RUN(test_type_print_api);

    lv_cleanup();
TEST_MAIN_END()
