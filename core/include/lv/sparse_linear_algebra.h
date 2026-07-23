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

#ifdef __cplusplus
}
#endif

#endif /* lv_SPARSE_LINEAR_ALGEBRA_H */
