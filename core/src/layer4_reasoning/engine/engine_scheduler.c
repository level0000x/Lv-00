/**
 * @file engine_scheduler.c
 * @brief 引擎调度器 —— 多后端求解引擎的动态路由与分发
 *
 * 实现约束图特征分析、自动后端选择、路由规则匹配、回退链机制
 * 以及 Groebner/SMT 求解分发。参考 polymake 的多后端架构。
 *
 * @version 3.0.0
 */

#include "engine_scheduler.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/engine.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/solver.h"
#include "lv/symbolic_coord.h"

/* ============================================================
 * EngineScheduler 内部结构（不透明）
 * ============================================================ */
struct EngineScheduler {
    SchedulerBackendEntry backends[SCHEDULER_MAX_BACKEND_INSTANCES];
    int backend_count;

    RoutingRule routing_rules[SCHEDULER_MAX_ROUTING_RULES];
    int routing_rule_count;

    SolverBackendType default_backend;
    SolverBackendType fallback_chain[SCHEDULER_MAX_FALLBACK_DEPTH];
    int fallback_depth;
    bool enable_fallback;
    bool auto_create;

    SchedulerStats stats;
};

/* ============================================================
 * 向后兼容 —— 全局静态引擎指针与默认调度器
 * ============================================================ */
static lvEngine *g_compat_engine = NULL;
static EngineScheduler *g_default_scheduler = NULL;

/* ============================================================
 * 内部辅助：路由规则按优先级排序的比较函数
 * ============================================================ */
static int rule_compare(const void *a, const void *b) {
    const RoutingRule *ra = (const RoutingRule *) a;
    const RoutingRule *rb = (const RoutingRule *) b;
    return ra->priority - rb->priority;
}

/* ============================================================
 * 内部辅助：检查路由条件是否满足
 * ============================================================ */
static bool check_condition(const RouteCondition *cond, const GraphFeatures *features,
                            const EngineScheduler *scheduler) {
    if (!cond)
        return false;

    switch (cond->type) {
        case ROUTE_COND_NONE:
            return true;

        case ROUTE_COND_VAR_COUNT_LE:
            return features->variable_nodes <= cond->int_value;

        case ROUTE_COND_VAR_COUNT_GE:
            return features->variable_nodes >= cond->int_value;

        case ROUTE_COND_NONLINEAR_RATIO_GE:
            return features->nonlinear_ratio >= cond->float_value;

        case ROUTE_COND_HAS_QUANTIFIER:
            return features->has_quantifier_like;

        case ROUTE_COND_HAS_BOOLEAN:
            return features->has_boolean_variables;

        case ROUTE_COND_DEGREE_GE:
            return features->estimated_degree_max >= cond->int_value;

        case ROUTE_COND_BACKEND_AVAILABLE: {
            SolverBackendType bt = (SolverBackendType) cond->int_value;
            return scheduler_is_backend_available(scheduler, bt);
        }

        default:
            return false;
    }
}

/* ============================================================
 * 内部辅助：检查路由规则是否匹配给定图特征
 * ============================================================ */
static bool rule_matches(const RoutingRule *rule, const GraphFeatures *features, const EngineScheduler *scheduler) {
    if (!rule || !rule->enabled || rule->condition_count <= 0) {
        return false;
    }

    if (rule->combine_mode == ROUTE_COMBINE_AND) {
        for (int i = 0; i < rule->condition_count; i++) {
            if (!check_condition(&rule->conditions[i], features, scheduler)) {
                return false;
            }
        }
        return true;
    } else {
        /* ROUTE_COMBINE_OR */
        for (int i = 0; i < rule->condition_count; i++) {
            if (check_condition(&rule->conditions[i], features, scheduler)) {
                return true;
            }
        }
        return false;
    }
}

/* ============================================================
 * 内部辅助：将 SolverStatus 映射为 SMTSatResult
 * ============================================================ */
static SMTSatResult solver_status_to_sat(SolverStatus status) {
    switch (status) {
        case SOLVER_STATUS_OK:
        case SOLVER_STATUS_UNIQUE:
        case SOLVER_STATUS_MULTIPLE:
            return SMT_RESULT_SAT;

        case SOLVER_STATUS_NO_SOLUTION:
            return SMT_RESULT_UNSAT;

        case SOLVER_STATUS_OVERCONSTRAINED:
            return SMT_RESULT_UNSAT;

        case SOLVER_STATUS_TIMEOUT:
            return SMT_RESULT_UNKNOWN;

        case SOLVER_STATUS_OUT_OF_SCOPE:
            return SMT_RESULT_UNKNOWN;

        case SOLVER_STATUS_OUT_OF_MEMORY:
        default:
            return SMT_RESULT_ERROR;
    }
}

/* ============================================================
 * 内部辅助：从 GroebnerResult 中提取 POINT 节点坐标到 SMTSolverResult
 *
 * 遍历约束图中的 POINT 节点，创建变量赋值数组。
 * ============================================================ */
static int extract_assignments_from_graph(const ConstraintGraph *graph, SMTVariableAssignment **out_assignments,
                                          int *out_count) {
    if (!graph || !out_assignments || !out_count)
        return -1;

    /* 统计 POINT 节点数 */
    int point_count = 0;
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (node && node->type == GEOM_POINT) {
            point_count++;
        }
    }

    if (point_count <= 0) {
        *out_assignments = NULL;
        *out_count = 0;
        return 0;
    }

    /* 每个点有两个坐标 (x, y)，所以最多 point_count * 2 个赋值 */
    if (point_count > INT_MAX / 2)
        return -1;
    int max_assignments = point_count * 2;
    SMTVariableAssignment *assignments =
        (SMTVariableAssignment *) lv_calloc((size_t) max_assignments, sizeof(SMTVariableAssignment));
    if (!assignments) {
        *out_assignments = NULL;
        *out_count = 0;
        return -1;
    }

    int idx = 0;
    for (int i = 0; i < graph->node_count && idx < max_assignments; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_POINT)
            continue;

        for (int ci = 0; ci < node->coord_count && idx < max_assignments; ci++) {
            SymbolicCoord *sc = node->symbolic_coords[ci];
            if (!sc)
                continue;

            assignments[idx].var_node_id = node->id;
            snprintf(assignments[idx].var_name, SMT_VAR_NAME_MAX_LEN, "p%d_%s", node->id, (ci == 0) ? "x" : "y");
            assignments[idx].is_boolean = false;
            assignments[idx].value.rational.numerator = 0;
            assignments[idx].value.rational.denominator = 1;
            assignments[idx].value.rational.is_approx = true;
            assignments[idx].value.rational.approx_value = sc->cache_valid ? sc->cached_value : 0.0;

            /* 从 Rational 中提取精确值（如可用） */
            if (sc->type == RATIONAL && sc->data.rational) {
                /* 用 rational_to_double 获取近似值已在上方设置 */
                /* 如果有必要可解析 mpq_t，但 long long 可能溢出 */
            }

            idx++;
        }
    }

    *out_assignments = assignments;
    *out_count = idx;
    return 0;
}

/* ============================================================
 * 生命周期
 * ============================================================ */
EngineScheduler *scheduler_create(void) {
    EngineScheduler *sched = (EngineScheduler *) lv_calloc(1, sizeof(EngineScheduler));
    if (!sched)
        return NULL;

    /* 默认后端：GROEBNER */
    sched->default_backend = GROEBNER;

    /* 默认回退链 */
    sched->fallback_chain[0] = GROEBNER;
    sched->fallback_depth = 1;
    sched->enable_fallback = true;
    sched->auto_create = true;

    /* 注册 GROEBNER 后端（始终可用） */
    SchedulerBackendEntry *entry = &sched->backends[sched->backend_count];
    entry->type = GROEBNER;
    entry->available = true;
    entry->priority = 100;
    lv_strncpy(entry->description, "Built-in Groebner basis solver", sizeof(entry->description));
    entry->detect_func = NULL;
    sched->backend_count++;

    /* 加载预设路由规则 */
    scheduler_load_preset_rules(sched);

    return sched;
}

void scheduler_destroy(EngineScheduler *scheduler) {
    lv_free((void **) &scheduler);
}

void scheduler_reset(EngineScheduler *scheduler) {
    if (!scheduler)
        return;

    memset(scheduler->backends, 0, sizeof(scheduler->backends));
    scheduler->backend_count = 0;
    memset(scheduler->routing_rules, 0, sizeof(scheduler->routing_rules));
    scheduler->routing_rule_count = 0;
    memset(&scheduler->stats, 0, sizeof(scheduler->stats));

    scheduler->default_backend = GROEBNER;
    scheduler->fallback_chain[0] = GROEBNER;
    scheduler->fallback_depth = 1;
    scheduler->enable_fallback = true;
    scheduler->auto_create = true;

    /* 重新注册 GROEBNER */
    SchedulerBackendEntry *entry = &scheduler->backends[scheduler->backend_count];
    entry->type = GROEBNER;
    entry->available = true;
    entry->priority = 100;
    lv_strncpy(entry->description, "Built-in Groebner basis solver", sizeof(entry->description));
    entry->detect_func = NULL;
    scheduler->backend_count++;

    scheduler_load_preset_rules(scheduler);
}

/* ============================================================
 * 后端注册
 * ============================================================ */
int scheduler_register_backend(EngineScheduler *scheduler, SolverBackendType type, int priority,
                               const char *description, SolverBackendDetectFunc detect_func) {
    if (!scheduler)
        return -1;

    /* 检查是否已注册 */
    for (int i = 0; i < scheduler->backend_count; i++) {
        if (scheduler->backends[i].type == type) {
            /* 更新现有条目 */
            scheduler->backends[i].priority = priority;
            scheduler->backends[i].detect_func = detect_func;
            if (description) {
                lv_strncpy(scheduler->backends[i].description, description, sizeof(scheduler->backends[i].description));
            }
            if (detect_func) {
                scheduler->backends[i].available = true;
            }
            return 0;
        }
    }

    /* 检查容量 */
    if (scheduler->backend_count >= SCHEDULER_MAX_BACKEND_INSTANCES) {
        return -1;
    }

    SchedulerBackendEntry *entry = &scheduler->backends[scheduler->backend_count];
    entry->type = type;
    entry->priority = priority;
    entry->detect_func = detect_func;
    if (description) {
        lv_strncpy(entry->description, description, sizeof(entry->description));
    } else {
        entry->description[0] = '\0';
    }
    entry->available = (detect_func != NULL) || (type == GROEBNER); /* GROEBNER 始终可用 */
    scheduler->backend_count++;

    return 0;
}

int scheduler_unregister_backend(EngineScheduler *scheduler, SolverBackendType type) {
    if (!scheduler)
        return -1;

    for (int i = 0; i < scheduler->backend_count; i++) {
        if (scheduler->backends[i].type == type) {
            /* 将后续条目前移 */
            for (int j = i; j < scheduler->backend_count - 1; j++) {
                scheduler->backends[j] = scheduler->backends[j + 1];
            }
            scheduler->backend_count--;
            memset(&scheduler->backends[scheduler->backend_count], 0, sizeof(SchedulerBackendEntry));
            return 0;
        }
    }
    return -1; /* 未找到 */
}

int scheduler_list_available_backends(const EngineScheduler *scheduler, SolverBackendType *out_types, int max_count) {
    if (!scheduler || !out_types || max_count <= 0)
        return 0;

    int count = 0;
    for (int i = 0; i < scheduler->backend_count && count < max_count; i++) {
        if (scheduler->backends[i].available) {
            out_types[count++] = scheduler->backends[i].type;
        }
    }
    return count;
}

/* ============================================================
 * 后端选择与可用性
 * ============================================================ */
bool scheduler_is_backend_available(const EngineScheduler *scheduler, SolverBackendType type) {
    if (!scheduler)
        return false;

    for (int i = 0; i < scheduler->backend_count; i++) {
        if (scheduler->backends[i].type == type) {
            return scheduler->backends[i].available;
        }
    }
    return (type == GROEBNER); /* GROEBNER 始终视为可用 */
}

void scheduler_set_backend_available(EngineScheduler *scheduler, SolverBackendType type, bool available) {
    if (!scheduler)
        return;

    for (int i = 0; i < scheduler->backend_count; i++) {
        if (scheduler->backends[i].type == type) {
            scheduler->backends[i].available = available;
            return;
        }
    }

    /* 未找到则自动注册 */
    if (scheduler->auto_create) {
        scheduler_register_backend(scheduler, type, 50, NULL, NULL);
        scheduler_set_backend_available(scheduler, type, available);
    }
}

/* ============================================================
 * 路由规则管理
 * ============================================================ */
int scheduler_add_routing_rule(EngineScheduler *scheduler, RoutingRule *rule) {
    if (!scheduler || !rule)
        return -1;
    if (scheduler->routing_rule_count >= SCHEDULER_MAX_ROUTING_RULES)
        return -1;

    /* 检查名称是否已存在 */
    for (int i = 0; i < scheduler->routing_rule_count; i++) {
        if (strcmp(scheduler->routing_rules[i].name, rule->name) == 0) {
            /* 更新现有规则 */
            scheduler->routing_rules[i] = *rule;
            return 0;
        }
    }

    scheduler->routing_rules[scheduler->routing_rule_count] = *rule;
    scheduler->routing_rule_count++;

    return 0;
}

int scheduler_remove_routing_rule(EngineScheduler *scheduler, const char *name) {
    if (!scheduler || !name)
        return -1;

    for (int i = 0; i < scheduler->routing_rule_count; i++) {
        if (strcmp(scheduler->routing_rules[i].name, name) == 0) {
            for (int j = i; j < scheduler->routing_rule_count - 1; j++) {
                scheduler->routing_rules[j] = scheduler->routing_rules[j + 1];
            }
            scheduler->routing_rule_count--;
            memset(&scheduler->routing_rules[scheduler->routing_rule_count], 0, sizeof(RoutingRule));
            return 0;
        }
    }
    return -1;
}

int scheduler_load_preset_rules(EngineScheduler *scheduler) {
    if (!scheduler)
        return -1;

    /* 清空现有规则 */
    scheduler->routing_rule_count = 0;
    memset(scheduler->routing_rules, 0, sizeof(scheduler->routing_rules));

    /* 规则 1: small-groebner — 变量数 < 50 时使用 Groebner */
    RoutingRule *r1 = &scheduler->routing_rules[scheduler->routing_rule_count];
    lv_strncpy(r1->name, "small-groebner", sizeof(r1->name));
    r1->priority = 20;
    r1->enabled = true;
    r1->conditions[0].type = ROUTE_COND_VAR_COUNT_LE;
    r1->conditions[0].int_value = 50;
    r1->conditions[0].float_value = 0.0;
    r1->condition_count = 1;
    r1->combine_mode = ROUTE_COMBINE_AND;
    r1->target_backend = GROEBNER;
    scheduler->routing_rule_count++;

    /* 规则 2: default-groebner — 无条件回退 */
    RoutingRule *r2 = &scheduler->routing_rules[scheduler->routing_rule_count];
    lv_strncpy(r2->name, "default-groebner", sizeof(r2->name));
    r2->priority = 100;
    r2->enabled = true;
    r2->conditions[0].type = ROUTE_COND_NONE;
    r2->conditions[0].int_value = 0;
    r2->conditions[0].float_value = 0.0;
    r2->condition_count = 1;
    r2->combine_mode = ROUTE_COMBINE_AND;
    r2->target_backend = GROEBNER;
    scheduler->routing_rule_count++;

    return 0;
}

/* ============================================================
 * 图特征分析
 * ============================================================ */
int scheduler_analyze_graph(const ConstraintGraph *graph, GraphFeatures *features) {
    if (!graph || !features)
        return -1;

    memset(features, 0, sizeof(GraphFeatures));

    uint64_t start_us = lv_get_time_us();

    features->total_nodes = graph->node_count;
    features->total_constraints = graph->constraint_count;

    /* 遍历节点分类 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node)
            continue;

        switch (node->type) {
            case GEOM_POINT:
                features->variable_nodes++;
                if (node->coord_count > 0) {
                    features->fixed_nodes++;
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

    /* 遍历约束分类 */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (!c)
            continue;

        switch (c->type) {
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
            default:
                break;
        }
    }

    /* 非线性特征：保守估计 */
    features->nonlinear_constraints = 0;
    features->nonlinear_ratio = 0.0;

    /* 量词与布尔特征 */
    features->has_quantifier_like = false;
    features->has_boolean_variables = false;

    /* 方程数量估计 = incidence + intersection + containment */
    features->estimated_equation_count =
        features->incidence_constraints + features->intersection_constraints + features->containment_constraints;

    /* 最高度数估计：核心求解器处理二次 */
    features->estimated_degree_max = 2;

    features->analysis_time_us = (int64_t) (lv_get_time_us() - start_us);

    return 0;
}

const char *scheduler_feature_summary(const GraphFeatures *features) {
    if (!features)
        return "NULL features";

    static char summary[512];
    snprintf(summary, sizeof(summary),
             "nodes=%d pts=%d fixed=%d ports=%d blocks=%d "
             "constraints=%d inc=%d btw=%d int=%d cnt=%d conn=%d "
             "nl=%d nl_ratio=%.2f eq_est=%d deg_max=%d",
             features->total_nodes, features->variable_nodes, features->fixed_nodes, features->port_nodes,
             features->block_nodes, features->total_constraints, features->incidence_constraints,
             features->betweenness_constraints, features->intersection_constraints, features->containment_constraints,
             features->connection_constraints, features->nonlinear_constraints, features->nonlinear_ratio,
             features->estimated_equation_count, features->estimated_degree_max);
    return summary;
}

/* ============================================================
 * 后端选择
 * ============================================================ */
SolverBackendType scheduler_select_backend(const EngineScheduler *scheduler, const ConstraintGraph *graph,
                                           char *out_reason, size_t reason_size) {
    if (!scheduler)
        return GROEBNER;

    GraphFeatures features;
    if (graph) {
        scheduler_analyze_graph(graph, &features);
    } else {
        memset(&features, 0, sizeof(features));
    }

    /* 创建规则的排序副本 */
    RoutingRule sorted_rules[SCHEDULER_MAX_ROUTING_RULES];
    int sorted_count = 0;
    for (int i = 0; i < scheduler->routing_rule_count; i++) {
        if (scheduler->routing_rules[i].enabled) {
            sorted_rules[sorted_count++] = scheduler->routing_rules[i];
        }
    }
    qsort(sorted_rules, (size_t) sorted_count, sizeof(RoutingRule), rule_compare);

    /* 按优先级依次检查 */
    for (int i = 0; i < sorted_count; i++) {
        if (rule_matches(&sorted_rules[i], &features, scheduler)) {
            if (out_reason && reason_size > 0) {
                snprintf(out_reason, reason_size, "Rule '%s' matched (priority=%d), backend=%d", sorted_rules[i].name,
                         sorted_rules[i].priority, (int) sorted_rules[i].target_backend);
            }
            return sorted_rules[i].target_backend;
        }
    }

    /* 无匹配规则，使用默认后端 */
    if (out_reason && reason_size > 0) {
        snprintf(out_reason, reason_size, "No matching rule, using default backend=%d",
                 (int) scheduler->default_backend);
    }
    return scheduler->default_backend;
}

/* ============================================================
 * 分发求解
 * ============================================================ */
int scheduler_solve_with_backend(EngineScheduler *scheduler, const ConstraintGraph *graph,
                                 SolverBackendType backend_type, SMTSolverResult *out_result) {
    if (!scheduler || !graph || !out_result)
        return -1;

    smtsolver_result_init(out_result);
    out_result->backend_used = backend_type;

    uint64_t start_us = lv_get_time_us();

    switch (backend_type) {
        case GROEBNER: {
            /* Groebner 后端 */
            GroebnerResult *groebner_result = NULL;

            /* 将 const 转换，因为 solve_algebraic_system 接受非 const 图 */
            ConstraintGraph *nc_graph = (ConstraintGraph *) (uintptr_t) graph;

            SolverStatus status = solve_algebraic_system(nc_graph, NULL, 0, &groebner_result);

            out_result->sat_result = solver_status_to_sat(status);

            /* 从图中提取赋值 */
            if (out_result->sat_result == SMT_RESULT_SAT) {
                extract_assignments_from_graph(graph, &out_result->assignments, &out_result->assignment_count);

                out_result->error_code = SMT_ERROR_NONE;
                out_result->error_message[0] = '\0';
            }

            /* 设置错误信息 */
            if (status == SOLVER_STATUS_TIMEOUT) {
                out_result->error_code = SMT_ERROR_TIMEOUT_REACHED;
                snprintf(out_result->error_message, sizeof(out_result->error_message), "Groebner solver timed out");
            } else if (status == SOLVER_STATUS_OUT_OF_MEMORY) {
                out_result->error_code = SMT_ERROR_MEMORY_EXHAUSTED;
                snprintf(out_result->error_message, sizeof(out_result->error_message), "Groebner solver out of memory");
            } else if (status == SOLVER_STATUS_NO_SOLUTION) {
                out_result->error_code = SMT_ERROR_NONE;
                snprintf(out_result->error_message, sizeof(out_result->error_message),
                         "Constraint system has no solution");
            }

            if (groebner_result) {
                groebner_result_destroy(groebner_result);
            }

            /* 更新统计 */
            uint64_t elapsed_us = lv_get_time_us() - start_us;
            out_result->solve_time_ms = (double) elapsed_us / 1000.0;
            break;
        }

        case SMT_Z3:
        case SMT_CVC5:
        case SMT_SINGULAR:
            out_result->sat_result = SMT_RESULT_ERROR;
            out_result->error_code = SMT_ERROR_BACKEND_UNAVAILABLE;
            snprintf(out_result->error_message, sizeof(out_result->error_message),
                     "Backend '%s' is not available (not linked)", smtsolver_backend_type_name(backend_type));
            break;

        default:
            out_result->sat_result = SMT_RESULT_ERROR;
            out_result->error_code = SMT_ERROR_BACKEND_UNAVAILABLE;
            snprintf(out_result->error_message, sizeof(out_result->error_message), "Unknown backend type %d",
                     (int) backend_type);
            break;
    }

    /* 更新调度器统计 */
    scheduler->stats.total_solves++;
    uint64_t elapsed_us = lv_get_time_us() - start_us;
    scheduler->stats.total_solve_time_us += (int64_t) elapsed_us;
    if ((int64_t) elapsed_us > scheduler->stats.max_solve_time_us) {
        scheduler->stats.max_solve_time_us = (int64_t) elapsed_us;
    }
    if (backend_type >= 0 && backend_type < SCHEDULER_MAX_BACKEND_TYPES) {
        scheduler->stats.backend_solve_counts[(int) backend_type]++;
    }

    return 0;
}

int scheduler_solve(EngineScheduler *scheduler, const ConstraintGraph *graph, SMTSolverResult *out_result) {
    if (!scheduler || !graph || !out_result)
        return -1;

    /* 自动选择后端 */
    char reason[128];
    SolverBackendType backend = scheduler_select_backend(scheduler, graph, reason, sizeof(reason));

    int rc = scheduler_solve_with_backend(scheduler, graph, backend, out_result);

    /* 如果失败且启用了回退，尝试回退链 */
    if (rc != 0 && scheduler->enable_fallback && out_result->sat_result == SMT_RESULT_ERROR) {
        for (int i = 0; i < scheduler->fallback_depth; i++) {
            SolverBackendType fb = scheduler->fallback_chain[i];
            if (fb == backend)
                continue; /* 跳过已经失败的后端 */

            SMTSolverResult fb_result;
            smtsolver_result_init(&fb_result);

            int fb_rc = scheduler_solve_with_backend(scheduler, graph, fb, &fb_result);

            if (fb_rc == 0 && fb_result.sat_result != SMT_RESULT_ERROR) {
                *out_result = fb_result;
                scheduler->stats.fallback_count++;
                return 0;
            }
            smtsolver_result_clear(&fb_result);
        }
    }

    return rc;
}

int scheduler_solve_groebner_compat(EngineScheduler *scheduler, const ConstraintGraph *graph,
                                    GroebnerResult **out_result) {
    if (!scheduler || !graph || !out_result)
        return -1;

    *out_result = NULL;

    ConstraintGraph *nc_graph = (ConstraintGraph *) (uintptr_t) graph;

    uint64_t start_us = lv_get_time_us();

    SolverStatus status = solve_algebraic_system(nc_graph, NULL, 0, out_result);

    uint64_t elapsed_us = lv_get_time_us() - start_us;

    /* 更新统计 */
    scheduler->stats.total_solves++;
    scheduler->stats.total_solve_time_us += (int64_t) elapsed_us;
    if ((int64_t) elapsed_us > scheduler->stats.max_solve_time_us) {
        scheduler->stats.max_solve_time_us = (int64_t) elapsed_us;
    }
    if (GROEBNER >= 0 && GROEBNER < SCHEDULER_MAX_BACKEND_TYPES) {
        scheduler->stats.backend_solve_counts[GROEBNER]++;
    }

    switch (status) {
        case SOLVER_STATUS_OK:
        case SOLVER_STATUS_UNIQUE:
        case SOLVER_STATUS_MULTIPLE:
            return 0; /* 成功 */

        case SOLVER_STATUS_NO_SOLUTION:
        case SOLVER_STATUS_OVERCONSTRAINED:
            return 1; /* 无解 */

        case SOLVER_STATUS_TIMEOUT:
            return 2; /* 超时 */

        case SOLVER_STATUS_OUT_OF_MEMORY:
            return -1; /* 内存不足 */

        case SOLVER_STATUS_OUT_OF_SCOPE:
        default:
            return -1; /* 其他错误 */
    }
}

/* ============================================================
 * 配置
 * ============================================================ */
void scheduler_set_default_backend(EngineScheduler *scheduler, SolverBackendType type) {
    if (!scheduler)
        return;
    scheduler->default_backend = type;
}

void scheduler_set_fallback_policy(EngineScheduler *scheduler, bool enable, const SolverBackendType *fallback_types,
                                   int depth) {
    if (!scheduler)
        return;

    scheduler->enable_fallback = enable;

    if (fallback_types && depth > 0) {
        int copy_depth = depth;
        if (copy_depth > SCHEDULER_MAX_FALLBACK_DEPTH) {
            copy_depth = SCHEDULER_MAX_FALLBACK_DEPTH;
        }
        for (int i = 0; i < copy_depth; i++) {
            scheduler->fallback_chain[i] = fallback_types[i];
        }
        scheduler->fallback_depth = copy_depth;
    }
}

void scheduler_set_auto_create(EngineScheduler *scheduler, bool auto_create) {
    if (!scheduler)
        return;
    scheduler->auto_create = auto_create;
}

/* ============================================================
 * 统计与诊断
 * ============================================================ */
void scheduler_get_stats(const EngineScheduler *scheduler, SchedulerStats *stats) {
    if (!scheduler || !stats)
        return;
    *stats = scheduler->stats;
}

void scheduler_reset_stats(EngineScheduler *scheduler) {
    if (!scheduler)
        return;
    memset(&scheduler->stats, 0, sizeof(SchedulerStats));
}

int scheduler_diagnose(const EngineScheduler *scheduler, char *buf, size_t buf_size) {
    if (!scheduler || !buf || buf_size == 0)
        return -1;

    int pos = 0;
    int rc;

    rc = snprintf(buf + pos, buf_size - (size_t) pos,
                  "Lv-00 EngineScheduler Diagnostic\n"
                  "================================\n"
                  "Default backend: %d (%s)\n"
                  "Fallback: %s, depth=%d\n"
                  "Auto-create: %s\n"
                  "Registered backends: %d\n",
                  (int) scheduler->default_backend, smtsolver_backend_type_name(scheduler->default_backend),
                  scheduler->enable_fallback ? "enabled" : "disabled", scheduler->fallback_depth,
                  scheduler->auto_create ? "yes" : "no", scheduler->backend_count);
    if (rc > 0)
        pos += rc;

    for (int i = 0; i < scheduler->backend_count; i++) {
        rc = snprintf(buf + pos, buf_size - (size_t) pos, "  [%d] type=%d (%s) available=%s priority=%d desc=%s\n", i,
                      (int) scheduler->backends[i].type, smtsolver_backend_type_name(scheduler->backends[i].type),
                      scheduler->backends[i].available ? "yes" : "no", scheduler->backends[i].priority,
                      scheduler->backends[i].description);
        if (rc > 0)
            pos += rc;
    }

    rc = snprintf(buf + pos, buf_size - (size_t) pos, "Routing rules: %d\n", scheduler->routing_rule_count);
    if (rc > 0)
        pos += rc;

    for (int i = 0; i < scheduler->routing_rule_count; i++) {
        rc = snprintf(buf + pos, buf_size - (size_t) pos,
                      "  [%d] '%s' priority=%d enabled=%s conditions=%d backend=%d\n", i,
                      scheduler->routing_rules[i].name, scheduler->routing_rules[i].priority,
                      scheduler->routing_rules[i].enabled ? "yes" : "no", scheduler->routing_rules[i].condition_count,
                      (int) scheduler->routing_rules[i].target_backend);
        if (rc > 0)
            pos += rc;
    }

    rc = snprintf(buf + pos, buf_size - (size_t) pos,
                  "Stats: solves=%lld time=%lldus max=%lldus fallback=%lld miss=%lld\n",
                  (long long) scheduler->stats.total_solves, (long long) scheduler->stats.total_solve_time_us,
                  (long long) scheduler->stats.max_solve_time_us, (long long) scheduler->stats.fallback_count,
                  (long long) scheduler->stats.selection_miss_count);
    if (rc > 0)
        pos += rc;

    return pos;
}

/* ============================================================
 * 结果转换：SMTSolverResult → GroebnerResult
 * ============================================================ */
GroebnerResult *scheduler_convert_smt_to_groebner(const SMTSolverResult *smt_result, const ConstraintGraph *graph) {
    if (!smt_result || !graph)
        return NULL;

    GroebnerResult *result = (GroebnerResult *) lv_calloc(1, sizeof(GroebnerResult));
    if (!result)
        return NULL;

    if (smt_result->sat_result != SMT_RESULT_SAT || smt_result->assignment_count <= 0) {
        /* 无解或无赋值 */
        result->solutions = NULL;
        result->solution_count = 0;
        result->unique = false;
        result->overdetermined = true;
        return result;
    }

    /* 为每个赋值创建一个 SymbolicCoord */
    result->solutions = (SymbolicCoord **) lv_calloc((size_t) smt_result->assignment_count, sizeof(SymbolicCoord *));
    if (!result->solutions) {
        lv_free((void **) &result);
        return NULL;
    }

    for (int i = 0; i < smt_result->assignment_count; i++) {
        const SMTVariableAssignment *assign = &smt_result->assignments[i];
        if (!assign)
            continue;

        /* 使用缓存的值创建有理数坐标 */
        Rational *rat =
            rational_create(assign->value.rational.numerator, (uint64_t) assign->value.rational.denominator);
        if (!rat)
            continue;

        SymbolicCoord *sc = (SymbolicCoord *) lv_calloc(1, sizeof(SymbolicCoord));
        if (!sc) {
            rational_destroy(rat);
            continue;
        }
        sc->type = RATIONAL;
        sc->data.rational = rat;
        sc->trust = TRUST_GREEN;
        sc->cache_valid = true;
        sc->cached_value = assign->value.rational.approx_value;

        result->solutions[i] = sc;
    }
    result->solution_count = smt_result->assignment_count;
    result->unique = true;
    result->overdetermined = false;

    return result;
}

/* ============================================================
 * 向后兼容 —— 旧版调度器 API
 * ============================================================ */
void lv_engine_scheduler_init(lvEngine *engine) {
    g_compat_engine = engine;

    if (!g_default_scheduler) {
        g_default_scheduler = scheduler_create();
    }
}

int lv_engine_schedule(const char *task_name, int priority) {
    if (!task_name)
        return -1;

    if (!g_default_scheduler) {
        g_default_scheduler = scheduler_create();
        if (!g_default_scheduler)
            return -1;
    }

    /* 旧版调度器是一个简单的优先级队列。
     * 在新的设计下，将其转换为路由规则的后端分发。
     * task_name 决定操作类型，priority 影响路由 */
    lv_UNUSED(priority);

    if (!g_compat_engine)
        return -1;
    if (!g_compat_engine->main_graph)
        return 0;

    SMTSolverResult result;
    smtsolver_result_init(&result);

    if (strcmp(task_name, "solve") == 0) {
        scheduler_solve(g_default_scheduler, g_compat_engine->main_graph, &result);
    } else if (strcmp(task_name, "normalize") == 0) {
        /* normalize 走正常的引擎路径 */
        return 0;
    } else if (strcmp(task_name, "unify") == 0) {
        return 0;
    } else if (strcmp(task_name, "rewrite") == 0) {
        scheduler_solve(g_default_scheduler, g_compat_engine->main_graph, &result);
    }

    smtsolver_result_clear(&result);
    return 0;
}

bool lv_engine_execute_pending(void) {
    if (!g_compat_engine || !g_default_scheduler)
        return false;
    if (!g_compat_engine->main_graph)
        return false;

    /* 用默认后端执行一次求解 */
    SMTSolverResult result;
    smtsolver_result_init(&result);

    scheduler_solve(g_default_scheduler, g_compat_engine->main_graph, &result);

    smtsolver_result_clear(&result);

    return true;
}

int lv_engine_pending_count(void) {
    return 0; /* 新版调度器无任务队列概念 */
}
