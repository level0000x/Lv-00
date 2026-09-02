#ifndef lv_HOST_LINALG_H
#define lv_HOST_LINALG_H

#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

lv_PUBLIC_API int host_lu_factor(double *data, int64_t n, double pivot_threshold);
lv_PUBLIC_API int host_lu_solve(const double *lu, int64_t n, const double *b, double *x);

#ifdef __cplusplus
}
#endif

#endif
