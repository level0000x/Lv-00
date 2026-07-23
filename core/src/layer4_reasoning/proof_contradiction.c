/**
 * @file proof_contradiction.c
 * @brief 反证法与矛盾推演系统实现
 *
 * @details 实现局部矛盾闭包机制，包括：
 *          - 假设栈管理
 *          - 矛盾闭包创建和关闭
 *          - 矛盾传播检测
 *          - 断点管理
 *          - 扩展证明导航器
 *
 * @version 4.0.0
 */

#include "lv/proof_contradiction.h"
#include "lv/proof.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================
 * 假设栈实现
 * ============================================================ */

lvAssumptionStack *lv_assumption_stack_create(int capacity) {
    if (capacity <= 0) capacity = 16;

    lvAssumptionStack *stack = lv_calloc(1, sizeof(lvAssumptionStack));
    if (!stack) return NULL;

    stack->entries = lv_calloc((size_t)capacity, sizeof(lvAssumptionEntry));
    if (!stack->entries) {
        lv_free((void **)&stack);
        return NULL;
    }

    stack->capacity = capacity;
    stack->count = 0;
    stack->max_depth = 10;  /* 默认最大嵌套深度 */
    stack->current_scope = 0;

    return stack;
}

void lv_assumption_stack_destroy(lvAssumptionStack *stack) {
    if (!stack) return;

    /* 释放每个条目中的命题 */
    for (int i = 0; i < stack->count; i++) {
        if (stack->entries[i].prop) {
            proposition_destroy(stack->entries[i].prop);
        }
    }

    lv_free((void **)&stack->entries);
    lv_free((void **)&stack);
}

int lv_assumption_stack_push(lvAssumptionStack *stack,
                                lvProofScopeId scope_id,
                                lvAssumptionType type,
                                Proposition *prop) {
    if (!stack || !prop) return -1;

    /* 检查容量 */
    if (stack->count >= stack->capacity) {
        int new_cap = stack->capacity * 2;
        lvAssumptionEntry *new_entries = lv_realloc(
            stack->entries, (size_t)new_cap * sizeof(lvAssumptionEntry));
        if (!new_entries) return -1;
        stack->entries = new_entries;
        stack->capacity = new_cap;
    }

    /* 计算深度 */
    int depth = 0;
    for (int i = stack->count - 1; i >= 0; i--) {
        if (stack->entries[i].scope_id == scope_id) {
            depth = stack->entries[i].depth + 1;
            break;
        }
    }

    /* 检查深度限制 */
    if (depth > stack->max_depth) return -1;

    /* 添加新假设 */
    lvAssumptionEntry *entry = &stack->entries[stack->count];
    entry->assumption_id = stack->count;
    entry->scope_id = scope_id;
    entry->type = type;
    entry->prop = prop;
    entry->depth = depth;
    entry->parent_assumption_id = (stack->count > 0) ? stack->count - 1 : -1;
    entry->derivation_step_count = 0;
    entry->is_contradictory = false;
    entry->timestamp = (int64_t)time(NULL);

    stack->current_scope = scope_id;

    return stack->count++;
}

lvAssumptionEntry *lv_assumption_stack_pop(lvAssumptionStack *stack) {
    if (!stack || stack->count == 0) return NULL;

    return &stack->entries[--stack->count];
}

lvAssumptionEntry *lv_assumption_stack_find(lvAssumptionStack *stack,
                                                  int assumption_id) {
    if (!stack) return NULL;

    for (int i = 0; i < stack->count; i++) {
        if (stack->entries[i].assumption_id == assumption_id) {
            return &stack->entries[i];
        }
    }

    return NULL;
}

int lv_assumption_stack_get_by_scope(lvAssumptionStack *stack,
                                        lvProofScopeId scope_id,
                                        lvAssumptionEntry **out,
                                        int max_out) {
    if (!stack || !out || max_out <= 0) return 0;

    int count = 0;
    for (int i = 0; i < stack->count && count < max_out; i++) {
        if (stack->entries[i].scope_id == scope_id) {
            out[count++] = &stack->entries[i];
        }
    }

    return count;
}

/* ============================================================
 * 矛盾闭包实现
 * ============================================================ */

lvContradictionClosure *lv_contradiction_closure_create(
    lvProofScopeId scope_id,
    lvContradictionType type,
    Proposition *prop) {
    if (!prop) return NULL;

    lvContradictionClosure *closure = lv_calloc(1, sizeof(lvContradictionClosure));
    if (!closure) return NULL;

    static int next_closure_id = 0;
    closure->closure_id = next_closure_id++;
    closure->scope_id = scope_id;
    closure->type = type;
    closure->contradiction_prop = prop;
    closure->is_closed = false;
    closure->created_timestamp = (int64_t)time(NULL);

    return closure;
}

void lv_contradiction_closure_destroy(lvContradictionClosure *closure) {
    if (!closure) return;

    if (closure->contradiction_prop) {
        proposition_destroy(closure->contradiction_prop);
    }
    if (closure->origin_prop) {
        proposition_destroy(closure->origin_prop);
    }

    lv_free((void **)&closure->derivation_path);
    lv_free((void **)&closure);
}

bool lv_contradiction_closure_close(lvContradictionClosure *closure) {
    if (!closure) return false;
    if (closure->is_closed) return false;

    closure->is_closed = true;
    closure->closed_timestamp = (int64_t)time(NULL);

    return true;
}

bool lv_contradiction_propagation_detect(
    lvProofScopeId scope,
    lvContradictionClosure **closures,
    int closure_count) {
    if (!closures || closure_count <= 0) return false;

    /* 检查是否有来自子作用域的矛盾传播到当前作用域 */
    for (int i = 0; i < closure_count; i++) {
        if (closures[i] && closures[i]->scope_id != scope && !closures[i]->is_closed) {
            /* 发现来自其他作用域的未关闭矛盾 */
            return true;
        }
    }

    return false;
}

/* ============================================================
 * 断点实现
 * ============================================================ */

lvContradictionBreakpoint *lv_contradiction_breakpoint_create(
    lvBreakpointType type,
    lvProofScopeId scope_id,
    int step_id,
    const char *description) {
    lvContradictionBreakpoint *bp = lv_calloc(1, sizeof(lvContradictionBreakpoint));
    if (!bp) return NULL;

    static int next_bp_id = 0;
    bp->breakpoint_id = next_bp_id++;
    bp->type = type;
    bp->scope_id = scope_id;
    bp->step_id = step_id;
    bp->depth = 0;
    bp->is_active = true;
    bp->timestamp = (int64_t)time(NULL);

    if (description) {
        bp->description = lv_strdup(description);
    }

    return bp;
}

void lv_contradiction_breakpoint_destroy(lvContradictionBreakpoint *bp) {
    if (!bp) return;
    lv_free((void **)&bp->description);
    lv_free((void **)&bp);
}

int lv_contradiction_detect_breakpoints(
    lvProofNavigatorEx *navigator,
    lvContradictionBreakpoint **breakpoints,
    int max_breakpoints) {
    if (!navigator || !breakpoints || max_breakpoints <= 0) return 0;

    int count = 0;
    int limit = (navigator->breakpoint_count < max_breakpoints) ?
                navigator->breakpoint_count : max_breakpoints;

    for (int i = 0; i < limit && count < max_breakpoints; i++) {
        if (navigator->breakpoints[i] && navigator->breakpoints[i]->is_active) {
            breakpoints[count++] = navigator->breakpoints[i];
        }
    }

    return count;
}

/* ============================================================
 * 扩展证明导航器实现
 * ============================================================ */

lvProofNavigatorEx *lv_proof_navigator_ex_create(void) {
    lvProofNavigatorEx *nav = lv_calloc(1, sizeof(lvProofNavigatorEx));
    if (!nav) return NULL;

    /* 创建假设栈 */
    nav->assumption_stack = lv_assumption_stack_create(16);
    if (!nav->assumption_stack) {
        lv_free((void **)&nav);
        return NULL;
    }

    /* 初始化闭包数组 */
    nav->closure_capacity = 16;
    nav->closures = lv_calloc((size_t)nav->closure_capacity,
                                 sizeof(lvContradictionClosure *));
    if (!nav->closures) {
        lv_assumption_stack_destroy(nav->assumption_stack);
        lv_free((void **)&nav);
        return NULL;
    }

    /* 初始化断点数组 */
    nav->breakpoint_capacity = 16;
    nav->breakpoints = lv_calloc((size_t)nav->breakpoint_capacity,
                                    sizeof(lvContradictionBreakpoint *));
    if (!nav->breakpoints) {
        lv_free((void **)&nav->closures);
        lv_assumption_stack_destroy(nav->assumption_stack);
        lv_free((void **)&nav);
        return NULL;
    }

    nav->total_assumptions = 0;
    nav->total_closures = 0;
    nav->total_breakpoints = 0;

    return nav;
}

void lv_proof_navigator_ex_destroy(lvProofNavigatorEx *navigator) {
    if (!navigator) return;

    /* 释放假设栈 */
    if (navigator->assumption_stack) {
        lv_assumption_stack_destroy(navigator->assumption_stack);
    }

    /* 释放闭包 */
    if (navigator->closures) {
        for (int i = 0; i < navigator->closure_count; i++) {
            if (navigator->closures[i]) {
                lv_contradiction_closure_destroy(navigator->closures[i]);
            }
        }
        lv_free((void **)&navigator->closures);
    }

    /* 释放断点 */
    if (navigator->breakpoints) {
        for (int i = 0; i < navigator->breakpoint_count; i++) {
            if (navigator->breakpoints[i]) {
                lv_contradiction_breakpoint_destroy(navigator->breakpoints[i]);
            }
        }
        lv_free((void **)&navigator->breakpoints);
    }

    lv_free((void **)&navigator);
}

lvProofScopeId lv_proof_begin_contradiction(lvProofNavigatorEx *navigator,
                                                  Proposition *negation_prop) {
    if (!navigator || !negation_prop) return -1;

    /* 生成新的作用域 ID */
    lvProofScopeId scope_id = navigator->total_assumptions + 1;

    /* 将否定命题压入假设栈 */
    int result = lv_assumption_stack_push(
        navigator->assumption_stack,
        scope_id,
        ASSUMPTION_TYPE_TEMPORARY,
        negation_prop);

    if (result < 0) return -1;

    navigator->total_assumptions++;

    return scope_id;
}

bool lv_proof_end_contradiction(lvProofNavigatorEx *navigator,
                                   lvProofScopeId scope_id,
                                   lvContradictionClosure **out_closure) {
    if (!navigator) return false;

    /* 查找并标记该作用域的假设为矛盾 */
    lvAssumptionEntry *entries[16];
    int count = lv_assumption_stack_get_by_scope(
        navigator->assumption_stack, scope_id, entries, 16);

    for (int i = 0; i < count; i++) {
        entries[i]->is_contradictory = true;
    }

    /* 创建矛盾闭包 */
    Proposition *dummy_prop = proposition_create(999, PROPOSITION_TYPE_ATOMIC);
    lvContradictionClosure *closure = lv_contradiction_closure_create(
        scope_id, CONTRADICTION_TYPE_DIRECT, dummy_prop);

    if (closure) {
        /* 添加到导航器 */
        if (navigator->closure_count >= navigator->closure_capacity) {
            int new_cap = navigator->closure_capacity * 2;
            lvContradictionClosure **new_closures = lv_realloc(
                navigator->closures,
                (size_t)new_cap * sizeof(lvContradictionClosure *));
            if (!new_closures) {
                lv_contradiction_closure_destroy(closure);
                return false;
            }
            navigator->closures = new_closures;
            navigator->closure_capacity = new_cap;
        }

        navigator->closures[navigator->closure_count++] = closure;
        navigator->total_closures++;

        if (out_closure) {
            *out_closure = closure;
        }
    }

    return true;
}

bool lv_proof_scope_is_valid(lvProofNavigatorEx *navigator,
                                lvProofScopeId scope_id) {
    if (!navigator) return false;

    /* 检查是否有该作用域的矛盾闭包 */
    for (int i = 0; i < navigator->closure_count; i++) {
        if (navigator->closures[i] &&
            navigator->closures[i]->scope_id == scope_id &&
            navigator->closures[i]->is_closed) {
            return false;  /* 该作用域已关闭 */
        }
    }

    return true;
}

char *lv_proof_export_contradiction_trace(lvProofNavigatorEx *navigator,
                                              lvProofScopeId scope_id) {
    if (!navigator) return NULL;

    /* 分配输出缓冲区 */
    size_t buf_size = 4096;
    char *buf = lv_malloc(buf_size);
    if (!buf) return NULL;

    int pos = 0;
    pos += snprintf(buf + pos, buf_size - (size_t)pos,
                    "=== 矛盾证明追踪 ===\n");
    pos += snprintf(buf + pos, buf_size - (size_t)pos,
                    "作用域 ID: %d\n", scope_id);

    /* 输出假设信息 */
    lvAssumptionEntry *entries[16];
    int count = lv_assumption_stack_get_by_scope(
        navigator->assumption_stack, scope_id, entries, 16);

    pos += snprintf(buf + pos, buf_size - (size_t)pos,
                    "\n假设栈 (%d 个假设):\n", count);
    for (int i = 0; i < count; i++) {
        pos += snprintf(buf + pos, buf_size - (size_t)pos,
                        "  [%d] 深度=%d, 类型=%d, 矛盾=%s\n",
                        entries[i]->assumption_id,
                        entries[i]->depth,
                        entries[i]->type,
                        entries[i]->is_contradictory ? "是" : "否");
    }

    /* 输出闭包信息 */
    int closure_count = 0;
    for (int i = 0; i < navigator->closure_count; i++) {
        if (navigator->closures[i] &&
            navigator->closures[i]->scope_id == scope_id) {
            closure_count++;
        }
    }

    pos += snprintf(buf + pos, buf_size - (size_t)pos,
                    "\n矛盾闭包 (%d 个):\n", closure_count);
    for (int i = 0; i < navigator->closure_count; i++) {
        if (navigator->closures[i] &&
            navigator->closures[i]->scope_id == scope_id) {
            pos += snprintf(buf + pos, buf_size - (size_t)pos,
                            "  [%d] 类型=%d, 已关闭=%s\n",
                            navigator->closures[i]->closure_id,
                            navigator->closures[i]->type,
                            navigator->closures[i]->is_closed ? "是" : "否");
        }
    }

    return buf;
}
