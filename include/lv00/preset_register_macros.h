/**
 * @file preset_register_macros.h
 * @brief 预设函数块注册宏 - 消除重复的注册代码模式
 *
 * 提供统一的宏定义，用于简化预设函数块的注册流程。
 * 所有预设模块应使用这些宏进行注册，确保代码风格一致。
 *
 * 使用示例：
 *   LV00_PRESET_REGISTER_BEGIN(my_category, "分类名称")
 *   LV00_PRESET_ENTRY("preset_name", "显示名称", "描述", category)
 *   LV00_PRESET_ENTRY("preset_name2", "显示名称2", "描述2", category)
 *   LV00_PRESET_REGISTER_END()
 */

#ifndef LV00_PRESET_REGISTER_MACROS_H
#define LV00_PRESET_REGISTER_MACROS_H

#include "func_block_registry.h"

#include <stdio.h>

/**
 * @brief 开始预设注册块
 *
 * @param category 预设分类枚举值
 * @param category_name 分类中文名称（用于日志）
 */
#define LV00_PRESET_REGISTER_BEGIN(category, category_name) \
    static int _preset_##category##_register_count = 0; \
    (void)_preset_##category##_register_count; \
    do { \
        const char* _cat_name = (category_name);

/**
 * @brief 注册单个预设条目
 *
 * @param name 预设内部名称
 * @param display_name 显示名称
 * @param desc 预设描述
 * @param cat 预设分类
 */
#define LV00_PRESET_ENTRY(name, display_name, desc, cat) \
        if (lv00_func_block_registry_register_builtin( \
                (name), (display_name), (desc), (cat))) { \
            _preset_##cat##_register_count++; \
        } else { \
            fprintf(stderr, "[PRESET WARN] 预设注册失败: %s (类别: %s)\n", \
                    (name), _cat_name); \
        }

/**
 * @brief 结束预设注册块
 */
#define LV00_PRESET_REGISTER_END() \
    } while(0);

#endif /* LV00_PRESET_REGISTER_MACROS_H */
