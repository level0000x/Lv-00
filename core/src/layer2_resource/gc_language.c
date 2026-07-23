/**
 * @file gc_language.c
 * @brief GC（几何构造）语言解析模块 —— Layer2 资源管理层
 *
 * 提供 GC 语言的解析入口、错误报告和命令计数。
 * GC 语言是用于描述几何构造和约束的 DSL（领域特定语言），
 * 支持 point、line、circle、segment、constraint、prove 等关键字。
 *
 * @version 1.0.0
 */

#include "lv/gc_language.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* ================================================================
 *  内部状态（线程安全：使用线程局部存储）
 * ================================================================ */

/** 最近一次解析错误信息缓冲区 */
static lv_THREAD_LOCAL char g_gc_error_buf[256];

/** 是否存在未清除的错误 */
static lv_THREAD_LOCAL int g_gc_has_error = 0;

/** 已解析的命令计数 */
static lv_THREAD_LOCAL int g_gc_cmd_count = 0;

/* ================================================================
 *  关键字定义
 * ================================================================ */

/**
 * @brief GC 语言关键字表
 *
 * 包含所有支持的几何构造关键字，NULL 为终止标记。
 */
static const char *gc_keywords[] = {
    "point",       /* 点构造 */
    "line",        /* 直线构造 */
    "circle",      /* 圆构造 */
    "segment",     /* 线段构造 */
    "ray",         /* 射线构造 */
    "parallel",    /* 平行约束 */
    "perpendicular", /* 垂直约束 */
    "intersect",   /* 交点计算 */
    "midpoint",    /* 中点计算 */
    "constraint",  /* 通用约束 */
    "prove",       /* 证明声明 */
    "let",         /* 变量绑定 */
    "assert",      /* 断言声明 */
    NULL           /* 终止标记 */
};

/* ================================================================
 *  内部辅助函数
 * ================================================================ */

/**
 * @brief 设置错误信息到线程局部缓冲区
 * @param msg 错误信息（NULL 则清空）
 */
static void gc_set_error(const char *msg)
{
    if (msg) {
        strncpy(g_gc_error_buf, msg, sizeof(g_gc_error_buf) - 1);
        g_gc_error_buf[sizeof(g_gc_error_buf) - 1] = '\0';
    } else {
        g_gc_error_buf[0] = '\0';
    }
    g_gc_has_error = 1;
}

/**
 * @brief 跳过空白字符
 * @param p 当前解析位置
 * @return 跳过空白后的新位置
 */
static const char *gc_skip_whitespace(const char *p)
{
    if (!p) return NULL;
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }
    return p;
}

/**
 * @brief 判断是否为标识符起始字符（字母或下划线）
 */
static int gc_is_ident_start(char c)
{
    return isalpha((unsigned char)c) || c == '_';
}

/**
 * @brief 判断是否为标识符延续字符（字母、数字、下划线或连字符）
 */
static int gc_is_ident_char(char c)
{
    return isalnum((unsigned char)c) || c == '_' || c == '-';
}

/**
 * @brief 跳过行注释（// 风格）
 * @param p 指向注释起始 '//' 之后的字符
 * @return 跳过注释后的新位置
 */
static const char *gc_skip_line_comment(const char *p)
{
    if (!p) return NULL;
    while (*p && *p != '\n') {
        p++;
    }
    return p;
}

/**
 * @brief 尝试解析一个标识符
 *
 * @param p     输入指针
 * @param buf   输出缓冲区
 * @param bufsz 缓冲区大小
 * @return 解析后的新指针位置，失败返回 NULL
 */
static const char *gc_parse_identifier(const char *p, char *buf, int bufsz)
{
    int len = 0;

    if (!p || !buf || bufsz <= 0) {
        return NULL;
    }

    if (!gc_is_ident_start(*p)) {
        return NULL;
    }

    while (*p && gc_is_ident_char(*p) && len < bufsz - 1) {
        buf[len++] = *p++;
    }
    buf[len] = '\0';

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
 * @param engine 引擎句柄（当前预留接口，传 NULL 有效）
 * @return 0 解析成功，-1 参数错误或解析失败
 */
int lv_gc_parse(const char *source, void *engine)
{
    const char *p;
    char ident[128];

    (void)engine;  /* 预留：未来传递给引擎注册构造 */

    if (!source) {
        gc_set_error("source is NULL");
        return -1;
    }

    /* 重置状态 */
    g_gc_has_error = 0;
    g_gc_cmd_count = 0;
    g_gc_error_buf[0] = '\0';

    p = source;
    while (*p) {
        p = gc_skip_whitespace(p);
        if (!*p) break;

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
            const char *next = gc_parse_identifier(p, ident, (int)sizeof(ident));
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
                    g_gc_cmd_count++;
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
        if (isdigit((unsigned char)*p) || *p == '.') {
            while (*p && (isdigit((unsigned char)*p) || *p == '.')) {
                p++;
            }
            continue;
        }

        /* 跳过字符串字面量 */
        if (*p == '"') {
            p++;
            while (*p && *p != '"') {
                if (*p == '\\') p++;  /* 跳过转义字符 */
                p++;
            }
            if (*p == '"') p++;
            continue;
        }

        /* 未知字符：报告错误 */
        {
            char err_msg[64];
            snprintf(err_msg, sizeof(err_msg),
                     "unexpected character '%c' (0x%02X)", *p, (unsigned char)*p);
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
const char *lv_gc_error(void)
{
    if (g_gc_has_error) {
        return g_gc_error_buf;
    }
    return NULL;
}

/**
 * @brief 获取已解析的 GC 命令数量
 *
 * @return 命令计数（即遇到的关键字数量）
 */
int lv_gc_command_count(void)
{
    return g_gc_cmd_count;
}
