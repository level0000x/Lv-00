/**
 * @file geo_constraint_solver_newton.c
 * @brief 几何约束求解器 —— Newton-Raphson 求解核心
 */

#include "geo_constraint_solver_internal.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * 第十部分：Newton-Raphson 求解核心
 * ======================================================================== */

/**
 * @brief 计算残差向量（遍历所有活动约束，跳过 FIXED 约束）
 *
 * @param sys   求解系统
 * @param out   输出残差数组
 * @param nrows 残差数组容量
 * @return 填充的行数
 */
static int compute_residuals(const lvSolverSystem *sys, double *out, int nrows) {
    int row = 0;

    for (int ci = 0; ci < sys->constraint_count; ci++) {
        const lvConstraint *c = &sys->constraints[ci];
        if (!c->is_active)
            continue;
        if (c->type == lv_CONSTRAINT_FIXED)
            continue;

        int n_eq = evaluate_constraint(sys, c, NULL);

        if (n_eq == 2) {
            lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
            lvEntity *eb = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_b);
            if (ea && eb) {
                out[row] = ea->params[0] - eb->params[0];
                row++;
                out[row] = ea->params[1] - eb->params[1];
                row++;
            } else {
                out[row] = 0.0;
                row++;
                if (row < nrows) {
                    out[row] = 0.0;
                    row++;
                }
            }
        } else {
            double err = 0.0;
            evaluate_constraint(sys, c, &err);
            out[row] = err;
            row++;
        }
    }

    return row;
}

/**
 * @brief 构建雅可比矩阵 J 和残差向量 F
 *
 * 使用数值差分（中心差分）计算雅可比矩阵的每个元素：
 *   J[i][j] = (F_i(params + eps*e_j) - F_i(params - eps*e_j)) / (2 * eps)
 *
 * @param sys           求解系统
 * @param J             输出雅可比矩阵（nrows x ncols，行优先）
 * @param F             输出残差向量（nrows）
 * @param nrows         残差维度（约束方程数量）
 * @param ncols         参数维度（自由参数数量）
 * @param param_map     参数映射表：param_map[j] = 全局参数索引
 * @param free_entities 自由实体索引数组
 * @param free_count    自由实体数量
 */
static void build_jacobian_and_residual(const lvSolverSystem *sys, double *J, double *F, int nrows, int ncols,
                                        const int *param_map, const int *free_entities, int free_count) {
    /* 计算残差向量 F */
    compute_residuals(sys, F, nrows);

    /* 使用数值差分计算雅可比矩阵 */
    double eps = NUMERICAL_DIFF_EPSILON;

    for (int j = 0; j < ncols; j++) {
        /* 找到 param_map[j] 对应的实体和参数偏移 */
        int global_param = param_map[j];
        int ent_idx = -1;
        int param_offset = 0;
        int accumulated = 0;

        for (int fi = 0; fi < free_count; fi++) {
            int ei = free_entities[fi];
            const lvEntity *e = &sys->entities[ei];
            int dof = e->param_count;
            if (global_param >= accumulated && global_param < accumulated + dof) {
                ent_idx = ei;
                param_offset = global_param - accumulated;
                break;
            }
            accumulated += dof;
        }

        if (ent_idx < 0)
            continue;

        /* 正向扰动 */
        double orig = sys->entities[ent_idx].params[param_offset];

        /* 自适应扰动步长：固定步长 1e-8 在参数值较大时会被舍入（10000 + 1e-8 == 10000），
         * 导致雅可比列为零。使用相对步长 eps * max(1, |orig|) 保证扰动显著。 */
        double h = eps * fmax(1.0, fabs(orig));
        sys->entities[ent_idx].params[param_offset] = orig + h;

        double *F_plus = (double *) lv_calloc(nrows, sizeof(double));
        compute_residuals(sys, F_plus, nrows);

        /* 负向扰动 */
        sys->entities[ent_idx].params[param_offset] = orig - h;

        double *F_minus = (double *) lv_calloc(nrows, sizeof(double));
        compute_residuals(sys, F_minus, nrows);

        /* 恢复原始值 */
        sys->entities[ent_idx].params[param_offset] = orig;

        /* 计算雅可比列：J[:, j] = (F_plus - F_minus) / (2 * h) */
        for (int i = 0; i < nrows; i++) {
            J[i * ncols + j] = (F_plus[i] - F_minus[i]) / (2.0 * h);
        }

        lv_free((void **) &(F_plus));
        lv_free((void **) &(F_minus));
    }
}

/**
 * @brief 执行 Newton-Raphson 求解
 *
 * 算法流程（借鉴 SolveSpace）：
 *   1. 确定自由参数（排除固定和被拖拽的实体）
 *   2. 构建雅可比矩阵 J 和残差向量 F
 *   3. 求解 J * delta = -F
 *   4. 更新参数：params += damping * delta
 *   5. 检查收敛：||F|| < tolerance
 *   6. 重复直到收敛或达到最大迭代次数
 *
 * @return 求解结果
 */
lv_PUBLIC_API lvSolveResult lv_geo_solver_solve(lvSolverSystem *sys) {
    if (!sys)
        return lv_SOLVE_FAILED;

    sys->iteration_count = 0;

    /* 统计自由实体和参数维度 */
    int free_entities[MAX_PARAMS];
    int free_count = 0;
    int total_params = 0;

    for (int i = 0; i < sys->entity_count; i++) {
        if (sys->entities[i].is_fixed || sys->entities[i].is_dragged)
            continue;
        if (free_count < MAX_PARAMS) {
            free_entities[free_count] = i;
            free_count++;
            total_params += sys->entities[i].param_count;
        }
    }

    if (total_params == 0) {
        sys->last_result = lv_SOLVE_OK;
        return lv_SOLVE_OK;
    }

    /* 统计约束方程数量（跳过 FIXED 约束，与 compute_residuals 保持一致：
     * FIXED 的 DOF 行数若计入 nrows 但残差/雅可比无对应行，会使雅可比出现全零行、JtJ 奇异） */
    int nrows = 0;
    for (int ci = 0; ci < sys->constraint_count; ci++) {
        const lvConstraint *c = &sys->constraints[ci];
        if (!c->is_active)
            continue;
        if (c->type == lv_CONSTRAINT_FIXED)
            continue;
        nrows += evaluate_constraint(sys, c, NULL);
    }

    if (nrows == 0) {
        sys->last_result = lv_SOLVE_OK;
        return lv_SOLVE_OK;
    }

    int ncols = total_params;

    /* 分配工作内存 */
    double *J = (double *) lv_calloc(nrows * ncols, sizeof(double));
    double *F = (double *) lv_calloc(nrows, sizeof(double));
    double *delta = (double *) lv_calloc(ncols > nrows ? ncols : nrows, sizeof(double));
    double *rhs = (double *) lv_calloc(nrows, sizeof(double));
    double *J_copy = (double *) lv_calloc(nrows * ncols, sizeof(double));

    if (!J || !F || !delta || !rhs || !J_copy) {
        lv_free((void **) &(J));
        lv_free((void **) &(F));
        lv_free((void **) &(delta));
        lv_free((void **) &(rhs));
        lv_free((void **) &(J_copy));
        sys->last_result = lv_SOLVE_FAILED;
        return lv_SOLVE_FAILED;
    }

    /* 构建参数映射表 */
    int *param_map = (int *) lv_calloc(ncols, sizeof(int));
    int acc = 0;
    for (int fi = 0; fi < free_count; fi++) {
        int ei = free_entities[fi];
        for (int p = 0; p < sys->entities[ei].param_count; p++) {
            if (acc < ncols) {
                param_map[acc] = acc;
                acc++;
            }
        }
    }

    lvSolveResult result = lv_SOLVE_NOT_CONVERGED;
    double damping = sys->config.damping_factor;
    double prev_norm_F = -1.0; /* 初始化为负值，跳过第一次迭代的比较 */

    for (int iter = 0; iter < sys->config.max_iterations; iter++) {
        sys->iteration_count = iter + 1;

        /* 构建雅可比矩阵和残差向量 */
        build_jacobian_and_residual(sys, J, F, nrows, ncols, param_map, free_entities, free_count);

        /* 检查收敛 */
        double norm_F = vec_norm(F, nrows);
        /* 检测 NaN/Inf 传播：约束求值可能产生 NaN，导致残差范数为 NaN，
         * 此时收敛检查和发散检查均无效，应提前终止迭代 */
        if (!isfinite(norm_F)) {
            result = lv_SOLVE_NOT_CONVERGED;
            break;
        }
        if (sys->config.verbose) {
            /* 静默模式下不输出 */
        }

        if (norm_F < sys->config.convergence_tol) {
            result = lv_SOLVE_OK;
            break;
        }

        /* 发散检测：如果残差相比上次迭代增长超过 10 倍，
         * 且本次残差已经超过初始残差，说明迭代正在发散。
         * 此时终止迭代避免无意义计算。 */
        if (prev_norm_F > 0.0 && norm_F > prev_norm_F * 10.0) {
            result = lv_SOLVE_NOT_CONVERGED;
            break;
        }
        prev_norm_F = norm_F;

        /* 确定求解维度 */
        int solve_n = (nrows < ncols) ? nrows : ncols;

        /* 处理超定/欠定系统 */
        if (nrows > ncols) {
            /* 超定系统：使用 J^T * J * delta = -J^T * F（正规方程） */
            /* J^T * J (ncols x ncols) */
            double *JtJ = (double *) lv_calloc(ncols * ncols, sizeof(double));
            double *JtF = (double *) lv_calloc(ncols, sizeof(double));
            if (!JtJ || !JtF) {
                lv_free((void **) &(JtJ));
                lv_free((void **) &(JtF));
                result = lv_SOLVE_FAILED;
                break;
            }

            for (int i = 0; i < ncols; i++) {
                for (int j = 0; j < ncols; j++) {
                    double sum = 0.0;
                    for (int k = 0; k < nrows; k++) {
                        sum += J[k * ncols + i] * J[k * ncols + j];
                    }
                    JtJ[i * ncols + j] = sum;
                }
                double sum = 0.0;
                for (int k = 0; k < nrows; k++) {
                    sum += J[k * ncols + i] * F[k];
                }
                JtF[i] = -sum;
            }

            memcpy(J_copy, JtJ, ncols * ncols * sizeof(double));
            memcpy(delta, JtF, ncols * sizeof(double));

            int ret = gauss_eliminate(J_copy, delta, ncols);
            lv_free((void **) &(JtJ));
            lv_free((void **) &(JtF));

            if (ret != 0) {
                result = lv_SOLVE_INCONSISTENT;
                break;
            }

            /* 更新参数 */
            int pidx = 0;
            for (int fi = 0; fi < free_count; fi++) {
                int ei = free_entities[fi];
                for (int p = 0; p < sys->entities[ei].param_count; p++) {
                    if (pidx < ncols) {
                        sys->entities[ei].params[p] += damping * delta[pidx];
                        pidx++;
                    }
                }
            }

        } else if (nrows == ncols) {
            /* 方阵系统：直接求解 J * delta = -F */
            memcpy(J_copy, J, nrows * ncols * sizeof(double));
            for (int i = 0; i < nrows; i++) {
                rhs[i] = -F[i];
            }

            int ret = gauss_eliminate(J_copy, rhs, solve_n);
            if (ret != 0) {
                result = lv_SOLVE_INCONSISTENT;
                break;
            }

            /* 更新参数 */
            int pidx = 0;
            for (int fi = 0; fi < free_count; fi++) {
                int ei = free_entities[fi];
                for (int p = 0; p < sys->entities[ei].param_count; p++) {
                    if (pidx < ncols) {
                        sys->entities[ei].params[p] += damping * rhs[pidx];
                        pidx++;
                    }
                }
            }

        } else {
            /* 欠定系统： nrows < ncols，使用最小范数解 */
            /* J * delta = -F，取 delta = J^T * (J * J^T)^{-1} * (-F) */
            double *JJt = (double *) lv_calloc(nrows * nrows, sizeof(double));
            double *neg_F = (double *) lv_calloc(nrows, sizeof(double));
            if (!JJt || !neg_F) {
                lv_free((void **) &(JJt));
                lv_free((void **) &(neg_F));
                result = lv_SOLVE_FAILED;
                break;
            }

            for (int i = 0; i < nrows; i++) {
                for (int j = 0; j < nrows; j++) {
                    double sum = 0.0;
                    for (int k = 0; k < ncols; k++) {
                        sum += J[i * ncols + k] * J[j * ncols + k];
                    }
                    JJt[i * nrows + j] = sum;
                }
                neg_F[i] = -F[i];
            }

            int ret = gauss_eliminate(JJt, neg_F, nrows);
            if (ret != 0) {
                lv_free((void **) &(JJt));
                lv_free((void **) &(neg_F));
                result = lv_SOLVE_FAILED;
                break;
            }

            /* delta = J^T * neg_F */
            memset(delta, 0, ncols * sizeof(double));
            for (int i = 0; i < ncols; i++) {
                double sum = 0.0;
                for (int k = 0; k < nrows; k++) {
                    sum += J[k * ncols + i] * neg_F[k];
                }
                delta[i] = sum;
            }

            lv_free((void **) &(JJt));
            lv_free((void **) &(neg_F));

            /* 更新参数 */
            int pidx = 0;
            for (int fi = 0; fi < free_count; fi++) {
                int ei = free_entities[fi];
                for (int p = 0; p < sys->entities[ei].param_count; p++) {
                    if (pidx < ncols) {
                        sys->entities[ei].params[p] += damping * delta[pidx];
                        pidx++;
                    }
                }
            }
        }
    }

    /* 清理 */
    lv_free((void **) &(J));
    lv_free((void **) &(F));
    lv_free((void **) &(delta));
    lv_free((void **) &(rhs));
    lv_free((void **) &(J_copy));
    lv_free((void **) &(param_map));

    sys->last_result = result;
    return result;
}

