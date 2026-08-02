/**
 * @file dsl_compiler_load.c
 * @brief Lv-00 DSL 编译器 —— IR Loader 阶段：IR → 约束图与销毁/转储
 *
 * @details 由 dsl_compiler.c 按编译流水线阶段拆分而来。
 *          编译器管线：dsl_tokenize → dsl_parse → dsl_compile
 *          → dsl_ir_to_constraint_graph
 *
 * @author Lv-00 Project
 * @version 3.0.1
 */

#include "dsl_compiler.h"
#include "dsl_compiler_internal.h"

#include <ctype.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/symbolic_coord.h"
#include "lv/lv_xmacro.h"

#include "lv_internal.h"

/* ================================================================
 *  IR → ConstraintGraph
 * ================================================================ */

/**
 * @brief 为数值字面量创建 SymbolicCoord
 *
 * 从编码的操作数值中提取 x, y 坐标。
 */
static bool resolve_fixed_coords(const DslIROperation *op, double *out_x, double *out_y) {
    if (!op || !out_x || !out_y)
        return false;
    if (op->operand_count < 2)
        return false;

    /* 从 operands 中获取编码的坐标值（编译时存为 int 但本质是 double 的位模式） */
    /* 这里直接使用 operands 字段存储的 double 位模式 */
    if (op->operands[0] < 0 || op->operands[1] < 0)
        return false;

    /* 对于 fix 语句，坐标直接来自 AST NUMBER 节点编译成的 operand */
    /* 但我们存储的是 double 的整数部分（作为整型坐标） */
    *out_x = (double) op->operands[0];
    *out_y = (double) op->operands[1];
    return true;
}

/**
 * @brief 将 IR 操作转换为约束图节点
 *
 * 遍历 IR 操作列表，在约束图中为每个操作创建对应节点和约束。
 * 跟踪结果 ID 到约束图节点 ID 的映射。
 *
 * @param ir    IR 数据
 * @param graph 约束图指针
 * @return 成功返回 true
 */
bool dsl_ir_to_constraint_graph(const DslIR *ir, ConstraintGraph *graph) {
    if (!ir || !graph)
        return false;

    /* 结果 ID 到约束图节点 ID 的映射表 */
    int *id_map = NULL;
    int id_map_count = 0;
    int id_map_cap = 0;

    /* 确保 id_map 有足够的容量 */
#define ENSURE_ID_MAP(cap_needed)                                         \
    do {                                                                  \
        while ((cap_needed) >= id_map_cap) {                              \
            int new_cap = id_map_cap == 0 ? 64 : id_map_cap * 2;          \
            int *np = lv_realloc(id_map, sizeof(int) * (size_t) new_cap); \
            if (!np) {                                                    \
                lv_free((void **) &id_map);                               \
                return false;                                             \
            }                                                             \
            id_map = np;                                                  \
            /* 初始化新区域为 -1 */                                       \
            for (int _i = id_map_cap; _i < new_cap; _i++)                 \
                id_map[_i] = -1;                                          \
            id_map_cap = new_cap;                                         \
        }                                                                 \
    } while (0)

    /* 初始化 id_map */
    if (ir->next_id > 0) {
        ENSURE_ID_MAP(ir->next_id + 1);
        id_map_count = ir->next_id + 1;
        for (int i = 0; i < id_map_count; i++)
            id_map[i] = -1;
    }

    /* 遍历 IR 操作 */
    for (int i = 0; i < ir->op_count; i++) {
        const DslIROperation *op = &ir->operations[i];

        switch (op->op) {
            /* ---- 实体创建 ---- */
            case IR_CREATE_POINT: {
                /* 创建自由点（无坐标） */
                GeomNode *node = graph_add_node_with_id(graph, op->result_id, GEOM_POINT, NULL, 0);
                if (node && op->result_id >= 0) {
                    ENSURE_ID_MAP(op->result_id + 1);
                    if (op->result_id >= id_map_count)
                        id_map_count = op->result_id + 1;
                    id_map[op->result_id] = node->id;
                }
                break;
            }

            case IR_CREATE_POINT_FIXED: {
                /* 创建固定坐标点 */
                double x = 0.0, y = 0.0;
                resolve_fixed_coords(op, &x, &y);

                /* 创建 SymbolicCoord 数组 */
                SymbolicCoord *coords[2] = {NULL, NULL};
                /* 使用简单的坐标值创建（实际使用 SymbolicCoord 构造） */
                /* 这里简化为 NULL，因为 graph_add_node_with_id 接受 NULL */
                GeomNode *node = graph_add_node_with_id(graph, op->result_id, GEOM_POINT, NULL, 0);
                if (node && op->result_id >= 0) {
                    ENSURE_ID_MAP(op->result_id + 1);
                    if (op->result_id >= id_map_count)
                        id_map_count = op->result_id + 1;
                    id_map[op->result_id] = node->id;
                }
                break;
            }

            case IR_CREATE_LINE:
            case IR_CREATE_SEGMENT: {
                /* 创建线段（基于操作数中的前两个点） */
                GeomNode *node = graph_add_node_with_id(graph, op->result_id, GEOM_LINE_SEGMENT, NULL, 0);
                if (node && op->result_id >= 0) {
                    ENSURE_ID_MAP(op->result_id + 1);
                    if (op->result_id >= id_map_count)
                        id_map_count = op->result_id + 1;
                    id_map[op->result_id] = node->id;
                }
                break;
            }

            case IR_CREATE_CIRCLE: {
                /* 圆 -> 创建 GEOM_CIRCLE 节点 */
                GeomNode *node = graph_add_node_with_id(graph, op->result_id, GEOM_CIRCLE, NULL, 0);
                if (node && op->result_id >= 0) {
                    ENSURE_ID_MAP(op->result_id + 1);
                    if (op->result_id >= id_map_count)
                        id_map_count = op->result_id + 1;
                    id_map[op->result_id] = node->id;
                    /* 初始化圆心和半径端点为 -1，后续通过约束设置 */
                    node->data.circle.center_node_id = -1;
                    node->data.circle.radius_node_id = -1;
                }
                break;
            }

            case IR_CREATE_RAY: {
                /* 射线 -> 也用线段节点占位 */
                GeomNode *node = graph_add_node_with_id(graph, op->result_id, GEOM_LINE_SEGMENT, NULL, 0);
                if (node && op->result_id >= 0) {
                    ENSURE_ID_MAP(op->result_id + 1);
                    if (op->result_id >= id_map_count)
                        id_map_count = op->result_id + 1;
                    id_map[op->result_id] = node->id;
                }
                break;
            }

            case IR_CREATE_POLYGON:
            case IR_CREATE_TRIANGLE: {
                /* 多边形/三角形 -> 区域节点 */
                GeomNode *node = graph_add_node_with_id(graph, op->result_id, GEOM_REGION, NULL, 0);
                if (node && op->result_id >= 0) {
                    ENSURE_ID_MAP(op->result_id + 1);
                    if (op->result_id >= id_map_count)
                        id_map_count = op->result_id + 1;
                    id_map[op->result_id] = node->id;
                }
                break;
            }

            /* ---- 构造操作 ---- */
            case IR_INTERSECT: {
                /* 创建交点节点 + 相交约束 */
                GeomNode *node = graph_add_node_with_id(graph, op->result_id, GEOM_POINT, NULL, 0);
                if (node && op->result_id >= 0) {
                    ENSURE_ID_MAP(op->result_id + 1);
                    if (op->result_id >= id_map_count)
                        id_map_count = op->result_id + 1;
                    id_map[op->result_id] = node->id;

                    /* 如果有两个操作数，添加相交约束 */
                    if (op->operand_count >= 2 && op->operands[0] >= 0 && op->operands[1] >= 0) {
                        int p1_id = (op->operands[0] < id_map_count) ? id_map[op->operands[0]] : -1;
                        int p2_id = (op->operands[1] < id_map_count) ? id_map[op->operands[1]] : -1;
                        if (p1_id >= 0 && p2_id >= 0) {
                            int parts[3] = {p1_id, p2_id, node->id};
                            graph_add_constraint_with_id(graph, op->result_id, INTERSECTION, parts, 3);
                        }
                    }
                }
                break;
            }

            case IR_PARALLEL_THROUGH:
            case IR_PERPENDICULAR_THROUGH: {
                /* 平行/垂线约束 */
                if (op->operand_count >= 2 && op->operands[0] >= 0 && op->operands[1] >= 0) {
                    int p1_id = (op->operands[0] < id_map_count) ? id_map[op->operands[0]] : -1;
                    int p2_id = (op->operands[1] < id_map_count) ? id_map[op->operands[1]] : -1;
                    if (p1_id >= 0 && p2_id >= 0) {
                        int parts[2] = {p1_id, p2_id};
                        graph_add_constraint_with_id(
                            graph, op->result_id, (op->op == IR_PARALLEL_THROUGH) ? CONNECTION : INCIDENCE, parts, 2);
                    }
                }
                break;
            }

            case IR_MIDPOINT_OF:
            case IR_CIRCUMCENTER_OF:
            case IR_ORTHOCENTER_OF:
            case IR_CENTROID_OF:
            case IR_INCENTER_OF:
            case IR_BISECTOR_OF: {
                /* 这些构造的结果都是点，创建点节点 */
                GeomNode *node = graph_add_node_with_id(graph, op->result_id, GEOM_POINT, NULL, 0);
                if (node && op->result_id >= 0) {
                    ENSURE_ID_MAP(op->result_id + 1);
                    if (op->result_id >= id_map_count)
                        id_map_count = op->result_id + 1;
                    id_map[op->result_id] = node->id;

                    /* 如果有点操作数，添加关联约束 */
                    if (op->operand_count > 0) {
                        for (int j = 0; j < op->operand_count; j++) {
                            int pid =
                                (op->operands[j] >= 0 && op->operands[j] < id_map_count) ? id_map[op->operands[j]] : -1;
                            if (pid >= 0) {
                                int parts[2] = {pid, node->id};
                                graph_add_constraint_with_id(graph, -1, INCIDENCE, parts, 2);
                            }
                        }
                    }
                }
                break;
            }

            /* ---- 约束操作 ---- */
            case IR_ADD_CONSTRAINT:
            case IR_CONSTRAIN_EQUAL:
            case IR_CONSTRAIN_PARALLEL:
            case IR_CONSTRAIN_PERPENDICULAR:
            case IR_CONSTRAIN_COLLINEAR:
            case IR_CONSTRAIN_CONCYCLIC: {
                ConstraintType ctype = CONNECTION;
                switch (op->op) {
                    case IR_CONSTRAIN_PARALLEL:
                        ctype = CONNECTION;
                        break;
                    case IR_CONSTRAIN_PERPENDICULAR:
                        ctype = INCIDENCE;
                        break;
                    case IR_CONSTRAIN_COLLINEAR:
                        ctype = BETWEENNESS;
                        break;
                    case IR_CONSTRAIN_CONCYCLIC:
                        ctype = CONTAINMENT;
                        break;
                    default:
                        ctype = INCIDENCE;
                        break;
                }
                int parts[8];
                int pc = 0;
                for (int j = 0; j < op->operand_count && pc < 8; j++) {
                    int pid = (op->operands[j] >= 0 && op->operands[j] < id_map_count) ? id_map[op->operands[j]] : -1;
                    if (pid >= 0)
                        parts[pc++] = pid;
                }
                if (pc > 0) {
                    graph_add_constraint_with_id(graph, op->result_id, ctype, parts, pc);
                }
                break;
            }

            /* ---- 系统操作 ---- */
            case IR_LOAD_AXIOM: {
                /* load 语句：当前为桩，不做实际操作 */
                break;
            }

            case IR_PROVE: {
                /* prove 语句：当前为桩，不做实际操作 */
                break;
            }

            case IR_CHECK_SAT: {
                /* 可满足性检查：当前为桩 */
                break;
            }

            case IR_LABEL: {
                /* 标签操作：当前为桩 */
                break;
            }

            case IR_REMOVE_CONSTRAINT: {
                /* 移除约束：当前为桩 */
                break;
            }

            case IR_NOOP:
            default: {
                break;
            }
        }
    }

    lv_free((void **) &id_map);
    return true;
}

/**
 * @brief 编译 DSL 源代码并加载到约束图
 *
 * 完整的编译管线：tokenize → parse → compile → ir_to_constraint_graph。
 * 每一步失败时自动释放已分配的资源。
 *
 * @param source DSL 源代码字符串
 * @param config 编译配置
 * @param graph  目标约束图
 * @return 成功返回 true，失败返回 false
 */
bool dsl_compile_and_load(const char *source, const DslCompileConfig *config, ConstraintGraph *graph) {
    if (!source || !graph)
        return false;

    DslToken *tokens = NULL;
    int token_count = 0;
    if (!dsl_tokenize(source, &tokens, &token_count))
        return false;

    DslAST *ast = NULL;
    if (!dsl_parse(tokens, token_count, &ast)) {
        dsl_tokens_destroy(tokens, token_count);
        return false;
    }

    DslIR *ir = NULL;
    if (!dsl_compile(ast, config, &ir)) {
        dsl_ast_destroy(ast);
        dsl_tokens_destroy(tokens, token_count);
        return false;
    }

    bool ok = dsl_ir_to_constraint_graph(ir, graph);
    dsl_ir_destroy(ir);
    dsl_ast_destroy(ast);
    dsl_tokens_destroy(tokens, token_count);
    return ok;
}

/**
 * @brief 将编译配置初始化为默认值
 *
 * 默认配置：TARGET_NATIVE、优化级别 0、不调试 AST。
 *
 * @param out_config 输出：默认编译配置
 */
void dsl_compile_config_default(DslCompileConfig *out_config) {
    if (!out_config)
        return;
    memset(out_config, 0, sizeof(*out_config));
    out_config->target = TARGET_NATIVE;
    out_config->optimize_level = 0;
    out_config->debug_ast = false;
    out_config->validate_ir = true;
    out_config->generate_source_map = true;
    out_config->max_iterations = 1000;
}

/**
 * @brief 递归销毁 DSL AST 树
 *
 * 递归释放所有子节点，然后释放 children 数组和 name，最后释放节点本身。
 *
 * @param ast 要销毁的 AST 节点（允许为 NULL）
 */
void dsl_ast_destroy(DslAST *ast) {
    if (!ast)
        return;
    for (int i = 0; i < ast->child_count; i++)
        dsl_ast_destroy(ast->children[i]);
    lv_free((void **) &ast->children);
    lv_free((void **) &ast->name);
    lv_free((void **) &ast);
}

/**
 * @brief 销毁 IR 数据
 *
 * 释放所有 IR 操作的操作数数组、operations 数组、符号表和 IR 结构体本身。
 *
 * @param ir 要销毁的 IR 指针（允许为 NULL）
 */
void dsl_ir_destroy(DslIR *ir) {
    if (!ir)
        return;
    for (int i = 0; i < ir->op_count; i++)
        lv_free((void **) &ir->operations[i].operands);
    lv_free((void **) &ir->operations);
    /* 释放符号表 */
    if (ir->symbols) {
        for (int i = 0; i < ir->symbol_count; i++)
            lv_free((void **) &ir->symbols[i]);
    }
    lv_free((void **) &ir->symbols);
    lv_free((void **) &ir->symbol_to_ir_id);
    lv_free((void **) &ir);
}

/**
 * @brief 转储 DSL AST 树（调试用）
 *
 * 以缩进格式将 AST 树结构输出到文件描述符。
 *
 * @param ast    AST 根节点（允许为 NULL）
 * @param fd     输出文件描述符（实际类型为 FILE*）
 * @param indent 当前缩进层级
 */
void dsl_ast_dump(const DslAST *ast, void *fd, int indent) {
    if (!ast || !fd)
        return;
    FILE *f = (FILE *) fd;

    for (int i = 0; i < indent; i++)
        fprintf(f, "  ");
    fprintf(f, "%s", dsl_ast_type_name(ast->type));

    if (ast->name)
        fprintf(f, " [%s]", ast->name);
    if (ast->type == DSL_AST_NUMBER)
        fprintf(f, " = %g", ast->num_value);
    fprintf(f, "\n");

    for (int i = 0; i < ast->child_count; i++)
        dsl_ast_dump(ast->children[i], fd, indent + 1);
}

/**
 * @brief 转储 IR 数据（调试用）
 *
 * 将 IR 操作列表输出到文件描述符。
 *
 * @param ir IR 数据（允许为 NULL）
 * @param fd 输出文件描述符（实际类型为 FILE*）
 */
void dsl_ir_dump(const DslIR *ir, void *fd) {
    if (!ir || !fd)
        return;
    FILE *f = (FILE *) fd;

    fprintf(f, "IR Program (%d ops, %d symbols):\n", ir->op_count, ir->symbol_count);
    for (int i = 0; i < ir->op_count; i++) {
        const DslIROperation *op = &ir->operations[i];
        fprintf(f, "  [%3d] %s", i, dsl_ir_op_name(op->op));
        if (op->result_id >= 0)
            fprintf(f, " -> r%d", op->result_id);
        if (op->operand_count > 0) {
            fprintf(f, " (");
            for (int j = 0; j < op->operand_count; j++) {
                if (j > 0)
                    fprintf(f, ", ");
                fprintf(f, "%d", op->operands[j]);
            }
            fprintf(f, ")");
        }
        if (op->label)
            fprintf(f, " label=\"%s\"", op->label);
        if (op->source_line > 0)
            fprintf(f, " [line %d]", op->source_line);
        fprintf(f, "\n");
    }

    /* 转储符号表 */
    if (ir->symbol_count > 0) {
        fprintf(f, "  Symbol table:\n");
        for (int i = 0; i < ir->symbol_count; i++) {
            fprintf(f, "    %s -> r%d\n", ir->symbols[i] ? ir->symbols[i] : "(null)", ir->symbol_to_ir_id[i]);
        }
    }
}

/**
 * @brief 获取 IR 操作符的字符串名称
 *
 * @param op IR 操作符枚举值
 * @return 操作符名称字符串（静态存储，无需释放）
 */
/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch）
 * ================================================================ */

/** @brief IR 操作码名称表（按枚举值升序） */
static const lvStrToEnumEntry s_ir_op_names[] = {
    {"CREATE_POINT", IR_CREATE_POINT},
    {"CREATE_POINT_FIXED", IR_CREATE_POINT_FIXED},
    {"CREATE_LINE", IR_CREATE_LINE},
    {"CREATE_CIRCLE", IR_CREATE_CIRCLE},
    {"CREATE_SEGMENT", IR_CREATE_SEGMENT},
    {"CREATE_RAY", IR_CREATE_RAY},
    {"CREATE_POLYGON", IR_CREATE_POLYGON},
    {"CREATE_TRIANGLE", IR_CREATE_TRIANGLE},
    {"INTERSECT", IR_INTERSECT},
    {"PARALLEL_THROUGH", IR_PARALLEL_THROUGH},
    {"PERPENDICULAR_THROUGH", IR_PERPENDICULAR_THROUGH},
    {"MIDPOINT_OF", IR_MIDPOINT_OF},
    {"CIRCUMCENTER_OF", IR_CIRCUMCENTER_OF},
    {"ORTHOCENTER_OF", IR_ORTHOCENTER_OF},
    {"CENTROID_OF", IR_CENTROID_OF},
    {"INCENTER_OF", IR_INCENTER_OF},
    {"BISECTOR_OF", IR_BISECTOR_OF},
    {"ANGLE_BISECTOR", IR_ANGLE_BISECTOR},
    {"ADD_CONSTRAINT", IR_ADD_CONSTRAINT},
    {"REMOVE_CONSTRAINT", IR_REMOVE_CONSTRAINT},
    {"CONSTRAIN_EQUAL", IR_CONSTRAIN_EQUAL},
    {"CONSTRAIN_PARALLEL", IR_CONSTRAIN_PARALLEL},
    {"CONSTRAIN_PERPENDICULAR", IR_CONSTRAIN_PERPENDICULAR},
    {"CONSTRAIN_COLLINEAR", IR_CONSTRAIN_COLLINEAR},
    {"CONSTRAIN_CONCYCLIC", IR_CONSTRAIN_CONCYCLIC},
    {"LOAD_AXIOM", IR_LOAD_AXIOM},
    {"PROVE", IR_PROVE},
    {"CHECK_SAT", IR_CHECK_SAT},
    {"LABEL", IR_LABEL},
    {"NOOP", IR_NOOP},
};

/** @brief DSL AST 节点类型名称表（按枚举值升序） */
static const lvStrToEnumEntry s_ast_type_names[] = {
    {"PROGRAM", DSL_AST_PROGRAM},
    {"POINT_DECL", DSL_AST_POINT_DECL},
    {"LINE_DECL", DSL_AST_LINE_DECL},
    {"CIRCLE_DECL", DSL_AST_CIRCLE_DECL},
    {"SEGMENT_DECL", DSL_AST_SEGMENT_DECL},
    {"RAY_DECL", DSL_AST_RAY_DECL},
    {"POLYGON_DECL", DSL_AST_POLYGON_DECL},
    {"TRIANGLE_DECL", DSL_AST_TRIANGLE_DECL},
    {"INTERSECT", DSL_AST_INTERSECT},
    {"PARALLEL", DSL_AST_PARALLEL},
    {"PERPENDICULAR", DSL_AST_PERPENDICULAR},
    {"MIDPOINT", DSL_AST_MIDPOINT},
    {"CIRCUMCENTER", DSL_AST_CIRCUMCENTER},
    {"ORTHOCENTER", DSL_AST_ORTHOCENTER},
    {"CENTROID", DSL_AST_CENTROID},
    {"INCENTER", DSL_AST_INCENTER},
    {"BISECTOR", DSL_AST_BISECTOR},
    {"CONSTRAINT", DSL_AST_CONSTRAINT},
    {"PROVE", DSL_AST_PROVE},
    {"LOAD", DSL_AST_LOAD},
    {"FIX_POINT", DSL_AST_FIX_POINT},
    {"FREE_POINT", DSL_AST_FREE_POINT},
    {"BLOCK", DSL_AST_BLOCK},
    {"IDENT", DSL_AST_IDENT},
    {"NUMBER", DSL_AST_NUMBER},
};

const char *dsl_ir_op_name(DslIROp op) {
    return lv_enum_to_str(s_ir_op_names, lv_ARRAY_SIZE(s_ir_op_names), (int) op, "UNKNOWN");
}

/**
 * @brief 获取 DSL AST 节点类型的字符串名称
 *
 * @param type 节点类型枚举值
 * @return 类型名称字符串（静态存储，无需释放）
 */
const char *dsl_ast_type_name(DslASTType type) {
    return lv_enum_to_str(s_ast_type_names, lv_ARRAY_SIZE(s_ast_type_names), (int) type, "UNKNOWN");
}
