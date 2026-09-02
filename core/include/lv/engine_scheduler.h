#ifndef lv_ENGINE_SCHEDULER_H
#define lv_ENGINE_SCHEDULER_H

#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lv/constraint_graph.h"
#include "lv/smt_backend.h" /* SolverBackendType, SMTSolverConfig, SMTSolverResult */
#include "lv/solver.h"      /* GroebnerResult */

#ifdef __cplusplus
extern "C" {
#endif

/* ── 图特征分析 ── */
typedef struct GraphFeatures {
    int total_nodes, total_constraints;
    int variable_nodes, fixed_nodes;
    int port_nodes, block_nodes;
    int incidence_constraints, betweenness_constraints;
    int intersection_constraints, containment_constraints;
    int connection_constraints;
    int angle_constraints;
    int nonlinear_constraints;
    double nonlinear_ratio;
    bool has_quantifier_like;
    bool has_boolean_variables;
    int estimated_equation_count;
    int estimated_degree_max;
    int64_t analysis_time_us;
} GraphFeatures;

/* ── 路由条件 ── */
typedef enum {
    ROUTE_COND_NONE,
    ROUTE_COND_VAR_COUNT_LE,
    ROUTE_COND_VAR_COUNT_GE,
    ROUTE_COND_NONLINEAR_RATIO_GE,
    ROUTE_COND_HAS_QUANTIFIER,
    ROUTE_COND_HAS_BOOLEAN,
    ROUTE_COND_DEGREE_GE,
    ROUTE_COND_BACKEND_AVAILABLE
} RouteConditionType;

typedef enum { ROUTE_COMBINE_AND, ROUTE_COMBINE_OR } RouteCombineMode;

typedef struct RouteCondition {
    RouteConditionType type;
    int int_value;      /* 阈值参数 */
    double float_value; /* 浮点阈值 */
} RouteCondition;

typedef struct RoutingRule {
    char name[64];
    int priority;
    bool enabled;
    RouteCondition conditions[4];
    int condition_count;
    RouteCombineMode combine_mode;
    SolverBackendType target_backend;
} RoutingRule;

/* ── 后端注册条目 ── */
typedef SolverBackendType (*SolverBackendDetectFunc)(void);

typedef struct SchedulerBackendEntry {
    SolverBackendType type;
    bool available;
    int priority; /* 选择优先级 */
    char description[128];
    SolverBackendDetectFunc detect_func;
} SchedulerBackendEntry;

/* ── 调度器统计 ── */
#define SCHEDULER_MAX_BACKEND_TYPES 8

typedef struct SchedulerStats {
    int64_t total_solves;
    int64_t total_solve_time_us;
    int64_t max_solve_time_us;
    int64_t fallback_count;
    int64_t selection_miss_count;
    int64_t backend_solve_counts[SCHEDULER_MAX_BACKEND_TYPES];
} SchedulerStats;

/* ── 调度器不透明句柄 ── */
typedef struct EngineScheduler EngineScheduler;

/* ── 常量 ── */
#define SCHEDULER_MAX_ROUTING_RULES 32
#define SCHEDULER_MAX_BACKEND_INSTANCES 8
#define SCHEDULER_MAX_FALLBACK_DEPTH 4

/* ── 生命周期 ── */
EngineScheduler *scheduler_create(void);
lv_PUBLIC_API void scheduler_destroy(EngineScheduler *scheduler);
lv_PUBLIC_API void scheduler_reset(EngineScheduler *scheduler);

/* ── 后端注册 ── */
int scheduler_register_backend(EngineScheduler *scheduler, SolverBackendType type, int priority,
                               const char *description, SolverBackendDetectFunc detect_func);
lv_PUBLIC_API int scheduler_unregister_backend(EngineScheduler *scheduler, SolverBackendType type);
lv_PUBLIC_API int scheduler_list_available_backends(const EngineScheduler *scheduler, SolverBackendType *out_types, int max_count);

/* ── 后端选择与可用性 ── */
lv_PUBLIC_API bool scheduler_is_backend_available(const EngineScheduler *scheduler, SolverBackendType type);
lv_PUBLIC_API void scheduler_set_backend_available(EngineScheduler *scheduler, SolverBackendType type, bool available);

/* ── 路由规则管理 ── */
lv_PUBLIC_API int scheduler_add_routing_rule(EngineScheduler *scheduler, RoutingRule *rule);
lv_PUBLIC_API int scheduler_remove_routing_rule(EngineScheduler *scheduler, const char *name);
lv_PUBLIC_API int scheduler_load_preset_rules(EngineScheduler *scheduler);

/* ── 图特征分析 ── */
lv_PUBLIC_API int scheduler_analyze_graph(const ConstraintGraph *graph, GraphFeatures *features);
lv_PUBLIC_API const char *scheduler_feature_summary(const GraphFeatures *features);

/* ── 后端选择 ── */
SolverBackendType scheduler_select_backend(const EngineScheduler *scheduler, const ConstraintGraph *graph,
                                           char *out_reason, size_t reason_size);

/* ── 分发求解 ── */
lv_PUBLIC_API int scheduler_solve(EngineScheduler *scheduler, const ConstraintGraph *graph, SMTSolverResult *out_result);
int scheduler_solve_with_backend(EngineScheduler *scheduler, const ConstraintGraph *graph,
                                 SolverBackendType backend_type, SMTSolverResult *out_result);
int scheduler_solve_groebner_compat(EngineScheduler *scheduler, const ConstraintGraph *graph,
                                    GroebnerResult **out_result);

/* ── 配置 ── */
lv_PUBLIC_API void scheduler_set_default_backend(EngineScheduler *scheduler, SolverBackendType type);
void scheduler_set_fallback_policy(EngineScheduler *scheduler, bool enable, const SolverBackendType *fallback_types,
                                   int depth);
lv_PUBLIC_API void scheduler_set_auto_create(EngineScheduler *scheduler, bool auto_create);

/* ── 统计与诊断 ── */
lv_PUBLIC_API void scheduler_get_stats(const EngineScheduler *scheduler, SchedulerStats *stats);
lv_PUBLIC_API void scheduler_reset_stats(EngineScheduler *scheduler);
lv_PUBLIC_API int scheduler_diagnose(const EngineScheduler *scheduler, char *buf, size_t buf_size);

/* ── 结果转换 ── */
GroebnerResult *scheduler_convert_smt_to_groebner(const SMTSolverResult *smt_result, const ConstraintGraph *graph);

/* ── 向后兼容 ── */
struct lvEngine;
lv_PUBLIC_API void lv_engine_scheduler_init(struct lvEngine *engine);
lv_PUBLIC_API void lv_engine_scheduler_shutdown(struct lvEngine *engine);
lv_PUBLIC_API int lv_engine_schedule(const char *task_name, int priority);
lv_PUBLIC_API bool lv_engine_execute_pending(void);
lv_PUBLIC_API int lv_engine_pending_count(void);

#ifdef __cplusplus
}
#endif

#endif
