/**
 * @file geo_constraint_solver_residual.c
 * @brief 几何约束求解器 —— 约束残差计算（核心）
 */

#include "geo_constraint_solver_internal.h"
#include "lv/geo_utils.h" /* geo_norm_2d（2D 向量模长统一工具） */
#include "lv/lv_numeric.h" /* lv_angle_diff_pi（角度差回绕到 [-π,π]） */

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * 第七部分：约束残差计算（核心）
 * ======================================================================== */

/* --- 约束求值：函数指针表 --- */
typedef double (*ConstraintEvalFunc)(const lvSolverSystem *sys, const lvConstraint *c, double *error_val);

static double eval_points_coincident(const lvSolverSystem *sys, const lvConstraint *c, double *error_val) {
    lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
    lvEntity *eb = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_b);
    if (!ea || !eb) {
        if (error_val) *error_val = 0.0;
        return 2;
    }
    double dx = ea->params[0] - eb->params[0];
    double dy = ea->params[1] - eb->params[1];
    double err = dx * dx + dy * dy;
    if (error_val) *error_val = err;
    return 2;
}

static double eval_pt_pt_distance(const lvSolverSystem *sys, const lvConstraint *c, double *error_val) {
    lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
    lvEntity *eb = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_b);
    if (!ea || !eb) {
        if (error_val) *error_val = 0.0;
        return 1;
    }
    double dx = ea->params[0] - eb->params[0];
    double dy = ea->params[1] - eb->params[1];
    double dist = geo_norm_2d(dx, dy);
    double err = dist - c->value;
    if (error_val) *error_val = err;
    return 1;
}

static double eval_pt_on_line(const lvSolverSystem *sys, const lvConstraint *c, double *error_val) {
    lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
    lvEntity *eb = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_b);
    if (!ea || !eb) {
        if (error_val) *error_val = 0.0;
        return 1;
    }
    double px = ea->params[0], py = ea->params[1];
    double ax = eb->params[0], ay = eb->params[1];
    double bx = eb->params[2], by = eb->params[3];
    double ldx = bx - ax, ldy = by - ay;
    double len = geo_norm_2d(ldx, ldy);
    if (len < lv_EPSILON_SUPERTINY) {
        if (error_val) *error_val = 0.0;
        return 1;
    }
    double err = ((px - ax) * ldy - (py - ay) * ldx) / len;
    if (error_val) *error_val = err;
    return 1;
}

static double eval_pt_line_distance(const lvSolverSystem *sys, const lvConstraint *c, double *error_val) {
    lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
    lvEntity *eb = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_b);
    if (!ea || !eb) {
        if (error_val) *error_val = 0.0;
        return 1;
    }
    double px = ea->params[0], py = ea->params[1];
    double ax = eb->params[0], ay = eb->params[1];
    double bx = eb->params[2], by = eb->params[3];
    double ldx = bx - ax, ldy = by - ay;
    double len = geo_norm_2d(ldx, ldy);
    if (len < lv_EPSILON_SUPERTINY) {
        if (error_val) *error_val = 0.0;
        return 1;
    }
    double dist = fabs(((px - ax) * ldy - (py - ay) * ldx) / len);
    double err = dist - c->value;
    if (error_val) *error_val = err;
    return 1;
}

static double eval_pt_on_segment(const lvSolverSystem *sys, const lvConstraint *c, double *error_val) {
    lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
    lvEntity *eb = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_b);
    if (!ea || !eb) {
        if (error_val) *error_val = 0.0;
        return 1;
    }
    double px = ea->params[0], py = ea->params[1];
    double x1 = eb->params[0], y1 = eb->params[1];
    double x2 = eb->params[2], y2 = eb->params[3];
    double sdx = x2 - x1, sdy = y2 - y1;
    double slen = geo_norm_2d(sdx, sdy);
    if (slen < lv_EPSILON_SUPERTINY) {
        if (error_val) *error_val = 0.0;
        return 1;
    }
    double err = fabs(((px - x1) * sdy - (py - y1) * sdx) / slen);
    if (error_val) *error_val = err;
    return 1;
}

static double eval_pt_on_circle(const lvSolverSystem *sys, const lvConstraint *c, double *error_val) {
    lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
    lvEntity *eb = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_b);
    if (!ea || !eb) {
        if (error_val) *error_val = 0.0;
        return 1;
    }
    double dx = ea->params[0] - eb->params[0];
    double dy = ea->params[1] - eb->params[1];
    double dist = geo_norm_2d(dx, dy);
    double err = dist - eb->params[2];
    if (error_val) *error_val = err;
    return 1;
}

static double eval_pt_pt_midpoint(const lvSolverSystem *sys, const lvConstraint *c, double *error_val) {
    lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
    lvEntity *eb = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_b);
    lvEntity *ec = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_c);
    if (!ea || !eb || !ec) {
        if (error_val) *error_val = 0.0;
        return 2;
    }
    double mx = (ea->params[0] + eb->params[0]) * 0.5;
    double my = (ea->params[1] + eb->params[1]) * 0.5;
    double err = (ec->params[0] - mx) * (ec->params[0] - mx) + (ec->params[1] - my) * (ec->params[1] - my);
    if (error_val) *error_val = err;
    return 2;
}

static double eval_parallel(const lvSolverSystem *sys, const lvConstraint *c, double *error_val) {
    lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
    lvEntity *eb = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_b);
    if (!ea || !eb) {
        if (error_val) *error_val = 0.0;
        return 1;
    }
    double dax = ea->params[2] - ea->params[0];
    double day = ea->params[3] - ea->params[1];
    double dbx = eb->params[2] - eb->params[0];
    double dby = eb->params[3] - eb->params[1];
    double err = dax * dby - day * dbx;
    if (error_val) *error_val = err;
    return 1;
}

static double eval_perpendicular(const lvSolverSystem *sys, const lvConstraint *c, double *error_val) {
    lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
    lvEntity *eb = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_b);
    if (!ea || !eb) {
        if (error_val) *error_val = 0.0;
        return 1;
    }
    double dax = ea->params[2] - ea->params[0];
    double day = ea->params[3] - ea->params[1];
    double dbx = eb->params[2] - eb->params[0];
    double dby = eb->params[3] - eb->params[1];
    double err = dax * dbx + day * dby;
    if (error_val) *error_val = err;
    return 1;
}

static double eval_angle(const lvSolverSystem *sys, const lvConstraint *c, double *error_val) {
    lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
    lvEntity *eb = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_b);
    if (!ea || !eb) {
        if (error_val) *error_val = 0.0;
        return 1;
    }
    double dax = ea->params[2] - ea->params[0];
    double day = ea->params[3] - ea->params[1];
    double dbx = eb->params[2] - eb->params[0];
    double dby = eb->params[3] - eb->params[1];
    double angle_a = (dax != 0.0 || day != 0.0) ? atan2(day, dax) : 0.0;
    double angle_b = (dbx != 0.0 || dby != 0.0) ? atan2(dby, dbx) : 0.0;
    double diff = angle_a - angle_b;
    diff = lv_angle_diff_pi(diff);
    double err = diff - c->value;
    if (error_val) *error_val = err;
    return 1;
}

static double eval_equal_length(const lvSolverSystem *sys, const lvConstraint *c, double *error_val) {
    lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
    lvEntity *eb = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_b);
    if (!ea || !eb) {
        if (error_val) *error_val = 0.0;
        return 1;
    }
    double dax = ea->params[2] - ea->params[0];
    double day = ea->params[3] - ea->params[1];
    double dbx = eb->params[2] - eb->params[0];
    double dby = eb->params[3] - eb->params[1];
    double len_a = geo_norm_2d(dax, day);
    double len_b = geo_norm_2d(dbx, dby);
    double err = len_a - len_b;
    if (error_val) *error_val = err;
    return 1;
}

static double eval_equal_radius(const lvSolverSystem *sys, const lvConstraint *c, double *error_val) {
    lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
    lvEntity *eb = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_b);
    if (!ea || !eb) {
        if (error_val) *error_val = 0.0;
        return 1;
    }
    double err = ea->params[2] - eb->params[2];
    if (error_val) *error_val = err;
    return 1;
}

static double eval_concentric(const lvSolverSystem *sys, const lvConstraint *c, double *error_val) {
    lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
    lvEntity *eb = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_b);
    if (!ea || !eb) {
        if (error_val) *error_val = 0.0;
        return 2;
    }
    double dx = ea->params[0] - eb->params[0];
    double dy = ea->params[1] - eb->params[1];
    double err = dx * dx + dy * dy;
    if (error_val) *error_val = err;
    return 2;
}

static double eval_tangent(const lvSolverSystem *sys, const lvConstraint *c, double *error_val) {
    lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
    lvEntity *eb = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_b);
    if (!ea || !eb) {
        if (error_val) *error_val = 0.0;
        return 1;
    }
    double ax = ea->params[0], ay = ea->params[1];
    double bx = ea->params[2], by = ea->params[3];
    double cx = eb->params[0], cy = eb->params[1];
    double r = eb->params[2];
    double ldx = bx - ax, ldy = by - ay;
    double len = geo_norm_2d(ldx, ldy);
    if (len < lv_EPSILON_SUPERTINY) {
        if (error_val) *error_val = 0.0;
        return 1;
    }
    double dist = fabs(((cx - ax) * ldy - (cy - ay) * ldx) / len);
    double err = dist - r;
    if (error_val) *error_val = err;
    return 1;
}

static double eval_fixed(const lvSolverSystem *sys, const lvConstraint *c, double *error_val) {
    lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
    if (!ea) {
        if (error_val) *error_val = 0.0;
        return 0;
    }
    int dof = lv_entity_dof(ea->type);
    if (error_val) *error_val = 0.0;
    return (double)dof;
}

static double eval_horizontal(const lvSolverSystem *sys, const lvConstraint *c, double *error_val) {
    lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
    if (!ea) {
        if (error_val) *error_val = 0.0;
        return 1;
    }
    double err = ea->params[1] - ea->params[3];
    if (error_val) *error_val = err;
    return 1;
}

static double eval_vertical(const lvSolverSystem *sys, const lvConstraint *c, double *error_val) {
    lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
    if (!ea) {
        if (error_val) *error_val = 0.0;
        return 1;
    }
    double err = ea->params[0] - ea->params[2];
    if (error_val) *error_val = err;
    return 1;
}

static ConstraintEvalFunc s_constraint_eval_funcs[] = {
    [lv_CONSTRAINT_POINTS_COINCIDENT] = eval_points_coincident,
    [lv_CONSTRAINT_PT_PT_DISTANCE]    = eval_pt_pt_distance,
    [lv_CONSTRAINT_PT_ON_LINE]        = eval_pt_on_line,
    [lv_CONSTRAINT_PT_LINE_DISTANCE]  = eval_pt_line_distance,
    [lv_CONSTRAINT_PT_ON_SEGMENT]     = eval_pt_on_segment,
    [lv_CONSTRAINT_PT_ON_CIRCLE]      = eval_pt_on_circle,
    [lv_CONSTRAINT_PT_PT_MIDPOINT]    = eval_pt_pt_midpoint,
    [lv_CONSTRAINT_PARALLEL]          = eval_parallel,
    [lv_CONSTRAINT_PERPENDICULAR]     = eval_perpendicular,
    [lv_CONSTRAINT_ANGLE]             = eval_angle,
    [lv_CONSTRAINT_EQUAL_LENGTH]      = eval_equal_length,
    [lv_CONSTRAINT_EQUAL_RADIUS]      = eval_equal_radius,
    [lv_CONSTRAINT_CONCENTRIC]        = eval_concentric,
    [lv_CONSTRAINT_TANGENT]           = eval_tangent,
    [lv_CONSTRAINT_FIXED]             = eval_fixed,
    [lv_CONSTRAINT_HORIZONTAL]        = eval_horizontal,
    [lv_CONSTRAINT_VERTICAL]          = eval_vertical,
};
static const int s_constraint_eval_func_count = (int)(sizeof(s_constraint_eval_funcs) / sizeof(s_constraint_eval_funcs[0]));

/**
 * @brief 计算单个约束的残差
 *
 * 对每种约束类型计算其残差值。当所有约束残差为零时，系统满足所有约束。
 *
 * @param sys  求解系统
 * @param c    约束
 * @param error_val  输出残差值（NULL 表示不输出）
 * @return 残差数量（1 或 2）
 */
double evaluate_constraint(const lvSolverSystem *sys, const lvConstraint *c, double *error_val) {
    if (lv_index_in_range(c->type, s_constraint_eval_func_count) && s_constraint_eval_funcs[c->type]) {
        return s_constraint_eval_funcs[c->type](sys, c, error_val);
    }
    if (error_val)
        *error_val = 0.0;
    return 0;
}

