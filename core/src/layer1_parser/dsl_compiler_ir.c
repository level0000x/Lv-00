/**
 * @file dsl_compiler_ir.c
 * @brief Lv-00 DSL 编译器 —— Compiler 阶段：DSL AST → IR 中间表示
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
 *  Compiler：AST → IR
 * ================================================================ */

/**
 * @brief 向 IR 添加符号（名称 → IR ID 映射）
 */
static int ir_add_symbol(DslIR *ir, const char *name, int result_id) {
    if (!ir || !name)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "IR or name is NULL");

    ENSURE_CAP(ir->symbols, ir->symbol_count, ir->symbol_capacity, sizeof(char *), -1);
    ENSURE_CAP(ir->symbol_to_ir_id, ir->symbol_count, ir->symbol_capacity, sizeof(int), -1);

    ir->symbols[ir->symbol_count] = lv_strdup(name);
    ir->symbol_to_ir_id[ir->symbol_count] = result_id;
    int idx = ir->symbol_count;
    ir->symbol_count++;
    return idx;
}

/**
 * @brief 在 IR 符号表中查找名称，返回 IR 操作索引
 */
static int ir_find_symbol(const DslIR *ir, const char *name) {
    if (!ir || !name)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "IR or name is NULL");
    for (int i = 0; i < ir->symbol_count; i++) {
        if (ir->symbols[i] && strcmp(ir->symbols[i], name) == 0)
            return ir->symbol_to_ir_id[i];
    }
    return -1;
}

/**
 * @brief 向 IR 添加操作
 */
static int ir_add_op(DslIR *ir, DslIROp op, int result_id, const int *operands, int operand_count, const char *label,
                     int source_line) {
    if (!ir)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "IR is NULL");

    ENSURE_CAP(ir->operations, ir->op_count, ir->op_capacity, sizeof(DslIROperation), -1);

    DslIROperation *op_entry = &ir->operations[ir->op_count];
    memset(op_entry, 0, sizeof(*op_entry));
    op_entry->op = op;
    op_entry->result_id = result_id;
    op_entry->source_line = source_line;
    op_entry->label = label;

    if (operand_count > 0 && operands) {
        op_entry->operands = lv_malloc(sizeof(int) * (size_t) operand_count);
        if (!op_entry->operands)
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "failed to allocate operands array");
        memcpy(op_entry->operands, operands, sizeof(int) * (size_t) operand_count);
        op_entry->operand_count = operand_count;
    } else {
        op_entry->operands = NULL;
        op_entry->operand_count = 0;
    }

    int idx = ir->op_count;
    ir->op_count++;
    if (result_id >= ir->next_id)
        ir->next_id = result_id + 1;
    return idx;
}

/**
 * @brief 递归编译 AST 节点为 IR 操作
 *
 * @param ir          IR 对象
 * @param node        AST 节点
 * @param result_id   该节点生成的结果 IR ID（-1 表示不产生结果）
 * @return 成功返回 true
 */
static bool compile_node(DslIR *ir, const DslAST *node, int *result_id) {
    if (!ir || !node)
        return false;

    int line = node->line;
    int rid = -1;

    switch (node->type) {
        /* ---- 几何原语声明 ---- */
        case DSL_AST_POINT_DECL: {
            rid = ir->next_id;
            ir_add_op(ir, IR_CREATE_POINT, rid, NULL, 0, node->name, line);
            if (node->name)
                ir_add_symbol(ir, node->name, rid);
            break;
        }

        case DSL_AST_LINE_DECL: {
            rid = ir->next_id;
            ir_add_op(ir, IR_CREATE_LINE, rid, NULL, 0, node->name, line);
            if (node->name)
                ir_add_symbol(ir, node->name, rid);
            break;
        }

        case DSL_AST_CIRCLE_DECL: {
            rid = ir->next_id;
            ir_add_op(ir, IR_CREATE_CIRCLE, rid, NULL, 0, node->name, line);
            if (node->name)
                ir_add_symbol(ir, node->name, rid);
            break;
        }

        case DSL_AST_SEGMENT_DECL: {
            rid = ir->next_id;
            ir_add_op(ir, IR_CREATE_SEGMENT, rid, NULL, 0, node->name, line);
            if (node->name)
                ir_add_symbol(ir, node->name, rid);
            break;
        }

        case DSL_AST_RAY_DECL: {
            rid = ir->next_id;
            ir_add_op(ir, IR_CREATE_RAY, rid, NULL, 0, node->name, line);
            if (node->name)
                ir_add_symbol(ir, node->name, rid);
            break;
        }

        case DSL_AST_POLYGON_DECL: {
            rid = ir->next_id;
            ir_add_op(ir, IR_CREATE_POLYGON, rid, NULL, 0, node->name, line);
            if (node->name)
                ir_add_symbol(ir, node->name, rid);
            break;
        }

        case DSL_AST_TRIANGLE_DECL: {
            rid = ir->next_id;
            ir_add_op(ir, IR_CREATE_TRIANGLE, rid, NULL, 0, node->name, line);
            if (node->name)
                ir_add_symbol(ir, node->name, rid);
            break;
        }

        /* ---- 构造操作 ---- */
        case DSL_AST_INTERSECT: {
            rid = ir->next_id;
            int ops[8];
            int oc = 0;
            for (int i = 0; i < node->child_count && oc < 8; i++) {
                int child_rid = -1;
                if (node->children[i]->type == DSL_AST_IDENT && node->children[i]->name)
                    child_rid = ir_find_symbol(ir, node->children[i]->name);
                if (child_rid >= 0)
                    ops[oc++] = child_rid;
            }
            ir_add_op(ir, IR_INTERSECT, rid, oc > 0 ? ops : NULL, oc, NULL, line);
            break;
        }

        case DSL_AST_PARALLEL: {
            rid = ir->next_id;
            int ops[8];
            int oc = 0;
            for (int i = 0; i < node->child_count && oc < 8; i++) {
                int child_rid = -1;
                if (node->children[i]->type == DSL_AST_IDENT && node->children[i]->name)
                    child_rid = ir_find_symbol(ir, node->children[i]->name);
                if (child_rid >= 0)
                    ops[oc++] = child_rid;
            }
            ir_add_op(ir, IR_PARALLEL_THROUGH, rid, oc > 0 ? ops : NULL, oc, NULL, line);
            break;
        }

        case DSL_AST_PERPENDICULAR: {
            rid = ir->next_id;
            int ops[8];
            int oc = 0;
            for (int i = 0; i < node->child_count && oc < 8; i++) {
                int child_rid = -1;
                if (node->children[i]->type == DSL_AST_IDENT && node->children[i]->name)
                    child_rid = ir_find_symbol(ir, node->children[i]->name);
                if (child_rid >= 0)
                    ops[oc++] = child_rid;
            }
            ir_add_op(ir, IR_PERPENDICULAR_THROUGH, rid, oc > 0 ? ops : NULL, oc, NULL, line);
            break;
        }

        case DSL_AST_MIDPOINT: {
            rid = ir->next_id;
            int ops[8];
            int oc = 0;
            for (int i = 0; i < node->child_count && oc < 8; i++) {
                int child_rid = -1;
                if (node->children[i]->type == DSL_AST_IDENT && node->children[i]->name)
                    child_rid = ir_find_symbol(ir, node->children[i]->name);
                if (child_rid >= 0)
                    ops[oc++] = child_rid;
            }
            ir_add_op(ir, IR_MIDPOINT_OF, rid, oc > 0 ? ops : NULL, oc, NULL, line);
            break;
        }

        case DSL_AST_CIRCUMCENTER: {
            rid = ir->next_id;
            int ops[8];
            int oc = 0;
            for (int i = 0; i < node->child_count && oc < 8; i++) {
                int child_rid = -1;
                if (node->children[i]->type == DSL_AST_IDENT && node->children[i]->name)
                    child_rid = ir_find_symbol(ir, node->children[i]->name);
                if (child_rid >= 0)
                    ops[oc++] = child_rid;
            }
            ir_add_op(ir, IR_CIRCUMCENTER_OF, rid, oc > 0 ? ops : NULL, oc, NULL, line);
            break;
        }

        case DSL_AST_ORTHOCENTER: {
            rid = ir->next_id;
            int ops[8];
            int oc = 0;
            for (int i = 0; i < node->child_count && oc < 8; i++) {
                int child_rid = -1;
                if (node->children[i]->type == DSL_AST_IDENT && node->children[i]->name)
                    child_rid = ir_find_symbol(ir, node->children[i]->name);
                if (child_rid >= 0)
                    ops[oc++] = child_rid;
            }
            ir_add_op(ir, IR_ORTHOCENTER_OF, rid, oc > 0 ? ops : NULL, oc, NULL, line);
            break;
        }

        case DSL_AST_CENTROID: {
            rid = ir->next_id;
            int ops[8];
            int oc = 0;
            for (int i = 0; i < node->child_count && oc < 8; i++) {
                int child_rid = -1;
                if (node->children[i]->type == DSL_AST_IDENT && node->children[i]->name)
                    child_rid = ir_find_symbol(ir, node->children[i]->name);
                if (child_rid >= 0)
                    ops[oc++] = child_rid;
            }
            ir_add_op(ir, IR_CENTROID_OF, rid, oc > 0 ? ops : NULL, oc, NULL, line);
            break;
        }

        case DSL_AST_INCENTER: {
            rid = ir->next_id;
            int ops[8];
            int oc = 0;
            for (int i = 0; i < node->child_count && oc < 8; i++) {
                int child_rid = -1;
                if (node->children[i]->type == DSL_AST_IDENT && node->children[i]->name)
                    child_rid = ir_find_symbol(ir, node->children[i]->name);
                if (child_rid >= 0)
                    ops[oc++] = child_rid;
            }
            ir_add_op(ir, IR_INCENTER_OF, rid, oc > 0 ? ops : NULL, oc, NULL, line);
            break;
        }

        case DSL_AST_BISECTOR: {
            rid = ir->next_id;
            int ops[8];
            int oc = 0;
            for (int i = 0; i < node->child_count && oc < 8; i++) {
                int child_rid = -1;
                if (node->children[i]->type == DSL_AST_IDENT && node->children[i]->name)
                    child_rid = ir_find_symbol(ir, node->children[i]->name);
                if (child_rid >= 0)
                    ops[oc++] = child_rid;
            }
            ir_add_op(ir, IR_BISECTOR_OF, rid, oc > 0 ? ops : NULL, oc, NULL, line);
            break;
        }

        /* ---- fix / free ---- */
        case DSL_AST_FIX_POINT: {
            rid = ir->next_id;
            /* 将坐标作为数值操作数 */
            int operand_ids[2] = {-1, -1};
            for (int i = 0; i < node->child_count && i < 2; i++) {
                if (node->children[i]->type == DSL_AST_NUMBER) {
                    /* 数值直接编码为操作数的 IR ID（后续 IR loader 解释） */
                    operand_ids[i] = (int) node->children[i]->num_value;
                }
            }
            ir_add_op(ir, IR_CREATE_POINT_FIXED, rid, operand_ids, 2, node->name, line);
            if (node->name)
                ir_add_symbol(ir, node->name, rid);
            break;
        }

        case DSL_AST_FREE_POINT: {
            rid = ir->next_id;
            ir_add_op(ir, IR_CREATE_POINT, rid, NULL, 0, node->name, line);
            if (node->name)
                ir_add_symbol(ir, node->name, rid);
            break;
        }

        /* ---- load / prove ---- */
        case DSL_AST_LOAD: {
            ir_add_op(ir, IR_LOAD_AXIOM, -1, NULL, 0, node->name, line);
            break;
        }

        case DSL_AST_PROVE: {
            ir_add_op(ir, IR_PROVE, -1, NULL, 0, node->name, line);
            break;
        }

        /* ---- constraint ---- */
        case DSL_AST_CONSTRAINT: {
            /* constraint { ... } 块内的子语句展开为约束操作 */
            rid = ir->next_id;
            int ops[8];
            int oc = 0;
            for (int i = 0; i < node->child_count; i++) {
                DslAST *child = node->children[i];
                if (!child)
                    continue;
                if (child->type == DSL_AST_BLOCK) {
                    for (int j = 0; j < child->child_count; j++) {
                        int op_rid = -1;
                        compile_node(ir, child->children[j], &op_rid);
                    }
                } else {
                    /* 标识符引用：在约束类型选择中使用 */
                    if (child->type == DSL_AST_IDENT && child->name && oc < 8) {
                        int sym_id = ir_find_symbol(ir, child->name);
                        if (sym_id >= 0)
                            ops[oc++] = sym_id;
                    }
                }
            }
            ir_add_op(ir, IR_ADD_CONSTRAINT, rid, oc > 0 ? ops : NULL, oc, NULL, line);
            break;
        }

        /* ---- block ---- */
        case DSL_AST_BLOCK: {
            for (int i = 0; i < node->child_count; i++) {
                int inner_result = -1;
                compile_node(ir, node->children[i], &inner_result);
            }
            break;
        }

        default:
            return false;
    }

    if (result_id)
        *result_id = rid;
    return true;
}

/**
 * @brief 将 DSL AST 编译为 IR（中间表示）
 *
 * 遍历 AST 子节点，为每个声明生成对应的 IR 操作。
 *
 * @param ast    DSL AST 根节点
 * @param config 编译配置（当前未使用）
 * @param out_ir 输出：IR 指针
 * @return 成功返回 true，失败返回 false
 */
bool dsl_compile(const DslAST *ast, const DslCompileConfig *config, DslIR **out_ir) {
    if (!ast || !out_ir)
        return false;

    DslIR *ir = lv_calloc(1, sizeof(DslIR));
    if (!ir)
        return false;

    int initial_cap = (ast->child_count > 0) ? (int) ((size_t) ast->child_count * 4) : 16;
    if (initial_cap < 16)
        initial_cap = 16;

    ir->op_capacity = initial_cap;
    ir->operations = lv_calloc((size_t) ir->op_capacity, sizeof(DslIROperation));
    if (!ir->operations) {
        lv_free(ir);
        return false;
    }

    ir->symbol_capacity = initial_cap;
    ir->symbols = lv_calloc((size_t) ir->symbol_capacity, sizeof(char *));
    if (!ir->symbols) {
        lv_free(ir->operations);
        lv_free(ir);
        return false;
    }
    ir->symbol_to_ir_id = lv_calloc((size_t) ir->symbol_capacity, sizeof(int));
    if (!ir->symbol_to_ir_id) {
        lv_free(ir->symbols);
        lv_free(ir->operations);
        lv_free(ir);
        return false;
    }

    ir->next_id = 0;

    /* 遍历 AST 子节点生成 IR 操作 */
    for (int i = 0; i < ast->child_count; i++) {
        int result_id = -1;
        if (!compile_node(ir, ast->children[i], &result_id)) {
            /* 继续编译其他节点 */
            continue;
        }
    }

    *out_ir = ir;
    (void) config;
    return true;
}
