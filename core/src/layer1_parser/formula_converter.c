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

#include "lv/lv_platform.h"
#include "formula_converter.h"
#include "formula_converter_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "formula_renderer.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "stream.h"
#include "stream_context_util.h"

/* ============================================================
 * 内部常量和宏
 * ============================================================ */

#define MAX_VAR_MAP_SIZE 256
#define MAX_NAME_LENGTH 64

/* 公式转换器内部缓冲区大小常量 */
#define FORMULA_LATEX_BUF_SIZE 512
#define FORMULA_PYTHON_BUF_SIZE 512
#define FORMULA_DSL_BUF_SIZE 512
#define FORMULA_EXPORT_BUF_SIZE 4096
#define FORMULA_SEG_LIST_SIZE 256
#define FORMULA_SEG_NAME_SIZE 64
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
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "invalid node or not a number");
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
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "invalid coord_list or out_count");
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
    static const char *s_node_prefixes[] = {
    [GEOM_POINT]          = "P",
    [GEOM_LINE_SEGMENT]   = "S",
    [GEOM_REGION]         = "R",
    [GEOM_CIRCLE]         = "C",
    [GEOM_PORT]           = "Port",
    [GEOM_FUNCTION_BLOCK] = "FB",
};

#define NODE_PREFIX_COUNT (sizeof(s_node_prefixes) / sizeof(s_node_prefixes[0]))
    {
    const char *prefix = ((unsigned)node->type < NODE_PREFIX_COUNT && s_node_prefixes[node->type])
                             ? s_node_prefixes[node->type] : "N";
    snprintf(out_name, buf_size, "%s%d", prefix, node->id);
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
            lv_RETURN_ERROR_BOOL(lv_ERROR_ALLOCATION_FAILED, "failed to allocate default coords");
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
    radius_coords[0] = symbolic_coord_from_double_scaled(radius, 1000); /* 圆心 x + radius */
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
    radius_coords[0] = symbolic_coord_from_double_scaled(rx, 1000);
    radius_coords[1] = symbolic_coord_from_double_scaled(ry, 1000);

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
     * 使用 ANGLE 约束类型，将角度信息存储在 numeric_value 中（度）。
     */

    if (formula_converter_stream_ctx) {
        stream_emit_warning(formula_converter_stream_ctx, "角度约束转换为 ANGLE 约束（数值近似）", 0);
    }

    /* 将弧度转换为度 */
    double angle_deg = angle_rad * 180.0 / M_PI;

    /* 创建两条线段 AB 和 BC */
    AddNodeResult seg_ab = graph_add_line_segment(graph, a_id, b_id);
    if (seg_ab != ADD_NODE_OK) {
        return false;
    }
    int seg_ab_id = graph_get_last_added_node_id(graph);

    AddNodeResult seg_bc = graph_add_line_segment(graph, b_id, c_id);
    if (seg_bc != ADD_NODE_OK) {
        return false;
    }
    int seg_bc_id = graph_get_last_added_node_id(graph);

    AddConstraintResult result = graph_add_angle(graph, seg_ab_id, seg_bc_id, angle_deg);

    if (result != ADD_CONSTRAINT_OK) {
        return false;
    }

    *out_constraint_id = graph_get_constraint_count(graph) - 1;

    /* 将角度约束的详细信息存储到约束节点上 */
    Constraint *constraint = graph_get_constraint(graph, *out_constraint_id);
    if (constraint) {
        /*
         * 角度信息已存储在 constraint->numeric_value 中。
         * 创建一个辅助点节点存储 cos(θ) 和 sin(θ) 以备后续代数求解。
         */
    }

    /* 创建一个辅助节点来存储角度约束的代数信息 */
    {
        double cos_theta = cos(angle_rad);
        double sin_theta = sin(angle_rad);

        /* 使用两个坐标存储 cos(θ) 和 sin(θ) */
        SymbolicCoord *angle_coords[2];
        angle_coords[0] = symbolic_coord_from_double_scaled(cos_theta, lv_RATIONAL_SCALE_DEFAULT);
        angle_coords[1] = symbolic_coord_from_double_scaled(sin_theta, lv_RATIONAL_SCALE_DEFAULT);

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

/* 函数指针类型：process_statement 分派 */
typedef bool (*ProcessStmtFunc)(const FormulaNode *stmt, ConstraintGraph *graph, FormulaToGraphResult *result);

static bool pstmt_p(const FormulaNode *s, ConstraintGraph *g, FormulaToGraphResult *r) {
    int nid = -1;
    if (formula_convert_point(s, g, &nid)) { if (r->created_node_count < 256) r->created_node_ids[r->created_node_count++] = nid; }
    return true; }
static bool pstmt_s(const FormulaNode *s, ConstraintGraph *g, FormulaToGraphResult *r) {
    int nid = -1;
    if (formula_convert_segment(s, g, &nid)) { if (r->created_node_count < 256) r->created_node_ids[r->created_node_count++] = nid; }
    return true; }
static bool pstmt_c(const FormulaNode *s, ConstraintGraph *g, FormulaToGraphResult *r) {
    int nid = -1;
    if (formula_convert_circle(s, g, &nid)) { if (r->created_node_count < 256) r->created_node_ids[r->created_node_count++] = nid; }
    return true; }
static bool pstmt_perp(const FormulaNode *s, ConstraintGraph *g, FormulaToGraphResult *r) {
    int cid = -1;
    if (formula_convert_perpendicular(s, g, &cid)) { if (r->created_constraint_count < 64) r->created_constraint_ids[r->created_constraint_count++] = cid; }
    return true; }
static bool pstmt_par(const FormulaNode *s, ConstraintGraph *g, FormulaToGraphResult *r) {
    int cid = -1;
    if (formula_convert_parallel(s, g, &cid)) { if (r->created_constraint_count < 64) r->created_constraint_ids[r->created_constraint_count++] = cid; }
    return true; }
static bool pstmt_mid(const FormulaNode *s, ConstraintGraph *g, FormulaToGraphResult *r) {
    int nid = -1;
    if (formula_convert_midpoint(s, g, &nid)) { if (r->created_node_count < 256) r->created_node_ids[r->created_node_count++] = nid; }
    return true; }
static bool pstmt_ang(const FormulaNode *s, ConstraintGraph *g, FormulaToGraphResult *r) {
    int cid = -1;
    if (formula_convert_angle(s, g, &cid)) { if (r->created_constraint_count < 64) r->created_constraint_ids[r->created_constraint_count++] = cid; }
    return true; }
static bool pstmt_eq(const FormulaNode *s, ConstraintGraph *g, FormulaToGraphResult *r) {
    int nid = -1;
    if (formula_convert_equation(s, g, &nid)) { if (r->created_node_count < 256) r->created_node_ids[r->created_node_count++] = nid; }
    return true; }
static bool pstmt_poly(const FormulaNode *s, ConstraintGraph *g, FormulaToGraphResult *r) {
    int ids[64]; int cnt = 0;
    if (formula_convert_polygon(s, g, ids, &cnt)) {
        if (cnt > 64) cnt = 64;
        for (int j = 0; j < cnt && r->created_node_count < 256; j++) r->created_node_ids[r->created_node_count++] = ids[j];
    }
    return true; }
static bool pstmt_reg(const FormulaNode *s, ConstraintGraph *g, FormulaToGraphResult *r) {
    int nid = -1;
    if (formula_convert_region(s, g, &nid)) { if (r->created_node_count < 256) r->created_node_ids[r->created_node_count++] = nid; }
    return true; }
static bool pstmt_arc(const FormulaNode *s, ConstraintGraph *g, FormulaToGraphResult *r) {
    int ids[10]; int cnt = 0;
    if (formula_convert_arc(s, g, ids, &cnt)) {
        if (cnt > 10) cnt = 10;
        for (int j = 0; j < cnt && r->created_node_count < 256; j++) r->created_node_ids[r->created_node_count++] = ids[j];
    }
    return true; }

static const ProcessStmtFunc s_stmt_funcs[] = {
    [NODE_GEOM_POINT] = pstmt_p,
    [NODE_GEOM_SEGMENT] = pstmt_s,
    [NODE_GEOM_CIRCLE] = pstmt_c,
    [NODE_CONSTRAINT_PERPENDICULAR] = pstmt_perp,
    [NODE_CONSTRAINT_PARALLEL] = pstmt_par,
    [NODE_CONSTRAINT_MIDPOINT] = pstmt_mid,
    [NODE_CONSTRAINT_ANGLE] = pstmt_ang,
    [NODE_EQUATION] = pstmt_eq,
    [NODE_GEOM_POLYGON] = pstmt_poly,
    [NODE_GEOM_REGION] = pstmt_reg,
    [NODE_GEOM_ARC] = pstmt_arc,
};

/* 创建节点/约束的最大数量限制 */
#define MAX_CREATED_NODES 256
#define MAX_CREATED_CONSTRAINTS 64

static bool formula_to_graph_process_statement(const FormulaNode *stmt, ConstraintGraph *graph,
                                               FormulaToGraphResult *result) {

        if ((unsigned)stmt->type < 36 && s_stmt_funcs[stmt->type]) {
        return s_stmt_funcs[stmt->type](stmt, graph, result);
    }
    return false;
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
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "failed to allocate result");
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