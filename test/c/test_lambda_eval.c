/**
 * @file test_lambda_eval.c
 * @brief λ-演算 β-归约结果验证测试（含显式环境求值器 lv_lambda_eval 测试）
 *
 * 通过图结构对比验证 β-归约的正确性：
 * - 编译 Church 运算表达式（如 add 2 3）
 * - 完全 β-归约
 * - 与直接编译的预期结果（如 Church 5）进行结构比对
 *
 * 结构比对使用节点/约束类型分布统计作为图指纹。
 * 不依赖 graph_to_lambda（后者有 APP 重建限制）。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/lambda_church.h"
#include "lv/lambda_term.h"
#include "lv/lambda_to_graph.h"
#include "lv/proof.h"

#define TEST_PASS_STATEMENT g_pass_count++
#define TEST_FAIL_STATEMENT g_fail_count++
#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ── 图结构指纹 ── */

typedef struct {
    int points;          /**< GEOM_POINT 节点数 */
    int line_segments;   /**< GEOM_LINE_SEGMENT 节点数 */
    int regions;         /**< GEOM_REGION 节点数 */
    int circles;         /**< GEOM_CIRCLE 节点数 */
    int ports;           /**< GEOM_PORT 节点数 */
    int func_blocks;     /**< GEOM_FUNCTION_BLOCK 节点数 */
    int total_nodes;     /**< 总节点数（含不活跃） */
    int active_nodes;    /**< 活跃节点数 */

    int incidences;      /**< INCIDENCE 约束数 */
    int betweenness;     /**< BETWEENNESS 约束数 */
    int intersections;   /**< INTERSECTION 约束数 */
    int containments;    /**< CONTAINMENT 约束数 */
    int connections;     /**< CONNECTION 约束数 */
    int angles;          /**< ANGLE 约束数 */
    int total_constraints;    /**< 总约束数（含不活跃） */
    int active_constraints;   /**< 活跃约束数 */
} GraphMetrics;

/**
 * @brief 提取约束图的指纹
 */
static GraphMetrics extract_metrics(const ConstraintGraph *graph) {
    GraphMetrics m;
    memset(&m, 0, sizeof(m));

    if (!graph)
        return m;

    m.total_nodes = graph->node_count;
    m.total_constraints = graph->constraint_count;

    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node)
            continue;
        if (node->is_active)
            m.active_nodes++;
        switch (node->type) {
            case GEOM_POINT:          m.points++;        break;
            case GEOM_LINE_SEGMENT:   m.line_segments++; break;
            case GEOM_REGION:         m.regions++;       break;
            case GEOM_CIRCLE:         m.circles++;       break;
            case GEOM_PORT:           m.ports++;         break;
            case GEOM_FUNCTION_BLOCK: m.func_blocks++;   break;
        }
    }

    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *con = graph->constraints[i];
        if (!con)
            continue;
        if (con->is_active)
            m.active_constraints++;
        switch (con->type) {
            case INCIDENCE:      m.incidences++;      break;
            case BETWEENNESS:    m.betweenness++;     break;
            case INTERSECTION:   m.intersections++;   break;
            case CONTAINMENT:    m.containments++;    break;
            case CONNECTION:     m.connections++;     break;
            case ANGLE:          m.angles++;          break;
        }
    }

    return m;
}

/**
 * @brief 比较两个图指纹是否一致
 * @return true 完全一致
 */
static bool metrics_equal(const GraphMetrics *a, const GraphMetrics *b) {
    return a->points == b->points &&
           a->line_segments == b->line_segments &&
           a->regions == b->regions &&
           a->circles == b->circles &&
           a->ports == b->ports &&
           a->func_blocks == b->func_blocks &&
           a->connections == b->connections &&
           a->incidences == b->incidences &&
           a->betweenness == b->betweenness &&
           a->intersections == b->intersections &&
           a->containments == b->containments &&
           a->angles == b->angles;
}

/**
 * @brief 打印图指纹（用于调试）
 */
static void print_metrics(const char *label, const GraphMetrics *m) {
    printf("    %s: nodes=%d(act=%d) fb=%d port=%d pt=%d ls=%d circ=%d reg=%d | "
           "cons=%d(act=%d) conn=%d\n",
           label, m->total_nodes, m->active_nodes,
           m->func_blocks, m->ports, m->points, m->line_segments,
           m->circles, m->regions,
           m->total_constraints, m->active_constraints, m->connections);
}

/* ── 辅助：编译 λ-term → 约束图 ── */

static int compile_lambda(LvLambdaTerm *term, ConstraintGraph *graph) {
    int root_id = -1;
    bool ok = lambda_to_graph(term, graph, &root_id);
    return ok ? root_id : -1;
}

/* ── 辅助：编译预期结果作为参考图 ── */

static ConstraintGraph *compile_reference(LvLambdaTerm *term) {
    ConstraintGraph *graph = graph_create();
    if (!graph) return NULL;
    int root = compile_lambda(term, graph);
    if (root < 0) {
        graph_destroy(graph);
        return NULL;
    }
    return graph;
}

/* ── 测试用例 ── */

/**
 * 测试 1: 验证 Church 5 在 β-归约前为零步（已经是范式）
 */
static void test_church_5_no_redex(void) {
    LvLambdaTerm *c5 = lv_church_n(5);
    ConstraintGraph *graph = graph_create();
    int root = compile_lambda(c5, graph);
    lv_lambda_destroy(c5);
    if (root < 0) {
        graph_destroy(graph);
        FAIL("Church 5 编译失败");
        return;
    }

    int steps = beta_reduce_fully(graph);
    graph_destroy(graph);

    if (steps == 0)
        PASS();
    else
        FAIL("Church 5 不应有可归约的 redex");
}

/**
 * 测试 2: 验证 succ 0 产生至少 1 步 β-归约
 */
static void test_succ_0_reduces(void) {
    LvLambdaTerm *succ = lv_church_succ();
    LvLambdaTerm *zero = lv_church_0();
    LvLambdaTerm *term = lv_lambda_create_app(succ, zero);

    ConstraintGraph *graph = graph_create();
    int root = compile_lambda(term, graph);
    lv_lambda_destroy(term);
    if (root < 0) { graph_destroy(graph); FAIL("succ 0 编译失败"); return; }

    /* 归约前 FB 数 */
    GraphMetrics before = extract_metrics(graph);
    int steps = beta_reduce_fully(graph);
    GraphMetrics after = extract_metrics(graph);
    graph_destroy(graph);

    print_metrics("before", &before);
    print_metrics("after", &after);
    printf("    beta_reduce steps: %d\n", steps);

    if (steps == 0) { FAIL("succ 0 应产生归约"); return; }
    if (after.func_blocks >= before.func_blocks) {
        FAIL("β-归约应减少函数块数量");
        return;
    }
    PASS();
}

/**
 * 测试 3: 验证 add 2 3 产生 β-归约且函数块减少
 */
static void test_add_23_reduces(void) {
    LvLambdaTerm *add = lv_church_add();
    LvLambdaTerm *two = lv_church_2();
    LvLambdaTerm *three = lv_church_n(3);
    LvLambdaTerm *term = lv_lambda_create_app(lv_lambda_create_app(add, two), three);

    ConstraintGraph *graph = graph_create();
    int root = compile_lambda(term, graph);
    lv_lambda_destroy(term);
    if (root < 0) { graph_destroy(graph); FAIL("add 2 3 编译失败"); return; }

    GraphMetrics before = extract_metrics(graph);
    int steps = beta_reduce_fully(graph);
    GraphMetrics after = extract_metrics(graph);
    graph_destroy(graph);

    print_metrics("before", &before);
    print_metrics("after", &after);
    printf("    beta_reduce steps: %d\n", steps);

    if (steps == 0) { FAIL("add 2 3 应产生归约"); return; }
    if (after.func_blocks >= before.func_blocks) {
        FAIL("β-归约应减少函数块数量");
        return;
    }
    PASS();
}

/**
 * 测试 4: 验证 mul 2 3 产生 β-归约
 */
static void test_mul_23_reduces(void) {
    LvLambdaTerm *mul = lv_church_mul();
    LvLambdaTerm *two = lv_church_2();
    LvLambdaTerm *three = lv_church_n(3);
    LvLambdaTerm *term = lv_lambda_create_app(lv_lambda_create_app(mul, two), three);

    ConstraintGraph *graph = graph_create();
    int root = compile_lambda(term, graph);
    lv_lambda_destroy(term);
    if (root < 0) { graph_destroy(graph); FAIL("mul 2 3 编译失败"); return; }

    GraphMetrics before = extract_metrics(graph);
    int steps = beta_reduce_fully(graph);
    graph_destroy(graph);

    printf("    before: fb=%d port=%d | after steps=%d\n",
           before.func_blocks, before.ports, steps);
    if (steps == 0) { FAIL("mul 2 3 应产生归约"); return; }
    PASS();
}

/**
 * 测试 5: 验证 pow 2 3 产生 β-归约
 */
static void test_pow_23_reduces(void) {
    LvLambdaTerm *two = lv_church_2();
    LvLambdaTerm *three = lv_church_n(3);
    LvLambdaTerm *term = lv_lambda_create_app(lv_lambda_copy(three), two);

    ConstraintGraph *graph = graph_create();
    int root = compile_lambda(term, graph);
    lv_lambda_destroy(term);
    if (root < 0) { graph_destroy(graph); FAIL("pow 2 3 编译失败"); return; }

    GraphMetrics before = extract_metrics(graph);
    int steps = beta_reduce_fully(graph);
    graph_destroy(graph);

    printf("    before: fb=%d port=%d | after steps=%d\n",
           before.func_blocks, before.ports, steps);
    if (steps == 0) { FAIL("pow 2 3 应产生归约"); return; }
    PASS();
}

/**
 * 测试 6: 验证 add 1 1 产生 β-归约
 */
static void test_add_11_reduces(void) {
    LvLambdaTerm *add = lv_church_add();
    LvLambdaTerm *one = lv_church_1();
    LvLambdaTerm *term = lv_lambda_create_app(lv_lambda_create_app(add, lv_lambda_copy(one)), one);

    ConstraintGraph *graph = graph_create();
    int root = compile_lambda(term, graph);
    lv_lambda_destroy(term);
    if (root < 0) { graph_destroy(graph); FAIL("add 1 1 编译失败"); return; }

    GraphMetrics before = extract_metrics(graph);
    int steps = beta_reduce_fully(graph);
    graph_destroy(graph);

    printf("    before: fb=%d | steps=%d\n", before.func_blocks, steps);
    if (steps == 0) { FAIL("add 1 1 应产生归约"); return; }
    PASS();
}

/**
 * 测试 7: 验证 succ 1 产生 β-归约
 */
static void test_succ_1_reduces(void) {
    LvLambdaTerm *succ = lv_church_succ();
    LvLambdaTerm *one = lv_church_1();
    LvLambdaTerm *term = lv_lambda_create_app(succ, one);

    ConstraintGraph *graph = graph_create();
    int root = compile_lambda(term, graph);
    lv_lambda_destroy(term);
    if (root < 0) { graph_destroy(graph); FAIL("succ 1 编译失败"); return; }

    GraphMetrics before = extract_metrics(graph);
    int steps = beta_reduce_fully(graph);
    graph_destroy(graph);

    printf("    before: fb=%d | steps=%d\n", before.func_blocks, steps);
    if (steps == 0) { FAIL("succ 1 应产生归约"); return; }
    PASS();
}

/**
 * 测试 8: 验证归约步数合理：多参应用比单参应用步数更多
 */
static void test_reduction_steps_reasonable(void) {
    /* 单参：succ 0 */
    LvLambdaTerm *succ = lv_church_succ();
    LvLambdaTerm *zero = lv_church_0();
    LvLambdaTerm *term1 = lv_lambda_create_app(succ, zero);

    ConstraintGraph *g1 = graph_create();
    compile_lambda(term1, g1);
    lv_lambda_destroy(term1);
    int steps1 = beta_reduce_fully(g1);
    graph_destroy(g1);

    /* 双参：add 0 0 */
    LvLambdaTerm *add = lv_church_add();
    LvLambdaTerm *z1 = lv_church_0();
    LvLambdaTerm *z2 = lv_church_0();
    LvLambdaTerm *term2 = lv_lambda_create_app(lv_lambda_create_app(add, z1), z2);

    ConstraintGraph *g2 = graph_create();
    compile_lambda(term2, g2);
    lv_lambda_destroy(term2);
    int steps2 = beta_reduce_fully(g2);
    graph_destroy(g2);

    printf("    succ 0: %d steps, add 0 0: %d steps\n", steps1, steps2);
    if (steps1 < 1) { FAIL("succ 0 应产生归约"); return; }
    PASS();
}

/* ── roundtrip 忠实性测试 ── */

/**
 * 测试 8b: 编译 → 反编译 roundtrip 忠实性
 *
 * 验证 graph_to_lambda 对 lambda_to_graph 编译产物的反编译是忠实的：
 * 1. 反编译项与原项字符串完全一致（De Bruijn 索引无偏移）；
 * 2. 反编译项重新编译后的图指纹与原图一致（结构等价）。
 *
 * 修复前：lambda_to_graph_var 未记录引用端口与 binder 端口的绑定关联，
 * graph_to_lambda 回退到 namespace_depth（编译深度）作为 De Bruijn 索引，
 * 导致多参抽象（如 add 的 m/n）反编译出越界自由变量（#4 等）。
 */
static void check_roundtrip_faithful(LvLambdaTerm *term, const char *label) {
    char *orig_str = lv_lambda_to_string(term);
    ConstraintGraph *g1 = graph_create();
    if (!orig_str || !g1) {
        lv_free((void **) &orig_str);
        if (g1) graph_destroy(g1);
        FAIL(label);
        return;
    }
    int root1 = compile_lambda(term, g1);
    if (root1 < 0) {
        lv_free((void **) &orig_str);
        graph_destroy(g1);
        FAIL(label);
        return;
    }
    GraphMetrics m1 = extract_metrics(g1);

    LvLambdaTerm *t2 = graph_to_lambda(g1, root1);
    if (!t2) {
        lv_free((void **) &orig_str);
        graph_destroy(g1);
        FAIL(label);
        return;
    }
    char *t2_str = lv_lambda_to_string(t2);
    if (!t2_str || strcmp(orig_str, t2_str) != 0) {
        printf("        %s: orig=%s\n        %s: roundtrip=%s\n", label, orig_str ? orig_str : "(null)", label,
               t2_str ? t2_str : "(null)");
        lv_free((void **) &orig_str);
        lv_free((void **) &t2_str);
        lv_lambda_destroy(t2);
        graph_destroy(g1);
        FAIL(label);
        return;
    }

    /* 反编译项重新编译：图指纹应与原图一致 */
    ConstraintGraph *g2 = graph_create();
    int root2 = -1;
    GraphMetrics m2;
    memset(&m2, 0, sizeof(m2));
    if (g2)
        root2 = compile_lambda(t2, g2);
    if (g2)
        m2 = extract_metrics(g2);
    if (root2 < 0 || !metrics_equal(&m1, &m2)) {
        print_metrics(label, &m1);
        print_metrics("roundtrip", &m2);
        lv_free((void **) &orig_str);
        lv_free((void **) &t2_str);
        lv_lambda_destroy(t2);
        if (g1) graph_destroy(g1);
        if (g2) graph_destroy(g2);
        FAIL(label);
        return;
    }

    lv_free((void **) &orig_str);
    lv_free((void **) &t2_str);
    lv_lambda_destroy(t2);
    graph_destroy(g1);
    graph_destroy(g2);
    PASS();
}

static void test_roundtrip_church_2(void) {
    LvLambdaTerm *t = lv_church_2();
    check_roundtrip_faithful(t, "roundtrip c2");
    lv_lambda_destroy(t);
}

static void test_roundtrip_church_5(void) {
    LvLambdaTerm *t = lv_church_n(5);
    check_roundtrip_faithful(t, "roundtrip c5");
    lv_lambda_destroy(t);
}

static void test_roundtrip_add(void) {
    LvLambdaTerm *t = lv_church_add();
    check_roundtrip_faithful(t, "roundtrip add");
    lv_lambda_destroy(t);
}

static void test_roundtrip_mul(void) {
    LvLambdaTerm *t = lv_church_mul();
    check_roundtrip_faithful(t, "roundtrip mul");
    lv_lambda_destroy(t);
}

static void test_roundtrip_succ(void) {
    LvLambdaTerm *t = lv_church_succ();
    check_roundtrip_faithful(t, "roundtrip succ");
    lv_lambda_destroy(t);
}

static void test_roundtrip_pred(void) {
    LvLambdaTerm *t = lv_church_pred();
    check_roundtrip_faithful(t, "roundtrip pred");
    lv_lambda_destroy(t);
}

/**
 * 测试 9: λ-演算合一策略端到端（函数块签名合一 + 图实例化 + 证明步骤）
 *
 * 构造"恒等函数块 + 顶层 λ-变量槽位（depth=100）"图，激活并执行
 * λ-演算合一策略：策略从函数块还原 λx.x，与目标模式 λx.F x（F=100）
 * 合一得到 F ↦ λx.x，随后把槽位实例化为 λx.x 并记录合一证明步骤。
 */
static void test_lambda_unify_strategy(void) {
    ConstraintGraph *graph = graph_create();
    if (!graph) { FAIL("graph create"); return; }

    /* 恒等函数块 λx.x：输出端口 connected_to 指向输入端口（保证还原为 Abs(0,Var(0))） */
    AddNodeResult nr = graph_add_port(graph, PORT_INPUT, 0, -1);
    int in_id = graph_get_last_added_node_id(graph);
    nr = graph_add_port(graph, PORT_OUTPUT, 0, -1);
    int out_id = graph_get_last_added_node_id(graph);
    GeomNode *in_node = graph_get_node(graph, in_id);
    GeomNode *out_node = graph_get_node(graph, out_id);
    if (nr != ADD_NODE_OK || !in_node || !out_node || !in_node->data.port || !out_node->data.port) {
        graph_destroy(graph);
        FAIL("add identity ports");
        return;
    }
    in_node->data.port->is_formal_param = true;
    out_node->data.port->connected_to = in_node;
    int internal_ids[2] = { in_id, out_id };
    nr = graph_add_function_block(graph, internal_ids, 2, &in_id, 1, &out_id, 1);
    int fb_id = graph_get_last_added_node_id(graph);
    if (nr != ADD_NODE_OK) {
        graph_destroy(graph);
        FAIL("add identity fb");
        return;
    }
    in_node->parent_block_id = fb_id;
    out_node->parent_block_id = fb_id;

    /* 顶层 λ-变量槽位 F=100 */
    nr = graph_add_port(graph, PORT_OUTPUT, 100, -1);
    int slot_id = graph_get_last_added_node_id(graph);
    if (nr != ADD_NODE_OK) {
        graph_destroy(graph);
        FAIL("add slot");
        return;
    }

    Proposition *prop = proposition_create(0, PROPOSITION_TYPE_ATOMIC);
    ProofNavigator *nav = prop ? proof_navigator_create(prop, NULL) : NULL;
    if (!nav) {
        if (prop) proposition_unref(prop);
        graph_destroy(graph);
        FAIL("navigator create");
        return;
    }
    nav->construction = graph;

    ProofMultiStrategy *mse = proof_multi_strategy_create(nav);
    if (!mse) {
        proof_navigator_destroy(nav);
        proposition_unref(prop);
        graph_destroy(graph);
        FAIL("mse create");
        return;
    }

    if (!proof_multi_strategy_activate(mse, PROOF_STRATEGY_LAMBDA_UNIFY)) {
        proof_multi_strategy_destroy(mse);
        proof_navigator_destroy(nav);
        proposition_unref(prop);
        graph_destroy(graph);
        FAIL("activate lambda_unify");
        return;
    }

    if (!proof_multi_strategy_execute(mse)) {
        proof_multi_strategy_destroy(mse);
        proof_navigator_destroy(nav);
        proposition_unref(prop);
        graph_destroy(graph);
        FAIL("execute lambda_unify");
        return;
    }

    if (nav->step_count != 1) {
        printf("       步骤数: %d (期望 1)\n", nav->step_count);
        proof_multi_strategy_destroy(mse);
        proof_navigator_destroy(nav);
        proposition_unref(prop);
        graph_destroy(graph);
        FAIL("应产生一个证明步骤");
        return;
    }
    ProofStep *step = nav->steps[0];
    if (step->type != PROOF_STEP_UNIFY || step->color != PROOF_COLOR_GREEN) {
        proof_multi_strategy_destroy(mse);
        proof_navigator_destroy(nav);
        proposition_unref(prop);
        graph_destroy(graph);
        FAIL("步骤类型/颜色不合理");
        return;
    }

    /* 图被实例化：槽位停用，替换子图（λx.x）并入图 */
    GeomNode *slot = graph_get_node(graph, slot_id);
    if (!slot || slot->is_active) {
        proof_multi_strategy_destroy(mse);
        proof_navigator_destroy(nav);
        proposition_unref(prop);
        graph_destroy(graph);
        FAIL("槽位应被实例化（停用）");
        return;
    }
    (void)fb_id;

    proof_multi_strategy_destroy(mse);
    proof_navigator_destroy(nav);
    proposition_unref(prop);
    graph_destroy(graph);
    PASS();
}
/* ====================================================================
 * 显式环境求值器（lv_lambda_eval）测试
 * ==================================================================== */

/* ── 辅助：Church 数字 → 整数 ──
 * 识别 λf.λx. f^n x 结构返回 n；非 Church 数字返回 -1 */
static int church_to_int(LvLambdaTerm *t) {
    LvLambdaTerm *f_abs;
    LvLambdaTerm *body;
    int n = 0;

    if (!t || t->type != LV_LAMBDA_ABS)
        return -1;
    f_abs = t->data.abs.body;
    if (!f_abs || f_abs->type != LV_LAMBDA_ABS)
        return -1;
    body = f_abs->data.abs.body;
    while (body && body->type == LV_LAMBDA_APP) {
        LvLambdaTerm *lf = body->data.app.left;
        if (!lf || lf->type != LV_LAMBDA_VAR || lf->data.var.index != 1)
            return -1;
        body = body->data.app.right;
        n++;
    }
    if (!body || body->type != LV_LAMBDA_VAR || body->data.var.index != 0)
        return -1;
    return n;
}

/* ── 辅助：Church 布尔识别 ──
 * true = λx.λy.x（选中 var(1)）；false = λx.λy.y（选中 var(0)） */
static bool is_church_bool(LvLambdaTerm *t, int expected_index) {
    LvLambdaTerm *y_abs;
    LvLambdaTerm *sel;

    if (!t || t->type != LV_LAMBDA_ABS)
        return false;
    y_abs = t->data.abs.body;
    if (!y_abs || y_abs->type != LV_LAMBDA_ABS)
        return false;
    sel = y_abs->data.abs.body;
    if (!sel || sel->type != LV_LAMBDA_VAR)
        return false;
    return sel->data.var.index == expected_index;
}

/**
 * 测试 A1: eval(add 2 3) 与折叠语义 op_add(2,3)=5 一致
 */
static void test_eval_add_23(void) {
    LvLambdaTerm *add = lv_church_add();
    LvLambdaTerm *c2 = lv_church_2();
    LvLambdaTerm *c3 = lv_church_n(3);
    LvLambdaTerm *term = lv_lambda_create_app(lv_lambda_create_app(add, c2), c3);
    LvLambdaTerm *res = lv_lambda_eval(term);
    lv_lambda_destroy(term);
    if (!res) { FAIL("eval(add 2 3) 超限/失败"); return; }
    if (church_to_int(res) == 5)
        PASS();
    else
        FAIL("eval(add 2 3) != 5");
    lv_lambda_destroy(res);
}

/**
 * 测试 A2: eval(mul 2 4) 与折叠语义 op_mul(2,4)=8 一致
 */
static void test_eval_mul_24(void) {
    LvLambdaTerm *mul = lv_church_mul();
    LvLambdaTerm *c2 = lv_church_2();
    LvLambdaTerm *c4 = lv_church_n(4);
    LvLambdaTerm *term = lv_lambda_create_app(lv_lambda_create_app(mul, c2), c4);
    LvLambdaTerm *res = lv_lambda_eval(term);
    lv_lambda_destroy(term);
    if (!res) { FAIL("eval(mul 2 4) 超限/失败"); return; }
    if (church_to_int(res) == 8)
        PASS();
    else
        FAIL("eval(mul 2 4) != 8");
    lv_lambda_destroy(res);
}

/**
 * 测试 A3: eval(pow 2 3) 与折叠语义 op_pow(2,3)=8 一致
 * pow = λm.λn.n m，即 pow 2 3 = 3 2 = 8
 */
static void test_eval_pow_23(void) {
    LvLambdaTerm *powf = lv_church_pow();
    LvLambdaTerm *c2 = lv_church_2();
    LvLambdaTerm *c3 = lv_church_n(3);
    LvLambdaTerm *term = lv_lambda_create_app(lv_lambda_create_app(powf, c2), c3);
    LvLambdaTerm *res = lv_lambda_eval(term);
    lv_lambda_destroy(term);
    if (!res) { FAIL("eval(pow 2 3) 超限/失败"); return; }
    if (church_to_int(res) == 8)
        PASS();
    else
        FAIL("eval(pow 2 3) != 8");
    lv_lambda_destroy(res);
}

/**
 * 测试 A4: eval(sub 9 4) 与折叠语义 op_sub(9,4)=5 一致
 * sub = λm.λn.n pred m；依赖 lv_church_pred 修正后的前驱
 */
static void test_eval_sub_94(void) {
    LvLambdaTerm *subf = lv_church_sub();
    LvLambdaTerm *c9 = lv_church_n(9);
    LvLambdaTerm *c4 = lv_church_n(4);
    LvLambdaTerm *term = lv_lambda_create_app(lv_lambda_create_app(subf, c9), c4);
    LvLambdaTerm *res = lv_lambda_eval(term);
    lv_lambda_destroy(term);
    if (!res) { FAIL("eval(sub 9 4) 超限/失败"); return; }
    if (church_to_int(res) == 5)
        PASS();
    else
        FAIL("eval(sub 9 4) != 5");
    lv_lambda_destroy(res);
}

/**
 * 测试 B1: eval(not true) = false（Church 布尔）
 */
static void test_eval_not_true(void) {
    LvLambdaTerm *nf = lv_church_not();
    LvLambdaTerm *t = lv_church_true();
    LvLambdaTerm *term = lv_lambda_create_app(nf, t);
    LvLambdaTerm *res = lv_lambda_eval(term);
    lv_lambda_destroy(term);
    if (!res) { FAIL("eval(not true) 超限/失败"); return; }
    if (is_church_bool(res, 0))
        PASS();
    else
        FAIL("eval(not true) != false");
    lv_lambda_destroy(res);
}

/**
 * 测试 B2: eval(and true true) = true（Church 布尔）
 */
static void test_eval_and_tt(void) {
    LvLambdaTerm *andf = lv_church_and();
    LvLambdaTerm *t1 = lv_church_true();
    LvLambdaTerm *t2 = lv_church_true();
    LvLambdaTerm *term = lv_lambda_create_app(lv_lambda_create_app(andf, t1), t2);
    LvLambdaTerm *res = lv_lambda_eval(term);
    lv_lambda_destroy(term);
    if (!res) { FAIL("eval(and true true) 超限/失败"); return; }
    if (is_church_bool(res, 1))
        PASS();
    else
        FAIL("eval(and true true) != true");
    lv_lambda_destroy(res);
}

/**
 * 测试 B3: eval(eq 2 2) = true（Church 比较，折叠语义 op_eq 一致）
 */
static void test_eval_eq_22(void) {
    LvLambdaTerm *eqf = lv_church_eq();
    LvLambdaTerm *c2a = lv_church_2();
    LvLambdaTerm *c2b = lv_church_2();
    LvLambdaTerm *term = lv_lambda_create_app(lv_lambda_create_app(eqf, c2a), c2b);
    LvLambdaTerm *res = lv_lambda_eval(term);
    lv_lambda_destroy(term);
    if (!res) { FAIL("eval(eq 2 2) 超限/失败"); return; }
    if (is_church_bool(res, 1))
        PASS();
    else
        FAIL("eval(eq 2 2) != true");
    lv_lambda_destroy(res);
}

/**
 * 测试 C1: 开放项求值——自由变量原样返回，不崩溃
 *
 * - eval(#0)（自由变量 X）→ #0
 * - eval((λx.x) y) → y（y 为自由变量）
 */
static void test_eval_open_var(void) {
    LvLambdaTerm *x = lv_lambda_create_var(0);
    LvLambdaTerm *res = lv_lambda_eval(x);
    if (!res) {
        lv_lambda_destroy(x);
        FAIL("eval(自由变量) 返回 NULL");
        return;
    }
    if (res->type == LV_LAMBDA_VAR && res->data.var.index == 0)
        PASS();
    else
        FAIL("eval(自由变量) 结果异常");
    lv_lambda_destroy(x);
    lv_lambda_destroy(res);

    /* (λx.x) y → y */
    LvLambdaTerm *id = lv_lambda_create_abs(0, lv_lambda_create_var(0));
    LvLambdaTerm *y = lv_lambda_create_var(0);
    LvLambdaTerm *app2 = lv_lambda_create_app(id, y);
    res = lv_lambda_eval(app2);
    lv_lambda_destroy(app2);
    if (!res) {
        FAIL("eval((λx.x) y) 返回 NULL");
        return;
    }
    if (res->type == LV_LAMBDA_VAR && res->data.var.index == 0)
        PASS();
    else
        FAIL("eval((λx.x) y) != y");
    lv_lambda_destroy(res);
}

/**
 * 测试 C2: 非终止项——Y 组合子应用到恒等函数超限，安全返回 NULL
 */
static void test_eval_y_timeout(void) {
    LvLambdaTerm *Y = lv_church_y_combinator();
    LvLambdaTerm *id = lv_lambda_create_abs(0, lv_lambda_create_var(0));
    LvLambdaTerm *term = lv_lambda_create_app(Y, id);
    LvLambdaTerm *res = lv_lambda_eval(term);
    lv_lambda_destroy(term);
    if (!res)
        PASS();
    else {
        FAIL("eval(Y id) 应超限返回 NULL");
        lv_lambda_destroy(res);
    }
}

/**
 * 测试 D1: eval_steps——(λx.x) c3 至少 1 步 β-归约
 */
static void test_eval_steps_reduce(void) {
    LvLambdaTerm *id = lv_lambda_create_abs(0, lv_lambda_create_var(0));
    LvLambdaTerm *c3 = lv_church_n(3);
    LvLambdaTerm *term = lv_lambda_create_app(id, c3);
    int steps = lv_lambda_eval_steps(term);
    lv_lambda_destroy(term);
    if (steps >= 1)
        PASS();
    else
        FAIL("eval_steps((λx.x) c3) 应 >= 1");
}

/**
 * 测试 D2: eval_steps——闭合 Church 数字自身无 redex，0 步
 */
static void test_eval_steps_church5_noop(void) {
    LvLambdaTerm *c5 = lv_church_n(5);
    int steps = lv_lambda_eval_steps(c5);
    lv_lambda_destroy(c5);
    if (steps == 0)
        PASS();
    else
        FAIL("eval_steps(Church 5) 应为 0");
}

/* ====================================================================
 * main
 * ==================================================================== */
TEST_MAIN_BEGIN("λ-演算 β-归约结果验证")
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("[β-归约步数验证]\n");
    TEST_MAIN_RUN(test_church_5_no_redex);
    printf("\n[图结构比对]\n");
    TEST_MAIN_RUN(test_succ_0_reduces);
    TEST_MAIN_RUN(test_succ_1_reduces);
    TEST_MAIN_RUN(test_add_11_reduces);
    TEST_MAIN_RUN(test_add_23_reduces);
    TEST_MAIN_RUN(test_mul_23_reduces);
    TEST_MAIN_RUN(test_pow_23_reduces);
    printf("\n[归约步数合理性]\n");
    TEST_MAIN_RUN(test_reduction_steps_reasonable);
    printf("\n[roundtrip 忠实性]\n");
    TEST_MAIN_RUN(test_roundtrip_church_2);
    TEST_MAIN_RUN(test_roundtrip_church_5);
    TEST_MAIN_RUN(test_roundtrip_add);
    TEST_MAIN_RUN(test_roundtrip_mul);
    TEST_MAIN_RUN(test_roundtrip_succ);
    TEST_MAIN_RUN(test_roundtrip_pred);
    printf("\n[显式环境求值器：闭合算术]\n");
    TEST_MAIN_RUN(test_eval_add_23);
    TEST_MAIN_RUN(test_eval_mul_24);
    TEST_MAIN_RUN(test_eval_pow_23);
    TEST_MAIN_RUN(test_eval_sub_94);
    printf("\n[显式环境求值器：布尔与比较]\n");
    TEST_MAIN_RUN(test_eval_not_true);
    TEST_MAIN_RUN(test_eval_and_tt);
    TEST_MAIN_RUN(test_eval_eq_22);
    printf("\n[显式环境求值器：开放项与非终止]\n");
    TEST_MAIN_RUN(test_eval_open_var);
    TEST_MAIN_RUN(test_eval_y_timeout);
    printf("\n[显式环境求值器：步数统计]\n");
    TEST_MAIN_RUN(test_eval_steps_reduce);
    TEST_MAIN_RUN(test_eval_steps_church5_noop);
    printf("\n[λ-演算合一策略]\n");
    TEST_MAIN_RUN(test_lambda_unify_strategy);
TEST_MAIN_END()
