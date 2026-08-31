#ifndef lv_PARSER_SAFETY_H
#define lv_PARSER_SAFETY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

#include "error_codes.h" /* lvErrorCode */

/** Validate input before parsing. Returns lv_OK or error code. */
lvErrorCode lv_input_validate(const char *input, size_t len);

/** 净化输入字符串（就地修改），返回净化后的字符串长度。 */
size_t lv_input_sanitize(char *input, size_t max_len);

/** 检查 AST 深度是否超过安全上限（K28：接入主解析链）。 */
lvErrorCode lv_check_ast_depth(int depth);

/** 检查 AST 节点数量是否超过安全上限。 */
lvErrorCode lv_check_ast_node_count(int count);

/** 检查 Token 长度是否超过安全上限。 */
lvErrorCode lv_check_token_length(size_t len);

/** 检查字节是否为安全控制字符（K20 无头声明实现补齐）。 */
bool lv_char_is_safe_ctrl(unsigned char c);

#ifdef __cplusplus
}
#endif
#endif
