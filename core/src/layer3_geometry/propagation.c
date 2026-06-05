/* ========================================================================
 * propagation.c — 约束传播引擎实现
 *
 * WFC 风格的动态约束传播，包含：
 *   - 节点状态空间管理
 *   - AC-3 弧相容性传播
 *   - 熵最小化节点选择
 *   - 坍缩与回溯
 *   - 完整 WFC 求解循环
 * ======================================================================== */

#include "propagation.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "error_codes.h"

/* ================================================================
 * 内部辅助函数
 * ================================================================ */

/** @brief 确保状态空间数组有足够容量 */
static bool state_ensure_capacity(NodeStateSpace *state, int needed) {
    if (needed <= state->capacity) return true;
    int new_cap = state->capacity < 8 ? 8 : state->capacity;
    while (new_cap < needed) new_cap *= 2;

    SymbolicCoord **new_coords = (SymbolicCoord **)realloc(state->possible_coords,
                                                            (size_t)new_cap * sizeof(SymbolicCoord *));
    int *new_dims = (int *)realloc(state->coord_dims, (size_t)new_cap * sizeof(int));
    if (!new_coords || !new_dims) {
        free(new_coords);
        free(new_dims);
        return false;
    }
    state->possible_coords = new_coords;
    state->coord_dims = new_dims;
    state->capacity = new_cap;
    return true;
}

/** @brief 创建一个节点状态空间 */
static NodeStateSpace *state_create(int node_id) {
    NodeStateSpace *s = (NodeStateSpace *)calloc(1, sizeof(NodeStateSpace));
    if (!s) return NULL;
    s->node_id = node_id;
    s->is_collapsed = false;
    s->is_unbounded = false;
    s->collapsed_value = NULL;
    s->possible_coords = NULL;
    s->coord_dims = NULL;
    s->coord_count = 0;
    s->capacity = 0;
    return s;
}

/** @brief 销毁节点状态空间 */
static void state_destroy(NodeStateSpace *s) {
    if (!s) return;
    if (s->collapsed_value) {
        symbolic_coord_destroy(s->collapsed_value);
        s->collapsed_value = NULL;
    }
    if (s->possible_coords) {
        for (int i = 0; i < s->coord_count; i++) {
            if (s->possible_coords[i]) {
                symbolic_coord_destroy(s->possible_coords[i]);
            }
        }
        free(s->possible_coords);
        s->possible_coords = NULL;
    }
    free(s->coord_dims);
    s->coord_dims = NULL;
    free(s);
}

/** @brief 深拷贝节点状态空间 */
static NodeStateSpace *state_deep_copy(const NodeStateSpace *src) {
    if (!src) return NULL;
    NodeStateSpace *dst = (NodeStateSpace *)calloc(1, sizeof(NodeStateSpace));
    if (!dst) return NULL;

    dst->node_id = src->node_id;
    dst->is_collapsed = src->is_collapsed;
    dst->is_unbounded = src->is_unbounded;
    dst->coord_count = src->coord_count;
    dst->capacity = src->coord_count > 0 ? src->coord_count : 1;

    if (src->collapsed_value) {
        dst->collapsed_value = symbolic_coord_copy(src->collapsed_value);
    }

    if (src->coord_count > 0 && src->possible_coords) {
        dst->possible_coords = (SymbolicCoord **)calloc((size_t)dst->capacity, sizeof(SymbolicCoord *));
        dst->coord_dims = (int *)calloc((size_t)dst->capacity, sizeof(int));
        if (!dst->possible_coords || !dst->coord_dims) {
            state_destroy(dst);
            return NULL;
        }
        for (int i = 0; i < src->coord_count; i++) {
            if (src->possible_coords[i]) {
                dst->possible_coords[i] = symbolic_coord_copy(src->possible_coords[i]);
                dst->coord_dims[i] = src->coord_dims[i];
            }
        }
    }
    return dst;
}

/** @brief 向状态空间添加一个候选坐标 */
static bool state_add_candidate(NodeStateSpace *state, const SymbolicCoord *coord, int dim) {
    if (state->is_collapsed) return false;
    if (!state_ensure_capacity(state, state->coord_count + 1)) return false;
    state->possible_coords[state->coord_count] = symbolic_coord_copy(coord);
    state->coord_dims[state->coord_count] = dim;
    state->coord_count++;
    return true;
}

/** @brief 从状态空间移除指定索引的候选 */
static bool state_remove_at(NodeStateSpace *state, int index) {
    if (index < 0 || index >= state->coord_count) return false;
    if (state->possible_coords[index]) {
        symbolic_coord_destroy(state->possible_coords[index]);
    }
    /* 将最后一个元素移到被删除位置 */
    int last = state->coord_count - 1;
    if (index != last) {
        state->possible_coords[index] = state->possible_coords[last];
        state->coord_dims[index] = state->coord_dims[last];
    }
    state->coord_count--;
    state->possible_coords[last] = NULL;
    return true;
}

/** @brief 检查两个坐标是否相等（使用 symbolic_coord_compare） */
static bool coords_equal(const SymbolicCoord *a, const SymbolicCoord *b) {
    if (!a || !b) return (a == b);
    return (symbolic_coord_compare(a, b) == 0);
}

/** @brief 检查状态空间中是否已包含某坐标（避免重复） */
static bool state_contains(const NodeStateSpace *state, const SymbolicCoord *coord) {
    if (!state || !coord) return false;
    for (int i = 0; i < state->coord_count; i++) {
        if (state->possible_coords[i] && coords_equal(state->possible_coords[i], coord)) {
            return true;
        }
    }
    return false;
}

/* ================================================================
 * 传播队列（环形缓冲区）
 * ================================================================ */

static bool queue_init(PropagationContext *ctx) {
    ctx->queue_capacity = PROP_DEFAULT_QUEUE_CAPACITY;
    ctx->propagation_queue = (int *)calloc((size_t)ctx->queue_capacity, sizeof(int));
    if (!ctx->propagation_queue) return false;
    ctx->queue_head = 0;
    ctx->queue_tail = 0;
    ctx->queue_size = 0;
    return true;
}

static void queue_destroy(PropagationContext *ctx) {
    free(ctx->propagation_queue);
    ctx->propagation_queue = NULL;
}

static bool queue_ensure_capacity(PropagationContext *ctx) {
    if (ctx->queue_size < ctx->queue_capacity) return true;
    int new_cap = ctx->queue_capacity * 2;
    int *new_q = (int *)realloc(ctx->propagation_queue, (size_t)new_cap * sizeof(int));
    if (!new_q) return false;

    /* 将数据从环形缓冲区展开到线性数组 */
    if (ctx->queue_tail > ctx->queue_head) {
        memmove(new_q, new_q + ctx->queue_head, (size_t)ctx->queue_size * sizeof(int));
    } else if (ctx->queue_tail < ctx->queue_head) {
        /* 两段数据：head..end 和 0..tail */
        int seg1 = ctx->queue_capacity - ctx->queue_head;
        int seg2 = ctx->queue_tail;
        memmove(new_q, new_q + ctx->queue_head, (size_t)seg1 * sizeof(int));
        memmove(new_q + seg1, ctx->propagation_queue, (size_t)seg2 * sizeof(int));
    }
    ctx->propagation_queue = new_q;
    ctx->queue_head = 0;
    ctx->queue_tail = ctx->queue_size;
    ctx->queue_capacity = new_cap;
    return true;
}

static bool queue_push(PropagationContext *ctx, int value) {
    if (!queue_ensure_capacity(ctx)) return false;
    ctx->propagation_queue[ctx->queue_tail] = value;
    ctx->queue_tail = (ctx->queue_tail + 1) % ctx->queue_capacity;
    ctx->queue_size++;
    return true;
}

static bool queue_pop(PropagationContext *ctx, int *out_value) {
    if (ctx->queue_size == 0) return false;
    *out_value = ctx->propagation_queue[ctx->queue_head];
    ctx->queue_head = (ctx->queue_head + 1) % ctx->queue_capacity;
    ctx->queue_size--;
    return true;
}

static void queue_clear(PropagationContext *ctx) {
    ctx->queue_head = 0;
    ctx->queue_tail = 0;
    ctx->queue_size = 0;
}

/* ================================================================
 * 生命周期管理
 * ================================================================ */

PropagationContext *propagation_context_create(ConstraintGraph *graph) {
    if (!graph) return NULL;

    PropagationContext *ctx = (PropagationContext *)calloc(1, sizeof(PropagationContext));
    if (!ctx) return NULL;

    ctx->graph = graph;
    ctx->strategy = PROP_STRATEGY_MIN_ENTROPY;
    ctx->collapse_strategy = PROP_COLLAPSE_FIRST;
    ctx->max_iterations = PROP_DEFAULT_MAX_ITERATIONS;
    ctx->max_backtracks = PROP_DEFAULT_MAX_BACKTRACKS;

    /* 初始化状态空间数组 */
    ctx->state_count = graph->node_count;
    ctx->state_spaces = (NodeStateSpace *)calloc((size_t)ctx->state_count, sizeof(NodeStateSpace));
    if (!ctx->state_spaces && ctx->state_count > 0) {
        free(ctx);
        return NULL;
    }

    /* 初始化传播队列 */
    if (!queue_init(ctx)) {
        free(ctx->state_spaces);
        free(ctx);
        return NULL;
    }

    /* 初始化快照栈 */
    ctx->snapshot_capacity = PROP_DEFAULT_SNAPSHOT_CAPACITY;
    ctx->snapshot_stack = (PropagationSnapshot **)calloc((size_t)ctx->snapshot_capacity,
                                                          sizeof(PropagationSnapshot *));
    if (!ctx->snapshot_stack) {
        queue_destroy(ctx);
        free(ctx->state_spaces);
        free(ctx);
        return NULL;
    }

    return ctx;
}

void propagation_context_destroy(PropagationContext *ctx) {
    if (!ctx) return;

    /* 销毁状态空间 */
    for (int i = 0; i < ctx->state_count; i++) {
        state_destroy(&ctx->state_spaces[i]);
    }
    free(ctx->state_spaces);

    /* 销毁传播队列 */
    queue_destroy(ctx);

    /* 销毁快照栈 */
    for (int i = 0; i < ctx->snapshot_count; i++) {
        if (ctx->snapshot_stack[i]) {
            propagation_snapshot_destroy(ctx->snapshot_stack[i]);
        }
    }
    free(ctx->snapshot_stack);

    free(ctx);
}

/* ================================================================
 * 状态空间初始化
 * ================================================================ */

PropagationResult propagation_init_state_spaces(PropagationContext *ctx) {
    if (!ctx || !ctx->graph) return PROP_RESULT_CONTRADICTION;

    for (int i = 0; i < ctx->state_count; i++) {
        state_destroy(&ctx->state_spaces[i]);
        memset(&ctx->state_spaces[i], 0, sizeof(NodeStateSpace));
        ctx->state_spaces[i].node_id = i;
    }

    for (int i = 0; i < ctx->graph->node_count; i++) {
        GeomNode *node = graph_get_node(ctx->graph, i);
        if (!node) continue;
        if (!node->is_active) continue;

        NodeStateSpace *ss = &ctx->state_spaces[i];

        switch (node->type) {
        case GEOM_POINT: {
            /* 点节点：使用当前坐标作为初始状态 */
            if (node->coord_count >= 2 && node->symbolic_coords &&
                node->symbolic_coords[0] && node->symbolic_coords[1]) {
                /* 已有坐标 → 已坍缩 */
                ss->collapsed_value = symbolic_coord_copy(node->symbolic_coords[0]);
                ss->is_collapsed = true;
            } else {
                /* 无坐标 → 检查是否有约束 */
                int constraints[64];
                int count = graph_find_constraints_involving(ctx->graph, i, constraints, 64);
                if (count == 0) {
                    ss->is_unbounded = true;
                }
                /* 有约束但无坐标 → 由传播引擎后续填充 */
            }
            break;
        }
        case GEOM_LINE_SEGMENT: {
            /* 线段节点：基于端点状态空间 */
            if (node->coord_count >= 4 && node->symbolic_coords) {
                bool all_coords = true;
                for (int j = 0; j < 4; j++) {
                    if (!node->symbolic_coords[j]) { all_coords = false; break; }
                }
                if (all_coords) {
                    ss->collapsed_value = symbolic_coord_copy(node->symbolic_coords[0]);
                    ss->is_collapsed = true;
                }
            }
            break;
        }
        case GEOM_REGION:
        case GEOM_PORT:
        case GEOM_FUNCTION_BLOCK:
            /* 其他类型暂不处理状态空间 */
            break;
        }
    }

    return PROP_RESULT_CONSISTENT;
}

NodeStateSpace *propagation_get_state_space(PropagationContext *ctx, int node_id) {
    if (!ctx || node_id < 0 || node_id >= ctx->state_count) return NULL;
    return &ctx->state_spaces[node_id];
}

/* ================================================================
 * AC-3 弧相容性传播
 * ================================================================ */

/**
 * @brief 检查候选坐标是否满足关联约束
 *
 * 对于 INCIDENCE（点在线段上）约束：
 *   计算行列式 (Px-Ax)*(By-Ay) - (Py-Ay)*(Bx-Ax) 是否为零
 *
 * @param point_coord   点节点的 x 坐标（SymbolicCoord）
 * @param point_node    点节点（用于获取 y 坐标）
 * @param line_node     线段节点（含端点坐标）
 */
static bool check_incidence_compatible(const SymbolicCoord *point_coord,
                                        const GeomNode *point_node,
                                        const GeomNode *line_node) {
    if (!point_coord || !line_node || !line_node->symbolic_coords) return false;
    if (line_node->coord_count < 4) return false;

    /* 线段端点 A(x0,y0), B(x1,y1) */
    const SymbolicCoord *ax = line_node->symbolic_coords[0];
    const SymbolicCoord *ay = line_node->symbolic_coords[1];
    const SymbolicCoord *bx = line_node->symbolic_coords[2];
    const SymbolicCoord *by = line_node->symbolic_coords[3];
    if (!ax || !ay || !bx || !by) return false;

    /* 点 P(px, py) — 使用 point_coord 的 x,y 分量 */
    /* 使用 double 近似检查行列式（符号坐标的数值评估） */
    double px = symbolic_coord_to_double(point_coord);
    double ax_d = symbolic_coord_to_double(ax);
    double ay_d = symbolic_coord_to_double(ay);
    double bx_d = symbolic_coord_to_double(bx);
    double by_d = symbolic_coord_to_double(by);

    /* 正确计算行列式：|PA × PB| = (Px-Ax)*(By-Ay) - (Py-Ay)*(Bx-Ax)
     * 这里 point_coord 是单值，需要分别获取 x 和 y */
    double py = 0.0;  /* 若 point_coord 只有 x，y 默认为 0 */
    /* 尝试从 point_node 获取 y 坐标 */
    if (point_node && point_node->coord_count >= 2) {
        py = symbolic_coord_to_double(point_node->symbolic_coords[1]);
    }
    double det = (px - ax_d) * (by_d - ay_d) - (py - ay_d) * (bx_d - ax_d);
    /* 使用容差判断点是否在线段上 */
    double tol = 1e-9;
    return (fabs(det) < tol);
}

/**
 * @brief 检查候选坐标是否满足约束
 *
 * 根据约束类型分派到具体的兼容性检查。
 */
static bool check_constraint_compatible(const SymbolicCoord *candidate,
                                         const Constraint *constraint,
                                         const ConstraintGraph *graph) {
    if (!candidate || !constraint || !graph) return false;

    switch (constraint->type) {
    case INCIDENCE: {
        /* 点在线段上：参与者 [point_id, line_id] */
        if (constraint->participant_count < 2) return true;
        int point_id = constraint->participants[0];
        int line_id = constraint->participants[1];
        GeomNode *point_node = graph_get_node(graph, point_id);
        GeomNode *line_node = graph_get_node(graph, line_id);
        return check_incidence_compatible(candidate, point_node, line_node);
    }
    case BETWEENNESS:
    case INTERSECTION:
    case CONTAINMENT:
    case CONNECTION:
        /* 其他约束类型暂不进行候选过滤 */
        return true;
    }
    return true;
}

bool propagation_arc_reduce(PropagationContext *ctx, int constraint_id) {
    if (!ctx || !ctx->graph) return false;

    Constraint *c = graph_get_constraint(ctx->graph, constraint_id);
    if (!c || !c->is_active) return false;

    bool changed = false;

    /* 对每个参与者执行弧相容性检查 */
    for (int p = 0; p < c->participant_count; p++) {
        int node_id = c->participants[p];
        if (node_id < 0 || node_id >= ctx->state_count) continue;

        NodeStateSpace *ss = &ctx->state_spaces[node_id];
        if (ss->is_collapsed || ss->is_unbounded || ss->coord_count == 0) continue;

        /* 检查每个候选是否与约束兼容 */
        int i = 0;
        while (i < ss->coord_count) {
            SymbolicCoord *candidate = ss->possible_coords[i];
            if (!candidate || check_constraint_compatible(candidate, c, ctx->graph)) {
                i++; /* 兼容，保留 */
            } else {
                state_remove_at(ss, i);
                changed = true;
                ctx->prune_count++;
            }
        }

        /* 若状态空间为空 → 矛盾 */
        if (ss->coord_count == 0 && !ss->is_unbounded) {
            return true; /* changed = true 表示有问题 */
        }

        /* 若状态空间收缩为单一值 → 自动坍缩 */
        if (ss->coord_count == 1 && !ss->is_collapsed) {
            ss->collapsed_value = symbolic_coord_copy(ss->possible_coords[0]);
            ss->is_collapsed = true;
            changed = true;
        }
    }

    return changed;
}

PropagationResult propagation_run(PropagationContext *ctx) {
    if (!ctx || !ctx->graph) return PROP_RESULT_CONTRADICTION;

    queue_clear(ctx);

    /* 将所有活跃约束的参与者加入传播队列 */
    for (int i = 0; i < ctx->graph->constraint_count; i++) {
        Constraint *c = ctx->graph->constraints[i];
        if (!c || !c->is_active) continue;
        for (int p = 0; p < c->participant_count; p++) {
            queue_push(ctx, c->participants[p]);
        }
    }

    int iterations = 0;

    while (ctx->queue_size > 0 && iterations < ctx->max_iterations) {
        int node_id;
        if (!queue_pop(ctx, &node_id)) break;

        /* 查找涉及该节点的所有约束 */
        int constraint_ids[128];
        int count = graph_find_constraints_involving(ctx->graph, node_id,
                                                      constraint_ids, 128);

        for (int i = 0; i < count; i++) {
            bool changed = propagation_arc_reduce(ctx, constraint_ids[i]);
            ctx->propagation_steps++;

            if (changed) {
                /* 检查是否有节点状态空间为空 */
                for (int j = 0; j < ctx->state_count; j++) {
                    NodeStateSpace *ss = &ctx->state_spaces[j];
                    if (!ss->is_unbounded && ss->coord_count == 0 && !ss->is_collapsed) {
                        return PROP_RESULT_CONTRADICTION;
                    }
                }

                /* 将受影响的邻域节点重新入队 */
                Constraint *c = graph_get_constraint(ctx->graph, constraint_ids[i]);
                if (c && c->is_active) {
                    for (int p = 0; p < c->participant_count; p++) {
                        if (c->participants[p] != node_id) {
                            queue_push(ctx, c->participants[p]);
                        }
                    }
                }
            }
        }

        iterations++;
    }

    if (iterations >= ctx->max_iterations) {
        return PROP_RESULT_TIMEOUT;
    }

    /* 检查是否所有节点都已坍缩 */
    if (propagation_is_fully_collapsed(ctx)) {
        return PROP_RESULT_SATISFIED;
    }

    return PROP_RESULT_STABLE;
}

/* ================================================================
 * WFC 节点选择与坍缩
 * ================================================================ */

double propagation_compute_entropy(const NodeStateSpace *state) {
    if (!state) return PROP_ENTROPY_UNBOUNDED;
    if (state->is_unbounded) return PROP_ENTROPY_UNBOUNDED;
    if (state->is_collapsed) return 0.0;
    if (state->coord_count <= 0) return PROP_ENTROPY_UNBOUNDED;
    return log2((double)state->coord_count);
}

int propagation_select_node(PropagationContext *ctx) {
    if (!ctx) return -1;

    double min_entropy = 1e30;
    int best_node = -1;
    int best_degree = -1;

    for (int i = 0; i < ctx->state_count; i++) {
        NodeStateSpace *ss = &ctx->state_spaces[i];
        if (ss->is_collapsed || ss->is_unbounded) continue;
        if (ss->coord_count <= 0) continue;

        double entropy = propagation_compute_entropy(ss);
        if (entropy < 0) continue; /* 跳过 unbounded */

        /* 计算度数（邻接约束数） */
        int constraints[64];
        int degree = graph_find_constraints_involving(ctx->graph, i, constraints, 64);

        switch (ctx->strategy) {
        case PROP_STRATEGY_MIN_ENTROPY:
        case PROP_STRATEGY_MRVS:
            if (entropy < min_entropy ||
                (entropy == min_entropy && degree > best_degree)) {
                min_entropy = entropy;
                best_node = i;
                best_degree = degree;
            }
            break;

        case PROP_STRATEGY_DEGREE:
            if (degree > best_degree ||
                (degree == best_degree && entropy < min_entropy)) {
                best_degree = degree;
                best_node = i;
                min_entropy = entropy;
            }
            break;

        case PROP_STRATEGY_BFS: {
            /* BFS 策略：从最近坍缩的节点出发，广度优先选择最近的未坍缩邻居。
             * 使用约束图中的邻接关系进行 BFS 遍历，选择第一个遇到的未坍缩节点。
             * 若无已坍缩节点作为起点，则回退到最小熵选择。 */
            bool found_bfs = false;
            /* 收集所有已坍缩节点作为 BFS 起点 */
            for (int start = 0; start < ctx->state_count && !found_bfs; start++) {
                const NodeStateSpace *ss_start = &ctx->state_spaces[start];
                if (!ss_start->is_collapsed) continue;

                /* 从 start 节点开始 BFS */
                int bfs_cap = ctx->state_count > 256 ? ctx->state_count : 256;
                int *bfs_queue = (int *)lv00_malloc((size_t)bfs_cap * sizeof(int));
                bool *visited = (bool *)lv00_calloc((size_t)ctx->state_count, sizeof(bool));
                if (!bfs_queue || !visited) {
                    lv00_free((void **)&bfs_queue);
                    lv00_free((void **)&visited);
                    continue;
                }
                int bfs_head = 0, bfs_tail = 0;

                bfs_queue[bfs_tail++] = start;
                visited[start] = true;

                while (bfs_head < bfs_tail && !found_bfs) {
                    int cur = bfs_queue[bfs_head++];

                    /* 查找 cur 的所有邻接节点（通过约束关系） */
                    int cids[128];
                    int nc = graph_find_constraints_involving(ctx->graph, cur, cids, 128);
                    for (int ci = 0; ci < nc && !found_bfs; ci++) {
                        Constraint *cc = graph_get_constraint(ctx->graph, cids[ci]);
                        if (!cc || !cc->is_active) continue;
                        for (int p = 0; p < cc->participant_count; p++) {
                            int nb = cc->participants[p];
                            if (nb < 0 || nb >= ctx->state_count) continue;
                            if (visited[nb]) continue;
                            visited[nb] = true;

                            const NodeStateSpace *ss_nb = &ctx->state_spaces[nb];
                            if (!ss_nb->is_collapsed && !ss_nb->is_unbounded
                                && ss_nb->coord_count > 0) {
                                /* 找到最近的未坍缩邻居 */
                                best_node = nb;
                                found_bfs = true;
                                break;
                            }
                            bfs_queue[bfs_tail++] = nb;
                        }
                    }
                }
            }
            lv00_free((void **)&bfs_queue);
            lv00_free((void **)&visited);
            /* 若 BFS 未找到（无已坍缩节点作为起点），回退到最小熵 */
            if (!found_bfs) {
                if (entropy < min_entropy ||
                    (entropy == min_entropy && degree > best_degree)) {
                    min_entropy = entropy;
                    best_node = i;
                    best_degree = degree;
                }
            }
            break;
        }

        case PROP_STRATEGY_TOPOLOGICAL: {
            /* 拓扑排序策略：按约束依赖关系选择"被依赖最多"的未坍缩节点。
             * 计算每个节点的入度（有多少约束以该节点为被动参与者），
             * 选择入度最高的节点优先坍缩（确保依赖链上游先确定）。
             * 若入度相同，选择熵最小的打破平局。 */
            int in_degree = 0;
            int cids_t[128];
            int nc_t = graph_find_constraints_involving(ctx->graph, i, cids_t, 128);
            for (int ci = 0; ci < nc_t; ci++) {
                Constraint *cc = graph_get_constraint(ctx->graph, cids_t[ci]);
                if (!cc || !cc->is_active) continue;
                /* 统计有多少其他未坍缩参与者依赖此节点 */
                for (int p = 0; p < cc->participant_count; p++) {
                    int other = cc->participants[p];
                    if (other != i && other >= 0 && other < ctx->state_count) {
                        const NodeStateSpace *ss_other = &ctx->state_spaces[other];
                        if (!ss_other->is_collapsed) {
                            in_degree++;
                        }
                    }
                }
            }
            /* 优先选择被依赖最多的节点，熵作为次要排序键 */
            if (in_degree > best_degree ||
                (in_degree == best_degree && entropy < min_entropy)) {
                best_degree = in_degree;
                best_node = i;
                min_entropy = entropy;
            }
            break;
        }
        }
    }

    return best_node;
}

bool propagation_collapse(PropagationContext *ctx, int node_id) {
    if (!ctx || node_id < 0 || node_id >= ctx->state_count) return false;

    NodeStateSpace *ss = &ctx->state_spaces[node_id];
    if (ss->is_collapsed || ss->is_unbounded) return false;
    if (ss->coord_count == 0) return false;

    int selected_index = 0;

    switch (ctx->collapse_strategy) {
    case PROP_COLLAPSE_FIRST:
        selected_index = 0;
        break;
    case PROP_COLLAPSE_WEIGHTED: {
        /* 基于约束兼容性的加权随机选择 */
        /* 计算每个候选坐标的兼容性权重：与邻域约束兼容的数量 */
        double *weights = (double *)calloc((size_t)ss->coord_count, sizeof(double));
        if (weights && ss->coord_count > 0) {
            int cids[128];
            int nc = graph_find_constraints_involving(ctx->graph, node_id, cids, 128);
            for (int k = 0; k < ss->coord_count; k++) {
                double w = 1.0;
                for (int ci = 0; ci < nc; ci++) {
                    Constraint *c = graph_get_constraint(ctx->graph, cids[ci]);
                    if (c && c->is_active &&
                        check_constraint_compatible(ss->possible_coords[k], c, ctx->graph)) {
                        w += 1.0;
                    }
                }
                weights[k] = w;
            }
            /* 计算总权重 */
            double total = 0.0;
            for (int k = 0; k < ss->coord_count; k++) total += weights[k];
            /* 加权随机选择（轮盘赌算法） */
            if (total > 0.0) {
                double r = ((double)rand() / (double)RAND_MAX) * total;
                double accum = 0.0;
                for (int k = 0; k < ss->coord_count; k++) {
                    accum += weights[k];
                    if (r <= accum) { selected_index = k; break; }
                }
            }
            free(weights);
        }
        break;
    }

    /* 保存选中的坐标，释放其余 */
    SymbolicCoord *selected = ss->possible_coords[selected_index];
    ss->possible_coords[selected_index] = NULL;

    /* 释放未选中的候选 */
    for (int i = 0; i < ss->coord_count; i++) {
        if (ss->possible_coords[i]) {
            symbolic_coord_destroy(ss->possible_coords[i]);
            ss->possible_coords[i] = NULL;
        }
    }

    ss->collapsed_value = selected;
    ss->is_collapsed = true;
    ss->coord_count = 0;
    ctx->collapse_count++;

    return true;
}

/* ================================================================
 * 完整 WFC 求解循环
 * ================================================================ */

PropagationResult propagation_wfc_solve(PropagationContext *ctx) {
    if (!ctx || !ctx->graph) return PROP_RESULT_CONTRADICTION;

    int wfc_iterations = 0;

    while (wfc_iterations < PROP_WFC_MAX_COLLABORATION_ITERATIONS) {
        /* 步骤 1: AC-3 约束传播 */
        PropagationResult prop_result = propagation_run(ctx);

        if (prop_result == PROP_RESULT_CONTRADICTION) {
            /* 死路 → 尝试回溯 */
            if (ctx->snapshot_count > 0 && ctx->backtrack_count < ctx->max_backtracks) {
                PropagationSnapshot *snap = ctx->snapshot_stack[ctx->snapshot_count - 1];
                ctx->snapshot_stack[ctx->snapshot_count - 1] = NULL;
                ctx->snapshot_count--;
                propagation_snapshot_restore(ctx, snap);
                ctx->backtrack_count++;
                wfc_iterations++;
                continue;
            }
            return PROP_RESULT_CONTRADICTION;
        }

        if (prop_result == PROP_RESULT_SATISFIED) {
            return PROP_RESULT_SATISFIED;
        }

        if (prop_result == PROP_RESULT_TIMEOUT) {
            return PROP_RESULT_TIMEOUT;
        }

        /* 步骤 2: 选择下一个要坍缩的节点 */
        int node_id = propagation_select_node(ctx);
        if (node_id < 0) {
            /* 没有可坍缩的节点 → 检查是否全部已确定 */
            if (propagation_is_fully_collapsed(ctx)) {
                return PROP_RESULT_SATISFIED;
            }
            /* 剩余节点都是 unbounded → 稳定但未完全确定 */
            return PROP_RESULT_STABLE;
        }

        /* 步骤 3: 保存快照 */
        PropagationSnapshot *snap = propagation_snapshot_save(ctx);
        if (snap) {
            snap->decision_node_id = node_id;
            if (ctx->state_spaces[node_id].coord_count > 0) {
                snap->decision_coord_index = 0;
            }
            /* 压入快照栈 */
            if (ctx->snapshot_count < ctx->snapshot_capacity) {
                ctx->snapshot_stack[ctx->snapshot_count] = snap;
                ctx->snapshot_count++;
            } else {
                propagation_snapshot_destroy(snap);
            }
        }

        /* 步骤 4: 坍缩 */
        if (!propagation_collapse(ctx, node_id)) {
            return PROP_RESULT_CONTRADICTION;
        }

        wfc_iterations++;
    }

    return PROP_RESULT_TIMEOUT;
}

/* ================================================================
 * 快照与回溯
 * ================================================================ */

PropagationSnapshot *propagation_snapshot_save(PropagationContext *ctx) {
    if (!ctx) return NULL;

    PropagationSnapshot *snap = (PropagationSnapshot *)calloc(1, sizeof(PropagationSnapshot));
    if (!snap) return NULL;

    snap->state_count = ctx->state_count;
    snap->states = (NodeStateSpace *)calloc((size_t)snap->state_count, sizeof(NodeStateSpace));
    if (!snap->states) {
        free(snap);
        return NULL;
    }

    for (int i = 0; i < snap->state_count; i++) {
        snap->states[i] = *(state_deep_copy(&ctx->state_spaces[i]));
    }

    snap->propagation_steps = ctx->propagation_steps;
    snap->collapse_count = ctx->collapse_count;
    snap->backtrack_count = ctx->backtrack_count;
    snap->prune_count = ctx->prune_count;
    snap->decision_node_id = -1;
    snap->decision_coord_index = -1;

    return snap;
}

void propagation_snapshot_restore(PropagationContext *ctx, PropagationSnapshot *snap) {
    if (!ctx || !snap) return;

    /* 销毁当前状态空间 */
    for (int i = 0; i < ctx->state_count; i++) {
        state_destroy(&ctx->state_spaces[i]);
    }

    /* 用快照替换 */
    for (int i = 0; i < snap->state_count && i < ctx->state_count; i++) {
        ctx->state_spaces[i] = snap->states[i];
    }

    ctx->propagation_steps = snap->propagation_steps;
    ctx->collapse_count = snap->collapse_count;
    ctx->backtrack_count = snap->backtrack_count;
    ctx->prune_count = snap->prune_count;

    /* 销毁快照壳 */
    free(snap->states);
    free(snap);
}

void propagation_snapshot_destroy(PropagationSnapshot *snap) {
    if (!snap) return;
    if (snap->states) {
        for (int i = 0; i < snap->state_count; i++) {
            state_destroy(&snap->states[i]);
        }
        free(snap->states);
    }
    free(snap);
}

/* ================================================================
 * 配置
 * ================================================================ */

void propagation_set_strategy(PropagationContext *ctx, PropagationStrategy strategy) {
    if (ctx) ctx->strategy = strategy;
}

void propagation_set_collapse_strategy(PropagationContext *ctx, CollapseStrategy strategy) {
    if (ctx) ctx->collapse_strategy = strategy;
}

void propagation_set_stream_context(PropagationContext *ctx, StreamContext *stream_ctx) {
    if (ctx) ctx->stream_ctx = stream_ctx;
}

void propagation_set_max_iterations(PropagationContext *ctx, int max_iterations) {
    if (ctx && max_iterations > 0) ctx->max_iterations = max_iterations;
}

void propagation_set_max_backtracks(PropagationContext *ctx, int max_backtracks) {
    if (ctx && max_backtracks >= 0) ctx->max_backtracks = max_backtracks;
}

/* ================================================================
 * 诊断与查询
 * ================================================================ */

void propagation_get_statistics(const PropagationContext *ctx,
                                 int64_t *out_steps,
                                 int64_t *out_collapses,
                                 int64_t *out_backtracks,
                                 int64_t *out_prunes) {
    if (!ctx) return;
    if (out_steps) *out_steps = ctx->propagation_steps;
    if (out_collapses) *out_collapses = ctx->collapse_count;
    if (out_backtracks) *out_backtracks = ctx->backtrack_count;
    if (out_prunes) *out_prunes = ctx->prune_count;
}

int propagation_count_uncollapsed(const PropagationContext *ctx) {
    if (!ctx) return 0;
    int count = 0;
    for (int i = 0; i < ctx->state_count; i++) {
        const NodeStateSpace *ss = &ctx->state_spaces[i];
        if (!ss->is_collapsed && !ss->is_unbounded && ss->coord_count > 0) {
            count++;
        }
    }
    return count;
}

bool propagation_is_fully_collapsed(const PropagationContext *ctx) {
    if (!ctx) return false;
    for (int i = 0; i < ctx->state_count; i++) {
        const NodeStateSpace *ss = &ctx->state_spaces[i];
        GeomNode *node = graph_get_node(ctx->graph, i);
        if (!node || !node->is_active) continue;
        if (node->type != GEOM_POINT) continue; /* 只关注点节点 */
        if (!ss->is_collapsed && !ss->is_unbounded) {
            return false;
        }
    }
    return true;
}
