#ifndef lv_REWRITE_STRATEGY_H
#define lv_REWRITE_STRATEGY_H

#include "lv/rewrite.h"
#include <stddef.h>
#include <stdbool.h>
#include "lv/lv_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============== 类型定义 ============== */

/** 重写条件函数指针类型 */
typedef bool (*lvRewriteConditionFn)(const char *term);

/** 重写策略枚举 */
typedef enum {
    lv_RWS_FIRST = 0,
    lv_RWS_BEST = 1,
    lv_RWS_BREADTH = 2,
    lv_RWS_DEPTH = 3,
    lv_RWS_INNERMOST = 4,
    lv_RWS_OUTERMOST = 5,
    lv_RWS_PARALLEL = 6,
    lv_RWS_EGRAPH = 7
} lvRewriteStrategyType;

/* 兼容旧命名 */
typedef lvRewriteStrategyType lvRewriteStrategyEx;

/* 兼容旧命名（用于 switch case） */
#ifndef REWRITE_INNERMOST
#define REWRITE_INNERMOST lv_RWS_INNERMOST
#endif
#ifndef REWRITE_OUTERMOST
#define REWRITE_OUTERMOST lv_RWS_OUTERMOST
#endif
#ifndef REWRITE_PARALLEL
#define REWRITE_PARALLEL lv_RWS_PARALLEL
#endif
#ifndef REWRITE_EGRAPH
#define REWRITE_EGRAPH lv_RWS_EGRAPH
#endif

/** 重写规则扩展结构体 */
typedef struct lvRewriteRuleEx {
    const char *name;
    const char *pattern;
    const char *replacement;
    int priority;
    lvRewriteConditionFn condition_fn;
} lvRewriteRuleEx;

/** 重写结果扩展结构体 */
typedef struct lvRewriteResultEx {
    char *output;
    int iterations;
    bool converged;
    bool hit_limit;
} lvRewriteResultEx;

/** 重写引擎扩展结构体 */
typedef struct lvRewriteEngineEx {
    lvRewriteRuleEx *rules;
    size_t rule_count;
    size_t rule_capacity;
    lvRewriteStrategyType strategy;
    int max_iterations;
} lvRewriteEngineEx;

/** 重写上下文结构体（用于 lv_rewrite_apply_strategy） */
typedef struct lvRewriteContext {
    void *impl;
} lvRewriteContext;

/* ============== 兼容宏（修复参数数量不匹配） ============== */

/** 修复：原宏不接受参数，但实现需要参数 */
lvRewriteEngineEx *rewrite_engine_ex_create(lvRewriteStrategyType strategy, int max_iterations);

/** 销毁重写引擎（NULL 安全） */
void rewrite_engine_ex_destroy(lvRewriteEngineEx *engine);

/** 添加重写规则（按优先级排序） */
bool rewrite_engine_ex_add_rule(lvRewriteEngineEx *engine,
    const char *name, const char *pattern, const char *replacement,
    int priority, lvRewriteConditionFn condition);

/** 执行重写引擎 */
bool rewrite_engine_ex_apply(lvRewriteEngineEx *engine,
    const char *input, lvRewriteResultEx *result);

/** 销毁重写结果（NULL 安全） */
void rewrite_engine_result_ex_destroy(lvRewriteResultEx *result);

/* ============== 函数声明 ============== */

/** 应用重写策略 */
int lv_rewrite_apply_strategy(lvRewriteContext *ctx, lvRewriteStrategyType strategy);

#ifdef __cplusplus
}
#endif

#endif
