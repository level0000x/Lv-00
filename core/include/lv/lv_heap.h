/**
 * @file lv_heap.h
 * @brief 泛型二叉堆（优先级队列）
 *
 * 基于数组实现的泛型二叉堆，支持最小堆和最大堆两种模式。
 * 元素通过比较函数进行排序，支持动态扩容。
 */
#ifndef lv_HEAP_H
#define lv_HEAP_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 堆类型：最小堆 / 最大堆 */
typedef enum {
    lv_MIN_HEAP,  /* 小根堆 */
    lv_MAX_HEAP   /* 大根堆 */
} lvHeapType;

/** @brief 元素比较函数
 *  @return < 0 表示 a < b, 0 表示相等, > 0 表示 a > b
 */
typedef int (*lvHeapCompareFunc)(const void *a, const void *b);

/** @brief 泛型二叉堆 */
typedef struct {
    char *data;              /* 元素数组 */
    size_t count;            /* 当前元素数 */
    size_t capacity;         /* 容量 */
    size_t elem_size;        /* 元素大小 */
    lvHeapType type;         /* 堆类型 */
    lvHeapCompareFunc compare; /* 比较函数 */
} lvHeap;

/**
 * @brief 初始化堆
 * @param heap      堆指针
 * @param elem_size 元素大小
 * @param type      堆类型（最小/最大）
 * @param compare   比较函数
 * @param initial_capacity 初始容量（0 则使用默认 16）
 * @return true 成功
 */
bool lv_heap_init(lvHeap *heap, size_t elem_size, lvHeapType type,
                  lvHeapCompareFunc compare, size_t initial_capacity);

/** @brief 销毁堆 */
void lv_heap_destroy(lvHeap *heap);

/**
 * @brief 压入元素
 * @param heap   堆指针
 * @param elem   元素数据
 * @return true 成功
 */
bool lv_heap_push(lvHeap *heap, const void *elem);

/**
 * @brief 弹出堆顶元素
 * @param heap     堆指针
 * @param out_elem 输出弹出的元素（可为 NULL）
 * @return true 成功（堆非空），false 堆空
 */
bool lv_heap_pop(lvHeap *heap, void *out_elem);

/**
 * @brief 获取堆顶元素（不弹出）
 * @param heap     堆指针
 * @param out_elem 输出堆顶元素
 * @return true 成功
 */
bool lv_heap_top(const lvHeap *heap, void *out_elem);

/** @brief 获取元素数量 */
size_t lv_heap_size(const lvHeap *heap);

/** @brief 堆是否为空 */
bool lv_heap_empty(const lvHeap *heap);

#ifdef __cplusplus
}
#endif

#endif /* lv_HEAP_H */
