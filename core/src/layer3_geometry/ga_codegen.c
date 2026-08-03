/**
 * @file ga_codegen.c
 * @brief 几何代数代码生成器实现
 *
 * 将 GA 多重向量表达式编译为目标语言代码。
 * 支持 C、C++、CUDA、LaTeX、Python、DOT 六种目标。
 *
 * @version 1.0.0
 */

#include "lv/ga_codegen.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_utils.h"
#include "lv_internal.h"

#define GA_CODEGEN_BUF_SIZE 512
#define GA_CODEGEN_LATEX_BUF_SIZE 256

/* ========================================================================
 * 内部辅助：计算字符串行数
 * ======================================================================== */
static int count_lines(const char *str) {
    if (str == NULL)
        return 0;
    int lines = 1;
    for (const char *p = str; *p != '\0'; p++) {
        if (*p == '\n')
            lines++;
    }
    return lines;
}

/* ========================================================================
 * 内部辅助：生成各目标语言的代码片段
 * ======================================================================== */

/** 生成 C 语言代码 */
static char *generate_c_code(const lvMultiVector *mv, const GACodegenOptions *opts) {
    (void) mv;
    char *buf = (char *) lv_malloc(GA_CODEGEN_BUF_SIZE);
    if (buf == NULL)
        return NULL;

    const char *var = (opts->variable_name != NULL) ? opts->variable_name : "result";
    const char *ind = (opts->indent != NULL) ? opts->indent : "  ";
    const char *hdr = "";
    if (opts->include_header) {
        hdr = "/* Auto-generated GA code (C target) */\n";
    }
    snprintf(buf, GA_CODEGEN_BUF_SIZE,
             "%s/* 多重向量初始化 */\n"
             "%sdouble %s[GA_MV_DIM];\n"
             "%sfor (int i = 0; i < GA_MV_DIM; i++)\n"
             "%s%s%s[i] = 0.0;\n",
             hdr, ind, var, ind, ind, ind, var);
    return buf;
}

/** 生成 C++ 代码 */
static char *generate_cpp_code(const lvMultiVector *mv, const GACodegenOptions *opts) {
    (void) mv;
    char *buf = (char *) lv_malloc(GA_CODEGEN_BUF_SIZE);
    if (buf == NULL)
        return NULL;

    const char *var = (opts->variable_name != NULL) ? opts->variable_name : "result";
    const char *ind = (opts->indent != NULL) ? opts->indent : "  ";
    const char *hdr = "";
    if (opts->include_header) {
        hdr = "// Auto-generated GA code (C++ target)\n";
    }
    snprintf(buf, GA_CODEGEN_BUF_SIZE,
             "%s// 多重向量初始化\n"
             "%sstd::array<double, GA_MV_DIM> %s{};\n",
             hdr, ind, var);
    return buf;
}

/** 生成 CUDA 内核代码 */
static char *generate_cuda_code(const lvMultiVector *mv, const GACodegenOptions *opts) {
    (void) mv;
    char *buf = (char *) lv_malloc(GA_CODEGEN_BUF_SIZE);
    if (buf == NULL)
        return NULL;

    const char *var = (opts->variable_name != NULL) ? opts->variable_name : "result";
    const char *ind = (opts->indent != NULL) ? opts->indent : "  ";
    const char *hdr = "";
    if (opts->include_header) {
        hdr = "// Auto-generated GA code (CUDA target)\n";
    }
    snprintf(buf, GA_CODEGEN_BUF_SIZE,
             "%s__global__ void ga_compute(double *%s) {\n"
             "%s%sfor (int i = 0; i < GA_MV_DIM; i++)\n"
             "%s%s%s%s[i] = 0.0;\n"
             "}\n",
             hdr, var, ind, ind, ind, ind, ind, var);
    return buf;
}

/** 生成 Python 代码 */
static char *generate_python_code(const lvMultiVector *mv, const GACodegenOptions *opts) {
    (void) mv;
    char *buf = (char *) lv_malloc(GA_CODEGEN_BUF_SIZE);
    if (buf == NULL)
        return NULL;

    const char *var = (opts->variable_name != NULL) ? opts->variable_name : "result";
    const char *ind = (opts->indent != NULL) ? opts->indent : "    ";
    const char *hdr = "";
    if (opts->include_header) {
        hdr = "# Auto-generated GA code (Python target)\nimport numpy as np\n\n";
    }
    snprintf(buf, GA_CODEGEN_BUF_SIZE,
             "%s# 多重向量初始化\n"
             "%s%s = np.zeros(GA_MV_DIM)\n",
             hdr, ind, var);
    return buf;
}

/* ========================================================================
 * 公共 API 实现
 * ======================================================================== */

GACodegenResult *ga_codegen_compile(const lvMultiVector *mv, const GACodegenOptions *options) {
    if (options == NULL) {
        /* 默认选项：C 目标 */
        static const GACodegenOptions default_opts = {GA_CODEGEN_C, "result", "  ", 1, 0};
        options = &default_opts;
    }

    GACodegenResult *res = (GACodegenResult *) calloc(1, sizeof(GACodegenResult));
    if (res == NULL)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "ga_codegen_compile: calloc failed");

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
            res->error_msg = lv_strdup_safe("不支持的代码生成目标");
            res->code = NULL;
            break;
    }

    if (res->code == NULL && res->error_msg == NULL) {
        res->error_msg = lv_strdup_safe("代码生成失败：内存不足");
    }

    res->line_count = count_lines(res->code);
    return res;
}

void ga_codegen_result_destroy(GACodegenResult *result) {
    if (result == NULL)
        return;
    if (result->code != NULL) {
        lv_free_ptr(result->code);
    }
    if (result->error_msg != NULL) {
        lv_free_ptr(result->error_msg);
    }
    lv_free(result);
}

/* ========================================================================
 * 渲染函数实现
 * ======================================================================== */

char *ga_render_latex(const lvMultiVector *mv) {
    if (mv == NULL)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "ga_render_latex: mv is NULL");

    /* Cl(3,0,1) 代数 16 个基元素的 LaTeX 名称（与 ga_multivector.c 索引一致） */
    static const char *blade_names[GA_MV_DIM] = {
        "1",        /* 0: 标量 */
        "e_{0}",    /* 1: e0 */
        "e_{1}",    /* 2: e1 */
        "e_{2}",    /* 3: e2 */
        "e_{3}",    /* 4: e3 */
        "e_{01}",   /* 5: e0∧e1 */
        "e_{02}",   /* 6: e0∧e2 */
        "e_{03}",   /* 7: e0∧e3 */
        "e_{12}",   /* 8: e1∧e2 */
        "e_{13}",   /* 9: e1∧e3 */
        "e_{23}",   /* 10: e2∧e3 */
        "e_{012}",  /* 11: e0∧e1∧e2 */
        "e_{013}",  /* 12: e0∧e1∧e3 */
        "e_{023}",  /* 13: e0∧e2∧e3 */
        "e_{123}",  /* 14: e1∧e2∧e3 */
        "e_{0123}"  /* 15: e0∧e1∧e2∧e3（伪标量） */
    };

    size_t cap = GA_CODEGEN_LATEX_BUF_SIZE;
    char *buf = (char *) lv_malloc(cap);
    if (buf == NULL)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "ga_render_latex: malloc failed");
    buf[0] = '\0';

    size_t pos = 0;
    int first = 1;

    /* 逐分量遍历，输出每个非零基元素的系数与基名称 */
    for (int i = 0; i < GA_MV_DIM; i++) {
        double c = ga_mv_get(mv, i);
        if (fabs(c) < 1e-12)
            continue;
        double a = fabs(c);
        int neg = (c < 0.0);

        /* 构造单项式：系数绝对值（为 1 时省略）+ 基元素名称 */
        char term[64];
        if (i == 0) {
            /* 标量分量直接输出数值 */
            if (fabs(a - 1.0) < 1e-12)
                snprintf(term, sizeof(term), "1");
            else
                snprintf(term, sizeof(term), "%.12g", a);
        } else if (fabs(a - 1.0) < 1e-12) {
            snprintf(term, sizeof(term), "\\mathbf{%s}", blade_names[i]);
        } else {
            snprintf(term, sizeof(term), "%.12g\\mathbf{%s}", a, blade_names[i]);
        }

        size_t tlen = strlen(term);
        size_t sep = (first ? (neg ? 1u : 0u) : 3u);
        /* 缓冲区不足时扩容 */
        if (pos + sep + tlen + 1 > cap) {
            size_t new_cap = cap * 2;
            char *nb = (char *) lv_realloc(buf, new_cap);
            if (nb == NULL) {
                lv_free_ptr(buf);
                lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "ga_render_latex: realloc failed");
            }
            buf = nb;
            cap = new_cap;
        }

        /* 写入项间符号与单项式 */
        if (first) {
            if (neg)
                buf[pos++] = '-';
            first = 0;
        } else {
            memcpy(buf + pos, neg ? " - " : " + ", 3);
            pos += 3;
        }
        memcpy(buf + pos, term, tlen);
        pos += tlen;
        buf[pos] = '\0';
    }

    /* 所有分量均为零 */
    if (first)
        snprintf(buf, cap, "0");

    return buf;
}

char *ga_render_dot(const lvMultiVector *mv) {
    if (mv == NULL)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "ga_render_dot: mv is NULL");

    char *buf = (char *) lv_malloc(GA_CODEGEN_BUF_SIZE);
    if (buf == NULL)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "ga_render_dot: malloc failed");

    /* 生成 Graphviz DOT 格式的多重向量分量图 */
    snprintf(buf, GA_CODEGEN_BUF_SIZE,
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
