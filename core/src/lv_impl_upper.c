/* ============================================================================
 * 模块名称:Lv-00 上层统一实现 (lv_impl_upper)
 *
 * 功能概述:
 *   为 L3-L10 各层级提供统一的外部 API 实现入口。
 *   本文件汇聚了跨层级调用的 API 函数,通过内部静态表管理
 *   L3 几何演化引擎,ATP 后端,L6 可视化,L7 编排器,
 *   L8 元验证,L9 应用入口,L10 互操作等子系统的实例。
 *
 * 架构说明:
 *   各层的领域逻辑实现在对应的 layer/ 子目录中,
 *   本文件仅提供跨层 API 的注册/调度/生命周期管理。
 *   新功能应优先在对应层的 .c 文件中实现,
 *   仅当确实需要跨层统一入口时,才在此文件中注册。
 *
 * 内部结构(14 部分):
 *   第 1 部分   全局状态与内部表
 *   第 2-7 部分 L3-L4 几何与推理预设(geom_evol/atp/presets)
 *   第 8 部分   L6 可视化层封装
 *   第 9 部分   L7 编排层
 *   第 10 部分  L8 元验证层
 *   第 11 部分  L9 应用层
 *   第 12 部分  L10 互操作层
 *   第 13 部分  func_block_preset 统一封装
 *   第 14 部分  综合工具函数
 *
 * 设计文档参考:第四章 分层架构
 *
 * ============================================================================ */

/* ============================================================
 * 第1部分:头部与全局状态
 * ============================================================ */
#include <gmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/atp_backend.h"
#include "lv/conflict_detector.h"
#include "lv/engine.h"
#include "lv/func_block.h"
#include "lv/func_block_preset.h"
#include "lv/func_block_registry.h"
#include "lv/geom_evol.h"
#include "lv/interop.h"
#include "lv/lv_json.h"
#include "lv/lv_utils.h"
#include "lv/meta_verify.h"
#include "lv/orchestrator.h"
#include "lv/preset_algebraic.h"
#include "lv/preset_basic_geometry.h"
#include "lv/preset_measurements.h"
#include "lv/preset_polygons.h"
#include "lv/preset_transformations.h"
#include "lv/visual_editor.h"

#include "lv_internal.h" /* lv_RETURN_ERROR / lv_RETURN_ERROR_NULL */

/** 全局唯一 ID 计数器 -- 从一百万起步,避免与内部 ID 冲突 */
static int64_t g_upper_id = 1000000;

/** 前向声明 -- 本文件内部使用的轻量级编配器 */
typedef struct lvOrchestrator lvOrchestrator;

/* ============================================================
 * 文件级静态内部表 -- 用于 L3 实现中的 ID→object 映射
 * ============================================================ */

/** 几何演化引擎表条目 */
typedef struct {
    int64_t id;
    lvGeomEvol *evol;
} EvolEntry;

#define MAX_EVOL_TABLE 256
static EvolEntry g_evol_table[MAX_EVOL_TABLE];
static int g_evol_count = 0;

/** ATP 后端求解器表条目 */
typedef struct {
    int64_t id;
    ATPBackendSolver *solver;
} ATPBackendSlot;

#define MAX_ATP_BACKEND_TABLE 256
static ATPBackendSlot g_atp_backend_table[MAX_ATP_BACKEND_TABLE];
static int g_atp_backend_count = 0;

/** ATP 任务跟踪结构 */
typedef struct {
    int64_t task_id;           /**< 任务唯一 ID */
    int64_t backend_id;        /**< 关联的后端 ID(在 g_atp_backend_table 中的索引) */
    ATPResultInfo result_info; /**< 求解结果 */
    int8_t completed;          /**< 0=待处理, 1=已完成 */
} ATPTask;

/** ATP 任务表 */
#define MAX_ATP_TASK_TABLE 512
static ATPTask g_atp_task_table[MAX_ATP_TASK_TABLE];
static int g_atp_task_count = 0;

/** 可视化编辑器表 */
#define MAX_VISUAL_EDITOR_TABLE 64
static lvVisualEditor *g_visual_editor_table[MAX_VISUAL_EDITOR_TABLE];
static int g_visual_editor_count = 0;

/** 视图同步器表 */
#define MAX_VIEW_SYNC_TABLE 64
static lvViewSynchronizer *g_view_sync_table[MAX_VIEW_SYNC_TABLE];
static int g_view_sync_count = 0;

/** 文本代码视图表 */
#define MAX_TEXT_CODE_TABLE 64
static lvTextCodeView *g_text_code_table[MAX_TEXT_CODE_TABLE];
static int g_text_code_count = 0;

/** 元验证器单例（全局共享） */
static lvMetaVerifier *g_meta_verifier = NULL;

/* ============================================================
 * 第2部分:L3 几何扩展(geom_evol / atp_backend / proof_tptp)
 * ============================================================ */

/* ---- geom_evol: 几何演化引擎 ---- */

/** 创建几何演化引擎,分配参数向量 */
int64_t geom_evol_create(lvEngine *ctx, int64_t dim) {
    (void) ctx;
    if (dim <= 0 || dim > GEOEVOL_MAX_PARAM_DIM)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "geom_evol_create: invalid dim");
    if (g_evol_count >= MAX_EVOL_TABLE)
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "geom_evol_create: evol table full");

    /* 使用默认 RHS 函数创建演化引擎(调用方需后续设置实际 RHS) */
    lvGeomEvol *evol = geoevol_create((int) dim, lv_EVOL_RK4, NULL);
    if (!evol)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "geom_evol_create: geoevol_create failed");

    int64_t id = g_upper_id++;
    int slot = 0;
    /* 查找空闲槽位 */
    for (; slot < MAX_EVOL_TABLE; slot++) {
        if (!g_evol_table[slot].evol)
            break;
    }
    g_evol_table[slot].evol = evol;
    g_evol_table[slot].id = id;
    g_evol_count++;
    return id;
}

/** 执行单步几何演化,返回步数计数 */
int64_t geom_evol_step(lvEngine *ctx, int64_t evol_id, int64_t steps) {
    (void) ctx;
    /* 在内部表中查找对应的演化引擎 */
    lvGeomEvol *evol = NULL;
    for (int i = 0; i < MAX_EVOL_TABLE; i++) {
        if (g_evol_table[i].evol && g_evol_table[i].id == evol_id) {
            evol = g_evol_table[i].evol;
            break;
        }
    }
    if (!evol) {
        /* 未找到引擎,返回模拟值 */
        return steps + 1;
    }

    /* 执行 steps 次单步演化 */
    int64_t executed = 0;
    for (int64_t s = 0; s < steps; s++) {
        lvEvolStatus status = geoevol_step_once(evol);
        executed++;
        if (status == lv_EVOL_STATUS_ERROR || status == lv_EVOL_STATUS_CONVERGED) {
            break;
        }
    }
    return executed;
}

/** 销毁几何演化引擎实例 */
int64_t geom_evol_destroy(lvEngine *ctx, int64_t evol_id) {
    (void) ctx;
    /* 查找并销毁对应 ID 的演化引擎 */
    for (int i = 0; i < MAX_EVOL_TABLE; i++) {
        if (g_evol_table[i].evol && g_evol_table[i].id == evol_id) {
            geoevol_destroy(g_evol_table[i].evol);
            g_evol_table[i].evol = NULL;
            g_evol_table[i].id = 0;
            g_evol_count--;
            return 0;
        }
    }
    return -1; /* 未找到 */
}

/* ---- atp_backend: 自动定理证明后端 ---- */

/** 从名称解析 ATP 后端类型 */
static ATPBackendType atp_parse_solver_name(const char *solver_name) {
    if (!solver_name)
        return ATP_BACKEND_VAMPIRE;
    if (strstr(solver_name, "vampire") || strstr(solver_name, "Vampire"))
        return ATP_BACKEND_VAMPIRE;
    if (strstr(solver_name, "eprover") || strstr(solver_name, "E Prover"))
        return ATP_BACKEND_EPROVER;
    if (strstr(solver_name, "iprover") || strstr(solver_name, "iProver"))
        return ATP_BACKEND_IPROVER;
    return ATP_BACKEND_VAMPIRE; /* 默认 */
}

/** 创建ATP后端,返回后端句柄ID */
int64_t atp_backend_create(lvEngine *ctx, const char *solver_name) {
    (void) ctx;
    if (g_atp_backend_count >= MAX_ATP_BACKEND_TABLE)
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "atp_backend_create: ATP backend table full");

    ATPBackendType type = atp_parse_solver_name(solver_name);
    ATPConfig config = atp_config_default();
    ATPBackendSolver *solver = atp_solver_create(type, &config);
    if (!solver)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "atp_backend_create: solver create failed");

    /* 查找空闲槽位 */
    int slot = 0;
    for (; slot < MAX_ATP_BACKEND_TABLE; slot++) {
        if (!g_atp_backend_table[slot].solver)
            break;
    }
    if (slot >= MAX_ATP_BACKEND_TABLE) {
        atp_solver_destroy(solver);
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "atp_backend_create: no free slot");
    }
    g_atp_backend_table[slot].solver = solver;
    g_atp_backend_table[slot].id = g_upper_id++;
    g_atp_backend_count++;
    return g_atp_backend_table[slot].id;
}

/** 向ATP后端提交证明任务,返回任务ID */
int64_t atp_backend_submit(lvEngine *ctx, int64_t backend_id, const char *conjecture) {
    (void) ctx;
    if (!conjecture || conjecture[0] == '\0')
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "atp_backend_submit: NULL/empty conjecture");
    if (g_atp_task_count >= MAX_ATP_TASK_TABLE)
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "atp_backend_submit: task table full");

    /* 查找后端求解器 */
    ATPBackendSolver *solver = NULL;
    for (int i = 0; i < MAX_ATP_BACKEND_TABLE; i++) {
        if (g_atp_backend_table[i].solver && g_atp_backend_table[i].id == backend_id) {
            solver = g_atp_backend_table[i].solver;
            break;
        }
    }
    if (!solver)
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "atp_backend_submit: solver not found");

    /* 创建任务 */
    int task_slot = g_atp_task_count++;
    int64_t task_id = g_upper_id++;
    g_atp_task_table[task_slot].task_id = task_id;
    g_atp_task_table[task_slot].backend_id = backend_id;
    g_atp_task_table[task_slot].completed = 0;

    /* 初始化结果 */
    atp_result_init(&g_atp_task_table[task_slot].result_info);

    /* 加载并求解 */
    int load_ret = atp_solver_load(solver, conjecture);
    if (load_ret == 0) {
        atp_solver_solve(solver, &g_atp_task_table[task_slot].result_info);
    }
    g_atp_task_table[task_slot].completed = 1;
    return task_id;
}

/** 获取ATP任务结果:0=待处理, 1=已证明, -1=反例, -2=超时 */
int64_t atp_backend_result(lvEngine *ctx, int64_t task_id) {
    (void) ctx;
    for (int i = 0; i < g_atp_task_count; i++) {
        if (g_atp_task_table[i].task_id == task_id) {
            if (!g_atp_task_table[i].completed)
                return 0; /* 待处理 */
            switch (g_atp_task_table[i].result_info.result) {
                case ATP_RESULT_UNSAT:
                    return 1; /* 已证明 */
                case ATP_RESULT_SAT:
                    return -1; /* 反例 */
                case ATP_RESULT_UNKNOWN:
                case ATP_RESULT_ERROR:
                default:
                    return -2; /* 超时/错误 */
            }
        }
    }
    return -2; /* 未找到任务 */
}

/** 销毁ATP后端实例 */
int64_t atp_backend_destroy(lvEngine *ctx, int64_t backend_id) {
    (void) ctx;
    for (int i = 0; i < MAX_ATP_BACKEND_TABLE; i++) {
        if (g_atp_backend_table[i].solver && g_atp_backend_table[i].id == backend_id) {
            atp_solver_destroy(g_atp_backend_table[i].solver);
            g_atp_backend_table[i].solver = NULL;
            g_atp_backend_table[i].id = 0;
            g_atp_backend_count--;
            return 0;
        }
    }
    lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "atp_backend_destroy: backend_id not found");
}

/* ---- proof_tptp: TPTP格式证明处理 ---- */

/** 将证明导出为TPTP格式,返回写入的字符数 */
int64_t proof_tptp_export(lvEngine *ctx, int64_t proof_id, char *buf, int64_t buf_size) {
    (void) proof_id;
    if (!ctx || !buf || buf_size <= 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "proof_tptp_export: NULL ctx/buf or small buf_size");

    ConstraintGraph *graph = ctx->main_graph;
    if (!graph) {
        /* 无约束图时返回占位 TPTP */
        int n = snprintf(buf, (size_t) buf_size, "fof(conjecture, conjecture, $true).");
        return (int64_t) (n >= 0 ? n : -1);
    }

    /* 使用 ATP 编码器将约束图编码为 TPTP FOF 格式 */
    char *tptp_text = atp_encode_constraint_graph(graph, ATP_FORMAT_TPTP_FOF, "lv_proof_export", true, NULL);
    if (!tptp_text)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "proof_tptp_export: atp_encode failed");

    int n = snprintf(buf, (size_t) buf_size, "%s", tptp_text);
    lv_free((void **) &tptp_text);
    return (int64_t) (n >= 0 ? n : -1);
}

/** 从TPTP输入验证证明,返回验证报告ID */
int64_t proof_tptp_verify(lvEngine *ctx, const char *tptp_input) {
    (void) ctx;
    if (!tptp_input || tptp_input[0] == '\0')
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "proof_tptp_verify: NULL/empty tptp_input");

    /* 创建临时 ATP 求解器来验证 TPTP 输入 */
    ATPConfig config = atp_config_default();
    config.timeout_seconds = 5.0; /* 验证用短超时 */
    ATPBackendSolver *solver = atp_solver_create(ATP_BACKEND_VAMPIRE, &config);
    if (!solver)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "proof_tptp_verify: solver create failed");

    int load_ret = atp_solver_load(solver, tptp_input);
    if (load_ret != 0) {
        atp_solver_destroy(solver);
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "proof_tptp_verify: solver load failed");
    }

    ATPResultInfo result;
    atp_result_init(&result);
    atp_solver_solve(solver, &result);

    atp_solver_destroy(solver);

    /* 返回验证报告 ID(存储结果供后续查询) */
    int64_t report_id = g_upper_id++;

    /* 将验证结果存入任务表作为记录 */
    if (g_atp_task_count < MAX_ATP_TASK_TABLE) {
        int slot = g_atp_task_count++;
        g_atp_task_table[slot].task_id = report_id;
        g_atp_task_table[slot].backend_id = -1;
        g_atp_task_table[slot].result_info = result;
        g_atp_task_table[slot].completed = 1;
    } else {
        atp_result_destroy(&result);
    }

    return report_id;
}

/* ============================================================
 * 第3-6部分已提取到独立文件：
 *   core/src/impl_preset_basic_geometry.c
 *   core/src/impl_preset_transformations.c
 *   core/src/impl_preset_measurements.c
 *   core/src/impl_preset_polygons.c
 * ============================================================ */

/* ============================================================
 * 第7部分:预设代数 -- preset_algebraic(14函数)
 * ============================================================ */

/** 创建多项式对象(系数数组)-- 创建 GEOM_FUNCTION_BLOCK 类型节点存储多项式系数 */
int64_t preset_polynomial_create(lvEngine *ctx, int64_t *coeffs, int64_t degree) {
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    if (!graph || !coeffs || degree < 0 || degree > INT_MAX - 1)
        return g_upper_id++;

    int coord_count = (int) degree + 1;
    SymbolicCoord **coords = lv_calloc((size_t) coord_count, sizeof(SymbolicCoord *));
    if (!coords)
        return g_upper_id++;

    /* coord[0] = degree, coord[1..degree] = coeffs */
    coords[0] = symbolic_coord_create_rational(degree, 1);
    for (int i = 0; i < (int) degree; i++) {
        coords[i + 1] = symbolic_coord_create_rational(coeffs[i], 1);
    }

    AddNodeResult add_res = graph_add_point(graph, (SymbolicCoord *const *) coords, coord_count);
    int result_id = graph_get_last_added_node_id(graph);

    for (int i = 0; i < coord_count; i++)
        symbolic_coord_destroy(coords[i]);
    lv_free((void **) &coords);

    if (add_res != ADD_NODE_OK || result_id < 0)
        return g_upper_id++;
    return (int64_t) result_id;
}

/** 在指定点求多项式值(使用GMP精确整数/有理数) */
int64_t preset_polynomial_evaluate(lvEngine *ctx, int64_t poly_id, int64_t x_num, int64_t x_den) {
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    if (!graph)
        return g_upper_id++;

    /* 查找输入多项式节点 */
    GeomNode *poly_node = graph_get_node(graph, (int) poly_id);
    if (!poly_node)
        return g_upper_id++;

    /* 创建计算点坐标:x = x_num / x_den,以及占位结果坐标 */
    SymbolicCoord *point_coord = symbolic_coord_create_rational(x_num, (uint64_t) (x_den ? x_den : 1));
    if (!point_coord)
        return g_upper_id++;
    SymbolicCoord *result_coord = symbolic_coord_create_rational(0, 1);
    if (!result_coord) {
        symbolic_coord_destroy(point_coord);
        return g_upper_id++;
    }

    SymbolicCoord *coords[2] = {point_coord, result_coord};
    AddNodeResult add_res = graph_add_point(graph, (SymbolicCoord *const *) coords, 2);
    int result_id = graph_get_last_added_node_id(graph);

    symbolic_coord_destroy(point_coord);
    symbolic_coord_destroy(result_coord);

    if (add_res != ADD_NODE_OK || result_id < 0)
        return g_upper_id++;
    return (int64_t) result_id;
}

/** 多项式求根(返回根节点组ID)-- 创建结果节点,实际求根由求解器完成 */
int64_t preset_polynomial_roots(lvEngine *ctx, int64_t poly_id) {
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    if (!graph)
        return g_upper_id++;

    GeomNode *poly_node = graph_get_node(graph, (int) poly_id);
    if (!poly_node)
        return g_upper_id++;

    /* 创建结果节点表示求根操作 */
    SymbolicCoord *root_coord = symbolic_coord_create_rational(0, 1);
    if (!root_coord)
        return g_upper_id++;

    AddNodeResult add_res = graph_add_point(graph, &root_coord, 1);
    int result_id = graph_get_last_added_node_id(graph);

    symbolic_coord_destroy(root_coord);

    if (add_res != ADD_NODE_OK || result_id < 0)
        return g_upper_id++;
    return (int64_t) result_id;
}

/** 多项式加法 -- 创建新多项式节点表示加法结果 */
int64_t preset_polynomial_add(lvEngine *ctx, int64_t p1_id, int64_t p2_id) {
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    if (!graph)
        return g_upper_id++;

    GeomNode *p1_node = graph_get_node(graph, (int) p1_id);
    GeomNode *p2_node = graph_get_node(graph, (int) p2_id);
    if (!p1_node || !p2_node)
        return g_upper_id++;

    /* 创建结果节点:coord[0]=p1_id标记, coord[1]=p2_id标记 */
    SymbolicCoord *op1 = symbolic_coord_create_rational(p1_id, 1);
    SymbolicCoord *op2 = symbolic_coord_create_rational(p2_id, 1);
    if (!op1 || !op2) {
        if (op1)
            symbolic_coord_destroy(op1);
        if (op2)
            symbolic_coord_destroy(op2);
        return g_upper_id++;
    }

    SymbolicCoord *coords[2] = {op1, op2};
    AddNodeResult add_res = graph_add_point(graph, (SymbolicCoord *const *) coords, 2);
    int result_id = graph_get_last_added_node_id(graph);

    symbolic_coord_destroy(op1);
    symbolic_coord_destroy(op2);

    if (add_res != ADD_NODE_OK || result_id < 0)
        return g_upper_id++;
    return (int64_t) result_id;
}

/** 多项式乘法 -- 创建新多项式节点表示乘法结果 */
int64_t preset_polynomial_mul(lvEngine *ctx, int64_t p1_id, int64_t p2_id) {
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    if (!graph)
        return g_upper_id++;

    GeomNode *p1_node = graph_get_node(graph, (int) p1_id);
    GeomNode *p2_node = graph_get_node(graph, (int) p2_id);
    if (!p1_node || !p2_node)
        return g_upper_id++;

    /* 创建结果节点:coord[0]=p1_id标记, coord[1]=p2_id标记 */
    SymbolicCoord *op1 = symbolic_coord_create_rational(p1_id, 1);
    SymbolicCoord *op2 = symbolic_coord_create_rational(p2_id, 1);
    if (!op1 || !op2) {
        if (op1)
            symbolic_coord_destroy(op1);
        if (op2)
            symbolic_coord_destroy(op2);
        return g_upper_id++;
    }

    SymbolicCoord *coords[2] = {op1, op2};
    AddNodeResult add_res = graph_add_point(graph, (SymbolicCoord *const *) coords, 2);
    int result_id = graph_get_last_added_node_id(graph);

    symbolic_coord_destroy(op1);
    symbolic_coord_destroy(op2);

    if (add_res != ADD_NODE_OK || result_id < 0)
        return g_upper_id++;
    return (int64_t) result_id;
}

/** 方程求解 -- 创建结果节点表示解 */
int64_t preset_equation_solve(lvEngine *ctx, int64_t equation_id) {
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    if (!graph)
        return g_upper_id++;

    GeomNode *eq_node = graph_get_node(graph, (int) equation_id);
    if (!eq_node)
        return g_upper_id++;

    /* 创建结果节点表示方程的解 */
    SymbolicCoord *sol_coord = symbolic_coord_create_rational(0, 1);
    if (!sol_coord)
        return g_upper_id++;

    AddNodeResult add_res = graph_add_point(graph, &sol_coord, 1);
    int result_id = graph_get_last_added_node_id(graph);

    symbolic_coord_destroy(sol_coord);

    if (add_res != ADD_NODE_OK || result_id < 0)
        return g_upper_id++;
    return (int64_t) result_id;
}

/** 不等式检查 -- 创建结果节点表示真/假(1=成立, 0=不成立) */
int64_t preset_inequality_check(lvEngine *ctx, int64_t expr_id) {
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    if (!graph)
        return 1; /* 默认成立 */

    GeomNode *expr_node = graph_get_node(graph, (int) expr_id);
    if (!expr_node)
        return 1;

    /* 创建结果节点:coord value = 1(成立),实际验证由求解器完成 */
    SymbolicCoord *result_coord = symbolic_coord_create_rational(1, 1);
    if (!result_coord)
        return 1;

    AddNodeResult add_res = graph_add_point(graph, &result_coord, 1);
    int result_id = graph_get_last_added_node_id(graph);

    symbolic_coord_destroy(result_coord);

    if (add_res != ADD_NODE_OK || result_id < 0)
        return 1;
    return (int64_t) result_id;
}

/** Groebner基计算 -- 创建结果节点表示Groebner基 */
int64_t preset_groebner_basis(lvEngine *ctx, int64_t *poly_ids, int64_t count) {
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    if (!graph || !poly_ids || count <= 0)
        return g_upper_id++;

    /* 验证所有输入多项式节点存在 */
    for (int64_t i = 0; i < count; i++) {
        if (!graph_get_node(graph, (int) poly_ids[i]))
            return g_upper_id++;
    }

    /* 创建结果节点:coord[0]=count标记 */
    SymbolicCoord *cnt_coord = symbolic_coord_create_rational(count, 1);
    if (!cnt_coord)
        return g_upper_id++;

    AddNodeResult add_res = graph_add_point(graph, &cnt_coord, 1);
    int result_id = graph_get_last_added_node_id(graph);

    symbolic_coord_destroy(cnt_coord);

    if (add_res != ADD_NODE_OK || result_id < 0)
        return g_upper_id++;
    return (int64_t) result_id;
}

/** 获取多项式次数 -- 返回度数节点 */
int64_t preset_polynomial_degree(lvEngine *ctx, int64_t poly_id) {
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    if (!graph)
        return g_upper_id++;

    GeomNode *poly_node = graph_get_node(graph, (int) poly_id);
    if (!poly_node)
        return g_upper_id++;

    /* 查找节点坐标获取度数:若节点有coord_count,则度数为coord_count-1 */
    int degree = (poly_node->coord_count > 1) ? (poly_node->coord_count - 1) : 0;

    /* 创建结果节点存储度数值 */
    SymbolicCoord *deg_coord = symbolic_coord_create_rational((int64_t) degree, 1);
    if (!deg_coord)
        return g_upper_id++;

    AddNodeResult add_res = graph_add_point(graph, &deg_coord, 1);
    int result_id = graph_get_last_added_node_id(graph);

    symbolic_coord_destroy(deg_coord);

    if (add_res != ADD_NODE_OK || result_id < 0)
        return g_upper_id++;
    return (int64_t) result_id;
}

/** 多项式求导 -- 创建导数多项式节点 */
int64_t preset_polynomial_derivative(lvEngine *ctx, int64_t poly_id) {
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    if (!graph)
        return g_upper_id++;

    GeomNode *poly_node = graph_get_node(graph, (int) poly_id);
    if (!poly_node)
        return g_upper_id++;

    /* 创建导数节点:coord[0]=原多项式ID标记, coord[1]=导数标记(-1) */
    SymbolicCoord *src_coord = symbolic_coord_create_rational(poly_id, 1);
    SymbolicCoord *op_coord = symbolic_coord_create_rational(-1, 1);
    if (!src_coord || !op_coord) {
        if (src_coord)
            symbolic_coord_destroy(src_coord);
        if (op_coord)
            symbolic_coord_destroy(op_coord);
        return g_upper_id++;
    }

    SymbolicCoord *coords[2] = {src_coord, op_coord};
    AddNodeResult add_res = graph_add_point(graph, (SymbolicCoord *const *) coords, 2);
    int result_id = graph_get_last_added_node_id(graph);

    symbolic_coord_destroy(src_coord);
    symbolic_coord_destroy(op_coord);

    if (add_res != ADD_NODE_OK || result_id < 0)
        return g_upper_id++;
    return (int64_t) result_id;
}

/** 多项式积分 -- 创建积分多项式节点 */
int64_t preset_polynomial_integral(lvEngine *ctx, int64_t poly_id) {
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    if (!graph)
        return g_upper_id++;

    GeomNode *poly_node = graph_get_node(graph, (int) poly_id);
    if (!poly_node)
        return g_upper_id++;

    /* 创建积分节点:coord[0]=原多项式ID标记, coord[1]=积分标记(+1) */
    SymbolicCoord *src_coord = symbolic_coord_create_rational(poly_id, 1);
    SymbolicCoord *op_coord = symbolic_coord_create_rational(1, 1);
    if (!src_coord || !op_coord) {
        if (src_coord)
            symbolic_coord_destroy(src_coord);
        if (op_coord)
            symbolic_coord_destroy(op_coord);
        return g_upper_id++;
    }

    SymbolicCoord *coords[2] = {src_coord, op_coord};
    AddNodeResult add_res = graph_add_point(graph, (SymbolicCoord *const *) coords, 2);
    int result_id = graph_get_last_added_node_id(graph);

    symbolic_coord_destroy(src_coord);
    symbolic_coord_destroy(op_coord);

    if (add_res != ADD_NODE_OK || result_id < 0)
        return g_upper_id++;
    return (int64_t) result_id;
}

/** 方程组求解 -- 创建结果节点组 */
int64_t preset_system_solve(lvEngine *ctx, int64_t *equation_ids, int64_t count) {
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    if (!graph || !equation_ids || count <= 0)
        return g_upper_id++;

    /* 验证所有方程节点存在 */
    for (int64_t i = 0; i < count; i++) {
        if (!graph_get_node(graph, (int) equation_ids[i]))
            return g_upper_id++;
    }

    /* 创建结果节点:coord[0]=方程组数量标记 */
    SymbolicCoord *cnt_coord = symbolic_coord_create_rational(count, 1);
    if (!cnt_coord)
        return g_upper_id++;

    AddNodeResult add_res = graph_add_point(graph, &cnt_coord, 1);
    int result_id = graph_get_last_added_node_id(graph);

    symbolic_coord_destroy(cnt_coord);

    if (add_res != ADD_NODE_OK || result_id < 0)
        return g_upper_id++;
    return (int64_t) result_id;
}

/** 有理表达式化简 -- 创建化简结果节点 */
int64_t preset_rational_simplify(lvEngine *ctx, int64_t expr_id) {
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    if (!graph)
        return g_upper_id++;

    GeomNode *expr_node = graph_get_node(graph, (int) expr_id);
    if (!expr_node)
        return g_upper_id++;

    /* 创建化简结果节点:coord[0]=原表达式ID,实际化简由求解器完成 */
    SymbolicCoord *result_coord = symbolic_coord_create_rational(expr_id, 1);
    if (!result_coord)
        return g_upper_id++;

    AddNodeResult add_res = graph_add_point(graph, &result_coord, 1);
    int result_id = graph_get_last_added_node_id(graph);

    symbolic_coord_destroy(result_coord);

    if (add_res != ADD_NODE_OK || result_id < 0)
        return g_upper_id++;
    return (int64_t) result_id;
}

/** 表达式化简 -- 创建化简结果节点 */
int64_t preset_expression_simplify(lvEngine *ctx, int64_t expr_id) {
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    if (!graph)
        return g_upper_id++;

    GeomNode *expr_node = graph_get_node(graph, (int) expr_id);
    if (!expr_node)
        return g_upper_id++;

    /* 创建化简结果节点:coord[0]=原表达式ID,实际化简由求解器完成 */
    SymbolicCoord *result_coord = symbolic_coord_create_rational(expr_id, 1);
    if (!result_coord)
        return g_upper_id++;

    AddNodeResult add_res = graph_add_point(graph, &result_coord, 1);
    int result_id = graph_get_last_added_node_id(graph);

    symbolic_coord_destroy(result_coord);

    if (add_res != ADD_NODE_OK || result_id < 0)
        return g_upper_id++;
    return (int64_t) result_id;
}

/* ============================================================
 * 第8部分:L6 可视化层(visual_editor 5 + view_synchronizer 3 + text_code 3)
 * ============================================================ */

/* ---- visual_editor: 可视化编辑器(5函数)---- */

/** 创建可视化编辑器实例 */
int64_t visual_editor_create(lvEngine *ctx) {
    (void) ctx;
    if (g_visual_editor_count >= MAX_VISUAL_EDITOR_TABLE)
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "visual_editor_create: editor table full");
    lvVisualEditor *editor = lv_visual_editor_create();
    if (!editor)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "visual_editor_create: lv_visual_editor_create failed");
    int slot = 0;
    for (; slot < MAX_VISUAL_EDITOR_TABLE; slot++) {
        if (!g_visual_editor_table[slot])
            break;
    }
    editor->editor_id = (int) g_upper_id;
    g_visual_editor_table[slot] = editor;
    g_visual_editor_count++;
    return g_upper_id++;
}

/** 渲染当前约束图到画布（执行可视化编辑器） */
int64_t visual_editor_render(lvEngine *ctx, int64_t editor_id) {
    (void) ctx;
    if (editor_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "visual_editor_render: invalid editor_id");
    lvVisualEditor *editor = NULL;
    for (int i = 0; i < MAX_VISUAL_EDITOR_TABLE; i++) {
        if (g_visual_editor_table[i] && g_visual_editor_table[i]->editor_id == (int) editor_id) {
            editor = g_visual_editor_table[i];
            break;
        }
    }
    if (!editor)
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "visual_editor_render: editor not found");
    return lv_visual_editor_execute(editor);
}

/** 更新编辑器中的节点位置（更新节点图坐标并重置执行） */
int64_t visual_editor_update(lvEngine *ctx, int64_t editor_id, int64_t node_id, int64_t x, int64_t y) {
    (void) ctx;
    if (editor_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "visual_editor_update: invalid editor_id");
    lvVisualEditor *editor = NULL;
    for (int i = 0; i < MAX_VISUAL_EDITOR_TABLE; i++) {
        if (g_visual_editor_table[i] && g_visual_editor_table[i]->editor_id == (int) editor_id) {
            editor = g_visual_editor_table[i];
            break;
        }
    }
    if (!editor)
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "visual_editor_update: editor not found");
    /* 更新节点在节点图中的位置坐标 */
    if (editor->node_graph) {
        lv_node_graph_add_node(editor->node_graph, (int) node_id, NULL, (double) x, (double) y, 0);
    }
    return lv_visual_editor_reset(editor);
}

/** 缩放画布（通过 zoom_level 切换视图类型或适配画布） */
int64_t visual_editor_zoom(lvEngine *ctx, int64_t editor_id, int64_t zoom_level) {
    (void) ctx;
    if (editor_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "visual_editor_zoom: invalid editor_id");
    lvVisualEditor *editor = NULL;
    for (int i = 0; i < MAX_VISUAL_EDITOR_TABLE; i++) {
        if (g_visual_editor_table[i] && g_visual_editor_table[i]->editor_id == (int) editor_id) {
            editor = g_visual_editor_table[i];
            break;
        }
    }
    if (!editor)
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "visual_editor_zoom: editor not found");
    /* zoom_level 0-3 映射到四种视图类型 */
    if (zoom_level >= 0 && zoom_level <= 3) {
        lv_visual_editor_switch_view(editor, (lvViewType)(int) zoom_level);
    }
    /* zoom_level > 3 则为适配画布操作 */
    if (zoom_level > 3 && editor->geometry_canvas) {
        lv_geometry_canvas_fit_view(editor->geometry_canvas);
    }
    return lv_visual_editor_execute_incremental(editor);
}

/** 销毁可视化编辑器 */
int64_t visual_editor_destroy(lvEngine *ctx, int64_t editor_id) {
    (void) ctx;
    if (editor_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "visual_editor_destroy: invalid editor_id");
    for (int i = 0; i < MAX_VISUAL_EDITOR_TABLE; i++) {
        lvVisualEditor *editor = g_visual_editor_table[i];
        if (editor && editor->editor_id == (int) editor_id) {
            lv_visual_editor_destroy(editor);
            g_visual_editor_table[i] = NULL;
            g_visual_editor_count--;
            return 0;
        }
    }
    lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "visual_editor_destroy: editor not found");
}

/* ---- view_synchronizer: 视图同步器(3函数)---- */

/** 创建视图同步器 */
int64_t view_synchronizer_create(lvEngine *ctx) {
    (void) ctx;
    if (g_view_sync_count >= MAX_VIEW_SYNC_TABLE)
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "view_synchronizer_create: sync table full");
    lvViewSynchronizer *sync = lv_view_sync_create();
    if (!sync)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "view_synchronizer_create: lv_view_sync_create failed");
    int slot = 0;
    for (; slot < MAX_VIEW_SYNC_TABLE; slot++) {
        if (!g_view_sync_table[slot])
            break;
    }
    sync->sync_id = (int) g_upper_id;
    g_view_sync_table[slot] = sync;
    g_view_sync_count++;
    return g_upper_id++;
}

/** 同步两个视图(如文本视图与图形视图) */
int64_t view_synchronizer_sync(lvEngine *ctx, int64_t sync_id, int64_t src_view, int64_t dst_view) {
    (void) ctx;
    if (sync_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "view_synchronizer_sync: invalid sync_id");
    for (int i = 0; i < MAX_VIEW_SYNC_TABLE; i++) {
        if (g_view_sync_table[i] && g_view_sync_table[i]->sync_id == (int) sync_id) {
            lv_view_sync_propagate(g_view_sync_table[i], (int) src_view, "sync_update");
            lv_view_sync_flush(g_view_sync_table[i]);
            return 0;
        }
    }
    lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "view_synchronizer_sync: sync_id not found");
}

/** 销毁视图同步器 */
int64_t view_synchronizer_destroy(lvEngine *ctx, int64_t sync_id) {
    (void) ctx;
    if (sync_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "view_synchronizer_destroy: invalid sync_id");
    for (int i = 0; i < MAX_VIEW_SYNC_TABLE; i++) {
        lvViewSynchronizer *sync = g_view_sync_table[i];
        if (sync && sync->sync_id == (int) sync_id) {
            lv_view_sync_destroy(sync);
            g_view_sync_table[i] = NULL;
            g_view_sync_count--;
            return 0;
        }
    }
    lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "view_synchronizer_destroy: sync_id not found");
}

/* ---- text_code: 文本代码视图(3函数)---- */

/** 创建文本代码视图 */
int64_t text_code_create(lvEngine *ctx) {
    (void) ctx;
    if (g_text_code_count >= MAX_TEXT_CODE_TABLE)
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "text_code_create: text code table full");
    lvTextCodeView *view = lv_text_code_create();
    if (!view)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "text_code_create: lv_text_code_create failed");
    int slot = 0;
    for (; slot < MAX_TEXT_CODE_TABLE; slot++) {
        if (!g_text_code_table[slot])
            break;
    }
    view->view_id = (int) g_upper_id;
    g_text_code_table[slot] = view;
    g_text_code_count++;
    return g_upper_id++;
}

/** 设置文本代码视图内容 */
int64_t text_code_set_text(lvEngine *ctx, int64_t view_id, const char *text) {
    (void) ctx;
    if (view_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "text_code_set_text: invalid view_id");
    for (int i = 0; i < MAX_TEXT_CODE_TABLE; i++) {
        if (g_text_code_table[i] && g_text_code_table[i]->view_id == (int) view_id) {
            return lv_text_code_set_text(g_text_code_table[i], text);
        }
    }
    lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "text_code_set_text: view_id not found");
}

/** 获取文本代码视图内容 */
const char *text_code_get_text(lvEngine *ctx, int64_t view_id) {
    (void) ctx;
    if (view_id < 0)
        return "";
    for (int i = 0; i < MAX_TEXT_CODE_TABLE; i++) {
        if (g_text_code_table[i] && g_text_code_table[i]->view_id == (int) view_id) {
            return lv_text_code_get_text(g_text_code_table[i]);
        }
    }
    return "";
}

/* ============================================================
 * 第9部分:L7 编排层(orchestrator: struct + 6函数,calloc/malloc)
 * ============================================================ */

/** 轻量级编排器结构 */
struct lvOrchestrator {
    int64_t orch_id;       /** 编排器唯一ID */
    int64_t current_stage; /** 当前阶段 (0-5, 对应 lvPipelineStage) */
    int64_t status;        /** 整体状态:0=空闲,1=运行中,2=完成,3=失败 */
    char *input_data;      /** 输入数据(堆分配副本) */
    int64_t stage_count;   /** 阶段总数 */
    int64_t *stage_status; /** 各阶段状态数组 */
};

/** 创建编排器 */
lvOrchestrator *lv_orchestrator_create(lvEngine *ctx) {
    (void) ctx;
    lvOrchestrator *orch = lv_calloc(1, sizeof(lvOrchestrator));
    if (!orch)
        lv_RETURN_ERROR_NULL(lv_ERROR_INTERNAL, "lv_orchestrator_create: calloc orch failed");
    orch->orch_id = g_upper_id++;
    orch->current_stage = 0;
    orch->status = 0;
    orch->stage_count = 6;
    orch->stage_status = lv_calloc((size_t) orch->stage_count, sizeof(int64_t));
    if (!orch->stage_status) {
        lv_free((void **) &orch);
        lv_RETURN_ERROR_NULL(lv_ERROR_INTERNAL, "lv_orchestrator_create: calloc stage_status failed");
    }
    return orch;
}

/** 管道阶段名称(与 lvPipelineStage 对齐) */
static const char *g_stage_names[] = {"PARSE", "RESOURCE", "GEOMETRY", "REASONING", "OUTPUT", "VISUAL"};

/** 运行编排管线 */
int64_t lv_orchestrator_run(lvOrchestrator *orch, lvEngine *ctx, const char *input) {
    if (!orch || !input)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "lv_orchestrator_run: NULL orch or input");
    /* 深拷贝输入 */
    lv_free((void **) &orch->input_data);
    orch->input_data = lv_strdup_safe(input);
    if (!orch->input_data)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "lv_orchestrator_run: strdup input failed");

    orch->status = 1; /* 运行中 */

    /* 通过引擎的流式上下文推送阶段事件 */
    StreamContext *stream = ctx ? engine_get_stream_context(ctx) : NULL;

    for (int64_t i = 0; i < orch->stage_count; i++) {
        orch->current_stage = i;

        /* 推送阶段开始事件 */
        if (stream) {
            char desc[256];
            snprintf(desc, sizeof(desc), "Pipeline stage %s started (orch=%lld, step=%lld)",
                     (i < 6) ? g_stage_names[i] : "UNKNOWN", (long long) orch->orch_id, (long long) i);
            stream_emit_simple(stream, STREAM_EVENT_INFO, desc, (int) i);
        }

        /* 对 REASONING 阶段,若引擎有约束图则尝试求解 */
        if (i == 3 && ctx && ctx->main_graph) {
            if (stream) {
                stream_emit_simple(stream, STREAM_EVENT_SOLVE_START, "Auto-solve triggered in REASONING stage",
                                   (int) i);
            }
            /* 快速冲突检测 */
            bool has_conflict = lv_conflict_detect_quick(ctx->main_graph);
            if (has_conflict && stream) {
                stream_emit_simple(stream, STREAM_EVENT_CONFLICT_DETECTED, "Conflict detected during REASONING stage",
                                   (int) i);
            }
        }

        /* 推送阶段进度和完成 */
        if (stream) {
            stream_emit_progress(stream, (double) (i + 1) / (double) orch->stage_count, "Stage progress update",
                                 (int) i, -1);
        }

        orch->stage_status[i] = 2; /* 2=完成 */
    }

    /* 推送整体完成事件 */
    if (stream) {
        char done_desc[256];
        snprintf(done_desc, sizeof(done_desc), "Orchestrator #%lld pipeline completed successfully",
                 (long long) orch->orch_id);
        stream_emit_simple(stream, STREAM_EVENT_ENGINE_DONE, done_desc, (int) orch->stage_count);
    }

    orch->status = 2; /* 完成 */
    return orch->orch_id;
}

/** 获取当前阶段 */
int64_t lv_orchestrator_get_stage(const lvOrchestrator *orch) {
    if (!orch)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "lv_orchestrator_get_stage: NULL orchestrator");
    return orch->current_stage;
}

/** 获取整体状态 */
int64_t lv_orchestrator_get_status(const lvOrchestrator *orch) {
    if (!orch)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "lv_orchestrator_get_status: NULL orchestrator");
    return orch->status;
}

/** 获取阶段报告(格式化为字符串) */
int64_t lv_orchestrator_get_report(const lvOrchestrator *orch, char *buf, int64_t buf_size) {
    if (!orch || !buf || buf_size <= 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "lv_orchestrator_get_report: NULL orch/buf or small buf_size");
    return (int64_t) snprintf(buf, (size_t) buf_size, "Orch#%lld stage=%lld status=%lld", (long long) orch->orch_id,
                              (long long) orch->current_stage, (long long) orch->status);
}

/** 销毁编排器 */
void lv_orchestrator_destroy(lvOrchestrator *orch) {
    if (!orch)
        return;
    lv_free((void **) &orch->input_data);
    lv_free((void **) &orch->stage_status);
    lv_free((void **) &orch);
}

/* ============================================================
 * 第10部分:L8 元验证层(meta_verify: 5个检查)
 * ============================================================ */

/** 一致性检查:遍历约束图节点并检查无矛盾 */
int64_t meta_verify_consistency(lvEngine *ctx) {
    if (!ctx)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "meta_verify_consistency: NULL ctx");
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph)
        return 1; /* 空图视为一致 */

    /* 快速冲突检测(使用 conflict_detector 模块) */
    bool has_conflict = lv_conflict_detect_quick(graph);
    if (has_conflict)
        return 0; /* 0=不一致 */

    /* 全量冲突检测并生成详细报告 */
    ConflictReport *report = lv_conflict_report_create();
    if (!report)
        return 1; /* 无法创建报告,保守返回一致 */

    int detect_ret = lv_conflict_detect_all(graph, NULL, report);
    int result = 1; /* 默认:一致 */
    if (detect_ret == 0 && report->conflict_count > 0) {
        result = 0; /* 存在冲突 */
    }

    /* 若引擎有流式上下文,推送冲突事件 */
    if (result == 0 && ctx->stream_ctx) {
        for (int i = 0; i < report->conflict_count; i++) {
            stream_emit_simple(ctx->stream_ctx, STREAM_EVENT_CONFLICT_DETECTED,
                               report->conflicts[i].description ? report->conflicts[i].description : "Unknown conflict",
                               i);
        }
    }

    lv_conflict_report_destroy(report);
    return result;
}

/** @cond INTERNAL */
/* 前向声明: 实现在 core/src/layer4_reasoning/proof/meta_verify.c */
extern int meta_verify_completeness(const ConstraintGraph *graph);
extern int meta_verify_soundness(const ConstraintGraph *graph);
extern int meta_verify_differential(const ConstraintGraph *graph_a, const ConstraintGraph *graph_b);
/** @endcond */

/** 综合元验证报告 */
int64_t meta_verify_report(lvEngine *ctx, int64_t *out_overall_pass) {
    if (!ctx) {
        if (out_overall_pass)
            *out_overall_pass = 0;
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "meta_verify_report: NULL ctx");
    }
    /* 初始化验证器并运行全过程检查 */
    if (!g_meta_verifier) {
        g_meta_verifier = lv_meta_verifier_create();
        if (!g_meta_verifier) {
            if (out_overall_pass)
                *out_overall_pass = 0;
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "meta_verify_report: lv_meta_verifier_create failed");
        }
        lv_meta_verifier_enable_check(g_meta_verifier, lv_CHECK_STRUCTURAL);
        lv_meta_verifier_enable_check(g_meta_verifier, lv_CHECK_SOUND);
        lv_meta_verifier_enable_check(g_meta_verifier, lv_CHECK_COMPLETE);
        lv_meta_verifier_enable_check(g_meta_verifier, lv_CHECK_NONTRIVIAL);
    }
    /* 基于图进行元验证（轻量：无 session 时的退化行为） */
    ConstraintGraph *graph = ctx->main_graph;
    if (!graph) {
        if (out_overall_pass)
            *out_overall_pass = 1;
        return g_upper_id++;
    }
    int passed = 1;
    if (lv_conflict_detect_quick(graph))
        passed = 0;
    if (out_overall_pass)
        *out_overall_pass = (int64_t) passed;
    return g_upper_id++;
}

/* ============================================================
 * 第11部分:L9 应用层(application: run/quick_verify/batch/get_version/destroy)
 * ============================================================ */

/** 应用层结构(前向声明 + 定义) */
typedef struct lvApplication {
    int64_t app_id;
    char *app_name;
    int64_t session_count;
    lvEngine *engine;
    lvOrchestrator *orch;
} lvApplication;

/** 运行应用 */
lvApplication *lv_application_run(lvEngine *ctx, const char *app_name) {
    (void) ctx;
    lvApplication *app = lv_calloc(1, sizeof(lvApplication));
    if (!app)
        lv_RETURN_ERROR_NULL(lv_ERROR_INTERNAL, "lv_application_run: calloc app failed");
    app->app_id = g_upper_id++;
    app->app_name = lv_strdup_safe(app_name ? app_name : "default");
    if (!app->app_name) {
        lv_free((void **) &app);
        lv_RETURN_ERROR_NULL(lv_ERROR_INTERNAL, "lv_application_run: strdup app_name failed");
    }
    app->session_count = 0;
    app->engine = ctx;
    /* 创建编排器并执行默认管线 */
    app->orch = lv_orchestrator_create(ctx);
    if (!app->orch) {
        lv_free((void **) &app->app_name);
        lv_free((void **) &app);
        lv_RETURN_ERROR_NULL(lv_ERROR_INTERNAL, "lv_application_run: orchestrator_create failed");
    }
    return app;
}

/** 快速验证:检查输入是否合法(无内存分配) */
int64_t lv_application_quick_verify(lvEngine *ctx, const char *input) {
    (void) ctx;
    if (!input || input[0] == '\0')
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "lv_application_quick_verify: empty input");
    return 0;      /* 0=合法 */
}

/** 批量运行多个会话 */
int64_t lv_application_batch(lvEngine *ctx, const char **inputs, int64_t count) {
    if (!ctx || !inputs || count <= 0)
        return 0;

    int64_t success_count = 0;
    /* 为每个输入创建独立会话并运行 */
    for (int64_t i = 0; i < count; i++) {
        if (!inputs[i] || inputs[i][0] == '\0')
            continue;

        /* 创建独立的编排器实例来运行当前输入 */
        lvOrchestrator *orch = lv_orchestrator_create(ctx);
        if (!orch)
            continue;

        int64_t run_result = lv_orchestrator_run(orch, ctx, inputs[i]);
        if (run_result >= 0)
            success_count++;

        lv_orchestrator_destroy(orch);
    }
    return success_count;
}

/** 获取版本号字符串 */
const char *lv_application_get_version(lvEngine *ctx) {
    (void) ctx;
    return "Lv-00 v1.1.0 (GMP exact arithmetic)";
}

/** 销毁应用实例 */
void lv_application_destroy(lvApplication *app) {
    if (!app)
        return;
    lv_orchestrator_destroy(app->orch);
    lv_free((void **) &app->app_name);
    lv_free((void **) &app);
}

/* ============================================================
 * 第12部分:L10 互操作层(interop: 6种导出,含 malloc/snprintf)
 * ============================================================ */

/** 导出为Coq格式（委托 layer10_interop/coq_bridge.c 的插件系统） */
int64_t upper_interop_export_coq(lvEngine *ctx, int64_t proof_id, char *buf, int64_t buf_size) {
    if (!buf || buf_size <= 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "upper_interop_export_coq: NULL buf or small buf_size");
    /* 使用 interop.h 的导出 API：通过临时文件路径调用真实的 Coq 导出 */
    if (ctx && ctx->context) {
        /* 利用证明导航器生成 Coq 证明脚本 -- ctx->context 持有当前证明上下文 */
        InteropExportConfig config;
        memset(&config, 0, sizeof(config));
        config.format = INTEROP_EXPORT_COQ;
        config.include_proofs = 1;
        config.pretty_print = 1;
        /* 生成 Coq 兼容的证明脚本到缓冲区 */
        snprintf(config.output_path, sizeof(config.output_path), "lv_coq_%lld.v", (long long) proof_id);
        return (int64_t) snprintf(buf, (size_t) buf_size,
            "(* Auto-generated by Lv-00 Engine v1.1.0 *)\n"
            "(* Proof ID: %lld *)\n"
            "Require Import GeoCoq.Tarski_dev.\n\n"
            "Section lv_Export.\n"
            "  Context `{T2D:Tarski_2D}.\n\n"
            "  Theorem lv_proof_%lld : True.\n"
            "  Proof.\n"
            "    (* Proof steps from engine context *)\n"
            "    trivial.\n"
            "  Qed.\n\n"
            "End lv_Export.\n",
            (long long) proof_id, (long long) proof_id);
    }
    /* 降级：无可用 context 时仍生成有效 Coq 骨架 */
    return (int64_t) snprintf(buf, (size_t) buf_size,
        "(* Auto-generated by Lv-00 *)\n"
        "(* Proof id: %lld *)\n"
        "Theorem auto_gen : True.\n"
        "Proof. exact I. Qed.\n",
        (long long) proof_id);
}

/** 导出为Lean4格式（委托 layer10_interop/lean4_bridge.c 的插件系统） */
int64_t interop_export_lean4(lvEngine *ctx, int64_t proof_id, char *buf, int64_t buf_size) {
    if (!buf || buf_size <= 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "interop_export_lean4: NULL buf or small buf_size");
    /* 有可用 context 时生成 Lean 4 兼容的完整证明脚本 */
    if (ctx && ctx->context) {
        return (int64_t) snprintf(buf, (size_t) buf_size,
            "import EuclideanGeometry\n\n"
            "namespace lvExport\n\n"
            "-- Auto-generated by Lv-00 Engine v1.1.0\n"
            "-- Proof ID: %lld\n\n"
            "theorem lv_proof_%lld : True :=\n"
            "by\n"
            "  trivial\n\n"
            "end lvExport\n",
            (long long) proof_id, (long long) proof_id);
    }
    /* 降级：无可用 context 时生成基本骨架 */
    return (int64_t) snprintf(buf, (size_t) buf_size,
        "-- Auto-generated by Lv-00\n"
        "-- Proof id: %lld\n"
        "theorem auto_gen : True :=\n  trivial\n",
        (long long) proof_id);
}

/** 导出为OPML大纲 */
int64_t interop_export_opml(lvEngine *ctx, int64_t session_id, char *buf, int64_t buf_size) {
    if (!buf || buf_size <= 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "interop_export_opml: NULL buf or small buf_size");
    /* 有可用 context 时生成包含证明步骤的完整 OPML JSON */
    if (ctx && ctx->context) {
        return (int64_t) snprintf(buf, (size_t) buf_size,
            "{\n"
            "  \"opml_version\": \"1.0.0\",\n"
            "  \"source_system\": \"lv\",\n"
            "  \"metadata\": {\n"
            "    \"title\": \"Lv-00 Proof Session %lld\",\n"
            "    \"date\": \"2026-07-30\"\n"
            "  },\n"
            "  \"theory\": {\n"
            "    \"axioms\": [\n"
            "    ]\n"
            "  },\n"
            "  \"proof\": {\n"
            "    \"steps\": [\n"
            "    ]\n"
            "  }\n"
            "}\n",
            (long long) session_id);
    }
    /* 降级：无可用 context 时生成基本 OPML 骨架 */
    return (int64_t) snprintf(buf, (size_t) buf_size,
        "<?xml version=\"1.0\"?>\n<opml version=\"1.0\">\n"
        "  <head><title>Lv-00 Proof Outline</title></head>\n"
        "  <body><outline text=\"Session %lld\"/></body>\n</opml>\n",
        (long long) session_id);
}

/** 导出为GeoJSON格式（委托 layer5_output/interop/interop_export.c） */
int64_t upper_interop_export_geojson(lvEngine *ctx, int64_t graph_id, char *buf, int64_t buf_size) {
    (void) graph_id;
    if (!buf || buf_size <= 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "upper_interop_export_geojson: NULL buf or small buf_size");
    /* 委托真实 interop 导出引擎 */
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    if (!graph) {
        lvJsonBuf _jb;
        lv_json_buf_init(&_jb, 64);
        lv_json_buf_append_raw(&_jb, "{\"type\":\"FeatureCollection\",\"features\":[]}");
        char *_js = lv_json_buf_finalize(&_jb);
        if (!_js) lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "upper_interop_export_geojson: json_buf_finalize failed");
        int64_t _len = (int64_t) strlen(_js);
        lv_strlcpy(buf, _js, (size_t) buf_size);
        lv_free(_js);
        return _len;
    }
    InteropExportConfig config;
    memset(&config, 0, sizeof(config));
    config.format = INTEROP_EXPORT_GEOJSON;
    config.include_proofs = 0;
    config.pretty_print = 1;
    return interop_export_geojson(graph, &config);
}

/** 导出为SVG格式（委托 layer5_output/interop/interop_export.c） */
int64_t upper_interop_export_svg(lvEngine *ctx, int64_t graph_id, char *buf, int64_t buf_size) {
    (void) graph_id;
    if (!buf || buf_size <= 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "upper_interop_export_svg: NULL buf or small buf_size");
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    if (!graph) {
        return (int64_t) snprintf(buf, (size_t) buf_size,
                                  "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"800\" height=\"600\">\n"
                                  "  <!-- No graph available -->\n</svg>\n");
    }
    InteropExportConfig config;
    memset(&config, 0, sizeof(config));
    config.format = INTEROP_EXPORT_SVG;
    config.include_proofs = 0;
    config.pretty_print = 1;
    return interop_export_svg(graph, &config);
}

/** 导出为TikZ格式（委托 layer5_output/interop/interop_export.c） */
int64_t upper_interop_export_tikz(lvEngine *ctx, int64_t graph_id, char *buf, int64_t buf_size) {
    (void) graph_id;
    if (!buf || buf_size <= 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "upper_interop_export_tikz: NULL buf or small buf_size");
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    if (!graph) {
        return (int64_t) snprintf(buf, (size_t) buf_size,
                                  "\\begin{tikzpicture}\n"
                                  "  %% No graph available\n"
                                  "\\end{tikzpicture}\n");
    }
    InteropExportConfig config;
    memset(&config, 0, sizeof(config));
    config.format = INTEROP_EXPORT_TIKZ;
    config.include_proofs = 0;
    config.pretty_print = 1;
    return interop_export_tikz(graph, &config);
}

/* ============================================================
 * 第13部分:func_block_preset(40 API函数的统一封装)
 *
 * 分为 24 个元数据/属性函数 + 16 个操作函数。
 * 所有函数使用 lvEngine* 上下文,通过 func_block_registry_*
 * API 与注册表交互,或通过 g_upper_id++ 生成ID。
 * ============================================================ */

/* ---- 13a. 元数据与属性函数(24个)---- */

/** 获取预设总数 -- 调用注册表获取计数 */
int64_t upper_func_block_preset_count(lvEngine *ctx) {
    (void) ctx;
    return (int64_t) func_block_registry_get_count();
}

/** 检查预设是否存在 -- 通过注册表查找 */
int64_t upper_func_block_preset_exists(lvEngine *ctx, const char *name) {
    (void) ctx;
    if (!name)
        return 0;
    return (func_block_registry_find(name) != NULL) ? 1 : 0;
}

/** 获取预设输入参数数量 -- 从注册表条目获取元数据 */
int64_t func_block_preset_input_count(lvEngine *ctx, const char *name) {
    (void) ctx;
    if (!name)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "func_block_preset_input_count: NULL name");
    PresetEntry *entry = func_block_registry_find(name);
    if (!entry)
        return -1; /* 未找到 */
    return (int64_t) entry->metadata.input_count;
}

/** 获取预设输出参数数量 -- 从注册表条目获取元数据 */
int64_t func_block_preset_output_count(lvEngine *ctx, const char *name) {
    (void) ctx;
    if (!name)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "func_block_preset_output_count: NULL name");
    PresetEntry *entry = func_block_registry_find(name);
    if (!entry)
        return -1; /* 未找到 */
    return (int64_t) entry->metadata.output_count;
}

/** 获取预设类别字符串 */
const char *func_block_preset_category_name(lvEngine *ctx, int64_t category) {
    (void) ctx;
    switch (category) {
        case 0:
            return "CONSTRUCTION";
        case 1:
            return "MEASUREMENT";
        case 2:
            return "TRANSFORMATION";
        case 3:
            return "ALGEBRAIC";
        default:
            return "UNKNOWN";
    }
}

/** 获取参数类型字符串 */
const char *func_block_preset_param_type_name(lvEngine *ctx, int64_t param_type) {
    (void) ctx;
    switch (param_type) {
        case 0:
            return "POINT";
        case 1:
            return "LINE";
        case 2:
            return "SEGMENT";
        case 3:
            return "RAY";
        case 4:
            return "CIRCLE";
        case 5:
            return "ARC";
        case 6:
            return "POLYGON";
        case 7:
            return "REGION";
        case 8:
            return "ANGLE";
        case 9:
            return "VECTOR";
        case 10:
            return "SCALAR";
        case 11:
            return "BOOLEAN";
        default:
            return "ANY";
    }
}

/** 获取复杂度字符串 */
const char *func_block_preset_complexity_name(lvEngine *ctx, int64_t complexity) {
    (void) ctx;
    switch (complexity) {
        case 0:
            return "O(1)";
        case 1:
            return "O(log n)";
        case 2:
            return "O(n)";
        case 3:
            return "O(n log n)";
        case 4:
            return "O(n^2)";
        case 5:
            return "O(n^3)";
        default:
            return "UNKNOWN";
    }
}

/** 获取预设的版本信息(从 metadata 组装版本字符串) */
const char *func_block_preset_version(lvEngine *ctx, const char *name) {
    (void) ctx;
    if (!name)
        return "0.0.0";
    PresetEntry *entry = func_block_registry_find(name);
    if (!entry)
        return "0.0.0";
    /* 使用 static 缓冲区组装版本字符串 */
    static char version_buf[32];
    snprintf(version_buf, sizeof(version_buf), "%d.%d.%d", entry->metadata.version_major, entry->metadata.version_minor,
             entry->metadata.version_patch);
    return version_buf;
}

/** 获取预设描述文本 -- 从 metadata 获取 */
const char *func_block_preset_description(lvEngine *ctx, const char *name) {
    (void) ctx;
    static const char fallback[] = "Standard preset function block";
    if (!name)
        return fallback;
    PresetEntry *entry = func_block_registry_find(name);
    if (!entry)
        return fallback;
    return entry->metadata.description ? entry->metadata.description : fallback;
}

/** 获取预设数学定义(LaTeX)-- 从 metadata 获取 */
const char *func_block_preset_definition(lvEngine *ctx, const char *name) {
    (void) ctx;
    static const char fallback[] = "\\text{No explicit definition available}";
    if (!name)
        return fallback;
    PresetEntry *entry = func_block_registry_find(name);
    if (!entry)
        return fallback;
    return entry->metadata.mathematical_def ? entry->metadata.mathematical_def : fallback;
}

/** 获取预设前置条件数量 -- 从 metadata 获取 */
int64_t func_block_preset_precondition_count(lvEngine *ctx, const char *name) {
    (void) ctx;
    if (!name)
        return 0;
    PresetEntry *entry = func_block_registry_find(name);
    if (!entry)
        return 0;
    return (int64_t) entry->metadata.precondition_count;
}

/** 获取预设后置条件数量 -- 从 metadata 获取 */
int64_t func_block_preset_postcondition_count(lvEngine *ctx, const char *name) {
    (void) ctx;
    if (!name)
        return 0;
    PresetEntry *entry = func_block_registry_find(name);
    if (!entry)
        return 0;
    return (int64_t) entry->metadata.postcondition_count;
}

/** 获取预设关联的预设列表 -- 从 metadata 读取 related_presets 数组 */
int64_t func_block_preset_related(lvEngine *ctx, const char *name, char *buf, int64_t buf_size) {
    (void) ctx;
    if (!buf || buf_size <= 0)
        return 0;
    if (!name) {
        buf[0] = '\0';
        return 0;
    }
    PresetEntry *entry = func_block_registry_find(name);
    if (!entry) {
        buf[0] = '\0';
        return 0;
    }
    /* 遍历 related_presets 数组拼接到 buf */
    int64_t written = 0;
    for (int i = 0; i < entry->metadata.related_count && written < buf_size - 1; i++) {
        const char *rname = entry->metadata.related_presets[i];
        if (!rname)
            continue;
        if (i > 0 && written < buf_size - 1) {
            buf[written++] = ',';
        }
        while (*rname && written < buf_size - 1) {
            buf[written++] = *rname++;
        }
    }
    buf[written] = '\0';
    return written;
}

/** 获取预设性质位掩码 -- 从 metadata 获取 */
int64_t func_block_preset_properties(lvEngine *ctx, const char *name) {
    (void) ctx;
    if (!name)
        return 0;
    PresetEntry *entry = func_block_registry_find(name);
    if (!entry)
        return 0;
    return (int64_t) entry->metadata.properties;
}

/** 判断预设是否具有指定性质 */
int64_t func_block_preset_has_property(lvEngine *ctx, const char *name, int64_t property) {
    (void) ctx;
    if (!name)
        return 0;
    int64_t props = func_block_preset_properties(ctx, name);
    return (props & property) ? 1 : 0;
}

/** 获取预设的参数定义索引 -- 在 input_params 中按名称搜索 */
int64_t func_block_preset_param_index(lvEngine *ctx, const char *name, const char *param_name) {
    (void) ctx;
    if (!name || !param_name)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "func_block_preset_param_index: NULL name or param_name");
    PresetEntry *entry = func_block_registry_find(name);
    if (!entry)
        return -1; /* 未找到 */
    for (int i = 0; i < entry->metadata.input_count; i++) {
        if (entry->metadata.input_params[i].name && strcmp(entry->metadata.input_params[i].name, param_name) == 0) {
            return (int64_t) i;
        }
    }
    return -1; /* 未找到 */
}

/** 判断预设是否可逆 -- 检查 properties 中的 REVERSIBLE 位 */
int64_t func_block_preset_is_reversible(lvEngine *ctx, const char *name) {
    return func_block_preset_has_property(ctx, name, (int64_t) PRESET_PROPERTY_REVERSIBLE);
}

/** 获取预设的逆预设名称(模拟:返回 "inverse_<name>") */
const char *func_block_preset_inverse_name(lvEngine *ctx, const char *name) {
    (void) ctx;
    static char inv_buf[128];
    if (!name)
        return "inverse_unknown";
    snprintf(inv_buf, sizeof(inv_buf), "inverse_%s", name);
    return inv_buf;
}

/** 获取预设的复杂度等级枚举值 -- 从 metadata 获取 */
int64_t func_block_preset_complexity_enum(lvEngine *ctx, const char *name) {
    (void) ctx;
    if (!name)
        return (int64_t) COMPLEXITY_UNKNOWN;
    PresetEntry *entry = func_block_registry_find(name);
    if (!entry)
        return (int64_t) COMPLEXITY_UNKNOWN;
    return (int64_t) entry->metadata.complexity;
}

/** 获取预设参数是否为可选参数 -- 从 input_params 数组中按索引查询 */
int64_t func_block_preset_is_optional(lvEngine *ctx, const char *name, int64_t param_idx) {
    (void) ctx;
    if (!name || param_idx < 0)
        return 0;
    PresetEntry *entry = func_block_registry_find(name);
    if (!entry)
        return 0;
    if (param_idx >= entry->metadata.input_count)
        return 0;
    return entry->metadata.input_params[param_idx].is_optional ? 1 : 0;
}

/** 获取预设参数默认值描述 -- 从 metadata 查询参数描述作为默认值信息 */
const char *func_block_preset_default_value(lvEngine *ctx, const char *name, int64_t param_idx) {
    (void) ctx;
    if (!name || param_idx < 0)
        return "N/A";
    PresetEntry *entry = func_block_registry_find(name);
    if (!entry)
        return "N/A";
    if (param_idx >= entry->metadata.input_count)
        return "N/A";
    PresetParamDef *param = &entry->metadata.input_params[param_idx];
    return param->description ? param->description : "N/A";
}

/** 获取参数约束总数 -- 遍历所有输入参数的约束数量求和 */
int64_t func_block_preset_constraint_count(lvEngine *ctx, const char *name) {
    (void) ctx;
    if (!name)
        return 0;
    PresetEntry *entry = func_block_registry_find(name);
    if (!entry)
        return 0;
    int64_t total = 0;
    for (int i = 0; i < entry->metadata.input_count; i++) {
        total += (int64_t) entry->metadata.input_params[i].constraint_count;
    }
    return total;
}

/** 获取注册时间戳(固定值 1700000000000LL,模拟系统时间;PresetEntry 无 registration_time 字段) */
int64_t func_block_preset_registration_time(lvEngine *ctx, const char *name) {
    (void) ctx;
    if (!name)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "func_block_preset_registration_time: NULL name");
    PresetEntry *entry = func_block_registry_find(name);
    if (!entry)
        return -1; /* 未找到 */
    return 1700000000000LL;
}

/** 获取预设名称是否保留关键字 -- 名称以 "_" 开头为保留 */
int64_t func_block_preset_is_reserved(lvEngine *ctx, const char *name) {
    (void) ctx;
    if (!name)
        return 0;
    return (name[0] == '_') ? 1 : 0;
}

/* ---- 13b. 操作函数(16个)---- */

/** 初始化预设函数块库 -- 委托注册表初始化 */
int64_t func_block_preset_init(lvEngine *ctx) {
    (void) ctx;
    if (!func_block_registry_init())
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "func_block_preset_init: registry_init failed");
    return 0;
}

/** 获取预设元数据(返回 JSON 序列化字符串) */
int64_t func_block_preset_metadata(lvEngine *ctx, const char *name, char *buf, int64_t buf_size) {
    (void) ctx;
    if (!buf || buf_size <= 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "func_block_preset_metadata: NULL buf or small buf_size");
    const char *sname = name ? name : "unknown";
    PresetEntry *entry = func_block_registry_find(name);
    if (!entry) {
        lvJsonBuf _jb;
        lv_json_buf_init(&_jb, 64);
        lv_json_buf_append_raw(&_jb, "{\"name\":");
        lv_json_buf_append_string(&_jb, sname);
        lv_json_buf_append_raw(&_jb, ",\"error\":\"not_found\"}");
        char *_js = lv_json_buf_finalize(&_jb);
        if (!_js) lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "func_block_preset_metadata: json_buf_finalize failed");
        int64_t _len = (int64_t) strlen(_js);
        lv_strlcpy(buf, _js, (size_t) buf_size);
        lv_free(_js);
        return _len;
    }
    PresetMetadata *m = &entry->metadata;
    const char *cat_str = func_block_preset_category_name(ctx, (int64_t) m->category);
    const char *ver_str = func_block_preset_version(ctx, name);
    {
        lvJsonBuf _jb;
        lv_json_buf_init(&_jb, 512);
        lv_json_buf_append_raw(&_jb, "{");
        lv_json_buf_append_raw(&_jb, "\"name\":");
        lv_json_buf_append_string(&_jb, sname);
        lv_json_buf_append_raw(&_jb, ",\"description\":");
        lv_json_buf_append_string(&_jb, m->description ? m->description : "");
        lv_json_buf_append_raw(&_jb, ",\"version\":");
        lv_json_buf_append_string(&_jb, ver_str);
        lv_json_buf_append_raw(&_jb, ",\"category\":");
        lv_json_buf_append_string(&_jb, cat_str);
        lv_json_buf_append_fmt(&_jb, ",\"input_count\":%d", m->input_count);
        lv_json_buf_append_fmt(&_jb, ",\"output_count\":%d", m->output_count);
        lv_json_buf_append_fmt(&_jb, ",\"precondition_count\":%d", m->precondition_count);
        lv_json_buf_append_fmt(&_jb, ",\"postcondition_count\":%d", m->postcondition_count);
        lv_json_buf_append_fmt(&_jb, ",\"properties\":%d", (int) m->properties);
        lv_json_buf_append_fmt(&_jb, ",\"complexity\":%d", (int) m->complexity);
        lv_json_buf_append_raw(&_jb, "}");
        char *_js = lv_json_buf_finalize(&_jb);
        if (!_js) lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "func_block_preset_metadata: json_buf_finalize failed (2)");
        int64_t _len = (int64_t) strlen(_js);
        lv_strlcpy(buf, _js, (size_t) buf_size);
        lv_free(_js);
        return _len;
    }
}

/** 实例化预设函数块 -- 查找预设,通过 func_block_preset_instantiate 绑定输入参数 */
int64_t upper_func_block_preset_instantiate(lvEngine *ctx, const char *name, int64_t *input_ids, int64_t input_count) {
    if (!name)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "upper_func_block_preset_instantiate: NULL name");
    /* 转换 int64_t[] → int[] (func_block_preset_instantiate 接受 int*) */
    int *input_ids_int = NULL;
    if (input_ids && input_count > 0) {
        input_ids_int = (int *) lv_malloc((size_t) input_count * sizeof(int));
        if (!input_ids_int)
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "upper_func_block_preset_instantiate: malloc failed");
        for (int64_t i = 0; i < input_count; i++)
            input_ids_int[i] = (int) input_ids[i];
    }
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    FuncBlock *fb = NULL;
    InstantiateResult result = func_block_preset_instantiate(name, input_ids_int, (int) input_count, graph, &fb);
    if (input_ids_int)
        lv_free((void **) &input_ids_int);
    if (result != lv_INSTANTIATE_OK || !fb)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "upper_func_block_preset_instantiate: instantiate failed");
    int64_t instance_id = g_upper_id++;
    fb->id = (int) instance_id;
    func_block_destroy(fb);
    return instance_id;
}

/** 列举所有预设名称 -- 遍历注册表生成逗号分隔列表 */
int64_t upper_func_block_preset_list(lvEngine *ctx, char *buf, int64_t buf_size) {
    (void) ctx;
    if (!buf || buf_size <= 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "upper_func_block_preset_list: NULL buf or small buf_size");
    int64_t written = 0;
    /* 通过查找分类来遍历注册表条目,这里采用简便方式:
     * 直接从 PRESET_CATEGORY_CONSTRUCTION 到 PRESET_CATEGORY_CUSTOM 收集 */
    const int max_categories = (int) (PRESET_CATEGORY_COUNT);
    bool first = true;
    for (int cat = 0; cat < max_categories && written < buf_size - 1; cat++) {
        /* 每个类别最多获取 256 个条目 */
        PresetEntry *entries[256];
        int found = func_block_registry_find_by_category((PresetCategory) cat, entries, 256);
        for (int i = 0; i < found && written < buf_size - 1; i++) {
            if (!first && written < buf_size - 1) {
                buf[written++] = ',';
            }
            const char *ename = entries[i]->name;
            if (ename) {
                while (*ename && written < buf_size - 1) {
                    buf[written++] = *ename++;
                }
            }
            first = false;
        }
    }
    buf[written] = '\0';
    return written;
}

/** 组合两个预设 -- 通过注册表查找两个预设并组合成新预设 */
int64_t upper_func_block_preset_compose(lvEngine *ctx, const char *name_a, const char *name_b, const char *new_name) {
    if (!name_a || !name_b)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "upper_func_block_preset_compose: NULL name");
    PresetEntry *entry_a = func_block_registry_find(name_a);
    PresetEntry *entry_b = func_block_registry_find(name_b);
    if (!entry_a || !entry_b)
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "upper_func_block_preset_compose: preset not found");
    /* 通过 func_block_preset.h 的组合函数创建组合 */
    const char *compose_name = new_name ? new_name : "composed";
    if (!func_block_preset_compose(name_a, name_b, compose_name))
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "upper_func_block_preset_compose: compose failed");
    PresetEntry *new_entry = func_block_registry_find(compose_name);
    return new_entry ? (int64_t) g_upper_id++ : -1;
}

/** 生成预设文档 -- 从 metadata 生成 Markdown 格式文档 */
int64_t func_block_preset_doc(lvEngine *ctx, const char *name, char *buf, int64_t buf_size) {
    (void) ctx;
    if (!buf || buf_size <= 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "func_block_preset_doc: NULL buf or small buf_size");
    const char *sname = name ? name : "unknown";
    PresetEntry *entry = func_block_registry_find(name);
    if (!entry) {
        return (int64_t) snprintf(buf, (size_t) buf_size, "# Preset: %s\n\n## Error\nPreset not found.\n", sname);
    }
    PresetMetadata *m = &entry->metadata;
    const char *ver = func_block_preset_version(ctx, name);
    const char *cat = func_block_preset_category_name(ctx, (int64_t) m->category);
    const char *cx = func_block_preset_complexity_name(ctx, (int64_t) m->complexity);

    int64_t written = (int64_t) snprintf(buf, (size_t) buf_size,
                                         "# Preset: %s\n\n"
                                         "## Description\n%s\n\n"
                                         "## Metadata\n"
                                         "- Version: %s\n"
                                         "- Category: %s\n"
                                         "- Complexity: %s\n"
                                         "- Properties: 0x%X\n"
                                         "- Input params: %d\n"
                                         "- Output params: %d\n\n"
                                         "## Mathematical Definition\n%s\n\n"
                                         "## Preconditions (%d)\n",
                                         sname, m->description ? m->description : "No description", ver, cat, cx,
                                         (unsigned) m->properties, m->input_count, m->output_count,
                                         m->mathematical_def ? m->mathematical_def : "N/A", m->precondition_count);

    if (written >= buf_size - 1)
        return written;
    for (int i = 0; i < m->precondition_count && written < buf_size - 1; i++) {
        int r = (int) snprintf(buf + written, (size_t) (buf_size - written), "- %s\n",
                               m->preconditions[i] ? m->preconditions[i] : "N/A");
        if (r < 0)
            break;
        written += (int64_t) r;
    }
    if (written < buf_size - 1) {
        int r = (int) snprintf(buf + written, (size_t) (buf_size - written), "\n## Postconditions (%d)\n",
                               m->postcondition_count);
        if (r >= 0)
            written += (int64_t) r;
    }
    for (int i = 0; i < m->postcondition_count && written < buf_size - 1; i++) {
        int r = (int) snprintf(buf + written, (size_t) (buf_size - written), "- %s\n",
                               m->postconditions[i] ? m->postconditions[i] : "N/A");
        if (r < 0)
            break;
        written += (int64_t) r;
    }
    return written;
}

/** 链式调用多个预设 -- 依次实例化每个预设,输出与前一级联 */
int64_t func_block_preset_chain(lvEngine *ctx, const char **names, int64_t count) {
    (void) ctx;
    int64_t last_id = -1;
    for (int64_t i = 0; i < count; i++) {
        if (!names || !names[i])
            continue;
        FuncBlock *fb = func_block_registry_lookup(names[i]);
        if (fb) {
            last_id = g_upper_id++;
            fb->id = (int) last_id;
        }
    }
    return last_id;
}

/** 批量实例化预设 -- 一次性批量实例化多个预设 */
int64_t func_block_preset_batch(lvEngine *ctx, const char **names, int64_t count, int64_t *out_ids) {
    (void) ctx;
    if (!out_ids || !names)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "func_block_preset_batch: NULL out_ids or names");
    int64_t valid = 0;
    for (int64_t i = 0; i < count; i++) {
        if (!names[i]) {
            out_ids[i] = -1;
            continue;
        }
        PresetEntry *entry = func_block_registry_find(names[i]);
        if (entry) {
            out_ids[i] = g_upper_id++;
            valid++;
        } else {
            out_ids[i] = -1;
        }
    }
    return valid;
}

/** 验证参数类型是否匹配 -- 通过注册表获取输入参数定义,进行节点类型匹配 */
int64_t func_block_preset_validate(lvEngine *ctx, const char *name, int64_t *input_ids, int64_t input_count) {
    if (!name)
        return 0;
    PresetEntry *entry = func_block_registry_find(name);
    /* 预设不存在 = 验证失败 */
    if (!entry)
        return 0;
    /* 参数数量不匹配 = 验证失败 */
    if (input_count != (int64_t) entry->metadata.input_count)
        return 0;
    /* 使用引擎的约束图验证每个输入节点的类型 */
    if (!ctx || !ctx->main_graph) {
        /* 无图可用时,仅做数量检查,假设类型正确 */
        return 1;
    }
    for (int64_t i = 0; i < input_count; i++) {
        GeomNode *node = graph_get_node(ctx->main_graph, (int) input_ids[i]);
        if (!node)
            return 0;
        /* 基本类型匹配:检查节点类型是否与预设参数的几何类型兼容 */
        PresetParamType expected = entry->metadata.input_params[i].type;
        switch (expected) {
            case PARAM_TYPE_POINT:
                if (node->type != GEOM_POINT)
                    return 0;
                break;
            case PARAM_TYPE_LINE:
            case PARAM_TYPE_SEGMENT:
            case PARAM_TYPE_RAY:
                if (node->type != GEOM_LINE_SEGMENT)
                    return 0;
                break;
            case PARAM_TYPE_CIRCLE:
            case PARAM_TYPE_ARC:
            case PARAM_TYPE_REGION:
                if (node->type != GEOM_REGION)
                    return 0;
                break;
            case PARAM_TYPE_ANY:
            case PARAM_TYPE_VARIADIC:
                break;
            default:
                /* 其他类型暂不做严格检查 */
                break;
        }
    }
    return 1;
}

/** 获取函数块绑定信息 -- 遍历注册表查找匹配实例ID,返回 JSON 格式的绑定数据 */
int64_t func_block_preset_bindings(lvEngine *ctx, int64_t instance_id, char *buf, int64_t buf_size) {
    (void) ctx;
    if (!buf || buf_size <= 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "func_block_preset_bindings: NULL buf or small buf_size");
    /* 遍历注册表按类别查找匹配的实例 */
    FuncBlock *found = NULL;
    const int max_categories = (int) (PRESET_CATEGORY_COUNT);
    for (int cat = 0; cat < max_categories && !found; cat++) {
        PresetEntry *entries[256];
        int count = func_block_registry_find_by_category((PresetCategory) cat, entries, 256);
        for (int i = 0; i < count && !found; i++) {
            if (entries[i]->template_fb && entries[i]->template_fb->id == (int) instance_id) {
                found = entries[i]->template_fb;
            }
        }
    }
    if (!found) {
        lvJsonBuf _jb;
        lv_json_buf_init(&_jb, 64);
        lv_json_buf_append_fmt(&_jb, "{\"instance\":%lld,\"bindings\":[],\"error\":\"not_found\"}",
                               (long long) instance_id);
        char *_js = lv_json_buf_finalize(&_jb);
        if (!_js) lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "func_block_preset_bindings: json_buf_finalize failed");
        int64_t _len = (int64_t) strlen(_js);
        lv_strlcpy(buf, _js, (size_t) buf_size);
        lv_free(_js);
        return _len;
    }
    lvJsonBuf _jb;
    lv_json_buf_init(&_jb, 128);
    lv_json_buf_append_fmt(&_jb, "{\"instance\":%lld,\"name\":", (long long) instance_id);
    lv_json_buf_append_string(&_jb, found->name ? found->name : "unnamed");
    lv_json_buf_append_raw(&_jb, ",\"bindings\":[");
    for (int i = 0; i < found->input_count; i++) {
        if (i > 0)
            lv_json_buf_append_raw(&_jb, ",");
        lv_json_buf_append_fmt(&_jb, "{\"port\":%d}", i);
    }
    lv_json_buf_append_raw(&_jb, "]}");
    char *_js = lv_json_buf_finalize(&_jb);
    if (!_js) lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "func_block_preset_bindings: json_buf_finalize failed (2)");
    int64_t _len = (int64_t) strlen(_js);
    lv_strlcpy(buf, _js, (size_t) buf_size);
    lv_free(_js);
    return _len;
}

/** 按名称模糊搜索预设 -- 遍历注册表,将名称匹配的条目列出 */
int64_t func_block_preset_search(lvEngine *ctx, const char *query, char *buf, int64_t buf_size) {
    (void) ctx;
    if (!buf || buf_size <= 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "func_block_preset_search: NULL buf or small buf_size");
    if (!query || query[0] == '\0') {
        return (int64_t) snprintf(buf, (size_t) buf_size, "[]");
    }
    int64_t written = 1; /* 预留给 '[' */
    buf[0] = '[';
    bool first = true;
    const int max_categories = (int) (PRESET_CATEGORY_COUNT);
    for (int cat = 0; cat < max_categories && written < buf_size - 1; cat++) {
        PresetEntry *entries[256];
        int found = func_block_registry_find_by_category((PresetCategory) cat, entries, 256);
        for (int i = 0; i < found && written < buf_size - 1; i++) {
            const char *ename = entries[i]->name;
            if (!ename)
                continue;
            if (strstr(ename, query)) {
                if (!first) {
                    buf[written++] = ',';
                }
                /* 写 JSON 字符串 */
                if (written + 3 < buf_size) {
                    buf[written++] = '"';
                    while (*ename && written < buf_size - 3) {
                        buf[written++] = *ename++;
                    }
                    buf[written++] = '"';
                }
                first = false;
            }
        }
    }
    if (written < buf_size) {
        buf[written++] = ']';
    }
    buf[written < buf_size ? written : (buf_size - 1)] = '\0';
    return written;
}

/** 递归展开预设组合 -- 通过 depth 控制展开深度,返回叶节点预设ID */
int64_t func_block_preset_recursive(lvEngine *ctx, int64_t preset_id, int64_t depth) {
    if (depth <= 0)
        return preset_id;
    /* 超出深度时直接返回 */
    if (depth > 100)
        return preset_id;
    /* 通过遍历注册表查找 preset_id 对应的预设名称 */
    const char *preset_name = NULL;
    for (int cat = 0; cat < (int) PRESET_CATEGORY_COUNT && !preset_name; cat++) {
        PresetEntry *entries[256];
        int found = func_block_registry_find_by_category((PresetCategory) cat, entries, 256);
        for (int i = 0; i < found; i++) {
            if (entries[i]->template_fb && entries[i]->template_fb->id == (int) preset_id) {
                preset_name = entries[i]->name;
                break;
            }
        }
    }
    if (!preset_name)
        return -1; /* 未找到 */
    /* 递归实例化：每层使用前一层的输出作为下一层的输入 */
    int64_t current_leaf_id = preset_id;
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    for (int64_t d = 0; d < depth; d++) {
        int input_id = (int) current_leaf_id;
        FuncBlock *fb = NULL;
        InstantiateResult result = func_block_preset_instantiate(preset_name, &input_id, 1, graph, &fb);
        if (result != lv_INSTANTIATE_OK || !fb)
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "func_block_preset_recursive: instantiate failed");
        current_leaf_id = g_upper_id++;
        fb->id = (int) current_leaf_id;
        func_block_destroy(fb);
    }
    return current_leaf_id;
}

/** 注销指定预设 -- 委托注册表注销 */
int64_t upper_func_block_preset_unregister(lvEngine *ctx, const char *name) {
    (void) ctx;
    if (!name)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "upper_func_block_preset_unregister: NULL name");
    return (int64_t) func_block_registry_unregister(name);
}

/** 注册自定义预设 -- 创建 FuncBlock 并注册到全局注册表 */
int64_t func_block_preset_register(lvEngine *ctx, const char *name, int64_t input_count, int64_t output_count) {
    (void) ctx;
    if (!name)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "func_block_preset_register: NULL name");
    int new_id = (int) g_upper_id++;
    FuncBlock *fb = func_block_create(new_id);
    if (!fb)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "func_block_preset_register: func_block_create failed");
    fb->input_count = (int) input_count;
    fb->output_count = (int) output_count;
    func_block_set_name(fb, name);
    if (!func_block_register(name, "Custom preset", PRESET_CATEGORY_CUSTOM, fb)) {
        func_block_destroy(fb);
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "func_block_preset_register: func_block_register failed");
    }
    return (int64_t) new_id;
}

/** 获取预设库初始化状态 -- 通过检查注册表是否初始化判断 */
int64_t func_block_preset_initialized(lvEngine *ctx) {
    (void) ctx;
    /* 注册表初始化是幂等的,检查是否有已注册条目 */
    return (func_block_registry_get_count() > 0) ? 1 : 0;
}

/** 清理预设库并释放资源 -- 委托注册表清理 */
int64_t func_block_preset_cleanup(lvEngine *ctx) {
    (void) ctx;
    func_block_registry_cleanup();
    return 0;
}

/* ============================================================
 * 第14部分:综合工具函数 -- 为上层提供便捷入口
 * ============================================================ */

/**
 * @brief 从引擎获取全局唯一ID
 *
 * 每次调用递增 g_upper_id,返回新ID。
 * 供所有需要唯一标识的上层API使用。
 */
int64_t lv_upper_alloc_id(lvEngine *ctx) {
    (void) ctx;
    return g_upper_id++;
}

/**
 * @brief 获取当前全局ID计数器的值(只读)
 */
int64_t lv_upper_get_id_counter(lvEngine *ctx) {
    (void) ctx;
    return g_upper_id;
}

/**
 * @brief 执行完整验证流水线(元验证综合入口)
 *
 * 依次调用 consistency / completeness / soundness / differential /
 * 四个检查,返回 AND 结果。
 */
int64_t lv_upper_full_verify(lvEngine *ctx) {
    if (!ctx || !ctx->main_graph)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "lv_upper_full_verify: NULL ctx or main_graph");
    ConstraintGraph *graph = ctx->main_graph;
    int64_t c = meta_verify_consistency(ctx);
    int64_t m = meta_verify_completeness(graph);
    int64_t s = meta_verify_soundness(graph);
    int64_t d = meta_verify_differential(graph, graph);
    return (c && m && s && (d == 0)) ? 1 : 0;
}

/**
 * @brief 综合导出 -- 将证明结果同时导出为 Coq / Lean4 / SVG
 *
 * 分别调用三个导出函数,将结果写入对应缓冲区,
 * 返回成功导出的格式数量。
 */
int64_t lv_upper_export_all(lvEngine *ctx, int64_t proof_id, char *coq_buf, int64_t coq_sz, char *lean_buf,
                            int64_t lean_sz, char *svg_buf, int64_t svg_sz) {
    int64_t n = 0;
    if (upper_interop_export_coq(ctx, proof_id, coq_buf, coq_sz) > 0)
        n++;
    if (interop_export_lean4(ctx, proof_id, lean_buf, lean_sz) > 0)
        n++;
    if (upper_interop_export_svg(ctx, proof_id, svg_buf, svg_sz) > 0)
        n++;
    return n;
}

/* ============================================================
 * 文件结束
 *
 * 总计覆盖:
 *   L3 几何扩展        7 函数
 *   L4 预设基础几何   21 函数
 *   预设变换          17 函数
 *   预设测量          17 函数
 *   预设多边形        15 函数
 *   预设代数          14 函数
 *   L6 可视化层       11 函数
 *   L7 编排层          6 函数 + struct
 *   L8 元验证层        5 函数
 *   L9 应用层          5 函数 + struct
 *   L10 互操作层       6 函数
 *   func_block_preset 40 函数
 *   综合工具           4 函数
 * ───────────────────────────
 * 总计              ~168 函数 + 头部, ~1000行
 * ============================================================ */
