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

#include "lv/proof_trace.h"
#include "lv/lv_utils.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define INITIAL_CHILD_CAPACITY 4
#define INITIAL_NODE_CAPACITY  16

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
    lvProofTreeNode *n = (lvProofTreeNode *)calloc(1, sizeof(lvProofTreeNode));
    if (!n) return NULL;
    n->id = id;
    n->depth = depth;
    n->step_type = 0;
    n->is_contradiction = false;
    n->is_contradiction_branch = false;
    n->parent = NULL;
    n->children = NULL;
    n->child_count = 0;
    n->child_capacity = 0;
    n->step_index = 0;
    n->premises = NULL;
    n->premise_count = 0;
    n->premise_capacity = 0;
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
static void free_node_recursive(lvProofTreeNode *n) {
    if (!n) return;
    for (int i = 0; i < n->child_count; i++) {
        free_node_recursive(n->children[i]);
    }
    free(n->children);
    free(n->premises);
    lv_free((void**)&n->axiom_used);
    lv_free((void**)&n->conclusion);
    free(n);
}

/**
 * @brief 确保证明树的全节点数组有足够容量
 *
 * 当 node_count 达到 node_capacity 时，将容量翻倍。
 *
 * @param tree 证明树指针
 * @return true 容量充足或扩展成功，false 扩展失败
 */
static bool ensure_node_capacity(lvProofTree *tree) {
    if (tree->node_count < tree->node_capacity) return true;
    int new_cap = tree->node_capacity * 2;
    lvProofTreeNode **p = (lvProofTreeNode **)realloc(tree->all_nodes, (size_t)new_cap * sizeof(lvProofTreeNode *));
    if (!p) return false;
    tree->all_nodes = p;
    tree->node_capacity = new_cap;
    return true;
}

/**
 * @brief 确保父节点的子节点数组有足够容量
 *
 * 当 child_count 达到 child_capacity 时，将容量翻倍（最小为 INITIAL_CHILD_CAPACITY）。
 *
 * @param parent 父节点指针
 * @return true 容量充足或扩展成功，false 扩展失败
 */
static bool ensure_child_capacity(lvProofTreeNode *parent) {
    if (parent->child_count < parent->child_capacity) return true;
    int new_cap = parent->child_capacity > 0 ? parent->child_capacity * 2 : INITIAL_CHILD_CAPACITY;
    lvProofTreeNode **p = (lvProofTreeNode **)realloc(parent->children, (size_t)new_cap * sizeof(lvProofTreeNode *));
    if (!p) return false;
    parent->children = p;
    parent->child_capacity = new_cap;
    return true;
}

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
    lvProofTree *tree = (lvProofTree *)calloc(1, sizeof(lvProofTree));
    if (!tree) return NULL;

    if (name) strncpy(tree->name, name, sizeof(tree->name) - 1);
    if (strategy) strncpy(tree->strategy, strategy, sizeof(tree->strategy) - 1);
    tree->name[sizeof(tree->name) - 1] = '\0';
    tree->strategy[sizeof(tree->strategy) - 1] = '\0';
    tree->theorem_name = name ? lv_strdup(name) : NULL;
    tree->proof_strategy = strategy ? lv_strdup(strategy) : NULL;

    tree->node_capacity = INITIAL_NODE_CAPACITY;
    tree->all_nodes = (lvProofTreeNode **)calloc((size_t)tree->node_capacity, sizeof(lvProofTreeNode *));
    if (!tree->all_nodes) { free(tree); return NULL; }

    lvProofTreeNode *root = create_node(0, 0, NULL, NULL);
    if (!root) { free(tree->all_nodes); free(tree); return NULL; }

    tree->root = root;
    root->step_index = -1;  /* Root node has no step index */
    root->conclusion = name ? lv_strdup(name) : NULL;
    tree->all_nodes[0] = root;
    tree->node_count = 1;
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
    if (!tree) return;
    free_node_recursive(tree->root);
    free(tree->all_nodes);
    lv_free((void**)&tree->theorem_name);
    lv_free((void**)&tree->proof_strategy);
    free(tree);
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
lvProofTreeNode *lv_proof_tree_add_step(lvProofTree *tree, lvProofTreeNode *parent,
                                             const char *desc, const char *detail, int id) {
    if (!tree) return NULL;
    (void)id;

    lvProofTreeNode *par = parent ? parent : tree->root;
    if (!par) return NULL;

    int new_depth = par->depth + 1;
    lvProofTreeNode *node = create_node(tree->next_id++, new_depth, desc, detail);
    if (!node) return NULL;

    node->parent = par;
    node->step_index = tree->total_steps;

    if (!ensure_child_capacity(par)) { free_node_recursive(node); return NULL; }
    par->children[par->child_count++] = node;

    if (!ensure_node_capacity(tree)) return node;
    tree->all_nodes[tree->node_count++] = node;
    tree->total_steps++;
    if (new_depth > tree->max_depth) tree->max_depth = new_depth;
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
    if (!tree) return;
    lvProofTreeNode *node = (lvProofTreeNode *)tree;

    /* Ensure capacity */
    if (node->premise_count >= node->premise_capacity) {
        int new_cap = node->premise_capacity > 0 ? node->premise_capacity * 2 : 4;
        lvProofPremise *p = (lvProofPremise *)realloc(node->premises,
            (size_t)new_cap * sizeof(lvProofPremise));
        if (!p) return;
        node->premises = p;
        node->premise_capacity = new_cap;
    }

    lvProofPremise *premise = &node->premises[node->premise_count];
    premise->premise_id = idx;
    if (name) {
        strncpy(premise->description, name, sizeof(premise->description) - 1);
        premise->description[sizeof(premise->description) - 1] = '\0';
    } else {
        premise->description[0] = '\0';
    }
    premise->is_axiom = negated;
    node->premise_count++;
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
    if (!node) return false;
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
static void export_node(const lvProofTreeNode *n, int indent, char **buf, size_t *len, size_t *cap) {
    if (!n) return;
    char line[512];
    int spaces = indent * 2;
    if (spaces > 40) spaces = 40;
    int written = snprintf(line, sizeof(line), "%*s[%d] %s%s\n",
                           spaces, "", n->id,
                           n->axiom_used ? n->axiom_used : "(no axiom)",
                           n->is_contradiction ? " [CONTRADICTION]" : "");
    if (written < 0) return;
    size_t need = *len + (size_t)written + 1;
    if (need > *cap) {
        *cap = *cap * 2 > need ? *cap * 2 : need;
        char *tmp = (char *)realloc(*buf, *cap);
        if (!tmp) return;
        *buf = tmp;
    }
    memcpy(*buf + *len, line, (size_t)written);
    *len += (size_t)written;
    (*buf)[*len] = '\0';

    for (int i = 0; i < n->child_count; i++) {
        export_node(n->children[i], indent + 1, buf, len, cap);
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
    (void)opts;
    if (!tree || !tree->root) return NULL;

    size_t cap = 1024;
    size_t len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    buf[0] = '\0';

    /* Header */
    int hdr = snprintf(buf, cap, "Proof Tree: %s\nStrategy: %s\n---\n",
                       tree->theorem_name ? tree->theorem_name : "(unnamed)",
                       tree->proof_strategy ? tree->proof_strategy : "(none)");
    if (hdr > 0) { len = (size_t)hdr; }

    export_node(tree->root, 0, &buf, &len, &cap);
    return buf;
}
