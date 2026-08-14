/**
 * @file lv_utils_array.c
 * @brief IntArray 整数动态数组工具
 *
 * @details 从 lv_utils.c 拆分的子模块（Lv-00 项目 v3.3.0+）。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/lv_utils.h"

#include "lv/lv_file.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lv/error_codes.h"
#include "lv/debug.h"
#include "lv/lv_internal.h"

/* ============================================================
 * 数组通用工具
 * ============================================================ */

/**
 * @brief 统一「倍增直到不小于 min_capacity」扩容算法
 *
 * IntArray 的扩容核心：按 lv_ARRAY_GROWTH_FACTOR 倍增直到满足
 * 最小容量，含两步溢出检查（倍增值、分配大小）与统一的失败语义。
 * zero_new 为 true 时清零新分配区域。
 */
static bool array_grow_to_fit(void **data, size_t *capacity, size_t min_capacity,
                              size_t elem_size, bool zero_new) {
    /* 输入验证：容量为0时按最小默认容量处理，避免死循环 */
    if (min_capacity == 0)
        min_capacity = 1;
    if (*capacity >= min_capacity)
        return true;

    size_t new_capacity = *capacity;
    while (new_capacity < min_capacity) {
        /* 修复：检查两步溢出
         * 1. new_capacity * lv_ARRAY_GROWTH_FACTOR 不能超过 SIZE_MAX
         * 2. new_capacity * elem_size 不能超过 SIZE_MAX（分配时使用） */
        if (new_capacity > SIZE_MAX / lv_ARRAY_GROWTH_FACTOR)
            lv_RETURN_ERROR_BOOL(lv_ERROR_OVERFLOW, "array_grow_to_fit 溢出");
        new_capacity *= lv_ARRAY_GROWTH_FACTOR;
    }

    if (new_capacity > SIZE_MAX / elem_size)
        lv_RETURN_ERROR_BOOL(lv_ERROR_OVERFLOW, "array_grow_to_fit 分配大小溢出");
    size_t alloc_size = new_capacity * elem_size;

    void *new_data = lv_realloc(*data, alloc_size);
    if (!new_data)
        lv_RETURN_ERROR_BOOL(lv_ERROR_ALLOCATION_FAILED, "array_grow_to_fit realloc 失败");

    if (zero_new)
        memset((char *) new_data + *capacity * elem_size, 0, (new_capacity - *capacity) * elem_size);

    *data = new_data;
    *capacity = new_capacity;
    return true;
}

void lv_insertion_sort(void *base, size_t n, size_t elem_size,
                       int (*cmp)(const void *a, const void *b, void *ctx), void *ctx) {
    if (!base || n < 2 || elem_size == 0 || !cmp)
        return;

    unsigned char stack_tmp[64];
    unsigned char *tmp = stack_tmp;
    unsigned char *heap_tmp = NULL;
    if (elem_size > sizeof(stack_tmp)) {
        heap_tmp = (unsigned char *) lv_malloc(elem_size);
        if (!heap_tmp)
            return;
        tmp = heap_tmp;
    }

    unsigned char *p = (unsigned char *) base;
    for (size_t i = 1; i < n; i++) {
        memcpy(tmp, p + i * elem_size, elem_size);
        size_t j = i;
        while (j > 0 && cmp(p + (j - 1) * elem_size, tmp, ctx) > 0) {
            memcpy(p + j * elem_size, p + (j - 1) * elem_size, elem_size);
            j--;
        }
        memcpy(p + j * elem_size, tmp, elem_size);
    }

    if (heap_tmp)
        lv_free((void **) &heap_tmp);
}

/**
 * @brief 从紧凑数组中删除下标 index 处的元素，将后续元素整体前移
 *
 * 收敛散落各模块的手写"for 逐元素前移"样板（func_block_registry /
 * high_dim_core / engine_scheduler / stream_context / debug_trace 等），
 * 统一用单次 memmove 完成移位。计数由调用方维护并自行递减。
 *
 * @param base      数组起始地址
 * @param elem_size 元素字节大小
 * @param index     待删除下标（越界时为空操作）
 * @param count     当前元素个数（不移除尾部残留，调用方负责递减）
 */
void lv_shift_left(void *base, size_t elem_size, size_t index, size_t count) {
    if (!base || elem_size == 0 || index >= count)
        return;
    if (index + 1 < count) {
        memmove((char *) base + index * elem_size, (char *) base + (index + 1) * elem_size,
                (count - index - 1) * elem_size);
    }
}

/**
 * @brief 在数组中右移腾位（数组中间插入的移位移除前辅助）
 *
 * 将 [index, count) 区间的元素整体右移一格到 [index+1, count+1)，
 * 在 index 处腾出空位供插入。与 lv_shift_left 对称（lv_shift_left
 * 删除 index 处元素前移；本函数在 index 处腾位右移）。统一用单次
 * memmove 完成移位，收敛散落的"for 逐元素右移"样板。计数由调用方
 * 维护并自行递增。
 *
 * @param base      数组起始地址
 * @param elem_size 元素字节大小
 * @param index     插入位置（腾位下标，越界时为空操作）
 * @param count     当前元素个数（右移 [index, count)，不移入尾部残留）
 */
void lv_shift_right(void *base, size_t elem_size, size_t index, size_t count) {
    if (!base || elem_size == 0 || index >= count)
        return;
    memmove((char *) base + (index + 1) * elem_size, (char *) base + index * elem_size,
            (count - index) * elem_size);
}

/**
 * @brief 消费缓冲前缀后将剩余数据前移压缩到头部
 *
 * 删除缓冲区前 pos 个元素，把 [pos, len) 剩余元素前移到头部并更新
 * 长度（recv 缓冲 consume 语义）。pos 为 0 时空操作；pos >= len 时
 * 全部消费（len 置 0）。与 lv_shift_left 同为 memmove 单次移位。
 *
 * @param buf       缓冲起始地址
 * @param elem_size 元素字节大小
 * @param pos       已消费的元素个数（前缀）
 * @param len       指向当前元素个数的指针（原地更新为剩余个数）
 */
void lv_buffer_consume(void *buf, size_t elem_size, size_t pos, size_t *len) {
    if (!buf || !len || elem_size == 0 || pos == 0)
        return;
    if (pos >= *len) {
        *len = 0;
        return;
    }
    memmove(buf, (char *) buf + pos * elem_size, (*len - pos) * elem_size);
    *len -= pos;
}

/**
 * @brief 判断两个 int 多集是否相等（排序后逐元素比较）
 *
 * 拷贝两份输入后排序比较，不修改入参。长度不等直接判不等。
 * 收敛 type_check / normalization 中手写"双 qsort + 双指针/逐元素
 * 比较"的多集相等判定样板（判据 A）。
 *
 * @param a  第一组元素数组（an==0 时可为 NULL）
 * @param an 第一组元素个数
 * @param b  第二组元素数组（bn==0 时可为 NULL）
 * @param bn 第二组元素个数
 * @return 1 相等；0 不相等；-1 内存分配失败（调用方按错误处理）
 */
int lv_int_multiset_equal(const int *a, int an, const int *b, int bn) {
    if (an != bn)
        return 0;
    if (an == 0)
        return 1;
    if (!a || !b)
        return 0;
    int *sa = lv_malloc((size_t) an * sizeof(int));
    int *sb = lv_malloc((size_t) bn * sizeof(int));
    if (!sa || !sb) {
        lv_free((void **) &sa);
        lv_free((void **) &sb);
        return -1;
    }
    memcpy(sa, a, (size_t) an * sizeof(int));
    memcpy(sb, b, (size_t) bn * sizeof(int));
    qsort(sa, (size_t) an, sizeof(int), lv_cmp_int);
    qsort(sb, (size_t) bn, sizeof(int), lv_cmp_int);
    int result = 1;
    for (int i = 0; i < an; i++) {
        if (sa[i] != sb[i]) {
            result = 0;
            break;
        }
    }
    lv_free((void **) &sa);
    lv_free((void **) &sb);
    return result;
}

/**
 * @brief 向紧凑 int 数组追加不重复值（unique append）
 *
 * 线性扫描 [arr, arr+*count)，若 value 已存在则跳过返回 false；
 * 否则写入 arr[*count] 并递增计数返回 true。收敛 solver / 导出模块
 * 中手写"bool found + 内层 for 查重 + append"样板（判据 B）。
 * 容量由调用方保证（原样板语义即为紧凑数组且调用方维护容量）。
 *
 * @param arr   目标数组起始地址
 * @param count 指向当前元素个数的指针（追加成功后原地递增）
 * @param value 待追加的值
 * @return true 已追加；false 值已存在或参数非法
 */
bool lv_int_append_unique(int *arr, int *count, int value) {
    if (!arr || !count)
        return false;
    for (int i = 0; i < *count; i++) {
        if (arr[i] == value)
            return false;
    }
    arr[(*count)++] = value;
    return true;
}

/* ============================================================
 * 整数数组
 * ============================================================ */

IntArray *int_array_create(size_t initial_capacity) {
    IntArray *arr = lv_calloc(1, sizeof(IntArray));
    if (!arr)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "int_array_create calloc 失败");

    arr->count = 0;
    arr->capacity = initial_capacity > 0 ? initial_capacity : lv_INITIAL_ARRAY_CAPACITY;
    arr->data = lv_calloc(arr->capacity, sizeof(int));

    if (!arr->data) {
        lv_free((void **) &arr);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "int_array_create data calloc 失败");
    }

    return arr;
}

void int_array_destroy(IntArray *arr) {
    if (!arr)
        return;
    lv_free((void **) &arr->data);
    lv_free((void **) &arr);
}

static bool int_array_ensure_capacity(IntArray *arr, size_t min_capacity) {
    if (!arr)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "int_array_ensure_capacity arr 为 NULL");
    return array_grow_to_fit((void **) &arr->data, &arr->capacity, min_capacity, sizeof(int), false);
}

bool int_array_push(IntArray *arr, int value) {
    if (!arr)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "int_array_push arr 为 NULL");
    if (!int_array_ensure_capacity(arr, arr->count + 1))
        lv_RETURN_ERROR_BOOL(lv_ERROR_ALLOCATION_FAILED, "int_array_push 扩容失败");

    arr->data[arr->count++] = value;
    return true;
}

/**
 * @brief 批量向整数数组末尾追加多个元素
 *
 * 将 values 数组中的 count 个整数依次追加到 arr 的末尾。
 * 若空间不足，会自动扩容。
 *
 * @param arr    目标整数数组指针，不允许为 NULL。
 * @param values 源数据数组指针，不允许为 NULL。
 * @param count  要追加的元素个数。
 * @return true  追加成功；
 *         false 参数无效或内存扩容失败。
 */
bool int_array_push_many(IntArray *arr, const int *values, size_t count) {
    if (!arr)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "int_array_push_many arr 为 NULL");
    if (!values)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "int_array_push_many values 为 NULL");
    if (!int_array_ensure_capacity(arr, arr->count + count))
        lv_RETURN_ERROR_BOOL(lv_ERROR_ALLOCATION_FAILED, "int_array_push_many 扩容失败");

    memcpy(arr->data + arr->count, values, count * sizeof(int));
    arr->count += count;
    return true;
}

/**
 * @brief 判断整数数组是否包含指定值
 *
 * 线性遍历数组，检查是否存在与 value 相等的元素。
 *
 * @param arr   整数数组指针，允许为 NULL。
 * @param value 要查找的值。
 * @return true  数组中存在该值；
 *         false arr 为 NULL 或数组中不存在该值。
 * @note 时间复杂度为 O(n)，不适用于对性能敏感的频繁查找场景。
 */
bool int_array_contains(const IntArray *arr, int value) {
    if (!arr)
        return false;
    for (size_t i = 0; i < arr->count; i++) {
        if (arr->data[i] == value)
            return true;
    }
    return false;
}

/**
 * @brief 查找指定值在整数数组中首次出现的索引
 *
 * 线性遍历数组，返回第一个与 value 相等的元素的下标。
 *
 * @param arr   整数数组指针，允许为 NULL。
 * @param value 要查找的值。
 * @return >=0  值在数组中的索引（从 0 开始）；
 *         -1   arr 为 NULL 或数组中不存在该值。
 * @note 时间复杂度为 O(n)。若数组中存在多个匹配项，仅返回第一个的索引。
 */
int int_array_index_of(const IntArray *arr, int value) {
    if (!arr)
        return -1;
    for (size_t i = 0; i < arr->count; i++) {
        if (arr->data[i] == value)
            return (int) i;
    }
    return -1;
}

/**
 * @brief 从整数数组中移除指定值的第一个匹配项
 *
 * 查找并移除数组中第一个与 value 相等的元素，后续元素前移以保持连续性。
 *
 * @param arr   整数数组指针，不允许为 NULL。
 * @param value 要移除的值。
 * @return true  成功找到并移除了该值；
 *         false arr 为 NULL 或数组中不存在该值。
 * @note 仅移除第一个匹配项，若存在多个相同值需多次调用。
 *       移除操作的时间复杂度为 O(n)（含查找和元素前移）。
 */
bool int_array_remove(IntArray *arr, int value) {
    if (!arr)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "int_array_remove arr 为 NULL");
    int idx = int_array_index_of(arr, value);
    if (idx < 0)
        return false;

    /* 移动后续元素（统一走 lv_shift_left 的 memmove 路径） */
    lv_shift_left(arr->data, sizeof(arr->data[0]), (size_t) idx, arr->count);
    arr->count--;
    return true;
}

/**
 * @brief 整数三向比较函数（用于 qsort 排序）
 *
 * 避免 (ia > ib) - (ia < ib) 写法在极端值情况下可能触发的
 * 未定义行为（INT_MIN 与 INT_MAX 相减导致有符号整数溢出）。
 *
 * @param a 指向第一个 int 的指针
 * @param b 指向第二个 int 的指针
 * @return 负数（a < b）、零（a == b）、正数（a > b）
 */
int lv_cmp_int(const void *a, const void *b) {
    int ia = *(const int *) a;
    int ib = *(const int *) b;
    /* 使用分支而非算术运算，避免有符号整数溢出风险 */
    if (ia < ib)
        return -1;
    if (ia > ib)
        return 1;
    return 0;
}

/**
 * @brief uint64_t 三向比较函数（用于 qsort 排序）
 *
 * @param a 指向第一个 uint64_t 的指针
 * @param b 指向第二个 uint64_t 的指针
 * @return 负数（a < b）、零（a == b）、正数（a > b）
 */
int lv_cmp_uint64(const void *a, const void *b) {
    uint64_t va = *(const uint64_t *) a;
    uint64_t vb = *(const uint64_t *) b;
    if (va < vb)
        return -1;
    if (va > vb)
        return 1;
    return 0;
}

void int_array_sort(IntArray *arr) {
    if (!arr || arr->count < 2)
        return;
    qsort(arr->data, arr->count, sizeof(int), lv_cmp_int);
}

IntArray *int_array_copy(const IntArray *arr) {
    if (!arr)
        return NULL;
    IntArray *copy = int_array_create(arr->capacity);
    if (!copy)
        return NULL;

    memcpy(copy->data, arr->data, arr->count * sizeof(int));
    copy->count = arr->count;
    return copy;
}

IntArray *int_array_from_carray(const int *data, size_t count) {
    if (!data)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "int_array_from_carray data 为 NULL");
    IntArray *arr = int_array_create(count);
    if (!arr)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "int_array_from_carray 创建失败");

    memcpy(arr->data, data, count * sizeof(int));
    arr->count = count;
    return arr;
}

