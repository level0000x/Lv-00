/**
 * @file probabilistic_constraint.c
 * @brief PRISM 概率模型检验 —— 桩实现
 *
 * 提供概率分布、概率约束节点和 PCTL 评估的框架实现。
 * 包含 Box-Muller 正态采样、逆 CDF 采样等基础采样方法。
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
 *
 * 根据分布类型使用不同的采样方法：
 * - UNIFORM: 逆 CDF 线性变换
 * - NORMAL: Box-Muller 变换
 * - BETA: 拒绝采样
 * - DISCRETE: 离散逆 CDF
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
                /* 简化采样：使用均匀随机数 Metropolis-Hastings
             * 桩实现：生成均匀随机数作为近似 */
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
 * 桩实现：根据 PCTL 公式类型做框架评估
 * ======================================================================== */

bool pctl_evaluate(const ConstraintGraph *graph, const PCTLFormula *formula, double *out_probability) {
    if (!graph || !formula || !out_probability)
        return false;

    *out_probability = 0.0;

    switch (formula->type) {
        case PCTL_PROB_BOUND:
            /* 概率边界：检查是否满足 P~p [ phi ]
         * 桩：返回值为 p_bound 本身 */
            *out_probability = formula->p_bound;
            break;

        case PCTL_NEXT:
            /* 下一状态：X phi
         * 桩：如果存在邻接节点 >= 1.0，否则 0.0 */
            *out_probability = (graph->node_count > 1) ? 1.0 : 0.0;
            break;

        case PCTL_UNTIL:
            /* phi U psi
         * 桩：基础返回 0.5 */
            *out_probability = 0.5;
            break;

        case PCTL_EVENTUALLY:
            /* F phi：最终满足
         * 桩：如状态谓词非空 >= 1.0，否则 0.0 */
            *out_probability = (formula->state_predicate && strlen(formula->state_predicate) > 0) ? 1.0 : 0.0;
            break;

        case PCTL_ALWAYS:
            /* G phi：总是满足
         * 框：估算为 1.0（乐观假设） */
            *out_probability = 1.0;
            break;

        case PCTL_STEADY_STATE:
            /* S~p [ phi ]：稳态概率
         * 桩：当节点数 > 0 时返回均匀分布概率 */
            *out_probability = (graph->node_count > 0) ? (1.0 / (double) graph->node_count) : 0.0;
            break;

        default:
            return false;
    }

    return true;
}

/* ========================================================================
 * pctl_check_constructibility —— PCTL 构造性检查
 *
 * 通过对概率分布进行 Monte Carlo 采样（默认 N=1000 次），
 * 统计满足约束的有效构造比例。
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
        (void) c; /* 框：假设所有约束都有解 */

        /* 简化：每种约束类型有特定有效概率 */
        double valid_prob = 1.0;
        if (c->type == INTERSECTION) {
            valid_prob = 0.95; /* 相交约束通常可满足 */
        } else if (c->type == BETWEENNESS) {
            valid_prob = 0.90; /* 介于约束略微严格 */
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
 *
 * 使用置信度网络传播的概念传播，从一组概率约束推导目标变量的置信度。
 * ======================================================================== */

bool prob_constraint_infer(const ConstraintGraph *graph, int target_var, ProbConstraintNode **constraints, int n,
                           double *out_conf) {
    if (!graph || !constraints || n <= 0 || !out_conf)
        return false;

    /* 框实现：使用朴素置信度推理
     * 对每个约束独立采样，汇总后计算置信区间
     */
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

        /* 计算样本置信度（简化：使用约束概率） */
        double conf = cn->is_soft ? cn->probability : 1.0;
        total_confidence += conf;
        valid_constraints++;

        lv00_free((void **)&samples);
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

    (void) target_var; /* 框实现中未使用目标变量 */
    return true;
}
