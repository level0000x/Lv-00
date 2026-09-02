/**
 * @file func_block_utils.h
 * @brief 函数块内部工具函数声明
 *
 * 提供整数数组操作相关的内部工具函数，包括：
 * - ID 存在性检查
 * - 整数数组深拷贝
 * - 整数数组合并
 *
 * 这些函数被函数块系统（func_block、func_block_compose 等）内部使用。
 */
#ifndef lv_FUNC_BLOCK_UTILS_H
#define lv_FUNC_BLOCK_UTILS_H
#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief 检查整数 ID 是否存在于数组中
 *
 * 线性扫描数组，查找指定 ID。数组为 NULL 时返回 false。
 *
 * @param id    要查找的 ID
 * @param array 整数数组
 * @param count 数组元素个数
 * @return true  存在
 * @return false 不存在或数组为空
 */
lv_PUBLIC_API bool is_id_in_array(int id, const int *array, int count);
/**
 * @brief 深拷贝整数数组
 *
 * 分配新内存并复制源数组的全部元素。
 * count <= 0 或 src 为 NULL 时返回 NULL。
 *
 * @param src   源数组指针
 * @param count 元素个数
 * @return 新分配的整数数组指针，失败时返回 NULL
 */
lv_PUBLIC_API int *dup_int_array(const int *src, int count);
/**
 * @brief 合并两个整数数组
 *
 * 将两个整数数组拼接为一个新数组，a 在前，b 在后。
 * 任一数组为 NULL 但其 count > 0 时视为错误，返回 NULL 并将 *out_count 置 0。
 *
 * @param a         第一个数组
 * @param a_count   第一个数组的元素个数
 * @param b         第二个数组
 * @param b_count   第二个数组的元素个数
 * @param out_count [输出] 合并后的元素个数
 * @return 新分配的整数数组指针，失败时返回 NULL
 */
lv_PUBLIC_API int *lv_int_array_merge(const int *a, int a_count, const int *b, int b_count, int *out_count);
/* 向后兼容别名 */
#define merge_int_arrays lv_int_array_merge
#ifdef __cplusplus
}
#endif
#endif /* lv_FUNC_BLOCK_UTILS_H */
