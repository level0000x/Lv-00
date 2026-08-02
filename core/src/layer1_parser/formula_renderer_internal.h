/**
 * @file formula_renderer_internal.h
 * @brief 公式渲染器内部共享声明（从 formula_renderer.c 拆分）
 *
 * @details 由 formula_renderer.c 各拆分模块共享的常量、类型与函数声明。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#ifndef lv_FORMULA_RENDERER_INTERNAL_H
#define lv_FORMULA_RENDERER_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "formula_renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- 缓冲区大小控制宏 ---------- */
#define lv_MAX_RENDER_BUFFER 16384     /* 渲染输出缓冲区大小 */
#define lv_MAX_ERROR_MESSAGE 256       /* 错误消息缓冲区大小 */
#define lv_POINT_LATEX_BUF_SIZE 256    /* 点坐标 LaTeX 渲染缓冲区大小 */
#define lv_SEGMENT_LATEX_BUF_SIZE 128  /* 线段 LaTeX 渲染缓冲区大小 */
#define lv_CIRCLE_LATEX_BUF_SIZE 512   /* 圆 LaTeX 渲染缓冲区大小 */
#define lv_FRACTION_LATEX_BUF_SIZE 128 /* 分数 LaTeX 渲染缓冲区大小 */

#ifndef lv_FORMULA_BUF_SIZE
#define lv_FORMULA_BUF_SIZE 1024
#endif

#define lv_FORMULA_BUF_SMALL 64
#define lv_FORMULA_BUF_MEDIUM 256
#define lv_FORMULA_BUF_LARGE 2048

#define lv_FORMULA_POOL_SLOTS 8

/* ---------- 缓冲区池槽位 ---------- */
typedef struct {
    char *data;  /**< 堆分配的缓冲区，未初始化时为 NULL */
    bool in_use; /**< 当前是否被占用 */
} FormulaPoolSlot;

/* ---------- 希腊字母/三角函数映射 ---------- */
typedef struct {
    const char *name;
    const char *latex;
} GreekLetterMapping;

typedef struct {
    const char *name;
    const char *latex;
} TrigFunctionMapping;

/* ---------- 渲染节点函数签名 ---------- */
typedef int (*RenderNodeFunc)(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options);

/* ---------- 共享遍历器标志 ---------- */
enum {
    RENDER_VIA_CHECK_RET = 1u << 0, /**< 子渲染返回 <0 时传播错误（返回 -1） */
    RENDER_VIA_ERROR_CTX = 1u << 1, /**< 失败时通过 lv_RETURN_ERROR 记录错误上下文 */
};

/* ---------- 缓冲区池（在 formula_renderer_internal.c 实现） ---------- */
char *formula_pool_alloc(size_t size);
void formula_pool_free(char *ptr);

/* ---------- 通用渲染工具（在 formula_renderer_internal.c 实现） ---------- */
bool needs_parentheses(const FormulaNode *node, NodeType parent_op, bool is_right);
const char *get_trig_latex(const char *name);
bool is_greek_letter(const char *name);

/* ---------- 共享遍历器（在 formula_renderer_internal.c 实现） ---------- */
int render_binary_via(const FormulaNode *node, const char *fmt, unsigned flags, char *buffer, size_t size,
                      const RenderOptions *options, RenderNodeFunc dispatch);
int render_unary_via(const FormulaNode *node, const char *prefix, const char *suffix, unsigned flags,
                     char *buffer, size_t size, const RenderOptions *options, RenderNodeFunc dispatch);
int dispatch_via(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options,
                 const RenderNodeFunc *table, size_t table_count, RenderNodeFunc fallback);
const char *formula_render_trig_name(const FormulaNode *node, const char *const *names, size_t count);

/* ---------- 各后端内部渲染入口（在对应 *_backend.c 实现） ---------- */
int render_latex_internal(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options);
int render_python_internal(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options);
int render_dsl_internal(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options);
int render_mathml_internal(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options);
int render_ascii_internal(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options);
int render_html_internal(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options);

#ifdef __cplusplus
}
#endif

#endif /* lv_FORMULA_RENDERER_INTERNAL_H */
