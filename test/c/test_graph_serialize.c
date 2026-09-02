/**
 * @file test_graph_serialize.c
 * @brief 测试约束图的序列化与反序列化
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

#include "lv.h"
#include "node_deep_copy.h" /* node_deep_copy_geom_node：深拷贝一致性断言 */

void test_point_serialization(void) {
    printf("=== 测试点序列化 ===\n");

    /* 创建图 */
    ConstraintGraph *graph = graph_create();
    lv_ASSERT_NOT_NULL(graph);

    /* 创建点 */
    add_point(graph, 1, 1, 2, 1);

    /* 序列化 */
    char *json = graph_serialize_to_json(graph);
    lv_ASSERT_NOT_NULL(json);
    printf("序列化结果:\n%s\n", json);

    /* 反序列化 */
    ConstraintGraph *restored = graph_deserialize_from_json(json);
    lv_ASSERT_NOT_NULL(restored);
    lv_ASSERT(graph_get_node_count(restored) == 1);

    /* 坐标 round-trip 断言（守护修复：序列化器输出 "num/den" 简写，
     * 反序列化器此前静默丢弃坐标导致节点添加失败/坐标丢失） */
    GeomNode *p0 = graph_get_node(restored, 0);
    lv_ASSERT_NOT_NULL(p0);
    lv_ASSERT(p0->coord_count == 2);
    lv_ASSERT_NOT_NULL(p0->symbolic_coords);
    for (int i = 0; i < 2; i++) {
        lv_ASSERT_NOT_NULL(p0->symbolic_coords[i]);
        lv_ASSERT(p0->symbolic_coords[i]->type == RATIONAL);
    }

    /* 清理 */
    lv_free_ptr(json);
    graph_destroy(restored);
    graph_destroy(graph);

    printf("点序列化测试通过!\n\n");
}

void test_line_segment_serialization(void) {
    printf("=== 测试线段序列化 ===\n");

    ConstraintGraph *graph = graph_create();
    lv_ASSERT_NOT_NULL(graph);

    /* 创建两个点 */
    add_point(graph, 0, 1, 0, 1);
    add_point(graph, 3, 1, 4, 1);

    /* 创建线段 */
    AddNodeResult result = graph_add_line_segment(graph, 0, 1);
    lv_ASSERT(result == ADD_NODE_OK);

    /* 序列化 */
    char *json = graph_serialize_to_json(graph);
    lv_ASSERT_NOT_NULL(json);
    printf("线段序列化结果:\n%s\n", json);

    /* 反序列化 */
    ConstraintGraph *restored = graph_deserialize_from_json(json);
    lv_ASSERT_NOT_NULL(restored);
    printf("恢复后节点数: %d\n", graph_get_node_count(restored));
    printf("恢复后约束数: %d\n", graph_get_constraint_count(restored));

    lv_free_ptr(json);
    graph_destroy(restored);
    graph_destroy(graph);

    printf("线段序列化测试通过!\n\n");
}

void test_constraint_serialization(void) {
    printf("=== 测试约束序列化 ===\n");

    ConstraintGraph *graph = graph_create();
    lv_ASSERT_NOT_NULL(graph);

    /* 创建点 */
    add_point(graph, 0, 1, 0, 1);

    /* 创建线段 */
    add_point(graph, 1, 1, 1, 1);
    add_point(graph, 2, 1, 2, 1);
    graph_add_line_segment(graph, 1, 2);

    /* 添加关联约束 */
    AddConstraintResult cr = graph_add_incidence(graph, 0, 3);
    lv_ASSERT(cr == ADD_CONSTRAINT_OK);

    printf("原始节点数: %d\n", graph_get_node_count(graph));
    printf("原始约束数: %d\n", graph_get_constraint_count(graph));

    /* 序列化 */
    char *json = graph_serialize_to_json(graph);
    lv_ASSERT_NOT_NULL(json);
    printf("带约束的图序列化结果:\n%s\n", json);

    /* 反序列化 */
    ConstraintGraph *restored = graph_deserialize_from_json(json);
    lv_ASSERT_NOT_NULL(restored);
    printf("恢复后节点数: %d\n", graph_get_node_count(restored));
    printf("恢复后约束数: %d\n", graph_get_constraint_count(restored));

    lv_free_ptr(json);
    graph_destroy(restored);
    graph_destroy(graph);

    printf("约束序列化测试通过!\n\n");
}

void test_module_graph_serialization(void) {
    printf("=== 测试模块图序列化 ===\n");

    Module *mod = module_create("TestModule", "1.0.0");
    lv_ASSERT_NOT_NULL(mod);

    /* 创建图 */
    module_set_graph(mod, graph_create());
    lv_ASSERT_NOT_NULL(module_get_graph(mod));

    /* 添加节点 */
    add_point(module_get_graph(mod), 5, 1, 6, 1);

    printf("原始模块节点数: %d\n", graph_get_node_count(module_get_graph(mod)));

    /* 序列化整个模块 */
    char *json = module_serialize_to_json(mod);
    lv_ASSERT_NOT_NULL(json);
    printf("模块序列化结果:\n%s\n", json);

    /* 反序列化模块 */
    Module *restored_mod = NULL;
    ModuleLoadStatus status = module_deserialize_from_json(json, &restored_mod);
    printf("反序列化状态: %d\n", status);
    lv_ASSERT(status == MODULE_LOAD_OK);
    lv_ASSERT_NOT_NULL(restored_mod);
    printf("恢复后图指针: %p\n", (void *) module_get_graph(restored_mod));

    if (module_get_graph(restored_mod) != NULL) {
        printf("恢复后模块节点数: %d\n", graph_get_node_count(module_get_graph(restored_mod)));
    } else {
        printf("错误: 恢复后图为 NULL\n");
    }

    /* 单独序列化图 */
    char *graph_json = module_serialize_graph_to_json(mod);
    lv_ASSERT_NOT_NULL(graph_json);
    printf("独立图序列化:\n%s\n", graph_json);
    lv_free_ptr(graph_json);

    lv_free_ptr(json);
    module_destroy(restored_mod);
    module_destroy(mod);

    printf("模块图序列化测试通过!\n\n");
}

/* 销毁 node_deep_copy_geom_node 返回的游离深拷贝节点（测试辅助：
 * 指针类字段（boundary_segments / internal_nodes）仅释放数组本身，
 * 元素为源图引用，不得释放） */
static void destroy_detached_node(GeomNode *node) {
    if (!node)
        return;
    if (node->symbolic_coords) {
        for (int i = 0; i < node->coord_count; i++)
            symbolic_coord_destroy(node->symbolic_coords[i]);
        lv_free_ptr(node->symbolic_coords);
    }
    lv_free_ptr(node->numeric_assumption_declaration);
    switch (node->type) {
    case GEOM_PORT:
        lv_free_ptr(node->data.port);
        break;
    case GEOM_REGION:
        lv_free_ptr(node->data.region.boundary_segments);
        break;
    case GEOM_FUNCTION_BLOCK:
        lv_free_ptr(node->data.func_block.internal_nodes);
        lv_free_ptr(node->data.func_block.input_port_ids);
        lv_free_ptr(node->data.func_block.output_port_ids);
        break;
    default:
        break;
    }
    lv_free_ptr(node);
}

void test_advanced_node_roundtrip_and_deepcopy(void) {
    printf("=== 测试 区域/端口/函数块/圆 序列化-反序列化-深拷贝三方一致 ===\n");

    ConstraintGraph *graph = graph_create();
    lv_ASSERT_NOT_NULL(graph);

    /* 4 个点 (id 0..3) */
    int p_ids[4];
    for (int i = 0; i < 4; i++) {
        SymbolicCoord *c[2] = {symbolic_coord_create_rational(i % 2, 1), symbolic_coord_create_rational(i / 2, 1)};
        lv_ASSERT(graph_add_point(graph, c, 2) == ADD_NODE_OK);
        p_ids[i] = graph_get_node_count(graph) - 1;
        /* graph_add_point 深拷贝坐标，节点拥有副本；测试自建坐标须自行销毁 */
        symbolic_coord_destroy(c[0]);
        symbolic_coord_destroy(c[1]);
    }

    /* 4 条线段 (id 4..7) */
    int s_ids[4];
    for (int i = 0; i < 4; i++) {
        lv_ASSERT(graph_add_line_segment(graph, p_ids[i], p_ids[(i + 1) % 4]) == ADD_NODE_OK);
        s_ids[i] = graph_get_node_count(graph) - 1;
    }

    /* 区域 (id 8)：边界为 4 条线段 */
    lv_ASSERT(graph_add_region(graph, s_ids, 4) == ADD_NODE_OK);
    int region_id = graph_get_node_count(graph) - 1;

    /* 输入/输出端口 (id 9/10) */
    lv_ASSERT(graph_add_port(graph, PORT_INPUT, 1, -1) == ADD_NODE_OK);
    int in_port_id = graph_get_node_count(graph) - 1;
    lv_ASSERT(graph_add_port(graph, PORT_OUTPUT, 1, -1) == ADD_NODE_OK);
    int out_port_id = graph_get_node_count(graph) - 1;

    /* 函数块 (id 11)：内部节点 p0,p1；输入端口 in；输出端口 out */
    int internal[] = {p_ids[0], p_ids[1]};
    int inputs[] = {in_port_id};
    int outputs[] = {out_port_id};
    lv_ASSERT(graph_add_function_block(graph, internal, 2, inputs, 1, outputs, 1) == ADD_NODE_OK);
    int fb_id = graph_get_node_count(graph) - 1;

    /* 圆 (id 12)：圆心 p0，半径端点 p1 */
    lv_ASSERT(graph_add_circle(graph, p_ids[0], p_ids[1]) == ADD_NODE_OK);
    int circle_id = graph_get_node_count(graph) - 1;

    /* ── 序列化 → 反序列化 ── */
    char *json = graph_serialize_to_json(graph);
    lv_ASSERT_NOT_NULL(json);
    ConstraintGraph *restored = graph_deserialize_from_json(json);
    lv_ASSERT_NOT_NULL(restored);
    lv_ASSERT(graph_get_node_count(restored) == graph_get_node_count(graph));

    /* ── 断言 1：round-trip 后区域/端口/函数块/圆字段与序列化前一致 ── */
    GeomNode *r_region = graph_get_node(restored, region_id);
    lv_ASSERT(r_region && r_region->type == GEOM_REGION);
    lv_ASSERT(r_region->data.region.segment_count == 4);
    for (int i = 0; i < 4; i++) {
        lv_ASSERT_NOT_NULL(r_region->data.region.boundary_segments[i]);
        lv_ASSERT(r_region->data.region.boundary_segments[i]->id == s_ids[i]);
    }

    GeomNode *r_in = graph_get_node(restored, in_port_id);
    lv_ASSERT(r_in && r_in->type == GEOM_PORT && r_in->data.port);
    lv_ASSERT(r_in->data.port->type == PORT_INPUT);
    lv_ASSERT(r_in->data.port->namespace_depth == 1);
    lv_ASSERT(r_in->data.port->parent_block_id == -1);
    lv_ASSERT(r_in->data.port->connected_to == NULL);

    GeomNode *r_out = graph_get_node(restored, out_port_id);
    lv_ASSERT(r_out && r_out->type == GEOM_PORT && r_out->data.port);
    lv_ASSERT(r_out->data.port->type == PORT_OUTPUT);

    GeomNode *r_fb = graph_get_node(restored, fb_id);
    lv_ASSERT(r_fb && r_fb->type == GEOM_FUNCTION_BLOCK);
    lv_ASSERT(r_fb->data.func_block.internal_node_count == 2);
    lv_ASSERT(r_fb->data.func_block.internal_nodes[0]->id == p_ids[0]);
    lv_ASSERT(r_fb->data.func_block.internal_nodes[1]->id == p_ids[1]);
    lv_ASSERT(r_fb->data.func_block.input_count == 1);
    lv_ASSERT(r_fb->data.func_block.input_port_ids[0] == in_port_id);
    lv_ASSERT(r_fb->data.func_block.output_count == 1);
    lv_ASSERT(r_fb->data.func_block.output_port_ids[0] == out_port_id);

    GeomNode *r_circle = graph_get_node(restored, circle_id);
    lv_ASSERT(r_circle && r_circle->type == GEOM_CIRCLE);
    lv_ASSERT(r_circle->data.circle.center_node_id == p_ids[0]);
    lv_ASSERT(r_circle->data.circle.radius_node_id == p_ids[1]);

    /* ── 断言 2：node_deep_copy_geom_node（统一走 vtable->clone）深拷贝
     *           restored 图节点后，类型特定字段与反序列化节点一致 ── */
    GeomNode *fb_copy = node_deep_copy_geom_node(r_fb, NULL);
    lv_ASSERT_NOT_NULL(fb_copy);
    lv_ASSERT(fb_copy->type == GEOM_FUNCTION_BLOCK);
    lv_ASSERT(fb_copy->id == fb_id);
    lv_ASSERT(fb_copy->data.func_block.internal_node_count == 2);
    lv_ASSERT(fb_copy->data.func_block.internal_nodes[0]->id == p_ids[0]);
    lv_ASSERT(fb_copy->data.func_block.internal_nodes[1]->id == p_ids[1]);
    lv_ASSERT(fb_copy->data.func_block.input_count == 1);
    lv_ASSERT(fb_copy->data.func_block.input_port_ids[0] == in_port_id);
    lv_ASSERT(fb_copy->data.func_block.output_count == 1);
    lv_ASSERT(fb_copy->data.func_block.output_port_ids[0] == out_port_id);

    GeomNode *region_copy = node_deep_copy_geom_node(r_region, NULL);
    lv_ASSERT_NOT_NULL(region_copy);
    lv_ASSERT(region_copy->type == GEOM_REGION);
    lv_ASSERT(region_copy->data.region.segment_count == 4);
    for (int i = 0; i < 4; i++) {
        lv_ASSERT(region_copy->data.region.boundary_segments[i]->id == s_ids[i]);
    }

    GeomNode *port_copy = node_deep_copy_geom_node(r_in, NULL);
    lv_ASSERT_NOT_NULL(port_copy);
    lv_ASSERT(port_copy->type == GEOM_PORT && port_copy->data.port);
    lv_ASSERT(port_copy->data.port->type == PORT_INPUT);
    lv_ASSERT(port_copy->data.port->namespace_depth == 1);
    lv_ASSERT(port_copy->data.port->parent_block_id == -1);

    GeomNode *circle_copy = node_deep_copy_geom_node(r_circle, NULL);
    lv_ASSERT_NOT_NULL(circle_copy);
    lv_ASSERT(circle_copy->type == GEOM_CIRCLE);
    lv_ASSERT(circle_copy->data.circle.center_node_id == p_ids[0]);
    lv_ASSERT(circle_copy->data.circle.radius_node_id == p_ids[1]);

    GeomNode *point_copy = node_deep_copy_geom_node(graph_get_node(restored, p_ids[0]), NULL);
    lv_ASSERT_NOT_NULL(point_copy);
    lv_ASSERT(point_copy->type == GEOM_POINT);
    lv_ASSERT(point_copy->coord_count == 2);
    lv_ASSERT_NOT_NULL(point_copy->symbolic_coords);

    /* 清理深拷贝的游离节点 */
    destroy_detached_node(fb_copy);
    destroy_detached_node(region_copy);
    destroy_detached_node(port_copy);
    destroy_detached_node(circle_copy);
    destroy_detached_node(point_copy);

    lv_free_ptr(json);
    graph_destroy(restored);
    graph_destroy(graph);

    printf("区域/端口/函数块/圆 三方一致测试通过!\n\n");
}

/* ========== 蓝图约束 JSON API（TEN_LAYER_OPTIMIZED_PLAN §12.8 R13，批次 G1b） ========== */

static void test_blueprint_constraint_json(void) {
    printf("=== 测试蓝图单约束 JSON（lv_constraint_to_json / from_json）===\n");

    /* 构建：图 + 两条线段 + 平行约束（取约束对象） */
    ConstraintGraph *graph = graph_create();
    lv_ASSERT_NOT_NULL(graph);
    add_point(graph, 0, 1, 0, 1);   /* p0 (0,0) */
    add_point(graph, 3, 1, 4, 1);   /* p1 (3,4) */
    add_point(graph, 1, 1, 1, 1);   /* p2 (1,1) */
    add_point(graph, 4, 1, 5, 1);   /* p3 (4,5) */
    lv_ASSERT(graph_add_line_segment(graph, 0, 1) == ADD_NODE_OK);  /* 线段 ID 4 */
    lv_ASSERT(graph_add_line_segment(graph, 2, 3) == ADD_NODE_OK);  /* 线段 ID 5 */
    AddConstraintResult cr = graph_add_parallel(graph, 4, 5);
    lv_ASSERT(cr == ADD_CONSTRAINT_OK);
    Constraint *con = graph_get_constraint(graph, 0);
    lv_ASSERT_NOT_NULL(con);
    lv_ASSERT(con->type == PARALLEL);
    lv_ASSERT(con->participant_count == 2);
    lv_ASSERT(con->participants[0] == 4);
    lv_ASSERT(con->participants[1] == 5);

    /* to_json */
    char *json = NULL;
    lv_ASSERT(lv_constraint_to_json(con, &json));
    lv_ASSERT_NOT_NULL(json);
    lv_ASSERT(strstr(json, "PARALLEL") != NULL);
    lv_ASSERT(strstr(json, "participants") != NULL);

    /* from_json：round-trip 字段一致 */
    Constraint *parsed = NULL;
    lv_ASSERT(lv_constraint_from_json(json, &parsed));
    lv_ASSERT_NOT_NULL(parsed);
    lv_ASSERT(parsed->type == PARALLEL);
    lv_ASSERT(parsed->id == con->id);
    lv_ASSERT(parsed->participant_count == con->participant_count);
    lv_ASSERT(parsed->participants[0] == con->participants[0]);
    lv_ASSERT(parsed->participants[1] == con->participants[1]);
    lv_ASSERT(parsed->is_active);

    /* 参数校验（独立变量，避免覆盖 parsed） */
    Constraint *bad = NULL;
    lv_ASSERT(!lv_constraint_to_json(NULL, &json));
    lv_ASSERT(!lv_constraint_to_json(con, NULL));
    lv_ASSERT(!lv_constraint_from_json(NULL, &bad));
    lv_ASSERT(!lv_constraint_from_json("{bad", &bad));
    lv_ASSERT(!lv_constraint_from_json("{}", &bad));

    /* 清理：parsed 为堆分配单约束，participants 内部存储由 lv_free 一并释放 */
    void *parts = parsed->participants;
    lv_free(&parts);
    lv_free((void **) &parsed);
    lv_free_ptr(json);
    graph_destroy(graph);
    printf("蓝图单约束 JSON 测试通过!\n\n");
}

TEST_MAIN_BEGIN("约束图序列化与反序列化测试")
    printf("========================================\n");
    printf("约束图序列化与反序列化测试\n");
    printf("========================================\n\n");
    TEST_MAIN_RUN(test_point_serialization);
    TEST_MAIN_RUN(test_line_segment_serialization);
    TEST_MAIN_RUN(test_constraint_serialization);
    TEST_MAIN_RUN(test_module_graph_serialization);
    TEST_MAIN_RUN(test_advanced_node_roundtrip_and_deepcopy);
    TEST_MAIN_RUN(test_blueprint_constraint_json);
    printf("========================================\n");
    printf("所有序列化测试通过!\n");
    printf("========================================\n");
TEST_MAIN_END()
