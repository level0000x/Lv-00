/**
 * @file recursion_test.c
 * @brief measure validation test suite
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

/* ============== 加载时验证的完整测试集 ============== */

/* 比较结果 → 测试判定 查找表条目 */
typedef struct {
    bool passed;         /**< 该比较结果是否判定为通过 */
    const char *message; /**< 失败时的错误消息（通过时为 NULL） */
} MeasureCompareTestEntry;

/* 比较结果判定静态查找表（越界按比较出错处理） */
static const MeasureCompareTestEntry kCompareTestTable[] = {
    [MEASURE_LESS]    = { true,  NULL },                                        /* 严格递减 → 通过 */
    [MEASURE_EQUAL]   = { false, "Measure values are equal (not decreasing)" }, /* 相等 → 未递减 */
    [MEASURE_GREATER] = { false, "Measure value increased (not decreasing)" },  /* 增大 → 未递减 */
    [MEASURE_UNKNOWN] = { false, "Cannot determine measure comparison" },       /* 无法比较 */
    [MEASURE_ERROR]   = { false, "Error comparing measure values" },            /* 比较出错 */
};

bool recursion_run_measure_tests(const Measure *measure, int test_count, SymbolicCoord ***test_before_values,
                                 SymbolicCoord ***test_after_values, MeasureTestResult *results) {
    if (!measure || test_count <= 0 || !test_before_values || !test_after_values || !results) {
        return false;
    }

    bool all_passed = true;

    for (int i = 0; i < test_count; i++) {
        results[i].passed = false;
        results[i].test_name = NULL;
        results[i].error_message = NULL;

        SymbolicCoord *before = test_before_values[i] ? test_before_values[i][0] : NULL;
        SymbolicCoord *after = test_after_values[i] ? test_after_values[i][0] : NULL;

        if (!before || !after) {
            results[i].passed = false;
            results[i].error_message = lv_strdup("NULL measure value in test case");
            all_passed = false;
            continue;
        }

        if (measure->type == MEASURE_SYMBOLIC) {
            /* 符号测度：使用符号坐标比较 */
            MeasureCompareResult cmp = measure_compare((Measure *) measure, after, before);

            /* 查表取得比较结果判定，越界（含负值）按比较出错处理 */
            const MeasureCompareTestEntry *entry = NULL;
            if ((unsigned)cmp < sizeof(kCompareTestTable) / sizeof(kCompareTestTable[0]))
                entry = &kCompareTestTable[cmp];

            if (!entry || !entry->passed) {
                results[i].passed = false;
                results[i].error_message = lv_strdup(entry ? entry->message : "Error comparing measure values");
                all_passed = false;
            } else {
                results[i].passed = true; /* error_message 保持 NULL */
            }
        } else if (measure->type == MEASURE_CUSTOM) {
            /* 非符号测度：使用自定义比较函数 */
            if (measure->compare_func) {
                /*
                 * 非符号测度的比较需要 GeomNode 参数，
                 * 但测试集提供的是 SymbolicCoord 值。
                 * 这里我们无法直接使用 compare_func，
                 * 标记为需要通过公理包的模板展开来验证。
                 */
                results[i].passed = false;
                results[i].error_message = lv_strdup("Non-symbolic measure requires axiom pack template expansion");
                all_passed = false;
            } else {
                results[i].passed = false;
                results[i].error_message = lv_strdup("No comparator for custom measure");
                all_passed = false;
            }
        }
    }

    return all_passed;
}
