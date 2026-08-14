/**
 * @file proof_rule_engine.c
 * @brief Proof rule search engine implementation
 *
 * Implements best-first proof search with weighted rule prioritization.
 * Supports depth limits and timeout control.
 *
 * Search algorithm (best-first):
 *   1. Collect all rules applicable to the current proof state.
 *   2. Sort applicable rules by weight in descending order.
 *   3. Apply the highest-weighted rule.
 *   4. Recurse on the resulting proof state.
 *   5. Backtrack if no rule leads to a solution within depth limit.
 *
 * For other strategies:
 *   - Depth-first: apply first applicable rule, recurse immediately.
 *   - Breadth-first: expand all applicable rules at current depth before going deeper.
 *   - Iterative deepening: run depth-first with increasing depth limits.
 */

#include "lv/proof_rule_engine.h"
#include "proof_rule_engine_internal.h"
#include "lv/lv_xmacro.h"

#include <stdlib.h>
#include <string.h>

#include "lv/circuit_breaker.h"
#include "lv/lv.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_utils.h"

/* 数值验证策略公共入口（proof_strategy_numeric.c：区间算术求值 + FPTaylor 误差界分级）：
 * 本文件的数值验证规则通过这两个公共 API 复用其提取/求值/TrustColor 能力，
 * 不在本文件复制解析逻辑。 */
#include "proof_multi_strategy_internal.h"

/* ============== Internal Helpers ============== */

/**
 * @brief 检查搜索是否已超时
 *
 * @param engine     规则引擎（包含 timeout_ms 配置）
 * @param start_time_us 搜索开始时间（微秒，由 lv_circuit_breaker_now_us() 返回）
 * @return true 如果已超时，false 否则
 */
static bool is_search_timed_out(const lvRuleEngine *engine, uint64_t start_time_us) {
    if (engine->timeout_ms == 0)
        return false;
    uint64_t elapsed_us = lv_circuit_breaker_now_us() - start_time_us;
    return elapsed_us >= (uint64_t) engine->timeout_ms * lv_US_PER_MS;
}



/**
 * @brief 按权重降序比较两条规则（大权重优先）
 */
static int cmp_rule_weight_desc(const void *a, const void *b, void *ctx) {
    (void) ctx;
    const lvProofRule *ra = *(const lvProofRule *const *) a;
    const lvProofRule *rb = *(const lvProofRule *const *) b;
    if (ra->weight > rb->weight)
        return -1;
    if (ra->weight < rb->weight)
        return 1;
    return 0;
}

/**
 * @brief Sort rules by weight in descending order (insertion sort)
 *
 * Uses insertion sort since the rule set is typically small.
 */
static void sort_rules_by_weight(lvProofRule **rules, int count) {
    lv_insertion_sort(rules, (size_t) count, sizeof(lvProofRule *), cmp_rule_weight_desc, NULL);
}

/**
 * @brief Collect applicable rules from the engine into a buffer
 *
 * @param engine  Rule engine
 * @param state   Current proof state
 * @param out     Output buffer for applicable rules
 * @param max_out Maximum number of rules to collect
 * @return Number of applicable rules found
 */
static int collect_applicable_rules(const lvRuleEngine *engine, const lvProofState *state, lvProofRule **out,
                                    int max_out) {
    int count = 0;
    int i;
    if (!engine || !state || !out || max_out <= 0)
        return 0;

    for (i = 0; i < engine->rule_count && count < max_out; i++) {
        lvProofRule *rule = engine->rule_set[i];
        if (rule && rule->applicability_check_fn) {
            if (rule->applicability_check_fn(rule, state)) {
                out[count++] = rule;
            }
        }
    }
    return count;
}

/**
 * @brief Internal recursive search for best-first strategy
 */
static lvSearchResultStatus search_best_first(lvRuleEngine *engine, lvProofState *state, int depth,
                                              uint64_t start_time_us) {
    lvProofRule *applicable[lv_RULE_SET_CAPACITY];
    int count, i;

    /* 超时检查 */
    if (is_search_timed_out(engine, start_time_us)) {
        return SEARCH_RESULT_TIMEOUT;
    }

    /* Check depth limit */
    if (depth >= engine->max_depth) {
        return SEARCH_RESULT_DEPTH_LIMIT;
    }

    /* Check if proof is complete */
    if (proof_state_is_complete(state)) {
        return SEARCH_RESULT_FOUND;
    }

    /* Collect applicable rules */
    count = collect_applicable_rules(engine, state, applicable, lv_RULE_SET_CAPACITY);
    if (count == 0) {
        return SEARCH_RESULT_EXHAUSTED;
    }

    /* Sort by weight (descending) for best-first ordering */
    sort_rules_by_weight(applicable, count);

    /* Try each rule in weight order */
    bool hit_depth_limit = false;
    for (i = 0; i < count; i++) {
        lvProofRule *rule = applicable[i];

        /* Save state for backtracking */
        int saved_depth = state->current_depth;
        int saved_goal_top = state->goal_stack_top;
        int saved_hyp_count = state->hypothesis_count;
        int saved_rule_count = state->applied_rule_count;

        /* Apply the rule */
        if (rule->apply_fn && rule->apply_fn(rule, state)) {
            state->current_depth = depth + 1;

            /* Record the applied rule */
            proof_state_record_rule(state, rule->name);

            /* Recurse or finish immediately when the rule closes all goals. */
            lvSearchResultStatus result = proof_state_is_complete(state)
                                              ? SEARCH_RESULT_FOUND
                                              : search_best_first(engine, state, depth + 1, start_time_us);
            if (result == SEARCH_RESULT_FOUND) {
                state->current_depth = saved_depth;
                return SEARCH_RESULT_FOUND;
            }

            /* 超时结果需要向上传播，不进行回溯 */
            if (result == SEARCH_RESULT_TIMEOUT) {
                return SEARCH_RESULT_TIMEOUT;
            }

            /* Track if any rule hit depth limit */
            if (result == SEARCH_RESULT_DEPTH_LIMIT) {
                hit_depth_limit = true;
            }

            /* Backtrack: restore goal stack, hypotheses, and rule history */
            /* Free any goals/hypotheses that were added during the failed branch */
            while (state->goal_stack_top > saved_goal_top) {
                if (state->goal_stack[state->goal_stack_top] != NULL) {
                    lv_free((void **) &state->goal_stack[state->goal_stack_top]);
                    state->goal_stack[state->goal_stack_top] = NULL;
                }
                state->goal_stack_top--;
            }
            while (state->hypothesis_count > saved_hyp_count) {
                state->hypothesis_count--;
                if (state->hypotheses[state->hypothesis_count] != NULL) {
                    lv_free((void **) &state->hypotheses[state->hypothesis_count]);
                    state->hypotheses[state->hypothesis_count] = NULL;
                }
            }
            while (state->applied_rule_count > saved_rule_count) {
                state->applied_rule_count--;
                if (state->applied_rules[state->applied_rule_count] != NULL) {
                    lv_free((void **) &state->applied_rules[state->applied_rule_count]);
                    state->applied_rules[state->applied_rule_count] = NULL;
                }
            }
            state->current_depth = saved_depth;
        }
    }

    return hit_depth_limit ? SEARCH_RESULT_DEPTH_LIMIT : SEARCH_RESULT_EXHAUSTED;
}

/**
 * @brief Internal recursive search for depth-first strategy
 */
static lvSearchResultStatus search_depth_first(lvRuleEngine *engine, lvProofState *state, int depth,
                                               uint64_t start_time_us) {
    lvProofRule *applicable[lv_RULE_SET_CAPACITY];
    int count, i;

    /* 超时检查 */
    if (is_search_timed_out(engine, start_time_us)) {
        return SEARCH_RESULT_TIMEOUT;
    }

    if (depth >= engine->max_depth) {
        return SEARCH_RESULT_DEPTH_LIMIT;
    }

    if (proof_state_is_complete(state)) {
        return SEARCH_RESULT_FOUND;
    }

    count = collect_applicable_rules(engine, state, applicable, lv_RULE_SET_CAPACITY);
    if (count == 0) {
        return SEARCH_RESULT_EXHAUSTED;
    }

    bool hit_depth_limit = false;
    for (i = 0; i < count; i++) {
        lvProofRule *rule = applicable[i];
        int saved_depth = state->current_depth;
        int saved_goal_top = state->goal_stack_top;
        int saved_hyp_count = state->hypothesis_count;
        int saved_rule_count = state->applied_rule_count;

        if (rule->apply_fn && rule->apply_fn(rule, state)) {
            state->current_depth = depth + 1;
            proof_state_record_rule(state, rule->name);

            lvSearchResultStatus result = proof_state_is_complete(state)
                                              ? SEARCH_RESULT_FOUND
                                              : search_depth_first(engine, state, depth + 1, start_time_us);
            if (result == SEARCH_RESULT_FOUND) {
                state->current_depth = saved_depth;
                return SEARCH_RESULT_FOUND;
            }

            /* 超时结果需要向上传播，不进行回溯 */
            if (result == SEARCH_RESULT_TIMEOUT) {
                return SEARCH_RESULT_TIMEOUT;
            }

            /* Track if any rule hit depth limit */
            if (result == SEARCH_RESULT_DEPTH_LIMIT) {
                hit_depth_limit = true;
            }

            /* Backtrack */
            while (state->goal_stack_top > saved_goal_top) {
                if (state->goal_stack[state->goal_stack_top] != NULL) {
                    lv_free((void **) &state->goal_stack[state->goal_stack_top]);
                    state->goal_stack[state->goal_stack_top] = NULL;
                }
                state->goal_stack_top--;
            }
            while (state->hypothesis_count > saved_hyp_count) {
                state->hypothesis_count--;
                if (state->hypotheses[state->hypothesis_count] != NULL) {
                    lv_free((void **) &state->hypotheses[state->hypothesis_count]);
                    state->hypotheses[state->hypothesis_count] = NULL;
                }
            }
            while (state->applied_rule_count > saved_rule_count) {
                state->applied_rule_count--;
                if (state->applied_rules[state->applied_rule_count] != NULL) {
                    lv_free((void **) &state->applied_rules[state->applied_rule_count]);
                    state->applied_rules[state->applied_rule_count] = NULL;
                }
            }
            state->current_depth = saved_depth;
        }
    }

    return hit_depth_limit ? SEARCH_RESULT_DEPTH_LIMIT : SEARCH_RESULT_EXHAUSTED;
}

/**
 * @brief BFS 队列节点，用于存储待搜索的证明状态快照
 */
typedef struct BfsQueueNode {
    lvProofState *state;       /* 证明状态副本 */
    int depth;                 /* 当前深度 */
    struct BfsQueueNode *next; /* 队列链表下一节点 */
} BfsQueueNode;

/**
 * @brief 创建 BFS 队列节点
 */
static BfsQueueNode *bfs_queue_node_create(lvProofState *state, int depth) {
    BfsQueueNode *node = (BfsQueueNode *) lv_calloc(1, sizeof(BfsQueueNode));
    if (!node)
        return NULL;
    node->state = state;
    node->depth = depth;
    node->next = NULL;
    return node;
}

/**
 * @brief BFS 队列结构（简单的链表队列）
 */
typedef struct {
    BfsQueueNode *front; /* 队首 */
    BfsQueueNode *rear;  /* 队尾 */
    int size;            /* 队列大小 */
} BfsQueue;

/**
 * @brief 初始化 BFS 队列
 */
static void bfs_queue_init(BfsQueue *q) {
    q->front = NULL;
    q->rear = NULL;
    q->size = 0;
}

/**
 * @brief 入队（将状态加入队尾）
 */
static bool bfs_queue_enqueue(BfsQueue *q, lvProofState *state, int depth) {
    BfsQueueNode *node = bfs_queue_node_create(state, depth);
    if (!node)
        return false;
    if (q->rear) {
        q->rear->next = node;
    } else {
        q->front = node;
    }
    q->rear = node;
    q->size++;
    return true;
}

/**
 * @brief 出队（从队首取出状态）
 */
static BfsQueueNode *bfs_queue_dequeue(BfsQueue *q) {
    if (!q->front)
        return NULL;
    BfsQueueNode *node = q->front;
    q->front = node->next;
    if (!q->front) {
        q->rear = NULL;
    }
    q->size--;
    return node;
}

/**
 * @brief 释放 BFS 队列中所有节点及其状态
 */
static void bfs_queue_clear(BfsQueue *q) {
    while (q->front) {
        BfsQueueNode *node = bfs_queue_dequeue(q);
        if (node) {
            proof_state_destroy(node->state);
            lv_free((void **) &node);
        }
    }
}

/**
 * @brief 深拷贝证明状态（用于 BFS 队列中的状态分支）
 */
static lvProofState *proof_state_clone(const lvProofState *src) {
    int i;
    if (!src)
        return NULL;

    lvProofState *dst = (lvProofState *) lv_calloc(1, sizeof(lvProofState));
    if (!dst)
        return NULL;

    /* 复制目标栈（动态扩容，消除 lv_GOAL_STACK_MAX 定长上限） */
    if (src->goal_stack_top >= 0) {
        if (!lv_ensure_capacity((void **) &dst->goal_stack, src->goal_stack_top + 1,
                                &dst->goal_stack_capacity, sizeof(char *), 0)) {
            lv_free((void **) &dst->goal_stack);
            lv_free((void **) &dst);
            return NULL;
        }
        for (i = 0; i <= src->goal_stack_top; i++) {
            if (src->goal_stack[i]) {
                dst->goal_stack[i] = lv_strdup_safe(src->goal_stack[i]);
            }
        }
    }
    dst->goal_stack_top = src->goal_stack_top;
    if (dst->goal_stack_top >= 0) {
        dst->current_goal = dst->goal_stack[dst->goal_stack_top];
    }

    /* 复制假设 */
    for (i = 0; i < src->hypothesis_count && i < lv_HYPOTHESIS_MAX; i++) {
        if (src->hypotheses[i]) {
            dst->hypotheses[i] = lv_strdup_safe(src->hypotheses[i]);
        }
    }
    dst->hypothesis_count = src->hypothesis_count;

    /* 复制已应用规则历史 */
    for (i = 0; i < src->applied_rule_count && i < lv_APPLIED_RULES_MAX; i++) {
        if (src->applied_rules[i]) {
            dst->applied_rules[i] = lv_strdup_safe(src->applied_rules[i]);
        }
    }
    dst->applied_rule_count = src->applied_rule_count;
    dst->current_depth = src->current_depth;

    return dst;
}

/**
 * @brief Internal search for breadth-first strategy (真正的 BFS，使用队列)
 *
 * 使用队列实现广度优先搜索：先扩展当前深度的所有状态，
 * 再进入下一深度。每一层展开所有适用规则的所有可能分支。
 *
 * exempt: 判据「BFS 图遍历收敛」——本函数为带证明状态快照 + 副作用执行
 * （rule->apply_fn 改写 lvProofState）的状态空间搜索，非 lv_bfs_run 的
 * 整型 id 图遍历语义，保留。
 */
static lvSearchResultStatus search_breadth_first(lvRuleEngine *engine, lvProofState *initial_state,
                                                 uint64_t start_time_us) {
    BfsQueue queue;
    bfs_queue_init(&queue);

    /* 将初始状态入队 */
    lvProofState *initial_clone = proof_state_clone(initial_state);
    if (!initial_clone) {
        return SEARCH_RESULT_ERROR;
    }
    if (!bfs_queue_enqueue(&queue, initial_clone, 0)) {
        proof_state_destroy(initial_clone);
        return SEARCH_RESULT_ERROR;
    }

    lvSearchResultStatus final_result = SEARCH_RESULT_EXHAUSTED;

    while (queue.front != NULL) {
        /* 超时检查 */
        if (is_search_timed_out(engine, start_time_us)) {
            bfs_queue_clear(&queue);
            return SEARCH_RESULT_TIMEOUT;
        }

        /* 出队当前状态 */
        BfsQueueNode *current_node = bfs_queue_dequeue(&queue);
        lvProofState *current_state = current_node->state;
        int current_depth = current_node->depth;
        lv_free((void **) &current_node);

        /* 深度限制检查 */
        if (current_depth >= engine->max_depth) {
            proof_state_destroy(current_state);
            final_result = SEARCH_RESULT_DEPTH_LIMIT;
            continue;
        }

        /* 检查是否已完成证明 */
        if (proof_state_is_complete(current_state)) {
            /* 将成功状态复制回初始状态 */
            /* 清空初始状态的目标栈 */
            while (initial_state->goal_stack_top >= 0) {
                if (initial_state->goal_stack[initial_state->goal_stack_top] != NULL) {
                    lv_free((void **) &initial_state->goal_stack[initial_state->goal_stack_top]);
                    initial_state->goal_stack[initial_state->goal_stack_top] = NULL;
                }
                initial_state->goal_stack_top--;
            }
            /* 清空初始状态的假设 */
            for (int h = 0; h < initial_state->hypothesis_count; h++) {
                if (initial_state->hypotheses[h] != NULL) {
                    lv_free((void **) &initial_state->hypotheses[h]);
                    initial_state->hypotheses[h] = NULL;
                }
            }
            /* 清空初始状态的已应用规则 */
            for (int r = 0; r < initial_state->applied_rule_count; r++) {
                if (initial_state->applied_rules[r] != NULL) {
                    lv_free((void **) &initial_state->applied_rules[r]);
                    initial_state->applied_rules[r] = NULL;
                }
            }

            /* 从成功状态复制数据到初始状态 */
            if (current_state->goal_stack_top >= 0) {
                if (!lv_ensure_capacity((void **) &initial_state->goal_stack,
                                        current_state->goal_stack_top + 1,
                                        &initial_state->goal_stack_capacity, sizeof(char *), 0)) {
                    proof_state_destroy(current_state);
                    bfs_queue_clear(&queue);
                    return SEARCH_RESULT_ERROR;
                }
            }
            for (int g = 0; g <= current_state->goal_stack_top; g++) {
                if (current_state->goal_stack[g]) {
                    initial_state->goal_stack[g] = lv_strdup_safe(current_state->goal_stack[g]);
                }
            }
            initial_state->goal_stack_top = current_state->goal_stack_top;
            if (initial_state->goal_stack_top >= 0) {
                initial_state->current_goal = initial_state->goal_stack[initial_state->goal_stack_top];
            } else {
                initial_state->current_goal = NULL;
            }
            for (int h = 0; h < current_state->hypothesis_count && h < lv_HYPOTHESIS_MAX; h++) {
                if (current_state->hypotheses[h]) {
                    initial_state->hypotheses[h] = lv_strdup_safe(current_state->hypotheses[h]);
                }
            }
            initial_state->hypothesis_count = current_state->hypothesis_count;
            for (int r = 0; r < current_state->applied_rule_count && r < lv_APPLIED_RULES_MAX; r++) {
                if (current_state->applied_rules[r]) {
                    initial_state->applied_rules[r] = lv_strdup_safe(current_state->applied_rules[r]);
                }
            }
            initial_state->applied_rule_count = current_state->applied_rule_count;
            initial_state->current_depth = current_state->current_depth;

            proof_state_destroy(current_state);
            bfs_queue_clear(&queue);
            return SEARCH_RESULT_FOUND;
        }

        /* 收集当前状态的所有适用规则 */
        lvProofRule *applicable[lv_RULE_SET_CAPACITY];
        int count = collect_applicable_rules(engine, current_state, applicable, lv_RULE_SET_CAPACITY);

        if (count == 0) {
            /* 当前状态无适用规则，丢弃 */
            proof_state_destroy(current_state);
            continue;
        }

        /* 对每个适用规则，克隆状态并应用规则，然后入队 */
        for (int i = 0; i < count; i++) {
            lvProofRule *rule = applicable[i];
            if (!rule->apply_fn)
                continue;

            /* 克隆当前状态 */
            lvProofState *child_state = proof_state_clone(current_state);
            if (!child_state)
                continue;

            /* 在克隆状态上应用规则 */
            if (rule->apply_fn(rule, child_state)) {
                child_state->current_depth = current_depth + 1;
                proof_state_record_rule(child_state, rule->name);

                /* 入队子状态 */
                if (!bfs_queue_enqueue(&queue, child_state, current_depth + 1)) {
                    proof_state_destroy(child_state);
                }
            } else {
                /* 规则应用失败，丢弃克隆状态 */
                proof_state_destroy(child_state);
            }
        }

        /* 释放当前状态（子状态已入队） */
        proof_state_destroy(current_state);
    }

    return final_result;
}

/**
 * @brief Internal search for iterative deepening strategy
 */
static lvSearchResultStatus search_iterative_deepening(lvRuleEngine *engine, lvProofState *state,
                                                       uint64_t start_time_us) {
    int depth_limit;
    for (depth_limit = 1; depth_limit <= engine->max_depth; depth_limit++) {
        /* 超时检查 */
        if (is_search_timed_out(engine, start_time_us)) {
            return SEARCH_RESULT_TIMEOUT;
        }

        int saved_max_depth = engine->max_depth;
        engine->max_depth = depth_limit;

        lvSearchResultStatus result = search_depth_first(engine, state, 0, start_time_us);

        engine->max_depth = saved_max_depth;

        if (result == SEARCH_RESULT_FOUND) {
            return SEARCH_RESULT_FOUND;
        }
        /* 超时结果需要向上传播 */
        if (result == SEARCH_RESULT_TIMEOUT) {
            return SEARCH_RESULT_TIMEOUT;
        }
    }
    return SEARCH_RESULT_DEPTH_LIMIT;
}

/* ============== Rule Engine API Implementation ============== */

lvRuleEngine *rule_engine_create(void) {
    return rule_engine_create_ex(SEARCH_BEST_FIRST, lv_DEFAULT_MAX_DEPTH, lv_DEFAULT_SEARCH_TIMEOUT_MS);
}

lvRuleEngine *rule_engine_create_ex(lvSearchStrategy strategy, int max_depth, uint64_t timeout_ms) {
    lvRuleEngine *engine = (lvRuleEngine *) lv_calloc(1, sizeof(lvRuleEngine));
    if (!engine)
        return NULL;

    engine->rule_set = (lvProofRule **) lv_malloc(sizeof(lvProofRule *) * lv_RULE_SET_CAPACITY);
    if (!engine->rule_set) {
        lv_free((void **) &engine);
        return NULL;
    }
    memset(engine->rule_set, 0, sizeof(lvProofRule *) * lv_RULE_SET_CAPACITY);

    engine->rule_count = 0;
    engine->rule_capacity = lv_RULE_SET_CAPACITY;
    engine->search_strategy = strategy;
    engine->max_depth = (max_depth > 0) ? max_depth : lv_DEFAULT_MAX_DEPTH;
    engine->timeout_ms = timeout_ms;

    return engine;
}

void rule_engine_destroy(lvRuleEngine *engine) {
    if (!engine)
        return;

    lv_free_ptr_array((void ***) &engine->rule_set, (size_t) engine->rule_count);

    lv_free((void **) &engine);
}

bool rule_engine_add_rule(lvRuleEngine *engine, lvProofRule *rule) {
    if (!engine || !rule)
        return false;
    if (engine->rule_count >= engine->rule_capacity)
        return false;

    engine->rule_set[engine->rule_count++] = rule;
    return true;
}

bool rule_engine_remove_rule(lvRuleEngine *engine, const char *name) {
    int i;
    if (!engine || !name)
        return false;

    for (i = 0; i < engine->rule_count; i++) {
        if (engine->rule_set[i] && strncmp(engine->rule_set[i]->name, name, lv_PROOF_RULE_NAME_MAX) == 0) {
            lv_free((void **) &engine->rule_set[i]);
            /* Shift remaining rules */
            int j;
            for (j = i; j < engine->rule_count - 1; j++) {
                engine->rule_set[j] = engine->rule_set[j + 1];
            }
            engine->rule_set[engine->rule_count - 1] = NULL;
            engine->rule_count--;
            return true;
        }
    }
    return false;
}

const lvProofRule *rule_engine_find_rule(const lvRuleEngine *engine, const char *name) {
    int i;
    if (!engine || !name)
        return NULL;

    for (i = 0; i < engine->rule_count; i++) {
        if (engine->rule_set[i] && strncmp(engine->rule_set[i]->name, name, lv_PROOF_RULE_NAME_MAX) == 0) {
            return engine->rule_set[i];
        }
    }
    return NULL;
}

/* ============== Search Strategy VTable ============== */

/**
 * @brief VTable for search strategy dispatch
 */
typedef struct {
    lvSearchResultStatus (*search)(lvRuleEngine *engine, lvProofState *state, uint64_t start_time_us);
} SearchStrategyVTable;

/**
 * @brief Wrapper for best-first search (adds depth=0 parameter)
 */
static lvSearchResultStatus search_best_first_wrap(lvRuleEngine *engine, lvProofState *state, uint64_t start_time_us) {
    return search_best_first(engine, state, 0, start_time_us);
}

/**
 * @brief Wrapper for depth-first search (adds depth=0 parameter)
 */
static lvSearchResultStatus search_depth_first_wrap(lvRuleEngine *engine, lvProofState *state, uint64_t start_time_us) {
    return search_depth_first(engine, state, 0, start_time_us);
}

/**
 * @brief Wrapper for breadth-first search
 */
static lvSearchResultStatus search_breadth_first_wrap(lvRuleEngine *engine, lvProofState *state, uint64_t start_time_us) {
    return search_breadth_first(engine, state, start_time_us);
}

/**
 * @brief Wrapper for iterative deepening search
 */
static lvSearchResultStatus search_iterative_deepening_wrap(lvRuleEngine *engine, lvProofState *state, uint64_t start_time_us) {
    return search_iterative_deepening(engine, state, start_time_us);
}

/**
 * @brief VTable array indexed by lvSearchStrategy enum
 */
static const SearchStrategyVTable kSearchStrategyVTables[] = {
    { search_best_first_wrap },
    { search_depth_first_wrap },
    { search_breadth_first_wrap },
    { search_iterative_deepening_wrap },
};

lvSearchResultStatus rule_engine_search(lvRuleEngine *engine, lvProofState *state) {
    if (!engine || !state)
        return SEARCH_RESULT_ERROR;

    /* Check if already complete */
    if (proof_state_is_complete(state)) {
        return SEARCH_RESULT_FOUND;
    }

    /* 记录搜索开始时间（用于超时检查，使用高精度微秒级计时） */
    uint64_t start_time_us = lv_circuit_breaker_now_us();

    /* Dispatch to strategy-specific search via VTable */
    if (engine->search_strategy >= 0 && (size_t)engine->search_strategy < sizeof(kSearchStrategyVTables) / sizeof(kSearchStrategyVTables[0])) {
        return kSearchStrategyVTables[engine->search_strategy].search(engine, state, start_time_us);
    }
    return SEARCH_RESULT_ERROR;
}

int rule_engine_rule_count(const lvRuleEngine *engine) {
    if (!engine)
        return -1;
    return engine->rule_count;
}

/* ============== Proof State API Implementation ============== */

lvProofState *proof_state_create(const char *initial_goal) {
    lvProofState *state;
    if (!initial_goal)
        return NULL;

    state = (lvProofState *) lv_calloc(1, sizeof(lvProofState));
    if (!state)
        return NULL;

    state->goal_stack_top = -1;
    state->goal_stack_capacity = 0; /* 目标栈惰性分配（push 时扩容） */
    state->current_depth = 0;

    /* Push the initial goal */
    if (!proof_state_push_goal(state, initial_goal)) {
        lv_free((void **) &state);
        return NULL;
    }

    return state;
}

void proof_state_destroy(lvProofState *state) {
    int i;
    if (!state)
        return;

    /* Free goal stack entries */
    for (i = 0; i <= state->goal_stack_top; i++) {
        if (state->goal_stack[i]) {
            lv_free((void **) &state->goal_stack[i]);
            state->goal_stack[i] = NULL;
        }
    }
    lv_free((void **) &state->goal_stack);
    state->goal_stack = NULL;
    state->goal_stack_capacity = 0;

    /* Free hypothesis entries */
    for (i = 0; i < state->hypothesis_count && i < lv_HYPOTHESIS_MAX; i++) {
        if (state->hypotheses[i]) {
            lv_free((void **) &state->hypotheses[i]);
            state->hypotheses[i] = NULL;
        }
    }

    /* Free applied rules history */
    for (i = 0; i < state->applied_rule_count && i < lv_APPLIED_RULES_MAX; i++) {
        if (state->applied_rules[i]) {
            lv_free((void **) &state->applied_rules[i]);
            state->applied_rules[i] = NULL;
        }
    }

    state->current_goal = NULL;
    lv_free((void **) &state);
}

bool proof_state_push_goal(lvProofState *state, const char *goal) {
    if (!state || !goal)
        return false;
    if (state->goal_stack_top + 1 >= state->goal_stack_capacity) {
        if (!lv_ensure_capacity((void **) &state->goal_stack, state->goal_stack_top + 1,
                                &state->goal_stack_capacity, sizeof(char *), 0))
            return false;
    }

    state->goal_stack_top++;
    state->goal_stack[state->goal_stack_top] = lv_strdup_safe(goal);
    state->current_goal = state->goal_stack[state->goal_stack_top];

    return state->goal_stack[state->goal_stack_top] != NULL;
}

bool proof_state_pop_goal(lvProofState *state) {
    if (!state)
        return false;
    if (state->goal_stack_top < 0)
        return false;

    if (state->goal_stack[state->goal_stack_top]) {
        lv_free((void **) &state->goal_stack[state->goal_stack_top]);
        state->goal_stack[state->goal_stack_top] = NULL;
    }
    state->goal_stack_top--;

    /* Update current_goal pointer */
    if (state->goal_stack_top >= 0) {
        state->current_goal = state->goal_stack[state->goal_stack_top];
    } else {
        state->current_goal = NULL;
    }

    return true;
}

bool proof_state_add_hypothesis(lvProofState *state, const char *hypothesis) {
    if (!state || !hypothesis)
        return false;
    if (state->hypothesis_count >= lv_HYPOTHESIS_MAX)
        return false;

    state->hypotheses[state->hypothesis_count] = lv_strdup_safe(hypothesis);
    if (!state->hypotheses[state->hypothesis_count])
        return false;

    state->hypothesis_count++;
    return true;
}

bool proof_state_record_rule(lvProofState *state, const char *name) {
    if (!state || !name)
        return false;
    if (state->applied_rule_count >= lv_APPLIED_RULES_MAX)
        return false;

    state->applied_rules[state->applied_rule_count] = lv_strdup_safe(name);
    if (!state->applied_rules[state->applied_rule_count])
        return false;

    state->applied_rule_count++;
    return true;
}

bool proof_state_is_complete(const lvProofState *state) {
    if (!state)
        return false;
    return state->goal_stack_top < 0;
}

const char *proof_state_current_goal(const lvProofState *state) {
    if (!state)
        return NULL;
    return state->current_goal;
}

/* ============== Utility Functions ============== */

/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch）
 * ================================================================ */

/** @brief proof_rule_type_to_string 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_proof_rule_type_to_string_entries[] = {
    {"INTRO", RULE_INTRO},
    {"ELIM", RULE_ELIM},
    {"REWRITE", RULE_REWRITE},
    {"INDUCTION", RULE_INDUCTION},
    {"CONTRADICTION", RULE_CONTRADICTION},
    {"CASE_SPLIT", RULE_CASE_SPLIT},
    {"GENERALIZE", RULE_GENERALIZE},
    {"SPECIALIZE", RULE_SPECIALIZE},
    {"NEURAL_SUGGEST", RULE_NEURAL_SUGGEST},
    {"AUX_CONSTRUCT", RULE_AUX_CONSTRUCT},
};

const char *proof_rule_type_to_string(lvProofRuleType type) {
    return lv_enum_to_str(s_proof_rule_type_to_string_entries, lv_ARRAY_SIZE(s_proof_rule_type_to_string_entries), (int) type, "UNKNOWN");
}

/** @brief search_strategy_to_string 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_search_strategy_to_string_entries[] = {
    {"BEST_FIRST", SEARCH_BEST_FIRST},
    {"DEPTH_FIRST", SEARCH_DEPTH_FIRST},
    {"BREADTH_FIRST", SEARCH_BREADTH_FIRST},
    {"ITERATIVE_DEEPENING", SEARCH_ITERATIVE_DEEPENING},
};

const char *search_strategy_to_string(lvSearchStrategy strategy) {
    return lv_enum_to_str(s_search_strategy_to_string_entries, lv_ARRAY_SIZE(s_search_strategy_to_string_entries), (int) strategy, "UNKNOWN");
}

/** @brief search_result_status_to_string 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_search_result_status_to_string_entries[] = {
    {"FOUND", SEARCH_RESULT_FOUND},
    {"TIMEOUT", SEARCH_RESULT_TIMEOUT},
    {"DEPTH_LIMIT", SEARCH_RESULT_DEPTH_LIMIT},
    {"EXHAUSTED", SEARCH_RESULT_EXHAUSTED},
    {"ERROR", SEARCH_RESULT_ERROR},
};

const char *search_result_status_to_string(lvSearchResultStatus status) {
    return lv_enum_to_str(s_search_result_status_to_string_entries, lv_ARRAY_SIZE(s_search_result_status_to_string_entries), (int) status, "UNKNOWN");
}

/* ============================================================
 * 数值验证规则（lv_PROOF_RULE_NAME_NUMERIC_VERIFICATION）
 *
 * 将 PROOF_STRATEGY_NUMERIC_VERIFICATION（proof_strategy_numeric.c：
 * 常量表达式提取 + lv_interval_* 区间求值 + fptaylor_verify_safety
 * TrustColor 分级）接入规则引擎，使区间验证成为可用规则：
 *   - 适用性：当前目标为「仅含实数常量的比较表达式」（复用
 *     numeric_verification_applicability_check；规则引擎状态是字符串目标，
 *     故构造仅填充 name 的临时 Proposition 承载目标文本）；
 *   - 应用：复用 execute_numeric_verification 完成区间求值与 TrustColor
 *     判定，成立则弹出该目标（目标被解决），返回 true。
 *
 * 复用方式（最小侵入）：proof_strategy_numeric.c 的提取/解析函数均为 static
 * 且该文件只读，故不复制解析逻辑，改经其公共 API 重新调用。execute 需要
 * ProofNavigator，这里构造仅填充 target_prop 的临时导航器（execute 只使用
 * target_prop + proof_navigator_add_step + proof_navigator_set_strategy_note，
 * 其余字段零初始化即可），调用后手动释放其持有的步骤与策略注释。
 * ============================================================ */

static bool rule_numeric_verification_applicable(const void *rule, const void *state) {
    const lvProofState *ps;
    Proposition tmp_prop;
    (void) rule;

    ps = (const lvProofState *) state;
    if (!ps || !ps->current_goal)
        return false;

    /* 临时命题：仅承载目标文本（extract_claim 只读 name/label/description） */
    memset(&tmp_prop, 0, sizeof(tmp_prop));
    tmp_prop.name = ps->current_goal;

    return numeric_verification_applicability_check(NULL, NULL, &tmp_prop);
}

static bool rule_numeric_verification_apply(void *rule, void *state) {
    lvProofState *ps;
    Proposition tmp_prop;
    ProofNavigator tmp_nav;
    bool ok;
    int i;
    (void) rule;

    ps = (lvProofState *) state;
    if (!ps || !ps->current_goal)
        return false;

    memset(&tmp_prop, 0, sizeof(tmp_prop));
    tmp_prop.name = ps->current_goal;

    memset(&tmp_nav, 0, sizeof(tmp_nav));
    tmp_nav.target_prop = &tmp_prop;

    ok = execute_numeric_verification(NULL, &tmp_nav);

    /* 释放临时导航器持有的资源（成功/失败路径都可能已 add 步骤） */
    if (tmp_nav.steps) {
        for (i = 0; i < tmp_nav.step_count; i++) {
            proof_step_destroy(tmp_nav.steps[i]);
        }
        lv_free((void **) &tmp_nav.steps);
    }
    lv_free((void **) &tmp_nav.strategy_note);

    if (ok) {
        proof_state_pop_goal(ps); /* 目标经数值验证解决 */
        return true;
    }
    return false;
}

lvProofRule *lv_proof_rule_numeric_verification_create(void) {
    lvProofRule *rule = (lvProofRule *) lv_calloc(1, sizeof(lvProofRule));
    if (!rule)
        return NULL;

    lv_strlcpy(rule->name, lv_PROOF_RULE_NAME_NUMERIC_VERIFICATION, lv_PROOF_RULE_NAME_MAX);
    rule->weight = 1.0;
    rule->type = RULE_REWRITE; /* 数值求值判定，语义等同重写步骤（策略层对应 PROOF_STEP_REWRITE） */
    rule->priority = 0;
    rule->applicability_check_fn = rule_numeric_verification_applicable;
    rule->apply_fn = rule_numeric_verification_apply;
    return rule;
}

/* ============================================================
 * lv_proof_rule_apply — 公开入口 API
 * ============================================================ */

int lv_proof_rule_apply(const char *rule, const void *input, void **output) {
    if (!rule || !input || !output) {
        return -1;
    }

    /* 创建规则引擎 */
    lvRuleEngine *engine = rule_engine_create();
    if (!engine) {
        return -1;
    }

    /* 从 rule 名称查找内置规则 */
    const lvProofRule *existing = rule_engine_find_rule(engine, rule);
    if (!existing) {
        /* 内置数值验证规则：规则引擎默认无内置规则集（rule_engine_create 为空），
         * 仅当调用方显式点名该规则时才实例化启用，其余行为保持不变 */
        if (lv_str_eq(rule, lv_PROOF_RULE_NAME_NUMERIC_VERIFICATION)) {
            lvProofRule *numeric_rule = lv_proof_rule_numeric_verification_create();
            if (!numeric_rule) {
                rule_engine_destroy(engine);
                return -1;
            }
            if (!rule_engine_add_rule(engine, numeric_rule)) {
                lv_free((void **) &numeric_rule);
                rule_engine_destroy(engine);
                return -1;
            }
        } else {
            /* 用名称作为匹配模式创建临时规则。
             * 必须堆分配：rule_engine_add_rule 后规则由引擎持有，
             * rule_engine_destroy 会逐条 lv_free；若用栈上对象，
             * destroy 将对栈地址调用 free（堆损坏）。 */
            lvProofRule *tmp_rule = (lvProofRule *) lv_calloc(1, sizeof(lvProofRule));
            if (!tmp_rule) {
                rule_engine_destroy(engine);
                return -1;
            }
            lv_strlcpy(tmp_rule->name, rule, lv_PROOF_RULE_NAME_MAX);
            tmp_rule->weight = 1.0;
            tmp_rule->type = RULE_REWRITE;
            tmp_rule->priority = 0;
            tmp_rule->applicability_check_fn = NULL;
            tmp_rule->apply_fn = NULL;

            if (!rule_engine_add_rule(engine, tmp_rule)) {
                lv_free((void **) &tmp_rule);
                rule_engine_destroy(engine);
                return -1;
            }
        }
    }

    /* 创建证明状态 */
    const char *goal_str = (const char *) input;
    lvProofState *state = proof_state_create(goal_str);
    if (!state) {
        rule_engine_destroy(engine);
        return -1;
    }

    /* 执行搜索 */
    lvSearchResultStatus status = rule_engine_search(engine, state);

    if (status == SEARCH_RESULT_FOUND) {
        /* 输出当前目标（证明已找到） */
        const char *result_goal = proof_state_current_goal(state);
        if (result_goal) {
            *output = (void *) result_goal;
        } else {
            *output = NULL;
        }
    } else {
        *output = NULL;
    }

    proof_state_destroy(state);
    rule_engine_destroy(engine);

    return (status == SEARCH_RESULT_FOUND) ? 0 : 1;
}
