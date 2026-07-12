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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "func_block_utils.h"
#include "lv00.h"

static int test_is_id_in_array_basic(void) {
    printf("Test: is_id_in_array basic...\n");

    int arr[] = {10, 20, 30, 40, 50};

    assert(is_id_in_array(10, arr, 5) == true);
    assert(is_id_in_array(30, arr, 5) == true);
    assert(is_id_in_array(50, arr, 5) == true);

    assert(is_id_in_array(15, arr, 5) == false);
    assert(is_id_in_array(60, arr, 5) == false);
    assert(is_id_in_array(0, arr, 5) == false);

    printf("  PASSED\n");
    return 0;
}

static int test_is_id_in_array_edge_cases(void) {
    printf("Test: is_id_in_array edge cases...\n");

    assert(is_id_in_array(42, NULL, 0) == false);
    assert(is_id_in_array(42, NULL, 5) == false);

    int empty_arr[1] = {0};
    assert(is_id_in_array(42, empty_arr, 0) == false);

    int single[] = {100};
    assert(is_id_in_array(100, single, 1) == true);
    assert(is_id_in_array(200, single, 1) == false);

    int large_arr[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
    assert(is_id_in_array(20, large_arr, 20) == true);
    assert(is_id_in_array(21, large_arr, 20) == false);

    printf("  PASSED\n");
    return 0;
}

static int test_dup_int_array_basic(void) {
    printf("Test: dup_int_array basic...\n");

    int src[] = {10, 20, 30, 40, 50};
    int *dst = dup_int_array(src, 5);

    assert(dst != NULL);
    assert(dst[0] == 10);
    assert(dst[1] == 20);
    assert(dst[2] == 30);
    assert(dst[3] == 40);
    assert(dst[4] == 50);

    memcpy(dst, src, 5 * sizeof(int));
    assert(dst[2] == 30);

    lv00_free_ptr(dst);
    printf("  PASSED\n");
    return 0;
}

static int test_dup_int_array_edge_cases(void) {
    printf("Test: dup_int_array edge cases...\n");

    int src[] = {1, 2, 3};

    assert(dup_int_array(NULL, 5) == NULL);
    assert(dup_int_array(NULL, 0) == NULL);
    assert(dup_int_array(src, 0) == NULL);
    assert(dup_int_array(src, -1) == NULL);

    int single[] = {42};
    int *single_copy = dup_int_array(src, 1);
    assert(single_copy != NULL);
    assert(single_copy[0] == 1);
    lv00_free_ptr(single_copy);

    int *empty_copy = dup_int_array(src, 0);
    assert(empty_copy == NULL);

    printf("  PASSED\n");
    return 0;
}

static int test_merge_int_arrays_basic(void) {
    printf("Test: merge_int_arrays basic...\n");

    int a[] = {1, 2, 3};
    int b[] = {4, 5, 6};
    int count = 0;

    int *result = merge_int_arrays(a, 3, b, 3, &count);

    assert(result != NULL);
    assert(count == 6);
    assert(result[0] == 1);
    assert(result[1] == 2);
    assert(result[2] == 3);
    assert(result[3] == 4);
    assert(result[4] == 5);
    assert(result[5] == 6);

    lv00_free_ptr(result);
    printf("  PASSED\n");
    return 0;
}

static int test_merge_int_arrays_edge_cases(void) {
    printf("Test: merge_int_arrays edge cases...\n");

    int a[] = {1, 2, 3};
    int b[] = {4, 5, 6};
    int count = 0;

    assert(merge_int_arrays(NULL, 0, NULL, 0, &count) == NULL);

    int *r1 = merge_int_arrays(a, 3, b, 3, NULL);
    assert(r1 == NULL);

    int *r2 = merge_int_arrays(a, 0, b, 0, &count);
    assert(r2 == NULL);
    assert(count == 0);

    /* 修复：NULL数组 + 0元素 = 空输入，应返回仅包含b元素的副本 */
    int *r3 = merge_int_arrays(NULL, 0, b, 3, &count);
    assert(r3 != NULL);
    assert(count == 3);
    assert(r3[0] == 4);
    assert(r3[1] == 5);
    assert(r3[2] == 6);
    lv00_free_ptr(r3);

    int *r4 = merge_int_arrays(a, 3, NULL, 0, &count);
    assert(r4 != NULL);
    assert(count == 3);
    assert(r4[0] == 1);
    assert(r4[1] == 2);
    assert(r4[2] == 3);
    lv00_free_ptr(r4);

    int *r5 = merge_int_arrays(NULL, 0, b, 3, &count);
    assert(r5 != NULL);
    assert(count == 3);
    assert(r5[0] == 4);
    assert(r5[1] == 5);
    assert(r5[2] == 6);
    lv00_free_ptr(r5);

    int *r6 = merge_int_arrays(a, 3, b, 3, &count);
    assert(r6 != NULL);
    assert(count == 6);

    memcpy(r6, a, 3 * sizeof(int));
    memcpy(r6 + 3, b, 3 * sizeof(int));
    assert(r6[5] == 6);
    lv00_free_ptr(r6);

    printf("  PASSED\n");
    return 0;
}

static int test_memory_independence(void) {
    printf("Test: memory independence...\n");

    int src[] = {10, 20, 30};
    int *dst = dup_int_array(src, 3);

    assert(dst != NULL);

    dst[0] = 999;
    assert(src[0] == 10);

    lv00_free_ptr(dst);

    int a[] = {1, 2};
    int b[] = {3, 4};
    int count = 0;
    int *merged = merge_int_arrays(a, 2, b, 2, &count);

    assert(merged != NULL);

    merged[0] = 888;
    assert(a[0] == 1);
    assert(b[0] == 3);

    lv00_free_ptr(merged);

    printf("  PASSED\n");
    return 0;
}

static int test_large_arrays(void) {
    printf("Test: large arrays...\n");

    const int SIZE = 10000;
    int *large = malloc(SIZE * sizeof(int));
    assert(large != NULL);

    for (int i = 0; i < SIZE; i++) {
        large[i] = i * 2;
    }

    int *copy = dup_int_array(large, SIZE);
    assert(copy != NULL);

    for (int i = 0; i < SIZE; i++) {
        assert(copy[i] == i * 2);
    }

    lv00_free_ptr(copy);

    int *half1 = malloc((SIZE / 2) * sizeof(int));
    int *half2 = malloc((SIZE / 2) * sizeof(int));
    assert(half1 != NULL && half2 != NULL);

    for (int i = 0; i < SIZE / 2; i++) {
        half1[i] = i;
        half2[i] = SIZE / 2 + i;
    }

    int merged_count = 0;
    int *merged = merge_int_arrays(half1, SIZE / 2, half2, SIZE / 2, &merged_count);
    assert(merged != NULL);
    assert(merged_count == SIZE);

    for (int i = 0; i < SIZE; i++) {
        assert(merged[i] == i);
    }

    lv00_free_ptr(merged);
    lv00_free_ptr(half1);
    lv00_free_ptr(half2);
    lv00_free_ptr(large);

    printf("  PASSED\n");
    return 0;
}

int main(void) {
    printf("=== Lv-00 func_block_utils Test Suite ===\n\n");

    test_is_id_in_array_basic();
    test_is_id_in_array_edge_cases();
    test_dup_int_array_basic();
    test_dup_int_array_edge_cases();
    test_merge_int_arrays_basic();
    test_merge_int_arrays_edge_cases();
    test_memory_independence();
    test_large_arrays();

    printf("\n=== All func_block_utils tests PASSED! ===\n");
    return 0;
}
