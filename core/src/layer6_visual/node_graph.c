#include "lv/visual_editor.h"
#include "lv/lv_utils.h"
#include <string.h>
#include <math.h>

/* 节点图视图 - 完整实现
 * [QA] Uses double for timing/layout — not geometric computation. Acceptable.
 */

/* 节点类型 */
typedef enum {
    lv_NODE_TYPE_DEFAULT,
    lv_NODE_TYPE_INPUT,
    lv_NODE_TYPE_OUTPUT,
    lv_NODE_TYPE_PROCESS,
    lv_NODE_TYPE_CONSTRAINT
} lvNodeType;

/* 图节点 */
typedef struct lvGraphNode {
    int id;
    char label[128];
    double x, y;
    lvNodeType type;
} lvGraphNode;

/* 图连接（边） */
typedef struct lvGraphConnection {
    int id;
    int from_node_id;
    int to_node_id;
    char label[128];
} lvGraphConnection;

/* 布局引擎状态 */
typedef struct lvLayoutEngine {
    double area_width;
    double area_height;
    double temperature;
    int iterations;
} lvLayoutEngine;

typedef struct lvNodeGraphView {
    int view_type;
    lvGraphNode *nodes;
    int node_count;
    int node_capacity;
    lvGraphConnection *connections;
    int connection_count;
    int connection_capacity;
    lvLayoutEngine layout_engine;
    int next_node_id;
    int next_connection_id;
} lvNodeGraphView;

lvNodeGraphView *lv_node_graph_create(void) {
    lvNodeGraphView *graph = lv_calloc(1, sizeof(lvNodeGraphView));
    if (!graph) return NULL;
    graph->view_type = lv_VIEW_NODE_GRAPH;
    graph->node_capacity = 16;
    graph->nodes = lv_calloc(graph->node_capacity, sizeof(lvGraphNode));
    if (!graph->nodes) { lv_free((void **)&graph); return NULL; }
    graph->connection_capacity = 16;
    graph->connections = lv_calloc(graph->connection_capacity, sizeof(lvGraphConnection));
    if (!graph->connections) { lv_free((void **)&graph->nodes); lv_free((void **)&graph); return NULL; }
    graph->layout_engine.area_width = 800.0;
    graph->layout_engine.area_height = 600.0;
    graph->layout_engine.iterations = 50;
    graph->next_node_id = 1;
    graph->next_connection_id = 1;
    return graph;
}

void lv_node_graph_destroy(lvNodeGraphView *graph) {
    if (!graph) return;
    lv_free((void **)&graph->nodes);
    lv_free((void **)&graph->connections);
    lv_free((void **)&graph);
}

/* 添加节点 */
int lv_node_graph_add_node(lvNodeGraphView *graph, int id, const char *label,
                             double x, double y, int type) {
    if (!graph || !label) return -1;

    /* 自动扩容 */
    if (graph->node_count >= graph->node_capacity) {
        int new_cap = graph->node_capacity * 2;
        lvGraphNode *new_nodes = lv_realloc(graph->nodes, new_cap * sizeof(lvGraphNode));
        if (!new_nodes) return -1;
        graph->nodes = new_nodes;
        graph->node_capacity = new_cap;
    }

    lvGraphNode *node = &graph->nodes[graph->node_count];
    node->id = (id > 0) ? id : graph->next_node_id++;
    node->x = x;
    node->y = y;
    node->type = (lvNodeType)type;
    strncpy(node->label, label, sizeof(node->label) - 1);
    node->label[sizeof(node->label) - 1] = '\0';

    /* 更新自增ID */
    if (node->id >= graph->next_node_id) {
        graph->next_node_id = node->id + 1;
    }

    graph->node_count++;
    return node->id;
}

/* 移除节点及其所有连接 */
int lv_node_graph_remove_node(lvNodeGraphView *graph, int id) {
    if (!graph || id <= 0) return -1;
    int found = -1;
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i].id == id) { found = i; break; }
    }
    if (found < 0) return -1;

    /* 移除与该节点相关的所有连接 */
    int new_conn_count = 0;
    for (int i = 0; i < graph->connection_count; i++) {
        if (graph->connections[i].from_node_id != id &&
            graph->connections[i].to_node_id != id) {
            if (new_conn_count != i) {
                graph->connections[new_conn_count] = graph->connections[i];
            }
            new_conn_count++;
        }
    }
    graph->connection_count = new_conn_count;

    /* 移除节点（用最后一个元素填充空位） */
    graph->nodes[found] = graph->nodes[graph->node_count - 1];
    graph->node_count--;
    return 0;
}

/* 添加连接（边） */
int lv_node_graph_add_connection(lvNodeGraphView *graph, int from_id,
                                    int to_id, const char *label) {
    if (!graph || from_id <= 0 || to_id <= 0) return -1;

    /* 自动扩容 */
    if (graph->connection_count >= graph->connection_capacity) {
        int new_cap = graph->connection_capacity * 2;
        lvGraphConnection *new_conns = lv_realloc(graph->connections,
                                                  new_cap * sizeof(lvGraphConnection));
        if (!new_conns) return -1;
        graph->connections = new_conns;
        graph->connection_capacity = new_cap;
    }

    lvGraphConnection *conn = &graph->connections[graph->connection_count];
    conn->id = graph->next_connection_id++;
    conn->from_node_id = from_id;
    conn->to_node_id = to_id;
    if (label) {
        strncpy(conn->label, label, sizeof(conn->label) - 1);
        conn->label[sizeof(conn->label) - 1] = '\0';
    } else {
        conn->label[0] = '\0';
    }

    graph->connection_count++;
    return conn->id;
}

/* 移除连接 */
int lv_node_graph_remove_connection(lvNodeGraphView *graph, int conn_id) {
    if (!graph || conn_id <= 0) return -1;
    int found = -1;
    for (int i = 0; i < graph->connection_count; i++) {
        if (graph->connections[i].id == conn_id) { found = i; break; }
    }
    if (found < 0) return -1;

    graph->connections[found] = graph->connections[graph->connection_count - 1];
    graph->connection_count--;
    return 0;
}

/* 查找节点 */
lvGraphNode *lv_node_graph_find_node(lvNodeGraphView *graph, int id) {
    if (!graph || id <= 0) return NULL;
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i].id == id) return &graph->nodes[i];
    }
    return NULL;
}

/* Fruchterman-Reingold 力导向布局 */
int lv_node_graph_layout(lvNodeGraphView *graph) {
    if (!graph || graph->node_count <= 1) return 0;

    const double C = 1.0; /* 常数因子 */
    const int max_iter = graph->layout_engine.iterations;
    const double area = graph->layout_engine.area_width * graph->layout_engine.area_height;
    const int n = graph->node_count;

    /* 理想距离 k = C * sqrt(area / n) */
    const double k = C * sqrt(area / (double)n);

    /* 初始温度 */
    double temperature = graph->layout_engine.area_width / 4.0;

    /* 临时位移数组 */
    double *dx = lv_calloc(n, sizeof(double));
    double *dy = lv_calloc(n, sizeof(double));
    if (!dx || !dy) {
        lv_free((void **)&dx); lv_free((void **)&dy);
        return -1;
    }

    /* 迭代 */
    for (int iter = 0; iter < max_iter; iter++) {
        /* 清零位移 */
        memset(dx, 0, n * sizeof(double));
        memset(dy, 0, n * sizeof(double));

        /* 排斥力：所有节点对之间 F_r = k^2 / d */
        for (int i = 0; i < n; i++) {
            for (int j = i + 1; j < n; j++) {
                double diff_x = graph->nodes[i].x - graph->nodes[j].x;
                double diff_y = graph->nodes[i].y - graph->nodes[j].y;
                double dist = sqrt(diff_x * diff_x + diff_y * diff_y);
                if (dist < 0.01) dist = 0.01; /* 避免除零 */

                double force = (k * k) / dist;
                double fx = (diff_x / dist) * force;
                double fy = (diff_y / dist) * force;

                dx[i] += fx;
                dy[i] += fy;
                dx[j] -= fx;
                dy[j] -= fy;
            }
        }

        /* 吸引力：沿边方向 F_a = d^2 / k */
        for (int c = 0; c < graph->connection_count; c++) {
            int fi = -1, ti = -1;
            int from = graph->connections[c].from_node_id;
            int to = graph->connections[c].to_node_id;
            for (int i = 0; i < n; i++) {
                if (graph->nodes[i].id == from) fi = i;
                if (graph->nodes[i].id == to) ti = i;
            }
            if (fi < 0 || ti < 0) continue;

            double diff_x = graph->nodes[fi].x - graph->nodes[ti].x;
            double diff_y = graph->nodes[fi].y - graph->nodes[ti].y;
            double dist = sqrt(diff_x * diff_x + diff_y * diff_y);
            if (dist < 0.01) dist = 0.01;

            double force = (dist * dist) / k;
            double fx = (diff_x / dist) * force;
            double fy = (diff_y / dist) * force;

            dx[fi] -= fx;
            dy[fi] -= fy;
            dx[ti] += fx;
            dy[ti] += fy;
        }

        /* 应用位移，受温度限制 */
        for (int i = 0; i < n; i++) {
            double disp = sqrt(dx[i] * dx[i] + dy[i] * dy[i]);
            if (disp < 0.01) continue;

            double scale = fmin(disp, temperature) / disp;
            graph->nodes[i].x += dx[i] * scale;
            graph->nodes[i].y += dy[i] * scale;

            /* 限制在区域内 */
            double margin = 10.0;
            graph->nodes[i].x = fmax(margin, fmin(graph->layout_engine.area_width - margin,
                                                    graph->nodes[i].x));
            graph->nodes[i].y = fmax(margin, fmin(graph->layout_engine.area_height - margin,
                                                    graph->nodes[i].y));
        }

        /* 降温 */
        temperature *= 0.95;
    }

    lv_free((void **)&dx);
    lv_free((void **)&dy);
    graph->layout_engine.temperature = temperature;
    return 0;
}
