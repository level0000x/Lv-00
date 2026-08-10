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
#include "lv/lv_numeric.h"
#include "lv/lv_registry.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_utils.h"
#include "lv/normalization.h"
#include "lv/solver.h"
#include "lv/symbolic_coord.h"

#include "debug.h"

/* ============================================================
 * EngineScheduler 内部结构（不透明）
 * ============================================================ */
struct EngineScheduler {
    SchedulerBackendEntry backends[SCHEDULER_MAX_BACKEND_INSTANCES];
    int backend_count;

    /* 路由规则注册表（通用注册表设施：key = 规则名，value = RoutingRule* 堆拷贝）。
     * 承担 strcmp 查重、尾部追加、删除前移紧凑（保持注册顺序）与析构回调；
     * 遍历顺序 = 注册顺序（get_at 语义与原数组遍历一致）。 */
    lvRegistry routing_rule_registry;

    SolverBackendType default_backend;
    SolverBackendType fallback_chain[SCHEDULER_MAX_FALLBACK_DEPTH];
    int fallback_depth;
    bool enable_fallback;
    bool auto_create;

    SchedulerStats stats;
};

/* ============================================================
 * 向后兼容 —— 线程局部引擎指针
 *
 * 用于不支持引擎实例参数的旧版 API（lv_engine_schedule、
 * lv_engine_execute_pending）。新代码应通过 lvEngine 实例
 * 的 scheduler 字段直接访问调度器。
 *
 * TLS 确保每个线程有独立的引擎关联，为多引擎并发做准备。
 * ============================================================ */
static lv_THREAD_LOCAL lvEngine *g_tls_engine = NULL;

/* ============================================================
 * 内部辅助：路由规则按优先级排序的比较函数
 * ============================================================ */
static int rule_compare(const void *a, const void *b) {
    const RoutingRule *ra = (const RoutingRule *) a;
    const RoutingRule *rb = (const RoutingRule *) b;
    return ra->priority - rb->priority;
}

/** @brief RoutingRule 堆拷贝的注册表 destroy 回调适配器（void(*)(void*) 形态） */
static void routing_rule_destroy(void *value) {
    lv_free((void **) &value);
}

/* ============================================================
 * 路由条件函数指针类型
 * ============================================================ */
typedef bool (*RouteConditionHandler)(const RouteCondition *cond, const GraphFeatures *features,
                                      const EngineScheduler *scheduler);

/* ============================================================
 * 路由条件处理函数（每个 ROUTE_COND_* 一个）
 * ============================================================ */
static bool handle_cond_none(const RouteCondition *cond, const GraphFeatures *features,
                             const EngineScheduler *scheduler) {
    (void)cond; (void)features; (void)scheduler;
    return true;
}

static bool handle_cond_var_count_le(const RouteCondition *cond, const GraphFeatures *features,
                                     const EngineScheduler *scheduler) {
    (void)scheduler;
    return features->variable_nodes <= cond->int_value;
}

static bool handle_cond_var_count_ge(const RouteCondition *cond, const GraphFeatures *features,
                                     const EngineScheduler *scheduler) {
    (void)scheduler;
    return features->variable_nodes >= cond->int_value;
}

static bool handle_cond_nonlinear_ratio_ge(const RouteCondition *cond, const GraphFeatures *features,
                                           const EngineScheduler *scheduler) {
    (void)scheduler;
    return features->nonlinear_ratio >= cond->float_value;
}

static bool handle_cond_has_quantifier(const RouteCondition *cond, const GraphFeatures *features,
                                       const EngineScheduler *scheduler) {
    (void)cond; (void)scheduler;
    return features->has_quantifier_like;
}

static bool handle_cond_has_boolean(const RouteCondition *cond, const GraphFeatures *features,
                                    const EngineScheduler *scheduler) {
    (void)cond; (void)scheduler;
    return features->has_boolean_variables;
}

static bool handle_cond_degree_ge(const RouteCondition *cond, const GraphFeatures *features,
                                  const EngineScheduler *scheduler) {
    (void)scheduler;
    return features->estimated_degree_max >= cond->int_value;
}

static bool handle_cond_backend_available(const RouteCondition *cond, const GraphFeatures *features,
                                          const EngineScheduler *scheduler) {
    (void)features;
    SolverBackendType bt = (SolverBackendType) cond->int_value;
    return scheduler_is_backend_available(scheduler, bt);
}

/* ============================================================
 * 路由条件处理函数查找表
 * ============================================================ */
static const RouteConditionHandler kRouteConditionHandlers[] = {
    [ROUTE_COND_NONE]               = handle_cond_none,
    [ROUTE_COND_VAR_COUNT_LE]       = handle_cond_var_count_le,
    [ROUTE_COND_VAR_COUNT_GE]       = handle_cond_var_count_ge,
    [ROUTE_COND_NONLINEAR_RATIO_GE] = handle_cond_nonlinear_ratio_ge,
    [ROUTE_COND_HAS_QUANTIFIER]     = handle_cond_has_quantifier,
    [ROUTE_COND_HAS_BOOLEAN]        = handle_cond_has_boolean,
    [ROUTE_COND_DEGREE_GE]          = handle_cond_degree_ge,
    [ROUTE_COND_BACKEND_AVAILABLE]  = handle_cond_backend_available,
};

/* ============================================================
 * 内部辅助：检查路由条件是否满足（查找表分发）
 * ============================================================ */
static bool check_condition(const RouteCondition *cond, const GraphFeatures *features,
                            const EngineScheduler *scheduler) {
    if (!cond)
        return false;

    if (lv_index_in_range(cond->type, (int)(sizeof(kRouteConditionHandlers)/sizeof(kRouteConditionHandlers[0])))
        && kRouteConditionHandlers[cond->type]) {
        return kRouteConditionHandlers[cond->type](cond, features, scheduler);
    }
    return false;
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
 * 内部辅助：SolverStatus → SMTSatResult 查找表（代替 switch）
 * ============================================================ */
static const SMTSatResult kSolverStatusToSat[] = {
    [SOLVER_STATUS_OK]            = SMT_RESULT_SAT,
    [SOLVER_STATUS_UNIQUE]        = SMT_RESULT_SAT,
    [SOLVER_STATUS_MULTIPLE]      = SMT_RESULT_SAT,
    [SOLVER_STATUS_NO_SOLUTION]   = SMT_RESULT_UNSAT,
    [SOLVER_STATUS_OVERCONSTRAINED] = SMT_RESULT_UNSAT,
    [SOLVER_STATUS_TIMEOUT]       = SMT_RESULT_UNKNOWN,
    [SOLVER_STATUS_OUT_OF_SCOPE]  = SMT_RESULT_UNKNOWN,
    [SOLVER_STATUS_OUT_OF_MEMORY] = SMT_RESULT_ERROR,
};

static SMTSatResult solver_status_to_sat(SolverStatus status) {
    int idx = (int)status;
    if (lv_index_in_range(idx, (int)(sizeof(kSolverStatusToSat) / sizeof(kSolverStatusToSat[0])))) {
        return kSolverStatusToSat[idx];
    }
    return SMT_RESULT_ERROR;
}

/* ============================================================
 * SolverStatus → 返回码查找表（用于 scheduler_solve_groebner_compat）
 * ============================================================ */
static const int kSolverStatusToReturnCode[] = {
    [SOLVER_STATUS_OK]            = 0,
    [SOLVER_STATUS_UNIQUE]        = 0,
    [SOLVER_STATUS_MULTIPLE]      = 0,
    [SOLVER_STATUS_NO_SOLUTION]   = 1,
    [SOLVER_STATUS_OVERCONSTRAINED] = 1,
    [SOLVER_STATUS_TIMEOUT]       = 2,
    [SOLVER_STATUS_OUT_OF_MEMORY] = -1,
    [SOLVER_STATUS_OUT_OF_SCOPE]  = -1,
};

/* ============================================================
 * 调度器后端虚函数表（VTable）—— 消除 backend_type switch 分发
 * ============================================================ */
typedef struct SchedulerBackendVTable {
    SolverBackendType type;  /**< 后端类型标识 */
    const char       *name;  /**< 后端名称 */
    int (*solve)(EngineScheduler *scheduler, const ConstraintGraph *graph,
                 SMTSolverResult *out_result);  /**< 求解函数 */
    void *(*create)(void);    /**< 创建后端实例（保留扩展用） */
    void  (*destroy)(void *backend); /**< 销毁后端实例（保留扩展用） */
} SchedulerBackendVTable;

/* ============================================================
 * 内部辅助：从 GroebnerResult 中提取 POINT 节点坐标到 SMTSolverResult
 *
 * 遍历约束图中的 POINT 节点，创建变量赋值数组。
 * ============================================================ */
static int extract_assignments_from_graph(const ConstraintGraph *graph, SMTVariableAssignment **out_assignments,
                                          int *out_count) {
    if (!graph || !out_assignments || !out_count)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "extract_assignments_from_graph: NULL parameter");

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
        lv_RETURN_ERROR(lv_ERROR_OVERFLOW, "extract_assignments_from_graph: point_count overflow");
    int max_assignments = point_count * 2;
    SMTVariableAssignment *assignments =
        (SMTVariableAssignment *) lv_calloc((size_t) max_assignments, sizeof(SMTVariableAssignment));
    if (!assignments) {
        *out_assignments = NULL;
        *out_count = 0;
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "extract_assignments_from_graph: calloc assignments failed");
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
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "scheduler_create: calloc failed");

    /* 初始化路由规则注册表 */
    lv_registry_init(&sched->routing_rule_registry, 8);

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
    if (!scheduler)
        return;
    /* 释放路由规则注册表（destroy 回调释放各 RoutingRule 堆拷贝与内部 name） */
    lv_registry_destroy(&scheduler->routing_rule_registry);
    lv_free((void **) &scheduler);
}

void scheduler_reset(EngineScheduler *scheduler) {
    if (!scheduler)
        return;

    memset(scheduler->backends, 0, sizeof(scheduler->backends));
    scheduler->backend_count = 0;
    /* 清空路由规则（destroy 回调释放各 RoutingRule 堆拷贝，保留注册表结构可继续使用） */
    lv_registry_clear(&scheduler->routing_rule_registry);
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
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "scheduler_register_backend: scheduler is NULL");

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
        lv_RETURN_ERROR(lv_ERROR_RESOURCE_EXHAUSTED, "scheduler_register_backend: max backends reached");
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
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "scheduler_unregister_backend: scheduler is NULL");

    for (int i = 0; i < scheduler->backend_count; i++) {
        if (scheduler->backends[i].type == type) {
            /* 将后续条目前移（统一走 lv_shift_left 的 memmove 路径） */
            lv_shift_left(scheduler->backends, sizeof(scheduler->backends[0]), (size_t) i,
                          (size_t) scheduler->backend_count);
            scheduler->backend_count--;
            memset(&scheduler->backends[scheduler->backend_count], 0, sizeof(SchedulerBackendEntry));
            return 0;
        }
    }
    lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "scheduler_unregister_backend: backend type not found");
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
 * 预设路由规则描述符表（静态数组，替代逐条字段填充）
 * ============================================================ */
static const RoutingRule kPresetRoutingRules[] = {
    {
        /* 规则 1: small-groebner — 变量数 <= 50 时使用 Groebner */
        .name = "small-groebner",
        .priority = 20,
        .enabled = true,
        .conditions = {{ROUTE_COND_VAR_COUNT_LE, 50, 0.0}},
        .condition_count = 1,
        .combine_mode = ROUTE_COMBINE_AND,
        .target_backend = GROEBNER,
    },
    {
        /* 规则 2: default-groebner — 无条件回退 */
        .name = "default-groebner",
        .priority = 100,
        .enabled = true,
        .conditions = {{ROUTE_COND_NONE, 0, 0.0}},
        .condition_count = 1,
        .combine_mode = ROUTE_COMBINE_AND,
        .target_backend = GROEBNER,
    },
};

/* ============================================================
 * 路由规则管理
 * ============================================================ */
int scheduler_add_routing_rule(EngineScheduler *scheduler, RoutingRule *rule) {
    if (!scheduler || !rule)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "scheduler_add_routing_rule: NULL scheduler or rule");

    /* 与旧语义一致：容量上限优先于查重（达到上限时即使更新现有规则也返回资源耗尽） */
    if (lv_registry_count(&scheduler->routing_rule_registry) >= SCHEDULER_MAX_ROUTING_RULES)
        lv_RETURN_ERROR(lv_ERROR_RESOURCE_EXHAUSTED, "scheduler_add_routing_rule: max routing rules reached");

    /* 检查名称是否已存在（委托注册表 strcmp 查重） */
    RoutingRule *existing = (RoutingRule *) lv_registry_get(&scheduler->routing_rule_registry, rule->name);
    if (existing) {
        /* 更新现有规则（保持注册顺序与注册表 key 不变） */
        *existing = *rule;
        return 0;
    }

    /* 尾部追加：堆拷贝 RoutingRule 交由注册表管理（remove/destroy 时释放） */
    RoutingRule *copy = (RoutingRule *) lv_malloc(sizeof(RoutingRule));
    if (!copy)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "scheduler_add_routing_rule: malloc failed");
    *copy = *rule;
    if (!lv_registry_put_ex(&scheduler->routing_rule_registry, rule->name, copy, routing_rule_destroy)) {
        lv_free((void **) &copy);
        lv_RETURN_ERROR(lv_ERROR_RESOURCE_EXHAUSTED, "scheduler_add_routing_rule: registry insert failed");
    }

    return 0;
}

int scheduler_remove_routing_rule(EngineScheduler *scheduler, const char *name) {
    if (!scheduler || !name)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "scheduler_remove_routing_rule: NULL scheduler or name");

    /* 委托注册表删除：destroy 回调释放 RoutingRule 堆拷贝，后续条目前移紧凑（保持顺序） */
    if (lv_registry_remove(&scheduler->routing_rule_registry, name))
        return 0;
    lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "scheduler_remove_routing_rule: rule not found");
}

int scheduler_load_preset_rules(EngineScheduler *scheduler) {
    if (!scheduler)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "scheduler_load_preset_rules: scheduler is NULL");

    /* 清空现有规则（destroy 回调释放堆拷贝，保留注册表结构可继续使用） */
    lv_registry_clear(&scheduler->routing_rule_registry);

    /* 从静态描述符表批量拷贝预设规则（防止超出容量） */
    size_t count = sizeof(kPresetRoutingRules) / sizeof(kPresetRoutingRules[0]);
    if (count > SCHEDULER_MAX_ROUTING_RULES) {
        count = SCHEDULER_MAX_ROUTING_RULES;
    }
    for (size_t i = 0; i < count; i++) {
        RoutingRule *copy = (RoutingRule *) lv_malloc(sizeof(RoutingRule));
        if (!copy)
            lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "scheduler_load_preset_rules: malloc failed");
        *copy = kPresetRoutingRules[i];
        if (!lv_registry_put_ex(&scheduler->routing_rule_registry, copy->name, copy, routing_rule_destroy)) {
            lv_free((void **) &copy);
            lv_RETURN_ERROR(lv_ERROR_RESOURCE_EXHAUSTED, "scheduler_load_preset_rules: registry insert failed");
        }
    }

    return 0;
}

/* ============================================================
 * 图特征分析
 * ============================================================ */

/* 节点特征增量查找表 —— 替代 switch 分支 */
typedef struct {
    int variable_inc;
    int port_inc;
    int block_inc;
    bool check_fixed;
} NodeFeatureDelta;

static const NodeFeatureDelta kNodeFeatureDeltas[] = {
    [GEOM_POINT]           = {1, 0, 0, true},
    [GEOM_PORT]            = {0, 1, 0, false},
    [GEOM_FUNCTION_BLOCK]  = {0, 0, 1, false},
};

/* 约束特征增量查找表 —— 替代 switch 分支 */
typedef struct {
    int incidence_inc;
    int betweenness_inc;
    int intersection_inc;
    int containment_inc;
    int angle_inc;
    int connection_inc;
} ConstraintFeatureDelta;

static const ConstraintFeatureDelta kConstraintFeatureDeltas[] = {
    [INCIDENCE]     = {1, 0, 0, 0, 0, 0},
    [BETWEENNESS]   = {0, 1, 0, 0, 0, 0},
    [INTERSECTION]  = {0, 0, 1, 0, 0, 0},
    [CONTAINMENT]   = {0, 0, 0, 1, 0, 0},
    [ANGLE]         = {0, 0, 0, 0, 1, 0},
    [CONNECTION]    = {0, 0, 0, 0, 0, 1},
};

int scheduler_analyze_graph(const ConstraintGraph *graph, GraphFeatures *features) {
    if (!graph || !features)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "scheduler_analyze_graph: NULL graph or features");

    memset(features, 0, sizeof(GraphFeatures));

    uint64_t start_us = lv_get_time_us();

    features->total_nodes = graph->node_count;
    features->total_constraints = graph->constraint_count;

    /* 遍历节点分类 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node)
            continue;

        if (lv_index_in_range(node->type, (int)(sizeof(kNodeFeatureDeltas)/sizeof(kNodeFeatureDeltas[0])))) {
            const NodeFeatureDelta *d = &kNodeFeatureDeltas[node->type];
            features->variable_nodes += d->variable_inc;
            features->port_nodes += d->port_inc;
            features->block_nodes += d->block_inc;
            if (d->check_fixed && node->coord_count > 0) {
                features->fixed_nodes++;
            }
        }
    }

    /* 遍历约束分类 */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (!c)
            continue;

        if (lv_index_in_range(c->type, (int)(sizeof(kConstraintFeatureDeltas)/sizeof(kConstraintFeatureDeltas[0])))) {
            const ConstraintFeatureDelta *d = &kConstraintFeatureDeltas[c->type];
            features->incidence_constraints += d->incidence_inc;
            features->betweenness_constraints += d->betweenness_inc;
            features->intersection_constraints += d->intersection_inc;
            features->containment_constraints += d->containment_inc;
            features->angle_constraints += d->angle_inc;
            features->connection_constraints += d->connection_inc;
        }
    }

    /* 非线性特征：保守估计 */
    features->nonlinear_constraints = 0;
    features->nonlinear_ratio = 0.0;

    /* 量词与布尔特征 */
    features->has_quantifier_like = false;
    features->has_boolean_variables = false;

    /* 方程数量估计 = incidence + intersection + containment + angle */
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

    static lv_THREAD_LOCAL char summary[512];
    snprintf(summary, sizeof(summary),
             "nodes=%d pts=%d fixed=%d ports=%d blocks=%d "
             "constraints=%d inc=%d btw=%d int=%d cnt=%d ang=%d conn=%d "
             "nl=%d nl_ratio=%.2f eq_est=%d deg_max=%d",
             features->total_nodes, features->variable_nodes, features->fixed_nodes, features->port_nodes,
             features->block_nodes, features->total_constraints, features->incidence_constraints,
             features->betweenness_constraints, features->intersection_constraints, features->containment_constraints,
             features->angle_constraints, features->connection_constraints, features->nonlinear_constraints, features->nonlinear_ratio,
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

    /* 创建规则的排序副本（按注册顺序遍历注册表，与旧数组遍历语义一致） */
    RoutingRule sorted_rules[SCHEDULER_MAX_ROUTING_RULES];
    int sorted_count = 0;
    int rule_total = lv_registry_count(&scheduler->routing_rule_registry);
    for (int i = 0; i < rule_total; i++) {
        const char *rule_reg_name = NULL;
        void *rule_reg_value = NULL;
        if (!lv_registry_get_at(&scheduler->routing_rule_registry, i, &rule_reg_name, &rule_reg_value)) {
            continue;
        }
        RoutingRule *rule = (RoutingRule *) rule_reg_value;
        if (rule->enabled && sorted_count < SCHEDULER_MAX_ROUTING_RULES) {
            sorted_rules[sorted_count++] = *rule;
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
 * Per-backend solve 函数（VTable 分发用）
 * ============================================================ */

/** GROEBNER 后端求解 */
static int groebner_solve(EngineScheduler *scheduler, const ConstraintGraph *graph,
                          SMTSolverResult *out_result) {
    (void)scheduler;
    GroebnerResult *groebner_result = NULL;

    ConstraintGraph *nc_graph = (ConstraintGraph *) (uintptr_t) graph;

    uint64_t start_us = lv_get_time_us();
    SolverStatus status = solve_algebraic_system(nc_graph, NULL, 0, &groebner_result);
    uint64_t elapsed_us = lv_get_time_us() - start_us;

    out_result->sat_result = solver_status_to_sat(status);
    out_result->solve_time_ms = (double) elapsed_us / 1000.0;

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

    return 0;
}

/** 不可用后端求解（SMT_Z3 / SMT_CVC5 / SMT_SINGULAR） */
static int smt_unavailable_solve(EngineScheduler *scheduler, const ConstraintGraph *graph,
                                 SMTSolverResult *out_result) {
    (void)scheduler;
    (void)graph;
    out_result->sat_result = SMT_RESULT_ERROR;
    out_result->error_code = SMT_ERROR_BACKEND_UNAVAILABLE;
    snprintf(out_result->error_message, sizeof(out_result->error_message),
             "Backend '%s' is not available (not linked)", smtsolver_backend_type_name(out_result->backend_used));
    return 0;
}

/* ============================================================
 * 静态 VTable 数组 —— 后端类型 → 操作函数映射
 * ============================================================ */
static const SchedulerBackendVTable kSchedulerBackendVTables[] = {
    {GROEBNER,     "Groebner",  groebner_solve,       NULL, NULL},
    {SMT_Z3,       "Z3",        smt_unavailable_solve, NULL, NULL},
    {SMT_CVC5,     "CVC5",      smt_unavailable_solve, NULL, NULL},
    {SMT_SINGULAR, "Singular",  smt_unavailable_solve, NULL, NULL},
};
static const int kSchedulerBackendVTableCount =
    (int)(sizeof(kSchedulerBackendVTables) / sizeof(kSchedulerBackendVTables[0]));

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

    /* VTable 分发 —— 替代 switch(backend_type) */
    {
        bool dispatched = false;
        for (int i = 0; i < kSchedulerBackendVTableCount; i++) {
            if (kSchedulerBackendVTables[i].type == backend_type) {
                if (kSchedulerBackendVTables[i].solve) {
                    kSchedulerBackendVTables[i].solve(scheduler, graph, out_result);
                }
                dispatched = true;
                break;
            }
        }

        if (!dispatched) {
            out_result->sat_result = SMT_RESULT_ERROR;
            out_result->error_code = SMT_ERROR_BACKEND_UNAVAILABLE;
            snprintf(out_result->error_message, sizeof(out_result->error_message), "Unknown backend type %d",
                     (int) backend_type);
        }
    }

    /* 更新调度器统计 */
    scheduler->stats.total_solves++;
    uint64_t elapsed_us = lv_get_time_us() - start_us;
    scheduler->stats.total_solve_time_us += (int64_t) elapsed_us;
    if ((int64_t) elapsed_us > scheduler->stats.max_solve_time_us) {
        scheduler->stats.max_solve_time_us = (int64_t) elapsed_us;
    }
    if (lv_index_in_range(backend_type, SCHEDULER_MAX_BACKEND_TYPES)) {
        scheduler->stats.backend_solve_counts[(int) backend_type]++;
    }

    return 0;
}

int scheduler_solve(EngineScheduler *scheduler, const ConstraintGraph *graph, SMTSolverResult *out_result) {
    if (!scheduler || !graph || !out_result)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "scheduler_solve: NULL parameter");

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
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "scheduler_solve_groebner_compat: NULL parameter");

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

    /* 查找表分发 —— 替代 switch(status) */
    {
        int idx = (int) status;
        if (lv_index_in_range(idx, (int)(sizeof(kSolverStatusToReturnCode) / sizeof(kSolverStatusToReturnCode[0])))) {
            return kSolverStatusToReturnCode[idx];
        }
        return -1; /* 未知状态视为错误 */
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
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "scheduler_diagnose: NULL parameter or zero buf_size");

    /* 盲区6：lvStrBuf 收敛游标样板（原 5 处 snprintf(buf+pos, buf_size-pos) + pos 手工累加）。
     * 输出字节不变：返回值为应有长度（与 snprintf 链的 rc 累加一致），
     * buf 内容为整体文本前 buf_size-1 字节 + NUL（与原逐段截断一致）。 */
    lvStrBuf sb = {0};
    lv_strbuf_printf(&sb,
                     "Lv-00 EngineScheduler Diagnostic\n"
                     "================================\n"
                     "Default backend: %d (%s)\n"
                     "Fallback: %s, depth=%d\n"
                     "Auto-create: %s\n"
                     "Registered backends: %d\n",
                     (int) scheduler->default_backend, smtsolver_backend_type_name(scheduler->default_backend),
                     scheduler->enable_fallback ? "enabled" : "disabled", scheduler->fallback_depth,
                     scheduler->auto_create ? "yes" : "no", scheduler->backend_count);

    for (int i = 0; i < scheduler->backend_count; i++) {
        lv_strbuf_printf(&sb, "  [%d] type=%d (%s) available=%s priority=%d desc=%s\n", i,
                         (int) scheduler->backends[i].type, smtsolver_backend_type_name(scheduler->backends[i].type),
                         scheduler->backends[i].available ? "yes" : "no", scheduler->backends[i].priority,
                         scheduler->backends[i].description);
    }

    lv_strbuf_printf(&sb, "Routing rules: %d\n",
                     lv_registry_count(&scheduler->routing_rule_registry));

    int rule_total = lv_registry_count(&scheduler->routing_rule_registry);
    for (int i = 0; i < rule_total; i++) {
        const char *rule_reg_name = NULL;
        void *rule_reg_value = NULL;
        if (!lv_registry_get_at(&scheduler->routing_rule_registry, i, &rule_reg_name, &rule_reg_value)) {
            continue;
        }
        RoutingRule *rule = (RoutingRule *) rule_reg_value;
        lv_strbuf_printf(&sb, "  [%d] '%s' priority=%d enabled=%s conditions=%d backend=%d\n", i,
                         rule->name, rule->priority,
                         rule->enabled ? "yes" : "no", rule->condition_count,
                         (int) rule->target_backend);
    }

    lv_strbuf_printf(&sb, "Stats: solves=%lld time=%lldus max=%lldus fallback=%lld miss=%lld\n",
                     (long long) scheduler->stats.total_solves, (long long) scheduler->stats.total_solve_time_us,
                     (long long) scheduler->stats.max_solve_time_us, (long long) scheduler->stats.fallback_count,
                     (long long) scheduler->stats.selection_miss_count);

    size_t written = sb.len;
    size_t copy = written < buf_size ? written : buf_size - 1;
    memcpy(buf, sb.data, copy);
    buf[copy] = '\0';
    lv_strbuf_destroy(&sb);

    return (int) written;
}

/* ============================================================
 * 结果转换：SMTSolverResult → GroebnerResult
 * ============================================================ */
GroebnerResult *scheduler_convert_smt_to_groebner(const SMTSolverResult *smt_result, const ConstraintGraph *graph) {
    if (!smt_result || !graph)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "scheduler_convert_smt_to_groebner: NULL parameter");

    GroebnerResult *result = (GroebnerResult *) lv_calloc(1, sizeof(GroebnerResult));
    if (!result)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "scheduler_convert_smt_to_groebner: calloc result failed");

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
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "scheduler_convert_smt_to_groebner: calloc solutions failed");
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
    if (!engine)
        return;

    /* 记录到 TLS，供旧版 API（lv_engine_schedule 等）使用 */
    g_tls_engine = engine;

    /* 调度器嵌入到引擎实例中，支持多引擎隔离 */
    if (!engine->scheduler) {
        engine->scheduler = scheduler_create();
    }
}

void lv_engine_scheduler_shutdown(lvEngine *engine) {
    if (!engine)
        return;

    /* 仅当本线程 TLS 仍关联到该引擎时清空，避免旧版 API 解引用已销毁的引擎 */
    if (g_tls_engine == engine) {
        g_tls_engine = NULL;
    }
}

/* ============================================================
 * 向后兼容 —— 旧版调度任务描述符表
 *
 * 旧版调度器是一个简单的优先级队列；在新的设计下，task_name
 * 决定操作类型（映射到描述符表中的处理函数），priority 保留
 * 兼容（不参与路由）。
 * ============================================================ */
typedef void (*SchedulerTaskHandler)(EngineScheduler *scheduler, lvEngine *engine, SMTSolverResult *result);

typedef struct {
    const char *name;            /**< 任务名（lv_engine_schedule 的 task_name） */
    SchedulerTaskHandler handler; /**< 任务处理函数 */
} SchedulerTaskEntry;

static void sched_task_solve(EngineScheduler *scheduler, lvEngine *engine, SMTSolverResult *result) {
    scheduler_solve(scheduler, engine->main_graph, result);
}

static void sched_task_normalize(EngineScheduler *scheduler, lvEngine *engine, SMTSolverResult *result) {
    (void)scheduler; (void)result;
    graph_normalize(engine->main_graph, false);
}

static void sched_task_unify(EngineScheduler *scheduler, lvEngine *engine, SMTSolverResult *result) {
    (void)scheduler; (void)engine; (void)result;
    /* unify 任务需要两个图（构造图 + 命题图），调度器只有一个主图。
     * 当前直接返回 0 表示"无操作完成"，上层应通过
     * unify_construction_with_proposition() 显式调用。 */
    LOG_WARN("scheduler", "unify 任务需要命题图，当前为无操作。请使用 unify_construction_with_proposition() 直接调用。");
}

static void sched_task_rewrite(EngineScheduler *scheduler, lvEngine *engine, SMTSolverResult *result) {
    /* rewrite 与 solve 当前共享同一求解路径 */
    scheduler_solve(scheduler, engine->main_graph, result);
}

/** 任务名 → 处理函数描述符表（新增任务只需追加一条） */
static const SchedulerTaskEntry kSchedulerTasks[] = {
    {"solve",     sched_task_solve},
    {"normalize", sched_task_normalize},
    {"unify",     sched_task_unify},
    {"rewrite",   sched_task_rewrite},
};

int lv_engine_schedule(const char *task_name, int priority) {
    if (!task_name)
        return -1;

    /* 从 TLS 获取当前线程关联的引擎实例 */
    lvEngine *engine = g_tls_engine;
    if (!engine)
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "lv_engine_schedule: engine not initialized (call lv_engine_scheduler_init first)");

    EngineScheduler *scheduler = engine->scheduler;
    if (!scheduler) {
        scheduler = scheduler_create();
        if (!scheduler)
            lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_engine_schedule: scheduler_create failed");
        engine->scheduler = scheduler;
    }

    lv_UNUSED(priority);

    if (!engine->main_graph)
        return 0;

    SMTSolverResult result;
    smtsolver_result_init(&result);

    /* 描述符表分发 —— 替代 if/else 任务名链；
     * 未知任务名保持原语义：无操作，返回 0 */
    for (size_t i = 0; i < sizeof(kSchedulerTasks) / sizeof(kSchedulerTasks[0]); i++) {
        if (strcmp(task_name, kSchedulerTasks[i].name) == 0) {
            kSchedulerTasks[i].handler(scheduler, engine, &result);
            break;
        }
    }

    smtsolver_result_clear(&result);
    return 0;
}

bool lv_engine_execute_pending(void) {
    lvEngine *engine = g_tls_engine;
    if (!engine)
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_STATE, "lv_engine_execute_pending: engine not initialized (call lv_engine_scheduler_init first)");
    if (!engine->scheduler)
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_STATE, "lv_engine_execute_pending: scheduler not initialized");
    if (!engine->main_graph)
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_STATE, "lv_engine_execute_pending: main_graph is NULL");

    /* 用默认后端执行一次求解 */
    SMTSolverResult result;
    smtsolver_result_init(&result);

    scheduler_solve(engine->scheduler, engine->main_graph, &result);

    smtsolver_result_clear(&result);

    return true;
}

int lv_engine_pending_count(void) {
    return 0; /* 新版调度器无任务队列概念 */
}
