/**
 * @file parser_safety.h
 * @brief 解析器安全加固 —— 常量定义与输入校验/净化API
 *
 * @details 提供解析器的输入安全边界常量、输入验证和净化函数。
 *          涵盖输入长度限制、token化安全、AST深度/节点数限制，
 *          以及控制字符过滤和Unicode空白标准化。
 *
 *          所有安全常量集中在此定义，确保一处修改、全局生效。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#ifndef LV00_PARSER_SAFETY_H
#define LV00_PARSER_SAFETY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

#include "error_codes.h"

/* ============================================================
 * 安全边界常量
 * ============================================================ */

/** @brief 输入字符串最大长度（字节），防止超大输入导致DoS */
#define LV00_MAX_INPUT_LENGTH  65536

/** @brief 单个token的最大长度（字节），防止超长标识符/数字耗尽内存 */
#define LV00_MAX_TOKEN_LENGTH  512

/** @brief 单次解析的最大token数量，防止token爆炸攻击 */
#define LV00_MAX_TOKEN_COUNT   10000

/** @brief AST最大嵌套深度，防止递归栈溢出 */
#define LV00_MAX_AST_DEPTH     256

/** @brief AST最大节点总数，防止内存耗尽攻击 */
#define LV00_MAX_AST_NODES     10000

/* ============================================================
 * 解析器安全错误码（130-139）
 * ============================================================ */

/** @brief 输入字符串为NULL */
#define LV00_ERROR_PARSER_NULL_INPUT        130

/** @brief 输入字符串为空（仅包含空白或长度为0） */
#define LV00_ERROR_PARSER_EMPTY_INPUT       131

/** @brief 输入长度超过最大限制 */
#define LV00_ERROR_PARSER_INPUT_TOO_LONG    132

/** @brief 输入包含非法控制字符或null字节 */
#define LV00_ERROR_PARSER_ILLEGAL_CHARS     133

/** @brief Token化阶段token数量超过最大限制 */
#define LV00_ERROR_PARSER_TOO_MANY_TOKENS   134

/** @brief AST深度超过最大限制 */
#define LV00_ERROR_PARSER_DEPTH_EXCEEDED    135

/** @brief AST节点总数超过最大限制 */
#define LV00_ERROR_PARSER_NODE_LIMIT        136

/** @brief token长度超限 */
#define LV00_ERROR_PARSER_TOKEN_TOO_LONG    137

/** @brief 内存池分配失败 */
#define LV00_ERROR_PARSER_POOL_EXHAUSTED    138

/* ── 向后兼容的枚举别名 ── */
#ifndef LV00_PARSER_SAFETY_ENUM_ALIASES
#define LV00_PARSER_SAFETY_ENUM_ALIASES

/* 这些宏直接映射到 Lv00ErrorCode 枚举值（定义于 error_codes.h），
   调用者可直接使用宏值或通过 lv00_error_string() 查询含义。 */

#endif

/* ============================================================
 * 公共 API —— 输入安全
 * ============================================================ */

/**
 * @brief 验证输入字符串是否符合解析器安全要求
 *
 * 检查项目：
 *   1. 非NULL且长度 > 0
 *   2. 长度不超过 LV00_MAX_INPUT_LENGTH
 *   3. 不含null字节（字符串中间嵌入 \0）
 *   4. 不含不可打印的控制字符（允许 \t, \n, \r）
 *
 * @param[in] input 输入的公式字符串（以null终止的C字符串）
 * @param[in] len   输入字符串的长度（可通过 strlen 获取）
 * @return LV00_OK 输入安全可用
 * @return 其他    错误码（使用 lv00_error_string 获取描述）
 */
Lv00ErrorCode lv00_input_validate(const char *input, size_t len);

/**
 * @brief 净化输入字符串（原地修改）
 *
 * 执行以下操作：
 *   1. 将Unicode空白字符（如 U+00A0 不间断空格、U+2000-U+200A等）
 *      转换为标准ASCII空格
 *   2. 将所有换行符统一为 '\n'
 *   3. 将控制字符（除 \t, \n, \r 外）替换为空格
 *   4. 移除嵌入的null字节
 *   5. 移除首尾空白（可选，默认开启）
 *
 * @param[in,out] input   要净化的输入字符串（原地修改）
 * @param[in]     max_len 输入缓冲区最大容量
 * @return 净化后的有效字符串长度（不含终止符），
 *         如果输入为NULL或max_len为0，返回0
 *
 * @note 此函数不分配新内存，仅在原缓冲区上原地修改。
 *       调用者需确保 input 指向可修改的内存。
 *       净化后的字符串长度 <= 原长度。
 */
size_t lv00_input_sanitize(char *input, size_t max_len);

/**
 * @brief 检查字符是否为允许的控制字符
 *
 * 在输入净化中，仅允许以下控制字符：
 *   - '\t' (0x09) 水平制表符
 *   - '\n' (0x0A) 换行符
 *   - '\r' (0x0D) 回车符
 *
 * @param[in] c 要检查的字符
 * @return true  字符是允许的控制字符
 * @return false 字符不是允许的控制字符
 */
bool lv00_char_is_safe_ctrl(unsigned char c);

/* ============================================================
 * 公共 API —— 解析器安全检查辅助
 * ============================================================ */

/**
 * @brief 检查AST深度是否在安全范围内
 *
 * @param[in] depth 当前AST深度
 * @return LV00_OK 深度安全
 * @return LV00_ERROR_PARSER_DEPTH_EXCEEDED 深度超限
 */
Lv00ErrorCode lv00_check_ast_depth(int depth);

/**
 * @brief 检查AST节点计数是否在安全范围内
 *
 * @param[in] count 当前AST节点数
 * @return LV00_OK 节点数安全
 * @return LV00_ERROR_PARSER_NODE_LIMIT 节点数超限
 */
Lv00ErrorCode lv00_check_ast_node_count(int count);

/**
 * @brief 检查token长度是否在安全范围内
 *
 * @param[in] len token长度（不含null终止符）
 * @return LV00_OK token长度安全
 * @return LV00_ERROR_PARSER_TOKEN_TOO_LONG token长度超限
 */
Lv00ErrorCode lv00_check_token_length(size_t len);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PARSER_SAFETY_H */
