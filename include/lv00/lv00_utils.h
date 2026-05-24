/**
 * @file lv00_utils.h
 * @brief Lv-00 工具函数库 - 提供通用辅助功能和便捷API
 *
 * 本模块提供：
 * - 内存管理辅助函数（安全分配、自动释放）
 * - 字符串处理工具
 * - 数组操作辅助
 * - 配置管理
 * - 版本兼容性检查
 * - 便捷宏和包装函数
 */

#ifndef LV00_UTILS_H
#define LV00_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"

/* ============================================================
 * 内存管理辅助
 * ============================================================ */

/**
 * @brief 安全内存分配 - 自动检查返回值并设置错误码
 * @param size 分配大小（字节）
 * @return 分配的内存指针，失败返回NULL并设置错误码
 */
void *lv00_malloc(size_t size);

/**
 * @brief 安全内存分配并清零
 * @param nmemb 元素个数
 * @param size 每个元素大小
 * @return 分配的内存指针，失败返回NULL并设置错误码
 */
void *lv00_calloc(size_t nmemb, size_t size);

/**
 * @brief 安全内存重新分配
 * @param ptr 原指针
 * @param size 新大小
 * @return 重新分配的内存指针，失败返回NULL并设置错误码
 */
void *lv00_realloc(void *ptr, size_t size);

/**
 * @brief 释放内存并将指针置NULL
 * @param ptr 指向指针的指针
 */
void lv00_free(void **ptr);

/**
 * @brief FFI 兼容的内存释放函数（接受 void* 而非 void**）
 * @param ptr 要释放的内存指针
 * @note 专供 Python ctypes 等外部绑定使用，C 内部代码请使用 lv00_free
 */
void lv00_free_ptr(void *ptr);

/**
 * @brief 批量释放多个指针
 * 用法: lv00_free_many(&p1, &p2, &p3, NULL);
 */
void lv00_free_many(void **first, ...);

/**
 * @brief 自动释放属性（GCC/Clang）
 * 使用示例：
 *   char *buf __attribute__((cleanup(lv00_auto_free))) = malloc(100);
 */
void lv00_auto_free(void *p);

/* ============================================================
 * 字符串处理
 * ============================================================ */

/**
 * @brief 安全字符串复制
 * @param dest 目标缓冲区
 * @param src 源字符串
 * @param dest_size 目标缓冲区大小
 * @return 实际复制的字符数（不含\0）
 */
size_t lv00_strlcpy(char *dest, const char *src, size_t dest_size);

/**
 * @brief 安全字符串连接
 * @param dest 目标缓冲区
 * @param src 源字符串
 * @param dest_size 目标缓冲区大小
 * @return 连接后的字符串长度
 */
size_t lv00_strlcat(char *dest, const char *src, size_t dest_size);

/**
 * @brief 创建字符串副本
 * @param str 源字符串
 * @return 新分配的副本，失败返回NULL
 */
char *lv00_strdup_safe(const char *str);

/**
 * @brief 格式化字符串（自动分配内存）
 * @param fmt 格式字符串
 * @return 新分配的字符串，失败返回NULL
 */
char *lv00_asprintf(const char *fmt, ...);

/**
 * @brief 检查字符串是否为空或仅包含空白
 */
bool lv00_str_is_blank(const char *str);

/**
 * @brief 去除字符串首尾空白
 * @param str 原字符串（会被修改）
 * @return 去除空白后的字符串指针（可能在原位置）
 */
char *lv00_str_trim(char *str);

/* ============================================================
 * 数组操作辅助
 * ============================================================ */

/**
 * @brief 动态数组结构
 */
typedef struct {
    void **data;         /* 元素指针数组 */
    size_t count;        /* 当前元素数 */
    size_t capacity;     /* 容量 */
    size_t elem_size;    /* 元素大小（用于值类型数组） */
    bool store_pointers; /* 是否存储指针 */
} LV00Array;

/**
 * @brief 创建动态数组
 * @param initial_capacity 初始容量
 * @param elem_size 元素大小（值类型数组使用，指针数组传0）
 * @return 动态数组对象
 */
LV00Array *lv00_array_create(size_t initial_capacity, size_t elem_size);

/**
 * @brief 销毁动态数组
 * @param arr 数组对象
 * @param free_elements 是否同时释放元素
 */
void lv00_array_destroy(LV00Array *arr, bool free_elements);

/**
 * @brief 添加元素到数组
 * @param arr 数组对象
 * @param elem 元素指针
 * @return 是否成功
 */
bool lv00_array_push(LV00Array *arr, void *elem);

/**
 * @brief 从数组移除元素
 * @param arr 数组对象
 * @param index 索引
 * @param free_elem 是否释放元素
 * @return 是否成功
 */
bool lv00_array_remove(LV00Array *arr, size_t index, bool free_elem);

/**
 * @brief 获取数组元素
 */
void *lv00_array_get(const LV00Array *arr, size_t index);

/**
 * @brief 设置数组元素
 */
bool lv00_array_set(LV00Array *arr, size_t index, void *elem);

/**
 * @brief 清空数组
 */
void lv00_array_clear(LV00Array *arr, bool free_elements);

/**
 * @brief 数组排序
 */
void lv00_array_sort(LV00Array *arr, int (*cmp)(const void *, const void *));

/**
 * @brief 在数组中查找元素
 */
int lv00_array_find(const LV00Array *arr, const void *elem);

/* ============================================================
 * 整数数组便捷操作
 * ============================================================ */

/**
 * @brief 整数动态数组
 */
typedef struct {
    int *data;
    size_t count;
    size_t capacity;
} IntArray;

/**
 * @brief 创建整数数组
 */
IntArray *int_array_create(size_t initial_capacity);

/**
 * @brief 销毁整数数组
 */
void int_array_destroy(IntArray *arr);

/**
 * @brief 添加整数到数组
 */
bool int_array_push(IntArray *arr, int value);

/**
 * @brief 批量添加整数
 */
bool int_array_push_many(IntArray *arr, const int *values, size_t count);

/**
 * @brief 检查数组中是否包含值
 */
bool int_array_contains(const IntArray *arr, int value);

/**
 * @brief 查找值的索引
 * @return 索引，未找到返回-1
 */
int int_array_index_of(const IntArray *arr, int value);

/**
 * @brief 移除指定值的元素
 * @return 是否成功移除
 */
bool int_array_remove(IntArray *arr, int value);

/**
 * @brief 整数数组排序（升序）
 */
void int_array_sort(IntArray *arr);

/**
 * @brief 创建整数数组的副本
 */
IntArray *int_array_copy(const IntArray *arr);

/**
 * @brief 从C数组创建整数数组
 */
IntArray *int_array_from_carray(const int *data, size_t count);

/* ============================================================
 * 配置管理
 * ============================================================ */

/**
 * @brief 配置项类型
 */
typedef enum {
    CONFIG_TYPE_INT,
    CONFIG_TYPE_BOOL,
    CONFIG_TYPE_DOUBLE,
    CONFIG_TYPE_STRING,
    CONFIG_TYPE_ARRAY
} ConfigType;

/**
 * @brief 配置项
 */
typedef struct ConfigItem {
    char *key;
    ConfigType type;
    union {
        int int_val;
        bool bool_val;
        double double_val;
        char *string_val;
        struct ConfigItem **array_val;
    } value;
    size_t array_count;
    struct ConfigItem *next;
} ConfigItem;

/**
 * @brief 配置管理器
 */
typedef struct {
    ConfigItem *items;
    char *config_file;
    bool auto_save;
} ConfigManager;

/**
 * @brief 创建配置管理器
 */
ConfigManager *config_manager_create(const char *config_file);

/**
 * @brief 销毁配置管理器
 */
void config_manager_destroy(ConfigManager *mgr);

/**
 * @brief 加载配置文件
 */
bool config_load(ConfigManager *mgr);

/**
 * @brief 保存配置到文件
 */
bool config_save(const ConfigManager *mgr);

/**
 * @brief 设置配置项
 */
bool config_set_int(ConfigManager *mgr, const char *key, int value);
bool config_set_bool(ConfigManager *mgr, const char *key, bool value);
bool config_set_double(ConfigManager *mgr, const char *key, double value);
bool config_set_string(ConfigManager *mgr, const char *key, const char *value);

/**
 * @brief 获取配置项
 */
int config_get_int(const ConfigManager *mgr, const char *key, int default_val);
bool config_get_bool(const ConfigManager *mgr, const char *key, bool default_val);
double config_get_double(const ConfigManager *mgr, const char *key, double default_val);
const char *config_get_string(const ConfigManager *mgr, const char *key, const char *default_val);

/**
 * @brief 检查配置项是否存在
 */
bool config_has_key(const ConfigManager *mgr, const char *key);

/**
 * @brief 删除配置项
 */
bool config_remove(ConfigManager *mgr, const char *key);

/* ============================================================
 * 版本管理
 * ============================================================ */

/**
 * @brief 版本结构
 */
typedef struct {
    int major;
    int minor;
    int patch;
    char *prerelease; /* 预发布标识，如 "alpha", "beta" */
    char *build;      /* 构建元数据 */
} LV00Version;

/**
 * @brief 解析版本字符串
 * @param version_str 版本字符串，如 "3.0.0-beta.1"
 * @return 版本结构，失败返回NULL
 */
LV00Version *version_parse(const char *version_str);

/**
 * @brief 销毁版本结构
 */
void version_destroy(LV00Version *ver);

/**
 * @brief 版本转字符串
 * @return 新分配的字符串
 */
char *version_to_string(const LV00Version *ver);

/**
 * @brief 比较两个版本
 * @return -1: v1 < v2, 0: v1 == v2, 1: v1 > v2
 */
int version_compare(const LV00Version *v1, const LV00Version *v2);

/**
 * @brief 检查版本兼容性
 * @param required 需要的最低版本
 * @param actual 实际版本
 * @return 是否兼容
 */
bool version_compatible(const LV00Version *required, const LV00Version *actual);

/**
 * @brief 检查当前系统版本是否满足要求
 */
bool lv00_check_version(const char *min_version);

/* ============================================================
 * 便捷宏
 * ============================================================ */

/** 
 * 获取数组元素个数（向后兼容别名，推荐使用 LV00_ARRAY_COUNT）
 * @note LV00_ARRAY_COUNT 在 lv00_internal.h 中定义为权威版本，
 *       本定义仅为公共头文件中的向后兼容保留。
 */
#ifndef LV00_ARRAY_SIZE
#define LV00_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

/**
 * @brief 安全释放指针并将指针置NULL
 */
#define LV00_SAFE_FREE(ptr)              \
    do {                                 \
        if (ptr) {                       \
            lv00_free((void **) &(ptr)); \
        }                                \
    } while (0)

/**
 * @brief 字符串化宏
 */
#define LV00_STRINGIFY(x) #x
#define LV00_TOSTRING(x) LV00_STRINGIFY(x)

/**
 * @brief 编译时断言
 */
#define LV00_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)

/** 返回两个整数中的较小值 */
static inline int lv00_min_i(int a, int b) {
    return a < b ? a : b;
}
/** 返回两个整数中的较大值 */
static inline int lv00_max_i(int a, int b) {
    return a > b ? a : b;
}
/** 返回两个 size_t 中的较小值 */
static inline size_t lv00_min_z(size_t a, size_t b) {
    return a < b ? a : b;
}
/** 返回两个 size_t 中的较大值 */
static inline size_t lv00_max_z(size_t a, size_t b) {
    return a > b ? a : b;
}

/**
 * @brief 确保数组容量足够——通用动态数组扩容辅助宏
 *
 * @details 项目中多处重复实现了数组扩容逻辑，此宏提供统一的扩容模式。
 * 使用模式：若 count >= capacity，则以 GROWTH_FACTOR 倍率扩容。
 *
 * 使用示例：
 *   LV00_ENSURE_ARRAY_CAP(my_arr, my_count, my_capacity, ret_on_fail);
 *   // 扩容后自动进行 realloc 并更新 capacity
 *
 * @param arr        数组指针（会通过 realloc 更新，类型需为 T*）
 * @param count      当前元素计数
 * @param cap        当前容量
 * @param ret_on_fail 失败时的返回值（典型为 false 或 NULL）
 * @warning 初始化容量为 8，增长因子为 2；失败时自动设置 LV00_ERROR_OUT_OF_MEMORY
 */
#define LV00_ENSURE_ARRAY_CAP(arr, count, cap, ret_on_fail)                  \
    do {                                                                     \
        if ((count) >= (cap)) {                                              \
            size_t _new_cap = (cap) == 0 ? 8 : (cap) * 2;                    \
            void *_new_arr = lv00_realloc((arr), _new_cap * sizeof(*(arr))); \
            if (!_new_arr) {                                                 \
                lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "数组扩容失败");    \
                return (ret_on_fail);                                        \
            }                                                                \
            (arr) = _new_arr;                                                \
            (cap) = _new_cap;                                                \
        }                                                                    \
    } while (0)

/** 通用最小/最大值宏（保留向后兼容，类型安全请使用 lv00_min_i/lv00_max_i） */
#ifndef LV00_MIN
#define LV00_MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef LV00_MAX
#define LV00_MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

/**
 * @brief 范围限制宏（将 val 限制在 [min, max] 区间内）
 */
#define LV00_CLAMP(val, min, max) LV00_MAX(min, LV00_MIN(max, val))

/**
 * @brief 交换两个同类型变量
 */
#define LV00_SWAP(type, a, b) \
    do {                      \
        type _tmp = (a);      \
        (a) = (b);            \
        (b) = _tmp;           \
    } while (0)

/**
 * @brief 检查指针是否为NULL，如果是则返回指定值
 */
#define LV00_RETURN_IF_NULL(ptr, ret)                                                          \
    do {                                                                                       \
        if (!(ptr)) {                                                                          \
            lv00_set_error(LV00_ERROR_NULL_POINTER,                                            \
                           "Null pointer: " #ptr " at " __FILE__ ":" LV00_TOSTRING(__LINE__)); \
            return (ret);                                                                      \
        }                                                                                      \
    } while (0)

/**
 * @brief 检查条件，不满足则返回错误
 */
#define LV00_RETURN_IF_FALSE(cond, err_code, ret)                                                               \
    do {                                                                                                        \
        if (!(cond)) {                                                                                          \
            lv00_set_error((err_code), "Condition failed: " #cond " at " __FILE__ ":" LV00_TOSTRING(__LINE__)); \
            return (ret);                                                                                       \
        }                                                                                                       \
    } while (0)

/**
 * @brief 安全调用函数并检查返回值
 */
#define LV00_SAFE_CALL(func, ret_val, ret)             \
    do {                                               \
        ret_val = (func);                              \
        if (lv00_is_error(lv00_get_last_error_code())) \
            return (ret);                              \
    } while (0)

/* ============================================================
 * 内存使用统计
 * ============================================================ */

typedef struct {
    size_t total_allocated;  /* 总分配内存 */
    size_t total_freed;      /* 总释放内存 */
    size_t current_used;     /* 当前使用内存 */
    size_t peak_used;        /* 峰值使用内存 */
    size_t allocation_count; /* 分配次数 */
    size_t free_count;       /* 释放次数 */
} MemoryStats;

/**
 * @brief 获取内存统计
 */
void lv00_get_memory_stats(MemoryStats *stats);

/**
 * @brief 重置内存统计
 */
void lv00_reset_memory_stats(void);

/**
 * @brief 设置内存使用限制（字节）
 * @param limit 限制值，0表示无限制
 */
void lv00_set_memory_limit(size_t limit);

/**
 * @brief 获取当前内存限制
 */
size_t lv00_get_memory_limit(void);

/**
 * @brief 检查是否超过内存限制
 */
bool lv00_memory_limit_exceeded(void);

/* ============================================================
 * 时间工具
 * ============================================================ */

/**
 * @brief 获取当前时间戳（微秒）
 */
uint64_t lv00_get_time_us(void);

/**
 * @brief 获取当前时间戳（毫秒）
 */
uint64_t lv00_get_time_ms(void);

/**
 * @brief 格式化时间戳为字符串
 * @param timestamp_us 微秒时间戳
 * @param buf 缓冲区
 * @param buf_size 缓冲区大小
 * @return 格式化后的字符串
 */
const char *lv00_format_time(uint64_t timestamp_us, char *buf, size_t buf_size);

/* ============================================================
 * 随机数生成
 * ============================================================ */

/**
 * @brief 初始化随机数生成器
 */
void lv00_random_init(uint64_t seed);

/**
 * @brief 生成随机整数
 */
int lv00_random_int(int min, int max);

/**
 * @brief 生成随机双精度浮点数
 */
double lv00_random_double(double min, double max);

/* ============================================================
 * 哈希函数
 * ============================================================ */

/**
 * @brief 计算字符串哈希值（FNV-1a）
 */
uint64_t lv00_hash_string(const char *str);

/**
 * @brief 计算内存块哈希值
 */
uint64_t lv00_hash_bytes(const void *data, size_t len);

/**
 * @brief 计算整数哈希值
 */
uint64_t lv00_hash_int(int value);

#ifdef __cplusplus
}
#endif

#endif /* LV00_UTILS_H */
