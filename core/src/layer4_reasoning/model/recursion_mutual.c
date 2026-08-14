/**
 * @file recursion_mutual.c
 * @brief mutual recursion support
 * @details Split from recursion.c
 */

#include "lv/lv_platform.h"
#include "lv/recursion.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_xmacro.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"
#include "lv/stream_context_util.h"
#include "recursion_internal.h"

/* ============== 互递归支持 ============== */

bool recursion_check_mutual(int *func_ids, int count, MeasureSystem *ms) {
    if (!func_ids || count <= 0 || !ms)
        return false;

    /* 互递归的测度检查要求所有函数在同一个全局测度下各自递减 */
    /* 这需要更复杂的分析，这里简化为检查是否有默认测度 */

    if (!ms->default_measure) {
        return false;
    }

    /* 检查测度是否为良基 */
    return ms->default_measure->is_well_founded;
}

/* ============== 修改2：互递归的完整测度验证 ============== */

bool recursion_check_mutual_with_contexts(RecursionContext *ctx_a, RecursionContext *ctx_b) {
    if (!ctx_a || !ctx_b)
        return false;

    /* 流式事件：互递归测度验证开始 */
    if (recursion_stream_ctx) {
        stream_emit_simple(recursion_stream_ctx, STREAM_EVENT_PROGRESS, "互递归测度验证", 0);
    }

    /* 检查1：两个上下文使用相同的测度系统 */
    if (ctx_a->active_measure != ctx_b->active_measure) {
        /* 两个上下文必须使用同一个测度 */
        return false;
    }

    Measure *measure = ctx_a->active_measure;
    if (!measure) {
        /* 没有活动测度，无法验证 */
        return false;
    }

    /* 检查2：在各自的调用链中，测度值单调递减 */
    /* 验证 ctx_a 的调用链 */
    for (int i = 0; i < ctx_a->measure_value_count - 1; i++) {
        MeasureCompareResult cmp = measure_compare(measure, ctx_a->measure_values[i + 1], ctx_a->measure_values[i]);
        if (cmp != MEASURE_LESS) {
            return false; /* ctx_a 的调用链不满足递减 */
        }
    }

    /* 验证 ctx_b 的调用链 */
    for (int i = 0; i < ctx_b->measure_value_count - 1; i++) {
        MeasureCompareResult cmp = measure_compare(measure, ctx_b->measure_values[i + 1], ctx_b->measure_values[i]);
        if (cmp != MEASURE_LESS) {
            return false; /* ctx_b 的调用链不满足递减 */
        }
    }

    /* 检查3：两个函数块的测度值序列合并后仍然单调递减（交叉递减）
     *
     * 互递归场景中，函数A调用函数B，函数B调用函数A，
     * 因此需要模拟交叉调用时的测度递减：
     * A的最后一个测度值 > B的第一个测度值 > B的最后一个测度值 > A的下一个测度值 ...
     *
     * 简化处理：验证 A的最后一个值 > B的第一个值，以及 B的最后一个值 > A的第一个值
     */
    if (ctx_a->measure_value_count > 0 && ctx_b->measure_value_count > 0) {
        /* A的最后一个测度值应该大于B的第一个测度值（A调用B时测度递减） */
        SymbolicCoord *a_last = ctx_a->measure_values[ctx_a->measure_value_count - 1];
        SymbolicCoord *b_first = ctx_b->measure_values[0];
        MeasureCompareResult cross_cmp_1 = measure_compare(measure, b_first, a_last);
        if (cross_cmp_1 != MEASURE_LESS) {
            return false; /* 交叉递减不满足 */
        }

        /* B的最后一个测度值应该大于A的第一个测度值（B调用A时测度递减） */
        SymbolicCoord *b_last = ctx_b->measure_values[ctx_b->measure_value_count - 1];
        SymbolicCoord *a_first = ctx_a->measure_values[0];
        MeasureCompareResult cross_cmp_2 = measure_compare(measure, a_first, b_last);
        if (cross_cmp_2 != MEASURE_LESS) {
            return false; /* 交叉递减不满足 */
        }
    }

    return true;
}
