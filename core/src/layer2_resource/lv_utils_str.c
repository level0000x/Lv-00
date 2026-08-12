/**
 * @file lv_utils_str.c
 * @brief 字符串工具
 *
 * @details 从 lv_utils.c 拆分的子模块（Lv-00 项目 v3.3.0+）。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv_utils.h"
#include "lv/lv_str_utils.h"

#include "lv/lv_file.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "error_codes.h"
#include "lv.h"
#include "debug.h"
#include "lv_internal.h"

/* ============================================================
 * 字符串处理
 * ============================================================ */

size_t lv_strlcpy(char *dest, const char *src, size_t dest_size) {
    if (!dest || !src || dest_size == 0)
        return 0;

    size_t src_len = strlen(src);
    if (src_len < dest_size) {
        memcpy(dest, src, src_len + 1);
    } else {
        memcpy(dest, src, dest_size - 1);
        dest[dest_size - 1] = '\0';
    }
    return src_len;
}

size_t lv_strlcpy_n(char *dest, size_t dest_size, const char *src, size_t src_len) {
    if (!dest || !src || dest_size == 0)
        return 0;

    size_t copy_len = src_len;
    if (copy_len >= dest_size)
        copy_len = dest_size - 1;
    if (copy_len > 0)
        memcpy(dest, src, copy_len);
    dest[copy_len] = '\0';
    return src_len;
}

size_t lv_strlcat(char *dest, const char *src, size_t dest_size) {
    if (!dest || !src || dest_size == 0)
        return 0;

    size_t dest_len = strlen(dest);
    if (dest_len >= dest_size)
        return dest_len + strlen(src);

    size_t remaining = dest_size - dest_len - 1;
    size_t src_len = strlen(src);

    if (src_len < remaining) {
        memcpy(dest + dest_len, src, src_len + 1);
    } else {
        memcpy(dest + dest_len, src, remaining);
        dest[dest_size - 1] = '\0';
    }
    return dest_len + src_len;
}

char *lv_strdup_safe(const char *str) {
    if (!str)
        return NULL;
    size_t len = strlen(str);
    char *copy = lv_malloc(len + 1);
    if (copy) {
        memcpy(copy, str, len + 1);
    }
    return copy;
}

char *lv_asprintf(const char *fmt, ...) {
    if (!fmt)
        return NULL;

    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    if (len < 0)
        lv_RETURN_ERROR_NULL(lv_ERROR_INTERNAL, "asprintf vsnprintf 返回负值");

    char *buf = lv_malloc((size_t) len + 1);
    if (!buf)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "asprintf malloc 失败");

    va_start(args, fmt);
    vsnprintf(buf, (size_t) len + 1, fmt, args);
    va_end(args);

    return buf;
}

/**
 * @brief 判断字符串是否为空白或空
 *
 * 检查给定字符串是否为 NULL、空字符串或仅包含空白字符（空格、制表符、
 * 换行符等）。
 *
 * @param str 待检查的字符串指针，允许为 NULL。
 * @return true  字符串为 NULL、空字符串或全部由空白字符组成；
 *         false 字符串包含至少一个非空白字符。
 */
bool lv_str_is_blank(const char *str) {
    if (!str)
        return true;
    while (*str) {
        if (!isspace((unsigned char) *str))
            return false;
        str++;
    }
    return true;
}

/**
 * @brief 安全字符串连接 —— 保证 \0 终止并全面检查参数有效性
 *
 * 查找 dest 中现有字符串的末尾，然后追加 src。
 * 若 dest 已经完全填满（无 \0 终止符），则仅保证 dest[dest_size-1] = '\0'。
 *
 * @param dest 目标缓冲区（必须已包含一个有效的 \0 终止字符串）
 * @param src  源字符串（可为 NULL）
 * @param dest_size 目标缓冲区总大小（字节）
 * @return 成功时返回 dest，失败时返回 NULL
 */
char *lv_strncat(char *dest, const char *src, size_t dest_size) {
    if (!dest || !src || dest_size == 0)
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "strncat 参数无效");

    /* 查找 dest 当前字符串的末尾 */
    size_t dest_len = 0;
    while (dest_len < dest_size && dest[dest_len] != '\0') {
        dest_len++;
    }

    /* 若 dest 已满（没有 \0），则保证末尾为 \0 */
    if (dest_len >= dest_size) {
        dest[dest_size - 1] = '\0';
        return dest;
    }

    /* 追加 src */
    size_t remaining = dest_size - dest_len - 1; /* -1 保留 \0 空间 */
    size_t i;
    for (i = 0; i < remaining && src[i] != '\0'; i++) {
        dest[dest_len + i] = src[i];
    }
    dest[dest_len + i] = '\0';
    return dest;
}

/**
 * @brief 安全格式化输出到定长缓冲区
 *
 * 包装 vsnprintf，添加参数有效性检查并确保 \0 终止。
 *
 * @param buf  输出缓冲区
 * @param size 缓冲区大小
 * @param fmt  格式字符串
 * @param ...  可变参数
 * @return 成功时返回写入的字符数（不含 \0），失败返回 -1
 */
int lv_snprintf(char *buf, size_t size, const char *fmt, ...) {
    if (!buf || size == 0 || !fmt)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "snprintf 参数无效");

    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(buf, size, fmt, args);
    va_end(args);

    /* 确保 \0 终止（防御 vsnprintf 的某些非标准实现） */
    if (written < 0) {
        buf[0] = '\0';
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "snprintf vsnprintf 返回负值");
    }
    if ((size_t) written >= size) {
        buf[size - 1] = '\0';
    }

    return written;
}

