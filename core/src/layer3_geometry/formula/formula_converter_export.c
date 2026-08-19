/**
 * @file formula_converter_export.c
 * @brief 公式转换器实现 —— 渲染辅助与图→公式导出、结果销毁
 *
 * @details 由 formula_converter.c 按功能边界拆分而来，
 *          属于公式 AST 与约束图双向转换的一部分。
 *
 * @author Lv-00 Project
 * @version 3.0.1
 */

#include "lv/lv_platform.h"
#include "lv/lv_lifecycle.h"
#include "lv/formula_converter.h"
#include "formula_converter_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/formula_renderer.h"
#include "lv/lv_internal.h"
#include "lv/lv_xmacro.h" /* LV_DISPATCH_VOID */
#include "lv/lv_utils.h"
#include "lv/lv_str_utils.h"
#include "lv/stream.h"
#include "lv/stream.h"

/* ============================================================
 * 图 → 公式 主转换函数
 * ============================================================ */

/**
 * @brief 增量输出目标：{out, len, size} 游标式追加
 *
 * 收敛原 7 个 render_geom_* 回调/约束循环中重复的 5 步追加样板
 * （snprintf → if n>0 → memcpy → len+=n → '\0'，全文件 21 处）。
 */
typedef struct {
    char *out;   /* 输出缓冲区 */
    size_t len;  /* 当前已写入长度 */
    size_t size; /* 缓冲区容量 */
} RenderTarget;

/**
 * @brief 将格式化结果（n 字节）追加到输出目标
 *
 * 语义契约：仅当 n > 0 且 len + n < size 时，将 text 的 n 字节拷贝到 out+len，
 * 推进 len 并在新位置写 NUL；否则保持目标不变。
 * 前置条件：t 与 t->out 非 NULL；t->len <= t->size；text 指向至少 n 字节可读数据。
 * 失败/截断语义：容量不足时静默跳过整次追加（len/size 均不变，不拷入部分内容），
 * 与原有行为逐字一致。
 * 边界行为：n == 0 跳过；len + n == size 跳过（严格 <，与原有判断一致）。
 * 扩展点：无。
 */
static void render_append(RenderTarget *t, const char *text, size_t n) {
    if (n > 0 && t->len + n < t->size) {
        memcpy(t->out + t->len, text, n);
        t->len += n;
        t->out[t->len] = '\0';
    }
}

typedef void (*GraphNodeRenderFunc)(const GeomNode *node, const char *name, RenderTarget *latex, RenderTarget *python,
                                    RenderTarget *dsl);

static void render_geom_point(const GeomNode *node, const char *name, RenderTarget *latex, RenderTarget *python,
                              RenderTarget *dsl) {
    char buf[FORMULA_LATEX_BUF_SIZE];
    /* 获取坐标 */
    double x = 0, y = 0;
    if (node->symbolic_coords && node->coord_count >= 2) {
        x = symbolic_coord_to_double(node->symbolic_coords[0]);
        y = symbolic_coord_to_double(node->symbolic_coords[1]);
    }

    /* LaTeX */
    int n = lv_snprintf(buf, sizeof(buf), "%s = \\left(%.2f, %.2f\\right)\\\\\n", name, x, y);
    render_append(latex, buf, (size_t) n);

    /* Python */
    n = lv_snprintf(buf, sizeof(buf), "%s = Point(%.2f, %.2f)\n", name, x, y);
    render_append(python, buf, (size_t) n);

    /* DSL */
    n = lv_snprintf(buf, sizeof(buf), "point %s(%.2f, %.2f); ", name, x, y);
    render_append(dsl, buf, (size_t) n);
}

static void render_geom_line_segment(const GeomNode *node, const char *name, RenderTarget *latex, RenderTarget *python,
                                     RenderTarget *dsl) {
    char buf[FORMULA_LATEX_BUF_SIZE];
    /* LaTeX */
    int n = lv_snprintf(buf, sizeof(buf), "\\overline{%s}\\\\\n", name);
    render_append(latex, buf, (size_t) n);

    /* Python */
    n = lv_snprintf(buf, sizeof(buf), "%s = Segment()\n", name);
    render_append(python, buf, (size_t) n);

    /* DSL */
    n = lv_snprintf(buf, sizeof(buf), "segment %s(); ", name);
    render_append(dsl, buf, (size_t) n);
}

static void render_geom_region(const GeomNode *node, const char *name, RenderTarget *latex, RenderTarget *python,
                               RenderTarget *dsl) {
    char buf[FORMULA_LATEX_BUF_SIZE];
    /* 获取边界线段信息 */
    int seg_count = node->data.region.segment_count;
    char seg_list[FORMULA_SEG_LIST_SIZE] = "";
    size_t seg_list_len = 0;
    for (int j = 0; j < seg_count && j < 10; j++) {
        char seg_name[FORMULA_SEG_NAME_SIZE];
        if (node->data.region.boundary_segments && node->data.region.boundary_segments[j]) {
            formula_node_to_name(node->data.region.boundary_segments[j], seg_name, sizeof(seg_name));
        } else {
            lv_snprintf(seg_name, sizeof(seg_name), "S?");
        }
        /* 统一走 lv_str_append_sep（游标式追加，首项自动省略分隔符） */
        lv_str_append_sep(seg_list, sizeof(seg_list), &seg_list_len, ", ", seg_name);
    }
    seg_list[seg_list_len] = '\0';

    /* LaTeX */
    int n = lv_snprintf(buf, sizeof(buf), "\\text{region } %s(\\{%s\\})\\\\\n", name, seg_list);
    render_append(latex, buf, (size_t) n);

    /* Python */
    n = lv_snprintf(buf, sizeof(buf), "%s = Region([%s])\n", name, seg_list);
    render_append(python, buf, (size_t) n);

    /* DSL */
    n = lv_snprintf(buf, sizeof(buf), "region %s(%s); ", name, seg_list);
    render_append(dsl, buf, (size_t) n);
}

static void render_geom_circle(const GeomNode *node, const char *name, RenderTarget *latex, RenderTarget *python,
                               RenderTarget *dsl) {
    char buf[FORMULA_LATEX_BUF_SIZE];
    /* LaTeX */
    int n = lv_snprintf(buf, sizeof(buf), "\\text{circle } %s\\\\\n", name);
    render_append(latex, buf, (size_t) n);

    /* Python */
    n = lv_snprintf(buf, sizeof(buf), "%s = Circle()\n", name);
    render_append(python, buf, (size_t) n);

    /* DSL */
    n = lv_snprintf(buf, sizeof(buf), "circle %s(); ", name);
    render_append(dsl, buf, (size_t) n);
}

static void render_geom_port(const GeomNode *node, const char *name, RenderTarget *latex, RenderTarget *python,
                             RenderTarget *dsl) {
    char buf[FORMULA_LATEX_BUF_SIZE];
    const char *port_type_str = "unknown";
    if (node->data.port) {
        port_type_str = (node->data.port->type == PORT_INPUT) ? "input" : "output";
    }

    /* LaTeX */
    int n = lv_snprintf(buf, sizeof(buf), "\\text{port } %s(\\text{%s})\\\\\n", name, port_type_str);
    render_append(latex, buf, (size_t) n);

    /* Python */
    n = lv_snprintf(buf, sizeof(buf), "%s = Port('%s')\n", name, port_type_str);
    render_append(python, buf, (size_t) n);

    /* DSL */
    n = lv_snprintf(buf, sizeof(buf), "port %s(%s); ", name, port_type_str);
    render_append(dsl, buf, (size_t) n);
}

static void render_geom_function_block(const GeomNode *node, const char *name, RenderTarget *latex, RenderTarget *python,
                                       RenderTarget *dsl) {
    char buf[FORMULA_LATEX_BUF_SIZE];
    /* 获取函数块信息 */
    int in_count = node->data.func_block.input_count;
    int out_count = node->data.func_block.output_count;

    /* LaTeX */
    int n = lv_snprintf(buf, sizeof(buf), "\\text{func\\_block } %s(\\text{in: }%d, \\text{out: }%d)\\\\\n", name, in_count,
                     out_count);
    render_append(latex, buf, (size_t) n);

    /* Python */
    n = lv_snprintf(buf, sizeof(buf), "%s = FuncBlock(inputs=%d, outputs=%d)\n", name, in_count, out_count);
    render_append(python, buf, (size_t) n);

    /* DSL */
    n = lv_snprintf(buf, sizeof(buf), "func_block %s(in=%d, out=%d); ", name, in_count, out_count);
    render_append(dsl, buf, (size_t) n);
}


/**
 * @brief 将约束图转换为公式 AST（主入口函数）
 *
 * 遍历约束图中的节点和约束，生成对应的公式 AST。
 *
 * @param graph 约束图指针
 * @return 转换结果结构体指针，失败返回 NULL
 */
GraphToFormulaResult *graph_to_formula(const ConstraintGraph *graph) {
    GraphToFormulaResult *result =
        (GraphToFormulaResult *) lv_calloc(1, sizeof(GraphToFormulaResult)); /* 统一内存分配器 */
    if (!result) {
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "failed to allocate result");
    }

    if (!graph) {
        result->success = false;
        lv_snprintf(result->error_message, sizeof(result->error_message), "NULL graph");
        return result;
    }

    /* 计算所需缓冲区大小 */
    size_t latex_size = FORMULA_EXPORT_BUF_SIZE;
    size_t python_size = FORMULA_EXPORT_BUF_SIZE;
    size_t dsl_size = FORMULA_EXPORT_BUF_SIZE;

    result->latex_output = (char *) lv_malloc(latex_size);   /* 统一内存分配器 */
    result->python_output = (char *) lv_malloc(python_size); /* 统一内存分配器 */
    result->dsl_output = (char *) lv_malloc(dsl_size);       /* 统一内存分配器 */

    if (!result->latex_output || !result->python_output || !result->dsl_output) {
        lv_ERROR_SET(lv_ERROR_ALLOCATION_FAILED, "failed to allocate output buffers");
        graph_to_formula_result_destroy(result);
        return NULL;
    }

    result->latex_output[0] = '\0';
    result->python_output[0] = '\0';
    result->dsl_output[0] = '\0';

    /* 修复：使用 RenderTarget 游标跟踪当前写入位置，替代反复调用 strlen 的 strncat 模式，
     * 避免每次拼接时的 O(n) strlen 扫描和潜在的缓冲区溢出风险 */
    RenderTarget latex_tgt = {result->latex_output, 0, latex_size};
    RenderTarget python_tgt = {result->python_output, 0, python_size};
    RenderTarget dsl_tgt = {result->dsl_output, 0, dsl_size};

    char buf[FORMULA_LATEX_BUF_SIZE];

    static const GraphNodeRenderFunc s_funcs[] = {
        [GEOM_POINT] = render_geom_point,
        [GEOM_LINE_SEGMENT] = render_geom_line_segment,
        [GEOM_REGION] = render_geom_region,
        [GEOM_CIRCLE] = render_geom_circle,
        [GEOM_PORT] = render_geom_port,
        [GEOM_FUNCTION_BLOCK] = render_geom_function_block,
    };

    /* 遍历所有节点 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node)
            continue;

        char name[MAX_NAME_LENGTH];
        formula_node_to_name(node, name, sizeof(name));

        LV_DISPATCH_VOID(s_funcs, node->type, node, name, &latex_tgt, &python_tgt, &dsl_tgt);
    }

    /* 约束名称/LaTeX 查找表 */
    static const struct {
        const char *name;
        const char *latex;
    } s_constraint_info[] = {
        [INCIDENCE]    = {"incidence",    "\\text{incidence}"},
        [BETWEENNESS]  = {"betweenness",  "\\text{betweenness}"},
        [INTERSECTION] = {"intersection", "\\cap"},
        [CONTAINMENT]  = {"containment",  "\\subset"},
        [CONNECTION]   = {"connection",   "\\leftrightarrow"},
        [ANGLE]        = {"angle",        "\\angle"},
        [PARALLEL]     = {"parallel",     "\\parallel"},
    };
#define CONSTRAINT_INFO_COUNT (sizeof(s_constraint_info) / sizeof(s_constraint_info[0]))

    /* 遍历所有约束 */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *constraint = graph->constraints[i];
        if (!constraint)
            continue;

        const char *constraint_name = NULL;
        const char *constraint_latex = NULL;

        if ((unsigned)constraint->type < CONSTRAINT_INFO_COUNT) {
            constraint_name = s_constraint_info[constraint->type].name;
            constraint_latex = s_constraint_info[constraint->type].latex;
        } else {
            constraint_name = "unknown";
            constraint_latex = "\\text{unknown}";
        }

        /* LaTeX */
        int n = lv_snprintf(buf, sizeof(buf), "\\text{Constraint: } %s\\\\\n", constraint_latex);
        render_append(&latex_tgt, buf, (size_t) n);

        /* Python */
        n = lv_snprintf(buf, sizeof(buf), "# Constraint: %s\n", constraint_name);
        render_append(&python_tgt, buf, (size_t) n);

        /* DSL */
        n = lv_snprintf(buf, sizeof(buf), "# constraint %s; ", constraint_name);
        render_append(&dsl_tgt, buf, (size_t) n);
    }

    result->success = true;
    return result;
}

/* ============================================================
 * 结果销毁函数
 * ============================================================ */

/**
 * @brief 销毁公式到图的转换结果
 *
 * @param result 转换结果指针（可为 NULL）
 */
/* formula_to_graph_result_destroy 字段描述表：2 个纯指针字段 */
static const lvFieldDesc s_formula_to_graph_destroy_fields[] = {
    lv_FIELD_PLAIN(FormulaToGraphResult, created_node_ids),
    lv_FIELD_PLAIN(FormulaToGraphResult, created_constraint_ids),
};

void formula_to_graph_result_destroy(FormulaToGraphResult *result) {
    if (!result)
        return;
    lv_obj_destroy_fields(result, s_formula_to_graph_destroy_fields,
                          sizeof(s_formula_to_graph_destroy_fields) / sizeof(s_formula_to_graph_destroy_fields[0]));
    lv_free((void **) &result);
}

/* graph_to_formula_result_destroy 字段描述表：3 个纯指针字段 */
static const lvFieldDesc s_graph_to_formula_destroy_fields[] = {
    lv_FIELD_PLAIN(GraphToFormulaResult, latex_output),
    lv_FIELD_PLAIN(GraphToFormulaResult, python_output),
    lv_FIELD_PLAIN(GraphToFormulaResult, dsl_output),
};

void graph_to_formula_result_destroy(GraphToFormulaResult *result) {
    if (!result)
        return;
    lv_obj_destroy_fields(result, s_graph_to_formula_destroy_fields,
                          sizeof(s_graph_to_formula_destroy_fields) / sizeof(s_graph_to_formula_destroy_fields[0]));
    lv_free((void **) &result);
}
