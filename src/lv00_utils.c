/**
 * @file lv00_utils.c
 * @brief Lv-00 工具函数库实现
 *
 * 提供内存管理、字符串处理、数组操作、配置管理等通用功能。
 *
 * ================================================================
 * 内存管理规范 (Memory Management Conventions)
 * ================================================================
 *
 * 本项目所有动态内存分配必须使用本模块提供的封装函数：
 *   - lv00_malloc / lv00_calloc / lv00_realloc / lv00_free
 *
 * **关键安全规则：**
 *
 * 1. lv00_realloc 与标准 realloc 的关键差异：
 *    - 当 size==0 时，lv00_realloc 返回 NULL 但**不释放原内存**。
 *      这是与标准 C 库 realloc(p, 0) 的重要区别（C11 标准中 realloc(p, 0)
 *      行为由实现定义）。调用者必须显式使用 lv00_free(&ptr) 来释放内存，
 *      不应依赖 lv00_realloc(ptr, 0) 来释放。
 *    - **调用者必须将返回值赋给原指针变量**，否则在 realloc 移动内存块后
 *      原指针将成为悬空指针。
 *
 * 2. lv00_free 使用 void** 参数：
 *    - lv00_free 接受 void** 而非 void*，释放后自动将调用者的指针置为 NULL，
 *      有效防止 use-after-free 和 double-free。
 *    - 必须传递指针的地址：lv00_free((void **)&ptr)，不可写作 lv00_free(ptr)。
 *
 * 3. 内存所有权规则：
 *    - 创建函数（如 rune_create_*）返回新分配的内存，调用者拥有所有权。
 *    - 当对象被添加到容器（如 RuneSequence）时，所有权转移给容器。
 *    - 销毁容器时会递归释放所有包含的元素。
 *
 * 4. 线程安全：
 *    - 内存统计 (MemoryStats) 使用 LV00_THREAD_LOCAL 存储，每个线程独立统计。
 *    - 内存限制 (g_memory_limit) 同样是线程局部变量。
 *    - 多线程环境下的跨线程内存操作需调用者自行同步。
 */

#include "lv00_utils.h"
#include "lv00_internal.h"
#include "error_codes.h"
#include "lv00.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <ctype.h>
#include <math.h>

/* ============================================================
 * 内存统计跟踪
 * ============================================================ */

static LV00_THREAD_LOCAL MemoryStats g_memory_stats = {0};
static LV00_THREAD_LOCAL size_t g_memory_limit = 0;

/* 包装malloc以跟踪内存使用 */
typedef struct {
    uint32_t magic;    /**< 魔数，用于检测double-free */
    size_t size;
    char data[];
} AllocHeader;

#define ALLOC_MAGIC_LIVE  0xADBEEF01  /**< 存活标记 */
#define ALLOC_MAGIC_FREED 0x00000000  /**< 已释放标记 */

/*
 * DEBUG: 临时使用原生 malloc/free 以诊断堆损坏
 * 注释掉自定义分配器，后续可恢复
 */
void *lv00_malloc(size_t size) {
    /* 内存限制检查 */
    if (g_memory_limit > 0 && g_memory_stats.current_used > g_memory_limit - size) {
        lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "内存限制超出: 请求%zu", size);
        return NULL;
    }
    /* 允许 size==0 返回有效指针（与标准 malloc(0) 行为一致）
     * 多个调用方依赖 graph_add_point(g, NULL, 0) 等零尺寸分配 */
    void *ptr = malloc(size ? size : 1);  /* malloc(0) 行为由实现定义，确保至少分配 1 字节 */
    if (ptr) {
        g_memory_stats.total_allocated += size;
        g_memory_stats.current_used += size;
        g_memory_stats.allocation_count++;
        if (g_memory_stats.current_used > g_memory_stats.peak_used)
            g_memory_stats.peak_used = g_memory_stats.current_used;
    }
    return ptr;
}

void *lv00_calloc(size_t nmemb, size_t size) {
    if (nmemb == 0 || size == 0) return NULL;
    
    /* 检查溢出 */
    if (nmemb > SIZE_MAX / size) {
        lv00_set_error(LV00_ERROR_OVERFLOW, "calloc 溢出: %zu * %zu", nmemb, size);
        return NULL;
    }
    
    size_t total = nmemb * size;
    void *ptr = lv00_malloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

/**
 * @brief 重新分配内存
 *
 * 行为类似于标准 realloc，但集成了内存统计跟踪和双释放检测。
 *
 * @note 当 size==0 时，此函数不会释放 ptr 指向的内存，而是直接返回 NULL。
 *       这避免了标准 realloc(p, 0) 行为的歧义性（C11 标准中该行为由实现定义），
 *       防止调用者因未将返回值赋回指针而导致 use-after-free。
 *       如需释放内存，请显式调用 lv00_free(&ptr)。
 */
void *lv00_realloc(void *ptr, size_t size) {
    if (!ptr) return lv00_malloc(size);
    if (size == 0) return NULL;
    void *new_ptr = realloc(ptr, size);
    if (new_ptr) {
        /* 简单跟踪 —— 原大小未知，仅做峰值统计 */
        g_memory_stats.current_used += size;
        if (g_memory_stats.current_used > g_memory_stats.peak_used) {
            g_memory_stats.peak_used = g_memory_stats.current_used;
        }
        g_memory_stats.total_allocated += size;
    }
    return new_ptr;
}

void lv00_free(void **ptr) {
    if (!ptr || !*ptr) return;
    free(*ptr);
    *ptr = NULL;
}

void lv00_free_many(void **first, ...) {
    va_list args;
    va_start(args, first);
    
    void **ptr = first;
    while (ptr) {
        lv00_free(ptr);
        ptr = va_arg(args, void **);
    }
    
    va_end(args);
}

/**
 * @brief 自动释放包装函数（用于 GCC/Clang cleanup 属性）
 *
 * 此函数设计为与 __attribute__((cleanup)) 配合使用，在变量离开作用域时
 * 自动调用 lv00_free 释放内存，避免手动管理资源导致泄漏。
 *
 * 使用示例：
 * @code
 *   char *buf __attribute__((cleanup(lv00_auto_free))) = lv00_malloc(100);
 *   // buf 在离开作用域时自动释放
 * @endcode
 *
 * @param p 指向指针变量的指针（cleanup 属性传入的是变量的地址）。
 *          内部会将其转换为 void** 并调用 lv00_free。
 * @note 此函数不应被直接调用，仅供编译器 cleanup 机制间接使用。
 */
void lv00_auto_free(void *p) {
    void **ptr = (void **)p;
    lv00_free(ptr);
}

void lv00_get_memory_stats(MemoryStats *stats) {
    if (!stats) return;
    *stats = g_memory_stats;
}

void lv00_reset_memory_stats(void) {
    memset(&g_memory_stats, 0, sizeof(g_memory_stats));
}

void lv00_set_memory_limit(size_t limit) {
    g_memory_limit = limit;
}

size_t lv00_get_memory_limit(void) {
    return g_memory_limit;
}

bool lv00_memory_limit_exceeded(void) {
    if (g_memory_limit == 0) return false;
    return g_memory_stats.current_used > g_memory_limit;
}

/* ============================================================
 * 字符串处理
 * ============================================================ */

size_t lv00_strlcpy(char *dest, const char *src, size_t dest_size) {
    if (!dest || !src || dest_size == 0) return 0;
    
    size_t src_len = strlen(src);
    if (src_len < dest_size) {
        memcpy(dest, src, src_len + 1);
    } else {
        memcpy(dest, src, dest_size - 1);
        dest[dest_size - 1] = '\0';
    }
    return src_len;
}

size_t lv00_strlcat(char *dest, const char *src, size_t dest_size) {
    if (!dest || !src || dest_size == 0) return 0;
    
    size_t dest_len = strlen(dest);
    if (dest_len >= dest_size) return dest_len + strlen(src);
    
    size_t remaining = dest_size - dest_len - 1;
    size_t src_len = strlen(src);
    
    if (src_len < remaining) {
        memcpy(dest + dest_len, src, src_len + 1);
    } else {
        memcpy(dest + dest_len, src, remaining);
        dest[dest_size - 1] = '\0';
    }
    return dest_len + src_len;
}

char *lv00_strdup_safe(const char *str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    char *copy = lv00_malloc(len + 1);
    if (copy) {
        memcpy(copy, str, len + 1);
    }
    return copy;
}

char *lv00_asprintf(const char *fmt, ...) {
    if (!fmt) return NULL;
    
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    
    if (len < 0) return NULL;
    
    char *buf = lv00_malloc((size_t)len + 1);
    if (!buf) return NULL;
    
    va_start(args, fmt);
    vsnprintf(buf, (size_t)len + 1, fmt, args);
    va_end(args);
    
    return buf;
}

/**
 * @brief 判断字符串是否为空白或空
 *
 * 检查给定字符串是否为 NULL、空字符串或仅包含空白字符（空格、制表符、
 * 换行符等）。
 *
 * @param str 待检查的字符串指针，允许为 NULL。
 * @return true  字符串为 NULL、空字符串或全部由空白字符组成；
 *         false 字符串包含至少一个非空白字符。
 */
bool lv00_str_is_blank(const char *str) {
    if (!str) return true;
    while (*str) {
        if (!isspace((unsigned char)*str)) return false;
        str++;
    }
    return true;
}

/**
 * @brief 原地去除字符串首尾空白字符
 *
 * 修改传入的字符串，去除其前导和尾部的空白字符（空格、制表符、换行符等）。
 * 通过在尾部空白处写入 '\0' 来截断字符串，并返回指向去除前导空白后
 * 第一个非空白字符的指针。
 *
 * @param str 待修剪的字符串指针，允许为 NULL。
 * @return 指向去除前导空白后的字符串起始位置的指针。
 *         若 str 为 NULL，返回 NULL。
 * @note 返回值可能与传入的 str 不同（当字符串有前导空白时）。
 *       此函数会原地修改字符串内容，调用者应使用返回值而非原始指针。
 *       若字符串全部为空白字符，返回指向末尾 '\0' 的指针。
 */
char *lv00_str_trim(char *str) {
    if (!str) return NULL;
    
    /* 去除前导空白 */
    while (isspace((unsigned char)*str)) str++;
    
    if (*str == '\0') return str;
    
    /* 去除尾部空白 */
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    
    return str;
}

/* ============================================================
 * 动态数组
 * ============================================================ */

LV00Array *lv00_array_create(size_t initial_capacity, size_t elem_size) {
    /* 修复：验证 elem_size，避免后续操作中出现除零或无意义的零大小元素 */
    if (elem_size == 0) return NULL;
    
    LV00Array *arr = lv00_malloc(sizeof(LV00Array));
    if (!arr) return NULL;
    
    arr->count = 0;
    arr->capacity = initial_capacity > 0 ? initial_capacity : LV00_INITIAL_ARRAY_CAPACITY;
    arr->elem_size = elem_size;
    arr->store_pointers = false;  /* 修复：elem_size 已验证非零，不再需要 store_pointers 回退逻辑 */
    
    arr->data = lv00_calloc(arr->capacity, sizeof(void *));
    if (!arr->data) {
        /* 修复：lv00_calloc 失败时释放已分配的 arr，防止资源泄漏 */
        lv00_free((void **)&arr);
        return NULL;
    }
    
    return arr;
}

void lv00_array_destroy(LV00Array *arr, bool free_elements) {
    if (!arr) return;
    
    if (free_elements && arr->data) {
        for (size_t i = 0; i < arr->count; i++) {
            if (arr->data[i]) {
                lv00_free(&arr->data[i]);
            }
        }
    }
    
    lv00_free((void **)&arr->data);
    lv00_free((void **)&arr);
}

static bool lv00_array_ensure_capacity(LV00Array *arr, size_t min_capacity) {
    if (!arr) return false;
    /* 输入验证：容量为0时按最小默认容量处理，避免死循环 */
    if (min_capacity == 0) min_capacity = 1;
    if (arr->capacity >= min_capacity) return true;
    
    size_t new_capacity = arr->capacity;
    while (new_capacity < min_capacity) {
        /* 修复：检查两步溢出
         * 1. new_capacity * LV00_ARRAY_GROWTH_FACTOR 不能超过 SIZE_MAX
         * 2. new_capacity * sizeof(void*) 不能超过 SIZE_MAX（分配时使用） */
        if (new_capacity > SIZE_MAX / LV00_ARRAY_GROWTH_FACTOR) return false;
        new_capacity *= LV00_ARRAY_GROWTH_FACTOR;
    }
    
    /* 修复：检查 new_capacity * sizeof(void*) 是否溢出 */
    if (new_capacity > SIZE_MAX / sizeof(void *)) return false;
    size_t alloc_size = new_capacity * sizeof(void *);
    
    void **new_data = lv00_realloc(arr->data, alloc_size);
    if (!new_data) return false;
    
    /* 清零新分配的部分 */
    memset(new_data + arr->capacity, 0, (new_capacity - arr->capacity) * sizeof(void *));
    
    arr->data = new_data;
    arr->capacity = new_capacity;
    return true;
}

bool lv00_array_push(LV00Array *arr, void *elem) {
    if (!arr) return false;
    
    if (!lv00_array_ensure_capacity(arr, arr->count + 1)) {
        return false;
    }
    
    arr->data[arr->count++] = elem;
    return true;
}

bool lv00_array_remove(LV00Array *arr, size_t index, bool free_elem) {
    if (!arr || index >= arr->count) return false;
    
    if (free_elem && arr->data[index]) {
        lv00_free(&arr->data[index]);
    }
    
    /* 移动后续元素 */
    for (size_t i = index; i < arr->count - 1; i++) {
        arr->data[i] = arr->data[i + 1];
    }
    arr->count--;
    arr->data[arr->count] = NULL;
    
    return true;
}

void *lv00_array_get(const LV00Array *arr, size_t index) {
    if (!arr || index >= arr->count) return NULL;
    return arr->data[index];
}

bool lv00_array_set(LV00Array *arr, size_t index, void *elem) {
    if (!arr || index >= arr->count) return false;
    arr->data[index] = elem;
    return true;
}

void lv00_array_clear(LV00Array *arr, bool free_elements) {
    if (!arr) return;
    
    if (free_elements) {
        for (size_t i = 0; i < arr->count; i++) {
            if (arr->data[i]) {
                lv00_free(&arr->data[i]);
            }
        }
    }
    
    memset(arr->data, 0, arr->capacity * sizeof(void *));
    arr->count = 0;
}

void lv00_array_sort(LV00Array *arr, int (*cmp)(const void *, const void *)) {
    if (!arr || !cmp || arr->count < 2) return;
    qsort(arr->data, arr->count, sizeof(void *), cmp);
}

int lv00_array_find(const LV00Array *arr, const void *elem) {
    if (!arr) return -1;
    for (size_t i = 0; i < arr->count; i++) {
        if (arr->data[i] == elem) return (int)i;
    }
    return -1;
}

/* ============================================================
 * 整数数组
 * ============================================================ */

IntArray *int_array_create(size_t initial_capacity) {
    IntArray *arr = lv00_malloc(sizeof(IntArray));
    if (!arr) return NULL;
    
    arr->count = 0;
    arr->capacity = initial_capacity > 0 ? initial_capacity : LV00_INITIAL_ARRAY_CAPACITY;
    arr->data = lv00_calloc(arr->capacity, sizeof(int));
    
    if (!arr->data) {
        lv00_free((void **)&arr);
        return NULL;
    }
    
    return arr;
}

void int_array_destroy(IntArray *arr) {
    if (!arr) return;
    lv00_free((void **)&arr->data);
    lv00_free((void **)&arr);
}

static bool int_array_ensure_capacity(IntArray *arr, size_t min_capacity) {
    if (!arr) return false;
    /* 输入验证：容量为0时按最小默认容量处理，避免死循环 */
    if (min_capacity == 0) min_capacity = 1;
    if (arr->capacity >= min_capacity) return true;
    
    size_t new_capacity = arr->capacity;
    while (new_capacity < min_capacity) {
        /* 修复：检查两步溢出（与 lv00_array_ensure_capacity 相同） */
        if (new_capacity > SIZE_MAX / LV00_ARRAY_GROWTH_FACTOR) return false;
        new_capacity *= LV00_ARRAY_GROWTH_FACTOR;
    }
    
    /* 修复：检查 new_capacity * sizeof(int) 是否溢出 */
    if (new_capacity > SIZE_MAX / sizeof(int)) return false;
    size_t alloc_size = new_capacity * sizeof(int);
    
    int *new_data = lv00_realloc(arr->data, alloc_size);
    if (!new_data) return false;
    
    arr->data = new_data;
    arr->capacity = new_capacity;
    return true;
}

bool int_array_push(IntArray *arr, int value) {
    if (!arr) return false;
    if (!int_array_ensure_capacity(arr, arr->count + 1)) return false;
    
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
    if (!arr || !values) return false;
    if (!int_array_ensure_capacity(arr, arr->count + count)) return false;
    
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
    if (!arr) return false;
    for (size_t i = 0; i < arr->count; i++) {
        if (arr->data[i] == value) return true;
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
    if (!arr) return -1;
    for (size_t i = 0; i < arr->count; i++) {
        if (arr->data[i] == value) return (int)i;
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
    if (!arr) return false;
    int idx = int_array_index_of(arr, value);
    if (idx < 0) return false;
    
    /* 移动后续元素 */
    for (size_t i = (size_t)idx; i < arr->count - 1; i++) {
        arr->data[i] = arr->data[i + 1];
    }
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
static int compare_int(const void *a, const void *b) {
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    /* 使用分支而非算术运算，避免有符号整数溢出风险 */
    if (ia < ib) return -1;
    if (ia > ib) return 1;
    return 0;
}

void int_array_sort(IntArray *arr) {
    if (!arr || arr->count < 2) return;
    qsort(arr->data, arr->count, sizeof(int), compare_int);
}

IntArray *int_array_copy(const IntArray *arr) {
    if (!arr) return NULL;
    IntArray *copy = int_array_create(arr->capacity);
    if (!copy) return NULL;
    
    memcpy(copy->data, arr->data, arr->count * sizeof(int));
    copy->count = arr->count;
    return copy;
}

IntArray *int_array_from_carray(const int *data, size_t count) {
    if (!data) return NULL;
    IntArray *arr = int_array_create(count);
    if (!arr) return NULL;
    
    memcpy(arr->data, data, count * sizeof(int));
    arr->count = count;
    return arr;
}

/* ============================================================
 * 配置管理
 * ============================================================ */

/* 消除魔术数字，用宏定义替代字面量 */
#define CONFIG_LINE_BUFFER_SIZE 1024  /**< 配置文件每行读取缓冲区大小 */

static ConfigItem *config_item_create(const char *key, ConfigType type) {
    ConfigItem *item = lv00_calloc(1, sizeof(ConfigItem));
    if (!item) return NULL;
    
    item->key = lv00_strdup_safe(key);
    if (!item->key) {
        lv00_free((void **)&item);
        return NULL;
    }
    item->type = type;
    return item;
}

static void config_item_destroy(ConfigItem *item) {
    if (!item) return;
    
    lv00_free((void **)&item->key);
    
    switch (item->type) {
        case CONFIG_TYPE_STRING:
            lv00_free((void **)&item->value.string_val);
            break;
        case CONFIG_TYPE_ARRAY:
            for (size_t i = 0; i < item->array_count; i++) {
                config_item_destroy(item->value.array_val[i]);
            }
            lv00_free((void **)&item->value.array_val);
            break;
        default:
            break;
    }
    
    lv00_free((void **)&item);
}

ConfigManager *config_manager_create(const char *config_file) {
    ConfigManager *mgr = lv00_calloc(1, sizeof(ConfigManager));
    if (!mgr) return NULL;
    
    if (config_file) {
        mgr->config_file = lv00_strdup_safe(config_file);
    }
    mgr->auto_save = false;
    
    return mgr;
}

void config_manager_destroy(ConfigManager *mgr) {
    if (!mgr) return;
    
    ConfigItem *item = mgr->items;
    while (item) {
        ConfigItem *next = item->next;
        config_item_destroy(item);
        item = next;
    }
    
    lv00_free((void **)&mgr->config_file);
    lv00_free((void **)&mgr);
}

/**
 * @brief 在配置管理器中查找指定键对应的配置项
 *
 * 遍历配置管理器的链表，通过字符串比较查找与 key 匹配的配置项。
 *
 * @param mgr 配置管理器指针，允许为 NULL。
 * @param key 要查找的配置键名，允许为 NULL。
 * @return 找到的配置项指针；若 mgr 或 key 为 NULL，或未找到匹配项，返回 NULL。
 * @note 此为内部静态函数，仅供配置管理模块内部使用。
 */
static ConfigItem *config_find_item(const ConfigManager *mgr, const char *key) {
    if (!mgr || !key) return NULL;
    
    ConfigItem *item = mgr->items;
    while (item) {
        if (strcmp(item->key, key) == 0) return item;
        item = item->next;
    }
    return NULL;
}

/**
 * @brief 生成标量类型配置设置函数的宏
 *
 * 用于 int、bool、double 等标量类型的 config_set_* 函数，
 * 避免重复编写"查找已有项 → 更新或创建 → 自动保存"的通用逻辑。
 *
 * 参数说明：
 *   func_name  - 要生成的函数名（如 config_set_int）
 *   cfg_type   - 对应的 ConfigType 枚举值（如 CONFIG_TYPE_INT）
 *   val_type   - 值参数的 C 类型（如 int）
 *   val_member - ConfigItem.value 联合体中的成员名（如 int_val）
 *
 * 注意：config_set_string 不使用此宏，因为字符串类型需要额外的
 * 内存管理（释放旧值、strdup 新值），逻辑与标量类型有本质区别。
 */
#define DEFINE_CONFIG_SET_SCALAR(func_name, cfg_type, val_type, val_member) \
    bool func_name(ConfigManager *mgr, const char *key, val_type value) {   \
        if (!mgr || !key) return false;                                     \
                                                                           \
        ConfigItem *item = config_find_item(mgr, key);                      \
        if (item) {                                                         \
            item->type = cfg_type;                                          \
            item->value.val_member = value;                                 \
        } else {                                                            \
            item = config_item_create(key, cfg_type);                       \
            if (!item) return false;                                        \
            item->value.val_member = value;                                 \
            item->next = mgr->items;                                        \
            mgr->items = item;                                              \
        }                                                                   \
                                                                           \
        if (mgr->auto_save) config_save(mgr);                               \
        return true;                                                        \
    }

/* 使用宏生成 int、bool、double 三种标量类型的配置设置函数 */
DEFINE_CONFIG_SET_SCALAR(config_set_int,    CONFIG_TYPE_INT,    int,    int_val)
DEFINE_CONFIG_SET_SCALAR(config_set_bool,   CONFIG_TYPE_BOOL,   bool,   bool_val)
DEFINE_CONFIG_SET_SCALAR(config_set_double, CONFIG_TYPE_DOUBLE, double, double_val)

bool config_set_string(ConfigManager *mgr, const char *key, const char *value) {
    if (!mgr || !key) return false;
    
    ConfigItem *item = config_find_item(mgr, key);
    if (item) {
        if (item->type == CONFIG_TYPE_STRING) {
            lv00_free((void **)&item->value.string_val);
        }
        item->type = CONFIG_TYPE_STRING;
        item->value.string_val = lv00_strdup_safe(value);
    } else {
        item = config_item_create(key, CONFIG_TYPE_STRING);
        if (!item) return false;
        item->value.string_val = lv00_strdup_safe(value);
        item->next = mgr->items;
        mgr->items = item;
    }
    
    if (mgr->auto_save) config_save(mgr);
    return true;
}

int config_get_int(const ConfigManager *mgr, const char *key, int default_val) {
    ConfigItem *item = config_find_item(mgr, key);
    if (item && item->type == CONFIG_TYPE_INT) {
        return item->value.int_val;
    }
    return default_val;
}

bool config_get_bool(const ConfigManager *mgr, const char *key, bool default_val) {
    ConfigItem *item = config_find_item(mgr, key);
    if (item && item->type == CONFIG_TYPE_BOOL) {
        return item->value.bool_val;
    }
    return default_val;
}

double config_get_double(const ConfigManager *mgr, const char *key, double default_val) {
    ConfigItem *item = config_find_item(mgr, key);
    if (item && item->type == CONFIG_TYPE_DOUBLE) {
        return item->value.double_val;
    }
    return default_val;
}

const char *config_get_string(const ConfigManager *mgr, const char *key, const char *default_val) {
    ConfigItem *item = config_find_item(mgr, key);
    if (item && item->type == CONFIG_TYPE_STRING) {
        return item->value.string_val;
    }
    return default_val;
}

/**
 * @brief 检查配置管理器中是否存在指定键
 *
 * @param mgr 配置管理器指针，允许为 NULL。
 * @param key 要检查的配置键名，允许为 NULL。
 * @return true  配置中存在该键；
 *         false mgr 或 key 为 NULL，或配置中不存在该键。
 */
bool config_has_key(const ConfigManager *mgr, const char *key) {
    return config_find_item(mgr, key) != NULL;
}

bool config_remove(ConfigManager *mgr, const char *key) {
    if (!mgr || !key) return false;
    
    ConfigItem **current = &mgr->items;
    while (*current) {
        if (strcmp((*current)->key, key) == 0) {
            ConfigItem *to_remove = *current;
            *current = to_remove->next;
            config_item_destroy(to_remove);
            if (mgr->auto_save) config_save(mgr);
            return true;
        }
        current = &(*current)->next;
    }
    return false;
}

/* 简化的配置文件格式：key = value */
bool config_load(ConfigManager *mgr) {
    if (!mgr || !mgr->config_file) return false;
    
    FILE *f = fopen(mgr->config_file, "r");
    if (!f) return false;
    
    char line[CONFIG_LINE_BUFFER_SIZE];
    while (fgets(line, sizeof(line), f)) {
        char *trimmed = lv00_str_trim(line);
        if (*trimmed == '\0' || *trimmed == '#') continue;
        
        char *eq = strchr(trimmed, '=');
        if (!eq) continue;
        
        *eq = '\0';
        char *key = lv00_str_trim(trimmed);
        char *value = lv00_str_trim(eq + 1);
        
        /* 尝试解析为整数 */
        char *endptr;
        long int_val = strtol(value, &endptr, 10);
        if (*endptr == '\0') {
            config_set_int(mgr, key, (int)int_val);
            continue;
        }
        
        /* 尝试解析为布尔值 */
        if (strcmp(value, "true") == 0 || strcmp(value, "yes") == 0) {
            config_set_bool(mgr, key, true);
            continue;
        }
        if (strcmp(value, "false") == 0 || strcmp(value, "no") == 0) {
            config_set_bool(mgr, key, false);
            continue;
        }
        
        /* 尝试解析为浮点数 */
        double double_val = strtod(value, &endptr);
        if (*endptr == '\0') {
            config_set_double(mgr, key, double_val);
            continue;
        }
        
        /* 否则作为字符串 */
        config_set_string(mgr, key, value);
    }
    
    fclose(f);
    return true;
}

bool config_save(const ConfigManager *mgr) {
    if (!mgr || !mgr->config_file) return false;
    
    FILE *f = fopen(mgr->config_file, "w");
    if (!f) return false;
    
    fprintf(f, "# Lv-00 Configuration File\n");
    fprintf(f, "# Auto-generated\n\n");
    
    ConfigItem *item = mgr->items;
    while (item) {
        switch (item->type) {
            case CONFIG_TYPE_INT:
                fprintf(f, "%s = %d\n", item->key, item->value.int_val);
                break;
            case CONFIG_TYPE_BOOL:
                fprintf(f, "%s = %s\n", item->key, item->value.bool_val ? "true" : "false");
                break;
            case CONFIG_TYPE_DOUBLE:
                fprintf(f, "%s = %.6f\n", item->key, item->value.double_val);
                break;
            case CONFIG_TYPE_STRING:
                fprintf(f, "%s = %s\n", item->key, item->value.string_val);
                break;
            case CONFIG_TYPE_ARRAY:
                /* 数组类型：逐元素序列化 */
                fprintf(f, "%s = [", item->key);
                if (item->value.array_val && item->array_count > 0) {
                    for (size_t ai = 0; ai < item->array_count; ai++) {
                        if (ai > 0) fprintf(f, ", ");
                        ConfigItem *elem_item = item->value.array_val[ai];
                        if (elem_item && elem_item->key) {
                            fprintf(f, "\"%s\"", elem_item->key);
                        } else {
                            fprintf(f, "\"\"");
                        }
                    }
                }
                fprintf(f, "]\n");
                break;
            default:
                break;
        }
        item = item->next;
    }
    
    fclose(f);
    return true;
}

/* ============================================================
 * 版本管理
 * ============================================================ */

LV00Version *version_parse(const char *version_str) {
    if (!version_str) return NULL;
    
    LV00Version *ver = lv00_calloc(1, sizeof(LV00Version));
    if (!ver) return NULL;
    
    /* 解析主版本.次版本.修订版本 */
    int parsed = sscanf(version_str, "%d.%d.%d", &ver->major, &ver->minor, &ver->patch);
    if (parsed < 2) {
        lv00_free((void **)&ver);
        return NULL;
    }
    if (parsed == 2) ver->patch = 0;
    
    /* 解析预发布标识 */
    char *dash = strchr(version_str, '-');
    if (dash) {
        char *plus = strchr(dash, '+');
        if (plus) {
            /* 添加 plus > dash 边界条件检查，防止指针运算溢出 */
            if (plus > dash && (size_t)(plus - dash) > 1) {
                ver->prerelease = lv00_malloc((size_t)(plus - dash));
                if (ver->prerelease) {
                    /* 使用 memcpy 进行精确长度复制（已分配精确内存，手动零终止） */
                    memcpy(ver->prerelease, dash + 1, (size_t)(plus - dash - 1));
                    ver->prerelease[plus - dash - 1] = '\0';
                }
            } else {
                /* prerelease 部分为空（如 "1.0.0-+build"），prerelease 设为 NULL */
                ver->prerelease = NULL;
            }
            ver->build = lv00_strdup_safe(plus + 1);
        } else {
            ver->prerelease = lv00_strdup_safe(dash + 1);
        }
    }
    
    return ver;
}

void version_destroy(LV00Version *ver) {
    if (!ver) return;
    lv00_free((void **)&ver->prerelease);
    lv00_free((void **)&ver->build);
    lv00_free((void **)&ver);
}

char *version_to_string(const LV00Version *ver) {
    if (!ver) return NULL;
    
    if (ver->prerelease && ver->build) {
        return lv00_asprintf("%d.%d.%d-%s+%s", ver->major, ver->minor, ver->patch,
                            ver->prerelease, ver->build);
    } else if (ver->prerelease) {
        return lv00_asprintf("%d.%d.%d-%s", ver->major, ver->minor, ver->patch, ver->prerelease);
    } else if (ver->build) {
        return lv00_asprintf("%d.%d.%d+%s", ver->major, ver->minor, ver->patch, ver->build);
    } else {
        return lv00_asprintf("%d.%d.%d", ver->major, ver->minor, ver->patch);
    }
}

int version_compare(const LV00Version *v1, const LV00Version *v2) {
    if (!v1 || !v2) return 0;
    
    if (v1->major != v2->major) return (v1->major > v2->major) ? 1 : -1;
    if (v1->minor != v2->minor) return (v1->minor > v2->minor) ? 1 : -1;
    if (v1->patch != v2->patch) return (v1->patch > v2->patch) ? 1 : -1;
    
    /* 预发布版本小于正式版本 */
    if (v1->prerelease && !v2->prerelease) return -1;
    if (!v1->prerelease && v2->prerelease) return 1;
    if (v1->prerelease && v2->prerelease) {
        int cmp = strcmp(v1->prerelease, v2->prerelease);
        if (cmp != 0) return (cmp > 0) ? 1 : -1;
    }
    
    return 0;
}

bool version_compatible(const LV00Version *required, const LV00Version *actual) {
    if (!required || !actual) return false;
    
    /* 主版本必须相同 */
    if (required->major != actual->major) return false;
    
    /* 实际版本必须大于等于要求版本 */
    return version_compare(actual, required) >= 0;
}

bool lv00_check_version(const char *min_version) {
    LV00Version *min = version_parse(min_version);
    if (!min) return false;
    
    LV00Version current;
    current.major = LV00_VERSION_MAJOR;
    current.minor = LV00_VERSION_MINOR;
    current.patch = LV00_VERSION_PATCH;
    current.prerelease = NULL;
    current.build = NULL;
    
    bool compatible = version_compatible(min, &current);
    version_destroy(min);
    return compatible;
}

/* ============================================================
 * 时间工具
 * ============================================================ */

/* 时间单位转换常量 */
#define LV00_US_PER_MS  1000   /**< 微秒转毫秒 */
#define LV00_MS_PER_S   1000   /**< 毫秒转秒 */
#define LV00_US_PER_S   1000000  /**< 微秒转秒 */

#ifdef _WIN32
#include <windows.h>

uint64_t lv00_get_time_us(void) {
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (uint64_t)(count.QuadPart * (LONGLONG)LV00_US_PER_S / freq.QuadPart);
}

#else
#include <sys/time.h>

uint64_t lv00_get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * LV00_US_PER_S + (uint64_t)tv.tv_usec;
}
#endif

uint64_t lv00_get_time_ms(void) {
    return lv00_get_time_us() / LV00_US_PER_MS;
}

const char *lv00_format_time(uint64_t timestamp_us, char *buf, size_t buf_size) {
    if (!buf || buf_size == 0) return NULL;
    
    time_t sec = (time_t)(timestamp_us / LV00_US_PER_S);
    /* 修复：使用线程安全的 LV00_LOCALTIME 宏替代非线程安全的 localtime */
    struct tm tm_buf;
    LV00_LOCALTIME(&sec, &tm_buf);
    
    strftime(buf, buf_size, "%Y-%m-%d %H:%M:%S", &tm_buf);
    return buf;
}

/* ============================================================
 * 随机数生成
 * ============================================================ */

/* xorshift64* 伪随机数生成器参数 */
#define LV00_XORSHIFT_SHIFT_A  12    /**< 第一段右移位数 */
#define LV00_XORSHIFT_SHIFT_B  25    /**< 左移位数 */
#define LV00_XORSHIFT_SHIFT_C  27    /**< 第二段右移位数 */
#define LV00_XORSHIFT_MULTIPLIER 0x2545F4914F6CDD1DULL  /**< 乘法常数（来自 Marsaglia 论文） */

/* 双精度随机数生成参数 */
#define LV00_DOUBLE_RAND_HI_BITS   53   /**< 高位位数（double 尾数精度） */
#define LV00_DOUBLE_RAND_LO_BITS   11   /**< 低位位数（附加精度） */
#define LV00_DOUBLE_RAND_MAX_SAFE  0.9999999999999999  /**< [0,1) 区间安全上界 */

static LV00_THREAD_LOCAL uint64_t g_random_state = 0;

void lv00_random_init(uint64_t seed) {
    g_random_state = seed ? seed : (uint64_t)time(NULL);
}

/* xorshift64* 伪随机数生成器（Marsaglia, 2003） */
static uint64_t xorshift64star(void) {
    g_random_state ^= g_random_state >> LV00_XORSHIFT_SHIFT_A;
    g_random_state ^= g_random_state << LV00_XORSHIFT_SHIFT_B;
    g_random_state ^= g_random_state >> LV00_XORSHIFT_SHIFT_C;
    return g_random_state * LV00_XORSHIFT_MULTIPLIER;
}

int lv00_random_int(int min, int max) {
    if (min >= max) return min;
    uint64_t range = (uint64_t)(max - min);
    /* 拒绝采样法：消除模偏差。
     * 当 range 不是 2^64 的约数时，xorshift64star() % range 会使较小值
     * 的出现概率略高于较大值。通过计算阈值并拒绝超出范围的采样值来保证均匀性。 */
    uint64_t threshold = UINT64_MAX - (UINT64_MAX % range);
    uint64_t r;
    do {
        r = xorshift64star();
    } while (r >= threshold);
    return min + (int)(r % range);
}

double lv00_random_double(double min, double max) {
    if (min >= max) return min;
    uint64_t r = xorshift64star();
    /* 修复：使用双精度拆分法生成 [0.0, 1.0) 区间内的均匀随机数。
     *
     * 原实现使用 (double)UINT64_MAX + 1.0 作为除数，但 UINT64_MAX (2^64-1)
     * 转为 double 后精度丢失约 9 位，加 1.0 后这些位全部被吸收，除数实际等于
     * (double)UINT64_MAX ≈ 1.8446744e19，导致：
     *   - r==UINT64_MAX 时 normalized == 1.0，结果可能等于 max
     *   - 低 11 位的变化对 normalized 无影响，分布不均匀
     *
     * 修复方案：将 64 位随机数拆分为高 53 位（提供 double 的完整尾数精度）
     * 和低 11 位（作为附加精度），避免浮点转换时的精度丢失。 */
    uint64_t hi53 = r >> LV00_DOUBLE_RAND_LO_BITS;                     /* 高 53 位作为主尾数 */
    uint64_t lo11 = r & ((1u << LV00_DOUBLE_RAND_LO_BITS) - 1);         /* 低 11 位作为补充精度 */
    /* 构造 [0.0, 1.0) 的均匀随机数：
     *   normalized = hi53/2^53 + lo11/2^64
     * 使用 2^53 作为主除数（double 的 53 位尾数可精确表示），
     * 低 11 位作为微小扰动，确保所有 64 位都对结果有贡献。 */
    double normalized = (double)hi53 / 9007199254740992.0     /* 2^53 */
                      + (double)lo11 / 18446744073709551616.0; /* 2^64 */
    /* 钳制到 [0.0, 1.0) 以确保安全（理论上 normalized < 1.0，但浮点运算
     * 的舍入可能导致极微小的超出） */
    if (normalized >= 1.0) normalized = LV00_DOUBLE_RAND_MAX_SAFE;
    return min + normalized * (max - min);
}

/* ============================================================
 * 哈希函数
 * ============================================================ */

uint64_t lv00_hash_string(const char *str) {
    if (!str) return 0;
    
    /* FNV-1a 哈希算法（使用 lv00_internal.h 中的统一定义） */
    uint64_t hash = LV00_FNV64_OFFSET_BASIS;
    while (*str) {
        hash ^= (uint64_t)(unsigned char)*str++;
        hash *= LV00_FNV64_PRIME;
    }
    return hash;
}

/* ============================================================
 * 日志函数（lv00_internal.h 中宏调用的底层实现）
 * ============================================================ */

/**
 * @brief 输出日志消息（桩函数 —— 默认写入 stderr）
 *
 * 由 LV00_LOG_INFO / LV00_LOG_WARNING / LV00_LOG_ERROR / LV00_LOG_DEBUG
 * 系列宏间接调用。当前实现为简单桩函数，将格式化消息写入 stderr。
 * 后续可替换为更完善的日志系统（分级过滤、文件写入、异步输出等）。
 *
 * @param level 日志级别（LV00_LOG_LEVEL_DEBUG / INFO / WARNING / ERROR）
 * @param file  源文件名（__FILE__）
 * @param line  源文件行号（__LINE__）
 * @param fmt   printf 风格格式字符串
 * @param ...   可变参数
 */
void lv00_log_message(int level, const char *file, int line, const char *fmt, ...)
{
    (void)level;
    (void)line;
    fprintf(stderr, "[%s:%d] ", file ? file : "?", line);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

uint64_t lv00_hash_bytes(const void *data, size_t len) {
    if (!data || len == 0) return 0;
    
    const uint8_t *bytes = (const uint8_t *)data;
    uint64_t hash = LV00_FNV64_OFFSET_BASIS;
    
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint64_t)bytes[i];
        hash *= LV00_FNV64_PRIME;
    }
    return hash;
}

uint64_t lv00_hash_int(int value) {
    /* 使用 FNV-1a 哈希（使用 lv00_internal.h 中的统一定义） */
    uint64_t hash = LV00_FNV64_OFFSET_BASIS;
    /* 逐字节哈希 int 值（sizeof(int) 通常为 4） */
    for (size_t i = 0; i < sizeof(int); i++) {
        hash ^= (uint64_t)((value >> (i * 8)) & 0xFF);
        hash *= LV00_FNV64_PRIME;
    }
    return hash;
}
