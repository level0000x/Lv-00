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
#include "formula_converter.h"
#include "formula_converter_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "formula_renderer.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/lv_str_utils.h"
#include "stream.h"
#include "stream_context_util.h"

/* ============================================================
 * 图 → 公式 主转换函数
 * ============================================================ */

typedef void (*GraphNodeRenderFunc)(const GeomNode *node, const char *name,
                                     char *out_latex, size_t *latex_len, size_t latex_size,
                                     char *out_python, size_t *python_len, size_t python_size,
                                     char *out_dsl, size_t *dsl_len, size_t dsl_size);
static void render_geom_point(const GeomNode *node, const char *name, char *out_latex, size_t *latex_len, size_t latex_size, char *out_python, size_t *python_len, size_t python_size, char *out_dsl, size_t *dsl_len, size_t dsl_size) {
    char latex_buf[FORMULA_LATEX_BUF_SIZE];
    char python_buf[FORMULA_PYTHON_BUF_SIZE];
    char dsl_buf[FORMULA_DSL_BUF_SIZE];
                /* 获取坐标 */
                double x = 0, y = 0;
                if (node->symbolic_coords && node->coord_count >= 2) {
                    x = symbolic_coord_to_double(node->symbolic_coords[0]);
                    y = symbolic_coord_to_double(node->symbolic_coords[1]);
                }

                /* LaTeX */
                int n = snprintf(latex_buf, sizeof(latex_buf), "%s = \\left(%.2f, %.2f\\right)\\\\\n", name, x, y);
                if (n > 0 && (*latex_len) + (size_t) n < latex_size) {
                    memcpy(out_latex + (*latex_len), latex_buf, (size_t) n);
                    (*latex_len) += (size_t) n;
                    out_latex[(*latex_len)] = '\0';
                }

                /* Python */
                n = snprintf(python_buf, sizeof(python_buf), "%s = Point(%.2f, %.2f)\n", name, x, y);
                if (n > 0 && (*python_len) + (size_t) n < python_size) {
                    memcpy(out_python + (*python_len), python_buf, (size_t) n);
                    (*python_len) += (size_t) n;
                    out_python[(*python_len)] = '\0';
                }

                /* DSL */
                n = snprintf(dsl_buf, sizeof(dsl_buf), "point %s(%.2f, %.2f); ", name, x, y);
                if (n > 0 && (*dsl_len) + (size_t) n < dsl_size) {
                    memcpy(out_dsl + (*dsl_len), dsl_buf, (size_t) n);
                    (*dsl_len) += (size_t) n;
                    out_dsl[(*dsl_len)] = '\0';
                }
            }

static void render_geom_line_segment(const GeomNode *node, const char *name, char *out_latex, size_t *latex_len, size_t latex_size, char *out_python, size_t *python_len, size_t python_size, char *out_dsl, size_t *dsl_len, size_t dsl_size) {
    char latex_buf[FORMULA_LATEX_BUF_SIZE];
    char python_buf[FORMULA_PYTHON_BUF_SIZE];
    char dsl_buf[FORMULA_DSL_BUF_SIZE];
                /* LaTeX */
                int n = snprintf(latex_buf, sizeof(latex_buf), "\\overline{%s}\\\\\n", name);
                if (n > 0 && (*latex_len) + (size_t) n < latex_size) {
                    memcpy(out_latex + (*latex_len), latex_buf, (size_t) n);
                    (*latex_len) += (size_t) n;
                    out_latex[(*latex_len)] = '\0';
                }

                /* Python */
                n = snprintf(python_buf, sizeof(python_buf), "%s = Segment()\n", name);
                if (n > 0 && (*python_len) + (size_t) n < python_size) {
                    memcpy(out_python + (*python_len), python_buf, (size_t) n);
                    (*python_len) += (size_t) n;
                    out_python[(*python_len)] = '\0';
                }

                /* DSL */
                n = snprintf(dsl_buf, sizeof(dsl_buf), "segment %s(); ", name);
                if (n > 0 && (*dsl_len) + (size_t) n < dsl_size) {
                    memcpy(out_dsl + (*dsl_len), dsl_buf, (size_t) n);
                    (*dsl_len) += (size_t) n;
                    out_dsl[(*dsl_len)] = '\0';
                }
            }

static void render_geom_region(const GeomNode *node, const char *name, char *out_latex, size_t *latex_len, size_t latex_size, char *out_python, size_t *python_len, size_t python_size, char *out_dsl, size_t *dsl_len, size_t dsl_size) {
    char latex_buf[FORMULA_LATEX_BUF_SIZE];
    char python_buf[FORMULA_PYTHON_BUF_SIZE];
    char dsl_buf[FORMULA_DSL_BUF_SIZE];
                /* 获取边界线段信息 */
                int seg_count = node->data.region.segment_count;
                char seg_list[FORMULA_SEG_LIST_SIZE] = "";
                size_t seg_list_len = 0;
                for (int j = 0; j < seg_count && j < 10; j++) {
                    char seg_name[FORMULA_SEG_NAME_SIZE];
                    if (node->data.region.boundary_segments && node->data.region.boundary_segments[j]) {
                        formula_node_to_name(node->data.region.boundary_segments[j], seg_name, sizeof(seg_name));
                    } else {
                        snprintf(seg_name, sizeof(seg_name), "S?");
                    }
                    /* 统一走 lv_str_append_sep（游标式追加，首项自动省略分隔符） */
                    lv_str_append_sep(seg_list, sizeof(seg_list), &seg_list_len, ", ", seg_name);
                }
                seg_list[seg_list_len] = '\0';

                /* LaTeX */
                int n = snprintf(latex_buf, sizeof(latex_buf), "\\text{region } %s(\\{%s\\})\\\\\n", name, seg_list);
                if (n > 0 && (*latex_len) + (size_t) n < latex_size) {
                    memcpy(out_latex + (*latex_len), latex_buf, (size_t) n);
                    (*latex_len) += (size_t) n;
                    out_latex[(*latex_len)] = '\0';
                }

                /* Python */
                n = snprintf(python_buf, sizeof(python_buf), "%s = Region([%s])\n", name, seg_list);
                if (n > 0 && (*python_len) + (size_t) n < python_size) {
                    memcpy(out_python + (*python_len), python_buf, (size_t) n);
                    (*python_len) += (size_t) n;
                    out_python[(*python_len)] = '\0';
                }

                /* DSL */
                n = snprintf(dsl_buf, sizeof(dsl_buf), "region %s(%s); ", name, seg_list);
                if (n > 0 && (*dsl_len) + (size_t) n < dsl_size) {
                    memcpy(out_dsl + (*dsl_len), dsl_buf, (size_t) n);
                    (*dsl_len) += (size_t) n;
                    out_dsl[(*dsl_len)] = '\0';
                }
            }

static void render_geom_circle(const GeomNode *node, const char *name, char *out_latex, size_t *latex_len, size_t latex_size, char *out_python, size_t *python_len, size_t python_size, char *out_dsl, size_t *dsl_len, size_t dsl_size) {
    char latex_buf[FORMULA_LATEX_BUF_SIZE];
    char python_buf[FORMULA_PYTHON_BUF_SIZE];
    char dsl_buf[FORMULA_DSL_BUF_SIZE];
                /* LaTeX */
                int n = snprintf(latex_buf, sizeof(latex_buf), "\\text{circle } %s\\\\\n", name);
                if (n > 0 && (*latex_len) + (size_t) n < latex_size) {
                    memcpy(out_latex + (*latex_len), latex_buf, (size_t) n);
                    (*latex_len) += (size_t) n;
                    out_latex[(*latex_len)] = '\0';
                }

                /* Python */
                n = snprintf(python_buf, sizeof(python_buf), "%s = Circle()\n", name);
                if (n > 0 && (*python_len) + (size_t) n < python_size) {
                    memcpy(out_python + (*python_len), python_buf, (size_t) n);
                    (*python_len) += (size_t) n;
                    out_python[(*python_len)] = '\0';
                }

                /* DSL */
                n = snprintf(dsl_buf, sizeof(dsl_buf), "circle %s(); ", name);
                if (n > 0 && (*dsl_len) + (size_t) n < dsl_size) {
                    memcpy(out_dsl + (*dsl_len), dsl_buf, (size_t) n);
                    (*dsl_len) += (size_t) n;
                    out_dsl[(*dsl_len)] = '\0';
                }
            }

static void render_geom_port(const GeomNode *node, const char *name, char *out_latex, size_t *latex_len, size_t latex_size, char *out_python, size_t *python_len, size_t python_size, char *out_dsl, size_t *dsl_len, size_t dsl_size) {
    char latex_buf[FORMULA_LATEX_BUF_SIZE];
    char python_buf[FORMULA_PYTHON_BUF_SIZE];
    char dsl_buf[FORMULA_DSL_BUF_SIZE];
                const char *port_type_str = "unknown";
                if (node->data.port) {
                    port_type_str = (node->data.port->type == PORT_INPUT) ? "input" : "output";
                }

                /* LaTeX */
                int n =
                    snprintf(latex_buf, sizeof(latex_buf), "\\text{port } %s(\\text{%s})\\\\\n", name, port_type_str);
                if (n > 0 && (*latex_len) + (size_t) n < latex_size) {
                    memcpy(out_latex + (*latex_len), latex_buf, (size_t) n);
                    (*latex_len) += (size_t) n;
                    out_latex[(*latex_len)] = '\0';
                }

                /* Python */
                n = snprintf(python_buf, sizeof(python_buf), "%s = Port('%s')\n", name, port_type_str);
                if (n > 0 && (*python_len) + (size_t) n < python_size) {
                    memcpy(out_python + (*python_len), python_buf, (size_t) n);
                    (*python_len) += (size_t) n;
                    out_python[(*python_len)] = '\0';
                }

                /* DSL */
                n = snprintf(dsl_buf, sizeof(dsl_buf), "port %s(%s); ", name, port_type_str);
                if (n > 0 && (*dsl_len) + (size_t) n < dsl_size) {
                    memcpy(out_dsl + (*dsl_len), dsl_buf, (size_t) n);
                    (*dsl_len) += (size_t) n;
                    out_dsl[(*dsl_len)] = '\0';
                }
            }

static void render_geom_function_block(const GeomNode *node, const char *name, char *out_latex, size_t *latex_len, size_t latex_size, char *out_python, size_t *python_len, size_t python_size, char *out_dsl, size_t *dsl_len, size_t dsl_size) {
    char latex_buf[FORMULA_LATEX_BUF_SIZE];
    char python_buf[FORMULA_PYTHON_BUF_SIZE];
    char dsl_buf[FORMULA_DSL_BUF_SIZE];
                /* 获取函数块信息 */
                int in_count = node->data.func_block.input_count;
                int out_count = node->data.func_block.output_count;

                /* LaTeX */
                int n = snprintf(latex_buf, sizeof(latex_buf),
                                 "\\text{func\\_block } %s(\\text{in: }%d, \\text{out: }%d)\\\\\n", name, in_count,
                                 out_count);
                if (n > 0 && (*latex_len) + (size_t) n < latex_size) {
                    memcpy(out_latex + (*latex_len), latex_buf, (size_t) n);
                    (*latex_len) += (size_t) n;
                    out_latex[(*latex_len)] = '\0';
                }

                /* Python */
                n = snprintf(python_buf, sizeof(python_buf), "%s = FuncBlock(inputs=%d, outputs=%d)\n", name, in_count,
                             out_count);
                if (n > 0 && (*python_len) + (size_t) n < python_size) {
                    memcpy(out_python + (*python_len), python_buf, (size_t) n);
                    (*python_len) += (size_t) n;
                    out_python[(*python_len)] = '\0';
                }

                /* DSL */
                n = snprintf(dsl_buf, sizeof(dsl_buf), "func_block %s(in=%d, out=%d); ", name, in_count, out_count);
                if (n > 0 && (*dsl_len) + (size_t) n < dsl_size) {
                    memcpy(out_dsl + (*dsl_len), dsl_buf, (size_t) n);
                    (*dsl_len) += (size_t) n;
                    out_dsl[(*dsl_len)] = '\0';
                }
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
        snprintf(result->error_message, sizeof(result->error_message), "NULL graph");
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

    /* 修复：使用偏移量变量跟踪当前写入位置，替代反复调用 strlen 的 strncat 模式，
     * 避免每次拼接时的 O(n) strlen 扫描和潜在的缓冲区溢出风险 */
    size_t latex_len = 0;
    size_t python_len = 0;
    size_t dsl_len = 0;

    char latex_buf[FORMULA_LATEX_BUF_SIZE];
    char python_buf[FORMULA_PYTHON_BUF_SIZE];
    char dsl_buf[FORMULA_DSL_BUF_SIZE];

    /* 遍历所有节点 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node)
            continue;

        char name[MAX_NAME_LENGTH];
        formula_node_to_name(node, name, sizeof(name));

    static const GraphNodeRenderFunc s_funcs[] = {
        [GEOM_POINT] = render_geom_point,
        [GEOM_LINE_SEGMENT] = render_geom_line_segment,
        [GEOM_REGION] = render_geom_region,
        [GEOM_CIRCLE] = render_geom_circle,
        [GEOM_PORT] = render_geom_port,
        [GEOM_FUNCTION_BLOCK] = render_geom_function_block,
    };
    if ((unsigned)node->type < sizeof(s_funcs)/sizeof(s_funcs[0]) && s_funcs[node->type]) {
        s_funcs[node->type](node, name, result->latex_output, &latex_len, latex_size, result->python_output, &python_len, python_size, result->dsl_output, &dsl_len, dsl_size);
    }
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
        int n = snprintf(latex_buf, sizeof(latex_buf), "\\text{Constraint: } %s\\\\\n", constraint_latex);
        if (n > 0 && latex_len + (size_t) n < latex_size) {
            memcpy(result->latex_output + latex_len, latex_buf, (size_t) n);
            latex_len += (size_t) n;
            result->latex_output[latex_len] = '\0';
        }

        /* Python */
        n = snprintf(python_buf, sizeof(python_buf), "# Constraint: %s\n", constraint_name);
        if (n > 0 && python_len + (size_t) n < python_size) {
            memcpy(result->python_output + python_len, python_buf, (size_t) n);
            python_len += (size_t) n;
            result->python_output[python_len] = '\0';
        }

        /* DSL */
        n = snprintf(dsl_buf, sizeof(dsl_buf), "# constraint %s; ", constraint_name);
        if (n > 0 && dsl_len + (size_t) n < dsl_size) {
            memcpy(result->dsl_output + dsl_len, dsl_buf, (size_t) n);
            dsl_len += (size_t) n;
            result->dsl_output[dsl_len] = '\0';
        }
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
