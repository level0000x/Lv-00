#include "lv/host_linalg.h"

#include <math.h>
#include <string.h>

#include "lv/numerical_backend.h"
#include "lv/error_codes.h"
#include "lv/lv_internal.h"

int host_lu_factor(double *data, int64_t n, double pivot_threshold) {
    for (int64_t k = 0; k < n; ++k) {
        double pivot = fabs(data[k * n + k]);
        int64_t pivot_row = k;
        for (int64_t i = k + 1; i < n; ++i) {
            double abs_val = fabs(data[k * n + i]);
            if (abs_val > pivot) {
                pivot = abs_val;
                pivot_row = i;
            }
        }

        if (pivot < pivot_threshold) {
            lv_ERROR_SET(lv_BACKEND_LINSOL_FAILED, "LU分解遇到奇异矩阵，pivot≈0 at col=%lld", (long long) k);
            return lv_BACKEND_SINGULAR;
        }

        if (pivot_row != k) {
            for (int64_t j = 0; j < n; ++j) {
                double tmp = data[j * n + k];
                data[j * n + k] = data[j * n + pivot_row];
                data[j * n + pivot_row] = tmp;
            }
        }

        double inv_pivot = 1.0 / data[k * n + k];
        for (int64_t i = k + 1; i < n; ++i) {
            double factor = data[k * n + i] * inv_pivot;
            data[k * n + i] = factor;
            for (int64_t j = k + 1; j < n; ++j) {
                data[j * n + i] -= factor * data[j * n + k];
            }
        }
    }

    return lv_BACKEND_OK;
}

int host_lu_solve(const double *lu, int64_t n, const double *b, double *x) {
    memcpy(x, b, (size_t) n * sizeof(double));

    for (int64_t k = 0; k < n; ++k) {
        for (int64_t i = k + 1; i < n; ++i) {
            x[i] -= lu[k * n + i] * x[k];
        }
    }

    for (int64_t k = n - 1; k >= 0; --k) {
        double diag = lu[k * n + k];
        if (fabs(diag) < lv_EPSILON_DOUBLE) {
            return lv_BACKEND_SINGULAR;
        }
        x[k] /= diag;
        for (int64_t i = 0; i < k; ++i) {
            x[i] -= lu[k * n + i] * x[k];
        }
    }

    return lv_BACKEND_OK;
}
