/**
 * @file proof_tree.c
 * @brief lvProofTree / lvProofTreeNode implementation
 */

#include "lv/proof_trace.h"
#include "lv/lv_utils.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define INITIAL_CHILD_CAPACITY 4
#define INITIAL_NODE_CAPACITY  16

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

static bool ensure_node_capacity(lvProofTree *tree) {
    if (tree->node_count < tree->node_capacity) return true;
    int new_cap = tree->node_capacity * 2;
    lvProofTreeNode **p = (lvProofTreeNode **)realloc(tree->all_nodes, (size_t)new_cap * sizeof(lvProofTreeNode *));
    if (!p) return false;
    tree->all_nodes = p;
    tree->node_capacity = new_cap;
    return true;
}

static bool ensure_child_capacity(lvProofTreeNode *parent) {
    if (parent->child_count < parent->child_capacity) return true;
    int new_cap = parent->child_capacity > 0 ? parent->child_capacity * 2 : INITIAL_CHILD_CAPACITY;
    lvProofTreeNode **p = (lvProofTreeNode **)realloc(parent->children, (size_t)new_cap * sizeof(lvProofTreeNode *));
    if (!p) return false;
    parent->children = p;
    parent->child_capacity = new_cap;
    return true;
}

lvProofTree *lv_proof_tree_create(const char *name, const char *strategy) {
    lvProofTree *tree = (lvProofTree *)calloc(1, sizeof(lvProofTree));
    if (!tree) return NULL;

    if (name) strncpy(tree->name, name, sizeof(tree->name) - 1);
    if (strategy) strncpy(tree->strategy, strategy, sizeof(tree->strategy) - 1);
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

void lv_proof_tree_destroy(lvProofTree *tree) {
    if (!tree) return;
    free_node_recursive(tree->root);
    free(tree->all_nodes);
    lv_free((void**)&tree->theorem_name);
    lv_free((void**)&tree->proof_strategy);
    free(tree);
}

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
