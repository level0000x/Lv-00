#ifndef LV00_GEO_HALFEDGE_MESH_H
#define LV00_GEO_HALFEDGE_MESH_H
/* TODO: Geo halfedge mesh module stub */

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/** Halfedge mesh handle. */
typedef struct Lv00HalfedgeMesh Lv00HalfedgeMesh;
/** Compatibility typedef for test code. */
typedef Lv00HalfedgeMesh Lv00HeMesh;
#define lv00_he_mesh_create() lv00_halfedge_mesh_create()

/** Create a halfedge mesh. */
Lv00HalfedgeMesh *lv00_halfedge_mesh_create(void);
/** Add vertex to mesh. */
int lv00_halfedge_mesh_add_vertex(Lv00HalfedgeMesh *mesh, double x, double y, double z);
/** Add face to mesh. */
int lv00_halfedge_mesh_add_face(Lv00HalfedgeMesh *mesh, const int *indices, size_t count);

#ifdef __cplusplus
}
#endif

#endif
