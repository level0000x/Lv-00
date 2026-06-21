#ifndef LV00_GEO_DYNAMIC_H
#define LV00_GEO_DYNAMIC_H
/* TODO: Geo dynamic module stub */

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/** Dynamic geometry point with velocity. */
typedef struct { double x, y, vx, vy; } Lv00DynamicPoint;
/** Compatibility typedefs for test code. */
typedef struct { int node_count; Lv00DynamicPoint *nodes; } Lv00DynGraph;
#define lv00_dyn_graph_create() ((Lv00DynGraph*)calloc(1, sizeof(Lv00DynGraph)))

/** Step dynamic simulation forward. */
void lv00_geo_dynamic_step(Lv00DynamicPoint *points, size_t count, double dt);

#ifdef __cplusplus
}
#endif

#endif
