/* ============================================================================
 * 模块名称：关键对计算引擎 (critical_pair)
 *
 * 功能概述：
 *   对一组图重写规则自动计算全部关键对（critical pairs）——
 *   即两条规则的重叠应用可能导致歧义的组合。对每个关键对分别
 *   沿两条规则归约一步，比较两个归约结果以判断汇合性。
 *
 *   这是汇合性验证的核心基础设施，服务于公理包编辑器的
 *   第二可信基审查。关键对比较依赖于 VF2 匹配器、
 *   图规范化遍引擎和合一算法——这些组件由内核测试担保，
 *   构成极小可信基。
 *
 * 设计文档参考：§3.6 图重写引擎、§十 关键对可视化器（核心计算部分）
 *
 * 主要 API：
 *   - critical_pair_compute_all  计算全部关键对（两两配对 + 自配对）
 *   - critical_pair_compare      对关键对执行归约比较并判断汇合性
 *   - critical_pair_compare_all  批量比较集合中所有关键对
 *   - critical_pair_export_text  导出标准化邻接表文本（供外部工具验证）
 *   - critical_pair_set_destroy  释放关键对集合
 *   - critical_pair_get_statistics 获取汇合性统计摘要
 *
 * 使用示例：
 *   @code
 *   CriticalPairSet *cps = critical_pair_compute_all(rules, rule_count, NULL);
 *   int confluent = critical_pair_compare_all(cps);
 *   if (confluent < cps->pair_count) {
 *       // 存在非汇合关键对 → 导出供审查
 *       critical_pair_export_text(&cps->pairs[0], "non_confluent.txt");
 *   }
 *   critical_pair_set_destroy(cps);
 *   @endcode
 *
 * ============================================================================ */

#ifndef LV00_CRITICAL_PAIR_H
#define LV00_CRITICAL_PAIR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "constraint_graph.h"
#include "rewrite.h"

/* ============================================================================
 * 数据结构
 * ============================================================================ */

/**
 * @brief 不匹配详情条目
 *
 * 记录关键对两条归约路径结果之间的一个具体差异点。
 */
typedef struct {
    /** 不匹配类型码：0 = 节点结构不匹配，1 = 约束不匹配，2 = 坐标不匹配 */
    int kind;
    /** 归约结果 A 中的节点 ID（-1 表示不适用） */
    int node_id_a;
    /** 归约结果 B 中的节点 ID（-1 表示不适用） */
    int node_id_b;
    /** 归约结果 A 中的约束 ID（-1 表示不适用） */
    int constraint_id_a;
    /** 归约结果 B 中的约束 ID（-1 表示不适用） */
    int constraint_id_b;
    /** 人类可读的不匹配描述（以 null 结尾的 UTF-8 字符串） */
    char description[256];
} CpMismatch;

/**
 * @brief 关键对结构
 *
 * 两条重写规则在某个重叠上下文中的一次歧义事件。
 * 包含重叠项、两条归约路径的结果，以及汇合性判定。
 */
typedef struct {
    /** 第一条重写规则（归约路径一） */
    RewriteRule *rule1;
    /** 第二条重写规则（归约路径二） */
    RewriteRule *rule2;
    /** 两条规则的重叠图（统一子，即被两条规则共同匹配的子图） */
    ConstraintGraph *overlap;
    /** 沿 rule1 归约一步后的结果图 */
    ConstraintGraph *reduced1;
    /** 沿 rule2 归约一步后的结果图 */
    ConstraintGraph *reduced2;
    /** 该关键对是否汇合（两条归约路径结果经规范化 + 合一判为等价） */
    bool is_confluent;
    /** 不匹配详情数组（仅当 is_confluent == false 时有效） */
    CpMismatch *mismatches;
    /** 不匹配详情条目数量 */
    int mismatch_count;
    /** 是否已执行比较（false 表示尚未调用 critical_pair_compare） */
    bool compared;
} CriticalPair;

/**
 * @brief 关键对集合
 *
 * 包含一次 critical_pair_compute_all 调用产生的全部关键对。
 */
typedef struct {
    /** 关键对数组 */
    CriticalPair *pairs;
    /** 当前关键对数量 */
    int pair_count;
    /** 数组容量（内部管理，自动扩容） */
    int capacity;
} CriticalPairSet;

/* ============================================================================
 * 核心 API
 * ============================================================================ */

/**
 * @brief 对一组规则计算全部关键对
 *
 * 对规则集执行两两配对（含自配对，即 j = i..N-1），寻找所有可能
 * 的重叠位置并为每个重叠创建关键对条目。
 * 仅创建关键对，不执行归约比较——比较由 critical_pair_compare 独立执行。
 *
 * @param rules       重写规则数组
 * @param rule_count  规则数量
 * @param base_graph  基础约束图（用于节点 ID 分配等上下文，可为 NULL）
 * @return 新分配的关键对集合，调用者负责通过 critical_pair_set_destroy 释放；
 *         空规则集或 rule_count < 1 返回 NULL
 */
CriticalPairSet *critical_pair_compute_all(RewriteRule **rules, int rule_count,
                                           ConstraintGraph *base_graph);

/**
 * @brief 比较一个关键对的两条归约路径结果
 *
 * 流程：
 *   1. 对关键对的重叠图分别沿 rule1 和 rule2 归约一步
 *   2. 对两个归约结果执行图规范化遍（graph_normalize）
 *   3. 通过合一检查（unify_construction_with_proposition_detailed）比较：
 *      - 合一成功 → is_confluent = true
 *      - 合一失败 → is_confluent = false，填充 mismatches 数组
 *
 * 幂等性：若 cp->compared == true 则直接返回 true，不重复比较。
 *
 * @param cp  要比较的关键对
 * @return true 比较成功执行（不代表汇合——汇合性见 cp->is_confluent）；
 *         false 表示 cp 为 NULL 或无重叠图
 */
int critical_pair_compare(CriticalPair *cp);

/**
 * @brief 批量比较关键对集合中的所有条目
 *
 * 对集合中的每个关键对依次调用 critical_pair_compare。
 *
 * @param set  关键对集合
 * @return 汇合的关键对数量（is_confluent == true 的条目数）
 */
int critical_pair_compare_all(CriticalPairSet *set);

/**
 * @brief 导出关键对的两个归约结果为标准化邻接表文本
 *
 * 生成格式：
 *   NODE <id> <type> [coords=<n>]
 *   EDGE <type> <participant_ids...>
 *
 * 此输出可被外部图同构工具（nauty/Traces）直接解析，
 * 打破 Lv-00 内核的信任循环，提供独立可验证的证据。
 *
 * @param cp        要导出的关键对
 * @param filepath  输出文件路径（已存在则覆盖）
 * @return true 导出成功，false 表示 cp 或 filepath 为 NULL
 */
int critical_pair_export_text(const CriticalPair *cp, const char *filepath);

/**
 * @brief 销毁关键对集合并释放所有资源
 *
 * NULL 安全：传入 NULL 无操作。
 *
 * @param set  要销毁的关键对集合
 */
void critical_pair_set_destroy(CriticalPairSet *set);

/**
 * @brief 获取汇合性统计摘要
 *
 * 遍历关键对集合，统计总数、已比较且汇合的数、尚未比较的数。
 * 所有输出参数均可为 NULL（跳过不关心的统计）。
 *
 * @param set            关键对集合（可为 NULL，此时各输出为 0）
 * @param out_total      输出：总关键对数
 * @param out_confluent  输出：已比较且汇合的关键对数
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
