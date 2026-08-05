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
#include "lv.h"
#include "test_helpers.h"

/** 全局测试计数器（test_helpers.h 声明 extern） */
int g_pass_count = 0;
int g_fail_count = 0;

/* 本文件断言语义为"失败继续"（单测函数内连续多条断言，失败后仍继续执行后续断言），
 * 统一委托公共宏 TEST_ASSERT_CONTINUE / TEST_ASSERT_EQ_CONTINUE（先取消公共定义避免重定义告警） */
#undef TEST_ASSERT
#define TEST_ASSERT(cond, msg) TEST_ASSERT_CONTINUE(cond, msg)
#undef TEST_ASSERT_EQ
#define TEST_ASSERT_EQ(a, b) TEST_ASSERT_EQ_CONTINUE(a, b)

/* ============== 测试用例 ============== */

/** 测试错误码到错误信息的映射 */
static void test_error_strings(void) {
    printf("  [错误信息映射]\n");

    const char *msg;
    msg = lv_error_string(lv_OK);
    TEST_ASSERT(msg != NULL, "lv_OK 的错误信息不应为 NULL");
    TEST_ASSERT(strlen(msg) > 0, "lv_OK 的错误信息不应为空");

    msg = lv_error_string(lv_ERROR_OUT_OF_MEMORY);
    TEST_ASSERT(msg != NULL, "OUT_OF_MEMORY 错误信息不应为 NULL");

    msg = lv_error_string(lv_ERROR_UNKNOWN);
    TEST_ASSERT(msg != NULL, "UNKNOWN 错误信息不应为 NULL");

    /* 无效错误码应返回回退信息 */
    msg = lv_error_string((lvErrorCode) -1);
    TEST_ASSERT(msg != NULL, "无效错误码不应返回 NULL");
    TEST_ASSERT(strcmp(msg, "未知错误码") == 0, "无效错误码应返回'未知错误码'");
}

/** 测试错误码名称映射 */
static void test_error_names(void) {
    printf("  [错误码名称映射]\n");

    const char *name;
    name = lv_error_name(lv_OK);
    TEST_ASSERT(strcmp(name, "lv_OK") == 0, "lv_OK 名称应为 lv_OK");

    name = lv_error_name(lv_ERROR_NULL_POINTER);
    TEST_ASSERT(strcmp(name, "lv_ERROR_NULL_POINTER") == 0, "名称应为 lv_ERROR_NULL_POINTER");

    name = lv_error_name(lv_ERROR_PROOF_VERIFICATION_FAILED);
    TEST_ASSERT(strcmp(name, "lv_ERROR_PROOF_VERIFICATION_FAILED") == 0, "名称不匹配");

    /* 无效错误码 */
    name = lv_error_name((lvErrorCode) -999);
    TEST_ASSERT(name != NULL, "无效错误码名称不应为 NULL");
}

/** 测试错误状态设置与获取 */
static void test_error_state(void) {
    printf("  [错误状态管理]\n");

    /* 清除错误状态 */
    lv_clear_error();
    TEST_ASSERT_EQ(lv_get_last_error_code(), lv_OK);

    /* 设置错误 */
    lv_set_error(lv_ERROR_NOT_FOUND, "测试错误: 节点未找到 id=%d", 42);
    TEST_ASSERT_EQ(lv_get_last_error_code(), lv_ERROR_NOT_FOUND);

    const char *err_msg = lv_get_last_error_message();
    TEST_ASSERT(err_msg != NULL, "错误消息不应为 NULL");
    TEST_ASSERT(strstr(err_msg, "42") != NULL, "错误消息应包含格式化参数 42");

    /* 设置无消息的错误 */
    lv_set_error(lv_ERROR_TIMEOUT, NULL);
    TEST_ASSERT_EQ(lv_get_last_error_code(), lv_ERROR_TIMEOUT);

    /* 清除后验证 */
    lv_clear_error();
    TEST_ASSERT_EQ(lv_get_last_error_code(), lv_OK);
}

/** 测试错误判断宏 */
static void test_error_macros(void) {
    printf("  [错误判断宏]\n");

    lv_clear_error();
    TEST_ASSERT(lv_is_success(lv_OK), "lv_OK 应判定为成功");
    TEST_ASSERT(!lv_is_error(lv_OK), "lv_OK 不应判定为错误");

    lv_set_error(lv_ERROR_INVALID_PARAM, "测试");
    TEST_ASSERT(!lv_is_success(lv_get_last_error_code()), "INVALID_PARAM 不应判定为成功");
    TEST_ASSERT(lv_is_error(lv_get_last_error_code()), "INVALID_PARAM 应判定为错误");
    lv_clear_error();
}

/** 测试错误表排序自校验 */
static void test_error_table_validation(void) {
    printf("  [错误表排序自校验]\n");

    bool valid = lv_error_table_validate();
    TEST_ASSERT(valid, "错误表排序应通过自校验");
    TEST_ASSERT_EQ(lv_get_last_error_code(), lv_OK);
}

/** 测试错误码转换函数 */
static void test_error_code_conversion(void) {
    printf("  [错误码转换]\n");

    /* AddNodeResult 转换 */
    TEST_ASSERT_EQ(lv_add_node_result_to_error(ADD_NODE_OK), lv_OK);
    TEST_ASSERT_EQ(lv_add_node_result_to_error(ADD_NODE_CONFLICT), lv_ERROR_NODE_CONFLICT);
    TEST_ASSERT_EQ(lv_add_node_result_to_error(ADD_NODE_INVALID_REGION), lv_ERROR_INVALID_REGION);

    /* AddConstraintResult 转换 */
    TEST_ASSERT_EQ(lv_add_constraint_result_to_error(ADD_CONSTRAINT_OK), lv_OK);
    TEST_ASSERT_EQ(lv_add_constraint_result_to_error(ADD_CONSTRAINT_DUPLICATE), lv_ERROR_CONSTRAINT_DUPLICATE);

    /* RemoveNodeResult 转换 */
    TEST_ASSERT_EQ(lv_remove_node_result_to_error(REMOVE_NODE_OK), lv_OK);
    TEST_ASSERT_EQ(lv_remove_node_result_to_error(REMOVE_NODE_NOT_FOUND), lv_ERROR_NODE_NOT_FOUND);
}

/** 测试所有已定义错误码都有映射 */
static void test_all_codes_mapped(void) {
    printf("  [全覆盖检查]\n");

    /* 抽样检查各范围的错误码 */
    lvErrorCode sample_codes[] = {lv_OK,
                                  lv_ERROR_UNKNOWN,
                                  lv_ERROR_INVALID_PARAM,
                                  lv_ERROR_NULL_POINTER,
                                  lv_ERROR_NOT_INITIALIZED,
                                  lv_ERROR_ALREADY_EXISTS,
                                  lv_ERROR_NOT_FOUND,
                                  lv_ERROR_UNSUPPORTED,
                                  lv_ERROR_OVERFLOW,
                                  lv_ERROR_UNDERFLOW,
                                  lv_ERROR_TIMEOUT,
                                  lv_ERROR_CANCELLED,
                                  lv_ERROR_IO,
                                  lv_ERROR_PARSE,
                                  lv_ERROR_INVALID_STATE,
                                  lv_ERROR_OUT_OF_MEMORY,
                                  lv_ERROR_ALLOCATION_FAILED,
                                  lv_ERROR_RESOURCE_EXHAUSTED,
                                  lv_ERROR_BUFFER_TOO_SMALL,
                                  lv_ERROR_NODE_CONFLICT,
                                  lv_ERROR_NODE_NOT_FOUND,
                                  lv_ERROR_CONSTRAINT_CONFLICT,
                                  lv_ERROR_CONSTRAINT_DUPLICATE,
                                  lv_ERROR_INVALID_REGION,
                                  lv_ERROR_INVALID_GEOM_TYPE,
                                  lv_ERROR_CYCLIC_DEPENDENCY,
                                  lv_ERROR_GRAPH_CORRUPTED,
                                  lv_ERROR_COORD_INVALID,
                                  lv_ERROR_COORD_OVERFLOW,
                                  lv_ERROR_PRECISION_LOSS,
                                  lv_ERROR_SYMBOLIC_EVAL_FAILED,
                                  lv_ERROR_SOLVER_NO_SOLUTION,
                                  lv_ERROR_SOLVER_INFINITE,
                                  lv_ERROR_SOLVER_NUMERIC,
                                  lv_ERROR_SOLVER_SINGULAR,
                                  lv_ERROR_SOLVER_NOT_CONVERGED,
                                  lv_ERROR_GROEBNER_FAILED,
                                  lv_ERROR_REWRITE_NO_MATCH,
                                  lv_ERROR_REWRITE_CYCLE,
                                  lv_ERROR_REWRITE_DEPTH,
                                  lv_ERROR_UNIFY_FAILED,
                                  lv_ERROR_UNIFY_OCCUR_CHECK,
                                  lv_ERROR_UNIFY_TYPE_MISMATCH,
                                  lv_ERROR_FUNC_BLOCK_INVALID,
                                  lv_ERROR_FUNC_BLOCK_NON_DETERMINISTIC,
                                  lv_ERROR_FUNC_BLOCK_CIRCULAR,
                                  lv_ERROR_FUNC_BLOCK_TYPE_ERROR,
                                  lv_ERROR_TYPE_MISMATCH,
                                  lv_ERROR_TYPE_INFERENCE_FAILED,
                                  lv_ERROR_UNIVERSE_INCONSISTENT,
                                  lv_ERROR_PROOF_INVALID,
                                  lv_ERROR_PROOF_INCOMPLETE,
                                  lv_ERROR_PROOF_VERIFICATION_FAILED};

    int count = sizeof(sample_codes) / sizeof(sample_codes[0]);
    for (int i = 0; i < count; i++) {
        const char *msg = lv_error_string(sample_codes[i]);
        const char *name = lv_error_name(sample_codes[i]);

        if (!msg || strlen(msg) == 0) {
            printf("  失败: 错误码 %d 的错误信息为空\n", (int) sample_codes[i]);
            g_fail_count++;
        } else if (!name || strlen(name) == 0) {
            printf("  失败: 错误码 %d 的名称信息为空\n", (int) sample_codes[i]);
            g_fail_count++;
        } else {
            g_pass_count++;
        }
    }
    printf("    已检查 %d 个错误码\n", count);
}

/* ============== 入口 ============== */

int main(void) {
    printf("=== Lv-00 错误码系统测试 ===\n\n");

    test_error_strings();
    test_error_names();
    test_error_state();
    test_error_macros();
    test_error_table_validation();
    test_error_code_conversion();
    test_all_codes_mapped();

    printf("\n=== 测试结果: %d 通过, %d 失败 ===\n", g_pass_count, g_fail_count);
    return g_fail_count > 0 ? 1 : 0;
}
