#ifndef LV00_REWRITE_STRATEGY_H
#define LV00_REWRITE_STRATEGY_H

#include "lv00/rewrite.h"
#include <stddef.h>
#include <stdbool.h>
#include "lv00/lv00_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============== 类型定义 ============== */

/** 重写条件函数指针类型 */
typedef bool (*Lv00RewriteConditionFn)(const char *term);

/** 重写策略枚举 */
typedef enum {
    LV00_RWS_FIRST = 0,
    LV00_RWS_BEST = 1,
    LV00_RWS_BREADTH = 2,
    LV00_RWS_DEPTH = 3,
    LV00_RWS_INNERMOST = 4,
    LV00_RWS_OUTERMOST = 5,
    LV00_RWS_PARALLEL = 6,
    LV00_RWS_EGRAPH = 7
} Lv00RewriteStrategyType;

/* 兼容旧命名 */
typedef Lv00RewriteStrategyType Lv00RewriteStrategyEx;

/* 兼容旧命名（用于 switch case） */
#ifndef REWRITE_INNERMOST
#define REWRITE_INNERMOST LV00_RWS_INNERMOST
#endif
#ifndef REWRITE_OUTERMOST
#define REWRITE_OUTERMOST LV00_RWS_OUTERMOST
#endif
#ifndef REWRITE_PARALLEL
#define REWRITE_PARALLEL LV00_RWS_PARALLEL
#endif
#ifndef REWRITE_EGRAPH
#define REWRITE_EGRAPH LV00_RWS_EGRAPH
#endif

/** 重写规则扩展结构体 */
typedef struct Lv00RewriteRuleEx {
    const char *name;
    const char *pattern;
    const char *replacement;
    int priority;
    Lv00RewriteConditionFn condition_fn;
} Lv00RewriteRuleEx;

/** 重写结果扩展结构体 */
typedef struct Lv00RewriteResultEx {
    char *output;
    int iterations;
    bool converged;
    bool hit_limit;
} Lv00RewriteResultEx;

/** 重写引擎扩展结构体 */
typedef struct Lv00RewriteEngineEx {
    Lv00RewriteRuleEx *rules;
    size_t rule_count;
    size_t rule_capacity;
    Lv00RewriteStrategyType strategy;
    int max_iterations;
} Lv00RewriteEngineEx;

/** 重写上下文结构体（用于 lv00_rewrite_apply_strategy） */
typedef struct Lv00RewriteContext {
    void *impl;
} Lv00RewriteContext;

/* ============== 兼容宏（修复参数数量不匹配） ============== */

/** 修复：原宏不接受参数，但实现需要参数 */
Lv00RewriteEngineEx *rewrite_engine_ex_create(Lv00RewriteStrategyType strategy, int max_iterations);

/** 销毁重写引擎（NULL 安全） */
void rewrite_engine_ex_destroy(Lv00RewriteEngineEx *engine);

/** 添加重写规则（按优先级排序） */
bool rewrite_engine_ex_add_rule(Lv00RewriteEngineEx *engine,
    const char *name, const char *pattern, const char *replacement,
    int priority, Lv00RewriteConditionFn condition);

/** 执行重写引擎 */
bool rewrite_engine_ex_apply(Lv00RewriteEngineEx *engine,
    const char *input, Lv00RewriteResultEx *result);

/** 销毁重写结果（NULL 安全） */
void rewrite_engine_result_ex_destroy(Lv00RewriteResultEx *result);

/* ============== 函数声明 ============== */

/** 应用重写策略 */
int lv00_rewrite_apply_strategy(Lv00RewriteContext *ctx, Lv00RewriteStrategyType strategy);

#ifdef __cplusplus
}
#endif

#endif
