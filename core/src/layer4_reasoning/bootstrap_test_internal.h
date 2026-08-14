/**
 * @file bootstrap_test_internal.h
 * @brief 自举测试框架内部共享声明
 *
 * @details 供 bootstrap_test_init.c / bootstrap_test_diff.c /
 *          bootstrap_test_random.c / bootstrap_test_primitive.c /
 *          bootstrap_test_oracle.c 等模块共享。
 *          包含兼容定义（graph_add_distance_constraint）与
 *          框架全局状态（s_test_state）的共享声明。
 */

#ifndef lv_BOOTSTRAP_TEST_INTERNAL_H
#define lv_BOOTSTRAP_TEST_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#include "lv/constraint_graph.h"
#include "lv/symbolic_coord.h"
#include "lv/config.h" /* lv_RATIONAL_SCALE_DEFAULT */

#ifdef __cplusplus
extern "C" {
#endif

/* ============== 兼容定义（原 bootstrap_test.c 顶部） ============== */

/** 距离约束类型兼容宏 */
#define CONSTRAINT_DISTANCE INCIDENCE

/**
 * @brief graph_add_distance_constraint 兼容 stub
 *
 * 创建辅助距离节点，通过有理符号坐标编码距离值，
 * 将距离约束降级为 containment + incidence 约束。
 *
 * @param g    约束图指针
 * @param a    端点 A ID
 * @param b    端点 B ID
 * @param dist 距离值
 * @return 添加结果
 */
static inline AddConstraintResult graph_add_distance_constraint(ConstraintGraph *g, int a, int b, double dist) {
    /* 创建辅助距离节点：symbolic_coords 编码距离值 */
    SymbolicCoord *dist_coord = symbolic_coord_create_rational((long long) (dist * lv_RATIONAL_SCALE_DEFAULT), lv_RATIONAL_SCALE_DEFAULT);
    SymbolicCoord *coords[1];
    coords[0] = dist_coord;
    graph_add_point(g, coords, 1);
    symbolic_coord_destroy(dist_coord);

    /* 获取辅助节点ID并将距离关联到端点a */
    int aux_id = graph_get_last_added_node_id(g);
    graph_add_containment(g, aux_id, a);

    /* 向后兼容：保留 incidence 约束 */
    return graph_add_incidence(g, a, b);
}

/* ============== 内部状态（bootstrap_test_init.c 定义） ============== */

/**
 * @brief 自举测试框架全局状态
 */
typedef struct BootstrapTestState {
    bool initialized;              /**< 框架是否已初始化 */
    uint64_t test_count;           /**< 总测试计数 */
    uint64_t pass_count;           /**< 通过测试计数 */
    uint64_t fail_count;           /**< 失败测试计数 */
} BootstrapTestState;

/** 模块级唯一状态实例（bootstrap_test_init.c 中定义） */
extern BootstrapTestState s_test_state;

#ifdef __cplusplus
}
#endif

#endif /* lv_BOOTSTRAP_TEST_INTERNAL_H */
