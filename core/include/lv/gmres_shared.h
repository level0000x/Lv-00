/**
 * @file gmres_shared.h
 * @brief GMRES(m) 共享迭代内核 —— 消除 SERIAL/CUDA/HIP 三个后端的算法重复
 *
 * @details 原先 numerical_backend.c（SERIAL）、cuda_backend.c（CUDA）、
 *          hip_backend.c（HIP）各自实现了一份约 230 行的重启 GMRES(m=30)
 *          主循环（Arnoldi 过程 + MGS 正交化 + Givens 旋转 + 重启机制），
 *          仅向量算子与 breakdown 阈值不同。本文件将其收敛为单一共享内核：
 *          - 各后端仅需实现 lvGmresOps 算子表（点积/范数/矩阵向量乘）
 *          - 内核在主机端数组上执行 Arnoldi 过程，适用于
 *            "GPU 加速 matvec + 主机端迭代" 模式
 *          - lv_linsol_default_params()（bicgstab_shared.h）统一默认迭代参数
 *
 * @note 算子表与 lvBicgstabOps 同构（ctx + vector_dot/vector_norm/matvec），
 *       各后端的 BiCGSTAB 算子实现可直接复用。
 *
 * @author Lv-00 Project
 */
#ifndef lv_GMRES_SHARED_H
#define lv_GMRES_SHARED_H

#include <stdint.h>

#include "numerical_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief GMRES 向量算子（各后端实现）
 *
 * @note 所有算子操作主机端 double 数组；ctx 为后端私有上下文
 *       （如迭代求解器数据，用于 matvec 访问 GPU 设备缓冲），
 *       由 lv_gmres_solve() 原样透传。与 lvBicgstabOps 同构，
 *       各后端可复用其 BiCGSTAB 算子实现。
 */
typedef struct {
    void *ctx; /**< 后端私有上下文，透传给各算子 */
    /**
     * @brief 点积：返回 <a, b>
     */
    double (*vector_dot)(void *ctx, const double *a, const double *b, int64_t n);
    /**
     * @brief L2 范数：返回 ||v||_2
     */
    double (*vector_norm)(void *ctx, const double *v, int64_t n);
    /**
     * @brief 矩阵向量乘：y = A*x（实现须清零/覆盖 y）
     */
    void (*matvec)(void *ctx, const lvMatrix *a, const double *x, double *y, int64_t n);
} lvGmresOps;

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
                   int max_iter, double tol, double breakdown_eps, int restart_m);

#ifdef __cplusplus
}
#endif

#endif /* lv_GMRES_SHARED_H */
