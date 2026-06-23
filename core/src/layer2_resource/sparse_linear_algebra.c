#include "lv00/lv00.h"
#include "lv00/sparse_linear_algebra.h"
#include <stdlib.h>

struct Lv00SparseMatrix {
    int rows;
    int cols;
};

Lv00SparseMatrix *lv00_sparse_create(int rows, int cols)
{
    Lv00SparseMatrix *m = (Lv00SparseMatrix *)malloc(sizeof(Lv00SparseMatrix));
    if (!m) return NULL;
    m->rows = rows;
    m->cols = cols;
    return m;
}

void lv00_sparse_destroy(Lv00SparseMatrix *m)
{
    free(m);
}

int lv00_sparse_set(Lv00SparseMatrix *m, int row, int col, double val)
{
    (void)row; (void)col; (void)val;
    if (!m) return -1;
    return 0;
}

double lv00_sparse_get(const Lv00SparseMatrix *m, int row, int col)
{
    (void)row; (void)col;
    if (!m) return 0.0;
    return 0.0;
}

int lv00_sparse_solve(const Lv00SparseMatrix *A, const double *b, double *x)
{
    (void)A; (void)b; (void)x;
    return 0;
}
