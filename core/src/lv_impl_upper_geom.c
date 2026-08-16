/*
 * @file lv_impl_upper_geom.c
 * @brief Lv-00 upper unified impl - L3 geometry extensions
 * @details Split from lv_impl_upper.c
 */

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

#include "lv/lv_internal.h" /* lv_RETURN_ERROR / lv_RETURN_ERROR_NULL */
#include "lv/lv_strbuf.h"
#include "lv_impl_upper_internal.h"

/* ============================================================
 * 第2部分:L3 几何扩展(geom_evol / atp_backend / proof_tptp)
 * ============================================================ */

/* ---- geom_evol: 几何演化引擎 ---- */

/**
 * @brief 默认 RHS 函数（恒零导数）
 *
 * geoevol_create 拒绝 NULL RHS；上层接口未提供 RHS 时使用恒零默认，
 * 调用方后续可通过 geoevol API 或公开字段设置实际 RHS。
 */
static int geom_evol_default_rhs(double t, const double *param, double *dparam, lvGeomEvol *evol) {
    (void) t;
    (void) param;
    if (evol && dparam && evol->dim > 0) {
        memset(dparam, 0, (size_t) evol->dim * sizeof(double));
    }
    return 0;
}

/** 创建几何演化引擎,分配参数向量 */
int64_t geom_evol_create(lvEngine *ctx, int64_t dim) {
    (void) ctx;
    if (dim <= 0 || dim > GEOEVOL_MAX_PARAM_DIM)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "geom_evol_create: invalid dim");
    if (s_upper_state.evol_count >= MAX_EVOL_TABLE)
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "geom_evol_create: evol table full");

    /* 使用默认 RHS 函数创建演化引擎(调用方需后续设置实际 RHS) */
    lvGeomEvol *evol = geoevol_create((int) dim, lv_EVOL_RK4, geom_evol_default_rhs);
    if (!evol)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "geom_evol_create: geoevol_create failed");

    int64_t id = s_upper_state.upper_id++;
    int slot = 0;
    /* 查找空闲槽位 */
    for (; slot < MAX_EVOL_TABLE; slot++) {
        if (!s_upper_state.evol_table[slot].evol)
            break;
    }
    s_upper_state.evol_table[slot].evol = evol;
    s_upper_state.evol_table[slot].id = id;
    s_upper_state.evol_count++;
    return id;
}

/** 执行单步几何演化,返回步数计数 */
int64_t geom_evol_step(lvEngine *ctx, int64_t evol_id, int64_t steps) {
    (void) ctx;
    /* 在内部表中查找对应的演化引擎 */
    lvGeomEvol *evol = NULL;
    for (int i = 0; i < MAX_EVOL_TABLE; i++) {
        if (s_upper_state.evol_table[i].evol && s_upper_state.evol_table[i].id == evol_id) {
            evol = s_upper_state.evol_table[i].evol;
            break;
        }
    }
    if (!evol) {
        /* 未找到引擎：显式错误（原实现返回模拟值 steps+1，属静默降级，已修复） */
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "geom_evol_step: evol_id not found");
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
        if (s_upper_state.evol_table[i].evol && s_upper_state.evol_table[i].id == evol_id) {
            geoevol_destroy(s_upper_state.evol_table[i].evol);
            s_upper_state.evol_table[i].evol = NULL;
            s_upper_state.evol_table[i].id = 0;
            s_upper_state.evol_count--;
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
    if (s_upper_state.atp_backend_count >= MAX_ATP_BACKEND_TABLE)
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "atp_backend_create: ATP backend table full");

    ATPBackendType type = atp_parse_solver_name(solver_name);
    ATPConfig config = atp_config_default();
    ATPBackendSolver *solver = atp_solver_create(type, &config);
    if (!solver)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "atp_backend_create: solver create failed");

    /* 查找空闲槽位 */
    int slot = 0;
    for (; slot < MAX_ATP_BACKEND_TABLE; slot++) {
        if (!s_upper_state.atp_backend_table[slot].solver)
            break;
    }
    if (slot >= MAX_ATP_BACKEND_TABLE) {
        atp_solver_destroy(solver);
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "atp_backend_create: no free slot");
    }
    s_upper_state.atp_backend_table[slot].solver = solver;
    s_upper_state.atp_backend_table[slot].id = s_upper_state.upper_id++;
    s_upper_state.atp_backend_count++;
    return s_upper_state.atp_backend_table[slot].id;
}

/** 向ATP后端提交证明任务,返回任务ID */
int64_t atp_backend_submit(lvEngine *ctx, int64_t backend_id, const char *conjecture) {
    (void) ctx;
    if (!conjecture || conjecture[0] == '\0')
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "atp_backend_submit: NULL/empty conjecture");
    if (s_upper_state.atp_task_count >= MAX_ATP_TASK_TABLE)
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "atp_backend_submit: task table full");

    /* 查找后端求解器 */
    ATPBackendSolver *solver = NULL;
    for (int i = 0; i < MAX_ATP_BACKEND_TABLE; i++) {
        if (s_upper_state.atp_backend_table[i].solver && s_upper_state.atp_backend_table[i].id == backend_id) {
            solver = s_upper_state.atp_backend_table[i].solver;
            break;
        }
    }
    if (!solver)
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "atp_backend_submit: solver not found");

    /* 创建任务 */
    int task_slot = s_upper_state.atp_task_count++;
    int64_t task_id = s_upper_state.upper_id++;
    s_upper_state.atp_task_table[task_slot].task_id = task_id;
    s_upper_state.atp_task_table[task_slot].backend_id = backend_id;
    s_upper_state.atp_task_table[task_slot].completed = 0;

    /* 初始化结果 */
    atp_result_init(&s_upper_state.atp_task_table[task_slot].result_info);

    /* 加载并求解 */
    int load_ret = atp_solver_load(solver, conjecture);
    if (load_ret == 0) {
        atp_solver_solve(solver, &s_upper_state.atp_task_table[task_slot].result_info);
    }
    s_upper_state.atp_task_table[task_slot].completed = 1;
    return task_id;
}

/** 获取ATP任务结果:0=待处理, 1=已证明, -1=反例, -2=超时 */
int64_t atp_backend_result(lvEngine *ctx, int64_t task_id) {
    (void) ctx;
    for (int i = 0; i < s_upper_state.atp_task_count; i++) {
        if (s_upper_state.atp_task_table[i].task_id == task_id) {
            if (!s_upper_state.atp_task_table[i].completed)
                return 0; /* 待处理 */
            switch (s_upper_state.atp_task_table[i].result_info.result) {
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
        if (s_upper_state.atp_backend_table[i].solver && s_upper_state.atp_backend_table[i].id == backend_id) {
            atp_solver_destroy(s_upper_state.atp_backend_table[i].solver);
            s_upper_state.atp_backend_table[i].solver = NULL;
            s_upper_state.atp_backend_table[i].id = 0;
            s_upper_state.atp_backend_count--;
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
    int64_t report_id = s_upper_state.upper_id++;

    /* 将验证结果存入任务表作为记录 */
    if (s_upper_state.atp_task_count < MAX_ATP_TASK_TABLE) {
        int slot = s_upper_state.atp_task_count++;
        s_upper_state.atp_task_table[slot].task_id = report_id;
        s_upper_state.atp_task_table[slot].backend_id = -1;
        s_upper_state.atp_task_table[slot].result_info = result;
        s_upper_state.atp_task_table[slot].completed = 1;
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
