/**
 * @file adaptive_threshold.c
 * @brief 自适应阈值管理 - 动态调整阈值以适应不同场景
 *
 * @details 实现自适应阈值的查询、设置与状态管理。
 *          使用模块级静态变量存储阈值状态。
 */
#include "lv00/adaptive_threshold.h"
#include "lv00/lv00_utils.h"

/* 默认阈值 */
#define LV00_DEFAULT_THRESHOLD 0.5

/* 模块级状态 */
static double s_threshold = LV00_DEFAULT_THRESHOLD;
static int s_is_adaptive = 0;

/**
 * @brief 检查阈值是否为自适应模式
 * @return 1 表示自适应，0 表示固定阈值
 */
int lv00_threshold_is_adaptive(void) {
    return s_is_adaptive;
}

/**
 * @brief 设置自适应阈值
 * @param value 阈值（0.0 ~ 1.0），同时启用自适应模式
 *
 * @details 设置后自动将模式切换为自适应。
 *          阈值会被限制在 [0.0, 1.0] 范围内。
 */
void lv00_set_adaptive_threshold(double value) {
    /* 限制范围 */
    if (value < 0.0) value = 0.0;
    if (value > 1.0) value = 1.0;
    s_threshold = value;
    s_is_adaptive = 1;
}

/**
 * @brief 获取当前自适应阈值
 * @return 当前阈值（未设置则返回默认值 0.5）
 */
double lv00_get_adaptive_threshold(void) {
    return s_threshold;
}
