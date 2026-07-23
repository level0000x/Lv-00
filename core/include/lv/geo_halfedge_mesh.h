#ifndef lv_GEO_HALFEDGE_MESH_H
#define lv_GEO_HALFEDGE_MESH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ── Opaque indices ── */
#define lv_HE_INVALID     (-1)
typedef int32_t lvHalfedge;
typedef int32_t lvVertex;
typedef int32_t lvFace;
typedef int32_t lvEdge;  /* halfedge-specific int index */

/* ── Point 3D ── */
typedef struct { double x, y, z; } lvPoint3D;

/* ── Mesh config ── */
typedef struct {
    int  initial_capacity;
    int  max_faces_per_edge;
    bool maintain_normals;
    bool maintain_curvature;
} lvHeMeshConfig;

/* ── Vertex / Face / Halfedge data ── */
typedef struct {
    lvPoint3D position;
    lvPoint3D normal;
    double      curvature;
    double      weight;
} lvVertexData;

typedef struct {
    lvPoint3D normal;
    double      area;
    int         material;
    int         valence;
    bool        marked;
} lvFaceData;

typedef struct { bool marked; int flags; } lvHalfedgeData;

/* ── Main halfedge mesh ── */
typedef struct lvHalfedgeMesh {
    lvHeMeshConfig config;
    lvVertexData  *vertex_data;
    lvHalfedge    *vertex_out_he;
    int              vertex_count, vertex_capacity;
    lvHalfedge    *he_twin, *he_next, *he_prev;
    lvFace        *he_face;
    lvVertex      *he_vertex;
    lvHalfedgeData *he_data;
    int              halfedge_count, halfedge_capacity;
    lvHalfedge    *edge_he;
    int              edge_count, edge_capacity;
    lvHalfedge    *face_he;
    lvFaceData    *face_data;
    int              face_count, face_capacity;
    int              operation_count;
} lvHalfedgeMesh;

/* ── Iterator / Stats ── */
typedef struct {
    lvHalfedgeMesh *mesh;
    lvVertex        current;
    int               index, count;
} lvHeVertexIterator;

typedef struct {
    lvHalfedgeMesh *mesh;
    lvFace          current;
    int               index, count;
} lvHeFaceIterator;

typedef struct {
    int vertex_count, edge_count, face_count, halfedge_count;
    int boundary_edges;
    double avg_face_area, total_area, average_edge_length;
    int euler_characteristic, max_vertex_valence;
} lvHeMeshStats;

/* ── Convenience macros ── */
#define lv_HE_ITER_VERTEX_OUT_HALFEDGES(mesh, v, iter) \
    for ((iter) = lv_he_mesh_vertex_out_iter_begin((mesh), (v)); \
         (iter).current != lv_HE_INVALID; \
         (iter).current = lv_he_mesh_vertex_out_iter_next(&(iter)))

/* ── Compatibility ── */
typedef lvHalfedgeMesh lvHeMesh;

/* ── API ── */
lvHeMeshConfig lv_he_mesh_default_config(void);
lvHeMesh      *lv_he_mesh_create(const lvHeMeshConfig *config);
void             lv_he_mesh_destroy(lvHeMesh *mesh);

lvVertex       lv_he_mesh_add_vertex(lvHeMesh *mesh, double x, double y, double z);
lvPoint3D      lv_he_mesh_get_vertex_position(const lvHeMesh *mesh, lvVertex v);
void             lv_he_mesh_set_vertex_position(lvHeMesh *mesh, lvVertex v, lvPoint3D pos);
lvFace         lv_he_mesh_add_face(lvHeMesh *mesh, const int *indices, int count);

lvEdge         lv_he_mesh_find_edge(const lvHeMesh *mesh, lvVertex v1, lvVertex v2);
lvHalfedge     lv_he_mesh_edge_halfedge(const lvHeMesh *mesh, lvEdge e);
double           lv_he_mesh_edge_length(const lvHeMesh *mesh, lvEdge e);

void lv_he_mesh_compute_normals(lvHeMesh *mesh);
void lv_he_mesh_compute_curvature(lvHeMesh *mesh);

int  lv_he_mesh_edge_count(const lvHeMesh *mesh);
int  lv_he_mesh_face_count(const lvHeMesh *mesh);
int  lv_he_mesh_vertex_count(const lvHeMesh *mesh);
bool lv_he_mesh_is_valid(const lvHeMesh *mesh);
bool lv_he_mesh_validate(const lvHeMesh *mesh);
void lv_he_mesh_edge_vertices(const lvHeMesh *mesh, lvEdge e, lvVertex *out_v1, lvVertex *out_v2);
void lv_he_mesh_get_stats(const lvHeMesh *mesh, lvHeMeshStats *s);

/* ── 高级几何查询 ── */
lvHalfedge lv_he_mesh_vertex_out_halfedge(const lvHeMesh *mesh, lvVertex v);
lvVertex   lv_he_mesh_halfedge_vertex(const lvHeMesh *mesh, lvHalfedge he);
lvHalfedge lv_he_mesh_halfedge_twin(const lvHeMesh *mesh, lvHalfedge he);
lvHalfedge lv_he_mesh_halfedge_next(const lvHeMesh *mesh, lvHalfedge he);
lvFace     lv_he_mesh_halfedge_face(const lvHeMesh *mesh, lvHalfedge he);
lvHalfedge lv_he_mesh_face_halfedge(const lvHeMesh *mesh, lvFace f);
lvPoint3D  lv_he_mesh_face_normal(const lvHeMesh *mesh, lvFace f);
double       lv_he_mesh_face_area(const lvHeMesh *mesh, lvFace f);
int          lv_he_mesh_face_valence(const lvHeMesh *mesh, lvFace f);
int          lv_he_mesh_face_vertices(const lvHeMesh *mesh, lvFace f, lvVertex *out_vertices);
double       lv_he_mesh_vertex_angle(const lvHeMesh *mesh, lvVertex v);
double       lv_he_mesh_vertex_curvature(const lvHeMesh *mesh, lvVertex v);
lvPoint3D  lv_he_mesh_vertex_normal(const lvHeMesh *mesh, lvVertex v);
double       lv_he_mesh_halfedge_angle(const lvHeMesh *mesh, lvHalfedge he1, lvHalfedge he2);
double       lv_he_mesh_halfedge_corner_angle(const lvHeMesh *mesh, lvHalfedge he);
lvVertex   lv_he_mesh_nearest_vertex(const lvHeMesh *mesh, lvPoint3D point, double *out_distance);
double       lv_he_mesh_total_area(const lvHeMesh *mesh);
int          lv_he_mesh_euler_characteristic(const lvHeMesh *mesh);

/* ── 快捷构造 ── */
lvFace lv_he_mesh_add_face_triangle(lvHeMesh *mesh, lvVertex v0, lvVertex v1, lvVertex v2);
lvFace lv_he_mesh_add_face_quad(lvHeMesh *mesh, lvVertex v0, lvVertex v1, lvVertex v2, lvVertex v3);

/* ── 短名迭代器 ── */
lvHeVertexIterator lv_he_vertex_iter_begin(const lvHeMesh *mesh, lvVertex v);
lvHeFaceIterator   lv_he_face_iter_begin(const lvHeMesh *mesh, lvFace f);
lvHalfedge         lv_he_vertex_iter_get(const lvHeVertexIterator *iter);
bool                 lv_he_vertex_iter_valid(const lvHeVertexIterator *iter);
void                 lv_he_vertex_iter_next(lvHeVertexIterator *iter);
lvHalfedge         lv_he_face_iter_get(const lvHeFaceIterator *iter);
bool                 lv_he_face_iter_valid(const lvHeFaceIterator *iter);
void                 lv_he_face_iter_next(lvHeFaceIterator *iter);

/* ── _mesh_ 前缀迭代器 ── */
lvHeVertexIterator lv_he_mesh_vertex_iter_begin(lvHeMesh *mesh, int flags);
lvHeVertexIterator lv_he_mesh_vertex_out_iter_begin(lvHeMesh *mesh, lvVertex v);
lvVertex           lv_he_mesh_vertex_out_iter_next(lvHeVertexIterator *iter);
bool lv_he_mesh_vertex_iter_next(lvHeVertexIterator *iter);
lvHeFaceIterator lv_he_mesh_face_iter_begin(lvHeMesh *mesh, int flags);
bool lv_he_mesh_face_iter_next(lvHeFaceIterator *iter);

/* ── Legacy ── */
static inline lvHalfedgeMesh *lv_he_mesh_create_default(void) {
    return lv_he_mesh_create(NULL);
}
int lv_halfedge_mesh_add_vertex(lvHalfedgeMesh *m, double x, double y, double z);
int lv_halfedge_mesh_add_face(lvHalfedgeMesh *m, const int *indices, size_t count);

#ifdef __cplusplus
}
#endif
#endif
