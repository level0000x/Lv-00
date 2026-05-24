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

#ifndef LV00_PRESET_COMMON_H
#define LV00_PRESET_COMMON_H

#include <stdio.h>

#include "func_block_preset.h"
/*
 * [P1 修复] preset_common.h 引用了 lv00_internal.h（内部头文件）。
 * 此依赖关系存在是因为 preset_common.h 中的宏（如 LV00_ERROR_SET、
 * LV00_ERROR_ALLOCATION_FAILED 等）和内存管理函数（lv00_malloc 等）
 * 定义在 lv00_internal.h / lv00_utils.h 中。
 * 注意：lv00_internal.h 本意是 src/ 目录的内部桥接头文件，
 * 但当前被公共头文件 preset_common.h 引用。
 * 未来应考虑将必要的错误码和内存管理声明提取到独立的公共头文件中，
 * 以消除公共头文件对内部头文件的依赖。
 */
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

/** 最大预设数量（与 lv00_internal.h 中 LV00_PRESET_MAX_COUNT 保持一致） */
#ifndef PRESET_MAX_COUNT
#define PRESET_MAX_COUNT 1024
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
#define PRESET_SAFE_STRCPY(dest, src, dest_size) lv00_safe_strncpy((dest), (src), (dest_size))

/**
 * @brief 安全字符串拼接
 * @param dest 目标缓冲区
 * @param src 源字符串
 * @param dest_size 目标缓冲区大小
 * @return 拼接后的字符串长度
 */
#define PRESET_SAFE_STRCAT(dest, src, dest_size) lv00_safe_strncat((dest), (src), (dest_size))

/**
 * @brief 安全格式化输出
 * @param dest 目标缓冲区
 * @param dest_size 目标缓冲区大小
 * @param fmt 格式字符串
 * @param ... 可变参数
 * @return 实际写入的字符数（不含终止符）
 */
#define PRESET_SAFE_SNPRINTF(dest, dest_size, fmt, ...) lv00_safe_snprintf((dest), (dest_size), (fmt), ##__VA_ARGS__)

/* ============================================================
 * 内存管理宏
 * ============================================================ */

/**
 * @brief 安全内存分配
 * @param ptr 指针变量
 * @param size 分配大小
 * @param label 错误处理标签
 */
#define PRESET_SAFE_MALLOC(ptr, size, label)                                        \
    do {                                                                            \
        (ptr) = lv00_malloc(size);                                                  \
        if ((ptr) == NULL) {                                                        \
            LV00_ERROR_SET(LV00_ERROR_ALLOCATION_FAILED, "内存分配失败: %s", #ptr); \
            goto label;                                                             \
        }                                                                           \
    } while (0)

/**
 * @brief 安全内存分配并清零
 * @param ptr 指针变量
 * @param count 元素数量
 * @param size 元素大小
 * @param label 错误处理标签
 */
#define PRESET_SAFE_CALLOC(ptr, count, size, label)                                 \
    do {                                                                            \
        (ptr) = lv00_calloc(count, size);                                           \
        if ((ptr) == NULL) {                                                        \
            LV00_ERROR_SET(LV00_ERROR_ALLOCATION_FAILED, "内存分配失败: %s", #ptr); \
            goto label;                                                             \
        }                                                                           \
    } while (0)

/**
 * @brief 安全重新分配内存
 * @param ptr 指针变量
 * @param size 新大小
 * @param label 错误处理标签
 */
#define PRESET_SAFE_REALLOC(ptr, size, label)                                           \
    do {                                                                                \
        void *_tmp = lv00_realloc((ptr), (size));                                       \
        if (_tmp == NULL && (size) > 0) {                                               \
            LV00_ERROR_SET(LV00_ERROR_ALLOCATION_FAILED, "内存重新分配失败: %s", #ptr); \
            goto label;                                                                 \
        }                                                                               \
        (ptr) = _tmp;                                                                   \
    } while (0)

/**
 * @brief 安全释放内存
 * @param ptr 指针变量的地址（与 lv00_free 调用约定一致）
 *
 * 注意：lv00_free 签名为 void lv00_free(void **ptr)，因此必须传入指针的地址。
 * 此宏封装了 NULL 检查和正确的调用约定，防止 use-after-free。
 */
#define PRESET_SAFE_FREE(ptr)            \
    do {                                 \
        if ((ptr) != NULL) {             \
            lv00_free((void **) &(ptr)); \
        }                                \
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
#define PRESET_CHECK(condition, error_code, fmt, label, ...)    \
    do {                                                        \
        if (!(condition)) {                                     \
            LV00_ERROR_SET((error_code), (fmt), ##__VA_ARGS__); \
            goto label;                                         \
        }                                                       \
    } while (0)

/**
 * @brief 检查指针非空
 * @param ptr 指针
 * @param label 跳转标签
 */
#define PRESET_CHECK_NULL(ptr, label) PRESET_CHECK((ptr) != NULL, LV00_ERROR_NULL_POINTER, "空指针: %s", label, #ptr)

/**
 * @brief 检查字符串非空且非空字符串
 * @param str 字符串
 * @param label 跳转标签
 */
#define PRESET_CHECK_STRING(str, label) \
    PRESET_CHECK((str) != NULL && (str)[0] != '\0', LV00_ERROR_INVALID_ARGUMENT, "无效字符串: %s", label, #str)

/**
 * @brief 检查索引范围
 * @param index 索引值
 * @param max 最大值（不包含）
 * @param label 跳转标签
 */
#define PRESET_CHECK_INDEX(index, max, label)                                                                        \
    PRESET_CHECK((index) >= 0 && (index) < (max), LV00_ERROR_INDEX_OUT_OF_RANGE, "索引越界: %d (范围: 0-%d)", label, \
                 (int) (index), (int) (max) - 1)

/**
 * @brief 检查数值范围
 * @param value 数值
 * @param min 最小值
 * @param max 最大值
 * @param label 跳转标签
 */
#define PRESET_CHECK_RANGE(value, min, max, label)                                                                  \
    PRESET_CHECK((value) >= (min) && (value) <= (max), LV00_ERROR_VALUE_OUT_OF_RANGE, "数值越界: %g (范围: %g-%g)", \
                 label, (double) (value), (double) (min), (double) (max))

/* ============================================================
 * 预设注册辅助宏
 * ============================================================ */

/**
 * @brief 定义预设输入类型数组
 * @param ... 类型列表
 */
#define PRESET_INPUTS(...) \
    (PresetType[]) {       \
        __VA_ARGS__        \
    }

/**
 * @brief 定义预设输入数量
 * @param ... 类型列表
 */
#define PRESET_INPUT_COUNT(...) (sizeof((PresetType[]) {__VA_ARGS__}) / sizeof(PresetType))

/**
 * @brief 简化预设注册调用（带类别参数）
 *
 * @param name 预设名称
 * @param desc 描述
 * @param cat  预设类别（PresetCategory 枚举值）
 * @param inputs 输入类型数组
 * @param input_count 输入数量
 * @param output 输出类型
 * @param math_def 数学定义
 * @param complexity 复杂度
 * @param constructive 是否构造性
 * @param reversible 是否可逆
 */
#define PRESET_REGISTER_EX(name, desc, cat, inputs, input_count, output, math_def, complexity, constructive,    \
                           reversible)                                                                          \
    do {                                                                                                        \
        if (preset_blocks_register_simple((name), (desc), (cat), (inputs), (input_count), (output), (math_def), \
                                          (complexity), (constructive), (reversible))) {                        \
            success_count++;                                                                                    \
        } else {                                                                                                \
            LV00_ERROR_SET(LV00_ERROR_PRESET_REGISTRATION_FAILED, "预设注册失败: %s", (name));                  \
        }                                                                                                       \
    } while (0)

/**
 * @brief 通用预设函数块注册宏（无 success_count 依赖）
 *
 * 该宏消除了各 preset 模块中重复的 register_xxx_preset 静态函数定义。
 * 所有模块共享相同的注册逻辑，仅类别参数不同。
 * 可直接替代各模块中的静态包装函数，如 register_calculus_preset、
 * register_basic_math_preset 等。
 *
 * 与 PRESET_REGISTER_EX 的区别：
 * - 不依赖外部 success_count 变量，可在任意上下文中使用
 * - 注册失败时记录警告但继续执行，不中断注册流程
 * - 可作为 if 条件使用，返回 bool 值
 *
 * @param name 预设名称
 * @param desc 描述
 * @param cat  预设类别（PresetCategory 枚举值）
 * @param in_types 输入类型数组
 * @param in_cnt 输入数量
 * @param out_type 输出类型
 * @param math_def 数学定义（LaTeX）
 * @param complexity 复杂度描述
 * @param constructive 是否构造性
 * @param reversible 是否可逆
 *
 * @return bool 注册是否成功（可在 if 条件中使用）
 *
 * 用法示例（替代 register_calculus_preset 静态函数）：
 * @code
 *   // 旧方式：
 *   //   static bool register_calculus_preset(...) {
 *   //       return preset_blocks_register_simple(..., PRESET_CATEGORY_ANALYSIS, ...);
 *   //   }
 *   //   if (register_calculus_preset(...)) { success_count++; }
 *
 *   // 新方式：
 *   //   if (PRESET_REGISTER_CAT(name, desc, PRESET_CATEGORY_ANALYSIS,
 *   //       inputs, 2, output, math, "O(n)", true, false)) {
 *   //       success_count++;
 *   //   }
 * @endcode
 */
#define PRESET_REGISTER_CAT(name, desc, cat, in_types, in_cnt, out_type, \
                             math_def, complexity, constructive, reversible) \
    preset_blocks_register_simple((name), (desc), (cat), \
        (in_types), (in_cnt), (out_type), (math_def), \
        (complexity), (constructive), (reversible))

/**
 * @brief 通用预设注册宏（带自动成功计数）
 *
 * 在 PRESET_REGISTER_CAT 基础上自动递增 success_count。
 * 适用于模块的 xxx_register() 函数内部，与 PRESET_REGISTER_BEGIN/END 配合使用。
 *
 * @param name 预设名称
 * @param desc 描述
 * @param cat  预设类别（PresetCategory 枚举值）
 * @param in_types 输入类型数组
 * @param in_cnt 输入数量
 * @param out_type 输出类型
 * @param math_def 数学定义（LaTeX）
 * @param complexity 复杂度描述
 * @param constructive 是否构造性
 * @param reversible 是否可逆
 */
#define PRESET_REGISTER_CAT_COUNTED(name, desc, cat, in_types, in_cnt, out_type, \
                                     math_def, complexity, constructive, reversible) \
    do { \
        if (PRESET_REGISTER_CAT(name, desc, cat, in_types, in_cnt, out_type, \
                                math_def, complexity, constructive, reversible)) { \
            success_count++; \
        } else { \
            LV00_ERROR_SET(LV00_ERROR_PRESET_REGISTRATION_FAILED, \
                           "预设注册失败: %s", (name)); \
        } \
    } while (0)

/**
 * @brief 简化预设注册调用（默认类别为 CONSTRUCTION，向后兼容）
 *
 * @param name 预设名称
 * @param desc 描述
 * @param inputs 输入类型数组
 * @param input_count 输入数量
 * @param output 输出类型
 * @param math_def 数学定义
 * @param complexity 复杂度
 * @param constructive 是否构造性
 * @param reversible 是否可逆
 *
 * @note 推荐使用 PRESET_REGISTER_EX 以指定正确的类别。
 *       此宏保留仅为向后兼容，默认使用 PRESET_CATEGORY_CONSTRUCTION。
 */
#define PRESET_REGISTER(name, desc, inputs, input_count, output, math_def, complexity, constructive, reversible)    \
    PRESET_REGISTER_EX(name, desc, PRESET_CATEGORY_CONSTRUCTION, inputs, input_count, output, math_def, complexity, \
                       constructive, reversible)

/**
 * @brief 批量注册预设的辅助宏
 * @param category 预设类别
 */
#define PRESET_REGISTER_BEGIN(category)                  \
    int success_count = 0;                               \
    const PresetCategory _current_category = (category); \
    (void) _current_category; /* 避免未使用警告 */

/**
 * @brief 结束批量注册并返回结果
 * @param expected_count 预期注册数量
 */
#define PRESET_REGISTER_END(expected_count) return success_count == (expected_count)

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
#define PRESET_METADATA_DEFINE(_name, _desc, _math_def, _category, _props, _complexity, _in_count, _out_count, \
                               _pre_count, _post_count, _rel_count)                                            \
    {.name = (_name),                                                                                          \
     .description = (_desc),                                                                                   \
     .mathematical_def = (_math_def),                                                                          \
     .category = (_category),                                                                                  \
     .properties = (_props),                                                                                   \
     .complexity = (_complexity),                                                                              \
     .input_count = (_in_count),                                                                               \
     .output_count = (_out_count),                                                                             \
     .precondition_count = (_pre_count),                                                                       \
     .postcondition_count = (_post_count),                                                                     \
     .related_count = (_rel_count),                                                                            \
     .version_major = PRESET_SYSTEM_VERSION_MAJOR,                                                             \
     .version_minor = PRESET_SYSTEM_VERSION_MINOR,                                                             \
     .version_patch = PRESET_SYSTEM_VERSION_PATCH}

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
bool lv00_int_arrays_equal(const int *a, int count_a, const int *b, int count_b);

/**
 * @brief 复制整型数组
 * @param src 源数组
 * @param count 元素数量
 * @return 新分配的数组（需调用者释放）
 */
int *lv00_dup_int_array(const int *src, int count);

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
bool preset_validate_type_combination(const PresetType *input_types, int input_count, PresetType output_type);

/**
 * @brief 获取预设类别的显示名称
 * @param category 类别
 * @return 显示名称（静态字符串）
 */
const char *preset_category_to_string(PresetCategory category);

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
const char *preset_type_to_string(PresetType type);

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
const char *preset_complexity_to_string(const char *complexity);

/**
 * @brief 获取属性标志的字符串表示
 * @param properties 属性标志
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 实际写入的字符数
 */
int preset_properties_to_string(PresetProperty properties, char *buffer, size_t buffer_size);

/**
 * @brief 解析属性标志字符串
 * @param str 字符串
 * @param properties 输出属性
 * @return true 解析成功
 */
bool preset_properties_from_string(const char *str, PresetProperty *properties);

/* ============================================================
 * 类型验证与边界检查函数（v12.0 新增）
 * ============================================================ */

/**
 * @brief 验证预设输入参数数量是否在有效范围内
 *
 * @param input_count 输入参数数量
 * @return true 数量有效（0 到 PRESET_MAX_INPUTS）
 * @return false 数量无效
 */
bool preset_validate_input_count(int input_count);

/**
 * @brief 验证预设输出参数数量是否在有效范围内
 *
 * @param output_count 输出参数数量
 * @return true 数量有效（0 到 PRESET_MAX_OUTPUTS）
 * @return false 数量无效
 */
bool preset_validate_output_count(int output_count);

/**
 * @brief 验证输入类型数组的有效性
 *
 * 检查每个类型是否在有效范围内，并验证类型组合的合理性。
 *
 * @param input_types 输入类型数组
 * @param input_count 输入数量
 * @return true 类型数组有效
 * @return false 类型数组无效
 */
bool preset_validate_input_types(const PresetType *input_types, int input_count);

/**
 * @brief 验证输出类型的有效性
 *
 * @param output_type 输出类型
 * @return true 类型有效
 * @return false 类型无效
 */
bool preset_validate_output_type(PresetType output_type);

/**
 * @brief 检查类型是否为基本类型（点、线、圆、标量等）
 *
 * @param type 预设类型
 * @return true 是基本类型
 * @return false 不是基本类型
 */
bool preset_type_is_basic(PresetType type);

/**
 * @brief 检查类型是否为代数结构类型（群、环、域等）
 *
 * @param type 预设类型
 * @return true 是代数结构类型
 * @return false 不是代数结构类型
 */
bool preset_type_is_algebraic(PresetType type);

/**
 * @brief 检查类型是否为分析类型（函数、极限、导数等）
 *
 * @param type 预设类型
 * @return true 是分析类型
 * @return false 不是分析类型
 */
bool preset_type_is_analytic(PresetType type);

/**
 * @brief 检查类型是否为拓扑类型（拓扑空间、流形等）
 *
 * @param type 预设类型
 * @return true 是拓扑类型
 * @return false 不是拓扑类型
 */
bool preset_type_is_topological(PresetType type);

/**
 * @brief 获取类型的类别归属
 *
 * 返回类型所属的主要数学领域类别。
 *
 * @param type 预设类型
 * @return 类别字符串（"几何"、"代数"、"分析"、"拓扑"、"逻辑"、"通用"）
 */
const char *preset_type_get_domain(PresetType type);

/**
 * @brief 检查两个类型是否兼容
 *
 * 用于验证预设函数块的输入输出类型是否可以连接。
 *
 * @param source_type 源类型（输出类型）
 * @param target_type 目标类型（输入类型）
 * @return true 类型兼容
 * @return false 类型不兼容
 */
bool preset_types_compatible(PresetType source_type, PresetType target_type);

/**
 * @brief 计算预设的签名哈希值
 *
 * 用于快速比较预设的类型签名是否相同。
 *
 * @param input_types 输入类型数组
 * @param input_count 输入数量
 * @param output_type 输出类型
 * @return 签名哈希值
 */
uint32_t preset_compute_signature_hash(const PresetType *input_types, int input_count, PresetType output_type);

/* ============================================================
 * 预设元数据验证函数（v12.0 新增）
 * ============================================================ */

/**
 * @brief 验证预设元数据的完整性
 *
 * 检查所有必需字段是否已正确设置。
 *
 * @param metadata 预设元数据
 * @param out_error_msg 输出错误信息（可为NULL）
 * @param error_msg_size 错误信息缓冲区大小
 * @return true 元数据有效
 * @return false 元数据无效
 */
bool preset_validate_metadata(const PresetMetadata *metadata, char *out_error_msg, size_t error_msg_size);

/**
 * @brief 验证数学定义格式
 *
 * 检查 LaTeX 格式的数学定义是否语法正确。
 *
 * @param math_def 数学定义字符串
 * @return true 格式有效
 * @return false 格式无效
 */
bool preset_validate_math_definition(const char *math_def);

/**
 * @brief 验证复杂度描述格式
 *
 * @param complexity 复杂度描述
 * @return true 格式有效
 * @return false 格式无效
 */
bool preset_validate_complexity(const char *complexity);

/* ============================================================
 * 调试和日志宏
 * ============================================================ */

#ifdef PRESET_DEBUG
/**
 * @brief 调试日志
 */
#define PRESET_DEBUG_LOG(fmt, ...) lv00_log_debug("[PRESET] " fmt, ##__VA_ARGS__)

/**
 * @brief 跟踪日志
 */
#define PRESET_TRACE_LOG(fmt, ...) lv00_log_trace("[PRESET] " fmt, ##__VA_ARGS__)
#else
#define PRESET_DEBUG_LOG(fmt, ...) ((void) 0)
#define PRESET_TRACE_LOG(fmt, ...) ((void) 0)
#endif

/**
 * @brief 错误日志
 */
#define PRESET_ERROR_LOG(fmt, ...) lv00_log_error("[PRESET ERROR] " fmt, ##__VA_ARGS__)

/**
 * @brief 警告日志
 */
#define PRESET_WARN_LOG(fmt, ...) lv00_log_warn("[PRESET WARN] " fmt, ##__VA_ARGS__)

/**
 * @brief 信息日志
 */
#define PRESET_INFO_LOG(fmt, ...) lv00_log_info("[PRESET] " fmt, ##__VA_ARGS__)

/* ============================================================
 * 预设模块公共模板宏
 * ============================================================ */

/**
 * @brief 定义预设模块的注册包装函数
 * @param module_name 模块名称（用于函数命名）
 * @param category 预设类别枚举值
 *
 * 此宏生成一个静态的 register_##module_name##_preset 函数，
 * 消除各预设模块中重复的注册包装代码。
 */
#define LV00_DEFINE_PRESET_REGISTER_WRAPPER(module_name, category)              \
    static bool register_##module_name##_preset(                               \
        const char *name, const char *brief, const char *description,           \
        int input_count, int output_count,                                     \
        PresetComplexity complexity,                                           \
        const char *mathematical_definition,                                   \
        const char *input_types, const char *output_types,                     \
        const char *dependencies, const char *domain_tags,                     \
        const int *internal_nodes, int internal_count,                         \
        const int *input_ports, int input_port_count,                          \
        const int *output_ports, int output_port_count)                        \
    {                                                                          \
        return preset_blocks_register_simple(                                  \
            name, brief, description,                                          \
            input_count, output_count,                                         \
            category, complexity,                                              \
            mathematical_definition,                                           \
            input_types, output_types,                                         \
            dependencies, domain_tags,                                         \
            internal_nodes, internal_count,                                    \
            input_ports, input_port_count,                                     \
            output_ports, output_port_count);                                  \
    }

/**
 * @brief 定义预设模块的标准接口函数
 * @param module_name 模块名称
 * @param category 预设类别枚举值
 * @param preset_count 预设数量常量
 * @param names_array 静态名称数组
 *
 * 此宏生成 register、count、category、get_names 四个标准函数，
 * 完整定义一个预设模块的外部接口。
 */
#define LV00_DEFINE_PRESET_MODULE(module_name, category, preset_count, names_array) \
                                                                              \
    bool preset_##module_name##_register(void) {                               \
        return true; /* 实际注册在 init 函数中完成 */                          \
    }                                                                          \
                                                                              \
    int preset_##module_name##_count(void) {                                   \
        return preset_count;                                                   \
    }                                                                          \
                                                                              \
    PresetCategory preset_##module_name##_category(void) {                     \
        return category;                                                       \
    }                                                                          \
                                                                              \
    bool preset_##module_name##_get_names(char ***out_names,                   \
                                          int *out_count) {                    \
        return preset_module_get_names(names_array, preset_count,              \
                                        out_names, out_count);                 \
    }

/**
 * @brief 通用预设名称列表获取函数
 */
bool preset_module_get_names(const char *const *names, int count,
                             char ***out_names, int *out_count);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_COMMON_H */
