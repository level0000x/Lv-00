/**
 * @file conflict_detector.c
 * @brief 矛盾约束检测器实现
 *
 * @details 实现多层次的矛盾检测算法：
 * - 基础检测：O(n) 扫描明显的局部冲突
 * - 组合检测：O(n^2) 分析约束对之间的逻辑冲突
 * - 传递检测：基于图的约束传播推导间接矛盾
 * - 代数检测：方程组无解、过约束验证
 *
 * @version 3.5.0
 */

#include "conflict_detector.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constraint_graph.h"
#include "debug.h"
#include "geometry_config.h"
#include "geo_utils.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

/* ================================================================
 * 内部常量
 * ================================================================ */

#define CONFLICT_REPORT_INIT_CAPACITY 16
#define CONFLICT_MAX_DESCRIPTION_LEN 512
#define CONFLICT_MAX_SUGGESTION_LEN 256

/* ================================================================
 * 默认配置
 * ================================================================ */

static const ConflictDetectorConfig g_default_config = {
    .enable_basic_checks = true,
    .enable_combination_checks = true,
    .enable_transitive_checks = true,
    .enable_algebraic_checks = false,  /* 代数检查较耗时，默认关闭 */
    
    .max_conflicts = 100,
    .max_check_time_ms = 0,  /* 无限制 */
    
    .position_tolerance = 1e-9,
    .distance_tolerance = 1e-9,
    .angle_tolerance = 1e-6
};

const ConflictDetectorConfig *lv00_conflict_detector_default_config(void) {
    return &g_default_config;
}

/* ================================================================
 * 报告管理
 * ================================================================ */

ConflictReport *lv00_conflict_report_create(void) {
    ConflictReport *report = (ConflictReport *)lv00_malloc(sizeof(ConflictReport));
    if (!report) return NULL;
    
    memset(report, 0, sizeof(ConflictReport));
    report->capacity = CONFLICT_REPORT_INIT_CAPACITY;
    report->conflicts = (ConflictRecord *)lv00_malloc(
        sizeof(ConflictRecord) * report->capacity);
    
    if (!report->conflicts) {
        lv00_free((void **)&report);
        return NULL;
    }
    
    memset(report->conflicts, 0, sizeof(ConflictRecord) * report->capacity);
    return report;
}

void lv00_conflict_report_destroy(ConflictReport *report) {
    if (!report) return;
    
    /* 释放每个矛盾记录的动态内存 */
    for (int i = 0; i < report->conflict_count; i++) {
        ConflictRecord *rec = &report->conflicts[i];
        lv00_free((void **)&rec->node_ids);
        lv00_free((void **)&rec->constraint_ids);
        lv00_free((void **)&rec->description);
        lv00_free((void **)&rec->suggestion);
    }
    
    lv00_free((void **)&report->conflicts);
    lv00_free((void **)&report);
}

void lv00_conflict_report_clear(ConflictReport *report) {
    if (!report) return;
    
    /* 释放每个记录的动态内存但保留数组 */
    for (int i = 0; i < report->conflict_count; i++) {
        ConflictRecord *rec = &report->conflicts[i];
        lv00_free((void **)&rec->node_ids);
        lv00_free((void **)&rec->constraint_ids);
        lv00_free((void **)&rec->description);
        lv00_free((void **)&rec->suggestion);
    }
    
    memset(report->conflicts, 0, sizeof(ConflictRecord) * report->capacity);
    report->conflict_count = 0;
    report->has_critical = false;
    report->has_error = false;
    report->has_warning = false;
    memset(report->by_type, 0, sizeof(report->by_type));
}

/* 扩展报告容量 */
static bool conflict_report_ensure_capacity(ConflictReport *report) {
    if (report->conflict_count < report->capacity) return true;
    
    int new_capacity = report->capacity * 2;
    ConflictRecord *new_conflicts = (ConflictRecord *)lv00_realloc(
        report->conflicts, sizeof(ConflictRecord) * new_capacity);
    
    if (!new_conflicts) return false;
    
    /* 清零新分配的内存 */
    memset(&new_conflicts[report->capacity], 0, 
           sizeof(ConflictRecord) * (new_capacity - report->capacity));
    
    report->conflicts = new_conflicts;
    report->capacity = new_capacity;
    return true;
}

/* 添加矛盾记录 */
static bool conflict_report_add(ConflictReport *report,
                                 ConflictType type,
                                 ConflictSeverity severity,
                                 const char *description,
                                 const char *suggestion) {
    if (!conflict_report_ensure_capacity(report)) return false;
    
    ConflictRecord *rec = &report->conflicts[report->conflict_count++];
    rec->type = type;
    rec->severity = severity;
    
    if (description) {
        rec->description = lv00_strdup(description);
    }
    if (suggestion) {
        rec->suggestion = lv00_strdup(suggestion);
    }
    
    /* 更新统计 */
    if (type < 16) report->by_type[type]++;
    switch (severity) {
        case CONFLICT_SEVERITY_CRITICAL: report->has_critical = true; break;
        case CONFLICT_SEVERITY_ERROR: report->has_error = true; break;
        case CONFLICT_SEVERITY_WARNING: report->has_warning = true; break;
    }
    
    return true;
}

/* ================================================================
 * 类型名称
 * ================================================================ */

const char *lv00_conflict_type_name(ConflictType type) {
    switch (type) {
        case CONFLICT_POINT_POSITION: return "PointPositionConflict";
        case CONFLICT_DISTANCE_MISMATCH: return "DistanceMismatch";
        case CONFLICT_ANGLE_MISMATCH: return "AngleMismatch";
        case CONFLICT_COLLINEAR_VS_ANGLE: return "CollinearVsAngle";
        case CONFLICT_PERPENDICULAR_VS_PARALLEL: return "PerpendicularVsParallel";
        case CONFLICT_CONTAINMENT_VS_SEPARATION: return "ContainmentVsSeparation";
        case CONFLICT_INTERSECTION_VS_PARALLEL: return "IntersectionVsParallel";
        case CONFLICT_TRANSITIVE_EQUALITY: return "TransitiveEquality";
        case CONFLICT_TRANSITIVE_ORDER: return "TransitiveOrder";
        case CONFLICT_CYCLIC_DEPENDENCY: return "CyclicDependency";
        case CONFLICT_ALGEBRAIC_NO_SOLUTION: return "AlgebraicNoSolution";
        case CONFLICT_ALGEBRAIC_OVERCONSTRAINED: return "AlgebraicOverconstrained";
        case CONFLICT_ALGEBRAIC_SINGULAR: return "AlgebraicSingular";
        case CONFLICT_UNKNOWN: return "Unknown";
        default: return "Invalid";
    }
}

const char *lv00_conflict_severity_name(ConflictSeverity severity) {
    switch (severity) {
        case CONFLICT_SEVERITY_WARNING: return "WARNING";
        case CONFLICT_SEVERITY_ERROR: return "ERROR";
        case CONFLICT_SEVERITY_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

/* ================================================================
 * 基础约束冲突检测
 * ================================================================ */

static int expected_participant_count(ConstraintType type) {
    switch (type) {
        case INCIDENCE:
        case CONTAINMENT:
        case CONNECTION:
            return 2;
        case BETWEENNESS:
        case INTERSECTION:
            return 3;
        default:
            return -1;
    }
}

static void add_constraint_entity_ids(ConflictRecord *rec, const Constraint *constraint) {
    if (!rec || !constraint) return;
    rec->constraint_ids = (int *)lv00_malloc(sizeof(int));
    if (rec->constraint_ids) {
        rec->constraint_ids[0] = constraint->id;
        rec->constraint_count = 1;
    }
    if (constraint->participant_count > 0 && constraint->participants) {
        rec->node_ids = (int *)lv00_malloc(sizeof(int) * (size_t)constraint->participant_count);
        if (rec->node_ids) {
            for (int i = 0; i < constraint->participant_count; i++) {
                rec->node_ids[i] = constraint->participants[i];
            }
            rec->node_count = constraint->participant_count;
        }
    }
}

static bool report_constraint_conflict(ConflictReport *report,
                                       const Constraint *constraint,
                                       ConflictType type,
                                       ConflictSeverity severity,
                                       const char *description,
                                       const char *suggestion) {
    int before = report ? report->conflict_count : 0;
    if (!conflict_report_add(report, type, severity, description, suggestion)) {
        return false;
    }
    add_constraint_entity_ids(&report->conflicts[before], constraint);
    return true;
}

static bool constraint_has_duplicate_participants(const Constraint *constraint) {
    if (!constraint || !constraint->participants || constraint->participant_count <= 1) {
        return false;
    }
    for (int i = 0; i < constraint->participant_count; i++) {
        for (int j = i + 1; j < constraint->participant_count; j++) {
            if (constraint->participants[i] == constraint->participants[j]) {
                return true;
            }
        }
    }
    return false;
}

static int detect_structural_constraint_conflicts(const ConstraintGraph *graph,
                                                  const ConflictDetectorConfig *config,
                                                  ConflictReport *report) {
    (void)config;
    if (!graph || !report) return LV00_ERROR_NULL_POINTER;

    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *constraint = graph->constraints[i];
        if (!constraint || !constraint->is_active) continue;

        int expected = expected_participant_count(constraint->type);
        if (expected < 0) {
            report_constraint_conflict(
                report, constraint, CONFLICT_UNKNOWN, CONFLICT_SEVERITY_ERROR,
                "约束类型未知，无法进行语义校验。",
                "检查约束创建逻辑，确保 ConstraintType 来自合法枚举值。");
            continue;
        }

        if (constraint->participant_count != expected ||
            (constraint->participant_count > 0 && !constraint->participants)) {
            report_constraint_conflict(
                report, constraint, CONFLICT_UNKNOWN, CONFLICT_SEVERITY_CRITICAL,
                "约束参与者数量或参与者数组无效。",
                "按约束类型重新创建约束：INCIDENCE/CONTAINMENT/CONNECTION 需要2个参与者，BETWEENNESS/INTERSECTION 需要3个参与者。");
            continue;
        }

        for (int p = 0; p < constraint->participant_count; p++) {
            int node_id = constraint->participants[p];
            GeomNode *node = graph_get_node(graph, node_id);
            if (!node || !node->is_active) {
                report_constraint_conflict(
                    report, constraint, CONFLICT_UNKNOWN, CONFLICT_SEVERITY_CRITICAL,
                    "约束引用了不存在或非活跃的几何节点。",
                    "删除该约束，或先创建并激活对应的几何节点后再添加约束。");
                break;
            }
        }

        if (constraint_has_duplicate_participants(constraint)) {
            switch (constraint->type) {
                case BETWEENNESS:
                    report_constraint_conflict(
                        report, constraint, CONFLICT_TRANSITIVE_ORDER, CONFLICT_SEVERITY_ERROR,
                        "BETWEENNESS 约束出现重复参与点，导致“点在自身与另一点之间”的退化关系。",
                        "BETWEENNESS 应使用三个互不相同的点，并在上层构造前校验输入。");
                    break;
                case INTERSECTION:
                    report_constraint_conflict(
                        report, constraint, CONFLICT_INTERSECTION_VS_PARALLEL, CONFLICT_SEVERITY_ERROR,
                        "INTERSECTION 约束出现重复参与对象，形成自相交或交点与线对象混同的退化关系。",
                        "INTERSECTION 应使用两条不同几何对象和一个独立交点。");
                    break;
                case CONNECTION:
                    report_constraint_conflict(
                        report, constraint, CONFLICT_CYCLIC_DEPENDENCY, CONFLICT_SEVERITY_ERROR,
                        "CONNECTION 约束将端口连接到自身，形成直接循环依赖。",
                        "拆除自连接，使用不同的输入/输出端口建立连接。");
                    break;
                default:
                    report_constraint_conflict(
                        report, constraint, CONFLICT_UNKNOWN, CONFLICT_SEVERITY_WARNING,
                        "约束包含重复参与者，可能表示退化几何关系。",
                        "检查该约束是否确实允许自引用；若不允许，应拆分或删除该约束。");
                    break;
            }
        }
    }

    return 0;
}

/**
 * @brief 检测点位置约束冲突
 *
 * 检查同一点是否被约束到不同位置，或点的坐标是否满足所有关联约束。
 */
static int detect_point_position_conflicts(const ConstraintGraph *graph,
                                            const ConflictDetectorConfig *config,
                                            ConflictReport *report) {
    if (!graph || !report) return LV00_ERROR_NULL_POINTER;
    
    const double eps = config ? config->position_tolerance : g_default_config.position_tolerance;
    
    /* 遍历所有点节点 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_POINT) continue;
        
        /* 检查点是否有有效坐标 */
        if (!node->symbolic_coords || node->coord_count < 2) continue;
        
        SymbolicCoord *x_coord = node->symbolic_coords[0];
        SymbolicCoord *y_coord = node->symbolic_coords[1];
        
        if (!x_coord || !y_coord) continue;
        
        /* TODO: 检查该点的所有关联约束是否一致 */
        /* 例如：如果点被约束到两条不相交的线上，则存在矛盾 */
    }
    
    return 0;
}

/**
 * @brief 检测距离约束冲突
 *
 * 检查距离约束是否与几何对象的实际距离一致。
 */
static int detect_distance_conflicts(const ConstraintGraph *graph,
                                      const ConflictDetectorConfig *config,
                                      ConflictReport *report) {
    if (!graph || !report) return LV00_ERROR_NULL_POINTER;
    
    const double eps = config ? config->distance_tolerance : g_default_config.distance_tolerance;
    
    /* 遍历所有约束，查找距离相关的约束 */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *cons = graph->constraints[i];
        if (!cons) continue;
        
        /* TODO: 实现距离约束检查 */
        /* 需要解析约束参数，计算实际距离，与约束值比较 */
    }
    
    return 0;
}

/**
 * @brief 快速基础检测
 */
static int detect_basic_conflicts(const ConstraintGraph *graph,
                                   const ConflictDetectorConfig *config,
                                   ConflictReport *report) {
    int err;

    err = detect_structural_constraint_conflicts(graph, config, report);
    if (err != 0) return err;
    
    err = detect_point_position_conflicts(graph, config, report);
    if (err != 0) return err;
    
    err = detect_distance_conflicts(graph, config, report);
    if (err != 0) return err;
    
    return 0;
}

/* ================================================================
 * 约束组合冲突检测
 * ================================================================ */

/**
 * @brief 检测两个约束之间的逻辑冲突
 */
static int check_constraint_pair_conflict(const ConstraintGraph *graph,
                                           Constraint *c1,
                                           Constraint *c2,
                                           const ConflictDetectorConfig *config,
                                           ConflictReport *report) {
    if (!c1 || !c2) return 0;
    
    /* 垂直 vs 平行：同一对线不能同时垂直和平行 */
    if ((c1->type == INCIDENCE && c2->type == INCIDENCE)) {
        /* TODO: 检查是否涉及垂直和平行的混合 */
    }
    
    /* 相交 vs 平行：两条线不能同时相交和平行 */
    if ((c1->type == INTERSECTION && c2->type == INCIDENCE) ||
        (c1->type == INCIDENCE && c2->type == INTERSECTION)) {
        /* TODO: 检查是否涉及同一线对 */
    }
    
    /* 包含 vs 分离：一个对象不能同时被包含和分离 */
    if ((c1->type == CONTAINMENT && c2->type == INCIDENCE) ||
        (c1->type == INCIDENCE && c2->type == CONTAINMENT)) {
        /* TODO: 检查是否涉及同一对象对 */
    }
    
    return 0;
}

/**
 * @brief 检测所有约束组合冲突
 */
static int detect_combination_conflicts(const ConstraintGraph *graph,
                                         const ConflictDetectorConfig *config,
                                         ConflictReport *report) {
    if (!graph || !report) return LV00_ERROR_NULL_POINTER;
    
    /* 检查所有约束对 */
    for (int i = 0; i < graph->constraint_count; i++) {
        for (int j = i + 1; j < graph->constraint_count; j++) {
            int err = check_constraint_pair_conflict(
                graph, graph->constraints[i], graph->constraints[j],
                config, report);
            if (err != 0) return err;
        }
    }
    
    return 0;
}

/* ================================================================
 * 传递闭包冲突检测
 * ================================================================ */

/**
 * @brief 检测传递等式矛盾
 *
 * 检查是否存在 A=B, B=C, 但 A≠C 的情况。
 */
static int detect_transitive_equality_conflicts(const ConstraintGraph *graph,
                                                 const ConflictDetectorConfig *config,
                                                 ConflictReport *report) {
    /* TODO: 实现传递等式检测 */
    /* 需要构建等式关系图，检查连通分量的等价性 */
    return 0;
}

/**
 * @brief 检测循环依赖矛盾
 */
static int detect_cyclic_dependency_conflicts(const ConstraintGraph *graph,
                                               const ConflictDetectorConfig *config,
                                               ConflictReport *report) {
    /* TODO: 实现循环依赖检测 */
    /* 使用 DFS 或拓扑排序检测约束图中的环 */
    return 0;
}

/**
 * @brief 传递闭包检测
 */
static int detect_transitive_conflicts(const ConstraintGraph *graph,
                                        const ConflictDetectorConfig *config,
                                        ConflictReport *report) {
    int err;
    
    err = detect_transitive_equality_conflicts(graph, config, report);
    if (err != 0) return err;
    
    err = detect_cyclic_dependency_conflicts(graph, config, report);
    if (err != 0) return err;
    
    return 0;
}

/* ================================================================
 * 公共 API 实现
 * ================================================================ */

int lv00_conflict_detect_all(const ConstraintGraph *graph,
                              const ConflictDetectorConfig *config,
                              ConflictReport *report) {
    if (!graph || !report) return LV00_ERROR_NULL_POINTER;
    
    const ConflictDetectorConfig *cfg = config ? config : &g_default_config;
    int err = 0;
    
    /* 清空之前的报告 */
    lv00_conflict_report_clear(report);
    
    /* 1. 基础检测 */
    if (cfg->enable_basic_checks) {
        err = detect_basic_conflicts(graph, cfg, report);
        if (err != 0) return err;
    }
    
    /* 2. 组合检测 */
    if (cfg->enable_combination_checks && 
        (cfg->max_conflicts == 0 || report->conflict_count < cfg->max_conflicts)) {
        err = detect_combination_conflicts(graph, cfg, report);
        if (err != 0) return err;
    }
    
    /* 3. 传递检测 */
    if (cfg->enable_transitive_checks &&
        (cfg->max_conflicts == 0 || report->conflict_count < cfg->max_conflicts)) {
        err = detect_transitive_conflicts(graph, cfg, report);
        if (err != 0) return err;
    }
    
    /* 4. 代数检测（较耗时，最后执行） */
    if (cfg->enable_algebraic_checks &&
        (cfg->max_conflicts == 0 || report->conflict_count < cfg->max_conflicts)) {
        /* TODO: 实现代数冲突检测 */
    }
    
    return 0;
}

bool lv00_conflict_detect_quick(const ConstraintGraph *graph) {
    if (!graph) return false;
    
    ConflictReport *report = lv00_conflict_report_create();
    if (!report) return false;
    
    ConflictDetectorConfig quick_config = g_default_config;
    quick_config.enable_combination_checks = false;
    quick_config.enable_transitive_checks = false;
    quick_config.enable_algebraic_checks = false;
    
    int err = detect_basic_conflicts(graph, &quick_config, report);
    bool has_conflict = (err == 0 && report->conflict_count > 0);
    
    lv00_conflict_report_destroy(report);
    return has_conflict;
}

int lv00_conflict_detect_for_node(const ConstraintGraph *graph,
                                   int node_id,
                                   ConflictReport *report) {
    if (!graph || !report) return LV00_ERROR_NULL_POINTER;
    
    /* TODO: 实现针对特定节点的检测 */
    /* 只检查涉及该节点的约束 */
    
    return 0;
}

int lv00_conflict_detect_for_constraint(const ConstraintGraph *graph,
                                         int constraint_id,
                                         ConflictReport *report) {
    if (!graph || !report) return LV00_ERROR_NULL_POINTER;
    
    /* TODO: 实现针对特定约束的检测 */
    /* 检查该约束与其他所有约束的兼容性 */
    
    return 0;
}

/* ================================================================
 * 报告输出
 * ================================================================ */

void lv00_conflict_report_print(const ConflictReport *report,
                                 void *output,
                                 bool verbose) {
    if (!report) return;
    
    FILE *out = output ? (FILE *)output : stdout;
    
    fprintf(out, "=== Conflict Detection Report ===\n");
    fprintf(out, "Total conflicts: %d\n", report->conflict_count);
    fprintf(out, "Critical: %s, Error: %s, Warning: %s\n",
            report->has_critical ? "YES" : "no",
            report->has_error ? "YES" : "no",
            report->has_warning ? "YES" : "no");
    
    if (report->conflict_count == 0) {
        fprintf(out, "No conflicts detected.\n");
        return;
    }
    
    fprintf(out, "\n--- Conflict Details ---\n");
    for (int i = 0; i < report->conflict_count; i++) {
        const ConflictRecord *rec = &report->conflicts[i];
        fprintf(out, "\n[%d] %s: %s\n", i + 1,
                lv00_conflict_severity_name(rec->severity),
                lv00_conflict_type_name(rec->type));
        
        if (rec->description) {
            fprintf(out, "    Description: %s\n", rec->description);
        }
        if (verbose && rec->suggestion) {
            fprintf(out, "    Suggestion: %s\n", rec->suggestion);
        }
        if (verbose && rec->node_count > 0) {
            fprintf(out, "    Nodes: ");
            for (int j = 0; j < rec->node_count; j++) {
                fprintf(out, "%d ", rec->node_ids[j]);
            }
            fprintf(out, "\n");
        }
    }
}

int lv00_conflict_report_to_json(const ConflictReport *report,
                                  char *buffer,
                                  size_t buffer_size) {
    if (!report || !buffer || buffer_size == 0) return -1;
    
    /* 简化的 JSON 序列化 */
    int written = snprintf(buffer, buffer_size,
        "{\"conflict_count\":%d,\"has_critical\":%s,\"has_error\":%s,\"has_warning\":%s,\"conflicts\":[",
        report->conflict_count,
        report->has_critical ? "true" : "false",
        report->has_error ? "true" : "false",
        report->has_warning ? "true" : "false");
    
    if (written < 0 || (size_t)written >= buffer_size) return -1;
    
    size_t pos = (size_t)written;
    
    for (int i = 0; i < report->conflict_count; i++) {
        const ConflictRecord *rec = &report->conflicts[i];
        
        int n = snprintf(buffer + pos, buffer_size - pos,
            "%s{\"type\":\"%s\",\"severity\":\"%s\"}",
            i > 0 ? "," : "",
            lv00_conflict_type_name(rec->type),
            lv00_conflict_severity_name(rec->severity));
        
        if (n < 0 || (size_t)n >= buffer_size - pos) return -1;
        pos += (size_t)n;
    }
    
    if (pos + 2 >= buffer_size) return -1;
    buffer[pos++] = ']';
    buffer[pos++] = '}';
    buffer[pos] = '\0';
    
    return (int)pos;
}

/* ================================================================
 * 便捷函数
 * ================================================================ */

bool lv00_conflict_graph_has_conflicts(const ConstraintGraph *graph) {
    return lv00_conflict_detect_quick(graph);
}

ConflictType lv00_conflict_get_worst_type(const ConflictReport *report) {
    if (!report || report->conflict_count == 0) return CONFLICT_UNKNOWN;
    
    /* 按严重程度优先级返回 */
    for (int i = 0; i < report->conflict_count; i++) {
        if (report->conflicts[i].severity == CONFLICT_SEVERITY_CRITICAL) {
            return report->conflicts[i].type;
        }
    }
    for (int i = 0; i < report->conflict_count; i++) {
        if (report->conflicts[i].severity == CONFLICT_SEVERITY_ERROR) {
            return report->conflicts[i].type;
        }
    }
    return report->conflicts[0].type;
}
