/**
 * @file preset_helper_cn.h
 * @brief 预设模块中文辅助系统
 *
 * @details 提供预设模块的中文辅助函数和宏，
 *          用于提升中文用户的交互体验和代码可读性。
 *
 * 【主要功能】
 * - 预设类别中文名称查询
 * - 预设类型中文描述查询
 * - 预设信息格式化输出
 * - 预设搜索辅助函数
 *
 * @author Lv-00 Project
 * @version 3.5.0
 */

#ifndef LV00_PRESET_HELPER_CN_H
#define LV00_PRESET_HELPER_CN_H

#include <stdbool.h>
#include <stddef.h>

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设类别中文名称
 * ============================================================ */

/**
 * @brief 获取预设类别的中文名称
 *
 * @param category 预设类别枚举值
 * @return 中文名称字符串（静态存储，无需释放）
 *
 * 示例:
 * @code
 *   printf("类别: %s\n", preset_category_to_string_cn(PRESET_CATEGORY_CONSTRUCTION));
 *   // 输出: 类别: 几何构造
 * @endcode
 */
const char *preset_category_to_string_cn(int category);

/**
 * @brief 获取预设类别的中文简称
 *
 * @param category 预设类别枚举值
 * @return 中文简称字符串（静态存储，无需释放）
 */
const char *preset_category_to_abbr_cn(int category);

/* ============================================================
 * 预设类型中文名称
 * ============================================================ */

/**
 * @brief 获取预设类型的几何学中文描述
 *
 * @param type 预设类型枚举值
 * @return 中文描述字符串（静态存储，无需释放）
 *
 * 示例:
 * @code
 *   PresetType type = PRESET_TYPE_POINT;
 *   printf("类型: %s\n", preset_type_to_string_cn(type));
 *   // 输出: 类型: 点
 * @endcode
 */
const char *preset_type_to_string_cn(int type);

/**
 * @brief 获取预设类型的完整中文描述（包含所属类别）
 *
 * @param type 预设类型枚举值
 * @return 完整描述字符串（静态存储，无需释放）
 */
const char *preset_type_to_full_string_cn(int type);

/* ============================================================
 * 预设信息格式化
 * ============================================================ */

/**
 * @brief 格式化预设信息为中文描述
 *
 * @param info 预设信息结构体指针
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return 写入的字符数，失败返回-1
 *
 * 示例:
 * @code
 *   char info[256];
 *   PresetInfo info = preset_blocks_get_info("midpoint");
 *   preset_info_format_cn(&info, info, sizeof(info));
 *   printf("%s\n", info);
 *   // 输出类似: [几何构造] 中点构造 - 构造两点之间的中点 M = (A+B)/2
 * @endcode
 */
int preset_info_format_cn(const void *info, char *buf, size_t buf_size);

/**
 * @brief 格式化预设信息为简洁的中文摘要
 *
 * @param name 预设名称
 * @param description 预设描述
 * @param category 预设类别
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return 写入的字符数，失败返回-1
 */
int preset_summary_format_cn(const char *name, const char *description, int category,
                              char *buf, size_t buf_size);

/* ============================================================
 * 预设搜索辅助
 * ============================================================ */

/**
 * @brief 根据中文关键词搜索预设
 *
 * @param keyword 中文关键词
 * @param results 结果缓冲区（预设名称数组）
 * @param max_results 最大结果数
 * @return 实际匹配数量
 *
 * @note results 由调用者分配，大小为 max_results * sizeof(const char*)
 *
 * 示例:
 * @code
 *   const char *results[10];
 *   int count = preset_search_by_keyword_cn("点", results, 10);
 *   for (int i = 0; i < count; i++) {
 *       printf("匹配: %s\n", results[i]);
 *   }
 * @endcode
 */
int preset_search_by_keyword_cn(const char *keyword, const char **results, int max_results);

/**
 * @brief 根据中文类别名称查找类别枚举值
 *
 * @param category_name 中文类别名称
 * @return 对应的类别枚举值，未找到返回-1
 */
int preset_category_from_name_cn(const char *category_name);

/* ============================================================
 * 预设统计信息
 * ============================================================ */

/**
 * @brief 获取指定类别的预设数量
 *
 * @param category 预设类别（-1 表示所有类别）
 * @return 预设数量
 */
int preset_get_count_by_category_cn(int category);

/**
 * @brief 获取预设类别的统计信息（中文）
 *
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return 写入的字符数，失败返回-1
 *
 * 示例输出:
 * @code
 *   预设统计：
 *   - 几何构造: 25 个
 *   - 代数运算: 18 个
 *   - 拓扑结构: 12 个
 *   - 总计: 55 个
 * @endcode
 */
int preset_stats_format_cn(char *buf, size_t buf_size);

/* ============================================================
 * 预设描述模板
 * ============================================================ */

/**
 * @brief 预设描述模板结构（中文）
 */
typedef struct {
    const char *name;           /**< 预设名称 */
    const char *cn_name;        /**< 中文名称 */
    const char *description;    /**< 中文描述 */
    const char *formula;       /**< 数学公式（可选） */
    const char *example;       /**< 使用示例（可选） */
} PresetDescriptionTemplateCN;

/**
 * @brief 获取预设的中文描述模板
 *
 * @param preset_name 预设名称
 * @return 描述模板指针，未找到返回NULL
 */
const PresetDescriptionTemplateCN *preset_get_description_template_cn(const char *preset_name);

/**
 * @brief 获取常用预设的中文帮助信息
 *
 * @param preset_name 预设名称
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return 写入的字符数，失败返回-1
 */
int preset_help_format_cn(const char *preset_name, char *buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_HELPER_CN_H */
