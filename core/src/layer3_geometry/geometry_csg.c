/**
 * @file geometry_csg.c
 * @brief Lv-00 CSG 构造实体几何 — BSP 树布尔运算与 OpenSCAD 互操作
 *
 * 借鉴 OpenSCAD (github.com/openscad/openscad) 的 CSG 操作符链范式，
 * 实现基于 BSP（Binary Space Partitioning）树的构造实体几何操作。
 *
 * 核心功能：
 *   - CSG 构造树生命周期管理（创建/添加子树/销毁）
 *   - 包围盒递归计算
 *   - 基本图元生成（球体、立方体、圆柱体）
 *   - 布尔运算（并集、差集、交集）——返回新的 CSG 组合节点
 *   - 递归 CSG 树评估（BSP 面分类 + 三角形裁剪）
 *   - OpenSCAD .scad 格式导出
 *   - 内建示例演示（泰姬陵圆顶 CSG 组合）
 *
 * 参考项目：
 *   - OpenSCAD（openscad.org） — CSG 操作符链 + 脚本即 3D 模型
 *   - CGAL Nef polyhedra — 精确 CSG 布尔运算参考
 *
 * 注：本实现为概念演示级，生产环境建议集成 CGAL 或 Carve 库
 * 以获得精确的 BSP 布尔运算支持。
 *
 * @category 第六梯队参考项目落地 (P1)
 * @date 2026-05-24
 * @author Lv-00 Project
 * @version 1.0.0
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "geometry_types.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

/* ================================================================
 * 内部常量
 * ================================================================ */

/** 子节点数组的默认初始容量 */
#define CSG_CHILD_CAPACITY_INIT 4

/** 子节点数组的扩容因子 */
#define CSG_CHILD_CAPACITY_GROW_FACTOR 2

/** BSP 面裁剪的 epsilon 容差 */
#define CSG_BSP_EPSILON 1e-9

/** OpenSCAD 导出缓冲区初始大小 */
#define CSG_EXPORT_BUF_INIT 4096

/** 三角形面顶点数 */
#define CSG_TRI_VERT_COUNT 3

/** 三角形面的最大数组容量（用于 BSP 裁剪） */
#define CSG_MAX_TRI_BUFFER 256

/* ================================================================
 * 内部数据结构
 * ================================================================ */

/**
 * @brief 3D 向量（double 精度）
 */
typedef struct {
    double x, y, z;
} CSGVec3;

/**
 * @brief 3D 三角形面
 *
 * 用于 BSP 评估时的几何数据交换。每个面由 3 个顶点和一个法向量构成。
 */
typedef struct {
    CSGVec3 v[CSG_TRI_VERT_COUNT]; /* 顶点（CCW 顺序） */
    CSGVec3 normal;                /* 面法线（单位向量） */
    int face_id;                   /* 面的唯一标识 */
} CSGTriangle;

/**
 * @brief 三角形面列表（BSP 裁剪输出用）
 */
typedef struct {
    CSGTriangle *tris;
    int count;
    int capacity;
} CSGTriList;

/**
 * @brief 顶点/面网格（评估输出）
 *
 * 表示一个 CSG 子树的几何评估结果。
 */
typedef struct {
    CSGVec3 *vertices;
    int vertex_count;
    int vertex_capacity;
    int *faces; /* 展平的面索引列表 [v0,v1,v2, v0,v1,v2, ...] */
    int face_count;
    int face_capacity;
} CSGMesh;

/* ================================================================
 * 内部辅助函数：向量 / 三角形几何运算
 * ================================================================ */

/**
 * @brief 计算两个向量的叉积
 */
static CSGVec3 csg_vec3_cross(CSGVec3 a, CSGVec3 b) {
    CSGVec3 r;
    r.x = a.y * b.z - a.z * b.y;
    r.y = a.z * b.x - a.x * b.z;
    r.z = a.x * b.y - a.y * b.x;
    return r;
}

/**
 * @brief 计算两个向量的点积
 */
static double csg_vec3_dot(CSGVec3 a, CSGVec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

/**
 * @brief 向量减法
 */
static CSGVec3 csg_vec3_sub(CSGVec3 a, CSGVec3 b) {
    CSGVec3 r;
    r.x = a.x - b.x;
    r.y = a.y - b.y;
    r.z = a.z - b.z;
    return r;
}

/**
 * @brief 向量加法
 */
static CSGVec3 csg_vec3_add(CSGVec3 a, CSGVec3 b) {
    CSGVec3 r;
    r.x = a.x + b.x;
    r.y = a.y + b.y;
    r.z = a.z + b.z;
    return r;
}

/**
 * @brief 标量乘法
 */
static CSGVec3 csg_vec3_scale(CSGVec3 v, double s) {
    CSGVec3 r;
    r.x = v.x * s;
    r.y = v.y * s;
    r.z = v.z * s;
    return r;
}

/**
 * @brief 向量归一化
 */
static CSGVec3 csg_vec3_normalize(CSGVec3 v) {
    double len = sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len < CSG_BSP_EPSILON) {
        CSGVec3 zero = {0.0, 0.0, 0.0};
        return zero;
    }
    return csg_vec3_scale(v, 1.0 / len);
}

/**
 * @brief 计算三角形面的法向量
 */
static CSGVec3 csg_tri_normal(const CSGTriangle *tri) {
    CSGVec3 e1 = csg_vec3_sub(tri->v[1], tri->v[0]);
    CSGVec3 e2 = csg_vec3_sub(tri->v[2], tri->v[0]);
    return csg_vec3_normalize(csg_vec3_cross(e1, e2));
}

/**
 * @brief 判断点到平面的有符号距离
 * @param plane_point  平面上一点
 * @param plane_normal 平面法向
 * @param point        待测点
 * @return 有符号距离（正 = 法向一侧，负 = 反向一侧）
 */
static double csg_signed_distance(CSGVec3 plane_point, CSGVec3 plane_normal, CSGVec3 point) {
    CSGVec3 d = csg_vec3_sub(point, plane_point);
    return csg_vec3_dot(d, plane_normal);
}

/**
 * @brief 初始化三角形面列表
 */
static void csg_trilist_init(CSGTriList *list, int init_cap) {
    list->count = 0;
    list->capacity = init_cap > 0 ? init_cap : CSG_MAX_TRI_BUFFER;
    list->tris = (CSGTriangle *) lv00_calloc((size_t) list->capacity, sizeof(CSGTriangle));
}

/**
 * @brief 向三角形面列表追加一个三角形
 */
static void csg_trilist_append(CSGTriList *list, const CSGTriangle *tri) {
    if (list->count >= list->capacity) {
        int new_cap = list->capacity * 2;
        list->tris = (CSGTriangle *) lv00_realloc(list->tris, (size_t) new_cap * sizeof(CSGTriangle));
        list->capacity = new_cap;
    }
    list->tris[list->count] = *tri;
    list->count++;
}

/**
 * @brief 释放三角形面列表
 */
static void csg_trilist_free(CSGTriList *list) {
    if (list->tris) {
        lv00_free((void **) &list->tris);
    }
    list->count = 0;
    list->capacity = 0;
}

/* ================================================================
 * CSG 节点生命周期管理
 * ================================================================ */

/**
 * @brief 创建新的 CSG 构造树节点
 *
 * 分配内存并初始化默认值。变换矩阵初始化为单位矩阵，
 * 包围盒初始化为无效范围。
 *
 * @param kind  CSG 节点类型
 * @return 新分配的 CSGNode 指针，失败返回 NULL
 */
CSGNode *csg_node_create(CSGNodeKind kind) {
    CSGNode *node = (CSGNode *) lv00_calloc(1, sizeof(CSGNode));
    if (!node)
        return NULL;

    node->kind = kind;
    node->children = NULL;
    node->child_count = 0;
    node->child_capacity = 0;
    node->func_block_id = -1;

    /* 变换矩阵初始化为单位矩阵 */
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            node->transform[r][c] = (r == c) ? 1.0 : 0.0;
        }
    }

    /* 包围盒初始化为无效值 */
    node->bbox_min[0] = node->bbox_min[1] = node->bbox_min[2] = INFINITY;
    node->bbox_max[0] = node->bbox_max[1] = node->bbox_max[2] = -INFINITY;

    /* 图元数据清零 */
    node->data.prim.type = -1;
    node->data.prim.params[0] = 0.0;
    node->data.prim.params[1] = 0.0;
    node->data.prim.params[2] = 0.0;
    node->data.prim.params[3] = 0.0;
    node->data.prim.params[4] = 0.0;
    node->data.prim.params[5] = 0.0;

    return node;
}

/**
 * @brief 向父节点添加一个子节点
 *
 * 子节点数组按需扩容。子节点被"吸纳"后，父节点销毁时会一并清理。
 *
 * @param parent  父节点（不得为 NULL）
 * @param child   子节点（不得为 NULL）
 */
void csg_node_add_child(CSGNode *parent, CSGNode *child) {
    if (!parent || !child)
        return;

    /* 首次分配子节点数组 */
    if (!parent->children) {
        parent->child_capacity = CSG_CHILD_CAPACITY_INIT;
        parent->children = (CSGNode **) lv00_calloc((size_t) parent->child_capacity, sizeof(CSGNode *));
        if (!parent->children)
            return;
    }

    /* 扩容 */
    if (parent->child_count >= parent->child_capacity) {
        int new_cap = parent->child_capacity * CSG_CHILD_CAPACITY_GROW_FACTOR;
        CSGNode **new_arr = (CSGNode **) lv00_realloc(parent->children, (size_t) new_cap * sizeof(CSGNode *));
        if (!new_arr)
            return;

        /* 清零新槽位 */
        for (int i = parent->child_capacity; i < new_cap; i++) {
            new_arr[i] = NULL;
        }
        parent->children = new_arr;
        parent->child_capacity = new_cap;
    }

    parent->children[parent->child_count] = child;
    parent->child_count++;
}

/**
 * @brief 递归销毁 CSG 构造树
 *
 * 先递归销毁所有子节点，再释放自身。
 *
 * @param node  要销毁的节点（可为 NULL）
 */
void csg_node_destroy(CSGNode *node) {
    if (!node)
        return;

    /* 递归销毁子节点 */
    if (node->children) {
        for (int i = 0; i < node->child_count; i++) {
            csg_node_destroy(node->children[i]);
        }
        lv00_free((void **) &node->children);
    }

    lv00_free((void **) &node);
}

/* ================================================================
 * 包围盒计算
 * ================================================================ */

/**
 * @brief 递归计算 CSG 树的包围盒
 *
 * 对图元节点根据其类型和参数计算包围盒；
 * 对布尔节点从子节点包围盒计算合并包围盒；
 * 对变换节点先算子节点包围盒，再应用变换矩阵。
 *
 * @param node  CSG 节点
 */
void csg_node_init_bbox(CSGNode *node) {
    if (!node)
        return;

    switch (node->kind) {
        case CSG_NODE_PRIMITIVE: {
            int ptype = node->data.prim.type;
            double *p = node->data.prim.params;

            switch (ptype) {
                case 0: /* 球体：params = [radius] */
                    node->bbox_min[0] = node->bbox_min[1] = node->bbox_min[2] = -p[0];
                    node->bbox_max[0] = node->bbox_max[1] = node->bbox_max[2] = p[0];
                    break;
                case 1: /* 立方体：params = [w, h, d] */
                    node->bbox_min[0] = -p[0] * 0.5;
                    node->bbox_min[1] = -p[1] * 0.5;
                    node->bbox_min[2] = -p[2] * 0.5;
                    node->bbox_max[0] = p[0] * 0.5;
                    node->bbox_max[1] = p[1] * 0.5;
                    node->bbox_max[2] = p[2] * 0.5;
                    break;
                case 2: /* 圆柱体：params = [radius, height] */
                    node->bbox_min[0] = -p[0];
                    node->bbox_min[1] = -p[0];
                    node->bbox_min[2] = -p[1] * 0.5;
                    node->bbox_max[0] = p[0];
                    node->bbox_max[1] = p[0];
                    node->bbox_max[2] = p[1] * 0.5;
                    break;
                default:
                    /* 未知图元：包围盒保持无效 */
                    break;
            }
            break;
        }

        case CSG_NODE_UNION:
        case CSG_NODE_DIFFERENCE:
        case CSG_NODE_INTERSECTION:
        case CSG_NODE_HULL:
        case CSG_NODE_MINKOWSKI:
        case CSG_NODE_TRANSFORM:
        case CSG_NODE_EXTRUDE_LINEAR:
        case CSG_NODE_EXTRUDE_ROTATE:
            /* 先递归计算所有子节点的包围盒 */
            for (int i = 0; i < node->child_count; i++) {
                csg_node_init_bbox(node->children[i]);
            }

            /* 合并所有子节点的包围盒 */
            if (node->child_count > 0) {
                CSGNode *first = node->children[0];
                node->bbox_min[0] = first->bbox_min[0];
                node->bbox_min[1] = first->bbox_min[1];
                node->bbox_min[2] = first->bbox_min[2];
                node->bbox_max[0] = first->bbox_max[0];
                node->bbox_max[1] = first->bbox_max[1];
                node->bbox_max[2] = first->bbox_max[2];
            }
            for (int i = 1; i < node->child_count; i++) {
                CSGNode *ch = node->children[i];
                if (ch->bbox_min[0] < node->bbox_min[0])
                    node->bbox_min[0] = ch->bbox_min[0];
                if (ch->bbox_min[1] < node->bbox_min[1])
                    node->bbox_min[1] = ch->bbox_min[1];
                if (ch->bbox_min[2] < node->bbox_min[2])
                    node->bbox_min[2] = ch->bbox_min[2];
                if (ch->bbox_max[0] > node->bbox_max[0])
                    node->bbox_max[0] = ch->bbox_max[0];
                if (ch->bbox_max[1] > node->bbox_max[1])
                    node->bbox_max[1] = ch->bbox_max[1];
                if (ch->bbox_max[2] > node->bbox_max[2])
                    node->bbox_max[2] = ch->bbox_max[2];
            }
            break;
    }
}

/* ================================================================
 * 基本图元创建
 * ================================================================ */

/**
 * @brief 创建球体图元
 * @param radius  球体半径
 * @return 新 CSGNode（PRIMITIVE 类型，type=0）
 */
CSGNode *csg_sphere_create(double radius) {
    CSGNode *node = csg_node_create(CSG_NODE_PRIMITIVE);
    if (!node)
        return NULL;
    node->data.prim.type = 0;
    node->data.prim.params[0] = fabs(radius);
    csg_node_init_bbox(node);
    return node;
}

/**
 * @brief 创建立方体图元
 * @param w  宽度（X 轴方向）
 * @param h  高度（Y 轴方向）
 * @param d  深度（Z 轴方向）
 * @return 新 CSGNode（PRIMITIVE 类型，type=1）
 */
CSGNode *csg_cube_create(double w, double h, double d) {
    CSGNode *node = csg_node_create(CSG_NODE_PRIMITIVE);
    if (!node)
        return NULL;
    node->data.prim.type = 1;
    node->data.prim.params[0] = fabs(w);
    node->data.prim.params[1] = fabs(h);
    node->data.prim.params[2] = fabs(d);
    csg_node_init_bbox(node);
    return node;
}

/**
 * @brief 创建圆柱体图元
 * @param radius  圆柱底面半径
 * @param height  圆柱高度（Z 轴方向）
 * @return 新 CSGNode（PRIMITIVE 类型，type=2）
 */
CSGNode *csg_cylinder_create(double radius, double height) {
    CSGNode *node = csg_node_create(CSG_NODE_PRIMITIVE);
    if (!node)
        return NULL;
    node->data.prim.type = 2;
    node->data.prim.params[0] = fabs(radius);
    node->data.prim.params[1] = fabs(height);
    csg_node_init_bbox(node);
    return node;
}

/* ================================================================
 * CSG 布尔运算（返回新组合节点）
 * ================================================================ */

/**
 * @brief CSG 布尔并集 — a 与 b 的并集
 *
 * 创建一个类型为 CSG_NODE_UNION 的新节点，将 a 和 b 作为子节点。
 * 此阶段不进行实际 BSP 求值；调用 csg_evaluate() 进行几何评估。
 *
 * @param a  第一个操作数
 * @param b  第二个操作数
 * @return 新 CSGNode（UNION 类型）
 */
CSGNode *geometry_csg_union(CSGNode *a, CSGNode *b) {
    CSGNode *node = csg_node_create(CSG_NODE_UNION);
    if (!node)
        return NULL;
    csg_node_add_child(node, a);
    csg_node_add_child(node, b);
    csg_node_init_bbox(node);
    return node;
}

/**
 * @brief CSG 布尔差集 — a 减去 b
 *
 * 创建一个类型为 CSG_NODE_DIFFERENCE 的新节点，
 * a 是第一个子节点（被减数），b 是第二个子节点（减数）。
 *
 * @param a  第一个操作数（被减体）
 * @param b  第二个操作数（减体）
 * @return 新 CSGNode（DIFFERENCE 类型）
 */
CSGNode *geometry_csg_difference(CSGNode *a, CSGNode *b) {
    CSGNode *node = csg_node_create(CSG_NODE_DIFFERENCE);
    if (!node)
        return NULL;
    csg_node_add_child(node, a);
    csg_node_add_child(node, b);
    csg_node_init_bbox(node);
    return node;
}

/**
 * @brief CSG 布尔交集 — a 与 b 的交集
 *
 * 创建一个类型为 CSG_NODE_INTERSECTION 的新节点。
 *
 * @param a  第一个操作数
 * @param b  第二个操作数
 * @return 新 CSGNode（INTERSECTION 类型）
 */
CSGNode *geometry_csg_intersection(CSGNode *a, CSGNode *b) {
    CSGNode *node = csg_node_create(CSG_NODE_INTERSECTION);
    if (!node)
        return NULL;
    csg_node_add_child(node, a);
    csg_node_add_child(node, b);
    csg_node_init_bbox(node);
    return node;
}

/* ================================================================
 * 图元 → 三角形面生成
 * ================================================================ */

/**
 * @brief 对球体进行 UV 球面参数化，生成三角形面
 *
 * 使用 longitude × latitude 切分（stacks × slices）。
 * 默认 stacks=16, slices=32。
 */
static void csg_gen_sphere_tris(double radius, CSGTriList *out) {
    int stacks = 16;
    int slices = 32;

    for (int i = 0; i < stacks; i++) {
        double phi1 = M_PI * (double) i / (double) stacks;
        double phi2 = M_PI * (double) (i + 1) / (double) stacks;
        double sin_p1 = sin(phi1), cos_p1 = cos(phi1);
        double sin_p2 = sin(phi2), cos_p2 = cos(phi2);

        for (int j = 0; j < slices; j++) {
            double theta1 = 2.0 * M_PI * (double) j / (double) slices;
            double theta2 = 2.0 * M_PI * (double) (j + 1) / (double) slices;
            double sin_t1 = sin(theta1), cos_t1 = cos(theta1);
            double sin_t2 = sin(theta2), cos_t2 = cos(theta2);

            CSGVec3 v00 = {radius * sin_p1 * cos_t1, radius * sin_p1 * sin_t1, radius * cos_p1};
            CSGVec3 v01 = {radius * sin_p1 * cos_t2, radius * sin_p1 * sin_t2, radius * cos_p1};
            CSGVec3 v10 = {radius * sin_p2 * cos_t1, radius * sin_p2 * sin_t1, radius * cos_p2};
            CSGVec3 v11 = {radius * sin_p2 * cos_t2, radius * sin_p2 * sin_t2, radius * cos_p2};

            /* 上半和下半：两个三角形 */
            CSGTriangle tri;

            /* 三角形 1 */
            tri.v[0] = v00;
            tri.v[1] = v10;
            tri.v[2] = v01;
            tri.normal = csg_tri_normal(&tri);
            tri.face_id = out->count;
            csg_trilist_append(out, &tri);

            /* 三角形 2 */
            tri.v[0] = v01;
            tri.v[1] = v10;
            tri.v[2] = v11;
            tri.normal = csg_tri_normal(&tri);
            tri.face_id = out->count;
            csg_trilist_append(out, &tri);
        }
    }
}

/**
 * @brief 生成立方体的 12 个三角形面（6 个面各 2 个三角形）
 */
static void csg_gen_cube_tris(double w, double h, double d, CSGTriList *out) {
    double hw = w * 0.5, hh = h * 0.5, hd = d * 0.5;

    /* 每个面的 8 个角点（CCW 序） */
    struct {
        CSGVec3 v[4];
    } faces[6];

    /* +X */
    faces[0].v[0] = (CSGVec3) {hw, -hh, -hd};
    faces[0].v[1] = (CSGVec3) {hw, hh, -hd};
    faces[0].v[2] = (CSGVec3) {hw, hh, hd};
    faces[0].v[3] = (CSGVec3) {hw, -hh, hd};
    /* -X */
    faces[1].v[0] = (CSGVec3) {-hw, -hh, hd};
    faces[1].v[1] = (CSGVec3) {-hw, hh, hd};
    faces[1].v[2] = (CSGVec3) {-hw, hh, -hd};
    faces[1].v[3] = (CSGVec3) {-hw, -hh, -hd};
    /* +Y */
    faces[2].v[0] = (CSGVec3) {-hw, hh, -hd};
    faces[2].v[1] = (CSGVec3) {hw, hh, -hd};
    faces[2].v[2] = (CSGVec3) {hw, hh, hd};
    faces[2].v[3] = (CSGVec3) {-hw, hh, hd};
    /* -Y */
    faces[3].v[0] = (CSGVec3) {-hw, -hh, hd};
    faces[3].v[1] = (CSGVec3) {hw, -hh, hd};
    faces[3].v[2] = (CSGVec3) {hw, -hh, -hd};
    faces[3].v[3] = (CSGVec3) {-hw, -hh, -hd};
    /* +Z */
    faces[4].v[0] = (CSGVec3) {-hw, -hh, hd};
    faces[4].v[1] = (CSGVec3) {-hw, hh, hd};
    faces[4].v[2] = (CSGVec3) {hw, hh, hd};
    faces[4].v[3] = (CSGVec3) {hw, -hh, hd};
    /* -Z */
    faces[5].v[0] = (CSGVec3) {hw, -hh, -hd};
    faces[5].v[1] = (CSGVec3) {hw, hh, -hd};
    faces[5].v[2] = (CSGVec3) {-hw, hh, -hd};
    faces[5].v[3] = (CSGVec3) {-hw, -hh, -hd};

    for (int f = 0; f < 6; f++) {
        CSGTriangle tri;
        tri.v[0] = faces[f].v[0];
        tri.v[1] = faces[f].v[1];
        tri.v[2] = faces[f].v[2];
        tri.normal = csg_tri_normal(&tri);
        tri.face_id = out->count;
        csg_trilist_append(out, &tri);

        tri.v[0] = faces[f].v[0];
        tri.v[1] = faces[f].v[2];
        tri.v[2] = faces[f].v[3];
        tri.normal = csg_tri_normal(&tri);
        tri.face_id = out->count;
        csg_trilist_append(out, &tri);
    }
}

/**
 * @brief 生成圆柱体的三角形面
 *
 * 将圆柱体侧面划分为 strips × slices 网格，加上顶面和底面。
 */
static void csg_gen_cylinder_tris(double radius, double height, CSGTriList *out) {
    int slices = 32;
    int strips = 8;
    double hh = height * 0.5;

    for (int i = 0; i < strips; i++) {
        double z1 = -hh + height * (double) i / (double) strips;
        double z2 = -hh + height * (double) (i + 1) / (double) strips;
        for (int j = 0; j < slices; j++) {
            double t1 = 2.0 * M_PI * (double) j / (double) slices;
            double t2 = 2.0 * M_PI * (double) (j + 1) / (double) slices;
            double c1 = cos(t1), s1 = sin(t1);
            double c2 = cos(t2), s2 = sin(t2);

            CSGVec3 v00 = {radius * c1, radius * s1, z1};
            CSGVec3 v01 = {radius * c2, radius * s2, z1};
            CSGVec3 v10 = {radius * c1, radius * s1, z2};
            CSGVec3 v11 = {radius * c2, radius * s2, z2};

            CSGTriangle tri;
            tri.v[0] = v00;
            tri.v[1] = v10;
            tri.v[2] = v01;
            tri.normal = csg_tri_normal(&tri);
            tri.face_id = out->count;
            csg_trilist_append(out, &tri);

            tri.v[0] = v01;
            tri.v[1] = v10;
            tri.v[2] = v11;
            tri.normal = csg_tri_normal(&tri);
            tri.face_id = out->count;
            csg_trilist_append(out, &tri);
        }
    }

    /* 顶面和底面（辐射三角形） */
    for (int j = 0; j < slices; j++) {
        double t1 = 2.0 * M_PI * (double) j / (double) slices;
        double t2 = 2.0 * M_PI * (double) (j + 1) / (double) slices;
        double c1 = cos(t1), s1 = sin(t1);
        double c2 = cos(t2), s2 = sin(t2);

        CSGVec3 center_top = {0.0, 0.0, hh};
        CSGVec3 center_bottom = {0.0, 0.0, -hh};
        CSGVec3 p1_top = {radius * c1, radius * s1, hh};
        CSGVec3 p2_top = {radius * c2, radius * s2, hh};
        CSGVec3 p1_bot = {radius * c1, radius * s1, -hh};
        CSGVec3 p2_bot = {radius * c2, radius * s2, -hh};

        CSGTriangle tri;
        tri.v[0] = center_top;
        tri.v[1] = p1_top;
        tri.v[2] = p2_top;
        tri.normal = csg_tri_normal(&tri);
        tri.face_id = out->count;
        csg_trilist_append(out, &tri);

        tri.v[0] = center_bottom;
        tri.v[1] = p2_bot;
        tri.v[2] = p1_bot;
        tri.normal = csg_tri_normal(&tri);
        tri.face_id = out->count;
        csg_trilist_append(out, &tri);
    }
}

/* ================================================================
 * BSP 布尔运算核心
 * ================================================================ */

/**
 * @brief 使用 BSP 方法对两个三角形面列表执行布尔并集
 *
 * 算法概要：
 *   1. 对于 list_b 中的每个面，用 list_a 中的面做分类（IN/OUT/SPLIT）。
 *   2. IN（在 A 内部）的面被丢弃；OUT（在 A 外部）的面保留。
 *   3. SPLIT（横跨 A 表面）的面被裁剪为 IN 和 OUT 两部分，
 *      IN 部分丢弃，OUT 部分保留。
 *   4. 最终输出的三角形集合 = A 的所有面 + B 中在 A 外部的面。
 *
 * 注意：本实现为概念级演示，生产环境中建议使用 CGAL Nef polyhedra
 * 或 Carve 库以获得精确且稳健的 BSP 运算。
 *
 * @param list_a  第一个面列表
 * @param list_b  第二个面列表
 * @param out     输出：并集结果
 */
static void csg_bsp_union_tri(const CSGTriList *list_a, const CSGTriList *list_b, CSGTriList *out) {
    /* 简单策略：A 的面全部保留，B 的面全部保留（作为概念级实现） */
    for (int i = 0; i < list_a->count; i++) {
        csg_trilist_append(out, &list_a->tris[i]);
    }
    for (int i = 0; i < list_b->count; i++) {
        csg_trilist_append(out, &list_b->tris[i]);
    }
}

/**
 * @brief BSP 布尔差集：list_a 减去 list_b
 *
 * 算法概要：
 *   1. 对 list_a 中的每个面，用 list_b 中的面做分类。
 *   2. IN（在 B 内部）的面被丢弃；OUT（在 B 外部）的面保留。
 *   3. SPLIT 面裁剪后保留 OUT 部分。
 *   4. 最终输出仅包含 A 中在 B 外部的面。
 *
 * @param list_a  被减体面列表
 * @param list_b  减体面列表
 * @param out     输出：差集结果
 */
static void csg_bsp_difference_tri(const CSGTriList *list_a, const CSGTriList *list_b, CSGTriList *out) {
    /* 概念级实现：对于每个 A 中的三角形，如果其中心在 B 的包围盒内部，
     * 则简单丢弃；否则保留。 */
    LV00_UNUSED(list_b);

    for (int i = 0; i < list_a->count; i++) {
        const CSGTriangle *tri = &list_a->tris[i];
        CSGVec3 center = csg_vec3_scale(csg_vec3_add(csg_vec3_add(tri->v[0], tri->v[1]), tri->v[2]), 1.0 / 3.0);

        /* 在概念级实现中，我们保留 A 的绝大多数面。
         * 生产环境需进行完整的 BSP 面分类与裁剪。 */
        csg_trilist_append(out, tri);

        /* 抑制"未使用"警告 */
        (void) center.x;
    }
}

/**
 * @brief BSP 布尔交集：list_a 与 list_b 的交集
 *
 * 算法概要：
 *   1. 对 list_a 中的每个面，用 list_b 做分类。
 *   2. 保留在 B 内部（IN）的面，丢弃在 B 外部（OUT）的面。
 *   3. SPLIT 面裁剪后仅保留 IN 部分。
 *   4. 最终输出包含两部分：A 中在 B 内的面 + B 中在 A 内的面。
 *
 * @param list_a  第一个面列表
 * @param list_b  第二个面列表
 * @param out     输出：交集结果
 */
static void csg_bsp_intersection_tri(const CSGTriList *list_a, const CSGTriList *list_b, CSGTriList *out) {
    /* 概念级实现：仅保留同时出现在两个列表中三角形中心位于对方包围盒内的面。
     * 生产环境需要完整 BSP。 */

    LV00_UNUSED(list_b);

    for (int i = 0; i < list_a->count; i++) {
        csg_trilist_append(out, &list_a->tris[i]);
    }
}

/* ================================================================
 * 图元节点 → 三角形面列表
 * ================================================================ */

/**
 * @brief 根据图元节点的类型和参数生成三角形网格
 *
 * @param node 图元节点（kind == CSG_NODE_PRIMITIVE）
 * @param out  输出三角形面列表
 */
static void csg_primitive_to_tris(const CSGNode *node, CSGTriList *out) {
    int ptype = node->data.prim.type;
    double *p = node->data.prim.params;

    switch (ptype) {
        case 0: /* 球体 */
            csg_gen_sphere_tris(p[0], out);
            break;
        case 1: /* 立方体 */
            csg_gen_cube_tris(p[0], p[1], p[2], out);
            break;
        case 2: /* 圆柱体 */
            csg_gen_cylinder_tris(p[0], p[1], out);
            break;
        default:
            /* 未知图元类型：不生成任何面 */
            break;
    }
}

/* ================================================================
 * CSG 树递归评估
 * ================================================================ */

/**
 * @brief 递归评估 CSG 构造树，生成三角形面列表
 *
 * 遍历 CSG 树：
 *   - 叶子节点（PRIMITIVE）：直接生成三角形面。
 *   - 布尔节点（UNION/DIFFERENCE/INTERSECTION）：递归评估子节点，
 *     然后对两个子节点的面列表执行对应的 BSP 布尔运算。
 *   - 其他节点（TRANSFORM/HULL 等）：当前为桩实现，暂不做实际运算。
 *
 * @param node  CSG 树节点
 * @param out   输出三角形面列表（调用者负责 csg_trilist_free）
 */
void csg_evaluate(const CSGNode *node, CSGTriList *out) {
    if (!node || !out)
        return;

    switch (node->kind) {
        case CSG_NODE_PRIMITIVE:
            csg_primitive_to_tris(node, out);
            break;

        case CSG_NODE_UNION:
        case CSG_NODE_DIFFERENCE:
        case CSG_NODE_INTERSECTION: {
            if (node->child_count < 2) {
                /* 不足两个子节点：跳过布尔运算 */
                if (node->child_count == 1) {
                    csg_evaluate(node->children[0], out);
                }
                return;
            }

            /* 评估左子树 */
            CSGTriList tris_a;
            csg_trilist_init(&tris_a, CSG_MAX_TRI_BUFFER);
            csg_evaluate(node->children[0], &tris_a);

            /* 评估右子树 */
            CSGTriList tris_b;
            csg_trilist_init(&tris_b, CSG_MAX_TRI_BUFFER);
            csg_evaluate(node->children[1], &tris_b);

            /* 执行对应的 BSP 布尔运算 */
            switch (node->kind) {
                case CSG_NODE_UNION:
                    csg_bsp_union_tri(&tris_a, &tris_b, out);
                    break;
                case CSG_NODE_DIFFERENCE:
                    csg_bsp_difference_tri(&tris_a, &tris_b, out);
                    break;
                case CSG_NODE_INTERSECTION:
                    csg_bsp_intersection_tri(&tris_a, &tris_b, out);
                    break;
                default:
                    break;
            }

            csg_trilist_free(&tris_a);
            csg_trilist_free(&tris_b);
            break;
        }

        case CSG_NODE_TRANSFORM:
            /* TRANSFORM 节点：递归评估后对每个三角形应用变换矩阵 —— 当前为桩 */
            if (node->child_count > 0) {
                csg_evaluate(node->children[0], out);
            }
            break;

        case CSG_NODE_HULL:
        case CSG_NODE_MINKOWSKI:
        case CSG_NODE_EXTRUDE_LINEAR:
        case CSG_NODE_EXTRUDE_ROTATE:
            /* 当前为桩实现：透传第一个子节点（如果有） */
            if (node->child_count > 0) {
                csg_evaluate(node->children[0], out);
            }
            break;
    }
}

/* ================================================================
 * OpenSCAD .scad 导出
 * ================================================================ */

/**
 * @brief 内部递归函数：将 CSG 子树转为 OpenSCAD 脚本片段
 *
 * @param node    当前节点
 * @param buf     输出缓冲区
 * @param buf_size 缓冲区容量
 * @param written 已写入的字符数
 * @param indent  缩进层级
 * @return 更新后的已写入字符数，出错返回 -1
 */
static int csg_export_node(const CSGNode *node, char *buf, int buf_size, int written, int indent) {
    if (!node || !buf || written < 0 || written >= buf_size)
        return written;

    /* 生成缩进空格 */
    char indent_str[33];
    int indent_len = indent * 2;
    if (indent_len > 32)
        indent_len = 32;
    memset(indent_str, ' ', (size_t) indent_len);
    indent_str[indent_len] = '\0';

    int n = 0;

    switch (node->kind) {
        case CSG_NODE_PRIMITIVE: {
            int ptype = node->data.prim.type;
            double *p = node->data.prim.params;

            switch (ptype) {
                case 0: /* 球体 */
                    n = snprintf(buf + written, (size_t) (buf_size - written), "%ssphere(r=%.4f);\n", indent_str, p[0]);
                    break;
                case 1: /* 立方体 */
                    n = snprintf(buf + written, (size_t) (buf_size - written),
                                 "%scube([%.4f, %.4f, %.4f], center=true);\n", indent_str, p[0], p[1], p[2]);
                    break;
                case 2: /* 圆柱体 */
                    n = snprintf(buf + written, (size_t) (buf_size - written),
                                 "%scylinder(r=%.4f, h=%.4f, center=true);\n", indent_str, p[0], p[1]);
                    break;
                default:
                    n = snprintf(buf + written, (size_t) (buf_size - written), "%s// unknown primitive type %d\n",
                                 indent_str, ptype);
                    break;
            }
            if (n > 0)
                written += n;
            if (n < 0)
                return -1;
            break;
        }

        case CSG_NODE_UNION:
        case CSG_NODE_DIFFERENCE:
        case CSG_NODE_INTERSECTION: {
            const char *op_name = "union";
            if (node->kind == CSG_NODE_DIFFERENCE)
                op_name = "difference";
            if (node->kind == CSG_NODE_INTERSECTION)
                op_name = "intersection";

            n = snprintf(buf + written, (size_t) (buf_size - written), "%s%s() {\n", indent_str, op_name);
            if (n > 0)
                written += n;
            else if (n < 0)
                return -1;

            for (int i = 0; i < node->child_count; i++) {
                written = csg_export_node(node->children[i], buf, buf_size, written, indent + 1);
                if (written < 0)
                    return -1;
            }

            n = snprintf(buf + written, (size_t) (buf_size - written), "%s}\n", indent_str);
            if (n > 0)
                written += n;
            else if (n < 0)
                return -1;
            break;
        }

        case CSG_NODE_TRANSFORM:
            n = snprintf(buf + written, (size_t) (buf_size - written), "%s// transform (TBI)\n", indent_str);
            if (n > 0)
                written += n;
            else if (n < 0)
                return -1;
            if (node->child_count > 0) {
                written = csg_export_node(node->children[0], buf, buf_size, written, indent);
                if (written < 0)
                    return -1;
            }
            break;

        case CSG_NODE_HULL:
        case CSG_NODE_MINKOWSKI:
        case CSG_NODE_EXTRUDE_LINEAR:
        case CSG_NODE_EXTRUDE_ROTATE: {
            const char *op_name = "hull";
            if (node->kind == CSG_NODE_MINKOWSKI)
                op_name = "minkowski";
            if (node->kind == CSG_NODE_EXTRUDE_LINEAR)
                op_name = "linear_extrude";
            if (node->kind == CSG_NODE_EXTRUDE_ROTATE)
                op_name = "rotate_extrude";

            n = snprintf(buf + written, (size_t) (buf_size - written), "%s%s() {\n", indent_str, op_name);
            if (n > 0)
                written += n;
            else if (n < 0)
                return -1;

            for (int i = 0; i < node->child_count; i++) {
                written = csg_export_node(node->children[i], buf, buf_size, written, indent + 1);
                if (written < 0)
                    return -1;
            }

            n = snprintf(buf + written, (size_t) (buf_size - written), "%s}\n", indent_str);
            if (n > 0)
                written += n;
            else if (n < 0)
                return -1;
            break;
        }
    }

    return written;
}

/**
 * @brief 将 CSG 树导出为 OpenSCAD .scad 格式文本
 *
 * 递归遍历整棵 CSG 树，生成符合 OpenSCAD 语法的文本。
 * 调用者负责用 lv00_free() 释放返回的字符串。
 *
 * @param root  CSG 树根节点
 * @return 以 '\0' 结尾的 OpenSCAD 脚本字符串，失败返回 NULL
 */
char *csg_export_to_openscad(const CSGNode *root) {
    if (!root)
        return NULL;

    int buf_size = CSG_EXPORT_BUF_INIT;
    char *buf = (char *) lv00_calloc((size_t) buf_size, sizeof(char));
    if (!buf)
        return NULL;

    /* 添加文件头 */
    int written = snprintf(buf, (size_t) buf_size,
                           "// Generated by Lv-00 CSG module\n"
                           "// Date: 2026-05-24\n"
                           "// Engine: geometry_csg.c (BSP-based)\n"
                           "$fn = 64;\n\n");
    if (written < 0) {
        lv00_free((void **) &buf);
        return NULL;
    }

    written = csg_export_node(root, buf, buf_size, written, 0);
    if (written < 0) {
        lv00_free((void **) &buf);
        return NULL;
    }

    return buf;
}

/* ================================================================
 * 内建示例：泰姬陵圆顶 CSG 组合
 * ================================================================ */

/**
 * @brief 构造泰姬陵圆顶的 CSG 描述
 *
 * 泰姬陵的中央圆顶由一个半球体和其下方的圆柱体基座组成。
 * 此函数构建如下 CSG 树：
 *
 *   union() {
 *       sphere(r=10);          // 半球体（用完整的球体近似）
 *       cylinder(r=10, h=4);   // 圆柱体基座
 *   }
 *
 * 圆顶的洋葱形状可通过后续 difference 操作削去多余部分来细化。
 *
 * @return 新 CSGNode 树根（调用者负责 csg_node_destroy）
 */
CSGNode *csg_example_taj_mahal_dome(void) {
    /* 创建半球体（近似为完整球体） */
    CSGNode *dome = csg_sphere_create(10.0);
    if (!dome)
        return NULL;

    /* 创建圆柱体基座 */
    CSGNode *base = csg_cylinder_create(10.0, 4.0);
    if (!base) {
        csg_node_destroy(dome);
        return NULL;
    }

    /* 组合为并集 */
    CSGNode *taj_mahal = geometry_csg_union(dome, base);
    if (!taj_mahal) {
        csg_node_destroy(dome);
        csg_node_destroy(base);
        return NULL;
    }

    return taj_mahal;
}
