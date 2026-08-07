/**
 * @file solver_feedback.c
 * @brief 求解器交互反馈（Solvespace 风格拖拽-实时反馈）
 *
 * @details 从 solver.c 拆分出的子模块（Lv-00 项目 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "solver_common.h"

int count_degrees_of_freedom(const ConstraintGraph *graph, int **out_free_var_ids);
void groebner_result_destroy(GroebnerResult *result);
GroebnerResult *solver_incremental_solve(ConstraintGraph *graph, const int *dirty_var_ids, int n_dirty_vars);



/* ================================================================== */
/*  Solvespace 风格交互式求解反馈                                        */
/* ================================================================== */

/**
 * @brief 创建求解器反馈
 */
SolverFeedback *solver_feedback_create(SolverFeedbackType type, const char *message) {
    SolverFeedback *fb = lv_calloc(1, sizeof(SolverFeedback));
    if (!fb)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "solver_feedback_create: lv_calloc for SolverFeedback failed");

    fb->type = type;
    fb->affected_var_id = -1;
    fb->degrees_of_freedom = -1;
    fb->free_var_count = 0;
    fb->overconstrained_count = 0;

    if (message && message[0] != '\0') {
        /* 手写 malloc+lv_strlcpy 收敛为 lv_strdup */
        fb->message = lv_strdup(message);
        if (!fb->message) {
            lv_free((void **) &fb);
            lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "solver_feedback_create: lv_malloc for message failed");
        }
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
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "solver_feedback_solve: graph is NULL");

    SolverFeedback *fb = solver_feedback_create(SOLVER_FEEDBACK_TYPE_CONSTRAINT_ADDED,
                                                dirty_count > 0 ? "约束已添加，增量求解开始" : "执行全量求解");

    if (!fb)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "solver_feedback_solve: solver_feedback_create failed");

    /* Step 1: 执行增量求解 */
    GroebnerResult *result = solver_incremental_solve(graph, dirty_vars, dirty_count);

    if (!result) {
        fb->type = SOLVER_FEEDBACK_TYPE_CONFLICT_DETECTED;
        lv_free((void **) &fb->message);
        fb->message = lv_strdup("求解失败：约束系统无解或超出范围");
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
        fb->message = lv_strdup("检测到过约束：某些变量被过多方程约束");

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
        fb->message = lv_strdup("所有变量已唯一确定（零自由度）");
        if (dirty_count > 0 && dirty_vars) {
            fb->affected_var_id = dirty_vars[0];
        }
    } else if (dof > 0) {
        fb->type = SOLVER_FEEDBACK_TYPE_DOF_CHANGED;
        /* 手写 snprintf 到栈缓冲 + malloc + strlcpy 收敛为 lv_asprintf */
        lv_free((void **) &fb->message);
        fb->message = lv_asprintf("当前仍有 %d 个自由度", dof);
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
        char detail[lv_SOLVER_DETAIL_BUF_SIZE];
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
