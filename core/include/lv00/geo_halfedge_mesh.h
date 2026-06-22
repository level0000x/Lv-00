#ifndef LV00_GEO_HALFEDGE_MESH_H
#define LV00_GEO_HALFEDGE_MESH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ── Constants ── */
#define LV00_HE_INVALID     (-1)
#define LV00_HE_ITER_VERTEX_OUT_HALFEDGES  0
#define LV00_HE_ITER_VERTEX_IN_HALFEDGES   1
#define LV00_HE_ITER_FACE_OUTER            0
#define LV00_HE_ITER_FACE_INNER            1

/* ── Basic types ── */
typedef int32_t Lv00Halfedge;
typedef int32_t Lv00Vertex;
typedef int32_t Lv00Face;
typedef int32_t Lv00Edge;   /* halfedge-specific: int index, not geo_topology struct */

/* ── Point 3D ── */
typedef struct {
    double x, y, z;
} Lv00Point3D;

/* ── Mesh config ── */
typedef struct {
    int  initial_capacity;
    int  max_faces_per_edge;
    bool maintain_normals;
    bool maintain_curvature;
} Lv00HeMeshConfig;

/* ── Vertex data ── */
typedef struct {
    Lv00Point3D position;
    double      normal[3];
    double      curvature;
    double      weight;
} Lv00VertexData;

/* ── Halfedge data ── */
typedef struct {
    bool marked;
    int  flags;
} Lv00HalfedgeData;

/* ── Face data ── */
typedef struct {
    double normal[3];
    double area;
    int    material;
    int    valence;
    bool   marked;
} Lv00FaceData;

/* ── Main halfedge mesh ── */
typedef struct Lv00HalfedgeMesh {
    Lv00HeMeshConfig config;

    Lv00VertexData  *vertex_data;
    Lv00Halfedge    *vertex_out_he;
    int              vertex_count;
    int              vertex_capacity;

    Lv00Halfedge     *he_twin;
    Lv00Halfedge     *he_next;
    Lv00Halfedge     *he_prev;
    Lv00Face         *he_face;
    Lv00Vertex       *he_vertex;
    Lv00HalfedgeData *he_data;
    int               halfedge_count;
    int               halfedge_capacity;

    Lv00Halfedge    *edge_he;
    int              edge_count;
    int              edge_capacity;

    Lv00Halfedge    *face_he;
    Lv00FaceData    *face_data;
    int              face_count;
    int              face_capacity;

    int              operation_count;
} Lv00HalfedgeMesh;

/* ── Iterator types ── */
typedef struct {
    Lv00HalfedgeMesh *mesh;
    Lv00Vertex        current;
    int               index;
    int               count;
} Lv00HeVertexIterator;

typedef struct {
    Lv00HalfedgeMesh *mesh;
    Lv00Face          current;
    int               index;
    int               count;
} Lv00HeFaceIterator;

/* ── Stats ── */
typedef struct {
    int vertex_count;
    int edge_count;
    int face_count;
    int halfedge_count;
    int boundary_edges;
    double avg_face_area;
} Lv00HeMeshStats;

/* ── Compatibility ── */
typedef Lv00HalfedgeMesh Lv00HeMesh;

/* ── Vector math helpers ── */
Lv00Point3D vector_sub(Lv00Point3D a, Lv00Point3D b);
double      vector_dot(Lv00Point3D a, Lv00Point3D b);
Lv00Point3D vector_cross(Lv00Point3D a, Lv00Point3D b);
Lv00Point3D vector_normalize(Lv00Point3D v);

/* ── Config ── */
Lv00HeMeshConfig lv00_he_mesh_default_config(void);

/* ── Create / destroy ── */
Lv00HeMesh *lv00_he_mesh_create(const Lv00HeMeshConfig *config);
void lv00_he_mesh_free(Lv00HeMesh *mesh);

/* ── Vertex ── */
Lv00Vertex lv00_he_mesh_add_vertex(Lv00HeMesh *mesh, double x, double y, double z);
Lv00Point3D lv00_he_mesh_get_vertex_position(const Lv00HeMesh *mesh, Lv00Vertex v);
void lv00_he_mesh_set_vertex_position(Lv00HeMesh *mesh, Lv00Vertex v, Lv00Point3D pos);

/* ── Face ── */
Lv00Face lv00_he_mesh_add_face(Lv00HeMesh *mesh, const int *indices, int count);

/* ── Edge ── */
Lv00Edge lv00_he_mesh_find_edge(const Lv00HeMesh *mesh, Lv00Vertex v1, Lv00Vertex v2);
Lv00Halfedge lv00_he_mesh_edge_halfedge(const Lv00HeMesh *mesh, Lv00Edge e);
double lv00_he_mesh_edge_length(const Lv00HeMesh *mesh, Lv00Edge e);

/* ── Normals / curvature ── */
void lv00_he_mesh_compute_normals(Lv00HeMesh *mesh);
void lv00_he_mesh_compute_curvature(Lv00HeMesh *mesh);

/* ── Queries ── */
int  lv00_he_mesh_edge_count(const Lv00HeMesh *mesh);
int  lv00_he_mesh_face_count(const Lv00HeMesh *mesh);
int  lv00_he_mesh_vertex_count(const Lv00HeMesh *mesh);
bool lv00_he_mesh_is_valid(const Lv00HeMesh *mesh);

/* ── Stats ── */
void lv00_he_mesh_get_stats(const Lv00HeMesh *mesh, Lv00HeMeshStats *stats);

/* ── Iterators ── */
Lv00HeVertexIterator lv00_he_mesh_vertex_iter_begin(Lv00HeMesh *mesh, int flags);
bool lv00_he_mesh_vertex_iter_next(Lv00HeVertexIterator *iter);
Lv00HeFaceIterator   lv00_he_mesh_face_iter_begin(Lv00HeMesh *mesh, int flags);
bool lv00_he_mesh_face_iter_next(Lv00HeFaceIterator *iter);

/* ── Legacy stubs ── */
#define lv00_he_mesh_create() lv00_he_mesh_create(NULL)
int lv00_halfedge_mesh_add_vertex(Lv00HalfedgeMesh *m, double x, double y, double z);
int lv00_halfedge_mesh_add_face(Lv00HalfedgeMesh *m, const int *indices, size_t count);

#ifdef __cplusplus
}
#endif
#endif
