/**
 * @file logic_check.c
 * @brief 逻辑自检系统实现 —— 一致性、循环性、完备性检查
 *
 * @details 实现证明质量的自动审查：
 *          - 一致性检查：断言矛盾检测
 *          - 循环性检查：依赖环检测（三色 DFS）
 *          - 完备性检查：未论证断言分析
 *
 *          内部数据结构：
 *          - 依赖邻接表（用于循环检测的 DFS）
 *          - 断言哈希表（用于一致性检查的命题比较）
 *          - 来源追溯链表（用于完备性检查的公理/引理/前提追踪）
 *
 * @author Lv-00 Project
 * @version 1.0.0
 *
 * @dependencies
 *   - logic_check.h       : 逻辑检查公共接口定义
 *   - proof.h             : 证明导航器与证明步骤
 *   - lv00_internal.h     : 内部宏与常量
 *   - lv00_utils.h        : 统一内存分配器
 *   - three_valued_logic.h: 三值真值
 */

#include "logic_check.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00_internal.h"
#include "lv00_utils.h"
#include "three_valued_logic.h"

/* ============== 内部常量 ============== */

/** 最大 DFS 深度（防止在异常大的依赖图中栈溢出） */
#define LOGIC_CHECK_MAX_DFS_DEPTH 2000

/** 报告初始容量 */
#define LOGIC_REPORT_INITIAL_CAPACITY 16

/** 最大断言数（用于一致性检查的哈希桶数） */
#define MAX_ASSERTION_BUCKETS 256

/* ============== 内部辅助结构 ============== */

/**
 * @brief 规范化断言条目 —— 用于命题一致性比较
 *
 * 将命题断言（A 或 ¬A）和几何约束统一编码，
 * 以便快速检测矛盾：A 和 ¬A 同时存在即为矛盾。
 */
typedef struct {
    int prop_id;              /**< 命题ID（正值 = 肯定，负值 = 否定） */
    int step_index;           /**< 来源证明步骤索引 */
    char *description;        /**< 人类可读断言描述 */
} NormalizedAssertion;

/**
 * @brief 断言哈希桶 —— 链表节点，用于快速查找互补断言
 */
typedef struct AssertionBucket {
    NormalizedAssertion assertion;        /**< 断言条目 */
    struct AssertionBucket *next;         /**< 链表下一节点 */
} AssertionBucket;

/**
 * @brief 断言哈希表 —— 用于一致性检查的命题去重和互补查找
 */
typedef struct {
    AssertionBucket *buckets[MAX_ASSERTION_BUCKETS]; /**< 哈希桶数组 */
    int total_assertions;                             /**< 总断言数 */
} AssertionTable;

/**
 * @brief 依赖图节点（用于循环检测的邻接表）
 */
typedef struct DepNode {
    int step_id;                /**< 步骤ID（1-based） */
    int *deps;                  /**< 直接依赖的步骤ID数组 */
    int dep_count;              /**< 依赖数量 */
    int dep_capacity;           /**< 依赖数组容量 */

    /* DFS 三色标记 */
    enum { COLOR_WHITE, COLOR_GRAY, COLOR_BLACK } dfs_color;
    int discovery_time;         /**< 发现时间戳 */
    int finish_time;            /**< 完成时间戳 */
} DepNode;

/**
 * @brief 依赖图 —— 用于循环检测
 */
typedef struct {
    DepNode *nodes;             /**< 节点数组（按步骤ID索引） */
    int node_count;             /**< 节点数量 */
    int node_capacity;          /**< 节点容量 */
    int time_counter;           /**< DFS 时间计数器 */
} DependencyGraph;

/* ============== 静态全局 ============== */

/** 流式输出上下文（静态全局、模块私有） */
static StreamContext *g_stream_ctx = NULL;

/* ============== 前向声明 ============== */

/* 内部辅助函数 */
static int  assertion_hash(int prop_id);
static void assertion_table_init(AssertionTable *table);
static void assertion_table_destroy(AssertionTable *table);
static bool assertion_table_insert(AssertionTable *table, const NormalizedAssertion *assertion);
static bool assertion_table_find_negation(const AssertionTable *table, int prop_id);

static Lv00LogicIssue *logic_issue_create(Lv00LogicIssueLevel level, const char *category,
                                          const char *description, const char *location,
                                          int step_index, int conflicting_step, const char *suggestion);
static void logic_issue_destroy(Lv00LogicIssue *issue);
static bool logic_report_add_issue(Lv00LogicReport *report, Lv00LogicIssue *issue,
                                   const char *check_type);
static bool logic_report_resize_issues(Lv00LogicReport *report, const char *check_type);

static DependencyGraph *dep_graph_create(int max_steps);
static void dep_graph_destroy(DependencyGraph *graph);
static bool dep_graph_add_node(DependencyGraph *graph, int step_id);
static bool dep_graph_add_edge(DependencyGraph *graph, int from_id, int to_id);
static int  dep_graph_detect_cycle(DependencyGraph *graph, Lv00LogicReport *report, Lv00LogicContext *ctx);
static bool dep_graph_cycle_from_node(DependencyGraph *graph, int node_idx, Lv00LogicReport *report,
                                      Lv00LogicContext *ctx, int *path, int path_len, int depth);
static bool dep_graph_is_on_path(const int *path, int path_len, int step_id);

/* 完备性辅助 */
static int check_step_justification(const ProofStep *step, const ProofNavigator *nav, int step_index,
                                    Lv00LogicReport *report);

/* ============== 内部辅助：断言哈希表 ============== */

/** @brief 简单哈希函数（基于命题ID） */
static int assertion_hash(int prop_id) {
    int abs_id = (prop_id < 0) ? -prop_id : prop_id;
    return (unsigned int)abs_id % MAX_ASSERTION_BUCKETS;
}

/** @brief 初始化断言哈希表 */
static void assertion_table_init(AssertionTable *table) {
    if (!table) return;
    memset(table->buckets, 0, sizeof(table->buckets));
    table->total_assertions = 0;
}

/** @brief 销毁断言哈希表 */
static void assertion_table_destroy(AssertionTable *table) {
    if (!table) return;
    for (int i = 0; i < MAX_ASSERTION_BUCKETS; i++) {
        AssertionBucket *bucket = table->buckets[i];
        while (bucket) {
            AssertionBucket *next = bucket->next;
            lv00_free_ptr(bucket->assertion.description);
            lv00_free_ptr(bucket);
            bucket = next;
        }
    }
    memset(table->buckets, 0, sizeof(table->buckets));
    table->total_assertions = 0;
}

/** @brief 向哈希表插入断言 */
static bool assertion_table_insert(AssertionTable *table, const NormalizedAssertion *assertion) {
    if (!table || !assertion) return false;

    int hash = assertion_hash(assertion->prop_id);
    AssertionBucket *bucket = (AssertionBucket *)lv00_malloc(sizeof(AssertionBucket));
    if (!bucket) return false;

    bucket->assertion.prop_id = assertion->prop_id;
    bucket->assertion.step_index = assertion->step_index;
    bucket->assertion.description = assertion->description
        ? lv00_malloc(strlen(assertion->description) + 1) : NULL;
    if (bucket->assertion.description && assertion->description) {
        /* 使用 snprintf 替代 strncpy，确保缓冲区安全 */
        snprintf(bucket->assertion.description, strlen(assertion->description) + 1, "%s", assertion->description);
    }
    bucket->next = table->buckets[hash];
    table->buckets[hash] = bucket;
    table->total_assertions++;
    return true;
}

/** @brief 在哈希表中查找互补断言（即查找 prop_id 的相反数） */
static bool assertion_table_find_negation(const AssertionTable *table, int prop_id) {
    if (!table) return false;
    int hash = assertion_hash(-prop_id);
    AssertionBucket *bucket = table->buckets[hash];
    while (bucket) {
        if (bucket->assertion.prop_id == -prop_id) {
            return true;
        }
        bucket = bucket->next;
    }
    return false;
}

/* ============== 内部辅助：问题条目 ============== */

/** @brief 创建逻辑问题条目 */
static Lv00LogicIssue *logic_issue_create(Lv00LogicIssueLevel level, const char *category,
                                          const char *description, const char *location,
                                          int step_index, int conflicting_step, const char *suggestion) {
    Lv00LogicIssue *issue = (Lv00LogicIssue *)lv00_calloc(1, sizeof(Lv00LogicIssue));
    if (!issue) return NULL;

    issue->id = -1; /* 由报告分配 */
    issue->level = level;
    issue->category = category ? lv00_malloc(strlen(category) + 1) : NULL;
    if (issue->category && category) snprintf(issue->category, strlen(category) + 1, "%s", category);
    issue->description = description ? lv00_malloc(strlen(description) + 1) : NULL;
    if (issue->description && description) snprintf(issue->description, strlen(description) + 1, "%s", description);
    issue->location = location ? lv00_malloc(strlen(location) + 1) : NULL;
    if (issue->location && location) snprintf(issue->location, strlen(location) + 1, "%s", location);
    issue->step_index = step_index;
    issue->conflicting_step = conflicting_step;
    issue->suggestion = suggestion ? lv00_malloc(strlen(suggestion) + 1) : NULL;
    if (issue->suggestion && suggestion) /* [Bug修复] strcpy → lv00_strlcpy 防止缓冲区溢出 */
        lv00_strlcpy(issue->suggestion, suggestion, strlen(suggestion) + 1);

    return issue;
}

/** @brief 销毁逻辑问题条目 */
static void logic_issue_destroy(Lv00LogicIssue *issue) {
    if (!issue) return;
    lv00_free_ptr(issue->category);
    lv00_free_ptr(issue->description);
    lv00_free_ptr(issue->location);
    lv00_free_ptr(issue->suggestion);
    lv00_free_ptr(issue);
}

/** @brief 向报告添加问题 */
static bool logic_report_add_issue(Lv00LogicReport *report, Lv00LogicIssue *issue, const char *check_type) {
    if (!report || !issue) return false;

    Lv00LogicIssue ***issue_array = NULL;
    int *issue_count = NULL;
    int *issue_capacity = NULL;

    if (strcmp(check_type, "consistency") == 0) {
        issue_array   = &report->consistency_issues;
        issue_count   = &report->consistency_issue_count;
        issue_capacity = &report->consistency_issue_capacity;
    } else if (strcmp(check_type, "circularity") == 0) {
        issue_array   = &report->circularity_issues;
        issue_count   = &report->circularity_issue_count;
        issue_capacity = &report->circularity_issue_capacity;
    } else if (strcmp(check_type, "completeness") == 0) {
        issue_array   = &report->completeness_issues;
        issue_count   = &report->completeness_issue_count;
        issue_capacity = &report->completeness_issue_capacity;
    } else {
        return false;
    }

    /* 扩容 */
    if (*issue_count >= *issue_capacity) {
        int new_capacity = (*issue_capacity == 0) ? LOGIC_REPORT_INITIAL_CAPACITY : (*issue_capacity) * 2;
        Lv00LogicIssue **new_array = (Lv00LogicIssue **)lv00_realloc(*issue_array,
                                                                     (size_t)new_capacity * sizeof(Lv00LogicIssue *));
        if (!new_array) return false;
        *issue_array = new_array;
        *issue_capacity = new_capacity;
    }

    issue->id = report->total_issues;
    (*issue_array)[(*issue_count)++] = issue;
    report->total_issues++;

    /* 更新统计 */
    switch (issue->level) {
    case LV00_LOGIC_ISSUE_INFO:    report->info_count++;    break;
    case LV00_LOGIC_ISSUE_WARNING: report->warning_count++; break;
    case LV00_LOGIC_ISSUE_ERROR:   report->error_count++;   break;
    case LV00_LOGIC_ISSUE_FATAL:   report->fatal_count++;   break;
    }

    return true;
}

/* ============== 内部辅助：依赖图 ============== */

/** @brief 创建依赖图 */
static DependencyGraph *dep_graph_create(int max_steps) {
    if (max_steps <= 0) return NULL;

    DependencyGraph *graph = (DependencyGraph *)lv00_calloc(1, sizeof(DependencyGraph));
    if (!graph) return NULL;

    graph->nodes = (DepNode *)lv00_calloc((size_t)max_steps, sizeof(DepNode));
    if (!graph->nodes) {
        lv00_free_ptr(graph);
        return NULL;
    }
    graph->node_capacity = max_steps;
    graph->node_count = 0;
    graph->time_counter = 0;

    for (int i = 0; i < max_steps; i++) {
        graph->nodes[i].step_id = -1;  /* 未初始化 */
        graph->nodes[i].dfs_color = COLOR_WHITE;
    }

    return graph;
}

/** @brief 销毁依赖图 */
static void dep_graph_destroy(DependencyGraph *graph) {
    if (!graph) return;
    if (graph->nodes) {
        for (int i = 0; i < graph->node_capacity; i++) {
            lv00_free_ptr(graph->nodes[i].deps);
        }
        lv00_free_ptr(graph->nodes);
    }
    lv00_free_ptr(graph);
}

/** @brief 向依赖图添加节点 */
static bool dep_graph_add_node(DependencyGraph *graph, int step_id) {
    if (!graph || step_id <= 0 || step_id > graph->node_capacity) return false;

    int idx = step_id - 1; /* 0-based 索引 */
    if (graph->nodes[idx].step_id < 0) {
        graph->nodes[idx].step_id = step_id;
        graph->node_count++;
    }
    return true;
}

/** @brief 向依赖图添加有向边 */
static bool dep_graph_add_edge(DependencyGraph *graph, int from_id, int to_id) {
    if (!graph || from_id <= 0 || from_id > graph->node_capacity
        || to_id <= 0 || to_id > graph->node_capacity) return false;

    int idx = from_id - 1;
    DepNode *node = &graph->nodes[idx];

    if (node->step_id < 0) {
        dep_graph_add_node(graph, from_id);
    }

    /* 扩容依赖数组 */
    if (node->dep_count >= node->dep_capacity) {
        int new_cap = (node->dep_capacity == 0) ? 4 : node->dep_capacity * 2;
        int *new_deps = (int *)lv00_realloc(node->deps, (size_t)new_cap * sizeof(int));
        if (!new_deps) return false;
        node->deps = new_deps;
        node->dep_capacity = new_cap;
    }

    node->deps[node->dep_count++] = to_id;
    return true;
}

/**
 * @brief 检测图中的环（三色 DFS）
 *
 * @return 发现的环数量
 */
static int dep_graph_detect_cycle(DependencyGraph *graph, Lv00LogicReport *report, Lv00LogicContext *ctx) {
    if (!graph || !report || !ctx) return 0;

    int cycle_count = 0;
    int *path = (int *)lv00_malloc((size_t)(graph->node_capacity + 1) * sizeof(int));
    if (!path) return 0;

    for (int i = 0; i < graph->node_capacity; i++) {
        if (graph->nodes[i].step_id >= 0 && graph->nodes[i].dfs_color == COLOR_WHITE) {
            path[0] = graph->nodes[i].step_id;
            if (dep_graph_cycle_from_node(graph, i, report, ctx, path, 1, 0)) {
                cycle_count++;
            }
        }
    }

    lv00_free_ptr(path);
    return cycle_count;
}

/**
 * @brief 从指定节点开始 DFS 检测环
 *
 * 三色算法：
 * - WHITE: 未访问
 * - GRAY:  正在访问（在递归栈中）
 * - BLACK: 已访问完成
 *
 * 当遇到 GRAY 节点时，发现环。
 */
static bool dep_graph_cycle_from_node(DependencyGraph *graph, int node_idx, Lv00LogicReport *report,
                                      Lv00LogicContext *ctx, int *path, int path_len, int depth) {
    if (!graph || !report || !ctx || node_idx < 0 || node_idx >= graph->node_capacity) return false;
    if (depth > LOGIC_CHECK_MAX_DFS_DEPTH) return false; /* 安全深度限制 */

    DepNode *node = &graph->nodes[node_idx];
    if (node->dfs_color == COLOR_BLACK) return false;

    if (node->dfs_color == COLOR_GRAY) {
        /* 发现环！构建环的描述 */
        char desc[512];
        char location[128];
        snprintf(desc, sizeof(desc), "发现循环推理：步骤 %d 通过依赖链间接依赖自身", node->step_id);

        /* 构建环路径描述 */
        char cycle_str[256] = "";
        for (int i = 0; i < path_len; i++) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%s%d", (i > 0 ? " → " : ""), path[i]);
            strncat(cycle_str, buf, sizeof(cycle_str) - strlen(cycle_str) - 1);
        }
        strncat(cycle_str, " → ", sizeof(cycle_str) - strlen(cycle_str) - 1);
        {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", node->step_id);
            strncat(cycle_str, buf, sizeof(cycle_str) - strlen(cycle_str) - 1);
        }
        snprintf(location, sizeof(location), "环路径: %s", cycle_str);

        Lv00LogicIssue *issue = logic_issue_create(
            LV00_LOGIC_ISSUE_ERROR,
            "循环性",
            desc,
            location,
            node->step_id,
            -1,
            "检查依赖链，确认某一步骤是否不当地依赖了自身或其推论。"
            "考虑引入中间引理来打破循环。"
        );
        if (issue) {
            logic_report_add_issue(report, issue, "circularity");
        }
        return true;
    }

    /* 标记为灰色（进入） */
    node->dfs_color = COLOR_GRAY;

    /* 递归访问所有后继 */
    bool found_cycle = false;
    for (int d = 0; d < node->dep_count; d++) {
        int dep_id = node->deps[d];
        int dep_idx = dep_id - 1;
        if (dep_idx >= 0 && dep_idx < graph->node_capacity && graph->nodes[dep_idx].step_id >= 0) {
            /* 检查自循环 */
            if (dep_id == node->step_id) {
                char location[64];
                snprintf(location, sizeof(location), "步骤 %d 直接依赖自身", node->step_id);
                Lv00LogicIssue *issue = logic_issue_create(
                    LV00_LOGIC_ISSUE_ERROR,
                    "循环性",
                    "自循环：步骤直接将自己列为依赖",
                    location,
                    node->step_id,
                    -1,
                    "从依赖列表中移除对自身的引用。"
                );
                if (issue) {
                    logic_report_add_issue(report, issue, "circularity");
                }
                found_cycle = true;
                break;
            }

            if (path_len < graph->node_capacity) {
                path[path_len] = dep_id;
                if (dep_graph_cycle_from_node(graph, dep_idx, report, ctx,
                                              path, path_len + 1, depth + 1)) {
                    found_cycle = true;
                }
            }
        }
    }

    /* 标记为黑色（退出） */
    node->dfs_color = COLOR_BLACK;
    return found_cycle;
}

/** @brief 检查某步骤ID是否在当前DFS路径上 */
static bool dep_graph_is_on_path(const int *path, int path_len, int step_id) {
    for (int i = 0; i < path_len; i++) {
        if (path[i] == step_id) return true;
    }
    return false;
}

/* ============== 内部辅助：完备性检查 ============== */

/**
 * @brief 检查单个证明步骤的断言是否有合法来源
 *
 * 合法来源包括：
 * - 公理包中的公理
 * - 已证引理
 * - 明确声明的前提
 * - 推理规则的合法应用
 */
static int check_step_justification(const ProofStep *step, const ProofNavigator *nav, int step_index,
                                    Lv00LogicReport *report) {
    if (!step || !nav || !report) return 0;

    int issues_found = 0;

    /* 检查依赖链：如果步骤有显式依赖，这些依赖必须是有效步骤 */
    if (step->dependency_count > 0) {
        for (int d = 0; d < step->dependency_count; d++) {
            int dep_id = step->dependency_step_ids[d];
            bool dep_valid = false;

            /* 检查依赖是否指向有效的已存在步骤 */
            if (nav->steps) {
                for (int s = 0; s < nav->step_count; s++) {
                    if (nav->steps[s] && nav->steps[s]->id == dep_id) {
                        dep_valid = true;
                        break;
                    }
                }
            }

            if (!dep_valid) {
                char location[128];
                char desc[256];
                snprintf(location, sizeof(location), "步骤 %d, 依赖步骤 %d", step->id, dep_id);
                snprintf(desc, sizeof(desc), "步骤 %d 依赖了不存在的步骤 %d", step->id, dep_id);
                Lv00LogicIssue *issue = logic_issue_create(
                    LV00_LOGIC_ISSUE_ERROR,
                    "完备性",
                    desc,
                    location,
                    step_index,
                    -1,
                    "确保依赖的步骤在证明中已经存在。如果该步骤是外部引理，"
                    "请将其导入到当前证明中。"
                );
                if (issue) {
                    logic_report_add_issue(report, issue, "completeness");
                    issues_found++;
                }
            }
        }
    }

    /* 检查爆炸原理步骤：需要 ⊥ 的证物 */
    if (step->type == PROOF_STEP_EX_FALSO) {
        /* 特殊处理：爆炸原理步骤在颜色系统中已有标记，此处仅记录 */
        /* 如果依赖为空且不是来自公理，标记为信息性提示 */
        if (step->dependency_count == 0) {
            char location[64];
            snprintf(location, sizeof(location), "步骤 %d (爆炸原理)", step->id);
            Lv00LogicIssue *issue = logic_issue_create(
                LV00_LOGIC_ISSUE_WARNING,
                "完备性",
                "爆炸原理步骤缺少对矛盾（⊥）的明确引用",
                location,
                step_index,
                -1,
                "为爆炸原理步骤添加对矛盾引理或反证法结论的显式依赖。"
            );
            if (issue) {
                logic_report_add_issue(report, issue, "completeness");
                issues_found++;
            }
        }
    }

    /* 检查 Oracle 步骤 */
    if (step->type == PROOF_STEP_ORACLE) {
        char location[64];
        snprintf(location, sizeof(location), "步骤 %d (Oracle)", step->id);
        Lv00LogicIssue *issue = logic_issue_create(
            LV00_LOGIC_ISSUE_WARNING,
            "完备性",
            "证明使用了 Oracle 依赖（非构造性外部求解器）",
            location,
            step_index,
            -1,
            "Oracle 步骤不是构造性证明。如果可能，用构造性推理替换。"
            "否则，在最终信任颜色中标记为橙色。"
        );
        if (issue) {
            logic_report_add_issue(report, issue, "completeness");
            issues_found++;
        }
    }

    return issues_found;
}

/* ============== 公共 API 实现 ============== */

/* ── 上下文管理 ── */

Lv00LogicContext *lv00_logic_check_context_create(ProofNavigator *nav) {
    if (!nav) return NULL;

    Lv00LogicContext *ctx = (Lv00LogicContext *)lv00_calloc(1, sizeof(Lv00LogicContext));
    if (!ctx) return NULL;

    ctx->nav = nav;
    ctx->max_issues = 1000;   /* 默认上限 */
    ctx->verbose = false;     /* 默认简洁模式 */
    ctx->stop_on_fatal = true; /* 默认致命处停止 */
    ctx->total_steps_checked = 0;
    ctx->total_issues_found = 0;

    return ctx;
}

void lv00_logic_check_context_destroy(Lv00LogicContext *ctx) {
    lv00_free_ptr(ctx);
}

/* ── 报告管理 ── */

Lv00LogicReport *lv00_logic_report_create(void) {
    Lv00LogicReport *report = (Lv00LogicReport *)lv00_calloc(1, sizeof(Lv00LogicReport));
    if (!report) return NULL;

    report->is_consistent = true;
    report->is_non_circular = true;
    report->is_complete = true;
    report->overall_health = LV00_TRUE;
    report->total_issues = 0;
    report->check_time_sec = 0.0;

    return report;
}

void lv00_logic_report_destroy(Lv00LogicReport *report) {
    if (!report) return;

    /* 释放各分项问题 */
    for (int i = 0; i < report->consistency_issue_count; i++) {
        logic_issue_destroy(report->consistency_issues[i]);
    }
    lv00_free_ptr(report->consistency_issues);

    for (int i = 0; i < report->circularity_issue_count; i++) {
        logic_issue_destroy(report->circularity_issues[i]);
    }
    lv00_free_ptr(report->circularity_issues);

    for (int i = 0; i < report->completeness_issue_count; i++) {
        logic_issue_destroy(report->completeness_issues[i]);
    }
    lv00_free_ptr(report->completeness_issues);

    lv00_free_ptr(report);
}

/* ── 一致性检查 ── */

int lv00_logic_check_consistency(Lv00LogicContext *ctx, Lv00LogicReport *report) {
    if (!ctx || !report || !ctx->nav) {
        return -1;
    }

    ProofNavigator *nav = ctx->nav;
    AssertionTable table;
    assertion_table_init(&table);

    int issues_found = 0;
    report->consistency_issue_count = 0;

    ctx->total_steps_checked = 0;
    ctx->total_issues_found = 0;

    /* 遍历所有证明步骤，收集断言 */
    for (int i = 0; i < nav->step_count && ctx->total_issues_found < ctx->max_issues; i++) {
        ProofStep *step = nav->steps[i];
        if (!step) continue;
        ctx->total_steps_checked++;

        /* 从步骤中提取命题ID作为断言标识 */
        /* 这里使用步骤的 node_id 和 constraint_id 作为基本断言ID */
        /* 同时检查命题模式的子命题 */

        /* 收集节点级别的断言 */
        if (step->node_id > 0) {
            NormalizedAssertion asrt;
            asrt.prop_id = step->node_id;  /* 肯定断言 */
            asrt.step_index = i;
            asrt.description = NULL;  /* 可由调用者补充 */
            assertion_table_insert(&table, &asrt);

            /* 检查互补：是否有 ¬node_id 在表中 */
            if (assertion_table_find_negation(&table, step->node_id)) {
                char desc[256];
                char location[128];
                snprintf(desc, sizeof(desc),
                         "逻辑矛盾：步骤 %d (节点 %d) 的断言与之前步骤的否定断言冲突。"
                         "命题及其否定同时被声明为真。",
                         i, step->node_id);
                snprintf(location, sizeof(location), "步骤 %d vs 冲突步骤", i);

                Lv00LogicIssue *issue = logic_issue_create(
                    LV00_LOGIC_ISSUE_FATAL,
                    "一致性",
                    desc,
                    location,
                    i,
                    -1,
                    "检查两个冲突的命题，确定何者确实为真。"
                    "如果这是反证法的结论，请确保矛盾被正确隔离处理。"
                );
                if (issue) {
                    logic_report_add_issue(report, issue, "consistency");
                    issues_found++;
                    ctx->total_issues_found++;
                }
            }
        }

        /* 收集约束级别的断言 */
        if (step->constraint_id > 0) {
            /* 约束的否定用负ID表示 */
            NormalizedAssertion asrt_pos;
            asrt_pos.prop_id = step->constraint_id + 1000000;  /* 偏移以避免与节点冲突 */
            asrt_pos.step_index = i;
            asrt_pos.description = NULL;
            assertion_table_insert(&table, &asrt_pos);

            if (assertion_table_find_negation(&table, asrt_pos.prop_id)) {
                char desc[256];
                char location[128];
                snprintf(desc, sizeof(desc),
                         "几何约束矛盾：步骤 %d (约束 %d) 的约束与之前的否定约束冲突。",
                         i, step->constraint_id);
                snprintf(location, sizeof(location), "步骤 %d", i);

                Lv00LogicIssue *issue = logic_issue_create(
                    LV00_LOGIC_ISSUE_FATAL,
                    "一致性",
                    desc,
                    location,
                    i,
                    -1,
                    "检查冲突的几何约束。可能其中一个约束条件有误。"
                );
                if (issue) {
                    logic_report_add_issue(report, issue, "consistency");
                    issues_found++;
                    ctx->total_issues_found++;
                }
            }
        }

        /* 规范性步骤：检查合并操作是否违反了不变量 */
        if (step->type == PROOF_STEP_NORMALIZATION && step->merged_count > 0) {
            char location[64];
            snprintf(location, sizeof(location), "步骤 %d (规范化)", i);
            Lv00LogicIssue *issue = logic_issue_create(
                LV00_LOGIC_ISSUE_INFO,
                "一致性",
                "规范化步骤：节点被合并。请验证合并不破坏任何已证断言。",
                location,
                i,
                -1,
                "检查合并后的节点是否仍然满足所有已被证明的命题约束。"
            );
            if (issue) {
                logic_report_add_issue(report, issue, "consistency");
                issues_found++;
                ctx->total_issues_found++;
            }
        }
    }

    /* 如果没有发现问题，标记为一致 */
    if (issues_found == 0) {
        report->is_consistent = true;
    } else {
        report->is_consistent = false;
        /* 致命问题 → overall_health = FALSE */
        if (report->fatal_count > 0) {
            report->overall_health = LV00_FALSE;
        } else {
            report->overall_health = LV00_UNKNOWN;
        }
    }

    assertion_table_destroy(&table);
    return issues_found;
}

/* ── 循环性检查 ── */

int lv00_logic_check_circularity(Lv00LogicContext *ctx, Lv00LogicReport *report) {
    if (!ctx || !report || !ctx->nav) {
        return -1;
    }

    ProofNavigator *nav = ctx->nav;
    int issues_found = 0;
    report->circularity_issue_count = 0;

    /* 构建依赖图 */
    DependencyGraph *graph = dep_graph_create(nav->step_count > 0 ? nav->step_count + 1 : 256);
    if (!graph) {
        report->is_non_circular = false;
        return -1;
    }

    /* 添加所有步骤作为节点 */
    for (int i = 0; i < nav->step_count; i++) {
        if (nav->steps[i]) {
            dep_graph_add_node(graph, nav->steps[i]->id);
        }
    }

    /* 添加依赖边 */
    for (int i = 0; i < nav->step_count; i++) {
        ProofStep *step = nav->steps[i];
        if (!step) continue;
        for (int d = 0; d < step->dependency_count; d++) {
            int dep_id = step->dependency_step_ids[d];
            /* 正向边：步 → 依赖的步 */
            dep_graph_add_edge(graph, step->id, dep_id);
        }
    }

    /* 执行环检测 */
    issues_found = dep_graph_detect_cycle(graph, report, ctx);

    if (issues_found == 0) {
        report->is_non_circular = true;
    } else {
        report->is_non_circular = false;
        if (report->error_count > 0) {
            report->overall_health = (report->overall_health == LV00_TRUE) ? LV00_UNKNOWN : report->overall_health;
        }
    }

    dep_graph_destroy(graph);
    return issues_found;
}

/* ── 完备性检查 ── */

int lv00_logic_check_completeness(Lv00LogicContext *ctx, Lv00LogicReport *report) {
    if (!ctx || !report || !ctx->nav) {
        return -1;
    }

    ProofNavigator *nav = ctx->nav;
    int issues_found = 0;
    report->completeness_issue_count = 0;

    /* 遍历每个步骤，检查来源合法性 */
    for (int i = 0; i < nav->step_count && ctx->total_issues_found < ctx->max_issues; i++) {
        ProofStep *step = nav->steps[i];
        if (!step) continue;

        issues_found += check_step_justification(step, nav, i, report);

        /* 检查预处理条件区域的断言是否来自证明环境 */
        if (step->type == PROOF_STEP_ADD_CONSTRAINT && step->dependency_count == 0) {
            char location[64];
            snprintf(location, sizeof(location), "步骤 %d (添加约束)", i);
            Lv00LogicIssue *issue = logic_issue_create(
                LV00_LOGIC_ISSUE_WARNING,
                "完备性",
                "添加约束的步骤没有声明依赖。此约束的合法来源不明。",
                location,
                i,
                -1,
                "添加显式依赖链，标明此约束是从哪些公理/引理/前提推导而来。"
            );
            if (issue) {
                logic_report_add_issue(report, issue, "completeness");
                issues_found++;
                ctx->total_issues_found++;
            }
        }
    }

    /* 检查引理折叠块：验证引理在其折叠的步骤中已被证明 */
    if (nav->lemma_view_count > 0) {
        for (int l = 0; l < nav->lemma_view_count; l++) {
            int lemma_step_id = nav->lemma_view_step_ids[l];
            bool lemma_proved = false;

            for (int s = 0; s < nav->step_count; s++) {
                if (nav->steps[s] && nav->steps[s]->id == lemma_step_id && nav->steps[s]->is_completed) {
                    lemma_proved = true;
                    break;
                }
            }

            if (!lemma_proved) {
                char location[64];
                snprintf(location, sizeof(location), "引理步骤 %d", lemma_step_id);
                Lv00LogicIssue *issue = logic_issue_create(
                    LV00_LOGIC_ISSUE_ERROR,
                    "完备性",
                    "引理被引用但未完成证明。其内部证明步骤不完整。",
                    location,
                    lemma_step_id,
                    -1,
                    "完成引理的所有子证明步骤后再引用。或标记该引理为显式前提。"
                );
                if (issue) {
                    logic_report_add_issue(report, issue, "completeness");
                    issues_found++;
                    ctx->total_issues_found++;
                }
            }
        }
    }

    /* 如果没有发现问题且证明已完成，视为完备 */
    if (issues_found == 0 && nav->is_complete) {
        report->is_complete = true;
    } else if (issues_found > 0) {
        report->is_complete = false;
        if (report->error_count > 0 || report->fatal_count > 0) {
            report->overall_health = (report->overall_health == LV00_TRUE) ? LV00_UNKNOWN : report->overall_health;
        }
    } else {
        /* 证明未完成但无来源问题：信息性标记 */
        report->is_complete = false; /* 证明本身就是不完整的 */
    }

    return issues_found;
}

/* ── 综合检查 ── */

int lv00_logic_check_all(Lv00LogicContext *ctx, Lv00LogicReport *report) {
    if (!ctx || !report) {
        return -1;
    }

    int total_issues = 0;
    report->overall_health = LV00_TRUE;

    /* 执行三项检查 */
    int cons_issues = lv00_logic_check_consistency(ctx, report);
    total_issues += cons_issues;

    int circ_issues = lv00_logic_check_circularity(ctx, report);
    total_issues += circ_issues;

    int comp_issues = lv00_logic_check_completeness(ctx, report);
    total_issues += comp_issues;

    /* 综合评估 overall_health */
    if (report->fatal_count > 0) {
        report->overall_health = LV00_FALSE;
    } else if (report->error_count > 0 || report->warning_count > 0) {
        report->overall_health = LV00_UNKNOWN;
    } else {
        report->overall_health = LV00_TRUE;
    }

    /* 更新最终标志 */
    report->is_consistent = (report->consistency_issue_count == 0);
    report->is_non_circular = (report->circularity_issue_count == 0);
    report->is_complete = (report->completeness_issue_count == 0);

    return total_issues;
}

/* ── 报告导出 ── */

char *lv00_logic_report_to_text(const Lv00LogicReport *report, bool verbose) {
    if (!report) return NULL;

    /* 估算所需缓冲区大小 */
    size_t buf_size = 4096;
    char *buf = (char *)lv00_malloc(buf_size);
    if (!buf) return NULL;
    buf[0] = '\0';

    /* 总体摘要 */
    snprintf(buf, buf_size,
             "========================================\n"
             "  Lv-00 逻辑检查报告\n"
             "========================================\n\n"
             "总体评估：\n"
             "  一致性: %s\n"
             "  无循环: %s\n"
             "  完备性: %s\n"
             "  健康度: %s\n\n"
             "问题统计：\n"
             "  总数: %d\n"
             "  致命: %d\n"
             "  错误: %d\n"
             "  警告: %d\n"
             "  信息: %d\n\n",
             report->is_consistent ? "通过" : "存在问题",
             report->is_non_circular ? "通过" : "存在问题",
             report->is_complete ? "通过" : "存在问题",
             lv00_tvl_to_string(report->overall_health),
             report->total_issues,
             report->fatal_count,
             report->error_count,
             report->warning_count,
             report->info_count);

    /* 分项输出问题 */

    /* 一致性问题 */
    if (report->consistency_issue_count > 0) {
        size_t remain = buf_size - strlen(buf);
        snprintf(buf + strlen(buf), remain,
                 "--- 一致性问题 (%d) ---\n", report->consistency_issue_count);
        for (int i = 0; i < report->consistency_issue_count && verbose; i++) {
            Lv00LogicIssue *issue = report->consistency_issues[i];
            if (!issue) continue;
            size_t r = buf_size - strlen(buf);
            snprintf(buf + strlen(buf), r,
                     "  [%s] %s\n"
                     "    位置: %s\n"
                     "    建议: %s\n\n",
                     lv00_logic_issue_level_to_string(issue->level),
                     issue->description ? issue->description : "(无描述)",
                     issue->location ? issue->location : "(未知)",
                     issue->suggestion ? issue->suggestion : "(无建议)");
        }
    }

    /* 循环性问题 */
    if (report->circularity_issue_count > 0) {
        size_t remain = buf_size - strlen(buf);
        snprintf(buf + strlen(buf), remain,
                 "--- 循环性问题 (%d) ---\n", report->circularity_issue_count);
        for (int i = 0; i < report->circularity_issue_count && verbose; i++) {
            Lv00LogicIssue *issue = report->circularity_issues[i];
            if (!issue) continue;
            size_t r = buf_size - strlen(buf);
            snprintf(buf + strlen(buf), r,
                     "  [%s] %s\n"
                     "    位置: %s\n"
                     "    建议: %s\n\n",
                     lv00_logic_issue_level_to_string(issue->level),
                     issue->description ? issue->description : "(无描述)",
                     issue->location ? issue->location : "(未知)",
                     issue->suggestion ? issue->suggestion : "(无建议)");
        }
    }

    /* 完备性问题 */
    if (report->completeness_issue_count > 0) {
        size_t remain = buf_size - strlen(buf);
        snprintf(buf + strlen(buf), remain,
                 "--- 完备性问题 (%d) ---\n", report->completeness_issue_count);
        for (int i = 0; i < report->completeness_issue_count && verbose; i++) {
            Lv00LogicIssue *issue = report->completeness_issues[i];
            if (!issue) continue;
            size_t r = buf_size - strlen(buf);
            snprintf(buf + strlen(buf), r,
                     "  [%s] %s\n"
                     "    位置: %s\n"
                     "    建议: %s\n\n",
                     lv00_logic_issue_level_to_string(issue->level),
                     issue->description ? issue->description : "(无描述)",
                     issue->location ? issue->location : "(未知)",
                     issue->suggestion ? issue->suggestion : "(无建议)");
        }
    }

    if (report->total_issues == 0) {
        size_t remain = buf_size - strlen(buf);
        snprintf(buf + strlen(buf), remain,
                 "结论：未发现逻辑问题。证明通过所有自动检查。\n");
    }

    return buf;
}

char *lv00_logic_report_to_json(const Lv00LogicReport *report) {
    if (!report) return NULL;

    size_t buf_size = 8192;
    char *buf = (char *)lv00_malloc(buf_size);
    if (!buf) return NULL;

    snprintf(buf, buf_size,
             "{\n"
             "  \"overall\": {\n"
             "    \"is_consistent\": %s,\n"
             "    \"is_non_circular\": %s,\n"
             "    \"is_complete\": %s,\n"
             "    \"overall_health\": \"%s\"\n"
             "  },\n"
             "  \"statistics\": {\n"
             "    \"total_issues\": %d,\n"
             "    \"fatal\": %d,\n"
             "    \"error\": %d,\n"
             "    \"warning\": %d,\n"
             "    \"info\": %d\n"
             "  },\n"
             "  \"consistency\": {\n"
             "    \"issue_count\": %d,\n"
             "    \"passed\": %s\n"
             "  },\n"
             "  \"circularity\": {\n"
             "    \"issue_count\": %d,\n"
             "    \"passed\": %s\n"
             "  },\n"
             "  \"completeness\": {\n"
             "    \"issue_count\": %d,\n"
             "    \"passed\": %s\n"
             "  }\n"
             "}\n",
             report->is_consistent ? "true" : "false",
             report->is_non_circular ? "true" : "false",
             report->is_complete ? "true" : "false",
             lv00_tvl_to_string(report->overall_health),
             report->total_issues,
             report->fatal_count,
             report->error_count,
             report->warning_count,
             report->info_count,
             report->consistency_issue_count,
             report->is_consistent ? "true" : "false",
             report->circularity_issue_count,
             report->is_non_circular ? "true" : "false",
             report->completeness_issue_count,
             report->is_complete ? "true" : "false");

    return buf;
}

/* ── 辅助函数 ── */

const char *lv00_logic_issue_level_to_string(Lv00LogicIssueLevel level) {
    switch (level) {
    case LV00_LOGIC_ISSUE_INFO:    return "INFO";
    case LV00_LOGIC_ISSUE_WARNING: return "WARNING";
    case LV00_LOGIC_ISSUE_ERROR:   return "ERROR";
    case LV00_LOGIC_ISSUE_FATAL:   return "FATAL";
    default:                       return "UNKNOWN";
    }
}
