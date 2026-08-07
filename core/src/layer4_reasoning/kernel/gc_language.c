/**
 * @file gc_language.c
 * @brief GC 语言支持模块（子目录版本）
 *
 * 提供 GC（几何构造）语言的解析入口、错误报告和命令计数。
 * GC 语言是一种用于描述几何构造和约束的 DSL。
 */

#include "lv/gc_language.h"
#include "lv/lv_str_utils.h"

#include <ctype.h>
#include <string.h>

/* ================================================================
 *  内部状态
 * ================================================================ */

/** @brief GC 语言解析器全局状态 */
typedef struct {
    char error_buf[256]; /**< 最近一次解析错误信息 */
    int has_error;       /**< 是否存在未清除的错误 */
    int cmd_count;       /**< 已解析的命令计数 */
} GcLanguageState;

/** @brief GC 语言解析器全局单例 */
static GcLanguageState s_gc_language = {0};

/* ================================================================
 *  关键字定义
 * ================================================================ */

/**
 * @brief GC 语言关键字表
 */
static const char *gc_keywords[] = {"point",    "line",          "circle",    "segment",  "ray",
                                    "parallel", "perpendicular", "intersect", "midpoint", "constraint",
                                    "prove",    "let",           "assert",    NULL};

/* ================================================================
 *  内部辅助函数
 * ================================================================ */

/**
 * @brief 设置错误信息
 */
static void gc_set_error(const char *msg) {
    if (msg) {
        strncpy(s_gc_language.error_buf, msg, sizeof(s_gc_language.error_buf) - 1);
        s_gc_language.error_buf[sizeof(s_gc_language.error_buf) - 1] = '\0';
    } else {
        s_gc_language.error_buf[0] = '\0';
    }
    s_gc_language.has_error = 1;
}

/**
 * @brief 跳过空白字符
 * @return 跳过后的指针位置
 */
static const char *gc_skip_whitespace(const char *p) {
    /* lv_str_ltrim 不修改原串（仅返回首指针），const 转换安全 */
    return lv_str_ltrim((char *) p);
}

/**
 * @brief 判断是否为标识符起始字符
 */
static int gc_is_ident_start(char c) {
    return isalpha((unsigned char) c) || c == '_';
}

/**
 * @brief 判断是否为标识符延续字符
 */
static int gc_is_ident_char(char c) {
    return isalnum((unsigned char) c) || c == '_' || c == '-';
}

/**
 * @brief 尝试解析一个标识符
 *
 * @param p     输入指针
 * @param buf   输出缓冲区
 * @param bufsz 缓冲区大小
 * @return 解析后的新指针位置，失败返回 NULL
 */
static const char *gc_parse_identifier(const char *p, char *buf, int bufsz) {
    int len = 0;
    if (!gc_is_ident_start(*p)) {
        return NULL;
    }
    while (*p && gc_is_ident_char(*p) && len < bufsz - 1) {
        buf[len++] = *p++;
    }
    buf[len] = '\0';
    return p;
}

/**
 * @brief 跳过行注释（// 风格）
 */
static const char *gc_skip_line_comment(const char *p) {
    while (*p && *p != '\n') {
        p++;
    }
    return p;
}

/* ================================================================
 *  公共 API 实现
 * ================================================================ */

/**
 * @brief 解析 GC 语言源码
 *
 * 逐字符扫描源码，识别关键字、标识符、注释和命令分隔符。
 * 统计命令数量并报告语法错误。
 *
 * @param source GC 语言源码字符串
 * @param engine 引擎句柄（当前未使用，预留接口）
 * @return 0 解析成功，-1 参数错误或解析失败
 */
int lv_gc_parse(const char *source, void *engine) {
    const char *p;
    char ident[128];

    (void) engine; /* 预留：未来传递给引擎注册构造 */

    if (!source) {
        gc_set_error("source is NULL");
        return -1;
    }

    /* 重置状态 */
    s_gc_language.has_error = 0;
    s_gc_language.cmd_count = 0;
    s_gc_language.error_buf[0] = '\0';

    p = source;
    while (*p) {
        p = gc_skip_whitespace(p);
        if (!*p)
            break;

        /* 跳过行注释 */
        if (*p == '/' && *(p + 1) == '/') {
            p = gc_skip_line_comment(p + 2);
            continue;
        }

        /* 跳过命令分隔符 */
        if (*p == ';' || *p == '\n') {
            p++;
            continue;
        }

        /* 尝试解析标识符/关键字 */
        if (gc_is_ident_start(*p)) {
            const char *next = gc_parse_identifier(p, ident, (int) sizeof(ident));
            if (!next) {
                gc_set_error("failed to parse identifier");
                return -1;
            }
            /* 检查是否为已知关键字 */
            {
                int i;
                int is_keyword = 0;
                for (i = 0; gc_keywords[i] != NULL; i++) {
                    if (strcmp(ident, gc_keywords[i]) == 0) {
                        is_keyword = 1;
                        break;
                    }
                }
                if (is_keyword) {
                    s_gc_language.cmd_count++;
                }
            }
            p = next;
            continue;
        }

        /* 跳过花括号、圆括号等分隔符 */
        if (*p == '{' || *p == '}' || *p == '(' || *p == ')' || *p == ',') {
            p++;
            continue;
        }

        /* 跳过赋值和运算符 */
        if (*p == '=' || *p == ':' || *p == '+' || *p == '-' || *p == '*' || *p == '/') {
            p++;
            continue;
        }

        /* 跳过数字字面量 */
        if (isdigit((unsigned char) *p) || *p == '.') {
            while (*p && (isdigit((unsigned char) *p) || *p == '.')) {
                p++;
            }
            continue;
        }

        /* 未知字符 */
        {
            char err_msg[64];
            snprintf(err_msg, sizeof(err_msg), "unexpected character '%c' (0x%02X)", *p, (unsigned char) *p);
            gc_set_error(err_msg);
            return -1;
        }
    }

    return 0;
}

/**
 * @brief 获取最近一次解析错误信息
 *
 * @return 错误信息字符串（内部存储，勿释放），无错误返回 NULL
 */
const char *lv_gc_error(void) {
    if (s_gc_language.has_error) {
        return s_gc_language.error_buf;
    }
    return NULL;
}

/**
 * @brief 获取已解析的命令数量
 *
 * @return 命令计数
 */
int lv_gc_command_count(void) {
    return s_gc_language.cmd_count;
}
