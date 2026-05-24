/**
 * @file engine_scheduler.c
 * @brief 多引擎调度框架实现 —— 后端注册、自动路由与分发求解
 *
 * @details 实现引擎调度器的完整功能：
 *          - 约束图特征提取与分析
 *          - 路由规则管理与匹配
 *          - 后端自动选择与回退
 *          - 分发求解管线
 *          - 结果转换（SMT → Gröbner 兼容）
 *
 *          调度器本身不实现求解逻辑，它纯粹是路由和编排层。
 *          所有求解能力由注册的后端提供。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "engine_scheduler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constraint_graph.h"
#include "lv00_internal.h"
#include "lv00_utils.h"
#include "smt_backend.h"
#include "solver.h"

/* ============================================================
 * 内部常量
 * ============================================================ */

/** @brief 诊断输出缓冲区的默认大小 */
#define SCHEDULER_DIAG_BUF_SIZE 4096

/* ============================================================
 * 图特征分析
 * ============================================================ */

void scheduler_features_init(GraphFeatures *features) {
    if (!features) return;
    memset(features, 0, sizeof(GraphFeatures));
}

int scheduler_analyze_graph(const ConstraintGraph *graph, GraphFeatures *features) {
    LV00_CHECK_NULL(graph, -1);
    LV00_CHECK_NULL(features, -1);

    memset(features, 0, sizeof(GraphFeatures));

    features->total_nodes = graph->node_count;
    features->total_constraints = graph->constraint_count;

    /* 遍历节点，统计类型分布 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node) continue;

        switch (node->type) {
        case GEOM_POINT:
            /* 检查是否有确定坐标：有理数坐标且非零坐标计数 > 0 视为已确定 */
            if (node->symbolic_coords && node->coord_count > 0) {
                bool is_fixed = true;
                for (int j = 0; j < node->coord_count && is_fixed; j++) {
                    if (!node->symbolic_coords[j]) {
                        is_fixed = false;
                    }
                }
                if (is_fixed) {
                    features->fixed_nodes++;
                } else {
                    features->variable_nodes++;
                }
            } else {
                features->variable_nodes++;
            }
            break;
        case GEOM_PORT:
            features->port_nodes++;
            break;
        case GEOM_FUNCTION_BLOCK:
            features->block_nodes++;
            break;
        default:
            break;
        }
    }

    /* 遍历约束，统计类型分布和特征 */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *con = graph->constraints[i];
        if (!con) continue;

        switch (con->type) {
        case INCIDENCE:
            features->incidence_constraints++;
            break;
        case BETWEENNESS:
            features->betweenness_constraints++;
            break;
        case INTERSECTION:
            features->intersection_constraints++;
            break;
        case CONTAINMENT:
            features->containment_constraints++;
            break;
        case CONNECTION:
            features->connection_constraints++;
            break;
        }

        /* 启发式检测非线性特征：
         * - BETWEENNESS 涉及共线条件（线性）
         * - CONTAINMENT 涉及卷绕数（非线性/类量词） */
        if (con->type == CONTAINMENT) {
            features->has_quantifier_like = true;
            features->nonlinear_constraints++;
        }
        if (con->type == INTERSECTION && con->participant_count > 2) {
            /* 多参与者相交通常产生二次方程 */
            features->nonlinear_constraints++;
        }
    }

    /* 计算非线性占比 */
    if (features->total_constraints > 0) {
        features->nonlinear_ratio = (double)features->nonlinear_constraints /
                                    (double)features->total_constraints;
    }

    /* 估计方程数和最高度数 */
    features->estimated_equation_count = features->incidence_constraints +
                                         features->betweenness_constraints +
                                         features->intersection_constraints * 2;
    features->estimated_degree_max = (features->nonlinear_constraints > 0) ? 2 : 1;

    return 0;
}

/* ============================================================
 * 路由规则
 * ============================================================ */

RoutingRule *scheduler_rule_create(void) {
    RoutingRule *rule = lv00_malloc(sizeof(RoutingRule));
    if (!rule) return NULL;
    memset(rule, 0, sizeof(RoutingRule));
    rule->enabled = true;
    rule->priority = 100;
    rule->target_backend = GROEBNER;
    return rule;
}

void scheduler_rule_destroy(RoutingRule *rule) {
    lv00_free((void **)&rule);
}

static bool scheduler_rule_check_condition(const RouteCondition *cond,
                                            const GraphFeatures *features) {
    switch (cond->type) {
    case ROUTE_COND_NONE:
    case ROUTE_COND_ALWAYS:
        return true;
    case ROUTE_COND_VAR_COUNT_LE:
        return features->variable_nodes <= (int)cond->threshold;
    case ROUTE_COND_VAR_COUNT_GE:
        return features->variable_nodes >= (int)cond->threshold;
    case ROUTE_COND_NONLINEAR_RATIO_GE:
        return features->nonlinear_ratio >= cond->threshold;
    case ROUTE_COND_HAS_QUANTIFIER:
        return features->has_quantifier_like;
    case ROUTE_COND_HAS_BOOLEAN:
        return features->has_boolean_variables;
    case ROUTE_COND_DEGREE_GE:
        return features->estimated_degree_max >= (int)cond->threshold;
    case ROUTE_COND_BACKEND_AVAILABLE:
        return smtsolver_is_backend_available(cond->backend);
    default:
        return false;
    }
}

bool scheduler_rule_matches(const RoutingRule *rule, const GraphFeatures *features) {
    if (!rule || !features || !rule->enabled) return false;
    if (rule->condition_count == 0) return true;

    bool has_match = false;

    for (int i = 0; i < rule->condition_count; i++) {
        bool cond_match = scheduler_rule_check_condition(&rule->conditions[i], features);
        if (rule->combine_mode == ROUTE_COMBINE_AND) {
            if (!cond_match) return false;
            has_match = true;
        } else {
            /* OR 模式 */
            if (cond_match) return true;
        }
    }

    /* AND 模式下所有条件都通过 */
    if (rule->combine_mode == ROUTE_COMBINE_AND) {
        return has_match || rule->condition_count == 0;
    }

    return false;
}

/* ============================================================
 * 调度器生命周期
 * ============================================================ */

EngineScheduler *scheduler_create(void) {
    EngineScheduler *sch = lv00_malloc(sizeof(EngineScheduler));
    if (!sch) return NULL;
    memset(sch, 0, sizeof(EngineScheduler));

    /* 初始化后端注册表 */
    sch->registry = smtsolver_get_registry();
    sch->backend_cache_count = 0;

    /* 默认配置 */
    sch->default_backend = GROEBNER;
    sch->enable_fallback = true;
    sch->auto_create_backends = true;

    /* 默认回退链：GROEBNER 作为最终兜底 */
    sch->fallback_chain[0] = GROEBNER;
    sch->fallback_depth = 1;

    /* 加载预置路由规则 */
    scheduler_load_preset_rules(sch);

    /* 清零统计 */
    memset(&sch->stats, 0, sizeof(SchedulerStats));

    return sch;
}

void scheduler_destroy(EngineScheduler *scheduler) {
    if (!scheduler) return;

    /* 销毁所有后端缓存实例 */
    for (int i = 0; i < scheduler->backend_cache_count; i++) {
        if (scheduler->backend_cache[i]) {
            smtsolver_destroy(scheduler->backend_cache[i]);
            scheduler->backend_cache[i] = NULL;
        }
    }
    scheduler->backend_cache_count = 0;

    /* 销毁路由规则 */
    for (int i = 0; i < scheduler->routing_rule_count; i++) {
        scheduler_rule_destroy(scheduler->routing_rules[i]);
        scheduler->routing_rules[i] = NULL;
    }
    scheduler->routing_rule_count = 0;

    lv00_free((void **)&scheduler);
}

void scheduler_reset(EngineScheduler *scheduler) {
    if (!scheduler) return;

    /* 清除后端缓存 */
    for (int i = 0; i < scheduler->backend_cache_count; i++) {
        if (scheduler->backend_cache[i]) {
            smtsolver_destroy(scheduler->backend_cache[i]);
            scheduler->backend_cache[i] = NULL;
        }
    }
    scheduler->backend_cache_count = 0;

    /* 保留注册表，但清除自定义路由规则 */
    for (int i = 0; i < scheduler->routing_rule_count; i++) {
        scheduler_rule_destroy(scheduler->routing_rules[i]);
        scheduler->routing_rules[i] = NULL;
    }
    scheduler->routing_rule_count = 0;

    /* 重新加载预置规则 */
    scheduler_load_preset_rules(scheduler);

    /* 重置统计 */
    memset(&scheduler->stats, 0, sizeof(SchedulerStats));
}

/* ============================================================
 * 后端注册管理
 * ============================================================ */

int scheduler_register_backend(EngineScheduler *scheduler, SolverBackendType type,
                                SMTSolverCreateFunc create_func, int priority,
                                const char *description) {
    LV00_CHECK_NULL(scheduler, -1);

    if (type >= COUNT) {
        lv00_set_error_ctx(LV00_ERROR_INVALID_PARAM, __FILE__, __LINE__, __func__,
                           "无效的后端类型: %d", (int)type);
        return -1;
    }

    SMTBackendRegistry *reg = scheduler->registry;
    if (!reg) {
        reg = smtsolver_get_registry();
        scheduler->registry = reg;
    }

    /* 检查是否已注册 */
    for (int i = 0; i < reg->count; i++) {
        if (reg->entries[i].type == type) {
            /* 更新现有条目 */
            reg->entries[i].create_func = create_func;
            reg->entries[i].priority = priority;
            reg->entries[i].description = description;
            reg->entries[i].available = smtsolver_is_backend_available(type);
            return 0;
        }
    }

    /* 新注册 */
    if (reg->count >= SMT_BACKEND_REGISTRY_CAPACITY) {
        lv00_set_error_ctx(LV00_ERROR_RESOURCE_EXHAUSTED, __FILE__, __LINE__, __func__,
                           "注册表已满 (容量=%d)", SMT_BACKEND_REGISTRY_CAPACITY);
        return -1;
    }

    SMTBackendEntry *entry = &reg->entries[reg->count];
    entry->type = type;
    entry->available = smtsolver_is_backend_available(type);
    entry->create_func = create_func;
    entry->priority = priority;
    entry->description = description;
    reg->count++;

    return 0;
}

int scheduler_unregister_backend(EngineScheduler *scheduler, SolverBackendType type) {
    LV00_CHECK_NULL(scheduler, -1);

    SMTBackendRegistry *reg = scheduler->registry;
    if (!reg) return -1;

    for (int i = 0; i < reg->count; i++) {
        if (reg->entries[i].type == type) {
            /* 移除：将后续条目向前移动 */
            for (int j = i; j < reg->count - 1; j++) {
                reg->entries[j] = reg->entries[j + 1];
            }
            memset(&reg->entries[reg->count - 1], 0, sizeof(SMTBackendEntry));
            reg->count--;

            /* 同时清除缓存 */
            for (int k = 0; k < scheduler->backend_cache_count; k++) {
                if (scheduler->backend_cache[k] &&
                    smtsolver_get_type(scheduler->backend_cache[k]) == type) {
                    smtsolver_destroy(scheduler->backend_cache[k]);
                    scheduler->backend_cache[k] = NULL;
                }
            }
            return 0;
        }
    }

    lv00_set_error_ctx(LV00_ERROR_NOT_FOUND, __FILE__, __LINE__, __func__,
                       "后端未注册: type=%d", (int)type);
    return -1;
}

int scheduler_list_available_backends(const EngineScheduler *scheduler,
                                       SolverBackendType *out_types, int max_count) {
    LV00_CHECK_NULL(scheduler, -1);
    LV00_CHECK_NULL(out_types, -1);

    SMTBackendRegistry *reg = scheduler->registry;
    if (!reg) return 0;

    int count = 0;
    for (int i = 0; i < reg->count && count < max_count; i++) {
        if (reg->entries[i].available) {
            out_types[count++] = reg->entries[i].type;
        }
    }
    return count;
}

/* ============================================================
 * 路由规则管理
 * ============================================================ */

int scheduler_add_routing_rule(EngineScheduler *scheduler, RoutingRule *rule) {
    LV00_CHECK_NULL(scheduler, -1);
    LV00_CHECK_NULL(rule, -1);

    /* 检查是否已存在同名规则，存在则替换 */
    for (int i = 0; i < scheduler->routing_rule_count; i++) {
        if (scheduler->routing_rules[i] &&
            strcmp(scheduler->routing_rules[i]->name, rule->name) == 0) {
            scheduler_rule_destroy(scheduler->routing_rules[i]);
            scheduler->routing_rules[i] = rule;
            return 0;
        }
    }

    /* 新增 */
    if (scheduler->routing_rule_count >= SCHEDULER_MAX_ROUTING_RULES) {
        lv00_set_error_ctx(LV00_ERROR_RESOURCE_EXHAUSTED, __FILE__, __LINE__, __func__,
                           "路由规则表已满 (容量=%d)", SCHEDULER_MAX_ROUTING_RULES);
        return -1;
    }

    /* 按优先级插入 */
    int insert_pos = scheduler->routing_rule_count;
    for (int i = 0; i < scheduler->routing_rule_count; i++) {
        if (rule->priority < scheduler->routing_rules[i]->priority) {
            insert_pos = i;
            break;
        }
    }

    for (int i = scheduler->routing_rule_count; i > insert_pos; i--) {
        scheduler->routing_rules[i] = scheduler->routing_rules[i - 1];
    }
    scheduler->routing_rules[insert_pos] = rule;
    scheduler->routing_rule_count++;

    return 0;
}

int scheduler_remove_routing_rule(EngineScheduler *scheduler, const char *name) {
    LV00_CHECK_NULL(scheduler, -1);
    LV00_CHECK_NULL(name, -1);

    for (int i = 0; i < scheduler->routing_rule_count; i++) {
        if (scheduler->routing_rules[i] &&
            strcmp(scheduler->routing_rules[i]->name, name) == 0) {
            scheduler_rule_destroy(scheduler->routing_rules[i]);
            for (int j = i; j < scheduler->routing_rule_count - 1; j++) {
                scheduler->routing_rules[j] = scheduler->routing_rules[j + 1];
            }
            scheduler->routing_rules[scheduler->routing_rule_count - 1] = NULL;
            scheduler->routing_rule_count--;
            return 0;
        }
    }

    lv00_set_error_ctx(LV00_ERROR_NOT_FOUND, __FILE__, __LINE__, __func__,
                       "路由规则未找到: %s", name);
    return -1;
}

int scheduler_load_preset_rules(EngineScheduler *scheduler) {
    LV00_CHECK_NULL(scheduler, -1);

    int loaded = 0;

    /* ================================================================
     * 规则 1: "quantifier-cvc5"
     * 条件: 含类量词约束 AND cvc5 可用
     * 目标: SMT_CVC5
     * 优先级: 0（最高）
     * ================================================================ */
    {
        RoutingRule *rule = scheduler_rule_create();
        if (rule) {
            snprintf(rule->name, sizeof(rule->name), "quantifier-cvc5");
            rule->priority = 0;
            rule->enabled = true;
            rule->conditions[0].type = ROUTE_COND_HAS_QUANTIFIER;
            rule->conditions[1].type = ROUTE_COND_BACKEND_AVAILABLE;
            rule->conditions[1].backend = SMT_CVC5;
            rule->condition_count = 2;
            rule->combine_mode = ROUTE_COMBINE_AND;
            rule->target_backend = SMT_CVC5;
            if (scheduler_add_routing_rule(scheduler, rule) == 0) loaded++;
        }
    }

    /* ================================================================
     * 规则 2: "nonlinear-smt"
     * 条件: 非线性占比 >= 0.3 AND (Z3 可用 OR cvc5 可用)
     * 目标: 优先 Z3，其次 cvc5
     * 优先级: 10
     * ================================================================ */
    {
        RoutingRule *rule = scheduler_rule_create();
        if (rule) {
            snprintf(rule->name, sizeof(rule->name), "nonlinear-smt");
            rule->priority = 10;
            rule->enabled = true;
            rule->conditions[0].type = ROUTE_COND_NONLINEAR_RATIO_GE;
            rule->conditions[0].threshold = 0.3;
            rule->condition_count = 1;
            rule->combine_mode = ROUTE_COMBINE_AND;
            if (smtsolver_is_backend_available(SMT_Z3)) {
                rule->target_backend = SMT_Z3;
            } else if (smtsolver_is_backend_available(SMT_CVC5)) {
                rule->target_backend = SMT_CVC5;
            } else {
                rule->target_backend = GROEBNER;
            }
            if (scheduler_add_routing_rule(scheduler, rule) == 0) loaded++;
        }
    }

    /* ================================================================
     * 规则 3: "small-groebner"
     * 条件: 变量数 < 50
     * 目标: GROEBNER
     * 优先级: 20
     * ================================================================ */
    {
        RoutingRule *rule = scheduler_rule_create();
        if (rule) {
            snprintf(rule->name, sizeof(rule->name), "small-groebner");
            rule->priority = 20;
            rule->enabled = true;
            rule->conditions[0].type = ROUTE_COND_VAR_COUNT_LE;
            rule->conditions[0].threshold = 49.0;
            rule->condition_count = 1;
            rule->combine_mode = ROUTE_COMBINE_AND;
            rule->target_backend = GROEBNER;
            if (scheduler_add_routing_rule(scheduler, rule) == 0) loaded++;
        }
    }

    /* ================================================================
     * 规则 4: "large-smt"
     * 条件: 变量数 >= 50 AND 非线性约束 > 0
     * 目标: 优先 Z3，其次 cvc5
     * 优先级: 30
     * ================================================================ */
    {
        RoutingRule *rule = scheduler_rule_create();
        if (rule) {
            snprintf(rule->name, sizeof(rule->name), "large-smt");
            rule->priority = 30;
            rule->enabled = true;
            rule->conditions[0].type = ROUTE_COND_VAR_COUNT_GE;
            rule->conditions[0].threshold = 50.0;
            rule->condition_count = 1;
            rule->combine_mode = ROUTE_COMBINE_AND;
            if (smtsolver_is_backend_available(SMT_Z3)) {
                rule->target_backend = SMT_Z3;
            } else if (smtsolver_is_backend_available(SMT_CVC5)) {
                rule->target_backend = SMT_CVC5;
            } else {
                rule->target_backend = GROEBNER;
            }
            if (scheduler_add_routing_rule(scheduler, rule) == 0) loaded++;
        }
    }

    /* ================================================================
     * 规则 5: "default-groebner"
     * 条件: 无条件（兜底）
     * 目标: GROEBNER
     * 优先级: 100（最低）
     * ================================================================ */
    {
        RoutingRule *rule = scheduler_rule_create();
        if (rule) {
            snprintf(rule->name, sizeof(rule->name), "default-groebner");
            rule->priority = 100;
            rule->enabled = true;
            rule->conditions[0].type = ROUTE_COND_ALWAYS;
            rule->condition_count = 1;
            rule->combine_mode = ROUTE_COMBINE_AND;
            rule->target_backend = GROEBNER;
            if (scheduler_add_routing_rule(scheduler, rule) == 0) loaded++;
        }
    }

    return loaded;
}

/* ============================================================
 * 后端缓存管理
 * ============================================================ */

/**
 * @brief 从缓存获取或创建后端实例
 *
 * @param[in,out] scheduler  调度器
 * @param[in]     type       后端类型
 * @return 后端实例句柄，失败返回 NULL
 */
static SMTSolver *scheduler_get_or_create_backend(EngineScheduler *scheduler,
                                                   SolverBackendType type) {
    if (!scheduler || type >= COUNT) return NULL;

    /* 检查缓存 */
    for (int i = 0; i < scheduler->backend_cache_count; i++) {
        if (scheduler->backend_cache[i] &&
            smtsolver_get_type(scheduler->backend_cache[i]) == type) {
            return scheduler->backend_cache[i];
        }
    }

    /* 未命中，检查可用性 */
    if (!smtsolver_is_backend_available(type)) return NULL;
    if (!scheduler->auto_create_backends) return NULL;

    /* 创建并缓存 */
    const SMTSolverConfig *config = smtsolver_default_config(type);
    SMTSolver *solver = smtsolver_create(type, config);
    if (!solver) return NULL;

    if (scheduler->backend_cache_count < SCHEDULER_MAX_BACKEND_INSTANCES) {
        scheduler->backend_cache[scheduler->backend_cache_count++] = solver;
    } else {
        /* 缓存已满，但不影响使用（调用者须自行管理） */
    }

    return solver;
}

/* ============================================================
 * 后端选择
 * ============================================================ */

SolverBackendType scheduler_select_backend(const EngineScheduler *scheduler,
                                            const ConstraintGraph *graph,
                                            char *out_reason, size_t reason_size) {
    if (!scheduler || !graph) {
        if (out_reason && reason_size > 0) {
            snprintf(out_reason, reason_size, "无效输入：scheduler=%p graph=%p",
                     (const void *)scheduler, (const void *)graph);
        }
        return COUNT;
    }

    /* Step 1: 分析图特征 */
    GraphFeatures features;
    if (scheduler_analyze_graph(graph, &features) != 0) {
        if (out_reason && reason_size > 0) {
            snprintf(out_reason, reason_size, "图特征分析失败");
        }
        return scheduler->default_backend;
    }

    /* Step 2: 按优先级评估路由规则 */
    for (int i = 0; i < scheduler->routing_rule_count; i++) {
        RoutingRule *rule = scheduler->routing_rules[i];
        if (!rule || !rule->enabled) continue;

        if (scheduler_rule_matches(rule, &features)) {
            /* 检查目标后端是否可用 */
            if (smtsolver_is_backend_available(rule->target_backend)) {
                if (out_reason && reason_size > 0) {
                    snprintf(out_reason, reason_size,
                             "匹配规则 '%s' (priority=%d): 变量数=%d < 50, "
                             "非线性占比=%.2f",
                             rule->name, rule->priority,
                             features.variable_nodes, features.nonlinear_ratio);
                }
                return rule->target_backend;
            }
        }
    }

    /* Step 3: 无规则匹配，使用默认后端 */
    if (out_reason && reason_size > 0) {
        snprintf(out_reason, reason_size, "无规则匹配，使用默认后端: %s",
                 smtsolver_backend_type_name(scheduler->default_backend));
    }
    return scheduler->default_backend;
}

/* ============================================================
 * 分发求解
 * ============================================================ */

int scheduler_solve(EngineScheduler *scheduler, const ConstraintGraph *graph,
                    SMTSolverResult *out_result) {
    LV00_CHECK_NULL(scheduler, -1);
    LV00_CHECK_NULL(graph, -1);
    LV00_CHECK_NULL(out_result, -1);

    /* Step 1: 自动选择后端 */
    char reason[256] = {0};
    SolverBackendType selected = scheduler_select_backend(scheduler, graph,
                                                          reason, sizeof(reason));

    /* Step 2: 依次尝试回退链 */
    SolverBackendType attempted[SCHEDULER_MAX_FALLBACK_DEPTH + 1];
    int attempt_count = 0;

    SMTSolver *solver = NULL;
    int ret = -1;

    /* 首选后端 */
    attempted[attempt_count++] = selected;

    solver = scheduler_get_or_create_backend(scheduler, selected);
    if (solver) {
        ret = smtsolver_solve(solver, graph, out_result);
        if (ret >= 0) {
            scheduler->stats.total_solves++;
            scheduler->stats.backend_solve_counts[selected]++;
            return ret;
        }
    }

    /* 回退链 */
    if (scheduler->enable_fallback) {
        for (int i = 0; i < scheduler->fallback_depth; i++) {
            SolverBackendType fb = scheduler->fallback_chain[i];
            if (fb == selected) continue;

            /* 检查是否已尝试过 */
            bool already_attempted = false;
            for (int j = 0; j < attempt_count; j++) {
                if (attempted[j] == fb) { already_attempted = true; break; }
            }
            if (already_attempted) continue;

            attempted[attempt_count++] = fb;

            solver = scheduler_get_or_create_backend(scheduler, fb);
            if (solver) {
                scheduler->stats.fallback_count++;
                ret = smtsolver_solve(solver, graph, out_result);
                if (ret >= 0) {
                    scheduler->stats.total_solves++;
                    scheduler->stats.backend_solve_counts[fb]++;
                    return ret;
                }
            }
        }
    }

    /* 所有后端均失败 */
    lv00_set_error_ctx(LV00_ERROR_SOLVER_NO_SOLUTION, __FILE__, __LINE__, __func__,
                       "所有可用后端均求解失败 (尝试 %d 个)", attempt_count);
    return -1;
}

int scheduler_solve_with_backend(EngineScheduler *scheduler, const ConstraintGraph *graph,
                                  SolverBackendType backend_type, SMTSolverResult *out_result) {
    LV00_CHECK_NULL(scheduler, -1);
    LV00_CHECK_NULL(graph, -1);
    LV00_CHECK_NULL(out_result, -1);

    /* 尝试指定后端 */
    SMTSolver *solver = scheduler_get_or_create_backend(scheduler, backend_type);
    if (solver) {
        int ret = smtsolver_solve(solver, graph, out_result);
        if (ret >= 0) {
            scheduler->stats.total_solves++;
            scheduler->stats.backend_solve_counts[backend_type]++;
            return ret;
        }
    }

    /* 回退链 */
    if (scheduler->enable_fallback) {
        for (int i = 0; i < scheduler->fallback_depth; i++) {
            SolverBackendType fb = scheduler->fallback_chain[i];
            if (fb == backend_type) continue;

            SMTSolver *fb_solver = scheduler_get_or_create_backend(scheduler, fb);
            if (fb_solver) {
                scheduler->stats.fallback_count++;
                int ret = smtsolver_solve(fb_solver, graph, out_result);
                if (ret >= 0) {
                    scheduler->stats.total_solves++;
                    scheduler->stats.backend_solve_counts[fb]++;
                    return ret;
                }
            }
        }
    }

    return -1;
}

/* ============================================================
 * 结果转换（向后兼容）
 * ============================================================ */

GroebnerResult *scheduler_convert_smt_to_groebner(const SMTSolverResult *smt_result,
                                                    const ConstraintGraph *graph) {
    LV00_CHECK_NULL(smt_result, NULL);
    LV00_CHECK_NULL(graph, NULL);

    GroebnerResult *gb = lv00_malloc(sizeof(GroebnerResult));
    if (!gb) return NULL;
    memset(gb, 0, sizeof(GroebnerResult));

    switch (smt_result->sat_result) {
    case SMT_RESULT_SAT:
        if (smt_result->assignment_count > 0) {
            /* 将 SMT 变量赋值转换为 SymbolicCoord 数组 */
            gb->solutions = lv00_malloc(sizeof(SymbolicCoord *) *
                                         smt_result->assignment_count);
            if (!gb->solutions) {
                lv00_free((void **)&gb);
                return NULL;
            }

            gb->solution_count = 0;
            for (int i = 0; i < smt_result->assignment_count; i++) {
                const SMTVariableAssignment *assign = &smt_result->assignments[i];
                if (assign->is_boolean) continue; /* 跳过布尔赋值 */

                SymbolicCoord *coord = NULL;
                if (assign->value.rational.denominator > 0) {
                    /* 精确有理数 */
                    coord = symbolic_coord_create_rational(
                        (int)assign->value.rational.numerator,
                        (int)assign->value.rational.denominator);
                } else {
                    /* 近似值 → 创建为超越常数标记 */
                    coord = symbolic_coord_create_transcendental(
                        assign->var_name);
                }
                if (coord) {
                    gb->solutions[gb->solution_count++] = coord;
                }
            }
            gb->unique = (gb->solution_count == 1);
        }
        break;

    case SMT_RESULT_UNSAT:
        gb->overdetermined = true;
        gb->unique = false;
        break;

    case SMT_RESULT_UNKNOWN:
    case SMT_RESULT_ERROR:
    default:
        lv00_free((void **)&gb);
        return NULL;
    }

    return gb;
}

int scheduler_solve_groebner_compat(EngineScheduler *scheduler,
                                     const ConstraintGraph *graph,
                                     GroebnerResult **out_result) {
    LV00_CHECK_NULL(scheduler, -1);
    LV00_CHECK_NULL(graph, -1);
    LV00_CHECK_NULL(out_result, -1);

    SMTSolverResult smt_result;
    smtsolver_result_init(&smt_result);

    int status = scheduler_solve(scheduler, graph, &smt_result);
    if (status < 0) {
        smtsolver_result_free(&smt_result);
        return status;
    }

    *out_result = scheduler_convert_smt_to_groebner(&smt_result, graph);
    smtsolver_result_free(&smt_result);

    if (!*out_result) {
        return -1;
    }

    /* 将 SMT 状态映射为 SolverStatus */
    switch (smt_result.sat_result) {
    case SMT_RESULT_SAT:
        return (*out_result)->unique ? SOLVER_UNIQUE : SOLVER_MULTIPLE;
    case SMT_RESULT_UNSAT:
        return SOLVER_NO_SOLUTION;
    default:
        return SOLVER_TIMEOUT;
    }
}

/* ============================================================
 * 调度器配置
 * ============================================================ */

void scheduler_set_default_backend(EngineScheduler *scheduler, SolverBackendType type) {
    if (!scheduler || type >= COUNT) return;
    scheduler->default_backend = type;
}

void scheduler_set_fallback_policy(EngineScheduler *scheduler, bool enable,
                                    const SolverBackendType *fallback_types, int depth) {
    if (!scheduler) return;
    scheduler->enable_fallback = enable;
    if (fallback_types && depth > 0) {
        int copy_depth = (depth > SCHEDULER_MAX_FALLBACK_DEPTH)
                             ? SCHEDULER_MAX_FALLBACK_DEPTH
                             : depth;
        memcpy(scheduler->fallback_chain, fallback_types,
               copy_depth * sizeof(SolverBackendType));
        scheduler->fallback_depth = copy_depth;
    }
}

void scheduler_set_auto_create(EngineScheduler *scheduler, bool auto_create) {
    if (!scheduler) return;
    scheduler->auto_create_backends = auto_create;
}

/* ============================================================
 * 统计与诊断
 * ============================================================ */

void scheduler_get_stats(const EngineScheduler *scheduler, SchedulerStats *stats) {
    if (!scheduler || !stats) return;
    memcpy(stats, &scheduler->stats, sizeof(SchedulerStats));
}

void scheduler_reset_stats(EngineScheduler *scheduler) {
    if (!scheduler) return;
    memset(&scheduler->stats, 0, sizeof(SchedulerStats));
}

int scheduler_diagnose(const EngineScheduler *scheduler, char *buf, size_t buf_size) {
    if (!scheduler || !buf || buf_size == 0) return 0;

    int written = 0;
    int ret;

    /* 标题 */
    ret = snprintf(buf + written, (buf_size > (size_t)written) ? buf_size - written : 0,
                   "=== Lv-00 引擎调度器诊断报告 ===\n\n");
    if (ret > 0) written += ret;
    if ((size_t)written >= buf_size) return written;

    /* 已注册后端 */
    ret = snprintf(buf + written, buf_size - written,
                   "--- 已注册后端 ---\n");
    if (ret > 0) written += ret;

    SMTBackendRegistry *reg = scheduler->registry;
    if (reg) {
        for (int i = 0; i < reg->count; i++) {
            ret = snprintf(buf + written, buf_size - written,
                           "  [%d] %s (%s) priority=%d\n",
                           i,
                           smtsolver_backend_type_name(reg->entries[i].type),
                           reg->entries[i].available ? "可用" : "不可用",
                           reg->entries[i].priority);
            if (ret > 0) written += ret;
        }
    } else {
        ret = snprintf(buf + written, buf_size - written,
                       "  (注册表未初始化)\n");
        if (ret > 0) written += ret;
    }

    /* 路由规则表 */
    ret = snprintf(buf + written, buf_size - written,
                   "\n--- 路由规则表 (%d 条) ---\n",
                   scheduler->routing_rule_count);
    if (ret > 0) written += ret;

    for (int i = 0; i < scheduler->routing_rule_count; i++) {
        RoutingRule *rule = scheduler->routing_rules[i];
        if (!rule) continue;
        ret = snprintf(buf + written, buf_size - written,
                       "  [%d] %-25s priority=%-4d enabled=%s target=%s\n",
                       i, rule->name, rule->priority,
                       rule->enabled ? "是" : "否",
                       smtsolver_backend_type_name(rule->target_backend));
        if (ret > 0) written += ret;
    }

    /* 回退策略 */
    ret = snprintf(buf + written, buf_size - written,
                   "\n--- 回退策略 ---\n"
                   "  启用: %s\n"
                   "  回退链深度: %d\n",
                   scheduler->enable_fallback ? "是" : "否",
                   scheduler->fallback_depth);
    if (ret > 0) written += ret;

    for (int i = 0; i < scheduler->fallback_depth; i++) {
        ret = snprintf(buf + written, buf_size - written,
                       "    [%d] %s\n", i,
                       smtsolver_backend_type_name(scheduler->fallback_chain[i]));
        if (ret > 0) written += ret;
    }

    /* 统计摘要 */
    ret = snprintf(buf + written, buf_size - written,
                   "\n--- 统计摘要 ---\n"
                   "  总求解次数: %lld\n"
                   "  回退次数:   %lld\n"
                   "  缓存后端数: %d\n"
                   "  自动创建:   %s\n"
                   "  默认后端:   %s\n",
                   (long long)scheduler->stats.total_solves,
                   (long long)scheduler->stats.fallback_count,
                   scheduler->backend_cache_count,
                   scheduler->auto_create_backends ? "是" : "否",
                   smtsolver_backend_type_name(scheduler->default_backend));
    if (ret > 0) written += ret;

    ret = snprintf(buf + written, buf_size - written,
                   "\n--- 各后端求解次数分布 ---\n");
    if (ret > 0) written += ret;

    for (int i = 0; i < COUNT; i++) {
        if (scheduler->stats.backend_solve_counts[i] > 0) {
            ret = snprintf(buf + written, buf_size - written,
                           "  %-12s: %lld\n",
                           smtsolver_backend_type_name((SolverBackendType)i),
                           (long long)scheduler->stats.backend_solve_counts[i]);
            if (ret > 0) written += ret;
        }
    }

    return written;
}
