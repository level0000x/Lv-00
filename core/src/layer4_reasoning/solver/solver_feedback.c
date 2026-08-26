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
 * @brief 识别真实过约束变量（完整实现，替代旧"标记脏变量"简化）
 * @details 与 count_degrees_of_freedom 同款方程分布语义：每个点的坐标
 *          自由度为 2；参与约束的权重（constraint_weight）按参与者均分
 *          到各点。某点累计方程数 > 2 即判定为过约束候选（被过多方程
 *          同时约束）。线段端点经 INCIDENCE 约束补记 1 个方程。
 * @return 过约束变量 ID 数组（lv_malloc，调用者释放），数量经 out_count
 */
static int *collect_overconstrained_ids(const ConstraintGraph *graph, int *out_count) {
    if (!graph || !out_count) {
        if (out_count)
            *out_count = 0;
        return NULL;
    }
    *out_count = 0;

    int *pt_ids = NULL;
    int pt_count = count_point_variables(graph, &pt_ids);
    if (pt_count <= 0) {
        if (pt_ids)
            lv_free((void **) &pt_ids);
        return NULL;
    }

    int *eq_per_point = lv_calloc((size_t) pt_count, sizeof(int));
    if (!eq_per_point) {
        lv_free((void **) &pt_ids);
        return NULL;
    }

    /* 约束权重分布到参与点（与 count_degrees_of_freedom 语义一致） */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (!c)
            continue;
        int weight = constraint_weight(c);
        int point_participants = 0;
        for (int j = 0; j < c->participant_count; j++) {
            GeomNode *n = graph_get_node(graph, c->participants[j]);
            if (n && n->type == GEOM_POINT) {
                for (int k = 0; k < pt_count; k++) {
                    if (pt_ids[k] == n->id) {
                        point_participants++;
                        break;
                    }
                }
            }
        }
        if (point_participants > 0) {
            int per_point = weight / point_participants;
            int remainder = weight % point_participants;
            for (int j = 0; j < c->participant_count; j++) {
                GeomNode *n = graph_get_node(graph, c->participants[j]);
                if (n && n->type == GEOM_POINT) {
                    for (int k = 0; k < pt_count; k++) {
                        if (pt_ids[k] == n->id) {
                            eq_per_point[k] += per_point;
                            if (remainder > 0) {
                                eq_per_point[k]++;
                                remainder--;
                            }
                            break;
                        }
                    }
                }
            }
        }
    }

    /* 线段端点：每个端点经 INCIDENCE 约束补记 1 个方程（与 dof 计算一致） */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *n = graph->nodes[i];
        if (!n || n->type != GEOM_LINE_SEGMENT)
            continue;
        for (int j = 0; j < graph->constraint_count; j++) {
            Constraint *c = graph->constraints[j];
            if (!c || c->type != INCIDENCE)
                continue;
            for (int p = 0; p < c->participant_count; p++) {
                if (c->participants[p] == n->id) {
                    int other = c->participants[1 - p];
                    for (int k = 0; k < pt_count; k++) {
                        if (pt_ids[k] == other)
                            eq_per_point[k]++;
                    }
                }
            }
        }
    }

    /* 收集方程数 > 2（坐标自由度）的点为过约束候选 */
    int cap = pt_count;
    int *ids = lv_calloc((size_t) cap, sizeof(int));
    int count = 0;
    if (ids) {
        for (int k = 0; k < pt_count; k++) {
            if (eq_per_point[k] > 2)
                ids[count++] = pt_ids[k];
        }
    }

    lv_free((void **) &eq_per_point);
    lv_free((void **) &pt_ids);
    *out_count = count;
    return ids;
}

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

        /* 识别真实过约束变量（方程数 > 坐标自由度 2 的点），
         * 替代旧实现"标记脏变量为过约束候选"的近似 */
        int oc_count = 0;
        int *oc_ids = collect_overconstrained_ids(graph, &oc_count);
        if (oc_ids && oc_count > 0) {
            fb->overconstrained_ids = oc_ids;
            fb->overconstrained_count = oc_count;
        } else {
            if (oc_ids)
                lv_free((void **) &oc_ids);
            /* 无精确过约束点识别结果时回退脏变量候选（保持反馈可用） */
            if (dirty_count > 0 && dirty_vars) {
                fb->overconstrained_ids = lv_calloc((size_t) dirty_count, sizeof(int));
                if (fb->overconstrained_ids) {
                    memcpy(fb->overconstrained_ids, dirty_vars, (size_t) dirty_count * sizeof(int));
                    fb->overconstrained_count = dirty_count;
                }
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
