/**
 * @file test_formula_ext.c
 * @brief 公式解析器扩展契约测试（批次 C-㊸：formula_parser.h 17 个零覆盖 API）
 *
 * 覆盖 17 个 ctest 零覆盖公共 API：
 *   - 解析族：formula_detect_syntax / formula_parser_get_last_error
 *   - 生命周期族：formula_node_ref / formula_node_refcount / formula_node_copy
 *   - 构建族：formula_create_identifier / _equation / _coord_list /
 *     _geom_point / _geom_segment / _geom_circle / _geom_triangle /
 *     _geom_polygon / _geom_region / _geom_arc / _constraint / _compound /
 *     formula_compound_add_statement
 *
 * 契约要点（与实现核对）：
 *   - 引用计数：create 后 refcount=1；父节点持有子节点引用（ref 递增）；
 *     destroy 递减，归零才释放（formula_node_destroy 内部 unref 语义）。
 *   - 数组构建（coord_list/polygon/region/constraint/compound）复制指针数组；
 *     coord_list 对子节点 ref，polygon/region/constraint/compound 不 ref
 *     （族内契约差异，按实现断言）。
 *   - formula_detect_syntax：NULL/空 → "unknown"，LaTeX 命令 → "latex"，
 *     DSL 关键字 → "dsl"，Python 特征 → "python"，默认 → "dsl"。
 *   - formula_create_number(denominator=0) → NULL。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/formula_parser.h" /* lv.h 聚合未导出 formula 家族，显式包含 */

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：语法检测与解析错误 ============== */

static void test_syntax_detect_api(void) {
    /* detect：NULL/空 → unknown */
    TEST_ASSERT_STR_EQ(formula_detect_syntax(NULL), "unknown");
    TEST_ASSERT_STR_EQ(formula_detect_syntax(""), "unknown");

    /* LaTeX / DSL / Python / 默认 */
    TEST_ASSERT_STR_EQ(formula_detect_syntax("\\frac{a}{b}"), "latex");
    TEST_ASSERT_STR_EQ(formula_detect_syntax("point A(1, 2)"), "dsl");
    TEST_ASSERT_STR_EQ(formula_detect_syntax("x**2 + 1"), "python");
    TEST_ASSERT_STR_EQ(formula_detect_syntax("plain text"), "dsl"); /* 默认 DSL */

    /* parse：NULL 契约 */
    TEST_ASSERT_NULL(formula_parse(NULL, "auto"));
    /* 非法输入 → NULL + 错误信息可查 */
    TEST_ASSERT_NULL(formula_parse("(((", "auto"));
    /* 合法输入 → 非 NULL（DSL 点声明） */
    FormulaNode *ast = formula_parse("point A(1, 2)", "auto");
    TEST_ASSERT_NOT_NULL(ast);
    formula_node_destroy(ast);

    /* get_last_error：错误后非 NULL（先触发错误） */
    formula_parse("((( ", "auto");
    const char *err = formula_parser_get_last_error();
    TEST_ASSERT(err != NULL, "错误后 last_error 非空");

    printf("  test_syntax_detect_api: PASSED\n");
}

/* ============== 测试：引用计数 ============== */

static void test_node_ref_api(void) {
    /* NULL 契约 */
    TEST_ASSERT_EQ(formula_node_ref(NULL), 0);
    TEST_ASSERT_EQ(formula_node_refcount(NULL), 0);

    FormulaNode *n = formula_create_number(3, 2);
    TEST_ASSERT_NOT_NULL(n);
    TEST_ASSERT_EQ(formula_node_refcount(n), 1);
    TEST_ASSERT_EQ(formula_node_ref(n), 2);
    TEST_ASSERT_EQ(formula_node_refcount(n), 2);
    TEST_ASSERT_EQ(formula_node_ref(n), 3);

    /* 释放引用：destroy 递减到 0 才真正释放（此处手动归还额外引用） */
    formula_node_destroy(n); /* refcount 3→2 */
    formula_node_destroy(n); /* 2→1 */
    formula_node_destroy(n); /* 1→0 释放 */
    printf("  test_node_ref_api: PASSED\n");
}

/* ============== 测试：深拷贝 ============== */

static void test_node_copy_api(void) {
    /* NULL → NULL */
    TEST_ASSERT_NULL(formula_node_copy(NULL));

    /* 数值节点拷贝 */
    FormulaNode *num = formula_create_number(7, 1);
    TEST_ASSERT_NOT_NULL(num);
    FormulaNode *num_copy = formula_node_copy(num);
    TEST_ASSERT_NOT_NULL(num_copy);
    TEST_ASSERT(num_copy != num, "深拷贝新对象");
    TEST_ASSERT_EQ(num_copy->type, NODE_NUMBER);
    TEST_ASSERT_EQ(num_copy->data.number.numerator, 7);
    TEST_ASSERT_EQ(num_copy->data.number.denominator, 1);
    TEST_ASSERT(num_copy->data.number.is_integer, "整数标记复制");
    formula_node_destroy(num_copy);
    formula_node_destroy(num);

    /* 变量节点拷贝：name 复制 */
    FormulaNode *var = formula_create_variable("x");
    TEST_ASSERT_NOT_NULL(var);
    FormulaNode *var_copy = formula_node_copy(var);
    TEST_ASSERT_NOT_NULL(var_copy);
    TEST_ASSERT_EQ(var_copy->type, NODE_VARIABLE);
    TEST_ASSERT_STR_EQ(var_copy->data.variable.name, "x");
    formula_node_destroy(var_copy);
    formula_node_destroy(var);

    /* 二元运算递归拷贝：子节点独立 */
    FormulaNode *a = formula_create_number(1, 1);
    FormulaNode *b = formula_create_number(2, 1);
    FormulaNode *op = formula_create_binary_op(NODE_BINARY_OP_ADD, a, b);
    TEST_ASSERT_NOT_NULL(op);
    FormulaNode *op_copy = formula_node_copy(op);
    TEST_ASSERT_NOT_NULL(op_copy);
    TEST_ASSERT_EQ(op_copy->type, NODE_BINARY_OP_ADD);
    TEST_ASSERT_NOT_NULL(op_copy->data.binary_op.left);
    TEST_ASSERT(op_copy->data.binary_op.left != op->data.binary_op.left, "左子节点深拷贝");
    TEST_ASSERT_EQ(op_copy->data.binary_op.left->type, NODE_NUMBER);
    TEST_ASSERT_NOT_NULL(op_copy->data.binary_op.right);
    TEST_ASSERT(op_copy->data.binary_op.right != op->data.binary_op.right, "右子节点深拷贝");
    /* 引用计数：父持有子引用 → destroy 父后子仍存活（refcount 递减） */
    formula_node_destroy(op_copy);
    formula_node_destroy(op);
    formula_node_destroy(a); /* a 仍存活（op 的引用已释放，回到 1）→ 释放 */
    formula_node_destroy(b);
    printf("  test_node_copy_api: PASSED\n");
}

/* ============== 测试：基础构建（number/identifier） ============== */

static void test_create_primitive_api(void) {
    /* number：denominator=0 → NULL */
    TEST_ASSERT_NULL(formula_create_number(1, 0));
    FormulaNode *n = formula_create_number(5, 2);
    TEST_ASSERT_NOT_NULL(n);
    TEST_ASSERT_EQ(n->type, NODE_NUMBER);
    TEST_ASSERT_EQ(n->data.number.numerator, 5);
    TEST_ASSERT_EQ(n->data.number.denominator, 2);
    TEST_ASSERT(!n->data.number.is_integer, "非整数标记");
    formula_node_destroy(n);

    /* identifier：NULL name → NULL；正常 name 复制 */
    TEST_ASSERT_NULL(formula_create_identifier(NULL));
    FormulaNode *id = formula_create_identifier("sin");
    TEST_ASSERT_NOT_NULL(id);
    TEST_ASSERT_EQ(id->type, NODE_IDENTIFIER);
    TEST_ASSERT_STR_EQ(id->data.identifier.name, "sin");
    formula_node_destroy(id);
    printf("  test_create_primitive_api: PASSED\n");
}

/* ============== 测试：几何对象构建 ============== */

static void test_create_geom_api(void) {
    FormulaNode *c0 = formula_create_number(0, 1);
    FormulaNode *c1 = formula_create_number(1, 1);
    FormulaNode *c2 = formula_create_number(2, 1);
    FormulaNode *c3 = formula_create_number(3, 1);

    /* name NULL → NULL（各几何构造器一致） */
    TEST_ASSERT_NULL(formula_create_geom_point(NULL, c0));
    TEST_ASSERT_NULL(formula_create_geom_segment(NULL, c0, c1));
    TEST_ASSERT_NULL(formula_create_geom_circle(NULL, c0, c1));
    TEST_ASSERT_NULL(formula_create_geom_triangle(NULL, c0, c1, c2));
    TEST_ASSERT_NULL(formula_create_geom_polygon(NULL, NULL, 0));
    TEST_ASSERT_NULL(formula_create_geom_region(NULL, NULL, 0));
    TEST_ASSERT_NULL(formula_create_geom_arc(NULL, c0, c1, c2, c3));

    /* 坐标列表（作为几何坐标载体） */
    FormulaNode *coords_arr[2] = {c0, c1};
    FormulaNode *coords = formula_create_coord_list(coords_arr, 2);
    TEST_ASSERT_NOT_NULL(coords);
    TEST_ASSERT_EQ(coords->type, NODE_COORDINATE_LIST);
    TEST_ASSERT_EQ(coords->data.coord_list.coord_count, 2);
    TEST_ASSERT_EQ(coords->data.coord_list.coords[0], c0);

    /* 点 */
    FormulaNode *p = formula_create_geom_point("A", coords);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQ(p->type, NODE_GEOM_POINT);
    TEST_ASSERT_STR_EQ(p->data.geom_point.name, "A");
    TEST_ASSERT(p->data.geom_point.coords == coords, "坐标列表引用");

    /* 线段 */
    FormulaNode *seg = formula_create_geom_segment("AB", c0, c1);
    TEST_ASSERT_NOT_NULL(seg);
    TEST_ASSERT_EQ(seg->type, NODE_GEOM_SEGMENT);
    TEST_ASSERT_STR_EQ(seg->data.geom_segment.name, "AB");
    TEST_ASSERT(seg->data.geom_segment.endpoint1 == c0, "端点1引用");
    TEST_ASSERT(seg->data.geom_segment.endpoint2 == c1, "端点2引用");

    /* 圆 */
    FormulaNode *circle = formula_create_geom_circle("O", c0, c1);
    TEST_ASSERT_NOT_NULL(circle);
    TEST_ASSERT_EQ(circle->type, NODE_GEOM_CIRCLE);
    TEST_ASSERT(circle->data.geom_circle.center == c0 && circle->data.geom_circle.radius == c1, "圆心/半径引用");

    /* 三角形 */
    FormulaNode *tri = formula_create_geom_triangle("ABC", c0, c1, c2);
    TEST_ASSERT_NOT_NULL(tri);
    TEST_ASSERT_EQ(tri->type, NODE_GEOM_TRIANGLE);
    TEST_ASSERT(tri->data.geom_triangle.vertex3 == c2, "顶点3引用");

    /* 多边形：数组复制 */
    FormulaNode *verts[3] = {c0, c1, c2};
    FormulaNode *poly = formula_create_geom_polygon("P", verts, 3);
    TEST_ASSERT_NOT_NULL(poly);
    TEST_ASSERT_EQ(poly->type, NODE_GEOM_POLYGON);
    TEST_ASSERT_EQ(poly->data.geom_polygon.vertex_count, 3);
    TEST_ASSERT(poly->data.geom_polygon.vertices[0] == c0, "顶点数组复制");

    /* 区域：数组复制 */
    FormulaNode *segs[2] = {seg, c1};
    FormulaNode *region = formula_create_geom_region("R", segs, 2);
    TEST_ASSERT_NOT_NULL(region);
    TEST_ASSERT_EQ(region->type, NODE_GEOM_REGION);
    TEST_ASSERT_EQ(region->data.geom_region.segment_count, 2);

    /* 弧 */
    FormulaNode *arc = formula_create_geom_arc("A1", c0, c1, c2, c3);
    TEST_ASSERT_NOT_NULL(arc);
    TEST_ASSERT_EQ(arc->type, NODE_GEOM_ARC);
    TEST_ASSERT(arc->data.geom_arc.start_angle == c2 && arc->data.geom_arc.end_angle == c3, "角度引用");

    /* 清理：先释放子节点引用（几何节点不 ref 子节点，destroy 各自独立） */
    formula_node_destroy(arc);
    formula_node_destroy(region);
    formula_node_destroy(poly);
    formula_node_destroy(tri);
    formula_node_destroy(circle);
    formula_node_destroy(seg);
    formula_node_destroy(p);
    formula_node_destroy(coords);
    formula_node_destroy(c0);
    formula_node_destroy(c1);
    formula_node_destroy(c2);
    formula_node_destroy(c3);
    printf("  test_create_geom_api: PASSED\n");
}

/* ============== 测试：表达式/约束/复合构建 ============== */

static void test_create_expr_api(void) {
    FormulaNode *x = formula_create_variable("x");
    FormulaNode *y = formula_create_variable("y");
    TEST_ASSERT_NOT_NULL(x);
    TEST_ASSERT_NOT_NULL(y);

    /* 方程：lhs/rhs 引用（父持有子引用） */
    FormulaNode *eq = formula_create_equation(x, y);
    TEST_ASSERT_NOT_NULL(eq);
    TEST_ASSERT_EQ(eq->type, NODE_EQUATION);
    TEST_ASSERT(eq->data.equation.lhs == x, "左式引用");
    TEST_ASSERT(eq->data.equation.rhs == y, "右式引用");
    formula_node_destroy(eq);
    formula_node_destroy(x); /* x 引用：eq 持有 + 调用者 → destroy 后剩 eq 的引用，再 destroy eq 后归零 */
    formula_node_destroy(y);

    /* 约束：参与者数组复制 */
    FormulaNode *a = formula_create_number(1, 1);
    FormulaNode *b = formula_create_number(2, 1);
    FormulaNode *parts[2] = {a, b};
    FormulaNode *con = formula_create_constraint(NODE_CONSTRAINT_PARALLEL, parts, 2);
    TEST_ASSERT_NOT_NULL(con);
    TEST_ASSERT_EQ(con->type, NODE_CONSTRAINT_PARALLEL);
    TEST_ASSERT_EQ(con->data.constraint.participant_count, 2);
    TEST_ASSERT(con->data.constraint.participants[0] == a, "参与者数组复制");
    formula_node_destroy(con);
    formula_node_destroy(a);
    formula_node_destroy(b);

    /* 复合语句：数组复制 + add_statement。
     * 注意：create_compound 不 ref 子节点（族内差异）——调用者须保持
     * 子节点存活直至 compound 销毁，先 destroy 父再 destroy 子。 */
    FormulaNode *s1 = formula_create_number(1, 1);
    FormulaNode *s2 = formula_create_number(2, 1);
    FormulaNode *stmts[2] = {s1, s2};
    FormulaNode *compound = formula_create_compound(stmts, 2);
    TEST_ASSERT_NOT_NULL(compound);
    TEST_ASSERT_EQ(compound->type, NODE_COMPOUND);
    TEST_ASSERT_EQ(compound->data.compound.statement_count, 2);
    TEST_ASSERT_EQ(compound->data.compound.statement_capacity, 2);

    /* add_statement：NULL 契约 */
    TEST_ASSERT_EQ(formula_compound_add_statement(NULL, NULL), -1);
    TEST_ASSERT_EQ(formula_compound_add_statement(compound, NULL), -1);
    FormulaNode *not_compound = formula_create_number(5, 1);
    TEST_ASSERT_EQ(formula_compound_add_statement(not_compound, not_compound), -1);
    formula_node_destroy(not_compound);

    /* 正路径：追加触发扩容（capacity 2 → 4） */
    FormulaNode *s3 = formula_create_number(3, 1);
    FormulaNode *s4 = formula_create_number(4, 1);
    FormulaNode *s5 = formula_create_number(5, 1);
    TEST_ASSERT_EQ(formula_compound_add_statement(compound, s3), 0);
    TEST_ASSERT_EQ(formula_compound_add_statement(compound, s4), 0);
    TEST_ASSERT_EQ(formula_compound_add_statement(compound, s5), 0);
    TEST_ASSERT_EQ(compound->data.compound.statement_count, 5);
    TEST_ASSERT(compound->data.compound.statement_capacity >= 5, "扩容");
    TEST_ASSERT(compound->data.compound.statements[4] == s5, "末条语句");

    /* 先销毁父（不 unref 子节点），再销毁子 */
    formula_node_destroy(compound);
    formula_node_destroy(s1);
    formula_node_destroy(s2);
    formula_node_destroy(s3);
    formula_node_destroy(s4);
    formula_node_destroy(s5);
    printf("  test_create_expr_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Formula Parser Ext Test Suite")
    printf("=== Lv-00 Formula Parser Ext Test Suite (batch C-㊸) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_syntax_detect_api);
    TEST_MAIN_RUN(test_node_ref_api);
    TEST_MAIN_RUN(test_node_copy_api);
    TEST_MAIN_RUN(test_create_primitive_api);
    TEST_MAIN_RUN(test_create_geom_api);
    TEST_MAIN_RUN(test_create_expr_api);

    lv_cleanup();
TEST_MAIN_END()
