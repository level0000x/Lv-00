/**
 * @file bicgstab_shared.h
 * @brief BiCGSTAB 共享迭代内核 —— 消除 SERIAL/CUDA/HIP 三个后端的算法重复
 *
 * @details 原先 numerical_backend.c（SERIAL）、cuda_backend.c（CUDA）、
 *          hip_backend.c（HIP）各自实现了一份约 200 行的 BiCGSTAB 主循环
 *          （van der Vorst 1992），仅向量算子与 breakdown 阈值不同。
 *          本文件将其收敛为单一共享内核：
 *          - 各后端仅需实现 lvBicgstabOps 算子表（点积/范数/矩阵向量乘）
 *          - 内核在主机端数组上进行递推，适用于
 *            "GPU 加速 matvec + 主机端迭代" 模式
 *          - lv_linsol_default_params() 统一三个后端的默认迭代参数
 *
 * @author Lv-00 Project
 */
#ifndef lv_BICGSTAB_SHARED_H
#define lv_BICGSTAB_SHARED_H

#include <stdint.h>

#include "numerical_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief BiCGSTAB 向量算子（各后端实现）
 *
 * @note 所有算子操作主机端 double 数组；ctx 为后端私有上下文
 *       （如迭代求解器数据，用于 matvec 访问 GPU 设备缓冲），
 *       由 lv_bicgstab_solve() 原样透传。
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
} lvBicgstabOps;

/**
 * @brief BiCGSTAB 共享内核 —— 稳定双共轭梯度法 (van der Vorst 1992)
 *
 * @details 算法骨架：r0 = b（影子残差），x = 0，r = b，rho/alpha/omega = 1，
 *          主循环依次为 rho_new → safe_rho/safe_omega → p 更新 → v = A*p →
 *          alpha → s → t = A*s → omega → x/r 更新 → 相对残差 ||r|| < tol*||b||
 *          收敛判据。
 *
 * @param[in]     ops            向量算子表（dot/norm/matvec）
 * @param[in]     a              系数矩阵
 * @param[in]     b              右端向量（主机端数据）
 * @param[in,out] x              解向量（主机端数据，内核清零后迭代）
 * @param[in]     n              向量长度（a->rows）
 * @param[in]     max_iters      最大迭代次数
 * @param[in]     tol            相对收敛容差
 * @param[in]     breakdown_eps  breakdown 阈值（rho/alpha/omega/r0v/tt 退化保护
 *                               及 b_norm 量级判断）
 * @return lv_BACKEND_OK；参数非法返回 lv_BACKEND_INVALID_ARGS；
 *         工作区分配失败返回 lv_BACKEND_MEM_ERROR
 */
int lv_bicgstab_solve(const lvBicgstabOps *ops, const lvMatrix *a,
                      const double *b, double *x, int64_t n,
                      int max_iters, double tol, double breakdown_eps);

/**
 * @brief 迭代法线性求解器统一默认参数
 *
 * @details 统一 SERIAL / CUDA / HIP 三个后端的默认值：
 *          max_iters = 200，tol = lv_EPSILON_HIGH (1e-10)。
 *          此前 HIP 后端硬编码 1e-10 且与命名常量 lv_EPSILON_HIGH 缺少
 *          统一来源，参数调整时易产生复制分叉。
 *
 * @param[out] max_iters 最大迭代次数（可为 NULL）
 * @param[out] tol       收敛容差（可为 NULL）
 */
void lv_linsol_default_params(int *max_iters, double *tol);

#ifdef __cplusplus
}
#endif

#endif /* lv_BICGSTAB_SHARED_H */
