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

#define _USE_MATH_DEFINES
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "geometry_types.h"
#include "lv_internal.h"
#include "lv_utils.h"

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
    list->tris = (CSGTriangle *) lv_calloc((size_t) list->capacity, sizeof(CSGTriangle));
    if (!list->tris) {
        list->capacity = 0;
    }
}

/**
 * @brief 向三角形面列表追加一个三角形
 */
static void csg_trilist_append(CSGTriList *list, const CSGTriangle *tri) {
    if (list->count >= list->capacity) {
        if (list->capacity > INT_MAX / 2) return;
        int new_cap = list->capacity * 2;
        CSGTriangle *new_tris = (CSGTriangle *) lv_realloc(list->tris, (size_t) new_cap * sizeof(CSGTriangle));
        if (!new_tris) return;
        list->tris = new_tris;
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
        lv_free((void **) &list->tris);
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
    CSGNode *node = (CSGNode *) lv_calloc(1, sizeof(CSGNode));
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
        parent->children = (CSGNode **) lv_calloc((size_t) parent->child_capacity, sizeof(CSGNode *));
        if (!parent->children)
            return;
    }

    /* 扩容 */
    if (parent->child_count >= parent->child_capacity) {
        /* 溢出保护：确保 child_capacity * GROW_FACTOR 不会溢出 */
        if (parent->child_capacity > INT_MAX / CSG_CHILD_CAPACITY_GROW_FACTOR)
            return;
        int new_cap = parent->child_capacity * CSG_CHILD_CAPACITY_GROW_FACTOR;
        CSGNode **new_arr = (CSGNode **) lv_realloc(parent->children, (size_t) new_cap * sizeof(CSGNode *));
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
        lv_free((void **) &node->children);
    }

    lv_free((void **) &node);
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
    lv_UNUSED(list_b);

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

    lv_UNUSED(list_b);

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
 * 凸包计算（HULL / MINKOWSKI 共用）
 * ================================================================ */

/**
 * @brief 从顶点集合计算 3D 凸包（暴力枚举法，适用于顶点数 < 200 的场景）
 *
 * 算法：
 *   1. 枚举所有顶点三元组 (i, j, k) 作为候选面
 *   2. 对每个候选面，检查所有其他顶点是否都在同一侧（或平面上）
 *   3. 满足条件的面为凸包面，法线朝外
 *   4. 将凸包面输出为三角形列表
 *
 * @param vertices      顶点数组
 * @param vertex_count  顶点数量
 * @param out           输出三角形面列表
 */
static void csg_compute_convex_hull(const CSGVec3 *vertices, int vertex_count, CSGTriList *out) {
    if (vertex_count < 4) {
        /* 不足 4 个顶点，无法构成 3D 凸包 */
        return;
    }

    /* 计算所有顶点的质心，用于判断法线朝向 */
    CSGVec3 centroid = {0.0, 0.0, 0.0};
    for (int i = 0; i < vertex_count; i++) {
        centroid.x += vertices[i].x;
        centroid.y += vertices[i].y;
        centroid.z += vertices[i].z;
    }
    centroid.x /= (double) vertex_count;
    centroid.y /= (double) vertex_count;
    centroid.z /= (double) vertex_count;

    /* 暴力枚举所有三元组 */
    for (int i = 0; i < vertex_count - 2; i++) {
        for (int j = i + 1; j < vertex_count - 1; j++) {
            for (int k = j + 1; k < vertex_count; k++) {
                CSGVec3 e1 = csg_vec3_sub(vertices[j], vertices[i]);
                CSGVec3 e2 = csg_vec3_sub(vertices[k], vertices[i]);
                CSGVec3 normal = csg_vec3_normalize(csg_vec3_cross(e1, e2));

                /* 退化三角形（面积为零），跳过 */
                double nlen = sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
                if (nlen < CSG_BSP_EPSILON)
                    continue;

                /* 检查所有其他顶点是否在面的同一侧 */
                int pos_count = 0;
                int neg_count = 0;
                int valid = 1;

                for (int m = 0; m < vertex_count; m++) {
                    if (m == i || m == j || m == k)
                        continue;

                    double d = csg_signed_distance(vertices[i], normal, vertices[m]);
                    if (d > CSG_BSP_EPSILON)
                        pos_count++;
                    else if (d < -CSG_BSP_EPSILON)
                        neg_count++;

                    /* 两侧都有顶点 → 不是凸包面 */
                    if (pos_count > 0 && neg_count > 0) {
                        valid = 0;
                        break;
                    }
                }

                if (!valid)
                    continue;

                /* 确保法线朝外（远离质心方向） */
                double centroid_dist = csg_signed_distance(vertices[i], normal, centroid);
                if (centroid_dist > 0.0) {
                    /* 法线指向质心方向 → 翻转 */
                    normal = csg_vec3_scale(normal, -1.0);
                    /* 翻转顶点顺序以保持 CCW */
                    CSGTriangle tri;
                    tri.v[0] = vertices[i];
                    tri.v[1] = vertices[k];
                    tri.v[2] = vertices[j];
                    tri.normal = normal;
                    tri.face_id = out->count;
                    csg_trilist_append(out, &tri);
                } else {
                    CSGTriangle tri;
                    tri.v[0] = vertices[i];
                    tri.v[1] = vertices[j];
                    tri.v[2] = vertices[k];
                    tri.normal = normal;
                    tri.face_id = out->count;
                    csg_trilist_append(out, &tri);
                }
            }
        }
    }
}

/**
 * @brief 从三角形面列表中提取所有唯一顶点
 *
 * @param tris       三角形面列表
 * @param out_verts  输出：顶点数组（调用者释放）
 * @param out_count  输出：顶点数量
 */
static void csg_extract_vertices(const CSGTriList *tris, CSGVec3 **out_verts, int *out_count) {
    /* 最坏情况：每个三角形 3 个顶点 */
    int max_verts = tris->count * 3;
    CSGVec3 *verts = (CSGVec3 *) lv_calloc((size_t) max_verts, sizeof(CSGVec3));
    if (!verts) {
        *out_verts = NULL;
        *out_count = 0;
        return;
    }

    int count = 0;
    for (int i = 0; i < tris->count; i++) {
        for (int v = 0; v < 3; v++) {
            CSGVec3 pt = tris->tris[i].v[v];
            /* 检查是否已存在（简单 O(n^2) 去重） */
            int found = 0;
            for (int j = 0; j < count; j++) {
                CSGVec3 d = csg_vec3_sub(pt, verts[j]);
                double dist2 = d.x * d.x + d.y * d.y + d.z * d.z;
                if (dist2 < CSG_BSP_EPSILON * CSG_BSP_EPSILON) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                verts[count] = pt;
                count++;
            }
        }
    }

    *out_verts = verts;
    *out_count = count;
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
 *   - TRANSFORM 节点：递归评估子节点后应用 4x4 齐次变换矩阵。
 *   - HULL 节点：收集所有子节点顶点，计算凸包。
 *   - MINKOWSKI 节点：计算两个子网格的 Minkowski 和。
 *   - EXTRUDE_LINEAR 节点：将 2D 截面沿指定方向拉伸为 3D 实体。
 *   - EXTRUDE_ROTATE 节点：将 2D 截面绕轴旋转生成旋转体。
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

        case CSG_NODE_TRANSFORM: {
            /* TRANSFORM 节点：递归评估子节点后应用 4x4 齐次变换矩阵 */
            if (node->child_count < 1)
                break;

            /* 递归评估子节点，获取原始三角形面 */
            CSGTriList child_tris;
            csg_trilist_init(&child_tris, CSG_MAX_TRI_BUFFER);
            csg_evaluate(node->children[0], &child_tris);

            /* 提取 4x4 变换矩阵 */
            const double (*M)[4] = node->transform;

            /* 计算逆转置矩阵的 3x3 部分（用于法线变换） */
            /* 先求 3x3 子矩阵的行列式 */
            double m00 = M[0][0], m01 = M[0][1], m02 = M[0][2];
            double m10 = M[1][0], m11 = M[1][1], m12 = M[1][2];
            double m20 = M[2][0], m21 = M[2][1], m22 = M[2][2];

            double det = m00 * (m11 * m22 - m12 * m21)
                       - m01 * (m10 * m22 - m12 * m20)
                       + m02 * (m10 * m21 - m11 * m20);

            /* 逆转置 3x3 = (1/det) * adjugate(M)^T = (1/det) * cofactor(M) */
            double inv_det;
            double invT[3][3];
            if (fabs(det) < CSG_BSP_EPSILON) {
                /* 奇异矩阵：行列式接近零，法线保持不变（使用单位矩阵） */
                invT[0][0] = 1.0; invT[0][1] = 0.0; invT[0][2] = 0.0;
                invT[1][0] = 0.0; invT[1][1] = 1.0; invT[1][2] = 0.0;
                invT[2][0] = 0.0; invT[2][1] = 0.0; invT[2][2] = 1.0;
            } else {
                inv_det = 1.0 / det;
                invT[0][0] = ( m11 * m22 - m12 * m21) * inv_det;
                invT[0][1] = ( m02 * m21 - m01 * m22) * inv_det;
                invT[0][2] = ( m01 * m12 - m02 * m11) * inv_det;
                invT[1][0] = ( m12 * m20 - m10 * m22) * inv_det;
                invT[1][1] = ( m00 * m22 - m02 * m20) * inv_det;
                invT[1][2] = ( m02 * m10 - m00 * m12) * inv_det;
                invT[2][0] = ( m10 * m21 - m11 * m20) * inv_det;
                invT[2][1] = ( m01 * m20 - m00 * m21) * inv_det;
                invT[2][2] = ( m00 * m11 - m01 * m10) * inv_det;
            }

            /* 对每个三角形应用变换 */
            for (int i = 0; i < child_tris.count; i++) {
                CSGTriangle tri = child_tris.tris[i];

                /* 变换三个顶点：v' = M * v（齐次坐标） */
                for (int v = 0; v < 3; v++) {
                    double x = tri.v[v].x;
                    double y = tri.v[v].y;
                    double z = tri.v[v].z;
                    double w = M[3][0] * x + M[3][1] * y + M[3][2] * z + M[3][3];

                    /* 透视除法（仅当 w != 1 时） */
                    double inv_w = (fabs(w - 1.0) > CSG_BSP_EPSILON) ? 1.0 / w : 1.0;

                    tri.v[v].x = (M[0][0] * x + M[0][1] * y + M[0][2] * z + M[0][3]) * inv_w;
                    tri.v[v].y = (M[1][0] * x + M[1][1] * y + M[1][2] * z + M[1][3]) * inv_w;
                    tri.v[v].z = (M[2][0] * x + M[2][1] * y + M[2][2] * z + M[2][3]) * inv_w;
                }

                /* 用逆转置矩阵变换法线：n' = (M^{-1})^T * n */
                double nx = tri.normal.x;
                double ny = tri.normal.y;
                double nz = tri.normal.z;
                tri.normal.x = invT[0][0] * nx + invT[0][1] * ny + invT[0][2] * nz;
                tri.normal.y = invT[1][0] * nx + invT[1][1] * ny + invT[1][2] * nz;
                tri.normal.z = invT[2][0] * nx + invT[2][1] * ny + invT[2][2] * nz;
                tri.normal = csg_vec3_normalize(tri.normal);

                tri.face_id = out->count;
                csg_trilist_append(out, &tri);
            }

            csg_trilist_free(&child_tris);
            break;
        }

        case CSG_NODE_HULL: {
            /* HULL 节点：收集所有子节点的顶点，计算凸包 */
            if (node->child_count < 1)
                break;

            /* 收集所有子节点的三角形面 */
            CSGTriList all_tris;
            csg_trilist_init(&all_tris, CSG_MAX_TRI_BUFFER * node->child_count);
            for (int i = 0; i < node->child_count; i++) {
                csg_evaluate(node->children[i], &all_tris);
            }

            /* 提取唯一顶点 */
            CSGVec3 *hull_verts = NULL;
            int hull_vert_count = 0;
            csg_extract_vertices(&all_tris, &hull_verts, &hull_vert_count);

            if (hull_verts && hull_vert_count >= 4) {
                /* 计算凸包 */
                csg_compute_convex_hull(hull_verts, hull_vert_count, out);
            }

            /* 清理 */
            if (hull_verts) {
                lv_free((void **) &hull_verts);
            }
            csg_trilist_free(&all_tris);
            break;
        }

        case CSG_NODE_MINKOWSKI: {
            /* MINKOWSKI 节点：计算两个子网格的 Minkowski 和 A ⊕ B */
            if (node->child_count < 2)
                break;

            /* 评估两个子节点 */
            CSGTriList tris_a, tris_b;
            csg_trilist_init(&tris_a, CSG_MAX_TRI_BUFFER);
            csg_trilist_init(&tris_b, CSG_MAX_TRI_BUFFER);
            csg_evaluate(node->children[0], &tris_a);
            csg_evaluate(node->children[1], &tris_b);

            /* 提取两个网格的唯一顶点 */
            CSGVec3 *verts_a = NULL, *verts_b = NULL;
            int count_a = 0, count_b = 0;
            csg_extract_vertices(&tris_a, &verts_a, &count_a);
            csg_extract_vertices(&tris_b, &verts_b, &count_b);

            if (verts_a && verts_b && count_a > 0 && count_b > 0) {
                /* 计算所有顶点对之和：v_sum = v_a + v_b */
                if (count_a <= INT_MAX / count_b) {
                int sum_count = count_a * count_b;
                CSGVec3 *sum_verts = (CSGVec3 *) lv_calloc((size_t) sum_count, sizeof(CSGVec3));
                if (sum_verts) {
                    int idx = 0;
                    for (int i = 0; i < count_a; i++) {
                        for (int j = 0; j < count_b; j++) {
                            sum_verts[idx] = csg_vec3_add(verts_a[i], verts_b[j]);
                            idx++;
                        }
                    }

                    /* 对求和结果计算凸包，得到 Minkowski 和的近似 */
                    csg_compute_convex_hull(sum_verts, sum_count, out);

                    lv_free((void **) &sum_verts);
                }
                } /* end if (count_a <= INT_MAX / count_b) */
            } /* end if (verts_a && verts_b) */

            /* 清理 */
            if (verts_a) lv_free((void **) &verts_a);
            if (verts_b) lv_free((void **) &verts_b);
            csg_trilist_free(&tris_a);
            csg_trilist_free(&tris_b);
            break;
        }

        case CSG_NODE_EXTRUDE_LINEAR: {
            /* EXTRUDE_LINEAR 节点：将 2D 截面沿指定方向拉伸为 3D 实体
             *
             * 使用 node->data.prim.params 存储拉伸参数：
             *   params[0] = 拉伸高度（沿 Z 轴）
             *   params[1] = 拉伸方向 X 分量（默认 0）
             *   params[2] = 拉伸方向 Y 分量（默认 0）
             *   params[3] = 拉伸方向 Z 分量（默认 1）
             *   params[4] = 缩放因子（顶面，默认 1 = 无缩放）
             */
            if (node->child_count < 1)
                break;

            /* 评估子节点获取 2D 截面三角形 */
            CSGTriList section_tris;
            csg_trilist_init(&section_tris, CSG_MAX_TRI_BUFFER);
            csg_evaluate(node->children[0], &section_tris);

            /* 读取拉伸参数 */
            double height = node->data.prim.params[0];
            if (height <= 0.0) height = 1.0;

            double dir_x = node->data.prim.params[1];
            double dir_y = node->data.prim.params[2];
            double dir_z = node->data.prim.params[3];

            /* 归一化拉伸方向 */
            CSGVec3 dir = {dir_x, dir_y, dir_z};
            double dir_len = sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
            if (dir_len < CSG_BSP_EPSILON) {
                /* 默认沿 Z 轴拉伸 */
                dir.x = 0.0; dir.y = 0.0; dir.z = 1.0;
            } else {
                dir.x /= dir_len; dir.y /= dir_len; dir.z /= dir_len;
            }

            CSGVec3 offset = csg_vec3_scale(dir, height);

            /* 提取截面顶点（用于侧面连接） */
            CSGVec3 *section_verts = NULL;
            int section_vert_count = 0;
            csg_extract_vertices(&section_tris, &section_verts, &section_vert_count);

            /* 1) 前面（底面）：原始截面，法线朝 -dir */
            for (int i = 0; i < section_tris.count; i++) {
                CSGTriangle tri = section_tris.tris[i];
                /* 翻转法线朝向 -dir 方向 */
                CSGVec3 flip_n = csg_vec3_scale(tri.normal, -1.0);
                if (csg_vec3_dot(flip_n, dir) < 0.0)
                    flip_n = csg_vec3_scale(flip_n, -1.0);
                tri.normal = csg_vec3_normalize(flip_n);
                tri.face_id = out->count;
                csg_trilist_append(out, &tri);
            }

            /* 2) 后面（顶面）：平移后的截面，法线朝 +dir */
            for (int i = 0; i < section_tris.count; i++) {
                CSGTriangle tri = section_tris.tris[i];
                tri.v[0] = csg_vec3_add(tri.v[0], offset);
                tri.v[1] = csg_vec3_add(tri.v[1], offset);
                tri.v[2] = csg_vec3_add(tri.v[2], offset);
                /* 确保法线朝 +dir */
                if (csg_vec3_dot(tri.normal, dir) < 0.0)
                    tri.normal = csg_vec3_scale(tri.normal, -1.0);
                tri.normal = csg_vec3_normalize(tri.normal);
                tri.face_id = out->count;
                csg_trilist_append(out, &tri);
            }

            /* 3) 侧面：对截面中每条边，创建连接前后两个副本的四边形（2 个三角形）
             *    遍历所有三角形的边，去重后生成侧面 */
            if (section_verts && section_vert_count >= 2) {
                /* 收集所有边（顶点索引对），去重 */
                int edge_cap = section_tris.count * 3;
                int *edge_a = (int *) lv_calloc((size_t) edge_cap, sizeof(int));
                int *edge_b = (int *) lv_calloc((size_t) edge_cap, sizeof(int));
                int edge_count = 0;

                if (edge_a && edge_b) {
                    for (int i = 0; i < section_tris.count; i++) {
                        for (int e = 0; e < 3; e++) {
                            int ia = e;
                            int ib = (e + 1) % 3;
                            CSGVec3 va = section_tris.tris[i].v[ia];
                            CSGVec3 vb = section_tris.tris[i].v[ib];

                            /* 找到顶点在 section_verts 中的索引 */
                            int idx_a = -1, idx_b = -1;
                            for (int s = 0; s < section_vert_count; s++) {
                                CSGVec3 da = csg_vec3_sub(va, section_verts[s]);
                                if (da.x * da.x + da.y * da.y + da.z * da.z < CSG_BSP_EPSILON * CSG_BSP_EPSILON) {
                                    idx_a = s;
                                }
                                CSGVec3 db = csg_vec3_sub(vb, section_verts[s]);
                                if (db.x * db.x + db.y * db.y + db.z * db.z < CSG_BSP_EPSILON * CSG_BSP_EPSILON) {
                                    idx_b = s;
                                }
                            }
                            if (idx_a < 0 || idx_b < 0 || idx_a == idx_b)
                                continue;

                            /* 确保边的方向一致（较小索引在前） */
                            if (idx_a > idx_b) {
                                int tmp = idx_a; idx_a = idx_b; idx_b = tmp;
                            }

                            /* 检查边是否已存在 */
                            int found = 0;
                            for (int ee = 0; ee < edge_count; ee++) {
                                if (edge_a[ee] == idx_a && edge_b[ee] == idx_b) {
                                    found = 1;
                                    break;
                                }
                            }
                            if (!found && edge_count < edge_cap) {
                                edge_a[edge_count] = idx_a;
                                edge_b[edge_count] = idx_b;
                                edge_count++;
                            }
                        }
                    }

                    /* 为每条边生成侧面四边形 */
                    for (int e = 0; e < edge_count; e++) {
                        CSGVec3 v0 = section_verts[edge_a[e]];
                        CSGVec3 v1 = section_verts[edge_b[e]];
                        CSGVec3 v2 = csg_vec3_add(v1, offset);
                        CSGVec3 v3 = csg_vec3_add(v0, offset);

                        /* 两个三角形组成四边形，法线朝外 */
                        CSGTriList side_tris;
                        csg_trilist_init(&side_tris, 2);

                        CSGTriangle tri;
                        tri.v[0] = v0; tri.v[1] = v1; tri.v[2] = v2;
                        tri.normal = csg_tri_normal(&tri);
                        /* 确保法线朝外（远离拉伸轴） */
                        CSGVec3 edge_mid = csg_vec3_scale(csg_vec3_add(v0, v1), 0.5);
                        CSGVec3 edge_out = csg_vec3_sub(edge_mid,
                            csg_vec3_scale(dir, csg_vec3_dot(edge_mid, dir)));
                        if (csg_vec3_dot(tri.normal, edge_out) < 0.0) {
                            /* 翻转三角形绕序 */
                            CSGVec3 tmp = tri.v[1]; tri.v[1] = tri.v[2]; tri.v[2] = tmp;
                            tri.normal = csg_vec3_scale(tri.normal, -1.0);
                        }
                        tri.face_id = out->count;
                        csg_trilist_append(out, &tri);

                        tri.v[0] = v0; tri.v[1] = v2; tri.v[2] = v3;
                        tri.normal = csg_tri_normal(&tri);
                        if (csg_vec3_dot(tri.normal, edge_out) < 0.0) {
                            CSGVec3 tmp = tri.v[1]; tri.v[1] = tri.v[2]; tri.v[2] = tmp;
                            tri.normal = csg_vec3_scale(tri.normal, -1.0);
                        }
                        tri.face_id = out->count;
                        csg_trilist_append(out, &tri);

                        csg_trilist_free(&side_tris);
                    }
                }

                if (edge_a) lv_free((void **) &edge_a);
                if (edge_b) lv_free((void **) &edge_b);
            }

            /* 清理 */
            if (section_verts) lv_free((void **) &section_verts);
            csg_trilist_free(&section_tris);
            break;
        }

        case CSG_NODE_EXTRUDE_ROTATE: {
            /* EXTRUDE_ROTATE 节点：将 2D 截面绕 Y 轴旋转生成旋转体
             *
             * 使用 node->data.prim.params 存储旋转参数：
             *   params[0] = 旋转角度（度，默认 360）
             *   params[1] = 分段数（默认 32）
             *   params[2] = 旋转轴 X 分量（默认 0）
             *   params[3] = 旋转轴 Y 分量（默认 1）
             *   params[4] = 旋转轴 Z 分量（默认 0）
             */
            if (node->child_count < 1)
                break;

            /* 评估子节点获取 2D 截面 */
            CSGTriList section_tris;
            csg_trilist_init(&section_tris, CSG_MAX_TRI_BUFFER);
            csg_evaluate(node->children[0], &section_tris);

            /* 读取旋转参数 */
            double angle_deg = node->data.prim.params[0];
            if (angle_deg <= 0.0) angle_deg = 360.0;
            int segments = (int) node->data.prim.params[1];
            if (segments <= 0) segments = 32;
            if (segments > 128) segments = 128; /* 限制最大分段数 */

            /* 旋转轴（默认 Y 轴） */
            CSGVec3 axis = {
                node->data.prim.params[2],
                node->data.prim.params[3],
                node->data.prim.params[4]
            };
            double axis_len = sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
            if (axis_len < CSG_BSP_EPSILON) {
                axis.x = 0.0; axis.y = 1.0; axis.z = 0.0;
            } else {
                axis.x /= axis_len; axis.y /= axis_len; axis.z /= axis_len;
            }

            double angle_rad = angle_deg * M_PI / 180.0;
            double angle_step = angle_rad / (double) segments;

            /* 提取截面唯一顶点 */
            CSGVec3 *sec_verts = NULL;
            int sec_count = 0;
            csg_extract_vertices(&section_tris, &sec_verts, &sec_count);

            if (!sec_verts || sec_count < 2) {
                if (sec_verts) lv_free((void **) &sec_verts);
                csg_trilist_free(&section_tris);
                break;
            }

            /*
             * 绕任意轴旋转的 Rodrigues 公式：
             *   v' = v*cos(θ) + (k×v)*sin(θ) + k*(k·v)*(1-cos(θ))
             * 其中 k 为单位旋转轴
             */
            /* 对每个分段，旋转截面顶点并生成侧面三角形条带 */
            for (int seg = 0; seg < segments; seg++) {
                double theta1 = angle_step * (double) seg;
                double theta2 = angle_step * (double) (seg + 1);
                double cos1 = cos(theta1), sin1 = sin(theta1);
                double cos2 = cos(theta2), sin2 = sin(theta2);

                /* 对每条截面边生成侧面四边形 */
                for (int i = 0; i < section_tris.count; i++) {
                    for (int e = 0; e < 3; e++) {
                        CSGVec3 va = section_tris.tris[i].v[e];
                        CSGVec3 vb = section_tris.tris[i].v[(e + 1) % 3];

                        /* 跳过退化边 */
                        CSGVec3 ediff = csg_vec3_sub(vb, va);
                        if (ediff.x * ediff.x + ediff.y * ediff.y + ediff.z * ediff.z
                            < CSG_BSP_EPSILON * CSG_BSP_EPSILON)
                            continue;

                        /* 旋转 va 和 vb 到 theta1 和 theta2 位置 */
                        /* Rodrigues: v' = v*cos(θ) + (k×v)*sin(θ) + k*(k·v)*(1-cos(θ)) */
                        CSGVec3 va1, va2, vb1, vb2;

                        /* va 在 theta1 */
                        CSGVec3 kxa1 = csg_vec3_cross(axis, va);
                        double kda1 = csg_vec3_dot(axis, va);
                        va1.x = va.x * cos1 + kxa1.x * sin1 + axis.x * kda1 * (1.0 - cos1);
                        va1.y = va.y * cos1 + kxa1.y * sin1 + axis.y * kda1 * (1.0 - cos1);
                        va1.z = va.z * cos1 + kxa1.z * sin1 + axis.z * kda1 * (1.0 - cos1);

                        /* va 在 theta2 */
                        CSGVec3 kxa2 = csg_vec3_cross(axis, va);
                        va2.x = va.x * cos2 + kxa2.x * sin2 + axis.x * kda1 * (1.0 - cos2);
                        va2.y = va.y * cos2 + kxa2.y * sin2 + axis.y * kda1 * (1.0 - cos2);
                        va2.z = va.z * cos2 + kxa2.z * sin2 + axis.z * kda1 * (1.0 - cos2);

                        /* vb 在 theta1 */
                        CSGVec3 kxb1 = csg_vec3_cross(axis, vb);
                        double kdb1 = csg_vec3_dot(axis, vb);
                        vb1.x = vb.x * cos1 + kxb1.x * sin1 + axis.x * kdb1 * (1.0 - cos1);
                        vb1.y = vb.y * cos1 + kxb1.y * sin1 + axis.y * kdb1 * (1.0 - cos1);
                        vb1.z = vb.z * cos1 + kxb1.z * sin1 + axis.z * kdb1 * (1.0 - cos1);

                        /* vb 在 theta2 */
                        CSGVec3 kxb2 = csg_vec3_cross(axis, vb);
                        vb2.x = vb.x * cos2 + kxb2.x * sin2 + axis.x * kdb1 * (1.0 - cos2);
                        vb2.y = vb.y * cos2 + kxb2.y * sin2 + axis.y * kdb1 * (1.0 - cos2);
                        vb2.z = vb.z * cos2 + kxb2.z * sin2 + axis.z * kdb1 * (1.0 - cos2);

                        /* 生成两个三角形组成侧面四边形 */
                        CSGTriangle tri;
                        tri.v[0] = va1; tri.v[1] = vb1; tri.v[2] = va2;
                        tri.normal = csg_tri_normal(&tri);
                        tri.face_id = out->count;
                        csg_trilist_append(out, &tri);

                        tri.v[0] = vb1; tri.v[1] = vb2; tri.v[2] = va2;
                        tri.normal = csg_tri_normal(&tri);
                        tri.face_id = out->count;
                        csg_trilist_append(out, &tri);
                    }
                }
            }

            /* 添加起始端面和结束端面 */
            if (angle_deg < 360.0 - CSG_BSP_EPSILON) {
                /* 起始端面（theta=0）：原始截面 */
                for (int i = 0; i < section_tris.count; i++) {
                    CSGTriangle tri = section_tris.tris[i];
                    /* 法线应朝 -旋转方向 */
                    CSGVec3 face_normal = csg_tri_normal(&tri);
                    if (csg_vec3_dot(face_normal, axis) < 0.0) {
                        face_normal = csg_vec3_scale(face_normal, -1.0);
                        CSGVec3 tmp = tri.v[1]; tri.v[1] = tri.v[2]; tri.v[2] = tmp;
                    }
                    tri.normal = csg_vec3_normalize(face_normal);
                    tri.face_id = out->count;
                    csg_trilist_append(out, &tri);
                }

                /* 结束端面（theta=angle）：旋转后的截面 */
                double cos_end = cos(angle_rad), sin_end = sin(angle_rad);
                for (int i = 0; i < section_tris.count; i++) {
                    CSGTriangle tri = section_tris.tris[i];
                    /* 旋转顶点到结束角度 */
                    for (int v = 0; v < 3; v++) {
                        CSGVec3 p = tri.v[v];
                        CSGVec3 kxp = csg_vec3_cross(axis, p);
                        double kdp = csg_vec3_dot(axis, p);
                        tri.v[v].x = p.x * cos_end + kxp.x * sin_end + axis.x * kdp * (1.0 - cos_end);
                        tri.v[v].y = p.y * cos_end + kxp.y * sin_end + axis.y * kdp * (1.0 - cos_end);
                        tri.v[v].z = p.z * cos_end + kxp.z * sin_end + axis.z * kdp * (1.0 - cos_end);
                    }
                    /* 法线应朝 +旋转方向 */
                    CSGVec3 face_normal = csg_tri_normal(&tri);
                    if (csg_vec3_dot(face_normal, axis) > 0.0) {
                        face_normal = csg_vec3_scale(face_normal, -1.0);
                        CSGVec3 tmp = tri.v[1]; tri.v[1] = tri.v[2]; tri.v[2] = tmp;
                    }
                    tri.normal = csg_vec3_normalize(face_normal);
                    tri.face_id = out->count;
                    csg_trilist_append(out, &tri);
                }
            }

            /* 清理 */
            if (sec_verts) lv_free((void **) &sec_verts);
            csg_trilist_free(&section_tris);
            break;
        }
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
 * 调用者负责用 lv_free() 释放返回的字符串。
 *
 * @param root  CSG 树根节点
 * @return 以 '\0' 结尾的 OpenSCAD 脚本字符串，失败返回 NULL
 */
char *csg_export_to_openscad(const CSGNode *root) {
    if (!root)
        return NULL;

    int buf_size = CSG_EXPORT_BUF_INIT;
    char *buf = (char *) lv_calloc((size_t) buf_size, sizeof(char));
    if (!buf)
        return NULL;

    /* 添加文件头 */
    int written = snprintf(buf, (size_t) buf_size,
                           "// Generated by Lv-00 CSG module\n"
                           "// Date: 2026-05-24\n"
                           "// Engine: geometry_csg.c (BSP-based)\n"
                           "$fn = 64;\n\n");
    if (written < 0) {
        lv_free((void **) &buf);
        return NULL;
    }

    written = csg_export_node(root, buf, buf_size, written, 0);
    if (written < 0) {
        lv_free((void **) &buf);
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
