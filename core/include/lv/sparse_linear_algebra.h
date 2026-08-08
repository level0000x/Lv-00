#ifndef lv_SPARSE_LINEAR_ALGEBRA_H
#define lv_SPARSE_LINEAR_ALGEBRA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv/lv.h"

typedef struct lvSparseMatrix lvSparseMatrix;

lvSparseMatrix *lv_sparse_create(int rows, int cols);
void lv_sparse_destroy(lvSparseMatrix *m);
int lv_sparse_set(lvSparseMatrix *m, int row, int col, double val);
double lv_sparse_get(const lvSparseMatrix *m, int row, int col);
int lv_sparse_solve(const lvSparseMatrix *A, const double *b, double *x);

/* ============================================================
 * 矩阵运算（支撑 numerical_backend 的 CSR 稀疏分支）
 *
 * lvMatrix 的稀疏分支（lv_matrix_create(..., sparse=true)）通过
 * 以下接口把 CSR 矩阵接入数值后端操作表：zero/copy/scale 对应
 * lvMatrixOps 的同名操作，matvec 供稀疏矩阵-向量乘法使用。
 * ============================================================ */
/** @brief 清空矩阵所有元素（等效于全部置零，保留已分配容量） */
void lv_sparse_zero(lvSparseMatrix *m);
/** @brief 矩阵-向量乘法：y = A * x（CSR 行遍历）
 *  @return 成功返回 0；参数无效返回 -1 */
int lv_sparse_matvec(const lvSparseMatrix *A, const double *x, double *y);
/** @brief 深拷贝：dst = src（维度须一致）
 *  @return 成功返回 0；参数无效返回 -1；扩容失败返回 -1 */
int lv_sparse_copy(lvSparseMatrix *dst, const lvSparseMatrix *src);
/** @brief 矩阵-标量乘法：m = c * m（c == 0.0 时等效于 lv_sparse_zero）
 *  @return 成功返回 0；参数无效返回 -1 */
int lv_sparse_scale(lvSparseMatrix *m, double c);

#ifdef __cplusplus
}
#endif

#endif /* lv_SPARSE_LINEAR_ALGEBRA_H */
