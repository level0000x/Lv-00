/**
 * @file preset_helper_cn.c
 * @brief 预设辅助函数（中文版）
 *
 * 提供中文预设几何构造的便捷辅助函数。
 * 包括常用几何图形的快速创建、中文命名的参数验证和格式化输出。
 *
 * @version 1.0.0
 */

#include <stdio.h>
#include <string.h>

#include "lv/lv.h"
#include "lv/lv_internal.h"
#include "lv/lv_str_utils.h"

/* ========================================================================
 * 预设几何名称表
 * ======================================================================== */

/** 预设名称条目 */
typedef struct {
    int preset_id;       /**< 预设ID */
    const char *name_cn; /**< 中文名称 */
    const char *name_en; /**< 英文名称 */
    const char *desc_cn; /**< 中文描述 */
} PresetNameEntry;

static const PresetNameEntry g_preset_names[] = {
    {0, "点", "point", "零维几何对象，表示空间中的位置"},
    {1, "线段", "line_segment", "一维有限几何对象，由两个端点定义"},
    {2, "直线", "line", "一维无限延伸的几何对象"},
    {3, "圆", "circle", "到定点距离相等的点的集合"},
    {4, "三角形", "triangle", "三条线段围成的封闭区域"},
    {5, "矩形", "rectangle", "四角均为直角的四边形"},
    {6, "正方形", "square", "四边等长的矩形"},
    {7, "平行四边形", "parallelogram", "对边平行且等长的四边形"},
    {8, "梯形", "trapezoid", "仅一组对边平行的四边形"},
    {9, "菱形", "rhombus", "四边等长的平行四边形"},
    {10, "圆弧", "arc", "圆上两点间的曲线段"},
    {11, "椭圆", "ellipse", "到两焦点距离之和相等的点的集合"},
    {12, "多边形", "polygon", "多条线段围成的封闭区域"},
};

#define PRESET_NAME_COUNT (sizeof(g_preset_names) / sizeof(g_preset_names[0]))

/* ========================================================================
 * 公共 API
 * ======================================================================== */

/**
 * @brief 获取预设的中文名称
 * @param preset_id 预设ID
 * @return 中文名称字符串，未找到时返回 "未知预设"
 */
const char *lv_preset_get_name_cn(int preset_id) {
    /* preset_id 连续 0..N-1，表项即数组下标，直接索引替代线性查找 */
    if (preset_id >= 0 && preset_id < (int) PRESET_NAME_COUNT)
        return g_preset_names[preset_id].name_cn;
    return "未知预设";
}

/**
 * @brief 获取预设的中文描述
 * @param preset_id 预设ID
 * @return 中文描述字符串，未找到时返回 "无描述"
 */
const char *lv_preset_get_desc_cn(int preset_id) {
    if (preset_id >= 0 && preset_id < (int) PRESET_NAME_COUNT)
        return g_preset_names[preset_id].desc_cn;
    return "无描述";
}

/**
 * @brief 根据中文名称查找预设ID
 * @param name_cn 中文名称
 * @return 预设ID，未找到返回 -1
 */
int lv_preset_find_by_name_cn(const char *name_cn) {
    if (name_cn == NULL)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "name_cn is NULL");

    for (size_t i = 0; i < PRESET_NAME_COUNT; i++) {
        if (lv_str_eq(g_preset_names[i].name_cn, name_cn)) {
            return g_preset_names[i].preset_id;
        }
    }
    return -1;
}

/**
 * @brief 格式化预设信息为中文字符串
 *
 * 将指定预设的信息格式化为可读的中文字符串。
 *
 * @param preset_id 预设ID
 * @param buf       输出缓冲区
 * @param buf_size  缓冲区大小
 * @return 写入的字符数（不含终止符），失败返回 -1
 */
int lv_preset_format_cn(int preset_id, char *buf, size_t buf_size) {
    if (buf == NULL || buf_size == 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "buf is NULL or buf_size is 0");

    const char *name = lv_preset_get_name_cn(preset_id);
    const char *desc = lv_preset_get_desc_cn(preset_id);

    return snprintf(buf, buf_size, "[%d] %s - %s", preset_id, name, desc);
}

/**
 * @brief 获取预设名称表的条目数量
 * @return 条目数量
 */
int lv_preset_name_count(void) {
    return (int) PRESET_NAME_COUNT;
}
