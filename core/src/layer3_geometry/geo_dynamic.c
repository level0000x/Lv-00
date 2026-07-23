/**
 * @file geo_dynamic.c
 * @brief 动态几何依赖图实现 — 借鉴 GeoGebra 动态几何系统
 *
 * 实现策略：
 *   - 使用邻接表压缩存储父子关系
 *   - DFS 实现级联更新和循环检测
 *   - 拓扑排序用于批量更新
 *
 * @version 1.1.0
 */

#include "lv/geo_dynamic.h"

#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lv_utils.h"

#ifndef lv_PUBLIC_API
#define lv_PUBLIC_API
#endif

/* ========================================================================
 * 内部常量
 * ======================================================================== */

#define INITIAL_NODE_CAPACITY 64
#define INITIAL_ADJ_CAPACITY 256

/* ========================================================================
 * 内部数据结构
 * ======================================================================== */

/**
 * @brief 深度优先搜索状态
 */
typedef struct {
    lvDynGraph *graph;
    uint8_t *marks;
    int *stack;
    int stack_top;
    bool has_cycle;
} DFSContext;

/* ========================================================================
 * 第一部分：ID 到索引映射
 * ======================================================================== */

/**
 * @brief 初始化 ID 映射表
 */
static void init_id_map(lvDynGraph *graph) {
    graph->id_to_index = (int *) lv_malloc(graph->node_capacity * sizeof(int));
    if (!graph->id_to_index)
        return; /* OOM: caller should check graph->id_to_index */
    for (int i = 0; i < graph->node_capacity; i++) {
        graph->id_to_index[i] = lv_DYN_INVALID;
    }
}

/**
 * @brief 确保 ID 映射表足够大
 */
static bool ensure_id_map_capacity(lvDynGraph *graph, int new_capacity) {
    if (new_capacity <= graph->node_capacity)
        return true;

    int *new_map = (int *) lv_realloc(graph->id_to_index, new_capacity * sizeof(int));
    if (!new_map)
        return false;

    for (int i = graph->node_capacity; i < new_capacity; i++) {
        new_map[i] = lv_DYN_INVALID;
    }

    graph->id_to_index = new_map;
    graph->node_capacity = new_capacity;
    return true;
}

/**
 * @brief 注册节点 ID 到索引的映射
 */
static void register_node_id(lvDynGraph *graph, int node_id, int index) {
    if (node_id >= graph->node_capacity) {
        ensure_id_map_capacity(graph, node_id + 1);
    }
    graph->id_to_index[node_id] = index;
}

/**
 * @brief 获取节点 ID 对应的索引
 */
static int get_node_index(const lvDynGraph *graph, int node_id) {
    if (node_id < 0 || node_id >= graph->node_capacity) {
        return lv_DYN_INVALID;
    }
    return graph->id_to_index[node_id];
}

/* ========================================================================
 * 第二部分：邻接表操作
 * ======================================================================== */

/**
 * @brief 初始化邻接表
 */
static void init_adjacency(lvDynGraph *graph) {
    graph->parent_adj = (int *) lv_malloc(INITIAL_ADJ_CAPACITY * sizeof(int));
    graph->parent_adj_offsets = (int *) lv_malloc((graph->node_capacity + 1) * sizeof(int));
    graph->child_adj = (int *) lv_malloc(INITIAL_ADJ_CAPACITY * sizeof(int));
    graph->child_adj_offsets = (int *) lv_malloc((graph->node_capacity + 1) * sizeof(int));

    if (!graph->parent_adj || !graph->parent_adj_offsets || !graph->child_adj || !graph->child_adj_offsets) {
        /* OOM: caller should check these pointers */
        return;
    }

    graph->adj_capacity = INITIAL_ADJ_CAPACITY;

    for (int i = 0; i <= graph->node_capacity; i++) {
        graph->parent_adj_offsets[i] = 0;
        graph->child_adj_offsets[i] = 0;
    }
}

/**
 * @brief 确保邻接表容量
 */
static bool ensure_adj_capacity(lvDynGraph *graph, int needed) {
    if (needed <= graph->adj_capacity)
        return true;

    int new_cap = graph->adj_capacity * 2;
    while (new_cap < needed)
        new_cap *= 2;

    int *new_parent = (int *) lv_realloc(graph->parent_adj, new_cap * sizeof(int));
    int *new_child = (int *) lv_realloc(graph->child_adj, new_cap * sizeof(int));

    if (!new_parent || !new_child) {
        if (new_parent)
            lv_free((void **) &(new_parent));
        if (new_child)
            lv_free((void **) &(new_child));
        return false;
    }

    graph->parent_adj = new_parent;
    graph->child_adj = new_child;
    graph->adj_capacity = new_cap;
    return true;
}

/**
 * @brief 添加父节点关系
 */
static void add_parent_edge(lvDynGraph *graph, int node_idx, int parent_idx) {
    /* 计算当前父节点数量 */
    int start = graph->parent_adj_offsets[node_idx];
    int end = graph->parent_adj_offsets[node_idx + 1];
    int count = end - start;

    /* 检查是否已存在 */
    for (int i = start; i < end; i++) {
        if (graph->parent_adj[i] == parent_idx)
            return;
    }

    /* 扩容 */
    int total_needed = graph->adj_capacity + 1;
    if (!ensure_adj_capacity(graph, total_needed))
        return;

    /* 移动后续节点 */
    for (int i = graph->node_count; i > node_idx; i--) {
        graph->parent_adj_offsets[i + 1] = graph->parent_adj_offsets[i] + 1;
    }
    graph->parent_adj_offsets[node_idx + 1]++;

    /* 插入新父节点 */
    graph->parent_adj[end] = parent_idx;
}

/**
 * @brief 添加子节点关系
 */
static void add_child_edge(lvDynGraph *graph, int node_idx, int child_idx) {
    /* 计算当前子节点数量 */
    int start = graph->child_adj_offsets[node_idx];
    int end = graph->child_adj_offsets[node_idx + 1];
    int count = end - start;

    /* 检查是否已存在 */
    for (int i = start; i < end; i++) {
        if (graph->child_adj[i] == child_idx)
            return;
    }

    /* 同时更新节点的 child_ids 数组 */
    lvDynNode *parent_node = &graph->nodes[node_idx];
    if (parent_node->child_count < 16) {
        parent_node->child_ids[parent_node->child_count++] = graph->nodes[child_idx].id;
    }

    /* 扩容 */
    int total_needed = graph->adj_capacity + 1;
    if (!ensure_adj_capacity(graph, total_needed))
        return;

    /* 移动后续节点 */
    for (int i = graph->node_count; i > node_idx; i--) {
        graph->child_adj_offsets[i + 1] = graph->child_adj_offsets[i] + 1;
    }
    graph->child_adj_offsets[node_idx + 1]++;

    /* 插入新子节点 */
    graph->child_adj[end] = child_idx;
}

/* ========================================================================
 * 第三部分：默认配置
 * ======================================================================== */

lvDynGraphConfig lv_dyn_graph_default_config(void) {
    lvDynGraphConfig cfg;
    cfg.max_nodes = 10000;
    cfg.max_parents = 4;
    cfg.max_children = 16;
    cfg.detect_cycles = true;
    cfg.max_update_depth = 100;
    return cfg;
}

/* ========================================================================
 * 第四部分：创建与释放
 * ======================================================================== */

lvDynGraph *lv_dyn_graph_create(const lvDynGraphConfig *config) {
    lvDynGraph *graph = (lvDynGraph *) lv_calloc(1, sizeof(lvDynGraph));
    if (!graph)
        return NULL;

    if (config) {
        graph->config = *config;
    } else {
        graph->config = lv_dyn_graph_default_config();
    }

    graph->node_capacity = INITIAL_NODE_CAPACITY;
    graph->node_count = 0;

    graph->nodes = (lvDynNode *) lv_calloc(graph->node_capacity, sizeof(lvDynNode));

    init_id_map(graph);
    init_adjacency(graph);

    return graph;
}

void lv_dyn_graph_destroy(lvDynGraph *graph) {
    if (!graph)
        return;

    lv_free((void **) &(graph->nodes));
    lv_free((void **) &(graph->id_to_index));
    lv_free((void **) &(graph->parent_adj));
    lv_free((void **) &(graph->parent_adj_offsets));
    lv_free((void **) &(graph->child_adj));
    lv_free((void **) &(graph->child_adj_offsets));
    lv_free((void **) &(graph));
}

/* ========================================================================
 * 第五部分：节点操作
 * ======================================================================== */

int lv_dyn_graph_add_node(lvDynGraph *graph, lvDynNodeType type, const int *parent_ids, int parent_count,
                          const double *params, int param_count) {
    if (!graph || graph->node_count >= graph->config.max_nodes) {
        return lv_DYN_INVALID;
    }

    /* 生成新节点 ID */
    int new_id = graph->node_count;

    /* 扩容节点数组 */
    if (graph->node_count >= graph->node_capacity) {
        int new_cap = graph->node_capacity * 2;
        lvDynNode *new_nodes = (lvDynNode *) lv_realloc(graph->nodes, new_cap * sizeof(lvDynNode));
        if (!new_nodes)
            return lv_DYN_INVALID;

        int *new_parent_offsets = (int *) lv_realloc(graph->parent_adj_offsets, (new_cap + 1) * sizeof(int));
        int *new_child_offsets = (int *) lv_realloc(graph->child_adj_offsets, (new_cap + 1) * sizeof(int));

        if (!new_parent_offsets || !new_child_offsets) {
            if (new_parent_offsets)
                lv_free((void **) &(new_parent_offsets));
            if (new_child_offsets)
                lv_free((void **) &(new_child_offsets));
            return lv_DYN_INVALID;
        }

        graph->nodes = new_nodes;
        graph->parent_adj_offsets = new_parent_offsets;
        graph->child_adj_offsets = new_child_offsets;
        graph->node_capacity = new_cap;
    }

    /* 初始化节点 */
    lvDynNode *node = &graph->nodes[graph->node_count];
    memset(node, 0, sizeof(lvDynNode));
    node->id = new_id;
    node->type = type;
    node->state = lv_DYN_STATE_VALID;
    node->parent_count = 0;
    node->child_count = 0;
    node->param_count = param_count;

    /* 复制父节点 */
    if (parent_ids && parent_count > 0) {
        for (int i = 0; i < parent_count && i < 4; i++) {
            int pindex = get_node_index(graph, parent_ids[i]);
            if (pindex != lv_DYN_INVALID) {
                node->parent_ids[i] = parent_ids[i];
                node->parent_count++;
                add_parent_edge(graph, graph->node_count, pindex);
                add_child_edge(graph, pindex, graph->node_count);
            }
        }
    }

    /* 复制参数 */
    if (params && param_count > 0) {
        for (int i = 0; i < param_count && i < 8; i++) {
            node->params[i] = params[i];
        }
    }

    /* 注册 ID */
    register_node_id(graph, new_id, graph->node_count);

    /* 更新偏移数组 */
    graph->parent_adj_offsets[graph->node_count + 1] = graph->parent_adj_offsets[graph->node_count];
    graph->child_adj_offsets[graph->node_count + 1] = graph->child_adj_offsets[graph->node_count];

    graph->node_count++;
    return new_id;
}

lvDynNode *lv_dyn_graph_get_node(lvDynGraph *graph, int node_id) {
    int index = get_node_index(graph, node_id);
    if (index == lv_DYN_INVALID || index >= graph->node_count) {
        return NULL;
    }
    return &graph->nodes[index];
}

bool lv_dyn_graph_remove_node(lvDynGraph *graph, int node_id) {
    lvDynNode *node = lv_dyn_graph_get_node(graph, node_id);
    if (!node)
        return false;

    int index = get_node_index(graph, node_id);

    /* 断开所有父子关系 */
    /* 移除作为子节点的关系（父节点不再指向此节点） */
    for (int i = 0; i < node->parent_count; i++) {
        int pindex = get_node_index(graph, node->parent_ids[i]);
        if (pindex != lv_DYN_INVALID) {
            lvDynNode *parent = &graph->nodes[pindex];
            /* 从父节点的子节点列表中移除 */
            for (int j = 0; j < parent->child_count; j++) {
                if (parent->child_ids[j] == node_id) {
                    /* 移动后续元素 */
                    for (int k = j; k < parent->child_count - 1; k++) {
                        parent->child_ids[k] = parent->child_ids[k + 1];
                    }
                    parent->child_count--;
                    break;
                }
            }
        }
    }

    /* 断开所有子节点的关系（子节点不再指向此父节点） */
    int pstart = graph->parent_adj_offsets[index];
    int pend = graph->parent_adj_offsets[index + 1];
    for (int i = pstart; i < pend; i++) {
        int child_idx = graph->parent_adj[i];
        if (child_idx != lv_DYN_INVALID && child_idx < graph->node_count) {
            lvDynNode *child = &graph->nodes[child_idx];
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

    /* 标记节点为无效 */
    graph->id_to_index[node_id] = lv_DYN_INVALID;
    node->state = lv_DYN_STATE_ERROR;

    return true;
}

int lv_dyn_graph_get_parents(const lvDynGraph *graph, int node_id, int *out_parents, int max_count) {
    lvDynNode *node = lv_dyn_graph_get_node((lvDynGraph *) graph, node_id);
    if (!node)
        return 0;

    int count = (node->parent_count < max_count) ? node->parent_count : max_count;
    for (int i = 0; i < count; i++) {
        out_parents[i] = node->parent_ids[i];
    }
    return node->parent_count;
}

int lv_dyn_graph_get_children(const lvDynGraph *graph, int node_id, int *out_children, int max_count) {
    lvDynNode *node = lv_dyn_graph_get_node((lvDynGraph *) graph, node_id);
    if (!node)
        return 0;

    int count = (node->child_count < max_count) ? node->child_count : max_count;
    for (int i = 0; i < count; i++) {
        out_children[i] = node->child_ids[i];
    }
    return node->child_count;
}

/* ========================================================================
 * 第六部分：级联更新
 * ======================================================================== */

static void update_node_params(lvDynGraph *graph, int node_id) {
    lvDynNode *node = lv_dyn_graph_get_node(graph, node_id);
    if (!node)
        return;

    /* 根据节点类型计算参数 */
    switch (node->type) {
        case lv_DYN_NODE_MIDPOINT: {
            /* 中点 = (p1 + p2) / 2 */
            if (node->parent_count >= 2) {
                lvDynNode *p1 = lv_dyn_graph_get_node(graph, node->parent_ids[0]);
                lvDynNode *p2 = lv_dyn_graph_get_node(graph, node->parent_ids[1]);
                if (p1 && p2 && p1->param_count >= 2 && p2->param_count >= 2) {
                    node->params[0] = p1->params[0] + (p2->params[0] - p1->params[0]) / 2.0;
                    node->params[1] = p1->params[1] + (p2->params[1] - p1->params[1]) / 2.0;
                    node->param_count = 2;
                }
            }
            break;
        }

        case lv_DYN_NODE_DISTANCE: {
            /* 距离 = sqrt((p1.x - p2.x)^2 + (p1.y - p2.y)^2) */
            if (node->parent_count >= 2) {
                lvDynNode *p1 = lv_dyn_graph_get_node(graph, node->parent_ids[0]);
                lvDynNode *p2 = lv_dyn_graph_get_node(graph, node->parent_ids[1]);
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
            /* 其他类型保持原参数 */
            break;
    }

    node->state = lv_DYN_STATE_VALID;
    node->update_count++;
    graph->total_updates++;
}

int lv_dyn_graph_update_cascade(lvDynGraph *graph, int root_id, lvDynUpdateFunc update_func) {
    if (!graph)
        return -1;

    lvDynNode *root = lv_dyn_graph_get_node(graph, root_id);
    if (!root)
        return -1;

    int updated = 0;
    int stack[256];
    int top = 0;

    stack[top++] = root_id;
    root->marks |= lv_DYN_MARK_VISITED;

    while (top > 0 && top < 256) {
        int current_id = stack[--top];
        lvDynNode *current = lv_dyn_graph_get_node(graph, current_id);

        if (!current)
            continue;

        /* 跳过已更新的节点 */
        if (current->marks & lv_DYN_MARK_UPDATED)
            continue;

        /* 检查是否有未更新的父节点 */
        bool all_parents_updated = true;
        for (int i = 0; i < current->parent_count; i++) {
            lvDynNode *parent = lv_dyn_graph_get_node(graph, current->parent_ids[i]);
            if (parent && !(parent->marks & lv_DYN_MARK_UPDATED)) {
                all_parents_updated = false;
                break;
            }
        }

        if (!all_parents_updated) {
            /* 将节点放回栈，等待父节点更新 */
            stack[top++] = current_id;
            /* 先处理父节点 */
            for (int i = 0; i < current->parent_count; i++) {
                lvDynNode *parent = lv_dyn_graph_get_node(graph, current->parent_ids[i]);
                if (parent && !(parent->marks & lv_DYN_MARK_VISITED)) {
                    stack[top++] = current->parent_ids[i];
                    parent->marks |= lv_DYN_MARK_VISITED;
                }
            }
            continue;
        }

        /* 更新当前节点 */
        if (update_func) {
            update_func(graph, current_id);
        } else {
            update_node_params(graph, current_id);
        }
        current->marks |= lv_DYN_MARK_UPDATED;
        updated++;

        /* 将子节点加入栈 */
        for (int i = 0; i < current->child_count; i++) {
            lvDynNode *child = lv_dyn_graph_get_node(graph, current->child_ids[i]);
            if (child && !(child->marks & lv_DYN_MARK_VISITED)) {
                stack[top++] = current->child_ids[i];
                child->marks |= lv_DYN_MARK_VISITED;
            }
        }
    }

    /* 清除标记 */
    for (int i = 0; i < graph->node_count; i++) {
        graph->nodes[i].marks &= ~(lv_DYN_MARK_VISITED | lv_DYN_MARK_UPDATED);
    }

    return updated;
}

int lv_dyn_graph_update_chain(lvDynGraph *graph, int leaf_id) {
    if (!graph)
        return 0;

    int updated = 0;
    int current = leaf_id;
    int visited[256];
    int visited_count = 0;

    while (current != lv_DYN_INVALID && visited_count < 256) {
        /* 检测循环 */
        for (int i = 0; i < visited_count; i++) {
            if (visited[i] == current) {
                return updated; /* 检测到循环 */
            }
        }
        visited[visited_count++] = current;

        lvDynNode *node = lv_dyn_graph_get_node(graph, current);
        if (!node)
            break;

        update_node_params(graph, current);
        updated++;

        /* 向上到第一个未更新的父节点 */
        bool found_unupdated_parent = false;
        for (int i = 0; i < node->parent_count; i++) {
            lvDynNode *parent = lv_dyn_graph_get_node(graph, node->parent_ids[i]);
            if (parent && parent->state == lv_DYN_STATE_DIRTY) {
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

void lv_dyn_graph_mark_dirty(lvDynGraph *graph, int node_id) {
    lvDynNode *node = lv_dyn_graph_get_node(graph, node_id);
    if (!node || node->state == lv_DYN_STATE_DIRTY)
        return;

    node->state = lv_DYN_STATE_DIRTY;

    /* 递归标记所有子节点 */
    int stack[256];
    int top = 0;
    stack[top++] = node_id;

    while (top > 0 && top < 256) {
        int current = stack[--top];
        lvDynNode *current_node = lv_dyn_graph_get_node(graph, current);
        if (!current_node)
            continue;

        for (int i = 0; i < current_node->child_count; i++) {
            lvDynNode *child = lv_dyn_graph_get_node(graph, current_node->child_ids[i]);
            if (child && child->state != lv_DYN_STATE_DIRTY) {
                child->state = lv_DYN_STATE_DIRTY;
                stack[top++] = current_node->child_ids[i];
            }
        }
    }
}

int lv_dyn_graph_update_all(lvDynGraph *graph) {
    if (!graph)
        return 0;

    /* 找出所有根节点（无父节点且不 DIRTY）并更新 */
    int updated = 0;
    for (int i = 0; i < graph->node_count; i++) {
        lvDynNode *node = &graph->nodes[i];
        if (node->state == lv_DYN_STATE_DIRTY && node->parent_count == 0) {
            updated += lv_dyn_graph_update_cascade(graph, node->id, NULL);
        }
    }

    return updated;
}

/* ========================================================================
 * 第七部分：循环检测
 * ======================================================================== */

bool lv_dyn_graph_has_path(const lvDynGraph *graph, int start_id, int target_id) {
    if (!graph || start_id == target_id)
        return false;

    bool visited[256] = {false};
    int queue[256];
    int front = 0, rear = 0;

    queue[rear++] = start_id;
    /* 使用 unsigned 类型取模确保下标非负（C 中负数取模结果为负） */
    visited[(unsigned int) start_id % 256] = true;

    while (front < rear && rear < 256) {
        int current = queue[front++];
        lvDynNode *node = lv_dyn_graph_get_node((lvDynGraph *) graph, current);
        if (!node)
            continue;

        for (int i = 0; i < node->child_count; i++) {
            int child_id = node->child_ids[i];
            if (child_id == target_id)
                return true;

            if (!visited[(unsigned int) child_id % 256]) {
                visited[(unsigned int) child_id % 256] = true;
                queue[rear++] = child_id;
            }
        }
    }

    return false;
}

bool lv_dyn_graph_would_create_cycle(const lvDynGraph *graph, int parent_id, int child_id) {
    /* 如果 parent 是 child 的祖先，则添加边会形成循环 */
    return lv_dyn_graph_has_path(graph, child_id, parent_id);
}

int lv_dyn_graph_topological_sort(const lvDynGraph *graph, int *out_order) {
    if (!graph || !out_order)
        return -1;

    /* Kahn 算法 */
    int *in_degree = (int *) lv_calloc(graph->node_count, sizeof(int));
    if (!in_degree)
        return -1;

    /* 计算入度 */
    for (int i = 0; i < graph->node_count; i++) {
        lvDynNode *node = &graph->nodes[i];
        in_degree[i] = node->parent_count;
    }

    /* 找到所有入度为 0 的节点（根节点） */
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

        lvDynNode *node = &graph->nodes[current];
        for (int i = 0; i < node->child_count; i++) {
            int child_idx = get_node_index(graph, node->child_ids[i]);
            if (child_idx != lv_DYN_INVALID) {
                in_degree[child_idx]--;
                if (in_degree[child_idx] == 0) {
                    queue[rear++] = child_idx;
                }
            }
        }
    }

    lv_free((void **) &(in_degree));

    /* 如果排序的节点数不等于总节点数，说明存在循环 */
    if (sorted_count != graph->node_count) {
        return -1;
    }

    return sorted_count;
}

/* ========================================================================
 * 第八部分：便捷构造函数
 * ======================================================================== */

int lv_dyn_create_point(lvDynGraph *graph, double x, double y) {
    double params[2] = {x, y};
    return lv_dyn_graph_add_node(graph, lv_DYN_NODE_POINT, NULL, 0, params, 2);
}

int lv_dyn_create_line(lvDynGraph *graph, int p1_id, int p2_id) {
    int parents[2] = {p1_id, p2_id};
    return lv_dyn_graph_add_node(graph, lv_DYN_NODE_LINE, parents, 2, NULL, 0);
}

int lv_dyn_create_circle(lvDynGraph *graph, int center_id, int point_id) {
    int parents[2] = {center_id, point_id};
    return lv_dyn_graph_add_node(graph, lv_DYN_NODE_CIRCLE, parents, 2, NULL, 0);
}

int lv_dyn_create_midpoint(lvDynGraph *graph, int p1_id, int p2_id) {
    int parents[2] = {p1_id, p2_id};
    return lv_dyn_graph_add_node(graph, lv_DYN_NODE_MIDPOINT, parents, 2, NULL, 0);
}

int lv_dyn_create_parallel(lvDynGraph *graph, int base_line_id, int through_point_id) {
    int parents[2] = {base_line_id, through_point_id};
    return lv_dyn_graph_add_node(graph, lv_DYN_NODE_PARALLEL, parents, 2, NULL, 0);
}

int lv_dyn_create_perpendicular(lvDynGraph *graph, int base_line_id, int through_point_id) {
    int parents[2] = {base_line_id, through_point_id};
    return lv_dyn_graph_add_node(graph, lv_DYN_NODE_PERPENDICULAR, parents, 2, NULL, 0);
}

int lv_dyn_create_distance(lvDynGraph *graph, int p1_id, int p2_id) {
    int parents[2] = {p1_id, p2_id};
    return lv_dyn_graph_add_node(graph, lv_DYN_NODE_DISTANCE, parents, 2, NULL, 0);
}

/* ========================================================================
 * 第九部分：统计
 * ======================================================================== */

void lv_dyn_graph_get_stats(const lvDynGraph *graph, lvDynGraphStats *out_stats) {
    if (!graph || !out_stats)
        return;

    memset(out_stats, 0, sizeof(lvDynGraphStats));

    out_stats->total_nodes = graph->node_count;
    out_stats->total_updates = graph->total_updates;

    int max_children = 0;
    int max_parents = 0;

    for (int i = 0; i < graph->node_count; i++) {
        lvDynNode *node = &graph->nodes[i];

        if (node->parent_count == 0) {
            out_stats->free_nodes++;
        } else {
            out_stats->derived_nodes++;
        }

        if (node->state == lv_DYN_STATE_DIRTY) {
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

void lv_dyn_graph_clear_dirty(lvDynGraph *graph) {
    if (!graph)
        return;

    for (int i = 0; i < graph->node_count; i++) {
        lvDynNode *node = &graph->nodes[i];
        if (node->state == lv_DYN_STATE_DIRTY) {
            node->state = lv_DYN_STATE_VALID;
        }
    }
}

void lv_dyn_graph_reset_states(lvDynGraph *graph) {
    if (!graph)
        return;

    for (int i = 0; i < graph->node_count; i++) {
        graph->nodes[i].state = lv_DYN_STATE_VALID;
    }
}
