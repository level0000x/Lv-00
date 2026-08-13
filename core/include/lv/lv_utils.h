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

/* lv_str_trim 等字符串工具函数的规范声明位于 lv_str_utils.h，
 * 此处包含以保持 lv_utils.h 使用方的既有可见性（避免重复声明）。 */
#include "lv_str_utils.h"

/* 数值容差的分级权威定义（lv_EPSILON_SUPERTINY/ULTRA/HIGH/MEDIUM/LOW、
 * lv_GEO_*_EPSILON、lv_SINGULARITY_THRESHOLD 等）位于 config.h；
 * 本文件的 epsilon 常量改为语义别名引用，数值去重（值不变，仅去重定义）。
 * 收敛说明：lv_EPSILON_DOUBLE（1e-12）为通用 double 阈值权威之一，
 * 与 config.h 的 lv_EPSILON_ULTRA（1e-12）同值，二者均保留为数值来源。 */
#include "config.h"

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
 * @brief 定长安全字符串复制（源不要求 NUL 终止）
 * @param dest 目标缓冲区
 * @param dest_size 目标缓冲区大小
 * @param src 源数据指针（可为非 NUL 终止的定长子串）
 * @param src_len 源数据长度
 * @return 请求复制的源长度 src_len；调用方可比较 src_len >= dest_size 检测截断
 * @note 等价于 memcpy + 手写 NUL 终止的样板；复制 min(src_len, dest_size-1) 字节并保证 NUL 终止
 */
lv_PUBLIC_API size_t lv_strlcpy_n(char *dest, size_t dest_size, const char *src, size_t src_len);

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
lv_PUBLIC_API bool lv_str_is_blank(const char *s);

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
 * @brief 从紧凑数组中删除下标 index 处的元素（memmove 整体前移）
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
void lv_shift_left(void *base, size_t elem_size, size_t index, size_t count);

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
void lv_shift_right(void *base, size_t elem_size, size_t index, size_t count);

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
void lv_buffer_consume(void *buf, size_t elem_size, size_t pos, size_t *len);

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
int lv_int_multiset_equal(const int *a, int an, const int *b, int bn);

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
bool lv_int_append_unique(int *arr, int *count, int value);

/* ============================================================
 * 位掩码内联助手（消除手写 1<<n 有符号移位的 UB 隐患）
 * ============================================================ */

/** @brief 生成单个位掩码（1u<<bit，避免 1<<31 符号位 UB） */
static inline unsigned lv_bit_mask(unsigned bit) {
    return 1u << bit;
}

/** @brief 生成低 nbits 位全 1 掩码（替代手写 (1<<N)-1；nbits>=32 时饱和为全 1） */
static inline unsigned lv_mask_all(unsigned nbits) {
    return nbits >= 32 ? 0xFFFFFFFFu : (1u << nbits) - 1u;
}

/** @brief 置位（mask 指向的掩码中设置 bit 位） */
static inline void lv_mask_set(unsigned *mask, unsigned bit) {
    *mask |= 1u << bit;
}

/** @brief 清位（mask 指向的掩码中清除 bit 位） */
static inline void lv_mask_clear(unsigned *mask, unsigned bit) {
    *mask &= ~(1u << bit);
}

/** @brief 测试位 */
static inline bool lv_mask_test(unsigned mask, unsigned bit) {
    return (mask & (1u << bit)) != 0;
}

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

/**
 * @brief 深拷贝裸整数数组（lv_malloc + memcpy）
 *
 * 分配 count 个 int 的新数组并复制源数组内容。
 * src 为 NULL 或 count <= 0 时返回 NULL；含整数溢出保护。
 * 作为 func_block_copy / func_block_clone 等模块手写
 * "lv_malloc + memcpy + 失败回滚" 并行实现的统一收敛入口
 * （func_block_utils.c 的 dup_int_array / preset_common.c 的
 * lv_dup_int_array 均委托本函数）。
 *
 * @param src   源数组（可为 NULL）
 * @param count 元素个数
 * @return 新分配的整数数组，失败返回 NULL
 */
lv_PUBLIC_API int *lv_copy_int_array(const int *src, int count);

/**
 * @brief 深拷贝裸指针数组（lv_malloc + memcpy）
 *
 * 分配 count 个 void* 的新数组并复制源数组内容（浅拷贝元素指针本身，
 * 所有权语义由调用方约定；func_block_clone 中用于 internal_nodes 指针数组）。
 * src 为 NULL 或 count <= 0 时返回 NULL；含整数溢出保护。
 *
 * @param src   源指针数组（可为 NULL）
 * @param count 元素个数
 * @return 新分配的指针数组，失败返回 NULL
 */
lv_PUBLIC_API void **lv_copy_ptr_array(void *const *src, int count);

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
 * @brief 返回两个 int 的三态升序比较结果（qsort 风格，值版）
 *
 * 语义契约：比较两个 int 值，返回 a<b 为 -1、a==b 为 0、a>b 为 +1；
 *           不修改任何状态，不分配资源。
 * 前置条件：无（a、b 为任意合法 int）。
 * 失败/截断语义：纯比较，无失败通道。
 * 边界行为：INT_MIN 与 INT_MAX 比较正确返回 -1，无有符号减法溢出
 *          （区别于 `ra->priority - rb->priority` 的 UB 形态）。
 * 扩展点：无（降序比较由调用方取负；uint64 三态比较已有 lv_cmp_uint64）。
 *
 * @note 收敛对象（判据 A）：六个「priority 升序」qsort 比较器
 *       （backend_plugin / module / routing / type_inference / rewrite /
 *       theory），原三分支、`a-b`、`(a>b)-(a<b)` 三种形态统一为本设施；
 *       `a-b` 形态的 engine_scheduler.rule_compare 与 smt_theory_combiner
 *       为缺陷修复（消除有符号溢出）。
 */
static inline int lv_cmp_int_asc(int a, int b) {
    return (a > b) - (a < b);
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
 *
 * @note LV_DEFER 为 lv_DEFER 的兼容别名（权威定义位于 lv_lifecycle.h:72）。
 *       与旧版固定槽位变量不同，lv_DEFER 基于 __COUNTER__ 唯一化变量名，
 *       同一作用域可注册多个，不再受"只能注册一个"限制。
 * @warning MSVC 不支持 cleanup 属性：在 MSVC 下该宏展开为空操作
 *          （不执行任何清理），需手动清理或使用 goto cleanup 模式。
 */
#define LV_DEFER lv_DEFER

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

/* ============================================================
 * 时间单位换算常量
 *
 * 语义常量族（批次 Q6）：时间单位换算一律使用具名常量，
 * 禁止裸字面量（1000/1000000/1000000000 等）。
 * ============================================================ */
#define lv_NS_PER_US 1000ULL        /**< 纳秒 → 微秒 */
#define lv_NS_PER_MS 1000000ULL     /**< 纳秒 → 毫秒 */
#define lv_US_PER_MS 1000           /**< 微秒 → 毫秒 */
#define lv_MS_PER_S 1000            /**< 毫秒 → 秒 */
#define lv_US_PER_S 1000000         /**< 微秒 → 秒 */
#define lv_NS_PER_S 1000000000ULL   /**< 纳秒 → 秒 */

/* ============================================================
 * 哈希黄金比乘数常量
 *
 * 语义常量族（批次 Q14）：哈希混合乘数一律使用具名常量，
 * 禁止裸字面量（0x9E3779B9 等）。
 * ============================================================ */
#define lv_HASH_GOLDEN_RATIO_64 0x9E3779B97F4A7C15ULL /**< 64 位黄金比（Knuth 乘法哈希） */
#define lv_HASH_GOLDEN_RATIO_32 0x9E3779B9ULL         /**< 32 位黄金比（Knuth 乘法哈希） */

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
    return (double) (clock() - start) / CLOCKS_PER_SEC * (double) lv_MS_PER_S;
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
 * lvTlsVector —— TLS 指针 + count + capacity 三件套
 *
 * 收敛散落各模块的「TLS 指针 + 计数 + 容量」手写样板
 * （unify_equivalence / formula_converter_util / lv_scratch_buf 等）：
 * 变量声明统一为 `static lv_THREAD_LOCAL lvTlsVector g_xxx = {0};`，
 * 扩容走 lv_ensure_capacity 倍增语义；线程退出或系统清理时调用
 * lv_tls_vector_cleanup 释放堆缓冲区，防止池化线程中的 TLS 堆表永久泄漏。
 * ============================================================ */

/** @brief TLS 动态向量：线程局部指针 + 元素计数 + 容量三件套 */
typedef struct {
    void *ptr;     /**< 数据缓冲区（由 lv_realloc 管理，零初始化时为 NULL） */
    int count;     /**< 当前元素数量 */
    int capacity;  /**< 当前缓冲区容量（元素个数） */
} lvTlsVector;

/**
 * @brief 确保 TLS 向量容量至少可容纳 need_count 个元素（lv_ensure_capacity 倍增策略）
 * @param v         向量指针（一般指向 lv_THREAD_LOCAL 静态变量）
 * @param need_count 需求元素个数（count+1 或按需字节数，scratch 场景 elem_size=1）
 * @param elem_size  每个元素的字节大小
 * @return true 成功（可直接写入），false 内存不足（原缓冲区保持不变）
 */
lv_PUBLIC_API bool lv_tls_vector_ensure(lvTlsVector *v, int need_count, size_t elem_size);

/**
 * @brief 清空元素计数（保留缓冲区供复用，等价于原样板只把 count 归零）
 */
lv_PUBLIC_API void lv_tls_vector_clear(lvTlsVector *v);

/**
 * @brief 释放缓冲区并重置为零（线程退出 / lv_cleanup 时调用，防泄漏）
 */
lv_PUBLIC_API void lv_tls_vector_cleanup(lvTlsVector *v);

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

/**
 * @brief 按大端序写入 64 位整数
 * @param dst 目标缓冲区（至少 8 字节）
 * @param v   要写入的值
 */
lv_PUBLIC_API void lv_store_be64(uint8_t *dst, uint64_t v);

/**
 * @brief 按大端序读取 64 位整数
 * @param src 源缓冲区（至少 8 字节）
 * @return 读取的值
 */
lv_PUBLIC_API uint64_t lv_load_be64(const uint8_t *src);

/**
 * @brief 按小端序写入 64 位整数
 * @param dst 目标缓冲区（至少 8 字节）
 * @param v   要写入的值
 */
lv_PUBLIC_API void lv_store_le64(uint8_t *dst, uint64_t v);

/**
 * @brief 按小端序读取 64 位整数
 * @param src 源缓冲区（至少 8 字节）
 * @return 读取的值
 */
lv_PUBLIC_API uint64_t lv_load_le64(const uint8_t *src);

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

/**
 * @brief 释放当前线程的 scratch 缓冲区（lv_cleanup 时调用，防 TLS 泄漏）
 * @note 池化工作线程退出时无 TLS 析构钩子，其副本由线程生命周期持有；
 *       主线程副本在此函数中回收。
 */
lv_PUBLIC_API void lv_scratch_buf_cleanup(void);

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
 *
 * 数值权威之一（1e-12，与 config.h 的 lv_EPSILON_ULTRA 同值）；
 * 其余 1e-12 语义常量（lv_ZERO_EPSILON、GEO_EVENT_DEFAULT_TOL、
 * PCTL_EPSILON、GROEBNER_SMT_ZERO_THRESHOLD 等）统一引用本宏。
 */
#ifndef lv_EPSILON_DOUBLE
#define lv_EPSILON_DOUBLE 1e-12
#endif

/**
 * @brief 数值比较精度阈值
 * 用于代数数隔离区间比较、数值验证等
 *
 * 语义别名 = config.h lv_EPSILON_HIGH（1e-10）。
 */
#ifndef lv_EPSILON_NUMERIC_COMPARE
#define lv_EPSILON_NUMERIC_COMPARE lv_EPSILON_HIGH
#endif

/**
 * @brief Newton 迭代收敛阈值
 * 用于代数数求根、区间收缩等 Newton 法迭代
 *
 * 独立值（1e-14，分级体系无对应档）；与 groebner 引擎的
 * GROEBNER_NEWTON_TOL（1e-12，= lv_EPSILON_ULTRA）场景不同，保留各自值。
 */
#ifndef lv_EPSILON_NEWTON
#define lv_EPSILON_NEWTON 1e-14
#endif

/**
 * @brief 分数判零阈值
 * 用于连分数展开等场景的分数值判零
 *
 * 语义别名 = config.h lv_EPSILON_SUPERTINY（1e-15）。
 */
#ifndef lv_EPSILON_FRACTION_ZERO
#define lv_EPSILON_FRACTION_ZERO lv_EPSILON_SUPERTINY
#endif

/**
 * @brief 线段内点判定阈值
 * 用于判断点是否在线段内部（排除端点）
 *
 * 语义别名 = config.h lv_EPSILON_MEDIUM（1e-9）。
 */
#ifndef lv_EPSILON_SEGMENT_INTERIOR
#define lv_EPSILON_SEGMENT_INTERIOR lv_EPSILON_MEDIUM
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
    void (*elem_destroy)(void *); /**< 元素析构回调（可为 NULL） */
} lvDArray;

/**
 * @brief 初始化动态数组
 * @param arr       数组指针
 * @param elem_size 每个元素的字节大小
 */
lv_PUBLIC_API void lv_darray_init(lvDArray *arr, size_t elem_size);

/**
 * @brief 以元素析构回调初始化动态数组
 * @param arr       数组指针
 * @param elem_size 每个元素的字节大小
 * @param dtor      元素析构回调（可为 NULL，lv_darray_free 时逐元素调用）
 */
lv_PUBLIC_API void lv_darray_init_with_dtor(lvDArray *arr, size_t elem_size, void (*dtor)(void *));

/**
 * @brief 释放动态数组内部缓冲区，重置为零
 * @note 若通过 lv_darray_init_with_dtor 设置了 elem_destroy，
 *       释放前会逐元素调用析构回调（无 dtor 时行为与旧版一致）
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

/* ============================================================
 * lv_dirty_set —— 去重 int 元素集合
 *
 * 收敛项目中多套"元素集合 + 含属 + 遍历/清空"脏标记数据结构：
 *   - block_scheduler 的 dirty_blocks（int 数组）
 *   - incremental_exec 的 validity_bitmap（位图，语义取反后等价）
 *   - view_synchronizer 的 dirty_views（int 数组）
 *   - solver 侧 DirtyVariableSet（layer4，lvDArray 版，与此同构）
 *
 * 语义与 DirtyVariableSet 完全一致（lvDArray of int + 线性查重 add），
 * 但为 header-only 静态内联实现，layer6 各模块直接复用无需引入
 * solver 依赖：
 *   - add:      已存在则忽略（幂等去重）
 *   - contains: 线性包含查询
 *   - clear:    计数归零、保留容量（供复用）
 *   - count/at: 按插入顺序遍历
 * ============================================================ */

/** 脏元素集合：包装 lvDArray（int 元素） */
typedef struct {
    lvDArray ids; /**< lvDArray of int */
} lv_dirty_set;

/**
 * @brief 初始化脏集合（内部为 int 元素 lvDArray）
 * @param ds 集合指针（可为零初始化后直接使用）
 */
static inline void lv_dirty_set_init(lv_dirty_set *ds) {
    lv_darray_init(&ds->ids, sizeof(int));
}

/**
 * @brief 释放脏集合内部缓冲区并重置
 */
static inline void lv_dirty_set_free(lv_dirty_set *ds) {
    lv_darray_free(&ds->ids);
}

/**
 * @brief 检查元素是否在集合中
 * @return true 已包含，false 未包含
 */
static inline bool lv_dirty_set_contains(const lv_dirty_set *ds, int id) {
    for (int i = 0; i < ds->ids.count; i++) {
        const int *p = (const int *) lv_darray_get(&ds->ids, i);
        if (p && *p == id)
            return true;
    }
    return false;
}

/**
 * @brief 添加元素（已存在则忽略）
 * @return true 添加成功或已存在，false 内存不足
 */
static inline bool lv_dirty_set_add(lv_dirty_set *ds, int id) {
    if (lv_dirty_set_contains(ds, id))
        return true;
    return lv_darray_push(&ds->ids, &id) >= 0;
}

/**
 * @brief 清空集合（count 归零，保留容量）
 */
static inline void lv_dirty_set_clear(lv_dirty_set *ds) {
    lv_darray_clear(&ds->ids);
}

/**
 * @brief 获取集合元素数量
 */
static inline int lv_dirty_set_count(const lv_dirty_set *ds) {
    return ds->ids.count;
}

/**
 * @brief 按插入顺序获取第 index 个元素
 * @return 元素值，越界返回 0
 */
static inline int lv_dirty_set_at(const lv_dirty_set *ds, int index) {
    const int *p = (const int *) lv_darray_get(&ds->ids, index);
    return p ? *p : 0;
}

/* ============================================================
 * INI 文件解析（统一收敛散落各模块的手写行解析样板）
 * ============================================================ */

/**
 * @brief INI 键值对解析回调
 * @param ctx     透传上下文
 * @param section 当前节名（全局节为 NULL；视图仅在本次回调内有效）
 * @param key     键名（已去除首尾空白）
 * @param value   值（'=' 之后原始内容，未去除空白，由调用方按需处理）
 * @return true 继续解析；false 中止解析（lv_ini_parse 提前返回 -1）
 */
typedef bool (*lv_ini_visit_fn)(void *ctx, const char *section, const char *key, const char *value);

/**
 * @brief 解析 INI 格式配置文件（注释/空行/节头/key=value 公共子集）
 *
 * 收敛两处手写 INI 行解析样板（lv_utils_config.c 的 config_load、
 * plugin_system_config.c 的 lv_plugin_config_load），公共处理：
 * - 逐行读取并去除行尾 \r\n
 * - 跳过空行与注释行（'#' 或 '//' 开头）
 * - 解析节头 [section]，后续键值对以 section 参数传给回调
 * - 拆分 key=value，key 去除首尾空白，value 保持原始内容
 * 各调用方的差异逻辑（值修剪、类型推断、数组解析等）留在回调中完成。
 *
 * @param path  文件路径
 * @param visit 回调（不可为 NULL）
 * @param ctx   透传上下文
 * @return 0 成功；-1 参数无效、文件无法打开或回调中止
 */
lv_PUBLIC_API int lv_ini_parse(const char *path, lv_ini_visit_fn visit, void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* lv_lv_UTILS_H */
