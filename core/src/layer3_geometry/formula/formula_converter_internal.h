/**
 * @file formula_converter_internal.h
 * @brief 公式转换器内部共享声明
 *
 * @details 供 formula_eval.c / formula_string.c / formula_curve.c 与
 *          formula_converter.c 拆分出的各模块（util/geom/complex/
 *          constraint/stmt/export）共用。
 */

#ifndef lv_FORMULA_CONVERTER_INTERNAL_H
#define lv_FORMULA_CONVERTER_INTERNAL_H

#include <stddef.h>

#include "lv/formula_parser.h"
#include "lv/constraint_graph.h"
#include "lv/stream.h"
#include "lv/cross_platform.h"

/* 表达式/结果缓冲区大小（formula_string.c 与 formula_converter.c 共享） */
#define FORMULA_EXPR_BUF_SIZE 128
#define FORMULA_BUF_SIZE 256
#define FORMULA_RESULT_BUF_SIZE 64
#define FORMULA_LARGE_BUF_SIZE 1024

/* 公式转换器内部缓冲区大小常量（formula_converter.c 拆分模块共享） */
#define MAX_VAR_MAP_SIZE 256
#define MAX_NAME_LENGTH 64
#define FORMULA_LATEX_BUF_SIZE 512
#define FORMULA_PYTHON_BUF_SIZE 512
#define FORMULA_DSL_BUF_SIZE 512
#define FORMULA_EXPORT_BUF_SIZE 4096
#define FORMULA_SEG_LIST_SIZE 256
#define FORMULA_SEG_NAME_SIZE 64
#define FORMULA_VAR_MAP_SIZE 64

#ifdef __cplusplus
extern "C" {
#endif

/* 公式转换器流式上下文（formula_converter_util.c 定义，跨模块共享） */
extern lv_THREAD_LOCAL StreamContext *formula_converter_stream_ctx;

/* 内部转换函数（未在公共头 formula_converter.h 声明，由 stmt 分派调用） */
bool formula_convert_polygon(const FormulaNode *polygon_node, ConstraintGraph *graph, int *out_node_ids, int *out_count);
bool formula_convert_region(const FormulaNode *region_node, ConstraintGraph *graph, int *out_node_id);
bool formula_convert_arc(const FormulaNode *arc_node, ConstraintGraph *graph, int *out_node_ids, int *out_count);

/* 公式节点求值（formula_eval.c） */
double eval_node(const FormulaNode *node, double x, double y);

/* 公式节点字符串渲染（formula_string.c） */
void node_to_string(const FormulaNode *node, char *buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* lv_FORMULA_CONVERTER_INTERNAL_H */
