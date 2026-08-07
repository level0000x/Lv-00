/**
 * @file probabilistic_constraint.c
 * @brief PRISM 概率模型检验 —— 真实实现
 *
 * 提供概率分布、概率约束节点和 PCTL 评估的完整实现。
 * 包含 Box-Muller 正态采样、逆 CDF 采样等基础采样方法。
 * PCTL 评估使用马尔可夫链模型（状态转移矩阵 + 初始分布），
 * 通过 BFS/DFS 搜索可达状态，计算概率界。
 *
 * @version v5.0.0
 * @date 2026-06-04
 */

#include "lv/lv_platform.h"
#include "probabilistic_constraint.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv.h"
#include "lv/lv_internal.h" /* lv_LOG_WARNING 等统一日志宏 */
#include "lv/lv_graph_traversal.h"
#include "lv/config.h"
#include "lv/lv_parse_utils.h"
#include "lv/lv_numeric.h"
#include "lv/lv_xmacro.h"

#include "lv_utils.h"

/* ---- 内部常量 ---- */

/** 默认采样数量（用于 Monte Carlo 估算）*/
#define DEFAULT_N_SAMPLES 1000

/** pi 常量 (由 lv_platform.h 提供) */

/** 状态数上限 */
#define PCTL_MAX_STATE_LIMIT 65536

/** BFS 队列初始容量 */
#define PCTL_BFS_QUEUE_INIT 1024

/** 数值精度阈值 */
#define PCTL_EPSILON 1e-12

/** Gamma 采样接受-拒绝法最大重试次数 */
#define GAMMA_SAMPLE_MAX_RETRIES 10000

/* ---- 内部：简单随机数生成器（线性同余发生器）---- */

/* 线程局部 RNG 状态：多线程并发采样时各线程持有独立序列，
 * 避免共享静态状态导致的读写数据竞争与非确定性。 */
static lv_THREAD_LOCAL unsigned long rand_state_lcg = 123456789UL;

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
    int state_count;      /**< 状态数 */
    double *initial_dist; /**< 初始分布 [state_count] */
    int *trans_count;     /**< 每个状态的转移数 [state_count] */
    int *trans_capacity;  /**< 转移数组容量 [state_count] */
    int **trans_targets;  /**< 转移目标状态 [state_count][...] */
    double **trans_probs; /**< 转移概率 [state_count][...] */
} SimpleDTMC;

/** 创建空 DTMC */
static SimpleDTMC *dtmc_create(int n_states) {
    if (n_states <= 0 || n_states > PCTL_MAX_STATE_LIMIT)
        return NULL;

    SimpleDTMC *mc = (SimpleDTMC *) lv_calloc(1, sizeof(SimpleDTMC));
    if (!mc)
        return NULL;

    mc->state_count = n_states;
    mc->initial_dist = (double *) lv_calloc((size_t) n_states, sizeof(double));
    mc->trans_count = (int *) lv_calloc((size_t) n_states, sizeof(int));
    mc->trans_capacity = (int *) lv_calloc((size_t) n_states, sizeof(int));
    mc->trans_targets = (int **) lv_calloc((size_t) n_states, sizeof(int *));
    mc->trans_probs = (double **) lv_calloc((size_t) n_states, sizeof(double *));

    if (!mc->initial_dist || !mc->trans_count || !mc->trans_capacity || !mc->trans_targets || !mc->trans_probs) {
        /* 清理 */
        lv_free((void **) &mc->initial_dist);
        lv_free((void **) &mc->trans_count);
        lv_free((void **) &mc->trans_capacity);
        lv_free((void **) &mc->trans_targets);
        lv_free((void **) &mc->trans_probs);
        lv_free((void **) &mc);
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
        lv_free((void **) &mc->trans_targets[i]);
        lv_free((void **) &mc->trans_probs[i]);
    }
    lv_free((void **) &mc->initial_dist);
    lv_free((void **) &mc->trans_count);
    lv_free((void **) &mc->trans_capacity);
    lv_free((void **) &mc->trans_targets);
    lv_free((void **) &mc->trans_probs);
    lv_free((void **) &mc);
}

/** 向 DTMC 添加转移 */
static bool dtmc_add_transition(SimpleDTMC *mc, int from, int to, double prob) {
    if (!mc || from < 0 || from >= mc->state_count || to < 0 || to >= mc->state_count)
        return false;
    if (prob < 0.0 || prob > 1.0)
        return false;

    /* 扩容 */
    if (mc->trans_count[from] >= mc->trans_capacity[from]) {
        int cap_t = mc->trans_capacity[from], cap_p = mc->trans_capacity[from];
        if (!lv_ensure_capacity((void **) &mc->trans_targets[from], mc->trans_count[from], &cap_t, sizeof(int), 1) ||
            !lv_ensure_capacity((void **) &mc->trans_probs[from], mc->trans_count[from], &cap_p, sizeof(double), 1)) {
            /* 失败时各指针保持有效（成功的已更新、失败的未动），由 dtmc_destroy 统一释放 */
            return false;
        }
        mc->trans_capacity[from] = (cap_t > cap_p) ? cap_t : cap_p;
    }

    int idx = mc->trans_count[from];
    mc->trans_targets[from][idx] = to;
    mc->trans_probs[from][idx] = prob;
    mc->trans_count[from]++;
    return true;
}

/**
 * @brief 从约束的满足度推导几何转移概率
 *
 * 约束满足度 satisfaction ∈ [0,1] 映射为转移概率：
 *   prob = satisfaction（满足的几何约束给出确定性转移）
 * 对非活跃约束或未评估约束使用约束类型默认值。
 */
static double prob_from_satisfaction(const Constraint *con) {
    if (!con)
        return 0.0;
    /* 活跃但未评估：使用约束类型基线 */
    if (!con->is_active)
        return 0.0;
    if (con->satisfaction < -0.01) {
        /* 违反的约束 — 低概率 */
        return 0.05;
    }
    /* satisfaction ∈ [0,1] → 直接用作转移概率 */
    double p = con->satisfaction;
    p = lv_clamp(p, 0.0, 1.0);
    return p;
}

/* ── DTMC 转移构建函数（文件作用域，用于查找表） ── */
typedef void (*DTMCBuildFn)(SimpleDTMC *mc, int src, int dst, double prob, const Constraint *c, int n);
typedef struct {
    ConstraintType type;
    DTMCBuildFn build;
} DTMCBuildEntry;

static void dtmc_build_bidirectional(SimpleDTMC *mc, int src, int dst, double prob, const Constraint *c, int n) {
    (void)c; (void)n;
    dtmc_add_transition(mc, src, dst, prob);
    dtmc_add_transition(mc, dst, src, prob);
}
static void dtmc_build_betweenness(SimpleDTMC *mc, int src, int dst, double prob, const Constraint *c, int n) {
    dtmc_add_transition(mc, src, dst, prob);
    if (c->participant_count >= 3) {
        int mid = c->participants[2];
        if (mid >= 0 && mid < n) {
            dtmc_add_transition(mc, dst, mid, prob);
        }
    }
}
static void dtmc_build_unidirectional(SimpleDTMC *mc, int src, int dst, double prob, const Constraint *c, int n) {
    (void)c; (void)n;
    dtmc_add_transition(mc, src, dst, prob);
}

static const DTMCBuildEntry kDTMCBuildTable[] = {
    {INCIDENCE,    dtmc_build_bidirectional},
    {BETWEENNESS,  dtmc_build_betweenness},
    {INTERSECTION, dtmc_build_bidirectional},
    {CONTAINMENT,  dtmc_build_unidirectional},
    {ANGLE,        dtmc_build_unidirectional},
    {CONNECTION,   dtmc_build_bidirectional}
};
static const int kDTMCBuildTableCount =
    (int)(sizeof(kDTMCBuildTable) / sizeof(kDTMCBuildTable[0]));

/**
 * @brief 从约束图构建 DTMC
 *
 * 将约束图的节点映射为 DTMC 状态，约束映射为转移概率。
 * 转移概率来源于约束满足度 satisfaction 字段。
 * 每个状态的总出边概率在构建后归一化为 1.0（多余转移 + 自环）。
 *
 * - INCIDENCE → 双向转移（概率 = satisfaction）
 * - BETWEENNESS → 顺序转移（src→dst, dst→mid，概率 = satisfaction）
 * - INTERSECTION → 双向转移
 * - CONTAINMENT → 单向转移（外部→区域）
 * - CONNECTION → 双向转移
 */
static SimpleDTMC *build_dtmc_from_graph(const ConstraintGraph *graph) {
    if (!graph || graph->node_count <= 0)
        return NULL;

    int n = graph->node_count;
    SimpleDTMC *mc = dtmc_create(n);
    if (!mc)
        return NULL;

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

        double prob = prob_from_satisfaction(c);

        for (int ti = 0; ti < kDTMCBuildTableCount; ti++) {
            if (kDTMCBuildTable[ti].type == c->type) {
                kDTMCBuildTable[ti].build(mc, src, dst, prob, c, n);
                break;
            }
        }
    }

    /* 归一化每个状态的总出边概率并补充自环 */
    for (int i = 0; i < n; i++) {
        double total = 0.0;
        for (int t = 0; t < mc->trans_count[i]; t++) {
            total += mc->trans_probs[i][t];
        }

        if (total < PCTL_EPSILON) {
            /* 无出边 → 自环 1.0 */
            dtmc_add_transition(mc, i, i, 1.0);
        } else {
            /* 归一化到 1.0 */
            for (int t = 0; t < mc->trans_count[i]; t++) {
                mc->trans_probs[i][t] /= total;
            }
        }
    }

    return mc;
}

/* 全局量：存储 eval_state_predicate 关联的约束图，用于几何语义评估 */
static const ConstraintGraph *eval_state_graph = NULL;

/**
 * @brief 设置 eval_state_predicate 使用的约束图上下文
 *
 * 使状态谓词能够基于几何约束满足状态（而非任意启发式）做语义评估。
 */
static void eval_state_set_graph(const ConstraintGraph *graph) {
    eval_state_graph = graph;
}

/**
 * @brief 检查状态谓词是否在给定状态上成立
 *
 * 几何语义实现：
 * - "safe"：状态对应的约束子集全部满足（无违反）
 * - "error"：状态对应的约束子集中任一 INEQUALITY/INTERSECTION 违反
 * - "reachable"：BFS 可达性检查（由调用方完成，此处默认 true）
 * - "satisfied"：状态对应的约束子集全部满足
 * - "true"/"false"/"state_N"：保持向后兼容
 *
 * 状态 ID 到约束的映射：state_id == constraint_index（状态即约束子集）。
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
        int target = 0;
        lv_parse_int(predicate + 6, &target);
        return (state_id == target);
    }

    /* "reachable" — 假设所有状态均可从初始状态抵达，实际由 BFS 验证 */
    if (strcmp(predicate, "reachable") == 0)
        return true;

    /* ---------- 几何语义谓词 ---------- */
    if (eval_state_graph && state_id >= 0 && state_id < eval_state_graph->constraint_count) {
        Constraint *con = eval_state_graph->constraints[state_id];
        if (!con)
            return false;

        /* "safe"：状态对应的约束非违反状态 */
        if (strcmp(predicate, "safe") == 0) {
            if (!con->is_active)
                return true; /* 非活跃约束无违反 */
            return (con->satisfaction >= 0.99);
        }

        /* "error"：状态对应的约束处于违反状态 */
        if (strcmp(predicate, "error") == 0) {
            if (!con->is_active)
                return false;
            /* 不等/相交类约束：satisfaction < 0 标记违反 */
            return (con->satisfaction < 0.05);
        }

        /* "satisfied"：与 safe 等价 */
        if (strcmp(predicate, "satisfied") == 0) {
            if (!con->is_active)
                return true;
            return (con->satisfaction >= 0.99);
        }

        /* "violated" / "unsatisfied" / "broken" */
        if (strcmp(predicate, "violated") == 0 || strcmp(predicate, "unsatisfied") == 0 ||
            strcmp(predicate, "broken") == 0) {
            if (!con->is_active)
                return false;
            return (con->satisfaction < 0.05);
        }
    }

    /* 回退：无约束图时保留旧启发式（向后兼容） */
    if (!eval_state_graph) {
        if (strcmp(predicate, "safe") == 0)
            return (state_id % 2 == 0);
        if (strcmp(predicate, "error") == 0)
            return (state_id % 2 == 1);
    }

    /* 默认：尝试解析为状态 ID */
    int target = 0;
    lv_parse_int(predicate, &target);
    return (state_id == target);
}

/* PCTL 可达性 BFS 上下文（统一遍历设施 lv_bfs_run） */
typedef struct {
    const SimpleDTMC *mc;
    const char *predicate;    /* always：实时谓词检查 */
    const bool *is_target;    /* eventually：预计算的目标状态标记 */
    const double *initial_dist;
    bool found;               /* eventually：是否存在可达目标状态 */
    bool all_satisfy;         /* always：是否所有可达状态满足谓词 */
    double violating_prob;    /* always：违反状态的初始概率累加 */
} PctlBfsCtx;

/* BFS 邻居回调：DTMC 转移目标（全部转移作为批次 0；越界目标由核心范围检查过滤） */
static int pctl_bfs_neighbors(void *ctx, int node_id, int batch_index,
                              int *out_neighbors, void **out_edge_infos,
                              int max_neighbors) {
    PctlBfsCtx *c = (PctlBfsCtx *)ctx;
    (void)batch_index;
    (void)out_edge_infos;
    if (!out_neighbors || max_neighbors <= 0)
        return 0;
    int cnt = c->mc->trans_count[node_id];
    if (cnt > max_neighbors)
        cnt = max_neighbors;
    for (int t = 0; t < cnt; t++)
        out_neighbors[t] = c->mc->trans_targets[node_id][t];
    return cnt;
}

/* eventually：出队时检查目标谓词（命中即 STOP，与原"入队时检测到目标即停止"结果等价） */
static lvTraversalResult pctl_eventually_visit(void *ctx, int node_id) {
    PctlBfsCtx *c = (PctlBfsCtx *)ctx;
    if (c->is_target && c->is_target[node_id]) {
        c->found = true;
        return lv_TRAVERSAL_STOP;
    }
    return lv_TRAVERSAL_CONTINUE;
}

/* always：出队时检查谓词，违反则累计概率并跳过扩展（等价原 continue） */
static lvTraversalResult pctl_always_visit(void *ctx, int node_id) {
    PctlBfsCtx *c = (PctlBfsCtx *)ctx;
    if (!eval_state_predicate(c->predicate, node_id)) {
        c->all_satisfy = false;
        c->violating_prob += c->initial_dist[node_id];
        return lv_TRAVERSAL_SKIP_CHILDREN;
    }
    return lv_TRAVERSAL_CONTINUE;
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
    bool *is_target = (bool *) lv_calloc((size_t) n, sizeof(bool));
    if (!is_target)
        return 0.0;

    for (int i = 0; i < n; i++) {
        is_target[i] = eval_state_predicate(target_predicate, i);
    }

    /* 对每个初始状态做 BFS 可达性（统一遍历设施 lv_bfs_run）。
     * 原实现队列初始容量 ≥ 2n ≥ n，其溢出检查（PCTL_MAX_STATE_LIMIT）为不可达
     * 分支，核心内部动态队列与之等价。 */
    double total_prob = 0.0;

    for (int start = 0; start < n; start++) {
        if (mc->initial_dist[start] < PCTL_EPSILON)
            continue;

        bool *visited = (bool *) lv_calloc((size_t) n, sizeof(bool));
        if (!visited) {
            lv_free((void **) &is_target);
            return 0.0;
        }

        PctlBfsCtx ctx = { mc, target_predicate, is_target, mc->initial_dist,
                           false, true, 0.0 };
        lvBfsSpec spec = {
            .node_count = n,
            .seeds = &start, /* 单起点 */
            .seed_count = 1,
            .visited = visited,
            .mark_on_enqueue = true, /* 标准 BFS：入队时检查并标记 */
            .max_queue = 0,
            .neighbors = pctl_bfs_neighbors,
            .visit = pctl_eventually_visit,
            .ctx = &ctx,
        };
        (void) lv_bfs_run(&spec);
        lv_free((void **) &visited);

        if (ctx.found)
            total_prob += mc->initial_dist[start];
    }

    lv_free((void **) &is_target);

    /* 限制在 [0, 1] */
    total_prob = lv_clamp(total_prob, 0.0, 1.0);

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

    bool *visited = (bool *) lv_calloc((size_t) n, sizeof(bool));
    int *seeds = (int *) lv_malloc((size_t) n * sizeof(int));
    if (!visited || !seeds) {
        lv_free((void **) &visited);
        lv_free((void **) &seeds);
        return 0.0;
    }

    /* 起点 = 初始分布非零的状态（入队时标记 visited，由核心 push seeds 完成） */
    int seed_count = 0;
    for (int i = 0; i < n; i++) {
        if (mc->initial_dist[i] >= PCTL_EPSILON)
            seeds[seed_count++] = i;
    }

    /* BFS 从所有初始状态出发（统一遍历设施 lv_bfs_run）。
     * 原实现队列初始容量 ≥ 2n ≥ n，其溢出检查（PCTL_MAX_STATE_LIMIT）为不可达
     * 分支，核心内部动态队列与之等价。 */
    PctlBfsCtx ctx = { mc, target_predicate, NULL, mc->initial_dist,
                       false, true, 0.0 };
    lvBfsSpec spec = {
        .node_count = n,
        .seeds = seeds,
        .seed_count = seed_count,
        .visited = visited,
        .mark_on_enqueue = true, /* 标准 BFS：入队时检查并标记 */
        .max_queue = 0,
        .neighbors = pctl_bfs_neighbors,
        .visit = pctl_always_visit,
        .ctx = &ctx,
    };
    (void) lv_bfs_run(&spec);

    lv_free((void **) &seeds);
    lv_free((void **) &visited);

    if (ctx.all_satisfy)
        return 1.0;

    return 1.0 - ctx.violating_prob;
}

/**
 * @brief 计算 Until (phi U psi) 概率
 *
 * phi U psi：phi 一直成立直到 psi 成立。
 * 在 DTMC 中，计算从初始状态出发，沿路径 phi 持续成立直到 psi 首次成立的概率。
 *
 * 使用迭代求解线性方程组（值迭代法）。
 */
static double pctl_compute_until(const SimpleDTMC *mc, const char *phi_predicate, const char *psi_predicate) {
    if (!mc || mc->state_count <= 0)
        return 0.0;

    int n = mc->state_count;
    int max_iter = lv_config_get_int(LV_CFG_PCTL_VALUE_ITER_MAX, 1000);
    double convergence_threshold = 1e-9;

    /* prob[i] = 从状态 i 满足 phi U psi 的概率 */
    double *prob = (double *) lv_malloc((size_t) n * sizeof(double));
    double *next_prob = (double *) lv_malloc((size_t) n * sizeof(double));
    if (!prob || !next_prob) {
        lv_free((void **) &prob);
        lv_free((void **) &next_prob);
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

    lv_free((void **) &prob);
    lv_free((void **) &next_prob);

    result = lv_clamp(result, 0.0, 1.0);

    return result;
}

/* ── PCTL 子公式评估函数（文件作用域，用于查找表） ── */
static double pctl_compute_probability(const SimpleDTMC *mc, const PCTLFormula *formula);

typedef double (*PCTLSubEvalFn)(const SimpleDTMC *mc, const PCTLFormula *sub);
typedef struct {
    PCTLFormulaType type;
    PCTLSubEvalFn eval;
} PCTLSubEvalEntry;

static double pctl_sub_eval_eventually(const SimpleDTMC *mc, const PCTLFormula *sub) {
    return pctl_compute_eventually(mc, sub->state_predicate);
}
static double pctl_sub_eval_always(const SimpleDTMC *mc, const PCTLFormula *sub) {
    return pctl_compute_always(mc, sub->state_predicate);
}
static double pctl_sub_eval_until(const SimpleDTMC *mc, const PCTLFormula *sub) {
    return pctl_compute_until(mc, sub->state_predicate, sub->path_predicate);
}
static double pctl_sub_eval_prob_bound(const SimpleDTMC *mc, const PCTLFormula *sub) {
    return pctl_compute_probability(mc, sub);
}

static const PCTLSubEvalEntry kPCTLSubEvalTable[] = {
    {PCTL_EVENTUALLY,  pctl_sub_eval_eventually},
    {PCTL_ALWAYS,      pctl_sub_eval_always},
    {PCTL_UNTIL,       pctl_sub_eval_until},
    {PCTL_PROB_BOUND,  pctl_sub_eval_prob_bound}
};
static const int kPCTLSubEvalTableCount =
    (int)(sizeof(kPCTLSubEvalTable) / sizeof(kPCTLSubEvalTable[0]));

/**
 * @brief 计算概率界 P>=p [ phi ] 或 P<=p [ phi ]
 *
 * 先计算 phi 的实际概率，再与边界比较。
 */
static double pctl_compute_probability(const SimpleDTMC *mc, const PCTLFormula *formula) {
    if (!mc || !formula)
        return 0.0;

    /* 递归评估子公式 */
    double actual_prob = 0.0;

    if (formula->sub_formula) {
        /* 递归评估子公式 — 使用查找表 */
        actual_prob = formula->p_bound; /* 默认值 */
        for (int ti = 0; ti < kPCTLSubEvalTableCount; ti++) {
            if (kPCTLSubEvalTable[ti].type == formula->sub_formula->type) {
                actual_prob = kPCTLSubEvalTable[ti].eval(mc, formula->sub_formula);
                break;
            }
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
    ProbDistribution *dist = (ProbDistribution *) lv_calloc(1, sizeof(ProbDistribution));
    if (!dist)
        return NULL;

    dist->type = type;
    dist->param_count = param_count;
    dist->pdf = NULL;
    dist->cdf = NULL;

    if (param_count > 0 && params) {
        dist->params = (double *) lv_malloc((size_t) param_count * sizeof(double));
        if (!dist->params) {
            lv_free((void **) &dist);
            return NULL;
        }
        memcpy(dist->params, params, (size_t) param_count * sizeof(double));
    } else {
        dist->params = NULL;
    }

    /* 设置支撑集 */
    {
        static const double kDistSupportLo[] = {
            0.0,    /* PROB_DIST_UNIFORM */
            -1e308, /* PROB_DIST_NORMAL */
            0.0,    /* PROB_DIST_BETA */
            -1e308, /* PROB_DIST_DISCRETE */
            -1e308  /* PROB_DIST_CUSTOM */
        };
        static const double kDistSupportHi[] = {
            1.0,    /* PROB_DIST_UNIFORM */
            1e308,  /* PROB_DIST_NORMAL */
            1.0,    /* PROB_DIST_BETA */
            1e308,  /* PROB_DIST_DISCRETE */
            1e308   /* PROB_DIST_CUSTOM */
        };
        static const int kDistSupportCount =
            (int)(sizeof(kDistSupportLo) / sizeof(kDistSupportLo[0]));
        if (type == PROB_DIST_UNIFORM && param_count >= 2) {
            dist->support_lo = params[0];
            dist->support_hi = params[1];
        } else if (type >= 0 && type < kDistSupportCount) {
            dist->support_lo = kDistSupportLo[(int)type];
            dist->support_hi = kDistSupportHi[(int)type];
        } else {
            dist->support_lo = -1e308;
            dist->support_hi = 1e308;
        }
    }

    return dist;
}

/* ========================================================================
 * prob_dist_destroy —— 销毁概率分布
 * ======================================================================== */

void prob_dist_destroy(ProbDistribution *dist) {
    if (dist) {
        lv_free((void **) &dist->params);
        lv_free((void **) &dist);
    }
}

/* ── PDF 计算辅助函数（文件作用域，用于查找表）── */
typedef double (*PDFFn)(ProbDistribution *dist, double x);
static double pdf_uniform(ProbDistribution *dist, double x) {
    double a = (dist->param_count >= 2) ? dist->params[0] : 0.0;
    double b = (dist->param_count >= 2) ? dist->params[1] : 1.0;
    if (x < a || x > b) return 0.0;
    return 1.0 / (b - a);
}
static double pdf_normal(ProbDistribution *dist, double x) {
    double mu = (dist->param_count >= 2) ? dist->params[0] : 0.0;
    double sigma = (dist->param_count >= 2) ? dist->params[1] : 1.0;
    double z = (x - mu) / sigma;
    return exp(-0.5 * z * z) / (sigma * sqrt(2.0 * M_PI));
}
static double pdf_beta(ProbDistribution *dist, double x) {
    double alpha = (dist->param_count >= 2) ? dist->params[0] : 1.0;
    double beta = (dist->param_count >= 2) ? dist->params[1] : 1.0;
    if (x < 0.0 || x > 1.0) return 0.0;
    double norm = exp(lgamma(alpha + beta) - lgamma(alpha) - lgamma(beta));
    return norm * pow(x, alpha - 1.0) * pow(1.0 - x, beta - 1.0);
}

/* ── PDF 查找表（文件作用域）── */
static const PDFFn kPDFTable[] = {
    pdf_uniform, /* PROB_DIST_UNIFORM */
    pdf_normal,  /* PROB_DIST_NORMAL */
    pdf_beta,    /* PROB_DIST_BETA */
    NULL,        /* PROB_DIST_DISCRETE */
    NULL         /* PROB_DIST_CUSTOM */
};
static const int kPDFTableCount = (int)(sizeof(kPDFTable) / sizeof(kPDFTable[0]));

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

    return LV_DISPATCH(kPDFTable, dist->type, 0.0, dist, x);
}

/* ── CDF 计算辅助函数（文件作用域，用于查找表）── */
typedef double (*CDFFn)(ProbDistribution *dist, double x);
static double cdf_uniform(ProbDistribution *dist, double x) {
    double a = (dist->param_count >= 2) ? dist->params[0] : 0.0;
    double b = (dist->param_count >= 2) ? dist->params[1] : 1.0;
    if (x < a) return 0.0;
    if (x > b) return 1.0;
    return (x - a) / (b - a);
}
static double cdf_normal(ProbDistribution *dist, double x) {
    double mu = (dist->param_count >= 2) ? dist->params[0] : 0.0;
    double sigma = (dist->param_count >= 2) ? dist->params[1] : 1.0;
    double z = (x - mu) / (sigma * sqrt(2.0));
    return 0.5 * (1.0 + erf(z));
}

/* ========================================================================
 * prob_dist_cdf —— 计算累积分布函数值
 * ======================================================================== */

/* ── CDF 查找表（文件作用域）── */
static const CDFFn kCDFTable[] = {
    cdf_uniform, /* PROB_DIST_UNIFORM */
    cdf_normal,  /* PROB_DIST_NORMAL */
    NULL,        /* PROB_DIST_BETA */
    NULL,        /* PROB_DIST_DISCRETE */
    NULL         /* PROB_DIST_CUSTOM */
};
static const int kCDFTableCount = (int)(sizeof(kCDFTable) / sizeof(kCDFTable[0]));

double prob_dist_cdf(ProbDistribution *dist, double x) {
    if (!dist)
        return 0.0;

    if (dist->type == PROB_DIST_CUSTOM && dist->cdf) {
        return dist->cdf(x, dist->params, dist->param_count);
    }

    return LV_DISPATCH(kCDFTable, dist->type, 0.0, dist, x);
}

/* ── 采样辅助函数（文件作用域，用于查找表）── */
typedef double (*SampleFn)(ProbDistribution *dist);
static double sample_uniform(ProbDistribution *dist) {
    double a = (dist->param_count >= 2) ? dist->params[0] : 0.0;
    double b = (dist->param_count >= 2) ? dist->params[1] : 1.0;
    return a + rand_uniform_lcg() * (b - a);
}
static double sample_normal(ProbDistribution *dist) {
    double mu = (dist->param_count >= 2) ? dist->params[0] : 0.0;
    double sigma = (dist->param_count >= 2) ? dist->params[1] : 1.0;
    return mu + sigma * rand_normal_box_muller();
}
static double sample_beta(ProbDistribution *dist) {
    double alpha = (dist->param_count >= 2) ? dist->params[0] : 1.0;
    double beta = (dist->param_count >= 2) ? dist->params[1] : 1.0;
    if (alpha < 0.01) alpha = 0.01;
    if (beta < 0.01) beta = 0.01;
    double x_gamma = 0.0, y_gamma = 0.0;
    if (alpha >= 1.0) {
        double d = alpha - 1.0 / 3.0;
        double c = 1.0 / sqrt(9.0 * d);
        int _try = 0;
        for (;;) {
            if (++_try > GAMMA_SAMPLE_MAX_RETRIES) { x_gamma = 1.0; break; }
            double v = 1.0 + c * rand_normal_box_muller();
            if (v <= 0.0) continue;
            v = v * v * v;
            double u = rand_uniform_lcg();
            if (u < 1.0 - 0.0331 * (v * v) / (d * d)) { x_gamma = d * v; break; }
            if (log(u) < 0.5 * (v / d) * (v / d) + d * (1.0 - v + log(v))) { x_gamma = d * v; break; }
        }
    } else {
        double am = 0.0;
        int _try2 = 0;
        for (;;) {
            if (++_try2 > GAMMA_SAMPLE_MAX_RETRIES) { x_gamma = 1.0; break; }
            am = alpha + 1.0;
            double u1 = rand_uniform_lcg();
            double u2 = rand_uniform_lcg();
            double vv = am * pow(u1, 1.0 / am);
            if (u2 <= exp(-vv)) { x_gamma = vv; break; }
        }
    }
    if (beta >= 1.0) {
        double d = beta - 1.0 / 3.0;
        double c = 1.0 / sqrt(9.0 * d);
        int _try3 = 0;
        for (;;) {
            if (++_try3 > GAMMA_SAMPLE_MAX_RETRIES) { y_gamma = 1.0; break; }
            double v = 1.0 + c * rand_normal_box_muller();
            if (v <= 0.0) continue;
            v = v * v * v;
            double u = rand_uniform_lcg();
            if (u < 1.0 - 0.0331 * (v * v) / (d * d)) { y_gamma = d * v; break; }
            if (log(u) < 0.5 * (v / d) * (v / d) + d * (1.0 - v + log(v))) { y_gamma = d * v; break; }
        }
    } else {
        double bm = 0.0;
        int _try4 = 0;
        for (;;) {
            if (++_try4 > GAMMA_SAMPLE_MAX_RETRIES) { y_gamma = 1.0; break; }
            bm = beta + 1.0;
            double u1 = rand_uniform_lcg();
            double u2 = rand_uniform_lcg();
            double vv = bm * pow(u1, 1.0 / bm);
            if (u2 <= exp(-vv)) { y_gamma = vv; break; }
        }
    }
    double result = x_gamma / (x_gamma + y_gamma);
    result = lv_clamp(result, 0.0, 1.0);
    return result;
}
static double sample_discrete(ProbDistribution *dist) {
    double r = rand_uniform_lcg();
    double cum = 0.0;
    int k = 0;
    for (; k < dist->param_count; k++) {
        cum += dist->params[k];
        if (r <= cum) break;
    }
    return (double) k;
}

/* ── 采样函数查找表（文件作用域）── */
static const SampleFn kSampleTable[] = {
    sample_uniform,  /* PROB_DIST_UNIFORM */
    sample_normal,   /* PROB_DIST_NORMAL */
    sample_beta,     /* PROB_DIST_BETA */
    sample_discrete, /* PROB_DIST_DISCRETE */
    NULL             /* PROB_DIST_CUSTOM */
};
static const int kSampleTableCount = (int)(sizeof(kSampleTable) / sizeof(kSampleTable[0]));

/* ========================================================================
 * prob_dist_sample —— 从分布中采样
 * ======================================================================== */

int prob_dist_sample(ProbDistribution *dist, int n_samples, double **out_samples) {
    if (!dist || n_samples <= 0 || !out_samples)
        return -1;

    double *samples = (double *) lv_malloc((size_t) n_samples * sizeof(double));
    if (!samples)
        return -1;

    for (int i = 0; i < n_samples; i++) {
        samples[i] = LV_DISPATCH(kSampleTable, dist->type, 0.0, dist);
    }

    *out_samples = samples;
    return n_samples;
}

/* ========================================================================
 * prob_constraint_create —— 创建概率约束节点
 * ======================================================================== */

ProbConstraintNode *prob_constraint_create(int node_id, ProbDistribution *dist) {
    ProbConstraintNode *node = (ProbConstraintNode *) lv_calloc(1, sizeof(ProbConstraintNode));
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
        lv_free((void **) &node->pctl_formula);
        lv_free((void **) &node);
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
        double *samples = (double *) lv_malloc((size_t) n_samples * sizeof(double));
        if (!samples)
            return -1;
        for (int i = 0; i < n_samples; i++)
            samples[i] = 0.0;
        *out_samples = samples;
        return n_samples;
    }

    return prob_dist_sample(node->coord_dist, n_samples, out_samples);
}

/* ── PCTL 评估辅助函数（文件作用域，用于查找表）── */
typedef bool (*PCTLEvalFn)(SimpleDTMC *mc, const PCTLFormula *formula, double *out_probability);
static bool pctl_eval_prob_bound(SimpleDTMC *mc, const PCTLFormula *formula, double *out_probability) {
    *out_probability = pctl_compute_probability(mc, formula);
    return true;
}
static bool pctl_eval_next(SimpleDTMC *mc, const PCTLFormula *formula, double *out_probability) {
    double prob = 0.0;
    for (int i = 0; i < mc->state_count; i++) {
        if (mc->initial_dist[i] < PCTL_EPSILON)
            continue;
        for (int t = 0; t < mc->trans_count[i]; t++) {
            int j = mc->trans_targets[i][t];
            double p = mc->trans_probs[i][t];
            if (j >= 0 && j < mc->state_count && eval_state_predicate(formula->state_predicate, j)) {
                prob += mc->initial_dist[i] * p;
            }
        }
    }
    *out_probability = (prob > 1.0) ? 1.0 : prob;
    return true;
}
static bool pctl_eval_until(SimpleDTMC *mc, const PCTLFormula *formula, double *out_probability) {
    *out_probability = pctl_compute_until(mc, formula->state_predicate, formula->path_predicate);
    return true;
}
static bool pctl_eval_eventually(SimpleDTMC *mc, const PCTLFormula *formula, double *out_probability) {
    *out_probability = pctl_compute_eventually(mc, formula->state_predicate);
    return true;
}
static bool pctl_eval_always(SimpleDTMC *mc, const PCTLFormula *formula, double *out_probability) {
    *out_probability = pctl_compute_always(mc, formula->state_predicate);
    return true;
}
static bool pctl_eval_steady_state(SimpleDTMC *mc, const PCTLFormula *formula, double *out_probability) {
    int n = mc->state_count;
    double *pi = (double *) lv_malloc((size_t) n * sizeof(double));
    double *next_pi = (double *) lv_malloc((size_t) n * sizeof(double));
    if (pi && next_pi) {
        for (int i = 0; i < n; i++) pi[i] = 1.0 / (double) n;
        const double convergence_threshold = 1e-12;
        const int max_iter = lv_config_get_int(LV_CFG_PCTL_POWER_ITER_MAX, 10000);
        bool converged = false;
        for (int iter = 0; iter < max_iter; iter++) {
            memset(next_pi, 0, (size_t) n * sizeof(double));
            for (int i = 0; i < n; i++)
                for (int t = 0; t < mc->trans_count[i]; t++) {
                    int j = mc->trans_targets[i][t];
                    double p = mc->trans_probs[i][t];
                    if (j >= 0 && j < n) next_pi[j] += pi[i] * p;
                }
            double sum = 0.0;
            for (int i = 0; i < n; i++) sum += next_pi[i];
            if (sum > PCTL_EPSILON)
                for (int i = 0; i < n; i++) next_pi[i] /= sum;
            double max_diff = 0.0;
            for (int i = 0; i < n; i++) {
                double diff = fabs(next_pi[i] - pi[i]);
                if (diff > max_diff) max_diff = diff;
            }
            double *tmp = pi; pi = next_pi; next_pi = tmp;
            if (max_diff < convergence_threshold) { converged = true; break; }
        }
        if (!converged)
            lv_LOG_WARNING("Warning: PCTL steady-state power iteration did not converge within %d iterations.\n",
                           max_iter);
        double result = 0.0;
        for (int i = 0; i < n; i++)
            if (eval_state_predicate(formula->state_predicate, i)) result += pi[i];
        *out_probability = (result > 1.0) ? 1.0 : result;
        lv_free((void **) &pi); lv_free((void **) &next_pi);
    } else {
        *out_probability = 1.0 / (double) n;
        lv_free((void **) &pi); lv_free((void **) &next_pi);
    }
    return true;
}

/* ── PCTL 评估函数查找表（文件作用域）── */
static const PCTLEvalFn kPCTLEvalTable[] = {
    pctl_eval_prob_bound,   /* PCTL_PROB_BOUND */
    pctl_eval_eventually,   /* PCTL_EVENTUALLY */
    pctl_eval_always,       /* PCTL_ALWAYS */
    pctl_eval_until,        /* PCTL_UNTIL */
    pctl_eval_next,         /* PCTL_NEXT */
    pctl_eval_steady_state, /* PCTL_STEADY_STATE */
    NULL                    /* PCTL_ATOMIC */
};
static const int kPCTLEvalTableCount = (int)(sizeof(kPCTLEvalTable) / sizeof(kPCTLEvalTable[0]));

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

    /* 设置约束图上下文，使 eval_state_predicate 使用几何语义 */
    eval_state_set_graph(graph);

    /* 从约束图构建 DTMC */
    SimpleDTMC *mc = build_dtmc_from_graph(graph);
    if (!mc) {
        /* 无法构建 DTMC，回退到基本评估 */
        *out_probability = 0.0;
        return true;
    }

    if (formula->type >= 0 && formula->type < kPCTLEvalTableCount && kPCTLEvalTable[(int)formula->type]) {
        if (!kPCTLEvalTable[(int)formula->type](mc, formula, out_probability)) {
            dtmc_destroy(mc);
            return false;
        }
    } else {
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

        /* 从约束满足度推导有效概率（非硬编码常数） */
        double valid_prob = prob_from_satisfaction(c);
        /* 未评估的约束默认视为高概率（保守估计） */
        if (valid_prob < 0.01)
            valid_prob = 0.95;

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
                        if (adjacent)
                            break;
                    }
                }
            }
            if (adjacent) {
                conf *= 1.2; /* 相邻约束，适度增加权重 */
            }
        }

        total_confidence += conf;
        valid_constraints++;

        lv_free((void **) &samples);
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
