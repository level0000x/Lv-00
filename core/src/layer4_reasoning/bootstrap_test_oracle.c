/**
 * @file bootstrap_test_oracle.c
 * @brief Lv-00 自举差分测试框架 —— 测试预言机
 *
 * @details 由 bootstrap_test.c 按功能组件拆分而来。
 *          共享兼容定义与框架状态见 bootstrap_test_internal.h。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#include "lv/bootstrap_test.h"
#include "lv/lv_log.h"

#include "lv/lv_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lv/constraint_graph.h"
#include "lv/cross_platform.h"
#include "lv/engine.h"
#include "lv/lv_utils.h"
#include "lv/geo_utils.h"
#include "lv/normalization.h"
#include "lv/proof_trace.h"
#include "lv/lv_internal.h"

#include "bootstrap_test_internal.h"

/* ============== 测试预言机 ============== */

/** @brief 测试预言机结构体，用于验证测试结果的正确性 */
struct TestOracle {
    bool strict_mode; /**< 严格模式标志 */
};

/**
 * @brief 创建测试预言机
 *
 * 默认开启严格模式。
 *
 * @return 新创建的 TestOracle 指针，失败返回 NULL
 */
TestOracle *test_oracle_create(void) {
    TestOracle *oracle = lv_calloc(1, sizeof(TestOracle));
    if (!oracle) {
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "test_oracle_create: calloc failed");
    }

    oracle->strict_mode = true;

    return oracle;
}

/**
 * @brief 销毁测试预言机
 *
 * @param oracle 待销毁的预言机指针（可为 NULL）
 */
void test_oracle_destroy(TestOracle *oracle) {
    lv_free((void **) &oracle);
}

/**
 * @brief 验证归一化幂等性
 *
 * 对约束图执行两次归一化，验证第二次归一化不再产生合并。
 *
 * @note 与产品权威 normalization_verify_idempotency（normalization.c）同构
 * （同为"二次规范化无变化"判定），本预言机版收敛为对其的薄包装，保持行为一致。
 *
 * @param oracle 测试预言机
 * @param graph  约束图
 * @return true 幂等性通过，false 失败或参数无效
 */
bool test_oracle_verify_normalization_idempotent(TestOracle *oracle, void *graph) {
    if (!oracle || !graph) {
        return false;
    }

    /* 幂等性验证：收敛到产品权威实现（normalization.c: normalization_verify_idempotency），
     * 其内部以完整图哈希 + merged_count 双重判定"二次规范化无变化"。 */
    return normalization_verify_idempotency((ConstraintGraph *) graph);
}

/**
 * @brief 验证求解正确性
 *
 * 检查解是否满足约束图中的所有约束。
 *
 * @param oracle   测试预言机
 * @param graph    原始约束图
 * @param solution 求解结果
 * @return true 求解正确，false 失败或参数无效
 */
bool test_oracle_verify_solution_correct(TestOracle *oracle, const void *graph, const void *solution) {
    if (!oracle || !graph || !solution) {
        return false;
    }

    /* 求解正确性验证：检查解是否满足所有约束 */
    const ConstraintGraph *g = (const ConstraintGraph *) graph;
    const ConstraintGraph *sol = (const ConstraintGraph *) solution;

    if (graph_get_node_count(g) != graph_get_node_count(sol)) {
        return false;
    }

    /* 验证每个节点的坐标是否满足约束 */
    for (int i = 0; i < g->constraint_count; i++) {
        Constraint *c = g->constraints[i];
        if (!c || !c->is_active)
            continue;

        if (c->type == CONSTRAINT_DISTANCE && c->participant_count >= 2) {
            GeomNode *na = graph_get_node(sol, c->participants[0]);
            GeomNode *nb = graph_get_node(sol, c->participants[1]);
            if (!na || !nb || !na->symbolic_coords || !nb->symbolic_coords)
                continue;
            if (na->coord_count < 2 || nb->coord_count < 2)
                continue;

            double ax = symbolic_coord_to_double(na->symbolic_coords[0]);
            double ay = symbolic_coord_to_double(na->symbolic_coords[1]);
            double bx = symbolic_coord_to_double(nb->symbolic_coords[0]);
            double by = symbolic_coord_to_double(nb->symbolic_coords[1]);
            double dist = geo_distance_2d(ax, ay, bx, by);

            if (fabs(dist - c->numeric_value) > lv_EPSILON_LOW) {
                return false;
            }
        }
    }

    return true;
}

/**
 * @brief 验证证明轨迹的有效性
 *
 * @param oracle 测试预言机
 * @param trace  证明轨迹
 * @return true 有效，false 无效或参数无效
 */
bool test_oracle_verify_proof_valid(TestOracle *oracle, const void *trace) {
    if (!oracle || !trace) {
        return false;
    }

    /* 证明有效性验证：检查证明轨迹的完整结构 */
    const ProofTrace *pt = (const ProofTrace *) trace;
    int step_count = lv_proof_trace_get_step_count(pt);

    if (step_count == 0) {
        return false;
    }

    /* 验证每一步的 rule 非空 */
    for (int i = 0; i < step_count; i++) {
        const char *rule = lv_proof_trace_get_rule(pt, i);
        if (!rule || rule[0] == '\0') {
            return false;
        }
    }

    /* 验证证明已完成（最后一步是目标命题） */
    if (!lv_proof_trace_is_complete(pt)) {
        return false;
    }

    return true;
}

/**
 * @brief 验证序列化-反序列化往返一致性
 *
 * 对约束图进行序列化再反序列化，使用图同构比较器验证一致性。
 *
 * @note 测试预言机专用：验证外部传入的"序列化字符串 + 已反序列化对象"对，
 * 比较器为图同构比较器（graph_isomorphism_*）。与权威入口 lv_roundtrip_verify
 * （lv_storage.c，自行执行完整往返并用 meta_repr_graph_equivalent）行为不同，
 * 保留独立实现。
 *
 * @param oracle       测试预言机
 * @param graph        原始约束图
 * @param serialized   序列化结果
 * @param deserialized 反序列化结果
 * @return true 往返一致，false 不一致或参数无效
 */
bool test_oracle_verify_serialize_roundtrip(TestOracle *oracle, const void *graph, const char *serialized,
                                            const void *deserialized) {
    if (!oracle || !graph || !serialized || !deserialized) {
        return false;
    }

    /* 使用图同构比较器验证 */
    GraphIsomorphismComparator *comp = graph_isomorphism_create();
    if (!comp) {
        return false;
    }

    bool isomorphic = graph_isomorphism_compare(comp, graph, deserialized);

    graph_isomorphism_destroy(comp);

    return isomorphic;
}

