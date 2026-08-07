/**
 * @file proof_trace_tree.c
 * @brief 证明溯源树操作实现（从 proof_engine_enhanced.c 拆分）
 *
 * @details 溯源节点与溯源树的创建/销毁、子节点管理、状态与信任色计算、
 *          路径查找、DOT 导出与 JSON 序列化。
 *
 * 【lv_graph_traversal / lv_bfs_run 收敛评估结论（不收敛，保留本实现）】
 *   lv_graph_traversal 系列是 ConstraintGraph 专用（GeomNode 节点、约束超边、
 *   int id 空间 0..node_count-1）；lv_bfs_run / lv_cycle_detect 要求整数 id
 *   空间 + 邻居回调。本模块是 lvProofTraceNode 指针树：
 *     1. 节点类型/ID 空间不同：节点是 lvProofTraceNode*（uint32 id 由
 *        g_trace_node_id_counter 递增生成、不连续），无 GeomNode、无
 *        ConstraintGraph，无法套用任何 lv_graph_traversal 入口。
 *     2. find_path 是"路径回溯"语义（DFS 栈 + 路径数组 + visited 集合），
 *        不是"遍历回调"语义；lv_tree_traverse 虽为通用树遍历（void* +
 *        get_children 回调），但仅有访问回调模式、无"查找两节点间路径并
 *        填充输出数组"的 API，为其新增路径查找接口等于新增功能而非收敛。
 *     3. find_path 内的 visited_map（uint32 开放寻址集合）是函数内一次性
 *        临时结构，仅存键不存值，且生命周期极短；lv_hashtable 是键值对
 *        （值为 NULL 视同键不存在），形态与用途均不匹配，强行迁移纯退化。
 *   结论：语义确实不同（指针树路径回溯 vs 整数 id 图遍历回调），无法通过
 *   给 lv_graph_traversal 增加"父指针回传/后向遍历"接口收敛（类型空间根本
 *   不同），保持本实现。
 */

#include "proof_engine_enhanced_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/proof.h"

#include "error_codes.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_dot_writer.h"

/* ============== 溯源树常量 ============== */

#define TRACE_TREE_INITIAL_CAPACITY 64
#define TRACE_NODE_INITIAL_CHILD_CAPACITY 8

static lv_THREAD_LOCAL uint32_t g_trace_node_id_counter = 1;

/**
 * @brief 生成下一个溯源节点 ID
 * @return 新的唯一 ID
 */
static uint32_t next_trace_node_id(void) {
    return g_trace_node_id_counter++;
}

static inline bool _visit_check(uint32_t *visited_map, uint32_t map_size, uint32_t node_id) {
    uint32_t idx = node_id % map_size;
    while (visited_map[idx] != 0) {
        if (visited_map[idx] == node_id)
            return true;
        idx = (idx + 1) % map_size;
    }
    return false;
}

int64_t get_time_ns(void) {
    return (int64_t) lv_get_time_ns();
}


/* ============== 溯源树节点操作 ============== */

/**
 * @brief 创建溯源节点
 *
 * 分配并初始化一个溯源树节点，设置节点类型和标签。
 * 节点 ID 自动递增生成，确保全局唯一。
 * 初始状态为 TRACE_STATUS_UNEXPLORED，信任颜色为绿色。
 *
 * @param type 节点类型（公理/定义/定理/引理/假设/推导/矛盾/目标）
 * @param label 节点标签（用于显示和导出）
 * @return 新分配的溯源节点指针，失败返回 NULL
 */
lvProofTraceNode *lv_trace_node_create(lvTraceNodeType type, const char *label) {
    lvProofTraceNode *node = (lvProofTraceNode *) lv_calloc(1, sizeof(lvProofTraceNode));
    if (!node)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "lv_trace_node_create: calloc failed");

    node->id = next_trace_node_id();
    node->type = type;
    node->status = TRACE_STATUS_UNEXPLORED;
    node->trust_color = TRUST_GREEN;
    node->depth = 0;
    node->create_time_ns = get_time_ns();
    node->complete_time_ns = 0;
    node->elapsed_ms = 0.0;

    if (label) {
        safe_strncpy(node->label, label, sizeof(node->label));
    }

    /* 初始化子节点数组 */
    lv_darray_init(&node->children, sizeof(lvProofTraceNode *));
    if (!lv_darray_reserve(&node->children, TRACE_NODE_INITIAL_CHILD_CAPACITY)) {
        lv_free((void **) &node);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "lv_trace_node_create: failed to reserve children array");
    }

    node->parent = NULL;
    node->proposition = NULL;
    node->step = NULL;
    node->rule = NULL;
    node->dependency_ids = NULL;
    node->dependency_count = 0;

    return node;
}

/**
 * @brief 递归销毁溯源节点及其所有子节点
 *
 * 采用后序遍历方式释放节点树：先递归销毁所有子节点，
 * 再释放当前节点的资源。防止内存泄漏。
 *
 * @param node 节点指针（可为 NULL，此时直接返回）
 */
void lv_trace_node_destroy(lvProofTraceNode *node) {
    if (!node)
        return;

    /* 递归销毁所有子节点 */
    for (int i = 0; i < node->children.count; i++) {
        lvProofTraceNode **child = (lvProofTraceNode **)lv_darray_get(&node->children, i);
        lv_trace_node_destroy(*child);
    }

    /* 释放子节点数组 */
    lv_darray_free(&node->children);

    /* 释放依赖 ID 数组 */
    if (node->dependency_ids) {
        lv_free((void **) &node->dependency_ids);
    }

    /* 注意：proposition、step、rule 不在此释放，它们由各自的管理器负责 */

    lv_free((void **) &node);
}

/**
 * @brief 添加子节点到溯源节点
 *
 * 将 child 添加为 parent 的子节点，并设置 child 的父指针。
 * 自动维护子节点数组的容量。如果子节点数组已满，
 * 将按 2 倍策略扩容。
 *
 * @param parent 父节点（不可为 NULL）
 * @param child  子节点（不可为 NULL）
 * @return true 添加成功，false 参数无效或扩容失败
 */
bool lv_trace_node_add_child(lvProofTraceNode *parent, lvProofTraceNode *child) {
    if (!parent || !child) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "lv_trace_node_add_child: parent or child is NULL");
    }

    /* lv_darray_push 自动扩容 */
    if (lv_darray_push(&parent->children, &child) < 0) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_ALLOCATION_FAILED, "lv_trace_node_add_child: failed to push child");
    }
    child->parent = parent;
    child->depth = parent->depth + 1;

    return true;
}

/**
 * @brief 设置溯源节点的状态
 *
 * 更新节点的探索状态，并在状态变为终态（已证明/已证伪/阻塞）时
 * 记录完成时间戳和计算耗时。
 *
 * @param node   节点指针（不可为 NULL）
 * @param status 新状态
 */
void lv_trace_node_set_status(lvProofTraceNode *node, lvTraceNodeStatus status) {
    if (!node)
        return;

    node->status = status;

    /* 终态时记录完成时间 */
    if (status == TRACE_STATUS_PROVED || status == TRACE_STATUS_DISPROVED || status == TRACE_STATUS_BLOCKED) {
        node->complete_time_ns = get_time_ns();
        if (node->create_time_ns > 0) {
            node->elapsed_ms = (double) (node->complete_time_ns - node->create_time_ns) / 1e6;
        }
    }
}

/* 溯源节点类型 → 初始信任颜色 映射表（数据表化，替代 lv_trace_node_compute_color 中的 switch）。
 * TRACE_NODE_DERIVATION / TRACE_NODE_GOAL 需递归取子节点最小颜色，用哨兵值标记。 */
#define TRACE_NODE_COLOR_AUTO (-1)    /* 需递归计算（推导/目标节点） */
#define TRACE_NODE_COLOR_UNKNOWN (-2) /* 未知类型：保持节点原有颜色 */

static const int s_trace_node_initial_colors[] = {
    [TRACE_NODE_AXIOM]         = TRUST_GREEN,
    [TRACE_NODE_DEFINITION]    = TRUST_GREEN,
    [TRACE_NODE_THEOREM]       = TRUST_GREEN,
    [TRACE_NODE_LEMMA]         = TRUST_GREEN,
    [TRACE_NODE_HYPOTHESIS]    = TRUST_BLUE_UNEXPLORED,
    [TRACE_NODE_DERIVATION]    = TRACE_NODE_COLOR_AUTO,
    [TRACE_NODE_CONTRADICTION] = TRUST_GREEN, /* 矛盾节点在反证法中视为有效推导 */
    [TRACE_NODE_GOAL]          = TRACE_NODE_COLOR_AUTO,
};

/**
 * @brief 计算溯源节点的信任颜色
 *
 * 根据节点类型和子节点的信任颜色，递归计算当前节点的信任颜色。
 * 规则如下：
 *   - 公理节点：绿色（最高信任）
 *   - 定义节点：绿色
 *   - 定理节点：绿色
 *   - 引理节点：绿色
 *   - 假设节点：蓝色（未探索）
 *   - 推导节点：取所有子节点中信任级别最低的颜色
 *   - 矛盾节点：根据矛盾类型确定
 *   - 目标节点：取所有子节点中信任级别最低的颜色
 *
 * @param node 溯源节点
 * @return 计算后的信任颜色
 */
TrustColor lv_trace_node_compute_color(lvProofTraceNode *node) {
    if (!node)
        return TRUST_GREEN;

    /* 通过查找表获取初始颜色，越界类型保持节点原有颜色 */
    int initial = TRACE_NODE_COLOR_UNKNOWN;
    if ((unsigned) node->type < lv_ARRAY_SIZE(s_trace_node_initial_colors))
        initial = s_trace_node_initial_colors[node->type];

    if (initial == TRACE_NODE_COLOR_AUTO) {
        /* 推导/目标节点：取子节点中最低信任颜色 */
        TrustColor min_color = TRUST_GREEN;
        for (int i = 0; i < node->children.count; i++) {
            lvProofTraceNode **child = (lvProofTraceNode **)lv_darray_get(&node->children, i);
            TrustColor child_color = lv_trace_node_compute_color(*child);
            if (child_color < min_color) {
                min_color = child_color;
            }
        }
        node->trust_color = min_color;
    } else if (initial != TRACE_NODE_COLOR_UNKNOWN) {
        node->trust_color = (TrustColor) initial;
    }

    return node->trust_color;
}

/* ============== 溯源树操作 ============== */

/**
 * @brief 创建溯源树
 *
 * 创建以 root_prop 为根命题的溯源树。根节点类型为 TRACE_NODE_GOAL，
 * 标签使用命题名称（如无名称则使用 "Goal"）。
 *
 * @param root_prop 根命题（可为 NULL，此时创建空目标节点）
 * @return 新溯源树指针，失败返回 NULL
 */
lvProofTraceTree *lv_trace_tree_create(Proposition *root_prop) {
    lvProofTraceTree *tree = (lvProofTraceTree *) lv_calloc(1, sizeof(lvProofTraceTree));
    if (!tree)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "lv_trace_tree_create: calloc failed");

    /* 初始化节点数组 */
    lv_darray_init(&tree->all_nodes, sizeof(lvProofTraceNode *));
    if (!lv_darray_reserve(&tree->all_nodes, TRACE_TREE_INITIAL_CAPACITY)) {
        lv_free((void **) &tree);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "lv_trace_tree_create: failed to reserve all_nodes");
    }

    /* 创建根节点 */
    const char *root_label = "Goal";
    if (root_prop && root_prop->name) {
        root_label = root_prop->name;
    }

    tree->root = lv_trace_node_create(TRACE_NODE_GOAL, root_label);
    if (!tree->root) {
        lv_free((void **) &tree->all_nodes);
        lv_free((void **) &tree);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "lv_trace_tree_create: failed to create root node");
    }

    tree->root->proposition = root_prop;

    /* 将根节点加入节点数组 */
    lv_darray_push(&tree->all_nodes, &tree->root);

    /* 初始化统计信息 */
    tree->proved_count = 0;
    tree->disproved_count = 0;
    tree->max_depth = 0;
    tree->is_complete = false;
    tree->final_color = TRUST_BLUE_UNEXPLORED;

    return tree;
}

/**
 * @brief 销毁溯源树并释放所有节点
 *
 * 递归销毁根节点及其所有子节点，然后释放节点数组和树结构本身。
 *
 * @param tree 溯源树指针（可为 NULL，此时直接返回）
 */
void lv_trace_tree_destroy(lvProofTraceTree *tree) {
    if (!tree)
        return;

    /* 递归销毁根节点（会递归销毁所有子节点） */
    if (tree->root) {
        lv_trace_node_destroy(tree->root);
    }

    /* 释放节点数组 */
    lv_darray_free(&tree->all_nodes);

    lv_free((void **) &tree);
}

/**
 * @brief 内部函数：将节点注册到溯源树
 *
 * @param tree 溯源树
 * @param node 要注册的节点
 * @return 是否成功
 */
bool trace_tree_register_node(lvProofTraceTree *tree, lvProofTraceNode *node) {
    if (!tree || !node)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "trace_tree_register_node: tree or node is NULL");

    /* lv_darray_push 自动扩容 */
    if (lv_darray_push(&tree->all_nodes, &node) < 0)
        lv_RETURN_ERROR_BOOL(lv_ERROR_ALLOCATION_FAILED, "trace_tree_register_node: failed to push node");

    /* 更新最大深度 */
    if ((uint32_t) node->depth > tree->max_depth) {
        tree->max_depth = (uint32_t) node->depth;
    }

    return true;
}

/**
 * @brief 内部函数：更新溯源树统计信息
 *
 * 遍历所有节点，统计已证明和已证伪的数量。
 *
 * @param tree 溯源树
 */
void trace_tree_update_stats(lvProofTraceTree *tree) {
    if (!tree)
        return;

    tree->proved_count = 0;
    tree->disproved_count = 0;
    tree->max_depth = 0;

    for (int i = 0; i < tree->all_nodes.count; i++) {
        lvProofTraceNode **node_p = (lvProofTraceNode **)lv_darray_get(&tree->all_nodes, i);
        lvProofTraceNode *node = *node_p;
        if (node->status == TRACE_STATUS_PROVED) {
            tree->proved_count++;
        } else if (node->status == TRACE_STATUS_DISPROVED) {
            tree->disproved_count++;
        }
        if ((uint32_t) node->depth > tree->max_depth) {
            tree->max_depth = (uint32_t) node->depth;
        }
    }
}

/**
 * @brief 在溯源树中查找两个节点之间的路径
 *
 * 使用深度优先搜索从 from_id 节点出发，查找到达 to_id 节点的路径。
 * 路径通过 out_path 数组返回，包含从起点到终点的所有节点。
 *
 * @param tree        溯源树
 * @param from_id     起始节点 ID
 * @param to_id       目标节点 ID
 * @param out_path    输出路径节点数组
 * @param max_length  最大路径长度
 * @return 实际路径长度（0 表示未找到路径）
 */
uint32_t lv_trace_tree_find_path(const lvProofTraceTree *tree, uint32_t from_id, uint32_t to_id,
                                 lvProofTraceNode **out_path, uint32_t max_length) {
    if (!tree || !out_path || max_length == 0)
        lv_RETURN_ERROR_VAL(lv_ERROR_NULL_POINTER, 0, "lv_trace_tree_find_path: NULL params or empty max_length");

    /* 查找起始节点 */
    lvProofTraceNode *start = NULL;
    for (int i = 0; i < tree->all_nodes.count; i++) {
        lvProofTraceNode **node_p = (lvProofTraceNode **)lv_darray_get(&tree->all_nodes, i);
        if ((*node_p)->id == from_id) {
            start = *node_p;
            break;
        }
    }
    if (!start)
        return 0;

    /* 检查目标是否就是起点 */
    if (from_id == to_id) {
        out_path[0] = start;
        return 1;
    }

    /* DFS 搜索路径 */
    /* 使用栈模拟递归，避免栈溢出 */
    typedef struct {
        lvProofTraceNode *node;
        uint32_t depth;
    } SearchFrame;

    int ncount = tree->all_nodes.count;
    SearchFrame *stack = (SearchFrame *) lv_calloc((size_t)ncount, sizeof(SearchFrame));
    if (!stack)
        lv_RETURN_ERROR_VAL(lv_ERROR_ALLOCATION_FAILED, 0, "lv_trace_tree_find_path: stack calloc failed");

    /* 访问标记：使用 ID 到索引的映射避免哈希碰撞 */
    /* visited_map[id % map_size] 存储已访问节点的 id，0 表示空槽 */
    uint32_t map_size = (uint32_t)(ncount * 2); /* 负载因子 <= 0.5 减少碰撞 */
    if (map_size < 8)
        map_size = 8;
    uint32_t *visited_map = (uint32_t *) lv_calloc(map_size, sizeof(uint32_t));
    if (!visited_map) {
        lv_free((void **) &stack);
        lv_RETURN_ERROR_VAL(lv_ERROR_ALLOCATION_FAILED, 0, "lv_trace_tree_find_path: visited_map calloc failed");
    }

    /* 记录路径 */
    lvProofTraceNode **path = (lvProofTraceNode **) lv_malloc((size_t)ncount * sizeof(lvProofTraceNode *));
    if (!path) {
        lv_free((void **) &visited_map);
        lv_free((void **) &stack);
        lv_RETURN_ERROR_VAL(lv_ERROR_ALLOCATION_FAILED, 0, "lv_trace_tree_find_path: path malloc failed");
    }

/* 辅助函数：检查/标记节点是否已访问（线性探测） */
#define VISIT_MARK(node_id)                             \
    do {                                                \
        uint32_t idx = (uint32_t) (node_id) % map_size; \
        while (visited_map[idx] != 0) {                 \
            idx = (idx + 1) % map_size;                 \
        }                                               \
        visited_map[idx] = (uint32_t) (node_id);        \
    } while (0)

    /* 内联函数：检查节点是否已访问（避免 GNU statement-expression 扩展） */
    uint32_t *__vm = visited_map;
    uint32_t __ms = map_size;

#define VISIT_CHECK(node_id) _visit_check(__vm, __ms, (uint32_t) (node_id))

    uint32_t top = 0;
    stack[top].node = start;
    stack[top].depth = 0;
    path[0] = start;
    VISIT_MARK(start->id);
    uint32_t path_len = 1;

    uint32_t result = 0;

    while (top < (uint32_t)ncount) {
        lvProofTraceNode *current = stack[top].node;
        bool found_child = false;

        for (int i = 0; i < current->children.count; i++) {
            lvProofTraceNode **child_p = (lvProofTraceNode **)lv_darray_get(&current->children, i);
            lvProofTraceNode *child = *child_p;

            if (!VISIT_CHECK(child->id)) {
                VISIT_MARK(child->id);
                path[path_len++] = child;

                if (child->id == to_id) {
                    /* 找到目标，复制路径到输出 */
                    uint32_t copy_len = path_len < max_length ? path_len : max_length;
                    memcpy(out_path, path, copy_len * sizeof(lvProofTraceNode *));
                    result = copy_len;
                    goto cleanup;
                }

                top++;
                stack[top].node = child;
                stack[top].depth = stack[top - 1].depth + 1;
                found_child = true;
                break;
            }
        }

        if (!found_child) {
            path_len--;
            top--;
        }
    }

cleanup:
#undef VISIT_MARK
#undef VISIT_CHECK
    lv_free((void **) &path);
    lv_free((void **) &visited_map);
    lv_free((void **) &stack);
    return result;
}

/**
 * @brief 导出溯源树为 DOT 格式（Graphviz）
 *
 * 生成 Graphviz DOT 语言格式的溯源树可视化描述。
 * 不同节点类型使用不同颜色和形状：
 *   - 公理：绿色椭圆
 *   - 定理：绿色方框
 *   - 假设：蓝色菱形
 *   - 推导：灰色圆角矩形
 *   - 矛盾：红色八角形
 *   - 目标：金色双圆
 *
 * @param tree 溯源树
 * @param path 输出文件路径
 * @return true 导出成功，false 参数无效或写入失败
 */
bool lv_trace_tree_export_dot(const lvProofTraceTree *tree, const char *path) {
    if (!tree || !path) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "lv_trace_tree_export_dot: tree or path is NULL");
    }

    lvStrBuf sb;
    lv_strbuf_init(&sb);

    lv_dot_begin(&sb, "ProofTraceTree", "TB",
                 "fontname=\"Helvetica\", fontsize=10",
                 "fontname=\"Helvetica\", fontsize=8");

    /* 节点颜色映射 */
    static const char *node_colors[] = {
        "lightgreen", /* TRACE_NODE_AXIOM */
        "palegreen",  /* TRACE_NODE_DEFINITION */
        "green",      /* TRACE_NODE_THEOREM */
        "limegreen",  /* TRACE_NODE_LEMMA */
        "lightblue",  /* TRACE_NODE_HYPOTHESIS */
        "lightgray",  /* TRACE_NODE_DERIVATION */
        "salmon",     /* TRACE_NODE_CONTRADICTION */
        "gold"        /* TRACE_NODE_GOAL */
    };

    static const char *node_shapes[] = {
        "ellipse",     /* TRACE_NODE_AXIOM */
        "box",         /* TRACE_NODE_DEFINITION */
        "box",         /* TRACE_NODE_THEOREM */
        "box",         /* TRACE_NODE_LEMMA */
        "diamond",     /* TRACE_NODE_HYPOTHESIS */
        "rounded",     /* TRACE_NODE_DERIVATION */
        "octagon",     /* TRACE_NODE_CONTRADICTION */
        "doublecircle" /* TRACE_NODE_GOAL */
    };

    /* 输出所有节点（label 经 lv_dot_node 内部 JSON/DOT 转义，行为与原实现一致） */
    for (int i = 0; i < tree->all_nodes.count; i++) {
        lvProofTraceNode **node_p = (lvProofTraceNode **)lv_darray_get(&tree->all_nodes, i);
        lvProofTraceNode *node = *node_p;
        int type_idx = (int) node->type;
        if (type_idx > 7)
            type_idx = 0;

        const char *label = node->label[0] ? node->label : "";
        lvStrBuf lbl;
        lv_strbuf_init(&lbl);
        lv_strbuf_printf(&lbl, "%s\n[%s]", label,
                node->status == TRACE_STATUS_PROVED      ? "proved"
                : node->status == TRACE_STATUS_DISPROVED ? "disproved"
                : node->status == TRACE_STATUS_EXPLORING ? "exploring"
                : node->status == TRACE_STATUS_BLOCKED   ? "blocked"
                                                         : "unexplored");

        char idbuf[32], extra[128];
        snprintf(idbuf, sizeof(idbuf), "n%d", node->id);
        snprintf(extra, sizeof(extra), "shape=%s, style=filled, fillcolor=%s",
                 node_shapes[type_idx], node_colors[type_idx]);
        lv_dot_node(&sb, idbuf, lv_strbuf_cstr(&lbl), extra);
        lv_strbuf_destroy(&lbl);
    }

    lv_strbuf_printf(&sb, "\n");

    /* 输出所有边 */
    for (int i = 0; i < tree->all_nodes.count; i++) {
        lvProofTraceNode **node_p = (lvProofTraceNode **)lv_darray_get(&tree->all_nodes, i);
        lvProofTraceNode *node = *node_p;
        for (int j = 0; j < node->children.count; j++) {
            lvProofTraceNode **child_p = (lvProofTraceNode **)lv_darray_get(&node->children, j);
            lvProofTraceNode *child = *child_p;
            char frombuf[32], tobuf[32];
            snprintf(frombuf, sizeof(frombuf), "n%d", node->id);
            snprintf(tobuf, sizeof(tobuf), "n%d", child->id);
            lv_dot_edge(&sb, frombuf, tobuf, NULL, NULL);
        }
    }

    lv_dot_end(&sb);

    if (!lv_dot_write_file(path, sb.data, sb.len)) {
        lv_strbuf_destroy(&sb);
        lv_RETURN_ERROR_BOOL(lv_ERROR_IO, "lv_trace_tree_export_dot: cannot open file: %s", path);
    }
    lv_strbuf_destroy(&sb);

    return true;
}

/**
 * @brief 导出溯源树为 JSON 格式
 *
 * 生成溯源树的 JSON 表示，包含所有节点信息和树结构。
 * JSON 格式遵循以下结构：
 * ```json
 * {
 *   "is_complete": true,
 *   "node_count": 10,
 *   "proved_count": 8,
 *   "max_depth": 5,
 *   "nodes": [...]
 * }
 * ```
 *
 * @param tree 溯源树
 * @return JSON 字符串（调用者需用 lv_free 释放），失败返回 NULL
 */
char *lv_trace_tree_to_json(const lvProofTraceTree *tree) {
    if (!tree) {
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_trace_tree_to_json: tree is NULL");
    }

    lvStrBuf buf = {0};

    lv_strbuf_printf(&buf, "{\n");
    lv_strbuf_printf(&buf, "  \"is_complete\": %s,\n", tree->is_complete ? "true" : "false");
    lv_strbuf_printf(&buf, "  \"node_count\": %d,\n", tree->all_nodes.count);
    lv_strbuf_printf(&buf, "  \"proved_count\": %u,\n", tree->proved_count);
    lv_strbuf_printf(&buf, "  \"disproved_count\": %u,\n", tree->disproved_count);
    lv_strbuf_printf(&buf, "  \"max_depth\": %u,\n", tree->max_depth);
    lv_strbuf_printf(&buf, "  \"nodes\": [\n");

    for (int i = 0; i < tree->all_nodes.count; i++) {
        lvProofTraceNode **node_p = (lvProofTraceNode **)lv_darray_get(&tree->all_nodes, i);
        lvProofTraceNode *node = *node_p;

        lv_strbuf_printf(&buf, "    {\n");
        lv_strbuf_printf(&buf, "      \"id\": %u,\n", node->id);
        lv_strbuf_printf(&buf, "      \"type\": %d,\n", (int) node->type);
        lv_strbuf_printf(&buf, "      \"status\": %d,\n", (int) node->status);
        lv_strbuf_printf(&buf, "      \"label\": \"");
        {
            const char *s = node->label[0] ? node->label : "";
            size_t slen = strlen(s);
            char *esc = lv_str_json_escape_alloc(s, slen, NULL);
            lv_strbuf_printf(&buf, "%s", esc ? esc : "");
            lv_free((void **)&esc);
        }
        lv_strbuf_printf(&buf, "\",\n");
        lv_strbuf_printf(&buf, "      \"description\": \"");
        {
            const char *s = node->description[0] ? node->description : "";
            size_t slen = strlen(s);
            char *esc = lv_str_json_escape_alloc(s, slen, NULL);
            lv_strbuf_printf(&buf, "%s", esc ? esc : "");
            lv_free((void **)&esc);
        }
        lv_strbuf_printf(&buf, "\",\n");
        lv_strbuf_printf(&buf, "      \"depth\": %d,\n", node->depth);
        lv_strbuf_printf(&buf, "      \"child_count\": %d,\n", node->children.count);
        lv_strbuf_printf(&buf, "      \"elapsed_ms\": %.3f\n", node->elapsed_ms);
        lv_strbuf_printf(&buf, "    }%s\n", (i + 1 < tree->all_nodes.count) ? "," : "");
    }

    lv_strbuf_printf(&buf, "  ]\n");
    lv_strbuf_printf(&buf, "}\n");

    return lv_strbuf_to_string(&buf);
}
