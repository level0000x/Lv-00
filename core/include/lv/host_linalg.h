#ifndef lv_HOST_LINALG_H
#define lv_HOST_LINALG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int host_lu_factor(double *data, int64_t n, double pivot_threshold);
int host_lu_solve(const double *lu, int64_t n, const double *b, double *x);

#ifdef __cplusplus
}
#endif

#endif
