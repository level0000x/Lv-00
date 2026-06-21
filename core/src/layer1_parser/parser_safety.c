/**
 * @file parser_safety.c
 * @brief 解析器安全加固实现
 *
 * @details 实现输入验证、输入净化、AST安全检查等安全函数。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#include "parser_safety.h"
#include "config.h"          /* LV00_MAX_* macros */

#include <ctype.h>
#include <string.h>

#include "lv00_utils.h"

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/**
 * @brief 检查字节是否为不可打印的控制字符（排除允许的空格制表换行）
 *
 * @param c 字节值
 * @return true  是不允许的控制字符
 * @return false 是允许的字符
 */
static bool is_disallowed_ctrl(unsigned char c) {
    /* 允许: 空格(0x20), 制表(0x09), 换行(0x0A), 回车(0x0D) */
    if (c >= 0x20) return false;  /* 可打印字符 */
    if (c == 0x09) return false;  /* '\t' */
    if (c == 0x0A) return false;  /* '\n' */
    if (c == 0x0D) return false;  /* '\r' */
    return true; /* 其他控制字符 (0x00-0x08, 0x0B-0x0C, 0x0E-0x1F) */
}

/**
 * @brief 检查字节是否为Unicode空白字符（多字节序列的起始字节）
 *
 * @details 识别常见Unicode空白字符的UTF-8编码起始字节：
 *          - U+00A0 (NBSP):            0xC2 0xA0
 *          - U+1680 (Ogham):           0xE1 0x9A 0x80
 *          - U+2000-U+200A (各种空格): 0xE2 0x80 0x80-0x8A
 *          - U+2028 (行分隔符):        0xE2 0x80 0xA8
 *          - U+2029 (段落分隔符):      0xE2 0x80 0xA9
 *          - U+202F (窄不间断空格):    0xE2 0x80 0xAF
 *          - U+205F (数学空格):        0xE2 0x81 0x9F
 *          - U+3000 (中文空格):        0xE3 0x80 0x80
 *          - U+FEFF (BOM/零宽):       0xEF 0xBB 0xBF
 *
 * @param c  字节值
 * @param n2 下一个字节（若可用）
 * @param n3 下下个字节（若可用）
 * @return 该字符的UTF-8编码字节数（1-3），0表示不是Unicode空白
 */
static int is_unicode_whitespace_start(unsigned char c, unsigned char n2, unsigned char n3) {
    switch (c) {
        case 0xC2:
            /* U+00A0 NBSP: C2 A0 */
            return (n2 == 0xA0) ? 2 : 0;
        case 0xE1:
            /* U+1680: E1 9A 80 */
            return (n2 == 0x9A && n3 == 0x80) ? 3 : 0;
        case 0xE2:
            /* U+2000-U+200A: E2 80 80-8A */
            if (n2 == 0x80 && n3 >= 0x80 && n3 <= 0x8A) return 3;
            /* U+2028: E2 80 A8, U+2029: E2 80 A9, U+202F: E2 80 AF */
            if (n2 == 0x80 && (n3 == 0xA8 || n3 == 0xA9 || n3 == 0xAF)) return 3;
            /* U+205F: E2 81 9F */
            if (n2 == 0x81 && n3 == 0x9F) return 3;
            return 0;
        case 0xE3:
            /* U+3000: E3 80 80 */
            return (n2 == 0x80 && n3 == 0x80) ? 3 : 0;
        case 0xEF:
            /* U+FEFF: EF BB BF */
            return (n2 == 0xBB && n3 == 0xBF) ? 3 : 0;
        default:
            return 0;
    }
}

/* ============================================================
 * 公共 API 实现
 * ============================================================ */

/**
 * @brief 检查字节是否为安全的控制字符
 *
 * 判断给定字节是否属于允许的控制字符集合（空格、制表符、换行符、回车符），
 * 或为可打印 ASCII 字符。
 *
 * @param c 字节值
 * @return true  字符安全（可打印或允许的控制字符）
 * @return false 字符不安全（不允许的控制字符）
 */
bool lv00_char_is_safe_ctrl(unsigned char c) {
    if (c >= 0x20) return true;  /* 可打印ASCII */
    if (c == 0x09) return true;  /* '\t' */
    if (c == 0x0A) return true;  /* '\n' */
    if (c == 0x0D) return true;  /* '\r' */
    return false;
}

/**
 * @brief 验证输入字符串的安全性
 *
 * 检查输入是否为 NULL、是否为空、是否超过最大长度限制，
 * 以及是否包含非法字符（null 字节或不允许的控制字符）。
 *
 * @param input 输入字符串指针
 * @param len   输入字符串长度
 * @return LV00_OK 验证通过，其他值为具体错误码
 */
Lv00ErrorCode lv00_input_validate(const char *input, size_t len) {
    /* 检查1：非NULL且非空 */
    if (!input) {
        lv00_set_error(LV00_ERROR_PARSER_NULL_INPUT, "输入字符串为NULL");
        return LV00_ERROR_PARSER_NULL_INPUT;
    }

    if (len == 0) {
        lv00_set_error(LV00_ERROR_PARSER_EMPTY_INPUT, "输入字符串为空");
        return LV00_ERROR_PARSER_EMPTY_INPUT;
    }

    /* 检查2：长度上限 */
    if (len > LV00_MAX_INPUT_LENGTH) {
        lv00_set_error(LV00_ERROR_PARSER_INPUT_TOO_LONG,
                       "输入长度 %zu 超过上限 %d", len, LV00_MAX_INPUT_LENGTH);
        return LV00_ERROR_PARSER_INPUT_TOO_LONG;
    }

    /* 检查3：扫描非法字符 */
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char) input[i];

        /* null字节检查 */
        if (c == 0x00) {
            lv00_set_error(LV00_ERROR_PARSER_ILLEGAL_CHARS,
                           "输入在位置 %zu 包含null字节", i);
            return LV00_ERROR_PARSER_ILLEGAL_CHARS;
        }

        /* 不允许的控制字符检查 */
        if (is_disallowed_ctrl(c)) {
            lv00_set_error(LV00_ERROR_PARSER_ILLEGAL_CHARS,
                           "输入在位置 %zu 包含非法控制字符 0x%02X", i, (unsigned int) c);
            return LV00_ERROR_PARSER_ILLEGAL_CHARS;
        }
    }

    return LV00_OK;
}

/**
 * @brief 净化输入字符串（就地修改）
 *
 * 对输入字符串进行就地净化处理：
 * - 将多字节 Unicode 空白字符替换为 ASCII 空格
 * - 移除 null 字节
 * - 将不允许的控制字符替换为空格
 * - 统一换行符格式（\r\n -> \n, \r -> \n）
 *
 * @param input   输入字符串缓冲区（就地修改）
 * @param max_len 缓冲区最大长度
 * @return 净化后的字符串长度
 */
size_t lv00_input_sanitize(char *input, size_t max_len) {
    if (!input || max_len == 0) return 0;

    size_t read = 0;
    size_t write = 0;

    while (read < max_len && input[read] != '\0') {
        unsigned char c = (unsigned char) input[read];
        unsigned char n2 = (read + 1 < max_len) ? (unsigned char) input[read + 1] : 0;
        unsigned char n3 = (read + 2 < max_len) ? (unsigned char) input[read + 2] : 0;

        /* 检查是否为Unicode空白字符 */
        int ws_len = is_unicode_whitespace_start(c, n2, n3);
        if (ws_len > 0) {
            /* 替换多字节Unicode空白为单个ASCII空格 */
            input[write++] = ' ';
            read += ws_len;
            continue;
        }

        /* null字节：跳过 */
        if (c == 0x00) {
            read++;
            continue;
        }

        /* 不允许的控制字符：替换为空格 */
        if (is_disallowed_ctrl(c)) {
            input[write++] = ' ';
            read++;
            continue;
        }

        /* 回车符 '\r'：统一为 '\n'（除非后面是 '\n') */
        if (c == '\r') {
            if (read + 1 < max_len && input[read + 1] == '\n') {
                /* \r\n -> \n */
                input[write++] = '\n';
                read += 2;
            } else {
                input[write++] = '\n';
                read++;
            }
            continue;
        }

        /* 正常字符：原样保留 */
        input[write++] = input[read++];
    }

    /* null终止 */
    input[write] = '\0';
    return write;
}

/**
 * @brief 检查 AST 深度是否超过安全上限
 *
 * @param depth 当前 AST 深度
 * @return LV00_OK 深度在安全范围内，其他值为错误码
 */
Lv00ErrorCode lv00_check_ast_depth(int depth) {
    if (depth > LV00_MAX_AST_DEPTH) {
        lv00_set_error(LV00_ERROR_PARSER_DEPTH_EXCEEDED,
                       "AST深度 %d 超过上限 %d", depth, LV00_MAX_AST_DEPTH);
        return LV00_ERROR_PARSER_DEPTH_EXCEEDED;
    }
    return LV00_OK;
}

/**
 * @brief 检查 AST 节点数量是否超过安全上限
 *
 * @param count 当前 AST 节点数量
 * @return LV00_OK 节点数在安全范围内，其他值为错误码
 */
Lv00ErrorCode lv00_check_ast_node_count(int count) {
    if (count > LV00_MAX_AST_NODES) {
        lv00_set_error(LV00_ERROR_PARSER_NODE_LIMIT,
                       "AST节点数 %d 超过上限 %d", count, LV00_MAX_AST_NODES);
        return LV00_ERROR_PARSER_NODE_LIMIT;
    }
    return LV00_OK;
}

/**
 * @brief 检查 Token 长度是否超过安全上限
 *
 * @param len 当前 Token 长度
 * @return LV00_OK Token 长度在安全范围内，其他值为错误码
 */
Lv00ErrorCode lv00_check_token_length(size_t len) {
    if (len > LV00_MAX_TOKEN_LENGTH) {
        lv00_set_error(LV00_ERROR_PARSER_TOKEN_TOO_LONG,
                       "Token长度 %zu 超过上限 %d", len, LV00_MAX_TOKEN_LENGTH);
        return LV00_ERROR_PARSER_TOKEN_TOO_LONG;
    }
    return LV00_OK;
}
