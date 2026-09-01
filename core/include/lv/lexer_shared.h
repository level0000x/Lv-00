/**
 * @file lexer_shared.h
 * @brief 共享词法分析器基础设施
 *
 * @details 为 axiom_pkg 和 module 提供公共的词法分析器基础类型和辅助函数。
 *          两个模块各自维护自己的 Token 类型和 lexer_next_token 实现
 *          （因数字解析规则和标识符规则不同），但共享 Lexer 结构体和
 *          空白/注释跳过、字符串字面量提取等公共操作。
 *
 *          共享范围：
 *          - Lexer 结构体（source, pos, line, col, error_msg）
 *          - lexer_init()：初始化词法分析器
 *          - lexer_skip_whitespace_and_comments()：跳过空白和注释
 *          - lexer_extract_string()：提取字符串字面量（含转义处理）
 *
 *          不共享范围（各自实现）：
 *          - Token 类型定义（字段差异：int vs double，有无 bool）
 *          - lexer_next_token()（数字解析和标识符规则不同）
 *
 * @author Lv-00 Project
 * @version 1.1.0
 */
#ifndef lv_LEXER_SHARED_H
#define lv_LEXER_SHARED_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* ================================================================
 *  词法分析器结构体
 * ================================================================ */
/**
 * @brief 词法分析器状态
 *
 * 追踪当前解析位置、行列号和错误信息。
 * 被 axiom_pkg 和 module 的解析器共同使用。
 */
typedef struct {
    const char *source; /**< 输入源字符串（以 null 终止） */
    const char *pos;    /**< 当前解析位置指针 */
    int line;           /**< 当前行号（从 1 开始） */
    int col;            /**< 当前列号（从 1 开始） */
    char *error_msg;    /**< 错误消息（堆分配，调用者负责释放） */
} lvLexer;
/* ================================================================
 *  公共词法分析器函数
 * ================================================================ */
/**
 * @brief 初始化词法分析器
 *
 * @param lex    指向词法分析器结构体的指针
 * @param source 要解析的源字符串（词法分析器不获取所有权）
 */
void lv_lexer_init(lvLexer *lex, const char *source);
/**
 * @brief 重置/清除词法分析器状态
 *
 * 释放词法分析器内部的堆分配资源（如 error_msg），
 * 将分析器重置为安全初始状态，以便重用。
 * 调用后词法分析器可以安全地通过 lv_lexer_init 重新初始化。
 *
 * @param lex 指向词法分析器结构体的指针
 */
void lv_lexer_clear(lvLexer *lex);
/**
 * @brief 跳过空白字符和注释
 *
 * 跳过空格、制表符、换行符等空白字符，以及从 '#' 到行尾的注释。
 * 在跳过过程中自动更新行号和列号。
 *
 * @param lex 指向词法分析器结构体的指针
 */
void lv_lexer_skip_whitespace_and_comments(lvLexer *lex);
/**
 * @brief 提取字符串字面量（含转义处理）
 *
 * 调用前，lex->pos 应指向字符串内容起始位置（即开引号 '"' 之后的位置）。
 * 该函数会：
 * 1. 计算内容长度（正确处理转义序列）
 * 2. 分配并填充结果字符串（解码转义序列 \n \t \r \" \\）
 * 3. 消费闭合引号
 *
 * 支持的转义序列：\\n, \\t, \\r, \\\", \\\\, 以及任意字符的 \\X
 *
 * @param lex 指向词法分析器结构体的指针
 * @return    新分配的字符串（[take] 语义：lv_malloc 分配，调用者须用 lv_free
 *            释放——原注释「free()」为混合分配器 UB，见 memory-ownership.md
 *            K10/F39），失败时返回 NULL
 */
char *lv_lexer_extract_string(lvLexer *lex);
#ifdef __cplusplus
}
#endif
#endif /* lv_LEXER_SHARED_H */