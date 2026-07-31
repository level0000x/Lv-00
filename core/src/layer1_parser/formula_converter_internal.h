/**
 * @file formula_converter_internal.h
 * @brief 公式转换器内部共享声明（formula_eval.c / formula_string.c 与 formula_converter.c 共用）
 */

#ifndef lv_FORMULA_CONVERTER_INTERNAL_H
#define lv_FORMULA_CONVERTER_INTERNAL_H

#include <stddef.h>

#include "lv/formula_parser.h"

/* 表达式/结果缓冲区大小（formula_string.c 与 formula_converter.c 共享） */
#define FORMULA_EXPR_BUF_SIZE 128
#define FORMULA_BUF_SIZE 256
#define FORMULA_RESULT_BUF_SIZE 64
#define FORMULA_LARGE_BUF_SIZE 1024

#ifdef __cplusplus
extern "C" {
#endif

/* 公式节点求值（formula_eval.c） */
double eval_node(const FormulaNode *node, double x, double y);

/* 公式节点字符串渲染（formula_string.c） */
void node_to_string(const FormulaNode *node, char *buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* lv_FORMULA_CONVERTER_INTERNAL_H */
