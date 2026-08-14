/**
 * @file graph_traversal_tree.c
 * @brief 树遍历 API（由 lv_graph_traversal.c 拆分子模块）
 *
 * @details lv_tree_traverse：通用树结构 DFS/BFS 迭代遍历。
 * @author Lv-00 Project
 * @version 1.0.0
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/lv_graph_traversal.h"
#include "lv/lv_lifecycle.h"
#include "lv/lv_utils.h"

#include "lv/lv_internal.h"
#include "graph_traversal_internal.h"

/* ============================================================
 * 树遍历 API 实现
 * ============================================================ */

int lv_tree_traverse(void *root,
                      lvTreeNodeVisitor visitor,
                      void *user_data,
                      lvGetChildrenFunc get_children,
                      const lvTreeTraversalConfig *config) {
    if (!root || !visitor || !get_children)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "lv_tree_traverse: NULL param");

    lvTreeTraversalConfig default_config = lv_TREE_TRAVERSAL_DEFAULT_CONFIG;
    if (!config)
        config = &default_config;

    /* 使用栈/队列进行迭代遍历 */
    if (config->order == lv_TRAVERSAL_BFS) {
        /* BFS 队列 */
        int cap = 64;
        void **queue = (void **)lv_malloc((size_t)cap * sizeof(void *));
        int *depths = (int *)lv_malloc((size_t)cap * sizeof(int));
        if (!queue || !depths) {
            lv_free((void **)&queue);
            lv_free((void **)&depths);
            lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_tree_traverse: queue alloc failed");
        }
        lv_DEFER_FREE(queue);
        lv_DEFER_FREE(depths);

        int head = 0, tail = 0;
        queue[tail] = root;
        depths[tail] = 0;
        tail++;

        while (head < tail) {
            void *node = queue[head];
            int depth = depths[head];
            head++;

            /* 深度检查 */
            if (config->max_depth > 0 && depth >= config->max_depth)
                continue;

            lvTraversalResult tr = visitor(node, depth, user_data);
            if (tr == lv_TRAVERSAL_STOP)
                break;
            if (tr == lv_TRAVERSAL_SKIP_CHILDREN)
                continue;

            void **children = NULL;
            int child_count = get_children(node, &children);

            for (int i = 0; i < child_count; i++) {
                if (tail >= cap) {
                    if (!lv_ensure_capacity((void **)&queue, tail, &cap, sizeof(void *), 0) ||
                        !lv_ensure_capacity((void **)&depths, tail, &cap, sizeof(int), 0)) {
                        lv_free((void **)&children);
                        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_tree_traverse: queue realloc failed");
                    }
                }
                queue[tail] = children[i];
                depths[tail] = depth + 1;
                tail++;
            }

            lv_free((void **)&children);
        }
        /* BFS 分支结束：lv_DEFER 守卫自动释放 queue/depths */
    } else {
        /* DFS 栈（前序/后序） */
        int cap = 64;
        typedef struct {
            void *node;
            int depth;
            bool is_exit;
        } TreeFrame;

        TreeFrame *stack = (TreeFrame *)lv_malloc((size_t)cap * sizeof(TreeFrame));
        if (!stack)
            lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_tree_traverse: stack alloc failed");
        lv_DEFER_FREE(stack);

        int top = 0;
        stack[top].node = root;
        stack[top].depth = 0;
        stack[top].is_exit = false;
        top++;

        while (top > 0) {
            top--;
            TreeFrame frame = stack[top];

            if (frame.is_exit) {
                if (config->order == lv_TRAVERSAL_DFS_POST) {
                    lvTraversalResult tr = visitor(frame.node, frame.depth, user_data);
                    if (tr == lv_TRAVERSAL_STOP)
                        break;
                }
                continue;
            }

            /* 深度检查 */
            if (config->max_depth > 0 && frame.depth >= config->max_depth)
                continue;

            /* 前序 */
            if (config->order != lv_TRAVERSAL_DFS_POST) {
                lvTraversalResult tr = visitor(frame.node, frame.depth, user_data);
                if (tr == lv_TRAVERSAL_STOP)
                    break;
                if (tr == lv_TRAVERSAL_SKIP_CHILDREN)
                    continue;
            }

            void **children = NULL;
            int child_count = get_children(frame.node, &children);

            if (config->order == lv_TRAVERSAL_DFS_POST && child_count > 0) {
                /* 后序：先压入退出帧，再压入子节点 */
                if (top >= cap) {
                    if (!lv_ensure_capacity((void **)&stack, top, &cap, sizeof(TreeFrame), 0)) {
                        lv_free((void **)&children);
                        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_tree_traverse: stack realloc failed");
                    }
                }
                stack[top].node = frame.node;
                stack[top].depth = frame.depth;
                stack[top].is_exit = true;
                top++;
            }

            /* 逆序压入子节点（保证从左到右遍历） */
            for (int i = child_count - 1; i >= 0; i--) {
                if (top >= cap) {
                    if (!lv_ensure_capacity((void **)&stack, top, &cap, sizeof(TreeFrame), 0)) {
                        lv_free((void **)&children);
                        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_tree_traverse: stack realloc failed");
                    }
                }
                stack[top].node = children[i];
                stack[top].depth = frame.depth + 1;
                stack[top].is_exit = false;
                top++;
            }

            lv_free((void **)&children);
        }
        /* DFS 分支结束：lv_DEFER 守卫自动释放 stack */
    }

    return lv_OK;
}

