/**
 * @file proof_contradiction.c
 * @brief 反证法与矛盾推演系统实现
 *
 * 实现假设栈、局部矛盾闭包、矛盾传播断点检测等核心功能。
 *
 * @version 4.0.0
 */

#include "proof_contradiction.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "lv00.h"
#include "lv00_utils.h"

/* ============== 假设栈实现 ============== */

/**
 * @brief 创建假设栈
 */
Lv00AssumptionStack *lv00_assumption_stack_create(int capacity) {
    if (capacity <= 0) capacity = 16;
    
    Lv00AssumptionStack *stack = (Lv00AssumptionStack *)lv00_malloc(sizeof(Lv00AssumptionStack));
    if (!stack) return NULL;
    
    stack->entries = (Lv00AssumptionEntry *)lv00_malloc(capacity * sizeof(Lv00AssumptionEntry));
    if (!stack->entries) {
        lv00_free((void **)&stack);
        return NULL;
    }
    
    stack->count = 0;
    stack->capacity = capacity;
    stack->max_depth = 0;
    stack->current_scope = LV00_PROOF_SCOPE_GLOBAL;
    
    return stack;
}

/**
 * @brief 销毁假设栈
 */
void lv00_assumption_stack_destroy(Lv00AssumptionStack *stack) {
    if (!stack) return;
    
    /* 释放所有假设条目中的命题引用 */
    for (int i = 0; i < stack->count; i++) {
        if (stack->entries[i].prop) {
            proposition_unref(stack->entries[i].prop);
        }
    }
    
    lv00_free((void **)&stack->entries);
    lv00_free((void **)&stack);
}

/**
 * @brief 确保栈容量足够
 */
static bool assumption_stack_ensure_capacity(Lv00AssumptionStack *stack) {
    if (!stack) return false;
    if (stack->count < stack->capacity) return true;
    
    int new_capacity = stack->capacity * 2;
    Lv00AssumptionEntry *new_entries = (Lv00AssumptionEntry *)lv00_realloc(
        stack->entries, new_capacity * sizeof(Lv00AssumptionEntry));
    
    if (!new_entries) return false;
    
    stack->entries = new_entries;
    stack->capacity = new_capacity;
    return true;
}

/**
 * @brief 推送假设到栈
 */
int lv00_assumption_stack_push(
    Lv00AssumptionStack *stack,
    Lv00ProofScopeId scope_id,
    Lv00AssumptionType type,
    Proposition *prop
) {
    if (!stack || !prop) return -1;
    
    /* 确保容量 */
    if (!assumption_stack_ensure_capacity(stack)) return -1;
    
    /* 确定父假设 */
    int parent_id = -1;
    if (stack->count > 0) {
        parent_id = stack->entries[stack->count - 1].assumption_id;
    }
    
    /* 计算深度 */
    int depth = 0;
    if (scope_id != stack->current_scope) {
        /* 新作用域，深度归零 */
        depth = 0;
        stack->current_scope = scope_id;
    } else {
        depth = stack->max_depth + 1;
    }
    
    if (depth > stack->max_depth) {
        stack->max_depth = depth;
    }
    
    /* 填充条目 */
    Lv00AssumptionEntry *entry = &stack->entries[stack->count];
    entry->assumption_id = stack->count;
    entry->scope_id = scope_id;
    entry->type = type;
    entry->prop = prop;
    proposition_ref(prop);  /* 增加引用计数 */
    entry->depth = depth;
    entry->parent_assumption_id = parent_id;
    entry->derivation_step_count = 0;
    entry->is_contradictory = false;
    entry->timestamp = lv00_circuit_breaker_now_us();
    
    stack->count++;
    return entry->assumption_id;
}

/**
 * @brief 从栈中弹出假设
 */
Lv00AssumptionEntry *lv00_assumption_stack_pop(Lv00AssumptionStack *stack) {
    if (!stack || stack->count <= 0) return NULL;
    
    stack->count--;
    Lv00AssumptionEntry *entry = &stack->entries[stack->count];
    
    /* 释放命题引用 */
    if (entry->prop) {
        proposition_unref(entry->prop);
        entry->prop = NULL;
    }
    
    return entry;
}

/**
 * @brief 查找假设
 */
Lv00AssumptionEntry *lv00_assumption_stack_find(
    Lv00AssumptionStack *stack,
    int assumption_id
) {
    if (!stack || assumption_id < 0 || assumption_id >= stack->count) {
        return NULL;
    }
    return &stack->entries[assumption_id];
}

/**
 * @brief 获取作用域内的所有假设
 */
int lv00_assumption_stack_get_by_scope(
    Lv00AssumptionStack *stack,
    Lv00ProofScopeId scope_id,
    Lv00AssumptionEntry **out,
    int max_out
) {
    if (!stack || !out || max_out <= 0) return 0;
    
    int count = 0;
    for (int i = 0; i < stack->count && count < max_out; i++) {
        if (stack->entries[i].scope_id == scope_id) {
            out[count++] = &stack->entries[i];
        }
    }
    return count;
}

/* ============== 矛盾闭包实现 ============== */

/**
 * @brief 创建矛盾闭包
 */
Lv00ContradictionClosure *lv00_contradiction_closure_create(
    Lv00ProofScopeId scope_id,
    Lv00ContradictionType type,
    Proposition *prop
) {
    Lv00ContradictionClosure *closure = (Lv00ContradictionClosure *)lv00_malloc(
        sizeof(Lv00ContradictionClosure));
    if (!closure) return NULL;
    
    closure->closure_id = 0;  /* 由管理器分配 */
    closure->scope_id = scope_id;
    closure->type = type;
    closure->contradiction_prop = prop;
    proposition_ref(prop);
    closure->origin_prop = NULL;
    closure->triggering_assumption = NULL;
    closure->derivation_path = NULL;
    closure->derivation_path_length = 0;
    closure->is_closed = false;
    closure->created_timestamp = lv00_circuit_breaker_now_us();
    closure->closed_timestamp = 0;
    
    return closure;
}

/**
 * @brief 销毁矛盾闭包
 */
void lv00_contradiction_closure_destroy(Lv00ContradictionClosure *closure) {
    if (!closure) return;
    
    if (closure->contradiction_prop) {
        proposition_unref(closure->contradiction_prop);
    }
    if (closure->origin_prop) {
        proposition_unref(closure->origin_prop);
    }
    if (closure->derivation_path) {
        lv00_free((void **)&closure->derivation_path);
    }
    
    lv00_free((void **)&closure);
}

/**
 * @brief 关闭矛盾闭包
 */
bool lv00_contradiction_closure_close(Lv00ContradictionClosure *closure) {
    if (!closure || closure->is_closed) return false;
    
    closure->is_closed = true;
    closure->closed_timestamp = lv00_circuit_breaker_now_us();
    return true;
}

/**
 * @brief 检测矛盾传播
 */
bool lv00_contradiction_propagation_detect(
    Lv00ProofScopeId scope,
    Lv00ContradictionClosure **closures,
    int closure_count
) {
    if (!closures || closure_count <= 0) return false;
    
    /* 检查是否存在传播类型的矛盾闭包 */
    for (int i = 0; i < closure_count; i++) {
        Lv00ContradictionClosure *c = closures[i];
        if (c && c->type == CONTRADICTION_TYPE_PROPAGATED && c->scope_id == scope) {
            /* 发现从子假设传播的矛盾 */
            return true;
        }
    }
    return false;
}

/* ============== 断点实现 ============== */

/**
 * @brief 创建断点
 */
Lv00ContradictionBreakpoint *lv00_contradiction_breakpoint_create(
    Lv00BreakpointType type,
    Lv00ProofScopeId scope_id,
    int step_id,
    const char *description
) {
    Lv00ContradictionBreakpoint *bp = (Lv00ContradictionBreakpoint *)lv00_malloc(
        sizeof(Lv00ContradictionBreakpoint));
    if (!bp) return NULL;
    
    bp->breakpoint_id = 0;  /* 由管理器分配 */
    bp->type = type;
    bp->scope_id = scope_id;
    bp->step_id = step_id;
    bp->depth = 0;
    bp->is_active = true;
    bp->timestamp = lv00_circuit_breaker_now_us();
    
    if (description) {
        bp->description = lv00_strdup(description);
    } else {
        bp->description = NULL;
    }
    
    return bp;
}

/**
 * @brief 销毁断点
 */
void lv00_contradiction_breakpoint_destroy(Lv00ContradictionBreakpoint *bp) {
    if (!bp) return;
    if (bp->description) {
        lv00_free((void **)&bp->description);
    }
    lv00_free((void **)&bp);
}

/**
 * @brief 检测推导断点
 */
int lv00_contradiction_detect_breakpoints(
    Lv00ProofNavigatorEx *navigator,
    Lv00ContradictionBreakpoint **breakpoints,
    int max_breakpoints
) {
    if (!navigator || !breakpoints || max_breakpoints <= 0) return 0;
    
    int count = 0;
    ProofNavigator *base = &navigator->base;
    
    /* 遍历证明步骤，检测关键断点 */
    for (int i = 0; i < base->step_count && count < max_breakpoints; i++) {
        ProofStep *step = base->steps[i];
        if (!step) continue;
        
        Lv00BreakpointType bp_type = BREAKPOINT_TYPE_DERIVATION;
        char desc[256] = {0};
        
        /* 根据步骤类型确定断点类型和描述 */
        switch (step->type) {
            case PROOF_STEP_EX_FALSO:
                bp_type = BREAKPOINT_TYPE_CONTRADICTION;
                snprintf(desc, sizeof(desc), "Ex Falso 步骤 (step %d): 矛盾推导", step->id);
                break;
                
            case PROOF_STEP_ADD_CONSTRAINT:
                snprintf(desc, sizeof(desc), "添加约束 (step %d)", step->id);
                break;
                
            default:
                snprintf(desc, sizeof(desc), "推导步骤 (step %d)", step->id);
                break;
        }
        
        /* 获取步骤所在的作用域 */
        Lv00ProofScopeId scope_id = LV00_PROOF_SCOPE_GLOBAL;
        if (navigator->assumption_stack && step->depth < navigator->assumption_stack->count) {
            scope_id = navigator->assumption_stack->entries[step->depth].scope_id;
        }
        
        /* 创建断点 */
        breakpoints[count] = lv00_contradiction_breakpoint_create(
            bp_type, scope_id, step->id, desc);
        
        if (breakpoints[count]) {
            breakpoints[count]->depth = step->depth;
            count++;
        }
    }
    
    return count;
}

/* ============== 扩展证明导航器实现 ============== */

/**
 * @brief 创建扩展证明导航器
 */
Lv00ProofNavigatorEx *lv00_proof_navigator_ex_create(void) {
    Lv00ProofNavigatorEx *navigator = (Lv00ProofNavigatorEx *)lv00_malloc(
        sizeof(Lv00ProofNavigatorEx));
    if (!navigator) return NULL;
    
    memset(navigator, 0, sizeof(Lv00ProofNavigatorEx));
    
    /* 初始化基类 */
    navigator->base.steps = NULL;
    navigator->base.step_count = 0;
    navigator->base.current_step = 0;
    navigator->base.target_prop = NULL;
    navigator->base.construction = NULL;
    navigator->base.is_complete = false;
    navigator->base.proof_state = PROOF_STATE_ONGOING;
    navigator->base.breakpoint_indices = NULL;
    navigator->base.breakpoint_count = 0;
    
    /* 初始化假设栈 */
    navigator->assumption_stack = lv00_assumption_stack_create(32);
    if (!navigator->assumption_stack) {
        lv00_free((void **)&navigator);
        return NULL;
    }
    
    /* 初始化闭包数组 */
    navigator->closure_capacity = 16;
    navigator->closures = (Lv00ContradictionClosure **)lv00_malloc(
        navigator->closure_capacity * sizeof(Lv00ContradictionClosure *));
    if (!navigator->closures) {
        lv00_assumption_stack_destroy(navigator->assumption_stack);
        lv00_free((void **)&navigator);
        return NULL;
    }
    
    /* 初始化断点数组 */
    navigator->breakpoint_capacity = 64;
    navigator->breakpoints = (Lv00ContradictionBreakpoint **)lv00_malloc(
        navigator->breakpoint_capacity * sizeof(Lv00ContradictionBreakpoint *));
    if (!navigator->breakpoints) {
        lv00_free((void **)&navigator->closures);
        lv00_assumption_stack_destroy(navigator->assumption_stack);
        lv00_free((void **)&navigator);
        return NULL;
    }
    
    /* 统计初始化 */
    navigator->total_assumptions = 0;
    navigator->total_closures = 0;
    navigator->total_breakpoints = 0;
    
    return navigator;
}

/**
 * @brief 销毁扩展证明导航器
 */
void lv00_proof_navigator_ex_destroy(Lv00ProofNavigatorEx *navigator) {
    if (!navigator) return;
    
    /* 销毁假设栈 */
    if (navigator->assumption_stack) {
        lv00_assumption_stack_destroy(navigator->assumption_stack);
    }
    
    /* 销毁所有闭包 */
    for (int i = 0; i < navigator->closure_count; i++) {
        if (navigator->closures[i]) {
            lv00_contradiction_closure_destroy(navigator->closures[i]);
        }
    }
    lv00_free((void **)&navigator->closures);
    
    /* 销毁所有断点 */
    for (int i = 0; i < navigator->breakpoint_count; i++) {
        if (navigator->breakpoints[i]) {
            lv00_contradiction_breakpoint_destroy(navigator->breakpoints[i]);
        }
    }
    lv00_free((void **)&navigator->breakpoints);
    
    /* 销毁基类 */
    /* 注意：如果基类有复杂的清理逻辑，需要在这里调用 */
    
    lv00_free((void **)&navigator);
}

/**
 * @brief 确保闭包容量足够
 */
static bool contradiction_ensure_closure_capacity(Lv00ProofNavigatorEx *navigator) {
    if (!navigator) return false;
    if (navigator->closure_count < navigator->closure_capacity) return true;
    
    int new_capacity = navigator->closure_capacity * 2;
    Lv00ContradictionClosure **new_closures = (Lv00ContradictionClosure **)lv00_realloc(
        navigator->closures, new_capacity * sizeof(Lv00ContradictionClosure *));
    
    if (!new_closures) return false;
    
    navigator->closures = new_closures;
    navigator->closure_capacity = new_capacity;
    return true;
}

/**
 * @brief 开始反证法证明
 */
Lv00ProofScopeId lv00_proof_begin_contradiction(
    Lv00ProofNavigatorEx *navigator,
    Proposition *negation_prop
) {
    if (!navigator || !negation_prop) return LV00_PROOF_SCOPE_INVALID;
    
    /* 创建新的作用域ID */
    Lv00ProofScopeId scope_id = navigator->base.next_scope_id;
    navigator->base.next_scope_id++;
    
    /* 将否定命题作为假设推入栈 */
    int assumption_id = lv00_assumption_stack_push(
        navigator->assumption_stack,
        scope_id,
        ASSUMPTION_TYPE_TEMPORARY,
        negation_prop
    );
    
    if (assumption_id < 0) return LV00_PROOF_SCOPE_INVALID;
    
    /* 创建假设引入断点 */
    char desc[128];
    snprintf(desc, sizeof(desc), "反证假设引入 (assumption %d)", assumption_id);
    
    Lv00ContradictionBreakpoint *bp = lv00_contradiction_breakpoint_create(
        BREAKPOINT_TYPE_ASSUMPTION, scope_id, assumption_id, desc);
    
    if (bp) {
        bp->depth = navigator->assumption_stack->max_depth;
        if (navigator->breakpoint_count < navigator->breakpoint_capacity) {
            navigator->breakpoints[navigator->breakpoint_count++] = bp;
            navigator->total_breakpoints++;
        } else {
            lv00_contradiction_breakpoint_destroy(bp);
        }
    }
    
    navigator->total_assumptions++;
    return scope_id;
}

/**
 * @brief 结束反证法证明
 */
bool lv00_proof_end_contradiction(
    Lv00ProofNavigatorEx *navigator,
    Lv00ProofScopeId scope_id,
    Lv00ContradictionClosure **out_closure
) {
    if (!navigator || scope_id == LV00_PROOF_SCOPE_INVALID) return false;
    
    /* 检查作用域是否有效 */
    if (!lv00_proof_scope_is_valid(navigator, scope_id)) {
        return false;
    }
    
    /* 获取作用域内的所有假设 */
    Lv00AssumptionEntry *assumptions[64];
    int count = lv00_assumption_stack_get_by_scope(
        navigator->assumption_stack, scope_id, assumptions, 64);
    
    /* 检查是否存在矛盾假设 */
    bool has_contradiction = false;
    Lv00AssumptionEntry *contradictory_assumption = NULL;
    
    for (int i = 0; i < count; i++) {
        if (assumptions[i]->is_contradictory) {
            has_contradiction = true;
            contradictory_assumption = assumptions[i];
            break;
        }
    }
    
    /* 创建矛盾闭包 */
    Lv00ContradictionClosure *closure = NULL;
    
    if (has_contradiction && contradictory_assumption) {
        /* 发现矛盾，创建闭包 */
        Proposition *bottom_prop = proposition_create(-1, PROPOSITION_TYPE_BOTTOM);
        if (bottom_prop) {
            closure = lv00_contradiction_closure_create(
                scope_id,
                CONTRADICTION_TYPE_INDIRECT,
                bottom_prop
            );
            proposition_destroy(bottom_prop);
            
            if (closure) {
                closure->closure_id = navigator->closure_count;
                closure->triggering_assumption = contradictory_assumption;
                
                /* 确保闭包容量 */
                if (contradiction_ensure_closure_capacity(navigator)) {
                    navigator->closures[navigator->closure_count++] = closure;
                    navigator->total_closures++;
                } else {
                    lv00_contradiction_closure_destroy(closure);
                    closure = NULL;
                }
            }
        }
    }
    
    /* 从假设栈中移除该作用域的所有假设 */
    while (navigator->assumption_stack->count > 0) {
        Lv00AssumptionEntry *top = &navigator->assumption_stack->entries[navigator->assumption_stack->count - 1];
        if (top->scope_id == scope_id) {
            lv00_assumption_stack_pop(navigator->assumption_stack);
        } else {
            break;
        }
    }
    
    /* 创建矛盾发现断点 */
    if (has_contradiction) {
        Lv00ContradictionBreakpoint *bp = lv00_contradiction_breakpoint_create(
            BREAKPOINT_TYPE_CONTRADICTION, scope_id, -1, "矛盾已发现，闭包关闭");
        
        if (bp && navigator->breakpoint_count < navigator->breakpoint_capacity) {
            navigator->breakpoints[navigator->breakpoint_count++] = bp;
            navigator->total_breakpoints++;
        }
    }
    
    /* 输出闭包 */
    if (out_closure) {
        *out_closure = closure;
    } else if (closure) {
        lv00_contradiction_closure_destroy(closure);
    }
    
    return true;
}

/**
 * @brief 检查作用域是否有效
 */
bool lv00_proof_scope_is_valid(
    Lv00ProofNavigatorEx *navigator,
    Lv00ProofScopeId scope_id
) {
    if (!navigator || scope_id == LV00_PROOF_SCOPE_INVALID) return false;
    
    /* 检查假设栈中是否存在该作用域的假设 */
    for (int i = 0; i < navigator->assumption_stack->count; i++) {
        if (navigator->assumption_stack->entries[i].scope_id == scope_id) {
            return true;
        }
    }
    
    return false;
}

/**
 * @brief 导出反证证明跟踪
 */
char *lv00_proof_export_contradiction_trace(
    Lv00ProofNavigatorEx *navigator,
    Lv00ProofScopeId scope_id
) {
    if (!navigator) return NULL;
    
    /* 分配缓冲区 */
    size_t buffer_size = 4096;
    char *buffer = (char *)lv00_malloc(buffer_size);
    if (!buffer) return NULL;
    buffer[0] = '\0';
    
    size_t offset = 0;
    
    /* 添加标题 */
    if (scope_id == LV00_PROOF_SCOPE_GLOBAL) {
        offset += snprintf(buffer + offset, buffer_size - offset,
            "=== 全局证明跟踪 ===\n\n");
    } else {
        offset += snprintf(buffer + offset, buffer_size - offset,
            "=== 反证作用域 %d 跟踪 ===\n\n", scope_id);
    }
    
    /* 导出假设 */
    offset += snprintf(buffer + offset, buffer_size - offset,
        "-- 假设栈 --\n");
    
    Lv00AssumptionEntry *assumptions[64];
    int count = lv00_assumption_stack_get_by_scope(
        navigator->assumption_stack, scope_id, assumptions, 64);
    
    for (int i = 0; i < count; i++) {
        const char *type_str = "未知";
        switch (assumptions[i]->type) {
            case ASSUMPTION_TYPE_TEMPORARY: type_str = "临时假设"; break;
            case ASSUMPTION_TYPE_LEMMA: type_str = "引理假设"; break;
            case ASSUMPTION_TYPE_CONDITIONAL: type_str = "条件假设"; break;
            case ASSUMPTION_TYPE_NUMERIC: type_str = "数值假设"; break;
        }
        
        offset += snprintf(buffer + offset, buffer_size - offset,
            "  [%d] depth=%d type=%s prop_id=%d %s\n",
            assumptions[i]->assumption_id,
            assumptions[i]->depth,
            type_str,
            assumptions[i]->prop ? assumptions[i]->prop->id : -1,
            assumptions[i]->is_contradictory ? "(矛盾)" : "");
    }
    
    offset += snprintf(buffer + offset, buffer_size - offset, "\n");
    
    /* 导出矛盾闭包 */
    offset += snprintf(buffer + offset, buffer_size - offset,
        "-- 矛盾闭包 --\n");
    
    for (int i = 0; i < navigator->closure_count; i++) {
        Lv00ContradictionClosure *c = navigator->closures[i];
        if (c && (scope_id == LV00_PROOF_SCOPE_GLOBAL || c->scope_id == scope_id)) {
            const char *type_str = "未知";
            switch (c->type) {
                case CONTRADICTION_TYPE_DIRECT: type_str = "直接矛盾"; break;
                case CONTRADICTION_TYPE_INDIRECT: type_str = "间接矛盾"; break;
                case CONTRADICTION_TYPE_PROPAGATED: type_str = "传播矛盾"; break;
                case CONTRADICTION_TYPE_SCOPE_LEAK: type_str = "作用域泄露"; break;
                default: type_str = "无"; break;
            }
            
            offset += snprintf(buffer + offset, buffer_size - offset,
                "  [闭包 %d] type=%s %s\n",
                c->closure_id,
                type_str,
                c->is_closed ? "(已关闭)" : "(活跃)");
        }
    }
    
    offset += snprintf(buffer + offset, buffer_size - offset, "\n");
    
    /* 导出断点 */
    offset += snprintf(buffer + offset, buffer_size - offset,
        "-- 断点 --\n");
    
    for (int i = 0; i < navigator->breakpoint_count; i++) {
        Lv00ContradictionBreakpoint *bp = navigator->breakpoints[i];
        if (bp && (scope_id == LV00_PROOF_SCOPE_GLOBAL || bp->scope_id == scope_id)) {
            const char *type_str = "未知";
            switch (bp->type) {
                case BREAKPOINT_TYPE_ASSUMPTION: type_str = "假设引入"; break;
                case BREAKPOINT_TYPE_DERIVATION: type_str = "推导步骤"; break;
                case BREAKPOINT_TYPE_CONTRADICTION: type_str = "矛盾发现"; break;
                case BREAKPOINT_TYPE_BACKTRACK: type_str = "回溯"; break;
            }
            
            offset += snprintf(buffer + offset, buffer_size - offset,
                "  [断点 %d] type=%s step=%d depth=%d %s\n",
                bp->breakpoint_id,
                type_str,
                bp->step_id,
                bp->depth,
                bp->is_active ? "(激活)" : "(禁用)");
        }
    }
    
    /* 添加统计信息 */
    offset += snprintf(buffer + offset, buffer_size - offset, "\n-- 统计 --\n");
    offset += snprintf(buffer + offset, buffer_size - offset,
        "总假设数: %d\n", navigator->total_assumptions);
    offset += snprintf(buffer + offset, buffer_size - offset,
        "总闭包数: %d\n", navigator->total_closures);
    offset += snprintf(buffer + offset, buffer_size - offset,
        "总断点数: %d\n", navigator->total_breakpoints);
    
    return buffer;
}
