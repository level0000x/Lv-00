#ifndef LV00_GEO_AABB_TREE_H
#define LV00_GEO_AABB_TREE_H
/* TODO: Geo AABB tree module stub */

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/** AABB bounding box. */
typedef struct { double xmin, ymin, xmax, ymax; } Lv00AABB;
/** Compatibility typedef for test code. */
typedef Lv00AABB Lv00AABB2D;
#define lv00_aabb2d_empty() ((Lv00AABB2D){0,0,0,0})
#define lv00_aabb2d_is_valid(b) ((b).xmax >= (b).xmin && (b).ymax >= (b).ymin)
/** AABB tree node. */
typedef struct Lv00AABBTree Lv00AABBTree;

/** Build AABB tree from points. */
Lv00AABBTree *lv00_aabb_tree_build(const double *points, size_t count, int dim);
/** Query AABB tree for points in box. */
size_t lv00_aabb_tree_query(const Lv00AABBTree *tree, const Lv00AABB *box, double *out, size_t max_out);

#ifdef __cplusplus
}
#endif

#endif
