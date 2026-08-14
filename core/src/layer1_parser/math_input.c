/**
 * @file math_input.c
 * @brief 数学输入处理 —— 解析 LaTeX 风格数学表达式并规范化
 *
 * @details 支持基本数学表达式的格式检测和规范化：
 *          - LaTeX 数学模式: $...$ 或 $$...$$
 *          - 纯文本代数表达式: x^2 + y^2 = r^2
 *          - GCLC 几何构造语句: point A 0 0
 *
 * @author Lv-00 Project
 * @version 1.1.0
 */

#include "lv/math_input.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_str_utils.h"

/**
 * @brief 解析并规范化数学输入表达式
 *
 * 检测输入格式（LaTeX / GCLC / 纯文本），提取净表达式内容并写入
 * normalized 缓冲区。LaTeX 格式去除 $ 包裹符号，纯文本去除首尾空白。
 *
 * @param input      原始输入字符串
 * @param normalized 输出缓冲区，存放规范化后的表达式
 * @param buf_size   输出缓冲区大小
 * @return 规范化后表达式长度（不含 null 终止符）；失败返回 -1
 */
int lv_math_input_parse(const char *input, char *normalized, size_t buf_size) {
    if (!input || !normalized || buf_size == 0)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "invalid input params");

    /* 检测输入格式 */
    int format = lv_math_input_detect_format(input);

    if (format < 0) {
        /* 未知格式，直接复制 */
        lv_strlcpy(normalized, input, buf_size);
        return 0;
    }

    /* LaTeX 格式：去除 $...$ 包裹 */
    if (format == 1) {
        const char *start = input;
        size_t len = strlen(input);

        /* 跳过开头的 $ */
        while (*start == '$')
            start++;
        /* 跳过末尾的 $ */
        const char *end = input + len;
        while (end > start && *(end - 1) == '$')
            end--;

        size_t out_len = (size_t) (end - start);
        if (out_len >= buf_size)
            out_len = buf_size - 1;
        lv_strlcpy_n(normalized, buf_size, start, out_len);
        return (int) out_len;
    }

    /* 纯文本表达式：去除首尾空白（左端收敛到 lv_str_ltrim，右端与 lv_str_rtrim 空白定义一致） */
    const char *p = lv_str_ltrim((char *) input);
    size_t len = strlen(p);
    while (len > 0 && (unsigned char) p[len - 1] <= ' ')
        len--;

    if (len >= buf_size)
        len = buf_size - 1;
    lv_strlcpy_n(normalized, buf_size, p, len);
    return (int) len;
}

/**
 * @brief 检测数学输入字符串的格式类型
 *
 * 根据输入内容判断其格式：LaTeX 数学模式（$ 开头）、GCLC 几何构造
 * （point/line/circle 关键字）或纯文本表达式。
 *
 * @param input 输入字符串
 * @return 格式类型：1=LaTeX, 2=GCLC, 0=纯文本；输入无效返回 -1
 */
int lv_math_input_detect_format(const char *input) {
    if (!input)
        return -1;

    /* 跳过前导空白 */
    input = lv_str_skip_ws(input);

    if (*input == '\0')
        return -1;

    /* LaTeX 数学模式 */
    if (input[0] == '$')
        return 1;

    /* GCLC 风格几何构造 */
    if (lv_str_startswith(input, "point") || lv_str_startswith(input, "line") || lv_str_startswith(input, "circle")) {
        return 2;
    }

    /* 纯文本表达式 */
    return 0;
}
