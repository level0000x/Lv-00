/**
 * @file blueprint_graph_api.c
 * @brief 蓝图图 API 实现（TEN_LAYER_OPTIMIZED_PLAN §12.4 / §15.2.2 / §15.4 落地）
 *
 * 提供 lv_graph_* 系列高层约束图操作，接线 constraint_graph 与
 * lv_graph_traversal 现有设施。规划文档类型名 lvConstraintGraph 对应
 * 库内 ConstraintGraph。
 */

#include "lv/constraint_graph.h"

#include <string.h>

#include "lv/lv_graph_traversal.h"
#include "lv/lv_utils.h"
#include "lv/symbolic_coord.h"

/* ============================================================
 * lv_graph_add_point：double 坐标 → 有理点节点
 * ============================================================ */

int lv_graph_add_point(ConstraintGraph *graph, double x, double y) {
    if (graph == NULL)
        return -1;

    /* double → 有理数（整数化：x/y 的有限精度表示取整）。库内 graph_add_point_xy
     * 需要 SymbolicCoord*；从 double 构造有理坐标（scale=1 四舍五入） */
    SymbolicCoord *cx = symbolic_coord_from_double_rounded(x, 1);
    SymbolicCoord *cy = symbolic_coord_from_double_rounded(y, 1);
    if (cx == NULL || cy == NULL) {
        symbolic_coord_destroy(cx);
        symbolic_coord_destroy(cy);
        return -1;
    }

    AddNodeResult result = graph_add_point_xy(graph, cx, cy);
    /* graph_add_point_xy 深拷贝坐标，调用方释放临时坐标 */
    symbolic_coord_destroy(cx);
    symbolic_coord_destroy(cy);
    if (result != ADD_NODE_OK)
        return -1;
    return graph_get_last_added_node_id(graph);
}

/* ============================================================
 * lv_graph_get_nodes_by_type：按几何类型查询节点 ID
 * ============================================================ */

bool lv_graph_get_nodes_by_type(const ConstraintGraph *graph, int type, int **out_ids, int *out_count) {
    if (graph == NULL || out_ids == NULL || out_count == NULL) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_PARAM, "lv_graph_get_nodes_by_type: NULL param");
    }
    *out_ids = NULL;
    *out_count = 0;

    int count = graph_get_node_count(graph);
    if (count <= 0)
        return true; /* 空图：空结果，成功 */

    int *ids = (int *) lv_malloc((size_t) count * sizeof(int));
    if (ids == NULL)
        lv_RETURN_ERROR_BOOL(lv_ERROR_OUT_OF_MEMORY, "lv_graph_get_nodes_by_type: alloc failed");

    int n = 0;
    for (int i = 0; i < count; i++) {
        GeomNode *node = graph_get_node(graph, i);
        if (node != NULL && (int) node->type == type)
            ids[n++] = i;
    }
    if (n < count) {
        /* 紧凑（缩容到实际数量） */
        int *compact = (int *) lv_realloc(ids, (size_t) n * sizeof(int));
        if (compact != NULL)
            ids = compact;
    }
    *out_ids = ids;
    *out_count = n;
    return true;
}

/* ============================================================
 * lv_graph_get_dependents：依赖指定节点的下游节点（-1 结尾）
 * ============================================================ */

int *lv_graph_get_dependents(const ConstraintGraph *graph, int node_id) {
    if (graph == NULL || node_id < 0)
        return NULL;

    /* 上界：节点总数 + 1（哨兵） */
    int max_nodes = graph_get_node_count(graph);
    int *dependents = (int *) lv_calloc((size_t) max_nodes + 1, sizeof(int));
    if (dependents == NULL)
        return NULL;

    /* 收集参与约束的另一侧节点（去重：hashtable 或线性扫描——规模小用线性） */
    int indices[64];
    int count = graph_find_constraints_involving(graph, node_id, indices, 64);
    if (count > 64) {
        /* 超上限：扩大缓冲重查（保守：单次 4096） */
        int *big = (int *) lv_malloc((size_t) count * sizeof(int));
        if (big == NULL) {
            lv_free((void **) &dependents);
            return NULL;
        }
        count = graph_find_constraints_involving(graph, node_id, big, count);
        /* 用大缓冲继续 */
        int n = 0;
        for (int i = 0; i < count; i++) {
            Constraint *con = graph_get_constraint(graph, big[i]);
            if (con == NULL)
                continue;
            for (int p = 0; p < con->participant_count; p++) {
                int other = con->participants[p];
                if (other == node_id || other < 0)
                    continue;
                bool seen = false;
                for (int k = 0; k < n; k++) {
                    if (dependents[k] == other) {
                        seen = true;
                        break;
                    }
                }
                if (!seen)
                    dependents[n++] = other;
            }
        }
        dependents[n] = -1;
        lv_free((void **) &big);
        return dependents;
    }

    int n = 0;
    for (int i = 0; i < count; i++) {
        Constraint *con = graph_get_constraint(graph, indices[i]);
        if (con == NULL)
            continue;
        for (int p = 0; p < con->participant_count; p++) {
            int other = con->participants[p];
            if (other == node_id || other < 0)
                continue;
            bool seen = false;
            for (int k = 0; k < n; k++) {
                if (dependents[k] == other) {
                    seen = true;
                    break;
                }
            }
            if (!seen)
                dependents[n++] = other;
        }
    }
    dependents[n] = -1;
    return dependents;
}

/* ============================================================
 * lv_graph_register_change_callback / lv_graph_on_node_changed
 *
 * 回调存储于图内：复用 dirty 旁的扩展字段不可行（结构体冻结），
 * 采用独立静态注册表（graph 指针 → 回调 + user_data），进程级。
 * ============================================================ */

typedef struct {
    ConstraintGraph *graph;
    lvNodeChangeCallback callback;
    void *user_data;
} ChangeCallbackEntry;

#define LV_MAX_CHANGE_CALLBACKS 32
static ChangeCallbackEntry g_change_callbacks[LV_MAX_CHANGE_CALLBACKS];
static int g_change_callback_count = 0;

bool lv_graph_register_change_callback(ConstraintGraph *graph, lvNodeChangeCallback callback, void *user_data) {
    if (graph == NULL)
        return false;

    /* 覆盖同图已有回调 */
    for (int i = 0; i < g_change_callback_count; i++) {
        if (g_change_callbacks[i].graph == graph) {
            g_change_callbacks[i].callback = callback;
            g_change_callbacks[i].user_data = user_data;
            return true;
        }
    }
    if (g_change_callback_count >= LV_MAX_CHANGE_CALLBACKS)
        return false; /* 满（图实例数超上限，罕见） */
    g_change_callbacks[g_change_callback_count].graph = graph;
    g_change_callbacks[g_change_callback_count].callback = callback;
    g_change_callbacks[g_change_callback_count].user_data = user_data;
    g_change_callback_count++;
    return true;
}

void lv_graph_on_node_changed(ConstraintGraph *graph, int node_id, int change_type) {
    if (graph == NULL)
        return;
    int graph_id = (int) ((uintptr_t) (const void *) graph & 0x7FFFFFFFu);
    for (int i = 0; i < g_change_callback_count; i++) {
        if (g_change_callbacks[i].graph == graph && g_change_callbacks[i].callback != NULL) {
            g_change_callbacks[i].callback(graph_id, node_id, change_type, g_change_callbacks[i].user_data);
        }
    }
}

/* ============================================================
 * lv_graph_decompose：连通分量分解（约束参与者邻接）
 * ============================================================ */

/** @brief 并查集 find（迭代 + 路径压缩） */
static int find_root(int *parent, int x) {
    int r = x;
    while (parent[r] != r)
        r = parent[r];
    while (parent[x] != x) {
        int next = parent[x];
        parent[x] = r;
        x = next;
    }
    return r;
}

/** @brief 并查集 union（按秩合并） */
static void union_sets(int *parent, int *rank, int a, int b) {
    int ra = find_root(parent, a);
    int rb = find_root(parent, b);
    if (ra == rb)
        return;
    if (rank[ra] < rank[rb]) {
        parent[ra] = rb;
    } else if (rank[ra] > rank[rb]) {
        parent[rb] = ra;
    } else {
        parent[rb] = ra;
        rank[ra]++;
    }
}

int lv_graph_decompose(const ConstraintGraph *graph, lvSubgraphTask **out_tasks, int *out_task_count) {
    if (graph == NULL || out_tasks == NULL || out_task_count == NULL)
        return -1;
    *out_tasks = NULL;
    *out_task_count = 0;

    int node_count = graph_get_node_count(graph);
    if (node_count <= 0)
        return 0; /* 空图：0 个子图 */

    /* 邻接表：node -> 参与同一约束的其他节点（简化：两两相连） */
    /* 并查集（路径压缩 + 按秩合并） */
    int *parent = (int *) lv_malloc((size_t) node_count * sizeof(int));
    int *rank = (int *) lv_calloc((size_t) node_count, sizeof(int));
    if (parent == NULL || rank == NULL) {
        lv_free((void **) &parent);
        lv_free((void **) &rank);
        return -1;
    }
    for (int i = 0; i < node_count; i++)
        parent[i] = i;

    /* 约束参与者两两相连 */
    int constraint_count = graph_get_constraint_count(graph);
    for (int ci = 0; ci < constraint_count; ci++) {
        Constraint *con = graph_get_constraint(graph, ci);
        if (con == NULL || !con->is_active)
            continue;
        for (int a = 0; a < con->participant_count; a++) {
            int pa = con->participants[a];
            if (pa < 0 || pa >= node_count)
                continue;
            for (int b = a + 1; b < con->participant_count; b++) {
                int pb = con->participants[b];
                if (pb < 0 || pb >= node_count)
                    continue;
                union_sets(parent, rank, pa, pb);
            }
        }
    }

    /* 几何内部关联：线段端点 ↔ 同名坐标点、圆 ↔ 圆心/半径端点。
     * 线段 symbolic_coords[0..3]=(A.x,A.y,B.x,B.y)，点 symbolic_coords[0..1]=(P.x,P.y)；
     * 坐标精确相等（symbolic_coord_equal）即视为相连。 */
    for (int i = 0; i < node_count; i++) {
        GeomNode *node = graph_get_node(graph, i);
        if (node == NULL)
            continue;
        if (node->type == GEOM_LINE_SEGMENT && node->coord_count >= 4 && node->symbolic_coords != NULL) {
            for (int j = 0; j < node_count; j++) {
                if (i == j)
                    continue;
                GeomNode *other = graph_get_node(graph, j);
                if (other == NULL || other->type != GEOM_POINT || other->coord_count < 2 ||
                    other->symbolic_coords == NULL)
                    continue;
                for (int e = 0; e < 2; e++) { /* 端点 A(0,1) / B(2,3) */
                    int cx = e * 2, cy = e * 2 + 1;
                    if (symbolic_coord_equal(node->symbolic_coords[cx], other->symbolic_coords[0]) &&
                        symbolic_coord_equal(node->symbolic_coords[cy], other->symbolic_coords[1])) {
                        union_sets(parent, rank, i, j);
                    }
                }
            }
        } else if (node->type == GEOM_CIRCLE && node->data.circle.center_node_id >= 0) {
            int cid = node->data.circle.center_node_id;
            if (cid < node_count)
                union_sets(parent, rank, i, cid);
            int rid = node->data.circle.radius_node_id;
            if (rid >= 0 && rid < node_count)
                union_sets(parent, rank, i, rid);
        }
    }

    /* 根 → 成员列表 */
    int *root_ids = (int *) lv_malloc((size_t) node_count * sizeof(int));
    if (root_ids == NULL) {
        lv_free((void **) &parent);
        lv_free((void **) &rank);
        return -1;
    }
    int root_count = 0;
    int *root_of = (int *) lv_malloc((size_t) node_count * sizeof(int));
    if (root_of == NULL) {
        lv_free((void **) &parent);
        lv_free((void **) &rank);
        lv_free((void **) &root_ids);
        return -1;
    }
    for (int i = 0; i < node_count; i++) {
        int r = find_root(parent, i);
        int found = -1;
        for (int k = 0; k < root_count; k++) {
            if (root_ids[k] == r) {
                found = k;
                break;
            }
        }
        if (found < 0) {
            root_ids[root_count] = r;
            found = root_count;
            root_count++;
        }
        root_of[i] = found;
    }

    /* 组装任务数组 */
    lvSubgraphTask *tasks = (lvSubgraphTask *) lv_calloc((size_t) root_count, sizeof(lvSubgraphTask));
    if (tasks == NULL) {
        lv_free((void **) &parent);
        lv_free((void **) &rank);
        lv_free((void **) &root_ids);
        lv_free((void **) &root_of);
        return -1;
    }
    int *counts = (int *) lv_calloc((size_t) root_count, sizeof(int));
    if (counts == NULL) {
        lv_free((void **) &parent);
        lv_free((void **) &rank);
        lv_free((void **) &root_ids);
        lv_free((void **) &root_of);
        lv_free((void **) &tasks);
        return -1;
    }
    for (int i = 0; i < node_count; i++)
        counts[root_of[i]]++;

    bool alloc_ok = true;
    for (int k = 0; k < root_count; k++) {
        tasks[k].subgraph_id = k;
        tasks[k].node_count = counts[k];
        tasks[k].partial_result = NULL;
        tasks[k].node_ids = (int *) lv_malloc((size_t) counts[k] * sizeof(int));
        if (tasks[k].node_ids == NULL) {
            alloc_ok = false;
            break;
        }
    }
    if (!alloc_ok) {
        for (int k = 0; k < root_count; k++)
            lv_free((void **) &tasks[k].node_ids);
        lv_free((void **) &tasks);
        lv_free((void **) &parent);
        lv_free((void **) &rank);
        lv_free((void **) &root_ids);
        lv_free((void **) &root_of);
        lv_free((void **) &counts);
        return -1;
    }

    int *cursor = (int *) lv_calloc((size_t) root_count, sizeof(int));
    if (cursor == NULL) {
        for (int k = 0; k < root_count; k++)
            lv_free((void **) &tasks[k].node_ids);
        lv_free((void **) &tasks);
        lv_free((void **) &parent);
        lv_free((void **) &rank);
        lv_free((void **) &root_ids);
        lv_free((void **) &root_of);
        lv_free((void **) &counts);
        return -1;
    }
    for (int i = 0; i < node_count; i++) {
        int k = root_of[i];
        tasks[k].node_ids[cursor[k]++] = i;
    }

    lv_free((void **) &parent);
    lv_free((void **) &rank);
    lv_free((void **) &root_ids);
    lv_free((void **) &root_of);
    lv_free((void **) &counts);
    lv_free((void **) &cursor);

    *out_tasks = tasks;
    *out_task_count = root_count;
    return 0;
}
