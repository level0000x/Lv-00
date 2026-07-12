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
#include "lv00/formula_parser.h"
#include "debug.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

FormulaNode *formula_create_number(int64_t numerator, uint64_t denominator) {
    if (denominator == 0) {
        lv00_set_error(LV00_ERROR_INVALID_PARAM, "formula_create_number: denominator must not be zero");
        return NULL;
    }
    FormulaNode *node = lv00_calloc(1, sizeof(FormulaNode));
    if (!node)
        return NULL;
    node->type = NODE_NUMBER;
    node->line = 1;
    node->column = 1;
    node->refcount = 1;
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
        return NULL;
    FormulaNode *node = lv00_calloc(1, sizeof(FormulaNode));
    if (!node)
        return NULL;
    node->type = NODE_VARIABLE;
    node->line = 1;
    node->column = 1;
    node->refcount = 1;
    node->data.variable.name = lv00_strdup_safe(name);
    if (!node->data.variable.name) {
        lv00_free((void **) &node);
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
    FormulaNode *node = lv00_calloc(1, sizeof(FormulaNode));
    if (!node)
        return NULL;
    node->type = NODE_IDENTIFIER;
    node->line = 1;
    node->column = 1;
    node->refcount = 1;
    node->data.identifier.name = lv00_strdup_safe(name);
    if (!node->data.identifier.name) {
        lv00_free((void **) &node);
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
    FormulaNode *node = lv00_calloc(1, sizeof(FormulaNode));
    if (!node)
        return NULL;
    node->type = op_type;
    node->line = 1;
    node->column = 1;
    node->refcount = 1;
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
    FormulaNode *node = lv00_calloc(1, sizeof(FormulaNode));
    if (!node)
        return NULL;
    node->type = op_type;
    node->line = 1;
    node->column = 1;
    node->refcount = 1;
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
    FormulaNode *node = lv00_calloc(1, sizeof(FormulaNode));
    if (!node)
        return NULL;
    node->type = NODE_EQUATION;
    node->line = 1;
    node->column = 1;
    node->refcount = 1;
    formula_node_ref(lhs);
    node->data.equation.lhs = lhs;
    formula_node_ref(rhs);
    node->data.equation.rhs = rhs;
    return node;
}

FormulaNode *formula_create_coord_list(FormulaNode **coords, int count) {
    FormulaNode *node = lv00_calloc(1, sizeof(FormulaNode));
    if (!node)
        return NULL;
    node->type = NODE_COORDINATE_LIST;
    node->line = 1;
    node->column = 1;
    node->refcount = 1;
    if (count > 0 && coords) {
        node->data.coord_list.coords = lv00_malloc(sizeof(FormulaNode *) * count);
        if (!node->data.coord_list.coords) {
            lv00_free((void **) &node);
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
        return NULL;
    FormulaNode *node = lv00_calloc(1, sizeof(FormulaNode));
    if (!node)
        return NULL;
    node->type = NODE_GEOM_POINT;
    node->line = 1;
    node->column = 1;
    node->refcount = 1;
    node->data.geom_point.name = lv00_strdup_safe(name);
    if (!node->data.geom_point.name) {
        lv00_free((void **) &node);
        return NULL;
    }
    node->data.geom_point.coords = coords;
    return node;
}

FormulaNode *formula_create_geom_segment(const char *name, FormulaNode *ep1, FormulaNode *ep2) {
    if (!name)
        return NULL;
    FormulaNode *node = lv00_calloc(1, sizeof(FormulaNode));
    if (!node)
        return NULL;
    node->type = NODE_GEOM_SEGMENT;
    node->line = 1;
    node->column = 1;
    node->refcount = 1;
    node->data.geom_segment.name = lv00_strdup_safe(name);
    if (!node->data.geom_segment.name) {
        lv00_free((void **) &node);
        return NULL;
    }
    node->data.geom_segment.endpoint1 = ep1;
    node->data.geom_segment.endpoint2 = ep2;
    return node;
}

FormulaNode *formula_create_geom_circle(const char *name, FormulaNode *center, FormulaNode *radius) {
    if (!name)
        return NULL;
    FormulaNode *node = lv00_calloc(1, sizeof(FormulaNode));
    if (!node)
        return NULL;
    node->type = NODE_GEOM_CIRCLE;
    node->line = 1;
    node->column = 1;
    node->refcount = 1;
    node->data.geom_circle.name = lv00_strdup_safe(name);
    if (!node->data.geom_circle.name) {
        lv00_free((void **) &node);
        return NULL;
    }
    node->data.geom_circle.center = center;
    node->data.geom_circle.radius = radius;
    return node;
}

FormulaNode *formula_create_geom_triangle(const char *name, FormulaNode *v1, FormulaNode *v2, FormulaNode *v3) {
    if (!name)
        return NULL;
    FormulaNode *node = lv00_calloc(1, sizeof(FormulaNode));
    if (!node)
        return NULL;
    node->type = NODE_GEOM_TRIANGLE;
    node->line = 1;
    node->column = 1;
    node->refcount = 1;
    node->data.geom_triangle.name = lv00_strdup_safe(name);
    if (!node->data.geom_triangle.name) {
        lv00_free((void **) &node);
        return NULL;
    }
    node->data.geom_triangle.vertex1 = v1;
    node->data.geom_triangle.vertex2 = v2;
    node->data.geom_triangle.vertex3 = v3;
    return node;
}

FormulaNode *formula_create_geom_polygon(const char *name, FormulaNode **vertices, int vertex_count) {
    if (!name)
        return NULL;
    FormulaNode *node = lv00_calloc(1, sizeof(FormulaNode));
    if (!node)
        return NULL;
    node->type = NODE_GEOM_POLYGON;
    node->line = 1;
    node->column = 1;
    node->refcount = 1;
    node->data.geom_polygon.name = lv00_strdup_safe(name);
    if (!node->data.geom_polygon.name) {
        lv00_free((void **) &node);
        return NULL;
    }
    if (vertex_count > 0 && vertices) {
        node->data.geom_polygon.vertices = lv00_malloc(sizeof(FormulaNode *) * vertex_count);
        if (!node->data.geom_polygon.vertices) {
            lv00_free((void **) &node->data.geom_polygon.name);
            lv00_free((void **) &node);
            return NULL;
        }
        memcpy(node->data.geom_polygon.vertices, vertices, sizeof(FormulaNode *) * vertex_count);
        node->data.geom_polygon.vertex_count = vertex_count;
    }
    return node;
}

FormulaNode *formula_create_geom_region(const char *name, FormulaNode **segments, int segment_count) {
    if (!name)
        return NULL;
    FormulaNode *node = lv00_calloc(1, sizeof(FormulaNode));
    if (!node)
        return NULL;
    node->type = NODE_GEOM_REGION;
    node->line = 1;
    node->column = 1;
    node->refcount = 1;
    node->data.geom_region.name = lv00_strdup_safe(name);
    if (!node->data.geom_region.name) {
        lv00_free((void **) &node);
        return NULL;
    }
    if (segment_count > 0 && segments) {
        node->data.geom_region.boundary_segments = lv00_malloc(sizeof(FormulaNode *) * segment_count);
        if (!node->data.geom_region.boundary_segments) {
            lv00_free((void **) &node->data.geom_region.name);
            lv00_free((void **) &node);
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
        return NULL;
    FormulaNode *node = lv00_calloc(1, sizeof(FormulaNode));
    if (!node)
        return NULL;
    node->type = NODE_GEOM_ARC;
    node->line = 1;
    node->column = 1;
    node->refcount = 1;
    node->data.geom_arc.name = lv00_strdup_safe(name);
    if (!node->data.geom_arc.name) {
        lv00_free((void **) &node);
        return NULL;
    }
    node->data.geom_arc.center = center;
    node->data.geom_arc.radius = radius;
    node->data.geom_arc.start_angle = start_angle;
    node->data.geom_arc.end_angle = end_angle;
    return node;
}

FormulaNode *formula_create_constraint(NodeType constraint_type, FormulaNode **participants, int count) {
    FormulaNode *node = lv00_calloc(1, sizeof(FormulaNode));
    if (!node)
        return NULL;
    node->type = constraint_type;
    node->line = 1;
    node->column = 1;
    node->refcount = 1;
    if (count > 0 && participants) {
        node->data.constraint.participants = lv00_malloc(sizeof(FormulaNode *) * count);
        if (!node->data.constraint.participants) {
            lv00_free((void **) &node);
            return NULL;
        }
        memcpy(node->data.constraint.participants, participants, sizeof(FormulaNode *) * count);
        node->data.constraint.participant_count = count;
    }
    return node;
}

FormulaNode *formula_create_compound(FormulaNode **statements, int count) {
    FormulaNode *node = lv00_calloc(1, sizeof(FormulaNode));
    if (!node)
        return NULL;
    node->type = NODE_COMPOUND;
    node->line = 1;
    node->column = 1;
    node->refcount = 1;
    if (count > 0 && statements) {
        node->data.compound.statements = lv00_malloc(sizeof(FormulaNode *) * count);
        if (!node->data.compound.statements) {
            lv00_free((void **) &node);
            return NULL;
        }
        memcpy(node->data.compound.statements, statements, sizeof(FormulaNode *) * count);
        node->data.compound.statement_count = count;
    }
    return node;
}

int formula_compound_add_statement(FormulaNode *compound, FormulaNode *statement) {
    if (!compound || compound->type != NODE_COMPOUND || !statement)
        return -1;

    int new_count = compound->data.compound.statement_count + 1;
    FormulaNode **new_statements = lv00_realloc(compound->data.compound.statements, sizeof(FormulaNode *) * new_count);
    if (!new_statements)
        return -1;

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
    if (!node) return 0;
    return ++node->refcount;
}

/**
 * @brief 获取 AST 节点当前引用计数
 *
 * @param[in] node AST节点指针
 * @return 当前引用计数，node为NULL时返回0
 */
int formula_node_refcount(const FormulaNode *node) {
    if (!node) return 0;
    return node->refcount;
}

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

    switch (node->type) {
        case NODE_NUMBER:
            /* 无需释放 */
            break;

        case NODE_VARIABLE:
            lv00_free((void **) &node->data.variable.name);
            break;

        case NODE_IDENTIFIER:
            lv00_free((void **) &node->data.identifier.name);
            break;

        case NODE_BINARY_OP_ADD:
        case NODE_BINARY_OP_SUB:
        case NODE_BINARY_OP_MUL:
        case NODE_BINARY_OP_DIV:
        case NODE_BINARY_OP_POW:
            formula_node_destroy(node->data.binary_op.left);
            formula_node_destroy(node->data.binary_op.right);
            break;

        case NODE_UNARY_OP_NEG:
        case NODE_UNARY_OP_SQRT:
        case NODE_UNARY_OP_SIN:
        case NODE_UNARY_OP_COS:
        case NODE_UNARY_OP_TAN:
        case NODE_UNARY_OP_ABS:
        case NODE_UNARY_OP_LN:
        case NODE_UNARY_OP_LOG:
            formula_node_destroy(node->data.unary_op.operand);
            break;

        case NODE_EQUATION:
            formula_node_destroy(node->data.equation.lhs);
            formula_node_destroy(node->data.equation.rhs);
            break;

        case NODE_COORDINATE_LIST:
            for (int i = 0; i < node->data.coord_list.coord_count; i++) {
                formula_node_destroy(node->data.coord_list.coords[i]);
            }
            lv00_free((void **) &node->data.coord_list.coords);
            break;

        case NODE_GEOM_POINT:
            lv00_free((void **) &node->data.geom_point.name);
            formula_node_destroy(node->data.geom_point.coords);
            break;

        case NODE_GEOM_SEGMENT:
            lv00_free((void **) &node->data.geom_segment.name);
            formula_node_destroy(node->data.geom_segment.endpoint1);
            formula_node_destroy(node->data.geom_segment.endpoint2);
            break;

        case NODE_GEOM_LINE:
            lv00_free((void **) &node->data.geom_line.name);
            formula_node_destroy(node->data.geom_line.point1);
            formula_node_destroy(node->data.geom_line.point2);
            formula_node_destroy(node->data.geom_line.equation);
            break;

        case NODE_GEOM_CIRCLE:
            lv00_free((void **) &node->data.geom_circle.name);
            formula_node_destroy(node->data.geom_circle.center);
            formula_node_destroy(node->data.geom_circle.radius);
            formula_node_destroy(node->data.geom_circle.equation);
            break;

        case NODE_GEOM_TRIANGLE:
            lv00_free((void **) &node->data.geom_triangle.name);
            formula_node_destroy(node->data.geom_triangle.vertex1);
            formula_node_destroy(node->data.geom_triangle.vertex2);
            formula_node_destroy(node->data.geom_triangle.vertex3);
            break;

        case NODE_GEOM_POLYGON:
            lv00_free((void **) &node->data.geom_polygon.name);
            for (int i = 0; i < node->data.geom_polygon.vertex_count; i++) {
                formula_node_destroy(node->data.geom_polygon.vertices[i]);
            }
            lv00_free((void **) &node->data.geom_polygon.vertices);
            break;

        case NODE_GEOM_REGION:
            lv00_free((void **) &node->data.geom_region.name);
            for (int i = 0; i < node->data.geom_region.segment_count; i++) {
                formula_node_destroy(node->data.geom_region.boundary_segments[i]);
            }
            lv00_free((void **) &node->data.geom_region.boundary_segments);
            break;

        case NODE_GEOM_ARC:
            lv00_free((void **) &node->data.geom_arc.name);
            formula_node_destroy(node->data.geom_arc.center);
            formula_node_destroy(node->data.geom_arc.radius);
            formula_node_destroy(node->data.geom_arc.start_angle);
            formula_node_destroy(node->data.geom_arc.end_angle);
            break;

        case NODE_GEOM_VECTOR:
            lv00_free((void **) &node->data.geom_vector.name);
            formula_node_destroy(node->data.geom_vector.start);
            formula_node_destroy(node->data.geom_vector.end);
            break;

        case NODE_CONSTRAINT_PERPENDICULAR:
        case NODE_CONSTRAINT_PARALLEL:
        case NODE_CONSTRAINT_MIDPOINT:
        case NODE_CONSTRAINT_BISECTOR:
        case NODE_CONSTRAINT_COLLINEAR:
        case NODE_CONSTRAINT_TANGENT:
        case NODE_CONSTRAINT_CONGRUENT:
        case NODE_CONSTRAINT_ANGLE:
            for (int i = 0; i < node->data.constraint.participant_count; i++) {
                formula_node_destroy(node->data.constraint.participants[i]);
            }
            lv00_free((void **) &node->data.constraint.participants);
            break;

        case NODE_COMPOUND:
            for (int i = 0; i < node->data.compound.statement_count; i++) {
                formula_node_destroy(node->data.compound.statements[i]);
            }
            lv00_free((void **) &node->data.compound.statements);
            break;
    }

    lv00_free((void **) &node);
}

FormulaNode *formula_node_copy(const FormulaNode *node) {
    if (!node)
        return NULL;

    FormulaNode *copy = lv00_calloc(1, sizeof(FormulaNode));
    if (!copy)
        return NULL;

    copy->type = node->type;
    copy->line = node->line;
    copy->column = node->column;
    copy->refcount = 1;

    switch (node->type) {
        case NODE_NUMBER:
            copy->data.number.numerator = node->data.number.numerator;
            copy->data.number.denominator = node->data.number.denominator;
            copy->data.number.is_integer = node->data.number.is_integer;
            break;

        case NODE_VARIABLE:
            if (node->data.variable.name)
                copy->data.variable.name = lv00_strdup(node->data.variable.name);
            break;

        case NODE_IDENTIFIER:
            if (node->data.identifier.name)
                copy->data.identifier.name = lv00_strdup(node->data.identifier.name);
            break;

        case NODE_BINARY_OP_ADD:
        case NODE_BINARY_OP_SUB:
        case NODE_BINARY_OP_MUL:
        case NODE_BINARY_OP_DIV:
        case NODE_BINARY_OP_POW:
            copy->data.binary_op.left = formula_node_copy(node->data.binary_op.left);
            copy->data.binary_op.right = formula_node_copy(node->data.binary_op.right);
            break;

        case NODE_UNARY_OP_NEG:
        case NODE_UNARY_OP_SQRT:
        case NODE_UNARY_OP_SIN:
        case NODE_UNARY_OP_COS:
        case NODE_UNARY_OP_TAN:
        case NODE_UNARY_OP_ABS:
        case NODE_UNARY_OP_LN:
        case NODE_UNARY_OP_LOG:
            copy->data.unary_op.operand = formula_node_copy(node->data.unary_op.operand);
            break;

        case NODE_EQUATION:
            copy->data.equation.lhs = formula_node_copy(node->data.equation.lhs);
            copy->data.equation.rhs = formula_node_copy(node->data.equation.rhs);
            break;

        case NODE_COORDINATE_LIST:
            copy->data.coord_list.coord_count = node->data.coord_list.coord_count;
            if (node->data.coord_list.coord_count > 0 && node->data.coord_list.coords) {
                copy->data.coord_list.coords = lv00_malloc(
                    (size_t) node->data.coord_list.coord_count * sizeof(FormulaNode *));
                if (copy->data.coord_list.coords) {
                    for (int i = 0; i < node->data.coord_list.coord_count; i++)
                        copy->data.coord_list.coords[i] = formula_node_copy(node->data.coord_list.coords[i]);
                }
            }
            break;

        case NODE_GEOM_POINT:
            if (node->data.geom_point.name)
                copy->data.geom_point.name = lv00_strdup(node->data.geom_point.name);
            copy->data.geom_point.coords = formula_node_copy(node->data.geom_point.coords);
            break;

        case NODE_GEOM_SEGMENT:
            if (node->data.geom_segment.name)
                copy->data.geom_segment.name = lv00_strdup(node->data.geom_segment.name);
            copy->data.geom_segment.endpoint1 = formula_node_copy(node->data.geom_segment.endpoint1);
            copy->data.geom_segment.endpoint2 = formula_node_copy(node->data.geom_segment.endpoint2);
            break;

        case NODE_GEOM_LINE:
            if (node->data.geom_line.name)
                copy->data.geom_line.name = lv00_strdup(node->data.geom_line.name);
            copy->data.geom_line.point1 = formula_node_copy(node->data.geom_line.point1);
            copy->data.geom_line.point2 = formula_node_copy(node->data.geom_line.point2);
            copy->data.geom_line.equation = formula_node_copy(node->data.geom_line.equation);
            break;

        case NODE_GEOM_CIRCLE:
            if (node->data.geom_circle.name)
                copy->data.geom_circle.name = lv00_strdup(node->data.geom_circle.name);
            copy->data.geom_circle.center = formula_node_copy(node->data.geom_circle.center);
            copy->data.geom_circle.radius = formula_node_copy(node->data.geom_circle.radius);
            copy->data.geom_circle.equation = formula_node_copy(node->data.geom_circle.equation);
            break;

        case NODE_GEOM_TRIANGLE:
            if (node->data.geom_triangle.name)
                copy->data.geom_triangle.name = lv00_strdup(node->data.geom_triangle.name);
            copy->data.geom_triangle.vertex1 = formula_node_copy(node->data.geom_triangle.vertex1);
            copy->data.geom_triangle.vertex2 = formula_node_copy(node->data.geom_triangle.vertex2);
            copy->data.geom_triangle.vertex3 = formula_node_copy(node->data.geom_triangle.vertex3);
            break;

        case NODE_GEOM_POLYGON:
            if (node->data.geom_polygon.name)
                copy->data.geom_polygon.name = lv00_strdup(node->data.geom_polygon.name);
            copy->data.geom_polygon.vertex_count = node->data.geom_polygon.vertex_count;
            if (node->data.geom_polygon.vertex_count > 0 && node->data.geom_polygon.vertices) {
                copy->data.geom_polygon.vertices = lv00_malloc(
                    (size_t) node->data.geom_polygon.vertex_count * sizeof(FormulaNode *));
                if (copy->data.geom_polygon.vertices) {
                    for (int i = 0; i < node->data.geom_polygon.vertex_count; i++)
                        copy->data.geom_polygon.vertices[i] = formula_node_copy(node->data.geom_polygon.vertices[i]);
                }
            }
            break;

        case NODE_GEOM_REGION:
            if (node->data.geom_region.name)
                copy->data.geom_region.name = lv00_strdup(node->data.geom_region.name);
            copy->data.geom_region.segment_count = node->data.geom_region.segment_count;
            if (node->data.geom_region.segment_count > 0 && node->data.geom_region.boundary_segments) {
                copy->data.geom_region.boundary_segments = lv00_malloc(
                    (size_t) node->data.geom_region.segment_count * sizeof(FormulaNode *));
                if (copy->data.geom_region.boundary_segments) {
                    for (int i = 0; i < node->data.geom_region.segment_count; i++)
                        copy->data.geom_region.boundary_segments[i] = formula_node_copy(node->data.geom_region.boundary_segments[i]);
                }
            }
            break;

        case NODE_GEOM_ARC:
            if (node->data.geom_arc.name)
                copy->data.geom_arc.name = lv00_strdup(node->data.geom_arc.name);
            copy->data.geom_arc.center = formula_node_copy(node->data.geom_arc.center);
            copy->data.geom_arc.radius = formula_node_copy(node->data.geom_arc.radius);
            copy->data.geom_arc.start_angle = formula_node_copy(node->data.geom_arc.start_angle);
            copy->data.geom_arc.end_angle = formula_node_copy(node->data.geom_arc.end_angle);
            break;

        case NODE_GEOM_VECTOR:
            if (node->data.geom_vector.name)
                copy->data.geom_vector.name = lv00_strdup(node->data.geom_vector.name);
            copy->data.geom_vector.start = formula_node_copy(node->data.geom_vector.start);
            copy->data.geom_vector.end = formula_node_copy(node->data.geom_vector.end);
            break;

        case NODE_CONSTRAINT_PERPENDICULAR:
        case NODE_CONSTRAINT_PARALLEL:
        case NODE_CONSTRAINT_MIDPOINT:
        case NODE_CONSTRAINT_BISECTOR:
        case NODE_CONSTRAINT_COLLINEAR:
        case NODE_CONSTRAINT_TANGENT:
        case NODE_CONSTRAINT_CONGRUENT:
        case NODE_CONSTRAINT_ANGLE:
            copy->data.constraint.participant_count = node->data.constraint.participant_count;
            if (node->data.constraint.participant_count > 0 && node->data.constraint.participants) {
                copy->data.constraint.participants = lv00_malloc(
                    (size_t) node->data.constraint.participant_count * sizeof(FormulaNode *));
                if (copy->data.constraint.participants) {
                    for (int i = 0; i < node->data.constraint.participant_count; i++)
                        copy->data.constraint.participants[i] = formula_node_copy(node->data.constraint.participants[i]);
                }
            }
            break;

        case NODE_COMPOUND:
            copy->data.compound.statement_count = node->data.compound.statement_count;
            if (node->data.compound.statement_count > 0 && node->data.compound.statements) {
                copy->data.compound.statements = lv00_malloc(
                    (size_t) node->data.compound.statement_count * sizeof(FormulaNode *));
                if (copy->data.compound.statements) {
                    for (int i = 0; i < node->data.compound.statement_count; i++)
                        copy->data.compound.statements[i] = formula_node_copy(node->data.compound.statements[i]);
                }
            }
            break;

        default:
            break;
    }

    return copy;
}
