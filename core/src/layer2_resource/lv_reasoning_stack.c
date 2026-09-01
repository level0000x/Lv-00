/**
 * @file lv_reasoning_stack.c
 * @brief 推理分支栈独立模块 —— 实现
 *
 * @details 从 context.c 中提取的推理栈操作实现。
 *          提供推理分支栈的完整生命周期管理。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 * @date   2026-07-31
 */

#include "lv/lv_reasoning_stack.h"

#include <string.h>

#include "lv/lv_internal.h"

/* ============================================================
 * 推理栈 API 实现
 * ============================================================ */

void lv_reasoning_stack_init(lvReasoningStack *stack) {
    if (!stack) {
        return;
    }
    stack->frames = NULL;
    stack->top = -1;
    stack->capacity = 0;
    stack->max_depth = lv_REASONING_STACK_MAX_DEPTH;
    stack->graph_snapshot_free = NULL; /* 由调用方（L0 注入）设置 */
}

/** 释放单帧内资源（graph_snapshot + assumption_node_ids + target_node_ids；user_data 由外部管理）
 *  F24/I5：graph_snapshot 为不透明资源，经 stack->graph_snapshot_free 回调释放
 *  （L0 注入的 ConstraintGraph 销毁承接，L2 不依赖 L3）。 */
static void reasoning_frame_release(lvReasoningStack *stack, lvReasoningFrame *frame) {
    if (!frame)
        return;
    if (frame->graph_snapshot) {
        if (stack && stack->graph_snapshot_free)
            stack->graph_snapshot_free(frame->graph_snapshot);
        frame->graph_snapshot = NULL;
    }
    if (frame->assumption_node_ids) {
        lv_free((void **) &frame->assumption_node_ids);
    }
    if (frame->target_node_ids) {
        lv_free((void **) &frame->target_node_ids);
    }
}

void lv_reasoning_stack_clear(lvReasoningStack *stack) {
    if (!stack) {
        return;
    }

    if (stack->frames) {
        for (int i = 0; i <= stack->top && i < stack->capacity; i++) {
            reasoning_frame_release(stack, &stack->frames[i]);
        }
        lv_free((void **) &stack->frames);
    }

    stack->frames = NULL;
    stack->top = -1;
    stack->capacity = 0;
    stack->max_depth = lv_REASONING_STACK_MAX_DEPTH;
}

int lv_reasoning_stack_ensure_capacity(lvReasoningStack *stack) {
    if (!stack) {
        return lv_ERROR_NULL_POINTER;
    }

    /* 栈未满，无需扩容 */
    if (stack->top + 1 < stack->capacity) {
        return lv_OK;
    }

    /* 已达最大深度限制 */
    if (stack->capacity >= stack->max_depth) {
        return lv_ERROR_RESOURCE_EXHAUSTED;
    }

    /* 上限钳制：先按倍增策略计算目标容量，再钳制到 max_depth
     * （capacity==0 时使用默认初始容量） */
    int old_capacity = stack->capacity;
    int new_capacity;
    if (old_capacity == 0) {
        new_capacity = lv_REASONING_STACK_DEFAULT_CAPACITY;
    } else {
        new_capacity = old_capacity * 2;
    }
    if (new_capacity > stack->max_depth) {
        new_capacity = stack->max_depth;
    }

    /* 统一扩容（min_growth 使 min_required = 钳制后的目标容量；
     * 注：倍增策略下分配容量可能略大于 max_depth，深度限制仍由
     * stack->max_depth 与上方检查保证） */
    if (!lv_ensure_capacity((void **) &stack->frames, old_capacity,
                            &stack->capacity, sizeof(lvReasoningFrame),
                            new_capacity - old_capacity))
        return lv_ERROR_OUT_OF_MEMORY;

    /* 清零新增部分 */
    size_t old_size = (size_t) old_capacity * sizeof(lvReasoningFrame);
    size_t new_size = (size_t) stack->capacity * sizeof(lvReasoningFrame);
    memset((char *) stack->frames + old_size, 0, new_size - old_size);

    return lv_OK;
}

int lv_reasoning_stack_push(lvReasoningStack *stack, lvReasoningBranchType branch_type) {
    if (!stack) {
        return lv_ERROR_NULL_POINTER;
    }

    /* 检查深度限制 */
    if (stack->top + 1 >= stack->max_depth) {
        return lv_ERROR_RESOURCE_EXHAUSTED;
    }

    /* 确保栈有足够容量 */
    int err = lv_reasoning_stack_ensure_capacity(stack);
    if (err != lv_OK) {
        return err;
    }

    /* 压入新帧 */
    stack->top++;
    lvReasoningFrame *frame = &stack->frames[stack->top];

    /* 初始化帧字段 */
    memset(frame, 0, sizeof(lvReasoningFrame));
    frame->branch_type = branch_type;
    frame->status = lv_BRANCH_ACTIVE;
    frame->depth = stack->top;
    frame->step_count = 0;

    return lv_OK;
}

int lv_reasoning_stack_pop(lvReasoningStack *stack) {
    if (!stack) {
        return lv_ERROR_NULL_POINTER;
    }

    if (stack->top < 0) {
        return lv_ERROR_INVALID_STATE;
    }

    lvReasoningFrame *frame = &stack->frames[stack->top];

    /* 释放帧内资源（与 clear 循环体共用 reasoning_frame_release） */
    reasoning_frame_release(stack, frame);

    /* 清零帧 */
    memset(frame, 0, sizeof(lvReasoningFrame));

    /* 弹出 */
    stack->top--;

    return lv_OK;
}

int lv_reasoning_stack_count(const lvReasoningStack *stack) {
    if (!stack) {
        return 0;
    }
    return stack->top + 1;
}

lvReasoningFrame *lv_reasoning_stack_top(lvReasoningStack *stack) {
    if (!stack) {
        return NULL;
    }
    if (stack->top < 0 || !stack->frames) {
        return NULL;
    }
    return &stack->frames[stack->top];
}
