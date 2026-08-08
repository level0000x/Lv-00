/**
 * @file recursion_context.c
 * @brief recursion context API
 * @details Split from recursion.c
 */

#include "lv/lv_platform.h"
#include "recursion.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv_internal.h"
#include "lv/lv_xmacro.h"
#include "lv_utils.h"
#include "stream.h"
#include "stream_context_util.h"
#include "recursion_internal.h"

/* ============== 递归上下文API ============== */

/**
 * @brief 创建递归上下文
 *
 * 分配并初始化递归上下文，设置最大递归深度限制。
 * 深度上限受 lv_MAX_RECURSION_DEPTH_LIMIT 约束。
 *
 * @param max_depth 最大递归深度（0 或负值时使用默认值 10000）
 * @return 新分配的递归上下文指针，失败返回 NULL
 */
RecursionContext *recursion_context_create(int max_depth) {
    RecursionContext *ctx = lv_calloc(1, sizeof(RecursionContext));
    if (!ctx)
        return NULL;

    /* 限制最大递归深度，防止内存耗尽（使用文件顶部定义的 lv_MAX_RECURSION_DEPTH_LIMIT） */
    if (max_depth > lv_MAX_RECURSION_DEPTH_LIMIT) {
        max_depth = lv_MAX_RECURSION_DEPTH_LIMIT;
    }
    ctx->max_depth = max_depth > 0 ? max_depth : 10000;
    ctx->current_depth = 0;
    ctx->is_terminated = false;
    ctx->depth_callback = NULL;
    ctx->depth_callback_user_data = NULL;

    return ctx;
}

/**
 * @brief 销毁递归上下文并释放所有资源
 *
 * @param ctx 递归上下文指针（可为 NULL）
 */
void recursion_context_destroy(RecursionContext *ctx) {
    if (!ctx)
        return;

    for (int i = 0; i < ctx->measure_value_count; i++) {
        symbolic_coord_destroy(ctx->measure_values[i]);
    }
    lv_free((void **) &ctx->measure_values);
    lv_free((void **) &ctx->call_stack);
    lv_free((void **) &ctx->termination_reason);
    lv_free((void **) &ctx);
}

/**
 * @brief 设置递归上下文的活跃测度
 *
 * @param ctx 递归上下文指针（可为 NULL）
 * @param m   测度指针
 */
void recursion_context_set_measure(RecursionContext *ctx, Measure *m) {
    if (ctx)
        ctx->active_measure = m;
}

/* ============== 修改5：深度超限回调注册 ============== */

/**
 * @brief 注册深度超限回调函数
 *
 * 当递归深度达到最大限制时，调用注册的回调函数让用户决定是否继续。
 *
 * @param ctx        递归上下文指针
 * @param callback   深度超限回调函数
 * @param user_data  回调透传数据
 */
void recursion_context_set_depth_callback(RecursionContext *ctx, RecursionDepthCallback callback, void *user_data) {
    if (!ctx)
        return;
    ctx->depth_callback = callback;
    ctx->depth_callback_user_data = user_data;
}

/**
 * @brief 进入递归调用（递归深度检查入口）
 *
 * 检查递归深度限制，计算当前测度值，验证单调递减性。
 * 超过深度限制时触发回调机制（如已注册）。
 *
 * @param ctx           递归上下文指针
 * @param func_block_id 函数块 ID
 * @param input         输入几何节点
 * @param graph         约束图指针
 * @return 递归检查结果
 */
RecursionCheckResult recursion_context_enter(RecursionContext *ctx, int func_block_id, const GeomNode *input,
                                             ConstraintGraph *graph) {
    if (!ctx)
        return RECURSION_ERROR;

    /* 流式输出：递归测度检查入口 */
    if (recursion_stream_ctx) {
        stream_emit_simple(recursion_stream_ctx, STREAM_EVENT_INFO, "递归测度检查", ctx->current_depth);
    }

    /* 检查深度限制（修改5：支持回调机制） */
    if (ctx->current_depth >= ctx->max_depth) {
        if (ctx->depth_callback) {
            /* 如果注册了回调，让用户决定是否继续 */
            RecursionAction action =
                ctx->depth_callback(ctx->current_depth, ctx->max_depth, ctx->depth_callback_user_data);

            if (action == RECURSION_ACTION_STOP) {
                /* 用户决定停止 */
                /* 流式事件：递归深度超限（回调停止） */
                if (recursion_stream_ctx) {
                    stream_emit_simple(recursion_stream_ctx, STREAM_EVENT_ERROR, "递归深度超限（回调停止）",
                                       ctx->current_depth);
                }
                ctx->is_terminated = true;
                lv_free((void **) &ctx->termination_reason);
                ctx->termination_reason = lv_strdup("Maximum recursion depth exceeded (user callback decided to stop)");
                return RECURSION_DEPTH_EXCEEDED;
            }
            /* RECURSION_ACTION_CONTINUE：用户决定继续，不终止 */
        } else {
            /* 未注册回调，保持原有行为 */
            /* 流式事件：递归深度超限 */
            if (recursion_stream_ctx) {
                stream_emit_simple(recursion_stream_ctx, STREAM_EVENT_ERROR, "递归深度超限", ctx->current_depth);
            }
            ctx->is_terminated = true;
            lv_free((void **) &ctx->termination_reason);
            ctx->termination_reason = lv_strdup("Maximum recursion depth exceeded");
            return RECURSION_DEPTH_EXCEEDED;
        }
    }

    /* 检查测度递减性 */
    if (ctx->active_measure && input) {
        SymbolicCoord *new_value = measure_compute_value(ctx->active_measure, input, graph);
        if (new_value) {
            RecursionCheckResult result = recursion_context_check_decreasing(ctx, new_value);

            if (result == RECURSION_NOT_DECREASING) {
                symbolic_coord_destroy(new_value);
                ctx->is_terminated = true;
                lv_free((void **) &ctx->termination_reason);
                ctx->termination_reason = lv_strdup("Measure not decreasing");
                return RECURSION_NOT_DECREASING;
            }

            /* 记录测度值 */
            /* 统一倍增扩容（capacity 字段 + lv_ensure_capacity，摊还 O(1)，含溢出检查） */
            if (!lv_ensure_capacity((void **) &ctx->measure_values, ctx->measure_value_count,
                                    &ctx->measure_values_capacity, sizeof(SymbolicCoord *), 1)) {
                symbolic_coord_destroy(new_value);
                return RECURSION_ERROR;
            }
            ctx->measure_values[ctx->measure_value_count] = new_value;
            ctx->measure_value_count++;
        }
    }

    /* 记录调用栈 */
    /* 统一倍增扩容（capacity 字段 + lv_ensure_capacity，摊还 O(1)，含溢出检查） */
    if (!lv_ensure_capacity((void **) &ctx->call_stack, ctx->call_stack_size,
                            &ctx->call_stack_capacity, sizeof(int), 1)) {
        return RECURSION_ERROR;
    }
    ctx->call_stack[ctx->call_stack_size] = func_block_id;
    ctx->call_stack_size++;

    /*
     * 检测循环调用：如果同一个函数块在调用栈中已经存在，
     * 说明可能存在无限递归循环。
     *
     * 策略：设置错误标志并返回 RECURSION_ERROR，让调用者决定如何处理。
     * 这比静默忽略更安全，因为即使测度在递减，循环调用模式本身
     * 也可能暗示着逻辑错误。
     */
    for (int i = 0; i < ctx->call_stack_size - 1; i++) {
        if (ctx->call_stack[i] == func_block_id) {
            /* 流式事件：递归循环检测 */
            if (recursion_stream_ctx) {
                stream_emit_simple(recursion_stream_ctx, STREAM_EVENT_CONFLICT_DETECTED, "递归循环检测", func_block_id);
            }
            ctx->is_terminated = true;
            lv_free((void **) &ctx->termination_reason);
            ctx->termination_reason = lv_strdup("Cycle detected: recursive call to the same function block");
            return RECURSION_ERROR;
        }
    }

    ctx->current_depth++;

    /* 流式输出：递归终止检查通过 */
    if (recursion_stream_ctx) {
        stream_emit_simple(recursion_stream_ctx, STREAM_EVENT_INFO, "递归终止检查通过", ctx->current_depth);
    }

    return RECURSION_OK;
}

/**
 * @brief 退出递归调用
 *
 * 递减当前深度，弹出调用栈和测度值栈。
 *
 * @param ctx 递归上下文指针（可为 NULL）
 */
void recursion_context_exit(RecursionContext *ctx) {
    if (!ctx)
        return;

    /* 流式事件：递归退出 */
    if (recursion_stream_ctx) {
        stream_emit_simple(recursion_stream_ctx, STREAM_EVENT_INFO, "递归退出", ctx->current_depth);
    }

    if (ctx->current_depth > 0) {
        ctx->current_depth--;
    }

    if (ctx->call_stack_size > 0) {
        ctx->call_stack_size--;
    }

    if (ctx->measure_value_count > 0) {
        symbolic_coord_destroy(ctx->measure_values[ctx->measure_value_count - 1]);
        ctx->measure_value_count--;
    }
}

/* ============== 修改1：验证整条调用链的单调递减 ============== */

/**
 * @brief 验证整条调用链的单调递减性
 *
 * 检查新测度值是否严格小于调用链中所有已有的测度值，
 * 确保递归终止条件（良基性）。
 *
 * @param ctx       递归上下文指针
 * @param new_value 新的测度值
 * @return 递归检查结果
 */
RecursionCheckResult recursion_context_check_decreasing(const RecursionContext *ctx, SymbolicCoord *new_value) {
    if (!ctx || !new_value)
        return RECURSION_ERROR;

    if (!ctx->active_measure)
        return RECURSION_OK;

    if (ctx->measure_value_count == 0) {
        /* 第一次调用，无需检查递减 */
        return RECURSION_OK;
    }

    /*
     * 修改1：遍历整个 measure_values 数组，验证严格单调递减
     * 即 measure_values[0] > measure_values[1] > ... > measure_values[count-1] > new_value
     * 如果发现任何相邻对不满足递减，返回 RECURSION_NOT_DECREASING
     */

    /* 首先检查最后一个值与 new_value 的关系 */
    SymbolicCoord *prev_value = ctx->measure_values[ctx->measure_value_count - 1];
    MeasureCompareResult cmp = measure_compare(ctx->active_measure, new_value, prev_value);

    if (cmp == MEASURE_LESS) {
        /* new_value < prev_value，满足递减 */
    } else if (cmp == MEASURE_EQUAL || cmp == MEASURE_GREATER) {
        /* 流式输出：测度未减小 */
        if (recursion_stream_ctx) {
            stream_emit_simple(recursion_stream_ctx, STREAM_EVENT_WARNING, "递归测度未减小", ctx->current_depth);
        }
        return RECURSION_NOT_DECREASING;
    } else {
        return RECURSION_MEASURE_UNKNOWN;
    }

    /* 然后遍历整个已有的 measure_values 数组，验证整体单调递减 */
    for (int i = 0; i < ctx->measure_value_count - 1; i++) {
        SymbolicCoord *current = ctx->measure_values[i];
        SymbolicCoord *next = ctx->measure_values[i + 1];

        MeasureCompareResult chain_cmp = measure_compare(ctx->active_measure, next, current);

        if (chain_cmp != MEASURE_LESS) {
            /* 发现不满足递减的相邻对 */
            if (chain_cmp == MEASURE_EQUAL || chain_cmp == MEASURE_GREATER) {
                /* 流式输出：测度未减小（调用链检查） */
                if (recursion_stream_ctx) {
                    stream_emit_simple(recursion_stream_ctx, STREAM_EVENT_WARNING, "递归测度未减小",
                                       ctx->current_depth);
                }
                return RECURSION_NOT_DECREASING;
            }
            return RECURSION_MEASURE_UNKNOWN;
        }
    }

    /* 所有相邻对都满足严格单调递减 */
    return RECURSION_OK;
}

/**
 * @brief 获取当前递归深度
 *
 * @param ctx 递归上下文指针
 * @return 当前递归深度，NULL 时返回 0
 */
int recursion_context_get_depth(RecursionContext *ctx) {
    return ctx ? ctx->current_depth : 0;
}

/**
 * @brief 重置递归上下文
 *
 * 清空调用栈、测度值和终止状态，恢复到初始状态。
 *
 * @param ctx 递归上下文指针（可为 NULL）
 */
void recursion_context_reset(RecursionContext *ctx) {
    if (!ctx)
        return;

    for (int i = 0; i < ctx->measure_value_count; i++) {
        symbolic_coord_destroy(ctx->measure_values[i]);
    }
    lv_free((void **) &ctx->measure_values);
    ctx->measure_values = NULL;
    ctx->measure_value_count = 0;
    ctx->measure_values_capacity = 0;

    lv_free((void **) &ctx->call_stack);
    ctx->call_stack = NULL;
    ctx->call_stack_size = 0;
    ctx->call_stack_capacity = 0;

    ctx->current_depth = 0;
    ctx->is_terminated = false;

    lv_free((void **) &ctx->termination_reason);
    ctx->termination_reason = NULL;

    /* 修改5：重置时保留回调设置 */
}
