#ifndef lv_PRESET_CATEGORY_H
#define lv_PRESET_CATEGORY_H

/**
 * @file preset_category.h
 * @brief 预设函数块类别（PresetCategory / PresetExtendedCategory）映射单一事实来源
 *
 * 散落各文件的类别中文名/英文 key 表（func_block_preset_query.c /
 * func_block_registry.c / preset_blocks.c）统一由本头文件的条目宏生成，
 * 禁止在其他文件重复定义同枚举的任何名称表。
 *
 * 中文名以查询侧（func_block_preset_query.c，UI 显示源）为准，
 * 即「数学分析 / 数论 / 群论 / 环论 / 域论 / 拓扑学」这一组。
 */

#include "lv/func_block_registry.h" /* PresetCategory 枚举 */
#include "lv/preset_blocks.h"        /* PresetExtendedCategory 枚举 */

/**
 * @brief PresetCategory 全字段条目宏（单一事实来源）
 *
 * 每行携带 3 列：ENUM（枚举值）、EN_KEY（英文 key，用于序列化/反序列化）、
 * ZH_NAME（中文名，UI 显示）。
 *
 * 用法：
 *   static const lvStrToEnumEntry kMap[] = { LV_PRESET_CATEGORY_ENTRY(LV_PRESET_ROW_ZH) };
 *   static const lvStrToEnumEntry kMap[] = { LV_PRESET_CATEGORY_ENTRY(LV_PRESET_ROW_EN) };
 */
#define LV_PRESET_CATEGORY_ENTRY(x) \
    x(PRESET_CATEGORY_CONSTRUCTION, "construction", "几何构造") \
    x(PRESET_CATEGORY_MEASUREMENT, "measurement", "度量计算") \
    x(PRESET_CATEGORY_TRANSFORMATION, "transformation", "几何变换") \
    x(PRESET_CATEGORY_ALGEBRAIC, "algebraic", "代数运算") \
    x(PRESET_CATEGORY_LOGIC, "logic", "逻辑推导") \
    x(PRESET_CATEGORY_ANALYSIS, "analysis", "数学分析") \
    x(PRESET_CATEGORY_NUMBER_THEORY, "number_theory", "数论") \
    x(PRESET_CATEGORY_GROUP_THEORY, "group_theory", "群论") \
    x(PRESET_CATEGORY_RING_THEORY, "ring_theory", "环论") \
    x(PRESET_CATEGORY_FIELD_THEORY, "field_theory", "域论") \
    x(PRESET_CATEGORY_TOPOLOGY, "topology", "拓扑学") \
    x(PRESET_CATEGORY_LINEAR_ALGEBRA, "linear_algebra", "线性代数") \
    x(PRESET_CATEGORY_COMBINATORICS, "combinatorics", "组合数学") \
    x(PRESET_CATEGORY_COMPLEX_ANALYSIS, "complex_analysis", "复分析") \
    x(PRESET_CATEGORY_PROBABILITY, "probability", "概率统计") \
    x(PRESET_CATEGORY_GEOMETRY, "geometry", "几何学") \
    x(PRESET_CATEGORY_ALGEBRA, "algebra", "代数学") \
    x(PRESET_CATEGORY_CATEGORY_THEORY, "category_theory", "范畴论") \
    x(PRESET_CATEGORY_SET_THEORY, "set_theory", "集合论") \
    x(PRESET_CATEGORY_CUSTOM, "custom", "自定义") \
    x(PRESET_CATEGORY_GRAPH_THEORY, "graph_theory", "图论") \
    x(PRESET_CATEGORY_DIFFERENTIAL_GEOMETRY, "differential_geometry", "微分几何") \
    x(PRESET_CATEGORY_NUMERICAL, "numerical", "数值分析") \
    x(PRESET_CATEGORY_OPTIMIZATION, "optimization", "优化理论") \
    x(PRESET_CATEGORY_MATH_LOGIC, "math_logic", "数理逻辑")

/**
 * @brief PresetExtendedCategory 条目宏（单一事实来源，中文名与既有表逐项一致）
 *
 * 每行携带 2 列：ENUM（PresetExtendedCategory 枚举值）、ZH_NAME（中文名）。
 */
#define LV_PRESET_EXTENDED_CATEGORY_ENTRY(x) \
    x(PRESET_EXT_BASIC_CONSTRUCTION, "基础几何构造") \
    x(PRESET_EXT_ADVANCED_CONSTRUCTION, "高级几何构造") \
    x(PRESET_EXT_POLYGON, "多边形") \
    x(PRESET_EXT_CIRCLE, "圆相关") \
    x(PRESET_EXT_TRANSFORMATION_BASIC, "基本变换") \
    x(PRESET_EXT_TRANSFORMATION_ADVANCED, "高级变换") \
    x(PRESET_EXT_MEASUREMENT, "度量计算") \
    x(PRESET_EXT_TRIGONOMETRY, "三角函数") \
    x(PRESET_EXT_COORDINATE, "坐标运算") \
    x(PRESET_EXT_ALGEBRA_BASIC, "基础代数") \
    x(PRESET_EXT_ALGEBRA_ADVANCED, "高级代数") \
    x(PRESET_EXT_LINEAR_ALGEBRA, "线性代数") \
    x(PRESET_EXT_POLYNOMIAL, "多项式") \
    x(PRESET_EXT_LOGIC_PROPOSITIONAL, "命题逻辑") \
    x(PRESET_EXT_LOGIC_PREDICATE, "谓词逻辑") \
    x(PRESET_EXT_PROOF_TACTICS, "证明策略") \
    x(PRESET_EXT_ANALYSIS_LIMIT, "极限") \
    x(PRESET_EXT_ANALYSIS_DIFFERENTIAL, "微分") \
    x(PRESET_EXT_ANALYSIS_INTEGRAL, "积分") \
    x(PRESET_EXT_TOPOLOGY, "拓扑") \
    x(PRESET_EXT_DIFFERENTIAL_GEOMETRY, "微分几何") \
    x(PRESET_EXT_NUMBER_THEORY, "数论") \
    x(PRESET_EXT_GROUP_THEORY, "群论") \
    x(PRESET_EXT_ANALYSIS, "分析学") \
    x(PRESET_EXT_COMBINATORICS, "组合数学") \
    x(PRESET_EXT_GRAPH_THEORY, "图论") \
    x(PRESET_EXT_NUMERICAL_ANALYSIS, "数值分析") \
    x(PRESET_EXT_OPTIMIZATION_THEORY, "优化理论") \
    x(PRESET_EXT_MATH_LOGIC, "数理逻辑")

#endif /* lv_PRESET_CATEGORY_H */
