/**
 * @file approx_counter.h
 * @brief ApproxMC 近似模型计数 —— PAC 保证的 #SAT 近似求解
 *
 * 借鉴 ApproxMC (github.com/meelgroup/approxmc) 的近似模型计数架构，
 * 为 Lv-00 提供带 PAC (Probably Approximately Correct) 保证的
 * 约束图模型计数能力。
 *
 * 设计借鉴：
 * - ApproxMC — 基于 XOR 哈希的近似 #SAT 求解器
 * - UniGen — 近似均匀采样器
 *
 * 核心思路：
 * 将约束图编码为 CNF，对每个约束的可满足赋值进行统计。
 * 使用 ApproxMC 的 `cell_sol_count * 2^hash_count` 公式进行估计。
 *
 * @version 1.1.0
 * @date 2026-05-24
 */
#ifndef LV00_APPROX_COUNTER_H
#define LV00_APPROX_COUNTER_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>
#include <stdint.h>
#include "constraint_graph.h"
/* ========================================================================
 * PAC 配置与计数结果
 * ======================================================================== */
/** PAC (Probably Approximately Correct) 配置 */
typedef struct {
    /** 精度参数：允许的相对误差（如 0.1 表示 ±10%） */
    double epsilon;
    /** 置信度参数：成功概率下界（如 0.99 表示 99% 置信度） */
    double delta;
    /** 随机种子（用于哈希函数生成） */
    int seed;
    /** 是否使用稀疏 XOR 哈希（稀疏适合大变量数） */
    bool sparse_xor;
    /** 哈希函数数量（0 = 自动选择，基于 epsilon/delta） */
    int num_hashes;
} PacConfig;
/** ApproxMC 近似模型计数结果 */
typedef struct {
    /** 单个哈希桶中采样的解数量 */
    uint64_t cell_sol_count;
    /** 实际使用的哈希函数层级数 */
    int hash_count;
    /** 估算的模型总数（cell_sol_count * 2^hash_count） */
    uint64_t total_count;
    /** PAC 置信度下界（0.0 ~ 1.0） */
    double confidence;
    /** 状态消息（如 "Model count: ~12345 with 99% confidence"） */
    char *status_msg;
} ApproxCountResult;
/* ========================================================================
 * 变量权重（用于加权计数）
 * ======================================================================== */
/** 变量权重（ApproxMC 加权模型计数） */
typedef struct {
    /** 变量 ID（对应约束图中的变量序号） */
    int var_id;
    /** 变量权重（1.0 = 无偏，>1 = 放大，<1 = 缩小） */
    double weight;
    /** 是否为固定变量（固定变量不参与计数，已知取值） */
    bool is_fixed;
} VarWeight;
/* ========================================================================
 * 近似模型计数 API
 * ======================================================================== */
/**
 * @brief 对约束图进行近似模型计数（PAC 保证）
 *
 * 使用 ApproxMC 风格的 XOR 哈希框架估计约束图的可满足赋值总数。
 * 结果以 PAC 保证返回，即：
 *   Pr[|total_count - true_count| <= epsilon * true_count] >= 1 - delta
 *
 * @param[in]  graph  约束图（非 NULL）
 * @param[in]  cfg    PAC 配置（非 NULL）
 * @param[out] out    计数结果（非 NULL，调用者用 approx_count_result_free 释放）
 * @return true 计数成功，false 失败（参数错误或内存不足）
 */
bool approx_count_solutions(const ConstraintGraph *graph, const PacConfig *cfg, ApproxCountResult *out);
/**
 * @brief 投影模型计数（只计指定变量的不同赋值）
 *
 * 与 approx_count_solutions 类似，但仅统计投影变量集合的
 * 不同赋值组合数，忽略非投影变量的取值。
 *
 * @param[in]  graph        约束图（非 NULL）
 * @param[in]  proj_vars    投影变量 ID 数组
 * @param[in]  proj_count   投影变量数量
 * @param[in]  cfg          PAC 配置
 * @param[out] out          计数结果
 * @return true 成功，false 失败
 */
bool approx_count_projected(const ConstraintGraph *graph, int *proj_vars, int proj_count, const PacConfig *cfg,
                            ApproxCountResult *out);
/**
 * @brief 将约束图编码为 DIMACS CNF 格式字符串
 *
 * 使用 Tseitin 变换将约束图编码为合取范式。
 * 遍历所有约束节点，对每个约束生成对应的子句。
 *
 * 编码映射：
 * - 几何节点位置 → 布尔变量（bit-blast 坐标编码）
 * - 约束关系 → 命题子句（Tseitin 变换）
 *
 * @param[in]  graph          约束图（非 NULL）
 * @param[out] out_cnf_vars   输出：CNF 变量数量（可为 NULL）
 * @return DIMACS CNF 格式字符串（调用者负责 free），失败返回 NULL
 */
char *approx_count_to_sat(const ConstraintGraph *graph, int *out_cnf_vars);
/**
 * @brief 计算指定配置和结果下的 PAC 置信度
 *
 * 根据 PAC 配置和计数结果，按 Chernoff-Hoeffding 界计算
 * 实际置信度下界。
 *
 * @param[in] cfg  PAC 配置
 * @param[in] res  计数结果
 * @return 置信度下界（0.0 ~ 1.0）
 */
double approx_count_get_pac_bound(const PacConfig *cfg, const ApproxCountResult *res);
/**
 * @brief 释放 ApproxCountResult 占用的资源
 *
 * @param[in,out] res 计数结果
 */
void approx_count_result_free(ApproxCountResult *res);
/* ========================================================================
 * 近似构造性判断
 * ======================================================================== */
/**
 * @brief 近似构造性判断
 *
 * 当近似模型计数 > 0 时，判断约束图是否近似可构造。
 * 如果 total_count == 0，说明约束系统无解，不可构造。
 * 如果 total_count > 0，说明存在至少一个有效构造。
 *
 * @param[in] graph      约束图
 * @param[in] min_prob   最小概率阈值（如 0.95 表示至少 95% 置信度要求可构造）
 * @return true 近似可构造，false 不可构造或无法判定
 */
bool is_approximately_constructible(const ConstraintGraph *graph, double min_prob);
#ifdef __cplusplus
}
#endif
#endif /* LV00_APPROX_COUNTER_H */
