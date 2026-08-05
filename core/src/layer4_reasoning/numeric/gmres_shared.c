/**
 * @file gmres_shared.c
 * @brief GMRES(m) 共享迭代内核实现
 *
 * @details 原先 SERIAL / CUDA / HIP 三个数值后端各自维护一份约 230 行的
 *          重启 GMRES(m=30) 主循环（Arnoldi 过程 + MGS 正交化 + Givens 旋转
 *          + 重启机制），仅向量算子与 breakdown 阈值不同。本文件将其
 *          收敛为一个共享内核：
 *          - 各后端仅需实现 lvGmresOps 算子表（点积/范数/矩阵向量乘）
 *          - 内核在主机端数组上执行 Arnoldi 过程，适合
 *            "GPU 加速 matvec + 主机端迭代" 模式
 *          - lv_linsol_default_params()（bicgstab_shared.c）统一默认迭代参数
 *
 * @note 有意的缺陷修复：原 HIP 后端把 breakdown 阈值硬编码为 1e-14，
 *       SERIAL / CUDA 使用 lv_EPSILON_DOUBLE (1e-12)，二者不一致；
 *       且 HIP 解更新分量跳过阈值 1e-14 与 SERIAL/CUDA 的 lv_NUM_EPSILON
 *       (1e-9) 也不一致。共享内核统一使用 lv_EPSILON_DOUBLE（breakdown
 *       保护）与 lv_NUM_EPSILON（x 更新分量跳过），使三个后端语义一致
 *       （见 BiCGSTAB round 6 报告的同类问题）。
 *
 * @author Lv-00 Project
 */

#include "lv/gmres_shared.h"

#include <math.h>
#include <string.h>

#include "lv/config.h"
#include "lv/lv_utils.h"
#include "lv/geo_utils.h"

#ifndef lv_NUM_EPSILON
#define lv_NUM_EPSILON lv_EPSILON_MEDIUM
#endif

/**
 * @brief GMRES(m) 共享内核 —— 带重启的广义最小残差法
 *
 * @details 算法骨架（与三个后端原实现逐行对应）：
 *          x 作为初始猜测参与迭代（不清零），每个重启周期：
 *            r0 = b - A*x；r0 过小或相对残差 r0_norm < tol*||b|| 则终止
 *            V[0] = r0 / ||r0||，rhs = e1 * ||r0||
 *            Arnoldi（MGS 正交化）构造上 Hessenberg 矩阵 H
 *            Givens 旋转求解最小二乘；|rhs[k+1]| < tol*||b|| 提前收敛
 *            happy breakdown（h_{k+1,k} 过小）提前截断周期
 *            回代求解 y，x += V*y；重启直到 total_iters >= max_iter
 *
 * @param[in]     ops            向量算子表（dot/norm/matvec）
 * @param[in]     a              系数矩阵
 * @param[in]     b              右端向量（主机端数据）
 * @param[in,out] x              解向量（主机端数据，作为初始猜测迭代）
 * @param[in]     n              向量长度（a->rows）
 * @param[in]     max_iter       最大迭代次数（GMRES 外循环累计步数）
 * @param[in]     tol            相对收敛容差
 * @param[in]     breakdown_eps  breakdown 阈值（b_norm/r0_norm/h_next/h_norm/
 *                               回代对角元退化保护，统一三个后端）
 * @param[in]     restart_m      重启周期（原实现固定为 30）
 * @return lv_BACKEND_OK；参数非法返回 lv_BACKEND_INVALID_ARGS；
 *         工作区分配失败返回 lv_BACKEND_MEM_ERROR
 */
int lv_gmres_solve(const lvGmresOps *ops, const lvMatrix *a,
                   const double *b, double *x, int64_t n,
                   int max_iter, double tol, double breakdown_eps, int restart_m) {
    if (!ops || !ops->vector_dot || !ops->vector_norm || !ops->matvec)
        return lv_BACKEND_INVALID_ARGS;
    if (!a || !b || !x || n <= 0 || max_iter <= 0 || tol < 0.0 || restart_m <= 0)
        return lv_BACKEND_INVALID_ARGS;

    int m = restart_m;

    /* ---- 分配 GMRES 专用工作区 ---- */
    /* V: 正交基，(m+1) 列，每列 n 个元素 */
    double *V = lv_calloc((size_t) (m + 1) * (size_t) n, sizeof(double));
    /* H: 上 Hessenberg 矩阵，(m+1) x m，按列存储 */
    double *H = lv_calloc((size_t) (m + 1) * (size_t) m, sizeof(double));
    /* Givens 旋转参数 */
    double *cs = lv_calloc((size_t) m, sizeof(double));
    double *sn = lv_calloc((size_t) m, sizeof(double));
    /* 最小二乘右端项及解向量 */
    double *rhs = lv_calloc((size_t) (m + 1), sizeof(double));
    double *y = lv_calloc((size_t) m, sizeof(double));
    /* 残差向量 r0 = b - A*x */
    double *r0 = lv_calloc((size_t) n, sizeof(double));

    if (!V || !H || !cs || !sn || !rhs || !y || !r0) {
        if (V) lv_free((void **) &V);
        if (H) lv_free((void **) &H);
        if (cs) lv_free((void **) &cs);
        if (sn) lv_free((void **) &sn);
        if (rhs) lv_free((void **) &rhs);
        if (y) lv_free((void **) &y);
        if (r0) lv_free((void **) &r0);
        return lv_BACKEND_MEM_ERROR;
    }

    /* 计算 ||b|| 用于相对收敛判据 */
    double b_norm = ops->vector_norm(ops->ctx, b, n);
    if (b_norm < breakdown_eps) {
        /* b ≈ 0，直接返回零解 */
        memset(x, 0, (size_t) n * sizeof(double));
        goto gmres_cleanup;
    }

    int total_iters = 0;
    int converged = 0;

    while (total_iters < max_iter && !converged) {
        int k_max = ((max_iter - total_iters) < m) ? (max_iter - total_iters) : m;

        /* ---- 计算初始残差 r0 = b - A*x ---- */
        ops->matvec(ops->ctx, a, x, r0, n);
        for (int64_t i = 0; i < n; ++i)
            r0[i] = b[i] - r0[i];

        double r0_norm = ops->vector_norm(ops->ctx, r0, n);
        if (r0_norm < breakdown_eps) {
            converged = 1;
            break;
        }
        if (r0_norm < tol * b_norm) {
            converged = 1;
            break;
        }

        /* V[0] = r0 / ||r0|| */
        double inv_r0 = 1.0 / r0_norm;
        for (int64_t i = 0; i < n; ++i)
            V[i] = r0[i] * inv_r0;

        /* rhs = e1 * ||r0|| */
        memset(rhs, 0, (size_t) (m + 1) * sizeof(double));
        rhs[0] = r0_norm;
        memset(H, 0, (size_t) (m + 1) * (size_t) m * sizeof(double));

        /* ---- Arnoldi 过程 ---- */
        int k;
        for (k = 0; k < k_max; ++k) {
            /* w = A * V[k] */
            double *vk = V + (int64_t) k * n;
            double *w = V + (int64_t) (k + 1) * n;
            ops->matvec(ops->ctx, a, vk, w, n);

            /* Modified Gram-Schmidt 正交化 */
            for (int jj = 0; jj <= k; ++jj) {
                double *vj = V + (int64_t) jj * n;
                double dot = ops->vector_dot(ops->ctx, w, vj, n);
                H[jj * m + k] = dot;
                for (int64_t i = 0; i < n; ++i)
                    w[i] -= dot * vj[i];
            }

            /* h_{k+1,k} = ||w|| */
            double h_next = ops->vector_norm(ops->ctx, w, n);
            H[(k + 1) * m + k] = h_next;

            /* 防止 happy breakdown */
            if (h_next < breakdown_eps) {
                k_max = k + 1;
                break;
            }

            /* 归一化 w -> V[k+1] */
            double inv_h = 1.0 / h_next;
            for (int64_t i = 0; i < n; ++i)
                w[i] *= inv_h;

            /* ---- 应用之前的 Givens 旋转到 H 的第 k 列 ---- */
            for (int jj = 0; jj < k; ++jj) {
                double tmp = cs[jj] * H[jj * m + k] + sn[jj] * H[(jj + 1) * m + k];
                H[(jj + 1) * m + k] = -sn[jj] * H[jj * m + k] + cs[jj] * H[(jj + 1) * m + k];
                H[jj * m + k] = tmp;
            }

            /* ---- 计算新的 Givens 旋转 ---- */
            double h_kk = H[k * m + k];
            double h_k1k = H[(k + 1) * m + k];
            double h_norm = geo_distance_2d(0.0, 0.0, h_kk, h_k1k);
            if (h_norm < breakdown_eps) {
                cs[k] = 1.0;
                sn[k] = 0.0;
            } else {
                cs[k] = h_kk / h_norm;
                sn[k] = h_k1k / h_norm;
            }
            H[k * m + k] = h_norm;
            H[(k + 1) * m + k] = 0.0;

            /* 更新 rhs */
            rhs[k + 1] = -sn[k] * rhs[k];
            rhs[k] = cs[k] * rhs[k];

            /* 收敛检查 */
            double res = fabs(rhs[k + 1]);
            if (res < tol * b_norm) {
                k_max = k + 1;
                converged = 1;
                break;
            }
        }

        /* ---- 回代求解上三角系统 H[0..k-1, 0..k-1] * y = rhs[0..k-1] ---- */
        if (k_max > 0) {
            for (int i = k_max - 1; i >= 0; --i) {
                double sum = rhs[i];
                for (int j = i + 1; j < k_max; ++j)
                    sum -= H[i * m + j] * y[j];
                /* 保护：H 对角线在 Givens 旋转后应非零，数值误差可能导致接近零 */
                double diag = H[i * m + i];
                if (fabs(diag) < breakdown_eps) {
                    converged = 0;
                    break;
                }
                y[i] = sum / diag;
            }
        } else {
            converged = 0;
        }

        /* ---- 更新解 x = x + V * y ---- */
        for (int j = 0; j < k_max; ++j) {
            double yj = y[j];
            if (fabs(yj) < lv_NUM_EPSILON)
                continue;
            double *vj = V + (int64_t) j * n;
            for (int64_t i = 0; i < n; ++i)
                x[i] += yj * vj[i];
        }

        total_iters += k_max;
    }

gmres_cleanup:
    lv_free((void **) &V);
    lv_free((void **) &H);
    lv_free((void **) &cs);
    lv_free((void **) &sn);
    lv_free((void **) &rhs);
    lv_free((void **) &y);
    lv_free((void **) &r0);

    return lv_BACKEND_OK;
}
