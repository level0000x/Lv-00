#ifndef SAT_ENCODING_INTERNAL_H
#define SAT_ENCODING_INTERNAL_H

#include "lv/sat_encoding.h" /* SatEncoding / SatLiteral */

/* 定义在 sat_encoding.c（核心文件）：元组相等比较（解码子模块复用） */

/** @brief 比较两个元组是否相等
 * @param arity  元数
 * @param a      元组 A
 * @param b      元组 B
 * @return true 相等 */
bool tuple_equals(int arity, const int *a, const int *b);

#endif /* SAT_ENCODING_INTERNAL_H */
