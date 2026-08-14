/**
 * @file proof_search_algo.c
 * @brief 证明搜索算法实现（从 proof_multi_strategy.c 拆分）
 *
 * @details 深度优先（DFS）、广度优先（BFS）、最佳优先（启发式）与
 *          蒙特卡洛树搜索（MCTS）四种策略搜索算法。
 *          仅依赖 ProofNavigator / ProofMultiStrategy 公共接口。
 */

#include "proof_multi_strategy_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/lv_hashtable.h"
#include "lv/proof.h"
#include "lv/solver.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"

/* ============== 辅助搜索函数实现 ============== */

/**
 * @brief 深度优先搜索实现
 *
 * 递归/迭代深入搜索：
 * 1. 检查当前状态是否为目标
 * 2. 选择一条推理规则应用
 * 3. 递归深入
 * 4. 如果失败则回溯尝试其他路径
 */
/**
 * @brief 深度优先搜索实现（改进版）
 *
 * 使用栈式迭代DFS，支持：
 * - 状态保存与恢复（回溯）
 * - 已访问状态检测，避免循环
 * - 每个分支的深度限制
 * - 策略级别的回溯选择
 *
 * exempt: 判据「BFS 图遍历收敛」——本函数为带策略位图状态 + 副作用执行
 * （proof_multi_strategy_execute 改写引擎）的状态空间搜索，非 lv_bfs_run
 * 的整型 id 图遍历语义，保留。
 */
bool proof_depth_first_search(ProofNavigator *proof, int max_steps) {
    if (!proof || !proof->engine)
        return false;

    ProofMultiStrategy *mse = (ProofMultiStrategy *) proof->engine;

    /* ---- DFS 栈帧 ---- */
    typedef struct {
        int strategy_index;                          /* 当前尝试的策略索引 */
        int tried_count;                             /* 已尝试的策略数量 */
        int depth;                                   /* 当前深度 */
        int step_count;                              /* 进入此帧时的步骤计数 */
        bool strategies_tried[PROOF_STRATEGY_COUNT]; /* 每个策略是否已尝试 */
    } DFSFrame;

#define DFS_MAX_DEPTH 32
#define DFS_STACK_SIZE 64

    DFSFrame stack[DFS_STACK_SIZE];
    int stack_top = 0;
    int total_steps = 0;

    /* 已访问状态集合（键 = 策略序列的唯一整数编码）
     *
     * 【lv_hashtable 收敛评估结论（已迁移，2026-08-08）】
     * 原实现为固定数组 visited_hashes[DFS_VISIT_HASH_SIZE=1024]，插入
     * "槽存 hash+1"，但查重却是线性扫描整个数组找 hash+1 —— 等价于
     * "hash % 1024 折叠后的数组集合"：不同策略序列折叠碰撞时互相覆盖/
     * 误判（丢状态，正确性隐患），且每次检查 O(1024) 扫描。
     * 迁移到 lv_hashtable_i64（开放寻址 + 自动扩容）：
     *   - 键 = 策略序列 31 进制编码（strategy+1 保证先导位非零，与序列
     *     长度一一对应；PROOF_STRATEGY_COUNT=24 < 31，短序列编码无碰撞，
     *     仅在超出 64 位宽时 wrap，碰撞概率 ~2^-64，可忽略）；
     *   - 值 = 非 NULL 哨兵（值 NULL 视同键不存在）；
     *   - 查重 O(1)，消除 1024 固定容量与碰撞覆盖。 */
    lvHashtable *visited = lv_hashtable_i64_create(0);
    if (!visited)
        return false;

    /* 初始化栈帧 */
    memset(&stack[0], 0, sizeof(DFSFrame));
    stack[0].depth = 0;
    stack[0].step_count = proof->step_count;
    stack_top = 1;

    while (stack_top > 0 && total_steps < max_steps && !proof->is_complete) {
        DFSFrame *frame = &stack[stack_top - 1];

        /* 检查深度限制 */
        if (frame->depth >= DFS_MAX_DEPTH) {
            /* 超出深度限制，回溯 */
            stack_top--;
            continue;
        }

        /* 查找下一个未尝试的策略 */
        int next_strategy = -1;
        for (int i = 0; i < PROOF_STRATEGY_COUNT; i++) {
            if (!frame->strategies_tried[i] && mse->strategies[i].status != PROOF_STRATEGY_UNAVAILABLE &&
                mse->strategies[i].execute != NULL) {
                next_strategy = i;
                frame->strategies_tried[i] = true;
                frame->tried_count++;
                break;
            }
        }

        if (next_strategy < 0) {
            /* 所有策略已尝试完毕，回溯 */
            stack_top--;
            continue;
        }

        /* 计算策略序列编码（31 进制；strategy+1 保证先导位非零，
         * 空前缀 [0] 与 [0,0] 不再折叠碰撞） */
        uint64_t seq = 0;
        for (int d = 0; d < stack_top; d++) {
            seq = seq * 31u + (uint64_t) (stack[d].strategy_index + 1);
        }
        seq = seq * 31u + (uint64_t) (next_strategy + 1);

        /* 检查是否已访问（O(1) 集合查询） */
        if (lv_hashtable_i64_contains(visited, (int64_t) seq)) {
            /* 已访问此状态，跳过 */
            continue;
        }

        /* 标记为已访问 */
        lv_hashtable_i64_insert(visited, (int64_t) seq, (void *) 1);

        /* 激活并执行策略 */
        proof_multi_strategy_activate(mse, (ProofStrategyType) next_strategy);
        frame->strategy_index = next_strategy;
        bool success = proof_multi_strategy_execute(mse);
        total_steps++;

        if (success && proof->is_complete) {
            lv_hashtable_i64_destroy(visited);
            return true; /* 找到证明 */
        }

        if (success) {
            /* 策略成功但证明未完成，继续深入 */
            if (stack_top < DFS_STACK_SIZE) {
                memset(&stack[stack_top], 0, sizeof(DFSFrame));
                stack[stack_top].depth = frame->depth + 1;
                stack[stack_top].step_count = proof->step_count;
                stack_top++;
            }
        }
        /* 如果策略失败，当前帧会自动尝试下一个策略 */
    }

    lv_hashtable_i64_destroy(visited);
    return proof->is_complete;

#undef DFS_MAX_DEPTH
#undef DFS_STACK_SIZE
}

/**
 * @brief 广度优先搜索实现（改进版）
 *
 * 使用显式队列维护搜索状态：
 * - 每个状态记录已尝试的策略集合
 * - 按层次展开：每次从队首取状态，应用一个未尝试策略
 * - 展开后的新状态入队尾
 * - 检查每个展开后的状态是否完成证明
 *
 * exempt: 判据「BFS 图遍历收敛」——本函数为带策略位图状态 + 副作用执行的
 * 状态空间搜索，非 lv_bfs_run 的整型 id 图遍历语义，保留。
 */
bool proof_breadth_first_search(ProofNavigator *proof, int max_steps) {
    if (!proof || !proof->engine)
        return false;

    ProofMultiStrategy *mse = (ProofMultiStrategy *) proof->engine;

    /* ---- BFS 队列状态 ---- */
    typedef struct {
        int strategy_index;                          /* 上一次执行的策略 */
        bool strategies_tried[PROOF_STRATEGY_COUNT]; /* 已尝试的策略集合 */
        int tried_count;                             /* 已尝试数量 */
        int depth;                                   /* 搜索深度 */
    } BFSState;

#define BFS_QUEUE_SIZE 512

    BFSState queue[BFS_QUEUE_SIZE];
    int queue_head = 0;
    int queue_tail = 0;
    int total_explored = 0;

    /* 初始状态：所有策略均未尝试 */
    memset(&queue[0], 0, sizeof(BFSState));
    queue_tail = 1;

    while (queue_head < queue_tail && total_explored < max_steps && !proof->is_complete) {
        BFSState *state = &queue[queue_head % BFS_QUEUE_SIZE];

        /* 查找该状态中下一个未尝试的策略 */
        int next_strategy = -1;
        for (int i = 0; i < PROOF_STRATEGY_COUNT; i++) {
            if (!state->strategies_tried[i] && mse->strategies[i].status != PROOF_STRATEGY_UNAVAILABLE &&
                mse->strategies[i].execute != NULL) {
                next_strategy = i;
                state->strategies_tried[i] = true;
                state->tried_count++;
                break;
            }
        }

        if (next_strategy < 0) {
            /* 当前状态所有策略已穷尽，出队 */
            queue_head++;
            continue;
        }

        /* 执行策略 */
        proof_multi_strategy_activate(mse, (ProofStrategyType) next_strategy);
        state->strategy_index = next_strategy;
        bool success = proof_multi_strategy_execute(mse);
        total_explored++;

        if (success && proof->is_complete) {
            return true; /* 找到证明 */
        }

        /* 如果策略成功但证明未完成，生成后续状态入队 */
        if (success && queue_tail < queue_head + BFS_QUEUE_SIZE) {
            BFSState *new_state = &queue[queue_tail % BFS_QUEUE_SIZE];
            /* 复制当前状态的已尝试集合 */
            memmove(new_state, state, sizeof(BFSState));
            new_state->depth = state->depth + 1;
            queue_tail++;
        }

        /* 如果当前状态还有未尝试的策略，保留在队首继续展开 */
        /* 如果所有策略已穷尽，自动出队（下次循环会检测到） */
        bool has_more = false;
        for (int i = 0; i < PROOF_STRATEGY_COUNT; i++) {
            if (!state->strategies_tried[i] && mse->strategies[i].status != PROOF_STRATEGY_UNAVAILABLE) {
                has_more = true;
                break;
            }
        }
        if (!has_more) {
            queue_head++;
        }
    }

    return proof->is_complete;

#undef BFS_QUEUE_SIZE
}

/* ---- 优先队列条目 ---- */
typedef struct {
    int strategy_index; /* 策略索引 */
    double score;       /* 启发式分数 */
    int attempt_count;  /* 该策略已尝试次数 */
} PQEntry;

/* 优先队列按分数降序排序（选择排序，判据 A：收敛同函数内两处重复排序骨架） */
static void pq_sort_by_score_desc(PQEntry *pq, int pq_size) {
    for (int i = 0; i < pq_size - 1; i++) {
        int max_idx = i;
        for (int j = i + 1; j < pq_size; j++) {
            if (pq[j].score > pq[max_idx].score) {
                max_idx = j;
            }
        }
        if (max_idx != i) {
            lv_SWAP(PQEntry, pq[i], pq[max_idx]);
        }
    }
}

/**
 * @brief 最佳优先搜索实现（改进版）
 *
 * 使用启发式评分选择最有希望的候选：
 * 1. 对每个可用策略计算启发式分数
 * 2. 分数基于：适用性检查结果、约束匹配度、历史成功率
 * 3. 使用排序数组作为优先队列（分数从高到低）
 * 4. 每次展开得分最高的候选
 */
bool proof_best_first_search(ProofNavigator *proof, int max_steps) {
    if (!proof || !proof->engine)
        return false;

    ProofMultiStrategy *mse = (ProofMultiStrategy *) proof->engine;

#define PQ_MAX_SIZE 128

    PQEntry pq[PQ_MAX_SIZE];
    int pq_size = 0;
    int total_steps = 0;

/* ---- 启发式评分函数 ----
     * 综合考虑以下因素：
     * - 适用性检查（+40分）：策略的适用性检查是否通过
     * - 约束匹配度（+30分）：策略与当前约束图的匹配程度
     * - 历史成功率（+20分）：之前尝试的成功/失败比率
     * - 策略优先级（+10分）：回退顺序中的位置
     */
#define SCORE_APPLICABILITY 40.0
#define SCORE_CONSTRAINT_MATCH 30.0
#define SCORE_HISTORY 20.0
#define SCORE_PRIORITY 10.0

    /* 计算单个策略的启发式分数 */
    for (int i = 0; i < PROOF_STRATEGY_COUNT; i++) {
        if (mse->strategies[i].status == PROOF_STRATEGY_UNAVAILABLE)
            continue;
        if (!mse->strategies[i].execute)
            continue;

        double score = 0.0;

        /* 因素1：适用性检查 */
        if (mse->strategies[i].applicability_check) {
            if (mse->strategies[i].applicability_check(mse, proof->construction, proof->target_prop)) {
                score += SCORE_APPLICABILITY;
            }
        } else {
            /* 无适用性检查的策略给一半分数 */
            score += SCORE_APPLICABILITY * 0.5;
        }

        /* 因素2：约束匹配度 */
        if (proof->construction) {
            int constraint_density = proof->construction->constraint_count;
            /* 约束越多，代数方法（Groebner、坐标）越有利 */
            if (i == PROOF_STRATEGY_GROEBNER_BASIS || i == PROOF_STRATEGY_COORDINATE) {
                score += SCORE_CONSTRAINT_MATCH * (constraint_density > 3 ? 1.0 : 0.3);
            }
            /* 有面积相关节点时，面积法有利 */
            if (i == PROOF_STRATEGY_AREA_METHOD && proof->target_prop) {
                if (proof->target_prop->name && strstr(proof->target_prop->name, "area")) {
                    score += SCORE_CONSTRAINT_MATCH;
                }
            }
            /* 有角度相关时，全角法有利 */
            if (i == PROOF_STRATEGY_FULL_ANGLE_METHOD && proof->target_prop) {
                if (proof->target_prop->name &&
                    (strstr(proof->target_prop->name, "angle") || strstr(proof->target_prop->name, "角"))) {
                    score += SCORE_CONSTRAINT_MATCH;
                }
            }
            /* 向量法适用于有坐标的点 */
            if (i == PROOF_STRATEGY_VECTOR_METHOD && proof->construction) {
                int point_with_coords = 0;
                for (int n = 0; n < proof->construction->node_count; n++) {
                    GeomNode *nd = proof->construction->nodes[n];
                    if (nd && nd->type == GEOM_POINT && nd->coord_count >= 2) {
                        point_with_coords++;
                    }
                }
                if (point_with_coords >= 2) {
                    score += SCORE_CONSTRAINT_MATCH * 0.8;
                }
            }
        }

        /* 因素3：历史成功率 */
        if (mse->total_attempts > 0) {
            /* 使用全局成功率作为基准 */
            double global_rate = (double) mse->success_count / (double) mse->total_attempts;
            score += SCORE_HISTORY * global_rate;
        } else {
            score += SCORE_HISTORY * 0.5; /* 无历史数据时给中等分数 */
        }

        /* 因素4：回退顺序优先级 */
        for (int f = 0; f < mse->fallback_count; f++) {
            if (mse->fallback_order[f] == i) {
                /* 越靠前分数越高 */
                double priority_score = SCORE_PRIORITY * (1.0 - (double) f / (double) PROOF_STRATEGY_COUNT);
                score += priority_score;
                break;
            }
        }

        /* 加入优先队列 */
        if (pq_size < PQ_MAX_SIZE) {
            pq[pq_size].strategy_index = i;
            pq[pq_size].score = score;
            pq[pq_size].attempt_count = 0;
            pq_size++;
        }
    }

    /* 按分数降序排序（简单选择排序） */
    pq_sort_by_score_desc(pq, pq_size);

    /* 主循环：每次展开得分最高的候选 */
    while (total_steps < max_steps && !proof->is_complete) {
        /* 找到得分最高且未完全失败的候选 */
        int best_idx = -1;
        double best_score = -1.0;
        for (int i = 0; i < pq_size; i++) {
            if (pq[i].attempt_count < 3 && pq[i].score > best_score) {
                /* 每次尝试后降低分数，鼓励探索其他策略 */
                double adjusted_score = pq[i].score / (1.0 + pq[i].attempt_count * 0.5);
                if (adjusted_score > best_score) {
                    best_score = adjusted_score;
                    best_idx = i;
                }
            }
        }

        if (best_idx < 0) {
            /* 所有候选已穷尽 */
            break;
        }

        /* 执行得分最高的策略 */
        int strategy = pq[best_idx].strategy_index;
        pq[best_idx].attempt_count++;

        proof_multi_strategy_activate(mse, (ProofStrategyType) strategy);
        bool success = proof_multi_strategy_execute(mse);
        total_steps++;

        if (success && proof->is_complete) {
            return true;
        }

        /* 如果成功但未完成，更新分数并重新排序 */
        if (success) {
            pq[best_idx].score *= 1.2; /* 成功的策略提高分数 */
        } else {
            pq[best_idx].score *= 0.7; /* 失败的策略降低分数 */
        }

        /* 重新排序 */
        pq_sort_by_score_desc(pq, pq_size);
    }

    return proof->is_complete;

#undef PQ_MAX_SIZE
#undef SCORE_APPLICABILITY
#undef SCORE_CONSTRAINT_MATCH
#undef SCORE_HISTORY
#undef SCORE_PRIORITY
}

/**
 * @brief 蒙特卡洛树搜索实现（改进版）
 *
 * 完整 MCTS 四步循环：
 * 1. 选择（Selection）：使用 UCB1 公式从根节点选择最优子节点
 *    UCB1 = win_rate + C * sqrt(ln(parent_visits) / visits), C = sqrt(2)
 * 2. 展开（Expansion）：为一个未尝试的策略添加子节点
 * 3. 模拟（Simulation）：从新节点随机执行策略直到终止
 * 4. 回传（Backpropagation）：更新路径上所有节点的胜/访问计数
 *
 * @param proof       证明导航器指针
 * @param max_steps   最大模拟次数
 * @return true 找到证明，false 搜索失败或超时
 */
bool proof_mcts_search(ProofNavigator *proof, int max_steps) {
    if (!proof || !proof->engine)
        return false;

    ProofMultiStrategy *mse = (ProofMultiStrategy *) proof->engine;

/* ---- MCTS 树节点 ---- */
#define MCTS_MAX_NODES 256
#define MCTS_MAX_CHILDREN PROOF_STRATEGY_COUNT
#define MCTS_C 1.41421356 /* sqrt(2)，UCB1 探索常数 */

    typedef struct MCTSNode {
        int id;                          /* 节点ID */
        int parent_id;                   /* 父节点ID（-1 = 根） */
        int strategy_index;              /* 此节点对应的策略索引（-1 = 根） */
        int children[MCTS_MAX_CHILDREN]; /* 子节点ID数组（-1 = 空） */
        int child_count;                 /* 子节点数量 */
        int visit_count;                 /* 访问次数 */
        int win_count;                   /* 胜利次数 */
        bool fully_expanded;             /* 是否已完全展开 */
    } MCTSNode;

    MCTSNode nodes[MCTS_MAX_NODES];
    int node_count = 0;

    /* 创建根节点 */
    memset(&nodes[0], 0, sizeof(MCTSNode));
    nodes[0].id = 0;
    nodes[0].parent_id = -1;
    nodes[0].strategy_index = -1;
    for (int i = 0; i < MCTS_MAX_CHILDREN; i++) {
        nodes[0].children[i] = -1;
    }
    node_count = 1;

/* ---- 辅助函数 ---- */

/* UCB1 评分 */
#define UCB1(win_rate, parent_visits, visits) \
    ((win_rate) + MCTS_C * sqrt(log((double) (parent_visits) + 1.0) / ((double) (visits) + 1e-10)))

/* 创建新节点 */
#define MCTS_CREATE_NODE(parent, strategy)                      \
    do {                                                        \
        if (node_count < MCTS_MAX_NODES) {                      \
            memset(&nodes[node_count], 0, sizeof(MCTSNode));    \
            nodes[node_count].id = node_count;                  \
            nodes[node_count].parent_id = (parent);             \
            nodes[node_count].strategy_index = (strategy);      \
            for (int _ci = 0; _ci < MCTS_MAX_CHILDREN; _ci++) { \
                nodes[node_count].children[_ci] = -1;           \
            }                                                   \
            node_count++;                                       \
        }                                                       \
    } while (0)

    /* ---- 主 MCTS 循环 ---- */
    for (int sim = 0; sim < max_steps && !proof->is_complete; sim++) {
        /* ---- 阶段1：选择（Selection） ----
         * 从根节点开始，使用 UCB1 选择最优子节点直到叶子 */
        int current = 0; /* 从根开始 */
        while (nodes[current].child_count > 0 && !nodes[current].fully_expanded) {
            int best_child = -1;
            double best_ucb = -lv_HUGE_NUMBER;

            for (int c = 0; c < nodes[current].child_count; c++) {
                int child_id = nodes[current].children[c];
                if (child_id < 0)
                    continue;

                MCTSNode *child = &nodes[child_id];
                if (child->visit_count == 0) {
                    /* 未访问的节点优先 */
                    best_child = child_id;
                    break;
                }

                double win_rate =
                    (child->visit_count > 0) ? (double) child->win_count / (double) child->visit_count : 0.0;
                double ucb = UCB1(win_rate, nodes[current].visit_count, child->visit_count);

                if (ucb > best_ucb) {
                    best_ucb = ucb;
                    best_child = child_id;
                }
            }

            if (best_child < 0)
                break;
            current = best_child;
        }

        /* ---- 阶段2：展开（Expansion） ----
         * 为当前节点添加一个未尝试的策略子节点 */
        if (!nodes[current].fully_expanded && node_count < MCTS_MAX_NODES) {
            /* 找到一个未展开的策略 */
            int untried_strategy = -1;
            bool already_child[MCTS_MAX_CHILDREN];
            memset(already_child, false, sizeof(already_child));

            for (int c = 0; c < nodes[current].child_count; c++) {
                int cid = nodes[current].children[c];
                if (cid >= 0 && cid < node_count) {
                    already_child[nodes[cid].strategy_index] = true;
                }
            }

            for (int s = 0; s < PROOF_STRATEGY_COUNT; s++) {
                if (!already_child[s] && mse->strategies[s].status != PROOF_STRATEGY_UNAVAILABLE &&
                    mse->strategies[s].execute != NULL) {
                    untried_strategy = s;
                    break;
                }
            }

            if (untried_strategy >= 0) {
                int new_node_id = node_count;
                MCTS_CREATE_NODE(current, untried_strategy);

                /* 添加为当前节点的子节点 */
                if (nodes[current].child_count < MCTS_MAX_CHILDREN) {
                    nodes[current].children[nodes[current].child_count] = new_node_id;
                    nodes[current].child_count++;
                }

                /* 检查是否已完全展开 */
                bool all_expanded = true;
                bool child_check[MCTS_MAX_CHILDREN];
                memset(child_check, false, sizeof(child_check));
                for (int c = 0; c < nodes[current].child_count; c++) {
                    int cid = nodes[current].children[c];
                    if (cid >= 0 && cid < node_count) {
                        child_check[nodes[cid].strategy_index] = true;
                    }
                }
                for (int s = 0; s < PROOF_STRATEGY_COUNT; s++) {
                    if (!child_check[s] && mse->strategies[s].status != PROOF_STRATEGY_UNAVAILABLE &&
                        mse->strategies[s].execute != NULL) {
                        all_expanded = false;
                        break;
                    }
                }
                nodes[current].fully_expanded = all_expanded;

                current = new_node_id; /* 移动到新节点 */
            }
        }

        /* ---- 阶段3：模拟（Simulation） ----
         * 从当前节点随机执行策略直到终止或达到模拟深度限制 */
        int sim_depth = 0;
#define MCTS_SIM_MAX_DEPTH 8
        bool sim_result = false;

        /* 执行当前节点对应的策略 */
        if (nodes[current].strategy_index >= 0) {
            proof_multi_strategy_activate(mse, (ProofStrategyType) nodes[current].strategy_index);
            sim_result = proof_multi_strategy_execute(mse);
            sim_depth++;

            if (sim_result && proof->is_complete) {
                /* 模拟直接成功 */
            }
        }

        /* 随机 playout：尝试剩余策略 */
        if (!proof->is_complete) {
            /* 生成随机顺序的策略列表 */
            int random_order[PROOF_STRATEGY_COUNT];
            for (int i = 0; i < PROOF_STRATEGY_COUNT; i++) {
                random_order[i] = i;
            }
            /* 简单洗牌（使用模拟步数作为种子） */
            for (int i = PROOF_STRATEGY_COUNT - 1; i > 0; i--) {
                int j = (sim + i) % (i + 1); /* 伪随机交换 */
                lv_SWAP(int, random_order[i], random_order[j]);
            }

            for (int r = 0; r < PROOF_STRATEGY_COUNT && sim_depth < MCTS_SIM_MAX_DEPTH; r++) {
                int s = random_order[r];
                if (mse->strategies[s].status == PROOF_STRATEGY_UNAVAILABLE)
                    continue;
                if (!mse->strategies[s].execute)
                    continue;

                proof_multi_strategy_activate(mse, (ProofStrategyType) s);
                bool result = proof_multi_strategy_execute(mse);
                sim_depth++;

                if (result && proof->is_complete) {
                    sim_result = true;
                    break;
                }
            }
        }

        /* ---- 阶段4：回传（Backpropagation） ----
         * 更新从当前节点到根的所有节点的统计信息 */
        int backprop = current;
        while (backprop >= 0) {
            nodes[backprop].visit_count++;
            if (sim_result) {
                nodes[backprop].win_count++;
            }
            backprop = nodes[backprop].parent_id;
        }

        /* 如果模拟中找到了证明，立即返回 */
        if (proof->is_complete) {
            return true;
        }
    }

    /* 模拟结束后，选择访问次数最多的根子节点作为最佳策略 */
    if (nodes[0].child_count > 0) {
        int best_child = -1;
        int best_visits = -1;
        for (int c = 0; c < nodes[0].child_count; c++) {
            int cid = nodes[0].children[c];
            if (cid >= 0 && nodes[cid].visit_count > best_visits) {
                best_visits = nodes[cid].visit_count;
                best_child = cid;
            }
        }

        if (best_child >= 0 && nodes[best_child].strategy_index >= 0) {
            proof_multi_strategy_activate(mse, (ProofStrategyType) nodes[best_child].strategy_index);
            proof_multi_strategy_execute(mse);
        }
    }

    return proof->is_complete;

#undef MCTS_MAX_NODES
#undef MCTS_MAX_CHILDREN
#undef MCTS_C
#undef UCB1
#undef MCTS_CREATE_NODE
#undef MCTS_SIM_MAX_DEPTH
}
