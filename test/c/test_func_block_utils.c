/**
 * @file test_func_block_utils.c
 * @brief func_block_utils 模块测试 - 整数数组工具函数
 *
 * 测试 func_block_utils.h 中定义的工具函数：
 * - is_id_in_array: ID 存在性检查
 * - dup_int_array: 整数数组深拷贝
 * - merge_int_arrays: 整数数组合并
 *
 * 这些函数是函数块系统的核心工具，被 func_block.c 内部使用。
 * 独立测试确保工具函数本身的正确性。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

#include "func_block_utils.h"
#include "lv.h"

static void test_is_id_in_array_basic(void) {
    printf("Test: is_id_in_array basic...\n");

    int arr[] = {10, 20, 30, 40, 50};

    lv_ASSERT(is_id_in_array(10, arr, 5) == true);
    lv_ASSERT(is_id_in_array(30, arr, 5) == true);
    lv_ASSERT(is_id_in_array(50, arr, 5) == true);

    lv_ASSERT(is_id_in_array(15, arr, 5) == false);
    lv_ASSERT(is_id_in_array(60, arr, 5) == false);
    lv_ASSERT(is_id_in_array(0, arr, 5) == false);

    printf("  PASSED\n");

}

static void test_is_id_in_array_edge_cases(void) {
    printf("Test: is_id_in_array edge cases...\n");

    lv_ASSERT(is_id_in_array(42, NULL, 0) == false);
    lv_ASSERT(is_id_in_array(42, NULL, 5) == false);

    int empty_arr[1] = {0};
    lv_ASSERT(is_id_in_array(42, empty_arr, 0) == false);

    int single[] = {100};
    lv_ASSERT(is_id_in_array(100, single, 1) == true);
    lv_ASSERT(is_id_in_array(200, single, 1) == false);

    int large_arr[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
    lv_ASSERT(is_id_in_array(20, large_arr, 20) == true);
    lv_ASSERT(is_id_in_array(21, large_arr, 20) == false);

    printf("  PASSED\n");

}

static void test_dup_int_array_basic(void) {
    printf("Test: dup_int_array basic...\n");

    int src[] = {10, 20, 30, 40, 50};
    int *dst = dup_int_array(src, 5);

    lv_ASSERT_NOT_NULL(dst);
    lv_ASSERT(dst[0] == 10);
    lv_ASSERT(dst[1] == 20);
    lv_ASSERT(dst[2] == 30);
    lv_ASSERT(dst[3] == 40);
    lv_ASSERT(dst[4] == 50);

    memcpy(dst, src, 5 * sizeof(int));
    lv_ASSERT(dst[2] == 30);

    lv_free_ptr(dst);
    printf("  PASSED\n");

}

static void test_dup_int_array_edge_cases(void) {
    printf("Test: dup_int_array edge cases...\n");

    int src[] = {1, 2, 3};

    lv_ASSERT(dup_int_array(NULL, 5) == NULL);
    lv_ASSERT(dup_int_array(NULL, 0) == NULL);
    lv_ASSERT(dup_int_array(src, 0) == NULL);
    lv_ASSERT(dup_int_array(src, -1) == NULL);

    int single[] = {42};
    int *single_copy = dup_int_array(src, 1);
    lv_ASSERT_NOT_NULL(single_copy);
    lv_ASSERT(single_copy[0] == 1);
    lv_free_ptr(single_copy);

    int *empty_copy = dup_int_array(src, 0);
    lv_ASSERT(empty_copy == NULL);

    printf("  PASSED\n");

}

static void test_merge_int_arrays_basic(void) {
    printf("Test: merge_int_arrays basic...\n");

    int a[] = {1, 2, 3};
    int b[] = {4, 5, 6};
    int count = 0;

    int *result = merge_int_arrays(a, 3, b, 3, &count);

    lv_ASSERT_NOT_NULL(result);
    lv_ASSERT(count == 6);
    lv_ASSERT(result[0] == 1);
    lv_ASSERT(result[1] == 2);
    lv_ASSERT(result[2] == 3);
    lv_ASSERT(result[3] == 4);
    lv_ASSERT(result[4] == 5);
    lv_ASSERT(result[5] == 6);

    lv_free_ptr(result);
    printf("  PASSED\n");

}

static void test_merge_int_arrays_edge_cases(void) {
    printf("Test: merge_int_arrays edge cases...\n");

    int a[] = {1, 2, 3};
    int b[] = {4, 5, 6};
    int count = 0;

    lv_ASSERT(merge_int_arrays(NULL, 0, NULL, 0, &count) == NULL);

    int *r1 = merge_int_arrays(a, 3, b, 3, NULL);
    lv_ASSERT(r1 == NULL);

    int *r2 = merge_int_arrays(a, 0, b, 0, &count);
    lv_ASSERT(r2 == NULL);
    lv_ASSERT(count == 0);

    /* 修复：NULL数组 + 0元素 = 空输入，应返回仅包含b元素的副本 */
    int *r3 = merge_int_arrays(NULL, 0, b, 3, &count);
    lv_ASSERT_NOT_NULL(r3);
    lv_ASSERT(count == 3);
    lv_ASSERT(r3[0] == 4);
    lv_ASSERT(r3[1] == 5);
    lv_ASSERT(r3[2] == 6);
    lv_free_ptr(r3);

    int *r4 = merge_int_arrays(a, 3, NULL, 0, &count);
    lv_ASSERT_NOT_NULL(r4);
    lv_ASSERT(count == 3);
    lv_ASSERT(r4[0] == 1);
    lv_ASSERT(r4[1] == 2);
    lv_ASSERT(r4[2] == 3);
    lv_free_ptr(r4);

    int *r5 = merge_int_arrays(NULL, 0, b, 3, &count);
    lv_ASSERT_NOT_NULL(r5);
    lv_ASSERT(count == 3);
    lv_ASSERT(r5[0] == 4);
    lv_ASSERT(r5[1] == 5);
    lv_ASSERT(r5[2] == 6);
    lv_free_ptr(r5);

    int *r6 = merge_int_arrays(a, 3, b, 3, &count);
    lv_ASSERT_NOT_NULL(r6);
    lv_ASSERT(count == 6);

    memcpy(r6, a, 3 * sizeof(int));
    memcpy(r6 + 3, b, 3 * sizeof(int));
    lv_ASSERT(r6[5] == 6);
    lv_free_ptr(r6);

    printf("  PASSED\n");

}

static void test_memory_independence(void) {
    printf("Test: memory independence...\n");

    int src[] = {10, 20, 30};
    int *dst = dup_int_array(src, 3);

    lv_ASSERT_NOT_NULL(dst);

    dst[0] = 999;
    lv_ASSERT(src[0] == 10);

    lv_free_ptr(dst);

    int a[] = {1, 2};
    int b[] = {3, 4};
    int count = 0;
    int *merged = merge_int_arrays(a, 2, b, 2, &count);

    lv_ASSERT_NOT_NULL(merged);

    merged[0] = 888;
    lv_ASSERT(a[0] == 1);
    lv_ASSERT(b[0] == 3);

    lv_free_ptr(merged);

    printf("  PASSED\n");

}

static void test_large_arrays(void) {
    printf("Test: large arrays...\n");

    const int SIZE = 10000;
    int *large = malloc(SIZE * sizeof(int));
    lv_ASSERT_NOT_NULL(large);

    for (int i = 0; i < SIZE; i++) {
        large[i] = i * 2;
    }

    int *copy = dup_int_array(large, SIZE);
    lv_ASSERT_NOT_NULL(copy);

    for (int i = 0; i < SIZE; i++) {
        lv_ASSERT(copy[i] == i * 2);
    }

    lv_free_ptr(copy);

    int *half1 = malloc((SIZE / 2) * sizeof(int));
    int *half2 = malloc((SIZE / 2) * sizeof(int));
    lv_ASSERT(half1 != NULL && half2 != NULL);

    for (int i = 0; i < SIZE / 2; i++) {
        half1[i] = i;
        half2[i] = SIZE / 2 + i;
    }

    int merged_count = 0;
    int *merged = merge_int_arrays(half1, SIZE / 2, half2, SIZE / 2, &merged_count);
    lv_ASSERT_NOT_NULL(merged);
    lv_ASSERT(merged_count == SIZE);

    for (int i = 0; i < SIZE; i++) {
        lv_ASSERT(merged[i] == i);
    }

    lv_free_ptr(merged);
    lv_free_ptr(half1);
    lv_free_ptr(half2);
    lv_free_ptr(large);

    printf("  PASSED\n");

}

TEST_MAIN_BEGIN("Lv-00 func_block_utils Test Suite")
    printf("=== Lv-00 func_block_utils Test Suite ===\n\n");
    TEST_MAIN_RUN(test_is_id_in_array_basic);
    TEST_MAIN_RUN(test_is_id_in_array_edge_cases);
    TEST_MAIN_RUN(test_dup_int_array_basic);
    TEST_MAIN_RUN(test_dup_int_array_edge_cases);
    TEST_MAIN_RUN(test_merge_int_arrays_basic);
    TEST_MAIN_RUN(test_merge_int_arrays_edge_cases);
    TEST_MAIN_RUN(test_memory_independence);
    TEST_MAIN_RUN(test_large_arrays);
    printf("\n=== All func_block_utils tests PASSED! ===\n");
TEST_MAIN_END()
