/**
 * @file preset_register_helper.h
 * @brief 预设模块统一注册辅助宏
 * @details 为所有预设模块提供统一的注册模式，确保错误处理和日志记录一致。
 * @module preset
 * @category internal
 * @version 3.4.0
 */

#ifndef LV00_PRESET_REGISTER_HELPER_H
#define LV00_PRESET_REGISTER_HELPER_H

#include "preset_blocks.h"
#include "error_codes.h"
#include "debug.h"

/**
 * @brief 统一的预设注册辅助宏
 * @details 封装注册调用、成功计数和错误日志，确保所有预设模块使用一致的注册模式。
 *
 * @param success_count  成功计数变量名（int 类型）
 * @param total_count    总数计数变量名（int 类型）
 * @param registry       函数块注册表指针
 * @param name           预设名称字符串
 * @param type           PresetType 枚举值
 * @param input_types    输入类型数组
 * @param input_count    输入类型数量
 * @param output_type    输出类型
 * @param desc           中文描述字符串
 */
#define LV00_PRESET_REGISTER(success_count, total_count, registry, name, type, \
                              input_types, input_count, output_type, desc)        \
    do {                                                                         \
        (total_count)++;                                                         \
        if (preset_blocks_register_simple((registry), (name), (type),            \
                                          (input_types), (input_count),          \
                                          (output_type), (desc))) {             \
            (success_count)++;                                                   \
        } else {                                                                 \
            LV00_LOG_ERROR("预设注册失败: %s (%s)", (name), (desc));             \
        }                                                                        \
    } while (0)

/**
 * @brief 带类别的预设注册辅助宏
 * @details 支持扩展类别系统的注册宏，调用 preset_blocks_register_simple 接口。
 *
 * @param success_count  成功计数变量名（int 类型）
 * @param total_count    总数计数变量名（int 类型）
 * @param registry       函数块注册表指针（保留参数，当前未使用）
 * @param name           预设名称字符串
 * @param output_type    输出类型（PresetType 枚举值）
 * @param input_types    输入类型数组
 * @param input_count    输入类型数量
 * @param desc           中文描述字符串
 * @param category       预设类别（PresetCategory 枚举值）
 * @param complexity     时间复杂度描述（如 "O(1)", "O(n)"）
 * @param reversible     是否可逆（bool）
 */
#define LV00_PRESET_REGISTER_EX(success_count, total_count, registry, name, output_type, \
                                 input_types, input_count, desc, category, complexity, \
                                 reversible)                                       \
    do {                                                                             \
        (total_count)++;                                                             \
        if (preset_blocks_register_simple((name), (desc), (category),                \
                                          (input_types), (input_count),              \
                                          (output_type), NULL, (complexity),         \
                                          false, (reversible))) {                    \
            (success_count)++;                                                       \
        } else {                                                                     \
            LV00_LOG_ERROR("预设注册失败: %s (%s)", (name), (desc));                 \
        }                                                                            \
    } while (0)

#endif /* LV00_PRESET_REGISTER_HELPER_H */
