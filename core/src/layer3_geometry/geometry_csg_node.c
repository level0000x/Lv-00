/*
 * @file geometry_csg_node.c
 * @brief CSG geometry module - node lifecycle
 * @details Split from geometry_csg.c
 */

#include "lv/lv_platform.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "geometry_types.h"
#include "geometry_csg_internal.h"
#include "lv_internal.h"
#include "lv_utils.h"

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
