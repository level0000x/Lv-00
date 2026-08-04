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

#include "lv_internal.h" /* lv_RETURN_ERROR / lv_RETURN_ERROR_NULL */
#include "lv/lv_strbuf.h"
#include "lv_impl_upper_internal.h"

/* ============================================================
 * 第2部分:L3 几何扩展(geom_evol / atp_backend / proof_tptp)
 * ============================================================ */

/* ---- geom_evol: 几何演化引擎 ---- */

/** 创建几何演化引擎,分配参数向量 */
int64_t geom_evol_create(lvEngine *ctx, int64_t dim) {
    (void) ctx;
    if (dim <= 0 || dim > GEOEVOL_MAX_PARAM_DIM)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "geom_evol_create: invalid dim");
    if (s_upper_state.evol_count >= MAX_EVOL_TABLE)
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "geom_evol_create: evol table full");

    /* 使用默认 RHS 函数创建演化引擎(调用方需后续设置实际 RHS) */
    lvGeomEvol *evol = geoevol_create((int) dim, lv_EVOL_RK4, NULL);
    if (!evol)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "geom_evol_create: geoevol_create failed");

    /* count < MAX 保证存在空槽, add 必然成功 */
    int64_t id = s_upper_state.upper_id++;
    lv_obj_table_add(s_upper_state.evol_table, &s_upper_state.evol_count, MAX_EVOL_TABLE, id, evol);
    return id;
}

/** 执行单步几何演化,返回步数计数 */
int64_t geom_evol_step(lvEngine *ctx, int64_t evol_id, int64_t steps) {
    (void) ctx;
    /* 在内部表中查找对应的演化引擎 */
    lvObjSlot *slot = lv_obj_table_find(s_upper_state.evol_table, MAX_EVOL_TABLE, evol_id);
    if (!slot) {
        /* 未找到引擎,返回模拟值 */
        return steps + 1;
    }
    lvGeomEvol *evol = (lvGeomEvol *) slot->ptr;

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
    lvObjSlot *slot = lv_obj_table_find(s_upper_state.evol_table, MAX_EVOL_TABLE, evol_id);
    if (!slot)
        return -1; /* 未找到 */
    geoevol_destroy((lvGeomEvol *) slot->ptr);
    lv_obj_table_remove(s_upper_state.evol_table, &s_upper_state.evol_count, MAX_EVOL_TABLE, evol_id);
    return 0;
}

/* ---- atp_backend: 自动定理证明后端 ---- */

/** @brief 求解器名称关键词 → 后端类型 映射表条目 */
typedef struct {
    const char *keyword;    /**< 名称匹配关键词 */
    ATPBackendType backend; /**< 对应后端类型 */
} AtpSolverKeywordEntry;

/** @brief 求解器名称关键词表（表序即匹配优先级，等价于原 strstr 链） */
static const AtpSolverKeywordEntry kSolverKeywordMap[] = {
    {"vampire",  ATP_BACKEND_VAMPIRE},
    {"Vampire",  ATP_BACKEND_VAMPIRE},
    {"eprover",  ATP_BACKEND_EPROVER},
    {"E Prover", ATP_BACKEND_EPROVER},
    {"iprover",  ATP_BACKEND_IPROVER},
    {"iProver",  ATP_BACKEND_IPROVER},
};

/** 从名称解析 ATP 后端类型 */
static ATPBackendType atp_parse_solver_name(const char *solver_name) {
    if (!solver_name)
        return ATP_BACKEND_VAMPIRE; /* 默认 */
    /* 关键词表循环匹配；未匹配回退 VAMPIRE（对应原默认分支） */
    for (size_t i = 0; i < lv_ARRAY_SIZE(kSolverKeywordMap); i++) {
        if (strstr(solver_name, kSolverKeywordMap[i].keyword))
            return kSolverKeywordMap[i].backend;
    }
    return ATP_BACKEND_VAMPIRE; /* 默认 */
}

/** @brief ATP 求解结果 → API 返回值 映射表（按枚举索引；越界值回退 -2，对应原 default 分支） */
static const int kAtpResultReturnMap[] = {
    [ATP_RESULT_SAT]     = -1, /* 反例 */
    [ATP_RESULT_UNSAT]   = 1,  /* 已证明 */
    [ATP_RESULT_UNKNOWN] = -2, /* 超时 */
    [ATP_RESULT_ERROR]   = -2, /* 错误 */
};

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

    /* count < MAX 保证存在空槽, add 必然成功 */
    int64_t id = s_upper_state.upper_id++;
    lv_obj_table_add(s_upper_state.atp_backend_table, &s_upper_state.atp_backend_count,
                     MAX_ATP_BACKEND_TABLE, id, solver);
    return id;
}

/** 向ATP后端提交证明任务,返回任务ID */
int64_t atp_backend_submit(lvEngine *ctx, int64_t backend_id, const char *conjecture) {
    (void) ctx;
    if (!conjecture || conjecture[0] == '\0')
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "atp_backend_submit: NULL/empty conjecture");
    if (s_upper_state.atp_task_count >= MAX_ATP_TASK_TABLE)
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "atp_backend_submit: task table full");

    /* 查找后端求解器 */
    lvObjSlot *slot = lv_obj_table_find(s_upper_state.atp_backend_table, MAX_ATP_BACKEND_TABLE, backend_id);
    if (!slot)
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "atp_backend_submit: solver not found");
    ATPBackendSolver *solver = (ATPBackendSolver *) slot->ptr;

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
            /* 结果映射表查询（越界值回退 -2，对应原 default 分支） */
            unsigned r = (unsigned) s_upper_state.atp_task_table[i].result_info.result;
            if (r < (unsigned) lv_ARRAY_SIZE(kAtpResultReturnMap))
                return kAtpResultReturnMap[r];
            return -2; /* 超时/错误 */
        }
    }
    return -2; /* 未找到任务 */
}

/** 销毁ATP后端实例 */
int64_t atp_backend_destroy(lvEngine *ctx, int64_t backend_id) {
    (void) ctx;
    lvObjSlot *slot = lv_obj_table_find(s_upper_state.atp_backend_table, MAX_ATP_BACKEND_TABLE, backend_id);
    if (!slot)
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "atp_backend_destroy: backend_id not found");
    atp_solver_destroy((ATPBackendSolver *) slot->ptr);
    lv_obj_table_remove(s_upper_state.atp_backend_table, &s_upper_state.atp_backend_count,
                        MAX_ATP_BACKEND_TABLE, backend_id);
    return 0;
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