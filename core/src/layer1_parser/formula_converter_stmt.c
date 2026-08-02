/**
 * @file formula_converter_stmt.c
 * @brief 公式转换器实现 —— 语句分派表与公式→图主入口
 *
 * @details 由 formula_converter.c 按功能边界拆分而来，
 *          属于公式 AST 与约束图双向转换的一部分。
 *
 * @author Lv-00 Project
 * @version 3.0.1
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
