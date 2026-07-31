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
 * @brief 获取状态码所属类别名称
 *
 * @param code 状态码
 * @return 类别名称字符串（中文，静态存储，无需释放）
 */
const char *lv_status_category(int code) {
    if (code == 0)
        return "成功";
    if (code >= 1 && code < 100)
        return "通用系统错误";
    if (code >= 100 && code < 130)
        return "内存与资源错误";
    if (code >= 130 && code < 140)
        return "解析器安全错误";
    if (code >= 200 && code < 300)
        return "约束图错误";
    if (code >= 300 && code < 400)
        return "符号坐标错误";
    if (code >= 400 && code < 500)
        return "求解器错误";
    if (code >= 500 && code < 600)
        return "重写引擎错误";
    if (code >= 600 && code < 700)
        return "合一检查错误";
    if (code >= 700 && code < 750)
        return "函数块错误";
    if (code >= 750 && code < 800)
        return "预设系统错误";
    if (code >= 800 && code < 900)
        return "类型系统错误";
    if (code >= 900 && code < 1000)
        return "证明系统错误";
    if (code < 0)
        return "警告";
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