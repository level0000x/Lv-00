/*
 * @file geometry_csg_bsp.c
 * @brief CSG geometry module - BSP tree boolean operations
 * @details Split from geometry_csg.c
 */

#include "lv/lv_platform.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/geometry_types.h"
#include "geometry_csg_internal.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"

CSGBSPNode *csg_bsp_node_create(void) {
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
 * @note 扩容已收敛到 lv_ensure_capacity（lv_utils.h）；首次分配容量
 *       lv_INITIAL_ARRAY_CAPACITY(8)，原实现为 4，仅初始容量不同，功能等价
 */
void csg_bsp_node_add_tri(CSGBSPNode *node, const CSGTriangle *tri) {
    if (!node || !tri)
        return;
    if (node->tri_count >= node->tri_capacity) {
        if (!lv_ensure_capacity((void **) &node->tris, node->tri_count, &node->tri_capacity,
                                sizeof(CSGTriangle), 1))
            return;
    }
    node->tris[node->tri_count] = *tri;
    node->tri_count++;
}

/**
 * @brief 递归销毁 BSP 树
 */
void csg_bsp_node_destroy(CSGBSPNode *node) {
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
CSGBSPClass csg_bsp_classify_triangle(const CSGBSPNode *node, const CSGTriangle *tri, double eps) {
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
void csg_bsp_split_triangle(const CSGTriangle *tri, CSGVec3 plane_point, CSGVec3 plane_normal, double eps,
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

/* --- Lookup table dispatch for BSP build --- */
typedef void (*csg_bsp_build_dispatch_t)(const CSGTriangle *cur, CSGBSPNode *node, CSGTriList *front_list, CSGTriList *back_list, double eps);

static void csg_bsp_build_dispatch_front(const CSGTriangle *cur, CSGBSPNode *node, CSGTriList *front_list, CSGTriList *back_list, double eps) {
    (void)node; (void)back_list; (void)eps;
    csg_trilist_append(front_list, cur);
}

static void csg_bsp_build_dispatch_back(const CSGTriangle *cur, CSGBSPNode *node, CSGTriList *front_list, CSGTriList *back_list, double eps) {
    (void)node; (void)front_list; (void)eps;
    csg_trilist_append(back_list, cur);
}

static void csg_bsp_build_dispatch_on(const CSGTriangle *cur, CSGBSPNode *node, CSGTriList *front_list, CSGTriList *back_list, double eps) {
    (void)front_list; (void)back_list; (void)eps;
    csg_bsp_node_add_tri(node, cur);
}

static void csg_bsp_build_dispatch_split(const CSGTriangle *cur, CSGBSPNode *node, CSGTriList *front_list, CSGTriList *back_list, double eps) {
    csg_bsp_split_triangle(cur, node->plane_point, node->plane_normal, eps, front_list, back_list);
}

static const csg_bsp_build_dispatch_t csg_bsp_build_dispatch_table[4] = {
    csg_bsp_build_dispatch_front,  /* CSG_BSP_FRONT = 0 */
    csg_bsp_build_dispatch_back,   /* CSG_BSP_BACK  = 1 */
    csg_bsp_build_dispatch_on,     /* CSG_BSP_ON    = 2 */
    csg_bsp_build_dispatch_split   /* CSG_BSP_SPLIT = 3 */
};

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
CSGBSPNode *csg_bsp_build(CSGTriList *tris, double eps) {
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

        csg_bsp_build_dispatch_table[cls](cur, node, &front_list, &back_list, eps);
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

/* --- Lookup table dispatch for BSP clip --- */
typedef void (*csg_bsp_clip_dispatch_t)(const CSGTriangle *tri, const CSGBSPNode *node, CSGTriList *out, double eps, int keep_inside);

static void csg_bsp_clip_dispatch_front(const CSGTriangle *tri, const CSGBSPNode *node, CSGTriList *out, double eps, int keep_inside) {
    (void)out;
    if (keep_inside) {
        return;
    }
    csg_bsp_clip_triangle(tri, node->front, out, eps, keep_inside);
}

static void csg_bsp_clip_dispatch_back(const CSGTriangle *tri, const CSGBSPNode *node, CSGTriList *out, double eps, int keep_inside) {
    (void)out;
    if (keep_inside) {
        csg_bsp_clip_triangle(tri, node->back, out, eps, keep_inside);
        return;
    }
}

static void csg_bsp_clip_dispatch_on(const CSGTriangle *tri, const CSGBSPNode *node, CSGTriList *out, double eps, int keep_inside) {
    (void)node; (void)eps; (void)keep_inside;
    csg_trilist_append(out, tri);
}

static void csg_bsp_clip_dispatch_split(const CSGTriangle *tri, const CSGBSPNode *node, CSGTriList *out, double eps, int keep_inside) {
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
}

static const csg_bsp_clip_dispatch_t csg_bsp_clip_dispatch_table[4] = {
    csg_bsp_clip_dispatch_front,  /* CSG_BSP_FRONT = 0 */
    csg_bsp_clip_dispatch_back,   /* CSG_BSP_BACK  = 1 */
    csg_bsp_clip_dispatch_on,     /* CSG_BSP_ON    = 2 */
    csg_bsp_clip_dispatch_split   /* CSG_BSP_SPLIT = 3 */
};

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
void csg_bsp_clip_triangle(const CSGTriangle *tri, const CSGBSPNode *node, CSGTriList *out, double eps, int keep_inside) {
    if (!node) {
        /* 到达叶子，保留（对于 keep_outside 是在外部；对于 keep_inside 是在内部） */
        csg_trilist_append(out, tri);
        return;
    }

    CSGBSPClass cls = csg_bsp_classify_triangle(node, tri, eps);

    csg_bsp_clip_dispatch_table[cls](tri, node, out, eps, keep_inside);
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
void csg_bsp_union_tri(const CSGTriList *list_a, const CSGTriList *list_b, CSGTriList *out) {
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
void csg_bsp_difference_tri(const CSGTriList *list_a, const CSGTriList *list_b, CSGTriList *out) {
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
void csg_bsp_intersection_tri(const CSGTriList *list_a, const CSGTriList *list_b, CSGTriList *out) {
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