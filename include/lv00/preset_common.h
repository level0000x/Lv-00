/**
 * @file preset_common.h
 * @brief 预设函数块系统 - 公共定义和工具
 *
 * @details 提供预设函数块系统的公共类型定义、常量、宏和工具函数。
 *          所有预设模块共享此头文件。
 *
 * @version 5.0.0
 * @author Lv-00 Project
 */

#ifndef PRESET_COMMON_H
#define PRESET_COMMON_H

#include "func_block_preset.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 版本信息
 * ============================================================ */

/** 预设系统主版本号 */
#define PRESET_SYSTEM_VERSION_MAJOR 5
/** 预设系统次版本号 */
#define PRESET_SYSTEM_VERSION_MINOR 0
/** 预设系统修订版本号 */
#define PRESET_SYSTEM_VERSION_PATCH 0

/* ============================================================
 * 容量限制（可配置）
 * ============================================================ */

/** 最大预设数量 */
#ifndef PRESET_MAX_COUNT
#define PRESET_MAX_COUNT 512
#endif

/** 最大参数数量 */
#ifndef PRESET_MAX_PARAMS
#define PRESET_MAX_PARAMS 32
#endif

/** 最大输入数量 */
#ifndef PRESET_MAX_INPUTS
#define PRESET_MAX_INPUTS 16
#endif

/** 最大输出数量 */
#ifndef PRESET_MAX_OUTPUTS
#define PRESET_MAX_OUTPUTS 8
#endif

/** 字符串缓冲区大小 */
#ifndef PRESET_BUFFER_SIZE
#define PRESET_BUFFER_SIZE 8192
#endif

/** 名称最大长度 */
#ifndef PRESET_MAX_NAME_LENGTH
#define PRESET_MAX_NAME_LENGTH 256
#endif

/** 描述最大长度 */
#ifndef PRESET_MAX_DESC_LENGTH
#define PRESET_MAX_DESC_LENGTH 1024
#endif

/** 预设ID起始偏移 */
#ifndef PRESET_ID_OFFSET
#define PRESET_ID_OFFSET 60000
#endif

/* ============================================================
 * 线程安全计数器
 * ============================================================ */

#ifdef _WIN32
#include <windows.h>
/** 线程安全的计数器类型 */
typedef volatile LONG PresetAtomicCounter;
/** 原子递增 */
#define PRESET_ATOMIC_INC(counter) InterlockedIncrement(&(counter))
/** 原子递减 */
#define PRESET_ATOMIC_DEC(counter) InterlockedDecrement(&(counter))
/** 原子读取 */
#define PRESET_ATOMIC_READ(counter) (counter)
#else
#include <stdatomic.h>
/** 线程安全的计数器类型 */
typedef _Atomic int PresetAtomicCounter;
/** 原子递增 */
#define PRESET_ATOMIC_INC(counter) atomic_fetch_add(&(counter), 1)
/** 原子递减 */
#define PRESET_ATOMIC_DEC(counter) atomic_fetch_sub(&(counter), 1)
/** 原子读取 */
#define PRESET_ATOMIC_READ(counter) atomic_load(&(counter))
#endif

/* ============================================================
 * 安全字符串操作宏
 * ============================================================ */

/**
 * @brief 安全字符串复制
 * @param dest 目标缓冲区
 * @param src 源字符串
 * @param dest_size 目标缓冲区大小
 * @return 实际复制的字节数
 */
#define PRESET_SAFE_STRCPY(dest, src, dest_size) \
    lv00_safe_strncpy((dest), (src), (dest_size))

/**
 * @brief 安全字符串拼接
 * @param dest 目标缓冲区
 * @param src 源字符串
 * @param dest_size 目标缓冲区大小
 * @return 拼接后的字符串长度
 */
#define PRESET_SAFE_STRCAT(dest, src, dest_size) \
    lv00_safe_strncat((dest), (src), (dest_size))

/**
 * @brief 安全格式化输出
 * @param dest 目标缓冲区
 * @param dest_size 目标缓冲区大小
 * @param fmt 格式字符串
 * @param ... 可变参数
 * @return 实际写入的字符数（不含终止符）
 */
#define PRESET_SAFE_SNPRINTF(dest, dest_size, fmt, ...) \
    lv00_safe_snprintf((dest), (dest_size), (fmt), ##__VA_ARGS__)

/* ============================================================
 * 内存管理宏
 * ============================================================ */

/**
 * @brief 安全内存分配
 * @param ptr 指针变量
 * @param size 分配大小
 * @param label 错误处理标签
 */
#define PRESET_SAFE_MALLOC(ptr, size, label) \
    do { \
        (ptr) = lv00_malloc(size); \
        if ((ptr) == NULL) { \
            LV00_ERROR_SET(LV00_ERROR_MEMORY_ALLOCATION_FAILED, \
                          "内存分配失败: %s", #ptr); \
            goto label; \
        } \
    } while (0)

/**
 * @brief 安全内存分配并清零
 * @param ptr 指针变量
 * @param count 元素数量
 * @param size 元素大小
 * @param label 错误处理标签
 */
#define PRESET_SAFE_CALLOC(ptr, count, size, label) \
    do { \
        (ptr) = lv00_calloc(count, size); \
        if ((ptr) == NULL) { \
            LV00_ERROR_SET(LV00_ERROR_MEMORY_ALLOCATION_FAILED, \
                          "内存分配失败: %s", #ptr); \
            goto label; \
        } \
    } while (0)

/**
 * @brief 安全重新分配内存
 * @param ptr 指针变量
 * @param size 新大小
 * @param label 错误处理标签
 */
#define PRESET_SAFE_REALLOC(ptr, size, label) \
    do { \
        void *_tmp = lv00_realloc((ptr), (size)); \
        if (_tmp == NULL && (size) > 0) { \
            LV00_ERROR_SET(LV00_ERROR_MEMORY_ALLOCATION_FAILED, \
                          "内存重新分配失败: %s", #ptr); \
            goto label; \
        } \
        (ptr) = _tmp; \
    } while (0)

/**
 * @brief 安全释放内存
 * @param ptr 指针变量
 */
#define PRESET_SAFE_FREE(ptr) \
    do { \
        if ((ptr) != NULL) { \
            lv00_free(ptr); \
            (ptr) = NULL; \
        } \
    } while (0)

/* ============================================================
 * 错误处理宏
 * ============================================================ */

/**
 * @brief 检查条件，失败则跳转到错误处理
 * @param condition 条件表达式
 * @param error_code 错误码
 * @param fmt 错误消息格式
 * @param label 跳转标签
 */
#define PRESET_CHECK(condition, error_code, fmt, label, ...) \
    do { \
        if (!(condition)) { \
            LV00_ERROR_SET((error_code), (fmt), ##__VA_ARGS__); \
            goto label; \
        } \
    } while (0)

/**
 * @brief 检查指针非空
 * @param ptr 指针
 * @param label 跳转标签
 */
#define PRESET_CHECK_NULL(ptr, label) \
    PRESET_CHECK((ptr) != NULL, LV00_ERROR_NULL_POINTER, \
                "空指针: %s", label, #ptr)

/**
 * @brief 检查字符串非空且非空字符串
 * @param str 字符串
 * @param label 跳转标签
 */
#define PRESET_CHECK_STRING(str, label) \
    PRESET_CHECK((str) != NULL && (str)[0] != '\0', \
                LV00_ERROR_INVALID_ARGUMENT, \
                "无效字符串: %s", label, #str)

/**
 * @brief 检查索引范围
 * @param index 索引值
 * @param max 最大值（不包含）
 * @param label 跳转标签
 */
#define PRESET_CHECK_INDEX(index, max, label) \
    PRESET_CHECK((index) >= 0 && (index) < (max), \
                LV00_ERROR_INDEX_OUT_OF_RANGE, \
                "索引越界: %d (范围: 0-%d)", label, \
                (int)(index), (int)(max) - 1)

/**
 * @brief 检查数值范围
 * @param value 数值
 * @param min 最小值
 * @param max 最大值
 * @param label 跳转标签
 */
#define PRESET_CHECK_RANGE(value, min, max, label) \
    PRESET_CHECK((value) >= (min) && (value) <= (max), \
                LV00_ERROR_VALUE_OUT_OF_RANGE, \
                "数值越界: %g (范围: %g-%g)", label, \
                (double)(value), (double)(min), (double)(max))

/* ============================================================
 * 预设注册辅助宏
 * ============================================================ */

/**
 * @brief 定义预设输入类型数组
 * @param ... 类型列表
 */
#define PRESET_INPUTS(...) \
    (PresetType[]){__VA_ARGS__}

/**
 * @brief 定义预设输入数量
 * @param ... 类型列表
 */
#define PRESET_INPUT_COUNT(...) \
    (sizeof((PresetType[]){__VA_ARGS__}) / sizeof(PresetType))

/**
 * @brief 简化预设注册调用
 * @param name 预设名称
 * @param desc 描述
 * @param inputs 输入类型数组
 * @param input_count 输入数量
 * @param output 输出类型
 * @param math_def 数学定义
 * @param complexity 复杂度
 * @param constructive 是否构造性
 * @param reversible 是否可逆
 */
#define PRESET_REGISTER(name, desc, inputs, input_count, output, \
                       math_def, complexity, constructive, reversible) \
    do { \
        if (preset_blocks_register_simple( \
                (name), (desc), PRESET_CATEGORY_CONSTRUCTION, \
                (inputs), (input_count), (output), \
                (math_def), (complexity), (constructive), (reversible))) { \
            success_count++; \
        } else { \
            LV00_ERROR_SET(LV00_ERROR_PRESET_REGISTRATION_FAILED, \
                          "预设注册失败: %s", (name)); \
        } \
    } while (0)

/**
 * @brief 批量注册预设的辅助宏
 * @param category 预设类别
 */
#define PRESET_REGISTER_BEGIN(category) \
    int success_count = 0; \
    const PresetCategory _current_category = (category); \
    (void)_current_category; /* 避免未使用警告 */

/**
 * @brief 结束批量注册并返回结果
 * @param expected_count 预期注册数量
 */
#define PRESET_REGISTER_END(expected_count) \
    return success_count == (expected_count)

/* ============================================================
 * 预设元数据定义辅助宏
 * ============================================================ */

/**
 * @brief 定义预设元数据
 * @param _name 名称
 * @param _desc 描述
 * @param _math_def 数学定义
 * @param _category 类别
 * @param _props 属性
 * @param _complexity 复杂度
 * @param _in_count 输入数量
 * @param _out_count 输出数量
 * @param _pre_count 前置条件数量
 * @param _post_count 后置条件数量
 * @param _rel_count 相关预设数量
 */
#define PRESET_METADATA_DEFINE( \
    _name, _desc, _math_def, _category, _props, _complexity, \
    _in_count, _out_count, _pre_count, _post_count, _rel_count) \
    { \
        .name = (_name), \
        .description = (_desc), \
        .mathematical_def = (_math_def), \
        .category = (_category), \
        .properties = (_props), \
        .complexity = (_complexity), \
        .input_count = (_in_count), \
        .output_count = (_out_count), \
        .precondition_count = (_pre_count), \
        .postcondition_count = (_post_count), \
        .related_count = (_rel_count), \
        .version_major = PRESET_SYSTEM_VERSION_MAJOR, \
        .version_minor = PRESET_SYSTEM_VERSION_MINOR, \
        .version_patch = PRESET_SYSTEM_VERSION_PATCH \
    }

/* ============================================================
 * 工具函数声明
 * ============================================================ */

/**
 * @brief 安全字符串复制（带长度限制）
 * @param dest 目标缓冲区
 * @param src 源字符串
 * @param dest_size 目标缓冲区大小
 * @return 实际复制的字节数
 */
size_t lv00_safe_strncpy(char *dest, const char *src, size_t dest_size);

/**
 * @brief 安全字符串拼接（带长度限制）
 * @param dest 目标缓冲区
 * @param src 源字符串
 * @param dest_size 目标缓冲区大小
 * @return 拼接后的字符串长度
 */
size_t lv00_safe_strncat(char *dest, const char *src, size_t dest_size);

/**
 * @brief 安全格式化输出（带长度限制）
 * @param dest 目标缓冲区
 * @param dest_size 目标缓冲区大小
 * @param fmt 格式字符串
 * @param ... 可变参数
 * @return 实际写入的字符数
 */
int lv00_safe_snprintf(char *dest, size_t dest_size, const char *fmt, ...);

/**
 * @brief 计算整型数组的哈希值
 * @param arr 数组
 * @param count 元素数量
 * @return 哈希值
 */
uint32_t lv00_hash_int_array(const int *arr, int count);

/**
 * @brief 比较两个整型数组
 * @param a 数组A
 * @param count_a A的元素数量
 * @param b 数组B
 * @param count_b B的元素数量
 * @return true 相等
 */
bool lv00_int_arrays_equal(const int *a, int count_a,
                           const int *b, int count_b);

/**
 * @brief 复制整型数组
 * @param src 源数组
 * @param count 元素数量
 * @return 新分配的数组（需调用者释放）
 */
int* lv00_dup_int_array(const int *src, int count);

/**
 * @brief 验证预设名称格式
 * @param name 名称
 * @return true 格式有效
 */
bool preset_validate_name(const char *name);

/**
 * @brief 验证预设描述格式
 * @param description 描述
 * @return true 格式有效
 */
bool preset_validate_description(const char *description);

/**
 * @brief 验证预设类型组合的有效性
 * @param input_types 输入类型数组
 * @param input_count 输入数量
 * @param output_type 输出类型
 * @return true 有效
 */
bool preset_validate_type_combination(const PresetType *input_types,
                                      int input_count,
                                      PresetType output_type);

/**
 * @brief 获取预设类别的显示名称
 * @param category 类别
 * @return 显示名称（静态字符串）
 */
const char* preset_category_to_string(PresetCategory category);

/**
 * @brief 从字符串解析预设类别
 * @param str 字符串
 * @param category 输出类别
 * @return true 解析成功
 */
bool preset_category_from_string(const char *str, PresetCategory *category);

/**
 * @brief 获取预设类型的显示名称
 * @param type 类型
 * @return 显示名称（静态字符串）
 */
const char* preset_type_to_string(PresetType type);

/**
 * @brief 从字符串解析预设类型
 * @param str 字符串
 * @param type 输出类型
 * @return true 解析成功
 */
bool preset_type_from_string(const char *str, PresetType *type);

/**
 * @brief 获取复杂度级别的显示名称
 * @param complexity 复杂度
 * @return 显示名称（静态字符串）
 */
const char* preset_complexity_to_string(const char *complexity);

/**
 * @brief 获取属性标志的字符串表示
 * @param properties 属性标志
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 实际写入的字符数
 */
int preset_properties_to_string(PresetProperty properties,
                                char *buffer, size_t buffer_size);

/**
 * @brief 解析属性标志字符串
 * @param str 字符串
 * @param properties 输出属性
 * @return true 解析成功
 */
bool preset_properties_from_string(const char *str,
                                   PresetProperty *properties);

/* ============================================================
 * 调试和日志宏
 * ============================================================ */

#ifdef PRESET_DEBUG
/**
 * @brief 调试日志
 */
#define PRESET_DEBUG_LOG(fmt, ...) \
    lv00_log_debug("[PRESET] " fmt, ##__VA_ARGS__)

/**
 * @brief 跟踪日志
 */
#define PRESET_TRACE_LOG(fmt, ...) \
    lv00_log_trace("[PRESET] " fmt, ##__VA_ARGS__)
#else
#define PRESET_DEBUG_LOG(fmt, ...) ((void)0)
#define PRESET_TRACE_LOG(fmt, ...) ((void)0)
#endif

/**
 * @brief 错误日志
 */
#define PRESET_ERROR_LOG(fmt, ...) \
    lv00_log_error("[PRESET] " fmt, ##__VA_ARGS__)

/**
 * @brief 警告日志
 */
#define PRESET_WARN_LOG(fmt, ...) \
    lv00_log_warn("[PRESET] " fmt, ##__VA_ARGS__)

/**
 * @brief 信息日志
 */
#define PRESET_INFO_LOG(fmt, ...) \
    lv00_log_info("[PRESET] " fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* PRESET_COMMON_H */
