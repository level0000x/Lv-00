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
#include <stdio.h>

#include "formula_renderer.h"
#include "lv/lv_numeric.h" /* lv_index_in_range */

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
const char *formula_unary_fn_name(const FormulaNode *node);

/* ---------- 简单二元/一元算子模板表（表驱动渲染） ----------
 * 各后端（dsl/ascii/latex/python）保留各自的模板数据表（格式串为外部契约，内容不得修改），
 * 调度样板收敛到下述三个 inline 函数：render_binary_spec / render_unary_spec / render_fn_name_spec。
 * 数据表按下标 == NodeType 布局（如 [NODE_BINARY_OP_ADD] = {...}），空槽位以 NULL 标记。 */

/**
 * @brief 二元算子渲染模板条目：{模板串, 遍历器标志}
 *
 * 语义契约：tpl 为 render_binary_via 的 fmt 格式串（外部契约，内容不得修改）；
 * flags 为 RENDER_VIA_* 位组合（0 表示无标志）。
 * 前置条件：tpl 非 NULL（NULL 表示空槽位，视为未命中）。
 * 失败/截断语义：纯数据，无。
 * 边界行为：空槽位 tpl == NULL 视为未命中。
 * 扩展点：flags 随 RENDER_VIA_* 扩展。
 */
typedef struct {
    const char *tpl;
    unsigned flags;
} RenderBinarySpec;

/**
 * @brief 一元算子渲染模板条目：{前缀, 后缀, 遍历器标志}
 *
 * 语义契约：prefix/suffix 为 render_unary_via 的前后缀串（外部契约，内容不得修改）；
 * flags 为 RENDER_VIA_* 位组合（0 表示无标志）。
 * 前置条件：prefix 非 NULL（NULL 表示空槽位，视为未命中）。
 * 失败/截断语义：纯数据，无。
 * 边界行为：空槽位 prefix == NULL 视为未命中。
 * 扩展点：flags 随 RENDER_VIA_* 扩展。
 */
typedef struct {
    const char *prefix;
    const char *suffix;
    unsigned flags;
} RenderUnarySpec;

/**
 * @brief 函数名一元算子渲染模板条目：{前缀格式串, 后缀, 遍历器标志, 未知名回退串}
 *
 * 语义契约：prefix_fmt 为含单个 %s 的 printf 格式串（函数名代入，如 "%s(" 或 "\\%s\\left("）；
 * suffix 为 render_unary_via 的后缀串；flags 为 RENDER_VIA_* 位组合；
 * fallback 为 formula_unary_fn_name 返回 NULL 时的输出串（外部契约，内容不得修改）。
 * 前置条件：prefix_fmt 与 fallback 非 NULL。
 * 失败/截断语义：纯数据，无。
 * 边界行为：函数名前缀缓冲固定 lv_FORMULA_BUF_SMALL，超长按 snprintf 截断（与原实现一致）。
 * 扩展点：flags 随 RENDER_VIA_* 扩展。
 */
typedef struct {
    const char *prefix_fmt;
    const char *suffix;
    unsigned flags;
    const char *fallback;
} RenderFnNameSpec;

/**
 * @brief 按 NodeType 查二元模板表并渲染（表驱动 render_binary_via）
 *
 * 语义契约：以 node->type 为下标查 table（表按下标 == NodeType 布局），命中时等价于
 * render_binary_via(node, table[type].tpl, table[type].flags, buffer, size, options, dispatch)；
 * 未命中返回 -1 且不写入 buffer。
 * 前置条件：table 非 NULL；count 为 table 元素数；dispatch 非 NULL。
 * 失败/截断语义：未命中返回 -1；命中后的失败/截断语义与 render_binary_via 一致。
 * 边界行为：node->type 为负或越界，或对应槽位 tpl == NULL 时返回 -1。
 * 扩展点：flags 透传 RENDER_VIA_*。
 */
static inline int render_binary_spec(const FormulaNode *node, const RenderBinarySpec *table, size_t count,
                                     char *buffer, size_t size, const RenderOptions *options, RenderNodeFunc dispatch)
{
    if (!lv_index_in_range(node->type, (int) count) || !table[node->type].tpl)
        return -1;
    return render_binary_via(node, table[node->type].tpl, table[node->type].flags, buffer, size, options, dispatch);
}

/**
 * @brief 按 NodeType 查一元模板表并渲染（表驱动 render_unary_via）
 *
 * 语义契约：以 node->type 为下标查 table（表按下标 == NodeType 布局），命中时等价于
 * render_unary_via(node, table[type].prefix, table[type].suffix, table[type].flags, buffer, size, options,
 * dispatch)；未命中返回 -1 且不写入 buffer。
 * 前置条件：table 非 NULL；count 为 table 元素数；dispatch 非 NULL。
 * 失败/截断语义：未命中返回 -1；命中后的失败/截断语义与 render_unary_via 一致。
 * 边界行为：node->type 为负或越界，或对应槽位 prefix == NULL 时返回 -1。
 * 扩展点：flags 透传 RENDER_VIA_*。
 */
static inline int render_unary_spec(const FormulaNode *node, const RenderUnarySpec *table, size_t count,
                                    char *buffer, size_t size, const RenderOptions *options, RenderNodeFunc dispatch)
{
    if (!lv_index_in_range(node->type, (int) count) || !table[node->type].prefix)
        return -1;
    return render_unary_via(node, table[node->type].prefix, table[node->type].suffix, table[node->type].flags,
                            buffer, size, options, dispatch);
}

/**
 * @brief 按函数名模板渲染 sin/cos/tan 一元算子（表驱动，公式化前缀）
 *
 * 语义契约：经 formula_unary_fn_name 取得函数名；名称为 NULL 时输出 spec->fallback 并返回其长度；
 * 否则以 prefix_fmt 构造前缀（snprintf 到 lv_FORMULA_BUF_SMALL），再等价于
 * render_unary_via(node, prefix, spec->suffix, spec->flags, buffer, size, options, dispatch)。
 * 前置条件：spec 非 NULL；dispatch 非 NULL。
 * 失败/截断语义：与 render_unary_via 一致。
 * 边界行为：函数名缓冲固定 lv_FORMULA_BUF_SMALL，超长截断（snprintf 语义）。
 * 扩展点：flags 透传 RENDER_VIA_*。
 */
static inline int render_fn_name_spec(const FormulaNode *node, const RenderFnNameSpec *spec, char *buffer,
                                      size_t size, const RenderOptions *options, RenderNodeFunc dispatch)
{
    const char *name = formula_unary_fn_name(node);
    if (!name)
        return snprintf(buffer, size, "%s", spec->fallback);
    char prefix[lv_FORMULA_BUF_SMALL];
    snprintf(prefix, sizeof(prefix), spec->prefix_fmt, name);
    return render_unary_via(node, prefix, spec->suffix, spec->flags, buffer, size, options, dispatch);
}

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
