/**
 * @file test_graph_serialize.c
 * @brief 测试约束图的序列化与反序列化
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "lv00.h"

void test_point_serialization(void) {
    printf("=== 测试点序列化 ===\n");
    
    /* 创建图 */
    ConstraintGraph *graph = graph_create();
    assert(graph != NULL);
    
    /* 创建点 */
    SymbolicCoord *x = symbolic_coord_create_rational(1, 1);
    SymbolicCoord *y = symbolic_coord_create_rational(2, 1);
    SymbolicCoord *coords[2] = {x, y};
    
    AddNodeResult result = graph_add_point(graph, coords, 2);
    assert(result == ADD_NODE_OK);
    
    /* 序列化 */
    char *json = graph_serialize_to_json(graph);
    assert(json != NULL);
    printf("序列化结果:\n%s\n", json);
    
    /* 反序列化 */
    ConstraintGraph *restored = graph_deserialize_from_json(json);
    assert(restored != NULL);
    assert(graph_get_node_count(restored) == 1);
    
    /* 清理 */
    lv00_free_ptr(json);
    graph_destroy(restored);
    graph_destroy(graph);
    
    printf("点序列化测试通过!\n\n");
}

void test_line_segment_serialization(void) {
    printf("=== 测试线段序列化 ===\n");
    
    ConstraintGraph *graph = graph_create();
    assert(graph != NULL);
    
    /* 创建两个点 */
    SymbolicCoord *coords1[2] = {
        symbolic_coord_create_rational(0, 1),
        symbolic_coord_create_rational(0, 1)
    };
    SymbolicCoord *coords2[2] = {
        symbolic_coord_create_rational(3, 1),
        symbolic_coord_create_rational(4, 1)
    };
    
    graph_add_point(graph, coords1, 2);
    graph_add_point(graph, coords2, 2);
    
    /* 创建线段 */
    AddNodeResult result = graph_add_line_segment(graph, 0, 1);
    assert(result == ADD_NODE_OK);
    
    /* 序列化 */
    char *json = graph_serialize_to_json(graph);
    assert(json != NULL);
    printf("线段序列化结果:\n%s\n", json);
    
    /* 反序列化 */
    ConstraintGraph *restored = graph_deserialize_from_json(json);
    assert(restored != NULL);
    printf("恢复后节点数: %d\n", graph_get_node_count(restored));
    printf("恢复后约束数: %d\n", graph_get_constraint_count(restored));
    
    lv00_free_ptr(json);
    graph_destroy(restored);
    graph_destroy(graph);
    
    printf("线段序列化测试通过!\n\n");
}

void test_constraint_serialization(void) {
    printf("=== 测试约束序列化 ===\n");
    
    ConstraintGraph *graph = graph_create();
    assert(graph != NULL);
    
    /* 创建点 */
    SymbolicCoord *coords[2] = {
        symbolic_coord_create_rational(0, 1),
        symbolic_coord_create_rational(0, 1)
    };
    graph_add_point(graph, coords, 2);
    
    /* 创建线段 */
    SymbolicCoord *coords2[2] = {
        symbolic_coord_create_rational(1, 1),
        symbolic_coord_create_rational(1, 1)
    };
    SymbolicCoord *coords3[2] = {
        symbolic_coord_create_rational(2, 1),
        symbolic_coord_create_rational(2, 1)
    };
    graph_add_point(graph, coords2, 2);
    graph_add_point(graph, coords3, 2);
    graph_add_line_segment(graph, 1, 2);
    
    /* 添加关联约束 */
    AddConstraintResult cr = graph_add_incidence(graph, 0, 3);
    assert(cr == ADD_CONSTRAINT_OK);
    
    printf("原始节点数: %d\n", graph_get_node_count(graph));
    printf("原始约束数: %d\n", graph_get_constraint_count(graph));
    
    /* 序列化 */
    char *json = graph_serialize_to_json(graph);
    assert(json != NULL);
    printf("带约束的图序列化结果:\n%s\n", json);
    
    /* 反序列化 */
    ConstraintGraph *restored = graph_deserialize_from_json(json);
    assert(restored != NULL);
    printf("恢复后节点数: %d\n", graph_get_node_count(restored));
    printf("恢复后约束数: %d\n", graph_get_constraint_count(restored));
    
    lv00_free_ptr(json);
    graph_destroy(restored);
    graph_destroy(graph);
    
    printf("约束序列化测试通过!\n\n");
}

void test_module_graph_serialization(void) {
    printf("=== 测试模块图序列化 ===\n");
    
    Module *mod = module_create("TestModule", "1.0.0");
    assert(mod != NULL);
    
    /* 创建图 */
    module_set_graph(mod, graph_create());
    assert(module_get_graph(mod) != NULL);
    
    /* 添加节点 */
    SymbolicCoord *coords[2] = {
        symbolic_coord_create_rational(5, 1),
        symbolic_coord_create_rational(6, 1)
    };
    graph_add_point(module_get_graph(mod), coords, 2);
    
    printf("原始模块节点数: %d\n", graph_get_node_count(module_get_graph(mod)));
    
    /* 序列化整个模块 */
    char *json = module_serialize_to_json(mod);
    assert(json != NULL);
    printf("模块序列化结果:\n%s\n", json);
    
    /* 反序列化模块 */
    Module *restored_mod = NULL;
    ModuleLoadStatus status = module_deserialize_from_json(json, &restored_mod);
    printf("反序列化状态: %d\n", status);
    assert(status == MODULE_LOAD_OK);
    assert(restored_mod != NULL);
    printf("恢复后图指针: %p\n", (void*)module_get_graph(restored_mod));
    
    if (module_get_graph(restored_mod) != NULL) {
        printf("恢复后模块节点数: %d\n", graph_get_node_count(module_get_graph(restored_mod)));
    } else {
        printf("错误: 恢复后图为 NULL\n");
    }
    
    /* 单独序列化图 */
    char *graph_json = module_serialize_graph_to_json(mod);
    assert(graph_json != NULL);
    printf("独立图序列化:\n%s\n", graph_json);
    lv00_free_ptr(graph_json);
    
    lv00_free_ptr(json);
    module_destroy(restored_mod);
    module_destroy(mod);
    
    printf("模块图序列化测试通过!\n\n");
}

int main(void) {
    printf("========================================\n");
    printf("约束图序列化与反序列化测试\n");
    printf("========================================\n\n");
    
    test_point_serialization();
    test_line_segment_serialization();
    test_constraint_serialization();
    test_module_graph_serialization();
    
    printf("========================================\n");
    printf("所有序列化测试通过!\n");
    printf("========================================\n");
    
    return 0;
}
