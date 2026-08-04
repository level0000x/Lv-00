/**
 * @file status_codes.c
 * @brief Lv-00 状态码系统实现
 *
 * 实现 status_codes.h 中声明的状态码查询接口。
 * 状态码 → 文本/名称映射统一由 error_codes.c 的规范错误表提供
 * （lv_error_string / lv_error_name），本文件不再维护重复的映射表。
 *
 * @author Lv-00 Project
 * @version 3.0.0
 */

#include "lv/status_codes.h"

#include "lv/error_codes.h"
#include "lv/lv.h"

#include <limits.h>
#include <string.h>

/* ==================== 状态码判断函数 ==================== */

int lv_status_is_success(int code) {
    return code == 0 ? 1 : 0;
}

int lv_status_is_error(int code) {
    return code != 0 ? 1 : 0;
}

/* ==================== 状态码描述映射 ==================== */

const char *lv_status_message(int code) {
    /* 委托给统一错误码系统的规范错误表；未收录的状态码保持原有回退文本 */
    const char *msg = lv_error_string((lvErrorCode) code);
    return strcmp(msg, "未知错误码") == 0 ? "未知状态码" : msg;
}

/* ==================== 辅助函数 ==================== */

/**
 * @brief 判断状态码是否为警告
 *
 * 警告状态码保留为 -1 到 -99 范围。
 *
 * @param code 状态码
 * @return 是警告返回 1，否则返回 0
 */
int lv_status_is_warning(int code) {
    return (code < 0 && code >= -99) ? 1 : 0;
}

/**
 * @brief 状态码类别区间映射表
 *
 * 每项定义 [min, max] 闭区间对应的类别名称，线性扫描匹配，
 * 替代手写硬编码区间 if 链。区间划分与顺序与原实现完全一致。
 */
static const struct {
    int min;
    int max;
    const char *category;
} kStatusCategoryRanges[] = {
    {0, 0, "成功"},
    {1, 99, "通用系统错误"},
    {100, 129, "内存与资源错误"},
    {130, 139, "解析器安全错误"},
    {200, 299, "约束图错误"},
    {300, 399, "符号坐标错误"},
    {400, 499, "求解器错误"},
    {500, 599, "重写引擎错误"},
    {600, 699, "合一检查错误"},
    {700, 749, "函数块错误"},
    {750, 799, "预设系统错误"},
    {800, 899, "类型系统错误"},
    {900, 999, "证明系统错误"},
    {INT_MIN, -1, "警告"},
};

/**
 * @brief 获取状态码所属类别名称
 *
 * @param code 状态码
 * @return 类别名称字符串（中文，静态存储，无需释放）
 */
const char *lv_status_category(int code) {
    for (size_t i = 0; i < lv_ARRAY_SIZE(kStatusCategoryRanges); i++) {
        if (code >= kStatusCategoryRanges[i].min && code <= kStatusCategoryRanges[i].max)
            return kStatusCategoryRanges[i].category;
    }
    return "未分类";
}

/**
 * @brief 获取状态码的简短名称
 *
 * @param code 状态码
 * @return 状态码名称字符串（如 "lv_OK"，静态存储，无需释放）
 */
const char *lv_status_name(int code) {
    /* 委托给统一错误码系统的规范错误表；未收录的状态码保持原有回退文本 */
    const char *name = lv_error_name((lvErrorCode) code);
    return strcmp(name, "UNKNOWN_ERROR") == 0 ? "lv_UNKNOWN" : name;
}
