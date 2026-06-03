/**
 * @file proof_score.h
 * @brief 证明有效性评分 —— 多维度质量评估与证明比较
 *
 * @details 为证明提供多维度的自动评分系统。
 *
 *          评分维度：
 *          1. 简洁性 (Brevity)：
 *             - 步骤越少越好
 *             - 但不过分压缩（否则可读性差）
 *             - 评分 = 1.0 - (steps / max_allowed) 当 steps <= max_allowed，否则 0
 *
 *          2. 优雅性 (Elegance)：
 *             - 公理使用的多样性
 *             - 避免过度依赖单一策略
 *             - 辅助构造的"自然"程度
 *             - 评分 = 使用的不同公理种类 / 总可用公理种类
 *
 *          3. 完备性 (Completeness)：
 *             - 所有断言有据
 *             - 无未论证的跳跃
 *             - 评分 = 有据步骤数 / 总步骤数
 *
 *          4. 深度 (Depth)：
 *             - 证明的逻辑深度
 *             - 最深的依赖链长度
 *             - 既不要过浅（不够严密）也不要过深（不够直观）
 *
 *          5. 冗余度 (Redundancy Ratio)：
 *             - 冗余步骤的比例
 *             - 评分 = 1.0 - (冗余步骤数 / 总步骤数)
 *             - 冗余步骤：该类步骤可被移除而不影响结论
 *
 *          综合评分 = w1*简洁性 + w2*优雅性 + w3*完备性 + w4*(1-冗余度)  (可配置权重)
 *
 *          应用场景：
 *          - Sledgehammer 多策略调度时选择最优证明
 *          - 自动证明搜索时的启发式指导
 *          - 教学/展示时优先呈现最优雅的证明
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#ifndef LV00_PROOF_SCORE_H
#define LV00_PROOF_SCORE_H

#include <stdbool.h>

#include "proof.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============== 前向声明 ============== */
typedef struct Lv00ProofScore Lv00ProofScore;
typedef struct Lv00ProofScoreConfig Lv00ProofScoreConfig;

/* ============== 证明评分结构体 ============== */

/**
 * @brief 证明评分
 *
 * 包含多维度评分信息和综合得分。
 * 所有评分归一化到 [0.0, 1.0] 区间。
 */
struct Lv00ProofScore {
    /* 基础统计 */
    int steps_count;               /**< 总步骤数 */
    int axiom_usage_diversity;     /**< 使用的不同公理种类数 */
    int max_dependency_depth;      /**< 最大依赖深度 */
    int total_axiom_applications;  /**< 公理应用总次数 */
    int redundant_steps;           /**< 冗余步骤数 */

    /* 维度评分（0.0 ~ 1.0，越高越好） */
    double brevity_score;          /**< 简洁性评分 */
    double elegance_score;         /**< 优雅性评分 */
    double completeness_score;     /**< 完备性评分 */
    double depth_preference_score; /**< 深度偏好评分（适中为佳） */
    double anti_redundancy_score;  /**< 反冗余评分（1.0 - 冗余率） */

    /* 综合评分 */
    double composite_score;        /**< 综合加权评分 */

    /* 冗余度 */
    double redundancy_ratio;       /**< 冗余度（对于此证明：冗余步骤/总步骤） */

    /* 附加属性 */
    bool has_counterexample_checks;  /**< 是否包含反例检查 */
    bool uses_natural_constructions; /**< 是否使用了自然辅助构造 */
    int natural_construction_count;  /**< 自然构造数量 */

    /* 颜色/信任 */
    ProofColor trust_color;        /**< 信任颜色 */

    /* 策略信息 */
    ProofStrategyType used_strategy; /**< 使用的证明策略 */

    /* 人类可读的摘要 */
    char *summary;                  /**< 评分摘要文本（调用者需释放） */
};

/* ============== 评分配置 ============== */

/**
 * @brief 证明评分配置
 *
 * 自定义各评分维度的权重和参数。
 */
struct Lv00ProofScoreConfig {
    /* 权重（总和应接近 1.0，但非强制） */
    double weight_brevity;          /**< 简洁性权重（默认 0.25） */
    double weight_elegance;         /**< 优雅性权重（默认 0.20） */
    double weight_completeness;     /**< 完备性权重（默认 0.30） */
    double weight_depth;            /**< 深度权重（默认 0.10） */
    double weight_anti_redundancy;  /**< 反冗余权重（默认 0.15） */

    /* 参数 */
    int max_allowed_steps;          /**< 允许的最大步骤数（用于简洁性评分归一化） */
    double ideal_depth;             /**< 理想深度（深度偏好评分的中点） */
    int max_axiom_diversity;        /**< 公理多样性的分母（用于优雅性评分归一化） */

    /* 模式 */
    bool prefer_shorter;            /**< 是否偏好更短的证明 */
    bool penalize_oracle_use;       /**< 是否惩罚 Oracle 依赖 */
    bool count_lemma_steps;         /**< 是否计入引理步骤 */
};

/**
 * @brief 默认评分配置
 */
#define LV00_PROOF_SCORE_CONFIG_DEFAULT { \
    0.25, 0.20, 0.30, 0.10, 0.15,       \
    100, 10.0, 30,                        \
    true, true, true                      \
}

/* ============== 核心评分 API ============== */

/**
 * @brief 对证明进行多维评分评估
 *
 * 遍历证明导航器中所有步骤，计算各维度评分。
 *
 * @param nav       证明导航器
 * @param config    评分配置（可为 NULL，使用默认值）
 * @return 新分配的评分对象（调用者需用 lv00_proof_score_destroy 释放），失败返回 NULL
 */
Lv00ProofScore *lv00_proof_evaluate(const ProofNavigator *nav, const Lv00ProofScoreConfig *config);

/**
 * @brief 对证明树进行多维评分评估
 *
 * @param tree      证明树（ProofTree）
 * @param config    评分配置（可为 NULL，使用默认值）
 * @return 新分配的评分对象，失败返回 NULL
 */
Lv00ProofScore *lv00_proof_evaluate_tree(const Lv00ProofTree *tree, const Lv00ProofScoreConfig *config);

/**
 * @brief 销毁证明评分对象
 *
 * @param score 评分对象（可为 NULL）
 */
void lv00_proof_score_destroy(Lv00ProofScore *score);

/* ============== 证明比较 ============== */

/**
 * @brief 证明比较结果
 */
typedef enum {
    LV00_PROOF_COMPARE_FIRST_BETTER  = -1, /**< 第一个证明更好 */
    LV00_PROOF_COMPARE_EQUAL         =  0, /**< 两个证明等优 */
    LV00_PROOF_COMPARE_SECOND_BETTER =  1   /**< 第二个证明更好 */
} Lv00ProofCompareResult;

/**
 * @brief 比较两个证明的评分，选出更优者
 *
 * 比较规则（优先级递减）：
 * 1. 如果颜色不同：绿色 > 黄色 > 蓝色 > 琥珀色 > 橙色
 * 2. 综合评分更高者胜出
 * 3. 如果评分相同：简洁性更高者胜出
 * 4. 如果仍相同：完备性更高者胜出
 *
 * @param s1 第一个证明的评分
 * @param s2 第二个证明的评分
 * @return 比较结果
 */
Lv00ProofCompareResult lv00_proof_compare(const Lv00ProofScore *s1, const Lv00ProofScore *s2);

/**
 * @brief 在多个证明评分中选出最优者
 *
 * @param scores    评分数组
 * @param count     数组长度
 * @return 最优证明的索引（0-based），-1 表示参数错误
 */
int lv00_proof_select_best(const Lv00ProofScore **scores, int count);

/* ============== 评分报告 ============== */

/**
 * @brief 生成人类可读的评分报告
 *
 * 生成结构化文本，包括：
 * - 各维度分数及条形图（ASCII art）
 * - 综合评分
 * - 改进建议
 *
 * @param score  证明评分
 * @return 新分配的文本字符串（调用者需释放），失败返回 NULL
 */
char *lv00_proof_score_report(const Lv00ProofScore *score);

/**
 * @brief 生成简短的评分摘要
 *
 * 单行摘要，适用于列表视图。
 * 格式: "[78/100] B:0.82 E:0.65 C:0.95 D:0.70 R:0.12 (13步, 绿)"
 *
 * @param score 证明评分
 * @return 新分配的字符串（调用者需释放），失败返回 NULL
 */
char *lv00_proof_score_summary(const Lv00ProofScore *score);

/* ============== 批量评分 ============== */

/**
 * @brief 对一组证明进行批量评分
 *
 * @param navs      证明导航器数组
 * @param count     导航器数量
 * @param config    评分配置（可为 NULL）
 * @param out_scores 输出评分数组（至少 count 个元素，调用者负责逐项释放）
 * @return 成功评分的数量，-1 表示参数错误
 */
int lv00_proof_evaluate_batch(ProofNavigator **navs, int count,
                              const Lv00ProofScoreConfig *config, Lv00ProofScore **out_scores);

/**
 * @brief 对一组证明进行批量评分并排序
 *
 * @param navs      证明导航器数组
 * @param count     导航器数量
 * @param config    评分配置（可为 NULL）
 * @param out_ranked_indices 输出排序后的索引数组（最佳在前，调用者需释放），大小为 count
 * @return 成功评分的数量，-1 表示参数错误
 */
int lv00_proof_rank(ProofNavigator **navs, int count,
                    const Lv00ProofScoreConfig *config, int **out_ranked_indices);

/* ============== 维度编辑 ============== */

/**
 * @brief 重新计算评分（在手动修改权重后使用）
 *
 * @param score  已有的评分对象
 * @param config 新的评分配置
 */
void lv00_proof_score_recompute(Lv00ProofScore *score, const Lv00ProofScoreConfig *config);

/* ============== 辅助函数 ============== */

/**
 * @brief 证明比较结果转字符串
 *
 * @param result 比较结果
 * @return 静态字符串，请勿释放
 */
const char *lv00_proof_compare_result_to_string(Lv00ProofCompareResult result);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PROOF_SCORE_H */
