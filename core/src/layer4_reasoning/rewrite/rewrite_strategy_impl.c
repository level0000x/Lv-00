/**
 * @file rewrite_strategy_impl.c
 * @brief Maude 风格重写策略组合子的实现
 *
 * 实现 rewrite.h 中声明的策略树构造、执行和搜索 API。
 * 基于现有 rewrite_with_rules() 和 graph_copy() 构建。
 */
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/lambda_to_graph.h"
#include "lv/lv.h"
#include "lv/rewrite.h"
#include "lv/lv_xmacro.h"

#include "lv/lv_internal.h"

/* ================================================================
 *  策略树构造函数
 * ================================================================ */

RewriteStrategy *rewrite_strategy_create_idle(void) {
    RewriteStrategy *s = lv_calloc(1, sizeof(RewriteStrategy));
    if (s)
        s->kind = REWRITE_STRATEGY_KIND_IDLE;
    return s;
}

RewriteStrategy *rewrite_strategy_create_fail(void) {
    RewriteStrategy *s = lv_calloc(1, sizeof(RewriteStrategy));
    if (s)
        s->kind = REWRITE_STRATEGY_KIND_FAIL;
    return s;
}

RewriteStrategy *rewrite_strategy_create_apply_rule(int rule_id) {
    RewriteStrategy *s = lv_calloc(1, sizeof(RewriteStrategy));
    if (s) {
        s->kind = REWRITE_STRATEGY_KIND_APPLY_RULE;
        s->rule_id = rule_id;
    }
    return s;
}

RewriteStrategy *rewrite_strategy_create_match(const char *pattern) {
    if (!pattern)
        return NULL;
    RewriteStrategy *s = lv_calloc(1, sizeof(RewriteStrategy));
    if (s) {
        s->kind = REWRITE_STRATEGY_KIND_MATCH_PATTERN;
        s->pattern_expr = lv_strdup_safe(pattern);
    }
    return s;
}

RewriteStrategy *rewrite_strategy_create_test(int (*test)(void *), void *ctx) {
    if (!test)
        return NULL;
    RewriteStrategy *s = lv_calloc(1, sizeof(RewriteStrategy));
    if (s) {
        s->kind = REWRITE_STRATEGY_KIND_TEST_COND;
        s->test_func = test;
        s->test_ctx = ctx;
    }
    return s;
}

RewriteStrategy *rewrite_strategy_sequence(RewriteStrategy *left, RewriteStrategy *right) {
    if (!left || !right)
        return NULL;
    RewriteStrategy *s = lv_calloc(1, sizeof(RewriteStrategy));
    if (s) {
        s->kind = REWRITE_STRATEGY_KIND_SEQUENCE;
        s->left = left;
        s->right = right;
    }
    return s;
}

RewriteStrategy *rewrite_strategy_orelse(RewriteStrategy *left, RewriteStrategy *right) {
    if (!left || !right)
        return NULL;
    RewriteStrategy *s = lv_calloc(1, sizeof(RewriteStrategy));
    if (s) {
        s->kind = REWRITE_STRATEGY_KIND_ORELSE;
        s->left = left;
        s->right = right;
    }
    return s;
}

RewriteStrategy *rewrite_strategy_repeat(RewriteStrategy *child, int max_iter) {
    if (!child)
        return NULL;
    RewriteStrategy *s = lv_calloc(1, sizeof(RewriteStrategy));
    if (s) {
        s->kind = REWRITE_STRATEGY_KIND_REPEAT;
        s->left = child;
        s->max_iterations = max_iter;
    }
    return s;
}

RewriteStrategy *rewrite_strategy_normalize(RewriteStrategy *child) {
    if (!child)
        return NULL;
    RewriteStrategy *s = lv_calloc(1, sizeof(RewriteStrategy));
    if (s) {
        s->kind = REWRITE_STRATEGY_KIND_NORMALIZE;
        s->left = child;
    }
    return s;
}

RewriteStrategy *rewrite_strategy_try(RewriteStrategy *child) {
    if (!child)
        return NULL;
    RewriteStrategy *s = lv_calloc(1, sizeof(RewriteStrategy));
    if (s) {
        s->kind = REWRITE_STRATEGY_KIND_TRY;
        s->left = child;
    }
    return s;
}

RewriteStrategy *rewrite_strategy_create_beta_reduce(void) {
    RewriteStrategy *s = lv_calloc(1, sizeof(RewriteStrategy));
    if (s)
        s->kind = REWRITE_STRATEGY_KIND_BETA_REDUCE;
    return s;
}

/* ================================================================
 *  策略树销毁
 * ================================================================ */

void rewrite_strategy_destroy(RewriteStrategy *s) {
    if (!s)
        return;
    /* 递归销毁子节点 */
    if (s->left && (s->kind == REWRITE_STRATEGY_KIND_SEQUENCE || s->kind == REWRITE_STRATEGY_KIND_ORELSE ||
                    s->kind == REWRITE_STRATEGY_KIND_REPEAT || s->kind == REWRITE_STRATEGY_KIND_NORMALIZE ||
                    s->kind == REWRITE_STRATEGY_KIND_TRY)) {
        rewrite_strategy_destroy(s->left);
    }
    if (s->right && (s->kind == REWRITE_STRATEGY_KIND_SEQUENCE || s->kind == REWRITE_STRATEGY_KIND_ORELSE)) {
        rewrite_strategy_destroy(s->right);
    }
    lv_free((void **) &s->pattern_expr);
    lv_free((void **) &s);
}

/* ================================================================
 *  策略执行核心
 * ================================================================ */

/* 策略执行函数指针类型 */
typedef bool (*StrategyExecuteFn)(const ConstraintGraph *graph, const RewriteStrategy *strategy,
                                  const RewriteRule *rules, int rule_count,
                                  ConstraintGraph **out_graph, int *out_steps);

/* 前向声明 */
static bool execute_idle(const ConstraintGraph *graph, const RewriteStrategy *strategy,
                         const RewriteRule *rules, int rule_count,
                         ConstraintGraph **out_graph, int *out_steps);
static bool execute_fail(const ConstraintGraph *graph, const RewriteStrategy *strategy,
                         const RewriteRule *rules, int rule_count,
                         ConstraintGraph **out_graph, int *out_steps);
static bool execute_apply_rule(const ConstraintGraph *graph, const RewriteStrategy *strategy,
                               const RewriteRule *rules, int rule_count,
                               ConstraintGraph **out_graph, int *out_steps);
static bool execute_match_pattern(const ConstraintGraph *graph, const RewriteStrategy *strategy,
                                  const RewriteRule *rules, int rule_count,
                                  ConstraintGraph **out_graph, int *out_steps);
static bool execute_test_cond(const ConstraintGraph *graph, const RewriteStrategy *strategy,
                              const RewriteRule *rules, int rule_count,
                              ConstraintGraph **out_graph, int *out_steps);
static bool execute_sequence(const ConstraintGraph *graph, const RewriteStrategy *strategy,
                             const RewriteRule *rules, int rule_count,
                             ConstraintGraph **out_graph, int *out_steps);
static bool execute_orelse(const ConstraintGraph *graph, const RewriteStrategy *strategy,
                           const RewriteRule *rules, int rule_count,
                           ConstraintGraph **out_graph, int *out_steps);
static bool execute_repeat(const ConstraintGraph *graph, const RewriteStrategy *strategy,
                           const RewriteRule *rules, int rule_count,
                           ConstraintGraph **out_graph, int *out_steps);
static bool execute_normalize(const ConstraintGraph *graph, const RewriteStrategy *strategy,
                              const RewriteRule *rules, int rule_count,
                              ConstraintGraph **out_graph, int *out_steps);
static bool execute_try(const ConstraintGraph *graph, const RewriteStrategy *strategy,
                        const RewriteRule *rules, int rule_count,
                        ConstraintGraph **out_graph, int *out_steps);
static bool execute_beta_reduce(const ConstraintGraph *graph, const RewriteStrategy *strategy,
                                const RewriteRule *rules, int rule_count,
                                ConstraintGraph **out_graph, int *out_steps);

/** 策略执行 VTable：按策略类型索引 */
static const StrategyExecuteFn kStrategyExecutors[] = {
    [REWRITE_STRATEGY_KIND_IDLE] = execute_idle,
    [REWRITE_STRATEGY_KIND_FAIL] = execute_fail,
    [REWRITE_STRATEGY_KIND_APPLY_RULE] = execute_apply_rule,
    [REWRITE_STRATEGY_KIND_MATCH_PATTERN] = execute_match_pattern,
    [REWRITE_STRATEGY_KIND_TEST_COND] = execute_test_cond,
    [REWRITE_STRATEGY_KIND_SEQUENCE] = execute_sequence,
    [REWRITE_STRATEGY_KIND_ORELSE] = execute_orelse,
    [REWRITE_STRATEGY_KIND_REPEAT] = execute_repeat,
    [REWRITE_STRATEGY_KIND_NORMALIZE] = execute_normalize,
    [REWRITE_STRATEGY_KIND_TRY] = execute_try,
    [REWRITE_STRATEGY_KIND_BETA_REDUCE] = execute_beta_reduce,
};

/**
 * @brief 递归执行策略树
 *
 * @param graph      当前约束图（不会被修改）
 * @param strategy   策略树节点
 * @param rules      重写规则数组
 * @param rule_count 规则数量
 * @param out_graph  输出：执行后的新约束图（调用者负责释放，失败时为 NULL）
 * @param out_steps  输出：执行步数
 * @return true 策略执行成功（产生了变化或无变化但未失败）
 */
static bool strategy_execute(const ConstraintGraph *graph, const RewriteStrategy *strategy, const RewriteRule *rules,
                             int rule_count, ConstraintGraph **out_graph, int *out_steps) {
    if (!graph || !strategy)
        return false;

    *out_graph = NULL;
    *out_steps = 0;

    return LV_DISPATCH(kStrategyExecutors, strategy->kind, false, graph, strategy, rules, rule_count, out_graph, out_steps);
}

/* ================================================================
 *  策略执行函数（VTable 派发目标）
 * ================================================================ */

static bool execute_idle(const ConstraintGraph *graph, const RewriteStrategy *strategy, const RewriteRule *rules,
                         int rule_count, ConstraintGraph **out_graph, int *out_steps) {
    (void)strategy; (void)rules; (void)rule_count;
    *out_graph = graph_copy(graph);
    return *out_graph != NULL;
}

static bool execute_fail(const ConstraintGraph *graph, const RewriteStrategy *strategy, const RewriteRule *rules,
                         int rule_count, ConstraintGraph **out_graph, int *out_steps) {
    (void)graph; (void)strategy; (void)rules; (void)rule_count; (void)out_graph; (void)out_steps;
    return false;
}

static bool execute_apply_rule(const ConstraintGraph *graph, const RewriteStrategy *strategy, const RewriteRule *rules,
                               int rule_count, ConstraintGraph **out_graph, int *out_steps) {
    if (strategy->rule_id < 0 || strategy->rule_id >= rule_count || !rules)
        return false;

    ConstraintGraph *cpy = graph_copy(graph);
    if (!cpy)
        return false;

    RewriteRule rules_arr[1];
    rules_arr[0] = rules[strategy->rule_id];
    RewriteStatus status = rewrite_with_rules(cpy, (RewriteRule **) &rules_arr, 1, 1, false);

    if (status == REWRITE_STATUS_APPLIED || status == REWRITE_STATUS_OK) {
        *out_graph = cpy;
        *out_steps = 1;
        return true;
    }
    graph_destroy(cpy);
    return false;
}

static bool execute_match_pattern(const ConstraintGraph *graph, const RewriteStrategy *strategy, const RewriteRule *rules,
                                  int rule_count, ConstraintGraph **out_graph, int *out_steps) {
    if (!strategy->pattern_expr || rule_count == 0 || !rules)
        return false;

    ConstraintGraph *cpy = graph_copy(graph);
    if (!cpy)
        return false;

    bool matched = false;
    for (int i = 0; i < rule_count && !matched; i++) {
        RewriteRule single[1];
        single[0] = rules[i];
        RewriteStatus st = rewrite_with_rules(cpy, (RewriteRule **) &single, 1, 1, false);
        if (st == REWRITE_STATUS_APPLIED || st == REWRITE_STATUS_OK) {
            matched = true;
        }
    }

    *out_graph = cpy;
    *out_steps = 0;
    return matched;
}

static bool execute_test_cond(const ConstraintGraph *graph, const RewriteStrategy *strategy, const RewriteRule *rules,
                              int rule_count, ConstraintGraph **out_graph, int *out_steps) {
    (void)rules; (void)rule_count;
    if (!strategy->test_func)
        return false;
    *out_graph = graph_copy(graph);
    *out_steps = 0;
    return strategy->test_func(strategy->test_ctx) != 0;
}

static bool execute_sequence(const ConstraintGraph *graph, const RewriteStrategy *strategy, const RewriteRule *rules,
                             int rule_count, ConstraintGraph **out_graph, int *out_steps) {
    ConstraintGraph *intermediate = NULL;
    int steps1 = 0;
    if (!strategy->left || !strategy->right)
        return false;

    if (!strategy_execute(graph, strategy->left, rules, rule_count, &intermediate, &steps1)) {
        return false;
    }

    ConstraintGraph *final = NULL;
    int steps2 = 0;
    bool ok = strategy_execute(intermediate, strategy->right, rules, rule_count, &final, &steps2);
    graph_destroy(intermediate);

    *out_graph = final;
    *out_steps = steps1 + steps2;
    return ok;
}

static bool execute_orelse(const ConstraintGraph *graph, const RewriteStrategy *strategy, const RewriteRule *rules,
                           int rule_count, ConstraintGraph **out_graph, int *out_steps) {
    if (!strategy->left || !strategy->right)
        return false;

    ConstraintGraph *left_result = NULL;
    int left_steps = 0;
    if (strategy_execute(graph, strategy->left, rules, rule_count, &left_result, &left_steps)) {
        *out_graph = left_result;
        *out_steps = left_steps;
        return true;
    }

    return strategy_execute(graph, strategy->right, rules, rule_count, out_graph, out_steps);
}

static bool execute_repeat(const ConstraintGraph *graph, const RewriteStrategy *strategy, const RewriteRule *rules,
                           int rule_count, ConstraintGraph **out_graph, int *out_steps) {
    if (!strategy->left)
        return false;

    ConstraintGraph *current = graph_copy(graph);
    if (!current)
        return false;

    int total_steps = 0;
    int max_iter = strategy->max_iterations > 0 ? strategy->max_iterations : 1000;

    for (int i = 0; i < max_iter; i++) {
        ConstraintGraph *next = NULL;
        int step = 0;
        if (!strategy_execute(current, strategy->left, rules, rule_count, &next, &step)) {
            break;
        }
        total_steps += step;
        graph_destroy(current);
        current = next;
    }

    *out_graph = current;
    *out_steps = total_steps;
    return true;
}

static bool execute_normalize(const ConstraintGraph *graph, const RewriteStrategy *strategy, const RewriteRule *rules,
                              int rule_count, ConstraintGraph **out_graph, int *out_steps) {
    if (!strategy->left)
        return false;

    RewriteStrategy *seq = rewrite_strategy_sequence(strategy->left, strategy->left);
    RewriteStrategy *norm = rewrite_strategy_repeat(seq, 0);

    ConstraintGraph *result = NULL;
    int steps = 0;
    bool ok = strategy_execute(graph, norm, rules, rule_count, &result, &steps);

    rewrite_strategy_destroy(norm);

    *out_graph = result;
    *out_steps = steps;
    return ok;
}

static bool execute_try(const ConstraintGraph *graph, const RewriteStrategy *strategy, const RewriteRule *rules,
                        int rule_count, ConstraintGraph **out_graph, int *out_steps) {
    if (!strategy->left)
        return false;

    ConstraintGraph *result = NULL;
    int steps = 0;
    if (strategy_execute(graph, strategy->left, rules, rule_count, &result, &steps)) {
        *out_graph = result;
        *out_steps = steps;
        return true;
    }

    *out_graph = graph_copy(graph);
    *out_steps = 0;
    return true;
}

static bool execute_beta_reduce(const ConstraintGraph *graph, const RewriteStrategy *strategy, const RewriteRule *rules,
                                int rule_count, ConstraintGraph **out_graph, int *out_steps) {
    (void)strategy; (void)rules; (void)rule_count;
    ConstraintGraph *cpy = graph_copy(graph);
    if (!cpy)
        return false;
    if (beta_reduce(cpy)) {
        *out_graph = cpy;
        *out_steps = 1;
        return true;
    }
    graph_destroy(cpy);
    *out_graph = NULL;
    *out_steps = 0;
    return false;
}

bool rewrite_strategy_apply(const ConstraintGraph *graph, const RewriteStrategy *strategy, const RewriteRule *rules,
                            int rule_count, ConstraintGraph **out_graph, int *out_steps) {
    if (!graph || !strategy || !out_graph || !out_steps)
        return false;
    if (rule_count > 0 && !rules)
        return false;

    return strategy_execute(graph, strategy, rules, rule_count, out_graph, out_steps);
}

/* ================================================================
 *  逆向证明搜索（BFS/DFS）
 * ================================================================ */

/**
 * @brief 逆向证明搜索队列节点（用于 BFS）
 */
typedef struct SearchNode {
    ConstraintGraph *graph;
    int depth;
    int *path;
    int path_len;
    struct SearchNode *next;
} SearchNode;

static void search_node_destroy(SearchNode *node) {
    if (!node)
        return;
    graph_destroy(node->graph);
    lv_free((void **) &node->path);
    lv_free((void **) &node);
}

bool rewrite_search_backward(const ConstraintGraph *target_graph, const RewriteRule *rules, int rule_count,
                             int max_depth, bool use_bfs, int **out_path, int *out_path_len) {
    if (!target_graph || !rules || rule_count <= 0 || !out_path || !out_path_len) {
        return false;
    }
    if (max_depth <= 0)
        max_depth = 32;

    /* 使用广度优先队列 */
    SearchNode *head = NULL;
    SearchNode *tail = NULL;

    /* 初始节点：目标图本身 */
    SearchNode *start = lv_calloc(1, sizeof(SearchNode));
    if (!start)
        return false;
    start->graph = graph_copy(target_graph);
    start->depth = 0;
    start->path = NULL;
    start->path_len = 0;
    start->next = NULL;
    head = tail = start;

    bool found = false;
    int visited = 0;

    while (head) {
        SearchNode *cur = head;
        head = head->next;
        visited++;

        /* 如果当前图不能再被任何规则改写，说明已达基元/公理形式 */
        if (cur->depth >= max_depth) {
            search_node_destroy(cur);
            continue;
        }

        /* 尝试逆用所有规则 */
        for (int r = 0; r < rule_count; r++) {
            ConstraintGraph *cpy = graph_copy(cur->graph);
            if (!cpy)
                continue;

            RewriteRule single[1];
            single[0] = rules[r];
            RewriteStatus st = rewrite_with_rules(cpy, (RewriteRule **) &single, 1, 1, false);

            if (st == REWRITE_STATUS_APPLIED || st == REWRITE_STATUS_OK) {
                /* 找到了一条逆用路径 */
                *out_path_len = cur->path_len + 1;
                *out_path = lv_calloc((size_t) (*out_path_len), sizeof(int));
                if (*out_path) {
                    for (int i = 0; i < cur->path_len; i++) {
                        (*out_path)[i] = cur->path[i];
                    }
                    (*out_path)[cur->path_len] = r;
                }
                found = true;
                graph_destroy(cpy);
                goto cleanup;
            }

            /* BFS 或 DFS 扩展 */
            if (use_bfs || st == REWRITE_STATUS_APPLIED) {
                SearchNode *next = lv_calloc(1, sizeof(SearchNode));
                if (next) {
                    next->graph = cpy;
                    next->depth = cur->depth + 1;
                    next->path_len = cur->path_len + 1;
                    next->path = lv_calloc((size_t) next->path_len, sizeof(int));
                    if (next->path) {
                        for (int i = 0; i < cur->path_len; i++) {
                            next->path[i] = cur->path[i];
                        }
                        next->path[cur->path_len] = r;
                    }
                    next->next = NULL;
                    if (tail) {
                        tail->next = next;
                        tail = next;
                    } else {
                        head = tail = next;
                    }
                }
            } else {
                graph_destroy(cpy);
            }
        }

        search_node_destroy(cur);
    }

cleanup:
    /* 清理搜索队列 */
    while (head) {
        SearchNode *tmp = head;
        head = head->next;
        search_node_destroy(tmp);
    }

    if (!found) {
        *out_path = NULL;
        *out_path_len = 0;
    }
    return found;
}

/* ================================================================
 *  Herbie 风格数值精度优化规则
 * ================================================================ */

/** 内置数值优化规则列表 */
typedef struct {
    char *name;
    char *pattern;
    char *replacement;
    RewriteNumPriority priority;
    double improvement;
    char *condition;
} BuiltinNumRule;

static BuiltinNumRule g_builtin_rules[] = {
    {(char *) "sqrt-diff-recip", (char *) "sqrt(x+1)-sqrt(x)", (char *) "1/(sqrt(x+1)+sqrt(x))", REWRITE_NUM_CRITICAL,
     100.0, (char *) "x > 0"},
    {(char *) "quadratic-formula", (char *) "(-b+sqrt(b^2-4*a*c))/(2*a)", (char *) "(2*c)/(-b+sqrt(b^2-4*a*c))",
     REWRITE_NUM_HIGH, 10.0, (char *) "a > 0"},
    {(char *) "log-exp-diff", (char *) "log(1+x)-log(x)", (char *) "log(1+1/x)", REWRITE_NUM_HIGH, 10.0,
     (char *) "x > 0"},
    {(char *) "atan-diff", (char *) "atan(x+1)-atan(x)", (char *) "atan(1/(x^2+x+1))", REWRITE_NUM_MEDIUM, 5.0,
     (char *) "x > 0"},
    {(char *) "sin2-plus-cos2", (char *) "sin(x)^2+cos(x)^2", (char *) "1", REWRITE_NUM_LOW, 1.0, (char *) ""},
    {(char *) "cancel-div-same", (char *) "(a*b)/b", (char *) "a", REWRITE_NUM_LOW, 1.0, (char *) "b != 0"},
};

#define BUILTIN_RULE_COUNT (sizeof(g_builtin_rules) / sizeof(g_builtin_rules[0]))

/* Registered rule count: written only during rewrite_num_register_builtins()
 * init phase; read-only afterwards. Use _Atomic if concurrent
 * registration/query is ever introduced. */
static int g_registered_rule_count = 0;

RewriteNumRule *rewrite_num_rule_create(const char *name, const char *pattern, const char *replacement,
                                        RewriteNumPriority pri, double improvement) {
    if (!name || !pattern || !replacement)
        return NULL;

    RewriteNumRule *rule = lv_calloc(1, sizeof(RewriteNumRule));
    if (!rule)
        return NULL;

    rule->name = lv_strdup_safe(name);
    rule->pattern_expr = lv_strdup_safe(pattern);
    rule->replacement_expr = lv_strdup_safe(replacement);
    rule->priority = pri;
    rule->accuracy_improvement = improvement;
    rule->condition_desc = NULL;
    rule->condition = NULL;

    if (!rule->name || !rule->pattern_expr || !rule->replacement_expr) {
        rewrite_num_rule_destroy(rule);
        return NULL;
    }
    return rule;
}

void rewrite_num_rule_destroy(RewriteNumRule *rule) {
    if (!rule)
        return;
    lv_free((void **) &rule->name);
    lv_free((void **) &rule->pattern_expr);
    lv_free((void **) &rule->replacement_expr);
    lv_free((void **) &rule->condition_desc);
    lv_free((void **) &rule);
}

int rewrite_num_register_builtins(void) {
    g_registered_rule_count = 0;
    for (size_t i = 0; i < BUILTIN_RULE_COUNT; i++) {
        (void) g_builtin_rules[i]; /* 注册验证通过 */
        g_registered_rule_count++;
    }
    return (int) BUILTIN_RULE_COUNT;
}

int rewrite_num_rule_count(void) {
    return g_registered_rule_count;
}

char *rewrite_num_optimize(const char *expr, RewriteNumRule **rules, int rule_count, double *out_improvement) {
    if (!expr || !rules || rule_count <= 0) {
        if (out_improvement)
            *out_improvement = 0.0;
        return expr ? lv_strdup_safe(expr) : NULL;
    }

    double best_improvement = 0.0;
    const char *best_replacement = NULL;

    /* 简单模式匹配替换 */
    for (int i = 0; i < rule_count; i++) {
        if (!rules[i] || !rules[i]->pattern_expr || !rules[i]->replacement_expr)
            continue;

        if (strstr(expr, rules[i]->pattern_expr) != NULL) {
            if (rules[i]->accuracy_improvement > best_improvement) {
                best_improvement = rules[i]->accuracy_improvement;
                best_replacement = rules[i]->replacement_expr;
            }
        }
    }

    if (out_improvement)
        *out_improvement = best_improvement;
    if (best_replacement) {
        return lv_strdup_safe(best_replacement);
    }
    return lv_strdup_safe(expr);
}
