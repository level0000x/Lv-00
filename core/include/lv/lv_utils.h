/**
 * @file lv_utils.h
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

#ifndef lv_lv_UTILS_H
#define lv_lv_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "error_codes.h"

#ifndef lv_PUBLIC_API
#define lv_PUBLIC_API
#endif

/* ============================================================
 * 内存管理辅助
 * ============================================================ */

/**
 * @brief 安全内存分配 - 自动检查返回值并设置错误码
 * @param size 分配大小（字节）
 * @return 分配的内存指针，失败返回NULL并设置错误码
 */
lv_PUBLIC_API void *lv_malloc(size_t size);

/**
 * @brief 安全内存分配并清零
 * @param nmemb 元素个数
 * @param size 每个元素大小
 * @return 分配的内存指针，失败返回NULL并设置错误码
 */
lv_PUBLIC_API void *lv_calloc(size_t nmemb, size_t size);

/**
 * @brief 安全内存重新分配
 * @param ptr 原指针
 * @param size 新大小
 * @return 重新分配的内存指针，失败返回NULL并设置错误码
 */
lv_PUBLIC_API void *lv_realloc(void *ptr, size_t size);

/**
 * @brief 释放内存并将指针置NULL
 * @param ptr 指向指针的指针
 */
lv_PUBLIC_API void lv_free(void **ptr);

/**
 * @brief FFI 兼容的内存释放函数（接受 void* 而非 void**）
 * @param ptr 要释放的内存指针
 * @note 专供 Python ctypes 等外部绑定使用，C 内部代码请使用 lv_free
 */
lv_PUBLIC_API void lv_free_ptr(void *ptr);

/**
 * @brief 批量释放多个指针
 * 用法: lv_free_many(&p1, &p2, &p3, NULL);
 */
lv_PUBLIC_API void lv_free_many(void **first, ...);

/**
 * @brief 自动释放属性（GCC/Clang）
 * 使用示例：
 *   char *buf __attribute__((cleanup(lv_auto_free))) = malloc(100);
 */
lv_PUBLIC_API void lv_auto_free(void *p);

/* ============================================================
 * POISON/MAGIC 内存安全检测
 * ============================================================ */

/**
 * @brief 毒模式值 —— 写入已释放内存，用于检测 use-after-free
 * @note 当调用 lv_free 时，会将此值写入整个已释放的数据区。
 *       若后续代码读取到 0xDEADBEEF，说明正在访问已释放的内存。
 */
#define lv_POISON_PATTERN 0xDEADBEEF

/**
 * @brief 分配头魔数 —— 标识存活分配块的起始
 * @note 用于快速检测 double-free 和内存损坏。
 */
#define lv_MAGIC_HEAD 0xADBEEF01

/**
 * @brief 分配尾魔数 —— 标识存活分配块的末尾
 * @note 写入用户数据区末尾之后，用于检测缓冲区溢出（buffer overflow）。
 */
#define lv_MAGIC_TAIL 0xADBEEF02

/**
 * @brief 已释放标记 —— 防止 double-free
 * @note 释放内存时，头部魔数被改写为此值。再次释放同一块时魔数不匹配，
 *       从而检测到 double-free 行为。
 */
#define lv_MAGIC_FREED 0xDEADDEAD

/**
 * @brief 检查内存块的 poison 标记是否完整
 *
 * 扫描指定内存区域，检测是否包含毒模式（即是否已被释放）。
 * 用于调试 use-after-free 类型的内存错误。
 *
 * @param ptr 要检查的内存指针
 * @param size 检查的字节数
 * @return true  未发现毒模式（内存完整）
 *         false 发现毒模式（内存可能已被释放）
 */
lv_PUBLIC_API bool lv_memory_check_poison(const void *ptr, size_t size);

/**
 * @brief 检查内存块的魔数完整性
 *
 * 验证指定内存块的头部魔数和尾部魔数是否完整。
 * 头部魔数被破坏说明可能发生了 double-free 或内存损坏；
 * 尾部魔数被破坏说明可能发生了缓冲区溢出。
 *
 * @param ptr 要检查的内存指针（必须由 lv_malloc 系列函数分配）
 * @return true  魔数完整
 *         false 魔数被破坏
 */
lv_PUBLIC_API bool lv_memory_check_magic(const void *ptr);

/**
 * @brief 使能/禁用全局毒模式填充
 * @param enable true 启用，false 禁用
 * @note 默认启用。毒模式填充会增加释放操作的开销，在生产环境中
 *       可禁用以提升性能，但会失去 use-after-free 检测能力。
 *       此设置对整个进程生效。
 */
lv_PUBLIC_API void lv_poison_enable(bool enable);

/**
 * @brief 释放由外部库（如GMP）分配的内存
 * @param ptr 指向指针的指针
 * @note 专用于释放使用系统malloc分配的内存（如GMP的mpz_get_str返回值）。
 *       与lv_free不同，此函数直接调用标准free，适用于非lv_malloc分配的内存。
 *       释放后会将指针置为NULL。
 * @warning 不要将此函数用于lv_malloc/calloc分配的内存，否则会导致未定义行为。
 */
lv_PUBLIC_API void lv_free_external(void **ptr);

/**
 * @brief 获取毒模式填充的当前状态
 * @return true 已启用，false 已禁用
 */
lv_PUBLIC_API bool lv_poison_is_enabled(void);

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
lv_PUBLIC_API size_t lv_strlcpy(char *dest, const char *src, size_t dest_size);

/**
 * @brief 安全字符串连接
 * @param dest 目标缓冲区
 * @param src 源字符串
 * @param dest_size 目标缓冲区大小
 * @return 连接后的字符串长度
 */
lv_PUBLIC_API size_t lv_strlcat(char *dest, const char *src, size_t dest_size);

/**
 * @brief 创建字符串副本
 * @param str 源字符串
 * @return 新分配的副本，失败返回NULL
 */
lv_PUBLIC_API char *lv_strdup_safe(const char *str);

/**
 * @brief lv_strdup 便捷别名 —— 映射到 lv_strdup_safe
 *
 * 项目代码中大量使用 lv_strdup 作为 strdup 的项目封装，
 * 统一映射到 lv_strdup_safe 以保持一致的错误处理行为。
 *
 * @note 优先使用 lv_strdup（更短），底层实现为 lv_strdup_safe。
 */
#ifndef lv_strdup
#define lv_strdup lv_strdup_safe
#endif

/**
 * @brief 格式化字符串（自动分配内存）
 * @param fmt 格式字符串
 * @return 新分配的字符串，失败返回NULL
 */
lv_PUBLIC_API char *lv_asprintf(const char *fmt, ...);

/**
 * @brief 检查字符串是否为空或仅包含空白
 */
lv_PUBLIC_API bool lv_str_is_blank(const char *str);

/**
 * @brief 去除字符串首尾空白
 * @param str 原字符串（会被修改）
 * @return 去除空白后的字符串指针（可能在原位置）
 */
lv_PUBLIC_API char *lv_str_trim(char *str);

/**
 * @brief 安全字符串复制 —— 保证 \0 终止并检查参数有效性
 *
 * 与标准 strncpy 不同：
 * - 始终以 \0 终止目标字符串（即使截断）
 * - 参数为 NULL 时安全返回 NULL（不崩溃）
 * - dest_size 为 0 时安全返回 NULL
 *
 * @param dest 目标缓冲区
 * @param src  源字符串（可为 NULL）
 * @param dest_size 目标缓冲区大小（字节）
 * @return 成功时返回 dest，失败时返回 NULL
 */
lv_PUBLIC_API char *lv_strncpy(char *dest, const char *src, size_t dest_size);

/**
 * @brief 安全字符串连接 —— 保证 \0 终止并检查参数有效性
 *
 * 将 src 连接到 dest 末尾，确保不溢出 dest 缓冲区。
 * 行为类似于标准 strncat，但：
 * - 始终以 \0 终止目标字符串
 * - 参数为 NULL 时安全返回 NULL
 * - dest 已满时仍保证 \0 终止
 *
 * @param dest 目标缓冲区（必须已包含一个有效的 \0 终止字符串）
 * @param src  源字符串（可为 NULL）
 * @param dest_size 目标缓冲区总大小（字节）
 * @return 成功时返回 dest，失败时返回 NULL
 */
lv_PUBLIC_API char *lv_strncat(char *dest, const char *src, size_t dest_size);

/**
 * @brief 安全格式化输出到定长缓冲区
 *
 * 与标准 snprintf 相同的行为，但额外：
 * - 参数为 NULL 时安全返回 -1（不崩溃）
 * - 始终保证 \0 终止
 *
 * @param buf  输出缓冲区
 * @param size 缓冲区大小
 * @param fmt  格式字符串
 * @param ...  可变参数
 * @return 成功时返回写入的字符数（不含 \0），失败返回 -1
 */
lv_PUBLIC_API int lv_snprintf(char *buf, size_t size, const char *fmt, ...);

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
} lvArray;

/**
 * @brief 创建动态数组
 * @param initial_capacity 初始容量
 * @param elem_size 元素大小（值类型数组使用，指针数组传0）
 * @return 动态数组对象
 */
lv_PUBLIC_API lvArray *lv_array_create(size_t initial_capacity, size_t elem_size);

/**
 * @brief 销毁动态数组
 * @param arr 数组对象
 * @param free_elements 是否同时释放元素
 */
lv_PUBLIC_API void lv_array_destroy(lvArray *arr, bool free_elements);

/**
 * @brief 添加元素到数组
 * @param arr 数组对象
 * @param elem 元素指针
 * @return 是否成功
 */
lv_PUBLIC_API bool lv_array_push(lvArray *arr, void *elem);

/**
 * @brief 从数组移除元素
 * @param arr 数组对象
 * @param index 索引
 * @param free_elem 是否释放元素
 * @return 是否成功
 */
lv_PUBLIC_API bool lv_array_remove(lvArray *arr, size_t index, bool free_elem);

/**
 * @brief 获取数组元素
 */
lv_PUBLIC_API void *lv_array_get(const lvArray *arr, size_t index);

/**
 * @brief 设置数组元素
 */
lv_PUBLIC_API bool lv_array_set(lvArray *arr, size_t index, void *elem);

/**
 * @brief 清空数组
 */
lv_PUBLIC_API void lv_array_clear(lvArray *arr, bool free_elements);

/**
 * @brief 数组排序
 */
void lv_array_sort(lvArray *arr, int (*cmp)(const void *, const void *));

/**
 * @brief 通用插入排序（小数组，n 通常很小）
 * @param base 数组起始地址
 * @param n    元素个数
 * @param elem_size 元素字节大小
 * @param cmp  比较回调（同 qsort 语义：<0 表示 a 在前）
 * @param ctx  透传给 cmp 的上下文（可为 NULL）
 */
void lv_insertion_sort(void *base, size_t n, size_t elem_size,
                       int (*cmp)(const void *a, const void *b, void *ctx), void *ctx);

/**
 * @brief 在数组中查找元素
 */
lv_PUBLIC_API int lv_array_find(const lvArray *arr, const void *elem);

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
lv_PUBLIC_API IntArray *int_array_create(size_t initial_capacity);

/**
 * @brief 销毁整数数组
 */
lv_PUBLIC_API void int_array_destroy(IntArray *arr);

/**
 * @brief 添加整数到数组
 */
lv_PUBLIC_API bool int_array_push(IntArray *arr, int value);

/**
 * @brief 批量添加整数
 */
lv_PUBLIC_API bool int_array_push_many(IntArray *arr, const int *values, size_t count);

/**
 * @brief 检查数组中是否包含值
 */
lv_PUBLIC_API bool int_array_contains(const IntArray *arr, int value);

/**
 * @brief 查找值的索引
 * @return 索引，未找到返回-1
 */
lv_PUBLIC_API int int_array_index_of(const IntArray *arr, int value);

/**
 * @brief 移除指定值的元素
 * @return 是否成功移除
 */
lv_PUBLIC_API bool int_array_remove(IntArray *arr, int value);

/**
 * @brief 整数数组排序（升序）
 */
lv_PUBLIC_API void int_array_sort(IntArray *arr);

/**
 * @brief 创建整数数组的副本
 */
lv_PUBLIC_API IntArray *int_array_copy(const IntArray *arr);

/**
 * @brief 从C数组创建整数数组
 */
lv_PUBLIC_API IntArray *int_array_from_carray(const int *data, size_t count);

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
lv_PUBLIC_API ConfigManager *config_manager_create(const char *config_file);

/**
 * @brief 销毁配置管理器
 */
lv_PUBLIC_API void config_manager_destroy(ConfigManager *mgr);

/**
 * @brief 加载配置文件
 */
lv_PUBLIC_API bool config_load(ConfigManager *mgr);

/**
 * @brief 保存配置到文件
 */
lv_PUBLIC_API bool config_save(const ConfigManager *mgr);

/**
 * @brief 设置配置项
 */
lv_PUBLIC_API bool config_set_int(ConfigManager *mgr, const char *key, int value);
lv_PUBLIC_API bool config_set_bool(ConfigManager *mgr, const char *key, bool value);
lv_PUBLIC_API bool config_set_double(ConfigManager *mgr, const char *key, double value);
lv_PUBLIC_API bool config_set_string(ConfigManager *mgr, const char *key, const char *value);

/**
 * @brief 获取配置项
 */
lv_PUBLIC_API int config_get_int(const ConfigManager *mgr, const char *key, int default_val);
lv_PUBLIC_API bool config_get_bool(const ConfigManager *mgr, const char *key, bool default_val);
lv_PUBLIC_API double config_get_double(const ConfigManager *mgr, const char *key, double default_val);
lv_PUBLIC_API const char *config_get_string(const ConfigManager *mgr, const char *key, const char *default_val);

/**
 * @brief 检查配置项是否存在
 */
lv_PUBLIC_API bool config_has_key(const ConfigManager *mgr, const char *key);

/**
 * @brief 删除配置项
 */
lv_PUBLIC_API bool config_remove(ConfigManager *mgr, const char *key);

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
} lvVersion;

/**
 * @brief 解析版本字符串
 * @param version_str 版本字符串，如 "3.0.0-beta.1"
 * @return 版本结构，失败返回NULL
 */
lv_PUBLIC_API lvVersion *version_parse(const char *version_str);

/**
 * @brief 销毁版本结构
 */
lv_PUBLIC_API void version_destroy(lvVersion *ver);

/**
 * @brief 版本转字符串
 * @return 新分配的字符串
 */
lv_PUBLIC_API char *version_to_string(const lvVersion *ver);

/**
 * @brief 比较两个版本
 * @return -1: v1 < v2, 0: v1 == v2, 1: v1 > v2
 */
lv_PUBLIC_API int version_compare(const lvVersion *v1, const lvVersion *v2);

/**
 * @brief 检查版本兼容性
 * @param required 需要的最低版本
 * @param actual 实际版本
 * @return 是否兼容
 */
lv_PUBLIC_API bool version_compatible(const lvVersion *required, const lvVersion *actual);

/**
 * @brief 检查当前系统版本是否满足要求
 */
lv_PUBLIC_API bool lv_check_version(const char *min_version);

/* ============================================================
 * 便捷宏
 * ============================================================ */

/** 
 * 获取数组元素个数（向后兼容别名，推荐使用 lv_ARRAY_COUNT）
 * @note lv_ARRAY_COUNT 在 lv_internal.h 中定义为权威版本，
 *       本定义仅为公共头文件中的向后兼容保留。
 */
#ifndef lv_ARRAY_SIZE
#define lv_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

/**
 * @brief 安全释放指针并将指针置NULL
 *
 * 注意：使用内联函数替代宏，避免宏参数被多次求值的副作用。
 */
static inline void lv_safe_free(void **ptr) {
    if (ptr && *ptr) {
        lv_free(ptr);
    }
}

/* 兼容旧代码的宏（已废弃，建议直接使用 lv_safe_free 函数） */
#define lv_SAFE_FREE(ptr) lv_safe_free((void **) &(ptr))

/**
 * @brief 字符串化宏
 */
#define lv_STRINGIFY(x) #x
#define lv_TOSTRING(x) lv_STRINGIFY(x)

/**
 * @brief 编译时断言
 */
#define lv_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)

/** 返回两个整数中的较小值 */
static inline int lv_min_i(int a, int b) {
    return a < b ? a : b;
}
/** 返回两个整数中的较大值 */
static inline int lv_max_i(int a, int b) {
    return a > b ? a : b;
}
/** 返回两个 size_t 中的较小值 */
static inline size_t lv_min_z(size_t a, size_t b) {
    return a < b ? a : b;
}
/** 返回两个 size_t 中的较大值 */
static inline size_t lv_max_z(size_t a, size_t b) {
    return a > b ? a : b;
}

/**
 * @brief 确保数组容量足够——通用动态数组扩容辅助宏
 *
 * @details 项目中多处重复实现了数组扩容逻辑，此宏提供统一的扩容模式。
 * 使用模式：若 count >= capacity，则以 GROWTH_FACTOR 倍率扩容。
 *
 * 使用示例：
 lv_PUBLIC_API *   lv_ENSURE_ARRAY_CAP(my_arr, my_count, my_capacity, ret_on_fail);
 *   // 扩容后自动进行 realloc 并更新 capacity
 *
 * @param arr        数组指针（会通过 realloc 更新，类型需为 T*）
 * @param count      当前元素计数
 * @param cap        当前容量
 * @param ret_on_fail 失败时的返回值（典型为 false 或 NULL）
 * @warning 初始化容量为 8，增长因子为 2；失败时自动设置 lv_ERROR_OUT_OF_MEMORY
 */
#define lv_ENSURE_ARRAY_CAP(arr, count, cap, ret_on_fail)                  \
    do {                                                                   \
        if ((count) >= (cap)) {                                            \
            size_t _new_cap = (cap) == 0 ? 8 : (cap) * 2;                  \
            void *_new_arr = lv_realloc((arr), _new_cap * sizeof(*(arr))); \
            if (!_new_arr) {                                               \
                lv_set_error(lv_ERROR_OUT_OF_MEMORY, "数组扩容失败");      \
                return (ret_on_fail);                                      \
            }                                                              \
            (arr) = _new_arr;                                              \
            (cap) = _new_cap;                                              \
        }                                                                  \
    } while (0)

/** 通用最小/最大值宏（保留向后兼容，类型安全请使用 lv_min_i/lv_max_i） */
#ifndef lv_MIN
#define lv_MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef lv_MAX
#define lv_MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

/**
 * @brief 范围限制宏（将 val 限制在 [min, max] 区间内）
 */
#define lv_CLAMP(val, min, max) lv_MAX(min, lv_MIN(max, val))

/**
 * @brief 交换两个同类型变量
 */
#define lv_SWAP(type, a, b) \
    do {                    \
        type _tmp = (a);    \
        (a) = (b);          \
        (b) = _tmp;         \
    } while (0)

/**
 * @brief 检查指针是否为NULL，如果是则返回指定值
 */
#define lv_RETURN_IF_NULL(ptr, ret)                                                                               \
    do {                                                                                                          \
        if (!(ptr)) {                                                                                             \
            lv_set_error(lv_ERROR_NULL_POINTER, "Null pointer: " #ptr " at " __FILE__ ":" lv_TOSTRING(__LINE__)); \
            return (ret);                                                                                         \
        }                                                                                                         \
    } while (0)

/**
 * @brief 检查条件，不满足则返回错误
 */
#define lv_RETURN_IF_FALSE(cond, err_code, ret)                                                             \
    do {                                                                                                    \
        if (!(cond)) {                                                                                      \
            lv_set_error((err_code), "Condition failed: " #cond " at " __FILE__ ":" lv_TOSTRING(__LINE__)); \
            return (ret);                                                                                   \
        }                                                                                                   \
    } while (0)

/**
 * @brief 安全调用函数并检查返回值
 */
#define lv_SAFE_CALL(func, ret_val, ret)           \
    do {                                           \
        ret_val = (func);                          \
        if (lv_is_error(lv_get_last_error_code())) \
            return (ret);                          \
    } while (0)

/**
 * @brief 安全赋值宏 —— 记录非空指针的覆盖操作
 *
 * 若 ptr 当前非 NULL，则通过 lv_LOG_WARNING 输出警告日志，
 * 提示可能存在内存泄漏（旧值未被释放就被覆盖）。
 * 然后无条件将 value 赋给 ptr。
 *
 * 使用示例：
 lv_PUBLIC_API *   lv_SAFE_ASSIGN(my_ptr, new_ptr);
 *
 * @param ptr  目标指针变量
 * @param value 要赋的值
 */
#define lv_SAFE_ASSIGN(ptr, value)                                                        \
    do {                                                                                  \
        if ((ptr) != NULL) {                                                              \
            lv_LOG_WARNING("lv_SAFE_ASSIGN: 指针 " #ptr " 非空时被覆盖 (0x%p)，可能泄漏", \
                           (const void *) (uintptr_t) (ptr));                             \
        }                                                                                 \
        (ptr) = (value);                                                                  \
    } while (0)

/**
 * @brief NULL 检查宏 —— 指针为空时直接返回
 *
 * 检查指针是否为 NULL，若为空则返回 ret。
 * 与 lv_RETURN_IF_NULL 不同，此宏不设置错误码，适用于
 * 不需要记录错误信息的简单防御场景。
 *
 * 使用示例：
 lv_PUBLIC_API *   lv_NULL_CHECK(ptr, -1);
 *
 * @param ptr 要检查的指针
 * @param ret 空指针时的返回值
 */
#define lv_NULL_CHECK(ptr, ret) \
    do {                        \
        if (!(ptr))             \
            return (ret);       \
    } while (0)

/**
 * @brief 释放并置 NULL 宏 —— 一步完成释放和置空
 *
 * 检查 ptr 是否为 NULL，若非 NULL 则调用 lv_free 释放内存
 * 并自动将 ptr 置为 NULL。此宏会自动取地址，使用更简单。
 *
 * 使用示例：
 lv_PUBLIC_API *   lv_FREE_AND_NULL(ptr);
 *
 * @param ptr 要释放的指针变量（不是指针的地址）
 * @note 此宏展开为 lv_free((void**)&(ptr))，
 *       ptr 必须是可以取地址的变量（不能是表达式或字面量）。
 */
#define lv_FREE_AND_NULL(ptr)          \
    do {                               \
        if ((ptr)) {                   \
            lv_free((void **) &(ptr)); \
        }                              \
    } while (0)

/* ============================================================
 * 作用域退出清理（defer）
 * ============================================================ */

/** @brief 作用域退出清理回调类型 */
typedef void (*lvDeferFn)(void *arg);

/** @brief defer 槽位：注册的清理回调与其参数 */
typedef struct {
    lvDeferFn fn;
    void *arg;
} lvDeferSlot;

/** @brief cleanup 属性回调：槽位变量离开作用域时执行注册的清理 */
static inline void lv_defer_slot_cleanup(void *p) {
    lvDeferSlot *slot = (lvDeferSlot *) p;
    if (slot && slot->fn)
        slot->fn(slot->arg);
}

/**
 * @brief 作用域退出时执行清理（类似 C 的 defer）
 *
 * 用法：
 * @code
 *   char *buf = lv_malloc(100);
 *   LV_DEFER(lv_defer_free_ptr, &buf);
 *   if (cond)
 *       return NULL;   // buf 在返回时自动释放
 *   ...
 * @endcode
 *
 * 清理注册顺序与执行顺序相反（后注册的先执行）。
 * 注意：同一作用域内只能注册一个 LV_DEFER。
 *
 * @note 基于 GCC/Clang 的 __attribute__((cleanup)) 实现，
 *       槽位变量离开作用域（含任何 return）时自动执行 fn(arg)。
 * @warning MSVC 不支持 cleanup 属性：在 MSVC 下该宏展开为空操作
 *          （不执行任何清理），需手动清理或使用 goto cleanup 模式。
 */
#if defined(__GNUC__) || defined(__clang__)
#define LV_DEFER(fn, arg) \
    lvDeferSlot _lv_defer_slot_ __attribute__((cleanup(lv_defer_slot_cleanup))) = {(fn), (arg)}
#else
#define LV_DEFER(fn, arg) ((void) (fn), (void) (arg))
#endif

/** @brief 常用清理回调：释放一个指针变量（配合 LV_DEFER(lv_defer_free_ptr, &ptr) 使用） */
static inline void lv_defer_free_ptr(void *arg) {
    if (!arg)
        return;
    void **pp = (void **) arg;
    if (*pp)
        lv_free(pp);
}

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
lv_PUBLIC_API void lv_get_memory_stats(MemoryStats *stats);

/**
 * @brief 重置内存统计
 */
lv_PUBLIC_API void lv_reset_memory_stats(void);

/**
 * @brief 设置内存使用限制（字节）
 * @param limit 限制值，0表示无限制
 */
lv_PUBLIC_API void lv_set_memory_limit(size_t limit);

/**
 * @brief 获取当前内存限制
 */
lv_PUBLIC_API size_t lv_get_memory_limit(void);

/**
 * @brief 检查是否超过内存限制
 */
lv_PUBLIC_API bool lv_memory_limit_exceeded(void);

/* ============================================================
 * 边界检查分配与内存溯源
 * ============================================================ */

/**
 * @brief 带最大大小限制的安全内存分配
 *
 * 防止请求过大内存导致系统不稳定。若请求大小超过 max_size，
 * 返回 NULL 并设置错误码。
 *
 * @param size 请求分配的大小（字节）
 * @param max_size 允许的最大分配大小（字节）
 * @return 分配的内存指针，超限或失败返回 NULL
 */
lv_PUBLIC_API void *lv_malloc_bounded(size_t size, size_t max_size);

/**
 * @brief 带溯源信息的追踪分配
 *
 * 与 lv_malloc 功能相同，但额外记录分配所在的源文件名和行号，
 * 用于内存泄漏报告中的精确定位。
 *
 * @param size 分配大小（字节）
 * @param file 源文件名（通常由 __FILE__ 宏自动填入）
 * @param line 源行号（通常由 __LINE__ 宏自动填入）
 * @return 分配的内存指针，失败返回 NULL
 *
 * @note 推荐使用 lv_TRACKED_MALLOC(size) 宏代替直接调用此函数，
 *       该宏会自动填入 __FILE__ 和 __LINE__。
 */
lv_PUBLIC_API void *lv_malloc_tracked(size_t size, const char *file, int line);

/**
 * @brief 带溯源信息的追踪分配宏
 *
 lv_PUBLIC_API * 使用方式：void *p = lv_TRACKED_MALLOC(1024);
 * 自动记录调用点所在的文件和行号，便于内存泄漏定位。
 */
#define lv_TRACKED_MALLOC(size) lv_malloc_tracked((size), __FILE__, __LINE__)

/**
 * @brief 带溯源信息的 calloc 宏
 */
#define lv_TRACKED_CALLOC(nmemb, size) lv_calloc_tracked((nmemb), (size), __FILE__, __LINE__)

lv_PUBLIC_API void *lv_calloc_tracked(size_t nmemb, size_t size, const char *file, int line);

/**
 * @brief 打印所有未释放的内存分配报告
 *
 * 遍历内部追踪链表，列出所有至今未被释放的内存块，
 * 包括分配大小、源文件和行号（若有记录）。
 * 此函数通常在主函数退出前调用，以检测内存泄漏。
 *
 * @param output 输出流，NULL 表示输出到 stderr
 * @return 泄漏的分配数量（0 表示无泄漏）
 */
lv_PUBLIC_API int lv_memory_leak_report(FILE *output);

/* ============================================================
 * 时间工具
 * ============================================================ */

/**
 * @brief 获取当前时间戳（纳秒）
 *
 * 高精度计时器，适用于性能分析。不保证挂钟时间精度。
 * Windows: QueryPerformanceCounter, POSIX: clock_gettime(CLOCK_MONOTONIC)
 */
lv_PUBLIC_API uint64_t lv_get_time_ns(void);

/**
 * @brief 获取当前时间戳（微秒）
 */
lv_PUBLIC_API uint64_t lv_get_time_us(void);

/**
 * @brief 获取当前时间戳（毫秒）
 */
lv_PUBLIC_API uint64_t lv_get_time_ms(void);

/**
 * @brief 获取墙钟时间（纳秒，CLOCK_REALTIME / GetSystemTimeAsFileTime）
 */
lv_PUBLIC_API uint64_t lv_get_wallclock_ns(void);

/**
 * @brief 获取墙钟时间（毫秒，Unix epoch）
 */
lv_PUBLIC_API uint64_t lv_get_wallclock_ms(void);

/**
 * @brief 格式化时间戳为字符串
 * @param timestamp_us 微秒时间戳
 * @param buf 缓冲区
 * @param buf_size 缓冲区大小
 * @return 格式化后的字符串
 */
lv_PUBLIC_API const char *lv_format_time(uint64_t timestamp_us, char *buf, size_t buf_size);

/**
 * @brief clock() 起点到当前经过的秒数（CPU 时间）
 * @param start clock() 起点
 * @return 经过的秒数
 */
static inline double lv_clock_elapsed_sec(clock_t start) {
    return (double) (clock() - start) / CLOCKS_PER_SEC;
}

/**
 * @brief clock() 起点到当前经过的毫秒数
 * @param start clock() 起点
 * @return 经过的毫秒数
 */
static inline double lv_clock_elapsed_ms(clock_t start) {
    return (double) (clock() - start) / CLOCKS_PER_SEC * 1000.0;
}

/**
 * @brief clock() 起点到当前经过的微秒数
 * @param start clock() 起点
 * @return 经过的微秒数
 */
static inline double lv_clock_elapsed_us(clock_t start) {
    return (double) (clock() - start) / CLOCKS_PER_SEC * 1000000.0;
}

/* ============================================================
 * 随机数生成
 * ============================================================ */

/**
 * @brief 初始化随机数生成器
 */
lv_PUBLIC_API void lv_random_init(uint64_t seed);

/**
 * @brief 生成随机整数
 */
lv_PUBLIC_API int lv_random_int(int min, int max);

/**
 * @brief 生成随机双精度浮点数
 */
lv_PUBLIC_API double lv_random_double(double min, double max);

/* ============================================================
 * 统一数组扩容
 * ============================================================ */

/**
 * @brief 确保动态数组有足够的容量
 * @param arr 当前数组指针（可能被 realloc）
 * @param count 当前元素数量
 * @param capacity 当前容量指针（会被更新）
 * @param elem_size 每个元素的大小
 * @param min_growth 最小增长量
 * @return 成功返回 true，失败返回 false
 * @note 使用 lv_ARRAY_GROWTH_FACTOR 倍增策略
 */
lv_PUBLIC_API bool lv_ensure_capacity(void **arr, int count, int *capacity, size_t elem_size, int min_growth);

/* ============================================================
 * 统一 FNV-1a 哈希函数
 * ============================================================ */

/**
 * @brief FNV-1a 哈希函数
 * @param data 输入数据
 * @param len 数据长度
 * @return 64位哈希值
 */
lv_PUBLIC_API uint64_t lv_fnv1a_hash(const void *data, size_t len);

/**
 * @brief FNV-1a 增量哈希：给定已有哈希值，混入 data/len，返回新哈希值
 * @param hash 已有哈希值（增量起点通常为 lv_FNV64_OFFSET_BASIS）
 * @param data 待混入的数据（可为 NULL，此时原样返回 hash）
 * @param len  数据长度（字节数）
 * @return 混入后的新哈希值
 */
lv_PUBLIC_API uint64_t lv_fnv1a_update(uint64_t hash, const void *data, size_t len);

/**
 * @brief FNV-1a 字符串哈希（增量起点）
 * @param s 输入字符串（可为 NULL，等价于空字符串，返回偏移基值）
 * @return 64位哈希值
 */
lv_PUBLIC_API uint64_t lv_fnv1a_hash_str(const char *s);

/**
 * @brief FNV-1a 整数增量哈希
 * @param hash 已有哈希值
 * @param v    待混入的 64 位整数（按 sizeof(uint64_t) 字节混入）
 * @return 混入后的新哈希值
 */
lv_PUBLIC_API uint64_t lv_fnv1a_hash_int(uint64_t hash, uint64_t v);

/* ============================================================
 * 字节序工具（显式大端/小端读写，不依赖主机字节序）
 * ============================================================ */

/**
 * @brief 按大端序写入 16 位整数
 * @param dst 目标缓冲区（至少 2 字节）
 * @param v   要写入的值
 */
lv_PUBLIC_API void lv_store_be16(uint8_t *dst, uint16_t v);

/**
 * @brief 按大端序写入 32 位整数
 * @param dst 目标缓冲区（至少 4 字节）
 * @param v   要写入的值
 */
lv_PUBLIC_API void lv_store_be32(uint8_t *dst, uint32_t v);

/**
 * @brief 按大端序读取 16 位整数
 * @param src 源缓冲区（至少 2 字节）
 * @return 读取的值
 */
lv_PUBLIC_API uint16_t lv_load_be16(const uint8_t *src);

/**
 * @brief 按大端序读取 32 位整数
 * @param src 源缓冲区（至少 4 字节）
 * @return 读取的值
 */
lv_PUBLIC_API uint32_t lv_load_be32(const uint8_t *src);

/**
 * @brief 按小端序写入 16 位整数
 * @param dst 目标缓冲区（至少 2 字节）
 * @param v   要写入的值
 */
lv_PUBLIC_API void lv_store_le16(uint8_t *dst, uint16_t v);

/**
 * @brief 按小端序写入 32 位整数
 * @param dst 目标缓冲区（至少 4 字节）
 * @param v   要写入的值
 */
lv_PUBLIC_API void lv_store_le32(uint8_t *dst, uint32_t v);

/**
 * @brief 按小端序读取 16 位整数
 * @param src 源缓冲区（至少 2 字节）
 * @return 读取的值
 */
lv_PUBLIC_API uint16_t lv_load_le16(const uint8_t *src);

/**
 * @brief 按小端序读取 32 位整数
 * @param src 源缓冲区（至少 4 字节）
 * @return 读取的值
 */
lv_PUBLIC_API uint32_t lv_load_le32(const uint8_t *src);

/* ============================================================
 * qsort 比较器
 * ============================================================ */

/**
 * @brief qsort 用 int 比较器（返回值 -(a<b) + (a>b)，无溢出）
 * @param a 指向第一个 int 的指针
 * @param b 指向第二个 int 的指针
 * @return 负数（a < b）、零（a == b）、正数（a > b）
 */
lv_PUBLIC_API int lv_cmp_int(const void *a, const void *b);

/**
 * @brief qsort 用 uint64_t 比较器
 * @param a 指向第一个 uint64_t 的指针
 * @param b 指向第二个 uint64_t 的指针
 * @return 负数（a < b）、零（a == b）、正数（a > b）
 */
lv_PUBLIC_API int lv_cmp_uint64(const void *a, const void *b);

/* ============================================================
 * 线程局部临时缓冲区（scratch）
 * ============================================================ */

/**
 * @brief 获取线程局部临时字符串缓冲区（scratch 语义，勿跨调用保存）
 * @param min_size 所需最小字节数
 * @return 指向 TLS 缓冲区的指针（至少 min_size 字节，可重复调用）
 */
lv_PUBLIC_API char *lv_scratch_buf(size_t min_size);

/**
 * @brief 线程局部临时格式化：结果写入 TLS 缓冲区
 * @param fmt printf 风格格式字符串
 * @param ... 可变参数
 * @return 指向 TLS 缓冲区的格式化字符串（scratch 语义，勿跨调用保存）
 */
lv_PUBLIC_API char *lv_fmt_tmp(const char *fmt, ...);

/* ============================================================
 * 数值数组聚合
 * ============================================================ */

/**
 * @brief 返回数组绝对值的最大值（空数组返回 0）
 * @param arr 输入数组（可为 NULL）
 * @param n   元素个数
 * @return 绝对值的最大值；arr 为 NULL 或 n <= 0 时返回 0
 */
lv_PUBLIC_API double lv_max_abs(const double *arr, int64_t n);

/**
 * @brief 返回 double 数组最大值
 * @param arr 输入数组（可为 NULL）
 * @param n   元素个数
 * @return 最大值；arr 为 NULL 或 n <= 0 时返回 0
 */
lv_PUBLIC_API double lv_max_d(const double *arr, int64_t n);

/* ============================================================
 * 哈希函数
 * ============================================================ */

/**
 * @brief 计算字符串哈希值（FNV-1a）
 */
lv_PUBLIC_API uint64_t lv_hash_string(const char *str);

/**
 * @brief 计算内存块哈希值
 */
lv_PUBLIC_API uint64_t lv_hash_bytes(const void *data, size_t len);

/**
 * @brief 计算整数哈希值
 */
lv_PUBLIC_API uint64_t lv_hash_int(int value);

/* ============================================================
 * 资源追踪器
 * ============================================================ */

/**
 * @brief 通用资源销毁回调函数类型
 *
 * 每个被追踪的资源都需要一个对应的销毁函数，
 * 在资源追踪器清理时被调用以释放资源。
 *
 * @param resource 资源指针
 */
typedef void (*lvResourceDestroyFunc)(void *resource);

/**
 * @brief 资源追踪器结构
 *
 * 维护一个资源（不仅仅是内存，还包括文件句柄、互斥锁等）的链表，
 * 在发生错误需要清理或程序退出时，统一调用所有资源的销毁函数。
 *
 * 典型使用场景：
 * - 函数中打开了多个文件，中途出错需要全部关闭
 * - 多层嵌套的资源分配，确保全部释放
 */
typedef struct ResourceTracker ResourceTracker;

/**
 * @brief 创建资源追踪器
 * @return 新创建的资源追踪器指针，失败返回 NULL
 */
lv_PUBLIC_API ResourceTracker *lv_resource_tracker_create(void);

/**
 * @brief 销毁资源追踪器并释放其自身内存（不清理追踪的资源）
 * @param rt 资源追踪器指针的地址
 * @note 此函数仅释放追踪器自身，不调用被追踪资源的销毁函数。
 *       如需清理所有资源，请先调用 lv_resource_tracker_cleanup。
 */
lv_PUBLIC_API void lv_resource_tracker_destroy(ResourceTracker **rt);

/**
 * @brief 追踪一个资源
 *
 * 将资源及其销毁函数注册到追踪器中。
 * 若 resource 为 NULL 或 destroy 为 NULL，注册失败返回 false。
 *
 * @param rt 资源追踪器
 * @param resource 资源指针
 * @param destroy 销毁回调函数
 * @param name 资源名称（用于调试输出，可为 NULL）
 * @return true 追踪成功，false 失败
 */
lv_PUBLIC_API bool lv_resource_track(ResourceTracker *rt, void *resource, lvResourceDestroyFunc destroy,
                                     const char *name);

/**
 * @brief 取消追踪一个资源
 *
 * 从追踪器中移除指定资源，但不销毁资源本身。
 * 常用于资源已成功移交给调用者或其他管理器的场景。
 *
 * @param rt 资源追踪器
 * @param resource 要取消追踪的资源指针
 * @return true 成功取消追踪，false 资源未被追踪
 */
lv_PUBLIC_API bool lv_resource_untrack(ResourceTracker *rt, void *resource);

/**
 * @brief 清理所有被追踪的资源
 *
 * 逆序调用所有被追踪资源的销毁函数（后注册的先销毁），
 * 然后清空追踪列表。常用于错误路径的资源回滚。
 *
 * @param rt 资源追踪器
 */
lv_PUBLIC_API void lv_resource_tracker_cleanup(ResourceTracker *rt);

/**
 * @brief 获取当前追踪的资源数量
 * @param rt 资源追踪器
 * @return 追踪的资源数量
 */
lv_PUBLIC_API int lv_resource_tracker_count(const ResourceTracker *rt);

/* ============================================================
 * 浮点精度常量
 * ============================================================ */

/**
 * @brief 通用 double 精度阈值
 * 用于一般浮点比较（判零、判相等）
 */
#ifndef lv_EPSILON_DOUBLE
#define lv_EPSILON_DOUBLE 1e-12
#endif

/**
 * @brief 数值比较精度阈值
 * 用于代数数隔离区间比较、数值验证等
 */
#ifndef lv_EPSILON_NUMERIC_COMPARE
#define lv_EPSILON_NUMERIC_COMPARE 1e-10
#endif

/**
 * @brief Newton 迭代收敛阈值
 * 用于代数数求根、区间收缩等 Newton 法迭代
 */
#ifndef lv_EPSILON_NEWTON
#define lv_EPSILON_NEWTON 1e-14
#endif

/**
 * @brief 分数判零阈值
 * 用于连分数展开等场景的分数值判零
 */
#ifndef lv_EPSILON_FRACTION_ZERO
#define lv_EPSILON_FRACTION_ZERO 1e-15
#endif

/**
 * @brief 线段内点判定阈值
 * 用于判断点是否在线段内部（排除端点）
 */
#ifndef lv_EPSILON_SEGMENT_INTERIOR
#define lv_EPSILON_SEGMENT_INTERIOR 1e-9
#endif

/* ============================================================
 * 动态字符串（lv_dstr）—— 统一的可变长度字符串构建器
 *
 * 支持按需扩容（倍增策略）、printf 风格格式化追加、
 * 原始字节追加和 C 字符串追加。
 * ============================================================ */

/** 动态字符串构建器 */
typedef struct {
    char *data;   /**< 缓冲区 */
    size_t len;   /**< 当前长度（不含终止符） */
    size_t cap;   /**< 缓冲区总容量 */
} lvDStr;

/** 动态字符串默认初始容量 */
#define lv_DSTR_INIT_CAP 4096

lv_PUBLIC_API int lv_dstr_init(lvDStr *d, size_t cap);
lv_PUBLIC_API int lv_dstr_grow(lvDStr *d, size_t extra);
lv_PUBLIC_API int lv_dstr_append_fmt(lvDStr *d, const char *fmt, ...);
lv_PUBLIC_API int lv_dstr_append_raw(lvDStr *d, const char *s, size_t n);
lv_PUBLIC_API int lv_dstr_append_str(lvDStr *d, const char *s);
lv_PUBLIC_API void lv_dstr_free(lvDStr *d);

/* ============================================================
 * lvDArray —— 泛型动态数组容器
 *
 * 使用 lv_ensure_capacity 做底层扩容，提供类型安全的
 * 元素追加/获取/清空操作。所有元素按 elem_size 字节 memcpy，
 * 适合存储基本类型和小型结构体。
 *
 * 使用示例：
 *   lvDArray arr;
 *   lv_darray_init(&arr, sizeof(int));
 *   int val = 42;
 *   lv_darray_push(&arr, &val);    // 自动扩容
 *   int *got = (int *)lv_darray_get(&arr, 0);
 *   lv_darray_free(&arr);
 * ============================================================ */

/** 动态数组状态 */
typedef struct {
    void *data;       /**< 数据缓冲区（通过 lv_realloc 管理） */
    int count;        /**< 当前元素数量 */
    int capacity;     /**< 当前缓冲区容量（元素个数） */
    size_t elem_size; /**< 每个元素的字节大小 */
} lvDArray;

/**
 * @brief 初始化动态数组
 * @param arr       数组指针
 * @param elem_size 每个元素的字节大小
 */
lv_PUBLIC_API void lv_darray_init(lvDArray *arr, size_t elem_size);

/**
 * @brief 释放动态数组内部缓冲区，重置为零
 */
lv_PUBLIC_API void lv_darray_free(lvDArray *arr);

/**
 * @brief 确保数组至少有 count 个元素的容量
 * @return true 成功，false 内存不足
 */
lv_PUBLIC_API bool lv_darray_reserve(lvDArray *arr, int count);

/**
 * @brief 向末尾追加一个元素（memcpy 复制内容）
 * @return 元素索引（arr->count - 1），失败返回 -1
 */
lv_PUBLIC_API int lv_darray_push(lvDArray *arr, const void *elem);

/**
 * @brief 弹出末尾元素（仅递减 count，不释放内存）
 */
lv_PUBLIC_API void lv_darray_pop(lvDArray *arr);

/**
 * @brief 获取元素指针
 * @return 第 index 个元素的指针，越界返回 NULL
 */
lv_PUBLIC_API void *lv_darray_get(const lvDArray *arr, int index);

/**
 * @brief 清空数组（count 归零，保留缓冲区）
 */
lv_PUBLIC_API void lv_darray_clear(lvDArray *arr);

#ifdef __cplusplus
}
#endif

#endif /* lv_lv_UTILS_H */
