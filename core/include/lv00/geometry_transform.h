#ifndef LV00_GEOMETRY_TRANSFORM_H
#define LV00_GEOMETRY_TRANSFORM_H
/* TODO: Geometry transform module stub */

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/** 4x4 transformation matrix. */
typedef double Lv00Transform4x4[16];

/** Identity transform. */
void lv00_transform_identity(Lv00Transform4x4 out);
/** Translation transform. */
void lv00_transform_translate(Lv00Transform4x4 out, double x, double y, double z);
/** Apply transform to points. */
void lv00_transform_apply(const Lv00Transform4x4 t, const double *in, double *out, size_t count);

#ifdef __cplusplus
}
#endif

#endif
