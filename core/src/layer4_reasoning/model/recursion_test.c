/**
 * @file recursion_test.c
 * @brief measure validation test suite
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

/* ============== 加载时验证的完整测试集 ============== */

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

            switch (cmp) {
                case MEASURE_LESS:
                    results[i].passed = true;
                    break;
                case MEASURE_EQUAL:
                    results[i].passed = false;
                    results[i].error_message = lv_strdup("Measure values are equal (not decreasing)");
                    all_passed = false;
                    break;
                case MEASURE_GREATER:
                    results[i].passed = false;
                    results[i].error_message = lv_strdup("Measure value increased (not decreasing)");
                    all_passed = false;
                    break;
                case MEASURE_UNKNOWN:
                    results[i].passed = false;
                    results[i].error_message = lv_strdup("Cannot determine measure comparison");
                    all_passed = false;
                    break;
                case MEASURE_ERROR:
                default:
                    results[i].passed = false;
                    results[i].error_message = lv_strdup("Error comparing measure values");
                    all_passed = false;
                    break;
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
