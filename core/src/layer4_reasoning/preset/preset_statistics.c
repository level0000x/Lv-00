/**
 * @file preset_statistics.c
 * @brief 统计学预设函数块 - 实现
 *
 * H1 评估结论（2026-08-07）：注册样板已由 LV_PRESET_REGISTER 宏抽象（每条目 1 行），无需代码生成；运行时注册由 module/presets/statistics.lvz 数据驱动（convert_presets.py 生成）。
 *
 * @details 实现统计学模块的所有预设函数块。
 *          采用统一的注册接口 preset_blocks_register_simple。
 *          共22个预设，涵盖描述统计、假设检验、非参数检验、
 *          回归分析、贝叶斯统计和Bootstrap方法。
 *
 * @module Statistics
 * @category PRESET_CATEGORY_PROBABILITY
 * @version 1.0.0
 */

#include "preset_statistics.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ============================================================
 * 预设数量定义
 * ============================================================ */

/** 统计学模块预设函数块总数：22（与头文件中 STATISTICS_PRESET_COUNT 一致） */

/* ============================================================
 * 模块信息接口
 * ============================================================ */

int preset_statistics_count(void) {
    return STATISTICS_PRESET_COUNT;
}

PresetCategory preset_statistics_category(void) {
    return PRESET_CATEGORY_PROBABILITY;
}

bool preset_statistics_get_names(char ***out_names, int *out_count) {
    static const char *const preset_names[] = {
        /* 描述统计 */
        PRESET_STAT_MEAN,
        PRESET_STAT_MEDIAN,
        PRESET_STAT_MODE,
        PRESET_STAT_QUANTILE,
        /* 分布基础 */
        PRESET_STAT_NORMAL_DIST,
        PRESET_STAT_T_DIST,
        PRESET_STAT_F_DIST,
        /* 假设检验 */
        PRESET_STAT_Z_TEST,
        PRESET_STAT_T_TEST,
        PRESET_STAT_F_TEST,
        PRESET_STAT_ANOVA,
        /* 非参数检验 */
        PRESET_STAT_WILCOXON,
        PRESET_STAT_KRUSKAL_WALLIS,
        /* 回归分析 */
        PRESET_STAT_MULTIPLE_LINEAR_REGRESSION,
        PRESET_STAT_LOGISTIC_REGRESSION,
        PRESET_STAT_R_SQUARED,
        /* 贝叶斯统计 */
        PRESET_STAT_PRIOR_DISTRIBUTION,
        PRESET_STAT_POSTERIOR_DISTRIBUTION,
        PRESET_STAT_BAYES_FACTOR,
        /* Bootstrap */
        PRESET_STAT_BOOTSTRAP,
        /* 其他 */
        PRESET_STAT_VARIANCE,
        PRESET_STAT_CONFIDENCE_INTERVAL,
    };

    return preset_module_get_names(preset_names,
        (int) (sizeof(preset_names) / sizeof(preset_names[0])), out_names, out_count);
}
