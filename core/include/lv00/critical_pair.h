/* ========================================================================
 * 模块名称：关键对计算引擎 (critical_pair)
 * 功能概述：对一组图重写规则自动计算全部关键对（critical pairs），
 *          即两条规则的重叠应用可能导致歧义的项。对每个关键对分别
 *          沿两条规则归约一步，比较归约结果。这是汇合性验证的
 *          核心基础设施，服务于公理包编辑器的第二可信基审查。
 *
 * 设计文档参考：§3.6 图重写引擎、§十 关键对可视化器（核心计算部分）
 *
 * 主要 API：
 *   - critical_pair_compute_all  — 对规则集两两计算所有关键对
 *   - critical_pair_compare      — 比较关键对的两条归约结果
 *   - critical_pair_export_text  — 导出标准化邻接表文本（供外部工具验证）
 *   - critical_pair_set_destroy  — 释放关键对集合
 *
 * 使用示例：
 *   CriticalPairSet *cps = critical_pair_compute_all(rules, rule_count, graph);
 *   for (int i = 0; i < cps->pair_count; i++) {
 *       critical_pair_compare(&cps->pairs[i]);
 *       if (!cps->pairs[i].is_confluent) {
 *           critical_pair_export_text(&cps->pairs[i], "non_confluent_cp.txt");
 *       }
 *   }
 *   critical_pair_set_destroy(cps);
 *
 * ======================================================================== */

#ifndef LV00_CRITICAL_PAIR_H
#define LV00_CRITICAL_PAIR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "constraint_graph.h"
#include "rewrite.h"

/* ========================================================================
 * 数据结构
 * ======================================================================== */

/**
 * @brief 不匹配详情条目
 *
 * 记录关键对两条归约路径结果之间的一个具体差异点。
 */
typedef struct {
    int kind;              /**< 不匹配类型：0=节点缺失，1=约束缺失，2=坐标不同 */
    int node_id_a;         /**< 归约结果A中的节点ID（-1 表示不适用） */
    int node_id_b;         /**< 归约结果B中的节点ID（-1 表示不适用） */
    int constraint_id_a;   /**< 归约结果A中的约束ID（-1 表示不适用） */
    int constraint_id_b;   /**< 归约结果B中的约束ID（-1 表示不适用） */
    char description[256]; /**< 人类可读的不匹配描述 */
} CpMismatch;

/**
 * @brief 关键对结构
 *
 * 两条重写规则在某个重叠上下文中的一次歧义事件。
 * 包含重叠项、两条归约路径的结果，以及汇合性判定。
 */
typedef struct {
    RewriteRule *rule1;           /**< 第一条规则 */
    RewriteRule *rule2;           /**< 第二条规则 */
    ConstraintGraph *overlap;     /**< 两条规则的重叠图（统一子） */
    ConstraintGraph *reduced1;    /**< 沿 rule1 归约一步的结果 */
    ConstraintGraph *reduced2;    /**< 沿 rule2 归约一步的结果 */
    bool is_confluent;            /**< 该关键对是否汇合（两条归约路径结果等价） */
    CpMismatch *mismatches;       /**< 不匹配详情数组（仅当 !is_confluent 时有效） */
    int mismatch_count;           /**< 不匹配详情数量 */
    bool compared;                /**< 是否已执行比较（false 表示尚未调用 compare） */
} CriticalPair;

/**
 * @brief 关键对集合
 *
 * 包含一次 critical_pair_compute_all 调用产生的全部关键对。
 */
typedef struct {
    CriticalPair *pairs;    /**< 关键对数组 */
    int pair_count;         /**< 当前关键对数量 */
    int capacity;           /**< 数组容量 */
} CriticalPairSet;

/* ========================================================================
 * 核心 API
 * ======================================================================== */

/**
 * @brief 对一组规则计算全部关键对
 *
 * 对规则集执行两两配对（包括自配对），找到所有可能的重叠位置，
 * 并为每个重叠创建关键对条目。仅创建关键对，不执行归约比较——
 * 比较由 critical_pair_compare 单独执行。
 *
 * @param rules       重写规则数组
 * @param rule_count  规则数量
 * @param base_graph  基础约束图（用于节点ID分配等上下文，可为 NULL）
 * @return 新分配的关键对集合，调用者负责通过 critical_pair_set_destroy 释放
 */
CriticalPairSet *critical_pair_compute_all(RewriteRule **rules, int rule_count,
                                           ConstraintGraph *base_graph);

/**
 * @brief 比较一个关键对的两条归约路径结果
 *
 * 对关键对的重叠图分别沿 rule1 和 rule2 归约一步，
 * 然后通过图规范化遍 + 合一检查比较两个结果：
 *   - 合一成功 → is_confluent = true
 *   - 合一失败 → is_confluent = false，填充 mismatches 数组
 *
 * @param cp  要比较的关键对
 * @return true 比较成功执行（不代表汇合——汇合性见 cp->is_confluent）
 */
bool critical_pair_compare(CriticalPair *cp);

/**
 * @brief 批量比较关键对集合中的所有条目
 *
 * @param set  关键对集合
 * @return 汇合的关键对数量
 */
int critical_pair_compare_all(CriticalPairSet *set);

/**
 * @brief 导出关键对的两个归约结果为标准化邻接表文本
 *
 * 生成格式：每行一个节点或约束，使用规范化符号坐标表达式。
 * 此输出可被外部图同构工具（nauty/Traces）直接解析，
 * 打破 Lv-00 内核的信任循环。
 *
 * 格式：
 *   NODE <id> <type> <coord_expr>
 *   EDGE <type> <participant_ids...>
 *
 * @param cp        要导出的关键对
 * @param filepath  输出文件路径（已存在则覆盖）
 * @return true 导出成功
 */
bool critical_pair_export_text(const CriticalPair *cp, const char *filepath);

/**
 * @brief 销毁关键对集合并释放所有资源
 *
 * @param set  要销毁的关键对集合
 */
void critical_pair_set_destroy(CriticalPairSet *set);

/**
 * @brief 获取汇合性统计摘要
 *
 * @param set            关键对集合
 * @param out_total      输出：总关键对数
 * @param out_confluent  输出：汇合的关键对数
 * @param out_pending    输出：尚未比较的关键对数
 */
void critical_pair_get_statistics(const CriticalPairSet *set,
                                  int *out_total,
                                  int *out_confluent,
                                  int *out_pending);

#ifdef __cplusplus
}
#endif

#endif /* LV00_CRITICAL_PAIR_H */
