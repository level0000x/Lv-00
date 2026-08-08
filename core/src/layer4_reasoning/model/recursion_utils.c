/**
 * @file recursion_utils.c
 * @brief string conversion helpers
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

/* ============== 辅助函数 ============== */

/** @brief measure_type_to_string 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_measure_type_to_string_entries[] = {
    {"Symbolic", MEASURE_SYMBOLIC},
    {"Custom", MEASURE_CUSTOM},
};

const char *measure_type_to_string(MeasureType type) {
    return lv_enum_to_str(s_measure_type_to_string_entries, lv_ARRAY_SIZE(s_measure_type_to_string_entries),
                          (int) type, "Unknown");
}

/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch）
 * ================================================================ */

/** @brief measure_compare_result_to_string 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_measure_compare_result_to_string_entries[] = {
    {"Less", MEASURE_LESS},
    {"Equal", MEASURE_EQUAL},
    {"Greater", MEASURE_GREATER},
    {"Unknown", MEASURE_UNKNOWN},
    {"Error", MEASURE_ERROR},
};

const char *measure_compare_result_to_string(MeasureCompareResult result) {
    return lv_enum_to_str(s_measure_compare_result_to_string_entries, lv_ARRAY_SIZE(s_measure_compare_result_to_string_entries), (int) result, "Unknown");
}

/** @brief recursion_check_result_to_string 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_recursion_check_result_to_string_entries[] = {
    {"OK", RECURSION_OK},
    {"Not Decreasing", RECURSION_NOT_DECREASING},
    {"Depth Exceeded", RECURSION_DEPTH_EXCEEDED},
    {"Cycle Detected", RECURSION_CYCLE_DETECTED},
    {"Measure Unknown", RECURSION_MEASURE_UNKNOWN},
    {"Error", RECURSION_ERROR},
};

const char *recursion_check_result_to_string(RecursionCheckResult result) {
    return lv_enum_to_str(s_recursion_check_result_to_string_entries, lv_ARRAY_SIZE(s_recursion_check_result_to_string_entries), (int) result, "Unknown");
}

/** @brief branch_state_to_string 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_branch_state_to_string_entries[] = {
    {"Inactive", BRANCH_INACTIVE},
    {"Active", BRANCH_ACTIVE},
    {"Pending", BRANCH_PENDING},
    {"Shadowed", BRANCH_SHADOWED},
};

const char *branch_state_to_string(BranchState state) {
    return lv_enum_to_str(s_branch_state_to_string_entries, lv_ARRAY_SIZE(s_branch_state_to_string_entries), (int) state, "Unknown");
}
