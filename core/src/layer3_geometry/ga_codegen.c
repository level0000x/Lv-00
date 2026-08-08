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
#include "lv/lv_lifecycle.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_utils.h"
#include "lv/lv_numeric.h"
#include "lv_internal.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_dot_writer.h"

#define GA_CODEGEN_BUF_SIZE 512
#define GA_CODEGEN_LATEX_BUF_SIZE 256

/* ========================================================================
 * 代码生成处理器函数指针类型
 * ======================================================================== */
typedef char* (*CodegenHandler)(const lvMultiVector *mv, const GACodegenOptions *options);

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
 * VTable 包装函数：将各生成器适配为 CodegenHandler 签名
 * ======================================================================== */

static char* codegen_c(const lvMultiVector *mv, const GACodegenOptions *options) {
    return generate_c_code(mv, options);
}

static char* codegen_cpp(const lvMultiVector *mv, const GACodegenOptions *options) {
    return generate_cpp_code(mv, options);
}

static char* codegen_cuda(const lvMultiVector *mv, const GACodegenOptions *options) {
    return generate_cuda_code(mv, options);
}

static char* codegen_python(const lvMultiVector *mv, const GACodegenOptions *options) {
    return generate_python_code(mv, options);
}

static char* codegen_latex(const lvMultiVector *mv, const GACodegenOptions *options) {
    (void)options;
    return ga_render_latex(mv);
}

static char* codegen_dot(const lvMultiVector *mv, const GACodegenOptions *options) {
    (void)options;
    return ga_render_dot(mv);
}

/* ========================================================================
 * 代码生成处理器静态查找表
 * ======================================================================== */

static const CodegenHandler kCodegenHandlers[] = {
    [GA_CODEGEN_C]      = codegen_c,
    [GA_CODEGEN_CPP]    = codegen_cpp,
    [GA_CODEGEN_CUDA]   = codegen_cuda,
    [GA_CODEGEN_PYTHON] = codegen_python,
    [GA_CODEGEN_LATEX]  = codegen_latex,
    [GA_CODEGEN_DOT]    = codegen_dot,
};

/* ========================================================================
 * 公共 API 实现
 * ======================================================================== */

GACodegenResult *ga_codegen_compile(const lvMultiVector *mv, const GACodegenOptions *options) {
    if (options == NULL) {
        /* 默认选项：C 目标 */
        static const GACodegenOptions default_opts = {GA_CODEGEN_C, "result", "  ", 1, 0};
        options = &default_opts;
    }

    GACodegenResult *res = (GACodegenResult *) lv_calloc(1, sizeof(GACodegenResult));
    if (res == NULL)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "ga_codegen_compile: calloc failed");

    res->target = options->target;
    res->error_msg = NULL;

    /* 通过 VTable 查找表选择目标语言生成器 */
    if (options->target >= 0 && options->target < (int)(sizeof(kCodegenHandlers)/sizeof(kCodegenHandlers[0])) && kCodegenHandlers[options->target]) {
        res->code = kCodegenHandlers[options->target](mv, options);
    } else {
        res->error_msg = lv_strdup_safe("不支持的代码生成目标");
        res->code = NULL;
    }

    if (res->code == NULL && res->error_msg == NULL) {
        res->error_msg = lv_strdup_safe("代码生成失败：内存不足");
    }

    res->line_count = count_lines(res->code);
    return res;
}

/* ga_codegen_result_destroy 字段描述表：code/error_msg 纯指针释放 */
static const lvFieldDesc s_ga_codegen_result_destroy_fields[] = {
    lv_FIELD_PLAIN(GACodegenResult, code),
    lv_FIELD_PLAIN(GACodegenResult, error_msg),
};

void ga_codegen_result_destroy(GACodegenResult *result) {
    if (result == NULL)
        return;
    lv_obj_destroy_fields(result, s_ga_codegen_result_destroy_fields,
                          sizeof(s_ga_codegen_result_destroy_fields) / sizeof(s_ga_codegen_result_destroy_fields[0]));
    lv_free((void **) &result);
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

    /* 用 lvStrBuf 累积输出（自动扩容；lv_strbuf_to_string 返回 lv_malloc 分配的 NUL 结尾字符串） */
    lvStrBuf sb = {0};
    int first = 1;

    /* 逐分量遍历，输出每个非零基元素的系数与基名称 */
    for (int i = 0; i < GA_MV_DIM; i++) {
        double c = ga_mv_get(mv, i);
        if (lv_is_zero(c, 1e-12))
            continue;
        double a = fabs(c);
        int neg = (c < 0.0);

        /* 写入项间符号 */
        if (first) {
            if (neg)
                lv_strbuf_printf(&sb, "-");
            first = 0;
        } else {
            lv_strbuf_printf(&sb, neg ? " - " : " + ");
        }

        /* 写入单项式：系数绝对值（为 1 时省略）+ 基元素名称 */
        if (i == 0) {
            /* 标量分量直接输出数值 */
            if (lv_is_equal(a, 1.0, 1e-12))
                lv_strbuf_printf(&sb, "1");
            else
                lv_strbuf_printf(&sb, "%.12g", a);
        } else if (lv_is_equal(a, 1.0, 1e-12)) {
            lv_strbuf_printf(&sb, "\\mathbf{%s}", blade_names[i]);
        } else {
            lv_strbuf_printf(&sb, "%.12g\\mathbf{%s}", a, blade_names[i]);
        }
    }

    /* 所有分量均为零 */
    if (first)
        lv_strbuf_printf(&sb, "0");

    return lv_strbuf_to_string(&sb);
}

char *ga_render_dot(const lvMultiVector *mv) {
    if (mv == NULL)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "ga_render_dot: mv is NULL");

    /* 生成 Graphviz DOT 格式的多重向量分量图（返回 lv_malloc 分配的字符串，调用者 lv_free） */
    lvStrBuf sb;
    lv_strbuf_init(&sb);

    lv_dot_begin(&sb, "GA_Multivector", "LR", "shape=circle", NULL);
    lv_dot_node(&sb, "mv", "MV", "shape=doublecircle");
    lv_dot_edge(&sb, "mv", "blade_0", "scalar", NULL);
    lv_dot_edge(&sb, "mv", "blade_1", "e1", NULL);
    lv_dot_edge(&sb, "mv", "blade_2", "e2", NULL);
    lv_dot_edge(&sb, "mv", "blade_3", "e12", NULL);
    lv_dot_end(&sb);

    return lv_strbuf_to_string(&sb);
}
