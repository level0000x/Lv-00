/**
 * @file logic_check.h
 * @brief 逻辑自检系统 —— 证明一致性、循环性、完备性的全自动检查
 *
 * @details 提供三个维度的证明质量自动审查：
 *
 *          1. 一致性检查 (lv00_logic_check_consistency):
 *             遍历证明中所有断言，检测是否存在内部矛盾。
 *             例如：同时断言 "coplanar(A,B,C,D)" 和 "¬coplanar(A,B,C,D)"。
 *             将矛盾对收集到报告中。
 *
 *          2. 循环推理检测 (lv00_logic_check_circularity):
 *             构建证明步骤的依赖图，通过 DFS 检测环。
 *             环表示某步骤直接或间接依赖自身——即循环论证。
 *
 *          3. 完备性检查 (lv00_logic_check_completeness):
 *             验证每个被使用的断言是否都有合法来源：
 *             - 来自公理包（axiom_pkg）
 *             - 来自已证引理（lemma）
 *             - 来自前提（premise）
 *             - 来自推理规则（rule）
 *             无来源的断言标记为"未论证"。
 *
 *          所有检查结果汇总到 Lv00LogicReport 中。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#ifndef LV00_LOGIC_CHECK_H
#define LV00_LOGIC_CHECK_H

#include <stdbool.h>

#include "proof.h"
#include "three_valued_logic.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============== 前向声明 ============== */
typedef struct Lv00LogicReport Lv00LogicReport;
typedef struct Lv00LogicContext Lv00LogicContext;

/* ============== 检查级别 ============== */

/**
 * @brief 逻辑检查严重性级别
 */
typedef enum {
    LV00_LOGIC_ISSUE_INFO,     /**< 信息性：建议性提示 */
    LV00_LOGIC_ISSUE_WARNING,  /**< 警告：可能有问题，但不阻塞 */
    LV00_LOGIC_ISSUE_ERROR,    /**< 错误：确定性问题，需修复 */
    LV00_LOGIC_ISSUE_FATAL     /**< 致命错误：证明确实无效 */
} Lv00LogicIssueLevel;

/* ============== 问题条目 ============== */

/**
 * @brief 逻辑检查中的单个问题条目
 */
typedef struct Lv00LogicIssue {
    int id;                     /**< 问题ID */
    Lv00LogicIssueLevel level;  /**< 严重性级别 */
    char *category;             /**< 问题类别（如"一致性", "循环性", "完备性"） */
    char *description;          /**< 问题描述 */
    char *location;             /**< 位置信息（步骤编号/命题名称） */
    int step_index;             /**< 关联的证明步骤索引（-1 = 无关联） */
    int conflicting_step;       /**< 冲突步骤索引（一致性检查）（-1 = 无关） */
    char *suggestion;           /**< 修复建议 */
} Lv00LogicIssue;

/* ============== 逻辑检查上下文 ============== */

/**
 * @brief 逻辑检查上下文
 *
 * 封装检查所需的证明导航器和检查配置。
 */
struct Lv00LogicContext {
    ProofNavigator *nav;        /**< 证明导航器（只读访问） */
    int max_issues;             /**< 最大问题数上限 */
    bool verbose;               /**< 是否输出详细信息 */
    bool stop_on_fatal;         /**< 是否在致命错误处停止 */

    /* 内部统计 */
    int total_steps_checked;    /**< 已检查的步骤数 */
    int total_issues_found;     /**< 已发现的问题数 */
};

/* ============== 逻辑检查报告 ============== */

/**
 * @brief 逻辑完整性报告
 *
 * 包含一致性、循环性、完备性三个维度的检查结果。
 */
struct Lv00LogicReport {
    /* 总体评估 */
    bool is_consistent;         /**< 整体是否一致 */
    bool is_non_circular;       /**< 整体是否无循环推理 */
    bool is_complete;           /**< 整体是否完备（所有断言有据） */
    Lv00TruthValue overall_health; /**< 总体健康度（三值逻辑） */

    /* 分项问题列表 */
    Lv00LogicIssue **consistency_issues;  /**< 一致性问题数组 */
    int consistency_issue_count;          /**< 一致性问题数量 */
    int consistency_issue_capacity;       /**< 一致性问题容量 */

    Lv00LogicIssue **circularity_issues;  /**< 循环性问题数组 */
    int circularity_issue_count;          /**< 循环性问题数量 */
    int circularity_issue_capacity;       /**< 循环性问题容量 */

    Lv00LogicIssue **completeness_issues; /**< 完备性问题数组 */
    int completeness_issue_count;         /**< 完备性问题数量 */
    int completeness_issue_capacity;      /**< 完备性问题容量 */

    /* 统计摘要 */
    int total_issues;           /**< 问题总数 */
    int error_count;            /**< 错误级问题数 */
    int warning_count;          /**< 警告级问题数 */
    int info_count;             /**< 信息级问题数 */
    int fatal_count;            /**< 致命问题数 */

    /* 性能计数 */
    double check_time_sec;      /**< 检查总耗时（秒） */
};

/* ============== 核心检查 API ============== */

/**
 * @brief 创建逻辑检查上下文
 *
 * @param nav  证明导航器（不能为 NULL）
 * @return 新分配的上下文，失败返回 NULL
 */
Lv00LogicContext *lv00_logic_check_context_create(ProofNavigator *nav);

/**
 * @brief 销毁逻辑检查上下文
 *
 * @param ctx 上下文（可为 NULL）
 */
void lv00_logic_check_context_destroy(Lv00LogicContext *ctx);

/**
 * @brief 创建空的逻辑检查报告
 *
 * @return 新分配的报告，失败返回 NULL
 */
Lv00LogicReport *lv00_logic_report_create(void);

/**
 * @brief 销毁逻辑检查报告
 *
 * @param report 报告（可为 NULL）
 */
void lv00_logic_report_destroy(Lv00LogicReport *report);

/**
 * @brief 检查整个证明的内部一致性
 *
 * 遍历证明中所有断言，检测是否存在互补对（A 和 ¬A 同时成立）。
 * 每个矛盾对记录为一个 Lv00LogicIssue。
 *
 * 检查策略：
 * - 遍历所有步骤，收集其中声明的命题断言
 * - 对每一步中的断言检查其否定是否在其他步骤中出现
 * - 同时检查约束图中的几何约束矛盾（如同一线段被同时要求相等和不等）
 * - 对多态/依赖类型的断言，考虑类型实例化后的等价性
 *
 * @param ctx    逻辑检查上下文
 * @param report 输出的报告（会填充 consistency_issues）
 * @return 0 表示一致（无矛盾），正数表示发现的问题数，-1 表示参数错误
 */
int lv00_logic_check_consistency(Lv00LogicContext *ctx, Lv00LogicReport *report);

/**
 * @brief 检测证明中的循环推理
 *
 * 构建证明步骤的依赖有向图，通过深度优先搜索检测环。
 *
 * 检测策略：
 * - 首遍：构建完整依赖图（步骤ID → 依赖步骤ID 的邻接表）
 * - 第二遍：对每个未访问的节点运行三色 DFS（白/灰/黑）
 * - 灰色节点再次被访问时即为环
 * - 报告每个环中涉及的步骤序列
 *
 * 此外还检测：
 * - 自循环（某步骤直接依赖自身）
 * - 间接循环（A依赖B，B依赖C，C依赖A）
 *
 * @param ctx    逻辑检查上下文
 * @param report 输出的报告（会填充 circularity_issues）
 * @return 0 表示无循环，正数表示发现的环数，-1 表示参数错误
 */
int lv00_logic_check_circularity(Lv00LogicContext *ctx, Lv00LogicReport *report);

/**
 * @brief 验证证明中所有使用的断言都有合法来源
 *
 * 完备性检查确保每个被使用的事实/断言能被追溯到：
 * - 已加载的公理包（axiom_pkg）
 * - 已证明的引理（lemma）
 * - 明确声明的前提（premise）
 * - 一个或多个推理规则的合法应用（rule）
 *
 * 无来源的断言标记为"未论证"问题。
 *
 * @param ctx    逻辑检查上下文
 * @param report 输出的报告（会填充 completeness_issues）
 * @return 0 表示完备，正数表示发现的问题数，-1 表示参数错误
 */
int lv00_logic_check_completeness(Lv00LogicContext *ctx, Lv00LogicReport *report);

/* ============== 综合检查 ============== */

/**
 * @brief 执行全面的逻辑检查（一致性 + 循环性 + 完备性）
 *
 * 相当于依次调用 lv00_logic_check_consistency、lv00_logic_check_circularity、
 * lv00_logic_check_completeness，并将所有结果汇总到一个报告中。
 *
 * 同时计算 overall_health：
 * - LV00_TRUE：  三项检查全部通过（无错误/致命问题）
 * - LV00_FALSE： 存在致命问题
 * - LV00_UNKNOWN：仅有警告/信息性问题
 *
 * @param ctx    逻辑检查上下文
 * @param report 输出的综合报告
 * @return 0 表示全部通过，正数表示发现问题，-1 表示参数错误
 */
int lv00_logic_check_all(Lv00LogicContext *ctx, Lv00LogicReport *report);

/* ============== 报告导出 ============== */

/**
 * @brief 将逻辑检查报告导出为人类可读文本
 *
 * 生成结构化文本报告，包括：
 * - 总体评估摘要
 * - 分项问题列表及详细信息
 * - 统计摘要
 *
 * @param report    逻辑检查报告
 * @param verbose   是否包含所有详细信息
 * @return 新分配的文本字符串（调用者需用 lv00_free 释放），失败返回 NULL
 */
char *lv00_logic_report_to_text(const Lv00LogicReport *report, bool verbose);

/**
 * @brief 将逻辑检查报告导出为 JSON 格式
 *
 * 生成结构化 JSON，适用于：
 * - Web GUI 展示
 * - 自动化 CI/CD 流水线检查
 * - 日志记录与分析
 *
 * @param report 逻辑检查报告
 * @return 新分配的 JSON 字符串（调用者需用 lv00_free 释放），失败返回 NULL
 */
char *lv00_logic_report_to_json(const Lv00LogicReport *report);

/* ============== 辅助函数 ============== */

/**
 * @brief 检查级别转字符串
 *
 * @param level 检查级别
 * @return 静态字符串（"INFO"/"WARNING"/"ERROR"/"FATAL"），请勿释放
 */
const char *lv00_logic_issue_level_to_string(Lv00LogicIssueLevel level);

#ifdef __cplusplus
}
#endif

#endif /* LV00_LOGIC_CHECK_H */
