/**
 * @file math_input.c
 * @brief 数学输入处理 —— 解析 LaTeX 风格数学表达式并规范化
 *
 * @details 支持基本数学表达式的格式检测和规范化：
 *          - LaTeX 数学模式: $...$ 或 $$...$$
 *          - 纯文本代数表达式: x^2 + y^2 = r^2
 *          - GCLC 几何构造语句: point A 0 0
 *
 * @version 1.1.0
 */

#include "math_input.h"
#include "lv_internal.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int lv_math_input_parse(const char *input, char *normalized, size_t buf_size) {
    if (!input || !normalized || buf_size == 0) return -1;

    /* 检测输入格式 */
    int format = lv_math_input_detect_format(input);

    if (format < 0) {
        /* 未知格式，直接复制 */
        size_t len = strlen(input);
        if (len >= buf_size) len = buf_size - 1;
        memcpy(normalized, input, len);
        normalized[len] = '\0';
        return 0;
    }

    /* LaTeX 格式：去除 $...$ 包裹 */
    if (format == 1) {
        const char *start = input;
        size_t len = strlen(input);

        /* 跳过开头的 $ */
        while (*start == '$') start++;
        /* 跳过末尾的 $ */
        const char *end = input + len;
        while (end > start && *(end - 1) == '$') end--;

        size_t out_len = (size_t)(end - start);
        if (out_len >= buf_size) out_len = buf_size - 1;
        memcpy(normalized, start, out_len);
        normalized[out_len] = '\0';
        return (int)out_len;
    }

    /* 纯文本表达式：去除首尾空白 */
    const char *p = input;
    while (isspace((unsigned char)*p)) p++;
    size_t len = strlen(p);
    while (len > 0 && isspace((unsigned char)p[len - 1])) len--;

    if (len >= buf_size) len = buf_size - 1;
    memcpy(normalized, p, len);
    normalized[len] = '\0';
    return (int)len;
}

int lv_math_input_detect_format(const char *input) {
    if (!input) return -1;

    /* 跳过前导空白 */
    while (isspace((unsigned char)*input)) input++;

    if (*input == '\0') return -1;

    /* LaTeX 数学模式 */
    if (input[0] == '$') return 1;

    /* GCLC 风格几何构造 */
    if (strncmp(input, "point", 5) == 0 || strncmp(input, "line", 4) == 0
        || strncmp(input, "circle", 6) == 0) {
        return 2;
    }

    /* 纯文本表达式 */
    return 0;
}
