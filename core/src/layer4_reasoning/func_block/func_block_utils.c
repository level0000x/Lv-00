/**
 * @file func_block_utils.c
 * @brief 函数块内部工具函数实现
 * @details 提供整数数组相关的工具函数：ID 检查、深拷贝、合并。
 *          这些函数原内嵌于 func_block.c，现独立为单独编译单元。
 */

#include "func_block_utils.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"

/* 前向声明：委托给 preset_common.c 中的主实现，避免循环依赖 */
extern int *lv_dup_int_array(const int *src, int count);

/* ==================== 命名常量 ==================== */

/* DEFAULT_DISTANCE_SQUARED 已统一定义在 lv_internal.h 中 (lv_DEFAULT_DISTANCE_SQUARED) */

/* ================================================================
 * ID 存在性检查
 * ================================================================ */

/**
 * @brief 检查整数 ID 是否存在于数组中
 *
 * 线性扫描整个数组，查找与给定 ID 相等的元素。
 * 数组为 NULL 时直接返回 false（不存在）。
 * 时间复杂度 O(n)，适用于计数较小的场景。
 *
 * @param id    要查找的 ID
 * @param arr   整数数组（可为 NULL）
 * @param count 数组元素个数
 * @return true  存在
 * @return false 不存在或数组为 NULL
 */
bool is_id_in_array(int id, const int *arr, int count) {
    if (!arr)
        return false;
    for (int i = 0; i < count; i++) {
        if (arr[i] == id)
            return true;
    }
    return false;
}

/* ================================================================
 * 整数数组深拷贝
 * ================================================================ */

/**
 * @brief 深拷贝整数数组（委托给 lv_dup_int_array）
 *
 * 此函数保留以维持向后兼容的公共 API。
 * 实际逻辑委托给 preset_common.c 中的 lv_dup_int_array，
 * 后者具有更完善的错误检查（含溢出检查和错误码设置）。
 *
 * @param src   源数组指针
 * @param count 元素个数
 * @return 新分配的整数数组指针，失败返回 NULL
 */
int *dup_int_array(const int *src, int count) {
    return lv_dup_int_array(src, count);
}

/* ================================================================
 * 整数数组合并
 * ================================================================ */

/**
 * @brief 合并两个整数数组
 *
 * 创建一个新数组，内容为 a 的全部元素后接 b 的全部元素。
 *
 * 安全检查：
 * - out_count 为 NULL 时返回 NULL
 * - 合并后总数为 0 时返回 NULL（不分配零长度内存）
 * - 任一数组为 NULL 但其 count > 0 时视为参数错误，返回 NULL
 *
 * @param a         第一个数组
 * @param a_count   第一个数组的元素个数
 * @param b         第二个数组
 * @param b_count   第二个数组的元素个数
 * @param out_count [输出] 合并后元素总数
 * @return 新分配的整数数组，调用方负责释放；失败返回 NULL
 */
int *merge_int_arrays(const int *a, int a_count, const int *b, int b_count, int *out_count) {
    if (!out_count)
        return NULL;
    /* 【修复】检查 count 参数是否为负数，防止 size_t 转换后回绕产生巨大值 */
    if (a_count < 0 || b_count < 0) {
        *out_count = 0;
        return NULL;
    }
    if (a_count > INT_MAX - b_count) {
        return NULL;  // 整数溢出
    }
    *out_count = a_count + b_count;
    if (*out_count == 0)
        return NULL;
    if ((!a && a_count > 0) || (!b && b_count > 0)) {
        *out_count = 0;
        return NULL;
    }
    /* 【修复】显式检查 (size_t)(*out_count) * sizeof(int) 不会溢出 size_t */
    if ((size_t) (*out_count) > SIZE_MAX / sizeof(int)) {
        *out_count = 0;
        return NULL;
    }
    /* 分配结果数组，调用者负责释放 */
    int *result = lv_malloc((size_t) (*out_count) * sizeof(int));
    if (!result) {
        *out_count = 0;
        return NULL; /* 分配失败，无需释放 result */
    }
    int idx = 0;
    for (int i = 0; i < a_count; i++)
        result[idx++] = a[i];
    for (int i = 0; i < b_count; i++)
        result[idx++] = b[i];
    /* 【说明】result 由调用者负责释放，此处直接返回所有权 */
    return result;
}
