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
    /* 委托给统一错误码系统的规范错误表；未收录的状态码保持原有回退文本。
     *
     * 说明：lv_error_string 目前不提供"是否未收录码"的显式查询接口（error_codes.h
     * 仅有 lv_error_string/lv_error_name/lv_error_category/lv_error_code_from_string
     * 等），只能通过返回的占位符文本区分，故此处保留占位符字符串比较，并与
     * error_codes.c 中 lv_error_string 的 "未知错误码" 字面量耦合；若后续
     * error_codes.h 增加显式 is_unknown 查询，应替换为显式判断。 */
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
 * 区间定义与类别名称来自 error_codes.h 的 LV_ERROR_CATEGORY_RANGES_X
 * （粗粒度类别区间的单一事实来源），此处仅作宏展开，不再手写区间。
 * 【两侧必须同步】该宏的类别名称与 LV_ERROR_CODES_X 第 5 字段（每码类别
 * 短名）的对应关系见 error_codes.h 中的映射注释，修改任一侧必须同步另一侧。
 * "警告" 区间（负码 INT_MIN..-1）是状态码模块特有语义，不属于错误码体系，
 * 单独保留于此。
 */
#define LV_X_STATUS_RANGE_ITEM(min, max, category) {min, max, category},
static const struct {
    int min;
    int max;
    const char *category;
} kStatusCategoryRanges[] = {
    LV_ERROR_CATEGORY_RANGES_X(LV_X_STATUS_RANGE_ITEM)
    {INT_MIN, -1, "警告"},
};
#undef LV_X_STATUS_RANGE_ITEM

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
    /* 委托给统一错误码系统的规范错误表；未收录的状态码保持原有回退文本。
     *
     * 说明：同 lv_status_message —— lv_error_name 暂无"是否未收录码"的显式查询
     * 接口，仅能通过占位符文本 "UNKNOWN_ERROR" 区分，此处保留字符串比较并与
     * error_codes.c 字面量耦合；若后续提供显式查询，应替换为显式判断。 */
    const char *name = lv_error_name((lvErrorCode) code);
    return strcmp(name, "UNKNOWN_ERROR") == 0 ? "lv_UNKNOWN" : name;
}
