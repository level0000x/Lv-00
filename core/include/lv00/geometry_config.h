/**
 * @file geometry_config.h
 * @brief 几何计算可配置容差参数
 *
 * 提供几何比较（共线、垂直、平行等）中使用的 epsilon 容差配置。
 * 支持运行时动态调整，线程安全。
 *
 * @version 1.0.0
 */

#ifndef LV00_GEOMETRY_CONFIG_H
#define LV00_GEOMETRY_CONFIG_H

#include <stddef.h>

/* LV00_PUBLIC_API 由 lv00.h 定义（DLL 导出/导入）；
 * 当几何子模块头文件在 lv00.h 之前被间接包含时，提供空回退。 */
#ifndef LV00_PUBLIC_API
#define LV00_PUBLIC_API
#endif

/**
 * @brief 几何计算容差配置结构体
 */
typedef struct {
    double collinear_epsilon;      /**< 共线判定容差，默认: 1e-9 */
    double perpendicular_epsilon;  /**< 垂直判定容差，默认: 1e-9 */
    double parallel_epsilon;       /**< 平行判定容差，默认: 1e-9 */
    double distance_epsilon;       /**< 距离比较容差，默认: 1e-9 */
    double angle_epsilon;          /**< 角度比较容差（弧度），默认: 1e-6 */
    double singular_threshold;     /**< 矩阵奇异性判定阈值，默认: 1e-12 */
} Lv00GeometryConfig;

/**
 * @brief 获取默认几何容差配置
 *
 * @return 指向静态默认配置的常量指针（无需释放）
 */
const Lv00GeometryConfig *lv00_geometry_default_config(void);

/**
 * @brief 设置自定义几何容差配置（线程安全）
 *
 * @param[in] config 指向自定义配置的指针，内容将被拷贝；NULL 时恢复默认值
 */
void lv00_geometry_set_config(const Lv00GeometryConfig *config);

/**
 * @brief 获取当前几何容差配置（线程安全）
 *
 * @return 指向当前活跃配置的常量指针（无需释放）
 */
const Lv00GeometryConfig *lv00_geometry_get_config(void);

#endif /* LV00_GEOMETRY_CONFIG_H */
