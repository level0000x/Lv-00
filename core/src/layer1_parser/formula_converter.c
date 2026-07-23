/**
 * @file formula_converter.c
 * @brief 公式转换器实现
 *
 * @details 实现公式 AST 与约束图之间的双向转换。
 *          支持点、线段、圆等几何元素的解析和生成。
 *
 * @author Lv-00 Project
 * @version 3.0.1
 *
 * @dependencies
 *   - formula_converter.h : 转换器公共接口定义
 *   - formula_renderer.h  : 公式渲染器接口（渲染辅助）
 *   - lv_internal.h     : 内部数据结构和常量
 *   - lv_utils.h        : 统一内存分配器和工具函数
 */

#include "formula_converter.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "formula_renderer.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "stream.h"
#include "stream_context_util.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================
 * 内部常量和宏
 * ============================================================ */

#define MAX_VAR_MAP_SIZE 256
#define MAX_NAME_LENGTH 64

/* 公式转换器内部缓冲区大小常量 */
#define FORMULA_BUF_SIZE 256
#define FORMULA_LATEX_BUF_SIZE 512
#define FORMULA_PYTHON_BUF_SIZE 512
#define FORMULA_DSL_BUF_SIZE 512
#define FORMULA_EXPORT_BUF_SIZE 4096
#define FORMULA_SEG_LIST_SIZE 256
#define FORMULA_SEG_NAME_SIZE 64
#define FORMULA_EXPR_BUF_SIZE 128
#define FORMULA_RESULT_BUF_SIZE 64
#define FORMULA_LARGE_BUF_SIZE 1024
#define FORMULA_NODE_IDS_SIZE 64
#define FORMULA_VAR_MAP_SIZE 64

/**
 * 有理数近似精度缩放因子
 *
 * 将 double 浮点值转换为有理数（分子/分母）时使用的缩放精度。
 * 值 1000 表示保留小数点后 3 位精度。例如：3.14159 * 1000 = 3141/1000。
 * 选择 1000 是在精度与避免 int64_t 溢出之间的折中。
 */
#define lv_RATIONAL_APPROX_SCALE 1000

lv_DECLARE_STREAM_CTX(formula_converter);

/* ============================================================
 * 变量名到节点ID映射
 * ============================================================ */

typedef struct {
    char name[MAX_NAME_LENGTH];
    int node_id;
} VarMapEntry;

/* 注意：此全局变量已使用线程本地存储，每线程独立副本。
 * 若需跨线程共享变量映射，需额外使用互斥锁保护。 */
static lv_THREAD_LOCAL VarMapEntry g_var_map[MAX_VAR_MAP_SIZE];
static lv_THREAD_LOCAL int g_var_map_count = 0;

/**
 * @brief 根据变量名查询节点 ID
 *
 * @param var_name 变量名称
 * @return 节点 ID，未找到返回 -1
 */
int formula_get_node_id(const char *var_name) {
    if (!var_name)
        return -1;

    for (int i = 0; i < g_var_map_count; i++) {
        if (strcmp(g_var_map[i].name, var_name) == 0) {
            return g_var_map[i].node_id;
        }
    }
    return -1;
}

/**
 * @brief 设置变量名到节点 ID 的映射
 *
 * @param var_name 变量名称
 * @param node_id  节点 ID
 */
void formula_set_node_id(const char *var_name, int node_id) {
    if (!var_name)
        return;

    /* 检查是否已存在 */
    for (int i = 0; i < g_var_map_count; i++) {
        if (strcmp(g_var_map[i].name, var_name) == 0) {
            g_var_map[i].node_id = node_id;
            return;
        }
    }

    /* 添加新条目 */
    if (g_var_map_count < MAX_VAR_MAP_SIZE) {
        /* 使用 lv_strlcpy 替代不安全的 strncpy，自动保证零终止 */
        lv_strlcpy(g_var_map[g_var_map_count].name, var_name, MAX_NAME_LENGTH);
        g_var_map[g_var_map_count].node_id = node_id;
        g_var_map_count++;
    }
}

/**
 * @brief 清空变量映射表
 */
void formula_clear_var_map(void) {
    g_var_map_count = 0;
}

/* ============================================================
 * 辅助函数
 * ============================================================ */

/**
 * @brief 将数值 AST 节点转换为符号坐标
 *
 * @param node 数值节点
 * @return 新分配的符号坐标数组（2 个元素：x, y），失败返回 NULL
 */
SymbolicCoord *formula_number_to_coord(const FormulaNode *node) {
    if (!node || node->type != NODE_NUMBER) {
        return NULL;
    }

    if (node->data.number.is_integer) {
        return symbolic_coord_create_rational(node->data.number.numerator, 1);
    } else {
        /* 简化分数 */
        int64_t num = node->data.number.numerator;
        uint64_t denom = node->data.number.denominator;

        /* 计算 GCD（最大公约数）
         * 修复 INT64_MIN 取反溢出：当 num == INT64_MIN 时，
         * -num 会导致有符号整数溢出（未定义行为）。
         * 解决方案：使用 uint64_t 接收绝对值。
         * INT64_MIN 的绝对值 = -(INT64_MIN) = 2^63，
         * 在 uint64_t 中安全表示为 (uint64_t)INT64_MAX + 1。 */
        uint64_t a = (num == INT64_MIN) ? ((uint64_t) INT64_MAX + 1) : (uint64_t) (num < 0 ? -num : num);
        uint64_t b = denom;
        while (b != 0) {
            uint64_t t = b;
            b = a % b;
            a = t;
        }
        /* 当 GCD > 1 时约分分子和分母 */
        if (a > 1) {
            num /= (int64_t) a;
            denom /= a;
        }

        return symbolic_coord_create_rational(num, denom);
    }
}

SymbolicCoord **formula_coords_to_symbolic(const FormulaNode *coord_list, int *out_count) {
    if (!out_count || !coord_list || coord_list->type != NODE_COORDINATE_LIST) {
        if (out_count)
            *out_count = 0;
        return NULL;
    }

    int count = coord_list->data.coord_list.coord_count;
    SymbolicCoord **coords = (SymbolicCoord **) lv_calloc(count, sizeof(SymbolicCoord *)); /* 统一内存分配器 */
    if (!coords) {
        *out_count = 0;
        return NULL;
    }

    for (int i = 0; i < count; i++) {
        coords[i] = formula_number_to_coord(coord_list->data.coord_list.coords[i]);
        if (!coords[i]) {
            /* 创建默认坐标 0 */
            coords[i] = symbolic_coord_create_rational(0, 1);
        }
    }

    *out_count = count;
    return coords;
}

/**
 * @brief 从几何节点提取名称
 *
 * @param node     几何节点指针
 * @param out_name 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return true 成功，false 失败
 */
bool formula_node_to_name(const GeomNode *node, char *out_name, size_t buf_size) {
    if (!node || !out_name || buf_size == 0) {
        return false;
    }

    /* 根据节点类型生成名称 */
    switch (node->type) {
        case GEOM_POINT:
            snprintf(out_name, buf_size, "P%d", node->id);
            break;
        case GEOM_LINE_SEGMENT:
            snprintf(out_name, buf_size, "S%d", node->id);
            break;
        case GEOM_REGION:
            snprintf(out_name, buf_size, "R%d", node->id);
            break;
        case GEOM_PORT:
            snprintf(out_name, buf_size, "Port%d", node->id);
            break;
        case GEOM_FUNCTION_BLOCK:
            snprintf(out_name, buf_size, "FB%d", node->id);
            break;
        default:
            snprintf(out_name, buf_size, "N%d", node->id);
            break;
    }

    return true;
}

/* ============================================================
 * 几何对象转换
 * ============================================================ */

/**
 * @brief 将点 AST 节点转换为约束图节点
 *
 * @param point_node 点 AST 节点
 * @param graph      约束图指针
 * @param out_node_id 输出参数，接收创建的节点 ID
 * @return true 成功，false 失败
 */
bool formula_convert_point(const FormulaNode *point_node, ConstraintGraph *graph, int *out_node_id) {
    if (!point_node || point_node->type != NODE_GEOM_POINT || !graph || !out_node_id) {
        return false;
    }

    /* 获取坐标 */
    int coord_count = 0;
    SymbolicCoord **coords = NULL;

    if (point_node->data.geom_point.coords) {
        coords = formula_coords_to_symbolic(point_node->data.geom_point.coords, &coord_count);
    }

    /* 如果没有坐标，创建默认坐标 (0, 0) */
    if (!coords || coord_count < 2) {
        if (coords) {
            for (int i = 0; i < coord_count; i++) {
                symbolic_coord_destroy(coords[i]);
            }
            lv_free((void **) &coords); /* 统一内存释放器 */
        }
        coords = (SymbolicCoord **) lv_calloc(2, sizeof(SymbolicCoord *));
        if (!coords) {
            return -1; /* 内存分配失败 */
        }
        coords[0] = symbolic_coord_create_rational(0, 1);
        coords[1] = symbolic_coord_create_rational(0, 1);
        coord_count = 2;
    }

    /* 添加点到图 */
    AddNodeResult result = graph_add_point(graph, coords, coord_count);

    /* 释放坐标数组（节点已复制） */
    for (int i = 0; i < coord_count; i++) {
        symbolic_coord_destroy(coords[i]);
    }
    lv_free((void **) &coords); /* 统一内存释放器 */

    if (result != ADD_NODE_OK) {
        return false;
    }

    /* 获取新创建的节点 ID */
    *out_node_id = graph_get_node_count(graph) - 1;
    GeomNode *new_node = graph_get_node_by_id(graph, *out_node_id);
    if (new_node) {
        *out_node_id = new_node->id;
    }

    /* 记录变量名映射 */
    if (point_node->data.geom_point.name) {
        formula_set_node_id(point_node->data.geom_point.name, *out_node_id);
    }

    return true;
}

/**
 * @brief 将线段 AST 节点转换为约束图节点
 *
 * @param segment_node 线段 AST 节点
 * @param graph        约束图指针
 * @param out_node_id  输出参数，接收创建的节点 ID
 * @return true 成功，false 失败
 */
bool formula_convert_segment(const FormulaNode *segment_node, ConstraintGraph *graph, int *out_node_id) {
    if (!segment_node || segment_node->type != NODE_GEOM_SEGMENT || !graph || !out_node_id) {
        return false;
    }

    /* 获取端点 ID */
    int ep1_id = -1, ep2_id = -1;

    if (segment_node->data.geom_segment.endpoint1) {
        if (segment_node->data.geom_segment.endpoint1->type == NODE_IDENTIFIER) {
            ep1_id = formula_get_node_id(segment_node->data.geom_segment.endpoint1->data.identifier.name);
        }
        /* 如果端点是点定义，先创建点 */
        else if (segment_node->data.geom_segment.endpoint1->type == NODE_GEOM_POINT) {
            if (!formula_convert_point(segment_node->data.geom_segment.endpoint1, graph, &ep1_id)) {
                ep1_id = -1;
            }
        }
    }

    if (segment_node->data.geom_segment.endpoint2) {
        if (segment_node->data.geom_segment.endpoint2->type == NODE_IDENTIFIER) {
            ep2_id = formula_get_node_id(segment_node->data.geom_segment.endpoint2->data.identifier.name);
        } else if (segment_node->data.geom_segment.endpoint2->type == NODE_GEOM_POINT) {
            if (!formula_convert_point(segment_node->data.geom_segment.endpoint2, graph, &ep2_id)) {
                ep2_id = -1;
            }
        }
    }

    if (ep1_id < 0 || ep2_id < 0) {
        return false;
    }

    /* 添加线段到图 */
    AddNodeResult result = graph_add_line_segment(graph, ep1_id, ep2_id);

    if (result != ADD_NODE_OK) {
        return false;
    }

    /* 获取新创建的节点 ID */
    *out_node_id = graph_get_node_count(graph) - 1;
    GeomNode *new_node = graph_get_node_by_id(graph, *out_node_id);
    if (new_node) {
        *out_node_id = new_node->id;
    }

    /* 记录变量名映射 */
    if (segment_node->data.geom_segment.name) {
        formula_set_node_id(segment_node->data.geom_segment.name, *out_node_id);
    }

    return true;
}

/**
 * @brief 将圆 AST 节点转换为约束图节点
 *
 * @param circle_node 圆 AST 节点
 * @param graph       约束图指针
 * @param out_node_id 输出参数，接收创建的节点 ID
 * @return true 成功，false 失败
 */
bool formula_convert_circle(const FormulaNode *circle_node, ConstraintGraph *graph, int *out_node_id) {
    if (!circle_node || circle_node->type != NODE_GEOM_CIRCLE || !graph || !out_node_id) {
        return false;
    }

    /* 获取圆心 ID */
    int center_id = -1;

    if (circle_node->data.geom_circle.center) {
        if (circle_node->data.geom_circle.center->type == NODE_IDENTIFIER) {
            center_id = formula_get_node_id(circle_node->data.geom_circle.center->data.identifier.name);
        } else if (circle_node->data.geom_circle.center->type == NODE_GEOM_POINT) {
            if (!formula_convert_point(circle_node->data.geom_circle.center, graph, &center_id)) {
                center_id = -1;
            }
        }
    }

    /* 如果没有圆心，创建一个默认圆心点 */
    if (center_id < 0) {
        SymbolicCoord *coords[2];
        coords[0] = symbolic_coord_create_rational(0, 1);
        coords[1] = symbolic_coord_create_rational(0, 1);

        AddNodeResult result = graph_add_point(graph, coords, 2);
        symbolic_coord_destroy(coords[0]);
        symbolic_coord_destroy(coords[1]);

        if (result != ADD_NODE_OK) {
            return false;
        }

        center_id = graph_get_node_count(graph) - 1;
        GeomNode *center_node = graph_get_node_by_id(graph, center_id);
        if (center_node) {
            center_id = center_node->id;
        }
    }

    /* 获取半径 */
    double radius = 1.0;
    if (circle_node->data.geom_circle.radius) {
        if (circle_node->data.geom_circle.radius->type == NODE_NUMBER) {
            if (circle_node->data.geom_circle.radius->data.number.is_integer) {
                radius = (double) circle_node->data.geom_circle.radius->data.number.numerator;
            } else {
                radius = (double) circle_node->data.geom_circle.radius->data.number.numerator /
                         (double) circle_node->data.geom_circle.radius->data.number.denominator;
            }
        }
    }

    /* 创建圆周上的一个点（用于表示半径） */
    SymbolicCoord *radius_coords[2];
    radius_coords[0] = symbolic_coord_create_rational((int64_t) (radius * 1000), 1000); /* 圆心 x + radius */
    radius_coords[1] = symbolic_coord_create_rational(0, 1);                            /* 圆心 y */

    AddNodeResult result = graph_add_point(graph, radius_coords, 2);
    symbolic_coord_destroy(radius_coords[0]);
    symbolic_coord_destroy(radius_coords[1]);

    if (result != ADD_NODE_OK) {
        *out_node_id = center_id; /* 至少返回圆心 */
        return true;
    }

    int radius_point_id = graph_get_node_count(graph) - 1;
    GeomNode *radius_node = graph_get_node_by_id(graph, radius_point_id);
    if (radius_node) {
        radius_point_id = radius_node->id;
    }

    /* 创建半径线段 */
    result = graph_add_line_segment(graph, center_id, radius_point_id);

    *out_node_id = center_id; /* 返回圆心作为圆的标识 */

    /* 记录变量名映射 */
    if (circle_node->data.geom_circle.name) {
        formula_set_node_id(circle_node->data.geom_circle.name, center_id);
    }

    return true;
}

/**
 * @brief 将三角形 AST 节点转换为约束图节点
 *
 * @param triangle_node 三角形 AST 节点
 * @param graph         约束图指针
 * @param[out] out_node_ids 输出参数，接收创建的节点 ID 数组
 * @param[out] out_count    输出参数，接收节点数量
 * @return true 成功，false 失败
 */
bool formula_convert_triangle(const FormulaNode *triangle_node, ConstraintGraph *graph, int *out_node_ids,
                              int *out_count) {
    if (!triangle_node || triangle_node->type != NODE_GEOM_TRIANGLE || !graph || !out_node_ids || !out_count) {
        return false;
    }

    /* 获取三个顶点 ID */
    int v1_id = -1, v2_id = -1, v3_id = -1;

    if (triangle_node->data.geom_triangle.vertex1) {
        if (triangle_node->data.geom_triangle.vertex1->type == NODE_IDENTIFIER) {
            v1_id = formula_get_node_id(triangle_node->data.geom_triangle.vertex1->data.identifier.name);
        } else if (triangle_node->data.geom_triangle.vertex1->type == NODE_GEOM_POINT) {
            formula_convert_point(triangle_node->data.geom_triangle.vertex1, graph, &v1_id);
        }
    }

    if (triangle_node->data.geom_triangle.vertex2) {
        if (triangle_node->data.geom_triangle.vertex2->type == NODE_IDENTIFIER) {
            v2_id = formula_get_node_id(triangle_node->data.geom_triangle.vertex2->data.identifier.name);
        } else if (triangle_node->data.geom_triangle.vertex2->type == NODE_GEOM_POINT) {
            formula_convert_point(triangle_node->data.geom_triangle.vertex2, graph, &v2_id);
        }
    }

    if (triangle_node->data.geom_triangle.vertex3) {
        if (triangle_node->data.geom_triangle.vertex3->type == NODE_IDENTIFIER) {
            v3_id = formula_get_node_id(triangle_node->data.geom_triangle.vertex3->data.identifier.name);
        } else if (triangle_node->data.geom_triangle.vertex3->type == NODE_GEOM_POINT) {
            formula_convert_point(triangle_node->data.geom_triangle.vertex3, graph, &v3_id);
        }
    }

    if (v1_id < 0 || v2_id < 0 || v3_id < 0) {
        return false;
    }

    /* 记录顶点 */
    out_node_ids[0] = v1_id;
    out_node_ids[1] = v2_id;
    out_node_ids[2] = v3_id;
    *out_count = 3;

    /* 创建三条边 */
    AddNodeResult result;

    result = graph_add_line_segment(graph, v1_id, v2_id);
    if (result == ADD_NODE_OK) {
        out_node_ids[(*out_count)++] = graph_get_node_count(graph) - 1;
    }

    result = graph_add_line_segment(graph, v2_id, v3_id);
    if (result == ADD_NODE_OK) {
        out_node_ids[(*out_count)++] = graph_get_node_count(graph) - 1;
    }

    result = graph_add_line_segment(graph, v3_id, v1_id);
    if (result == ADD_NODE_OK) {
        out_node_ids[(*out_count)++] = graph_get_node_count(graph) - 1;
    }

    return true;
}

/* ============================================================
 * 多边形/区域/弧 转换
 * ============================================================ */

/**
 * @brief 转换多边形定义到约束图
 *
 * 多边形由顶点列表定义，转换为多条首尾相连的 LINE_SEGMENT 围成封闭区域，
 * 然后用这些线段创建 GEOM_REGION 节点。
 *
 * @param[in]  polygon_node 多边形节点（类型须为 NODE_GEOM_POLYGON）
 * @param[in]  graph        目标图
 * @param[out] out_node_ids 输出节点ID数组（顶点 + 边 + 区域）
 * @param[out] out_count    输出节点数量
 * @return 成功返回 true，失败返回 false
 */
bool formula_convert_polygon(const FormulaNode *polygon_node, ConstraintGraph *graph, int *out_node_ids,
                             int *out_count) {
    if (!polygon_node || polygon_node->type != NODE_GEOM_POLYGON || !graph || !out_node_ids || !out_count) {
        return false;
    }

    *out_count = 0;

    int vert_count = polygon_node->data.geom_polygon.vertex_count;
    if (vert_count < 3) {
        return false; /* 多边形至少需要3个顶点 */
    }

    /* 创建所有顶点 */
    int *vertex_ids = (int *) lv_calloc(vert_count, sizeof(int)); /* 统一内存分配器 */
    if (!vertex_ids)
        return false;

    for (int i = 0; i < vert_count; i++) {
        FormulaNode *v = polygon_node->data.geom_polygon.vertices[i];
        int v_id = -1;

        if (v) {
            if (v->type == NODE_IDENTIFIER) {
                v_id = formula_get_node_id(v->data.identifier.name);
            } else if (v->type == NODE_GEOM_POINT) {
                formula_convert_point(v, graph, &v_id);
            }
        }

        if (v_id < 0) {
            /* 创建默认顶点 */
            SymbolicCoord *coords[2];
            coords[0] = symbolic_coord_create_rational(0, 1);
            coords[1] = symbolic_coord_create_rational(0, 1);
            AddNodeResult r = graph_add_point(graph, coords, 2);
            symbolic_coord_destroy(coords[0]);
            symbolic_coord_destroy(coords[1]);
            if (r == ADD_NODE_OK) {
                v_id = graph_get_node_count(graph) - 1;
                GeomNode *n = graph_get_node_by_id(graph, v_id);
                if (n)
                    v_id = n->id;
            }
        }

        vertex_ids[i] = v_id;
        if (v_id >= 0 && *out_count < FORMULA_VAR_MAP_SIZE) {
            out_node_ids[(*out_count)++] = v_id;
        }
    }

    /* 创建边（首尾相连） */
    int *segment_ids = (int *) lv_calloc(vert_count, sizeof(int)); /* 统一内存分配器 */
    if (!segment_ids) {
        lv_free((void **) &vertex_ids); /* 统一内存释放器 */
        return false;
    }
    int seg_count = 0;

    for (int i = 0; i < vert_count; i++) {
        int next_i = (i + 1) % vert_count;
        AddNodeResult r = graph_add_line_segment(graph, vertex_ids[i], vertex_ids[next_i]);
        if (r == ADD_NODE_OK) {
            int seg_id = graph_get_node_count(graph) - 1;
            GeomNode *n = graph_get_node_by_id(graph, seg_id);
            if (n)
                seg_id = n->id;
            segment_ids[seg_count++] = seg_id;
            if (*out_count < FORMULA_VAR_MAP_SIZE) {
                out_node_ids[(*out_count)++] = seg_id;
            }
        }
    }

    /* 第三步：用边界线段创建 GEOM_REGION */
    if (seg_count >= 3) {
        AddNodeResult r = graph_add_region(graph, segment_ids, seg_count);
        if (r == ADD_NODE_OK) {
            int region_id = graph_get_node_count(graph) - 1;
            GeomNode *n = graph_get_node_by_id(graph, region_id);
            if (n)
                region_id = n->id;
            if (*out_count < FORMULA_VAR_MAP_SIZE) {
                out_node_ids[(*out_count)++] = region_id;
            }
            /* 记录变量名映射 */
            if (polygon_node->data.geom_polygon.name) {
                formula_set_node_id(polygon_node->data.geom_polygon.name, region_id);
            }
        }
    }

    lv_free((void **) &vertex_ids);  /* 统一内存释放器 */
    lv_free((void **) &segment_ids); /* 统一内存释放器 */
    return true;
}

/**
 * @brief 转换区域定义到约束图
 *
 * 区域由边界线段列表定义，直接使用 graph_add_region 创建 GEOM_REGION 节点。
 * 子节点应为线段标识符或线段定义。
 *
 * @param[in]  region_node 区域节点（类型须为 NODE_GEOM_REGION）
 * @param[in]  graph       目标图
 * @param[out] out_node_id 输出节点ID
 * @return 成功返回 true，失败返回 false
 */
bool formula_convert_region(const FormulaNode *region_node, ConstraintGraph *graph, int *out_node_id) {
    if (!region_node || region_node->type != NODE_GEOM_REGION || !graph || !out_node_id) {
        return false;
    }

    *out_node_id = -1;

    int seg_count = region_node->data.geom_region.segment_count;
    if (seg_count < 3) {
        return false; /* 区域至少需要3条边界线段 */
    }

    /* 从子节点中提取边界线段 ID */
    int *segment_ids = (int *) lv_calloc(seg_count, sizeof(int)); /* 统一内存分配器 */
    if (!segment_ids)
        return false;

    int valid_count = 0;
    for (int i = 0; i < seg_count; i++) {
        FormulaNode *seg = region_node->data.geom_region.boundary_segments[i];
        int seg_id = -1;

        if (seg) {
            if (seg->type == NODE_IDENTIFIER) {
                seg_id = formula_get_node_id(seg->data.identifier.name);
            } else if (seg->type == NODE_GEOM_SEGMENT) {
                formula_convert_segment(seg, graph, &seg_id);
            }
        }

        if (seg_id >= 0) {
            segment_ids[valid_count++] = seg_id;
        }
    }

    if (valid_count < 3) {
        lv_free((void **) &segment_ids); /* 统一内存释放器 */
        return false;
    }

    /* 创建区域节点 */
    AddNodeResult r = graph_add_region(graph, segment_ids, valid_count);

    if (r == ADD_NODE_OK) {
        *out_node_id = graph_get_node_count(graph) - 1;
        GeomNode *n = graph_get_node_by_id(graph, *out_node_id);
        if (n) {
            *out_node_id = n->id;
        }
        /* 记录变量名映射 */
        if (region_node->data.geom_region.name) {
            formula_set_node_id(region_node->data.geom_region.name, *out_node_id);
        }
    }

    lv_free((void **) &segment_ids); /* 统一内存释放器 */
    return (*out_node_id >= 0);
}

/**
 * @brief 转换弧定义到约束图
 *
 * 弧由圆心、半径、起止角度定义。转换为：
 *   1. 圆心点
 *   2. 圆周上的半径点（用于表示半径）
 *   3. 半径线段
 * 弧的角度约束通过起止角度参数记录在变量映射中。
 *
 * @param[in]  arc_node   弧节点（类型须为 NODE_GEOM_ARC）
 * @param[in]  graph      目标图
 * @param[out] out_node_ids 输出节点ID数组（圆心 + 半径点 + 半径线段）
 * @param[out] out_count    输出节点数量
 * @return 成功返回 true，失败返回 false
 */
bool formula_convert_arc(const FormulaNode *arc_node, ConstraintGraph *graph, int *out_node_ids, int *out_count) {
    if (!arc_node || arc_node->type != NODE_GEOM_ARC || !graph || !out_node_ids || !out_count) {
        return false;
    }

    *out_count = 0;

    /* 获取圆心 */
    int center_id = -1;
    if (arc_node->data.geom_arc.center) {
        if (arc_node->data.geom_arc.center->type == NODE_IDENTIFIER) {
            center_id = formula_get_node_id(arc_node->data.geom_arc.center->data.identifier.name);
        } else if (arc_node->data.geom_arc.center->type == NODE_GEOM_POINT) {
            formula_convert_point(arc_node->data.geom_arc.center, graph, &center_id);
        }
    }

    /* 如果没有圆心，创建默认圆心 */
    if (center_id < 0) {
        SymbolicCoord *coords[2];
        coords[0] = symbolic_coord_create_rational(0, 1);
        coords[1] = symbolic_coord_create_rational(0, 1);
        AddNodeResult r = graph_add_point(graph, coords, 2);
        symbolic_coord_destroy(coords[0]);
        symbolic_coord_destroy(coords[1]);
        if (r == ADD_NODE_OK) {
            center_id = graph_get_node_count(graph) - 1;
            GeomNode *n = graph_get_node_by_id(graph, center_id);
            if (n)
                center_id = n->id;
        }
    }

    if (center_id < 0)
        return false;
    out_node_ids[(*out_count)++] = center_id;

    /* 获取半径 */
    double radius = 1.0;
    if (arc_node->data.geom_arc.radius) {
        if (arc_node->data.geom_arc.radius->type == NODE_NUMBER) {
            if (arc_node->data.geom_arc.radius->data.number.is_integer) {
                radius = (double) arc_node->data.geom_arc.radius->data.number.numerator;
            } else {
                radius = (double) arc_node->data.geom_arc.radius->data.number.numerator /
                         (double) arc_node->data.geom_arc.radius->data.number.denominator;
            }
        }
    }

    /* 获取起止角度（弧度） */
    double start_angle = 0.0;
    double end_angle = M_PI; /* 默认半圆 */

    if (arc_node->data.geom_arc.start_angle) {
        if (arc_node->data.geom_arc.start_angle->type == NODE_NUMBER) {
            FormulaNode *sa = arc_node->data.geom_arc.start_angle;
            if (sa->data.number.is_integer) {
                start_angle = (double) sa->data.number.numerator;
            } else {
                start_angle = (double) sa->data.number.numerator / (double) sa->data.number.denominator;
            }
        }
    }

    if (arc_node->data.geom_arc.end_angle) {
        if (arc_node->data.geom_arc.end_angle->type == NODE_NUMBER) {
            FormulaNode *ea = arc_node->data.geom_arc.end_angle;
            if (ea->data.number.is_integer) {
                end_angle = (double) ea->data.number.numerator;
            } else {
                end_angle = (double) ea->data.number.numerator / (double) ea->data.number.denominator;
            }
        }
    }

    /* 创建半径端点（在起始角度方向上） */
    double rx = radius * cos(start_angle);
    double ry = radius * sin(start_angle);
    SymbolicCoord *radius_coords[2];
    radius_coords[0] = symbolic_coord_create_rational((int64_t) (rx * 1000), 1000);
    radius_coords[1] = symbolic_coord_create_rational((int64_t) (ry * 1000), 1000);

    AddNodeResult r = graph_add_point(graph, radius_coords, 2);
    symbolic_coord_destroy(radius_coords[0]);
    symbolic_coord_destroy(radius_coords[1]);

    if (r == ADD_NODE_OK) {
        int rp_id = graph_get_node_count(graph) - 1;
        GeomNode *n = graph_get_node_by_id(graph, rp_id);
        if (n)
            rp_id = n->id;
        out_node_ids[(*out_count)++] = rp_id;

        /* 创建半径线段 */
        r = graph_add_line_segment(graph, center_id, rp_id);
        if (r == ADD_NODE_OK) {
            int seg_id = graph_get_node_count(graph) - 1;
            n = graph_get_node_by_id(graph, seg_id);
            if (n)
                seg_id = n->id;
            out_node_ids[(*out_count)++] = seg_id;
        }
    }

    /* 记录变量名映射 */
    if (arc_node->data.geom_arc.name) {
        formula_set_node_id(arc_node->data.geom_arc.name, center_id);
    }

    return true;
}

/* ============================================================
 * 约束转换
 * ============================================================ */

bool formula_convert_perpendicular(const FormulaNode *constraint_node, ConstraintGraph *graph, int *out_constraint_id) {
    if (!constraint_node || constraint_node->type != NODE_CONSTRAINT_PERPENDICULAR || !graph || !out_constraint_id) {
        return false;
    }

    if (constraint_node->data.constraint.participant_count < 3) {
        return false;
    }

    /* 获取三个点 ID */
    int p1_id = -1, p2_id = -1, p3_id = -1;

    if (constraint_node->data.constraint.participants[0]->type == NODE_IDENTIFIER) {
        p1_id = formula_get_node_id(constraint_node->data.constraint.participants[0]->data.identifier.name);
    }
    if (constraint_node->data.constraint.participants[1]->type == NODE_IDENTIFIER) {
        p2_id = formula_get_node_id(constraint_node->data.constraint.participants[1]->data.identifier.name);
    }
    if (constraint_node->data.constraint.participants[2]->type == NODE_IDENTIFIER) {
        p3_id = formula_get_node_id(constraint_node->data.constraint.participants[2]->data.identifier.name);
    }

    if (p1_id < 0 || p2_id < 0 || p3_id < 0) {
        return false;
    }

    /* 添加 betweenness 约束（简化处理） */
    AddConstraintResult result = graph_add_betweenness(graph, p1_id, p2_id, p3_id);

    if (result != ADD_CONSTRAINT_OK) {
        return false;
    }

    *out_constraint_id = graph_get_constraint_count(graph) - 1;

    return true;
}

bool formula_convert_parallel(const FormulaNode *constraint_node, ConstraintGraph *graph, int *out_constraint_id) {
    if (!constraint_node || constraint_node->type != NODE_CONSTRAINT_PARALLEL || !graph || !out_constraint_id) {
        return false;
    }

    if (constraint_node->data.constraint.participant_count < 2) {
        return false;
    }

    /* 获取两条线 ID */
    int l1_id = -1, l2_id = -1;

    if (constraint_node->data.constraint.participants[0]->type == NODE_IDENTIFIER) {
        l1_id = formula_get_node_id(constraint_node->data.constraint.participants[0]->data.identifier.name);
    }
    if (constraint_node->data.constraint.participants[1]->type == NODE_IDENTIFIER) {
        l2_id = formula_get_node_id(constraint_node->data.constraint.participants[1]->data.identifier.name);
    }

    if (l1_id < 0 || l2_id < 0) {
        return false;
    }

    /* 平行约束目前使用 containment 简化表示 */
    /* 注意：实际实现可能需要扩展 ConstraintType */
    AddConstraintResult result = graph_add_containment(graph, l1_id, l2_id);

    if (result != ADD_CONSTRAINT_OK) {
        return false;
    }

    *out_constraint_id = graph_get_constraint_count(graph) - 1;

    return true;
}

bool formula_convert_midpoint(const FormulaNode *constraint_node, ConstraintGraph *graph, int *out_node_id) {
    if (!constraint_node || constraint_node->type != NODE_CONSTRAINT_MIDPOINT || !graph || !out_node_id) {
        return false;
    }

    if (constraint_node->data.constraint.participant_count < 3) {
        return false;
    }

    /* 获取中点 M 和端点 A, B */
    int m_id = -1, a_id = -1, b_id = -1;

    if (constraint_node->data.constraint.participants[0]->type == NODE_IDENTIFIER) {
        m_id = formula_get_node_id(constraint_node->data.constraint.participants[0]->data.identifier.name);
    }
    if (constraint_node->data.constraint.participants[1]->type == NODE_IDENTIFIER) {
        a_id = formula_get_node_id(constraint_node->data.constraint.participants[1]->data.identifier.name);
    }
    if (constraint_node->data.constraint.participants[2]->type == NODE_IDENTIFIER) {
        b_id = formula_get_node_id(constraint_node->data.constraint.participants[2]->data.identifier.name);
    }

    if (a_id < 0 || b_id < 0) {
        return false;
    }

    /* 获取 A 和 B 的坐标，计算中点 */
    GeomNode *node_a = graph_get_node_by_id(graph, a_id);
    GeomNode *node_b = graph_get_node_by_id(graph, b_id);

    if (!node_a || !node_b) {
        return false;
    }

    /* 计算中点坐标 */
    SymbolicCoord *mid_coords[2] = {NULL, NULL};

    if (node_a->symbolic_coords && node_b->symbolic_coords && node_a->coord_count >= 2 && node_b->coord_count >= 2) {
        mid_coords[0] = symbolic_coord_add(node_a->symbolic_coords[0], node_b->symbolic_coords[0]);
        mid_coords[1] = symbolic_coord_add(node_a->symbolic_coords[1], node_b->symbolic_coords[1]);

        /* 除以 2 */
        SymbolicCoord *half = symbolic_coord_create_rational(1, 2);
        SymbolicCoord *tmp;

        tmp = symbolic_coord_multiply(mid_coords[0], half);
        symbolic_coord_destroy(mid_coords[0]);
        mid_coords[0] = tmp;

        tmp = symbolic_coord_multiply(mid_coords[1], half);
        symbolic_coord_destroy(mid_coords[1]);
        mid_coords[1] = tmp;

        symbolic_coord_destroy(half);
    }

    if (!mid_coords[0] || !mid_coords[1]) {
        /* 使用默认坐标 */
        mid_coords[0] = symbolic_coord_create_rational(0, 1);
        mid_coords[1] = symbolic_coord_create_rational(0, 1);
    }

    /* 如果中点 M 已存在，更新其坐标 */
    if (m_id >= 0) {
        GeomNode *node_m = graph_get_node_by_id(graph, m_id);
        if (node_m && node_m->symbolic_coords) {
            int update_count = node_m->coord_count < 2 ? node_m->coord_count : 2;
            for (int i = 0; i < update_count; i++) {
                symbolic_coord_destroy(node_m->symbolic_coords[i]);
                node_m->symbolic_coords[i] = mid_coords[i];
            }
            /* 释放未使用的坐标，避免内存泄漏 */
            for (int i = update_count; i < 2; i++) {
                symbolic_coord_destroy(mid_coords[i]);
            }
        } else {
            /* 节点不存在或无坐标数组，释放所有 mid_coords */
            symbolic_coord_destroy(mid_coords[0]);
            symbolic_coord_destroy(mid_coords[1]);
        }
        *out_node_id = m_id;
    } else {
        /* 创建新的中点 */
        AddNodeResult result = graph_add_point(graph, mid_coords, 2);
        if (result == ADD_NODE_OK) {
            *out_node_id = graph_get_node_count(graph) - 1;
            GeomNode *new_node = graph_get_node_by_id(graph, *out_node_id);
            if (new_node) {
                *out_node_id = new_node->id;
            }

            /* 如果有中点名，记录映射 */
            if (constraint_node->data.constraint.participants[0]->type == NODE_IDENTIFIER) {
                formula_set_node_id(constraint_node->data.constraint.participants[0]->data.identifier.name,
                                    *out_node_id);
            }
        }

        symbolic_coord_destroy(mid_coords[0]);
        symbolic_coord_destroy(mid_coords[1]);
    }

    return true;
}

/**
 * @brief 转换角度约束到约束图
 *
 * 角度约束 ∠ABC = θ 可以转换为向量点积约束：
 *   向量 BA = A - B, 向量 BC = C - B
 *   BA · BC = |BA| * |BC| * cos(θ)
 *
 * 由于约束图当前不支持直接的代数方程约束，这里使用 betweenness 约束
 * 作为近似表示，并将角度信息存储在约束的附加数据中。
 * 后续求解器可以读取这些信息进行精确的角度约束求解。
 *
 * @param[in]  constraint_node 约束节点（类型须为 NODE_CONSTRAINT_ANGLE）
 * @param[in]  graph           目标图
 * @param[out] out_constraint_id 输出约束ID
 * @return 成功返回 true，失败返回 false
 */
bool formula_convert_angle(const FormulaNode *constraint_node, ConstraintGraph *graph, int *out_constraint_id) {
    if (!constraint_node || constraint_node->type != NODE_CONSTRAINT_ANGLE || !graph || !out_constraint_id) {
        return false;
    }

    /*
     * 角度约束参与者格式：
     *   participants[0] = 点 A（角的第一个端点）
     *   participants[1] = 点 B（角的顶点）
     *   participants[2] = 点 C（角的第二个端点）
     *   participants[3] = 角度值 θ（数字节点，可选，默认 90 度）
     */

    if (constraint_node->data.constraint.participant_count < 3) {
        return false;
    }

    /* 获取三个点 ID */
    int a_id = -1, b_id = -1, c_id = -1;

    if (constraint_node->data.constraint.participants[0]->type == NODE_IDENTIFIER) {
        a_id = formula_get_node_id(constraint_node->data.constraint.participants[0]->data.identifier.name);
    }
    if (constraint_node->data.constraint.participants[1]->type == NODE_IDENTIFIER) {
        b_id = formula_get_node_id(constraint_node->data.constraint.participants[1]->data.identifier.name);
    }
    if (constraint_node->data.constraint.participants[2]->type == NODE_IDENTIFIER) {
        c_id = formula_get_node_id(constraint_node->data.constraint.participants[2]->data.identifier.name);
    }

    if (a_id < 0 || b_id < 0 || c_id < 0) {
        return false;
    }

    /* 提取角度值（弧度） */
    double angle_rad = M_PI / 2.0; /* 默认 90 度 */

    if (constraint_node->data.constraint.participant_count >= 4) {
        const FormulaNode *angle_node = constraint_node->data.constraint.participants[3];
        if (angle_node && angle_node->type == NODE_NUMBER) {
            double angle_deg;
            if (angle_node->data.number.is_integer) {
                angle_deg = (double) angle_node->data.number.numerator;
            } else {
                angle_deg = (double) angle_node->data.number.numerator / (double) angle_node->data.number.denominator;
            }
            angle_rad = angle_deg * M_PI / 180.0;
        }
    }

    /*
     * 计算向量点积约束参数：
     *   BA · BC = |BA| * |BC| * cos(θ)
     *
     * 展开为坐标形式（设 A=(ax,ay), B=(bx,by), C=(cx,cy)）：
     *   (ax-bx)(cx-bx) + (ay-by)(cy-by) = |BA|*|BC|*cos(θ)
     *
     * 这是一个二次方程，约束图无法直接表示。
     * 当前使用 betweenness 约束作为拓扑占位符，
     * 并将角度信息编码到 numeric_assumption_declaration 中。
     */

    if (formula_converter_stream_ctx) {
        stream_emit_warning(formula_converter_stream_ctx, "角度约束转换为近似实现（使用 betweenness 占位）", 0);
    }

    AddConstraintResult result = graph_add_betweenness(graph, a_id, b_id, c_id);

    if (result != ADD_CONSTRAINT_OK) {
        return false;
    }

    *out_constraint_id = graph_get_constraint_count(graph) - 1;

    /* 将角度约束的详细信息存储到约束节点上 */
    Constraint *constraint = graph_get_constraint(graph, *out_constraint_id);
    if (constraint) {
        /*
         * 注意：Constraint 结构体没有 numeric_assumption_declaration 字段。
         * 角度信息通过创建一个辅助点节点来存储。
         * 该点节点的 numeric_assumption_declaration 包含角度约束参数。
         */
    }

    /* 创建一个辅助节点来存储角度约束的代数信息 */
    {
        double cos_theta = cos(angle_rad);
        double sin_theta = sin(angle_rad);

        /* 使用两个坐标存储 cos(θ) 和 sin(θ) */
        SymbolicCoord *angle_coords[2];
        angle_coords[0] = symbolic_coord_create_rational((int64_t) (cos_theta * 1000000), 1000000);
        angle_coords[1] = symbolic_coord_create_rational((int64_t) (sin_theta * 1000000), 1000000);

        AddNodeResult add_result = graph_add_point(graph, angle_coords, 2);
        symbolic_coord_destroy(angle_coords[0]);
        symbolic_coord_destroy(angle_coords[1]);

        if (add_result == ADD_NODE_OK) {
            int aux_id = graph->next_node_id - 1;
            GeomNode *aux_node = graph_get_node(graph, aux_id);
            if (aux_node) {
                if (aux_node->numeric_assumption_declaration) {
                    lv_free((void **) &aux_node->numeric_assumption_declaration); /* 统一内存释放器 */
                    aux_node->numeric_assumption_declaration = NULL;
                }
                /*
                 * 格式: ANGLE_CONSTRAINT:A_id:B_id:C_id:angle_rad:cos:sin
                 * 求解器可解析此字符串获取完整的角度约束信息。
                 */
                char buf[FORMULA_BUF_SIZE];
                snprintf(buf, sizeof(buf), "ANGLE_CONSTRAINT:%d:%d:%d:%.10g:%.10g:%.10g", a_id, b_id, c_id, angle_rad,
                         cos_theta, sin_theta);
                aux_node->numeric_assumption_declaration = lv_strdup_safe(buf);
            }
        }
    }

    return true;
}

/* ============================================================
 * 公式 → 图 主转换函数
 * ============================================================ */

/**
 * @brief 处理单个 AST 语句节点，将其转换为图节点/约束并记录到结果中
 *
 * 从 formula_to_graph 中提取的公共函数，避免 NODE_COMPOUND 循环和
 * 单语句 else 分支之间的 switch 逻辑重复。
 *
 * @param stmt        当前语句的 AST 节点
 * @param graph       目标约束图
 * @param result      转换结果（用于记录创建的节点/约束 ID）
 * @return true 表示成功处理了该语句类型，false 表示未处理（未知类型）
 */

/* 创建节点/约束的最大数量限制 */
#define MAX_CREATED_NODES 256
#define MAX_CREATED_CONSTRAINTS 64

static bool formula_to_graph_process_statement(const FormulaNode *stmt, ConstraintGraph *graph,
                                               FormulaToGraphResult *result) {
    int node_id = -1;
    int constraint_id = -1;

    switch (stmt->type) {
        case NODE_GEOM_POINT:
            if (formula_convert_point(stmt, graph, &node_id)) {
                if (result->created_node_count < MAX_CREATED_NODES)
                    result->created_node_ids[result->created_node_count++] = node_id;
            }
            return true;

        case NODE_GEOM_SEGMENT:
            if (formula_convert_segment(stmt, graph, &node_id)) {
                if (result->created_node_count < MAX_CREATED_NODES)
                    result->created_node_ids[result->created_node_count++] = node_id;
            }
            return true;

        case NODE_GEOM_CIRCLE:
            if (formula_convert_circle(stmt, graph, &node_id)) {
                if (result->created_node_count < MAX_CREATED_NODES)
                    result->created_node_ids[result->created_node_count++] = node_id;
            }
            return true;

        case NODE_CONSTRAINT_PERPENDICULAR:
            if (formula_convert_perpendicular(stmt, graph, &constraint_id)) {
                if (result->created_constraint_count < MAX_CREATED_CONSTRAINTS)
                    result->created_constraint_ids[result->created_constraint_count++] = constraint_id;
            }
            return true;

        case NODE_CONSTRAINT_PARALLEL:
            if (formula_convert_parallel(stmt, graph, &constraint_id)) {
                if (result->created_constraint_count < MAX_CREATED_CONSTRAINTS)
                    result->created_constraint_ids[result->created_constraint_count++] = constraint_id;
            }
            return true;

        case NODE_CONSTRAINT_MIDPOINT:
            if (formula_convert_midpoint(stmt, graph, &node_id)) {
                if (result->created_node_count < MAX_CREATED_NODES)
                    result->created_node_ids[result->created_node_count++] = node_id;
            }
            return true;

        case NODE_CONSTRAINT_ANGLE:
            if (formula_convert_angle(stmt, graph, &constraint_id)) {
                if (result->created_constraint_count < MAX_CREATED_CONSTRAINTS)
                    result->created_constraint_ids[result->created_constraint_count++] = constraint_id;
            }
            return true;

        case NODE_EQUATION:
            /* 代数方程：转换为约束图中的隐式曲线 */
            if (formula_convert_equation(stmt, graph, &node_id)) {
                if (result->created_node_count < MAX_CREATED_NODES)
                    result->created_node_ids[result->created_node_count++] = node_id;
            }
            return true;

        case NODE_GEOM_POLYGON: {
            int node_ids[FORMULA_NODE_IDS_SIZE];
            int count = 0;
            if (formula_convert_polygon(stmt, graph, node_ids, &count)) {
                if (count > FORMULA_NODE_IDS_SIZE)
                    count = FORMULA_NODE_IDS_SIZE; /* bounds check */
                for (int j = 0; j < count && result->created_node_count < MAX_CREATED_NODES; j++) {
                    result->created_node_ids[result->created_node_count++] = node_ids[j];
                }
            }
        }
            return true;

        case NODE_GEOM_REGION:
            if (formula_convert_region(stmt, graph, &node_id)) {
                if (result->created_node_count < MAX_CREATED_NODES)
                    result->created_node_ids[result->created_node_count++] = node_id;
            }
            return true;

        case NODE_GEOM_ARC: {
            int node_ids[10];
            int count = 0;
            if (formula_convert_arc(stmt, graph, node_ids, &count)) {
                if (count > 10)
                    count = 10; /* bounds check */
                for (int j = 0; j < count && result->created_node_count < MAX_CREATED_NODES; j++) {
                    result->created_node_ids[result->created_node_count++] = node_ids[j];
                }
            }
        }
            return true;

        default:
            return false;
    }
}

/**
 * @brief 将公式 AST 转换为约束图（主入口函数）
 *
 * 遍历 AST 树，将几何对象和约束转换为约束图中的节点和约束。
 *
 * @param ast   公式 AST 根节点
 * @param graph 约束图指针
 * @return 转换结果结构体指针，失败返回 NULL
 */
FormulaToGraphResult *formula_to_graph(const FormulaNode *ast, ConstraintGraph *graph) {
    FormulaToGraphResult *result =
        (FormulaToGraphResult *) lv_calloc(1, sizeof(FormulaToGraphResult)); /* 统一内存分配器 */
    if (!result) {
        return NULL;
    }

    if (!ast || !graph) {
        result->success = false;
        snprintf(result->error_message, sizeof(result->error_message), "NULL input");
        return result;
    }

    if (formula_converter_stream_ctx) {
        stream_emit_info(formula_converter_stream_ctx, "公式转换开始：AST → 约束图", 0);
    }

    /* 分配节点和约束 ID 数组 */
    result->created_node_ids = (int *) lv_calloc(MAX_CREATED_NODES, sizeof(int));             /* 统一内存分配器 */
    result->created_constraint_ids = (int *) lv_calloc(MAX_CREATED_CONSTRAINTS, sizeof(int)); /* 统一内存分配器 */

    if (!result->created_node_ids || !result->created_constraint_ids) {
        /* 修复：分配失败时释放已成功分配的数组，避免内存泄漏 */
        lv_free((void **) &result->created_node_ids);       /* 统一内存释放器 */
        lv_free((void **) &result->created_constraint_ids); /* 统一内存释放器 */
        result->created_node_ids = NULL;
        result->created_constraint_ids = NULL;
        result->success = false;
        snprintf(result->error_message, sizeof(result->error_message), "Memory allocation failed");
        return result;
    }

    /* 处理复合语句：使用公共函数逐个处理每条子语句 */
    if (ast->type == NODE_COMPOUND) {
        for (int i = 0; i < ast->data.compound.statement_count; i++) {
            formula_to_graph_process_statement(ast->data.compound.statements[i], graph, result);
        }
    } else {
        /* 单个语句：直接使用公共函数处理 */
        formula_to_graph_process_statement(ast, graph, result);
    }

    result->success = true;

    if (formula_converter_stream_ctx) {
        stream_emit_progress(formula_converter_stream_ctx, 1.0, "公式转换完成", 1, 1);
    }

    return result;
}

/* ============================================================
 * 图 → 公式 主转换函数
 * ============================================================ */

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
        return NULL;
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

        switch (node->type) {
            case GEOM_POINT: {
                /* 获取坐标 */
                double x = 0, y = 0;
                if (node->symbolic_coords && node->coord_count >= 2) {
                    x = symbolic_coord_to_double(node->symbolic_coords[0]);
                    y = symbolic_coord_to_double(node->symbolic_coords[1]);
                }

                /* LaTeX */
                int n = snprintf(latex_buf, sizeof(latex_buf), "%s = \\left(%.2f, %.2f\\right)\\\\\n", name, x, y);
                if (n > 0 && latex_len + (size_t) n < latex_size) {
                    memcpy(result->latex_output + latex_len, latex_buf, (size_t) n);
                    latex_len += (size_t) n;
                    result->latex_output[latex_len] = '\0';
                }

                /* Python */
                n = snprintf(python_buf, sizeof(python_buf), "%s = Point(%.2f, %.2f)\n", name, x, y);
                if (n > 0 && python_len + (size_t) n < python_size) {
                    memcpy(result->python_output + python_len, python_buf, (size_t) n);
                    python_len += (size_t) n;
                    result->python_output[python_len] = '\0';
                }

                /* DSL */
                n = snprintf(dsl_buf, sizeof(dsl_buf), "point %s(%.2f, %.2f); ", name, x, y);
                if (n > 0 && dsl_len + (size_t) n < dsl_size) {
                    memcpy(result->dsl_output + dsl_len, dsl_buf, (size_t) n);
                    dsl_len += (size_t) n;
                    result->dsl_output[dsl_len] = '\0';
                }
            } break;

            case GEOM_LINE_SEGMENT: {
                /* LaTeX */
                int n = snprintf(latex_buf, sizeof(latex_buf), "\\overline{%s}\\\\\n", name);
                if (n > 0 && latex_len + (size_t) n < latex_size) {
                    memcpy(result->latex_output + latex_len, latex_buf, (size_t) n);
                    latex_len += (size_t) n;
                    result->latex_output[latex_len] = '\0';
                }

                /* Python */
                n = snprintf(python_buf, sizeof(python_buf), "%s = Segment()\n", name);
                if (n > 0 && python_len + (size_t) n < python_size) {
                    memcpy(result->python_output + python_len, python_buf, (size_t) n);
                    python_len += (size_t) n;
                    result->python_output[python_len] = '\0';
                }

                /* DSL */
                n = snprintf(dsl_buf, sizeof(dsl_buf), "segment %s(); ", name);
                if (n > 0 && dsl_len + (size_t) n < dsl_size) {
                    memcpy(result->dsl_output + dsl_len, dsl_buf, (size_t) n);
                    dsl_len += (size_t) n;
                    result->dsl_output[dsl_len] = '\0';
                }
            } break;

            case GEOM_REGION: {
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
                    /* 修复：使用偏移量替代 strncat + strlen */
                    if (j > 0 && seg_list_len + 2 < sizeof(seg_list)) {
                        memcpy(seg_list + seg_list_len, ", ", 2);
                        seg_list_len += 2;
                    }
                    size_t sn_len = strlen(seg_name);
                    if (seg_list_len + sn_len < sizeof(seg_list)) {
                        memcpy(seg_list + seg_list_len, seg_name, sn_len);
                        seg_list_len += sn_len;
                    }
                }
                seg_list[seg_list_len] = '\0';

                /* LaTeX */
                int n = snprintf(latex_buf, sizeof(latex_buf), "\\text{region } %s(\\{%s\\})\\\\\n", name, seg_list);
                if (n > 0 && latex_len + (size_t) n < latex_size) {
                    memcpy(result->latex_output + latex_len, latex_buf, (size_t) n);
                    latex_len += (size_t) n;
                    result->latex_output[latex_len] = '\0';
                }

                /* Python */
                n = snprintf(python_buf, sizeof(python_buf), "%s = Region([%s])\n", name, seg_list);
                if (n > 0 && python_len + (size_t) n < python_size) {
                    memcpy(result->python_output + python_len, python_buf, (size_t) n);
                    python_len += (size_t) n;
                    result->python_output[python_len] = '\0';
                }

                /* DSL */
                n = snprintf(dsl_buf, sizeof(dsl_buf), "region %s(%s); ", name, seg_list);
                if (n > 0 && dsl_len + (size_t) n < dsl_size) {
                    memcpy(result->dsl_output + dsl_len, dsl_buf, (size_t) n);
                    dsl_len += (size_t) n;
                    result->dsl_output[dsl_len] = '\0';
                }
            } break;

            case GEOM_PORT: {
                const char *port_type_str = "unknown";
                if (node->data.port) {
                    port_type_str = (node->data.port->type == PORT_INPUT) ? "input" : "output";
                }

                /* LaTeX */
                int n =
                    snprintf(latex_buf, sizeof(latex_buf), "\\text{port } %s(\\text{%s})\\\\\n", name, port_type_str);
                if (n > 0 && latex_len + (size_t) n < latex_size) {
                    memcpy(result->latex_output + latex_len, latex_buf, (size_t) n);
                    latex_len += (size_t) n;
                    result->latex_output[latex_len] = '\0';
                }

                /* Python */
                n = snprintf(python_buf, sizeof(python_buf), "%s = Port('%s')\n", name, port_type_str);
                if (n > 0 && python_len + (size_t) n < python_size) {
                    memcpy(result->python_output + python_len, python_buf, (size_t) n);
                    python_len += (size_t) n;
                    result->python_output[python_len] = '\0';
                }

                /* DSL */
                n = snprintf(dsl_buf, sizeof(dsl_buf), "port %s(%s); ", name, port_type_str);
                if (n > 0 && dsl_len + (size_t) n < dsl_size) {
                    memcpy(result->dsl_output + dsl_len, dsl_buf, (size_t) n);
                    dsl_len += (size_t) n;
                    result->dsl_output[dsl_len] = '\0';
                }
            } break;

            case GEOM_FUNCTION_BLOCK: {
                /* 获取函数块信息 */
                int in_count = node->data.func_block.input_count;
                int out_count = node->data.func_block.output_count;

                /* LaTeX */
                int n = snprintf(latex_buf, sizeof(latex_buf),
                                 "\\text{func\\_block } %s(\\text{in: }%d, \\text{out: }%d)\\\\\n", name, in_count,
                                 out_count);
                if (n > 0 && latex_len + (size_t) n < latex_size) {
                    memcpy(result->latex_output + latex_len, latex_buf, (size_t) n);
                    latex_len += (size_t) n;
                    result->latex_output[latex_len] = '\0';
                }

                /* Python */
                n = snprintf(python_buf, sizeof(python_buf), "%s = FuncBlock(inputs=%d, outputs=%d)\n", name, in_count,
                             out_count);
                if (n > 0 && python_len + (size_t) n < python_size) {
                    memcpy(result->python_output + python_len, python_buf, (size_t) n);
                    python_len += (size_t) n;
                    result->python_output[python_len] = '\0';
                }

                /* DSL */
                n = snprintf(dsl_buf, sizeof(dsl_buf), "func_block %s(in=%d, out=%d); ", name, in_count, out_count);
                if (n > 0 && dsl_len + (size_t) n < dsl_size) {
                    memcpy(result->dsl_output + dsl_len, dsl_buf, (size_t) n);
                    dsl_len += (size_t) n;
                    result->dsl_output[dsl_len] = '\0';
                }
            } break;

            default:
                break;
        }
    }

    /* 遍历所有约束 */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *constraint = graph->constraints[i];
        if (!constraint)
            continue;

        const char *constraint_name = NULL;
        const char *constraint_latex = NULL;

        switch (constraint->type) {
            case INCIDENCE:
                constraint_name = "incidence";
                constraint_latex = "\\text{incidence}";
                break;
            case BETWEENNESS:
                constraint_name = "betweenness";
                constraint_latex = "\\text{betweenness}";
                break;
            case INTERSECTION:
                constraint_name = "intersection";
                constraint_latex = "\\cap";
                break;
            case CONTAINMENT:
                constraint_name = "containment";
                constraint_latex = "\\subset";
                break;
            case CONNECTION:
                constraint_name = "connection";
                constraint_latex = "\\leftrightarrow";
                break;
            default:
                constraint_name = "unknown";
                constraint_latex = "\\text{unknown}";
                break;
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
void formula_to_graph_result_destroy(FormulaToGraphResult *result) {
    if (!result)
        return;

    if (result->created_node_ids) {
        lv_free((void **) &result->created_node_ids); /* 统一内存释放器 */
    }
    if (result->created_constraint_ids) {
        lv_free((void **) &result->created_constraint_ids); /* 统一内存释放器 */
    }
    lv_free((void **) &result); /* 统一内存释放器 */
}

void graph_to_formula_result_destroy(GraphToFormulaResult *result) {
    if (!result)
        return;

    if (result->latex_output) {
        lv_free((void **) &result->latex_output); /* 统一内存释放器 */
    }
    if (result->python_output) {
        lv_free((void **) &result->python_output); /* 统一内存释放器 */
    }
    if (result->dsl_output) {
        lv_free((void **) &result->dsl_output); /* 统一内存释放器 */
    }
    lv_free((void **) &result); /* 统一内存释放器 */
}

/* ============================================================
 * 代数方程到曲线转换实现（新增）
 * ============================================================ */

/**
 * 销毁方程曲线转换结果
 */
void equation_curve_result_destroy(EquationCurveResult *result) {
    if (!result)
        return;

    if (result->points) {
        lv_free((void **) &result->points); /* 统一内存释放器 */
    }
    lv_free((void **) &result); /* 统一内存释放器 */
}

/**
 * 评估公式节点在特定点的值 (递归)
 * 用于计算方程在特定点的值
 */
static double eval_node(const FormulaNode *node, double x, double y);

/**
 * 评估公式节点在特定点的值
 */
static double eval_node(const FormulaNode *node, double x, double y) {
    if (!node)
        return 0.0;

    switch (node->type) {
        case NODE_NUMBER:
            if (node->data.number.is_integer) {
                return (double) node->data.number.numerator;
            } else {
                if (node->data.number.denominator == 0)
                    return 0.0;
                return (double) node->data.number.numerator / (double) node->data.number.denominator;
            }

        case NODE_VARIABLE: {
            /* 变量名可能是 'x' 或 'y' */
            if (node->data.variable.name) {
                if (strcmp(node->data.variable.name, "x") == 0) {
                    return x;
                } else if (strcmp(node->data.variable.name, "y") == 0) {
                    return y;
                }
            }
            return 0.0;
        }

        case NODE_IDENTIFIER: {
            /* 标识符作为变量处理 */
            if (node->data.identifier.name) {
                if (strcmp(node->data.identifier.name, "x") == 0) {
                    return x;
                } else if (strcmp(node->data.identifier.name, "y") == 0) {
                    return y;
                }
            }
            return 0.0;
        }

        case NODE_BINARY_OP_ADD: {
            double l = eval_node(node->data.binary_op.left, x, y);
            double r = eval_node(node->data.binary_op.right, x, y);
            return l + r;
        }

        case NODE_BINARY_OP_SUB: {
            double l = eval_node(node->data.binary_op.left, x, y);
            double r = eval_node(node->data.binary_op.right, x, y);
            return l - r;
        }

        case NODE_BINARY_OP_MUL: {
            double l = eval_node(node->data.binary_op.left, x, y);
            double r = eval_node(node->data.binary_op.right, x, y);
            return l * r;
        }

        case NODE_BINARY_OP_DIV: {
            double l = eval_node(node->data.binary_op.left, x, y);
            double r = eval_node(node->data.binary_op.right, x, y);
            /* 使用容差检查代替精确零比较，防止次正规数除法溢出 */
            return (fabs(r) > 1e-15) ? l / r : 0.0;
        }

        case NODE_BINARY_OP_POW: {
            double l = eval_node(node->data.binary_op.left, x, y);
            double r = eval_node(node->data.binary_op.right, x, y);
            /* Guard: pow(negative, non-integer) is undefined in reals.
             * Return 0.0 for consistency with the SQRT handling below. */
            if (l < 0.0 && fabs(r - round(r)) > 1e-12)
                return 0.0;
            return pow(l, r);
        }

        case NODE_UNARY_OP_NEG: {
            double v = eval_node(node->data.unary_op.operand, x, y);
            return -v;
        }

        case NODE_UNARY_OP_SQRT: {
            double v = eval_node(node->data.unary_op.operand, x, y);
            return (v >= 0) ? sqrt(v) : 0.0;
        }

        case NODE_UNARY_OP_SIN: {
            double v = eval_node(node->data.unary_op.operand, x, y);
            return sin(v);
        }

        case NODE_UNARY_OP_COS: {
            double v = eval_node(node->data.unary_op.operand, x, y);
            return cos(v);
        }

        case NODE_UNARY_OP_TAN: {
            double v = eval_node(node->data.unary_op.operand, x, y);
            /* tan(x) 在 x ≈ π/2 + nπ 处发散为 HUGE_VAL，使用容差避开奇点 */
            double rem = fmod(v + M_PI_2, M_PI);
            if (fabs(rem) < 1e-12 || fabs(rem - M_PI) < 1e-12) {
                return 0.0;
            }
            return tan(v);
        }

        case NODE_UNARY_OP_ABS: {
            double v = eval_node(node->data.unary_op.operand, x, y);
            return fabs(v);
        }

        case NODE_UNARY_OP_LN: {
            double v = eval_node(node->data.unary_op.operand, x, y);
            return (v > 0) ? log(v) : 0.0;
        }

        case NODE_UNARY_OP_LOG: {
            double v = eval_node(node->data.unary_op.operand, x, y);
            return (v > 0) ? log10(v) : 0.0;
        }

        case NODE_EQUATION: {
            /* 方程: 返回 lhs - rhs 的值 (零值表示在曲线上) */
            double l = eval_node(node->data.equation.lhs, x, y);
            double r = eval_node(node->data.equation.rhs, x, y);
            return l - r;
        }

        case NODE_GEOM_POINT: {
            /* 点: 返回坐标值的组合 (x + y) */
            double px = 0.0, py = 0.0;
            if (node->data.geom_point.coords && node->data.geom_point.coords->type == NODE_COORDINATE_LIST &&
                node->data.geom_point.coords->data.coord_list.coord_count >= 2) {
                px = eval_node(node->data.geom_point.coords->data.coord_list.coords[0], x, y);
                py = eval_node(node->data.geom_point.coords->data.coord_list.coords[1], x, y);
            }
            return px + py;
        }

        case NODE_GEOM_SEGMENT: {
            /* 线段: 返回长度 */
            double x1 = 0.0, y1 = 0.0, x2 = 0.0, y2 = 0.0;
            if (node->data.geom_segment.endpoint1 && node->data.geom_segment.endpoint1->type == NODE_GEOM_POINT) {
                FormulaNode *coords = node->data.geom_segment.endpoint1->data.geom_point.coords;
                if (coords && coords->type == NODE_COORDINATE_LIST && coords->data.coord_list.coord_count >= 2) {
                    x1 = eval_node(coords->data.coord_list.coords[0], x, y);
                    y1 = eval_node(coords->data.coord_list.coords[1], x, y);
                }
            }
            if (node->data.geom_segment.endpoint2 && node->data.geom_segment.endpoint2->type == NODE_GEOM_POINT) {
                FormulaNode *coords = node->data.geom_segment.endpoint2->data.geom_point.coords;
                if (coords && coords->type == NODE_COORDINATE_LIST && coords->data.coord_list.coord_count >= 2) {
                    x2 = eval_node(coords->data.coord_list.coords[0], x, y);
                    y2 = eval_node(coords->data.coord_list.coords[1], x, y);
                }
            }
            return sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
        }

        case NODE_GEOM_CIRCLE: {
            /* 圆: 返回半径 */
            if (node->data.geom_circle.radius) {
                return eval_node(node->data.geom_circle.radius, x, y);
            }
            return 0.0;
        }

        case NODE_GEOM_TRIANGLE: {
            /* 三角形: 返回面积 (使用海伦公式) */
            double x1 = 0.0, y1 = 0.0, x2 = 0.0, y2 = 0.0, x3 = 0.0, y3 = 0.0;
            if (node->data.geom_triangle.vertex1 && node->data.geom_triangle.vertex1->type == NODE_GEOM_POINT) {
                FormulaNode *coords = node->data.geom_triangle.vertex1->data.geom_point.coords;
                if (coords && coords->type == NODE_COORDINATE_LIST && coords->data.coord_list.coord_count >= 2) {
                    x1 = eval_node(coords->data.coord_list.coords[0], x, y);
                    y1 = eval_node(coords->data.coord_list.coords[1], x, y);
                }
            }
            if (node->data.geom_triangle.vertex2 && node->data.geom_triangle.vertex2->type == NODE_GEOM_POINT) {
                FormulaNode *coords = node->data.geom_triangle.vertex2->data.geom_point.coords;
                if (coords && coords->type == NODE_COORDINATE_LIST && coords->data.coord_list.coord_count >= 2) {
                    x2 = eval_node(coords->data.coord_list.coords[0], x, y);
                    y2 = eval_node(coords->data.coord_list.coords[1], x, y);
                }
            }
            if (node->data.geom_triangle.vertex3 && node->data.geom_triangle.vertex3->type == NODE_GEOM_POINT) {
                FormulaNode *coords = node->data.geom_triangle.vertex3->data.geom_point.coords;
                if (coords && coords->type == NODE_COORDINATE_LIST && coords->data.coord_list.coord_count >= 2) {
                    x3 = eval_node(coords->data.coord_list.coords[0], x, y);
                    y3 = eval_node(coords->data.coord_list.coords[1], x, y);
                }
            }
            /* 使用叉积公式计算面积: 0.5 * |x1(y2-y3) + x2(y3-y1) + x3(y1-y2)| */
            return 0.5 * fabs(x1 * (y2 - y3) + x2 * (y3 - y1) + x3 * (y1 - y2));
        }

        case NODE_GEOM_POLYGON: {
            /* 多边形: 返回面积 (使用鞋带公式) */
            double area = 0.0;
            int n = node->data.geom_polygon.vertex_count;
            if (n < 3)
                return 0.0;
            for (int i = 0; i < n; i++) {
                FormulaNode *vi = node->data.geom_polygon.vertices[i];
                FormulaNode *vj = node->data.geom_polygon.vertices[(i + 1) % n];
                double xi = 0.0, yi = 0.0, xj = 0.0, yj = 0.0;
                if (vi && vi->type == NODE_GEOM_POINT) {
                    FormulaNode *coords = vi->data.geom_point.coords;
                    if (coords && coords->type == NODE_COORDINATE_LIST && coords->data.coord_list.coord_count >= 2) {
                        xi = eval_node(coords->data.coord_list.coords[0], x, y);
                        yi = eval_node(coords->data.coord_list.coords[1], x, y);
                    }
                }
                if (vj && vj->type == NODE_GEOM_POINT) {
                    FormulaNode *coords = vj->data.geom_point.coords;
                    if (coords && coords->type == NODE_COORDINATE_LIST && coords->data.coord_list.coord_count >= 2) {
                        xj = eval_node(coords->data.coord_list.coords[0], x, y);
                        yj = eval_node(coords->data.coord_list.coords[1], x, y);
                    }
                }
                area += xi * yj - xj * yi;
            }
            return 0.5 * fabs(area);
        }

        case NODE_GEOM_REGION: {
            /* 区域: 返回边界总长度 */
            double perimeter = 0.0;
            int n = node->data.geom_region.segment_count;
            for (int i = 0; i < n; i++) {
                FormulaNode *seg = node->data.geom_region.boundary_segments[i];
                if (seg) {
                    perimeter += eval_node(seg, x, y);
                }
            }
            return perimeter;
        }

        case NODE_GEOM_ARC: {
            /* 弧: 返回弧长 = r * |theta2 - theta1| */
            double r = 0.0, theta1 = 0.0, theta2 = 0.0;
            if (node->data.geom_arc.radius) {
                r = eval_node(node->data.geom_arc.radius, x, y);
            }
            if (node->data.geom_arc.start_angle) {
                theta1 = eval_node(node->data.geom_arc.start_angle, x, y);
            }
            if (node->data.geom_arc.end_angle) {
                theta2 = eval_node(node->data.geom_arc.end_angle, x, y);
            }
            return r * fabs(theta2 - theta1);
        }

        default:
            return 0.0;
    }
}

/**
 * 将公式节点渲染为字符串（简化版）
 */
static void node_to_string(const FormulaNode *node, char *buf, size_t buf_size) {
    if (!node || !buf || buf_size == 0)
        return;

    buf[0] = '\0';

    switch (node->type) {
        case NODE_NUMBER:
            if (node->data.number.is_integer) {
                snprintf(buf, buf_size, "%lld", (long long) node->data.number.numerator);
            } else {
                snprintf(buf, buf_size, "%lld/%llu", (long long) node->data.number.numerator,
                         (unsigned long long) node->data.number.denominator);
            }
            break;

        case NODE_VARIABLE:
            if (node->data.variable.name) {
                /* 使用 lv_strlcpy 替代不安全的 strncpy */
                lv_strlcpy(buf, node->data.variable.name, buf_size);
            }
            break;

        case NODE_IDENTIFIER:
            if (node->data.identifier.name) {
                /* 使用 lv_strlcpy 替代不安全的 strncpy */
                lv_strlcpy(buf, node->data.identifier.name, buf_size);
            }
            break;

        case NODE_BINARY_OP_ADD: {
            char left[FORMULA_EXPR_BUF_SIZE], right[FORMULA_EXPR_BUF_SIZE];
            node_to_string(node->data.binary_op.left, left, sizeof(left));
            node_to_string(node->data.binary_op.right, right, sizeof(right));
            int n = snprintf(buf, buf_size, "(%s + %s)", left, right);
            if (n < 0 || (size_t) n >= buf_size) {
                /* 缓冲区不足时使用安全截断标记 */
                lv_strlcpy(buf, "(... + ...)", buf_size);
            }
            break;
        }

        case NODE_BINARY_OP_SUB: {
            char left[FORMULA_EXPR_BUF_SIZE], right[FORMULA_EXPR_BUF_SIZE];
            node_to_string(node->data.binary_op.left, left, sizeof(left));
            node_to_string(node->data.binary_op.right, right, sizeof(right));
            int n = snprintf(buf, buf_size, "(%s - %s)", left, right);
            if (n < 0 || (size_t) n >= buf_size) {
                lv_strlcpy(buf, "(... - ...)", buf_size);
            }
            break;
        }

        case NODE_BINARY_OP_MUL: {
            char left[FORMULA_EXPR_BUF_SIZE], right[FORMULA_EXPR_BUF_SIZE];
            node_to_string(node->data.binary_op.left, left, sizeof(left));
            node_to_string(node->data.binary_op.right, right, sizeof(right));
            int n = snprintf(buf, buf_size, "(%s * %s)", left, right);
            if (n < 0 || (size_t) n >= buf_size) {
                lv_strlcpy(buf, "(... * ...)", buf_size);
            }
            break;
        }

        case NODE_BINARY_OP_DIV: {
            char left[FORMULA_EXPR_BUF_SIZE], right[FORMULA_EXPR_BUF_SIZE];
            node_to_string(node->data.binary_op.left, left, sizeof(left));
            node_to_string(node->data.binary_op.right, right, sizeof(right));
            int n = snprintf(buf, buf_size, "(%s / %s)", left, right);
            if (n < 0 || (size_t) n >= buf_size) {
                lv_strlcpy(buf, "(... / ...)", buf_size);
            }
            break;
        }

        case NODE_BINARY_OP_POW: {
            char left[FORMULA_EXPR_BUF_SIZE], right[FORMULA_EXPR_BUF_SIZE];
            node_to_string(node->data.binary_op.left, left, sizeof(left));
            node_to_string(node->data.binary_op.right, right, sizeof(right));
            int n = snprintf(buf, buf_size, "(%s ^ %s)", left, right);
            if (n < 0 || (size_t) n >= buf_size) {
                lv_strlcpy(buf, "(... ^ ...)", buf_size);
            }
            break;
        }

        case NODE_EQUATION: {
            char left[FORMULA_EXPR_BUF_SIZE], right[FORMULA_EXPR_BUF_SIZE];
            node_to_string(node->data.equation.lhs, left, sizeof(left));
            node_to_string(node->data.equation.rhs, right, sizeof(right));
            int n = snprintf(buf, buf_size, "(%s = %s)", left, right);
            if (n < 0 || (size_t) n >= buf_size) {
                lv_strlcpy(buf, "(... = ...)", buf_size);
            }
            break;
        }

        case NODE_GEOM_POINT: {
            const char *name = node->data.geom_point.name ? node->data.geom_point.name : "?";
            int n = snprintf(buf, buf_size, "point(%s)", name);
            if (n < 0 || (size_t) n >= buf_size) {
                lv_strlcpy(buf, "point(...)", buf_size);
            }
            break;
        }

        case NODE_GEOM_SEGMENT: {
            const char *name = node->data.geom_segment.name ? node->data.geom_segment.name : "?";
            int n = snprintf(buf, buf_size, "segment(%s)", name);
            if (n < 0 || (size_t) n >= buf_size) {
                lv_strlcpy(buf, "segment(...)", buf_size);
            }
            break;
        }

        case NODE_GEOM_CIRCLE: {
            const char *name = node->data.geom_circle.name ? node->data.geom_circle.name : "?";
            char r[FORMULA_RESULT_BUF_SIZE] = "?";
            if (node->data.geom_circle.radius) {
                node_to_string(node->data.geom_circle.radius, r, sizeof(r));
            }
            int n = snprintf(buf, buf_size, "circle(%s, r=%s)", name, r);
            if (n < 0 || (size_t) n >= buf_size) {
                lv_strlcpy(buf, "circle(...)", buf_size);
            }
            break;
        }

        case NODE_GEOM_TRIANGLE: {
            const char *name = node->data.geom_triangle.name ? node->data.geom_triangle.name : "?";
            int n = snprintf(buf, buf_size, "triangle(%s)", name);
            if (n < 0 || (size_t) n >= buf_size) {
                lv_strlcpy(buf, "triangle(...)", buf_size);
            }
            break;
        }

        case NODE_GEOM_POLYGON: {
            const char *name = node->data.geom_polygon.name ? node->data.geom_polygon.name : "?";
            int n = snprintf(buf, buf_size, "polygon(%s, %d vertices)", name, node->data.geom_polygon.vertex_count);
            if (n < 0 || (size_t) n >= buf_size) {
                lv_strlcpy(buf, "polygon(...)", buf_size);
            }
            break;
        }

        case NODE_GEOM_REGION: {
            const char *name = node->data.geom_region.name ? node->data.geom_region.name : "?";
            int n = snprintf(buf, buf_size, "region(%s, %d segments)", name, node->data.geom_region.segment_count);
            if (n < 0 || (size_t) n >= buf_size) {
                lv_strlcpy(buf, "region(...)", buf_size);
            }
            break;
        }

        case NODE_GEOM_ARC: {
            const char *name = node->data.geom_arc.name ? node->data.geom_arc.name : "?";
            char r[FORMULA_RESULT_BUF_SIZE] = "?", t1[FORMULA_RESULT_BUF_SIZE] = "?", t2[FORMULA_RESULT_BUF_SIZE] = "?";
            if (node->data.geom_arc.radius) {
                node_to_string(node->data.geom_arc.radius, r, sizeof(r));
            }
            if (node->data.geom_arc.start_angle) {
                node_to_string(node->data.geom_arc.start_angle, t1, sizeof(t1));
            }
            if (node->data.geom_arc.end_angle) {
                node_to_string(node->data.geom_arc.end_angle, t2, sizeof(t2));
            }
            int n = snprintf(buf, buf_size, "arc(%s, r=%s, %s, %s)", name, r, t1, t2);
            if (n < 0 || (size_t) n >= buf_size) {
                lv_strlcpy(buf, "arc(...)", buf_size);
            }
            break;
        }

        default:
            /* 使用 lv_strlcpy 替代不安全的 strncpy */
            lv_strlcpy(buf, "?", buf_size);
            break;
    }
}

/**
 * 使用行进正方形算法（Marching Squares）采样隐式曲线
 * 这是计算机图形学中用于提取等值线的标准算法
 */
EquationCurveResult *formula_convert_equation_to_curve(const FormulaNode *equation_node, int sample_count, double x_min,
                                                       double x_max, double y_min, double y_max) {
    EquationCurveResult *result =
        (EquationCurveResult *) lv_calloc(1, sizeof(EquationCurveResult)); /* 统一内存分配器 */
    if (!result) {
        return NULL;
    }

    /* 参数验证 */
    if (!equation_node || sample_count <= 0) {
        result->success = false;
        snprintf(result->error_message, sizeof(result->error_message), "Invalid parameters");
        return result;
    }

    /* 分配采样点数组 */
    result->points = (CurveSamplePoint *) lv_calloc(sample_count, sizeof(CurveSamplePoint)); /* 统一内存分配器 */
    if (!result->points) {
        result->success = false;
        snprintf(result->error_message, sizeof(result->error_message), "Memory allocation failed");
        return result;
    }

    /* 生成方程字符串表示 */
    node_to_string(equation_node, result->equation_str, sizeof(result->equation_str));

    /* 设置边界框 */
    result->bbox_min_x = x_min;
    result->bbox_min_y = y_min;
    result->bbox_max_x = x_max;
    result->bbox_max_y = y_max;

    /*
     * 使用自适应网格采样策略：
     * 1. 首先在整个区域进行粗采样
     * 2. 识别可能包含曲线的区域（函数值变号或接近零）
     * 3. 在这些区域进行精细采样
     */

    int grid_size = (int) sqrt((double) sample_count * 2);
    if (grid_size < 10)
        grid_size = 10;

    double dx = (x_max - x_min) / grid_size;
    double dy = (y_max - y_min) / grid_size;

    /* 第一阶段：粗采样，计算网格点上的函数值 */
    double *grid_values = (double *) lv_calloc((grid_size + 1) * (grid_size + 1), sizeof(double)); /* 统一内存分配器 */
    if (!grid_values) {
        result->success = false;
        snprintf(result->error_message, sizeof(result->error_message), "Memory allocation failed for grid");
        return result;
    }

    for (int i = 0; i <= grid_size; i++) {
        for (int j = 0; j <= grid_size; j++) {
            double x = x_min + i * dx;
            double y = y_min + j * dy;
            grid_values[i * (grid_size + 1) + j] = eval_node(equation_node, x, y);
        }
    }

    /* 第二阶段：识别等值线穿越的边并插值 */
    int point_idx = 0;
    double threshold = 0.1; /* 接近零的阈值 */

    for (int i = 0; i < grid_size && point_idx < sample_count; i++) {
        for (int j = 0; j < grid_size && point_idx < sample_count; j++) {
            /* 获取当前单元格的四个角点值 */
            double v00 = grid_values[i * (grid_size + 1) + j];
            double v10 = grid_values[(i + 1) * (grid_size + 1) + j];
            double v01 = grid_values[i * (grid_size + 1) + (j + 1)];
            double v11 = grid_values[(i + 1) * (grid_size + 1) + (j + 1)];

            /* 检查是否穿越等值线（变号） */
            bool crosses = (v00 * v10 < 0) || (v00 * v01 < 0) || (v10 * v11 < 0) || (v01 * v11 < 0);

            /* 或者值接近零 */
            bool near_zero = (fabs(v00) < threshold) || (fabs(v10) < threshold) || (fabs(v01) < threshold) ||
                             (fabs(v11) < threshold);

            if (crosses || near_zero) {
                /* 在单元格中心添加一个采样点 */
                double cx = x_min + (i + 0.5) * dx;
                double cy = y_min + (j + 0.5) * dy;

                /* 使用牛顿迭代法细化到曲线上 */
                double x = cx, y = cy;
                double f = eval_node(equation_node, x, y);

                /* 简单的梯度下降细化 */
                for (int iter = 0; iter < 10 && fabs(f) > 1e-6; iter++) {
                    double h = 1e-6;
                    double fx = (eval_node(equation_node, x + h, y) - f) / h;
                    double fy = (eval_node(equation_node, x, y + h) - f) / h;

                    double grad_sq = fx * fx + fy * fy;
                    if (grad_sq < 1e-12)
                        break;

                    x -= f * fx / grad_sq;
                    y -= f * fy / grad_sq;
                    f = eval_node(equation_node, x, y);
                }

                result->points[point_idx].x = x;
                result->points[point_idx].y = y;
                result->points[point_idx].is_valid = true;
                point_idx++;
            }
        }
    }

    lv_free((void **) &grid_values); /* 统一内存释放器 */

    result->point_count = point_idx;
    result->success = (point_idx > 0);

    if (!result->success) {
        snprintf(result->error_message, sizeof(result->error_message), "No curve points found in the specified region");
    }

    return result;
}

/**
 * @brief 辅助函数：将 AST 子树扁平化为多项式系数数组
 *
 * 将表达式 F(x,y) 展开为二维多项式，系数按字典序存储：
 *   coeffs[i] 对应 x^((i/MAX_DEG)%MAX_DEG) * y^(i%MAX_DEG)
 * 其中 MAX_DEG 为每维最大次数。
 *
 * @param[in]  node   AST 子树根节点
 * @param[out] coeffs 输出系数数组（调用者分配，至少 coeffs_size 个 double）
 * @param[in]  coeffs_size 系数数组大小
 * @param[in]  max_deg 每维最大次数
 * @return 成功返回 true，失败返回 false
 */
#define IMPLICIT_MAX_DEG 4
#define IMPLICIT_COEFFS_SIZE (IMPLICIT_MAX_DEG * IMPLICIT_MAX_DEG)

static bool flatten_to_polynomial(const FormulaNode *node, double *coeffs, int coeffs_size, int max_deg) {
    if (!node)
        return false;

    memset(coeffs, 0, sizeof(double) * coeffs_size);

    /*
     * 递归求值辅助：将 AST 节点视为多项式并累加到 coeffs 中。
     * 使用简单的递归下降方法处理 +, -, *, ^ 等运算。
     *
     * 返回值含义：
     *   0 = 成功
     *  -1 = 遇到无法识别的节点类型
     */
    /* 使用栈式迭代避免深层递归 */
    /* 这里采用递归实现，方程深度通常有限 */

    /* 辅助：将一个单项式 c * x^a * y^b 累加到 coeffs */
    /* 辅助：从 AST 节点提取多项式系数 */

    /*
     * 简化的多项式提取：
     * 只处理常见模式：
     *   - 数字常量 -> c * x^0 * y^0
     *   - 变量 x -> 1 * x^1 * y^0
     *   - 变量 y -> 1 * x^0 * y^1
     *   - a + b -> 合并
     *   - a - b -> 合并
     *   - a * b -> 卷积
     *   - a^n -> 重复卷积
     */

    /* 临时缓冲区用于中间计算 */
    double tmp[IMPLICIT_COEFFS_SIZE];
    double lhs[IMPLICIT_COEFFS_SIZE];
    double rhs[IMPLICIT_COEFFS_SIZE];

    switch (node->type) {
        case NODE_NUMBER: {
            double val;
            if (node->data.number.is_integer) {
                val = (double) node->data.number.numerator;
            } else {
                if (node->data.number.denominator == 0)
                    return false;
                val = (double) node->data.number.numerator / (double) node->data.number.denominator;
            }
            coeffs[0] = val;
            return true;
        }

        case NODE_VARIABLE: {
            if (node->data.variable.name) {
                if (strcmp(node->data.variable.name, "x") == 0) {
                    if (max_deg >= 1)
                        coeffs[1 * max_deg + 0] = 1.0; /* x^1 * y^0 */
                    return true;
                } else if (strcmp(node->data.variable.name, "y") == 0) {
                    if (max_deg >= 1)
                        coeffs[0 * max_deg + 1] = 1.0; /* x^0 * y^1 */
                    return true;
                }
            }
            /* 未知变量视为常量 0（或可扩展为参数） */
            return false;
        }

        case NODE_BINARY_OP_ADD: {
            if (!flatten_to_polynomial(node->data.binary_op.left, lhs, coeffs_size, max_deg))
                return false;
            if (!flatten_to_polynomial(node->data.binary_op.right, rhs, coeffs_size, max_deg))
                return false;
            for (int i = 0; i < coeffs_size; i++)
                coeffs[i] = lhs[i] + rhs[i];
            return true;
        }

        case NODE_BINARY_OP_SUB: {
            if (!flatten_to_polynomial(node->data.binary_op.left, lhs, coeffs_size, max_deg))
                return false;
            if (!flatten_to_polynomial(node->data.binary_op.right, rhs, coeffs_size, max_deg))
                return false;
            for (int i = 0; i < coeffs_size; i++)
                coeffs[i] = lhs[i] - rhs[i];
            return true;
        }

        case NODE_BINARY_OP_MUL: {
            if (!flatten_to_polynomial(node->data.binary_op.left, lhs, coeffs_size, max_deg))
                return false;
            if (!flatten_to_polynomial(node->data.binary_op.right, rhs, coeffs_size, max_deg))
                return false;
            /* 多项式乘法（卷积），截断到 max_deg */
            memset(coeffs, 0, sizeof(double) * coeffs_size);
            for (int i = 0; i < max_deg; i++) {
                for (int j = 0; j < max_deg; j++) {
                    int ai = i * max_deg + j;
                    for (int k = 0; k < max_deg; k++) {
                        for (int l = 0; l < max_deg; l++) {
                            int bi = k * max_deg + l;
                            int ci = (i + k) * max_deg + (j + l);
                            if (i + k < max_deg && j + l < max_deg) {
                                coeffs[ci] += lhs[ai] * rhs[bi];
                            }
                        }
                    }
                }
            }
            return true;
        }

        case NODE_BINARY_OP_POW: {
            if (!flatten_to_polynomial(node->data.binary_op.left, lhs, coeffs_size, max_deg))
                return false;
            /* 指数必须为非负整数 */
            int exp_val = 0;
            if (node->data.binary_op.right && node->data.binary_op.right->type == NODE_NUMBER) {
                if (node->data.binary_op.right->data.number.is_integer) {
                    exp_val = (int) node->data.binary_op.right->data.number.numerator;
                }
            }
            if (exp_val < 0)
                return false;

            /* 初始化结果为 1（即 x^0*y^0 = 1） */
            memset(coeffs, 0, sizeof(double) * coeffs_size);
            coeffs[0] = 1.0;

            for (int e = 0; e < exp_val; e++) {
                memset(tmp, 0, sizeof(double) * coeffs_size);
                for (int i = 0; i < max_deg; i++) {
                    for (int j = 0; j < max_deg; j++) {
                        int ai = i * max_deg + j;
                        for (int k = 0; k < max_deg; k++) {
                            for (int l = 0; l < max_deg; l++) {
                                int bi = k * max_deg + l;
                                int ci = (i + k) * max_deg + (j + l);
                                if (i + k < max_deg && j + l < max_deg) {
                                    tmp[ci] += coeffs[ai] * lhs[bi];
                                }
                            }
                        }
                    }
                }
                memcpy(coeffs, tmp, sizeof(double) * coeffs_size);
            }
            return true;
        }

        case NODE_UNARY_OP_NEG: {
            if (!flatten_to_polynomial(node->data.unary_op.operand, coeffs, coeffs_size, max_deg))
                return false;
            for (int i = 0; i < coeffs_size; i++)
                coeffs[i] = -coeffs[i];
            return true;
        }

        case NODE_UNARY_OP_SIN: {
            /* 泰勒展开 sin(x) ≈ x - x^3/3! + x^5/5!
         * 先递归展开操作数为多项式 P(x)，然后计算 P - P^3/6 + P^5/120
         * 注意：这是近似多项式，仅在小范围 |x| < pi/2 内有效 */
            if (!flatten_to_polynomial(node->data.unary_op.operand, lhs, coeffs_size, max_deg))
                return false;
            /* coeffs = P(x) */
            memcpy(coeffs, lhs, sizeof(double) * coeffs_size);

            /* 计算 P^3 / 6 */
            memset(rhs, 0, sizeof(double) * coeffs_size);
            /* P^2 = P * P */
            memset(tmp, 0, sizeof(double) * coeffs_size);
            for (int i = 0; i < max_deg; i++) {
                for (int j = 0; j < max_deg; j++) {
                    int ai = i * max_deg + j;
                    for (int k = 0; k < max_deg; k++) {
                        for (int l = 0; l < max_deg; l++) {
                            int bi = k * max_deg + l;
                            int ci = (i + k) * max_deg + (j + l);
                            if (i + k < max_deg && j + l < max_deg) {
                                tmp[ci] += lhs[ai] * lhs[bi];
                            }
                        }
                    }
                }
            }
            /* P^3 = P^2 * P */
            double p3[IMPLICIT_COEFFS_SIZE];
            memset(p3, 0, sizeof(double) * coeffs_size);
            for (int i = 0; i < max_deg; i++) {
                for (int j = 0; j < max_deg; j++) {
                    int ai = i * max_deg + j;
                    for (int k = 0; k < max_deg; k++) {
                        for (int l = 0; l < max_deg; l++) {
                            int bi = k * max_deg + l;
                            int ci = (i + k) * max_deg + (j + l);
                            if (i + k < max_deg && j + l < max_deg) {
                                p3[ci] += tmp[ai] * lhs[bi];
                            }
                        }
                    }
                }
            }
            /* 保存 P^3 的副本用于后续 P^5 计算 */
            double p3_raw[IMPLICIT_COEFFS_SIZE];
            memcpy(p3_raw, p3, sizeof(double) * coeffs_size);
            /* P^3 / 6 */
            for (int i = 0; i < coeffs_size; i++)
                p3[i] /= 6.0;
            /* coeffs = P - P^3/6 */
            for (int i = 0; i < coeffs_size; i++)
                coeffs[i] -= p3[i];

            /* 计算 P^5 / 120 */
            /* P^4 = P^3 * P (使用未除以6的 P^3) */
            double p4[IMPLICIT_COEFFS_SIZE];
            memset(p4, 0, sizeof(double) * coeffs_size);
            for (int i = 0; i < max_deg; i++) {
                for (int j = 0; j < max_deg; j++) {
                    int ai = i * max_deg + j;
                    for (int k = 0; k < max_deg; k++) {
                        for (int l = 0; l < max_deg; l++) {
                            int bi = k * max_deg + l;
                            int ci = (i + k) * max_deg + (j + l);
                            if (i + k < max_deg && j + l < max_deg) {
                                p4[ci] += p3_raw[ai] * lhs[bi];
                            }
                        }
                    }
                }
            }
            /* P^5 = P^4 * P */
            double p5[IMPLICIT_COEFFS_SIZE];
            memset(p5, 0, sizeof(double) * coeffs_size);
            for (int i = 0; i < max_deg; i++) {
                for (int j = 0; j < max_deg; j++) {
                    int ai = i * max_deg + j;
                    for (int k = 0; k < max_deg; k++) {
                        for (int l = 0; l < max_deg; l++) {
                            int bi = k * max_deg + l;
                            int ci = (i + k) * max_deg + (j + l);
                            if (i + k < max_deg && j + l < max_deg) {
                                p5[ci] += p4[ai] * lhs[bi];
                            }
                        }
                    }
                }
            }
            /* P^5 / 120 */
            for (int i = 0; i < coeffs_size; i++)
                p5[i] /= 120.0;
            /* coeffs = P - P^3/6 + P^5/120 */
            for (int i = 0; i < coeffs_size; i++)
                coeffs[i] += p5[i];

            return true;
        }

        case NODE_UNARY_OP_COS: {
            /* 泰勒展开 cos(x) ≈ 1 - x^2/2! + x^4/4!
         * 先递归展开操作数为多项式 P(x)，然后计算 1 - P^2/2 + P^4/24 */
            if (!flatten_to_polynomial(node->data.unary_op.operand, lhs, coeffs_size, max_deg))
                return false;
            /* coeffs = 1 (常数项) */
            memset(coeffs, 0, sizeof(double) * coeffs_size);
            coeffs[0] = 1.0;

            /* 计算 P^2 / 2 */
            memset(tmp, 0, sizeof(double) * coeffs_size);
            for (int i = 0; i < max_deg; i++) {
                for (int j = 0; j < max_deg; j++) {
                    int ai = i * max_deg + j;
                    for (int k = 0; k < max_deg; k++) {
                        for (int l = 0; l < max_deg; l++) {
                            int bi = k * max_deg + l;
                            int ci = (i + k) * max_deg + (j + l);
                            if (i + k < max_deg && j + l < max_deg) {
                                tmp[ci] += lhs[ai] * lhs[bi];
                            }
                        }
                    }
                }
            }
            /* coeffs = 1 - P^2/2 */
            for (int i = 0; i < coeffs_size; i++)
                coeffs[i] -= tmp[i] / 2.0;

            /* 计算 P^4 / 24 */
            /* P^3 = P^2 * P */
            double p3[IMPLICIT_COEFFS_SIZE];
            memset(p3, 0, sizeof(double) * coeffs_size);
            for (int i = 0; i < max_deg; i++) {
                for (int j = 0; j < max_deg; j++) {
                    int ai = i * max_deg + j;
                    for (int k = 0; k < max_deg; k++) {
                        for (int l = 0; l < max_deg; l++) {
                            int bi = k * max_deg + l;
                            int ci = (i + k) * max_deg + (j + l);
                            if (i + k < max_deg && j + l < max_deg) {
                                p3[ci] += tmp[ai] * lhs[bi];
                            }
                        }
                    }
                }
            }
            /* P^4 = P^3 * P */
            double p4[IMPLICIT_COEFFS_SIZE];
            memset(p4, 0, sizeof(double) * coeffs_size);
            for (int i = 0; i < max_deg; i++) {
                for (int j = 0; j < max_deg; j++) {
                    int ai = i * max_deg + j;
                    for (int k = 0; k < max_deg; k++) {
                        for (int l = 0; l < max_deg; l++) {
                            int bi = k * max_deg + l;
                            int ci = (i + k) * max_deg + (j + l);
                            if (i + k < max_deg && j + l < max_deg) {
                                p4[ci] += p3[ai] * lhs[bi];
                            }
                        }
                    }
                }
            }
            /* coeffs = 1 - P^2/2 + P^4/24 */
            for (int i = 0; i < coeffs_size; i++)
                coeffs[i] += p4[i] / 24.0;

            return true;
        }

        case NODE_UNARY_OP_SQRT: {
            /* 如果参数是常数，直接计算平方根 */
            const FormulaNode *operand = node->data.unary_op.operand;
            if (operand && operand->type == NODE_NUMBER) {
                double val;
                if (operand->data.number.is_integer) {
                    val = (double) operand->data.number.numerator;
                } else {
                    if (operand->data.number.denominator == 0)
                        return false;
                    val = (double) operand->data.number.numerator / (double) operand->data.number.denominator;
                }
                if (val >= 0.0) {
                    memset(coeffs, 0, sizeof(double) * coeffs_size);
                    coeffs[0] = sqrt(val);
                    return true;
                }
            }
            /* 非常数参数的 sqrt 不支持多项式展开 */
            return false;
        }

        default:
            return false;
    }
}

/**
 * @brief 辅助函数：尝试识别圆方程 (x-a)^2 + (y-b)^2 = r^2
 *
 * 将 F(x,y) = lhs - rhs 展开为多项式，检查是否符合圆方程模式。
 * 圆方程展开后：x^2 - 2ax + a^2 + y^2 - 2by + b^2 - r^2 = 0
 * 即：x^2 + y^2 + Dx + Ey + F = 0，其中 x^2 和 y^2 系数相同且无 xy 项。
 *
 * @param[in]  coeffs 多项式系数数组
 * @param[out] cx     圆心 x 坐标
 * @param[out] cy     圆心 y 坐标
 * @param[out] r      半径
 * @return 识别为圆返回 true，否则返回 false
 */
static bool identify_circle(const double *coeffs, double *cx, double *cy, double *r) {
    /*
     * 系数索引映射（max_deg=4）：
     *   coeffs[x*4+y] 对应 x^x * y^y
     *   coeffs[0] = 常数项
     *   coeffs[4] = x 系数
     *   coeffs[1] = y 系数
     *   coeffs[8] = x^2 系数
     *   coeffs[2] = y^2 系数
     *   coeffs[5] = xy 系数
     */
    double c_xy = coeffs[1 * IMPLICIT_MAX_DEG + 1]; /* xy 项 */
    double c_x2 = coeffs[2 * IMPLICIT_MAX_DEG + 0]; /* x^2 项 */
    double c_y2 = coeffs[0 * IMPLICIT_MAX_DEG + 2]; /* y^2 项 */
    double c_x = coeffs[1 * IMPLICIT_MAX_DEG + 0];  /* x 项 */
    double c_y = coeffs[0 * IMPLICIT_MAX_DEG + 1];  /* y 项 */
    double c_0 = coeffs[0 * IMPLICIT_MAX_DEG + 0];  /* 常数项 */

    /* 检查：x^2 和 y^2 系数相同且非零，xy 系数为零 */
    if (fabs(c_x2 - c_y2) > 1e-9 || fabs(c_x2) < 1e-9 || fabs(c_xy) > 1e-9) {
        return false;
    }

    /* 从 x^2 + y^2 + Dx + Ey + F = 0 提取参数 */
    /* 圆心：(-D/2, -E/2)，半径^2 = D^2/4 + E^2/4 - F */
    double D = c_x / c_x2;
    double E = c_y / c_x2;
    double F = c_0 / c_x2;

    *cx = -D / 2.0;
    *cy = -E / 2.0;
    double r_sq = (D * D + E * E) / 4.0 - F;

    if (r_sq < 0)
        return false; /* 半径为虚数，不是有效的圆 */
    *r = sqrt(r_sq);
    return true;
}

/**
 * @brief 辅助函数：尝试识别直线方程 Ax + By + C = 0
 *
 * 检查多项式是否为一次方程（无 x^2, y^2, xy 等高次项）。
 *
 * @param[in]  coeffs 多项式系数数组
 * @param[out] a      x 系数
 * @param[out] b      y 系数
 * @param[out] c      常数项
 * @return 识别为直线返回 true，否则返回 false
 */
static bool identify_line(const double *coeffs, double *a, double *b, double *c) {
    /* 检查所有二次及以上项是否为零 */
    for (int i = 0; i < IMPLICIT_COEFFS_SIZE; i++) {
        int deg_x = i / IMPLICIT_MAX_DEG;
        int deg_y = i % IMPLICIT_MAX_DEG;
        if (deg_x + deg_y >= 2 && fabs(coeffs[i]) > 1e-9) {
            return false;
        }
    }

    *a = coeffs[1 * IMPLICIT_MAX_DEG + 0]; /* x 系数 */
    *b = coeffs[0 * IMPLICIT_MAX_DEG + 1]; /* y 系数 */
    *c = coeffs[0 * IMPLICIT_MAX_DEG + 0]; /* 常数项 */

    /* a 和 b 不能同时为零 */
    if (fabs(*a) < 1e-9 && fabs(*b) < 1e-9) {
        return false;
    }

    return true;
}

/**
 * 将代数方程节点添加到约束图
 * 创建隐式曲线表示
 */
bool formula_convert_equation(const FormulaNode *equation_node, ConstraintGraph *graph, int *out_node_id) {
    if (!equation_node || !graph || !out_node_id) {
        return false;
    }

    if (equation_node->type != NODE_EQUATION) {
        return false;
    }

    /*
     * 当前实现：将代数方程作为特殊的几何节点添加到图中
     * 这种节点类型可以用于后续求解和渲染
     *
     * 注意：这里我们创建一个表示隐式曲线的节点
     * 实际的几何意义是满足方程 F(x,y) = 0 的所有点集
     */

    /* 提取方程的左右两边 */
    const FormulaNode *lhs = equation_node->data.equation.lhs;
    const FormulaNode *rhs = equation_node->data.equation.rhs;

    if (!lhs) {
        return false;
    }

    /*
     * 对于简单情况，尝试识别曲线类型：
     * - 圆: (x-a)^2 + (y-b)^2 = r^2
     * - 直线: ax + by = c
     * - 抛物线: y = ax^2 + bx + c
     * - 椭圆: (x/a)^2 + (y/b)^2 = 1
     */

    /* 尝试将方程展开为多项式 F(x,y) = lhs - rhs = 0 */
    double coeffs[IMPLICIT_COEFFS_SIZE];
    bool poly_ok = false;

    if (rhs) {
        /* 计算 F = lhs - rhs */
        double lhs_coeffs[IMPLICIT_COEFFS_SIZE];
        double rhs_coeffs[IMPLICIT_COEFFS_SIZE];
        if (flatten_to_polynomial(lhs, lhs_coeffs, IMPLICIT_COEFFS_SIZE, IMPLICIT_MAX_DEG) &&
            flatten_to_polynomial(rhs, rhs_coeffs, IMPLICIT_COEFFS_SIZE, IMPLICIT_MAX_DEG)) {
            for (int i = 0; i < IMPLICIT_COEFFS_SIZE; i++) {
                coeffs[i] = lhs_coeffs[i] - rhs_coeffs[i];
            }
            poly_ok = true;
        }
    } else {
        /* F = lhs = 0 */
        poly_ok = flatten_to_polynomial(lhs, coeffs, IMPLICIT_COEFFS_SIZE, IMPLICIT_MAX_DEG);
    }

    if (poly_ok) {
        /* 尝试识别为圆方程 */
        double cx, cy, r;
        if (identify_circle(coeffs, &cx, &cy, &r)) {
            /* 创建圆心点 (cx, cy) */
            SymbolicCoord *center_coords[2];
            center_coords[0] = symbolic_coord_create_rational((int64_t) (cx * 1000), 1000);
            center_coords[1] = symbolic_coord_create_rational((int64_t) (cy * 1000), 1000);

            AddNodeResult add_result = graph_add_point(graph, center_coords, 2);
            symbolic_coord_destroy(center_coords[0]);
            symbolic_coord_destroy(center_coords[1]);

            if (add_result != ADD_NODE_OK) {
                return false;
            }

            *out_node_id = graph->next_node_id - 1;

            /* 创建圆周上的一个点表示半径 */
            SymbolicCoord *radius_coords[2];
            radius_coords[0] = symbolic_coord_create_rational((int64_t) ((cx + r) * 1000), 1000);
            radius_coords[1] = symbolic_coord_create_rational((int64_t) (cy * 1000), 1000);

            add_result = graph_add_point(graph, radius_coords, 2);
            symbolic_coord_destroy(radius_coords[0]);
            symbolic_coord_destroy(radius_coords[1]);

            if (add_result == ADD_NODE_OK) {
                int radius_pt_id = graph->next_node_id - 1;
                graph_add_line_segment(graph, *out_node_id, radius_pt_id);
            }

            /* 标记为圆类型 */
            GeomNode *node = graph_get_node(graph, *out_node_id);
            if (node && node->numeric_assumption_declaration) {
                lv_free((void **) &node->numeric_assumption_declaration); /* 统一内存释放器 */
                node->numeric_assumption_declaration = NULL;
            }
            if (node) {
                char buf[FORMULA_BUF_SIZE];
                int n = snprintf(buf, sizeof(buf), "IMPLICIT_CURVE:CIRCLE:%.6f:%.6f:%.6f", cx, cy, r);
                /* 检查snprintf返回值：尺寸安全（CIRCLE格式最大约60字节），但防御性检查不可省略 */
                if (n < 0 || (size_t) n >= sizeof(buf)) {
                    buf[sizeof(buf) - 1] = '\0'; /* 确保零终止 */
                }
                node->numeric_assumption_declaration = lv_strdup_safe(buf);
            }

            return true;
        }

        /* 尝试识别为直线方程 Ax + By + C = 0 */
        double a, b, c;
        if (identify_line(coeffs, &a, &b, &c)) {
            /* 创建两个点表示直线 */
            SymbolicCoord *p1_coords[2];
            SymbolicCoord *p2_coords[2];

            if (fabs(b) > 1e-9) {
                /* y = (-Ax - C) / B，取 x=0 和 x=1 */
                double y0 = -c / b;
                double y1 = -(a + c) / b;
                p1_coords[0] = symbolic_coord_create_rational(0, 1);
                p1_coords[1] = symbolic_coord_create_rational((int64_t) (y0 * 1000), 1000);
                p2_coords[0] = symbolic_coord_create_rational(1000, 1000);
                p2_coords[1] = symbolic_coord_create_rational((int64_t) (y1 * 1000), 1000);
            } else {
                /* 垂直线 x = -C/A，取 y=0 和 y=1 */
                double x0 = -c / a;
                p1_coords[0] = symbolic_coord_create_rational((int64_t) (x0 * 1000), 1000);
                p1_coords[1] = symbolic_coord_create_rational(0, 1);
                p2_coords[0] = symbolic_coord_create_rational((int64_t) (x0 * 1000), 1000);
                p2_coords[1] = symbolic_coord_create_rational(1000, 1000);
            }

            AddNodeResult add_result = graph_add_point(graph, p1_coords, 2);
            symbolic_coord_destroy(p1_coords[0]);
            symbolic_coord_destroy(p1_coords[1]);

            if (add_result != ADD_NODE_OK) {
                return false;
            }
            int p1_id = graph->next_node_id - 1;

            add_result = graph_add_point(graph, p2_coords, 2);
            symbolic_coord_destroy(p2_coords[0]);
            symbolic_coord_destroy(p2_coords[1]);

            if (add_result != ADD_NODE_OK) {
                return false;
            }
            int p2_id = graph->next_node_id - 1;

            /* 创建线段 */
            graph_add_line_segment(graph, p1_id, p2_id);

            *out_node_id = p1_id;

            /* 标记为直线类型 */
            GeomNode *node = graph_get_node(graph, *out_node_id);
            if (node && node->numeric_assumption_declaration) {
                lv_free((void **) &node->numeric_assumption_declaration); /* 统一内存释放器 */
                node->numeric_assumption_declaration = NULL;
            }
            if (node) {
                char buf[FORMULA_BUF_SIZE];
                int n = snprintf(buf, sizeof(buf), "IMPLICIT_CURVE:LINE:%.6f:%.6f:%.6f", a, b, c);
                /* 防御性检查：确保snprintf输出零终止 */
                if (n < 0 || (size_t) n >= sizeof(buf)) {
                    buf[sizeof(buf) - 1] = '\0';
                }
                node->numeric_assumption_declaration = lv_strdup_safe(buf);
            }

            return true;
        }
    }

    /*
     * 无法识别为特定曲线类型，回退到通用隐式曲线表示。
     * 将多项式系数存储到 numeric_assumption_declaration 中，
     * 格式为 "IMPLICIT_CURVE:degree:coeffs..."
     */
    SymbolicCoord *coords[2];
    coords[0] = symbolic_coord_create_rational(0, 1);
    coords[1] = symbolic_coord_create_rational(0, 1);

    AddNodeResult add_result = graph_add_point(graph, coords, 2);
    symbolic_coord_destroy(coords[0]);
    symbolic_coord_destroy(coords[1]);

    if (add_result != ADD_NODE_OK) {
        return false;
    }

    *out_node_id = graph->next_node_id - 1;

    GeomNode *node = graph_get_node(graph, *out_node_id);
    if (node) {
        if (node->numeric_assumption_declaration) {
            lv_free((void **) &node->numeric_assumption_declaration); /* 统一内存释放器 */
            node->numeric_assumption_declaration = NULL;
        }

        if (poly_ok) {
            /* 计算实际多项式次数 */
            int max_total_deg = 0;
            for (int i = 0; i < IMPLICIT_COEFFS_SIZE; i++) {
                int deg_x = i / IMPLICIT_MAX_DEG;
                int deg_y = i % IMPLICIT_MAX_DEG;
                if (fabs(coeffs[i]) > 1e-12 && (deg_x + deg_y) > max_total_deg) {
                    max_total_deg = deg_x + deg_y;
                }
            }

            /* 格式: IMPLICIT_CURVE:max_degree:c00:c01:c10:... */
            char buf[FORMULA_LARGE_BUF_SIZE];
            int offset = snprintf(buf, sizeof(buf), "IMPLICIT_CURVE:%d", max_total_deg);
            /* 检查初始snprintf返回值：确保后续写入有有效起点 */
            if (offset < 0) {
                offset = 0;
                buf[0] = '\0';
            } else if (offset >= (int) sizeof(buf)) {
                offset = (int) sizeof(buf) - 1;
            }
            for (int i = 0; i < IMPLICIT_COEFFS_SIZE && offset < (int) sizeof(buf) - 1; i++) {
                if (fabs(coeffs[i]) > 1e-12) {
                    int added = snprintf(buf + offset, sizeof(buf) - offset, ":%d:%.10g", i, coeffs[i]);
                    if (added > 0) {
                        offset += added;
                    }
                }
            }
            node->numeric_assumption_declaration = lv_strdup_safe(buf);
        } else {
            node->numeric_assumption_declaration = lv_strdup_safe("IMPLICIT_CURVE:UNKNOWN");
        }
    }

    return true;
}
