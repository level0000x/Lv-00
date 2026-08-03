/**
 * @file node_graph.c
 * @brief 节点图视图实现
 *
 * @details 实现节点图视图，支持节点的添加/删除、连接的添加/删除、
 *          节点查找以及基于 Fruchterman-Reingold 力导向算法的布局引擎。
 *          节点和连接使用动态数组管理，支持自动扩容。
 *
 * @note 涉及 double 运算用于布局，不涉及几何精度计算
 * @author Lv-00 Project
 */

#include <math.h>
#include <string.h>

#include "lv/lv_check.h"
#include "lv/lv_utils.h"
#include "lv/visual_editor.h"
#include "lv/lv_internal.h"

/* 节点图视图 - 完整实现
 * [QA] Uses double for timing/layout — not geometric computation. Acceptable.
 */

/** @brief 节点类型枚举 */
typedef enum {
    lv_NODE_TYPE_DEFAULT,   /**< 默认节点 */
    lv_NODE_TYPE_INPUT,     /**< 输入节点 */
    lv_NODE_TYPE_OUTPUT,    /**< 输出节点 */
    lv_NODE_TYPE_PROCESS,   /**< 处理节点 */
    lv_NODE_TYPE_CONSTRAINT /**< 约束节点 */
} lvNodeType;

/** @brief 图节点结构 */
typedef struct lvGraphNode {
    int id;          /**< 节点唯一标识 */
    char label[128]; /**< 节点标签 */
    double x, y;     /**< 节点位置坐标 */
    lvNodeType type; /**< 节点类型 */
} lvGraphNode;

/** @brief 图连接（边）结构 */
typedef struct lvGraphConnection {
    int id;           /**< 连接唯一标识 */
    int from_node_id; /**< 源节点ID */
    int to_node_id;   /**< 目标节点ID */
    char label[128];  /**< 连接标签 */
} lvGraphConnection;

/** @brief 布局引擎状态结构 */
typedef struct lvLayoutEngine {
    double area_width;  /**< 布局区域宽度 */
    double area_height; /**< 布局区域高度 */
    double temperature; /**< 当前温度（控制位移步长） */
    int iterations;     /**< 迭代次数 */
} lvLayoutEngine;

/** @brief 节点图视图内部结构 */
typedef struct lvNodeGraphView {
    int view_type;                  /**< 视图类型标识 */
    lvDArray nodes_da;              /**< 节点动态数组（lvDArray<lvGraphNode>） */
    lvDArray connections_da;        /**< 连接动态数组（lvDArray<lvGraphConnection>） */
    lvLayoutEngine layout_engine;   /**< 布局引擎 */
    int next_node_id;               /**< 下一个节点ID */
    int next_connection_id;         /**< 下一个连接ID */
} lvNodeGraphView;

/**
 * @brief 创建节点图视图
 *
 * 分配并初始化节点图，预分配节点和连接数组的初始容量。
 *
 * @return 成功返回节点图指针，失败返回NULL
 */
lvNodeGraphView *lv_node_graph_create(void) {
    lvNodeGraphView *graph = lv_calloc(1, sizeof(lvNodeGraphView));
    if (!graph)
        return NULL;
    graph->view_type = lv_VIEW_NODE_GRAPH;
    lv_darray_init(&graph->nodes_da, sizeof(lvGraphNode));
    lv_darray_init(&graph->connections_da, sizeof(lvGraphConnection));
    graph->layout_engine.area_width = 800.0;
    graph->layout_engine.area_height = 600.0;
    graph->layout_engine.iterations = 50;
    graph->next_node_id = 1;
    graph->next_connection_id = 1;
    return graph;
}

/**
 * @brief 销毁节点图视图
 *
 * 释放节点数组、连接数组和视图结构体占用的内存。
 *
 * @param graph 节点图指针
 */
void lv_node_graph_destroy(lvNodeGraphView *graph) {
    if (!graph)
        return;
    lv_darray_free(&graph->nodes_da);
    lv_darray_free(&graph->connections_da);
    lv_free((void **) &graph);
}

/**
 * @brief 添加节点
 *
 * 向图中添加一个新节点。如果节点数组已满，自动扩容为当前容量的2倍。
 *
 * @param graph 节点图指针
 * @param id    节点ID（<=0时自动分配）
 * @param label 节点标签
 * @param x     节点X坐标
 * @param y     节点Y坐标
 * @param type  节点类型
 * @return 成功返回节点ID，失败返回-1
 */
int lv_node_graph_add_node(lvNodeGraphView *graph, int id, const char *label, double x, double y, int type) {
    lv_CHECK_NOT_NULL(graph);
    lv_CHECK_NOT_NULL(label);

    lvGraphNode node;
    node.id = (id > 0) ? id : graph->next_node_id++;
    node.x = x;
    node.y = y;
    node.type = (lvNodeType) type;
    strncpy(node.label, label, sizeof(node.label) - 1);
    node.label[sizeof(node.label) - 1] = '\0';

    if (lv_darray_push(&graph->nodes_da, &node) < 0)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "failed to push node to darray");

    /* 更新自增ID */
    if (node.id >= graph->next_node_id) {
        graph->next_node_id = node.id + 1;
    }

    return node.id;
}

/**
 * @brief 移除节点及其所有连接
 *
 * 删除指定ID的节点，同时移除所有与该节点相关的连接。
 * 使用最后一个元素填充空位以保持数组紧凑。
 *
 * @param graph 节点图指针
 * @param id    要移除的节点ID
 * @return 成功返回0，失败返回-1
 */
int lv_node_graph_remove_node(lvNodeGraphView *graph, int id) {
    lv_CHECK_NOT_NULL(graph);
    lv_CHECK_ARG(id > 0, lv_ERROR_INVALID_PARAM, "invalid node id %d", id);
    int found = -1;
    lvGraphNode *nodes = (lvGraphNode *)graph->nodes_da.data;
    for (int i = 0; i < graph->nodes_da.count; i++) {
        if (nodes[i].id == id) {
            found = i;
            break;
        }
    }
    if (found < 0)
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "node not found");

    /* 移除与该节点相关的所有连接 */
    lvGraphConnection *conns = (lvGraphConnection *)graph->connections_da.data;
    int new_conn_count = 0;
    for (int i = 0; i < graph->connections_da.count; i++) {
        if (conns[i].from_node_id != id && conns[i].to_node_id != id) {
            if (new_conn_count != i) {
                conns[new_conn_count] = conns[i];
            }
            new_conn_count++;
        }
    }
    graph->connections_da.count = new_conn_count;

    /* 移除节点（用最后一个元素填充空位） */
    nodes[found] = nodes[graph->nodes_da.count - 1];
    graph->nodes_da.count--;
    return 0;
}

/**
 * @brief 添加连接（边）
 *
 * 在两个节点之间添加一条有向连接。如果连接数组已满，自动扩容。
 *
 * @param graph  节点图指针
 * @param from_id 源节点ID
 * @param to_id   目标节点ID
 * @param label   连接标签（可为NULL）
 * @return 成功返回连接ID，失败返回-1
 */
int lv_node_graph_add_connection(lvNodeGraphView *graph, int from_id, int to_id, const char *label) {
    lv_CHECK_NOT_NULL(graph);
    lv_CHECK_ARG(from_id > 0 && to_id > 0, lv_ERROR_INVALID_PARAM,
                 "invalid node id (from=%d, to=%d)", from_id, to_id);

    lvGraphConnection conn;
    conn.id = graph->next_connection_id++;
    conn.from_node_id = from_id;
    conn.to_node_id = to_id;
    if (label) {
        strncpy(conn.label, label, sizeof(conn.label) - 1);
        conn.label[sizeof(conn.label) - 1] = '\0';
    } else {
        conn.label[0] = '\0';
    }

    if (lv_darray_push(&graph->connections_da, &conn) < 0)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "failed to push connection to darray");

    return conn.id;
}

/**
 * @brief 移除连接
 *
 * 删除指定ID的连接，使用最后一个元素填充空位。
 *
 * @param graph   节点图指针
 * @param conn_id 要移除的连接ID
 * @return 成功返回0，失败返回-1
 */
int lv_node_graph_remove_connection(lvNodeGraphView *graph, int conn_id) {
    lv_CHECK_NOT_NULL(graph);
    lv_CHECK_ARG(conn_id > 0, lv_ERROR_INVALID_PARAM, "invalid conn_id %d", conn_id);
    int found = -1;
    lvGraphConnection *conns = (lvGraphConnection *)graph->connections_da.data;
    for (int i = 0; i < graph->connections_da.count; i++) {
        if (conns[i].id == conn_id) {
            found = i;
            break;
        }
    }
    if (found < 0)
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "connection not found");

    conns[found] = conns[graph->connections_da.count - 1];
    graph->connections_da.count--;
    return 0;
}

/**
 * @brief 查找节点
 *
 * 根据节点ID在图中查找节点。
 *
 * @param graph 节点图指针
 * @param id    要查找的节点ID
 * @return 成功返回节点指针，失败返回NULL
 */
lvGraphNode *lv_node_graph_find_node(lvNodeGraphView *graph, int id) {
    if (!graph || id <= 0)
        return NULL;
    lvGraphNode *nodes = (lvGraphNode *)graph->nodes_da.data;
    for (int i = 0; i < graph->nodes_da.count; i++) {
        if (nodes[i].id == id)
            return &nodes[i];
    }
    return NULL;
}

/**
 * @brief Fruchterman-Reingold 力导向布局
 *
 * 对图中的节点应用力导向布局算法，计算排斥力（所有节点对之间）
 * 和吸引力（沿边方向），通过温度衰减控制收敛。
 *
 * @param graph 节点图指针
 * @return 成功返回0，失败返回-1
 */
int lv_node_graph_layout(lvNodeGraphView *graph) {
    if (!graph)
        return -1;
    if (graph->nodes_da.count <= 1)
        return 0;

    const double C = 1.0; /* 常数因子 */
    const int max_iter = graph->layout_engine.iterations;
    const double area = graph->layout_engine.area_width * graph->layout_engine.area_height;
    const int n = graph->nodes_da.count;
    lvGraphNode *nodes = (lvGraphNode *)graph->nodes_da.data;

    /* 理想距离 k = C * sqrt(area / n) */
    const double k = C * sqrt(area / (double) n);

    /* 初始温度 */
    double temperature = graph->layout_engine.area_width / 4.0;

    /* 临时位移数组 */
    double *dx = lv_calloc(n, sizeof(double));
    double *dy = lv_calloc(n, sizeof(double));
    if (!dx || !dy) {
        lv_free((void **) &dx);
        lv_free((void **) &dy);
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "failed to allocate layout displacement arrays");
    }

    /* 迭代 */
    for (int iter = 0; iter < max_iter; iter++) {
        /* 清零位移 */
        memset(dx, 0, n * sizeof(double));
        memset(dy, 0, n * sizeof(double));

        /* 排斥力：所有节点对之间 F_r = k^2 / d */
        for (int i = 0; i < n; i++) {
            for (int j = i + 1; j < n; j++) {
                double diff_x = nodes[i].x - nodes[j].x;
                double diff_y = nodes[i].y - nodes[j].y;
                double dist = sqrt(diff_x * diff_x + diff_y * diff_y);
                if (dist < 0.01)
                    dist = 0.01; /* 避免除零 */

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
        lvGraphConnection *conns = (lvGraphConnection *)graph->connections_da.data;
        for (int c = 0; c < graph->connections_da.count; c++) {
            int fi = -1, ti = -1;
            int from = conns[c].from_node_id;
            int to = conns[c].to_node_id;
            for (int i = 0; i < n; i++) {
                if (nodes[i].id == from)
                    fi = i;
                if (nodes[i].id == to)
                    ti = i;
            }
            if (fi < 0 || ti < 0)
                continue;

            double diff_x = nodes[fi].x - nodes[ti].x;
            double diff_y = nodes[fi].y - nodes[ti].y;
            double dist = sqrt(diff_x * diff_x + diff_y * diff_y);
            if (dist < 0.01)
                dist = 0.01;

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
            if (disp < 0.01)
                continue;

            double scale = fmin(disp, temperature) / disp;
            nodes[i].x += dx[i] * scale;
            nodes[i].y += dy[i] * scale;

            /* 限制在区域内 */
            double margin = 10.0;
            nodes[i].x = fmax(margin, fmin(graph->layout_engine.area_width - margin, nodes[i].x));
            nodes[i].y = fmax(margin, fmin(graph->layout_engine.area_height - margin, nodes[i].y));
        }

        /* 降温 */
        temperature *= 0.95;
    }

    lv_free((void **) &dx);
    lv_free((void **) &dy);
    graph->layout_engine.temperature = temperature;
    return 0;
}
