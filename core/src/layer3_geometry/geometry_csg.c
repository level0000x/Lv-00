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

#include "lv/lv_platform.h"
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
        if (list->capacity > INT_MAX / 2)
            return;
        int new_cap = list->capacity * 2;
        CSGTriangle *new_tris = (CSGTriangle *) lv_realloc(list->tris, (size_t) new_cap * sizeof(CSGTriangle));
        if (!new_tris)
            return;
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
/* --- 包围盒计算：函数指针表 --- */
typedef void (*CSGBBoxFunc)(CSGNode *node);

static void bbox_primitive(CSGNode *node) {
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
            break;
    }
}

static void bbox_merge_children(CSGNode *node) {
    for (int i = 0; i < node->child_count; i++) {
        csg_node_init_bbox(node->children[i]);
    }
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
}

static CSGBBoxFunc s_bbox_funcs[] = {
    [CSG_NODE_PRIMITIVE] = bbox_primitive,
    [CSG_NODE_UNION] = bbox_merge_children,
    [CSG_NODE_DIFFERENCE] = bbox_merge_children,
    [CSG_NODE_INTERSECTION] = bbox_merge_children,
    [CSG_NODE_HULL] = bbox_merge_children,
    [CSG_NODE_MINKOWSKI] = bbox_merge_children,
    [CSG_NODE_TRANSFORM] = bbox_merge_children,
    [CSG_NODE_EXTRUDE_LINEAR] = bbox_merge_children,
    [CSG_NODE_EXTRUDE_ROTATE] = bbox_merge_children,
};
static const int s_bbox_func_count = (int)(sizeof(s_bbox_funcs) / sizeof(s_bbox_funcs[0]));

void csg_node_init_bbox(CSGNode *node) {
    if (!node)
        return;
    if (node->kind >= 0 && node->kind < s_bbox_func_count && s_bbox_funcs[node->kind]) {
        s_bbox_funcs[node->kind](node);
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
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "csg_cylinder_create: node allocation failed");
    node->data.prim.type = 2;
    node->data.prim.params[0] = fabs(radius);
    node->data.prim.params[1] = fabs(height);
    csg_node_init_bbox(node);
    return node;
}

/**
 * @brief 创建长方体/盒子图元
 * @param width   宽度（X 轴方向）
 * @param height  高度（Y 轴方向）
 * @param depth   深度（Z 轴方向）
 * @return 新 CSGNode（PRIMITIVE 类型，type=1）
 */
CSGNode *csg_box_create(double width, double height, double depth) {
    return csg_cube_create(width, height, depth);
}

/**
 * @brief 创建圆锥/圆台图元
 * @param radius1  下底面半径
 * @param radius2  上底面半径
 * @param height   高度（Z 轴方向）
 * @return 新 CSGNode（PRIMITIVE 类型，type=3）
 */
CSGNode *csg_cone_create(double radius1, double radius2, double height) {
    CSGNode *node = csg_node_create(CSG_NODE_PRIMITIVE);
    if (!node)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "csg_cone_create: node allocation failed");
    node->data.prim.type = 3;
    node->data.prim.params[0] = fabs(radius1);
    node->data.prim.params[1] = fabs(radius2);
    node->data.prim.params[2] = fabs(height);
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
 * BSP 树数据结构
 * ================================================================ */

/**
 * @brief 三角形相对于平面的分类
 */
typedef enum {
    CSG_BSP_FRONT = 0,   /**< 完全在平面前方（法向一侧） */
    CSG_BSP_BACK  = 1,   /**< 完全在平面后方（法向反向一侧） */
    CSG_BSP_ON    = 2,   /**< 完全在平面上 */
    CSG_BSP_SPLIT = 3    /**< 横跨平面 */
} CSGBSPClass;

/**
 * @brief BSP 树节点
 *
 * 每个节点存储一个分割平面（由平面上一点和法线定义），
 * 以及与该平面共面的三角形列表。
 * front/back 子树分别对应平面的正/负半空间。
 */
typedef struct CSGBSPNode {
    CSGVec3 plane_point;           /**< 平面上一点 */
    CSGVec3 plane_normal;          /**< 平面法线（单位向量） */
    struct CSGBSPNode *front;      /**< 前半空间子树（法向侧） */
    struct CSGBSPNode *back;       /**< 后半空间子树（法向反侧） */
    CSGTriangle *tris;             /**< 与该平面共面的三角形数组 */
    int tri_count;                 /**< 共面三角形数量 */
    int tri_capacity;              /**< 共面三角形数组容量 */
} CSGBSPNode;

/* ================================================================
 * BSP 树构建与操作
 * ================================================================ */

/**
 * @brief 创建一个 BSP 树节点
 */
static CSGBSPNode *csg_bsp_node_create(void) {
    CSGBSPNode *node = (CSGBSPNode *) lv_calloc(1, sizeof(CSGBSPNode));
    if (!node)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "csg_bsp_node_create: allocation failed");
    node->front = NULL;
    node->back = NULL;
    node->tris = NULL;
    node->tri_count = 0;
    node->tri_capacity = 0;
    return node;
}

/**
 * @brief 向 BSP 节点添加一个共面三角形
 */
static void csg_bsp_node_add_tri(CSGBSPNode *node, const CSGTriangle *tri) {
    if (!node || !tri)
        return;
    if (node->tri_count >= node->tri_capacity) {
        int new_cap = node->tri_capacity > 0 ? node->tri_capacity * 2 : 4;
        CSGTriangle *new_tris = (CSGTriangle *) lv_realloc(node->tris, (size_t) new_cap * sizeof(CSGTriangle));
        if (!new_tris)
            return;
        node->tris = new_tris;
        node->tri_capacity = new_cap;
    }
    node->tris[node->tri_count] = *tri;
    node->tri_count++;
}

/**
 * @brief 递归销毁 BSP 树
 */
static void csg_bsp_node_destroy(CSGBSPNode *node) {
    if (!node)
        return;
    csg_bsp_node_destroy(node->front);
    csg_bsp_node_destroy(node->back);
    if (node->tris)
        lv_free((void **) &node->tris);
    lv_free((void **) &node);
}

/**
 * @brief 将三角形相对于分割平面分类
 *
 * @param node   BSP 节点（含分割平面）
 * @param tri    待分类三角形
 * @param eps    距离容差
 * @return 分类结果（FRONT / BACK / ON / SPLIT）
 */
static CSGBSPClass csg_bsp_classify_triangle(const CSGBSPNode *node, const CSGTriangle *tri, double eps) {
    int pos = 0, neg = 0;
    for (int i = 0; i < 3; i++) {
        double d = csg_signed_distance(node->plane_point, node->plane_normal, tri->v[i]);
        if (d > eps)
            pos++;
        else if (d < -eps)
            neg++;
    }
    if (pos > 0 && neg == 0) return CSG_BSP_FRONT;
    if (neg > 0 && pos == 0) return CSG_BSP_BACK;
    if (pos == 0 && neg == 0) return CSG_BSP_ON;
    return CSG_BSP_SPLIT;
}

/**
 * @brief 用分割平面切割三角形
 *
 * 当三角形横跨分割平面时，将其分为前半部分和后半部分。
 *
 * @param tri          待切割三角形
 * @param plane_point  平面上一点
 * @param plane_normal 平面法线
 * @param eps          距离容差
 * @param front_list   输出：前半部分三角形列表
 * @param back_list    输出：后半部分三角形列表
 */
static void csg_bsp_split_triangle(const CSGTriangle *tri, CSGVec3 plane_point, CSGVec3 plane_normal, double eps,
                                    CSGTriList *front_list, CSGTriList *back_list) {
    /* 计算每个顶点相对于平面的有符号距离 */
    double d[3];
    int sid[3]; /* 0=负侧, 1=正侧, 2=平面上 */
    int pos_count = 0, neg_count = 0;

    for (int i = 0; i < 3; i++) {
        d[i] = csg_signed_distance(plane_point, plane_normal, tri->v[i]);
        if (d[i] > eps) {
            sid[i] = 1;
            pos_count++;
        } else if (d[i] < -eps) {
            sid[i] = 0;
            neg_count++;
        } else {
            sid[i] = 2;
        }
    }

    /* 如果不在分割状态，直接返回 */
    if (pos_count == 0 || neg_count == 0)
        return;

    /* 找到被分割的边并计算插值点 */
    /* 对于每条边 (i, (i+1)%3): */
    /* 对每对顶点分类不同的边，求直线与平面的交点 */

    /* 收集正侧和负侧的顶点以及交点 */
    CSGVec3 front_verts[4];
    CSGVec3 back_verts[4];
    int front_n = 0, back_n = 0;

    for (int i = 0; i < 3; i++) {
        int j = (i + 1) % 3;

        /* 添加当前顶点到对应侧 */
        if (sid[i] == 1 || sid[i] == 2) {
            front_verts[front_n++] = tri->v[i];
        }
        if (sid[i] == 0 || sid[i] == 2) {
            back_verts[back_n++] = tri->v[i];
        }

        /* 如果边的两端在不同侧，计算交点 */
        if ((sid[i] == 1 && sid[j] == 0) || (sid[i] == 0 && sid[j] == 1)) {
            double t = d[i] / (d[i] - d[j]); /* 线性插值因子 */
            CSGVec3 edge = csg_vec3_sub(tri->v[j], tri->v[i]);
            CSGVec3 p;
            p.x = tri->v[i].x + t * edge.x;
            p.y = tri->v[i].y + t * edge.y;
            p.z = tri->v[i].z + t * edge.z;

            front_verts[front_n++] = p;
            back_verts[back_n++] = p;
        }
    }

    /* 将正侧顶点组装成三角形 */
    if (front_n >= 3) {
        CSGTriangle ftri;
        ftri.v[0] = front_verts[0];
        ftri.v[1] = front_verts[1];
        ftri.v[2] = front_verts[2];
        ftri.normal = tri->normal;
        ftri.face_id = tri->face_id;

        if (front_n == 4) {
            /* 四边形 → 两个三角形 */
            CSGTriangle ftri2;
            ftri2.v[0] = front_verts[0];
            ftri2.v[1] = front_verts[2];
            ftri2.v[2] = front_verts[3];
            ftri2.normal = tri->normal;
            ftri2.face_id = tri->face_id;

            if (front_list) {
                csg_trilist_append(front_list, &ftri);
                csg_trilist_append(front_list, &ftri2);
            }
        } else {
            if (front_list)
                csg_trilist_append(front_list, &ftri);
        }
    }

    /* 将负侧顶点组装成三角形 */
    if (back_n >= 3) {
        CSGTriangle btri;
        btri.v[0] = back_verts[0];
        btri.v[1] = back_verts[1];
        btri.v[2] = back_verts[2];
        btri.normal = tri->normal;
        btri.face_id = tri->face_id;

        if (back_n == 4) {
            CSGTriangle btri2;
            btri2.v[0] = back_verts[0];
            btri2.v[1] = back_verts[2];
            btri2.v[2] = back_verts[3];
            btri2.normal = tri->normal;
            btri2.face_id = tri->face_id;

            if (back_list) {
                csg_trilist_append(back_list, &btri);
                csg_trilist_append(back_list, &btri2);
            }
        } else {
            if (back_list)
                csg_trilist_append(back_list, &btri);
        }
    }
}

/**
 * @brief 从三角形列表构建 BSP 树
 *
 * 递归算法：
 *   1. 选择列表中的第一个三角形作为分割平面
 *   2. 将其余三角形分类为 FRONT/BACK/ON
 *   3. ON 的三角形存储在当前节点
 *   4. FRONT/BACK 的三角形递归构建子树
 *   5. 横跨分割面的三角形先切割再递归
 *
 * @param tris  三角形列表（会被修改）
 * @param eps   容差
 * @return BSP 树根节点（失败返回 NULL）
 */
static CSGBSPNode *csg_bsp_build(CSGTriList *tris, double eps) {
    if (!tris || tris->count == 0)
        return NULL;

    CSGBSPNode *node = csg_bsp_node_create();
    if (!node)
        return NULL;

    /* 选择第一个三角形作为分割平面 */
    const CSGTriangle *split_tri = &tris->tris[0];
    node->plane_point = split_tri->v[0];
    node->plane_normal = split_tri->normal;

    /* 将该三角形加入共面列表 */
    csg_bsp_node_add_tri(node, split_tri);

    /* 分类其余三角形 */
    CSGTriList front_list, back_list;
    csg_trilist_init(&front_list, 16);
    csg_trilist_init(&back_list, 16);

    for (int i = 1; i < tris->count; i++) {
        const CSGTriangle *cur = &tris->tris[i];
        CSGBSPClass cls = csg_bsp_classify_triangle(node, cur, eps);

        switch (cls) {
            case CSG_BSP_FRONT:
                csg_trilist_append(&front_list, cur);
                break;
            case CSG_BSP_BACK:
                csg_trilist_append(&back_list, cur);
                break;
            case CSG_BSP_ON:
                csg_bsp_node_add_tri(node, cur);
                break;
            case CSG_BSP_SPLIT: {
                /* 切割三角形，前半部分加入 front_list，后半部分加入 back_list */
                csg_bsp_split_triangle(cur, node->plane_point, node->plane_normal, eps, &front_list, &back_list);
                break;
            }
        }
    }

    /* 递归构建子树 */
    if (front_list.count > 0) {
        node->front = csg_bsp_build(&front_list, eps);
    }
    if (back_list.count > 0) {
        node->back = csg_bsp_build(&back_list, eps);
    }

    csg_trilist_free(&front_list);
    csg_trilist_free(&back_list);

    return node;
}

/**
 * @brief 将三角形相对于 BSP 树做裁剪
 *
 * 递归遍历 BSP 树：
 *   - 到达 NULL 节点：三角形完全在外部/内部（取决于 keep_inside），保留
 *   - SPLIT：分割后根据 keep_inside 分别递归
 *   - FRONT：在平面前方（外部），根据 keep_inside 决定是否递归
 *   - BACK：在平面后方（内部），根据 keep_inside 决定是否递归
 *   - ON：在分割平面上，保留（构成边界）
 *
 * @param tri         待裁剪三角形
 * @param node        BSP 树节点（可以为 NULL）
 * @param out         输出：裁剪后保留的三角形
 * @param eps         容差
 * @param keep_inside 非零：保留在 BSP 内部的部分；零：保留在 BSP 外部的部分
 */
static void csg_bsp_clip_triangle(const CSGTriangle *tri, const CSGBSPNode *node, CSGTriList *out, double eps, int keep_inside) {
    if (!node) {
        /* 到达叶子，保留（对于 keep_outside 是在外部；对于 keep_inside 是在内部） */
        csg_trilist_append(out, tri);
        return;
    }

    CSGBSPClass cls = csg_bsp_classify_triangle(node, tri, eps);

    switch (cls) {
        case CSG_BSP_FRONT:
            /* 在平面前方（外部半空间） */
            if (keep_inside) {
                /* 保留内部：外部部分丢弃 */
                break;
            }
            /* 保留外部：继续在前半子树中测试 */
            csg_bsp_clip_triangle(tri, node->front, out, eps, keep_inside);
            break;

        case CSG_BSP_BACK:
            /* 在平面后方（内部半空间） */
            if (keep_inside) {
                /* 保留内部：继续在后半子树中测试 */
                csg_bsp_clip_triangle(tri, node->back, out, eps, keep_inside);
                break;
            }
            /* 保留外部：内部部分丢弃 */
            break;

        case CSG_BSP_ON:
            /* 三角形在分割平面上 → 保留（构成边界） */
            csg_trilist_append(out, tri);
            break;

        case CSG_BSP_SPLIT: {
            /* 横跨平面 → 切割后分别递归 */
            CSGTriList front_list, back_list;
            csg_trilist_init(&front_list, 2);
            csg_trilist_init(&back_list, 2);

            csg_bsp_split_triangle(tri, node->plane_point, node->plane_normal, eps, &front_list, &back_list);

            if (keep_inside) {
                /* 保留内部：只递归后半部分（内部），前半部分（外部）丢弃 */
                for (int i = 0; i < back_list.count; i++) {
                    csg_bsp_clip_triangle(&back_list.tris[i], node->back, out, eps, keep_inside);
                }
            } else {
                /* 保留外部：前半部分（外部）继续在前半子树中测试 */
                for (int i = 0; i < front_list.count; i++) {
                    csg_bsp_clip_triangle(&front_list.tris[i], node->front, out, eps, keep_inside);
                }
                /* 后半部分（内部）继续在后半子树中测试 */
                for (int i = 0; i < back_list.count; i++) {
                    csg_bsp_clip_triangle(&back_list.tris[i], node->back, out, eps, keep_inside);
                }
            }

            csg_trilist_free(&front_list);
            csg_trilist_free(&back_list);
            break;
        }
    }
}

/* ================================================================
 * BSP 布尔运算核心
 * ================================================================ */

/**
 * @brief 使用 BSP 方法对两个三角形面列表执行布尔并集
 *
 * 基于 BSP 树的正确并集算法：
 *   1. 用 list_b 构建 BSP 树
 *   2. 对 list_a 每个三角形，用 BSP 树裁剪，保留在 BSP 外部的部分（A - B）
 *   3. 用 list_a 构建 BSP 树
 *   4. 对 list_b 每个三角形，用 BSP 树裁剪，保留在 BSP 外部的部分（B - A）
 *   5. 输出 = (A - B) ∪ (B - A) = A ∪ B
 *
 * @param list_a  第一个面列表
 * @param list_b  第二个面列表
 * @param out     输出：并集结果
 */
static void csg_bsp_union_tri(const CSGTriList *list_a, const CSGTriList *list_b, CSGTriList *out) {
    if (!list_a || !list_b || !out)
        return;

    /* 阶段 1：保留 list_a 中在 list_b 外部的部分 */
    if (list_b->count > 0 && list_a->count > 0) {
        CSGTriList b_copy;
        csg_trilist_init(&b_copy, list_b->count);
        for (int i = 0; i < list_b->count; i++) {
            csg_trilist_append(&b_copy, &list_b->tris[i]);
        }

        CSGBSPNode *bsp_b = csg_bsp_build(&b_copy, CSG_BSP_EPSILON);
        csg_trilist_free(&b_copy);

        if (bsp_b) {
            for (int i = 0; i < list_a->count; i++) {
                csg_bsp_clip_triangle(&list_a->tris[i], bsp_b, out, CSG_BSP_EPSILON, 0);
            }
            csg_bsp_node_destroy(bsp_b);
        } else {
            /* BSP 构建失败：回退，保留 A 的所有面 */
            for (int i = 0; i < list_a->count; i++) {
                csg_trilist_append(out, &list_a->tris[i]);
            }
        }
    } else {
        /* list_b 为空：保留 A 的所有面 */
        for (int i = 0; i < list_a->count; i++) {
            csg_trilist_append(out, &list_a->tris[i]);
        }
    }

    /* 阶段 2：保留 list_b 中在 list_a 外部的部分 */
    if (list_a->count > 0 && list_b->count > 0) {
        CSGTriList a_copy;
        csg_trilist_init(&a_copy, list_a->count);
        for (int i = 0; i < list_a->count; i++) {
            csg_trilist_append(&a_copy, &list_a->tris[i]);
        }

        CSGBSPNode *bsp_a = csg_bsp_build(&a_copy, CSG_BSP_EPSILON);
        csg_trilist_free(&a_copy);

        if (bsp_a) {
            for (int i = 0; i < list_b->count; i++) {
                csg_bsp_clip_triangle(&list_b->tris[i], bsp_a, out, CSG_BSP_EPSILON, 0);
            }
            csg_bsp_node_destroy(bsp_a);
        } else {
            /* BSP 构建失败：回退，保留 B 的所有面 */
            for (int i = 0; i < list_b->count; i++) {
                csg_trilist_append(out, &list_b->tris[i]);
            }
        }
    } else if (list_a->count == 0) {
        /* list_a 为空：保留 B 的所有面 */
        for (int i = 0; i < list_b->count; i++) {
            csg_trilist_append(out, &list_b->tris[i]);
        }
    }
}

/**
 * @brief BSP 布尔差集：list_a 减去 list_b
 *
 * 真实基于 BSP 树的差集算法：
 *   1. 用 list_b 的三角形构建 BSP 树
 *   2. 对 list_a 的每个三角形，用 BSP 树裁剪
 *   3. 在 B 内部的三角形部分被丢弃
 *   4. 在 B 外部的三角形部分被保留
 *   5. 横跨 B 表面的三角形被分割后只保留外部部分
 *
 * @param list_a  被减体面列表
 * @param list_b  减体面列表
 * @param out     输出：差集结果
 */
static void csg_bsp_difference_tri(const CSGTriList *list_a, const CSGTriList *list_b, CSGTriList *out) {
    if (!list_a || !list_b || !out)
        return;
    if (list_b->count == 0) {
        /* 减体为空：保留 A 的所有三角形 */
        for (int i = 0; i < list_a->count; i++) {
            csg_trilist_append(out, &list_a->tris[i]);
        }
        return;
    }

    /* 从 list_b 构建 BSP 树 */
    /* 需要复制一份 list_b，因为 bsp_build 会消耗列表 */
    CSGTriList b_copy;
    csg_trilist_init(&b_copy, list_b->count);
    for (int i = 0; i < list_b->count; i++) {
        csg_trilist_append(&b_copy, &list_b->tris[i]);
    }

    CSGBSPNode *bsp_tree = csg_bsp_build(&b_copy, CSG_BSP_EPSILON);
    csg_trilist_free(&b_copy);

    if (!bsp_tree) {
        /* BSP 树构建失败：回退，保留 A 的所有面 */
        for (int i = 0; i < list_a->count; i++) {
            csg_trilist_append(out, &list_a->tris[i]);
        }
        return;
    }

    /* 用 BSP 树裁剪 list_a 中的每个三角形 */
    for (int i = 0; i < list_a->count; i++) {
        csg_bsp_clip_triangle(&list_a->tris[i], bsp_tree, out, CSG_BSP_EPSILON, 0);
    }

    /* 清理 BSP 树 */
    csg_bsp_node_destroy(bsp_tree);
}

/**
 * @brief BSP 布尔交集：list_a 与 list_b 的交集
 *
 * 基于 BSP 树的正确交集算法：
 *   1. 用 list_b 构建 BSP 树
 *   2. 对 list_a 每个三角形，用 BSP 树裁剪，保留在 BSP 内部的部分（A ∩ B）
 *   3. 用 list_a 构建 BSP 树
 *   4. 对 list_b 每个三角形，用 BSP 树裁剪，保留在 BSP 内部的部分（B ∩ A）
 *   5. 输出 = (A ∩ B) ∪ (B ∩ A) = A ∩ B
 *
 * @param list_a  第一个面列表
 * @param list_b  第二个面列表
 * @param out     输出：交集结果
 */
static void csg_bsp_intersection_tri(const CSGTriList *list_a, const CSGTriList *list_b, CSGTriList *out) {
    if (!list_a || !list_b || !out)
        return;
    if (list_a->count == 0 || list_b->count == 0) {
        /* 其中一者为空：交集为空 */
        return;
    }

    /* 阶段 1：保留 list_a 中在 list_b 内部的部分 */
    CSGTriList b_copy;
    csg_trilist_init(&b_copy, list_b->count);
    for (int i = 0; i < list_b->count; i++) {
        csg_trilist_append(&b_copy, &list_b->tris[i]);
    }

    CSGBSPNode *bsp_b = csg_bsp_build(&b_copy, CSG_BSP_EPSILON);
    csg_trilist_free(&b_copy);

    if (bsp_b) {
        for (int i = 0; i < list_a->count; i++) {
            csg_bsp_clip_triangle(&list_a->tris[i], bsp_b, out, CSG_BSP_EPSILON, 1);
        }
        csg_bsp_node_destroy(bsp_b);
    }

    /* 阶段 2：保留 list_b 中在 list_a 内部的部分 */
    CSGTriList a_copy;
    csg_trilist_init(&a_copy, list_a->count);
    for (int i = 0; i < list_a->count; i++) {
        csg_trilist_append(&a_copy, &list_a->tris[i]);
    }

    CSGBSPNode *bsp_a = csg_bsp_build(&a_copy, CSG_BSP_EPSILON);
    csg_trilist_free(&a_copy);

    if (bsp_a) {
        for (int i = 0; i < list_b->count; i++) {
            csg_bsp_clip_triangle(&list_b->tris[i], bsp_a, out, CSG_BSP_EPSILON, 1);
        }
        csg_bsp_node_destroy(bsp_a);
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

    /* 计算顶点坐标最大绝对值，用于相对 epsilon 缩放 */
    double max_vertex_abs = 0.0;
    for (int i = 0; i < vertex_count; i++) {
        max_vertex_abs = fmax(max_vertex_abs, fabs(vertices[i].x));
        max_vertex_abs = fmax(max_vertex_abs, fabs(vertices[i].y));
        max_vertex_abs = fmax(max_vertex_abs, fabs(vertices[i].z));
    }
    double csg_hull_eps = CSG_BSP_EPSILON * fmax(1.0, max_vertex_abs);

    /* 暴力枚举所有三元组 */
    for (int i = 0; i < vertex_count - 2; i++) {
        for (int j = i + 1; j < vertex_count - 1; j++) {
            for (int k = j + 1; k < vertex_count; k++) {
                CSGVec3 e1 = csg_vec3_sub(vertices[j], vertices[i]);
                CSGVec3 e2 = csg_vec3_sub(vertices[k], vertices[i]);
                CSGVec3 normal = csg_vec3_normalize(csg_vec3_cross(e1, e2));

                /* 退化三角形（面积为零），跳过 */
                double nlen = sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
                if (nlen < csg_hull_eps)
                    continue;

                /* 检查所有其他顶点是否在面的同一侧 */
                int pos_count = 0;
                int neg_count = 0;
                int valid = 1;

                for (int m = 0; m < vertex_count; m++) {
                    if (m == i || m == j || m == k)
                        continue;

                    double d = csg_signed_distance(vertices[i], normal, vertices[m]);
                    if (d > csg_hull_eps)
                        pos_count++;
                    else if (d < -csg_hull_eps)
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
            /* 使用相对容差：计算已存顶点的最大量级 */
            double max_coord = 0.0;
            for (int j = 0; j < count; j++) {
                double cx = fabs(verts[j].x), cy = fabs(verts[j].y), cz = fabs(verts[j].z);
                if (cx > max_coord)
                    max_coord = cx;
                if (cy > max_coord)
                    max_coord = cy;
                if (cz > max_coord)
                    max_coord = cz;
            }
            double eps_sq = CSG_BSP_EPSILON * CSG_BSP_EPSILON * (1.0 + max_coord * max_coord);
            for (int j = 0; j < count; j++) {
                CSGVec3 d = csg_vec3_sub(pt, verts[j]);
                double dist2 = d.x * d.x + d.y * d.y + d.z * d.z;
                if (dist2 < eps_sq) {
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

/* --- CSG 树评估：函数指针表 --- */
typedef void (*CSGEvalFunc)(const CSGNode *node, CSGTriList *out);

/* 前向声明：提取的函数需要递归调用 csg_evaluate */
static void eval_csg_bool(const CSGNode *node, CSGTriList *out);
static void eval_csg_transform(const CSGNode *node, CSGTriList *out);
static void eval_csg_hull(const CSGNode *node, CSGTriList *out);
static void eval_csg_minkowski(const CSGNode *node, CSGTriList *out);
static void eval_csg_extrude_linear(const CSGNode *node, CSGTriList *out);
static void eval_csg_extrude_rotate(const CSGNode *node, CSGTriList *out);

/* csg_evaluate 前向声明（提取的函数递归调用它） */
void csg_evaluate(const CSGNode *node, CSGTriList *out);

static void eval_csg_primitive(const CSGNode *node, CSGTriList *out) {
    csg_primitive_to_tris(node, out);
}

static void eval_csg_bool(const CSGNode *node, CSGTriList *out) {
    if (node->child_count < 2) {
        if (node->child_count == 1) {
            csg_evaluate(node->children[0], out);
        }
        return;
    }

    CSGTriList tris_a;
    csg_trilist_init(&tris_a, CSG_MAX_TRI_BUFFER);
    csg_evaluate(node->children[0], &tris_a);

    CSGTriList tris_b;
    csg_trilist_init(&tris_b, CSG_MAX_TRI_BUFFER);
    csg_evaluate(node->children[1], &tris_b);

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
}

static void eval_csg_transform(const CSGNode *node, CSGTriList *out) {
    if (node->child_count < 1)
        return;

    CSGTriList child_tris;
    csg_trilist_init(&child_tris, CSG_MAX_TRI_BUFFER);
    csg_evaluate(node->children[0], &child_tris);

    const double (*M)[4] = node->transform;

    double m00 = M[0][0], m01 = M[0][1], m02 = M[0][2];
    double m10 = M[1][0], m11 = M[1][1], m12 = M[1][2];
    double m20 = M[2][0], m21 = M[2][1], m22 = M[2][2];

    double det = m00 * (m11 * m22 - m12 * m21) - m01 * (m10 * m22 - m12 * m20) + m02 * (m10 * m21 - m11 * m20);

    /* 计算矩阵元素绝对值的最大值（用于行列式容差缩放） */
    const double m_els[9] = {m00, m01, m02,
                             m10, m11, m12,
                             m20, m21, m22};
    double max_el = lv_max_abs(m_els, 9);
    double det_tol = CSG_BSP_EPSILON * fmax(1.0, max_el * max_el);

    double inv_det;
    double invT[3][3];
    if (fabs(det) < det_tol) {
        invT[0][0] = 1.0; invT[0][1] = 0.0; invT[0][2] = 0.0;
        invT[1][0] = 0.0; invT[1][1] = 1.0; invT[1][2] = 0.0;
        invT[2][0] = 0.0; invT[2][1] = 0.0; invT[2][2] = 1.0;
    } else {
        inv_det = 1.0 / det;
        invT[0][0] = (m11 * m22 - m12 * m21) * inv_det;
        invT[0][1] = (m02 * m21 - m01 * m22) * inv_det;
        invT[0][2] = (m01 * m12 - m02 * m11) * inv_det;
        invT[1][0] = (m12 * m20 - m10 * m22) * inv_det;
        invT[1][1] = (m00 * m22 - m02 * m20) * inv_det;
        invT[1][2] = (m02 * m10 - m00 * m12) * inv_det;
        invT[2][0] = (m10 * m21 - m11 * m20) * inv_det;
        invT[2][1] = (m01 * m20 - m00 * m21) * inv_det;
        invT[2][2] = (m00 * m11 - m01 * m10) * inv_det;
    }

    for (int i = 0; i < child_tris.count; i++) {
        CSGTriangle tri = child_tris.tris[i];
        for (int v = 0; v < 3; v++) {
            double x = tri.v[v].x;
            double y = tri.v[v].y;
            double z = tri.v[v].z;
            double w = M[3][0] * x + M[3][1] * y + M[3][2] * z + M[3][3];
            double inv_w = (fabs(w - 1.0) > CSG_BSP_EPSILON) ? 1.0 / w : 1.0;
            tri.v[v].x = (M[0][0] * x + M[0][1] * y + M[0][2] * z + M[0][3]) * inv_w;
            tri.v[v].y = (M[1][0] * x + M[1][1] * y + M[1][2] * z + M[1][3]) * inv_w;
            tri.v[v].z = (M[2][0] * x + M[2][1] * y + M[2][2] * z + M[2][3]) * inv_w;
        }
        double nx = tri.normal.x, ny = tri.normal.y, nz = tri.normal.z;
        tri.normal.x = invT[0][0] * nx + invT[0][1] * ny + invT[0][2] * nz;
        tri.normal.y = invT[1][0] * nx + invT[1][1] * ny + invT[1][2] * nz;
        tri.normal.z = invT[2][0] * nx + invT[2][1] * ny + invT[2][2] * nz;
        tri.normal = csg_vec3_normalize(tri.normal);
        tri.face_id = out->count;
        csg_trilist_append(out, &tri);
    }
    csg_trilist_free(&child_tris);
}
static void eval_csg_hull(const CSGNode *node, CSGTriList *out) {
    if (node->child_count < 1)
        return;

    CSGTriList all_tris;
    csg_trilist_init(&all_tris, CSG_MAX_TRI_BUFFER * node->child_count);
    for (int i = 0; i < node->child_count; i++) {
        csg_evaluate(node->children[i], &all_tris);
    }

    CSGVec3 *hull_verts = NULL;
    int hull_vert_count = 0;
    csg_extract_vertices(&all_tris, &hull_verts, &hull_vert_count);

    if (hull_verts && hull_vert_count >= 4) {
        csg_compute_convex_hull(hull_verts, hull_vert_count, out);
    }

    if (hull_verts) {
        lv_free((void **) &hull_verts);
    }
    csg_trilist_free(&all_tris);
}

static void eval_csg_minkowski(const CSGNode *node, CSGTriList *out) {
    if (node->child_count < 2)
        return;

    CSGTriList tris_a, tris_b;
    csg_trilist_init(&tris_a, CSG_MAX_TRI_BUFFER);
    csg_trilist_init(&tris_b, CSG_MAX_TRI_BUFFER);
    csg_evaluate(node->children[0], &tris_a);
    csg_evaluate(node->children[1], &tris_b);

    CSGVec3 *verts_a = NULL, *verts_b = NULL;
    int count_a = 0, count_b = 0;
    csg_extract_vertices(&tris_a, &verts_a, &count_a);
    csg_extract_vertices(&tris_b, &verts_b, &count_b);

    if (verts_a && verts_b && count_a > 0 && count_b > 0) {
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
                csg_compute_convex_hull(sum_verts, sum_count, out);
                lv_free((void **) &sum_verts);
            }
        }
    }

    if (verts_a)
        lv_free((void **) &verts_a);
    if (verts_b)
        lv_free((void **) &verts_b);
    csg_trilist_free(&tris_a);
    csg_trilist_free(&tris_b);
}

static void eval_csg_extrude_linear(const CSGNode *node, CSGTriList *out) {
    if (node->child_count < 1)
        return;

    CSGTriList section_tris;
    csg_trilist_init(&section_tris, CSG_MAX_TRI_BUFFER);
    csg_evaluate(node->children[0], &section_tris);

    double height = node->data.prim.params[0];
    if (height <= 0.0)
        height = 1.0;

    double dir_x = node->data.prim.params[1];
    double dir_y = node->data.prim.params[2];
    double dir_z = node->data.prim.params[3];

    CSGVec3 dir = {dir_x, dir_y, dir_z};
    double dir_len = sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (dir_len < CSG_BSP_EPSILON) {
        dir.x = 0.0; dir.y = 0.0; dir.z = 1.0;
    } else {
        dir.x /= dir_len; dir.y /= dir_len; dir.z /= dir_len;
    }

    CSGVec3 offset = csg_vec3_scale(dir, height);

    CSGVec3 *section_verts = NULL;
    int section_vert_count = 0;
    csg_extract_vertices(&section_tris, &section_verts, &section_vert_count);

    for (int i = 0; i < section_tris.count; i++) {
        CSGTriangle tri = section_tris.tris[i];
        CSGVec3 flip_n = csg_vec3_scale(tri.normal, -1.0);
        if (csg_vec3_dot(flip_n, dir) < 0.0)
            flip_n = csg_vec3_scale(flip_n, -1.0);
        tri.normal = csg_vec3_normalize(flip_n);
        tri.face_id = out->count;
        csg_trilist_append(out, &tri);
    }

    for (int i = 0; i < section_tris.count; i++) {
        CSGTriangle tri = section_tris.tris[i];
        tri.v[0] = csg_vec3_add(tri.v[0], offset);
        tri.v[1] = csg_vec3_add(tri.v[1], offset);
        tri.v[2] = csg_vec3_add(tri.v[2], offset);
        if (csg_vec3_dot(tri.normal, dir) < 0.0)
            tri.normal = csg_vec3_scale(tri.normal, -1.0);
        tri.normal = csg_vec3_normalize(tri.normal);
        tri.face_id = out->count;
        csg_trilist_append(out, &tri);
    }

    if (section_verts && section_vert_count >= 2) {
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
                                int tmp = idx_a;
                                idx_a = idx_b;
                                idx_b = tmp;
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
                        tri.v[0] = v0;
                        tri.v[1] = v1;
                        tri.v[2] = v2;
                        tri.normal = csg_tri_normal(&tri);
                        /* 确保法线朝外（远离拉伸轴） */
                        CSGVec3 edge_mid = csg_vec3_scale(csg_vec3_add(v0, v1), 0.5);
                        CSGVec3 edge_out = csg_vec3_sub(edge_mid, csg_vec3_scale(dir, csg_vec3_dot(edge_mid, dir)));
                        if (csg_vec3_dot(tri.normal, edge_out) < 0.0) {
                            /* 翻转三角形绕序 */
                            CSGVec3 tmp = tri.v[1];
                            tri.v[1] = tri.v[2];
                            tri.v[2] = tmp;
                            tri.normal = csg_vec3_scale(tri.normal, -1.0);
                        }
                        tri.face_id = out->count;
                        csg_trilist_append(out, &tri);

                        tri.v[0] = v0;
                        tri.v[1] = v2;
                        tri.v[2] = v3;
                        tri.normal = csg_tri_normal(&tri);
                        if (csg_vec3_dot(tri.normal, edge_out) < 0.0) {
                            CSGVec3 tmp = tri.v[1];
                            tri.v[1] = tri.v[2];
                            tri.v[2] = tmp;
                            tri.normal = csg_vec3_scale(tri.normal, -1.0);
                        }
                        tri.face_id = out->count;
                        csg_trilist_append(out, &tri);

                        csg_trilist_free(&side_tris);
                    }
                }

                if (edge_a)
                    lv_free((void **) &edge_a);
                if (edge_b)
                    lv_free((void **) &edge_b);
        }
}

static void eval_csg_extrude_rotate(const CSGNode *node, CSGTriList *out) {
    if (node->child_count < 1)
        return;

    CSGTriList section_tris;
    csg_trilist_init(&section_tris, CSG_MAX_TRI_BUFFER);
    csg_evaluate(node->children[0], &section_tris);

    double angle_deg = node->data.prim.params[0];
    if (angle_deg <= 0.0) angle_deg = 360.0;
    int segments = (int)node->data.prim.params[1];
    if (segments <= 0) segments = 32;
    if (segments > 128) segments = 128;

    CSGVec3 axis = {node->data.prim.params[2], node->data.prim.params[3], node->data.prim.params[4]};
    double axis_len = sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
    if (axis_len < CSG_BSP_EPSILON) {
        axis.x = 0.0; axis.y = 1.0; axis.z = 0.0;
    } else {
        axis.x /= axis_len; axis.y /= axis_len; axis.z /= axis_len;
    }

    double angle_rad = angle_deg * M_PI / 180.0;
    double angle_step = angle_rad / (double)segments;

    CSGVec3 *sec_verts = NULL;
    int sec_count = 0;
    csg_extract_vertices(&section_tris, &sec_verts, &sec_count);

    if (!sec_verts || sec_count < 2) {
        if (sec_verts) lv_free((void **)&sec_verts);
        csg_trilist_free(&section_tris);
        return;
    }

    for (int seg = 0; seg < segments; seg++) {
        double theta1 = angle_step * (double)seg;
        double theta2 = angle_step * (double)(seg + 1);
        double cos1 = cos(theta1), sin1 = sin(theta1);
        double cos2 = cos(theta2), sin2 = sin(theta2);

        for (int i = 0; i < section_tris.count; i++) {
            for (int e = 0; e < 3; e++) {
                CSGVec3 va = section_tris.tris[i].v[e];
                CSGVec3 vb = section_tris.tris[i].v[(e + 1) % 3];

                CSGVec3 ediff = csg_vec3_sub(vb, va);
                if (ediff.x * ediff.x + ediff.y * ediff.y + ediff.z * ediff.z < CSG_BSP_EPSILON * CSG_BSP_EPSILON)
                    continue;

                CSGVec3 va1, va2, vb1, vb2;
                CSGVec3 kxva = csg_vec3_cross(axis, va);
                double kdva = csg_vec3_dot(axis, va);
                CSGVec3 kxvb = csg_vec3_cross(axis, vb);
                double kdvb = csg_vec3_dot(axis, vb);

                double c1_complement = 1.0 - cos1;
                va1.x = va.x * cos1 + kxva.x * sin1 + axis.x * kdva * c1_complement;
                va1.y = va.y * cos1 + kxva.y * sin1 + axis.y * kdva * c1_complement;
                va1.z = va.z * cos1 + kxva.z * sin1 + axis.z * kdva * c1_complement;

                double c2_complement = 1.0 - cos2;
                va2.x = va.x * cos2 + kxva.x * sin2 + axis.x * kdva * c2_complement;
                va2.y = va.y * cos2 + kxva.y * sin2 + axis.y * kdva * c2_complement;
                va2.z = va.z * cos2 + kxva.z * sin2 + axis.z * kdva * c2_complement;

                vb1.x = vb.x * cos1 + kxvb.x * sin1 + axis.x * kdvb * c1_complement;
                vb1.y = vb.y * cos1 + kxvb.y * sin1 + axis.y * kdvb * c1_complement;
                vb1.z = vb.z * cos1 + kxvb.z * sin1 + axis.z * kdvb * c1_complement;

                vb2.x = vb.x * cos2 + kxvb.x * sin2 + axis.x * kdvb * c2_complement;
                vb2.y = vb.y * cos2 + kxvb.y * sin2 + axis.y * kdvb * c2_complement;
                vb2.z = vb.z * cos2 + kxvb.z * sin2 + axis.z * kdvb * c2_complement;

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

    if (angle_deg < 360.0 - CSG_BSP_EPSILON) {
        for (int i = 0; i < section_tris.count; i++) {
            CSGTriangle tri = section_tris.tris[i];
            CSGVec3 face_normal = csg_tri_normal(&tri);
            if (csg_vec3_dot(face_normal, axis) < 0.0) {
                face_normal = csg_vec3_scale(face_normal, -1.0);
                CSGVec3 tmp = tri.v[1]; tri.v[1] = tri.v[2]; tri.v[2] = tmp;
            }
            tri.normal = csg_vec3_normalize(face_normal);
            tri.face_id = out->count;
            csg_trilist_append(out, &tri);
        }

        double cos_end = cos(angle_rad), sin_end = sin(angle_rad);
        for (int i = 0; i < section_tris.count; i++) {
            CSGTriangle tri = section_tris.tris[i];
            for (int v = 0; v < 3; v++) {
                CSGVec3 p = tri.v[v];
                CSGVec3 kxp = csg_vec3_cross(axis, p);
                double kdp = csg_vec3_dot(axis, p);
                tri.v[v].x = p.x * cos_end + kxp.x * sin_end + axis.x * kdp * (1.0 - cos_end);
                tri.v[v].y = p.y * cos_end + kxp.y * sin_end + axis.y * kdp * (1.0 - cos_end);
                tri.v[v].z = p.z * cos_end + kxp.z * sin_end + axis.z * kdp * (1.0 - cos_end);
            }
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

    if (sec_verts) lv_free((void **)&sec_verts);
    csg_trilist_free(&section_tris);
}

static CSGEvalFunc s_eval_funcs[] = {
    [CSG_NODE_PRIMITIVE] = eval_csg_primitive,
    [CSG_NODE_UNION] = eval_csg_bool,
    [CSG_NODE_DIFFERENCE] = eval_csg_bool,
    [CSG_NODE_INTERSECTION] = eval_csg_bool,
    [CSG_NODE_TRANSFORM] = eval_csg_transform,
    [CSG_NODE_HULL] = eval_csg_hull,
    [CSG_NODE_MINKOWSKI] = eval_csg_minkowski,
    [CSG_NODE_EXTRUDE_LINEAR] = eval_csg_extrude_linear,
    [CSG_NODE_EXTRUDE_ROTATE] = eval_csg_extrude_rotate,
};
static const int s_eval_func_count = (int)(sizeof(s_eval_funcs) / sizeof(s_eval_funcs[0]));

void csg_evaluate(const CSGNode *node, CSGTriList *out) {
    if (!node || !out)
        return;
    if (node->kind >= 0 && node->kind < s_eval_func_count && s_eval_funcs[node->kind]) {
        s_eval_funcs[node->kind](node, out);
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
                    n = snprintf(buf + written, (size_t) (buf_size - written), "%ssphere(r=%.10g);\n", indent_str,
                                 p[0]);
                    break;
                case 1: /* 立方体 */
                    n = snprintf(buf + written, (size_t) (buf_size - written),
                                 "%scube([%.10g, %.10g, %.10g], center=true);\n", indent_str, p[0], p[1], p[2]);
                    break;
                case 2: /* 圆柱体 */
                    n = snprintf(buf + written, (size_t) (buf_size - written),
                                 "%scylinder(r=%.10g, h=%.10g, center=true);\n", indent_str, p[0], p[1]);
                    break;
                default:
                    n = snprintf(buf + written, (size_t) (buf_size - written), "%s// unknown primitive type %d\n",
                                 indent_str, ptype);
                    break;
            }
            if (n > 0)
                written += n;
            if (n < 0)
                lv_RETURN_ERROR(lv_ERROR_INTERNAL, "csg_export_node: snprintf failed for primitive");
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
                lv_RETURN_ERROR(lv_ERROR_INTERNAL, "csg_export_node: snprintf failed for boolean op");

            for (int i = 0; i < node->child_count; i++) {
                written = csg_export_node(node->children[i], buf, buf_size, written, indent + 1);
                if (written < 0)
                    lv_RETURN_ERROR(lv_ERROR_INTERNAL, "csg_export_node: child export failed");
            }

            n = snprintf(buf + written, (size_t) (buf_size - written), "%s}\n", indent_str);
            if (n > 0)
                written += n;
            else if (n < 0)
                lv_RETURN_ERROR(lv_ERROR_INTERNAL, "csg_export_node: snprintf failed for boolean close");
            break;
        }

        case CSG_NODE_TRANSFORM:
            n = snprintf(buf + written, (size_t) (buf_size - written), "%s// transform (TBI)\n", indent_str);
            if (n > 0)
                written += n;
            else if (n < 0)
                lv_RETURN_ERROR(lv_ERROR_INTERNAL, "csg_export_node: snprintf failed for transform");
            if (node->child_count > 0) {
                written = csg_export_node(node->children[0], buf, buf_size, written, indent);
                if (written < 0)
                    lv_RETURN_ERROR(lv_ERROR_INTERNAL, "csg_export_node: transform child export failed");
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
                lv_RETURN_ERROR(lv_ERROR_INTERNAL, "csg_export_node: snprintf failed for hull/minkowski/extrude");

            for (int i = 0; i < node->child_count; i++) {
                written = csg_export_node(node->children[i], buf, buf_size, written, indent + 1);
                if (written < 0)
                    lv_RETURN_ERROR(lv_ERROR_INTERNAL, "csg_export_node: child export failed");
            }

            n = snprintf(buf + written, (size_t) (buf_size - written), "%s}\n", indent_str);
            if (n > 0)
                written += n;
            else if (n < 0)
                lv_RETURN_ERROR(lv_ERROR_INTERNAL, "csg_export_node: snprintf failed for hull/minkowski/extrude close");
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
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "csg_export_to_openscad: root is NULL");

    int buf_size = CSG_EXPORT_BUF_INIT;
    char *buf = (char *) lv_calloc((size_t) buf_size, sizeof(char));
    if (!buf)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "csg_export_to_openscad: buffer allocation failed");

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
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "csg_example_taj_mahal_dome: dome allocation failed");

    /* 创建圆柱体基座 */
    CSGNode *base = csg_cylinder_create(10.0, 4.0);
    if (!base) {
        csg_node_destroy(dome);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "csg_example_taj_mahal_dome: base allocation failed");
    }

    /* 组合为并集 */
    CSGNode *taj_mahal = geometry_csg_union(dome, base);
    if (!taj_mahal) {
        csg_node_destroy(dome);
        csg_node_destroy(base);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "csg_example_taj_mahal_dome: union allocation failed");
    }

    return taj_mahal;
}
