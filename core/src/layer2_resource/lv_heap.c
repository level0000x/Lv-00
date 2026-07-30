/**
 * @file lv_heap.c
 * @brief 泛型二叉堆（优先级队列）实现
 *
 * 标准二叉堆实现，push 时 sift-up，pop 时 sift-down。
 * 使用 lv_malloc/lv_free 管理内部数组，容量不足时翻倍扩容。
 */
#include <string.h>

#include "lv/lv_heap.h"
#include "lv/lv_utils.h"

/** @brief 默认初始容量 */
#define lv_HEAP_DEFAULT_CAPACITY 16

/**
 * @brief 获取元素在数组中的指针
 */
static inline char *elem_ptr(const lvHeap *heap, size_t index) {
    return heap->data + index * heap->elem_size;
}

/**
 * @brief 比较两个元素
 * @return true 表示 a 应排在 b 之上（即 a 优先级高于 b）
 */
static inline bool higher_than(const lvHeap *heap, size_t a_idx, size_t b_idx) {
    int cmp = heap->compare(elem_ptr(heap, a_idx), elem_ptr(heap, b_idx));
    /* 最小堆：a < b 则 a 在上；最大堆：a > b 则 a 在上 */
    return (heap->type == lv_MIN_HEAP) ? (cmp < 0) : (cmp > 0);
}

/**
 * @brief 交换两个元素
 */
static inline void swap_elem(lvHeap *heap, size_t i, size_t j) {
    char tmp[64]; /* 栈上暂存，适用于小元素；大元素会增加拷贝 */
    /* 若元素较大，使用动态分配避免栈溢出 */
    size_t sz = heap->elem_size;

    if (sz <= sizeof(tmp)) {
        memcpy(tmp, elem_ptr(heap, i), sz);
        memcpy(elem_ptr(heap, i), elem_ptr(heap, j), sz);
        memcpy(elem_ptr(heap, j), tmp, sz);
    } else {
        /* 大元素使用 lv_malloc 临时缓冲区 */
        void *buf = lv_malloc(sz);
        if (!buf)
            return; /* 交换失败，堆结构可能受损 */
        memcpy(buf, elem_ptr(heap, i), sz);
        memcpy(elem_ptr(heap, i), elem_ptr(heap, j), sz);
        memcpy(elem_ptr(heap, j), buf, sz);
        lv_free(&buf);
    }
}

static void sift_up(lvHeap *heap, size_t idx) {
    while (idx > 0) {
        size_t parent = (idx - 1) / 2;
        if (!higher_than(heap, idx, parent))
            break;
        swap_elem(heap, idx, parent);
        idx = parent;
    }
}

static void sift_down(lvHeap *heap, size_t idx) {
    size_t n = heap->count;
    while (1) {
        size_t candidate = idx;
        size_t left = 2 * idx + 1;
        size_t right = 2 * idx + 2;

        if (left < n && higher_than(heap, left, candidate))
            candidate = left;
        if (right < n && higher_than(heap, right, candidate))
            candidate = right;

        if (candidate == idx)
            break;
        swap_elem(heap, idx, candidate);
        idx = candidate;
    }
}

bool lv_heap_init(lvHeap *heap, size_t elem_size, lvHeapType type,
                  lvHeapCompareFunc compare, size_t initial_capacity) {
    if (!heap || elem_size == 0 || !compare)
        return false;

    if (initial_capacity == 0)
        initial_capacity = lv_HEAP_DEFAULT_CAPACITY;

    heap->data = (char *)lv_malloc(initial_capacity * elem_size);
    if (!heap->data)
        return false;

    heap->count = 0;
    heap->capacity = initial_capacity;
    heap->elem_size = elem_size;
    heap->type = type;
    heap->compare = compare;

    return true;
}

void lv_heap_destroy(lvHeap *heap) {
    if (!heap)
        return;
    lv_free((void **)&heap->data);
    memset(heap, 0, sizeof(*heap));
}

bool lv_heap_push(lvHeap *heap, const void *elem) {
    if (!heap || !elem)
        return false;

    /* 扩容 */
    if (heap->count >= heap->capacity) {
        size_t new_cap = (heap->capacity == 0)
                             ? lv_HEAP_DEFAULT_CAPACITY
                             : heap->capacity * 2;
        char *new_data = (char *)lv_realloc(heap->data, new_cap * heap->elem_size);
        if (!new_data)
            return false;
        heap->data = new_data;
        heap->capacity = new_cap;
    }

    /* 在末尾放入元素 */
    memcpy(elem_ptr(heap, heap->count), elem, heap->elem_size);
    size_t idx = heap->count;
    heap->count++;

    /* 上浮 */
    sift_up(heap, idx);
    return true;
}

bool lv_heap_pop(lvHeap *heap, void *out_elem) {
    if (!heap || heap->count == 0)
        return false;

    /* 保存堆顶 */
    if (out_elem)
        memcpy(out_elem, elem_ptr(heap, 0), heap->elem_size);

    /* 将最后一个元素移到堆顶 */
    heap->count--;
    if (heap->count > 0) {
        memcpy(elem_ptr(heap, 0), elem_ptr(heap, heap->count), heap->elem_size);
        sift_down(heap, 0);
    }

    return true;
}

bool lv_heap_top(const lvHeap *heap, void *out_elem) {
    if (!heap || heap->count == 0 || !out_elem)
        return false;
    memcpy(out_elem, elem_ptr(heap, 0), heap->elem_size);
    return true;
}

size_t lv_heap_size(const lvHeap *heap) {
    return heap ? heap->count : 0;
}

bool lv_heap_empty(const lvHeap *heap) {
    return (heap == NULL) || (heap->count == 0);
}
