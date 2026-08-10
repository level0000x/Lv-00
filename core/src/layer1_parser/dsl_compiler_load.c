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

#include "lv/lv_lifecycle.h"

#include <ctype.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/symbolic_coord.h"
#include "lv/lv_xmacro.h"
#include "lv/context.h"
#include "lv/axiom_pkg.h"
#include "lv/lv_hashtable.h"

#include "lv_internal.h"
#include "lv/lv_log.h"

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
 * @brief 将 IR 系统操作的结果文本记录到图的消息通道
 *
 * 约束图节点/约束没有通用的结果存储字段，这里复用 graph_set_error 的消息通道
 * （优先写入 graph->context->error_message，回退到图 error_buffer），供上层通过
 * graph_get_error() 观察系统操作（load/prove/check_sat/label）的结果。
 *
 * @param graph  目标约束图（可为 NULL，此时不记录）
 * @param kind   操作类别（如 "check_sat"）
 * @param detail 结果描述文本（可为 NULL）
 */
static void dsl_record_ir_result(ConstraintGraph *graph, const char *kind, const char *detail) {
    if (!graph || !kind)
        return;
    if (detail)
        graph_set_error(graph, "%s: %s", kind, detail);
    else
        graph_set_error(graph, "%s", kind);
}

/**
 * @brief 将公理包登记到 lvContext 的公理库（axiom_pkg_refs）
 *
 * 按 lvContext 的既有设计，axiom_pkg_refs 是"不拥有所有权"的引用数组，
 * 上下文销毁时仅释放数组本身。由 DSL loader 创建的公理包对象登记进公理库后，
 * 其生命周期与上下文共存（符合公理库语义：已加载公理在上下文存活期间有效）。
 *
 * @param ctx 编译上下文（可为 NULL）
 * @param pkg 公理包对象（调用方创建，登记成功后由公理库持有）
 * @return 成功返回 true
 */
static bool dsl_ctx_register_axiom_pkg(struct lvContext *ctx, AxiomPackage *pkg) {
    if (!ctx || !pkg)
        return false;
    /* 倍增扩容：委托 lv_ensure_capacity（初始 8，此后每次倍增；失败语义与原来一致：返回 false） */
    if (!lv_ensure_capacity((void **) &ctx->axiom_pkg_refs, ctx->axiom_pkg_ref_count, &ctx->axiom_pkg_ref_capacity,
                            sizeof(void *), 1))
        return false;
    ctx->axiom_pkg_refs[ctx->axiom_pkg_ref_count] = pkg;
    ctx->axiom_pkg_ref_count++;
    return true;
}

/* ================================================================
 *  IR 操作处理器 VTable
 * ================================================================ */

/**
 * @brief IR 操作处理器函数指针类型
 *
 * 每个 handler 处理一种或多种 IR 操作类型。
 *
 * @param graph         约束图指针
 * @param op            当前 IR 操作
 * @param id_map        结果 ID → 约束图节点 ID 映射表（可 realloc）
 * @param id_map_count  映射表有效条目数
 * @param id_map_cap    映射表容量
 * @return 成功返回 true
 */
typedef bool (*IROpHandler)(ConstraintGraph *graph, const DslIROperation *op,
                            int **id_map, int *id_map_count, int *id_map_cap);

/**
 * @brief 确保 id_map 容量足够，不足则扩容
 */
static bool ensure_id_map_cap(int **id_map, int *id_map_cap, int *id_map_count, int needed) {
    (void)id_map_count;
    while (needed >= *id_map_cap) {
        int old_cap = *id_map_cap;
        /* 失败时释放旧指针并置 NULL（与原始语义一致） */
        if (!lv_ensure_capacity((void **) id_map, old_cap,
                                id_map_cap, sizeof(int),
                                needed - old_cap)) {
            lv_free((void **) id_map);
            return false;
        }
        /* 初始化新区域为 -1 */
        for (int _i = old_cap; _i < *id_map_cap; _i++)
            (*id_map)[_i] = -1;
    }
    return true;
}

/* ---- 实体创建 handler ---- */

/** IR_CREATE_POINT: 创建自由点 */
static bool handle_create_point(ConstraintGraph *graph, const DslIROperation *op,
                                int **id_map, int *id_map_count, int *id_map_cap) {
    GeomNode *node = graph_add_node_with_id(graph, op->result_id, GEOM_POINT, NULL, 0);
    if (node && op->result_id >= 0) {
        if (!ensure_id_map_cap(id_map, id_map_cap, id_map_count, op->result_id + 1))
            return false;
        if (op->result_id >= *id_map_count)
            *id_map_count = op->result_id + 1;
        (*id_map)[op->result_id] = node->id;
    }
    return true;
}

/** IR_CREATE_POINT_FIXED: 创建固定坐标点 */
static bool handle_create_point_fixed(ConstraintGraph *graph, const DslIROperation *op,
                                      int **id_map, int *id_map_count, int *id_map_cap) {
    double x = 0.0, y = 0.0;
    resolve_fixed_coords(op, &x, &y);
    GeomNode *node = graph_add_node_with_id(graph, op->result_id, GEOM_POINT, NULL, 0);
    if (node && op->result_id >= 0) {
        if (!ensure_id_map_cap(id_map, id_map_cap, id_map_count, op->result_id + 1))
            return false;
        if (op->result_id >= *id_map_count)
            *id_map_count = op->result_id + 1;
        (*id_map)[op->result_id] = node->id;
    }
    return true;
}

/** IR_CREATE_LINE / IR_CREATE_SEGMENT: 创建线段 */
static bool handle_create_line_segment(ConstraintGraph *graph, const DslIROperation *op,
                                       int **id_map, int *id_map_count, int *id_map_cap) {
    GeomNode *node = graph_add_node_with_id(graph, op->result_id, GEOM_LINE_SEGMENT, NULL, 0);
    if (node && op->result_id >= 0) {
        if (!ensure_id_map_cap(id_map, id_map_cap, id_map_count, op->result_id + 1))
            return false;
        if (op->result_id >= *id_map_count)
            *id_map_count = op->result_id + 1;
        (*id_map)[op->result_id] = node->id;
    }
    return true;
}

/** IR_CREATE_CIRCLE: 创建圆 */
static bool handle_create_circle(ConstraintGraph *graph, const DslIROperation *op,
                                 int **id_map, int *id_map_count, int *id_map_cap) {
    GeomNode *node = graph_add_node_with_id(graph, op->result_id, GEOM_CIRCLE, NULL, 0);
    if (node && op->result_id >= 0) {
        if (!ensure_id_map_cap(id_map, id_map_cap, id_map_count, op->result_id + 1))
            return false;
        if (op->result_id >= *id_map_count)
            *id_map_count = op->result_id + 1;
        (*id_map)[op->result_id] = node->id;
        node->data.circle.center_node_id = -1;
        node->data.circle.radius_node_id = -1;
    }
    return true;
}

/** IR_CREATE_RAY: 创建射线 */
static bool handle_create_ray(ConstraintGraph *graph, const DslIROperation *op,
                              int **id_map, int *id_map_count, int *id_map_cap) {
    GeomNode *node = graph_add_node_with_id(graph, op->result_id, GEOM_LINE_SEGMENT, NULL, 0);
    if (node && op->result_id >= 0) {
        if (!ensure_id_map_cap(id_map, id_map_cap, id_map_count, op->result_id + 1))
            return false;
        if (op->result_id >= *id_map_count)
            *id_map_count = op->result_id + 1;
        (*id_map)[op->result_id] = node->id;
        for (int j = 0; j < op->operand_count; j++) {
            int pid = (op->operands[j] >= 0 && op->operands[j] < *id_map_count) ? (*id_map)[op->operands[j]] : -1;
            if (pid >= 0) {
                int parts[2] = {pid, node->id};
                graph_add_constraint_with_id(graph, -1, INCIDENCE, parts, 2);
            }
        }
    }
    return true;
}

/** IR_CREATE_POLYGON / IR_CREATE_TRIANGLE: 创建多边形/三角形区域 */
static bool handle_create_polygon_triangle(ConstraintGraph *graph, const DslIROperation *op,
                                           int **id_map, int *id_map_count, int *id_map_cap) {
    GeomNode *node = graph_add_node_with_id(graph, op->result_id, GEOM_REGION, NULL, 0);
    if (node && op->result_id >= 0) {
        if (!ensure_id_map_cap(id_map, id_map_cap, id_map_count, op->result_id + 1))
            return false;
        if (op->result_id >= *id_map_count)
            *id_map_count = op->result_id + 1;
        (*id_map)[op->result_id] = node->id;
    }
    return true;
}

/* ---- 构造操作 handler ---- */

/** IR_INTERSECT: 创建交点 */
static bool handle_intersect(ConstraintGraph *graph, const DslIROperation *op,
                             int **id_map, int *id_map_count, int *id_map_cap) {
    GeomNode *node = graph_add_node_with_id(graph, op->result_id, GEOM_POINT, NULL, 0);
    if (node && op->result_id >= 0) {
        if (!ensure_id_map_cap(id_map, id_map_cap, id_map_count, op->result_id + 1))
            return false;
        if (op->result_id >= *id_map_count)
            *id_map_count = op->result_id + 1;
        (*id_map)[op->result_id] = node->id;
        if (op->operand_count >= 2 && op->operands[0] >= 0 && op->operands[1] >= 0) {
            int p1_id = (op->operands[0] < *id_map_count) ? (*id_map)[op->operands[0]] : -1;
            int p2_id = (op->operands[1] < *id_map_count) ? (*id_map)[op->operands[1]] : -1;
            if (p1_id >= 0 && p2_id >= 0) {
                int parts[3] = {p1_id, p2_id, node->id};
                graph_add_constraint_with_id(graph, op->result_id, INTERSECTION, parts, 3);
            }
        }
    }
    return true;
}

/** IR_PARALLEL_THROUGH / IR_PERPENDICULAR_THROUGH: 平行/垂线约束 */
static bool handle_parallel_perpendicular_through(ConstraintGraph *graph, const DslIROperation *op,
                                                   int **id_map, int *id_map_count, int *id_map_cap) {
    (void)id_map;
    (void)id_map_count;
    (void)id_map_cap;
    if (op->operand_count >= 2 && op->operands[0] >= 0 && op->operands[1] >= 0) {
        int p1_id = (op->operands[0] < *id_map_count) ? (*id_map)[op->operands[0]] : -1;
        int p2_id = (op->operands[1] < *id_map_count) ? (*id_map)[op->operands[1]] : -1;
        if (p1_id >= 0 && p2_id >= 0) {
            int parts[2] = {p1_id, p2_id};
            graph_add_constraint_with_id(
                graph, op->result_id, (op->op == IR_PARALLEL_THROUGH) ? CONNECTION : INCIDENCE, parts, 2);
        }
    }
    return true;
}

/** IR_MIDPOINT_OF / IR_CIRCUMCENTER_OF / IR_ORTHOCENTER_OF / IR_CENTROID_OF / IR_INCENTER_OF / IR_BISECTOR_OF */
static bool handle_center_ops(ConstraintGraph *graph, const DslIROperation *op,
                              int **id_map, int *id_map_count, int *id_map_cap) {
    GeomNode *node = graph_add_node_with_id(graph, op->result_id, GEOM_POINT, NULL, 0);
    if (node && op->result_id >= 0) {
        if (!ensure_id_map_cap(id_map, id_map_cap, id_map_count, op->result_id + 1))
            return false;
        if (op->result_id >= *id_map_count)
            *id_map_count = op->result_id + 1;
        (*id_map)[op->result_id] = node->id;
        if (op->operand_count > 0) {
            for (int j = 0; j < op->operand_count; j++) {
                int pid = (op->operands[j] >= 0 && op->operands[j] < *id_map_count) ? (*id_map)[op->operands[j]] : -1;
                if (pid >= 0) {
                    int parts[2] = {pid, node->id};
                    graph_add_constraint_with_id(graph, -1, INCIDENCE, parts, 2);
                }
            }
        }
    }
    return true;
}

/* ---- 约束操作 handler ---- */

/** IR_ADD_CONSTRAINT / IR_CONSTRAIN_EQUAL / IR_CONSTRAIN_PARALLEL / IR_CONSTRAIN_PERPENDICULAR / IR_CONSTRAIN_COLLINEAR / IR_CONSTRAIN_CONCYCLIC */
static bool handle_constraint_ops(ConstraintGraph *graph, const DslIROperation *op,
                                  int **id_map, int *id_map_count, int *id_map_cap) {
    (void)id_map_cap;
    /* IR 约束操作 → 约束类型 映射表（designated initializer；
     * 未列出的操作（如 IR_CONSTRAIN_EQUAL/IR_ADD_CONSTRAINT）与越界值回退 INCIDENCE，
     * 对应原 switch 的 default 分支） */
    static const ConstraintType kConstraintOpMap[] = {
        [IR_CONSTRAIN_PARALLEL]      = CONNECTION,
        [IR_CONSTRAIN_PERPENDICULAR] = INCIDENCE,
        [IR_CONSTRAIN_COLLINEAR]     = BETWEENNESS,
        [IR_CONSTRAIN_CONCYCLIC]     = CONTAINMENT,
    };
    ConstraintType ctype = INCIDENCE; /* 默认：INCIDENCE（原 default 分支） */
    if ((unsigned)op->op < (unsigned)lv_ARRAY_SIZE(kConstraintOpMap))
        ctype = kConstraintOpMap[op->op];
    int parts[8];
    int pc = 0;
    for (int j = 0; j < op->operand_count && pc < 8; j++) {
        int pid = (op->operands[j] >= 0 && op->operands[j] < *id_map_count) ? (*id_map)[op->operands[j]] : -1;
        if (pid >= 0)
            parts[pc++] = pid;
    }
    if (pc > 0) {
        graph_add_constraint_with_id(graph, op->result_id, ctype, parts, pc);
    }
    return true;
}

/** IR_REMOVE_CONSTRAINT: 移除约束 */
static bool handle_remove_constraint(ConstraintGraph *graph, const DslIROperation *op,
                                     int **id_map, int *id_map_count, int *id_map_cap) {
    (void)id_map;
    (void)id_map_count;
    (void)id_map_cap;
    for (int j = 0; j < op->operand_count; j++) {
        int cid = op->operands[j];
        int rc = graph_deactivate_constraint(graph, cid);
        if (rc != lv_OK) {
            char detail[128];
            snprintf(detail, sizeof(detail), "约束 #%d 移除失败（错误码 %d）", cid, rc);
            dsl_record_ir_result(graph, "remove_constraint", detail);
        }
    }
    graph_sync_nodes(graph);
    return true;
}

/* ---- 系统操作 handler ---- */

/** IR_LOAD_AXIOM: 加载公理包 */
static bool handle_load_axiom(ConstraintGraph *graph, const DslIROperation *op,
                              int **id_map, int *id_map_count, int *id_map_cap) {
    (void)id_map;
    (void)id_map_count;
    (void)id_map_cap;
    const char *pkg_name = op->label ? op->label : "unnamed";
    AxiomPackage *pkg = lv_axiom_package_create(pkg_name, "0.0.0");
    if (!pkg) {
        dsl_record_ir_result(graph, "load axiom", "公理包对象创建失败");
        return true;
    }
    if (graph->context) {
        if (!dsl_ctx_register_axiom_pkg(graph->context, pkg)) {
            axiom_package_destroy(pkg);
            dsl_record_ir_result(graph, "load axiom", "公理库登记失败（内存不足）");
        }
    } else {
        axiom_package_destroy(pkg);
        dsl_record_ir_result(graph, "load axiom", pkg_name);
    }
    return true;
}

/** IR_PROVE: 登记待证明目标 */
static bool handle_prove(ConstraintGraph *graph, const DslIROperation *op,
                         int **id_map, int *id_map_count, int *id_map_cap) {
    (void)id_map;
    (void)id_map_count;
    (void)id_map_cap;
    const char *goal = op->label ? op->label : "(unnamed)";
    dsl_record_ir_result(graph, "prove", goal);
    lvConstraintCompatibilityResult comp = {0};
    if (graph_check_compatibility(graph, &comp)) {
        if (graph->context)
            graph->context->last_status = (int) comp.status;
        if (comp.status == lv_CONSTRAINT_STATUS_INCONSISTENT)
            dsl_record_ir_result(graph, "prove 预检",
                                 comp.diagnostic ? comp.diagnostic : "约束矛盾，目标不可证明");
    }
    return true;
}

/** IR_CHECK_SAT: 可满足性检查 */
static bool handle_check_sat(ConstraintGraph *graph, const DslIROperation *op,
                             int **id_map, int *id_map_count, int *id_map_cap) {
    (void)id_map;
    (void)id_map_count;
    (void)id_map_cap;
    lvConstraintCompatibilityResult comp = {0};
    if (!graph_check_compatibility(graph, &comp)) {
        dsl_record_ir_result(graph, "check_sat", "检查失败：输入无效");
        return true;
    }
    if (graph->context)
        graph->context->last_status = (int) comp.status;
    const char *verdict = (comp.status == lv_CONSTRAINT_STATUS_INCONSISTENT) ? "unsat" : "sat";
    char detail[256];
    snprintf(detail, sizeof(detail), "%s（status=%d，%s；冲突约束=%d，冗余=%d，自由度=%d）",
             verdict, (int) comp.status, comp.diagnostic ? comp.diagnostic : "无诊断",
             comp.conflicting_constraint_id, comp.redundant_constraint_count, comp.free_degree_count);
    dsl_record_ir_result(graph, "check_sat", detail);
    return true;
}

/** IR_LABEL: 标签操作 */
static bool handle_label(ConstraintGraph *graph, const DslIROperation *op,
                         int **id_map, int *id_map_count, int *id_map_cap) {
    (void)id_map_cap;
    const char *text = op->label ? op->label : "(unnamed)";
    if (op->operand_count > 0 && op->operands[0] >= 0 && op->operands[0] < *id_map_count) {
        int gid = (*id_map)[op->operands[0]];
        GeomNode *node = (gid >= 0) ? graph_get_node(graph, gid) : NULL;
        if (node && !node->numeric_assumption_declaration) {
            char *s = lv_strdup(text);
            if (s)
                node->numeric_assumption_declaration = s;
        } else {
            char detail[192];
            snprintf(detail, sizeof(detail), "实体#%d label=\"%s\"", gid, text);
            dsl_record_ir_result(graph, "label", detail);
        }
    }
    return true;
}

/** IR_NOOP: 空操作 */
static bool handle_noop(ConstraintGraph *graph, const DslIROperation *op,
                        int **id_map, int *id_map_count, int *id_map_cap) {
    (void)graph;
    (void)op;
    (void)id_map;
    (void)id_map_count;
    (void)id_map_cap;
    return true;
}

/* 确保 id_map 有足够的容量（原 ENSURE_ID_MAP 宏函数化） */
static bool ensure_id_map(int **map, int *cap, int needed) {
    while (needed >= *cap) {
        int old_cap = *cap;
        /* 失败时释放旧指针并置 NULL（与原始语义一致） */
        if (!lv_ensure_capacity((void **) map, old_cap,
                                cap, sizeof(int),
                                needed - old_cap)) {
            lv_free((void **) map);
            return false;
        }
        /* 初始化新区域为 -1 */
        for (int _i = old_cap; _i < *cap; _i++)
            (*map)[_i] = -1;
    }
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

    /* 初始化 id_map */
    if (ir->next_id > 0) {
        if (!ensure_id_map(&id_map, &id_map_cap, ir->next_id + 1))
            return false;
        id_map_count = ir->next_id + 1;
        for (int i = 0; i < id_map_count; i++)
            id_map[i] = -1;
    }

    /* IR 操作处理器 VTable（按枚举值索引，共 30 项：0..29） */
    static const IROpHandler kIROpHandlers[] = {
        handle_create_point,                    /* IR_CREATE_POINT (0) */
        handle_create_point_fixed,              /* IR_CREATE_POINT_FIXED (1) */
        handle_create_line_segment,             /* IR_CREATE_LINE (2) */
        handle_create_circle,                   /* IR_CREATE_CIRCLE (3) */
        handle_create_line_segment,             /* IR_CREATE_SEGMENT (4) */
        handle_create_ray,                      /* IR_CREATE_RAY (5) */
        handle_create_polygon_triangle,         /* IR_CREATE_POLYGON (6) */
        handle_create_polygon_triangle,         /* IR_CREATE_TRIANGLE (7) */
        handle_intersect,                       /* IR_INTERSECT (8) */
        handle_parallel_perpendicular_through,  /* IR_PARALLEL_THROUGH (9) */
        handle_parallel_perpendicular_through,  /* IR_PERPENDICULAR_THROUGH (10) */
        handle_center_ops,                      /* IR_MIDPOINT_OF (11) */
        handle_center_ops,                      /* IR_CIRCUMCENTER_OF (12) */
        handle_center_ops,                      /* IR_ORTHOCENTER_OF (13) */
        handle_center_ops,                      /* IR_CENTROID_OF (14) */
        handle_center_ops,                      /* IR_INCENTER_OF (15) */
        handle_center_ops,                      /* IR_BISECTOR_OF (16) */
        NULL,                                   /* IR_ANGLE_BISECTOR (17) -> no-op */
        handle_constraint_ops,                  /* IR_ADD_CONSTRAINT (18) */
        handle_remove_constraint,               /* IR_REMOVE_CONSTRAINT (19) */
        handle_constraint_ops,                  /* IR_CONSTRAIN_EQUAL (20) */
        handle_constraint_ops,                  /* IR_CONSTRAIN_PARALLEL (21) */
        handle_constraint_ops,                  /* IR_CONSTRAIN_PERPENDICULAR (22) */
        handle_constraint_ops,                  /* IR_CONSTRAIN_COLLINEAR (23) */
        handle_constraint_ops,                  /* IR_CONSTRAIN_CONCYCLIC (24) */
        handle_load_axiom,                      /* IR_LOAD_AXIOM (25) */
        handle_prove,                           /* IR_PROVE (26) */
        handle_check_sat,                       /* IR_CHECK_SAT (27) */
        handle_label,                           /* IR_LABEL (28) */
        handle_noop,                            /* IR_NOOP (29) */
    };

    /* 遍历 IR 操作 */
    for (int i = 0; i < ir->op_count; i++) {
        const DslIROperation *op = &ir->operations[i];

        /* VTable 调度 */
        if (op->op >= 0 && op->op < (int)(sizeof(kIROpHandlers)/sizeof(kIROpHandlers[0])) && kIROpHandlers[op->op]) {
            if (!kIROpHandlers[op->op](graph, op, &id_map, &id_map_count, &id_map_cap))
                return false;
        }
        /* NULL handler 或越界值 = no-op（对应原始 default: break） */
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
/* ── dsl_ast_destroy / dsl_ir_destroy 子资源销毁适配 ── */

/* AST 子节点元素销毁：递归委托 dsl_ast_destroy */
static void destroy_dsl_ast_child_elem(void *elem) {
    dsl_ast_destroy((DslAST *) elem);
}

/* operations 为 DslIROperation 值数组（非指针数组），不适用
 * lv_FIELD_ARRAY 的指针数组语义：逐元素释放 operands 后释放数组本身 */
static void destroy_dsl_ir_operations(void *obj, void *field_ptr) {
    (void) field_ptr;
    DslIR *ir = (DslIR *) obj;
    for (int i = 0; i < ir->op_count; i++)
        lv_free((void **) &ir->operations[i].operands);
    lv_free((void **) &ir->operations);
}

/* 符号表元素销毁：释放单个符号字符串 */
static void destroy_dsl_ir_symbol_elem(void *elem) {
    char *sym = (char *) elem;
    if (sym)
        lv_free((void **) &sym);
}

/* dsl_ast_destroy 字段描述表：children 逐元素递归销毁后释放数组，
 * name 纯指针释放 */
static const lvFieldDesc s_dsl_ast_destroy_fields[] = {
    lv_FIELD_ARRAY(DslAST, children, child_count, destroy_dsl_ast_child_elem),
    lv_FIELD_PLAIN(DslAST, name),
};

void dsl_ast_destroy(DslAST *ast) {
    if (!ast)
        return;
    lv_obj_destroy_fields(ast, s_dsl_ast_destroy_fields,
                          sizeof(s_dsl_ast_destroy_fields) / sizeof(s_dsl_ast_destroy_fields[0]));
    lv_free((void **) &ast);
}

/**
 * @brief 销毁 IR 数据
 *
 * 释放所有 IR 操作的操作数数组、operations 数组、符号表和 IR 结构体本身。
 *
 * @param ir 要销毁的 IR 指针（允许为 NULL）
 */
/* dsl_ir_destroy 字段描述表：operations 逐元素释放操作数数组后释放数组，
 * symbols 逐元素释放字符串后释放数组，symbol_to_ir_id 纯指针释放 */
static const lvFieldDesc s_dsl_ir_destroy_fields[] = {
    lv_FIELD_CUSTOM(DslIR, operations, destroy_dsl_ir_operations),
    lv_FIELD_ARRAY(DslIR, symbols, symbol_count, destroy_dsl_ir_symbol_elem),
    lv_FIELD_PLAIN(DslIR, symbol_to_ir_id),
};

void dsl_ir_destroy(DslIR *ir) {
    if (!ir)
        return;
    lv_hashtable_str_destroy(ir->symbol_index);
    lv_obj_destroy_fields(ir, s_dsl_ir_destroy_fields,
                          sizeof(s_dsl_ir_destroy_fields) / sizeof(s_dsl_ir_destroy_fields[0]));
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

/* ── 字符串↔枚举 X-macro 列表 ── */

#define DSL_IR_OP_X(x) \
    x(IR_CREATE_POINT, "CREATE_POINT") \
    x(IR_CREATE_POINT_FIXED, "CREATE_POINT_FIXED") \
    x(IR_CREATE_LINE, "CREATE_LINE") \
    x(IR_CREATE_CIRCLE, "CREATE_CIRCLE") \
    x(IR_CREATE_SEGMENT, "CREATE_SEGMENT") \
    x(IR_CREATE_RAY, "CREATE_RAY") \
    x(IR_CREATE_POLYGON, "CREATE_POLYGON") \
    x(IR_CREATE_TRIANGLE, "CREATE_TRIANGLE") \
    x(IR_INTERSECT, "INTERSECT") \
    x(IR_PARALLEL_THROUGH, "PARALLEL_THROUGH") \
    x(IR_PERPENDICULAR_THROUGH, "PERPENDICULAR_THROUGH") \
    x(IR_MIDPOINT_OF, "MIDPOINT_OF") \
    x(IR_CIRCUMCENTER_OF, "CIRCUMCENTER_OF") \
    x(IR_ORTHOCENTER_OF, "ORTHOCENTER_OF") \
    x(IR_CENTROID_OF, "CENTROID_OF") \
    x(IR_INCENTER_OF, "INCENTER_OF") \
    x(IR_BISECTOR_OF, "BISECTOR_OF") \
    x(IR_ANGLE_BISECTOR, "ANGLE_BISECTOR") \
    x(IR_ADD_CONSTRAINT, "ADD_CONSTRAINT") \
    x(IR_REMOVE_CONSTRAINT, "REMOVE_CONSTRAINT") \
    x(IR_CONSTRAIN_EQUAL, "CONSTRAIN_EQUAL") \
    x(IR_CONSTRAIN_PARALLEL, "CONSTRAIN_PARALLEL") \
    x(IR_CONSTRAIN_PERPENDICULAR, "CONSTRAIN_PERPENDICULAR") \
    x(IR_CONSTRAIN_COLLINEAR, "CONSTRAIN_COLLINEAR") \
    x(IR_CONSTRAIN_CONCYCLIC, "CONSTRAIN_CONCYCLIC") \
    x(IR_LOAD_AXIOM, "LOAD_AXIOM") \
    x(IR_PROVE, "PROVE") \
    x(IR_CHECK_SAT, "CHECK_SAT") \
    x(IR_LABEL, "LABEL") \
    x(IR_NOOP, "NOOP")

#define DSL_AST_TYPE_X(x) \
    x(DSL_AST_PROGRAM, "PROGRAM") \
    x(DSL_AST_POINT_DECL, "POINT_DECL") \
    x(DSL_AST_LINE_DECL, "LINE_DECL") \
    x(DSL_AST_CIRCLE_DECL, "CIRCLE_DECL") \
    x(DSL_AST_SEGMENT_DECL, "SEGMENT_DECL") \
    x(DSL_AST_RAY_DECL, "RAY_DECL") \
    x(DSL_AST_POLYGON_DECL, "POLYGON_DECL") \
    x(DSL_AST_TRIANGLE_DECL, "TRIANGLE_DECL") \
    x(DSL_AST_INTERSECT, "INTERSECT") \
    x(DSL_AST_PARALLEL, "PARALLEL") \
    x(DSL_AST_PERPENDICULAR, "PERPENDICULAR") \
    x(DSL_AST_MIDPOINT, "MIDPOINT") \
    x(DSL_AST_CIRCUMCENTER, "CIRCUMCENTER") \
    x(DSL_AST_ORTHOCENTER, "ORTHOCENTER") \
    x(DSL_AST_CENTROID, "CENTROID") \
    x(DSL_AST_INCENTER, "INCENTER") \
    x(DSL_AST_BISECTOR, "BISECTOR") \
    x(DSL_AST_CONSTRAINT, "CONSTRAINT") \
    x(DSL_AST_PROVE, "PROVE") \
    x(DSL_AST_LOAD, "LOAD") \
    x(DSL_AST_FIX_POINT, "FIX_POINT") \
    x(DSL_AST_FREE_POINT, "FREE_POINT") \
    x(DSL_AST_BLOCK, "BLOCK") \
    x(DSL_AST_IDENT, "IDENT") \
    x(DSL_AST_NUMBER, "NUMBER")

/** @brief IR 操作码名称表（按枚举值升序） */
static const lvStrToEnumEntry s_ir_op_names[] = {
    lv_XMACRO_TO_ENUM_TABLE(DSL_IR_OP_X)
};

/** @brief DSL AST 节点类型名称表（按枚举值升序） */
static const lvStrToEnumEntry s_ast_type_names[] = {
    lv_XMACRO_TO_ENUM_TABLE(DSL_AST_TYPE_X)
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
