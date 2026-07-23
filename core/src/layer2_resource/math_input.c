/**
 * @file math_input.c
 * @brief 数学输入处理模块 —— Layer2 资源管理层
 *
 * 提供数学表达式的输入解析与格式标准化功能。
 * 支持检测输入格式（LaTeX、ASCII 数学、自然语言），
 * 并将各种格式统一规范化为内部标准表示。
 *
 * @version 1.0.0
 */

#include "lv/math_input.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* ================================================================
 *  格式类型常量
 * ================================================================ */

#define MATH_FORMAT_UNKNOWN 0 /**< 未知格式 */
#define MATH_FORMAT_LATEX 1   /**< LaTeX 格式 */
#define MATH_FORMAT_ASCII 2   /**< ASCII 数学格式 */
#define MATH_FORMAT_NATURAL 3 /**< 自然语言描述 */
#define MATH_FORMAT_JSON 4    /**< JSON 格式 */

/* ================================================================
 *  内部辅助函数
 * ================================================================ */

/**
 * @brief 跳过空白字符
 * @param p 当前解析位置
 * @return 跳过空白后的新位置
 */
static const char *skip_whitespace(const char *p) {
    if (!p)
        return NULL;
    while (*p && isspace((unsigned char) *p)) {
        p++;
    }
    return p;
}

/**
 * @brief 检测字符串是否包含 LaTeX 特征标记
 *
 * 识别 \frac、\sqrt、\int、\sum、\alpha 等命令，
 * 以及 $...$ 或 $$...$$ 数学环境标记。
 *
 * @param input 输入字符串
 * @return 包含 LaTeX 特征返回 1，否则返回 0
 */
static int has_latex_features(const char *input) {
    const char *p;

    if (!input)
        return 0;

    /* 检测反斜杠命令（\frac, \sqrt, \int 等） */
    p = input;
    while (*p) {
        if (*p == '\\' && isalpha((unsigned char) *(p + 1))) {
            return 1;
        }
        /* 检测数学环境定界符 */
        if (*p == '$') {
            return 1;
        }
        p++;
    }

    return 0;
}

/**
 * @brief 检测字符串是否包含 JSON 结构特征
 *
 * @param input 输入字符串
 * @return 包含 JSON 特征返回 1，否则返回 0
 */
static int has_json_features(const char *input) {
    const char *p;

    if (!input)
        return 0;

    p = skip_whitespace(input);
    return (*p == '{' || *p == '[') ? 1 : 0;
}

/**
 * @brief 检测字符串是否为 ASCII 数学表达式
 *
 * 识别典型的 ASCII 数学符号：+、-、*、/、^、=、(、)
 * 以及数字和字母标识符的组合。
 *
 * @param input 输入字符串
 * @return 是 ASCII 数学表达式返回 1，否则返回 0
 */
static int has_ascii_math_features(const char *input) {
    int has_operator = 0;
    int has_operand = 0;
    const char *p;

    if (!input)
        return 0;

    p = input;
    while (*p) {
        char c = *p;

        /* 运算符检测 */
        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '^' || c == '=' || c == '<' || c == '>') {
            has_operator = 1;
        }

        /* 操作数检测（数字或字母标识符） */
        if (isdigit((unsigned char) c) || isalpha((unsigned char) c)) {
            has_operand = 1;
        }

        p++;
    }

    return (has_operator && has_operand) ? 1 : 0;
}

/**
 * @brief 安全追加字符串到缓冲区
 *
 * @param dst     目标缓冲区当前位置
 * @param buf_end 缓冲区末尾
 * @param src     源字符串
 * @return 追加后的新位置，缓冲区不足返回 NULL
 */
static char *safe_append(char *dst, const char *buf_end, const char *src) {
    if (!dst || !buf_end || !src || dst >= buf_end) {
        return NULL;
    }

    while (*src && dst < buf_end - 1) {
        *dst++ = *src++;
    }
    *dst = '\0';
    return dst;
}

/**
 * @brief 去除首尾空白字符（原地标记）
 *
 * @param str 输入字符串
 * @param out 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return 写入的字符数
 */
static int trim_copy(const char *str, char *out, size_t buf_size) {
    const char *start;
    const char *end;
    size_t len;
    size_t i;

    if (!str || !out || buf_size == 0)
        return 0;

    /* 跳过前导空白 */
    start = str;
    while (*start && isspace((unsigned char) *start)) {
        start++;
    }

    /* 找到尾部非空白位置 */
    end = start + strlen(start);
    while (end > start && isspace((unsigned char) *(end - 1))) {
        end--;
    }

    len = (size_t) (end - start);
    if (len >= buf_size) {
        len = buf_size - 1;
    }

    for (i = 0; i < len; i++) {
        out[i] = start[i];
    }
    out[len] = '\0';

    return (int) len;
}

/* ================================================================
 *  公共 API 实现
 * ================================================================ */

/**
 * @brief 解析数学输入并输出规范化形式
 *
 * 将输入的数学表达式去除首尾空白，标准化空格使用，
 * 并写入到 normalized 缓冲区。
 *
 * @param input      输入数学表达式字符串
 * @param normalized 输出缓冲区（存放规范化结果）
 * @param buf_size   输出缓冲区大小
 * @return 写入规范化字符串的字符数（不含终止符），
 *         失败返回 -1
 */
int lv_math_input_parse(const char *input, char *normalized, size_t buf_size) {
    const char *p;
    char *out;
    const char *out_end;
    int last_was_space = 0;

    if (!input || !normalized || buf_size < 2) {
        return -1;
    }

    /* 跳过前导空白 */
    p = skip_whitespace(input);
    if (!p) {
        normalized[0] = '\0';
        return 0;
    }

    out = normalized;
    out_end = normalized + buf_size - 1; /* 预留终止符空间 */

    /* 逐字符复制并规范化空白 */
    while (*p && out < out_end) {
        if (isspace((unsigned char) *p)) {
            /* 连续空白合并为单个空格 */
            if (!last_was_space) {
                *out++ = ' ';
                last_was_space = 1;
            }
            p++;
        } else {
            *out++ = *p++;
            last_was_space = 0;
        }
    }

    /* 去除尾部空白 */
    if (out > normalized && *(out - 1) == ' ') {
        out--;
    }

    *out = '\0';
    return (int) (out - normalized);
}

/**
 * @brief 检测数学输入的格式类型
 *
 * 按优先级检测：JSON > LaTeX > ASCII 数学 > 自然语言。
 *
 * @param input 输入字符串
 * @return 格式类型常量：
 *         MATH_FORMAT_LATEX (1)  —— LaTeX 格式
 *         MATH_FORMAT_ASCII (2)  —— ASCII 数学格式
 *         MATH_FORMAT_NATURAL (3) —— 自然语言描述
 *         MATH_FORMAT_JSON (4)   —— JSON 格式
 *         MATH_FORMAT_UNKNOWN (0) —— 无法识别
 */
int lv_math_input_detect_format(const char *input) {
    const char *p;

    if (!input) {
        return MATH_FORMAT_UNKNOWN;
    }

    p = skip_whitespace(input);
    if (!p || *p == '\0') {
        return MATH_FORMAT_UNKNOWN;
    }

    /* 优先检测 JSON 格式 */
    if (has_json_features(p)) {
        return MATH_FORMAT_JSON;
    }

    /* 检测 LaTeX 格式 */
    if (has_latex_features(p)) {
        return MATH_FORMAT_LATEX;
    }

    /* 检测 ASCII 数学格式 */
    if (has_ascii_math_features(p)) {
        return MATH_FORMAT_ASCII;
    }

    /* 默认归类为自然语言描述 */
    return MATH_FORMAT_NATURAL;
}
