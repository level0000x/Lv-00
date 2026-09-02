/**
 * @file test_func_block_preset.c
 * @brief 预设函数块库测试
 *
 * @details 测试预设函数块库的初始化、查找、实例化和文档生成功能。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "func_block_preset.h"
#include "lv.h"
#include "test_helpers.h"

/* 全局测试计数器 */
int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * 测试辅助宏（仅保留项目特有的）
 * ============================================================ */

#define TEST_PASS(name)              \
    do {                             \
        printf("[PASS] %s\n", name); \
    } while (0)

/* ============================================================
 * 测试用例
 * ============================================================ */

/**
 * @brief 测试库初始化和清理
 */
static void test_library_lifecycle(void) {
    /* 测试重复初始化（幂等性） */
    TEST_ASSERT(func_block_preset_library_init(), "第一次初始化失败");
    TEST_ASSERT(func_block_preset_library_init(), "第二次初始化应成功（幂等）");

    /* 测试清理 */
    func_block_preset_library_cleanup();

    /* 重新初始化 */
    TEST_ASSERT(func_block_preset_library_init(), "清理后重新初始化失败");

    TEST_PASS("test_library_lifecycle");
}

/**
 * @brief 测试预设查找和元数据获取
 */
static void test_preset_lookup(void) {
    func_block_preset_library_init();

    /* 测试存在的预设 */
    TEST_ASSERT(func_block_preset_exists("midpoint"), "midpoint应存在");
    TEST_ASSERT(func_block_preset_exists("centroid"), "centroid应存在");
    TEST_ASSERT(func_block_preset_exists("distance"), "distance应存在");
    TEST_ASSERT(func_block_preset_exists("translation"), "translation应存在");
    TEST_ASSERT(func_block_preset_exists("vector_add"), "vector_add应存在");
    TEST_ASSERT(func_block_preset_exists("collinearity_test"), "collinearity_test应存在");

    /* 测试不存在的预设 */
    TEST_ASSERT(!func_block_preset_exists("nonexistent"), "不存在的预设应返回false");
    TEST_ASSERT(!func_block_preset_exists(NULL), "NULL应返回false");

    /* 测试元数据获取 */
    const PresetMetadata *m = func_block_preset_get_metadata("midpoint");
    TEST_ASSERT(m != NULL, "应能获取midpoint元数据");
    TEST_ASSERT_STR_EQ(m->name, "midpoint");
    TEST_ASSERT_EQ_MSG(m->input_count, 2, "midpoint应有2个输入");
    TEST_ASSERT_EQ_MSG(m->output_count, 1, "midpoint应有1个输出");
    TEST_ASSERT(m->category == PRESET_CATEGORY_CONSTRUCTION, "类别应为几何构造");

    /* 测试不存在的元数据 */
    TEST_ASSERT(func_block_preset_get_metadata("nonexistent") == NULL, "不存在的预设应返回NULL");
    TEST_ASSERT(func_block_preset_get_metadata(NULL) == NULL, "NULL应返回NULL");

    TEST_PASS("test_preset_lookup");
}

/**
 * @brief 测试参数数量查询
 */
static void test_param_counts(void) {
    func_block_preset_library_init();

    /* 测试输入参数数量 */
    TEST_ASSERT_EQ_MSG(func_block_preset_get_input_count("midpoint"), 2, "midpoint输入数应为2");
    TEST_ASSERT_EQ_MSG(func_block_preset_get_input_count("centroid"), 3, "centroid输入数应为3");
    TEST_ASSERT_EQ_MSG(func_block_preset_get_input_count("distance"), 2, "distance输入数应为2");
    TEST_ASSERT_EQ_MSG(func_block_preset_get_input_count("translation"), 3, "translation输入数应为3");
    TEST_ASSERT_EQ_MSG(func_block_preset_get_input_count("rotation"), 4, "rotation输入数应为4");

    /* 测试输出参数数量 */
    TEST_ASSERT_EQ_MSG(func_block_preset_get_output_count("midpoint"), 1, "midpoint输出数应为1");
    TEST_ASSERT_EQ_MSG(func_block_preset_get_output_count("centroid"), 1, "centroid输出数应为1");
    TEST_ASSERT_EQ_MSG(func_block_preset_get_output_count("tangent_lines"), 2, "tangent_lines输出数应为2");

    /* 测试不存在的预设 */
    TEST_ASSERT_EQ_MSG(func_block_preset_get_input_count("nonexistent"), -1, "不存在的预设应返回-1");
    TEST_ASSERT_EQ_MSG(func_block_preset_get_output_count("nonexistent"), -1, "不存在的预设应返回-1");

    TEST_PASS("test_param_counts");
}

/**
 * @brief 测试预设列表获取
 */
static void test_preset_list(void) {
    func_block_preset_library_init();

    const char *names[100];
    int count;

    /* 测试获取所有预设 */
    count = func_block_preset_list(names, 100, -1);
    TEST_ASSERT(count > 0, "应返回预设数量");
    printf("  [INFO] 总预设数量: %d\n", count);

    /* 测试按类别筛选 */
    count = func_block_preset_list(names, 100, PRESET_CATEGORY_CONSTRUCTION);
    TEST_ASSERT(count > 0, "几何构造类应有预设");
    printf("  [INFO] 几何构造类预设数量: %d\n", count);

    count = func_block_preset_list(names, 100, PRESET_CATEGORY_MEASUREMENT);
    TEST_ASSERT(count > 0, "度量计算类应有预设");
    printf("  [INFO] 度量计算类预设数量: %d\n", count);

    count = func_block_preset_list(names, 100, PRESET_CATEGORY_TRANSFORMATION);
    TEST_ASSERT(count > 0, "几何变换类应有预设");
    printf("  [INFO] 几何变换类预设数量: %d\n", count);

    count = func_block_preset_list(names, 100, PRESET_CATEGORY_ALGEBRAIC);
    TEST_ASSERT(count > 0, "代数运算类应有预设");
    printf("  [INFO] 代数运算类预设数量: %d\n", count);

    count = func_block_preset_list(names, 100, PRESET_CATEGORY_LOGIC);
    TEST_ASSERT(count > 0, "逻辑推导类应有预设");
    printf("  [INFO] 逻辑推导类预设数量: %d\n", count);

    /* 测试缓冲区限制 */
    count = func_block_preset_list(names, 5, -1);
    TEST_ASSERT(count >= 0, "即使缓冲区小也应返回数量");

    TEST_PASS("test_preset_list");
}

/**
 * @brief 测试字符串转换函数
 */
static void test_string_conversions(void) {
    func_block_preset_library_init();

    /* 测试类别字符串 */
    TEST_ASSERT_STR_EQ(func_block_preset_category_string(PRESET_CATEGORY_CONSTRUCTION), "几何构造");
    TEST_ASSERT_STR_EQ(func_block_preset_category_string(PRESET_CATEGORY_MEASUREMENT), "度量计算");
    TEST_ASSERT_STR_EQ(func_block_preset_category_string(PRESET_CATEGORY_TRANSFORMATION), "几何变换");
    TEST_ASSERT_STR_EQ(func_block_preset_category_string(PRESET_CATEGORY_ALGEBRAIC), "代数运算");
    TEST_ASSERT_STR_EQ(func_block_preset_category_string(PRESET_CATEGORY_LOGIC), "逻辑推导");

    /* 测试参数类型字符串 */
    TEST_ASSERT_STR_EQ(func_block_preset_param_type_string(PARAM_TYPE_POINT), "点");
    TEST_ASSERT_STR_EQ(func_block_preset_param_type_string(PARAM_TYPE_LINE), "直线");
    TEST_ASSERT_STR_EQ(func_block_preset_param_type_string(PARAM_TYPE_CIRCLE), "圆");
    TEST_ASSERT_STR_EQ(func_block_preset_param_type_string(PARAM_TYPE_SCALAR), "标量");

    /* 测试复杂度字符串 */
    TEST_ASSERT_STR_EQ(func_block_preset_complexity_string(COMPLEXITY_O1), "O(1) - 常数时间");
    TEST_ASSERT_STR_EQ(func_block_preset_complexity_string(COMPLEXITY_ON), "O(n) - 线性时间");

    /* 测试性质字符串 */
    char buffer[256];
    int len = func_block_preset_properties_string(
        PRESET_PROPERTY_DETERMINISTIC | PRESET_PROPERTY_CONTINUOUS | PRESET_PROPERTY_CONSTRUCTIVE, buffer,
        sizeof(buffer));
    TEST_ASSERT(len > 0, "性质字符串应有内容");
    printf("  [INFO] 性质字符串: %s\n", buffer);

    TEST_PASS("test_string_conversions");
}

/**
 * @brief 测试文档生成
 */
static void test_documentation(void) {
    func_block_preset_library_init();

    char buffer[4096];
    size_t len;

    /* 测试单个预设文档生成 */
    len = func_block_preset_generate_doc("midpoint", buffer, sizeof(buffer));
    TEST_ASSERT(len > 0 && len <= sizeof(buffer), "应成功生成文档");
    TEST_ASSERT(strstr(buffer, "midpoint") != NULL, "文档应包含预设名称");
    TEST_ASSERT(strstr(buffer, "描述") != NULL, "文档应包含描述");
    printf("  [INFO] midpoint文档长度: %zu\n", len);

    /* 测试不存在的预设 */
    len = func_block_preset_generate_doc("nonexistent", buffer, sizeof(buffer));
    TEST_ASSERT_EQ_MSG((int) len, 0, "不存在的预设应返回0");

    /* 测试索引生成 */
    len = func_block_preset_generate_index(buffer, sizeof(buffer));
    TEST_ASSERT(len > 0 && len <= sizeof(buffer), "应成功生成索引");
    TEST_ASSERT(strstr(buffer, "Lv-00 预设函数块库") != NULL, "索引应包含标题");
    printf("  [INFO] 索引文档长度: %zu\n", len);

    TEST_PASS("test_documentation");
}

/**
 * @brief 测试逆操作查询
 */
static void test_inverse_operations(void) {
    func_block_preset_library_init();

    /* 测试可逆操作 */
    const char *inv;

    inv = func_block_preset_get_inverse("translation");
    TEST_ASSERT(inv != NULL, "translation应有逆操作");

    inv = func_block_preset_get_inverse("rotation");
    TEST_ASSERT(inv != NULL, "rotation应有逆操作");

    inv = func_block_preset_get_inverse("scaling");
    TEST_ASSERT(inv != NULL, "scaling应有逆操作");

    inv = func_block_preset_get_inverse("inversion");
    TEST_ASSERT(inv != NULL, "inversion应有逆操作");

    inv = func_block_preset_get_inverse("reflection_point");
    TEST_ASSERT(inv != NULL, "reflection_point应有逆操作");

    /* 完整实现：元数据性质驱动（INVOLUTIVE 对合 / REVERSIBLE 可逆） */
    inv = func_block_preset_get_inverse("reflection_line");
    TEST_ASSERT(inv != NULL, "reflection_line(对合)应有逆操作");
    TEST_ASSERT(strcmp(inv, "reflection_line") == 0, "对合预设逆为自身");

    inv = func_block_preset_get_inverse("harmonic_conjugate");
    TEST_ASSERT(inv != NULL, "harmonic_conjugate(对合)应有逆操作");

    inv = func_block_preset_get_inverse("homothety");
    TEST_ASSERT(inv != NULL, "homothety(可逆)应有逆操作");
    TEST_ASSERT(strcmp(inv, "homothety") == 0, "可逆变换族逆为同名预设");

    /* 测试不存在的预设 */
    inv = func_block_preset_get_inverse("nonexistent");
    TEST_ASSERT(inv == NULL, "不存在的预设应返回NULL");

    TEST_PASS("test_inverse_operations");
}

/**
 * @brief 测试实例化时 add_to_graph 真实入图（回归：P1-1 空分支缺陷）
 *
 * 旧实现中 func_block_preset_instantiate_ex 的 add_to_graph 分支为空，
 * 而 func_block_preset_instantiate 默认 add_to_graph=true，调用方期望
 * 函数块被加入约束图实际没有。修复后应新增 GEOM_FUNCTION_BLOCK 节点。
 */
static void test_instantiate_add_to_graph(void) {
    func_block_preset_library_init();

    ConstraintGraph *g = graph_create();
    TEST_ASSERT(g != NULL, "graph_create 失败");

    /* 创建两个输入点（midpoint 预设需要 2 个输入） */
    SymbolicCoord *ax = symbolic_coord_create_rational(0, 1);
    SymbolicCoord *ay = symbolic_coord_create_rational(0, 1);
    SymbolicCoord *bx = symbolic_coord_create_rational(2, 1);
    SymbolicCoord *by = symbolic_coord_create_rational(0, 1);
    SymbolicCoord *coords_a[2] = {ax, ay};
    SymbolicCoord *coords_b[2] = {bx, by};
    AddNodeResult ar = graph_add_point(g, coords_a, 2);
    AddNodeResult br = graph_add_point(g, coords_b, 2);
    symbolic_coord_destroy(ax);
    symbolic_coord_destroy(ay);
    symbolic_coord_destroy(bx);
    symbolic_coord_destroy(by);
    TEST_ASSERT(ar == ADD_NODE_OK, "添加点 A 失败");
    TEST_ASSERT(br == ADD_NODE_OK, "添加点 B 失败");

    int before = g->node_count;

    FuncBlock *fb = NULL;
    int inputs[2];
    inputs[0] = 0; /* 点 ID 0 */
    inputs[1] = 1; /* 点 ID 1 */
    InstantiateResult result =
        func_block_preset_instantiate("midpoint", inputs, 2, g, &fb);

    TEST_ASSERT(result == lv_INSTANTIATE_OK, "midpoint 实例化应成功");
    TEST_ASSERT(fb != NULL, "实例化应返回函数块");
    if (fb) {
        /* 默认 add_to_graph=true：图中应新增 GEOM_FUNCTION_BLOCK 节点 */
        TEST_ASSERT(g->node_count == before + 1, "图中应新增 1 个函数块节点");
        GeomNode *last = g->nodes[g->node_count - 1];
        TEST_ASSERT(last != NULL && last->type == GEOM_FUNCTION_BLOCK, "新增节点应为函数块类型");
        func_block_destroy(fb);
    }

    graph_destroy(g);
    TEST_PASS("test_instantiate_add_to_graph");
}

/* ============================================================
 * 蓝图预设 API（TEN_LAYER_OPTIMIZED_PLAN §12.9 R14，批次 G1d）
 * ============================================================ */

/** 自定义预设 execute 桩：记录被调用并输出 1 个节点 ID */
static int g_exec_calls = 0;
static bool custom_exec(ConstraintGraph *graph, const int *inputs, int input_count, int **outputs, int *output_count) {
    (void) graph;
    (void) inputs;
    (void) input_count;
    g_exec_calls++;
    if (outputs == NULL || output_count == NULL)
        return false;
    int *out = (int *) lv_malloc(sizeof(int));
    if (out == NULL)
        return false;
    out[0] = 42;
    *outputs = out;
    *output_count = 1;
    return true;
}

static void test_blueprint_preset_api(void) {
    /* 注册自定义预设 */
    lvPresetBlockDef def;
    memset(&def, 0, sizeof(def));
    def.name = "test_custom_preset";
    def.category = "自定义";
    def.description = "自定义测试预设";
    def.min_inputs = NULL;
    def.max_inputs = NULL;
    def.execute = custom_exec;
    TEST_ASSERT(lv_preset_register(&def), "自定义预设注册成功");
    TEST_ASSERT(!lv_preset_register(&def), "同名重复注册拒绝");
    TEST_ASSERT(!lv_preset_register(NULL), "NULL 注册拒绝");

    /* get：查自定义表 */
    const lvPresetBlockDef *got = lv_preset_get("test_custom_preset");
    TEST_ASSERT(got != NULL, "自定义预设可查到");
    TEST_ASSERT(got->execute != NULL, "execute 保留");
    TEST_ASSERT(strcmp(got->category, "自定义") == 0, "类别保留");
    TEST_ASSERT(lv_preset_get("no_such_preset") == NULL, "未知预设返回 NULL");

    /* unregister：自定义表注销 */
    TEST_ASSERT(lv_preset_unregister("test_custom_preset"), "自定义注销成功");
    TEST_ASSERT(lv_preset_get("test_custom_preset") == NULL, "注销后查不到");
    TEST_ASSERT(!lv_preset_unregister("test_custom_preset"), "重复注销失败");

    /* get 回退：preset_blocks 注册表（内置预设应有元数据） */
    const lvPresetBlockDef *builtin = lv_preset_get("midpoint");
    if (builtin == NULL)
        builtin = lv_preset_get("common"); /* 通用占位兜底 */
    TEST_ASSERT(builtin != NULL, "内置预设回退查询成功");
    TEST_ASSERT(builtin->execute == NULL, "内置预设无 execute（仅元数据）");

    /* 几何构造：建图 + 三点 */
    ConstraintGraph *g = graph_create();
    TEST_ASSERT(g != NULL, "图创建成功");
    add_point(g, 0, 1, 0, 1); /* A(0,0) */
    add_point(g, 4, 1, 0, 1); /* B(4,0) */
    add_point(g, 0, 1, 4, 1); /* C(0,4) */

    /* 中点：A(0,0) 与 B(4,0) → (2,0) */
    int mid = -1;
    TEST_ASSERT(lv_preset_create_midpoint(g, 0, 1, &mid), "中点构造成功");
    TEST_ASSERT(mid >= 0, "中点 ID 有效");
    GeomNode *mnode = graph_get_node(g, mid);
    TEST_ASSERT(mnode != NULL && mnode->coord_count >= 2, "中点有坐标");
    double mx, my;
    TEST_ASSERT(symbolic_coord_get_xy(mnode->symbolic_coords, 2, &mx, &my), "中点坐标读取");
    TEST_ASSERT(mx > 1.9 && mx < 2.1, "中点 x≈2");
    TEST_ASSERT(my > -0.1 && my < 0.1, "中点 y≈0");

    /* 重心：A(0,0) B(4,0) C(0,4) → (4/3, 4/3) */
    int cen = -1;
    TEST_ASSERT(lv_preset_create_centroid(g, 0, 1, 2, &cen), "重心构造成功");
    GeomNode *cnode = graph_get_node(g, cen);
    TEST_ASSERT(cnode != NULL, "重心节点存在");
    double gx, gy;
    TEST_ASSERT(symbolic_coord_get_xy(cnode->symbolic_coords, 2, &gx, &gy), "重心坐标读取");
    TEST_ASSERT(gx > 1.2 && gx < 1.5, "重心 x≈4/3");
    TEST_ASSERT(gy > 1.2 && gy < 1.5, "重心 y≈4/3");

    /* 反射：B(4,0) 关于 C(0,4) → ( -4, 8 ) */
    int ref = -1;
    TEST_ASSERT(lv_preset_create_reflection(g, 1, 2, &ref), "反射构造成功");
    GeomNode *rnode = graph_get_node(g, ref);
    TEST_ASSERT(rnode != NULL, "反射节点存在");
    double rx, ry;
    TEST_ASSERT(symbolic_coord_get_xy(rnode->symbolic_coords, 2, &rx, &ry), "反射坐标读取");
    TEST_ASSERT(rx < -3.9 && rx > -4.1, "反射 x≈-4");
    TEST_ASSERT(ry > 7.9 && ry < 8.1, "反射 y≈8");

    /* 平移：B(4,0) + C(0,4) → (4,4) */
    int tr = -1;
    TEST_ASSERT(lv_preset_create_translation(g, 1, 2, &tr), "平移构造成功");
    GeomNode *tnode = graph_get_node(g, tr);
    TEST_ASSERT(tnode != NULL, "平移节点存在");
    double tx2, ty2;
    TEST_ASSERT(symbolic_coord_get_xy(tnode->symbolic_coords, 2, &tx2, &ty2), "平移坐标读取");
    TEST_ASSERT(tx2 > 3.9 && tx2 < 4.1, "平移 x≈4");
    TEST_ASSERT(ty2 > 3.9 && ty2 < 4.1, "平移 y≈4");

    /* 外心/垂心/内心：直角三角形 A(0,0) B(4,0) C(0,4) → 外心 (2,2) */
    int circ = -1;
    TEST_ASSERT(lv_preset_create_circumcenter(g, 0, 1, 2, &circ), "外心构造成功");
    GeomNode *ccnode = graph_get_node(g, circ);
    TEST_ASSERT(ccnode != NULL, "外心节点存在");
    double ccx, ccy;
    TEST_ASSERT(symbolic_coord_get_xy(ccnode->symbolic_coords, 2, &ccx, &ccy), "外心坐标读取");
    TEST_ASSERT(ccx > 1.9 && ccx < 2.1, "外心 x≈2");
    TEST_ASSERT(ccy > 1.9 && ccy < 2.1, "外心 y≈2");

    /* 垂心：直角顶点 A(0,0)（直角在 A，垂心=A） */
    int orth = -1;
    TEST_ASSERT(lv_preset_create_orthocenter(g, 0, 1, 2, &orth), "垂心构造成功");
    GeomNode *onode = graph_get_node(g, orth);
    TEST_ASSERT(onode != NULL, "垂心节点存在");
    double ox, oy;
    TEST_ASSERT(symbolic_coord_get_xy(onode->symbolic_coords, 2, &ox, &oy), "垂心坐标读取");
    TEST_ASSERT(ox > -0.1 && ox < 0.1, "垂心 x≈0");
    TEST_ASSERT(oy > -0.1 && oy < 0.1, "垂心 y≈0");

    /* 内心：直角等腰三角形内切圆半径 r=(4+4-4√2)/2≈1.172，内心 (r,r) */
    int inc = -1;
    TEST_ASSERT(lv_preset_create_incenter(g, 0, 1, 2, &inc), "内心构造成功");
    GeomNode *inode = graph_get_node(g, inc);
    TEST_ASSERT(inode != NULL, "内心节点存在");
    double ix2, iy2;
    TEST_ASSERT(symbolic_coord_get_xy(inode->symbolic_coords, 2, &ix2, &iy2), "内心坐标读取");
    TEST_ASSERT(ix2 > 1.0 && ix2 < 1.3, "内心 x≈1.172");
    TEST_ASSERT(iy2 > 1.0 && iy2 < 1.3, "内心 y≈1.172");

    /* NULL/越界契约 */
    TEST_ASSERT(!lv_preset_create_midpoint(g, 0, 99, &mid), "越界节点拒绝");
    TEST_ASSERT(!lv_preset_create_midpoint(NULL, 0, 1, &mid), "NULL 图拒绝");
    TEST_ASSERT(!lv_preset_create_midpoint(g, 0, 1, NULL), "NULL 输出拒绝");

    graph_destroy(g);
    TEST_PASS("test_blueprint_preset_api");
}

/* ============================================================
 * 主函数
 * ============================================================ */

TEST_MAIN_BEGIN("预设函数块库测试")
    printf("========================================\n");
    printf("预设函数块库测试\n");
    printf("========================================\n\n");
    g_pass_count = 0;
    g_fail_count = 0;
    TEST_MAIN_RUN(test_library_lifecycle);
    TEST_MAIN_RUN(test_preset_lookup);
    TEST_MAIN_RUN(test_param_counts);
    TEST_MAIN_RUN(test_preset_list);
    TEST_MAIN_RUN(test_string_conversions);
    TEST_MAIN_RUN(test_documentation);
    TEST_MAIN_RUN(test_inverse_operations);
    TEST_MAIN_RUN(test_instantiate_add_to_graph);
    TEST_MAIN_RUN(test_blueprint_preset_api);
    printf("\n========================================\n");
    if (g_fail_count == 0) {
        printf("所有测试通过! (%d 项)\n", g_pass_count);
    } else {
        printf("测试结果: %d 通过, %d 失败, %d 总计\n", g_pass_count, g_fail_count, g_pass_count + g_fail_count);
    }
    printf("========================================\n");
    func_block_preset_library_cleanup();
    return g_fail_count > 0 ? 1 : 0;
TEST_MAIN_END()
