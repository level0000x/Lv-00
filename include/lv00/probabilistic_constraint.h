/**
 * @file probabilistic_constraint.h
 * @brief PRISM 概率模型检测 —— 概率约束与 PCTL 评估
 *
 * 借鉴 PRISM (prismmodelchecker.org) 的概率模型检测框架，
 * 为 Lv-00 提供概率分布约束、PCTL 公式评估与概率推理能力。
 *
 * 设计借鉴：
 * - PRISM — Probabilistic Model Checker
 *   - DTMC/MDP/CTMC 模型类型
 *   - PCTL (Probabilistic Computation Tree Logic) 规约语言
 *   - 统计模型检测（SMC，Statistical Model Checking）
 *   - 符号化引擎（MTBDD/稀疏矩阵）
 *
 * Lv-00 适配：
 * - 约束图节点 → PRISM 状态
 * - 概率分布 → 状态转移不确定性
 * - PCTL 公式 → 构造性/可满足性判断
 *
 * @version v3.3.0
 * @date 2026-05-24
 */

#ifndef LV00_PROBABILISTIC_CONSTRAINT_H
#define LV00_PROBABILISTIC_CONSTRAINT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "constraint_graph.h"
#include <stdbool.h>
#include <stdint.h>

/* ========================================================================
 * 概率分布类型
 * ======================================================================== */

/** 概率分布类型枚举 */
typedef enum {
    PROB_DIST_UNIFORM = 0,  /**< 均匀分布 U(a, b) */
    PROB_DIST_NORMAL  = 1,  /**< 正态分布 N(mu, sigma^2) */
    PROB_DIST_DISCRETE = 2, /**< 离散分布（自定义概率质量函数） */
    PROB_DIST_BETA    = 3,  /**< Beta 分布 Beta(alpha, beta) */
    PROB_DIST_CUSTOM  = 4   /**< 自定义分布（用户提供 PDF/CDF） */
} ProbDistType;

/** 概率分布结构体 */
typedef struct {
    /** 分布类型 */
    ProbDistType type;

    /** 分布参数数组
     * UNIFORM: [a, b]
     * NORMAL:  [mu, sigma]
     * BETA:    [alpha, beta]
     * DISCRETE: [p1, p2, ..., pn]（概率质量）
     * CUSTOM:  用户自定义
     */
    double *params;
    int param_count;

    /** 自定义概率密度函数（仅 CUSTOM 类型使用） */
    double (*pdf)(double x, double *params, int n);

    /** 自定义累积分布函数（仅 CUSTOM 类型使用） */
    double (*cdf)(double x, double *params, int n);

    /** 分布支撑集下界 */
    double support_lo;

    /** 分布支撑集上界 */
    double support_hi;
} ProbDistribution;

/* ========================================================================
 * 概率约束节点
 * ======================================================================== */

/** 概率约束节点
 *
 * 将约束图中的节点替换为概率分布约束：
 * - 坐标不再是确定值，而是服从某概率分布
 * - 约束变为概率约束（以一定概率成立）
 * - 可定义软约束（is_soft=true）和硬约束（is_soft=false）
 */
typedef struct {
    /** 对应的基础约束图节点 ID */
    int base_node_id;

    /** 坐标的概率分布 */
    ProbDistribution *coord_dist;

    /** 是否为软约束（软约束允许以一定概率违反） */
    bool is_soft;

    /** 约束成立的概率（is_soft=true 时有效，范围 [0,1]） */
    double probability;

    /** PCTL 公式字符串表示（如 "P>=0.95 [ F (reachable) ]"） */
    char *pctl_formula;
} ProbConstraintNode;

/* ========================================================================
 * PCTL 公式
 * ======================================================================== */

/** PCTL 公式类型枚举 */
typedef enum {
    PCTL_PROB_BOUND  = 0,  /**< P~p [ phi ] — 概率边界 */
    PCTL_NEXT        = 1,  /**< X phi — 下一状态满足 phi */
    PCTL_UNTIL       = 2,  /**< phi U psi — phi 一直成立直到 psi */
    PCTL_EVENTUALLY  = 3,  /**< F phi — 最终满足 phi */
    PCTL_ALWAYS      = 4,  /**< G phi — 总是满足 phi */
    PCTL_STEADY_STATE = 5 /**< S~p [ phi ] — 稳态概率满足 phi */
} PCTLFormulaType;

/** PCTL 公式（概率计算树逻辑公式） */
typedef struct PCTLFormula {
    /** 公式类型 */
    PCTLFormulaType type;

    /** 状态谓词（命题公式，描述状态属性） */
    char *state_predicate;

    /** 路径谓词（用于 UNTIL 等时序算子） */
    char *path_predicate;

    /** 概率边界值（如 0.95） */
    double p_bound;

    /** 边界方向：true=上界(P<=p)，false=下界(P>=p) */
    bool upper_bound;

    /** 子公式（用于嵌套 PCTL 公式，如 F (X phi)） */
    struct PCTLFormula *sub_formula;
} PCTLFormula;

/* ========================================================================
 * 概率分布操作
 * ======================================================================== */

/**
 * @brief 创建概率分布
 *
 * @param[in] type       分布类型
 * @param[in] params     参数数组
 * @param[in] param_count 参数数量
 * @return 新分布，失败返回 NULL
 */
ProbDistribution *prob_dist_create(ProbDistType type,
                                    double *params,
                                    int param_count);

/**
 * @brief 销毁概率分布
 */
void prob_dist_destroy(ProbDistribution *dist);

/**
 * @brief 计算分布的 PDF 值
 */
double prob_dist_pdf(ProbDistribution *dist, double x);

/**
 * @brief 计算分布的 CDF 值
 */
double prob_dist_cdf(ProbDistribution *dist, double x);

/**
 * @brief 从分布中采样
 *
 * @param[in]  dist        概率分布
 * @param[in]  n_samples   采样数量
 * @param[out] out_samples 输出样本数组（调用者负责 free）
 * @return 实际采样数量，失败返回 -1
 */
int prob_dist_sample(ProbDistribution *dist,
                      int n_samples,
                      double **out_samples);

/* ========================================================================
 * 概率约束管理
 * ======================================================================== */

/**
 * @brief 创建概率约束节点
 *
 * @param[in] node_id 基础节点 ID
 * @param[in] dist    坐标概率分布（可为 NULL，表示确定性坐标）
 * @return 新概率约束节点，失败返回 NULL
 */
ProbConstraintNode *prob_constraint_create(int node_id,
                                            ProbDistribution *dist);

/**
 * @brief 销毁概率约束节点
 */
void prob_constraint_destroy(ProbConstraintNode *node);

/**
 * @brief 从概率约束节点采样坐标
 *
 * 从关联的概率分布中采样 n 个坐标值。
 *
 * @param[in]  node        概率约束节点
 * @param[in]  n_samples   采样数量
 * @param[out] out_samples 输出样本数组（n_samples 个 double，调用者负责 free）
 * @return 采样数量，失败返回 -1
 */
int prob_constraint_sample(ProbConstraintNode *node,
                            int n_samples,
                            double **out_samples);

/* ========================================================================
 * PCTL 评估
 * ======================================================================== */

/**
 * @brief 在约束图上评估 PCTL 公式
 *
 * 对约束图的状态空间评估概率计算树逻辑公式，
 * 计算满足公式的概率。
 *
 * @param[in]  graph           约束图（看作 DTMC/MDP 状态空间）
 * @param[in]  formula         PCTL 公式
 * @param[out] out_probability 输出：满足公式的概率（0.0 ~ 1.0）
 * @return true 评估成功，false 失败
 */
bool pctl_evaluate(const ConstraintGraph *graph,
                   const PCTLFormula *formula,
                   double *out_probability);

/**
 * @brief PCTL 构造性检查
 *
 * 以给定置信度判断构造是否有效。
 * 通过对概率分布进行 Monte Carlo 采样（默认 N=1000 次），
 * 统计满足约束的有效构造比例。
 *
 * @param[in] graph       约束图
 * @param[in] confidence  置信度阈值（如 0.95）
 * @return true 以 probability >= confidence 的概率可构造，false 不可
 */
bool pctl_check_constructibility(const ConstraintGraph *graph,
                                  double confidence);

/* ========================================================================
 * 概率推理
 * ======================================================================== */

/**
 * @brief 概率约束推理
 *
 * 给定一组概率约束节点，推断目标变量的置信度。
 * 使用贝叶斯网络风格的信念传播。
 *
 * @param[in]  graph         约束图
 * @param[in]  target_var    目标变量 ID（约束图中的节点）
 * @param[in]  constraints   概率约束数组
 * @param[in]  n             约束数量
 * @param[out] out_conf      输出：推断置信度（0.0 ~ 1.0）
 * @return true 推理成功，false 失败
 */
bool prob_constraint_infer(const ConstraintGraph *graph,
                            int target_var,
                            ProbConstraintNode **constraints,
                            int n,
                            double *out_conf);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PROBABILISTIC_CONSTRAINT_H */
