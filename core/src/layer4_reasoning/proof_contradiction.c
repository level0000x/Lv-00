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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_utils.h"
#include "lv/proof.h"

/* Process-wide ID allocators: atomic increment, safe for concurrent
 * creation (same pattern as s_next_context_id in context.c). */
static int g_next_closure_id = 0;
static int g_next_bp_id = 0;

/* ============================================================
 * 假设栈实现
 * ============================================================ */

lvAssumptionStack *lv_assumption_stack_create(int capacity) {
    if (capacity <= 0)
        capacity = 16;

    lvAssumptionStack *stack = lv_calloc(1, sizeof(lvAssumptionStack));
    if (!stack)
        return NULL;

    lv_darray_init(&stack->entries, sizeof(lvAssumptionEntry));
    if (!lv_darray_reserve(&stack->entries, capacity)) {
        lv_free((void **) &stack);
        return NULL;
    }

    stack->max_depth = 10; /* 默认最大嵌套深度 */
    stack->current_scope = 0;

    return stack;
}

void lv_assumption_stack_destroy(lvAssumptionStack *stack) {
    if (!stack)
        return;

    /* 释放每个条目中的命题 */
    for (int i = 0; i < stack->entries.count; i++) {
        lvAssumptionEntry *entry = (lvAssumptionEntry *)lv_darray_get(&stack->entries, i);
        if (entry->prop) {
            proposition_destroy(entry->prop);
        }
    }

    lv_darray_free(&stack->entries);
    lv_free((void **) &stack);
}

int lv_assumption_stack_push(lvAssumptionStack *stack, lvProofScopeId scope_id, lvAssumptionType type,
                             Proposition *prop) {
    if (!stack || !prop)
        return -1;

    /* 计算深度 */
    int depth = 0;
    for (int i = stack->entries.count - 1; i >= 0; i--) {
        lvAssumptionEntry *prev = (lvAssumptionEntry *)lv_darray_get(&stack->entries, i);
        if (prev->scope_id == scope_id) {
            depth = prev->depth + 1;
            break;
        }
    }

    /* 检查深度限制 */
    if (depth > stack->max_depth)
        return -1;

    /* 添加新假设（lv_darray_push 自动扩容） */
    lvAssumptionEntry entry;
    memset(&entry, 0, sizeof(entry));
    entry.assumption_id = stack->entries.count;
    entry.scope_id = scope_id;
    entry.type = type;
    entry.prop = prop;
    entry.depth = depth;
    entry.parent_assumption_id = (stack->entries.count > 0) ? stack->entries.count - 1 : -1;
    entry.derivation_step_count = 0;
    entry.is_contradictory = false;
    entry.timestamp = (int64_t) time(NULL);

    stack->current_scope = scope_id;

    return lv_darray_push(&stack->entries, &entry);
}

lvAssumptionEntry *lv_assumption_stack_pop(lvAssumptionStack *stack) {
    if (!stack || stack->entries.count == 0)
        return NULL;

    stack->entries.count--;
    return (lvAssumptionEntry *)lv_darray_get(&stack->entries, stack->entries.count);
}

lvAssumptionEntry *lv_assumption_stack_find(lvAssumptionStack *stack, int assumption_id) {
    if (!stack)
        return NULL;

    for (int i = 0; i < stack->entries.count; i++) {
        lvAssumptionEntry *entry = (lvAssumptionEntry *)lv_darray_get(&stack->entries, i);
        if (entry->assumption_id == assumption_id) {
            return entry;
        }
    }

    return NULL;
}

int lv_assumption_stack_get_by_scope(lvAssumptionStack *stack, lvProofScopeId scope_id, lvAssumptionEntry **out,
                                     int max_out) {
    if (!stack || !out || max_out <= 0)
        return 0;

    int count = 0;
    for (int i = 0; i < stack->entries.count && count < max_out; i++) {
        lvAssumptionEntry *entry = (lvAssumptionEntry *)lv_darray_get(&stack->entries, i);
        if (entry->scope_id == scope_id) {
            out[count++] = entry;
        }
    }

    return count;
}

/* ============================================================
 * 矛盾闭包实现
 * ============================================================ */

lvContradictionClosure *lv_contradiction_closure_create(lvProofScopeId scope_id, lvContradictionType type,
                                                        Proposition *prop) {
    if (!prop)
        return NULL;

    lvContradictionClosure *closure = lv_calloc(1, sizeof(lvContradictionClosure));
    if (!closure)
        return NULL;

    closure->closure_id = lv_ATOMIC_ADD(&g_next_closure_id, 1);
    closure->scope_id = scope_id;
    closure->type = type;
    closure->contradiction_prop = prop;
    closure->is_closed = false;
    closure->created_timestamp = (int64_t) time(NULL);

    return closure;
}

void lv_contradiction_closure_destroy(lvContradictionClosure *closure) {
    if (!closure)
        return;

    if (closure->contradiction_prop) {
        proposition_destroy(closure->contradiction_prop);
    }
    if (closure->origin_prop) {
        proposition_destroy(closure->origin_prop);
    }

    lv_free((void **) &closure->derivation_path);
    lv_free((void **) &closure);
}

bool lv_contradiction_closure_close(lvContradictionClosure *closure) {
    if (!closure)
        return false;
    if (closure->is_closed)
        return false;

    closure->is_closed = true;
    closure->closed_timestamp = (int64_t) time(NULL);

    return true;
}

bool lv_contradiction_propagation_detect(lvProofScopeId scope, lvContradictionClosure **closures, int closure_count) {
    if (!closures || closure_count <= 0)
        return false;

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

lvContradictionBreakpoint *lv_contradiction_breakpoint_create(lvBreakpointType type, lvProofScopeId scope_id,
                                                              int step_id, const char *description) {
    lvContradictionBreakpoint *bp = lv_calloc(1, sizeof(lvContradictionBreakpoint));
    if (!bp)
        return NULL;

    bp->breakpoint_id = lv_ATOMIC_ADD(&g_next_bp_id, 1);
    bp->type = type;
    bp->scope_id = scope_id;
    bp->step_id = step_id;
    bp->depth = 0;
    bp->is_active = true;
    bp->timestamp = (int64_t) time(NULL);

    if (description) {
        bp->description = lv_strdup(description);
    }

    return bp;
}

void lv_contradiction_breakpoint_destroy(lvContradictionBreakpoint *bp) {
    if (!bp)
        return;
    lv_free((void **) &bp->description);
    lv_free((void **) &bp);
}

int lv_contradiction_detect_breakpoints(lvProofNavigatorEx *navigator, lvContradictionBreakpoint **breakpoints,
                                        int max_breakpoints) {
    if (!navigator || !breakpoints || max_breakpoints <= 0)
        return 0;

    int count = 0;
    int limit = (navigator->breakpoints.count < max_breakpoints) ? navigator->breakpoints.count : max_breakpoints;

    for (int i = 0; i < limit && count < max_breakpoints; i++) {
        lvContradictionBreakpoint **bp = (lvContradictionBreakpoint **)lv_darray_get(&navigator->breakpoints, i);
        if (*bp && (*bp)->is_active) {
            breakpoints[count++] = *bp;
        }
    }

    return count;
}

/* ============================================================
 * 扩展证明导航器实现
 * ============================================================ */

lvProofNavigatorEx *lv_proof_navigator_ex_create(void) {
    lvProofNavigatorEx *nav = lv_calloc(1, sizeof(lvProofNavigatorEx));
    if (!nav)
        return NULL;

    /* 创建假设栈 */
    nav->assumption_stack = lv_assumption_stack_create(16);
    if (!nav->assumption_stack) {
        lv_free((void **) &nav);
        return NULL;
    }

    /* 初始化闭包数组 */
    lv_darray_init(&nav->closures, sizeof(lvContradictionClosure *));
    if (!lv_darray_reserve(&nav->closures, 16)) {
        lv_assumption_stack_destroy(nav->assumption_stack);
        lv_free((void **) &nav);
        return NULL;
    }

    /* 初始化断点数组 */
    lv_darray_init(&nav->breakpoints, sizeof(lvContradictionBreakpoint *));
    if (!lv_darray_reserve(&nav->breakpoints, 16)) {
        lv_darray_free(&nav->closures);
        lv_assumption_stack_destroy(nav->assumption_stack);
        lv_free((void **) &nav);
        return NULL;
    }

    nav->total_assumptions = 0;
    nav->total_closures = 0;
    nav->total_breakpoints = 0;

    return nav;
}

void lv_proof_navigator_ex_destroy(lvProofNavigatorEx *navigator) {
    if (!navigator)
        return;

    /* 释放假设栈 */
    if (navigator->assumption_stack) {
        lv_assumption_stack_destroy(navigator->assumption_stack);
    }

    /* 释放闭包 */
    for (int i = 0; i < navigator->closures.count; i++) {
        lvContradictionClosure **closure = (lvContradictionClosure **)lv_darray_get(&navigator->closures, i);
        if (*closure) {
            lv_contradiction_closure_destroy(*closure);
        }
    }
    lv_darray_free(&navigator->closures);

    /* 释放断点 */
    for (int i = 0; i < navigator->breakpoints.count; i++) {
        lvContradictionBreakpoint **bp = (lvContradictionBreakpoint **)lv_darray_get(&navigator->breakpoints, i);
        if (*bp) {
            lv_contradiction_breakpoint_destroy(*bp);
        }
    }
    lv_darray_free(&navigator->breakpoints);

    lv_free((void **) &navigator);
}

lvProofScopeId lv_proof_begin_contradiction(lvProofNavigatorEx *navigator, Proposition *negation_prop) {
    if (!navigator || !negation_prop)
        return -1;

    /* 生成新的作用域 ID */
    lvProofScopeId scope_id = navigator->total_assumptions + 1;

    /* 将否定命题压入假设栈 */
    int result =
        lv_assumption_stack_push(navigator->assumption_stack, scope_id, ASSUMPTION_TYPE_TEMPORARY, negation_prop);

    if (result < 0)
        return -1;

    navigator->total_assumptions++;

    return scope_id;
}

bool lv_proof_end_contradiction(lvProofNavigatorEx *navigator, lvProofScopeId scope_id,
                                lvContradictionClosure **out_closure) {
    if (!navigator)
        return false;

    /* 查找并标记该作用域的假设为矛盾 */
    lvAssumptionEntry *entries[16];
    int count = lv_assumption_stack_get_by_scope(navigator->assumption_stack, scope_id, entries, 16);

    for (int i = 0; i < count; i++) {
        entries[i]->is_contradictory = true;
    }

    /* 创建矛盾闭包 */
    Proposition *dummy_prop = proposition_create(999, PROPOSITION_TYPE_ATOMIC);
    lvContradictionClosure *closure = lv_contradiction_closure_create(scope_id, CONTRADICTION_TYPE_DIRECT, dummy_prop);

    if (closure) {
        /* 添加到导航器（lv_darray_push 自动扩容） */
        lv_darray_push(&navigator->closures, &closure);
        navigator->total_closures++;

        if (out_closure) {
            *out_closure = closure;
        }
    }

    return true;
}

bool lv_proof_scope_is_valid(lvProofNavigatorEx *navigator, lvProofScopeId scope_id) {
    if (!navigator)
        return false;

    /* 检查是否有该作用域的矛盾闭包 */
    for (int i = 0; i < navigator->closures.count; i++) {
        lvContradictionClosure **closure = (lvContradictionClosure **)lv_darray_get(&navigator->closures, i);
        if (*closure && (*closure)->scope_id == scope_id && (*closure)->is_closed) {
            return false; /* 该作用域已关闭 */
        }
    }

    return true;
}

char *lv_proof_export_contradiction_trace(lvProofNavigatorEx *navigator, lvProofScopeId scope_id) {
    if (!navigator)
        return NULL;

    lvStrBuf sb = {0};
    lv_strbuf_printf(&sb, "=== 矛盾证明追踪 ===\n");
    lv_strbuf_printf(&sb, "作用域 ID: %d\n", scope_id);

    /* 输出假设信息 */
    lvAssumptionEntry *entries[16];
    int count = lv_assumption_stack_get_by_scope(navigator->assumption_stack, scope_id, entries, 16);

    lv_strbuf_printf(&sb, "\n假设栈 (%d 个假设):\n", count);
    for (int i = 0; i < count; i++) {
        lv_strbuf_printf(&sb, "  [%d] 深度=%d, 类型=%d, 矛盾=%s\n", entries[i]->assumption_id, entries[i]->depth,
                            entries[i]->type, entries[i]->is_contradictory ? "是" : "否");
    }

    /* 输出闭包信息 */
    int closure_count = 0;
    for (int i = 0; i < navigator->closures.count; i++) {
        lvContradictionClosure **closure = (lvContradictionClosure **)lv_darray_get(&navigator->closures, i);
        if (*closure && (*closure)->scope_id == scope_id) {
            closure_count++;
        }
    }

    lv_strbuf_printf(&sb, "\n矛盾闭包 (%d 个):\n", closure_count);
    for (int i = 0; i < navigator->closures.count; i++) {
        lvContradictionClosure **closure = (lvContradictionClosure **)lv_darray_get(&navigator->closures, i);
        if (*closure && (*closure)->scope_id == scope_id) {
            lv_strbuf_printf(&sb, "  [%d] 类型=%d, 已关闭=%s\n", (*closure)->closure_id,
                                (*closure)->type, (*closure)->is_closed ? "是" : "否");
        }
    }

    return lv_strbuf_to_string(&sb);
}
