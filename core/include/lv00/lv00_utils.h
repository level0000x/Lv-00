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

#ifndef LV00_LV00_UTILS_H
#define LV00_LV00_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "error_codes.h"

/* LV00_PUBLIC_API 由 lv00.h 或 error_codes.h 定义，此处提供回退 */
#ifndef LV00_PUBLIC_API
#define LV00_PUBLIC_API
#endif

/* ============================================================
 * 内存管理辅助
 * ============================================================ */

/**
 * @brief 安全内存分配 - 自动检查返回值并设置错误码
 * @param size 分配大小（字节）
 * @return 分配的内存指针，失败返回NULL并设置错误码
 */
LV00_PUBLIC_API void *lv00_malloc(size_t size);

/**
 * @brief 安全内存分配并清零
 * @param nmemb 元素个数
 * @param size 每个元素大小
 * @return 分配的内存指针，失败返回NULL并设置错误码
 */
LV00_PUBLIC_API void *lv00_calloc(size_t nmemb, size_t size);

/**
 * @brief 安全内存重新分配
 * @param ptr 原指针
 * @param size 新大小
 * @return 重新分配的内存指针，失败返回NULL并设置错误码
 */
LV00_PUBLIC_API void *lv00_realloc(void *ptr, size_t size);

/**
 * @brief 释放内存并将指针置NULL
 * @param ptr 指向指针的指针
 */
LV00_PUBLIC_API void lv00_free(void **ptr);

/**
 * @brief FFI 兼容的内存释放函数（接受 void* 而非 void**）
 * @param ptr 要释放的内存指针
 * @note 专供 Python ctypes 等外部绑定使用，C 内部代码请使用 lv00_free
 */
LV00_PUBLIC_API void lv00_free_ptr(void *ptr);

/**
 * @brief 批量释放多个指针
 * 用法: lv00_free_many(&p1, &p2, &p3, NULL);
 */
LV00_PUBLIC_API void lv00_free_many(void **first, ...);

/**
 * @brief 自动释放属性（GCC/Clang）
 * 使用示例：
 *   char *buf __attribute__((cleanup(lv00_auto_free))) = malloc(100);
 */
LV00_PUBLIC_API void lv00_auto_free(void *p);

/* ============================================================
 * POISON/MAGIC 内存安全检测
 * ============================================================ */

/**
 * @brief 毒模式值 —— 写入已释放内存，用于检测 use-after-free
 * @note 当调用 lv00_free 时，会将此值写入整个已释放的数据区。
 *       若后续代码读取到 0xDEADBEEF，说明正在访问已释放的内存。
 */
#define LV00_POISON_PATTERN 0xDEADBEEF

/**
 * @brief 分配头魔数 —— 标识存活分配块的起始
 * @note 用于快速检测 double-free 和内存损坏。
 */
#define LV00_MAGIC_HEAD 0xADBEEF01

/**
 * @brief 分配尾魔数 —— 标识存活分配块的末尾
 * @note 写入用户数据区末尾之后，用于检测缓冲区溢出（buffer overflow）。
 */
#define LV00_MAGIC_TAIL 0xADBEEF02

/**
 * @brief 已释放标记 —— 防止 double-free
 * @note 释放内存时，头部魔数被改写为此值。再次释放同一块时魔数不匹配，
 *       从而检测到 double-free 行为。
 */
#define LV00_MAGIC_FREED 0xDEADDEAD

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
LV00_PUBLIC_API bool lv00_memory_check_poison(const void *ptr, size_t size);

/**
 * @brief 检查内存块的魔数完整性
 *
 * 验证指定内存块的头部魔数和尾部魔数是否完整。
 * 头部魔数被破坏说明可能发生了 double-free 或内存损坏；
 * 尾部魔数被破坏说明可能发生了缓冲区溢出。
 *
 * @param ptr 要检查的内存指针（必须由 lv00_malloc 系列函数分配）
 * @return true  魔数完整
 *         false 魔数被破坏
 */
LV00_PUBLIC_API bool lv00_memory_check_magic(const void *ptr);

/**
 * @brief 使能/禁用全局毒模式填充
 * @param enable true 启用，false 禁用
 * @note 默认启用。毒模式填充会增加释放操作的开销，在生产环境中
 *       可禁用以提升性能，但会失去 use-after-free 检测能力。
 *       此设置对整个进程生效。
 */
LV00_PUBLIC_API void lv00_poison_enable(bool enable);

/**
 * @brief 释放由外部库（如GMP）分配的内存
 * @param ptr 指向指针的指针
 * @note 专用于释放使用系统malloc分配的内存（如GMP的mpz_get_str返回值）。
 *       与lv00_free不同，此函数直接调用标准free，适用于非lv00_malloc分配的内存。
 *       释放后会将指针置为NULL。
 * @warning 不要将此函数用于lv00_malloc/calloc分配的内存，否则会导致未定义行为。
 */
LV00_PUBLIC_API void lv00_free_external(void **ptr);

/**
 * @brief 获取毒模式填充的当前状态
 * @return true 已启用，false 已禁用
 */
LV00_PUBLIC_API bool lv00_poison_is_enabled(void);

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
LV00_PUBLIC_API size_t lv00_strlcpy(char *dest, const char *src, size_t dest_size);

/**
 * @brief 安全字符串连接
 * @param dest 目标缓冲区
 * @param src 源字符串
 * @param dest_size 目标缓冲区大小
 * @return 连接后的字符串长度
 */
LV00_PUBLIC_API size_t lv00_strlcat(char *dest, const char *src, size_t dest_size);

/**
 * @brief 创建字符串副本
 * @param str 源字符串
 * @return 新分配的副本，失败返回NULL
 */
LV00_PUBLIC_API char *lv00_strdup_safe(const char *str);

/**
 * @brief lv00_strdup 便捷别名 —— 映射到 lv00_strdup_safe
 *
 * 项目代码中大量使用 lv00_strdup 作为 strdup 的项目封装，
 * 统一映射到 lv00_strdup_safe 以保持一致的错误处理行为。
 *
 * @note 优先使用 lv00_strdup（更短），底层实现为 lv00_strdup_safe。
 */
#ifndef lv00_strdup
#define lv00_strdup lv00_strdup_safe
#endif

/**
 * @brief 格式化字符串（自动分配内存）
 * @param fmt 格式字符串
 * @return 新分配的字符串，失败返回NULL
 */
LV00_PUBLIC_API char *lv00_asprintf(const char *fmt, ...);

/**
 * @brief 检查字符串是否为空或仅包含空白
 */
LV00_PUBLIC_API bool lv00_str_is_blank(const char *str);

/**
 * @brief 去除字符串首尾空白
 * @param str 原字符串（会被修改）
 * @return 去除空白后的字符串指针（可能在原位置）
 */
LV00_PUBLIC_API char *lv00_str_trim(char *str);

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
LV00_PUBLIC_API char *lv00_strncpy(char *dest, const char *src, size_t dest_size);

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
LV00_PUBLIC_API char *lv00_strncat(char *dest, const char *src, size_t dest_size);

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
LV00_PUBLIC_API int lv00_snprintf(char *buf, size_t size, const char *fmt, ...);

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
LV00_PUBLIC_API LV00Array *lv00_array_create(size_t initial_capacity, size_t elem_size);

/**
 * @brief 销毁动态数组
 * @param arr 数组对象
 * @param free_elements 是否同时释放元素
 */
LV00_PUBLIC_API void lv00_array_destroy(LV00Array *arr, bool free_elements);

/**
 * @brief 添加元素到数组
 * @param arr 数组对象
 * @param elem 元素指针
 * @return 是否成功
 */
LV00_PUBLIC_API bool lv00_array_push(LV00Array *arr, void *elem);

/**
 * @brief 从数组移除元素
 * @param arr 数组对象
 * @param index 索引
 * @param free_elem 是否释放元素
 * @return 是否成功
 */
LV00_PUBLIC_API bool lv00_array_remove(LV00Array *arr, size_t index, bool free_elem);

/**
 * @brief 获取数组元素
 */
LV00_PUBLIC_API void *lv00_array_get(const LV00Array *arr, size_t index);

/**
 * @brief 设置数组元素
 */
LV00_PUBLIC_API bool lv00_array_set(LV00Array *arr, size_t index, void *elem);

/**
 * @brief 清空数组
 */
LV00_PUBLIC_API void lv00_array_clear(LV00Array *arr, bool free_elements);

/**
 * @brief 数组排序
 */
void lv00_array_sort(LV00Array *arr, int (*cmp)(const void *, const void *));

/**
 * @brief 在数组中查找元素
 */
LV00_PUBLIC_API int lv00_array_find(const LV00Array *arr, const void *elem);

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
LV00_PUBLIC_API IntArray *int_array_create(size_t initial_capacity);

/**
 * @brief 销毁整数数组
 */
LV00_PUBLIC_API void int_array_destroy(IntArray *arr);

/**
 * @brief 添加整数到数组
 */
LV00_PUBLIC_API bool int_array_push(IntArray *arr, int value);

/**
 * @brief 批量添加整数
 */
LV00_PUBLIC_API bool int_array_push_many(IntArray *arr, const int *values, size_t count);

/**
 * @brief 检查数组中是否包含值
 */
LV00_PUBLIC_API bool int_array_contains(const IntArray *arr, int value);

/**
 * @brief 查找值的索引
 * @return 索引，未找到返回-1
 */
LV00_PUBLIC_API int int_array_index_of(const IntArray *arr, int value);

/**
 * @brief 移除指定值的元素
 * @return 是否成功移除
 */
LV00_PUBLIC_API bool int_array_remove(IntArray *arr, int value);

/**
 * @brief 整数数组排序（升序）
 */
LV00_PUBLIC_API void int_array_sort(IntArray *arr);

/**
 * @brief 创建整数数组的副本
 */
LV00_PUBLIC_API IntArray *int_array_copy(const IntArray *arr);

/**
 * @brief 从C数组创建整数数组
 */
LV00_PUBLIC_API IntArray *int_array_from_carray(const int *data, size_t count);

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
LV00_PUBLIC_API ConfigManager *config_manager_create(const char *config_file);

/**
 * @brief 销毁配置管理器
 */
LV00_PUBLIC_API void config_manager_destroy(ConfigManager *mgr);

/**
 * @brief 加载配置文件
 */
LV00_PUBLIC_API bool config_load(ConfigManager *mgr);

/**
 * @brief 保存配置到文件
 */
LV00_PUBLIC_API bool config_save(const ConfigManager *mgr);

/**
 * @brief 设置配置项
 */
LV00_PUBLIC_API bool config_set_int(ConfigManager *mgr, const char *key, int value);
LV00_PUBLIC_API bool config_set_bool(ConfigManager *mgr, const char *key, bool value);
LV00_PUBLIC_API bool config_set_double(ConfigManager *mgr, const char *key, double value);
LV00_PUBLIC_API bool config_set_string(ConfigManager *mgr, const char *key, const char *value);

/**
 * @brief 获取配置项
 */
LV00_PUBLIC_API int config_get_int(const ConfigManager *mgr, const char *key, int default_val);
LV00_PUBLIC_API bool config_get_bool(const ConfigManager *mgr, const char *key, bool default_val);
LV00_PUBLIC_API double config_get_double(const ConfigManager *mgr, const char *key, double default_val);
LV00_PUBLIC_API const char *config_get_string(const ConfigManager *mgr, const char *key, const char *default_val);

/**
 * @brief 检查配置项是否存在
 */
LV00_PUBLIC_API bool config_has_key(const ConfigManager *mgr, const char *key);

/**
 * @brief 删除配置项
 */
LV00_PUBLIC_API bool config_remove(ConfigManager *mgr, const char *key);

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
LV00_PUBLIC_API LV00Version *version_parse(const char *version_str);

/**
 * @brief 销毁版本结构
 */
LV00_PUBLIC_API void version_destroy(LV00Version *ver);

/**
 * @brief 版本转字符串
 * @return 新分配的字符串
 */
LV00_PUBLIC_API char *version_to_string(const LV00Version *ver);

/**
 * @brief 比较两个版本
 * @return -1: v1 < v2, 0: v1 == v2, 1: v1 > v2
 */
LV00_PUBLIC_API int version_compare(const LV00Version *v1, const LV00Version *v2);

/**
 * @brief 检查版本兼容性
 * @param required 需要的最低版本
 * @param actual 实际版本
 * @return 是否兼容
 */
LV00_PUBLIC_API bool version_compatible(const LV00Version *required, const LV00Version *actual);

/**
 * @brief 检查当前系统版本是否满足要求
 */
LV00_PUBLIC_API bool lv00_check_version(const char *min_version);

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
 LV00_PUBLIC_API *   LV00_ENSURE_ARRAY_CAP(my_arr, my_count, my_capacity, ret_on_fail);
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

/**
 * @brief 安全赋值宏 —— 记录非空指针的覆盖操作
 *
 * 若 ptr 当前非 NULL，则通过 fprintf(stderr, ...) 输出警告日志，
 * 提示可能存在内存泄漏（旧值未被释放就被覆盖）。
 * 然后无条件将 value 赋给 ptr。
 *
 * 使用示例：
 LV00_PUBLIC_API *   LV00_SAFE_ASSIGN(my_ptr, new_ptr);
 *
 * @param ptr  目标指针变量
 * @param value 要赋的值
 */
#define LV00_SAFE_ASSIGN(ptr, value)                                                       \
    do {                                                                                   \
        if ((ptr) != NULL) {                                                               \
            fprintf(stderr, "[WARNING] LV00_SAFE_ASSIGN: 指针 " #ptr                       \
                            " 非空时被覆盖 (0x%p)，可能泄漏 (%s:%d)\n",                     \
                            (const void *)(uintptr_t)(ptr), __FILE__, __LINE__);            \
        }                                                                                  \
        (ptr) = (value);                                                                   \
    } while (0)

/**
 * @brief NULL 检查宏 —— 指针为空时直接返回
 *
 * 检查指针是否为 NULL，若为空则返回 ret。
 * 与 LV00_RETURN_IF_NULL 不同，此宏不设置错误码，适用于
 * 不需要记录错误信息的简单防御场景。
 *
 * 使用示例：
 LV00_PUBLIC_API *   LV00_NULL_CHECK(ptr, -1);
 *
 * @param ptr 要检查的指针
 * @param ret 空指针时的返回值
 */
#define LV00_NULL_CHECK(ptr, ret) \
    do {                          \
        if (!(ptr))               \
            return (ret);         \
    } while (0)

/**
 * @brief 释放并置 NULL 宏 —— 一步完成释放和置空
 *
 * 检查 ptr 是否为 NULL，若非 NULL 则调用 lv00_free 释放内存
 * 并自动将 ptr 置为 NULL。此宏会自动取地址，使用更简单。
 *
 * 使用示例：
 LV00_PUBLIC_API *   LV00_FREE_AND_NULL(ptr);
 *
 * @param ptr 要释放的指针变量（不是指针的地址）
 * @note 此宏展开为 lv00_free((void**)&(ptr))，
 *       ptr 必须是可以取地址的变量（不能是表达式或字面量）。
 */
#define LV00_FREE_AND_NULL(ptr)      \
    do {                             \
        if ((ptr)) {                 \
            lv00_free((void **)&(ptr)); \
        }                            \
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
LV00_PUBLIC_API void lv00_get_memory_stats(MemoryStats *stats);

/**
 * @brief 重置内存统计
 */
LV00_PUBLIC_API void lv00_reset_memory_stats(void);

/**
 * @brief 设置内存使用限制（字节）
 * @param limit 限制值，0表示无限制
 */
LV00_PUBLIC_API void lv00_set_memory_limit(size_t limit);

/**
 * @brief 获取当前内存限制
 */
LV00_PUBLIC_API size_t lv00_get_memory_limit(void);

/**
 * @brief 检查是否超过内存限制
 */
LV00_PUBLIC_API bool lv00_memory_limit_exceeded(void);

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
LV00_PUBLIC_API void *lv00_malloc_bounded(size_t size, size_t max_size);

/**
 * @brief 带溯源信息的追踪分配
 *
 * 与 lv00_malloc 功能相同，但额外记录分配所在的源文件名和行号，
 * 用于内存泄漏报告中的精确定位。
 *
 * @param size 分配大小（字节）
 * @param file 源文件名（通常由 __FILE__ 宏自动填入）
 * @param line 源行号（通常由 __LINE__ 宏自动填入）
 * @return 分配的内存指针，失败返回 NULL
 *
 * @note 推荐使用 LV00_TRACKED_MALLOC(size) 宏代替直接调用此函数，
 *       该宏会自动填入 __FILE__ 和 __LINE__。
 */
LV00_PUBLIC_API void *lv00_malloc_tracked(size_t size, const char *file, int line);

/**
 * @brief 带溯源信息的追踪分配宏
 *
 LV00_PUBLIC_API * 使用方式：void *p = LV00_TRACKED_MALLOC(1024);
 * 自动记录调用点所在的文件和行号，便于内存泄漏定位。
 */
#define LV00_TRACKED_MALLOC(size) lv00_malloc_tracked((size), __FILE__, __LINE__)

/**
 * @brief 带溯源信息的 calloc 宏
 */
#define LV00_TRACKED_CALLOC(nmemb, size) lv00_calloc_tracked((nmemb), (size), __FILE__, __LINE__)

LV00_PUBLIC_API void *lv00_calloc_tracked(size_t nmemb, size_t size, const char *file, int line);

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
LV00_PUBLIC_API int lv00_memory_leak_report(FILE *output);

/* ============================================================
 * 时间工具
 * ============================================================ */

/**
 * @brief 获取当前时间戳（微秒）
 */
LV00_PUBLIC_API uint64_t lv00_get_time_us(void);

/**
 * @brief 获取当前时间戳（毫秒）
 */
LV00_PUBLIC_API uint64_t lv00_get_time_ms(void);

/**
 * @brief 格式化时间戳为字符串
 * @param timestamp_us 微秒时间戳
 * @param buf 缓冲区
 * @param buf_size 缓冲区大小
 * @return 格式化后的字符串
 */
LV00_PUBLIC_API const char *lv00_format_time(uint64_t timestamp_us, char *buf, size_t buf_size);

/* ============================================================
 * 随机数生成
 * ============================================================ */

/**
 * @brief 初始化随机数生成器
 */
LV00_PUBLIC_API void lv00_random_init(uint64_t seed);

/**
 * @brief 生成随机整数
 */
LV00_PUBLIC_API int lv00_random_int(int min, int max);

/**
 * @brief 生成随机双精度浮点数
 */
LV00_PUBLIC_API double lv00_random_double(double min, double max);

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
 * @note 使用 LV00_ARRAY_GROWTH_FACTOR 倍增策略
 */
LV00_PUBLIC_API bool lv00_ensure_capacity(void **arr, int count, int *capacity, size_t elem_size, int min_growth);

/* ============================================================
 * 统一 FNV-1a 哈希函数
 * ============================================================ */

/**
 * @brief FNV-1a 哈希函数
 * @param data 输入数据
 * @param len 数据长度
 * @return 64位哈希值
 */
LV00_PUBLIC_API uint64_t lv00_fnv1a_hash(const void *data, size_t len);

/* ============================================================
 * 哈希函数
 * ============================================================ */

/**
 * @brief 计算字符串哈希值（FNV-1a）
 */
LV00_PUBLIC_API uint64_t lv00_hash_string(const char *str);

/**
 * @brief 计算内存块哈希值
 */
LV00_PUBLIC_API uint64_t lv00_hash_bytes(const void *data, size_t len);

/**
 * @brief 计算整数哈希值
 */
LV00_PUBLIC_API uint64_t lv00_hash_int(int value);

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
typedef void (*Lv00ResourceDestroyFunc)(void *resource);

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
LV00_PUBLIC_API ResourceTracker *lv00_resource_tracker_create(void);

/**
 * @brief 销毁资源追踪器并释放其自身内存（不清理追踪的资源）
 * @param rt 资源追踪器指针的地址
 * @note 此函数仅释放追踪器自身，不调用被追踪资源的销毁函数。
 *       如需清理所有资源，请先调用 lv00_resource_tracker_cleanup。
 */
LV00_PUBLIC_API void lv00_resource_tracker_destroy(ResourceTracker **rt);

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
LV00_PUBLIC_API bool lv00_resource_track(ResourceTracker *rt, void *resource,
                          Lv00ResourceDestroyFunc destroy, const char *name);

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
LV00_PUBLIC_API bool lv00_resource_untrack(ResourceTracker *rt, void *resource);

/**
 * @brief 清理所有被追踪的资源
 *
 * 逆序调用所有被追踪资源的销毁函数（后注册的先销毁），
 * 然后清空追踪列表。常用于错误路径的资源回滚。
 *
 * @param rt 资源追踪器
 */
LV00_PUBLIC_API void lv00_resource_tracker_cleanup(ResourceTracker *rt);

/**
 * @brief 获取当前追踪的资源数量
 * @param rt 资源追踪器
 * @return 追踪的资源数量
 */
LV00_PUBLIC_API int lv00_resource_tracker_count(const ResourceTracker *rt);

#ifdef __cplusplus
}
#endif

#endif /* LV00_LV00_UTILS_H */
