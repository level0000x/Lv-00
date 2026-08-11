/**
 * @file proof_tree.c
 * @brief lvProofTree / lvProofTreeNode 实现
 *
 * @details 实现证明树的创建、销毁、节点添加、前提管理、
 *          矛盾标记传播和文本导出功能。证明树用于跟踪
 *          推理引擎的证明步骤及其层次结构。
 * @author Lv-00 Project
 * @version 1.0.0
 */

#include "lv/lv_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_utils.h"
#include "lv/lv_graph_traversal.h" /* lv_tree_release_recursive */
#include "lv/proof_trace.h"
#include "lv/lv_strbuf.h"

#define INITIAL_CHILD_CAPACITY 4
#define INITIAL_NODE_CAPACITY 16

/**
 * @brief 创建新的证明树节点
 *
 * 分配并初始化一个新的 lvProofTreeNode，包括复制描述和结论字符串。
 *
 * @param id     节点 ID
 * @param depth  节点深度（根节点为 0）
 * @param desc   使用的公理描述（可选，会被复制）
 * @param detail 结论详细信息（可选，会被复制）
 * @return 新创建的节点指针，失败返回 NULL
 */
static lvProofTreeNode *create_node(int id, int depth, const char *desc, const char *detail) {
    lvProofTreeNode *n = (lvProofTreeNode *) lv_calloc(1, sizeof(lvProofTreeNode));
    if (!n)
        return NULL;
    n->id = id;
    n->depth = depth;
    n->step_type = 0;
    n->is_contradiction = false;
    n->is_contradiction_branch = false;
    n->parent = NULL;
    lv_darray_init(&n->children, sizeof(lvProofTreeNode *));
    n->step_index = 0;
    lv_darray_init(&n->premises, sizeof(lvProofPremise));
    n->axiom_used = desc ? lv_strdup(desc) : NULL;
    n->conclusion = detail ? lv_strdup(detail) : NULL;
    return n;
}

/**
 * @brief 递归释放证明树节点及其所有子节点
 *
 * 深度优先遍历释放子节点、子节点数组、前提数组以及动态分配的字符串。
 *
 * @param n 要释放的节点指针（为 NULL 时直接返回）
 */
/* 递归子节点销毁：后序骨架收敛至 lv_tree_release_recursive（lv_graph_traversal.h），
 * lvDArray 容器经 proof_node_children 适配为裸指针数组 */
static void **proof_node_children(void *node, int *count) {
    lvProofTreeNode *n = (lvProofTreeNode *)node;
    *count = n->children.count;
    return (void **)n->children.data;
}

static void proof_node_cleanup(void *node) {
    lvProofTreeNode *n = (lvProofTreeNode *)node;
    lv_darray_free(&n->children);
    lv_darray_free(&n->premises);
    lv_free((void **) &n->axiom_used);
    lv_free((void **) &n->conclusion);
}

static void free_node_recursive(lvProofTreeNode *n) {
    lv_tree_release_recursive(n, proof_node_children, proof_node_cleanup);
}

/* lvDArray 已提供扩容，不再需要单独的 ensure_*_capacity 函数 */

/**
 * @brief 创建新的证明树
 *
 * 分配并初始化一个 lvProofTree，包含根节点和初始容量。
 * 根节点的深度为 0，step_index 为 -1。
 *
 * @param name     证明名称（可选，会同时复制到 tree->name 和 tree->theorem_name）
 * @param strategy 证明策略名称（可选，会同时复制到 tree->strategy 和 tree->proof_strategy）
 * @return 新创建的证明树指针，失败返回 NULL
 */
lvProofTree *lv_proof_tree_create(const char *name, const char *strategy) {
    lvProofTree *tree = (lvProofTree *) lv_calloc(1, sizeof(lvProofTree));
    if (!tree)
        return NULL;

    if (name)
        lv_strlcpy(tree->name, name, sizeof(tree->name));
    if (strategy)
        lv_strlcpy(tree->strategy, strategy, sizeof(tree->strategy));
    tree->theorem_name = name ? lv_strdup(name) : NULL;
    tree->proof_strategy = strategy ? lv_strdup(strategy) : NULL;

    lv_darray_init(&tree->all_nodes, sizeof(lvProofTreeNode *));
    if (!lv_darray_reserve(&tree->all_nodes, INITIAL_NODE_CAPACITY)) {
        lv_free((void **) &(tree));
        return NULL;
    }

    lvProofTreeNode *root = create_node(0, 0, NULL, NULL);
    if (!root) {
        lv_darray_free(&tree->all_nodes);
        lv_free((void **) &(tree));
        return NULL;
    }

    tree->root = root;
    root->step_index = -1; /* Root node has no step index */
    root->conclusion = name ? lv_strdup(name) : NULL;
    lv_darray_push(&tree->all_nodes, &root);
    tree->next_id = 1;
    tree->total_steps = 0;
    tree->max_depth = 0;
    tree->is_complete = false;
    return tree;
}

/**
 * @brief 销毁证明树，释放所有资源
 *
 * 递归释放所有节点、字符串副本和数组，然后释放证明树本身。
 *
 * @param tree 要销毁的证明树指针（为 NULL 时直接返回）
 */
void lv_proof_tree_destroy(lvProofTree *tree) {
    if (!tree)
        return;
    free_node_recursive(tree->root);
    lv_darray_free(&tree->all_nodes);
    lv_free((void **) &tree->theorem_name);
    lv_free((void **) &tree->proof_strategy);
    lv_free((void **) &(tree));
}

/**
 * @brief 向证明树添加新的证明步骤节点
 *
 * 在指定父节点下创建新的子节点，并注册到证明树的全节点数组中。
 * 如果 parent 为 NULL，则以根节点作为父节点。
 *
 * @param tree   证明树指针
 * @param parent 父节点指针（为 NULL 时使用根节点）
 * @param desc   步骤描述（公理使用信息）
 * @param detail 步骤详细结论
 * @param id     步骤 ID（当前未使用，保留给未来扩展）
 * @return 新创建的节点指针，失败返回 NULL
 */
lvProofTreeNode *lv_proof_tree_add_step(lvProofTree *tree, lvProofTreeNode *parent, const char *desc,
                                        const char *detail, int id) {
    if (!tree)
        return NULL;
    (void) id;

    lvProofTreeNode *par = parent ? parent : tree->root;
    if (!par)
        return NULL;

    int new_depth = par->depth + 1;
    lvProofTreeNode *node = create_node(tree->next_id++, new_depth, desc, detail);
    if (!node)
        return NULL;

    node->parent = par;
    node->step_index = tree->total_steps;

    /* 使用 lv_darray_push 追加子节点（自动扩容） */
    if (lv_darray_push(&par->children, &node) < 0) {
        free_node_recursive(node);
        return NULL;
    }

    /* 注册到全节点数组 */
    lv_darray_push(&tree->all_nodes, &node);
    tree->total_steps++;
    if (new_depth > tree->max_depth)
        tree->max_depth = new_depth;
    return node;
}

/**
 * @brief 为证明树节点添加前提
 *
 * 将一个新的证明前提（前提 ID、名称描述、是否公理）添加到指定节点。
 * tree 参数实际接受 lvProofTreeNode *，使用 void * 是为了与上层 API 兼容。
 *
 * @param tree   指向 lvProofTreeNode 的指针（void * 兼容接口）
 * @param idx    前提 ID
 * @param name   前提描述名称（可选）
 * @param negated 是否为公理（true 表示 is_axiom）
 */
void lv_proof_tree_add_premise(void *tree, int idx, const char *name, bool negated) {
    if (!tree)
        return;
    lvProofTreeNode *node = (lvProofTreeNode *) tree;

    /* 使用 lv_darray_push 自动扩容 */
    lvProofPremise premise;
    memset(&premise, 0, sizeof(premise));
    premise.premise_id = idx;
    if (name) {
        lv_strlcpy(premise.description, name, sizeof(premise.description));
    }
    premise.is_axiom = negated;
    lv_darray_push(&node->premises, &premise);
}

/**
 * @brief 标记节点为矛盾，并向上传播矛盾分支标记
 *
 * 将指定节点标记为矛盾，同时递归向上遍历所有祖先节点，
 * 将其 is_contradiction_branch 标记为 true。
 *
 * @param node 要标记为矛盾的节点指针
 * @return true 标记成功，false 节点为 NULL
 */
bool lv_proof_tree_mark_contradiction(lvProofTreeNode *node) {
    if (!node)
        return false;
    node->is_contradiction = true;
    node->is_contradiction_branch = true;
    /* Propagate to ancestors */
    lvProofTreeNode *p = node->parent;
    while (p) {
        p->is_contradiction_branch = true;
        p = p->parent;
    }
    return true;
}

/**
 * @brief 递归将节点及其子节点导出为文本行
 *
 * 使用缩进表示树深度（每层 2 空格，最多 40 空格），输出节点 ID 和公理使用信息。
 * 动态扩展缓冲区以适应输出。
 *
 * @param n     当前节点指针
 * @param indent 当前缩进层级
 * @param buf   指向输出缓冲区的指针（可能通过 realloc 扩展）
 * @param len   当前写入长度
 * @param cap   当前缓冲区容量
 */
static void export_node(const lvProofTreeNode *n, int indent, lvStrBuf *sb) {
    if (!n)
        return;
    int spaces = indent * 2;
    if (spaces > 40)
        spaces = 40;
    lv_strbuf_printf(sb, "%*s[%d] %s%s\n", spaces, "", n->id,
                     n->axiom_used ? n->axiom_used : "(no axiom)", n->is_contradiction ? " [CONTRADICTION]" : "");

    for (int i = 0; i < n->children.count; i++) {
        lvProofTreeNode **child = (lvProofTreeNode **)lv_darray_get(&n->children, i);
        export_node(*child, indent + 1, sb);
    }
}

/**
 * @brief 将证明树导出为文本格式
 *
 * 生成包含证明树头部（定理名称和策略）和层次化节点列表的文本。
 * 调用者负责使用 free() 释放返回的字符串。
 *
 * @param tree 证明树指针
 * @param opts 可选参数（当前未使用）
 * @return 动态分配的文本字符串，失败返回 NULL
 */
char *lv_proof_tree_export_text(const lvProofTree *tree, const char *opts) {
    (void) opts;
    if (!tree || !tree->root)
        return NULL;

    /* 用 lvStrBuf 累积输出（自动扩容；lv_strbuf_to_string 返回 lv_malloc 分配的 NUL 结尾字符串） */
    lvStrBuf sb = {0};

    /* Header */
    lv_strbuf_printf(&sb, "Proof Tree: %s\nStrategy: %s\n---\n", tree->theorem_name ? tree->theorem_name : "(unnamed)",
                     tree->proof_strategy ? tree->proof_strategy : "(none)");

    export_node(tree->root, 0, &sb);
    return lv_strbuf_to_string(&sb);
}
