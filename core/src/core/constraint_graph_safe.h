/**
 * @file constraint_graph_safe.h
 * @brief 约束图安全操作辅助头文件
 * @details 提供安全的数组操作、边界检查和溢出保护函数，
 *          用于修复 constraint_graph.c 中的安全风险。
 *
 * @version 3.4.2
 * @date 2026-05-25
 */

#ifndef LV00_CONSTRAINT_GRAPH_SAFE_H
#define LV00_CONSTRAINT_GRAPH_SAFE_H

#include <stddef.h>
#include <stdbool.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 安全数组操作宏
 * ============================================================================ */

/**
 * @brief 安全数组索引访问宏
 * @details 检查索引是否在有效范围内，避免缓冲区溢出
 */
#define SAFE_ARRAY_GET(arr, idx, count, default_val) \
    (((idx) >= 0 && (idx) < (count)) ? (arr)[(idx)] : (default_val))

/**
 * @brief 安全数组赋值宏
 * @details 检查索引是否在有效范围内，避免缓冲区溢出
 */
#define SAFE_ARRAY_SET(arr, idx, count, value) \
    do { \
        if ((idx) >= 0 && (idx) < (count)) { \
            (arr)[(idx)] = (value); \
        } \
    } while(0)

/* ============================================================================
 * 整数溢出保护函数
 * ============================================================================ */

/**
 * @brief 安全整数乘法（带溢出检查）
 * @param a 乘数
 * @param b 被乘数
 * @param result 输出结果指针
 * @return 成功返回true，溢出返回false
 */
static inline bool safe_multiply_int(int a, int b, int *result) {
    if (a == 0 || b == 0) {
        if (result) *result = 0;
        return true;
    }
    /* 检查溢出：a * b > INT_MAX 或 a * b < INT_MIN */
    if (a > 0) {
        if (b > 0) {
            if (a > INT_MAX / b) return false;
        } else {
            if (b < INT_MIN / a) return false;
        }
    } else {
        if (b > 0) {
            if (a < INT_MIN / b) return false;
        } else {
            if (a < INT_MAX / b) return false;  /* 注意：a和b都是负数 */
        }
    }
    if (result) *result = a * b;
    return true;
}

/**
 * @brief 安全整数加法（带溢出检查）
 * @param a 加数
 * @param b 被加数
 * @param result 输出结果指针
 * @return 成功返回true，溢出返回false
 */
static inline bool safe_add_int(int a, int b, int *result) {
    if (b > 0) {
        if (a > INT_MAX - b) return false;
    } else if (b < 0) {
        if (a < INT_MIN - b) return false;
    }
    if (result) *result = a + b;
    return true;
}

/**
 * @brief 安全计算扩容后的新容量
 * @param current 当前容量
 * @param min_required 最小需求容量
 * @param growth_factor 增长因子（通常为2）
 * @param max_cap 最大允许容量
 * @param result 输出结果指针
 * @return 成功返回true，溢出或超限返回false
 */
static inline bool safe_compute_new_capacity(int current, int min_required,
                                              int growth_factor, int max_cap,
                                              int *result) {
    if (current < 0 || min_required < 0 || growth_factor <= 0) {
        return false;
    }

    /* 初始容量处理 */
    if (current == 0) {
        int new_cap = min_required > 0 ? min_required : 4;
        if (max_cap > 0 && new_cap > max_cap) {
            new_cap = max_cap;
        }
        if (result) *result = new_cap;
        return true;
    }

    /* 计算扩容后的容量 */
    int new_cap;
    if (!safe_multiply_int(current, growth_factor, &new_cap)) {
        /* 溢出，尝试使用最大允许值 */
        if (max_cap > 0 && min_required <= max_cap) {
            if (result) *result = max_cap;
            return true;
        }
        return false;
    }

    /* 确保满足最小需求 */
    if (new_cap < min_required) {
        new_cap = min_required;
    }

    /* 检查最大限制 */
    if (max_cap > 0 && new_cap > max_cap) {
        new_cap = max_cap;
    }

    if (result) *result = new_cap;
    return true;
}

/* ============================================================================
 * 边界检查辅助函数
 * ============================================================================ */

/**
 * @brief 检查指针是否有效（非NULL）
 */
static inline bool ptr_is_valid(const void *ptr) {
    return ptr != NULL;
}

/**
 * @brief 检查索引是否在有效范围内
 */
static inline bool index_is_valid(int index, int count) {
    return index >= 0 && index < count;
}

/**
 * @brief 检查范围是否有效（start <= end 且都在边界内）
 */
static inline bool range_is_valid(int start, int end, int count) {
    return start >= 0 && end >= start && end <= count;
}

/**
 * @brief 计算安全的数组元素大小（防止size_t溢出）
 * @param count 元素数量
 * @param elem_size 单个元素大小
 * @param result 输出结果指针
 * @return 成功返回true，溢出返回false
 */
static inline bool safe_array_size(size_t count, size_t elem_size, size_t *result) {
    if (count == 0 || elem_size == 0) {
        if (result) *result = 0;
        return true;
    }
    /* 检查溢出 */
    if (count > SIZE_MAX / elem_size) {
        return false;
    }
    if (result) *result = count * elem_size;
    return true;
}

#ifdef __cplusplus
}
#endif

#endif /* LV00_CONSTRAINT_GRAPH_SAFE_H */
