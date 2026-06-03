/**
 * @file test_error_codes.c
 * @brief 错误码系统单元测试
 *
 * 测试 error_codes 模块的核心功能：
 * - 错误码到字符串的映射
 * - 错误码名称映射
 * - 线程局部错误状态设置与获取
 * - 二分查找的正确性
 * - 错误表排序自校验
 * - 错误码判断宏
 */

#include <stdio.h>
#include <string.h>

#include "error_codes.h"
#include "lv00.h"

/** 统计通过的测试数 */
static int g_passed = 0;
/** 统计失败的测试数 */
static int g_failed = 0;

#define TEST_ASSERT(cond, msg)                                       \
    do {                                                             \
        if (cond) {                                                  \
            g_passed++;                                              \
        } else {                                                     \
            g_failed++;                                              \
            printf("  失败 [%s:%d]: %s\n", __FILE__, __LINE__, msg); \
        }                                                            \
    } while (0)

#define TEST_ASSERT_EQ(a, b, msg)                                                                             \
    do {                                                                                                      \
        if ((a) == (b)) {                                                                                     \
            g_passed++;                                                                                       \
        } else {                                                                                              \
            g_failed++;                                                                                       \
            printf("  失败 [%s:%d]: %s (期望=%d, 实际=%d)\n", __FILE__, __LINE__, msg, (int) (b), (int) (a)); \
        }                                                                                                     \
    } while (0)

/* ============== 测试用例 ============== */

/** 测试错误码到错误信息的映射 */
static void test_error_strings(void) {
    printf("  [错误信息映射]\n");

    const char *msg;
    msg = lv00_error_string(LV00_OK);
    TEST_ASSERT(msg != NULL, "LV00_OK 的错误信息不应为 NULL");
    TEST_ASSERT(strlen(msg) > 0, "LV00_OK 的错误信息不应为空");

    msg = lv00_error_string(LV00_ERROR_OUT_OF_MEMORY);
    TEST_ASSERT(msg != NULL, "OUT_OF_MEMORY 错误信息不应为 NULL");

    msg = lv00_error_string(LV00_ERROR_UNKNOWN);
    TEST_ASSERT(msg != NULL, "UNKNOWN 错误信息不应为 NULL");

    /* 无效错误码应返回回退信息 */
    msg = lv00_error_string((Lv00ErrorCode) -1);
    TEST_ASSERT(msg != NULL, "无效错误码不应返回 NULL");
    TEST_ASSERT(strcmp(msg, "未知错误码") == 0, "无效错误码应返回'未知错误码'");
}

/** 测试错误码名称映射 */
static void test_error_names(void) {
    printf("  [错误码名称映射]\n");

    const char *name;
    name = lv00_error_name(LV00_OK);
    TEST_ASSERT(strcmp(name, "LV00_OK") == 0, "LV00_OK 名称应为 LV00_OK");

    name = lv00_error_name(LV00_ERROR_NULL_POINTER);
    TEST_ASSERT(strcmp(name, "LV00_ERROR_NULL_POINTER") == 0, "名称应为 LV00_ERROR_NULL_POINTER");

    name = lv00_error_name(LV00_ERROR_PROOF_VERIFICATION_FAILED);
    TEST_ASSERT(strcmp(name, "LV00_ERROR_PROOF_VERIFICATION_FAILED") == 0, "名称不匹配");

    /* 无效错误码 */
    name = lv00_error_name((Lv00ErrorCode) -999);
    TEST_ASSERT(name != NULL, "无效错误码名称不应为 NULL");
}

/** 测试错误状态设置与获取 */
static void test_error_state(void) {
    printf("  [错误状态管理]\n");

    /* 清除错误状态 */
    lv00_clear_error();
    TEST_ASSERT_EQ(lv00_get_last_error_code(), LV00_OK, "清除后应为 LV00_OK");

    /* 设置错误 */
    lv00_set_error(LV00_ERROR_NOT_FOUND, "测试错误: 节点未找到 id=%d", 42);
    TEST_ASSERT_EQ(lv00_get_last_error_code(), LV00_ERROR_NOT_FOUND, "错误码应为 NOT_FOUND");

    const char *err_msg = lv00_get_last_error_message();
    TEST_ASSERT(err_msg != NULL, "错误消息不应为 NULL");
    TEST_ASSERT(strstr(err_msg, "42") != NULL, "错误消息应包含格式化参数 42");

    /* 设置无消息的错误 */
    lv00_set_error(LV00_ERROR_TIMEOUT, NULL);
    TEST_ASSERT_EQ(lv00_get_last_error_code(), LV00_ERROR_TIMEOUT, "错误码应为 TIMEOUT");

    /* 清除后验证 */
    lv00_clear_error();
    TEST_ASSERT_EQ(lv00_get_last_error_code(), LV00_OK, "再次清除后应为 LV00_OK");
}

/** 测试错误判断宏 */
static void test_error_macros(void) {
    printf("  [错误判断宏]\n");

    lv00_clear_error();
    TEST_ASSERT(lv00_is_success(LV00_OK), "LV00_OK 应判定为成功");
    TEST_ASSERT(!lv00_is_error(LV00_OK), "LV00_OK 不应判定为错误");

    lv00_set_error(LV00_ERROR_INVALID_PARAM, "测试");
    TEST_ASSERT(!lv00_is_success(lv00_get_last_error_code()), "INVALID_PARAM 不应判定为成功");
    TEST_ASSERT(lv00_is_error(lv00_get_last_error_code()), "INVALID_PARAM 应判定为错误");
    lv00_clear_error();
}

/** 测试错误表排序自校验 */
static void test_error_table_validation(void) {
    printf("  [错误表排序自校验]\n");

    bool valid = lv00_error_table_validate();
    TEST_ASSERT(valid, "错误表排序应通过自校验");
    TEST_ASSERT_EQ(lv00_get_last_error_code(), LV00_OK, "通过校验后错误码应为 LV00_OK");
}

/** 测试错误码转换函数 */
static void test_error_code_conversion(void) {
    printf("  [错误码转换]\n");

    /* AddNodeResult 转换 */
    TEST_ASSERT_EQ(lv00_add_node_result_to_error(ADD_NODE_OK), LV00_OK, "ADD_NODE_OK 应转换为 LV00_OK");
    TEST_ASSERT_EQ(lv00_add_node_result_to_error(ADD_NODE_CONFLICT), LV00_ERROR_NODE_CONFLICT,
                   "ADD_NODE_CONFLICT 应转换为 LV00_ERROR_NODE_CONFLICT");
    TEST_ASSERT_EQ(lv00_add_node_result_to_error(ADD_NODE_INVALID_REGION), LV00_ERROR_INVALID_REGION,
                   "ADD_NODE_INVALID_REGION 应转换为 LV00_ERROR_INVALID_REGION");

    /* AddConstraintResult 转换 */
    TEST_ASSERT_EQ(lv00_add_constraint_result_to_error(ADD_CONSTRAINT_OK), LV00_OK,
                   "ADD_CONSTRAINT_OK 应转换为 LV00_OK");
    TEST_ASSERT_EQ(lv00_add_constraint_result_to_error(ADD_CONSTRAINT_DUPLICATE), LV00_ERROR_CONSTRAINT_DUPLICATE,
                   "ADD_CONSTRAINT_DUPLICATE 应转换正确");

    /* RemoveNodeResult 转换 */
    TEST_ASSERT_EQ(lv00_remove_node_result_to_error(REMOVE_NODE_OK), LV00_OK, "REMOVE_NODE_OK 应转换为 LV00_OK");
    TEST_ASSERT_EQ(lv00_remove_node_result_to_error(REMOVE_NODE_NOT_FOUND), LV00_ERROR_NODE_NOT_FOUND,
                   "REMOVE_NODE_NOT_FOUND 应转换正确");
}

/** 测试所有已定义错误码都有映射 */
static void test_all_codes_mapped(void) {
    printf("  [全覆盖检查]\n");

    /* 抽样检查各范围的错误码 */
    Lv00ErrorCode sample_codes[] = {LV00_OK,
                                    LV00_ERROR_UNKNOWN,
                                    LV00_ERROR_INVALID_PARAM,
                                    LV00_ERROR_NULL_POINTER,
                                    LV00_ERROR_NOT_INITIALIZED,
                                    LV00_ERROR_ALREADY_EXISTS,
                                    LV00_ERROR_NOT_FOUND,
                                    LV00_ERROR_UNSUPPORTED,
                                    LV00_ERROR_OVERFLOW,
                                    LV00_ERROR_UNDERFLOW,
                                    LV00_ERROR_TIMEOUT,
                                    LV00_ERROR_CANCELLED,
                                    LV00_ERROR_IO,
                                    LV00_ERROR_PARSE,
                                    LV00_ERROR_INVALID_STATE,
                                    LV00_ERROR_OUT_OF_MEMORY,
                                    LV00_ERROR_ALLOCATION_FAILED,
                                    LV00_ERROR_RESOURCE_EXHAUSTED,
                                    LV00_ERROR_BUFFER_TOO_SMALL,
                                    LV00_ERROR_NODE_CONFLICT,
                                    LV00_ERROR_NODE_NOT_FOUND,
                                    LV00_ERROR_CONSTRAINT_CONFLICT,
                                    LV00_ERROR_CONSTRAINT_DUPLICATE,
                                    LV00_ERROR_INVALID_REGION,
                                    LV00_ERROR_INVALID_GEOM_TYPE,
                                    LV00_ERROR_CYCLIC_DEPENDENCY,
                                    LV00_ERROR_GRAPH_CORRUPTED,
                                    LV00_ERROR_COORD_INVALID,
                                    LV00_ERROR_COORD_OVERFLOW,
                                    LV00_ERROR_PRECISION_LOSS,
                                    LV00_ERROR_SYMBOLIC_EVAL_FAILED,
                                    LV00_ERROR_SOLVER_NO_SOLUTION,
                                    LV00_ERROR_SOLVER_INFINITE,
                                    LV00_ERROR_SOLVER_NUMERIC,
                                    LV00_ERROR_SOLVER_SINGULAR,
                                    LV00_ERROR_SOLVER_NOT_CONVERGED,
                                    LV00_ERROR_GROEBNER_FAILED,
                                    LV00_ERROR_REWRITE_NO_MATCH,
                                    LV00_ERROR_REWRITE_CYCLE,
                                    LV00_ERROR_REWRITE_DEPTH,
                                    LV00_ERROR_UNIFY_FAILED,
                                    LV00_ERROR_UNIFY_OCCUR_CHECK,
                                    LV00_ERROR_UNIFY_TYPE_MISMATCH,
                                    LV00_ERROR_FUNC_BLOCK_INVALID,
                                    LV00_ERROR_FUNC_BLOCK_NON_DETERMINISTIC,
                                    LV00_ERROR_FUNC_BLOCK_CIRCULAR,
                                    LV00_ERROR_FUNC_BLOCK_TYPE_ERROR,
                                    LV00_ERROR_TYPE_MISMATCH,
                                    LV00_ERROR_TYPE_INFERENCE_FAILED,
                                    LV00_ERROR_UNIVERSE_INCONSISTENT,
                                    LV00_ERROR_PROOF_INVALID,
                                    LV00_ERROR_PROOF_INCOMPLETE,
                                    LV00_ERROR_PROOF_VERIFICATION_FAILED};

    int count = sizeof(sample_codes) / sizeof(sample_codes[0]);
    for (int i = 0; i < count; i++) {
        const char *msg = lv00_error_string(sample_codes[i]);
        const char *name = lv00_error_name(sample_codes[i]);

        if (!msg || strlen(msg) == 0) {
            printf("  失败: 错误码 %d 的错误信息为空\n", (int) sample_codes[i]);
            g_failed++;
        } else if (!name || strlen(name) == 0) {
            printf("  失败: 错误码 %d 的名称信息为空\n", (int) sample_codes[i]);
            g_failed++;
        } else {
            g_passed++;
        }
    }
    printf("    已检查 %d 个错误码\n", count);
}

/* ============== 入口 ============== */

int main(void) {
    printf("=== Lv-00 错误码系统测试 ===\n\n");

    g_passed = 0;
    g_failed = 0;

    test_error_strings();
    test_error_names();
    test_error_state();
    test_error_macros();
    test_error_table_validation();
    test_error_code_conversion();
    test_all_codes_mapped();

    printf("\n=== 测试结果: %d 通过, %d 失败 ===\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
