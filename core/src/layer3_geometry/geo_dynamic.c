/**
 * @file geo_dynamic.c
 * @brief åŠ¨æ€å‡ ä½•ä¾èµ–å›¾å®žçŽ° â€”â€?å€Ÿé‰´ GeoGebra åŠ¨æ€å‡ ä½•ç³»ç»? *
 * å®žçŽ°ç­–ç•¥ï¼? *   - ä½¿ç”¨é‚»æŽ¥è¡¨åŽ‹ç¼©å­˜å‚¨çˆ¶å­å…³ç³? *   - DFS å®žçŽ°çº§è”æ›´æ–°å’Œå¾ªçŽ¯æ£€æµ? *   - æ‹“æ‰‘æŽ’åºç”¨äºŽæ‰¹é‡æ›´æ–°
 *
 * @version v3.6.0
 */

#include "lv00/geo_dynamic.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <float.h>
#include <math.h>

#ifndef LV00_PUBLIC_API
#define LV00_PUBLIC_API
#endif

/* ========================================================================
 * å†…éƒ¨å¸¸é‡
 * ======================================================================== */

#define INITIAL_NODE_CAPACITY 64
#define INITIAL_ADJ_CAPACITY 256

/* ========================================================================
 * å†…éƒ¨æ•°æ®ç»“æž„
 * ======================================================================== */

/**
 * @brief æ·±åº¦ä¼˜å…ˆæœç´¢çŠ¶æ€? */
typedef struct {
    Lv00DynGraph *graph;
    uint8_t *marks;
    int *stack;
    int stack_top;
    bool has_cycle;
} DFSContext;

/* ========================================================================
 * ç¬¬ä¸€éƒ¨åˆ†ï¼šID åˆ°ç´¢å¼•æ˜ å°? * ======================================================================== */

/**
 * @brief åˆå§‹åŒ?ID æ˜ å°„è¡? */
static void init_id_map(Lv00DynGraph *graph)
{
    graph->id_to_index = (int *)malloc(graph->node_capacity * sizeof(int));
    for (int i = 0; i < graph->node_capacity; i++) {
        graph->id_to_index[i] = LV00_DYN_INVALID;
    }
}

/**
 * @brief ç¡®ä¿ ID æ˜ å°„è¡¨è¶³å¤Ÿå¤§
 */
static bool ensure_id_map_capacity(Lv00DynGraph *graph, int new_capacity)
{
    if (new_capacity <= graph->node_capacity) return true;

    int *new_map = (int *)realloc(graph->id_to_index, new_capacity * sizeof(int));
    if (!new_map) return false;

    for (int i = graph->node_capacity; i < new_capacity; i++) {
        new_map[i] = LV00_DYN_INVALID;
    }

    graph->id_to_index = new_map;
    graph->node_capacity = new_capacity;
    return true;
}

/**
 * @brief æ³¨å†ŒèŠ‚ç‚¹ ID åˆ°ç´¢å¼•çš„æ˜ å°„
 */
static void register_node_id(Lv00DynGraph *graph, int node_id, int index)
{
    if (node_id >= graph->node_capacity) {
        ensure_id_map_capacity(graph, node_id + 1);
    }
    graph->id_to_index[node_id] = index;
}

/**
 * @brief èŽ·å–èŠ‚ç‚¹ ID å¯¹åº”çš„ç´¢å¼? */
static int get_node_index(const Lv00DynGraph *graph, int node_id)
{
    if (node_id < 0 || node_id >= graph->node_capacity) {
        return LV00_DYN_INVALID;
    }
    return graph->id_to_index[node_id];
}

/* ========================================================================
 * ç¬¬äºŒéƒ¨åˆ†ï¼šé‚»æŽ¥è¡¨æ“ä½œ
 * ======================================================================== */

/**
 * @brief åˆå§‹åŒ–é‚»æŽ¥è¡¨
 */
static void init_adjacency(Lv00DynGraph *graph)
{
    graph->parent_adj = (int *)malloc(INITIAL_ADJ_CAPACITY * sizeof(int));
    graph->parent_adj_offsets = (int *)malloc((graph->node_capacity + 1) * sizeof(int));
    graph->child_adj = (int *)malloc(INITIAL_ADJ_CAPACITY * sizeof(int));
    graph->child_adj_offsets = (int *)malloc((graph->node_capacity + 1) * sizeof(int));

    graph->adj_capacity = INITIAL_ADJ_CAPACITY;

    for (int i = 0; i <= graph->node_capacity; i++) {
        graph->parent_adj_offsets[i] = 0;
        graph->child_adj_offsets[i] = 0;
    }
}

/**
 * @brief ç¡®ä¿é‚»æŽ¥è¡¨å®¹é‡? */
static bool ensure_adj_capacity(Lv00DynGraph *graph, int needed)
{
    if (needed <= graph->adj_capacity) return true;

    int new_cap = graph->adj_capacity * 2;
    while (new_cap < needed) new_cap *= 2;

    int *new_parent = (int *)realloc(graph->parent_adj, new_cap * sizeof(int));
    int *new_child = (int *)realloc(graph->child_adj, new_cap * sizeof(int));

    if (!new_parent || !new_child) {
        if (new_parent) free(new_parent);
        if (new_child) free(new_child);
        return false;
    }

    graph->parent_adj = new_parent;
    graph->child_adj = new_child;
    graph->adj_capacity = new_cap;
    return true;
}

/**
 * @brief æ·»åŠ çˆ¶èŠ‚ç‚¹å…³ç³? */
static void add_parent_edge(Lv00DynGraph *graph, int node_idx, int parent_idx)
{
    /* è®¡ç®—å½“å‰çˆ¶èŠ‚ç‚¹æ•°é‡?*/
    int start = graph->parent_adj_offsets[node_idx];
    int end = graph->parent_adj_offsets[node_idx + 1];
    int count = end - start;

    /* æ£€æŸ¥æ˜¯å¦å·²å­˜åœ¨ */
    for (int i = start; i < end; i++) {
        if (graph->parent_adj[i] == parent_idx) return;
    }

    /* æ‰©å®¹ */
    int total_needed = graph->adj_capacity + 1;
    if (!ensure_adj_capacity(graph, total_needed)) return;

    /* ç§»åŠ¨åŽç»­èŠ‚ç‚¹ */
    for (int i = graph->node_count; i > node_idx; i--) {
        graph->parent_adj_offsets[i + 1] = graph->parent_adj_offsets[i] + 1;
    }
    graph->parent_adj_offsets[node_idx + 1]++;

    /* æ’å…¥æ–°çˆ¶èŠ‚ç‚¹ */
    graph->parent_adj[end] = parent_idx;
}

/**
 * @brief æ·»åŠ å­èŠ‚ç‚¹å…³ç³? */
static void add_child_edge(Lv00DynGraph *graph, int node_idx, int child_idx)
{
    /* è®¡ç®—å½“å‰å­èŠ‚ç‚¹æ•°é‡?*/
    int start = graph->child_adj_offsets[node_idx];
    int end = graph->child_adj_offsets[node_idx + 1];
    int count = end - start;

    /* æ£€æŸ¥æ˜¯å否å·²å­˜åœ¨ */
    for (int i = start; i < end; i++) {
        if (graph->child_adj[i] == child_idx) return;
    }

    /* 同时更新节点的 child_ids 数组 */
    Lv00DynNode *parent_node = &graph->nodes[node_idx];
    if (parent_node->child_count < 16) {
        parent_node->child_ids[parent_node->child_count++] = graph->nodes[child_idx].id;
    }

    /* æ‰©å®¹ */
    int total_needed = graph->adj_capacity + 1;
    if (!ensure_adj_capacity(graph, total_needed)) return;

    /* ç§»åŠ¨åŽç»­èŠ‚ç‚¹ */
    for (int i = graph->node_count; i > node_idx; i--) {
        graph->child_adj_offsets[i + 1] = graph->child_adj_offsets[i] + 1;
    }
    graph->child_adj_offsets[node_idx + 1]++;

    /* æ’å…¥æ–°å­èŠ‚ç‚¹ */
    graph->child_adj[end] = child_idx;
}

/* ========================================================================
 * ç¬¬ä¸‰éƒ¨åˆ†ï¼šé»˜è®¤é…ç½? * ======================================================================== */

Lv00DynGraphConfig lv00_dyn_graph_default_config(void)
{
    Lv00DynGraphConfig cfg;
    cfg.max_nodes = 10000;
    cfg.max_parents = 4;
    cfg.max_children = 16;
    cfg.detect_cycles = true;
    cfg.max_update_depth = 100;
    return cfg;
}

/* ========================================================================
 * ç¬¬å››éƒ¨åˆ†ï¼šåˆ›å»ºä¸Žé‡Šæ”¾
 * ======================================================================== */

Lv00DynGraph *lv00_dyn_graph_create(const Lv00DynGraphConfig *config)
{
    Lv00DynGraph *graph = (Lv00DynGraph *)calloc(1, sizeof(Lv00DynGraph));
    if (!graph) return NULL;

    if (config) {
        graph->config = *config;
    } else {
        graph->config = lv00_dyn_graph_default_config();
    }

    graph->node_capacity = INITIAL_NODE_CAPACITY;
    graph->node_count = 0;

    graph->nodes = (Lv00DynNode *)calloc(graph->node_capacity, sizeof(Lv00DynNode));

    init_id_map(graph);
    init_adjacency(graph);

    return graph;
}

void lv00_dyn_graph_free(Lv00DynGraph *graph)
{
    if (!graph) return;

    free(graph->nodes);
    free(graph->id_to_index);
    free(graph->parent_adj);
    free(graph->parent_adj_offsets);
    free(graph->child_adj);
    free(graph->child_adj_offsets);
    free(graph);
}

/* ========================================================================
 * ç¬¬äº”éƒ¨åˆ†ï¼šèŠ‚ç‚¹æ“ä½? * ======================================================================== */

int lv00_dyn_graph_add_node(
    Lv00DynGraph *graph,
    Lv00DynNodeType type,
    const int *parent_ids,
    int parent_count,
    const double *params,
    int param_count)
{
    if (!graph || graph->node_count >= graph->config.max_nodes) {
        return LV00_DYN_INVALID;
    }

    /* ç”Ÿæˆæ–°èŠ‚ç‚?ID */
    int new_id = graph->node_count;

    /* æ‰©å®¹èŠ‚ç‚¹æ•°ç»„ */
    if (graph->node_count >= graph->node_capacity) {
        int new_cap = graph->node_capacity * 2;
        Lv00DynNode *new_nodes = (Lv00DynNode *)realloc(
            graph->nodes, new_cap * sizeof(Lv00DynNode));
        if (!new_nodes) return LV00_DYN_INVALID;

        int *new_parent_offsets = (int *)realloc(
            graph->parent_adj_offsets, (new_cap + 1) * sizeof(int));
        int *new_child_offsets = (int *)realloc(
            graph->child_adj_offsets, (new_cap + 1) * sizeof(int));

        if (!new_parent_offsets || !new_child_offsets) {
            if (new_parent_offsets) free(new_parent_offsets);
            if (new_child_offsets) free(new_child_offsets);
            return LV00_DYN_INVALID;
        }

        graph->nodes = new_nodes;
        graph->parent_adj_offsets = new_parent_offsets;
        graph->child_adj_offsets = new_child_offsets;
        graph->node_capacity = new_cap;
    }

    /* åˆå§‹åŒ–èŠ‚ç‚?*/
    Lv00DynNode *node = &graph->nodes[graph->node_count];
    memset(node, 0, sizeof(Lv00DynNode));
    node->id = new_id;
    node->type = type;
    node->state = LV00_DYN_STATE_VALID;
    node->parent_count = 0;
    node->child_count = 0;
    node->param_count = param_count;

    /* å¤åˆ¶çˆ¶èŠ‚ç‚?*/
    if (parent_ids && parent_count > 0) {
        for (int i = 0; i < parent_count && i < 4; i++) {
            int pindex = get_node_index(graph, parent_ids[i]);
            if (pindex != LV00_DYN_INVALID) {
                node->parent_ids[i] = parent_ids[i];
                node->parent_count++;
                add_parent_edge(graph, graph->node_count, pindex);
                add_child_edge(graph, pindex, graph->node_count);
            }
        }
    }

    /* å¤åˆ¶å‚æ•° */
    if (params && param_count > 0) {
        for (int i = 0; i < param_count && i < 8; i++) {
            node->params[i] = params[i];
        }
    }

    /* æ³¨å†Œ ID */
    register_node_id(graph, new_id, graph->node_count);

    /* æ›´æ–°åç§»æ•°ç»„ */
    graph->parent_adj_offsets[graph->node_count + 1] =
        graph->parent_adj_offsets[graph->node_count];
    graph->child_adj_offsets[graph->node_count + 1] =
        graph->child_adj_offsets[graph->node_count];

    graph->node_count++;
    return new_id;
}

Lv00DynNode *lv00_dyn_graph_get_node(Lv00DynGraph *graph, int node_id)
{
    int index = get_node_index(graph, node_id);
    if (index == LV00_DYN_INVALID || index >= graph->node_count) {
        return NULL;
    }
    return &graph->nodes[index];
}

bool lv00_dyn_graph_remove_node(Lv00DynGraph *graph, int node_id)
{
    Lv00DynNode *node = lv00_dyn_graph_get_node(graph, node_id);
    if (!node) return false;

    int index = get_node_index(graph, node_id);

    /* æ–­å¼€æ‰€æœ‰çˆ¶å­å…³ç³?*/
    /* ç§»é™¤ä½œä¸ºå­èŠ‚ç‚¹çš„å…³ç³»ï¼ˆçˆ¶èŠ‚ç‚¹ä¸å†æŒ‡å‘æ­¤èŠ‚ç‚¹ï¼‰ */
    for (int i = 0; i < node->parent_count; i++) {
        int pindex = get_node_index(graph, node->parent_ids[i]);
        if (pindex != LV00_DYN_INVALID) {
            Lv00DynNode *parent = &graph->nodes[pindex];
            /* ä»Žçˆ¶èŠ‚ç‚¹çš„å­èŠ‚ç‚¹åˆ—è¡¨ä¸­ç§»é™?*/
            for (int j = 0; j < parent->child_count; j++) {
                if (parent->child_ids[j] == node_id) {
                    /* ç§»åŠ¨åŽç»­å…ƒç´  */
                    for (int k = j; k < parent->child_count - 1; k++) {
                        parent->child_ids[k] = parent->child_ids[k + 1];
                    }
                    parent->child_count--;
                    break;
                }
            }
        }
    }

    /* æ–­å¼€æ‰€æœ‰å­èŠ‚ç‚¹çš„å…³ç³»ï¼ˆå­èŠ‚ç‚¹ä¸å†æŒ‡å‘æ­¤çˆ¶èŠ‚ç‚¹ï¼‰ */
    int pstart = graph->parent_adj_offsets[index];
    int pend = graph->parent_adj_offsets[index + 1];
    for (int i = pstart; i < pend; i++) {
        int child_idx = graph->parent_adj[i];
        if (child_idx != LV00_DYN_INVALID && child_idx < graph->node_count) {
            Lv00DynNode *child = &graph->nodes[child_idx];
            for (int j = 0; j < child->parent_count; j++) {
                if (child->parent_ids[j] == node_id) {
                    for (int k = j; k < child->parent_count - 1; k++) {
                        child->parent_ids[k] = child->parent_ids[k + 1];
                    }
                    child->parent_count--;
                    break;
                }
            }
        }
    }

    /* æ ‡è®°èŠ‚ç‚¹ä¸ºæ— æ•?*/
    graph->id_to_index[node_id] = LV00_DYN_INVALID;
    node->state = LV00_DYN_STATE_ERROR;

    return true;
}

int lv00_dyn_graph_get_parents(
    const Lv00DynGraph *graph,
    int node_id,
    int *out_parents,
    int max_count)
{
    Lv00DynNode *node = lv00_dyn_graph_get_node((Lv00DynGraph *)graph, node_id);
    if (!node) return 0;

    int count = (node->parent_count < max_count) ? node->parent_count : max_count;
    for (int i = 0; i < count; i++) {
        out_parents[i] = node->parent_ids[i];
    }
    return node->parent_count;
}

int lv00_dyn_graph_get_children(
    const Lv00DynGraph *graph,
    int node_id,
    int *out_children,
    int max_count)
{
    Lv00DynNode *node = lv00_dyn_graph_get_node((Lv00DynGraph *)graph, node_id);
    if (!node) return 0;

    int count = (node->child_count < max_count) ? node->child_count : max_count;
    for (int i = 0; i < count; i++) {
        out_children[i] = node->child_ids[i];
    }
    return node->child_count;
}

/* ========================================================================
 * ç¬¬å…­éƒ¨åˆ†ï¼šçº§è”æ›´æ–? * ======================================================================== */

static void update_node_params(Lv00DynGraph *graph, int node_id)
{
    Lv00DynNode *node = lv00_dyn_graph_get_node(graph, node_id);
    if (!node) return;

    /* æ ¹æ®èŠ‚ç‚¹ç±»åž‹è®¡ç®—å‚æ•° */
    switch (node->type) {
        case LV00_DYN_NODE_MIDPOINT: {
            /* ä¸­ç‚¹ = (p1 + p2) / 2 */
            if (node->parent_count >= 2) {
                Lv00DynNode *p1 = lv00_dyn_graph_get_node(graph, node->parent_ids[0]);
                Lv00DynNode *p2 = lv00_dyn_graph_get_node(graph, node->parent_ids[1]);
                if (p1 && p2 && p1->param_count >= 2 && p2->param_count >= 2) {
                    node->params[0] = (p1->params[0] + p2->params[0]) / 2.0;
                    node->params[1] = (p1->params[1] + p2->params[1]) / 2.0;
                    node->param_count = 2;
                }
            }
            break;
        }

        case LV00_DYN_NODE_DISTANCE: {
            /* è·ç¦» = sqrt((p1.x - p2.x)^2 + (p1.y - p2.y)^2) */
            if (node->parent_count >= 2) {
                Lv00DynNode *p1 = lv00_dyn_graph_get_node(graph, node->parent_ids[0]);
                Lv00DynNode *p2 = lv00_dyn_graph_get_node(graph, node->parent_ids[1]);
                if (p1 && p2 && p1->param_count >= 2 && p2->param_count >= 2) {
                    double dx = p1->params[0] - p2->params[0];
                    double dy = p1->params[1] - p2->params[1];
                    node->params[0] = sqrt(dx * dx + dy * dy);
                    node->param_count = 1;
                }
            }
            break;
        }

        default:
            /* å…¶ä»–ç±»åž‹ä¿æŒåŽŸå‚æ•?*/
            break;
    }

    node->state = LV00_DYN_STATE_VALID;
    node->update_count++;
    graph->total_updates++;
}

int lv00_dyn_graph_update_cascade(
    Lv00DynGraph *graph,
    int root_id,
    Lv00DynUpdateFunc update_func)
{
    if (!graph) return -1;

    Lv00DynNode *root = lv00_dyn_graph_get_node(graph, root_id);
    if (!root) return -1;

    int updated = 0;
    int stack[256];
    int top = 0;

    stack[top++] = root_id;
    root->marks |= LV00_DYN_MARK_VISITED;

    while (top > 0 && top < 256) {
        int current_id = stack[--top];
        Lv00DynNode *current = lv00_dyn_graph_get_node(graph, current_id);

        if (!current) continue;

        /* è·³è¿‡å·²æ›´æ–°çš„èŠ‚ç‚¹ */
        if (current->marks & LV00_DYN_MARK_UPDATED) continue;

        /* æ£€æŸ¥æ˜¯å¦æœ‰æœªæ›´æ–°çš„çˆ¶èŠ‚ç‚?*/
        bool all_parents_updated = true;
        for (int i = 0; i < current->parent_count; i++) {
            Lv00DynNode *parent = lv00_dyn_graph_get_node(graph, current->parent_ids[i]);
            if (parent && !(parent->marks & LV00_DYN_MARK_UPDATED)) {
                all_parents_updated = false;
                break;
            }
        }

        if (!all_parents_updated) {
            /* å°†èŠ‚ç‚¹æ”¾å›žæ ˆï¼Œç­‰å¾…çˆ¶èŠ‚ç‚¹æ›´æ–° */
            stack[top++] = current_id;
            /* å…ˆå¤„ç†çˆ¶èŠ‚ç‚¹ */
            for (int i = 0; i < current->parent_count; i++) {
                Lv00DynNode *parent = lv00_dyn_graph_get_node(graph, current->parent_ids[i]);
                if (parent && !(parent->marks & LV00_DYN_MARK_VISITED)) {
                    stack[top++] = current->parent_ids[i];
                    parent->marks |= LV00_DYN_MARK_VISITED;
                }
            }
            continue;
        }

        /* æ›´æ–°å½“å‰èŠ‚ç‚¹ */
        if (update_func) {
            update_func(graph, current_id);
        } else {
            update_node_params(graph, current_id);
        }
        current->marks |= LV00_DYN_MARK_UPDATED;
        updated++;

        /* å°†å­èŠ‚ç‚¹åŠ å…¥æ ?*/
        for (int i = 0; i < current->child_count; i++) {
            Lv00DynNode *child = lv00_dyn_graph_get_node(graph, current->child_ids[i]);
            if (child && !(child->marks & LV00_DYN_MARK_VISITED)) {
                stack[top++] = current->child_ids[i];
                child->marks |= LV00_DYN_MARK_VISITED;
            }
        }
    }

    /* æ¸…é™¤æ ‡è®° */
    for (int i = 0; i < graph->node_count; i++) {
        graph->nodes[i].marks &= ~(LV00_DYN_MARK_VISITED | LV00_DYN_MARK_UPDATED);
    }

    return updated;
}

int lv00_dyn_graph_update_chain(Lv00DynGraph *graph, int leaf_id)
{
    if (!graph) return 0;

    int updated = 0;
    int current = leaf_id;
    int visited[256];
    int visited_count = 0;

    while (current != LV00_DYN_INVALID && visited_count < 256) {
        /* æ£€æµ‹å¾ªçŽ?*/
        for (int i = 0; i < visited_count; i++) {
            if (visited[i] == current) {
                return updated; /* æ£€æµ‹åˆ°å¾ªçŽ¯ */
            }
        }
        visited[visited_count++] = current;

        Lv00DynNode *node = lv00_dyn_graph_get_node(graph, current);
        if (!node) break;

        update_node_params(graph, current);
        updated++;

        /* å‘ä¸Šåˆ°ç¬¬ä¸€ä¸ªæœªæ›´æ–°çš„çˆ¶èŠ‚ç‚¹ */
        bool found_unupdated_parent = false;
        for (int i = 0; i < node->parent_count; i++) {
            Lv00DynNode *parent = lv00_dyn_graph_get_node(graph, node->parent_ids[i]);
            if (parent && parent->state == LV00_DYN_STATE_DIRTY) {
                current = node->parent_ids[i];
                found_unupdated_parent = true;
                break;
            }
        }

        if (!found_unupdated_parent) {
            break;
        }
    }

    return updated;
}

void lv00_dyn_graph_mark_dirty(Lv00DynGraph *graph, int node_id)
{
    Lv00DynNode *node = lv00_dyn_graph_get_node(graph, node_id);
    if (!node || node->state == LV00_DYN_STATE_DIRTY) return;

    node->state = LV00_DYN_STATE_DIRTY;

    /* é€’å½’æ ‡è®°æ‰€æœ‰å­èŠ‚ç‚¹ */
    int stack[256];
    int top = 0;
    stack[top++] = node_id;

    while (top > 0 && top < 256) {
        int current = stack[--top];
        Lv00DynNode *current_node = lv00_dyn_graph_get_node(graph, current);
        if (!current_node) continue;

        for (int i = 0; i < current_node->child_count; i++) {
            Lv00DynNode *child = lv00_dyn_graph_get_node(graph, current_node->child_ids[i]);
            if (child && child->state != LV00_DYN_STATE_DIRTY) {
                child->state = LV00_DYN_STATE_DIRTY;
                stack[top++] = current_node->child_ids[i];
            }
        }
    }
}

int lv00_dyn_graph_update_all(Lv00DynGraph *graph)
{
    if (!graph) return 0;

    /* æ‰¾å‡ºæ‰€æœ‰æ ¹èŠ‚ç‚¹ï¼ˆæ— çˆ¶èŠ‚ç‚¹ä¸”ä¸?DIRTYï¼‰å¹¶æ›´æ–° */
    int updated = 0;
    for (int i = 0; i < graph->node_count; i++) {
        Lv00DynNode *node = &graph->nodes[i];
        if (node->state == LV00_DYN_STATE_DIRTY && node->parent_count == 0) {
            updated += lv00_dyn_graph_update_cascade(graph, node->id, NULL);
        }
    }

    return updated;
}

/* ========================================================================
 * ç¬¬ä¸ƒéƒ¨åˆ†ï¼šå¾ªçŽ¯æ£€æµ? * ======================================================================== */

bool lv00_dyn_graph_has_path(
    const Lv00DynGraph *graph,
    int start_id,
    int target_id)
{
    if (!graph || start_id == target_id) return false;

    bool visited[256] = {false};
    int queue[256];
    int front = 0, rear = 0;

    queue[rear++] = start_id;
    visited[start_id % 256] = true;

    while (front < rear && rear < 256) {
        int current = queue[front++];
        Lv00DynNode *node = lv00_dyn_graph_get_node((Lv00DynGraph *)graph, current);
        if (!node) continue;

        for (int i = 0; i < node->child_count; i++) {
            int child_id = node->child_ids[i];
            if (child_id == target_id) return true;

            if (!visited[child_id % 256]) {
                visited[child_id % 256] = true;
                queue[rear++] = child_id;
            }
        }
    }

    return false;
}

bool lv00_dyn_graph_would_create_cycle(
    const Lv00DynGraph *graph,
    int parent_id,
    int child_id)
{
    /* å¦‚æžœ parent æ˜?child çš„ç¥–å…ˆï¼Œåˆ™æ·»åŠ è¾¹ä¼šå½¢æˆå¾ªçŽ?*/
    return lv00_dyn_graph_has_path(graph, child_id, parent_id);
}

int lv00_dyn_graph_topological_sort(
    const Lv00DynGraph *graph,
    int *out_order)
{
    if (!graph || !out_order) return -1;

    /* Kahn ç®—æ³• */
    int *in_degree = (int *)calloc(graph->node_count, sizeof(int));
    if (!in_degree) return -1;

    /* è®¡ç®—å…¥åº¦ */
    for (int i = 0; i < graph->node_count; i++) {
        Lv00DynNode *node = &graph->nodes[i];
        in_degree[i] = node->parent_count;
    }

    /* æ‰¾åˆ°æ‰€æœ‰å…¥åº¦ä¸º 0 çš„èŠ‚ç‚¹ï¼ˆæ ¹èŠ‚ç‚¹ï¼‰ */
    int queue[1024];
    int front = 0, rear = 0;
    for (int i = 0; i < graph->node_count; i++) {
        if (in_degree[i] == 0) {
            queue[rear++] = i;
        }
    }

    int sorted_count = 0;

    while (front < rear && rear < 1024) {
        int current = queue[front++];
        out_order[sorted_count++] = graph->nodes[current].id;

        Lv00DynNode *node = &graph->nodes[current];
        for (int i = 0; i < node->child_count; i++) {
            int child_idx = get_node_index(graph, node->child_ids[i]);
            if (child_idx != LV00_DYN_INVALID) {
                in_degree[child_idx]--;
                if (in_degree[child_idx] == 0) {
                    queue[rear++] = child_idx;
                }
            }
        }
    }

    free(in_degree);

    /* å¦‚æžœæŽ’åºçš„èŠ‚ç‚¹æ•°ä¸ç­‰äºŽæ€»èŠ‚ç‚¹æ•°ï¼Œè¯´æ˜Žå­˜åœ¨å¾ªçŽ?*/
    if (sorted_count != graph->node_count) {
        return -1;
    }

    return sorted_count;
}

/* ========================================================================
 * ç¬¬å…«éƒ¨åˆ†ï¼šä¾¿æ·æž„é€ å‡½æ•? * ======================================================================== */

int lv00_dyn_create_point(Lv00DynGraph *graph, double x, double y)
{
    double params[2] = {x, y};
    return lv00_dyn_graph_add_node(
        graph, LV00_DYN_NODE_POINT, NULL, 0, params, 2);
}

int lv00_dyn_create_line(Lv00DynGraph *graph, int p1_id, int p2_id)
{
    int parents[2] = {p1_id, p2_id};
    return lv00_dyn_graph_add_node(
        graph, LV00_DYN_NODE_LINE, parents, 2, NULL, 0);
}

int lv00_dyn_create_circle(Lv00DynGraph *graph, int center_id, int point_id)
{
    int parents[2] = {center_id, point_id};
    return lv00_dyn_graph_add_node(
        graph, LV00_DYN_NODE_CIRCLE, parents, 2, NULL, 0);
}

int lv00_dyn_create_midpoint(Lv00DynGraph *graph, int p1_id, int p2_id)
{
    int parents[2] = {p1_id, p2_id};
    return lv00_dyn_graph_add_node(
        graph, LV00_DYN_NODE_MIDPOINT, parents, 2, NULL, 0);
}

int lv00_dyn_create_parallel(Lv00DynGraph *graph, int base_line_id, int through_point_id)
{
    int parents[2] = {base_line_id, through_point_id};
    return lv00_dyn_graph_add_node(
        graph, LV00_DYN_NODE_PARALLEL, parents, 2, NULL, 0);
}

int lv00_dyn_create_perpendicular(Lv00DynGraph *graph, int base_line_id, int through_point_id)
{
    int parents[2] = {base_line_id, through_point_id};
    return lv00_dyn_graph_add_node(
        graph, LV00_DYN_NODE_PERPENDICULAR, parents, 2, NULL, 0);
}

int lv00_dyn_create_distance(Lv00DynGraph *graph, int p1_id, int p2_id)
{
    int parents[2] = {p1_id, p2_id};
    return lv00_dyn_graph_add_node(
        graph, LV00_DYN_NODE_DISTANCE, parents, 2, NULL, 0);
}

/* ========================================================================
 * ç¬¬ä¹éƒ¨åˆ†ï¼šç»Ÿè®? * ======================================================================== */

void lv00_dyn_graph_get_stats(
    const Lv00DynGraph *graph,
    Lv00DynGraphStats *out_stats)
{
    if (!graph || !out_stats) return;

    memset(out_stats, 0, sizeof(Lv00DynGraphStats));

    out_stats->total_nodes = graph->node_count;
    out_stats->total_updates = graph->total_updates;

    int max_children = 0;
    int max_parents = 0;

    for (int i = 0; i < graph->node_count; i++) {
        Lv00DynNode *node = &graph->nodes[i];

        if (node->parent_count == 0) {
            out_stats->free_nodes++;
        } else {
            out_stats->derived_nodes++;
        }

        if (node->state == LV00_DYN_STATE_DIRTY) {
            out_stats->dirty_nodes++;
        }

        if (node->child_count > max_children) {
            max_children = node->child_count;
        }
        if (node->parent_count > max_parents) {
            max_parents = node->parent_count;
        }
    }

    out_stats->max_children = max_children;
    out_stats->max_parents = max_parents;
}

void lv00_dyn_graph_clear_dirty(Lv00DynGraph *graph)
{
    if (!graph) return;

    for (int i = 0; i < graph->node_count; i++) {
        Lv00DynNode *node = &graph->nodes[i];
        if (node->state == LV00_DYN_STATE_DIRTY) {
            node->state = LV00_DYN_STATE_VALID;
        }
    }
}

void lv00_dyn_graph_reset_states(Lv00DynGraph *graph)
{
    if (!graph) return;

    for (int i = 0; i < graph->node_count; i++) {
        graph->nodes[i].state = LV00_DYN_STATE_VALID;
    }
}
