#ifndef LV00_SPARSE_LINEAR_ALGEBRA_H
#define LV00_SPARSE_LINEAR_ALGEBRA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv00/lv00.h"

typedef struct Lv00SparseMatrix Lv00SparseMatrix;

Lv00SparseMatrix *lv00_sparse_create(int rows, int cols);
void lv00_sparse_destroy(Lv00SparseMatrix *m);
int lv00_sparse_set(Lv00SparseMatrix *m, int row, int col, double val);
double lv00_sparse_get(const Lv00SparseMatrix *m, int row, int col);
int lv00_sparse_solve(const Lv00SparseMatrix *A, const double *b, double *x);

#ifdef __cplusplus
}
#endif

#endif /* LV00_SPARSE_LINEAR_ALGEBRA_H */
