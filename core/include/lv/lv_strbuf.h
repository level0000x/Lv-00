#ifndef lv_STRBUF_H
#define lv_STRBUF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/**
 * @file lv_strbuf.h
 * @brief 安全字符串构建器 —— 消除固定栈缓冲区 + snprintf 模式
 *
 * lvStrBuf 使用小字符串优化（SSO）：短字符串直接在栈上存储，
 * 长字符串自动切换到堆分配。始终保证 NUL 结尾。
 *
 * 用法：
 * @code
 *   lvStrBuf sb = {0};
 *   lv_strbuf_printf(&sb, "point(%d, %d)", x, y);
 *   use(sb.data);  // 或 lv_strbuf_cstr(&sb)
 *   lv_strbuf_destroy(&sb);
 * @endcode
 */

/** @brief 小字符串优化的栈缓冲区大小 */
#define lv_STRBUF_SSO_SIZE 256

/**
 * @brief 字符串构建器
 */
typedef struct lvStrBuf {
    char *data;                      /**< 字符串数据（始终 NUL 结尾） */
    char stack[lv_STRBUF_SSO_SIZE];  /**< SSO 栈缓冲区 */
    size_t len;                      /**< 当前字符串长度（不含 NUL） */
    size_t cap;                      /**< 总容量（含 NUL） */
} lvStrBuf;

/**
 * @brief 初始化字符串构建器（零初始化也可行）
 * @param sb  构建器指针
 */
void lv_strbuf_init(lvStrBuf *sb);

/**
 * @brief 追加格式化字符串
 * @param sb  构建器指针
 * @param fmt printf 风格格式
 * @param ... 可变参数
 */
void lv_strbuf_printf(lvStrBuf *sb, const char *fmt, ...);

/**
 * @brief 追加格式化字符串（va_list 版本）
 * @param sb   构建器指针
 * @param fmt  printf 风格格式
 * @param args 可变参数列表（由调用方负责 va_start/va_end）
 */
void lv_strbuf_vprintf(lvStrBuf *sb, const char *fmt, va_list args);

/**
 * @brief 获取 C 字符串（始终 NUL 结尾）
 * @param sb  构建器指针
 * @return  C 字符串指针
 */
static inline const char *lv_strbuf_cstr(const lvStrBuf *sb) {
    return sb ? sb->data : "";
}

/**
 * @brief 重置构建器（保留已分配内存）
 * @param sb  构建器指针
 */
void lv_strbuf_reset(lvStrBuf *sb);

/**
 * @brief 销毁构建器（释放堆内存）
 * @param sb  构建器指针（可 NULL）
 */
void lv_strbuf_destroy(lvStrBuf *sb);

/**
 * @brief 将 lvStrBuf 转换为堆分配的字符串并清理
 *
 * 从 lvStrBuf 中提取字符串内容（堆分配），然后销毁缓冲区。
 * 调用者负责使用 lv_free 释放返回的字符串。
 * 适用于在 to_string 函数末尾使用，返回堆分配结果。
 *
 * @param sb lvStrBuf 指针
 * @return 堆分配的 NUL 结尾字符串（调用者 lv_free），失败返回 NULL
 */
char *lv_strbuf_to_string(lvStrBuf *sb);

/**
 * @brief 重复追加字符 count 次
 * @param sb    lvStrBuf 指针
 * @param ch    要重复的字符
 * @param count 重复次数
 */
void lv_strbuf_append_n(lvStrBuf *sb, char ch, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* lv_STRBUF_H */
