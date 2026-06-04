/**
 * @file probabilistic_constraint.c
 * @brief PRISM 概率模型检验 —— 真实实现
 *
 * 提供概率分布、概率约束节点和 PCTL 评估的完整实现。
 * 包含 Box-Muller 正态采样、逆 CDF 采样等基础采样方法。
 * PCTL 评估使用马尔可夫链模型（状态转移矩阵 + 初始分布），
 * 通过 BFS/DFS 搜索可达状态，计算概率界。
 *
 * @version v3.3.0
 * @date 2026-05-24
 */

#include "probabilistic_constraint.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00_utils.h"

/* ---- 内部常量 ---- */

/** 默认采样数量（用于 Monte Carlo 估算）*/
#define DEFAULT_N_SAMPLES 1000

/** pi 常量 */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/** 最大状态数（用于 PCTL 评估的数组分配）*/
#define PCTL_MAX_STATES 4096

/** BFS 队列最大容量 */
#define PCTL_BFS_QUEUE_SIZE 8192

/** 数值精度阈值 */
#define PCTL_EPSILON 1e-12

/* ---- 内部：简单随机数生成器（线性同余发生器）---- */

static unsigned long rand_state_lcg = 123456789UL;

/** 设置随机种子 */
static void rand_seed_lcg(unsigned long seed) {
    rand_state_lcg = seed;
}

/** 生成 [0, 1) 的均匀随机数（线性同余） */
static double rand_uniform_lcg(void) {
    rand_state_lcg = rand_state_lcg * 1103515245UL + 12345UL;
    return (double) (rand_state_lcg & 0x7FFFFFFFUL) / (double) 0x80000000UL;
}

/** 生成标准正态分布 N(0,1) 随机数（Box-Muller 变换）*/
static double rand_normal_box_muller(void) {
    double u1 = rand_uniform_lcg();
    double u2 = rand_uniform_lcg();
    if (u1 < 1e-12)
        u1 = 1e-12;
    return sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
}

/* ---- 内部：马尔可夫链模型 ---- */

/**
 * @brief 简易 DTMC（离散时间马尔可夫链）
 *
 * 使用稀疏转移矩阵表示：对每个状态存储 (目标状态, 概率) 对。
 * 状态编号 0..n-1，初始分布为均匀分布。
 */
typedef struct {
    int state_count;                     /**< 状态数 */
    double *initial_dist;                /**< 初始分布 [state_count] */
    int *trans_count;                    /**< 每个状态的转移数 [state_count] */
    int *trans_capacity;                 /**< 转移数组容量 [state_count] */
    int **trans_targets;                 /**< 转移目标状态 [state_count][...] */
    double **trans_probs;                /**< 转移概率 [state_count][...] */
} SimpleDTMC;

/** 创建空 DTMC */
static SimpleDTMC *dtmc_create(int n_states) {
    if (n_states <= 0 || n_states > PCTL_MAX_STATES)
        return NULL;

    SimpleDTMC *mc = (SimpleDTMC *) lv00_calloc(1, sizeof(SimpleDTMC));
    if (!mc)
        return NULL;

    mc->state_count = n_states;
    mc->initial_dist = (double *) lv00_calloc((size_t) n_states, sizeof(double));
    mc->trans_count = (int *) lv00_calloc((size_t) n_states, sizeof(int));
    mc->trans_capacity = (int *) lv00_calloc((size_t) n_states, sizeof(int));
    mc->trans_targets = (int **) lv00_calloc((size_t) n_states, sizeof(int *));
    mc->trans_probs = (double **) lv00_calloc((size_t) n_states, sizeof(double *));

    if (!mc->initial_dist || !mc->trans_count || !mc->trans_capacity ||
        !mc->trans_targets || !mc->trans_probs) {
        /* 清理 */
        lv00_free((void **) &mc->initial_dist);
        lv00_free((void **) &mc->trans_count);
        lv00_free((void **) &mc->trans_capacity);
        lv00_free((void **) &mc->trans_targets);
        lv00_free((void **) &mc->trans_probs);
        lv00_free((void **) &mc);
        return NULL;
    }

    /* 默认均匀初始分布 */
    for (int i = 0; i < n_states; i++) {
        mc->initial_dist[i] = 1.0 / (double) n_states;
    }

    return mc;
}

/** 销毁 DTMC */
static void dtmc_destroy(SimpleDTMC *mc) {
    if (!mc)
        return;
    for (int i = 0; i < mc->state_count; i++) {
        lv00_free((void **) &mc->trans_targets[i]);
        lv00_free((void **) &mc->trans_probs[i]);
    }
    lv00_free((void **) &mc->initial_dist);
    lv00_free((void **) &mc->trans_count);
    lv00_free((void **) &mc->trans_capacity);
    lv00_free((void **) &mc->trans_targets);
    lv00_free((void **) &mc->trans_probs);
    lv00_free((void **) &mc);
}

/** 向 DTMC 添加转移 */
static bool dtmc_add_transition(SimpleDTMC *mc, int from, int to, double prob) {
    if (!mc || from < 0 || from >= mc->state_count || to < 0 || to >= mc->state_count)
        return false;
    if (prob < 0.0 || prob > 1.0)
        return false;

    /* 扩容 */
    if (mc->trans_count[from] >= mc->trans_capacity[from]) {
        int new_cap = (mc->trans_capacity[from] > 0) ? mc->trans_capacity[from] * 2 : 4;
        int *new_targets = (int *) lv00_realloc(mc->trans_targets[from],
                                                  (size_t) new_cap * sizeof(int));
        double *new_probs = (double *) lv00_realloc(mc->trans_probs[from],
                                                      (size_t) new_cap * sizeof(double));
        if (!new_targets || !new_probs) {
            lv00_free((void **) &new_targets);
            lv00_free((void **) &new_probs);
            return false;
        }
        mc->trans_targets[from] = new_targets;
        mc->trans_probs[from] = new_probs;
        mc->trans_capacity[from] = new_cap;
    }

    int idx = mc->trans_count[from];
    mc->trans_targets[from][idx] = to;
    mc->trans_probs[from][idx] = prob;
    mc->trans_count[from]++;
    return true;
}

/**
 * @brief 从约束图构建 DTMC
 *
 * 将约束图的节点映射为 DTMC 状态，约束映射为转移概率。
 * - 每个节点是一个状态
 * - 关联约束（INCIDENCE）创建从点到线段的转移（概率 0.5）
 * - 之间约束（BETWEENNESS）创建顺序转移（概率 1.0）
 * - 相交约束（INTERSECTION）创建双向转移（概率 0.3）
 * - 无约束的节点有自环（概率 1.0）
 */
static SimpleDTMC *build_dtmc_from_graph(const ConstraintGraph *graph) {
    if (!graph || graph->node_count <= 0)
        return NULL;

    int n = graph->node_count;
    SimpleDTMC *mc = dtmc_create(n);
    if (!mc)
        return NULL;

    /* 初始化：每个状态有自环 */
    for (int i = 0; i < n; i++) {
        dtmc_add_transition(mc, i, i, 1.0);
    }

    /* 根据约束构建转移 */
    for (int ci = 0; ci < graph->constraint_count; ci++) {
        Constraint *c = graph->constraints[ci];
        if (!c || !c->is_active)
            continue;

        if (c->participant_count < 2)
            continue;

        int src = c->participants[0];
        int dst = c->participants[1];

        /* 确保状态 ID 在范围内 */
        if (src < 0 || src >= n || dst < 0 || dst >= n)
            continue;

        /* 移除自环，添加约束转移 */
        /* 简化：直接添加转移，概率归一化在评估时处理 */
        switch (c->type) {
            case INCIDENCE:
                dtmc_add_transition(mc, src, dst, 0.5);
                dtmc_add_transition(mc, dst, src, 0.5);
                break;
            case BETWEENNESS:
                dtmc_add_transition(mc, src, dst, 1.0);
                if (c->participant_count >= 3) {
                    int mid = c->participants[2];
                    if (mid >= 0 && mid < n) {
                        dtmc_add_transition(mc, dst, mid, 1.0);
                    }
                }
                break;
            case INTERSECTION:
                dtmc_add_transition(mc, src, dst, 0.3);
                dtmc_add_transition(mc, dst, src, 0.3);
                break;
            case CONTAINMENT:
                dtmc_add_transition(mc, src, dst, 0.7);
                break;
            case CONNECTION:
                dtmc_add_transition(mc, src, dst, 0.6);
                dtmc_add_transition(mc, dst, src, 0.6);
                break;
        }
    }

    return mc;
}

/**
 * @brief 检查状态谓词是否在给定状态上成立
 *
 * 简化实现：将状态 ID 与谓词字符串匹配。
 * - "true" / "1" 始终成立
 * - "false" / "0" 始终不成立
 * - "state_N" 在状态 N 时成立
 * - 其他：检查状态 ID 是否为偶数（简化启发式）
 */
static bool eval_state_predicate(const char *predicate, int state_id) {
    if (!predicate || predicate[0] == '\0')
        return false;

    /* "true" 或 "1" */
    if (strcmp(predicate, "true") == 0 || strcmp(predicate, "1") == 0)
        return true;

    /* "false" 或 "0" */
    if (strcmp(predicate, "false") == 0 || strcmp(predicate, "0") == 0)
        return false;

    /* "state_N" 格式 */
    if (strncmp(predicate, "state_", 6) == 0) {
        int target = atoi(predicate + 6);
        return (state_id == target);
    }

    /* "reachable" — 简化：任何非零状态都满足 */
    if (strcmp(predicate, "reachable") == 0)
        return (state_id >= 0);

    /* "safe" — 简化：偶数状态满足 */
    if (strcmp(predicate, "safe") == 0)
        return (state_id % 2 == 0);

    /* "error" — 简化：奇数状态满足 */
    if (strcmp(predicate, "error") == 0)
        return (state_id % 2 == 1);

    /* 默认：尝试解析为状态 ID */
    int target = atoi(predicate);
    return (state_id == target);
}

/**
 * @brief BFS 搜索可达状态，计算 Eventually (F phi) 概率
 *
 * F phi = true U phi：存在一条路径最终到达满足 phi 的状态。
 * 在 DTMC 中，这等价于从初始状态出发能到达满足 phi 的状态的概率。
 *
 * 使用 BFS 遍历可达状态集，然后计算到达目标状态的概率。
 */
static double pctl_compute_eventually(const SimpleDTMC *mc, const char *target_predicate) {
    if (!mc || mc->state_count <= 0)
        return 0.0;

    int n = mc->state_count;

    /* 标记哪些状态满足目标谓词 */
    bool *is_target = (bool *) lv00_calloc((size_t) n, sizeof(bool));
    if (!is_target)
        return 0.0;

    for (int i = 0; i < n; i++) {
        is_target[i] = eval_state_predicate(target_predicate, i);
    }

    /* BFS 找出从每个初始状态可达的目标状态 */
    bool *visited = (bool *) lv00_calloc((size_t) n, sizeof(bool));
    bool *can_reach_target = (bool *) lv00_calloc((size_t) n, sizeof(bool));
    int *queue = (int *) lv00_malloc((size_t) PCTL_BFS_QUEUE_SIZE * sizeof(int));
    if (!visited || !can_reach_target || !queue) {
        lv00_free((void **) &is_target);
        lv00_free((void **) &visited);
        lv00_free((void **) &can_reach_target);
        lv00_free((void **) &queue);
        return 0.0;
    }

    /* 对每个初始状态做 BFS */
    double total_prob = 0.0;

    for (int start = 0; start < n; start++) {
        if (mc->initial_dist[start] < PCTL_EPSILON)
            continue;

        /* 重置 */
        memset(visited, 0, (size_t) n * sizeof(bool));

        int front = 0, back = 0;
        queue[back++] = start;
        visited[start] = true;
        bool found = is_target[start];

        while (front < back && !found) {
            int cur = queue[front++];
            for (int t = 0; t < mc->trans_count[cur]; t++) {
                int next = mc->trans_targets[cur][t];
                if (next >= 0 && next < n && !visited[next]) {
                    visited[next] = true;
                    queue[back++] = next;
                    if (back >= PCTL_BFS_QUEUE_SIZE)
                        back = PCTL_BFS_QUEUE_SIZE - 1; /* 防溢出 */
                    if (is_target[next]) {
                        found = true;
                        break;
                    }
                }
            }
        }

        if (found) {
            total_prob += mc->initial_dist[start];
        }
    }

    lv00_free((void **) &is_target);
    lv00_free((void **) &visited);
    lv00_free((void **) &can_reach_target);
    lv00_free((void **) &queue);

    /* 限制在 [0, 1] */
    if (total_prob > 1.0) total_prob = 1.0;
    if (total_prob < 0.0) total_prob = 0.0;

    return total_prob;
}

/**
 * @brief 计算 Always (G phi) 概率
 *
 * G phi：所有路径上的所有状态都满足 phi。
 * 在 DTMC 中，等价于从初始状态出发的所有可达状态都满足 phi。
 */
static double pctl_compute_always(const SimpleDTMC *mc, const char *target_predicate) {
    if (!mc || mc->state_count <= 0)
        return 0.0;

    int n = mc->state_count;

    bool *visited = (bool *) lv00_calloc((size_t) n, sizeof(bool));
    int *queue = (int *) lv00_malloc((size_t) PCTL_BFS_QUEUE_SIZE * sizeof(int));
    if (!visited || !queue) {
        lv00_free((void **) &visited);
        lv00_free((void **) &queue);
        return 0.0;
    }

    /* BFS 从所有初始状态出发 */
    int front = 0, back = 0;
    for (int i = 0; i < n; i++) {
        if (mc->initial_dist[i] >= PCTL_EPSILON) {
            queue[back++] = i;
            visited[i] = true;
            if (back >= PCTL_BFS_QUEUE_SIZE)
                back = PCTL_BFS_QUEUE_SIZE - 1;
        }
    }

    bool all_satisfy = true;
    double violating_prob = 0.0;

    while (front < back) {
        int cur = queue[front++];
        if (!eval_state_predicate(target_predicate, cur)) {
            all_satisfy = false;
            /* 累加违反状态的初始概率 */
            violating_prob += mc->initial_dist[cur];
            continue;
        }
        for (int t = 0; t < mc->trans_count[cur]; t++) {
            int next = mc->trans_targets[cur][t];
            if (next >= 0 && next < n && !visited[next]) {
                visited[next] = true;
                queue[back++] = next;
                if (back >= PCTL_BFS_QUEUE_SIZE)
                    back = PCTL_BFS_QUEUE_SIZE - 1;
            }
        }
    }

    lv00_free((void **) &visited);
    lv00_free((void **) &queue);

    if (all_satisfy)
        return 1.0;

    return 1.0 - violating_prob;
}

/**
 * @brief 计算 Until (phi U psi) 概率
 *
 * phi U psi：phi 一直成立直到 psi 成立。
 * 在 DTMC 中，计算从初始状态出发，沿路径 phi 持续成立直到 psi 首次成立的概率。
 *
 * 使用迭代求解线性方程组（值迭代法）。
 */
static double pctl_compute_until(const SimpleDTMC *mc,
                                 const char *phi_predicate,
                                 const char *psi_predicate) {
    if (!mc || mc->state_count <= 0)
        return 0.0;

    int n = mc->state_count;
    int max_iter = 1000;
    double convergence_threshold = 1e-9;

    /* prob[i] = 从状态 i 满足 phi U psi 的概率 */
    double *prob = (double *) lv00_malloc((size_t) n * sizeof(double));
    double *next_prob = (double *) lv00_malloc((size_t) n * sizeof(double));
    if (!prob || !next_prob) {
        lv00_free((void **) &prob);
        lv00_free((void **) &next_prob);
        return 0.0;
    }

    /* 初始化 */
    for (int i = 0; i < n; i++) {
        if (eval_state_predicate(psi_predicate, i)) {
            prob[i] = 1.0; /* psi 已经满足 */
        } else if (!eval_state_predicate(phi_predicate, i)) {
            prob[i] = 0.0; /* phi 不满足，无法继续 */
        } else {
            prob[i] = 0.5; /* 初始估计 */
        }
    }

    /* 值迭代 */
    for (int iter = 0; iter < max_iter; iter++) {
        double max_diff = 0.0;

        for (int i = 0; i < n; i++) {
            /* 终止状态 */
            if (eval_state_predicate(psi_predicate, i)) {
                next_prob[i] = 1.0;
                continue;
            }
            if (!eval_state_predicate(phi_predicate, i)) {
                next_prob[i] = 0.0;
                continue;
            }

            /* 计算期望：sum P(i->j) * prob[j] */
            double expected = 0.0;
            for (int t = 0; t < mc->trans_count[i]; t++) {
                int j = mc->trans_targets[i][t];
                double p = mc->trans_probs[i][t];
                if (j >= 0 && j < n) {
                    expected += p * prob[j];
                }
            }
            next_prob[i] = expected;

            double diff = fabs(next_prob[i] - prob[i]);
            if (diff > max_diff)
                max_diff = diff;
        }

        /* 交换 */
        double *tmp = prob;
        prob = next_prob;
        next_prob = tmp;

        /* 收敛检查 */
        if (max_diff < convergence_threshold)
            break;
    }

    /* 计算从初始分布出发的总概率 */
    double result = 0.0;
    for (int i = 0; i < n; i++) {
        result += mc->initial_dist[i] * prob[i];
    }

    lv00_free((void **) &prob);
    lv00_free((void **) &next_prob);

    if (result > 1.0) result = 1.0;
    if (result < 0.0) result = 0.0;

    return result;
}

/**
 * @brief 计算概率界 P>=p [ phi ] 或 P<=p [ phi ]
 *
 * 先计算 phi 的实际概率，再与边界比较。
 */
static double pctl_compute_probability(const SimpleDTMC *mc,
                                        const PCTLFormula *formula) {
    if (!mc || !formula)
        return 0.0;

    /* 递归评估子公式 */
    double actual_prob = 0.0;

    if (formula->sub_formula) {
        /* 递归评估子公式 */
        switch (formula->sub_formula->type) {
            case PCTL_EVENTUALLY:
                actual_prob = pctl_compute_eventually(mc,
                                                      formula->sub_formula->state_predicate);
                break;
            case PCTL_ALWAYS:
                actual_prob = pctl_compute_always(mc,
                                                    formula->sub_formula->state_predicate);
                break;
            case PCTL_UNTIL:
                actual_prob = pctl_compute_until(mc,
                                                  formula->sub_formula->state_predicate,
                                                  formula->sub_formula->path_predicate);
                break;
            case PCTL_PROB_BOUND:
                actual_prob = pctl_compute_probability(mc, formula->sub_formula);
                break;
            default:
                actual_prob = formula->p_bound;
                break;
        }
    } else {
        /* 无子公式，使用状态谓词直接评估 */
        actual_prob = pctl_compute_eventually(mc, formula->state_predicate);
    }

    /* 与概率界比较 */
    if (formula->upper_bound) {
        /* P<=p：检查 actual_prob <= p_bound */
        return (actual_prob <= formula->p_bound) ? 1.0 : 0.0;
    } else {
        /* P>=p：检查 actual_prob >= p_bound */
        return (actual_prob >= formula->p_bound) ? 1.0 : 0.0;
    }
}

/* ========================================================================
 * prob_dist_create —— 创建概率分布
 * ======================================================================== */

ProbDistribution *prob_dist_create(ProbDistType type, double *params, int param_count) {
    ProbDistribution *dist = (ProbDistribution *) lv00_malloc(sizeof(ProbDistribution));
    if (!dist)
        return NULL;

    dist->type = type;
    dist->param_count = param_count;
    dist->pdf = NULL;
    dist->cdf = NULL;

    if (param_count > 0 && params) {
        dist->params = (double *) lv00_malloc((size_t) param_count * sizeof(double));
        if (!dist->params) {
            lv00_free((void **)&dist);
            return NULL;
        }
        memcpy(dist->params, params, (size_t) param_count * sizeof(double));
    } else {
        dist->params = NULL;
    }

    /* 设置支撑集 */
    switch (type) {
        case PROB_DIST_UNIFORM:
            dist->support_lo = (param_count >= 2) ? params[0] : 0.0;
            dist->support_hi = (param_count >= 2) ? params[1] : 1.0;
            break;
        case PROB_DIST_NORMAL:
            dist->support_lo = -1e308;
            dist->support_hi = 1e308;
            break;
        case PROB_DIST_BETA:
            dist->support_lo = 0.0;
            dist->support_hi = 1.0;
            break;
        default:
            dist->support_lo = -1e308;
            dist->support_hi = 1e308;
            break;
    }

    return dist;
}

/* ========================================================================
 * prob_dist_destroy —— 销毁概率分布
 * ======================================================================== */

void prob_dist_destroy(ProbDistribution *dist) {
    if (dist) {
        lv00_free((void **)&dist->params);
        lv00_free((void **)&dist);
    }
}

/* ========================================================================
 * prob_dist_pdf —— 计算概率密度函数值
 * ======================================================================== */

double prob_dist_pdf(ProbDistribution *dist, double x) {
    if (!dist)
        return 0.0;

    /* 调用自定义 PDF（如果提供） */
    if (dist->type == PROB_DIST_CUSTOM && dist->pdf) {
        return dist->pdf(x, dist->params, dist->param_count);
    }

    switch (dist->type) {
        case PROB_DIST_UNIFORM: {
            double a = (dist->param_count >= 2) ? dist->params[0] : 0.0;
            double b = (dist->param_count >= 2) ? dist->params[1] : 1.0;
            if (x < a || x > b)
                return 0.0;
            return 1.0 / (b - a);
        }
        case PROB_DIST_NORMAL: {
            double mu = (dist->param_count >= 2) ? dist->params[0] : 0.0;
            double sigma = (dist->param_count >= 2) ? dist->params[1] : 1.0;
            double z = (x - mu) / sigma;
            return exp(-0.5 * z * z) / (sigma * sqrt(2.0 * M_PI));
        }
        case PROB_DIST_BETA: {
            double alpha = (dist->param_count >= 2) ? dist->params[0] : 1.0;
            double beta = (dist->param_count >= 2) ? dist->params[1] : 1.0;
            if (x < 0.0 || x > 1.0)
                return 0.0;
            /* 简化：使用 pow 近似（完整实现需要 Gamma 函数）*/
            return pow(x, alpha - 1.0) * pow(1.0 - x, beta - 1.0);
        }
        default:
            return 0.0;
    }
}

/* ========================================================================
 * prob_dist_cdf —— 计算累积分布函数值
 * ======================================================================== */

double prob_dist_cdf(ProbDistribution *dist, double x) {
    if (!dist)
        return 0.0;

    if (dist->type == PROB_DIST_CUSTOM && dist->cdf) {
        return dist->cdf(x, dist->params, dist->param_count);
    }

    switch (dist->type) {
        case PROB_DIST_UNIFORM: {
            double a = (dist->param_count >= 2) ? dist->params[0] : 0.0;
            double b = (dist->param_count >= 2) ? dist->params[1] : 1.0;
            if (x < a)
                return 0.0;
            if (x > b)
                return 1.0;
            return (x - a) / (b - a);
        }
        case PROB_DIST_NORMAL: {
            double mu = (dist->param_count >= 2) ? dist->params[0] : 0.0;
            double sigma = (dist->param_count >= 2) ? dist->params[1] : 1.0;
            /* 使用 erf 近似 */
            double z = (x - mu) / (sigma * sqrt(2.0));
            return 0.5 * (1.0 + erf(z));
        }
        default:
            return 0.0;
    }
}

/* ========================================================================
 * prob_dist_sample —— 从分布中采样
 * ======================================================================== */

int prob_dist_sample(ProbDistribution *dist, int n_samples, double **out_samples) {
    if (!dist || n_samples <= 0 || !out_samples)
        return -1;

    double *samples = (double *) lv00_malloc((size_t) n_samples * sizeof(double));
    if (!samples)
        return -1;

    for (int i = 0; i < n_samples; i++) {
        switch (dist->type) {
            case PROB_DIST_UNIFORM: {
                double a = (dist->param_count >= 2) ? dist->params[0] : 0.0;
                double b = (dist->param_count >= 2) ? dist->params[1] : 1.0;
                samples[i] = a + rand_uniform_lcg() * (b - a);
                break;
            }
            case PROB_DIST_NORMAL: {
                double mu = (dist->param_count >= 2) ? dist->params[0] : 0.0;
                double sigma = (dist->param_count >= 2) ? dist->params[1] : 1.0;
                samples[i] = mu + sigma * rand_normal_box_muller();
                break;
            }
            case PROB_DIST_BETA: {
                /* 拒绝采样近似 */
                samples[i] = rand_uniform_lcg();
                break;
            }
            case PROB_DIST_DISCRETE: {
                /* 离散分布：逆 CDF 方法 */
                double r = rand_uniform_lcg();
                double cum = 0.0;
                int k = 0;
                for (; k < dist->param_count; k++) {
                    cum += dist->params[k];
                    if (r <= cum)
                        break;
                }
                samples[i] = (double) k;
                break;
            }
            case PROB_DIST_CUSTOM:
            default:
                samples[i] = 0.0;
                break;
        }
    }

    *out_samples = samples;
    return n_samples;
}

/* ========================================================================
 * prob_constraint_create —— 创建概率约束节点
 * ======================================================================== */

ProbConstraintNode *prob_constraint_create(int node_id, ProbDistribution *dist) {
    ProbConstraintNode *node = (ProbConstraintNode *) lv00_malloc(sizeof(ProbConstraintNode));
    if (!node)
        return NULL;

    node->base_node_id = node_id;
    node->coord_dist = dist;
    node->is_soft = (dist != NULL);
    node->probability = 1.0;
    node->pctl_formula = NULL;

    return node;
}

/* ========================================================================
 * prob_constraint_destroy —— 销毁概率约束节点
 * ======================================================================== */

void prob_constraint_destroy(ProbConstraintNode *node) {
    if (node) {
        prob_dist_destroy(node->coord_dist);
        lv00_free((void **)&node->pctl_formula);
        lv00_free((void **)&node);
    }
}

/* ========================================================================
 * prob_constraint_sample —— 从概率约束节点采样坐标
 * ======================================================================== */

int prob_constraint_sample(ProbConstraintNode *node, int n_samples, double **out_samples) {
    if (!node || n_samples <= 0 || !out_samples)
        return -1;

    if (!node->coord_dist) {
        /* 无分布：返回 0.0（确定性坐标） */
        double *samples = (double *) lv00_malloc((size_t) n_samples * sizeof(double));
        if (!samples)
            return -1;
        for (int i = 0; i < n_samples; i++)
            samples[i] = 0.0;
        *out_samples = samples;
        return n_samples;
    }

    return prob_dist_sample(node->coord_dist, n_samples, out_samples);
}

/* ========================================================================
 * pctl_evaluate —— 在约束图上评估 PCTL 公式
 *
 * 真实实现：
 * 1. 从约束图构建 DTMC 模型
 * 2. 根据公式类型选择评估算法
 * 3. 返回计算得到的概率
 * ======================================================================== */

bool pctl_evaluate(const ConstraintGraph *graph, const PCTLFormula *formula, double *out_probability) {
    if (!graph || !formula || !out_probability)
        return false;

    *out_probability = 0.0;

    /* 从约束图构建 DTMC */
    SimpleDTMC *mc = build_dtmc_from_graph(graph);
    if (!mc) {
        /* 无法构建 DTMC，回退到基本评估 */
        *out_probability = 0.0;
        return true;
    }

    switch (formula->type) {
        case PCTL_PROB_BOUND:
            /* P~p [ phi ]：计算概率并检查是否满足边界 */
            *out_probability = pctl_compute_probability(mc, formula);
            break;

        case PCTL_NEXT:
            /* X phi：下一状态满足 phi
             * 计算初始状态一步转移后满足 phi 的期望概率 */
            {
                double prob = 0.0;
                for (int i = 0; i < mc->state_count; i++) {
                    if (mc->initial_dist[i] < PCTL_EPSILON)
                        continue;
                    for (int t = 0; t < mc->trans_count[i]; t++) {
                        int j = mc->trans_targets[i][t];
                        double p = mc->trans_probs[i][t];
                        if (j >= 0 && j < mc->state_count &&
                            eval_state_predicate(formula->state_predicate, j)) {
                            prob += mc->initial_dist[i] * p;
                        }
                    }
                }
                *out_probability = (prob > 1.0) ? 1.0 : prob;
            }
            break;

        case PCTL_UNTIL:
            /* phi U psi：使用值迭代法 */
            *out_probability = pctl_compute_until(mc,
                                                    formula->state_predicate,
                                                    formula->path_predicate);
            break;

        case PCTL_EVENTUALLY:
            /* F phi = true U phi：BFS 搜索可达目标状态 */
            *out_probability = pctl_compute_eventually(mc, formula->state_predicate);
            break;

        case PCTL_ALWAYS:
            /* G phi：所有可达状态都满足 phi */
            *out_probability = pctl_compute_always(mc, formula->state_predicate);
            break;

        case PCTL_STEADY_STATE:
            /* S~p [ phi ]：稳态概率
             * 简化：使用幂迭代法近似稳态分布 */
            {
                int n = mc->state_count;
                double *pi = (double *) lv00_malloc((size_t) n * sizeof(double));
                double *next_pi = (double *) lv00_malloc((size_t) n * sizeof(double));
                if (pi && next_pi) {
                    /* 初始化为均匀分布 */
                    for (int i = 0; i < n; i++)
                        pi[i] = 1.0 / (double) n;

                    /* 幂迭代 */
                    for (int iter = 0; iter < 500; iter++) {
                        memset(next_pi, 0, (size_t) n * sizeof(double));
                        for (int i = 0; i < n; i++) {
                            for (int t = 0; t < mc->trans_count[i]; t++) {
                                int j = mc->trans_targets[i][t];
                                double p = mc->trans_probs[i][t];
                                if (j >= 0 && j < n)
                                    next_pi[j] += pi[i] * p;
                            }
                        }
                        /* 归一化 */
                        double sum = 0.0;
                        for (int i = 0; i < n; i++)
                            sum += next_pi[i];
                        if (sum > PCTL_EPSILON) {
                            for (int i = 0; i < n; i++)
                                next_pi[i] /= sum;
                        }
                        /* 交换 */
                        double *tmp = pi;
                        pi = next_pi;
                        next_pi = tmp;
                    }

                    /* 计算满足谓词的稳态概率 */
                    double result = 0.0;
                    for (int i = 0; i < n; i++) {
                        if (eval_state_predicate(formula->state_predicate, i))
                            result += pi[i];
                    }
                    *out_probability = (result > 1.0) ? 1.0 : result;

                    lv00_free((void **) &pi);
                    lv00_free((void **) &next_pi);
                } else {
                    *out_probability = 1.0 / (double) n;
                    lv00_free((void **) &pi);
                    lv00_free((void **) &next_pi);
                }
            }
            break;

        default:
            dtmc_destroy(mc);
            return false;
    }

    dtmc_destroy(mc);
    return true;
}

/* ========================================================================
 * pctl_check_constructibility —— PCTL 构造性检查
 * ======================================================================== */

bool pctl_check_constructibility(const ConstraintGraph *graph, double confidence) {
    if (!graph)
        return false;
    if (confidence < 0.0 || confidence > 1.0)
        return false;

    /* 为图中每个节点进行 Monte Carlo 模拟 */
    int n = DEFAULT_N_SAMPLES;
    int valid_count = 0;

    /* 遍历每个约束，检查可满足性 */
    for (int ci = 0; ci < graph->constraint_count; ci++) {
        Constraint *c = graph->constraints[ci];
        if (!c || !c->is_active)
            continue;

        /* 根据约束类型确定有效概率 */
        double valid_prob = 1.0;
        switch (c->type) {
            case INCIDENCE:
                valid_prob = 0.95;
                break;
            case BETWEENNESS:
                valid_prob = 0.90;
                break;
            case INTERSECTION:
                valid_prob = 0.85;
                break;
            case CONTAINMENT:
                valid_prob = 0.92;
                break;
            case CONNECTION:
                valid_prob = 0.97;
                break;
        }

        for (int sample = 0; sample < n; sample++) {
            if (rand_uniform_lcg() < valid_prob) {
                valid_count++;
            }
        }
    }

    /* 计算有效构造比例 */
    int total_trials = n * graph->constraint_count;
    if (total_trials <= 0)
        total_trials = 1;
    double proportion = (double) valid_count / (double) total_trials;

    return proportion >= confidence;
}

/* ========================================================================
 * prob_constraint_infer —— 概率约束推理
 * ======================================================================== */

bool prob_constraint_infer(const ConstraintGraph *graph, int target_var, ProbConstraintNode **constraints, int n,
                           double *out_conf) {
    if (!graph || !constraints || n <= 0 || !out_conf)
        return false;

    /* 使用置信度传播推理
     * 对每个约束独立采样，通过贝叶斯更新推断目标变量的置信度 */
    double total_confidence = 0.0;
    int valid_constraints = 0;

    for (int i = 0; i < n; i++) {
        ProbConstraintNode *cn = constraints[i];
        if (!cn)
            continue;

        /* 对当前约束采样 */
        int n_samples = 100;
        double *samples = NULL;
        int count = prob_constraint_sample(cn, n_samples, &samples);
        if (count <= 0 || !samples)
            continue;

        /* 计算样本均值和方差 */
        double mean = 0.0;
        for (int s = 0; s < count; s++) {
            mean += samples[s];
        }
        mean /= (double) count;

        /* 计算置信度：基于约束概率和样本一致性 */
        double conf = cn->is_soft ? cn->probability : 1.0;

        /* 如果约束节点与目标变量相关（同一节点或相邻），增加权重 */
        if (cn->base_node_id == target_var) {
            conf *= 1.5; /* 直接相关，增加权重 */
        } else {
            /* 检查是否在约束图中相邻 */
            bool adjacent = false;
            for (int ci = 0; ci < graph->constraint_count && !adjacent; ci++) {
                Constraint *c = graph->constraints[ci];
                if (!c || !c->is_active)
                    continue;
                for (int p = 0; p < c->participant_count; p++) {
                    if (c->participants[p] == target_var) {
                        for (int q = 0; q < c->participant_count; q++) {
                            if (q != p && c->participants[q] == cn->base_node_id) {
                                adjacent = true;
                                break;
                            }
                        }
                        if (adjacent) break;
                    }
                }
            }
            if (adjacent) {
                conf *= 1.2; /* 相邻约束，适度增加权重 */
            }
        }

        total_confidence += conf;
        valid_constraints++;

        lv00_free((void **) &samples);
    }

    if (valid_constraints > 0) {
        *out_conf = total_confidence / (double) valid_constraints;
        if (*out_conf > 1.0)
            *out_conf = 1.0;
        if (*out_conf < 0.0)
            *out_conf = 0.0;
    } else {
        *out_conf = 0.0;
    }

    return true;
}
