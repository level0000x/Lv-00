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

/* ============================================================
 * 类型描述表 + 宏生成 copy/destroy（v3.4.0 起，替换原 36 个手写回调）
 * ============================================================ */

/**
 * @brief FormulaNode 变体字段类型描述表（copy/destroy 回调的单一事实来源）
 *
 * 新增节点类型流程：
 *   formula_parser.h 枚举 +1 → union +1 变体 → 本表 FORMULA_NODE_FIELDS_X +1 行
 *   →（宏自动生成 copy/destroy；kFormulaVTable 由 FORMULA_NODE_VTABLE_X 按枚举展开）
 *   → create/parse/sema 语义代码仍需手写；toString 在 formula_string.c 手写
 *   （已知 s_funcs 只覆盖 17/36，漏项输出 '?'，本次不改）。
 *
 * 字段 kind 语义：
 *   F_SCALAR(field)   — 标量：copy 直接赋值（等价 memcpy），destroy 无操作
 *   F_STR(field)      — char*（strdup 语义）：copy→lv_strdup（失败静默留 NULL），
 *                       destroy→lv_free
 *   F_CHILD(field)    — FormulaNode*：copy→formula_node_copy 深拷贝，
 *                       destroy→formula_node_destroy 递归释放
 *   F_ARRAY(arr,cnt)  — FormulaNode** + int 计数字段：
 *                       copy→lv_calloc + 逐元素深拷贝（失败静默留 NULL），
 *                       destroy→逐元素递归释放后 lv_free 数组
 *
 * 按「变体」而非「枚举值」组织（陷阱提示）：5 个二元运算共享 binary_op 变体、
 * 8 个一元运算共享 unary_op 变体、8 个几何约束共享 constraint 变体；
 * 枚举值 → 变体的映射见下方 FORMULA_NODE_VTABLE_X（36 行）。
 * 每行：x(回调名后缀, union 成员名, 字段列表...)
 */
#define FORMULA_NODE_FIELDS_X(x) \
    x(NUMBER, number, F_SCALAR(numerator), F_SCALAR(denominator), F_SCALAR(is_integer)) \
    x(VARIABLE, variable, F_STR(name)) \
    x(IDENTIFIER, identifier, F_STR(name)) \
    x(BINARY_OP, binary_op, F_CHILD(left), F_CHILD(right)) \
    x(UNARY_OP, unary_op, F_CHILD(operand)) \
    x(EQUATION, equation, F_CHILD(lhs), F_CHILD(rhs)) \
    x(COORD_LIST, coord_list, F_ARRAY(coords, coord_count)) \
    x(GEOM_POINT, geom_point, F_STR(name), F_CHILD(coords)) \
    x(GEOM_SEGMENT, geom_segment, F_STR(name), F_CHILD(endpoint1), F_CHILD(endpoint2)) \
    x(GEOM_LINE, geom_line, F_STR(name), F_CHILD(point1), F_CHILD(point2), F_CHILD(equation)) \
    x(GEOM_CIRCLE, geom_circle, F_STR(name), F_CHILD(center), F_CHILD(radius), F_CHILD(equation)) \
    x(GEOM_TRIANGLE, geom_triangle, F_STR(name), F_CHILD(vertex1), F_CHILD(vertex2), F_CHILD(vertex3)) \
    x(GEOM_POLYGON, geom_polygon, F_STR(name), F_ARRAY(vertices, vertex_count)) \
    x(GEOM_REGION, geom_region, F_STR(name), F_ARRAY(boundary_segments, segment_count)) \
    x(GEOM_ARC, geom_arc, F_STR(name), F_CHILD(center), F_CHILD(radius), F_CHILD(start_angle), F_CHILD(end_angle)) \
    x(GEOM_VECTOR, geom_vector, F_STR(name), F_CHILD(start), F_CHILD(end)) \
    x(CONSTRAINT, constraint, F_ARRAY(participants, participant_count)) \
    x(COMPOUND, compound, F_ARRAY(statements, statement_count))

/* 字段 kind 标记：展开为 (kind, 字段[, 计数字段]) 元组 */
#define F_SCALAR(field)     (S, field)
#define F_STR(field)        (T, field)
#define F_CHILD(field)      (C, field)
#define F_ARRAY(arr, count) (A, arr, count)

/* --- 小型预处理器 FOR_EACH（仅本文件使用，展开后 #undef） --- */
#define LV_CAT_(a, b) a##b
#define LV_CAT(a, b) LV_CAT_(a, b)

#define LV_FEA_ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, N, ...) N
#define LV_FEA_NARG_(...) LV_FEA_ARG_N(__VA_ARGS__)
#define LV_FEA_NARG(...) LV_FEA_NARG_(__VA_ARGS__, 8, 7, 6, 5, 4, 3, 2, 1, 0)

#define LV_FEA_N(N, M, V, ...) LV_CAT(LV_FEA_, N)(M, V, __VA_ARGS__)
#define LV_FOR_EACH_ARG(M, V, ...) LV_FEA_N(LV_FEA_NARG(__VA_ARGS__), M, V, __VA_ARGS__)

#define LV_FEA_0(M, V, ...)
#define LV_FEA_1(M, V, a, ...) M(V, a)
#define LV_FEA_2(M, V, a, ...) M(V, a) LV_FEA_1(M, V, __VA_ARGS__)
#define LV_FEA_3(M, V, a, ...) M(V, a) LV_FEA_2(M, V, __VA_ARGS__)
#define LV_FEA_4(M, V, a, ...) M(V, a) LV_FEA_3(M, V, __VA_ARGS__)
#define LV_FEA_5(M, V, a, ...) M(V, a) LV_FEA_4(M, V, __VA_ARGS__)
#define LV_FEA_6(M, V, a, ...) M(V, a) LV_FEA_5(M, V, __VA_ARGS__)
#define LV_FEA_7(M, V, a, ...) M(V, a) LV_FEA_6(M, V, __VA_ARGS__)
#define LV_FEA_8(M, V, a, ...) M(V, a) LV_FEA_7(M, V, __VA_ARGS__)

/* 元组拆分：取 kind（首元素）与剩余字段参数 */
#define LV_TUPLE_HEAD(TUPLE) LV_TUPLE_HEAD_ TUPLE
#define LV_TUPLE_HEAD_(head, ...) head
#define LV_TUPLE_TAIL(TUPLE) LV_TUPLE_TAIL_ TUPLE
#define LV_TUPLE_TAIL_(head, ...) __VA_ARGS__

/* destroy 逐字段展开（MEMBER 为 union 成员名，即 node->data.<MEMBER>）
 * 注意：## 粘贴产生的函数宏名必须经 LV_DF_CALL 转发一层再调用——同一趟重扫中
 * 直接 `LV_CAT(...)(...)` 时 GCC 不会把粘贴结果当宏调用处理（数组元组尤甚）。 */
#define LV_DF_APPLY(MEMBER, TUPLE) \
    LV_DF_CALL(LV_CAT(LV_DF_, LV_TUPLE_HEAD(TUPLE)), MEMBER, LV_TUPLE_TAIL(TUPLE))
#define LV_DF_CALL(FN, ...) FN(__VA_ARGS__)
#define LV_DF_T(MEMBER, field) lv_free((void **) &node->data.MEMBER.field);
#define LV_DF_S(MEMBER, field) /* 标量：destroy 无操作 */
#define LV_DF_C(MEMBER, field) formula_node_destroy(node->data.MEMBER.field);
#define LV_DF_A(MEMBER, arr, count) \
    for (int i = 0; i < node->data.MEMBER.count; i++) \
        formula_node_destroy(node->data.MEMBER.arr[i]); \
    lv_free((void **) &node->data.MEMBER.arr);

/* copy 逐字段展开（保持原语义：分配失败静默留 NULL） */
#define LV_CF_APPLY(MEMBER, TUPLE) \
    LV_CF_CALL(LV_CAT(LV_CF_, LV_TUPLE_HEAD(TUPLE)), MEMBER, LV_TUPLE_TAIL(TUPLE))
#define LV_CF_CALL(FN, ...) FN(__VA_ARGS__)
#define LV_CF_T(MEMBER, field) \
    if (src->data.MEMBER.field) \
        dst->data.MEMBER.field = lv_strdup(src->data.MEMBER.field);
#define LV_CF_S(MEMBER, field) dst->data.MEMBER.field = src->data.MEMBER.field;
#define LV_CF_C(MEMBER, field) dst->data.MEMBER.field = formula_node_copy(src->data.MEMBER.field);
#define LV_CF_A(MEMBER, arr, count) \
    dst->data.MEMBER.count = src->data.MEMBER.count; \
    if (src->data.MEMBER.count > 0 && src->data.MEMBER.arr) { \
        dst->data.MEMBER.arr = \
            lv_calloc((size_t) src->data.MEMBER.count, sizeof(FormulaNode *)); \
        if (dst->data.MEMBER.arr) { \
            for (int i = 0; i < src->data.MEMBER.count; i++) \
                dst->data.MEMBER.arr[i] = formula_node_copy(src->data.MEMBER.arr[i]); \
        } \
    }

/* 变体级生成器：FN=回调名后缀（destroy_##FN），MEMBER=union 成员名 */
#define LV_GEN_DESTROY(FN, MEMBER, ...) \
    static void destroy_##FN(FormulaNode *node) { \
        (void)node; \
        LV_FOR_EACH_ARG(LV_DF_APPLY, MEMBER, __VA_ARGS__) \
    }

#define LV_GEN_COPY(FN, MEMBER, ...) \
    static void copy_##FN(const FormulaNode *src, FormulaNode *dst) { \
        LV_FOR_EACH_ARG(LV_CF_APPLY, MEMBER, __VA_ARGS__) \
    }

#define LV_GEN_BOTH(FN, MEMBER, ...) \
    LV_GEN_DESTROY(FN, MEMBER, __VA_ARGS__) \
    LV_GEN_COPY(FN, MEMBER, __VA_ARGS__)

/* 生成 18 变体 × (destroy + copy) = 36 个回调函数 */
FORMULA_NODE_FIELDS_X(LV_GEN_BOTH)

#undef LV_GEN_BOTH
#undef LV_GEN_COPY
#undef LV_GEN_DESTROY
#undef LV_CF_CALL
#undef LV_CF_A
#undef LV_CF_S
#undef LV_CF_C
#undef LV_CF_T
#undef LV_CF_APPLY
#undef LV_DF_CALL
#undef LV_DF_A
#undef LV_DF_S
#undef LV_DF_C
#undef LV_DF_T
#undef LV_DF_APPLY
#undef LV_TUPLE_TAIL_
#undef LV_TUPLE_TAIL
#undef LV_TUPLE_HEAD_
#undef LV_TUPLE_HEAD
#undef LV_FEA_8
#undef LV_FEA_7
#undef LV_FEA_6
#undef LV_FEA_5
#undef LV_FEA_4
#undef LV_FEA_3
#undef LV_FEA_2
#undef LV_FEA_1
#undef LV_FEA_0
#undef LV_FOR_EACH_ARG
#undef LV_FEA_N
#undef LV_FEA_NARG
#undef LV_FEA_NARG_
#undef LV_FEA_ARG_N
#undef LV_CAT
#undef LV_CAT_
#undef F_ARRAY
#undef F_CHILD
#undef F_STR
#undef F_SCALAR

/* --- VTable 查找表（36 行，枚举 → 变体映射宏生成 designated initializer） --- */
#define FORMULA_NODE_VTABLE_X(x) \
    x(NODE_NUMBER, NUMBER) \
    x(NODE_VARIABLE, VARIABLE) \
    x(NODE_IDENTIFIER, IDENTIFIER) \
    x(NODE_BINARY_OP_ADD, BINARY_OP) \
    x(NODE_BINARY_OP_SUB, BINARY_OP) \
    x(NODE_BINARY_OP_MUL, BINARY_OP) \
    x(NODE_BINARY_OP_DIV, BINARY_OP) \
    x(NODE_BINARY_OP_POW, BINARY_OP) \
    x(NODE_UNARY_OP_NEG, UNARY_OP) \
    x(NODE_UNARY_OP_SQRT, UNARY_OP) \
    x(NODE_UNARY_OP_SIN, UNARY_OP) \
    x(NODE_UNARY_OP_COS, UNARY_OP) \
    x(NODE_UNARY_OP_TAN, UNARY_OP) \
    x(NODE_UNARY_OP_ABS, UNARY_OP) \
    x(NODE_UNARY_OP_LN, UNARY_OP) \
    x(NODE_UNARY_OP_LOG, UNARY_OP) \
    x(NODE_EQUATION, EQUATION) \
    x(NODE_COORDINATE_LIST, COORD_LIST) \
    x(NODE_GEOM_POINT, GEOM_POINT) \
    x(NODE_GEOM_SEGMENT, GEOM_SEGMENT) \
    x(NODE_GEOM_LINE, GEOM_LINE) \
    x(NODE_GEOM_CIRCLE, GEOM_CIRCLE) \
    x(NODE_GEOM_TRIANGLE, GEOM_TRIANGLE) \
    x(NODE_GEOM_POLYGON, GEOM_POLYGON) \
    x(NODE_GEOM_REGION, GEOM_REGION) \
    x(NODE_GEOM_ARC, GEOM_ARC) \
    x(NODE_GEOM_VECTOR, GEOM_VECTOR) \
    x(NODE_CONSTRAINT_PERPENDICULAR, CONSTRAINT) \
    x(NODE_CONSTRAINT_PARALLEL, CONSTRAINT) \
    x(NODE_CONSTRAINT_MIDPOINT, CONSTRAINT) \
    x(NODE_CONSTRAINT_BISECTOR, CONSTRAINT) \
    x(NODE_CONSTRAINT_COLLINEAR, CONSTRAINT) \
    x(NODE_CONSTRAINT_TANGENT, CONSTRAINT) \
    x(NODE_CONSTRAINT_CONGRUENT, CONSTRAINT) \
    x(NODE_CONSTRAINT_ANGLE, CONSTRAINT) \
    x(NODE_COMPOUND, COMPOUND)

#define LV_VTABLE_ROW(ENUM, FN) [ENUM] = { destroy_##FN, copy_##FN },
static const FormulaNodeVTable kFormulaVTable[] = {
    FORMULA_NODE_VTABLE_X(LV_VTABLE_ROW)
};

#undef LV_VTABLE_ROW
#undef FORMULA_NODE_VTABLE_X
#undef FORMULA_NODE_FIELDS_X

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
