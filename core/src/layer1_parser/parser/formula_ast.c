/**
 * @file formula_ast.c
 * @brief FormulaNode AST 节点
 *
 * @details 拆分子模块（Lv-00 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/formula_parser.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"

/**
 * @brief 分配 FormulaNode 骨架并初始化公共字段
 *
 * 收敛 7+ 处 create_* 的「calloc + 失败检查 + 四字段初始化」三连样板；
 * 失败时统一走 lv_RETURN_ERROR_NULL 记录分配错误。
 */
static FormulaNode *formula_ast_alloc(NodeType type) {
    FormulaNode *node = lv_calloc(1, sizeof(FormulaNode));
    if (!node)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "failed to allocate AST node");
    node->type = type;
    node->line = 1;
    node->column = 1;
    node->refcount = 1;
    return node;
}

FormulaNode *formula_create_number(int64_t numerator, uint64_t denominator) {
    if (denominator == 0) {
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "formula_create_number: denominator must not be zero");
    }
    FormulaNode *node = formula_ast_alloc(NODE_NUMBER);
    node->data.number.numerator = numerator;
    node->data.number.denominator = denominator;
    node->data.number.is_integer = (denominator == 1);
    return node;
}

/**
 * @brief 创建变量 AST 节点
 *
 * @param name 变量名称
 * @return 新分配的 AST 节点指针，失败返回 NULL
 */
FormulaNode *formula_create_variable(const char *name) {
    if (!name)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "name is NULL");
    FormulaNode *node = formula_ast_alloc(NODE_VARIABLE);
    node->data.variable.name = lv_strdup_safe(name);
    if (!node->data.variable.name) {
        lv_ERROR_SET(lv_ERROR_ALLOCATION_FAILED, "failed to allocate variable name");
        lv_free((void **) &node);
        return NULL;
    }
    return node;
}

/**
 * @brief 创建标识符 AST 节点
 *
 * @param name 标识符名称
 * @return 新分配的 AST 节点指针，失败返回 NULL
 */
FormulaNode *formula_create_identifier(const char *name) {
    if (!name)
        return NULL;
    FormulaNode *node = formula_ast_alloc(NODE_IDENTIFIER);
    node->data.identifier.name = lv_strdup_safe(name);
    if (!node->data.identifier.name) {
        lv_free((void **) &node);
        return NULL;
    }
    return node;
}

/**
 * @brief 创建二元运算 AST 节点
 *
 * @param op_type 运算符类型
 * @param left    左操作数
 * @param right   右操作数
 * @return 新分配的 AST 节点指针，失败返回 NULL
 */
FormulaNode *formula_create_binary_op(NodeType op_type, FormulaNode *left, FormulaNode *right) {
    FormulaNode *node = formula_ast_alloc(op_type);
    node->data.binary_op.left = left;
    node->data.binary_op.right = right;
    /* 增加子节点引用计数：父节点持有对子节点的引用 */
    formula_node_ref(left);
    formula_node_ref(right);
    return node;
}

/**
 * @brief 创建一元运算 AST 节点
 *
 * @param op_type 运算符类型
 * @param operand 操作数
 * @return 新分配的 AST 节点指针，失败返回 NULL
 */
FormulaNode *formula_create_unary_op(NodeType op_type, FormulaNode *operand) {
    FormulaNode *node = formula_ast_alloc(op_type);
    node->data.unary_op.operand = operand;
    /* 引用计数管理策略：父节点持有对子节点的引用，
     * 因此需要递增操作数的引用计数，与 formula_create_binary_op 保持一致。
     * 这样当父节点被销毁时（formula_node_unref），会递减子节点的引用计数，
     * 只有当引用计数归零时才真正释放子节点，避免悬垂指针。 */
    formula_node_ref(operand);
    return node;
}

/**
 * @brief 创建等式 AST 节点
 *
 * @param lhs 左侧表达式
 * @param rhs 右侧表达式
 * @return 新分配的 AST 节点指针，失败返回 NULL
 */
FormulaNode *formula_create_equation(FormulaNode *lhs, FormulaNode *rhs) {
    FormulaNode *node = formula_ast_alloc(NODE_EQUATION);
    formula_node_ref(lhs);
    node->data.equation.lhs = lhs;
    formula_node_ref(rhs);
    node->data.equation.rhs = rhs;
    return node;
}

FormulaNode *formula_create_coord_list(FormulaNode **coords, int count) {
    FormulaNode *node = formula_ast_alloc(NODE_COORDINATE_LIST);
    if (count > 0 && coords) {
        node->data.coord_list.coords = lv_calloc(count, sizeof(FormulaNode *));
        if (!node->data.coord_list.coords) {
            lv_ERROR_SET(lv_ERROR_ALLOCATION_FAILED, "failed to allocate coord_list array");
            lv_free((void **) &node);
            return NULL;
        }
        memcpy(node->data.coord_list.coords, coords, sizeof(FormulaNode *) * count);
        for (int i = 0; i < count; i++) {
            formula_node_ref(coords[i]);
        }
        node->data.coord_list.coord_count = count;
    }
    return node;
}

FormulaNode *formula_create_geom_point(const char *name, FormulaNode *coords) {
    if (!name)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "name is NULL");
    FormulaNode *node = formula_ast_alloc(NODE_GEOM_POINT);
    node->data.geom_point.name = lv_strdup_safe(name);
    if (!node->data.geom_point.name) {
        lv_ERROR_SET(lv_ERROR_ALLOCATION_FAILED, "failed to allocate geom_point name");
        lv_free((void **) &node);
        return NULL;
    }
    node->data.geom_point.coords = coords;
    return node;
}

FormulaNode *formula_create_geom_segment(const char *name, FormulaNode *ep1, FormulaNode *ep2) {
    if (!name)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "name is NULL");
    FormulaNode *node = formula_ast_alloc(NODE_GEOM_SEGMENT);
    node->data.geom_segment.name = lv_strdup_safe(name);
    if (!node->data.geom_segment.name) {
        lv_ERROR_SET(lv_ERROR_ALLOCATION_FAILED, "failed to allocate geom_segment name");
        lv_free((void **) &node);
        return NULL;
    }
    node->data.geom_segment.endpoint1 = ep1;
    node->data.geom_segment.endpoint2 = ep2;
    return node;
}

FormulaNode *formula_create_geom_circle(const char *name, FormulaNode *center, FormulaNode *radius) {
    if (!name)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "name is NULL");
    FormulaNode *node = formula_ast_alloc(NODE_GEOM_CIRCLE);
    node->data.geom_circle.name = lv_strdup_safe(name);
    if (!node->data.geom_circle.name) {
        lv_free((void **) &node);
        return NULL;
    }
    node->data.geom_circle.center = center;
    node->data.geom_circle.radius = radius;
    return node;
}

FormulaNode *formula_create_geom_triangle(const char *name, FormulaNode *v1, FormulaNode *v2, FormulaNode *v3) {
    if (!name)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "name is NULL");
    FormulaNode *node = formula_ast_alloc(NODE_GEOM_TRIANGLE);
    node->data.geom_triangle.name = lv_strdup_safe(name);
    if (!node->data.geom_triangle.name) {
        lv_ERROR_SET(lv_ERROR_ALLOCATION_FAILED, "failed to allocate geom_triangle name");
        lv_free((void **) &node);
        return NULL;
    }
    node->data.geom_triangle.vertex1 = v1;
    node->data.geom_triangle.vertex2 = v2;
    node->data.geom_triangle.vertex3 = v3;
    return node;
}

FormulaNode *formula_create_geom_polygon(const char *name, FormulaNode **vertices, int vertex_count) {
    if (!name)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "name is NULL");
    FormulaNode *node = formula_ast_alloc(NODE_GEOM_POLYGON);
    node->data.geom_polygon.name = lv_strdup_safe(name);
    if (!node->data.geom_polygon.name) {
        lv_ERROR_SET(lv_ERROR_ALLOCATION_FAILED, "failed to allocate geom_polygon name");
        lv_free((void **) &node);
        return NULL;
    }
    if (vertex_count > 0 && vertices) {
        node->data.geom_polygon.vertices = lv_calloc(vertex_count, sizeof(FormulaNode *));
        if (!node->data.geom_polygon.vertices) {
            lv_ERROR_SET(lv_ERROR_ALLOCATION_FAILED, "failed to allocate geom_polygon vertices array");
            lv_free((void **) &node->data.geom_polygon.name);
            lv_free((void **) &node);
            return NULL;
        }
        memcpy(node->data.geom_polygon.vertices, vertices, sizeof(FormulaNode *) * vertex_count);
        node->data.geom_polygon.vertex_count = vertex_count;
    }
    return node;
}

FormulaNode *formula_create_geom_region(const char *name, FormulaNode **segments, int segment_count) {
    if (!name)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "name is NULL");
    FormulaNode *node = formula_ast_alloc(NODE_GEOM_REGION);
    node->data.geom_region.name = lv_strdup_safe(name);
    if (!node->data.geom_region.name) {
        lv_ERROR_SET(lv_ERROR_ALLOCATION_FAILED, "failed to allocate geom_region name");
        lv_free((void **) &node);
        return NULL;
    }
    if (segment_count > 0 && segments) {
        node->data.geom_region.boundary_segments = lv_calloc(segment_count, sizeof(FormulaNode *));
        if (!node->data.geom_region.boundary_segments) {
            lv_ERROR_SET(lv_ERROR_ALLOCATION_FAILED, "failed to allocate geom_region boundary_segments array");
            lv_free((void **) &node->data.geom_region.name);
            lv_free((void **) &node);
            return NULL;
        }
        memcpy(node->data.geom_region.boundary_segments, segments, sizeof(FormulaNode *) * segment_count);
        node->data.geom_region.segment_count = segment_count;
    }
    return node;
}

FormulaNode *formula_create_geom_arc(const char *name, FormulaNode *center, FormulaNode *radius,
                                     FormulaNode *start_angle, FormulaNode *end_angle) {
    if (!name)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "name is NULL");
    FormulaNode *node = formula_ast_alloc(NODE_GEOM_ARC);
    node->data.geom_arc.name = lv_strdup_safe(name);
    if (!node->data.geom_arc.name) {
        lv_ERROR_SET(lv_ERROR_ALLOCATION_FAILED, "failed to allocate geom_arc name");
        lv_free((void **) &node);
        return NULL;
    }
    node->data.geom_arc.center = center;
    node->data.geom_arc.radius = radius;
    node->data.geom_arc.start_angle = start_angle;
    node->data.geom_arc.end_angle = end_angle;
    return node;
}

FormulaNode *formula_create_constraint(NodeType constraint_type, FormulaNode **participants, int count) {
    FormulaNode *node = formula_ast_alloc(constraint_type);
    if (count > 0 && participants) {
        node->data.constraint.participants = lv_calloc(count, sizeof(FormulaNode *));
        if (!node->data.constraint.participants) {
            lv_ERROR_SET(lv_ERROR_ALLOCATION_FAILED, "failed to allocate constraint participants array");
            lv_free((void **) &node);
            return NULL;
        }
        memcpy(node->data.constraint.participants, participants, sizeof(FormulaNode *) * count);
        node->data.constraint.participant_count = count;
    }
    return node;
}

FormulaNode *formula_create_compound(FormulaNode **statements, int count) {
    FormulaNode *node = formula_ast_alloc(NODE_COMPOUND);
    if (count > 0 && statements) {
        node->data.compound.statements = lv_calloc(count, sizeof(FormulaNode *));
        if (!node->data.compound.statements) {
            lv_ERROR_SET(lv_ERROR_ALLOCATION_FAILED, "failed to allocate compound statements array");
            lv_free((void **) &node);
            return NULL;
        }
        memcpy(node->data.compound.statements, statements, sizeof(FormulaNode *) * count);
        node->data.compound.statement_count = count;
    }
    return node;
}

int formula_compound_add_statement(FormulaNode *compound, FormulaNode *statement) {
    if (!compound || compound->type != NODE_COMPOUND || !statement)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "invalid compound or statement");

    int new_count = compound->data.compound.statement_count + 1;
    FormulaNode **new_statements = lv_realloc(compound->data.compound.statements, sizeof(FormulaNode *) * new_count);
    if (!new_statements)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "failed to realloc statements array");

    compound->data.compound.statements = new_statements;
    compound->data.compound.statements[compound->data.compound.statement_count++] = statement;
    return 0;
}

/**
 * @brief 增加 AST 节点引用计数
 *
 * 调用此函数表示调用者持有了对该节点的额外引用。
 * 节点在其引用计数归零之前不会被公式销毁释放。
 *
 * @param[in] node AST节点指针（不能为NULL）
 * @return 增加后的引用计数
 */
int formula_node_ref(FormulaNode *node) {
    if (!node)
        return 0;
    return ++node->refcount;
}

/**
 * @brief 获取 AST 节点当前引用计数
 *
 * @param[in] node AST节点指针
 * @return 当前引用计数，node为NULL时返回0
 */
int formula_node_refcount(const FormulaNode *node) {
    if (!node)
        return 0;
    return node->refcount;
}

/* ============================================================
 * VTable 模式：节点类型分发表
 * ============================================================ */

/** @brief 节点虚拟方法表 */
typedef struct {
    void (*destroy)(FormulaNode *node);
    void (*copy)(const FormulaNode *src, FormulaNode *dst);
} FormulaNodeVTable;

/* --- destroy 处理函数 --- */

static void destroy_NUMBER(FormulaNode *node) {
    (void)node;
}

static void destroy_VARIABLE(FormulaNode *node) {
    lv_free((void **) &node->data.variable.name);
}

static void destroy_IDENTIFIER(FormulaNode *node) {
    lv_free((void **) &node->data.identifier.name);
}

static void destroy_BINARY_OP(FormulaNode *node) {
    formula_node_destroy(node->data.binary_op.left);
    formula_node_destroy(node->data.binary_op.right);
}

static void destroy_UNARY_OP(FormulaNode *node) {
    formula_node_destroy(node->data.unary_op.operand);
}

static void destroy_EQUATION(FormulaNode *node) {
    formula_node_destroy(node->data.equation.lhs);
    formula_node_destroy(node->data.equation.rhs);
}

static void destroy_COORD_LIST(FormulaNode *node) {
    for (int i = 0; i < node->data.coord_list.coord_count; i++) {
        formula_node_destroy(node->data.coord_list.coords[i]);
    }
    lv_free((void **) &node->data.coord_list.coords);
}

static void destroy_GEOM_POINT(FormulaNode *node) {
    lv_free((void **) &node->data.geom_point.name);
    formula_node_destroy(node->data.geom_point.coords);
}

static void destroy_GEOM_SEGMENT(FormulaNode *node) {
    lv_free((void **) &node->data.geom_segment.name);
    formula_node_destroy(node->data.geom_segment.endpoint1);
    formula_node_destroy(node->data.geom_segment.endpoint2);
}

static void destroy_GEOM_LINE(FormulaNode *node) {
    lv_free((void **) &node->data.geom_line.name);
    formula_node_destroy(node->data.geom_line.point1);
    formula_node_destroy(node->data.geom_line.point2);
    formula_node_destroy(node->data.geom_line.equation);
}

static void destroy_GEOM_CIRCLE(FormulaNode *node) {
    lv_free((void **) &node->data.geom_circle.name);
    formula_node_destroy(node->data.geom_circle.center);
    formula_node_destroy(node->data.geom_circle.radius);
    formula_node_destroy(node->data.geom_circle.equation);
}

static void destroy_GEOM_TRIANGLE(FormulaNode *node) {
    lv_free((void **) &node->data.geom_triangle.name);
    formula_node_destroy(node->data.geom_triangle.vertex1);
    formula_node_destroy(node->data.geom_triangle.vertex2);
    formula_node_destroy(node->data.geom_triangle.vertex3);
}

static void destroy_GEOM_POLYGON(FormulaNode *node) {
    lv_free((void **) &node->data.geom_polygon.name);
    for (int i = 0; i < node->data.geom_polygon.vertex_count; i++) {
        formula_node_destroy(node->data.geom_polygon.vertices[i]);
    }
    lv_free((void **) &node->data.geom_polygon.vertices);
}

static void destroy_GEOM_REGION(FormulaNode *node) {
    lv_free((void **) &node->data.geom_region.name);
    for (int i = 0; i < node->data.geom_region.segment_count; i++) {
        formula_node_destroy(node->data.geom_region.boundary_segments[i]);
    }
    lv_free((void **) &node->data.geom_region.boundary_segments);
}

static void destroy_GEOM_ARC(FormulaNode *node) {
    lv_free((void **) &node->data.geom_arc.name);
    formula_node_destroy(node->data.geom_arc.center);
    formula_node_destroy(node->data.geom_arc.radius);
    formula_node_destroy(node->data.geom_arc.start_angle);
    formula_node_destroy(node->data.geom_arc.end_angle);
}

static void destroy_GEOM_VECTOR(FormulaNode *node) {
    lv_free((void **) &node->data.geom_vector.name);
    formula_node_destroy(node->data.geom_vector.start);
    formula_node_destroy(node->data.geom_vector.end);
}

static void destroy_CONSTRAINT(FormulaNode *node) {
    for (int i = 0; i < node->data.constraint.participant_count; i++) {
        formula_node_destroy(node->data.constraint.participants[i]);
    }
    lv_free((void **) &node->data.constraint.participants);
}

static void destroy_COMPOUND(FormulaNode *node) {
    for (int i = 0; i < node->data.compound.statement_count; i++) {
        formula_node_destroy(node->data.compound.statements[i]);
    }
    lv_free((void **) &node->data.compound.statements);
}

/* --- copy 处理函数 --- */

static void copy_NUMBER(const FormulaNode *src, FormulaNode *dst) {
    dst->data.number.numerator = src->data.number.numerator;
    dst->data.number.denominator = src->data.number.denominator;
    dst->data.number.is_integer = src->data.number.is_integer;
}

static void copy_VARIABLE(const FormulaNode *src, FormulaNode *dst) {
    if (src->data.variable.name)
        dst->data.variable.name = lv_strdup(src->data.variable.name);
}

static void copy_IDENTIFIER(const FormulaNode *src, FormulaNode *dst) {
    if (src->data.identifier.name)
        dst->data.identifier.name = lv_strdup(src->data.identifier.name);
}

static void copy_BINARY_OP(const FormulaNode *src, FormulaNode *dst) {
    dst->data.binary_op.left = formula_node_copy(src->data.binary_op.left);
    dst->data.binary_op.right = formula_node_copy(src->data.binary_op.right);
}

static void copy_UNARY_OP(const FormulaNode *src, FormulaNode *dst) {
    dst->data.unary_op.operand = formula_node_copy(src->data.unary_op.operand);
}

static void copy_EQUATION(const FormulaNode *src, FormulaNode *dst) {
    dst->data.equation.lhs = formula_node_copy(src->data.equation.lhs);
    dst->data.equation.rhs = formula_node_copy(src->data.equation.rhs);
}

static void copy_COORD_LIST(const FormulaNode *src, FormulaNode *dst) {
    dst->data.coord_list.coord_count = src->data.coord_list.coord_count;
    if (src->data.coord_list.coord_count > 0 && src->data.coord_list.coords) {
        dst->data.coord_list.coords =
            lv_calloc((size_t) src->data.coord_list.coord_count, sizeof(FormulaNode *));
        if (dst->data.coord_list.coords) {
            for (int i = 0; i < src->data.coord_list.coord_count; i++)
                dst->data.coord_list.coords[i] = formula_node_copy(src->data.coord_list.coords[i]);
        }
    }
}

static void copy_GEOM_POINT(const FormulaNode *src, FormulaNode *dst) {
    if (src->data.geom_point.name)
        dst->data.geom_point.name = lv_strdup(src->data.geom_point.name);
    dst->data.geom_point.coords = formula_node_copy(src->data.geom_point.coords);
}

static void copy_GEOM_SEGMENT(const FormulaNode *src, FormulaNode *dst) {
    if (src->data.geom_segment.name)
        dst->data.geom_segment.name = lv_strdup(src->data.geom_segment.name);
    dst->data.geom_segment.endpoint1 = formula_node_copy(src->data.geom_segment.endpoint1);
    dst->data.geom_segment.endpoint2 = formula_node_copy(src->data.geom_segment.endpoint2);
}

static void copy_GEOM_LINE(const FormulaNode *src, FormulaNode *dst) {
    if (src->data.geom_line.name)
        dst->data.geom_line.name = lv_strdup(src->data.geom_line.name);
    dst->data.geom_line.point1 = formula_node_copy(src->data.geom_line.point1);
    dst->data.geom_line.point2 = formula_node_copy(src->data.geom_line.point2);
    dst->data.geom_line.equation = formula_node_copy(src->data.geom_line.equation);
}

static void copy_GEOM_CIRCLE(const FormulaNode *src, FormulaNode *dst) {
    if (src->data.geom_circle.name)
        dst->data.geom_circle.name = lv_strdup(src->data.geom_circle.name);
    dst->data.geom_circle.center = formula_node_copy(src->data.geom_circle.center);
    dst->data.geom_circle.radius = formula_node_copy(src->data.geom_circle.radius);
    dst->data.geom_circle.equation = formula_node_copy(src->data.geom_circle.equation);
}

static void copy_GEOM_TRIANGLE(const FormulaNode *src, FormulaNode *dst) {
    if (src->data.geom_triangle.name)
        dst->data.geom_triangle.name = lv_strdup(src->data.geom_triangle.name);
    dst->data.geom_triangle.vertex1 = formula_node_copy(src->data.geom_triangle.vertex1);
    dst->data.geom_triangle.vertex2 = formula_node_copy(src->data.geom_triangle.vertex2);
    dst->data.geom_triangle.vertex3 = formula_node_copy(src->data.geom_triangle.vertex3);
}

static void copy_GEOM_POLYGON(const FormulaNode *src, FormulaNode *dst) {
    if (src->data.geom_polygon.name)
        dst->data.geom_polygon.name = lv_strdup(src->data.geom_polygon.name);
    dst->data.geom_polygon.vertex_count = src->data.geom_polygon.vertex_count;
    if (src->data.geom_polygon.vertex_count > 0 && src->data.geom_polygon.vertices) {
        dst->data.geom_polygon.vertices =
            lv_calloc((size_t) src->data.geom_polygon.vertex_count, sizeof(FormulaNode *));
        if (dst->data.geom_polygon.vertices) {
            for (int i = 0; i < src->data.geom_polygon.vertex_count; i++)
                dst->data.geom_polygon.vertices[i] = formula_node_copy(src->data.geom_polygon.vertices[i]);
        }
    }
}

static void copy_GEOM_REGION(const FormulaNode *src, FormulaNode *dst) {
    if (src->data.geom_region.name)
        dst->data.geom_region.name = lv_strdup(src->data.geom_region.name);
    dst->data.geom_region.segment_count = src->data.geom_region.segment_count;
    if (src->data.geom_region.segment_count > 0 && src->data.geom_region.boundary_segments) {
        dst->data.geom_region.boundary_segments =
            lv_calloc((size_t) src->data.geom_region.segment_count, sizeof(FormulaNode *));
        if (dst->data.geom_region.boundary_segments) {
            for (int i = 0; i < src->data.geom_region.segment_count; i++)
                dst->data.geom_region.boundary_segments[i] =
                    formula_node_copy(src->data.geom_region.boundary_segments[i]);
        }
    }
}

static void copy_GEOM_ARC(const FormulaNode *src, FormulaNode *dst) {
    if (src->data.geom_arc.name)
        dst->data.geom_arc.name = lv_strdup(src->data.geom_arc.name);
    dst->data.geom_arc.center = formula_node_copy(src->data.geom_arc.center);
    dst->data.geom_arc.radius = formula_node_copy(src->data.geom_arc.radius);
    dst->data.geom_arc.start_angle = formula_node_copy(src->data.geom_arc.start_angle);
    dst->data.geom_arc.end_angle = formula_node_copy(src->data.geom_arc.end_angle);
}

static void copy_GEOM_VECTOR(const FormulaNode *src, FormulaNode *dst) {
    if (src->data.geom_vector.name)
        dst->data.geom_vector.name = lv_strdup(src->data.geom_vector.name);
    dst->data.geom_vector.start = formula_node_copy(src->data.geom_vector.start);
    dst->data.geom_vector.end = formula_node_copy(src->data.geom_vector.end);
}

static void copy_CONSTRAINT(const FormulaNode *src, FormulaNode *dst) {
    dst->data.constraint.participant_count = src->data.constraint.participant_count;
    if (src->data.constraint.participant_count > 0 && src->data.constraint.participants) {
        dst->data.constraint.participants =
            lv_calloc((size_t) src->data.constraint.participant_count, sizeof(FormulaNode *));
        if (dst->data.constraint.participants) {
            for (int i = 0; i < src->data.constraint.participant_count; i++)
                dst->data.constraint.participants[i] =
                    formula_node_copy(src->data.constraint.participants[i]);
        }
    }
}

static void copy_COMPOUND(const FormulaNode *src, FormulaNode *dst) {
    dst->data.compound.statement_count = src->data.compound.statement_count;
    if (src->data.compound.statement_count > 0 && src->data.compound.statements) {
        dst->data.compound.statements =
            lv_calloc((size_t) src->data.compound.statement_count, sizeof(FormulaNode *));
        if (dst->data.compound.statements) {
            for (int i = 0; i < src->data.compound.statement_count; i++)
                dst->data.compound.statements[i] = formula_node_copy(src->data.compound.statements[i]);
        }
    }
}

/* --- VTable 查找表 --- */
static const FormulaNodeVTable kFormulaVTable[] = {
    [NODE_NUMBER]                = { destroy_NUMBER,       copy_NUMBER },
    [NODE_VARIABLE]              = { destroy_VARIABLE,     copy_VARIABLE },
    [NODE_IDENTIFIER]            = { destroy_IDENTIFIER,   copy_IDENTIFIER },
    [NODE_BINARY_OP_ADD]         = { destroy_BINARY_OP,    copy_BINARY_OP },
    [NODE_BINARY_OP_SUB]         = { destroy_BINARY_OP,    copy_BINARY_OP },
    [NODE_BINARY_OP_MUL]         = { destroy_BINARY_OP,    copy_BINARY_OP },
    [NODE_BINARY_OP_DIV]         = { destroy_BINARY_OP,    copy_BINARY_OP },
    [NODE_BINARY_OP_POW]         = { destroy_BINARY_OP,    copy_BINARY_OP },
    [NODE_UNARY_OP_NEG]          = { destroy_UNARY_OP,     copy_UNARY_OP },
    [NODE_UNARY_OP_SQRT]         = { destroy_UNARY_OP,     copy_UNARY_OP },
    [NODE_UNARY_OP_SIN]          = { destroy_UNARY_OP,     copy_UNARY_OP },
    [NODE_UNARY_OP_COS]          = { destroy_UNARY_OP,     copy_UNARY_OP },
    [NODE_UNARY_OP_TAN]          = { destroy_UNARY_OP,     copy_UNARY_OP },
    [NODE_UNARY_OP_ABS]          = { destroy_UNARY_OP,     copy_UNARY_OP },
    [NODE_UNARY_OP_LN]           = { destroy_UNARY_OP,     copy_UNARY_OP },
    [NODE_UNARY_OP_LOG]          = { destroy_UNARY_OP,     copy_UNARY_OP },
    [NODE_EQUATION]              = { destroy_EQUATION,     copy_EQUATION },
    [NODE_COORDINATE_LIST]       = { destroy_COORD_LIST,   copy_COORD_LIST },
    [NODE_GEOM_POINT]            = { destroy_GEOM_POINT,   copy_GEOM_POINT },
    [NODE_GEOM_SEGMENT]          = { destroy_GEOM_SEGMENT, copy_GEOM_SEGMENT },
    [NODE_GEOM_LINE]             = { destroy_GEOM_LINE,    copy_GEOM_LINE },
    [NODE_GEOM_CIRCLE]           = { destroy_GEOM_CIRCLE,  copy_GEOM_CIRCLE },
    [NODE_GEOM_TRIANGLE]         = { destroy_GEOM_TRIANGLE, copy_GEOM_TRIANGLE },
    [NODE_GEOM_POLYGON]          = { destroy_GEOM_POLYGON, copy_GEOM_POLYGON },
    [NODE_GEOM_REGION]           = { destroy_GEOM_REGION,  copy_GEOM_REGION },
    [NODE_GEOM_ARC]              = { destroy_GEOM_ARC,     copy_GEOM_ARC },
    [NODE_GEOM_VECTOR]           = { destroy_GEOM_VECTOR,  copy_GEOM_VECTOR },
    [NODE_CONSTRAINT_PERPENDICULAR] = { destroy_CONSTRAINT, copy_CONSTRAINT },
    [NODE_CONSTRAINT_PARALLEL]       = { destroy_CONSTRAINT, copy_CONSTRAINT },
    [NODE_CONSTRAINT_MIDPOINT]       = { destroy_CONSTRAINT, copy_CONSTRAINT },
    [NODE_CONSTRAINT_BISECTOR]       = { destroy_CONSTRAINT, copy_CONSTRAINT },
    [NODE_CONSTRAINT_COLLINEAR]      = { destroy_CONSTRAINT, copy_CONSTRAINT },
    [NODE_CONSTRAINT_TANGENT]        = { destroy_CONSTRAINT, copy_CONSTRAINT },
    [NODE_CONSTRAINT_CONGRUENT]      = { destroy_CONSTRAINT, copy_CONSTRAINT },
    [NODE_CONSTRAINT_ANGLE]          = { destroy_CONSTRAINT, copy_CONSTRAINT },
    [NODE_COMPOUND]              = { destroy_COMPOUND,    copy_COMPOUND },
};

/**
 * @brief 销毁 AST 节点（引用计数管理）
 *
 * 递减节点的引用计数。仅当引用计数归零时，才递归销毁子节点并释放内存。
 * 传入 NULL 指针安全无操作。
 *
 * @param[in] node AST节点指针，可为 NULL
 */
void formula_node_destroy(FormulaNode *node) {
    if (!node)
        return;

    /* 引用计数递减：仅当引用计数归零时才真正销毁 */
    node->refcount--;
    if (node->refcount > 0)
        return;

    kFormulaVTable[node->type].destroy(node);

    lv_free((void **) &node);
}

FormulaNode *formula_node_copy(const FormulaNode *node) {
    if (!node)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "source node is NULL");

    FormulaNode *copy = lv_calloc(1, sizeof(FormulaNode));
    if (!copy)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "failed to allocate copy node");

    copy->type = node->type;
    copy->line = node->line;
    copy->column = node->column;
    copy->refcount = 1;

    kFormulaVTable[node->type].copy(node, copy);

    return copy;
}
