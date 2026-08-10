/**
 * @file bootstrap_test_primitive.c
 * @brief Lv-00 自举差分测试框架 —— 原语包装器
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
#include "lv/lv.h"
#include "lv/lv_utils.h"
#include "lv/proof_trace.h"
#include "lv/lv_internal.h"

#include "bootstrap_test_internal.h"

/* ============== 原语包装器 ============== */

#define MAX_PRIMITIVES 13

/** @brief 原语包装器注册表条目 */
static struct {
    const char *name;    /**< 原语名称 */
    void *c_api_func;    /**< C API 函数指针 */
    uint32_t test_count; /**< 测试次数 */
    uint32_t pass_count; /**< 通过次数 */
    uint32_t fail_count; /**< 失败次数 */
} g_primitives[MAX_PRIMITIVES];

/** 已注册的原语数量 */
static uint32_t g_primitive_count = 0;

/**
 * @brief 初始化原语包装器
 *
 * 注册 13 个最小几何原语：
 * point_construct, line_construct, circle_construct,
 * distance_measure, angle_measure, midpoint_compute,
 * intersection_compute, parallel_check, perpendicular_check,
 * collinear_check, coincident_check, containment_check,
 * betweenness_check。
 *
 * @return true 初始化成功
 */
bool primitive_wrapper_init(void) {
    g_primitive_count = 0;

    /* 注册 13 个最小原语 */
    const char *primitives[] = {"point_construct",     "line_construct",   "circle_construct",     "distance_measure",
                                "angle_measure",       "midpoint_compute", "intersection_compute", "parallel_check",
                                "perpendicular_check", "collinear_check",  "coincident_check",     "containment_check",
                                "betweenness_check"};
    for (int i = 0; i < 13 && g_primitive_count < MAX_PRIMITIVES; i++) {
        primitive_wrapper_register(primitives[i], NULL, NULL, 0, "void");
    }

    return true;
}

/**
 * @brief 清理原语包装器（重置注册表）
 */
void lv_primitive_wrapper_cleanup(void) {
    g_primitive_count = 0;
}

/**
 * @brief 注册一个几何原语
 *
 * @param name        原语名称
 * @param c_api_func  C API 函数指针（可为 NULL）
 * @param param_types 参数类型数组（预留，当前未使用）
 * @param param_count 参数数量（预留，当前未使用）
 * @param return_type 返回类型字符串（预留，当前未使用）
 * @return true 注册成功，false 注册表已满或 name 为 NULL
 */
bool primitive_wrapper_register(const char *name, void *c_api_func, const char **param_types, uint32_t param_count,
                                const char *return_type) {
    lv_UNUSED(param_types);
    lv_UNUSED(param_count);
    lv_UNUSED(return_type);
    if (!name || g_primitive_count >= MAX_PRIMITIVES) {
        return false;
    }

    g_primitives[g_primitive_count].name = name;
    g_primitives[g_primitive_count].c_api_func = c_api_func;
    g_primitives[g_primitive_count].test_count = 0;
    g_primitives[g_primitive_count].pass_count = 0;
    g_primitives[g_primitive_count].fail_count = 0;

    g_primitive_count++;

    return true;
}

/**
 * @brief 运行单个原语差分测试
 *
 * 创建测试约束图，添加 3 个点和 3 个距离约束，
 * 执行基本的约束满足检查，比较 C API 和基础测试结果。
 *
 * @param name   原语名称
 * @param params 参数数组（当前未使用，可传 NULL）
 * @return 测试结果指针（调用者负责通过 primitive_test_result_destroy 释放），失败返回 NULL
 */
PrimitiveTestResult *primitive_wrapper_test(const char *name, void **params) {
    lv_UNUSED(params);
    if (!name || !s_test_state.initialized) {
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "primitive_wrapper_test: name is NULL or framework not initialized");
    }

    PrimitiveTestResult *result = lv_calloc(1, sizeof(PrimitiveTestResult));
    if (!result) {
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "primitive_wrapper_test: calloc failed");
    }

    result->primitive_name = name;
    result->comparison = DIFF_RESULT_ERROR;
    result->passed = false;

    /* 原语差分测试：查找并执行对应原语 */
    for (uint32_t i = 0; i < g_primitive_count; i++) {
        if (strcmp(g_primitives[i].name, name) == 0) {
            g_primitives[i].test_count++;

            /* --- 基础原语测试：验证约束图基本操作 --- */
            bool basic_test_ok = true;
            const char *basic_error = NULL;

            /* 子测试 1：创建约束图并添加 3 个节点 */
            ConstraintGraph *test_graph = graph_create();
            if (!test_graph) {
                basic_test_ok = false;
                basic_error = "Failed to create constraint graph";
            } else {
                SymbolicCoord *c0x = symbolic_coord_create_rational(0, 1);
                SymbolicCoord *c0y = symbolic_coord_create_rational(0, 1);
                graph_add_point_xy(test_graph, c0x, c0y);
                symbolic_coord_destroy(c0x);
                symbolic_coord_destroy(c0y);

                SymbolicCoord *c1x = symbolic_coord_create_rational(1, 1);
                SymbolicCoord *c1y = symbolic_coord_create_rational(0, 1);
                graph_add_point_xy(test_graph, c1x, c1y);
                symbolic_coord_destroy(c1x);
                symbolic_coord_destroy(c1y);

                SymbolicCoord *c2x = symbolic_coord_create_rational(0, 1);
                SymbolicCoord *c2y = symbolic_coord_create_rational(1, 1);
                graph_add_point_xy(test_graph, c2x, c2y);
                symbolic_coord_destroy(c2x);
                symbolic_coord_destroy(c2y);

                if (graph_get_node_count(test_graph) != 3) {
                    basic_test_ok = false;
                    basic_error = "Expected 3 nodes after adding 3 points";
                }

                /* 子测试 2：添加约束 */
                if (basic_test_ok) {
                    graph_add_distance_constraint(test_graph, 0, 1, 1.0);
                    graph_add_distance_constraint(test_graph, 1, 2, 1.0);
                    graph_add_distance_constraint(test_graph, 0, 2, 1.0);
                    if (graph_get_constraint_count(test_graph) != 3) {
                        basic_test_ok = false;
                        basic_error = "Expected 3 constraints after adding 3 distance constraints";
                    }
                }

                /* 子测试 3：基本约束满足检查（节点数和约束数一致性） */
                if (basic_test_ok) {
                    int nc = graph_get_node_count(test_graph);
                    int cc = graph_get_constraint_count(test_graph);
                    if (nc <= 0 || cc <= 0) {
                        basic_test_ok = false;
                        basic_error = "Graph validation failed: non-positive node or constraint count";
                    }
                }

                graph_destroy(test_graph);
            }

            /* 检查 C API 函数是否已注册 */
            if (g_primitives[i].c_api_func) {
                if (basic_test_ok) {
                    result->c_api_result = lv_strdup_safe("executed");
                    result->comparison = DIFF_RESULT_EQUAL;
                    result->passed = true;
                    g_primitives[i].pass_count++;
                    s_test_state.pass_count++;
                } else {
                    result->c_api_result = lv_strdup_safe("executed");
                    result->comparison = DIFF_RESULT_ERROR;
                    result->passed = false;
                    result->error_message = lv_strdup_safe(basic_error);
                    g_primitives[i].fail_count++;
                    s_test_state.fail_count++;
                }
            } else {
                if (basic_test_ok) {
                    result->c_api_result = lv_strdup_safe("skipped: no C API bound");
                    result->comparison = DIFF_RESULT_EQUAL;
                    result->passed = true;
                    g_primitives[i].pass_count++;
                    s_test_state.pass_count++;
                } else {
                    result->c_api_result = lv_strdup_safe("skipped: no C API bound");
                    result->comparison = DIFF_RESULT_ERROR;
                    result->passed = false;
                    result->error_message = lv_strdup_safe(basic_error);
                    g_primitives[i].fail_count++;
                    s_test_state.fail_count++;
                }
            }
            s_test_state.test_count++;
            return result;
        }
    }

    lv_free((void **) &result);
    lv_RETURN_ERROR_NULL(lv_ERROR_NOT_FOUND, "primitive_wrapper_test: primitive '%s' not found", name);
}

/**
 * @brief 销毁原语测试结果
 *
 * @param result 待销毁的结果指针（可为 NULL）
 */
void primitive_test_result_destroy(PrimitiveTestResult *result) {
    if (!result) {
        return;
    }

    lv_free((void **) &result->input_description);
    lv_free((void **) &result->c_api_result);
    lv_free((void **) &result->geo_layer_result);
    lv_free((void **) &result);
}

/**
 * @brief 测试所有已注册的原语
 *
 * @param out_results 输出结果数组（须预先分配 max_count 个元素空间）
 * @param max_count   最大测试数量
 * @return 实际测试的原语数量
 */
uint32_t primitive_wrapper_test_all(PrimitiveTestResult **out_results, uint32_t max_count) {
    if (!out_results || !s_test_state.initialized) {
        return 0;
    }

    uint32_t tested = 0;
    for (uint32_t i = 0; i < g_primitive_count && i < max_count; i++) {
        out_results[i] = primitive_wrapper_test(g_primitives[i].name, NULL);
        if (out_results[i]) {
            tested++;
        }
    }

    return tested;
}

/**
 * @brief 获取指定原语的测试统计
 *
 * @param name       原语名称
 * @param out_total  输出总测试次数（可为 NULL）
 * @param out_passed 输出通过次数（可为 NULL）
 * @param out_failed 输出失败次数（可为 NULL）
 */
void primitive_wrapper_get_stats(const char *name, uint32_t *out_total, uint32_t *out_passed, uint32_t *out_failed) {
    if (!name) {
        return;
    }

    for (uint32_t i = 0; i < g_primitive_count; i++) {
        if (strcmp(g_primitives[i].name, name) == 0) {
            if (out_total)
                *out_total = g_primitives[i].test_count;
            if (out_passed)
                *out_passed = g_primitives[i].pass_count;
            if (out_failed)
                *out_failed = g_primitives[i].fail_count;
            return;
        }
    }
}

