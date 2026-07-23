/**
 * @file solver_feedback.c
 * @brief 求解器交互反馈（Solvespace 风格拖拽-实时反馈）
 *
 * @details 从 solver.c 拆分出的子模块（Lv-00 项目 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/solver.h"
#include "lv/stream.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "mpz_poly.h"
#include "stream_context_util.h"

/* --- 共享宏 --- */
#define lv_SOLVER_DYNARRAY_INIT_CAP 16
#define lv_SOLVER_LINEAR_COEFF_COUNT 2
#define lv_SOLVER_QUADRATIC_COEFF_COUNT 3
#define lv_ZERO_EPSILON 1e-12
#define SOLVER_DETAIL_BUF_SIZE 512
#define EQUATION_PUSH_OR_GOTO(sys, poly, vid, ci, label)               \
    do {                                                               \
        if (equation_system_push((sys), (poly), (vid), (ci)) != 0) {   \
            lv_set_error(lv_ERROR_OUT_OF_MEMORY, "push failed (OOM)"); \
            goto label;                                                \
        }                                                              \
    } while (0)

/* ── PolyEquation + EquationSystem ── */
typedef struct {
    mpz_poly_t poly;
    int var_node_id;
    int coord_index;
} PolyEquation;

typedef struct EquationSystem {
    PolyEquation *eqs;
    int count;
    int capacity;
} EquationSystem;

/* 前向声明：solver_coord_extract 和 solver_symbolic 模块中的函数 */
void equation_system_init(EquationSystem *sys);
int equation_system_push(EquationSystem *sys, mpz_poly_t poly, int var_node_id, int coord_index);
void equation_system_clear(EquationSystem *sys);
int count_degrees_of_freedom(const ConstraintGraph *graph, int **out_free_var_ids);
void groebner_result_destroy(GroebnerResult *result);
GroebnerResult *solver_incremental_solve(ConstraintGraph *graph, const int *dirty_var_ids, int n_dirty_vars);

lv_DECLARE_STREAM_CTX(solver);

/* ================================================================== */
/*  Solvespace 风格交互式求解反馈                                        */
/* ================================================================== */

/**
 * @brief 创建求解器反馈
 */
SolverFeedback *solver_feedback_create(SolverFeedbackType type, const char *message) {
    SolverFeedback *fb = lv_calloc(1, sizeof(SolverFeedback));
    if (!fb)
        return NULL;

    fb->type = type;
    fb->affected_var_id = -1;
    fb->degrees_of_freedom = -1;
    fb->free_var_count = 0;
    fb->overconstrained_count = 0;

    if (message && message[0] != '\0') {
        fb->message = lv_malloc(strlen(message) + 1);
        if (!fb->message) {
            lv_free((void **) &fb);
            return NULL;
        }
        /* [Bug修复] strcpy → lv_strlcpy 防止缓冲区溢出 */
        lv_strlcpy(fb->message, message, strlen(message) + 1);
    }

    return fb;
}

/**
 * @brief 销毁求解器反馈
 */
void solver_feedback_destroy(SolverFeedback *feedback) {
    if (!feedback)
        return;
    lv_free((void **) &feedback->message);
    lv_free((void **) &feedback->free_var_ids);
    lv_free((void **) &feedback->overconstrained_ids);
    lv_free((void **) &feedback);
}

/**
 * @brief 增量求解并返回交互反馈（Solvespace 风格拖拽-实时反馈）
 */
SolverFeedback *solver_feedback_solve(ConstraintGraph *graph, const int *dirty_vars, int dirty_count) {
    if (!graph)
        return NULL;

    SolverFeedback *fb = solver_feedback_create(SOLVER_FEEDBACK_TYPE_CONSTRAINT_ADDED,
                                                dirty_count > 0 ? "约束已添加，增量求解开始" : "执行全量求解");

    if (!fb)
        return NULL;

    /* Step 1: 执行增量求解 */
    GroebnerResult *result = solver_incremental_solve(graph, dirty_vars, dirty_count);

    if (!result) {
        fb->type = SOLVER_FEEDBACK_TYPE_CONFLICT_DETECTED;
        lv_free((void **) &fb->message);
        fb->message = lv_malloc(64);
        if (fb->message)
            /* [Bug修复] strcpy → lv_strlcpy 防止缓冲区溢出 */
            lv_strlcpy(fb->message, "求解失败：约束系统无解或超出范围", 64);
        return fb;
    }

    /* Step 2: 计算自由度 */
    int *free_var_ids = NULL;
    int dof = count_degrees_of_freedom(graph, &free_var_ids);
    fb->degrees_of_freedom = (dof >= 0) ? dof : -1;

    if (free_var_ids && dof > 0) {
        fb->free_var_ids = lv_calloc((size_t) dof, sizeof(int));
        if (fb->free_var_ids) {
            memcpy(fb->free_var_ids, free_var_ids, (size_t) dof * sizeof(int));
            fb->free_var_count = dof;
        }
        lv_free((void **) &free_var_ids);
    }

    /* Step 3: 判断反馈类型 */
    if (result->overdetermined) {
        fb->type = SOLVER_FEEDBACK_TYPE_OVERCONSTRAINED;
        lv_free((void **) &fb->message);
        fb->message = lv_malloc(64);
        if (fb->message)
            /* [Bug修复] strcpy → lv_strlcpy 防止缓冲区溢出 */
            lv_strlcpy(fb->message, "检测到过约束：某些变量被过多方程约束", 64);

        /* 标记过约束变量（简化处理：标记脏变量为过约束候选） */
        if (dirty_count > 0 && dirty_vars) {
            fb->overconstrained_ids = lv_calloc((size_t) dirty_count, sizeof(int));
            if (fb->overconstrained_ids) {
                memcpy(fb->overconstrained_ids, dirty_vars, (size_t) dirty_count * sizeof(int));
                fb->overconstrained_count = dirty_count;
            }
        }
    } else if (dof == 0) {
        fb->type = SOLVER_FEEDBACK_TYPE_VARIABLE_SOLVED;
        lv_free((void **) &fb->message);
        fb->message = lv_malloc(64);
        if (fb->message)
            /* [Bug修复] strcpy → lv_strlcpy 防止缓冲区溢出 */
            lv_strlcpy(fb->message, "所有变量已唯一确定（零自由度）", 64);
        if (dirty_count > 0 && dirty_vars) {
            fb->affected_var_id = dirty_vars[0];
        }
    } else if (dof > 0) {
        fb->type = SOLVER_FEEDBACK_TYPE_DOF_CHANGED;
        char buf[SOLVER_DETAIL_BUF_SIZE];
        snprintf(buf, sizeof(buf), "当前仍有 %d 个自由度", dof);
        lv_free((void **) &fb->message);
        fb->message = lv_malloc(strlen(buf) + 1);
        if (fb->message)
            /* [Bug修复] strcpy → lv_strlcpy 防止缓冲区溢出 */
            lv_strlcpy(fb->message, buf, strlen(buf) + 1);
        if (dirty_count > 0 && dirty_vars) {
            fb->affected_var_id = dirty_vars[0];
        }
    }

    /* 流式输出求解反馈 */
    if (solver_stream_ctx) {
        StreamEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = STREAM_EVENT_SOLVE_DONE;
        ev.timestamp_ms = stream_timestamp_ms();
        ev.var_id = fb->affected_var_id;
        ev.description = fb->message ? fb->message : "";
        char detail[SOLVER_DETAIL_BUF_SIZE];
        int _snw_fb;
        lv_SAFE_SNPRINTF(_snw_fb, detail, sizeof(detail),
                         "{\"type\":\"%s\",\"dof\":%d,\"free_count\":%d,\"overconstrained\":%d}",
                         (fb->type == SOLVER_FEEDBACK_TYPE_VARIABLE_SOLVED)     ? "solved"
                         : (fb->type == SOLVER_FEEDBACK_TYPE_OVERCONSTRAINED)   ? "overconstrained"
                         : (fb->type == SOLVER_FEEDBACK_TYPE_DOF_CHANGED)       ? "dof_changed"
                         : (fb->type == SOLVER_FEEDBACK_TYPE_CONFLICT_DETECTED) ? "conflict"
                                                                                : "constraint_added",
                         fb->degrees_of_freedom, fb->free_var_count, fb->overconstrained_count);
        lv_UNUSED(_snw_fb);
        ev.detail_json = detail;
        stream_emit(solver_stream_ctx, &ev);
    }

    groebner_result_destroy(result);
    return fb;
}
