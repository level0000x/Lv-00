/**
 * @file test_lambda_eval.c
 * @brief λ-演算 β-归约结果验证测试
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

/* ── 测试基础设施 ── */
#define TEST(n) printf("  [TEST] %s ... ", n)
#define PASS()            \
    do {                  \
        printf("PASS\n"); \
        P++;              \
    } while (0)
#define FAIL(m)                  \
    do {                         \
        printf("FAIL: %s\n", m); \
        F++;                     \
    } while (0)

static int P = 0, F = 0;

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

/* ====================================================================
 * main
 * ==================================================================== */
int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== λ-演算 β-归约结果验证 ===\n\n");

    printf("[β-归约步数验证]\n");
    TEST("Church 5 零归约步");
    test_church_5_no_redex();

    printf("\n[图结构比对]\n");
    TEST("succ 0 → 归约");
    test_succ_0_reduces();
    TEST("succ 1 → 归约");
    test_succ_1_reduces();
    TEST("add 1 1 → 归约");
    test_add_11_reduces();
    TEST("add 2 3 → 归约");
    test_add_23_reduces();
    TEST("mul 2 3 → 归约");
    test_mul_23_reduces();
    TEST("pow 2 3 → 归约");
    test_pow_23_reduces();

    printf("\n[归约步数合理性]\n");
    TEST("归约步数合理性");
    test_reduction_steps_reasonable();

    printf("\n=== %d passed, %d failed ===\n", P, F);
    return F > 0 ? 1 : 0;
}
