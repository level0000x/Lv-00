/**
 * @file geo_halfedge_mesh.c
 * @brief Halfedge 网格拓扑数据结构实现
 *
 * @details 实现策略：
 *   - 使用结构化数组存储拓扑关系
 *   - 半边自动配对（twin）构建
 *   - 面迭代使用半边环绕遍历
 *   - 支持三角面/四边面添加、邻接查询和几何量计算
 *
 * @author Lv-00 Project
 * @version v3.6.0
 */

#include "lv/lv_platform.h"

#include "lv/geo_halfedge_mesh.h"
#include "lv/lv_lifecycle.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/geo_utils.h"

#ifndef lv_PUBLIC_API
#define lv_PUBLIC_API
#endif

/* ========================================================================
 * 内部常量
 * ======================================================================== */

#define INITIAL_CAPACITY 64

/* ========================================================================
 * 第一部分：默认配置与创建释放
 * ======================================================================== */

/**
 * @brief 获取默认 Halfedge 网格配置
 * @return 默认配置（初始容量 64，维护法向量，不维护曲率）
 */
lvHeMeshConfig lv_he_mesh_default_config(void) {
    lvHeMeshConfig cfg;
    cfg.initial_capacity = INITIAL_CAPACITY;
    cfg.max_faces_per_edge = 2;
    cfg.maintain_normals = true;
    cfg.maintain_curvature = false;
    return cfg;
}

/**
 * @brief 确保顶点数组有足够容量，必要时扩容
 * @param mesh 网格指针
 * @return 成功返回 true，失败返回 false
 */
static bool ensure_capacity(lvHeMesh *mesh) {
    /* 双数组联动扩容：capacity 回退技巧保持两数组容量一致（同 proof_priority 模式）；
     * 倍增策略/溢出检查/失败语义统一委托 lv_ensure_capacity。
     * 注：旧实现的 INITIAL_CAPACITY 下限钳制在初始容量=64 后不可达，等价删除。 */
    int old_cap = mesh->vertex_capacity;
    if (!lv_ensure_capacity((void **) &mesh->vertex_data, old_cap,
                            &mesh->vertex_capacity, sizeof(lvVertexData), 1))
        return false;

    mesh->vertex_capacity = old_cap;
    if (!lv_ensure_capacity((void **) &mesh->vertex_out_he, old_cap,
                            &mesh->vertex_capacity, sizeof(lvHalfedge), 1))
        return false;

    return true;
}

/**
 * @brief 创建 Halfedge 网格
 * @param config 配置指针（可为 NULL，使用默认配置）
 * @return 新网格（调用者通过 lv_he_mesh_destroy 释放），失败返回 NULL
 */
lvHeMesh *lv_he_mesh_create(const lvHeMeshConfig *config) {
    lvHeMesh *mesh = (lvHeMesh *) lv_calloc(1, sizeof(lvHeMesh));
    if (!mesh)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "lv_he_mesh_create: calloc mesh failed");

    if (config) {
        mesh->config = *config;
    } else {
        mesh->config = lv_he_mesh_default_config();
    }

    mesh->vertex_capacity = INITIAL_CAPACITY;
    mesh->halfedge_capacity = INITIAL_CAPACITY * 6;
    mesh->edge_capacity = INITIAL_CAPACITY * 3;
    mesh->face_capacity = INITIAL_CAPACITY * 2;

    mesh->vertex_data = (lvVertexData *) lv_calloc(mesh->vertex_capacity, sizeof(lvVertexData));
    mesh->vertex_out_he = (lvHalfedge *) lv_calloc(mesh->vertex_capacity, sizeof(lvHalfedge));

    mesh->he_twin = (lvHalfedge *) lv_malloc(mesh->halfedge_capacity * sizeof(lvHalfedge));
    mesh->he_next = (lvHalfedge *) lv_malloc(mesh->halfedge_capacity * sizeof(lvHalfedge));
    mesh->he_prev = (lvHalfedge *) lv_malloc(mesh->halfedge_capacity * sizeof(lvHalfedge));
    mesh->he_face = (lvFace *) lv_malloc(mesh->halfedge_capacity * sizeof(lvFace));
    mesh->he_vertex = (lvVertex *) lv_malloc(mesh->halfedge_capacity * sizeof(lvVertex));
    mesh->he_data = (lvHalfedgeData *) lv_calloc(mesh->halfedge_capacity, sizeof(lvHalfedgeData));

    mesh->edge_he = (lvHalfedge *) lv_malloc(mesh->edge_capacity * sizeof(lvHalfedge));

    mesh->face_he = (lvHalfedge *) lv_malloc(mesh->face_capacity * sizeof(lvHalfedge));
    mesh->face_data = (lvFaceData *) lv_calloc(mesh->face_capacity, sizeof(lvFaceData));

    if (!mesh->vertex_data || !mesh->vertex_out_he || !mesh->he_twin || !mesh->he_next || !mesh->he_prev ||
        !mesh->he_face || !mesh->he_vertex || !mesh->edge_he || !mesh->face_he) {
        lv_he_mesh_destroy(mesh);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "lv_he_mesh_create: malloc internal arrays failed");
    }

    /* 初始化为 INVALID */
    for (int i = 0; i < mesh->vertex_capacity; i++) {
        mesh->vertex_out_he[i] = lv_HE_INVALID;
    }
    for (int i = 0; i < mesh->halfedge_capacity; i++) {
        mesh->he_twin[i] = lv_HE_INVALID;
        mesh->he_face[i] = lv_HE_INVALID;
        mesh->he_vertex[i] = lv_HE_INVALID;
    }

    return mesh;
}

/**
 * @brief 销毁 Halfedge 网格并释放所有资源
 * @param mesh 网格指针（可为 NULL）
 */
/* lv_he_mesh_destroy 字段描述表：11 个纯指针字段直接释放 */
static const lvFieldDesc s_he_mesh_destroy_fields[] = {
    lv_FIELD_PLAIN(lvHeMesh, vertex_data),
    lv_FIELD_PLAIN(lvHeMesh, vertex_out_he),
    lv_FIELD_PLAIN(lvHeMesh, he_twin),
    lv_FIELD_PLAIN(lvHeMesh, he_next),
    lv_FIELD_PLAIN(lvHeMesh, he_prev),
    lv_FIELD_PLAIN(lvHeMesh, he_face),
    lv_FIELD_PLAIN(lvHeMesh, he_vertex),
    lv_FIELD_PLAIN(lvHeMesh, he_data),
    lv_FIELD_PLAIN(lvHeMesh, edge_he),
    lv_FIELD_PLAIN(lvHeMesh, face_he),
    lv_FIELD_PLAIN(lvHeMesh, face_data),
};

void lv_he_mesh_destroy(lvHeMesh *mesh) {
    if (!mesh)
        return;

    lv_obj_destroy_fields(mesh, s_he_mesh_destroy_fields,
                          sizeof(s_he_mesh_destroy_fields) / sizeof(s_he_mesh_destroy_fields[0]));
    lv_free((void **) &mesh);
}

/**
 * @brief 清空网格所有拓扑数据（保留容量）
 * @param mesh 网格指针
 */
void lv_he_mesh_clear(lvHeMesh *mesh) {
    if (!mesh)
        return;

    mesh->vertex_count = 0;
    mesh->halfedge_count = 0;
    mesh->edge_count = 0;
    mesh->face_count = 0;

    for (int i = 0; i < mesh->vertex_capacity; i++) {
        mesh->vertex_out_he[i] = lv_HE_INVALID;
    }
    for (int i = 0; i < mesh->halfedge_capacity; i++) {
        mesh->he_twin[i] = lv_HE_INVALID;
        mesh->he_face[i] = lv_HE_INVALID;
        mesh->he_vertex[i] = lv_HE_INVALID;
    }
}

/* ========================================================================
 * 第二部分：顶点和半边操作
 * ======================================================================== */

/**
 * @brief 添加顶点
 * @param mesh 网格指针
 * @param x, y, z  三维坐标
 * @return 顶点索引，失败返回 lv_HE_INVALID
 */
lvVertex lv_he_mesh_add_vertex(lvHeMesh *mesh, double x, double y, double z) {
    if (!mesh)
        return lv_HE_INVALID;

    if (mesh->vertex_count >= mesh->vertex_capacity) {
        if (!ensure_capacity(mesh))
            return lv_HE_INVALID;
    }

    lvVertex v = mesh->vertex_count++;
    mesh->vertex_data[v].position.x = x;
    mesh->vertex_data[v].position.y = y;
    mesh->vertex_data[v].position.z = z;
    mesh->vertex_data[v].normal.x = 0;
    mesh->vertex_data[v].normal.y = 0;
    mesh->vertex_data[v].normal.z = 1;
    mesh->vertex_data[v].curvature = 0;
    mesh->vertex_data[v].weight = 1.0;
    mesh->vertex_out_he[v] = lv_HE_INVALID;

    return v;
}

/**
 * @brief 获取顶点位置
 * @param mesh 网格指针
 * @param v    顶点索引
 * @return 三维坐标
 */
lvPoint3D lv_he_mesh_get_vertex_position(const lvHeMesh *mesh, lvVertex v) {
    lvPoint3D p = {0, 0, 0};
    if (mesh && v >= 0 && v < mesh->vertex_count) {
        p = mesh->vertex_data[v].position;
    }
    return p;
}

/**
 * @brief 设置顶点位置
 * @param mesh 网格指针
 * @param v    顶点索引
 * @param pos  新坐标
 */
void lv_he_mesh_set_vertex_position(lvHeMesh *mesh, lvVertex v, lvPoint3D pos) {
    if (mesh && v >= 0 && v < mesh->vertex_count) {
        mesh->vertex_data[v].position = pos;
    }
}

/**
 * @brief 获取顶点的出半边
 * @param mesh 网格指针
 * @param v    顶点索引
 * @return 半边索引，无效返回 lv_HE_INVALID
 */
lvHalfedge lv_he_mesh_vertex_out_halfedge(const lvHeMesh *mesh, lvVertex v) {
    if (mesh && v >= 0 && v < mesh->vertex_count) {
        return mesh->vertex_out_he[v];
    }
    return lv_HE_INVALID;
}

/**
 * @brief 获取半边的起点顶点
 * @param mesh 网格指针
 * @param he   半边索引
 * @return 顶点索引，无效返回 lv_HE_INVALID
 */
lvVertex lv_he_mesh_halfedge_vertex(const lvHeMesh *mesh, lvHalfedge he) {
    if (mesh && he >= 0 && he < mesh->halfedge_count) {
        return mesh->he_vertex[he];
    }
    return lv_HE_INVALID;
}

/**
 * @brief 获取半边的孪生半边
 * @param mesh 网格指针
 * @param he   半边索引
 * @return 孪生半边索引，无效返回 lv_HE_INVALID
 */
lvHalfedge lv_he_mesh_halfedge_twin(const lvHeMesh *mesh, lvHalfedge he) {
    if (mesh && he >= 0 && he < mesh->halfedge_count) {
        return mesh->he_twin[he];
    }
    return lv_HE_INVALID;
}

/**
 * @brief 获取半边的下一条半边
 * @param mesh 网格指针
 * @param he   半边索引
 * @return 下一条半边索引，无效返回 lv_HE_INVALID
 */
lvHalfedge lv_he_mesh_halfedge_next(const lvHeMesh *mesh, lvHalfedge he) {
    if (mesh && he >= 0 && he < mesh->halfedge_count) {
        return mesh->he_next[he];
    }
    return lv_HE_INVALID;
}

/**
 * @brief 获取半边所属的面
 * @param mesh 网格指针
 * @param he   半边索引
 * @return 面索引，无效返回 lv_HE_INVALID
 */
lvFace lv_he_mesh_halfedge_face(const lvHeMesh *mesh, lvHalfedge he) {
    if (mesh && he >= 0 && he < mesh->halfedge_count) {
        return mesh->he_face[he];
    }
    return lv_HE_INVALID;
}

/* ========================================================================
 * 第三部分：边操作
 * ======================================================================== */

static lvEdge find_or_create_edge(lvHeMesh *mesh, lvVertex v1, lvVertex v2) {
    /* 完整实现：先沿 v1 的出半边环局部查找（O(valence)），
     * 再回退全边扫描（对边界/破损网格鲁棒，与 lv_he_vertex_iter_begin
     * 同款扫描兜底语义）。 */
    lvHalfedge out = mesh->vertex_out_he[v1];
    if (out >= 0 && out < mesh->halfedge_count) {
        /* 出半边环：he_twin→he_next 旋转直到回到起点 */
        lvHalfedge he = out;
        int guard = 0;
        while (he >= 0 && he < mesh->halfedge_count && guard++ < mesh->halfedge_count) {
            lvHalfedge twin = mesh->he_twin[he];
            if (twin >= 0 && twin < mesh->halfedge_count && mesh->he_vertex[twin] == v2) {
                /* 找到 (v1→v2) 对应半边；沿环回退定位其边索引 */
                for (lvEdge e = 0; e < mesh->edge_count; e++) {
                    if (mesh->edge_he[e] == he || mesh->edge_he[e] == twin)
                        return e;
                }
                return lv_HE_INVALID;
            }
            he = mesh->he_next[twin];
            if (he == out)
                break;
        }
    }

    /* 回退：全边扫描（含顶点出半边未建立/环不完整场景） */
    for (lvEdge e = 0; e < mesh->edge_count; e++) {
        lvHalfedge he = mesh->edge_he[e];
        if (he < 0 || he >= mesh->halfedge_count)
            continue;
        lvHalfedge twin = mesh->he_twin[he];
        if (twin < 0 || twin >= mesh->halfedge_count)
            continue;
        lvVertex ev1 = mesh->he_vertex[he];
        lvVertex ev2 = mesh->he_vertex[twin];
        if ((ev1 == v1 && ev2 == v2) || (ev1 == v2 && ev2 == v1)) {
            return e;
        }
    }
    return lv_HE_INVALID;
}

lvEdge lv_he_mesh_find_edge(const lvHeMesh *mesh, lvVertex v1, lvVertex v2) {
    if (!mesh || v1 == v2)
        return lv_HE_INVALID;
    return find_or_create_edge((lvHeMesh *) mesh, v1, v2);
}

lvHalfedge lv_he_mesh_edge_halfedge(const lvHeMesh *mesh, lvEdge e) {
    if (mesh && e >= 0 && e < mesh->edge_count) {
        return mesh->edge_he[e];
    }
    return lv_HE_INVALID;
}

double lv_he_mesh_edge_length(const lvHeMesh *mesh, lvEdge e) {
    if (!mesh || e < 0 || e >= mesh->edge_count)
        return 0;

    lvHalfedge he = mesh->edge_he[e];
    lvVertex v1 = mesh->he_vertex[he];
    lvVertex v2 = mesh->he_vertex[mesh->he_twin[he]];

    lvPoint3D p1 = mesh->vertex_data[v1].position;
    lvPoint3D p2 = mesh->vertex_data[v2].position;

    return geo_distance_3d(p1.x, p1.y, p1.z, p2.x, p2.y, p2.z);
}

void lv_he_mesh_edge_vertices(const lvHeMesh *mesh, lvEdge e, lvVertex *out_v1, lvVertex *out_v2) {
    if (!mesh || e < 0 || e >= mesh->edge_count)
        return;

    lvHalfedge he = mesh->edge_he[e];
    /* [安全] 防止 out_v1/out_v2 为空指针 */
    if (he < 0 || he >= mesh->halfedge_count)
        return;
    if (mesh->he_twin[he] < 0 || mesh->he_twin[he] >= mesh->halfedge_count)
        return;
    if (out_v1)
        *out_v1 = mesh->he_vertex[he];
    if (out_v2)
        *out_v2 = mesh->he_vertex[mesh->he_twin[he]];
}

/* ========================================================================
 * 第四部分：面操作
 * ======================================================================== */

/**
 * @brief 同步扩容 5 个共享 halfedge_capacity 的平行数组（事务语义）
 *
 * 5 个数组共享同一容量，必须同步扩容。任一数组扩容失败时，
 * 已成功扩容的新块会被释放，所有数组保持原状（旧指针仍有效）。
 *
 * @param mesh 网格
 * @return true 扩容成功，false 失败（数组保持原状）
 */
static bool he_mesh_grow_halfedge_arrays(lvHeMesh *mesh) {
    /* [安全] 防止整数溢出（与 lv_ensure_capacity 内部检查一致） */
    if (mesh->halfedge_capacity > INT_MAX / 2)
        return false;

    /* 各数组独立持有容量变量，以相同参数调用 lv_ensure_capacity，
     * 保证所有数组同步扩容到相同的新容量 */
    int cap_twin = mesh->halfedge_capacity;
    int cap_next = mesh->halfedge_capacity;
    int cap_prev = mesh->halfedge_capacity;
    int cap_face = mesh->halfedge_capacity;
    int cap_vertex = mesh->halfedge_capacity;
    lvHalfedge *new_twin = mesh->he_twin;
    lvHalfedge *new_next = mesh->he_next;
    lvHalfedge *new_prev = mesh->he_prev;
    lvFace *new_face = mesh->he_face;
    lvVertex *new_vertex = mesh->he_vertex;

    /* [安全] 先全部扩容，任一失败则回滚整个操作 */
    bool ok = lv_ensure_capacity((void **) &new_twin, mesh->halfedge_count + 1, &cap_twin, sizeof(lvHalfedge), 1) &&
              lv_ensure_capacity((void **) &new_next, mesh->halfedge_count + 1, &cap_next, sizeof(lvHalfedge), 1) &&
              lv_ensure_capacity((void **) &new_prev, mesh->halfedge_count + 1, &cap_prev, sizeof(lvHalfedge), 1) &&
              lv_ensure_capacity((void **) &new_face, mesh->halfedge_count + 1, &cap_face, sizeof(lvFace), 1) &&
              lv_ensure_capacity((void **) &new_vertex, mesh->halfedge_count + 1, &cap_vertex, sizeof(lvVertex), 1);
    if (!ok) {
        /* [安全] 任一失败：仅释放已成功扩容的新块（失败时 new_* 仍指向旧数组，跳过）；
         * 旧数组未被覆盖，仍由 mesh 持有 */
        if (new_twin != mesh->he_twin)
            lv_free((void **) &new_twin);
        if (new_next != mesh->he_next)
            lv_free((void **) &new_next);
        if (new_prev != mesh->he_prev)
            lv_free((void **) &new_prev);
        if (new_face != mesh->he_face)
            lv_free((void **) &new_face);
        if (new_vertex != mesh->he_vertex)
            lv_free((void **) &new_vertex);
        return false;
    }

    mesh->he_twin = new_twin;
    mesh->he_next = new_next;
    mesh->he_prev = new_prev;
    mesh->he_face = new_face;
    mesh->he_vertex = new_vertex;
    mesh->halfedge_capacity = cap_twin;
    return true;
}

static lvHalfedge add_halfedge_pair(lvHeMesh *mesh, lvVertex v1, lvVertex v2) {
    /* 检查是否已存在 */
    for (lvEdge e = 0; e < mesh->edge_count; e++) {
        lvHalfedge existing = mesh->edge_he[e];
        lvVertex ev1 = mesh->he_vertex[existing];
        lvVertex ev2 = mesh->he_vertex[mesh->he_twin[existing]];

        if ((ev1 == v1 && ev2 == v2) || (ev1 == v2 && ev2 == v1)) {
            /* 边已存在 */
            if (mesh->he_twin[existing] != lv_HE_INVALID) {
                /* 边已配对：返回请求方向的半边（existing 或其孪生）。
                 * [修复] 原实现无条件返回 existing，当请求方向与存储方向相反时
                 * （如共享边 v2->v0 已存、请求 v0->v2），返回方向错误的半边，
                 * 导致两个面环交叉覆盖 he_next，拓扑损坏。 */
                if (ev1 == v1 && ev2 == v2)
                    return existing;
                return mesh->he_twin[existing];
            }
        }
    }

    /* 创建新边 */
    if (mesh->edge_count >= mesh->edge_capacity) {
        if (!lv_ensure_capacity((void **) &mesh->edge_he, mesh->edge_count, &mesh->edge_capacity,
                                sizeof(lvHalfedge), 1))
            return lv_HE_INVALID;
    }

    lvEdge new_edge = mesh->edge_count++;

    /* 创建两个半边 */
    if (mesh->halfedge_count + 1 >= mesh->halfedge_capacity) {
        /* [安全] 5 个平行数组同步扩容，任一失败则回滚整个操作 */
        if (!he_mesh_grow_halfedge_arrays(mesh)) {
            mesh->edge_count--;
            return lv_HE_INVALID;
        }
    }

    lvHalfedge he1 = mesh->halfedge_count++;
    lvHalfedge he2 = mesh->halfedge_count++;

    /* 设置半边数据 */
    mesh->he_vertex[he1] = v1;
    mesh->he_vertex[he2] = v2;
    mesh->he_twin[he1] = he2;
    mesh->he_twin[he2] = he1;
    mesh->he_face[he1] = lv_HE_INVALID;
    mesh->he_face[he2] = lv_HE_INVALID;
    mesh->he_next[he1] = lv_HE_INVALID;
    mesh->he_next[he2] = lv_HE_INVALID;
    mesh->he_prev[he1] = lv_HE_INVALID;
    mesh->he_prev[he2] = lv_HE_INVALID;

    /* 更新顶点的 outgoing halfedge */
    if (mesh->vertex_out_he[v1] == lv_HE_INVALID) {
        mesh->vertex_out_he[v1] = he1;
    }
    if (mesh->vertex_out_he[v2] == lv_HE_INVALID) {
        mesh->vertex_out_he[v2] = he2;
    }

    /* 记录边 */
    mesh->edge_he[new_edge] = he1;

    return he1;
}

lvFace lv_he_mesh_add_face_triangle(lvHeMesh *mesh, lvVertex v1, lvVertex v2, lvVertex v3) {
    /* 委托通用面构造（3 边形）；退化检查由 add_face 的相邻重复检测覆盖 */
    const int idx[3] = {v1, v2, v3};
    return lv_he_mesh_add_face(mesh, idx, 3);
}

lvFace lv_he_mesh_add_face_quad(lvHeMesh *mesh, lvVertex v1, lvVertex v2, lvVertex v3, lvVertex v4) {
    /* 简化为两个三角形 */
    lvFace f1 = lv_he_mesh_add_face_triangle(mesh, v1, v2, v3);
    if (f1 == lv_HE_INVALID)
        return lv_HE_INVALID;

    lvFace f2 = lv_he_mesh_add_face_triangle(mesh, v1, v3, v4);
    if (f2 == lv_HE_INVALID)
        return f1; /* 返回第一个面作为近似 */

    return f1;
}

/**
 * @brief 按顶点索引数组添加任意多边形面（3..16 边形）
 *
 * 通用面构造：对每个相邻顶点对复用 add_halfedge_pair 建边，
 * 面法线/面积用扇形三角化（v0, vi, vi+1）累加。
 */
lvFace lv_he_mesh_add_face(lvHeMesh *mesh, const int *indices, int count) {
    if (!mesh || !indices || count < 3 || count > 16)
        return lv_HE_INVALID;
    for (int i = 0; i < count; i++) {
        if (indices[i] < 0 || indices[i] >= mesh->vertex_count)
            return lv_HE_INVALID;
        /* 相邻顶点退化（含首尾闭环）检查 */
        if (indices[i] == indices[(i + 1) % count])
            return lv_HE_INVALID;
    }

    /* 创建 count 条边 */
    lvHalfedge hes[16];
    for (int i = 0; i < count; i++) {
        hes[i] = add_halfedge_pair(mesh, indices[i], indices[(i + 1) % count]);
        if (hes[i] == lv_HE_INVALID)
            return lv_HE_INVALID;
    }

    /* 创建面（与 triangle 相同扩容模式） */
    if (mesh->face_count >= mesh->face_capacity) {
        int cap_he = mesh->face_capacity;
        int cap_data = mesh->face_capacity;
        lvHalfedge *new_face_he = mesh->face_he;
        lvFaceData *new_face_data = mesh->face_data;
        bool ok = lv_ensure_capacity((void **) &new_face_he, mesh->face_count, &cap_he, sizeof(lvHalfedge), 1) &&
                  lv_ensure_capacity((void **) &new_face_data, mesh->face_count, &cap_data, sizeof(lvFaceData), 1);
        if (!ok) {
            if (new_face_he != mesh->face_he)
                lv_free((void **) &new_face_he);
            if (new_face_data != mesh->face_data)
                lv_free((void **) &new_face_data);
            return lv_HE_INVALID;
        }
        mesh->face_he = new_face_he;
        mesh->face_data = new_face_data;
        mesh->face_capacity = cap_he;
    }

    lvFace f = mesh->face_count++;
    mesh->face_he[f] = hes[0];
    for (int i = 0; i < count; i++) {
        mesh->he_face[hes[i]] = f;
        mesh->he_next[hes[i]] = hes[(i + 1) % count];
        mesh->he_prev[hes[i]] = hes[(i + count - 1) % count];
    }

    /* 法线/面积：扇形三角化（v0, vi, vi+1）累加 */
    lvPoint3D p0 = mesh->vertex_data[indices[0]].position;
    double nx = 0, ny = 0, nz = 0, area_sum = 0;
    for (int i = 1; i + 1 < count; i++) {
        lvPoint3D p1 = mesh->vertex_data[indices[i]].position;
        lvPoint3D p2 = mesh->vertex_data[indices[i + 1]].position;
        double ax = p1.x - p0.x, ay = p1.y - p0.y, az = p1.z - p0.z;
        double bx = p2.x - p0.x, by = p2.y - p0.y, bz = p2.z - p0.z;
        double cx = ay * bz - az * by;
        double cy = az * bx - ax * bz;
        double cz = ax * by - ay * bx;
        nx += cx;
        ny += cy;
        nz += cz;
        area_sum += sqrt(cx * cx + cy * cy + cz * cz) / 2.0;
    }
    double len = sqrt(nx * nx + ny * ny + nz * nz);
    if (len > lv_EPSILON_ULTRA) {
        mesh->face_data[f].normal.x = nx / len;
        mesh->face_data[f].normal.y = ny / len;
        mesh->face_data[f].normal.z = nz / len;
    } else {
        mesh->face_data[f].normal.x = 0.0;
        mesh->face_data[f].normal.y = 0.0;
        mesh->face_data[f].normal.z = 1.0;
    }
    mesh->face_data[f].area = area_sum;
    mesh->face_data[f].valence = count;

    return f;
}

lvHalfedge lv_he_mesh_face_halfedge(const lvHeMesh *mesh, lvFace f) {
    if (mesh && f >= 0 && f < mesh->face_count) {
        return mesh->face_he[f];
    }
    return lv_HE_INVALID;
}

lvPoint3D lv_he_mesh_face_normal(const lvHeMesh *mesh, lvFace f) {
    lvPoint3D n = {0, 0, 0};
    if (mesh && f >= 0 && f < mesh->face_count) {
        n = mesh->face_data[f].normal;
    }
    return n;
}

double lv_he_mesh_face_area(const lvHeMesh *mesh, lvFace f) {
    if (mesh && f >= 0 && f < mesh->face_count) {
        return mesh->face_data[f].area;
    }
    return 0;
}

int lv_he_mesh_face_valence(const lvHeMesh *mesh, lvFace f) {
    if (mesh && f >= 0 && f < mesh->face_count) {
        return mesh->face_data[f].valence;
    }
    return 0;
}

int lv_he_mesh_face_vertices(const lvHeMesh *mesh, lvFace f, lvVertex *out_vertices) {
    if (!mesh || f < 0 || f >= mesh->face_count || !out_vertices) {
        return 0;
    }

    lvHalfedge start = mesh->face_he[f];
    lvHalfedge current = start;
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

lvHeVertexIterator lv_he_vertex_iter_begin(const lvHeMesh *mesh, lvVertex v) {
    lvHeVertexIterator iter;
    iter.mesh = mesh;
    iter.index = 0;
    iter.count = 0;
    iter.current = lv_HE_INVALID;

    if (!mesh || v < 0 || v >= mesh->vertex_count)
        return iter;

    /* 扫描式实现：遍历以 v 为起点的所有半边。
     * [修复] 原实现依赖 twin→next 拓扑链计算 count 并推进，但边界网格中
     * 边界半边的 next 未链成环，导致迭代在第一个边界半边处提前终止
     * （旧测试刻意跳过依赖迭代器的几何量，未暴露）。扫描式不依赖拓扑链，
     * 对任意网格（含边界/破损）均鲁棒。 */
    for (lvHalfedge he = 0; he < mesh->halfedge_count; he++) {
        if (mesh->he_vertex[he] == v)
            iter.count++;
    }
    for (lvHalfedge he = 0; he < mesh->halfedge_count; he++) {
        if (mesh->he_vertex[he] == v) {
            iter.current = he;
            break;
        }
    }
    return iter;
}

lvHalfedge lv_he_vertex_iter_get(const lvHeVertexIterator *iter) {
    if (!iter || !iter->mesh || iter->current == lv_HE_INVALID) {
        return lv_HE_INVALID;
    }
    return iter->mesh->he_twin[iter->current];
}

bool lv_he_vertex_iter_valid(const lvHeVertexIterator *iter) {
    if (!iter)
        return false;
    return iter->current != lv_HE_INVALID && iter->index < iter->count;
}

void lv_he_vertex_iter_next(lvHeVertexIterator *iter) {
    if (!iter || !iter->mesh || iter->current == lv_HE_INVALID)
        return;

    lvVertex v = iter->mesh->he_vertex[iter->current];
    iter->index++;
    iter->current = lv_HE_INVALID;

    /* 定位第 index 个以 v 为起点的半边 */
    int seen = 0;
    for (lvHalfedge he = 0; he < iter->mesh->halfedge_count; he++) {
        if (iter->mesh->he_vertex[he] == v) {
            if (seen == iter->index) {
                iter->current = he;
                break;
            }
            seen++;
        }
    }
}

lvHeFaceIterator lv_he_face_iter_begin(const lvHeMesh *mesh, lvFace f) {
    lvHeFaceIterator iter;
    iter.mesh = mesh;
    iter.current = (mesh && f >= 0 && f < mesh->face_count) ? mesh->face_he[f] : lv_HE_INVALID;
    iter.count = (iter.current != lv_HE_INVALID) ? mesh->face_data[f].valence : 0;
    iter.index = 0;
    return iter;
}

lvHalfedge lv_he_face_iter_get(const lvHeFaceIterator *iter) {
    if (!iter || !iter->mesh || iter->current == lv_HE_INVALID) {
        return lv_HE_INVALID;
    }
    return iter->current;
}

bool lv_he_face_iter_valid(const lvHeFaceIterator *iter) {
    if (!iter)
        return false;
    return iter->current != lv_HE_INVALID && iter->index < iter->count;
}

void lv_he_face_iter_next(lvHeFaceIterator *iter) {
    if (!iter || !iter->mesh || iter->current == lv_HE_INVALID)
        return;

    iter->current = iter->mesh->he_next[iter->current];
    iter->index++;

    if (iter->index >= iter->count) {
        iter->current = lv_HE_INVALID;
    }
}

/* ========================================================================
 * 第六部分：几何量计算
 * ========================================================================
 * 向量运算（dot/cross/sub）已收敛到公共内联 lv_vec3_dot/lv_vec3_cross/
 * lv_vec3_sub（core/include/lv/lv_vec3.h），原本地 static vector_* 已删除；
 * lvPoint3D 即 lvVec3，double 运算逐位一致。 */

double lv_he_mesh_vertex_angle(const lvHeMesh *mesh, lvVertex v) {
    if (!mesh || v < 0 || v >= mesh->vertex_count)
        return 0;

    double total_angle = 0;
    int count = 0;

    /* 遍历 v 的每条入边 he（he 终点为 v）：he 所在面内 v 的内角由
     * he 与 he_next[he] 张成（向量 pos(twin(he)起点)-pos(v) 与
     * pos(he_next[he]终点)-pos(v)）。
     * [修复] 原实现取 he_vertex[twin(prev_he)]，在 any 网格下恒等于
     * he 自身终点（twin 反转起点终点），导致 p0==p2 恒成立、角恒为 0；
     * 旧测试刻意跳过该函数未暴露。 */
    lvHeVertexIterator it;
    lv_HE_ITER_VERTEX_OUT_HALFEDGES(mesh, v, it) {
        lvHalfedge he = lv_he_vertex_iter_get(&it); /* 入边，终点 v */
        if (he < 0 || he >= mesh->halfedge_count || mesh->he_face[he] < 0)
            continue; /* 边界入边不构成面角 */
        lvHalfedge nxt = mesh->he_next[he];
        if (nxt < 0 || nxt >= mesh->halfedge_count)
            continue;
        lvVertex va = mesh->he_vertex[he];                      /* 入边起点（邻居 A） */
        lvVertex vb = mesh->he_vertex[mesh->he_twin[nxt]];      /* nxt 终点（邻居 B） */

        lvPoint3D pa = mesh->vertex_data[va].position;
        lvPoint3D pv = mesh->vertex_data[v].position;
        lvPoint3D pb = mesh->vertex_data[vb].position;

        lvPoint3D a = lv_vec3_sub(pa, pv);
        lvPoint3D b = lv_vec3_sub(pb, pv);

        double dot = lv_vec3_dot(a, b);
        double len_a = sqrt(lv_vec3_dot(a, a));
        double len_b = sqrt(lv_vec3_dot(b, b));

        if (len_a > 0 && len_b > 0) {
            double cos_angle = dot / (len_a * len_b);
            if (cos_angle > 1)
                cos_angle = 1;
            if (cos_angle < -1)
                cos_angle = -1;
            total_angle += acos(cos_angle);
            count++;
        }
    }

    return (count > 0) ? total_angle : 0;
}

double lv_he_mesh_vertex_curvature(const lvHeMesh *mesh, lvVertex v) {
    if (!mesh || v < 0 || v >= mesh->vertex_count)
        return 0;

    /* 简化的离散曲率：2π - 邻接角和 */
    double angle_sum = lv_he_mesh_vertex_angle(mesh, v);
    return lv_TWO_PI - angle_sum;
}

lvPoint3D lv_he_mesh_vertex_normal(const lvHeMesh *mesh, lvVertex v) {
    lvPoint3D n = {0, 0, 0};
    if (!mesh || v < 0 || v >= mesh->vertex_count)
        return n;

    double total_area = 0;

    /* 遍历 v 的每条入边 he：he 所在面内三角形 (v, twin(he)起点?, ...)
     * 实际取 he 起点 va 与 nxt 终点 vb 构成面三角形（同 vertex_angle）。 */
    lvHeVertexIterator it;
    lv_HE_ITER_VERTEX_OUT_HALFEDGES(mesh, v, it) {
        lvHalfedge he = lv_he_vertex_iter_get(&it); /* 入边，终点 v */
        if (he < 0 || he >= mesh->halfedge_count || mesh->he_face[he] < 0)
            continue; /* 边界入边不构成面 */
        lvHalfedge nxt = mesh->he_next[he];
        if (nxt < 0 || nxt >= mesh->halfedge_count)
            continue;
        lvVertex va = mesh->he_vertex[he];                  /* 入边起点（邻居 A） */
        lvVertex vb = mesh->he_vertex[mesh->he_twin[nxt]];  /* nxt 终点（邻居 B） */

        lvPoint3D pa = mesh->vertex_data[va].position;
        lvPoint3D pv = mesh->vertex_data[v].position;
        lvPoint3D pb = mesh->vertex_data[vb].position;

        lvPoint3D a = lv_vec3_sub(pa, pv);
        lvPoint3D b = lv_vec3_sub(pb, pv);
        /* 面内三角形 (v, vb, va) 顺序保证法线朝面外（b × a） */
        lvPoint3D cross = lv_vec3_cross(b, a);
        double area = sqrt(lv_vec3_dot(cross, cross)) / 2.0;

        n.x += cross.x;
        n.y += cross.y;
        n.z += cross.z;
        total_area += area;
    }

    if (total_area > 0) {
        double len = sqrt(lv_vec3_dot(n, n));
        if (len > 0) {
            n.x /= len;
            n.y /= len;
            n.z /= len;
        }
    }

    return n;
}

double lv_he_mesh_halfedge_angle(const lvHeMesh *mesh, lvHalfedge he1, lvHalfedge he2) {
    if (!mesh || he1 == lv_HE_INVALID || he2 == lv_HE_INVALID)
        return 0;

    lvVertex v = mesh->he_vertex[he1];
    lvVertex v0 = mesh->he_vertex[mesh->he_twin[he1]];
    lvVertex v2 = mesh->he_vertex[he2];

    lvPoint3D p0 = mesh->vertex_data[v0].position;
    lvPoint3D p1 = mesh->vertex_data[v].position;
    lvPoint3D p2 = mesh->vertex_data[v2].position;

    lvPoint3D a = lv_vec3_sub(p0, p1);
    lvPoint3D b = lv_vec3_sub(p2, p1);

    double dot = lv_vec3_dot(a, b);
    double len_a = sqrt(lv_vec3_dot(a, a));
    double len_b = sqrt(lv_vec3_dot(b, b));

    if (len_a == 0 || len_b == 0)
        return 0;

    double cos_angle = dot / (len_a * len_b);
    if (cos_angle > 1)
        cos_angle = 1;
    if (cos_angle < -1)
        cos_angle = -1;

    return acos(cos_angle);
}

double lv_he_mesh_halfedge_corner_angle(const lvHeMesh *mesh, lvHalfedge he) {
    if (!mesh || he == lv_HE_INVALID)
        return 0;

    lvHalfedge prev_he = mesh->he_prev[he];
    return lv_he_mesh_halfedge_angle(mesh, prev_he, he);
}

void lv_he_mesh_update_geometry(lvHeMesh *mesh) {
    if (!mesh)
        return;

    /* 更新所有面的法向量和面积 */
    for (lvFace f = 0; f < mesh->face_count; f++) {
        lvHalfedge start = mesh->face_he[f];
        lvVertex v0 = mesh->he_vertex[start];
        lvVertex v1 = mesh->he_vertex[mesh->he_next[start]];
        lvVertex v2 = mesh->he_vertex[mesh->he_next[mesh->he_next[start]]];

        lvPoint3D p0 = mesh->vertex_data[v0].position;
        lvPoint3D p1 = mesh->vertex_data[v1].position;
        lvPoint3D p2 = mesh->vertex_data[v2].position;

        lvPoint3D a = lv_vec3_sub(p1, p0);
        lvPoint3D b = lv_vec3_sub(p2, p0);
        lvPoint3D cross = lv_vec3_cross(a, b);
        double area = sqrt(lv_vec3_dot(cross, cross)) / 2.0;

        /* 使用相对 epsilon 防止零面积面产生未归一化法线：
         * area 较小时，用 |cross|^(1/2) 的量级缩放 epsilon */
        double area_eps = lv_EPSILON_ULTRA * (1.0 + fabs(cross.x) + fabs(cross.y) + fabs(cross.z));
        mesh->face_data[f].normal.x = cross.x / (2 * area + area_eps);
        mesh->face_data[f].normal.y = cross.y / (2 * area + area_eps);
        mesh->face_data[f].normal.z = cross.z / (2 * area + area_eps);
        mesh->face_data[f].area = area;
    }

    /* 更新顶点法向量 */
    for (lvVertex v = 0; v < mesh->vertex_count; v++) {
        mesh->vertex_data[v].normal = lv_he_mesh_vertex_normal(mesh, v);
        mesh->vertex_data[v].curvature = lv_he_mesh_vertex_curvature(mesh, v);
    }

    mesh->operation_count++;
}

/**
 * @brief 计算网格法向量（面法线 + 顶点法线）
 *
 * 等价于 lv_he_mesh_update_geometry 的几何刷新路径：
 * 重算所有面法线/面积与顶点法线/曲率。
 */
void lv_he_mesh_compute_normals(lvHeMesh *mesh) {
    lv_he_mesh_update_geometry(mesh);
}

/**
 * @brief 计算顶点离散曲率
 *
 * 使用 lv_he_mesh_vertex_curvature 逐顶点刷新（2π - 邻接角和）。
 */
void lv_he_mesh_compute_curvature(lvHeMesh *mesh) {
    if (!mesh)
        return;
    for (lvVertex v = 0; v < mesh->vertex_count; v++) {
        mesh->vertex_data[v].curvature = lv_he_mesh_vertex_curvature(mesh, v);
    }
    mesh->operation_count++;
}

/* ========================================================================
 * 第七部分：网格查询
 * ======================================================================== */

lvVertex lv_he_mesh_nearest_vertex(const lvHeMesh *mesh, lvPoint3D point, double *out_distance) {
    if (!mesh || mesh->vertex_count == 0) {
        if (out_distance)
            *out_distance = DBL_MAX;
        return lv_HE_INVALID;
    }

    double min_dist = DBL_MAX;
    lvVertex nearest = 0;

    for (lvVertex v = 0; v < mesh->vertex_count; v++) {
        lvPoint3D p = mesh->vertex_data[v].position;
        double dx = p.x - point.x;
        double dy = p.y - point.y;
        double dz = p.z - point.z;
        double dist = geo_norm_sq_3d(dx, dy, dz);

        if (dist < min_dist) {
            min_dist = dist;
            nearest = v;
        }
    }

    if (out_distance)
        *out_distance = sqrt(min_dist);
    return nearest;
}

lvFace lv_he_mesh_point_in_face(const lvHeMesh *mesh, lvPoint3D point, double *out_barycentric) {
    /* 简化的实现：射线投射法 */
    if (!mesh || mesh->face_count == 0)
        return lv_HE_INVALID;

    /* 投影到 XY 平面 */
    double px = point.x;
    double py = point.y;

    for (lvFace f = 0; f < mesh->face_count; f++) {
        lvVertex verts[3];
        lv_he_mesh_face_vertices(mesh, f, verts);

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
            if (out_barycentric && total_area > lv_EPSILON_HIGH) {
                double a1 = ((y2 - y3) * (px - x3) + (x3 - x2) * (py - y3)) / total_area;
                double a2 = ((y3 - y1) * (px - x3) + (x1 - x3) * (py - y3)) / total_area;
                out_barycentric[0] = a1;
                out_barycentric[1] = a2;
                out_barycentric[2] = 1 - a1 - a2;
            }
            return f;
        }
    }

    return lv_HE_INVALID;
}

double lv_he_mesh_total_area(const lvHeMesh *mesh) {
    if (!mesh)
        return 0;

    double total = 0;
    for (lvFace f = 0; f < mesh->face_count; f++) {
        total += mesh->face_data[f].area;
    }
    return total;
}

int lv_he_mesh_euler_characteristic(const lvHeMesh *mesh) {
    if (!mesh)
        return 0;
    return mesh->vertex_count - mesh->edge_count + mesh->face_count;
}

/* ========================================================================
 * 第八部分：统计
 * ======================================================================== */

void lv_he_mesh_get_stats(const lvHeMesh *mesh, lvHeMeshStats *out_stats) {
    if (!mesh || !out_stats)
        return;

    memset(out_stats, 0, sizeof(lvHeMeshStats));

    out_stats->vertex_count = mesh->vertex_count;
    out_stats->edge_count = mesh->edge_count;
    out_stats->halfedge_count = mesh->halfedge_count;
    out_stats->face_count = mesh->face_count;
    out_stats->total_area = lv_he_mesh_total_area(mesh);
    out_stats->euler_characteristic = lv_he_mesh_euler_characteristic(mesh);

    /* 顶点最大度数（valence）：扫描式统计每个顶点的出半边数量（与
     * lv_he_vertex_iter_begin 同款扫描语义，对边界/破损网格鲁棒） */
    int max_valence = 0;
    if (mesh->vertex_count > 0) {
        int *valence = (int *) lv_calloc((size_t) mesh->vertex_count, sizeof(int));
        if (valence) {
            for (lvHalfedge he = 0; he < mesh->halfedge_count; he++) {
                lvVertex v = mesh->he_vertex[he];
                if (v >= 0 && v < mesh->vertex_count)
                    valence[v]++;
            }
            for (lvVertex v = 0; v < mesh->vertex_count; v++) {
                if (valence[v] > max_valence)
                    max_valence = valence[v];
            }
            lv_free((void **) &valence);
        }
    }
    out_stats->max_vertex_valence = max_valence;

    /* 平均边长 */
    if (mesh->edge_count > 0) {
        double total_len = 0;
        for (lvEdge e = 0; e < mesh->edge_count; e++) {
            total_len += lv_he_mesh_edge_length(mesh, e);
        }
        out_stats->average_edge_length = total_len / mesh->edge_count;
    }
}

bool lv_he_mesh_validate(const lvHeMesh *mesh) {
    if (!mesh)
        return false;

    /* 检查半边有效性 */
    for (lvHalfedge he = 0; he < mesh->halfedge_count; he++) {
        if (mesh->he_vertex[he] < 0 || mesh->he_vertex[he] >= mesh->vertex_count) {
            return false;
        }
        if (mesh->he_twin[he] < 0 || mesh->he_twin[he] >= mesh->halfedge_count) {
            return false;
        }
        /* 边界半边允许 he_next / he_prev 为 -1 */
        if (mesh->he_face[he] < 0)
            continue;
        if (mesh->he_next[he] < 0 || mesh->he_next[he] >= mesh->halfedge_count) {
            return false;
        }
        if (mesh->he_prev[he] < 0 || mesh->he_prev[he] >= mesh->halfedge_count) {
            return false;
        }
    }

    /* 检查面有效性 */
    for (lvFace f = 0; f < mesh->face_count; f++) {
        lvHalfedge he = mesh->face_he[f];
        if (he < 0 || he >= mesh->halfedge_count) {
            return false;
        }
    }

    return true;
}

/* ── _mesh_ 前缀迭代器 ──
 * [修复] 原实现把 flags 当索引 0 硬编码委托 lv_he_vertex_iter_begin(mesh, 0) /
 * lv_he_face_iter_begin(mesh, 0)，导致"网格级顶点/面迭代"实际只迭代顶点 0 /
 * 面 0。按命名语义改为网格级遍历：current 依次为顶点/面索引。 */

lvHeVertexIterator lv_he_mesh_vertex_iter_begin(lvHeMesh *mesh, int flags) {
    (void) flags;
    lvHeVertexIterator iter;
    iter.mesh = mesh;
    iter.index = 0;
    iter.count = mesh ? mesh->vertex_count : 0;
    iter.current = (iter.count > 0) ? 0 : lv_HE_INVALID;
    return iter;
}

bool lv_he_mesh_vertex_iter_next(lvHeVertexIterator *iter) {
    if (!iter || !iter->mesh)
        return false;
    iter->index++;
    iter->current = (iter->index < iter->count) ? iter->index : lv_HE_INVALID;
    return iter->current != lv_HE_INVALID;
}

lvHeVertexIterator lv_he_mesh_vertex_out_iter_begin(lvHeMesh *mesh, lvVertex v) {
    return lv_he_vertex_iter_begin(mesh, v);
}

lvVertex lv_he_mesh_vertex_out_iter_next(lvHeVertexIterator *iter) {
    lv_he_vertex_iter_next(iter);
    return iter->current;
}

lvHeFaceIterator lv_he_mesh_face_iter_begin(lvHeMesh *mesh, int flags) {
    (void) flags;
    lvHeFaceIterator iter;
    iter.mesh = mesh;
    iter.index = 0;
    iter.count = mesh ? mesh->face_count : 0;
    iter.current = (iter.count > 0) ? 0 : lv_HE_INVALID;
    return iter;
}

bool lv_he_mesh_face_iter_next(lvHeFaceIterator *iter) {
    if (!iter || !iter->mesh)
        return false;
    iter->index++;
    iter->current = (iter->index < iter->count) ? iter->index : lv_HE_INVALID;
    return iter->current != lv_HE_INVALID;
}

/* ========================================================================
 * 计数 / 有效性查询（头文件契约补齐）
 * ======================================================================== */

int lv_he_mesh_edge_count(const lvHeMesh *mesh) {
    return mesh ? mesh->edge_count : 0;
}

int lv_he_mesh_face_count(const lvHeMesh *mesh) {
    return mesh ? mesh->face_count : 0;
}

int lv_he_mesh_vertex_count(const lvHeMesh *mesh) {
    return mesh ? mesh->vertex_count : 0;
}

bool lv_he_mesh_is_valid(const lvHeMesh *mesh) {
    return lv_he_mesh_validate(mesh);
}

/* ========================================================================
 * Legacy API（委托到新命名实现）
 * ======================================================================== */

int lv_halfedge_mesh_add_vertex(lvHalfedgeMesh *m, double x, double y, double z) {
    return lv_he_mesh_add_vertex(m, x, y, z);
}

int lv_halfedge_mesh_add_face(lvHalfedgeMesh *m, const int *indices, size_t count) {
    if (!m || !indices || count < 3)
        return lv_HE_INVALID;
    return lv_he_mesh_add_face(m, indices, (int) count);
}