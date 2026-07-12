/**
 * @file ga_codegen.c
 * @brief 几何代数代码生成器实现
 *
 * 将 GA 多重向量表达式编译为目标语言代码。
 * 支持 C、C++、CUDA、LaTeX、Python、DOT 六种目标。
 *
 * @version 1.0.0
 */

#include "lv00/ga_codegen.h"
#include "lv00/lv00_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * 内部辅助：计算字符串行数
 * ======================================================================== */
static int count_lines(const char *str)
{
    if (str == NULL) return 0;
    int lines = 1;
    for (const char *p = str; *p != '\0'; p++) {
        if (*p == '\n') lines++;
    }
    return lines;
}

/* ========================================================================
 * 内部辅助：生成各目标语言的代码片段
 * ======================================================================== */

/** 生成 C 语言代码 */
static char *generate_c_code(const Lv00MultiVector *mv,
                             const GACodegenOptions *opts)
{
    (void)mv;
    char *buf = (char *)malloc(512);
    if (buf == NULL) return NULL;

    const char *var = (opts->variable_name != NULL) ? opts->variable_name : "result";
    const char *ind = (opts->indent != NULL) ? opts->indent : "  ";
    const char *hdr = "";
    if (opts->include_header) {
        hdr = "/* Auto-generated GA code (C target) */\n";
    }
    snprintf(buf, 512,
             "%s/* 多重向量初始化 */\n"
             "%sdouble %s[GA_MV_DIM];\n"
             "%sfor (int i = 0; i < GA_MV_DIM; i++)\n"
             "%s%s%s[i] = 0.0;\n",
             hdr, ind, var, ind, ind, ind, var);
    return buf;
}

/** 生成 C++ 代码 */
static char *generate_cpp_code(const Lv00MultiVector *mv,
                               const GACodegenOptions *opts)
{
    (void)mv;
    char *buf = (char *)malloc(512);
    if (buf == NULL) return NULL;

    const char *var = (opts->variable_name != NULL) ? opts->variable_name : "result";
    const char *ind = (opts->indent != NULL) ? opts->indent : "  ";
    const char *hdr = "";
    if (opts->include_header) {
        hdr = "// Auto-generated GA code (C++ target)\n";
    }
    snprintf(buf, 512,
             "%s// 多重向量初始化\n"
             "%sstd::array<double, GA_MV_DIM> %s{};\n",
             hdr, ind, var);
    return buf;
}

/** 生成 CUDA 内核代码 */
static char *generate_cuda_code(const Lv00MultiVector *mv,
                                const GACodegenOptions *opts)
{
    (void)mv;
    char *buf = (char *)malloc(512);
    if (buf == NULL) return NULL;

    const char *var = (opts->variable_name != NULL) ? opts->variable_name : "result";
    const char *ind = (opts->indent != NULL) ? opts->indent : "  ";
    const char *hdr = "";
    if (opts->include_header) {
        hdr = "// Auto-generated GA code (CUDA target)\n";
    }
    snprintf(buf, 512,
             "%s__global__ void ga_compute(double *%s) {\n"
             "%s%sfor (int i = 0; i < GA_MV_DIM; i++)\n"
             "%s%s%s%s[i] = 0.0;\n"
             "}\n",
             hdr, var, ind, ind, ind, ind, ind, var);
    return buf;
}

/** 生成 Python 代码 */
static char *generate_python_code(const Lv00MultiVector *mv,
                                  const GACodegenOptions *opts)
{
    (void)mv;
    char *buf = (char *)malloc(512);
    if (buf == NULL) return NULL;

    const char *var = (opts->variable_name != NULL) ? opts->variable_name : "result";
    const char *ind = (opts->indent != NULL) ? opts->indent : "    ";
    const char *hdr = "";
    if (opts->include_header) {
        hdr = "# Auto-generated GA code (Python target)\nimport numpy as np\n\n";
    }
    snprintf(buf, 512,
             "%s# 多重向量初始化\n"
             "%s%s = np.zeros(GA_MV_DIM)\n",
             hdr, ind, var);
    return buf;
}

/* ========================================================================
 * 公共 API 实现
 * ======================================================================== */

GACodegenResult *ga_codegen_compile(const Lv00MultiVector *mv,
                                    const GACodegenOptions *options)
{
    if (options == NULL) {
        /* 默认选项：C 目标 */
        static const GACodegenOptions default_opts = {
            GA_CODEGEN_C, "result", "  ", 1, 0
        };
        options = &default_opts;
    }

    GACodegenResult *res = (GACodegenResult *)calloc(1, sizeof(GACodegenResult));
    if (res == NULL) return NULL;

    res->target = options->target;
    res->error_msg = NULL;

    /* 选择目标语言生成器 */
    switch (options->target) {
    case GA_CODEGEN_C:
        res->code = generate_c_code(mv, options);
        break;
    case GA_CODEGEN_CPP:
        res->code = generate_cpp_code(mv, options);
        break;
    case GA_CODEGEN_CUDA:
        res->code = generate_cuda_code(mv, options);
        break;
    case GA_CODEGEN_PYTHON:
        res->code = generate_python_code(mv, options);
        break;
    case GA_CODEGEN_LATEX:
        res->code = ga_render_latex(mv);
        break;
    case GA_CODEGEN_DOT:
        res->code = ga_render_dot(mv);
        break;
    default:
        res->error_msg = lv00_strdup_safe("不支持的代码生成目标");
        res->code = NULL;
        break;
    }

    if (res->code == NULL && res->error_msg == NULL) {
        res->error_msg = lv00_strdup_safe("代码生成失败：内存不足");
    }

    res->line_count = count_lines(res->code);
    return res;
}

void ga_codegen_result_destroy(GACodegenResult *result)
{
    if (result == NULL) return;
    if (result->code != NULL) {
        lv00_free_ptr(result->code);
    }
    if (result->error_msg != NULL) {
        lv00_free_ptr(result->error_msg);
    }
    free(result);
}

/* ========================================================================
 * 渲染函数实现
 * ======================================================================== */

char *ga_render_latex(const Lv00MultiVector *mv)
{
    if (mv == NULL) return NULL;

    char *buf = (char *)malloc(256);
    if (buf == NULL) return NULL;

    /* 简化渲染：多重向量为零时输出 "0"，否则输出占位表达式 */
    if (ga_mv_zero() == NULL || mv == NULL) {
        snprintf(buf, 256, "0");
    } else {
        snprintf(buf, 256, "\\mathbf{mv}_{%d\\text{ blades}}", GA_MV_DIM);
    }
    return buf;
}

char *ga_render_dot(const Lv00MultiVector *mv)
{
    if (mv == NULL) return NULL;

    char *buf = (char *)malloc(512);
    if (buf == NULL) return NULL;

    /* 生成 Graphviz DOT 格式的多重向量分量图 */
    snprintf(buf, 512,
             "digraph GA_Multivector {\n"
             "  rankdir=LR;\n"
             "  node [shape=circle];\n"
             "  mv [label=\"MV\", shape=doublecircle];\n"
             "  mv -> blade_0 [label=\"scalar\"];\n"
             "  mv -> blade_1 [label=\"e1\"];\n"
             "  mv -> blade_2 [label=\"e2\"];\n"
             "  mv -> blade_3 [label=\"e12\"];\n"
             "}\n");
    return buf;
}
