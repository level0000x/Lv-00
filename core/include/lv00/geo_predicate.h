#ifndef LV00_GEO_PREDICATE_H
#define LV00_GEO_PREDICATE_H
/* TODO: Geo predicate module stub */

#ifdef __cplusplus
extern "C" {
#endif

/** Orientation enum. */
typedef enum { LV00_ORIENT_LEFT = -1, LV00_ORIENT_COLLINEAR = 0, LV00_ORIENT_RIGHT = 1 } Lv00Orientation;
#define lv00_orientation_2d lv00_orient2d
#define LV00_PREDICATE_APPROX 0

/** Orientation test (2D). */
double lv00_orient2d(double ax, double ay, double bx, double by, double cx, double cy);
/** Orientation test (3D). */
double lv00_orient3d(double ax, double ay, double az, double bx, double by, double bz,
                     double cx, double cy, double cz, double dx, double dy, double dz);
/** In-circle test. */
double lv00_incircle(double ax, double ay, double bx, double by, double cx, double cy, double dx, double dy);

#ifdef __cplusplus
}
#endif

#endif
