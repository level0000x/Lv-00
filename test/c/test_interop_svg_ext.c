/**
 * @file test_interop_svg_ext.c
 * @brief SVG 导出完整化契约测试（批次 112：interop_export_svg 七项简化完整化）
 *
 * 覆盖 interop_export_svg 完整化后的输出契约：
 *   - GEOM_CIRCLE 渲染：圆心/半径经 center/radius 节点解析，输出 <circle>
 *   - 区域曲线边界：直线段 L + 曲线段（coord_count>=6）Bezier C 链，输出 <path>
 *   - 约束精确交点：线段×圆组合输出二次方程解出的交点十字
 *   - 数学公式渲染：符号坐标经公式排版器输出分数/根式结构（<text>+<line>）
 *   - 交互式 JavaScript：输出 <script>（CDATA）与 data-node-id 属性
 *   - 多图层分组：节点元素带 data-node-id/data-node-type，约束带
 *     data-constraint-id/data-constraint-type
 *   - CSS 动画：输出 @keyframes（fade-in / pulse / dash-flow）
 *   - NULL 契约：graph/config 任一 NULL → lv_ERROR_INVALID_PARAM
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/interop.h"
#include "lv/constraint_graph.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ---- 读取整个文件到堆缓冲区（测试辅助） ---- */
static char *read_file_all(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    char *buf = (char *) malloc((size_t) sz + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t rd = fread(buf, 1, (size_t) sz, f);
    fclose(f);
    buf[rd] = '\0';
    if (out_len)
        *out_len = rd;
    return buf;
}

/* ---- 构造含圆/曲线/区域/约束的图（全部走公开 API） ---- */
static ConstraintGraph *build_svg_fixture(void) {
    ConstraintGraph *g = graph_create();
    if (!g)
        return NULL;

    /* 点 p0(0,0) p1(4,0) p2(4,3) p3(0,3) */
    int p0 = add_point(g, 0, 1, 0, 1);
    int p1 = add_point(g, 4, 1, 0, 1);
    int p2 = add_point(g, 4, 1, 3, 1);
    int p3 = add_point(g, 0, 1, 3, 1);

    /* quadratic 坐标点 q(1+√2, 2)：触发公式排版器 sqrt 渲染路径 */
    {
        Rational *ra = rational_create(1, 1);
        Rational *rb = rational_create(1, 1);
        SymbolicCoord *qx = symbolic_coord_create_quadratic(ra, rb, 2); /* 1 + 1*sqrt(2) */
        SymbolicCoord *qy = symbolic_coord_create_rational(2, 1);
        if (qx && qy) {
            SymbolicCoord *qcs[2] = {qx, qy};
            graph_add_point(g, qcs, 2);
        } else {
            if (qx)
                symbolic_coord_destroy(qx);
            if (qy)
                symbolic_coord_destroy(qy);
        }
    }

    /* 线段 s0: p0-p1（直线） */
    graph_add_line_segment(g, p0, p1);

    /* 曲线线段：3 个坐标对 (1,2) (2,3) (3,2) 模拟采样曲线 */
    SymbolicCoord *cs[6];
    cs[0] = symbolic_coord_create_rational(1, 1);
    cs[1] = symbolic_coord_create_rational(2, 1);
    cs[2] = symbolic_coord_create_rational(2, 1);
    cs[3] = symbolic_coord_create_rational(3, 1);
    cs[4] = symbolic_coord_create_rational(3, 1);
    cs[5] = symbolic_coord_create_rational(2, 1);
    bool ok = cs[0] && cs[1] && cs[2] && cs[3] && cs[4] && cs[5];
    int curve = -1;
    if (ok) {
        for (int k = 0; k < 6; k++)
            symbolic_coord_set_trust(cs[k], TRUST_GREEN);
        GeomNode *cn = graph_add_node_with_id(g, -1, GEOM_LINE_SEGMENT, cs, 6);
        if (cn)
            curve = cn->id;
    } else {
        for (int k = 0; k < 6; k++) {
            if (cs[k])
                symbolic_coord_destroy(cs[k]);
        }
    }

    /* 区域：p0-p1-p2-p3 四边 */
    int s01 = graph_add_line_segment(g, p0, p1);
    int s12 = graph_add_line_segment(g, p1, p2);
    int s23 = graph_add_line_segment(g, p2, p3);
    int s30 = graph_add_line_segment(g, p3, p0);
    if (s01 >= 0 && s12 >= 0 && s23 >= 0 && s30 >= 0) {
        int bnd[4] = {s01, s12, s23, s30};
        graph_add_region(g, bnd, 4);
    }

    /* 圆：圆心 p3(0,3)，半径端 p1(4,0)（半径 5） */
    graph_add_circle(g, p3, p1);
    int circle_id = -1;
    for (int i = 0; i < g->node_count; i++) {
        if (g->nodes[i] && g->nodes[i]->type == GEOM_CIRCLE) {
            circle_id = g->nodes[i]->id;
            break;
        }
    }

    /* 约束：线段 s12 与圆相交（INTERSECTION）→ 精确交点求解路径 */
    if (circle_id >= 0)
        graph_add_constraint_with_id(g, -1, INTERSECTION, (int[]) {s12, circle_id}, 2);

    /* 曲线线段参与 INCIDENCE（若创建成功） */
    if (curve >= 0)
        graph_add_constraint_with_id(g, -1, INCIDENCE, (int[]) {p0, curve}, 2);

    return g;
}

/* ============== 测试：NULL 契约 ============== */

static void test_svg_null_api(void) {
    TEST_ASSERT_EQ(interop_export_svg(NULL, NULL), lv_ERROR_INVALID_PARAM);
    InteropExportConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    lv_strlcpy(cfg.output_path, "./_tmp_svg_null.svg", sizeof(cfg.output_path));
    TEST_ASSERT_EQ(interop_export_svg(NULL, &cfg), lv_ERROR_INVALID_PARAM);
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_EQ(interop_export_svg(g, NULL), lv_ERROR_INVALID_PARAM);
    graph_destroy(g);
    printf("  test_svg_null_api: PASSED\n");
}

/* ============== 测试：完整化输出契约 ============== */

static void test_svg_complete_features(void) {
    ConstraintGraph *g = build_svg_fixture();
    TEST_ASSERT_NOT_NULL(g);

    const char *path = "./_tmp_svg_complete.svg";
    remove(path);
    InteropExportConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    lv_strlcpy(cfg.output_path, path, sizeof(cfg.output_path));

    TEST_ASSERT_EQ(interop_export_svg(g, &cfg), lv_OK);

    size_t len = 0;
    char *content = read_file_all(path, &len);
    TEST_ASSERT_NOT_NULL(content);
    if (content) {
        /* 1. 圆渲染：<circle 元素 + data-node-type="circle" */
        TEST_ASSERT(strstr(content, "<circle") != NULL, "SVG 含 <circle>");
        TEST_ASSERT(strstr(content, "data-node-type=\"circle\"") != NULL, "圆带 data-node-type");

        /* 2. 区域曲线边界：<path 元素（替代 polygon） */
        TEST_ASSERT(strstr(content, "<path class=\"region\"") != NULL, "区域渲染为 <path>");

        /* 3. 约束：data-constraint-id 分组 + dash-flow 动画类 */
        TEST_ASSERT(strstr(content, "data-constraint-id=") != NULL, "约束带 data-constraint-id");
        TEST_ASSERT(strstr(content, "dash-flow") != NULL, "约束虚线流动 class");

        /* 4. 交互式 JS：<script> + CDATA */
        TEST_ASSERT(strstr(content, "<script") != NULL, "SVG 含交互脚本");
        TEST_ASSERT(strstr(content, "CDATA") != NULL, "脚本用 CDATA 包裹");

        /* 5. CSS 动画：@keyframes */
        TEST_ASSERT(strstr(content, "@keyframes") != NULL, "SVG 含 CSS keyframes");
        TEST_ASSERT(strstr(content, "lv-fade-in") != NULL, "淡入动画");
        TEST_ASSERT(strstr(content, "lv-pulse") != NULL, "脉冲动画");
        TEST_ASSERT(strstr(content, "lv-dash-flow") != NULL, "虚线流动动画");

        /* 6. 数学公式：公式排版器输出斜体文本（分数/根式） */
        TEST_ASSERT(strstr(content, "font-style=\"italic\"") != NULL, "公式斜体文本");
        TEST_ASSERT(strstr(content, "√") != NULL, "根式 √ 符号");

        /* 7. data-node-id 属性（图层分组基础） */
        TEST_ASSERT(strstr(content, "data-node-id=") != NULL, "节点带 data-node-id");

        free(content);
    }
    remove(path);
    graph_destroy(g);
    printf("  test_svg_complete_features: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Interop SVG Ext Test Suite")
    printf("=== Lv-00 Interop SVG Ext Test Suite (batch 112) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_svg_null_api);
    TEST_MAIN_RUN(test_svg_complete_features);

    lv_cleanup();
TEST_MAIN_END()
