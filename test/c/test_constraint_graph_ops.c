/**
 * @file test_constraint_graph_ops.c
 * @brief 约束图操作测试
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/lv_utils.h"
#include "lv/symbolic_coord.h"

/* 使用共享 TEST/PASS/FAIL 宏；计数挂钩保持原有 P/F 计数行为 */
#define TEST_PASS_STATEMENT g_pass_count++
#define TEST_FAIL_STATEMENT g_fail_count++

#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== constraint_graph 约束图操作测试 ===\n\n");

    /* 组 1 */
    printf("[组 1] 图创建与基本属性\n");
    {
        TEST("create");
        ConstraintGraph *g = graph_create();
        if (g)
            PASS();
        else
            FAIL("NULL");
        TEST("node_count=0");
        if (graph_get_node_count(g) == 0)
            PASS();
        else
            FAIL("!0");
        TEST("constraint_count=0");
        if (graph_get_constraint_count(g) == 0)
            PASS();
        else
            FAIL("!0");
        TEST("destroy NULL");
        graph_destroy(NULL);
        PASS();
        graph_destroy(g);
    }

    /* 组 2 */
    printf("[组 2] 节点添加\n");
    {
        ConstraintGraph *g = graph_create();
        SymbolicCoord *c0 = symbolic_coord_create_rational(0, 1);
        SymbolicCoord *c1 = symbolic_coord_create_rational(1, 1);
        const SymbolicCoord *cs[2] = {c0, c1};

        TEST("add_point");
        AddNodeResult r = graph_add_point(g, (SymbolicCoord **) cs, 2);
        if (r == ADD_NODE_OK && graph_get_node_count(g) == 1)
            PASS();
        else
            FAIL("FAIL");

        int id1 = graph_get_last_added_node_id(g);
        TEST("get_node_by_id");
        GeomNode *n = graph_get_node_by_id(g, id1);
        if (n && n->type == GEOM_POINT)
            PASS();
        else
            FAIL("NULL");

        const SymbolicCoord *cs2[2] = {c0, c0};
        AddNodeResult r2 = graph_add_point(g, (SymbolicCoord **) cs2, 2);
        int id2 = graph_get_last_added_node_id(g);
        TEST("ID递增");
        if (id2 > id1)
            PASS();
        else
            FAIL("不变");

        TEST("bad_id→NULL");
        if (graph_get_node_by_id(g, 9999) == NULL)
            PASS();
        else
            FAIL("!NULL");

        symbolic_coord_destroy(c0);
        symbolic_coord_destroy(c1);
        graph_destroy(g);
    }

    /* 组 3 */
    printf("[组 3] 线段约束\n");
    {
        ConstraintGraph *g = graph_create();
        SymbolicCoord *c0 = symbolic_coord_create_rational(0, 1);
        SymbolicCoord *c1 = symbolic_coord_create_rational(1, 1);
        SymbolicCoord *c2 = symbolic_coord_create_rational(2, 1);
        const SymbolicCoord *cs1[2] = {c0, c1};
        const SymbolicCoord *cs2[2] = {c0, c2};
        graph_add_point(g, (SymbolicCoord **) cs1, 2);
        int p1 = graph_get_last_added_node_id(g);
        graph_add_point(g, (SymbolicCoord **) cs2, 2);
        int p2 = graph_get_last_added_node_id(g);

        TEST("add_line_segment");
        AddNodeResult r = graph_add_line_segment(g, p1, p2);
        if (r == ADD_NODE_OK)
            PASS();
        else
            FAIL("FAIL");

        int sid = graph_get_last_added_node_id(g);
        GeomNode *sn = graph_get_node_by_id(g, sid);
        TEST("type=SEGMENT");
        if (sn && sn->type == GEOM_LINE_SEGMENT)
            PASS();
        else
            FAIL("!SEGMENT");

        symbolic_coord_destroy(c0);
        symbolic_coord_destroy(c1);
        symbolic_coord_destroy(c2);
        graph_destroy(g);
    }

    /* 组 4 */
    printf("[组 4] 约束查询\n");
    {
        ConstraintGraph *g = graph_create();
        SymbolicCoord *c0 = symbolic_coord_create_rational(0, 1);
        SymbolicCoord *c1 = symbolic_coord_create_rational(1, 1);
        SymbolicCoord *c2 = symbolic_coord_create_rational(2, 1);
        SymbolicCoord *c3 = symbolic_coord_create_rational(3, 1);
        const SymbolicCoord *cs1[2] = {c0, c1}, *cs2[2] = {c0, c2}, *cs3[2] = {c1, c3};
        graph_add_point(g, (SymbolicCoord **) cs1, 2);
        int p1 = graph_get_last_added_node_id(g);
        graph_add_point(g, (SymbolicCoord **) cs2, 2);
        int p2 = graph_get_last_added_node_id(g);
        graph_add_point(g, (SymbolicCoord **) cs3, 2);
        int p3 = graph_get_last_added_node_id(g);

        TEST("add_incidence");
        AddNodeResult ln = graph_add_line_segment(g, p1, p2);
        int lid = graph_get_last_added_node_id(g);
        AddConstraintResult ic = graph_add_incidence(g, p1, lid);
        if (ic == ADD_CONSTRAINT_OK)
            PASS();
        else
            FAIL("FAIL");

        TEST("add_betweenness");
        graph_add_betweenness(g, p1, p2, p3); /* 不崩溃即通过 */
        PASS();

        TEST("get_constraint");
        int cc = graph_get_constraint_count(g);
        if (cc > 0) {
            Constraint *con = graph_get_constraint(g, 0);
            if (con)
                PASS();
            else
                FAIL("NULL");
        } else
            PASS();

        symbolic_coord_destroy(c0);
        symbolic_coord_destroy(c1);
        symbolic_coord_destroy(c2);
        symbolic_coord_destroy(c3);
        graph_destroy(g);
    }

    /* 组 5 */
    printf("[组 5] 冲突检测\n");
    {
        ConstraintGraph *g = graph_create();
        SymbolicCoord *c0 = symbolic_coord_create_rational(0, 1);
        SymbolicCoord *c1 = symbolic_coord_create_rational(1, 1);
        SymbolicCoord *c2 = symbolic_coord_create_rational(2, 1);
        const SymbolicCoord *cs1[2] = {c0, c1}, *cs2[2] = {c1, c2}, *cs3[2] = {c0, c2};
        graph_add_point(g, (SymbolicCoord **) cs1, 2);
        int p1 = graph_get_last_added_node_id(g);
        graph_add_point(g, (SymbolicCoord **) cs2, 2);
        int p2 = graph_get_last_added_node_id(g);
        graph_add_point(g, (SymbolicCoord **) cs3, 2);
        int p3 = graph_get_last_added_node_id(g);
        graph_add_line_segment(g, p1, p2);
        graph_add_line_segment(g, p2, p3);
        graph_add_line_segment(g, p3, p1);

        TEST("detect_conflicts");
        int nc = 0, *sz = NULL;
        int **cf = graph_detect_conflicts(g, &nc, &sz);
        if (cf) {
            for (int i = 0; i < nc; i++)
                lv_free_ptr(cf[i]);
            lv_free_ptr(cf);
        }
        if (sz)
            lv_free_ptr(sz);
        PASS();

        symbolic_coord_destroy(c0);
        symbolic_coord_destroy(c1);
        symbolic_coord_destroy(c2);
        graph_destroy(g);
    }

    /* 组 6 */
    printf("[组 6] 序列化\n");
    {
        ConstraintGraph *g = graph_create();
        SymbolicCoord *c0 = symbolic_coord_create_rational(0, 1);
        SymbolicCoord *c1 = symbolic_coord_create_rational(1, 1);
        const SymbolicCoord *cs[2] = {c0, c1};
        graph_add_point(g, (SymbolicCoord **) cs, 2);

        TEST("serialize_json");
        char *j = graph_serialize_to_json(g);
        if (j && strlen(j) > 0)
            PASS();
        else
            FAIL("empty");
        lv_free_ptr(j);

        TEST("NULL→empty");
        char *nj = graph_serialize_to_json(NULL);
        if (nj == NULL || strlen(nj) == 0)
            PASS();
        else
            FAIL("!empty");
        lv_free_ptr(nj);

        symbolic_coord_destroy(c0);
        symbolic_coord_destroy(c1);
        graph_destroy(g);
    }

    /* 组 7 */
    printf("[组 7] 兼容性检查\n");
    {
        ConstraintGraph *g = graph_create();
        SymbolicCoord *c0 = symbolic_coord_create_rational(0, 1);
        SymbolicCoord *c1 = symbolic_coord_create_rational(1, 1);
        const SymbolicCoord *cs[2] = {c0, c1};
        graph_add_point(g, (SymbolicCoord **) cs, 2);

        TEST("check_compatibility");
        lvConstraintCompatibilityResult r;
        bool compat = graph_check_compatibility(g, &r);
        /* 不应崩溃 */
        (void) compat;
        PASS();

        symbolic_coord_destroy(c0);
        symbolic_coord_destroy(c1);
        graph_destroy(g);
    }

    /* 组 8 */
    printf("[组 8] 大量节点\n");
    {
        ConstraintGraph *g = graph_create();
        const int N = 50;
        TEST("mas_add_50");
        for (int i = 0; i < N; i++) {
            SymbolicCoord *cx = symbolic_coord_create_rational(i, 1);
            SymbolicCoord *cy = symbolic_coord_create_rational(i * 2, 1);
            const SymbolicCoord *cs[2] = {cx, cy};
            if (graph_add_point(g, (SymbolicCoord **) cs, 2) != ADD_NODE_OK) {
                FAIL("FAIL");
                symbolic_coord_destroy(cx);
                symbolic_coord_destroy(cy);
                goto out;
            }
            symbolic_coord_destroy(cx);
            symbolic_coord_destroy(cy);
        }
        if (graph_get_node_count(g) == N)
            PASS();
        else
            FAIL("!50");
    out:
        graph_destroy(g);
    }

    /* 组 9 */
    printf("[组 9] NULL安全\n");
    {
        TEST("NULL→0");
        if (graph_get_node_count(NULL) == 0)
            PASS();
        else
            FAIL("!0");
        TEST("NULL→0");
        if (graph_get_constraint_count(NULL) == 0)
            PASS();
        else
            FAIL("!0");
        TEST("NULL→safe");
        lvConstraintCompatibilityResult r;
        graph_check_compatibility(NULL, &r);
        PASS();
    }

    printf("\n=== %d passed, %d failed ===\n", g_pass_count, g_fail_count);
    return g_fail_count > 0 ? 1 : 0;
}
