/**
 * @file bicgstab_shared.c
 * @brief BiCGSTAB 共享迭代内核实现
 *
 * @details 原先 SERIAL / CUDA / HIP 三个数值后端各自维护一份约 200 行的
 *          BiCGSTAB 主循环（van der Vorst 1992），仅向量算子与
 *          breakdown 阈值不同。本文件将其收敛为一个共享内核：
 *          - 各后端仅需实现 lvBicgstabOps 算子表（点积/范数/矩阵向量乘）
 *          - 内核在主机端数组上进行递推，适合 "GPU 加速 matvec + 主机端迭代" 模式
 *          - lv_linsol_default_params() 统一三个后端的默认迭代参数
 *
 * @author Lv-00 Project
 */

#include "lv/bicgstab_shared.h"

#include <math.h>
#include <string.h>

#include "lv/config.h"
#include "lv/lv_utils.h"

/**
 * @brief 迭代法线性求解器统一默认参数
 *
 * @details 统一 SERIAL / CUDA / HIP 三个后端的默认值：
 *          max_iters = 200，tol = lv_EPSILON_HIGH (1e-10)。
 *          此前 HIP 后端硬编码 1e-10，与命名常量 lv_EPSILON_HIGH 语义等价
 *          但缺少统一来源，参数调整时易产生复制分叉。
 *
 * @param[out] max_iters 最大迭代次数（可为 NULL）
 * @param[out] tol       收敛容差（可为 NULL）
 */
void lv_linsol_default_params(int *max_iters, double *tol) {
    if (max_iters) *max_iters = 200;
    if (tol) *tol = lv_EPSILON_HIGH;
}

/**
 * @brief BiCGSTAB 共享内核 —— 稳定双共轭梯度法 (van der Vorst 1992)
 *
 * @details 算法骨架（与三个后端原实现逐行对应）：
 *          r0 = b（影子残差），x = 0，r = b，rho/alpha/omega = 1
 *          主循环：
 *            rho_new = <r0, r>；breakdown 保护后计算 beta
 *            p = r + beta * (p - omega * v)
 *            v = A*p；alpha = rho_new / <r0, v>
 *            s = r - alpha*v；t = A*s
 *            omega = <t,s> / <t,t>；tt 退化时 x += alpha*p 后退出
 *            x += alpha*p + omega*s；r = s - omega*t
 *            收敛判据：||r|| < tol * ||b||（b 范数过小时取绝对容差）
 *
 * @param[in]     ops            向量算子表（dot/norm/matvec）
 * @param[in]     a              系数矩阵
 * @param[in]     b              右端向量（主机端数据）
 * @param[in,out] x              解向量（主机端数据，内核清零后迭代）
 * @param[in]     n              向量长度（a->rows）
 * @param[in]     max_iters      最大迭代次数
 * @param[in]     tol            相对收敛容差
 * @param[in]     breakdown_eps  breakdown 阈值（rho/alpha/omega/r0v/tt 退化保护）
 * @return lv_BACKEND_OK；参数非法返回 lv_BACKEND_INVALID_ARGS；
 *         工作区分配失败返回 lv_BACKEND_MEM_ERROR
 */
int lv_bicgstab_solve(const lvBicgstabOps *ops, const lvMatrix *a,
                      const double *b, double *x, int64_t n,
                      int max_iters, double tol, double breakdown_eps) {
    if (!ops || !ops->vector_dot || !ops->vector_norm || !ops->matvec)
        return lv_BACKEND_INVALID_ARGS;
    if (!a || !b || !x || n <= 0 || max_iters <= 0 || tol < 0.0)
        return lv_BACKEND_INVALID_ARGS;

    /* 工作向量：r 残差 / r0 影子残差 / p 搜索方向 / v A*p / s 中间残差 / t A*s */
    double *r = lv_calloc((size_t) n, sizeof(double));
    double *r0 = lv_calloc((size_t) n, sizeof(double));
    double *p = lv_calloc((size_t) n, sizeof(double));
    double *v = lv_calloc((size_t) n, sizeof(double));
    double *s = lv_calloc((size_t) n, sizeof(double));
    double *t = lv_calloc((size_t) n, sizeof(double));

    if (!r || !r0 || !p || !v || !s || !t) {
        if (r) lv_free((void **) &r);
        if (r0) lv_free((void **) &r0);
        if (p) lv_free((void **) &p);
        if (v) lv_free((void **) &v);
        if (s) lv_free((void **) &s);
        if (t) lv_free((void **) &t);
        return lv_BACKEND_MEM_ERROR;
    }

    /* 计算 ||b|| 用于相对收敛判据 */
    double b_norm = ops->vector_norm(ops->ctx, b, n);

    /* 初始猜测 x0 = 0，r = b，r0_shadow = b */
    memset(x, 0, (size_t) n * sizeof(double));
    memcpy(r, b, (size_t) n * sizeof(double));
    memcpy(r0, b, (size_t) n * sizeof(double));

    double rho = 1.0;
    double alpha = 1.0;
    double omega = 1.0;

    memset(v, 0, (size_t) n * sizeof(double));
    memset(p, 0, (size_t) n * sizeof(double));

    for (int iter = 0; iter < max_iters; ++iter) {
        /* rho_new = <r0, r> */
        double rho_new = ops->vector_dot(ops->ctx, r0, r, n);

        /* 处理 rho=0 的 breakdown */
        if (fabs(rho_new) < breakdown_eps)
            break;
        /* 防止除零：rho 和 omega 在后续迭代中可能为零 */
        double safe_rho = (fabs(rho) < breakdown_eps) ? 1.0 : rho;
        double safe_omega = (fabs(omega) < breakdown_eps) ? 1.0 : omega;
        double beta = (rho_new / safe_rho) * (alpha / safe_omega);

        /* p = r + beta * (p - omega * v) */
        for (int64_t i = 0; i < n; ++i)
            p[i] = r[i] + beta * (p[i] - omega * v[i]);

        /* v = A * p */
        ops->matvec(ops->ctx, a, p, v, n);

        /* alpha = rho_new / <r0, v> */
        double r0v = ops->vector_dot(ops->ctx, r0, v, n);
        if (fabs(r0v) < breakdown_eps)
            break;
        alpha = rho_new / r0v;

        /* s = r - alpha * v */
        for (int64_t i = 0; i < n; ++i)
            s[i] = r[i] - alpha * v[i];

        /* t = A * s */
        ops->matvec(ops->ctx, a, s, t, n);

        /* omega = <t, s> / <t, t> */
        double ts = ops->vector_dot(ops->ctx, t, s, n);
        double tt = ops->vector_dot(ops->ctx, t, t, n);
        if (fabs(tt) < breakdown_eps) {
            /* omega breakdown，用当前 alpha 更新后退出 */
            for (int64_t i = 0; i < n; ++i)
                x[i] += alpha * p[i];
            break;
        }
        omega = ts / tt;

        /* x = x + alpha * p + omega * s */
        for (int64_t i = 0; i < n; ++i)
            x[i] += alpha * p[i] + omega * s[i];

        /* r = s - omega * t */
        for (int64_t i = 0; i < n; ++i)
            r[i] = s[i] - omega * t[i];

        rho = rho_new;

        /* 收敛检查：||r|| < tol * ||b|| */
        double r_norm = ops->vector_norm(ops->ctx, r, n);

        double threshold = (b_norm > breakdown_eps) ? tol * b_norm : tol;
        if (r_norm < threshold)
            break;
    }

    lv_free((void **) &r);
    lv_free((void **) &r0);
    lv_free((void **) &p);
    lv_free((void **) &v);
    lv_free((void **) &s);
    lv_free((void **) &t);
    return lv_BACKEND_OK;
}
