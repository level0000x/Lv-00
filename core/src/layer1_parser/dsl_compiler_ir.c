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
#include "lv/lv_hashtable.h"
#include "lv/lv_lifecycle.h"

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

    /* 扩容符号表数组（统一走 lv_ENSURE_ARRAY_CAP） */
    lv_ENSURE_ARRAY_CAP(ir->symbols, ir->symbol_count, ir->symbol_capacity, -1);
    lv_ENSURE_ARRAY_CAP(ir->symbol_to_ir_id, ir->symbol_count, ir->symbol_capacity, -1);

    ir->symbols[ir->symbol_count] = lv_strdup(name);
    ir->symbol_to_ir_id[ir->symbol_count] = result_id;
    int idx = ir->symbol_count;
    ir->symbol_count++;
    if (ir->symbol_index)
        lv_hashtable_str_insert(ir->symbol_index, name, (void *) (intptr_t) (idx + 1));
    return idx;
}

/**
 * @brief 在 IR 符号表中查找名称，返回 IR 操作索引
 */
static int ir_find_symbol(const DslIR *ir, const char *name) {
    if (!ir || !name)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "IR or name is NULL");
    if (ir->symbol_index) {
        void *v = lv_hashtable_str_get(ir->symbol_index, name);
        if (v) {
            int idx = (int) (intptr_t) v - 1;
            if (idx >= 0 && idx < ir->symbol_count)
                return ir->symbol_to_ir_id[idx];
        }
    }
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

    /* 扩容 IR 操作数组（统一走 lv_ENSURE_ARRAY_CAP） */
    lv_ENSURE_ARRAY_CAP(ir->operations, ir->op_count, ir->op_capacity, -1);

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

/* ================================================================
 *  VTable-based dispatch for AST node compilation
 * ================================================================ */

/* Forward declaration needed by recursive handlers (constraint, block) */
static bool compile_node(DslIR *ir, const DslAST *node, int *result_id);

/** Function pointer type for compilation handlers */
typedef bool (*CompileHandler)(DslIR *ir, const DslAST *node, int *result_id);

/* ---- Simple declaration handlers ---- */

static bool compile_point_decl(DslIR *ir, const DslAST *node, int *result_id) {
    int rid = ir->next_id;
    ir_add_op(ir, IR_CREATE_POINT, rid, NULL, 0, node->name, node->line);
    if (node->name)
        ir_add_symbol(ir, node->name, rid);
    *result_id = rid;
    return true;
}

static bool compile_line_decl(DslIR *ir, const DslAST *node, int *result_id) {
    int rid = ir->next_id;
    ir_add_op(ir, IR_CREATE_LINE, rid, NULL, 0, node->name, node->line);
    if (node->name)
        ir_add_symbol(ir, node->name, rid);
    *result_id = rid;
    return true;
}

static bool compile_circle_decl(DslIR *ir, const DslAST *node, int *result_id) {
    int rid = ir->next_id;
    ir_add_op(ir, IR_CREATE_CIRCLE, rid, NULL, 0, node->name, node->line);
    if (node->name)
        ir_add_symbol(ir, node->name, rid);
    *result_id = rid;
    return true;
}

static bool compile_segment_decl(DslIR *ir, const DslAST *node, int *result_id) {
    int rid = ir->next_id;
    ir_add_op(ir, IR_CREATE_SEGMENT, rid, NULL, 0, node->name, node->line);
    if (node->name)
        ir_add_symbol(ir, node->name, rid);
    *result_id = rid;
    return true;
}

static bool compile_ray_decl(DslIR *ir, const DslAST *node, int *result_id) {
    int rid = ir->next_id;
    ir_add_op(ir, IR_CREATE_RAY, rid, NULL, 0, node->name, node->line);
    if (node->name)
        ir_add_symbol(ir, node->name, rid);
    *result_id = rid;
    return true;
}

static bool compile_polygon_decl(DslIR *ir, const DslAST *node, int *result_id) {
    int rid = ir->next_id;
    ir_add_op(ir, IR_CREATE_POLYGON, rid, NULL, 0, node->name, node->line);
    if (node->name)
        ir_add_symbol(ir, node->name, rid);
    *result_id = rid;
    return true;
}

static bool compile_triangle_decl(DslIR *ir, const DslAST *node, int *result_id) {
    int rid = ir->next_id;
    ir_add_op(ir, IR_CREATE_TRIANGLE, rid, NULL, 0, node->name, node->line);
    if (node->name)
        ir_add_symbol(ir, node->name, rid);
    *result_id = rid;
    return true;
}

/* ---- Construction operation handlers ---- */

static bool compile_intersect(DslIR *ir, const DslAST *node, int *result_id) {
    int rid = ir->next_id;
    int ops[8];
    int oc = 0;
    for (int i = 0; i < node->child_count && oc < 8; i++) {
        int child_rid = -1;
        if (node->children[i]->type == DSL_AST_IDENT && node->children[i]->name)
            child_rid = ir_find_symbol(ir, node->children[i]->name);
        if (child_rid >= 0)
            ops[oc++] = child_rid;
    }
    ir_add_op(ir, IR_INTERSECT, rid, oc > 0 ? ops : NULL, oc, NULL, node->line);
    *result_id = rid;
    return true;
}

static bool compile_parallel(DslIR *ir, const DslAST *node, int *result_id) {
    int rid = ir->next_id;
    int ops[8];
    int oc = 0;
    for (int i = 0; i < node->child_count && oc < 8; i++) {
        int child_rid = -1;
        if (node->children[i]->type == DSL_AST_IDENT && node->children[i]->name)
            child_rid = ir_find_symbol(ir, node->children[i]->name);
        if (child_rid >= 0)
            ops[oc++] = child_rid;
    }
    ir_add_op(ir, IR_PARALLEL_THROUGH, rid, oc > 0 ? ops : NULL, oc, NULL, node->line);
    *result_id = rid;
    return true;
}

static bool compile_perpendicular(DslIR *ir, const DslAST *node, int *result_id) {
    int rid = ir->next_id;
    int ops[8];
    int oc = 0;
    for (int i = 0; i < node->child_count && oc < 8; i++) {
        int child_rid = -1;
        if (node->children[i]->type == DSL_AST_IDENT && node->children[i]->name)
            child_rid = ir_find_symbol(ir, node->children[i]->name);
        if (child_rid >= 0)
            ops[oc++] = child_rid;
    }
    ir_add_op(ir, IR_PERPENDICULAR_THROUGH, rid, oc > 0 ? ops : NULL, oc, NULL, node->line);
    *result_id = rid;
    return true;
}

static bool compile_midpoint(DslIR *ir, const DslAST *node, int *result_id) {
    int rid = ir->next_id;
    int ops[8];
    int oc = 0;
    for (int i = 0; i < node->child_count && oc < 8; i++) {
        int child_rid = -1;
        if (node->children[i]->type == DSL_AST_IDENT && node->children[i]->name)
            child_rid = ir_find_symbol(ir, node->children[i]->name);
        if (child_rid >= 0)
            ops[oc++] = child_rid;
    }
    ir_add_op(ir, IR_MIDPOINT_OF, rid, oc > 0 ? ops : NULL, oc, NULL, node->line);
    *result_id = rid;
    return true;
}

static bool compile_circumcenter(DslIR *ir, const DslAST *node, int *result_id) {
    int rid = ir->next_id;
    int ops[8];
    int oc = 0;
    for (int i = 0; i < node->child_count && oc < 8; i++) {
        int child_rid = -1;
        if (node->children[i]->type == DSL_AST_IDENT && node->children[i]->name)
            child_rid = ir_find_symbol(ir, node->children[i]->name);
        if (child_rid >= 0)
            ops[oc++] = child_rid;
    }
    ir_add_op(ir, IR_CIRCUMCENTER_OF, rid, oc > 0 ? ops : NULL, oc, NULL, node->line);
    *result_id = rid;
    return true;
}

static bool compile_orthocenter(DslIR *ir, const DslAST *node, int *result_id) {
    int rid = ir->next_id;
    int ops[8];
    int oc = 0;
    for (int i = 0; i < node->child_count && oc < 8; i++) {
        int child_rid = -1;
        if (node->children[i]->type == DSL_AST_IDENT && node->children[i]->name)
            child_rid = ir_find_symbol(ir, node->children[i]->name);
        if (child_rid >= 0)
            ops[oc++] = child_rid;
    }
    ir_add_op(ir, IR_ORTHOCENTER_OF, rid, oc > 0 ? ops : NULL, oc, NULL, node->line);
    *result_id = rid;
    return true;
}

static bool compile_centroid(DslIR *ir, const DslAST *node, int *result_id) {
    int rid = ir->next_id;
    int ops[8];
    int oc = 0;
    for (int i = 0; i < node->child_count && oc < 8; i++) {
        int child_rid = -1;
        if (node->children[i]->type == DSL_AST_IDENT && node->children[i]->name)
            child_rid = ir_find_symbol(ir, node->children[i]->name);
        if (child_rid >= 0)
            ops[oc++] = child_rid;
    }
    ir_add_op(ir, IR_CENTROID_OF, rid, oc > 0 ? ops : NULL, oc, NULL, node->line);
    *result_id = rid;
    return true;
}

static bool compile_incenter(DslIR *ir, const DslAST *node, int *result_id) {
    int rid = ir->next_id;
    int ops[8];
    int oc = 0;
    for (int i = 0; i < node->child_count && oc < 8; i++) {
        int child_rid = -1;
        if (node->children[i]->type == DSL_AST_IDENT && node->children[i]->name)
            child_rid = ir_find_symbol(ir, node->children[i]->name);
        if (child_rid >= 0)
            ops[oc++] = child_rid;
    }
    ir_add_op(ir, IR_INCENTER_OF, rid, oc > 0 ? ops : NULL, oc, NULL, node->line);
    *result_id = rid;
    return true;
}

static bool compile_bisector(DslIR *ir, const DslAST *node, int *result_id) {
    int rid = ir->next_id;
    int ops[8];
    int oc = 0;
    for (int i = 0; i < node->child_count && oc < 8; i++) {
        int child_rid = -1;
        if (node->children[i]->type == DSL_AST_IDENT && node->children[i]->name)
            child_rid = ir_find_symbol(ir, node->children[i]->name);
        if (child_rid >= 0)
            ops[oc++] = child_rid;
    }
    ir_add_op(ir, IR_BISECTOR_OF, rid, oc > 0 ? ops : NULL, oc, NULL, node->line);
    *result_id = rid;
    return true;
}

/* ---- Special case handlers ---- */

static bool compile_fix_point(DslIR *ir, const DslAST *node, int *result_id) {
    int rid = ir->next_id;
    int operand_ids[2] = {-1, -1};
    for (int i = 0; i < node->child_count && i < 2; i++) {
        if (node->children[i]->type == DSL_AST_NUMBER) {
            operand_ids[i] = (int) node->children[i]->num_value;
        }
    }
    ir_add_op(ir, IR_CREATE_POINT_FIXED, rid, operand_ids, 2, node->name, node->line);
    if (node->name)
        ir_add_symbol(ir, node->name, rid);
    *result_id = rid;
    return true;
}

static bool compile_load(DslIR *ir, const DslAST *node, int *result_id) {
    ir_add_op(ir, IR_LOAD_AXIOM, -1, NULL, 0, node->name, node->line);
    *result_id = -1;
    return true;
}

static bool compile_prove(DslIR *ir, const DslAST *node, int *result_id) {
    ir_add_op(ir, IR_PROVE, -1, NULL, 0, node->name, node->line);
    *result_id = -1;
    return true;
}

static bool compile_constraint(DslIR *ir, const DslAST *node, int *result_id) {
    int rid = ir->next_id;
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
            if (child->type == DSL_AST_IDENT && child->name && oc < 8) {
                int sym_id = ir_find_symbol(ir, child->name);
                if (sym_id >= 0)
                    ops[oc++] = sym_id;
            }
        }
    }
    ir_add_op(ir, IR_ADD_CONSTRAINT, rid, oc > 0 ? ops : NULL, oc, NULL, node->line);
    *result_id = rid;
    return true;
}

static bool compile_block(DslIR *ir, const DslAST *node, int *result_id) {
    for (int i = 0; i < node->child_count; i++) {
        int inner_result = -1;
        compile_node(ir, node->children[i], &inner_result);
    }
    *result_id = -1;
    return true;
}

/* ---- VTable lookup table ---- */
static CompileHandler s_compile_handlers[] = {
    NULL,                   /* DSL_AST_PROGRAM (0) */
    compile_point_decl,     /* DSL_AST_POINT_DECL */
    compile_line_decl,      /* DSL_AST_LINE_DECL */
    compile_circle_decl,    /* DSL_AST_CIRCLE_DECL */
    compile_segment_decl,   /* DSL_AST_SEGMENT_DECL */
    compile_ray_decl,       /* DSL_AST_RAY_DECL */
    compile_polygon_decl,   /* DSL_AST_POLYGON_DECL */
    compile_triangle_decl,  /* DSL_AST_TRIANGLE_DECL */
    compile_intersect,      /* DSL_AST_INTERSECT */
    compile_parallel,       /* DSL_AST_PARALLEL */
    compile_perpendicular,  /* DSL_AST_PERPENDICULAR */
    compile_midpoint,       /* DSL_AST_MIDPOINT */
    compile_circumcenter,   /* DSL_AST_CIRCUMCENTER */
    compile_orthocenter,    /* DSL_AST_ORTHOCENTER */
    compile_centroid,       /* DSL_AST_CENTROID */
    compile_incenter,       /* DSL_AST_INCENTER */
    compile_bisector,       /* DSL_AST_BISECTOR */
    compile_constraint,     /* DSL_AST_CONSTRAINT */
    compile_prove,          /* DSL_AST_PROVE */
    compile_load,           /* DSL_AST_LOAD */
    compile_fix_point,      /* DSL_AST_FIX_POINT */
    compile_point_decl,     /* DSL_AST_FREE_POINT (same as POINT_DECL) */
    compile_block,          /* DSL_AST_BLOCK */
    NULL,                   /* DSL_AST_IDENT */
    NULL                    /* DSL_AST_NUMBER */
};

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

    if ((size_t)node->type < sizeof(s_compile_handlers) / sizeof(s_compile_handlers[0])) {
        CompileHandler handler = s_compile_handlers[node->type];
        if (handler)
            return handler(ir, node, result_id);
    }
    return false;
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

/* DslIR 部分构建守卫：任一成员分配失败时统一释放已分配成员与外壳，
 * 替代递增回滚样板 */
typedef struct {
    DslIR *ir;
} DslIrGuard;

static void dsl_ir_guard_cleanup(void *p) {
    DslIrGuard *g = (DslIrGuard *) p;
    if (g->ir) {
        if (g->ir->symbol_index)
            lv_hashtable_str_destroy(g->ir->symbol_index);
        lv_free((void **) &g->ir->symbol_to_ir_id);
        lv_free((void **) &g->ir->symbols);
        lv_free((void **) &g->ir->operations);
        lv_free((void **) &g->ir);
    }
}

bool dsl_compile(const DslAST *ast, const DslCompileConfig *config, DslIR **out_ir) {
    if (!ast || !out_ir)
        return false;

    DslIR *ir = lv_calloc(1, sizeof(DslIR));
    if (!ir)
        return false;

    /* 部分构建守卫：后续任一分配失败自动释放已分配成员；成功路径 guard.ir = NULL 解除 */
    DslIrGuard guard = {ir};
    lv_DEFER(dsl_ir_guard_cleanup, &guard);

    int initial_cap = (ast->child_count > 0) ? (int) ((size_t) ast->child_count * 4) : 16;
    if (initial_cap < 16)
        initial_cap = 16;

    ir->op_capacity = initial_cap;
    ir->operations = lv_calloc((size_t) ir->op_capacity, sizeof(DslIROperation));
    if (!ir->operations)
        lv_RETURN_ERROR_BOOL(lv_ERROR_OUT_OF_MEMORY, "dsl_compile: operations calloc failed");

    ir->symbol_capacity = initial_cap;
    ir->symbols = lv_calloc((size_t) ir->symbol_capacity, sizeof(char *));
    if (!ir->symbols)
        lv_RETURN_ERROR_BOOL(lv_ERROR_OUT_OF_MEMORY, "dsl_compile: symbols calloc failed");
    ir->symbol_to_ir_id = lv_calloc((size_t) ir->symbol_capacity, sizeof(int));
    if (!ir->symbol_to_ir_id)
        lv_RETURN_ERROR_BOOL(lv_ERROR_OUT_OF_MEMORY, "dsl_compile: symbol_to_ir_id calloc failed");

    /* 哈希索引为可选加速结构：分配失败时保持 NULL，后续访问由 NULL 保护回退线性扫描 */
    ir->symbol_index = lv_hashtable_str_create(ir->symbol_capacity);
    if (!ir->symbol_index)
        lv_set_error(lv_ERROR_OUT_OF_MEMORY, "dsl_compile: symbol_index hashtable create failed");
    ir->next_id = 0;

    /* 遍历 AST 子节点生成 IR 操作 */
    for (int i = 0; i < ast->child_count; i++) {
        int result_id = -1;
        if (!compile_node(ir, ast->children[i], &result_id)) {
            /* 继续编译其他节点 */
            continue;
        }
    }

    guard.ir = NULL; /* 守卫解除：结果移交调用方 */
    *out_ir = ir;
    (void) config;
    return true;
}
