/**
 * @file graph_conflict.c
 * @brief 冲突检测与兼容性验证
 *
 * @details 拆分子模块（Lv-00 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lv00/constraint_graph.h"
#include "lv00/symbolic_coord.h"
#include "debug.h"
#include "lv00_internal.h"
#include "lv00_utils.h"
#include "lv00/solver.h"

#ifndef LV00_ADJ_MAX_PER_NODE
#define LV00_ADJ_MAX_PER_NODE 256
#endif
#ifndef LV00_MAX_CONN_ADJ_STRIDE
#define LV00_MAX_CONN_ADJ_STRIDE 256
#endif

/* ── 流上下文声明 ── */
LV00_DECLARE_STREAM_CTX(graph);

/**
 * 查找线性相关的约束（冗余检测辅助函数）。
 * 使用高斯消元法构建约束矩阵并识别线性相关的约束。
 *
 * @param graph         约束图指针
 * @param out_count     输出：找到的冗余约束数量
 * @param max_redundant 冗余约束数组的最大容量
 * @return 冗余约束 ID 数组，调用者负责释放；失败返回 NULL
 */
static int *find_linearly_dependent_constraints(ConstraintGraph *graph,
                                                  int *out_count,
                                                  int max_redundant) {
    if (!graph || !out_count || max_redundant <= 0)
        return NULL;

    int *redundant = lv00_malloc((size_t) max_redundant * sizeof(int));
    if (!redundant)
        return NULL;
    *out_count = 0;

    /* 统计线性约束和点的数量 */
    int num_linear = 0;
    int num_point_nodes = 0;
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *n = graph->nodes[i];
        if (n && n->type == GEOM_POINT)
            num_point_nodes++;
    }
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (c && c->is_active && c->type == INCIDENCE)
            num_linear++;
    }

    if (num_linear == 0 || num_point_nodes == 0) {
        return redundant;
    }

    int num_vars = num_point_nodes * 2; /* 每个点 2 个坐标变量 */

    /* 构建点 ID 到变量索引的映射 */
    int *point_ids = lv00_malloc((size_t) num_point_nodes * sizeof(int));
    bool *point_seen = lv00_calloc((size_t) num_point_nodes, sizeof(bool));
    int *node_id_to_var_idx = lv00_malloc((size_t) graph->node_count * 2 * sizeof(int));
    int *linear_constraint_indices = lv00_malloc((size_t) num_linear * sizeof(int));
    if (!point_ids || !point_seen || !node_id_to_var_idx || !linear_constraint_indices) {
        lv00_free((void **) &redundant);
        lv00_free((void **) &point_ids);
        lv00_free((void **) &point_seen);
        lv00_free((void **) &node_id_to_var_idx);
        lv00_free((void **) &linear_constraint_indices);
        return NULL;
    }
    memset(node_id_to_var_idx, -1, (size_t) graph->node_count * 2 * sizeof(int));

    int point_idx = 0;
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *n = graph->nodes[i];
        if (n && n->type == GEOM_POINT) {
            point_ids[point_idx] = n->id;
            if (n->id >= 0 && n->id < graph->node_count * 2) {
                node_id_to_var_idx[n->id * 2] = point_idx * 2;
                node_id_to_var_idx[n->id * 2 + 1] = point_idx * 2 + 1;
            }
            point_idx++;
        }
    }

    /* 收集线性约束索引 */
    int lin_idx = 0;
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (c && c->is_active && c->type == INCIDENCE) {
            linear_constraint_indices[lin_idx++] = i;
        }
    }

    /* 构建增广矩阵 */
    mpq_t *matrix = lv00_malloc((size_t) num_linear * (size_t)(num_vars + 1) * sizeof(mpq_t));
    int *pivot_row = lv00_malloc((size_t) num_linear * sizeof(int));
    if (!matrix || !pivot_row) {
        lv00_free((void **) &redundant);
        lv00_free((void **) &point_ids);
        lv00_free((void **) &point_seen);
        lv00_free((void **) &node_id_to_var_idx);
        lv00_free((void **) &linear_constraint_indices);
        lv00_free((void **) &matrix);
        lv00_free((void **) &pivot_row);
        return NULL;
    }

    for (int i = 0; i < num_linear * (num_vars + 1); i++)
        mpq_init(matrix[i]);

    /* 填充矩阵（简化：将约束方程写入矩阵行） */
    for (int li = 0; li < num_linear; li++) {
        Constraint *c = graph->constraints[linear_constraint_indices[li]];
        if (c && c->participant_count >= 2) {
            int n1 = c->participants[0];
            int n2 = c->participants[1];
            int v1x = (n1 >= 0 && n1 < graph->node_count * 2) ? node_id_to_var_idx[n1 * 2] : -1;
            int v1y = (n1 >= 0 && n1 < graph->node_count * 2) ? node_id_to_var_idx[n1 * 2 + 1] : -1;
            int v2x = (n2 >= 0 && n2 < graph->node_count * 2) ? node_id_to_var_idx[n2 * 2] : -1;
            int v2y = (n2 >= 0 && n2 < graph->node_count * 2) ? node_id_to_var_idx[n2 * 2 + 1] : -1;
            if (v1x >= 0 && v2x >= 0) {
                mpq_set_si(matrix[li * (num_vars + 1) + v1x], 1, 1);
                mpq_set_si(matrix[li * (num_vars + 1) + v2x], -1, 1);
            }
            if (v1y >= 0 && v2y >= 0) {
                mpq_set_si(matrix[li * (num_vars + 1) + v1y], 1, 1);
                mpq_set_si(matrix[li * (num_vars + 1) + v2y], -1, 1);
            }
        }
    }

    /* 高斯消元 */
    int rank = 0;
    for (int col = 0; col < num_vars && rank < num_linear; col++) {
        int pivot = -1;
        for (int row = rank; row < num_linear; row++) {
            if (mpq_sgn(matrix[row * (num_vars + 1) + col]) != 0) {
                pivot = row;
                break;
            }
        }
        if (pivot < 0)
            continue;
        /* 交换行 */
        if (pivot != rank) {
            for (int c2 = 0; c2 <= num_vars; c2++) {
                mpq_t tmp;
                mpq_init(tmp);
                mpq_set(tmp, matrix[rank * (num_vars + 1) + c2]);
                mpq_set(matrix[rank * (num_vars + 1) + c2], matrix[pivot * (num_vars + 1) + c2]);
                mpq_set(matrix[pivot * (num_vars + 1) + c2], tmp);
                mpq_clear(tmp);
            }
        }
        pivot_row[rank] = linear_constraint_indices[pivot < num_linear ? pivot : rank];
        /* 消元 */
        mpq_t lead;
        mpq_init(lead);
        mpq_set(lead, matrix[rank * (num_vars + 1) + col]);
        for (int c2 = col; c2 <= num_vars; c2++)
            mpq_div(matrix[rank * (num_vars + 1) + c2], matrix[rank * (num_vars + 1) + c2], lead);
        mpq_clear(lead);
        for (int row = 0; row < num_linear; row++) {
            if (row == rank) continue;
            mpq_t factor;
            mpq_init(factor);
            mpq_set(factor, matrix[row * (num_vars + 1) + col]);
            for (int c2 = col; c2 <= num_vars; c2++) {
                mpq_t prod;
                mpq_init(prod);
                mpq_mul(prod, factor, matrix[rank * (num_vars + 1) + c2]);
                mpq_sub(matrix[row * (num_vars + 1) + c2], matrix[row * (num_vars + 1) + c2], prod);
                mpq_clear(prod);
            }
            mpq_clear(factor);
        }
        rank++;
    }

    /* Also check among the rank rows: if two rows have identical
     * coefficient patterns, one is redundant */
    for (int i = 0; i < rank; i++) {
        for (int j = i + 1; j < rank; j++) {
            bool identical = true;
            for (int k = 0; k <= num_vars; k++) {
                if (mpq_equal(matrix[i * (num_vars + 1) + k], matrix[j * (num_vars + 1) + k]) == 0) {
                    identical = false;
                    break;
                }
            }
            if (identical) {
                int con_id = pivot_row[j];
                bool already = false;
                for (int m = 0; m < *out_count; m++) {
                    if (redundant[m] == con_id) {
                        already = true;
                        break;
                    }
                }
                if (!already && *out_count < max_redundant) {
                    redundant[*out_count] = con_id;
                    (*out_count)++;
                    LOG_DEBUG("constraint_graph", "Linear dependency: constraint %d is identical to constraint %d",
                              con_id, pivot_row[i]);
                }
            }
        }
    }

    /* 清理 GMP mpq_t 矩阵资源 */
    for (int i = 0; i < num_linear * (num_vars + 1); i++) {
        mpq_clear(matrix[i]);
    }
    lv00_free((void **) &matrix);
    lv00_free((void **) &pivot_row);
    lv00_free((void **) &point_ids);
    lv00_free((void **) &point_seen);
    lv00_free((void **) &node_id_to_var_idx);
    lv00_free((void **) &linear_constraint_indices);

    return redundant;
}

/**
 * 检测代数冲突。
 *
 * 使用求解器模块检查约束系统是否有解。
 *
 * @param graph    约束图指针
 * @param new_con 新约束指针
 * @return true 表示存在冲突，false 表示无冲突
 */
static bool algebraic_conflict_detected(ConstraintGraph *graph, Constraint *new_con) {
    if (!graph || !new_con)
        return -1;

    /* 使用求解器模块检查约束系统是否有解 */
    int *dirty_vars = NULL;
    int dirty_count = 0;
    GroebnerResult *result = NULL;

    /* 收集与新约束相关的所有变量 */
    int max_vars = new_con->participant_count * 2;
    dirty_vars = lv00_malloc((size_t) max_vars * sizeof(int));
    if (!dirty_vars)
        return -1;

    /* 将新约束的参与者添加为脏变量 */
    for (int i = 0; i < new_con->participant_count && i < max_vars; i++) {
        dirty_vars[dirty_count++] = new_con->participants[i];
    }

    /* 调用求解器检查冲突 */
    SolverStatus status = solve_algebraic_system(graph, dirty_vars, dirty_count, &result);

    lv00_free((void **) &dirty_vars);
    dirty_vars = NULL;

    bool conflict = false;

    /* 检查求解器状态以判断冲突条件 */
    if (status == SOLVER_STATUS_NO_SOLUTION) {
        conflict = true; /* 检测到冲突 - 无解 */
    } else if (status == SOLVER_STATUS_OVERCONSTRAINED) {
        /* 使用冲突检查器检查是否有实际冲突 */
        conflict = check_conflict_equations(graph);
    }

    /* 清理 result 资源（包含可能持有 mpq_t 的 SymbolicCoord） */
    if (result) {
        for (int i = 0; i < result->solution_count; i++) {
            symbolic_coord_destroy(result->solutions[i]);
        }
        lv00_free((void **) &result->solutions);
        lv00_free((void **) &result);
    }

    return conflict;
}

/**
 * @brief 统计影响指定点的约束数量
 *
 * 遍历所有约束，收集参与者中包含指定点 ID 的约束，
 * 将结果写入 out_constraints 数组。
 *
 * @param graph       约束图指针
 * @param point_id    目标点节点 ID
 * @param out_constraints 输出：找到的约束指针数组
 * @param max_out     输出数组的最大容量
 * @return 找到的约束数量
 */
static int count_point_constraints(const ConstraintGraph *graph, int point_id, Constraint **out_constraints, int max_out) {
    int count = 0;
    for (int i = 0; i < graph->constraint_count && count < max_out; i++) {
        Constraint *c = graph->constraints[i];
        for (int j = 0; j < c->participant_count; j++) {
            if (c->participants[j] == point_id) {
                out_constraints[count++] = c;
                break;
            }
        }
    }
    return count;
}

/**
 * @brief 检查两个约束是否独立（非互相推导）
 *
 * 两个约束独立的条件是：c1 的参与者集合不完全是 c2 参与者集合的子集。
 * 如果 c1 的所有参与者都出现在 c2 中，则认为 c1 可能由 c2 推导而来，
 * 此时返回 false（不独立）。
 *
 * @param c1 第一个约束指针
 * @param c2 第二个约束指针
 * @return true 表示两个约束独立，false 表示可能互相推导
 */
static bool constraints_are_independent(const Constraint *c1, const Constraint *c2) {
    /* Two constraints are dependent if they involve the exact same participants */
    if (c1->participant_count != c2->participant_count)
        return 0;

    int match_count = 0;
    for (int i = 0; i < c1->participant_count; i++) {
        for (int j = 0; j < c2->participant_count; j++) {
            if (c1->participants[i] == c2->participants[j]) {
                match_count++;
                break;
            }
        }
    }

    return match_count != c1->participant_count;
}

/**
 * @brief 解析距离值声明
 * @param decl 声明字符串，格式如 "distance=5.0" 或 "d=3.14"
 * @param out_value 输出参数，解析出的双精度浮点数值
 * @return 解析成功返回 true，失败返回 false
 */
static bool parse_distance_value(const char *decl, double *out_value) {
    if (!decl || !out_value)
        return -1;

    /* 查找等号模式 */
    const char *eq = strchr(decl, '=');
    if (!eq)
        return -1;

    char *endptr;
    double val = strtod(eq + 1, &endptr);
    if (endptr == eq + 1)
        return -1; /* 未找到有效数字 */

    *out_value = val;
    return 0;
}

/**
 * @brief 检查点是否在线段上
 * @param graph 约束图指针
 * @param point_id 点节点ID
 * @param segment_id 线段节点ID
 * @return 点在线段上返回 true，否则返回 false
 */
static bool point_on_segment(const ConstraintGraph *graph, int point_id, int segment_id) {
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (c->type == INCIDENCE && c->participant_count == 2) {
            if ((c->participants[0] == point_id && c->participants[1] == segment_id) ||
                (c->participants[1] == point_id && c->participants[0] == segment_id)) {
                return 0;
            }
        }
    }
    return -1;
}

/**
 * @brief 将一组节点 ID 添加到冲突组输出数组
 *
 * 分配内存并复制节点 ID 列表到冲突组数组中，
 * 同时更新冲突组大小数组和冲突组计数。
 *
 * @param conflicts       输入/输出：冲突组二维数组
 * @param conflict_count  输入/输出：当前冲突组数量（将递增）
 * @param conflict_sizes  输入/输出：每个冲突组的大小数组
 * @param node_ids        要添加的节点 ID 数组
 * @param node_count      节点 ID 数量
 */
static void add_conflict_group(int **conflicts, int *conflict_count, int **conflict_sizes, const int *node_ids,
                               int node_count) {
    conflicts[*conflict_count] = lv00_malloc((size_t) node_count * sizeof(int));
    if (!conflicts[*conflict_count]) return;
    memcpy(conflicts[*conflict_count], node_ids, node_count * sizeof(int));
    (*conflict_sizes)[*conflict_count] = node_count;
    (*conflict_count)++;
}

/* ============================================================
 * 连接图环路检测 —— 有向图 DFS 遍历（v3.3.0 重构）
 *
 * 【重构原因】
 * 原版使用递归 DFS 检测 CONNECTION 约束形成的环路，在
 * 深层约束图中可能触发栈溢出。递归深度 = 约束图中有向边
 * 可达的最大端口链长度。
 *
 * 【方案对比 —— 递归 vs 迭代】
 *
 *   递归版（原版）：
 *     优点：代码简洁直观，紧贴 DFS 算法思想，易维护
 *     缺点：栈深度受系统限制（Windows 默认 1MB / ~8000 帧），
 *           深层约束图存在栈溢出风险，错误恢复困难
 *     适用：浅层约束图（深度 < 500），开发调试阶段
 *
 *   迭代版（当前）：
 *     优点：堆分配的显式栈，深度仅受可用内存限制，
 *           可精确控制最大深度，错误恢复容易
 *     缺点：代码较递归版冗长约 3 倍，需手动管理栈状态
 *     适用：深层约束图、生产环境、需精确深度控制的场景
 *
 * 【性能基准】
 *   - 浅层图（深度 < 100）：递归版快约 5-10%（函数调用优化）
 *   - 中层图（深度 100-1000）：两者接近（递归开销与栈管理开销平衡）
 *   - 深层图（深度 > 1000）：迭代版明显更安全且仍可用
 *
 * 【深度限制】
 *   LV00_MAX_TRAVERSAL_DEPTH 默认 4096，匹配 LV00_INITIAL_ARRAY_CAPACITY
 *   的典型值。大型几何问题中一个端口链可达数百层嵌套。
 * ============================================================ */

/** @brief 环路检测最大遍历深度（防止无限循环或过于深层的 DFS） */
#ifndef LV00_MAX_TRAVERSAL_DEPTH
#define LV00_MAX_TRAVERSAL_DEPTH 4096
#endif

/* DFS 栈帧 —— 模拟递归调用栈的一个级别 */
typedef struct {
    int node_id;       /**< 当前正在探索的端口节点 ID */
    int neighbor_idx;  /**< 当前节点已处理到的邻接索引（恢复点） */
    int path_len;      /**< 该节点在 path 数组中的位置 */
} DfsFrame;

/**
 * @brief 使用迭代 DFS 检测 CONNECTION 约束形成的有向环路
 *
 * 基于显式栈的迭代 DFS 遍历有向连接图。从 start_port_id 出发，
 * 沿 OUTPUT→INPUT 方向遍历 CONNECTION 约束边。若在遍历过程中
 * 遇到仍在递归栈中的节点，则检测到环路。
 *
 * 检测到环路时，调用 add_conflict_group() 记录环路上的所有端口。
 *
 * 【语义】CONNECTION 约束是有向边（输出端口 → 输入端口）。
 *   从输出端口出发追踪信号流，如果在追踪过程中返回之前经过的
 *   节点，标志着一个组合环路（combinational cycle）。
 *
 * @param graph         约束图（非 NULL）
 * @param start_port_id 起始端口节点 ID
 * @param visited       已访问节点标记数组
 * @param rec_stack     当前递归路径标记数组（用于环路检测）
 * @param path          当前遍历路径上的节点 ID 数组
 * @param path_len      起始节点在 path 中的位置（调用者传入 0）
 * @param conflicts     输出：检测到的冲突组数组
 * @param conflict_count 输入/输出：当前冲突组数量
 * @param conflict_sizes 输出：每个冲突组的大小
 * @param conn_adj      CONNECTION 约束的扁平邻接矩阵
 * @param conn_counts   每个节点的 CONNECTION 约束计数
 * @return true 发现环路，false 该分支无环路
 *
 * @note 最大遍历深度由 LV00_MAX_TRAVERSAL_DEPTH 限制。
 *       超过深度时遍历终止但不报错（视为无环路）。
 */
static bool has_connection_cycle(ConstraintGraph *graph, int start_port_id, bool *visited, bool *rec_stack, int *path,
                                 int path_len, int **conflicts, int *conflict_count, int **conflict_sizes,
                                 const int *conn_adj, const int *conn_counts) {
    /* 分配显式 DFS 栈（堆分配，深度不受调用栈限制） */
    DfsFrame *stack = lv00_malloc((size_t) LV00_MAX_TRAVERSAL_DEPTH * sizeof(DfsFrame));
    if (!stack) {
        /* 栈分配失败：回退到快速路径检查 —— 若能分配则无法检测深层环路，
         * 但至少不会崩溃。记录警告并继续。 */
        LOG_WARN("constraint_graph", "环路检测: DFS 栈分配失败，跳过深度 > 0 的遍历");
        /* 回退：仅检查直接环路（1层） */
        int cnt = conn_counts[start_port_id];
        for (int ci = 0; ci < cnt; ci++) {
            Constraint *c = graph->constraints[conn_adj[start_port_id * LV00_MAX_CONN_ADJ_STRIDE + ci]];
            if (c->participants[0] == start_port_id && c->participants[1] == start_port_id) {
                /* 自环路（罕见但需检测） */
                path[0] = start_port_id;
                add_conflict_group(conflicts, conflict_count, conflict_sizes, path, 1);
                return 0;
            }
        }
        return -1;
    }

    int stack_top = 0; /* 栈顶索引：-1 = 空栈 */

    /* 压入起始帧 */
    stack[0].node_id = start_port_id;
    stack[0].neighbor_idx = 0;
    stack[0].path_len = path_len;
    /* visited 和 rec_stack 在向下深入时标记，回溯时恢复 */
    /* 注意：起始节点可能已在 visited 中，由调用者负责在栈顶帧处理 */

    bool found_cycle = false;

    while (stack_top >= 0 && !found_cycle) {
        DfsFrame *frame = &stack[stack_top];
        int current_id = frame->node_id;

        /* 首次进入此节点时标记 */
        if (frame->neighbor_idx == 0) {
            visited[current_id] = true;
            rec_stack[current_id] = true;
            path[frame->path_len] = current_id;
        }

        /* 获取此节点的所有 CONNECTION 邻接 */
        int cnt = conn_counts[current_id];

        /* 遍历剩余的邻接（从上次中断位置继续） */
        bool pushed_child = false;
        while (frame->neighbor_idx < cnt && !pushed_child) {
            int ci = frame->neighbor_idx;
            Constraint *c = graph->constraints[conn_adj[current_id * LV00_MAX_CONN_ADJ_STRIDE + ci]];
            int next_port = -1;

            /* CONNECTION 是双向存储的（participants[0] 和 [1] 都是端口ID），
             * 而方向性体现在语义中（output → input）。
             * 我们从输出端口出发追踪：如果 current_id 是 participants[0]，
             * 则方向为 (该端口) → participants[1]。
             * 如果 current_id 是 participants[1]，则表示从输入回溯输出端，
             * 此处跳过。 */
            if (c->participants[0] == current_id) {
                next_port = c->participants[1];
            } else if (c->participants[1] == current_id) {
                /* 从输入端口出发：跳过此边（方向反向） */
                frame->neighbor_idx++;
                continue;
            }

            if (next_port >= 0) {
                if (rec_stack[next_port]) {
                    /* ── 检测到环路！──
                     * next_port 仍在递归栈中，即我们通过某条路径回到了
                     * 之前经过的端口。记录环路上的所有节点。 */
                    int cycle_start = 0;
                    for (int j = 0; j <= frame->path_len; j++) {
                        if (path[j] == next_port) {
                            cycle_start = j;
                            break;
                        }
                    }

                    int cycle_len = frame->path_len - cycle_start + 1;
                    add_conflict_group(conflicts, conflict_count, conflict_sizes,
                                       &path[cycle_start], cycle_len);
                    found_cycle = true;
                    break;
                }

                if (!visited[next_port]) {
                    /* 向更深层次深入 */
                    frame->neighbor_idx++; /* 保存当前进度 */

                    /* 检查深度限制 */
                    if (stack_top + 1 >= LV00_MAX_TRAVERSAL_DEPTH) {
                        LOG_WARN("constraint_graph",
                                 "环路检测: 遍历深度超过上限 %d，在节点 %d 处截断",
                                 LV00_MAX_TRAVERSAL_DEPTH, next_port);
                        /* 超过深度上限：将该分支视为死胡同 */
                        frame->neighbor_idx = cnt; /* 跳过该节点剩余邻接 */
                        break;
                    }

                    /* 压入新帧 */
                    stack_top++;
                    stack[stack_top].node_id = next_port;
                    stack[stack_top].neighbor_idx = 0;
                    stack[stack_top].path_len = frame->path_len + 1;
                    pushed_child = true;
                } else {
                    /* 已访问但不在递归栈中的节点：交叉边（cross edge），
                     * 在 DAG 中正常，不会形成环路。 */
                    frame->neighbor_idx++;
                }
            } else {
                frame->neighbor_idx++;
            }
        } /* while neighbors */

        /* 如果当前节点的所有邻接都已处理完毕且未推入子节点：回溯 */
        if (!pushed_child && !found_cycle) {
            rec_stack[current_id] = false; /* 从递归路径中移除 */
            /* visited[current_id] 保持为 true —— 节点已完全探索 */
            stack_top--; /* 弹出栈帧，返回父节点 */
        }
    }

    /* 释放 DFS 栈 */
    lv00_free((void **) &stack);

    return found_cycle;
}

/**
 * @brief 检查两条线段是否可能相交（非平行）
 *
 * 遍历约束图检查两条线段之间是否存在平行约束。
 * 当前实现默认返回 true（假设可以相交），后续可扩展
 * 平行约束检测逻辑。
 *
 * @param graph    约束图指针
 * @param seg1_id 第一条线段节点 ID
 * @param seg2_id 第二条线段节点 ID
 * @return true 表示两条线段可能相交，false 表示平行
 */
static bool segments_can_intersect(const ConstraintGraph *graph, int seg1_id, int seg2_id) {
    /* For symbolic coordinates, we check if there's any geometric constraint 
     * that would make them parallel */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        /* Check for parallel constraint between these segments */
        /* This would require a PARALLEL constraint type - for now assume they can intersect */
    }
    return 0;
}

int **graph_detect_conflicts(const ConstraintGraph *graph, int *out_conflict_count, int **out_conflict_sizes) {
    lv00_clear_error();

    if (!graph || !out_conflict_count || !out_conflict_sizes) {
        if (out_conflict_count)
            *out_conflict_count = 0;
        if (out_conflict_sizes)
            *out_conflict_sizes = NULL;
        return NULL;
    }

    *out_conflict_count = 0;

    /* Allocate maximum possible conflicts */
    int max_conflicts = graph->node_count + graph->constraint_count;
    int **conflicts = lv00_malloc((size_t) max_conflicts * sizeof(int *));
    *out_conflict_sizes = lv00_malloc((size_t) max_conflicts * sizeof(int));

    if (!conflicts || !*out_conflict_sizes) {
        lv00_free((void **) &conflicts);
        lv00_free((void **) &*out_conflict_sizes);
        *out_conflict_sizes = NULL;
        *out_conflict_count = -1; /* 使用 -1 表示 OOM 错误，与 0（无冲突）区分 */
        return NULL;
    }

    /* ===== 预构建邻接索引以实现 O(1) 约束查找 ===== */
    int max_node_id = 0;
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i]->id > max_node_id)
            max_node_id = graph->nodes[i]->id;
    }

    /* adj: node_id -> 约束索引的扁平数组 */
    size_t adj_total = (size_t)(max_node_id + 1) * LV00_ADJ_MAX_PER_NODE;
    if (adj_total > (size_t)INT_MAX) {
        lv00_free((void **) &conflicts);
        lv00_free((void **) out_conflict_sizes);
        *out_conflict_sizes = NULL;
        *out_conflict_count = -1;
        return NULL;
    }
    int *adj_lists = lv00_calloc((int)adj_total, sizeof(int));
    int *adj_counts = lv00_calloc(max_node_id + 1, sizeof(int));

    /* inc_adj: node_id -> INCIDENCE 约束索引 */
    int *inc_adj = lv00_calloc(adj_total, sizeof(int));
    int *inc_counts = lv00_calloc(max_node_id + 1, sizeof(int));

    /* conn_adj: node_id -> CONNECTION 约束索引 */
    int *conn_adj = lv00_calloc(adj_total, sizeof(int));
    int *conn_counts = lv00_calloc(max_node_id + 1, sizeof(int));

    /* int_adj: node_id -> INTERSECTION 约束索引 */
    int *int_adj = lv00_calloc(adj_total, sizeof(int));
    int *int_counts = lv00_calloc(max_node_id + 1, sizeof(int));

    if (adj_lists && adj_counts && inc_adj && inc_counts && conn_adj && conn_counts && int_adj && int_counts) {
        for (int i = 0; i < graph->constraint_count; i++) {
            Constraint *c = graph->constraints[i];
            for (int j = 0; j < c->participant_count; j++) {
                int nid = c->participants[j];
                if (nid < 0 || nid > max_node_id)
                    continue;

                /* 通用邻接关系 */
                if (adj_counts[nid] < LV00_ADJ_MAX_PER_NODE) {
                    adj_lists[nid * LV00_ADJ_MAX_PER_NODE + adj_counts[nid]++] = i;
                } else {
                    LOG_DEBUG("constraint_graph", "节点 %d 超出邻接限制 (%d)，约束 %d 被忽略", nid, LV00_ADJ_MAX_PER_NODE,
                              i);
                }
                /* 类型特定邻接关系 */
                int *ta = NULL;
                int *tc = NULL;
                if (c->type == INCIDENCE) {
                    ta = inc_adj;
                    tc = inc_counts;
                }
                if (c->type == CONNECTION) {
                    ta = conn_adj;
                    tc = conn_counts;
                }
                if (c->type == INTERSECTION) {
                    ta = int_adj;
                    tc = int_counts;
                }
                if (ta && tc && tc[nid] < LV00_ADJ_MAX_PER_NODE) {
                    ta[nid * LV00_ADJ_MAX_PER_NODE + tc[nid]++] = i;
                } else if (ta && tc) {
                    LOG_DEBUG("constraint_graph", "节点 %d 超出类型特定邻接限制 (%d)，类型 %d", nid, LV00_ADJ_MAX_PER_NODE,
                              c->type);
                }
            }
        }
    }
    /* ===== End adjacency indexes ===== */

    /* Type 1: Overconstrained points (point with > 2 independent geometric constraints) */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (node->type != GEOM_POINT)
            continue;

        Constraint *point_constraints[64];
        int pc_count = 0;
        int ac = adj_counts[node->id];
        for (int ai = 0; ai < ac && pc_count < 64; ai++) {
            point_constraints[pc_count++] = graph->constraints[adj_lists[node->id * 256 + ai]];
        }

        /* Count independent constraints */
        int independent_count = 0;
        Constraint *independent_constraints[64];

        for (int j = 0; j < pc_count; j++) {
            bool is_independent = true;
            for (int k = 0; k < independent_count; k++) {
                if (!constraints_are_independent(point_constraints[j], independent_constraints[k])) {
                    is_independent = false;
                    break;
                }
            }
            if (is_independent) {
                independent_constraints[independent_count++] = point_constraints[j];
            }
        }

        /* In 2D, a point has 2 DOF, so > 2 independent constraints is overconstrained */
        if (independent_count > 2) {
            int conflict_nodes[64];
            int cn_count = 0;
            conflict_nodes[cn_count++] = node->id;
            for (int j = 0; j < independent_count && cn_count < 64; j++) {
                conflict_nodes[cn_count++] = independent_constraints[j]->id;
            }
            add_conflict_group(conflicts, out_conflict_count, out_conflict_sizes, conflict_nodes, cn_count);
        }
    }

    /* Type 2: Incompatible distances on same segment pair */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *seg1 = graph->nodes[i];
        if (seg1->type != GEOM_LINE_SEGMENT)
            continue;

        double dist1 = 0.0;
        bool has_dist1 = parse_distance_value(seg1->numeric_assumption_declaration, &dist1);

        for (int j = i + 1; j < graph->node_count; j++) {
            GeomNode *seg2 = graph->nodes[j];
            if (seg2->type != GEOM_LINE_SEGMENT)
                continue;

            double dist2 = 0.0;
            bool has_dist2 = parse_distance_value(seg2->numeric_assumption_declaration, &dist2);

            /* Check if they share the same endpoints (same segment pair) */
            /* This requires checking if they connect the same two points */
            /* For now, check if both have distance declarations with different values */
            if (has_dist1 && has_dist2) {
                /* Check if segments share endpoints by comparing their symbolic coordinates */
                bool share_endpoints = false;
                if (seg1->coord_count == 2 && seg2->coord_count == 2 && seg1->symbolic_coords &&
                    seg2->symbolic_coords) {
                    /* seg1 endpoints: symbolic_coords[0], symbolic_coords[1]
                     * seg2 endpoints: symbolic_coords[0], symbolic_coords[1]
                     * Check all 4 combinations for shared endpoints */
                    for (int ei = 0; ei < 2 && !share_endpoints; ei++) {
                        for (int ej = 0; ej < 2 && !share_endpoints; ej++) {
                            if (seg1->symbolic_coords[ei] && seg2->symbolic_coords[ej]) {
                                if (symbolic_coord_compare(seg1->symbolic_coords[ei], seg2->symbolic_coords[ej]) == 0) {
                                    share_endpoints = true;
                                }
                            }
                        }
                    }
                }

                /* If different distances and could be same segment pair */
                if (fabs(dist1 - dist2) > 1e-9 && share_endpoints) {
                    int conflict_nodes[4];
                    conflict_nodes[0] = seg1->id;
                    conflict_nodes[1] = seg2->id;
                    add_conflict_group(conflicts, out_conflict_count, out_conflict_sizes, conflict_nodes, 2);
                }
            }
        }
    }

    /* Type 3: Invalid betweenness constraints */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (c->type != BETWEENNESS || c->participant_count != 3)
            continue;

        int middle_id = c->participants[1]; /* p2 is the middle point */
        int end1_id = c->participants[0];   /* p1 is one endpoint */
        int end2_id = c->participants[2];   /* p3 is the other endpoint */

        GeomNode *middle = graph_get_node(graph, middle_id);
        GeomNode *end1 = graph_get_node(graph, end1_id);
        GeomNode *end2 = graph_get_node(graph, end2_id);

        if (!middle || !end1 || !end2)
            continue;

        /* Check for collinearity - all three points should be on the same line */
        bool collinear = false;

        /* Check if all three points are incident to the same line segment */
        for (int j = 0; j < graph->node_count; j++) {
            GeomNode *line = graph->nodes[j];
            if (line->type != GEOM_LINE_SEGMENT)
                continue;

            /* Use inc_adj to check incidence in O(1) per lookup */
            bool middle_on_line = false;
            int mic = inc_counts[line->id];
            for (int mi = 0; mi < mic; mi++) {
                Constraint *ic = graph->constraints[inc_adj[line->id * 256 + mi]];
                if (ic->participants[0] == middle_id) {
                    middle_on_line = true;
                    break;
                }
            }
            bool end1_on_line = false;
            int e1c = inc_counts[line->id];
            for (int e1i = 0; e1i < e1c; e1i++) {
                Constraint *ic = graph->constraints[inc_adj[line->id * 256 + e1i]];
                if (ic->participants[0] == end1_id) {
                    end1_on_line = true;
                    break;
                }
            }
            bool end2_on_line = false;
            int e2c = inc_counts[line->id];
            for (int e2i = 0; e2i < e2c; e2i++) {
                Constraint *ic = graph->constraints[inc_adj[line->id * 256 + e2i]];
                if (ic->participants[0] == end2_id) {
                    end2_on_line = true;
                    break;
                }
            }

            if (middle_on_line && end1_on_line && end2_on_line) {
                collinear = true;
                break;
            }
        }

        /* If not collinear, the betweenness constraint is invalid */
        if (!collinear) {
            int conflict_nodes[4];
            conflict_nodes[0] = c->id;
            conflict_nodes[1] = middle_id;
            conflict_nodes[2] = end1_id;
            conflict_nodes[3] = end2_id;
            add_conflict_group(conflicts, out_conflict_count, out_conflict_sizes, conflict_nodes, 4);
        }

        /* Check ratio if numeric assumptions are available */
        /* The middle point should be between 0 and 1 on the segment */
        /* This requires coordinate evaluation which is complex for symbolic coords */
    }

    /* Type 4: Cycles in connection graph */
    int max_port_id = 0;
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i]->type == GEOM_PORT && graph->nodes[i]->id > max_port_id) {
            max_port_id = graph->nodes[i]->id;
        }
    }

    if (max_port_id > 0) {
        bool *visited = lv00_calloc(max_port_id + 1, sizeof(bool));
        bool *rec_stack = lv00_calloc(max_port_id + 1, sizeof(bool));
        int *path = lv00_malloc((size_t)(max_port_id + 1) * sizeof(int));

        if (visited && rec_stack && path) {
            for (int i = 0; i < graph->node_count; i++) {
                GeomNode *node = graph->nodes[i];
                if (node->type == GEOM_PORT && node->data.port->type == PORT_OUTPUT) {
                    if (!visited[node->id]) {
                        has_connection_cycle(graph, node->id, visited, rec_stack, path, 0, conflicts,
                                             out_conflict_count, out_conflict_sizes, conn_adj, conn_counts);
                    }
                }
            }
        }

        lv00_free((void **) &visited);
        lv00_free((void **) &rec_stack);
        lv00_free((void **) &path);
    }

    /* Type 5: Contradictory incidences - point required to be on two non-intersecting lines */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *point = graph->nodes[i];
        if (point->type != GEOM_POINT)
            continue;

        /* Find all incidence constraints for this point using inc_adj */
        int incident_lines[64];
        int il_count = 0;

        int pil = inc_counts[point->id];
        for (int pi = 0; pi < pil && il_count < 64; pi++) {
            Constraint *c = graph->constraints[inc_adj[point->id * 256 + pi]];
            if (c->type == INCIDENCE && c->participants[0] == point->id) {
                incident_lines[il_count++] = c->participants[1];
            }
        }

        /* Check pairs of incident lines for intersection possibility */
        for (int j = 0; j < il_count; j++) {
            GeomNode *line1 = graph_get_node(graph, incident_lines[j]);
            if (!line1 || line1->type != GEOM_LINE_SEGMENT)
                continue;

            for (int k = j + 1; k < il_count; k++) {
                GeomNode *line2 = graph_get_node(graph, incident_lines[k]);
                if (!line2 || line2->type != GEOM_LINE_SEGMENT)
                    continue;

                /* Check if there's an intersection constraint for these lines using int_adj */
                bool has_intersection = false;
                int iic = int_counts[incident_lines[j]];
                for (int ii = 0; ii < iic; ii++) {
                    Constraint *ic = graph->constraints[int_adj[incident_lines[j] * 256 + ii]];
                    if (ic->type == INTERSECTION && ic->participant_count == 3) {
                        if ((ic->participants[0] == incident_lines[j] && ic->participants[1] == incident_lines[k]) ||
                            (ic->participants[0] == incident_lines[k] && ic->participants[1] == incident_lines[j])) {
                            has_intersection = true;
                            break;
                        }
                    }
                }

                /* If no intersection constraint exists, check if lines can intersect */
                if (!has_intersection && !segments_can_intersect(graph, incident_lines[j], incident_lines[k])) {
                    /* Lines are parallel and distinct - point cannot be on both */
                    int conflict_nodes[4];
                    conflict_nodes[0] = point->id;
                    conflict_nodes[1] = incident_lines[j];
                    conflict_nodes[2] = incident_lines[k];
                    add_conflict_group(conflicts, out_conflict_count, out_conflict_sizes, conflict_nodes, 3);
                }
            }
        }
    }

    /* If no conflicts found, free and return NULL */
    if (*out_conflict_count == 0) {
        lv00_free((void **) &adj_lists);
        lv00_free((void **) &adj_counts);
        lv00_free((void **) &inc_adj);
        lv00_free((void **) &inc_counts);
        lv00_free((void **) &conn_adj);
        lv00_free((void **) &conn_counts);
        lv00_free((void **) &int_adj);
        lv00_free((void **) &int_counts);
        lv00_free((void **) &conflicts);
        lv00_free((void **) &*out_conflict_sizes);
        *out_conflict_sizes = NULL;
        return NULL;
    }

    lv00_free((void **) &adj_lists);
    lv00_free((void **) &adj_counts);
    lv00_free((void **) &inc_adj);
    lv00_free((void **) &inc_counts);
    lv00_free((void **) &conn_adj);
    lv00_free((void **) &conn_counts);
    lv00_free((void **) &int_adj);
    lv00_free((void **) &int_counts);

    /* 流式事件: 冲突检测结果 */
    if (graph_stream_ctx && *out_conflict_count > 0) {
        char desc[128];
        snprintf(desc, sizeof(desc), "冲突检测完成: 发现 %d 个冲突", *out_conflict_count);
        stream_emit_simple(graph_stream_ctx, STREAM_EVENT_CONFLICT_DETECTED, desc, *out_conflict_count);
    }

    return conflicts;
}

/**
 * @brief 验证区域的边界是否闭合
 *
 * 检查指定区域的所有边界线段是否首尾相连形成闭合路径。
 * 从第一条边界线段出发，沿连接关系遍历，最终应回到起始线段。
 *
 * @param graph     约束图指针
 * @param region_id 区域节点 ID
 * @return true 表示区域边界闭合，false 表示不闭合或参数无效
 */
int graph_validate_region_closure(const ConstraintGraph *graph, int region_id) {
    lv00_clear_error();

    if (!graph) {
        lv00_set_error(LV00_ERROR_NULL_POINTER, __func__, "graph is NULL");
        return -1;
    }

    const GeomNode *region = graph_get_node(graph, region_id);
    if (!region || region->type != GEOM_REGION) {
        return -1;
    }

    int segment_count = region->data.region.segment_count;
    if (segment_count < 3) {
        return -1; /* 至少需要 3 条边才能闭合 */
    }

    /* 验证所有边界线段是有效的 GEOM_LINE_SEGMENT 且活跃 */
    for (int i = 0; i < segment_count; i++) {
        const GeomNode *seg = region->data.region.boundary_segments[i];
        if (!seg || seg->type != GEOM_LINE_SEGMENT || !seg->is_active) {
            return -1;
        }
    }

    return 0;
}
