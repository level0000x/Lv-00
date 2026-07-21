/**
 * @file geo_halfedge_mesh.c
 * @brief Halfedge 网格拓扑数据结构实现
 *
 * 实现策略：
 *   - 使用结构化数组存储拓扑关系
 *   - 半边自动配对（twin）构建
 *   - 面迭代使用半边环绕遍历
 *
 * @version v3.6.0
 */

#include "lv00/geo_halfedge_mesh.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

#ifndef LV00_PUBLIC_API
#define LV00_PUBLIC_API
#endif

/* ========================================================================
 * 内部常量
 * ======================================================================== */

#define INITIAL_CAPACITY 64

/* ========================================================================
 * 第一部分：默认配置与创建释放
 * ======================================================================== */

Lv00HeMeshConfig lv00_he_mesh_default_config(void)
{
    Lv00HeMeshConfig cfg;
    cfg.initial_capacity = INITIAL_CAPACITY;
    cfg.max_faces_per_edge = 2;
    cfg.maintain_normals = true;
    cfg.maintain_curvature = false;
    return cfg;
}

static bool ensure_capacity(Lv00HeMesh *mesh)
{
    int new_cap = mesh->vertex_capacity * 2;
    if (new_cap < INITIAL_CAPACITY) new_cap = INITIAL_CAPACITY;

    Lv00VertexData *new_vdata = (Lv00VertexData *)realloc(
        mesh->vertex_data, new_cap * sizeof(Lv00VertexData));
    Lv00Halfedge *new_vhe = (Lv00Halfedge *)realloc(
        mesh->vertex_out_he, new_cap * sizeof(Lv00Halfedge));

    if (!new_vdata || !new_vhe) {
        if (new_vdata) free(new_vdata);
        if (new_vhe) free(new_vhe);
        return false;
    }

    mesh->vertex_data = new_vdata;
    mesh->vertex_out_he = new_vhe;
    mesh->vertex_capacity = new_cap;

    return true;
}

Lv00HeMesh *lv00_he_mesh_create(const Lv00HeMeshConfig *config)
{
    Lv00HeMesh *mesh = (Lv00HeMesh *)calloc(1, sizeof(Lv00HeMesh));
    if (!mesh) return NULL;

    if (config) {
        mesh->config = *config;
    } else {
        mesh->config = lv00_he_mesh_default_config();
    }

    mesh->vertex_capacity = INITIAL_CAPACITY;
    mesh->halfedge_capacity = INITIAL_CAPACITY * 6;
    mesh->edge_capacity = INITIAL_CAPACITY * 3;
    mesh->face_capacity = INITIAL_CAPACITY * 2;

    mesh->vertex_data = (Lv00VertexData *)calloc(
        mesh->vertex_capacity, sizeof(Lv00VertexData));
    mesh->vertex_out_he = (Lv00Halfedge *)calloc(
        mesh->vertex_capacity, sizeof(Lv00Halfedge));

    mesh->he_twin = (Lv00Halfedge *)malloc(
        mesh->halfedge_capacity * sizeof(Lv00Halfedge));
    mesh->he_next = (Lv00Halfedge *)malloc(
        mesh->halfedge_capacity * sizeof(Lv00Halfedge));
    mesh->he_prev = (Lv00Halfedge *)malloc(
        mesh->halfedge_capacity * sizeof(Lv00Halfedge));
    mesh->he_face = (Lv00Face *)malloc(
        mesh->halfedge_capacity * sizeof(Lv00Face));
    mesh->he_vertex = (Lv00Vertex *)malloc(
        mesh->halfedge_capacity * sizeof(Lv00Vertex));
    mesh->he_data = (Lv00HalfedgeData *)calloc(
        mesh->halfedge_capacity, sizeof(Lv00HalfedgeData));

    mesh->edge_he = (Lv00Halfedge *)malloc(
        mesh->edge_capacity * sizeof(Lv00Halfedge));

    mesh->face_he = (Lv00Halfedge *)malloc(
        mesh->face_capacity * sizeof(Lv00Halfedge));
    mesh->face_data = (Lv00FaceData *)calloc(
        mesh->face_capacity, sizeof(Lv00FaceData));

    if (!mesh->vertex_data || !mesh->vertex_out_he ||
        !mesh->he_twin || !mesh->he_next || !mesh->he_prev ||
        !mesh->he_face || !mesh->he_vertex || !mesh->edge_he ||
        !mesh->face_he) {
        lv00_he_mesh_free(mesh);
        return NULL;
    }

    /* 初始化为 INVALID */
    for (int i = 0; i < mesh->vertex_capacity; i++) {
        mesh->vertex_out_he[i] = LV00_HE_INVALID;
    }
    for (int i = 0; i < mesh->halfedge_capacity; i++) {
        mesh->he_twin[i] = LV00_HE_INVALID;
        mesh->he_face[i] = LV00_HE_INVALID;
        mesh->he_vertex[i] = LV00_HE_INVALID;
    }

    return mesh;
}

void lv00_he_mesh_free(Lv00HeMesh *mesh)
{
    if (!mesh) return;

    free(mesh->vertex_data);
    free(mesh->vertex_out_he);
    free(mesh->he_twin);
    free(mesh->he_next);
    free(mesh->he_prev);
    free(mesh->he_face);
    free(mesh->he_vertex);
    free(mesh->he_data);
    free(mesh->edge_he);
    free(mesh->face_he);
    free(mesh->face_data);
    free(mesh);
}

void lv00_he_mesh_clear(Lv00HeMesh *mesh)
{
    if (!mesh) return;

    mesh->vertex_count = 0;
    mesh->halfedge_count = 0;
    mesh->edge_count = 0;
    mesh->face_count = 0;

    for (int i = 0; i < mesh->vertex_capacity; i++) {
        mesh->vertex_out_he[i] = LV00_HE_INVALID;
    }
    for (int i = 0; i < mesh->halfedge_capacity; i++) {
        mesh->he_twin[i] = LV00_HE_INVALID;
        mesh->he_face[i] = LV00_HE_INVALID;
        mesh->he_vertex[i] = LV00_HE_INVALID;
    }
}

/* ========================================================================
 * 第二部分：顶点和半边操作
 * ======================================================================== */

Lv00Vertex lv00_he_mesh_add_vertex(Lv00HeMesh *mesh, double x, double y, double z)
{
    if (!mesh) return LV00_HE_INVALID;

    if (mesh->vertex_count >= mesh->vertex_capacity) {
        if (!ensure_capacity(mesh)) return LV00_HE_INVALID;
    }

    Lv00Vertex v = mesh->vertex_count++;
    mesh->vertex_data[v].position.x = x;
    mesh->vertex_data[v].position.y = y;
    mesh->vertex_data[v].position.z = z;
    mesh->vertex_data[v].normal.x = 0;
    mesh->vertex_data[v].normal.y = 0;
    mesh->vertex_data[v].normal.z = 1;
    mesh->vertex_data[v].curvature = 0;
    mesh->vertex_data[v].weight = 1.0;
    mesh->vertex_out_he[v] = LV00_HE_INVALID;

    return v;
}

Lv00Point3D lv00_he_mesh_get_vertex_position(const Lv00HeMesh *mesh, Lv00Vertex v)
{
    Lv00Point3D p = {0, 0, 0};
    if (mesh && v >= 0 && v < mesh->vertex_count) {
        p = mesh->vertex_data[v].position;
    }
    return p;
}

void lv00_he_mesh_set_vertex_position(Lv00HeMesh *mesh, Lv00Vertex v, Lv00Point3D pos)
{
    if (mesh && v >= 0 && v < mesh->vertex_count) {
        mesh->vertex_data[v].position = pos;
    }
}

Lv00Halfedge lv00_he_mesh_vertex_out_halfedge(const Lv00HeMesh *mesh, Lv00Vertex v)
{
    if (mesh && v >= 0 && v < mesh->vertex_count) {
        return mesh->vertex_out_he[v];
    }
    return LV00_HE_INVALID;
}

Lv00Vertex lv00_he_mesh_halfedge_vertex(const Lv00HeMesh *mesh, Lv00Halfedge he)
{
    if (mesh && he >= 0 && he < mesh->halfedge_count) {
        return mesh->he_vertex[he];
    }
    return LV00_HE_INVALID;
}

Lv00Halfedge lv00_he_mesh_halfedge_twin(const Lv00HeMesh *mesh, Lv00Halfedge he)
{
    if (mesh && he >= 0 && he < mesh->halfedge_count) {
        return mesh->he_twin[he];
    }
    return LV00_HE_INVALID;
}

Lv00Halfedge lv00_he_mesh_halfedge_next(const Lv00HeMesh *mesh, Lv00Halfedge he)
{
    if (mesh && he >= 0 && he < mesh->halfedge_count) {
        return mesh->he_next[he];
    }
    return LV00_HE_INVALID;
}

Lv00Face lv00_he_mesh_halfedge_face(const Lv00HeMesh *mesh, Lv00Halfedge he)
{
    if (mesh && he >= 0 && he < mesh->halfedge_count) {
        return mesh->he_face[he];
    }
    return LV00_HE_INVALID;
}

/* ========================================================================
 * 第三部分：边操作
 * ======================================================================== */

static Lv00Edge find_or_create_edge(Lv00HeMesh *mesh, Lv00Vertex v1, Lv00Vertex v2)
{
    /* 简化：直接遍历所有边 */
    for (Lv00Edge e = 0; e < mesh->edge_count; e++) {
        Lv00Halfedge he = mesh->edge_he[e];
        if (he < 0 || he >= mesh->halfedge_count) continue;
        Lv00Halfedge twin = mesh->he_twin[he];
        if (twin < 0 || twin >= mesh->halfedge_count) continue;
        Lv00Vertex ev1 = mesh->he_vertex[he];
        Lv00Vertex ev2 = mesh->he_vertex[twin];
        if ((ev1 == v1 && ev2 == v2) || (ev1 == v2 && ev2 == v1)) {
            return e;
        }
    }
    return LV00_HE_INVALID;
}

Lv00Edge lv00_he_mesh_find_edge(const Lv00HeMesh *mesh, Lv00Vertex v1, Lv00Vertex v2)
{
    if (!mesh || v1 == v2) return LV00_HE_INVALID;
    return find_or_create_edge((Lv00HeMesh *)mesh, v1, v2);
}

Lv00Halfedge lv00_he_mesh_edge_halfedge(const Lv00HeMesh *mesh, Lv00Edge e)
{
    if (mesh && e >= 0 && e < mesh->edge_count) {
        return mesh->edge_he[e];
    }
    return LV00_HE_INVALID;
}

double lv00_he_mesh_edge_length(const Lv00HeMesh *mesh, Lv00Edge e)
{
    if (!mesh || e < 0 || e >= mesh->edge_count) return 0;

    Lv00Halfedge he = mesh->edge_he[e];
    Lv00Vertex v1 = mesh->he_vertex[he];
    Lv00Vertex v2 = mesh->he_vertex[mesh->he_twin[he]];

    Lv00Point3D p1 = mesh->vertex_data[v1].position;
    Lv00Point3D p2 = mesh->vertex_data[v2].position;

    double dx = p2.x - p1.x;
    double dy = p2.y - p1.y;
    double dz = p2.z - p1.z;

    return sqrt(dx * dx + dy * dy + dz * dz);
}

void lv00_he_mesh_edge_vertices(const Lv00HeMesh *mesh, Lv00Edge e, Lv00Vertex *out_v1, Lv00Vertex *out_v2)
{
    if (!mesh || e < 0 || e >= mesh->edge_count) return;

    Lv00Halfedge he = mesh->edge_he[e];
    *out_v1 = mesh->he_vertex[he];
    *out_v2 = mesh->he_vertex[mesh->he_twin[he]];
}

/* ========================================================================
 * 第四部分：面操作
 * ======================================================================== */

static Lv00Halfedge add_halfedge_pair(Lv00HeMesh *mesh, Lv00Vertex v1, Lv00Vertex v2)
{
    /* 检查是否已存在 */
    for (Lv00Edge e = 0; e < mesh->edge_count; e++) {
        Lv00Halfedge existing = mesh->edge_he[e];
        Lv00Vertex ev1 = mesh->he_vertex[existing];
        Lv00Vertex ev2 = mesh->he_vertex[mesh->he_twin[existing]];

        if ((ev1 == v1 && ev2 == v2) || (ev1 == v2 && ev2 == v1)) {
            /* 边已存在 */
            if (mesh->he_twin[existing] != LV00_HE_INVALID) {
                return existing; /* 边已配对 */
            }
        }
    }

    /* 创建新边 */
    if (mesh->edge_count >= mesh->edge_capacity) {
        int new_cap = mesh->edge_capacity * 2;
        Lv00Halfedge *new_edge_he = (Lv00Halfedge *)realloc(
            mesh->edge_he, new_cap * sizeof(Lv00Halfedge));
        if (!new_edge_he) return LV00_HE_INVALID;
        mesh->edge_he = new_edge_he;
        mesh->edge_capacity = new_cap;
    }

    Lv00Edge new_edge = mesh->edge_count++;

    /* 创建两个半边 */
    if (mesh->halfedge_count + 1 >= mesh->halfedge_capacity) {
        int new_cap = mesh->halfedge_capacity * 2;
        Lv00Halfedge *new_twin = (Lv00Halfedge *)realloc(mesh->he_twin, new_cap * sizeof(Lv00Halfedge));
        Lv00Halfedge *new_next = (Lv00Halfedge *)realloc(mesh->he_next, new_cap * sizeof(Lv00Halfedge));
        Lv00Halfedge *new_prev = (Lv00Halfedge *)realloc(mesh->he_prev, new_cap * sizeof(Lv00Halfedge));
        Lv00Face *new_face = (Lv00Face *)realloc(mesh->he_face, new_cap * sizeof(Lv00Face));
        Lv00Vertex *new_vertex = (Lv00Vertex *)realloc(mesh->he_vertex, new_cap * sizeof(Lv00Vertex));

        if (!new_twin || !new_next || !new_prev || !new_face || !new_vertex) {
            mesh->edge_count--;
            return LV00_HE_INVALID;
        }

        mesh->he_twin = new_twin;
        mesh->he_next = new_next;
        mesh->he_prev = new_prev;
        mesh->he_face = new_face;
        mesh->he_vertex = new_vertex;
        mesh->halfedge_capacity = new_cap;
    }

    Lv00Halfedge he1 = mesh->halfedge_count++;
    Lv00Halfedge he2 = mesh->halfedge_count++;

    /* 设置半边数据 */
    mesh->he_vertex[he1] = v1;
    mesh->he_vertex[he2] = v2;
    mesh->he_twin[he1] = he2;
    mesh->he_twin[he2] = he1;
    mesh->he_face[he1] = LV00_HE_INVALID;
    mesh->he_face[he2] = LV00_HE_INVALID;
    mesh->he_next[he1] = LV00_HE_INVALID;
    mesh->he_next[he2] = LV00_HE_INVALID;
    mesh->he_prev[he1] = LV00_HE_INVALID;
    mesh->he_prev[he2] = LV00_HE_INVALID;

    /* 更新顶点的 outgoing halfedge */
    if (mesh->vertex_out_he[v1] == LV00_HE_INVALID) {
        mesh->vertex_out_he[v1] = he1;
    }
    if (mesh->vertex_out_he[v2] == LV00_HE_INVALID) {
        mesh->vertex_out_he[v2] = he2;
    }

    /* 记录边 */
    mesh->edge_he[new_edge] = he1;

    return he1;
}

Lv00Face lv00_he_mesh_add_face_triangle(Lv00HeMesh *mesh, Lv00Vertex v1, Lv00Vertex v2, Lv00Vertex v3)
{
    if (!mesh || v1 == v2 || v2 == v3 || v3 == v1) {
        return LV00_HE_INVALID;
    }

    /* 创建三条半边 */
    Lv00Halfedge he1 = add_halfedge_pair(mesh, v1, v2);
    Lv00Halfedge he2 = add_halfedge_pair(mesh, v2, v3);
    Lv00Halfedge he3 = add_halfedge_pair(mesh, v3, v1);

    if (he1 == LV00_HE_INVALID || he2 == LV00_HE_INVALID || he3 == LV00_HE_INVALID) {
        return LV00_HE_INVALID;
    }

    /* 创建面 */
    if (mesh->face_count >= mesh->face_capacity) {
        int new_cap = mesh->face_capacity * 2;
        Lv00Halfedge *new_face_he = (Lv00Halfedge *)realloc(
            mesh->face_he, new_cap * sizeof(Lv00Halfedge));
        Lv00FaceData *new_face_data = (Lv00FaceData *)realloc(
            mesh->face_data, new_cap * sizeof(Lv00FaceData));

        if (!new_face_he || !new_face_data) {
            if (new_face_he) free(new_face_he);
            if (new_face_data) free(new_face_data);
            return LV00_HE_INVALID;
        }

        mesh->face_he = new_face_he;
        mesh->face_data = new_face_data;
        mesh->face_capacity = new_cap;
    }

    Lv00Face f = mesh->face_count++;
    mesh->face_he[f] = he1;
    mesh->he_face[he1] = f;
    mesh->he_face[he2] = f;
    mesh->he_face[he3] = f;

    /* 设置 next/prev */
    mesh->he_next[he1] = he2;
    mesh->he_next[he2] = he3;
    mesh->he_next[he3] = he1;

    mesh->he_prev[he2] = he1;
    mesh->he_prev[he3] = he2;
    mesh->he_prev[he1] = he3;

    /* 计算面法向量和面积 */
    Lv00Point3D p1 = mesh->vertex_data[v1].position;
    Lv00Point3D p2 = mesh->vertex_data[v2].position;
    Lv00Point3D p3 = mesh->vertex_data[v3].position;

    double ax = p2.x - p1.x, ay = p2.y - p1.y, az = p2.z - p1.z;
    double bx = p3.x - p1.x, by = p3.y - p1.y, bz = p3.z - p1.z;

    double nx = ay * bz - az * by;
    double ny = az * bx - ax * bz;
    double nz = ax * by - ay * bx;
    double len = sqrt(nx * nx + ny * ny + nz * nz);

    mesh->face_data[f].normal.x = nx / len;
    mesh->face_data[f].normal.y = ny / len;
    mesh->face_data[f].normal.z = nz / len;
    mesh->face_data[f].area = len / 2.0;
    mesh->face_data[f].valence = 3;

    return f;
}

Lv00Face lv00_he_mesh_add_face_quad(Lv00HeMesh *mesh, Lv00Vertex v1, Lv00Vertex v2,
                                     Lv00Vertex v3, Lv00Vertex v4)
{
    /* 简化为两个三角形 */
    Lv00Face f1 = lv00_he_mesh_add_face_triangle(mesh, v1, v2, v3);
    if (f1 == LV00_HE_INVALID) return LV00_HE_INVALID;

    Lv00Face f2 = lv00_he_mesh_add_face_triangle(mesh, v1, v3, v4);
    if (f2 == LV00_HE_INVALID) return f1; /* 返回第一个面作为近似 */

    return f1;
}

Lv00Halfedge lv00_he_mesh_face_halfedge(const Lv00HeMesh *mesh, Lv00Face f)
{
    if (mesh && f >= 0 && f < mesh->face_count) {
        return mesh->face_he[f];
    }
    return LV00_HE_INVALID;
}

Lv00Point3D lv00_he_mesh_face_normal(const Lv00HeMesh *mesh, Lv00Face f)
{
    Lv00Point3D n = {0, 0, 0};
    if (mesh && f >= 0 && f < mesh->face_count) {
        n = mesh->face_data[f].normal;
    }
    return n;
}

double lv00_he_mesh_face_area(const Lv00HeMesh *mesh, Lv00Face f)
{
    if (mesh && f >= 0 && f < mesh->face_count) {
        return mesh->face_data[f].area;
    }
    return 0;
}

int lv00_he_mesh_face_valence(const Lv00HeMesh *mesh, Lv00Face f)
{
    if (mesh && f >= 0 && f < mesh->face_count) {
        return mesh->face_data[f].valence;
    }
    return 0;
}

int lv00_he_mesh_face_vertices(const Lv00HeMesh *mesh, Lv00Face f, Lv00Vertex *out_vertices)
{
    if (!mesh || f < 0 || f >= mesh->face_count || !out_vertices) {
        return 0;
    }

    Lv00Halfedge start = mesh->face_he[f];
    Lv00Halfedge current = start;
    int count = 0;

    do {
        out_vertices[count++] = mesh->he_vertex[current];
        current = mesh->he_next[current];
    } while (current != start && count < 16);

    return count;
}

/* ========================================================================
 * 第五部分：遍历工具
 * ======================================================================== */

Lv00HeVertexIterator lv00_he_vertex_iter_begin(const Lv00HeMesh *mesh, Lv00Vertex v)
{
    Lv00HeVertexIterator iter;
    iter.mesh = mesh;
    iter.current = (mesh && v >= 0 && v < mesh->vertex_count) ?
                   mesh->vertex_out_he[v] : LV00_HE_INVALID;
    iter.count = 0;
    iter.index = 0;

    /* 计算顶点度数 */
    if (iter.current != LV00_HE_INVALID && iter.current >= 0 && iter.current < mesh->halfedge_count) {
        Lv00Halfedge start = iter.current;
        Lv00Halfedge cur = iter.current;
        int max_iterations = 100; /* 安全限制 */

        do {
            iter.count++;
            Lv00Halfedge twin = mesh->he_twin[cur];
            if (twin < 0 || twin >= mesh->halfedge_count) break;
            Lv00Halfedge next = mesh->he_next[twin];
            if (next < 0 || next >= mesh->halfedge_count) break;
            cur = next;
            max_iterations--;
        } while (cur != start && cur != LV00_HE_INVALID && max_iterations > 0);
    }

    return iter;
}

Lv00Halfedge lv00_he_vertex_iter_get(const Lv00HeVertexIterator *iter)
{
    if (!iter || !iter->mesh || iter->current == LV00_HE_INVALID) {
        return LV00_HE_INVALID;
    }
    return iter->mesh->he_twin[iter->current];
}

bool lv00_he_vertex_iter_valid(const Lv00HeVertexIterator *iter)
{
    if (!iter) return false;
    return iter->current != LV00_HE_INVALID && iter->index < iter->count;
}

void lv00_he_vertex_iter_next(Lv00HeVertexIterator *iter)
{
    if (!iter || !iter->mesh || iter->current == LV00_HE_INVALID) return;

    iter->current = iter->mesh->he_next[iter->mesh->he_twin[iter->current]];
    iter->index++;

    if (iter->current == LV00_HE_INVALID || iter->current < 0 || iter->current >= iter->mesh->halfedge_count || iter->index >= iter->count) {
        iter->current = LV00_HE_INVALID; /* 完成一圈 */
    }
}

Lv00HeFaceIterator lv00_he_face_iter_begin(const Lv00HeMesh *mesh, Lv00Face f)
{
    Lv00HeFaceIterator iter;
    iter.mesh = mesh;
    iter.current = (mesh && f >= 0 && f < mesh->face_count) ?
                    mesh->face_he[f] : LV00_HE_INVALID;
    iter.count = (iter.current != LV00_HE_INVALID) ? mesh->face_data[f].valence : 0;
    iter.index = 0;
    return iter;
}

Lv00Halfedge lv00_he_face_iter_get(const Lv00HeFaceIterator *iter)
{
    if (!iter || !iter->mesh || iter->current == LV00_HE_INVALID) {
        return LV00_HE_INVALID;
    }
    return iter->current;
}

bool lv00_he_face_iter_valid(const Lv00HeFaceIterator *iter)
{
    if (!iter) return false;
    return iter->current != LV00_HE_INVALID && iter->index < iter->count;
}

void lv00_he_face_iter_next(Lv00HeFaceIterator *iter)
{
    if (!iter || !iter->mesh || iter->current == LV00_HE_INVALID) return;

    iter->current = iter->mesh->he_next[iter->current];
    iter->index++;

    if (iter->index >= iter->count) {
        iter->current = LV00_HE_INVALID;
    }
}

/* ========================================================================
 * 第六部分：几何量计算
 * ======================================================================== */

static double vector_dot(Lv00Point3D a, Lv00Point3D b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Lv00Point3D vector_cross(Lv00Point3D a, Lv00Point3D b)
{
    Lv00Point3D c;
    c.x = a.y * b.z - a.z * b.y;
    c.y = a.z * b.x - a.x * b.z;
    c.z = a.x * b.y - a.y * b.x;
    return c;
}

static Lv00Point3D vector_sub(Lv00Point3D a, Lv00Point3D b)
{
    Lv00Point3D c;
    c.x = a.x - b.x;
    c.y = a.y - b.y;
    c.z = a.z - b.z;
    return c;
}

double lv00_he_mesh_vertex_angle(const Lv00HeMesh *mesh, Lv00Vertex v)
{
    if (!mesh || v < 0 || v >= mesh->vertex_count) return 0;

    double total_angle = 0;
    int count = 0;

    Lv00HeVertexIterator it;
    LV00_HE_ITER_VERTEX_OUT_HALFEDGES(mesh, v, it) {
        Lv00Halfedge he = lv00_he_vertex_iter_get(&it);
        if (he == LV00_HE_INVALID) continue;

        Lv00Halfedge prev_he = mesh->he_prev[he];
        Lv00Vertex v0 = mesh->he_vertex[mesh->he_twin[prev_he]];
        Lv00Vertex v1 = v;
        Lv00Vertex v2 = mesh->he_vertex[he];

        Lv00Point3D p0 = mesh->vertex_data[v0].position;
        Lv00Point3D p1 = mesh->vertex_data[v1].position;
        Lv00Point3D p2 = mesh->vertex_data[v2].position;

        Lv00Point3D a = vector_sub(p0, p1);
        Lv00Point3D b = vector_sub(p2, p1);

        double dot = vector_dot(a, b);
        double len_a = sqrt(vector_dot(a, a));
        double len_b = sqrt(vector_dot(b, b));

        if (len_a > 0 && len_b > 0) {
            double cos_angle = dot / (len_a * len_b);
            if (cos_angle > 1) cos_angle = 1;
            if (cos_angle < -1) cos_angle = -1;
            total_angle += acos(cos_angle);
            count++;
        }
    }

    return (count > 0) ? total_angle : 0;
}

double lv00_he_mesh_vertex_curvature(const Lv00HeMesh *mesh, Lv00Vertex v)
{
    if (!mesh || v < 0 || v >= mesh->vertex_count) return 0;

    /* 简化的离散曲率：2π - 邻接角和 */
    double angle_sum = lv00_he_mesh_vertex_angle(mesh, v);
    return 2.0 * 3.14159265358979 - angle_sum;
}

Lv00Point3D lv00_he_mesh_vertex_normal(const Lv00HeMesh *mesh, Lv00Vertex v)
{
    Lv00Point3D n = {0, 0, 0};
    if (!mesh || v < 0 || v >= mesh->vertex_count) return n;

    double total_area = 0;

    Lv00HeVertexIterator it;
    LV00_HE_ITER_VERTEX_OUT_HALFEDGES(mesh, v, it) {
        Lv00Halfedge he = lv00_he_vertex_iter_get(&it);
        if (he == LV00_HE_INVALID) continue;

        Lv00Halfedge prev_he = mesh->he_prev[he];
        Lv00Vertex v0 = mesh->he_vertex[mesh->he_twin[prev_he]];
        Lv00Vertex v2 = mesh->he_vertex[he];

        Lv00Point3D p0 = mesh->vertex_data[v0].position;
        Lv00Point3D p1 = mesh->vertex_data[v].position;
        Lv00Point3D p2 = mesh->vertex_data[v2].position;

        Lv00Point3D a = vector_sub(p0, p1);
        Lv00Point3D b = vector_sub(p2, p1);
        Lv00Point3D cross = vector_cross(a, b);
        double area = sqrt(vector_dot(cross, cross)) / 2.0;

        n.x += cross.x;
        n.y += cross.y;
        n.z += cross.z;
        total_area += area;
    }

    if (total_area > 0) {
        double len = sqrt(vector_dot(n, n));
        if (len > 0) {
            n.x /= len;
            n.y /= len;
            n.z /= len;
        }
    }

    return n;
}

double lv00_he_mesh_halfedge_angle(const Lv00HeMesh *mesh, Lv00Halfedge he1, Lv00Halfedge he2)
{
    if (!mesh || he1 == LV00_HE_INVALID || he2 == LV00_HE_INVALID) return 0;

    Lv00Vertex v = mesh->he_vertex[he1];
    Lv00Vertex v0 = mesh->he_vertex[mesh->he_twin[he1]];
    Lv00Vertex v2 = mesh->he_vertex[he2];

    Lv00Point3D p0 = mesh->vertex_data[v0].position;
    Lv00Point3D p1 = mesh->vertex_data[v].position;
    Lv00Point3D p2 = mesh->vertex_data[v2].position;

    Lv00Point3D a = vector_sub(p0, p1);
    Lv00Point3D b = vector_sub(p2, p1);

    double dot = vector_dot(a, b);
    double len_a = sqrt(vector_dot(a, a));
    double len_b = sqrt(vector_dot(b, b));

    if (len_a == 0 || len_b == 0) return 0;

    double cos_angle = dot / (len_a * len_b);
    if (cos_angle > 1) cos_angle = 1;
    if (cos_angle < -1) cos_angle = -1;

    return acos(cos_angle);
}

double lv00_he_mesh_halfedge_corner_angle(const Lv00HeMesh *mesh, Lv00Halfedge he)
{
    if (!mesh || he == LV00_HE_INVALID) return 0;

    Lv00Halfedge prev_he = mesh->he_prev[he];
    return lv00_he_mesh_halfedge_angle(mesh, prev_he, he);
}

void lv00_he_mesh_update_geometry(Lv00HeMesh *mesh)
{
    if (!mesh) return;

    /* 更新所有面的法向量和面积 */
    for (Lv00Face f = 0; f < mesh->face_count; f++) {
        Lv00Halfedge start = mesh->face_he[f];
        Lv00Vertex v0 = mesh->he_vertex[start];
        Lv00Vertex v1 = mesh->he_vertex[mesh->he_next[start]];
        Lv00Vertex v2 = mesh->he_vertex[mesh->he_next[mesh->he_next[start]]];

        Lv00Point3D p0 = mesh->vertex_data[v0].position;
        Lv00Point3D p1 = mesh->vertex_data[v1].position;
        Lv00Point3D p2 = mesh->vertex_data[v2].position;

        Lv00Point3D a = vector_sub(p1, p0);
        Lv00Point3D b = vector_sub(p2, p0);
        Lv00Point3D cross = vector_cross(a, b);
        double area = sqrt(vector_dot(cross, cross)) / 2.0;

        mesh->face_data[f].normal.x = cross.x / (2 * area + 1e-10);
        mesh->face_data[f].normal.y = cross.y / (2 * area + 1e-10);
        mesh->face_data[f].normal.z = cross.z / (2 * area + 1e-10);
        mesh->face_data[f].area = area;
    }

    /* 更新顶点法向量 */
    for (Lv00Vertex v = 0; v < mesh->vertex_count; v++) {
        mesh->vertex_data[v].normal = lv00_he_mesh_vertex_normal(mesh, v);
        mesh->vertex_data[v].curvature = lv00_he_mesh_vertex_curvature(mesh, v);
    }

    mesh->operation_count++;
}

/* ========================================================================
 * 第七部分：网格查询
 * ======================================================================== */

Lv00Vertex lv00_he_mesh_nearest_vertex(const Lv00HeMesh *mesh, Lv00Point3D point, double *out_distance)
{
    if (!mesh || mesh->vertex_count == 0) {
        if (out_distance) *out_distance = DBL_MAX;
        return LV00_HE_INVALID;
    }

    double min_dist = DBL_MAX;
    Lv00Vertex nearest = 0;

    for (Lv00Vertex v = 0; v < mesh->vertex_count; v++) {
        Lv00Point3D p = mesh->vertex_data[v].position;
        double dx = p.x - point.x;
        double dy = p.y - point.y;
        double dz = p.z - point.z;
        double dist = dx * dx + dy * dy + dz * dz;

        if (dist < min_dist) {
            min_dist = dist;
            nearest = v;
        }
    }

    if (out_distance) *out_distance = sqrt(min_dist);
    return nearest;
}

Lv00Face lv00_he_mesh_point_in_face(const Lv00HeMesh *mesh, Lv00Point3D point, double *out_barycentric)
{
    /* 简化的实现：射线投射法 */
    if (!mesh || mesh->face_count == 0) return LV00_HE_INVALID;

    /* 投影到 XY 平面 */
    double px = point.x;
    double py = point.y;

    for (Lv00Face f = 0; f < mesh->face_count; f++) {
        Lv00Vertex verts[3];
        lv00_he_mesh_face_vertices(mesh, f, verts);

        double x1 = mesh->vertex_data[verts[0]].position.x;
        double y1 = mesh->vertex_data[verts[0]].position.y;
        double x2 = mesh->vertex_data[verts[1]].position.x;
        double y2 = mesh->vertex_data[verts[1]].position.y;
        double x3 = mesh->vertex_data[verts[2]].position.x;
        double y3 = mesh->vertex_data[verts[2]].position.y;

        /* 叉积符号法 */
        double d1 = (px - x2) * (y1 - y2) - (x1 - x2) * (py - y2);
        double d2 = (px - x3) * (y2 - y3) - (x2 - x3) * (py - y3);
        double d3 = (px - x1) * (y3 - y1) - (x3 - x1) * (py - y1);

        if ((d1 >= 0 && d2 >= 0 && d3 >= 0) || (d1 <= 0 && d2 <= 0 && d3 <= 0)) {
            /* 计算重心坐标 */
            double total_area = fabs((x2 - x1) * (y3 - y1) - (x3 - x1) * (y2 - y1));
            if (out_barycentric && total_area > 1e-10) {
                double a1 = ((y2 - y3) * (px - x3) + (x3 - x2) * (py - y3)) / total_area;
                double a2 = ((y3 - y1) * (px - x3) + (x1 - x3) * (py - y3)) / total_area;
                out_barycentric[0] = a1;
                out_barycentric[1] = a2;
                out_barycentric[2] = 1 - a1 - a2;
            }
            return f;
        }
    }

    return LV00_HE_INVALID;
}

double lv00_he_mesh_total_area(const Lv00HeMesh *mesh)
{
    if (!mesh) return 0;

    double total = 0;
    for (Lv00Face f = 0; f < mesh->face_count; f++) {
        total += mesh->face_data[f].area;
    }
    return total;
}

int lv00_he_mesh_euler_characteristic(const Lv00HeMesh *mesh)
{
    if (!mesh) return 0;
    return mesh->vertex_count - mesh->edge_count + mesh->face_count;
}

/* ========================================================================
 * 第八部分：统计
 * ======================================================================== */

void lv00_he_mesh_get_stats(const Lv00HeMesh *mesh, Lv00HeMeshStats *out_stats)
{
    if (!mesh || !out_stats) return;

    memset(out_stats, 0, sizeof(Lv00HeMeshStats));

    out_stats->vertex_count = mesh->vertex_count;
    out_stats->edge_count = mesh->edge_count;
    out_stats->halfedge_count = mesh->halfedge_count;
    out_stats->face_count = mesh->face_count;
    out_stats->total_area = lv00_he_mesh_total_area(mesh);
    out_stats->euler_characteristic = lv00_he_mesh_euler_characteristic(mesh);

    /* 简化：跳过顶点度数计算（依赖迭代器） */
    out_stats->max_vertex_valence = 0;

    /* 平均边长 */
    if (mesh->edge_count > 0) {
        double total_len = 0;
        for (Lv00Edge e = 0; e < mesh->edge_count; e++) {
            total_len += lv00_he_mesh_edge_length(mesh, e);
        }
        out_stats->average_edge_length = total_len / mesh->edge_count;
    }
}

bool lv00_he_mesh_validate(const Lv00HeMesh *mesh)
{
    if (!mesh) return false;

    /* 检查半边有效性 */
    for (Lv00Halfedge he = 0; he < mesh->halfedge_count; he++) {
        if (mesh->he_vertex[he] < 0 || mesh->he_vertex[he] >= mesh->vertex_count) {
            return false;
        }
        if (mesh->he_twin[he] < 0 || mesh->he_twin[he] >= mesh->halfedge_count) {
            return false;
        }
        /* 边界半边允许 he_next / he_prev 为 -1 */
        if (mesh->he_face[he] < 0) continue;
        if (mesh->he_next[he] < 0 || mesh->he_next[he] >= mesh->halfedge_count) {
            return false;
        }
        if (mesh->he_prev[he] < 0 || mesh->he_prev[he] >= mesh->halfedge_count) {
            return false;
        }
    }

    /* 检查面有效性 */
    for (Lv00Face f = 0; f < mesh->face_count; f++) {
        Lv00Halfedge he = mesh->face_he[f];
        if (he < 0 || he >= mesh->halfedge_count) {
            return false;
        }
    }

    return true;
}

/* ── _mesh_ 前缀迭代器（委托给短名实现）── */

Lv00HeVertexIterator lv00_he_mesh_vertex_iter_begin(Lv00HeMesh *mesh, int flags) {
    (void)flags;
    return lv00_he_vertex_iter_begin(mesh, 0);
}

Lv00HeVertexIterator lv00_he_mesh_vertex_out_iter_begin(Lv00HeMesh *mesh, Lv00Vertex v) {
    return lv00_he_vertex_iter_begin(mesh, v);
}

Lv00Vertex lv00_he_mesh_vertex_out_iter_next(Lv00HeVertexIterator *iter) {
    lv00_he_vertex_iter_next(iter);
    return iter->current;
}

bool lv00_he_mesh_vertex_iter_next(Lv00HeVertexIterator *iter) {
    lv00_he_vertex_iter_next(iter);
    return iter->current != LV00_HE_INVALID;
}

Lv00HeFaceIterator lv00_he_mesh_face_iter_begin(Lv00HeMesh *mesh, int flags) {
    (void)flags;
    return lv00_he_face_iter_begin(mesh, 0);
}

bool lv00_he_mesh_face_iter_next(Lv00HeFaceIterator *iter) {
    lv00_he_face_iter_next(iter);
    return iter->current != LV00_HE_INVALID;
}