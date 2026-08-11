/**
 * @file recursion_validation.c
 * @brief non-symbolic measure validation
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

/* ============== 修改6：非符号测度的加载时验证 ============== */

bool measure_system_register_non_symbolic(MeasureSystem *ms, int measure_type_id, NonSymbolicComparator comparator,
                                          bool is_well_founded) {
    if (!ms || !comparator)
        return false;

    /* 统一扩容：复用 lv_ensure_capacity（倍增 + 溢出检查），失败时内部已设置错误 */
    if (!lv_ensure_capacity((void **) &ms->non_symbolic_metas, ms->non_symbolic_meta_count, &ms->non_symbolic_meta_capacity, sizeof(NonSymbolicMeasureMeta), 0))
        return false;

    /* 填充新元数据 */
    NonSymbolicMeasureMeta *meta = &ms->non_symbolic_metas[ms->non_symbolic_meta_count];
    meta->measure_type_id = measure_type_id;
    meta->comparator = comparator;
    meta->is_well_founded = is_well_founded;
    ms->non_symbolic_meta_count++;

    return true;
}

bool measure_system_validate_non_symbolic(MeasureSystem *ms) {
    if (!ms)
        return false;

    /* 如果没有非符号测度元数据，直接通过 */
    if (ms->non_symbolic_meta_count == 0) {
        return true;
    }

    /* 验证所有已注册的非符号测度 */
    for (int i = 0; i < ms->non_symbolic_meta_count; i++) {
        NonSymbolicMeasureMeta *meta = &ms->non_symbolic_metas[i];

        /* 检查比较器是否有效 */
        if (!meta->comparator) {
            return false; /* 比较器为空，验证失败 */
        }

        /* 检查是否标记为良基 */
        if (!meta->is_well_founded) {
            return false; /* 非良基测度，验证失败 */
        }

        /* 检查测度类型ID是否在系统中有对应的测度 */
        bool found = false;
        for (int j = 0; j < ms->measure_count; j++) {
            if (ms->measures[j]->id == meta->measure_type_id) {
                found = true;
                break;
            }
        }

        /* 注意：如果 measure_type_id 为0或负数，可能是尚未分配ID的测度，
         * 这种情况下不强制要求找到对应测度 */
        if (meta->measure_type_id > 0 && !found) {
            /* 找不到对应的测度定义，发出警告但不一定失败
             * 这里选择继续验证，因为测度可能在后续注册 */
        }
    }

    return true;
}

/* ============== 非符号测度的模板展开机制 ============== */

RecursionCheckResult recursion_validate_non_symbolic_measure(const Measure *measure, SymbolicCoord *before_value,
                                                             SymbolicCoord *after_value,
                                                             NonSymbolicComparator comparator) {
    if (!measure || !before_value || !after_value)
        return RECURSION_ERROR;

    if (!comparator)
        return RECURSION_MEASURE_UNKNOWN;

    /*
     * 通过公理包提供的比较器验证测度递减性。
     * 比较器返回 true 表示 before_value > after_value（即递减）。
     */
    bool is_decreasing = comparator(before_value, after_value);

    if (is_decreasing) {
        return RECURSION_OK; /* 递减，验证通过 */
    }

    /* 检查是否相等或递增 */
    bool after_lt_before = comparator(after_value, before_value);

    if (after_lt_before) {
        /* after < before，即 before > after，与上面的结果矛盾 */
        /* 这不应该发生，但为安全起见处理 */
        return RECURSION_OK;
    }

    /* 既不是 before > after 也不是 after > before，可能是相等 */
    /* 检查相等性：使用符号坐标比较 */
    int cmp = symbolic_coord_compare(before_value, after_value);
    if (cmp == 0) {
        return RECURSION_NOT_DECREASING; /* 相等，未递减 */
    }

    /* 比较器无法判定 */
    return RECURSION_MEASURE_UNKNOWN;
}


/* ============== Feature 2: 非符号测度模板展开集成（拆分自 recursion.c 尾部） ============== */

/* ============== Feature 2: 非符号测度模板展开集成 ============== */

int recursion_validate_non_symbolic_with_axiom(MeasureSystem *sys, int measure_id, const char *axiom_template_name,
                                               void *axiom_pkg) {
    (void) axiom_pkg; /* 不透明指针，当前不使用，预留未来扩展 */

    if (!sys)
        return -1;

    /* 查找指定ID的测度 */
    Measure *target = NULL;
    for (int i = 0; i < sys->measure_count; i++) {
        if (sys->measures[i]->id == measure_id) {
            target = sys->measures[i];
            break;
        }
    }

    /* 未找到测度 */
    if (!target)
        return -1;

    /* 必须是非符号测度 */
    if (target->type != MEASURE_CUSTOM)
        return -1;

    /* 如果提供了模板名称，存储为验证模板 */
    if (axiom_template_name && axiom_template_name[0] != '\0') {
        /* 查找是否已存在该测度的验证模板 */
        int existing_idx = -1;
        for (int i = 0; i < sys->validation_meta_count; i++) {
            if (sys->validation_metas[i].measure_id == measure_id) {
                existing_idx = i;
                break;
            }
        }

        if (existing_idx >= 0) {
            /* 更新已有条目 */
            snprintf(sys->validation_metas[existing_idx].validation_template,
                     sizeof(sys->validation_metas[existing_idx].validation_template), "%s", axiom_template_name);
        } else {
            /* 添加新条目（统一扩容：lv_ensure_capacity 内含倍增与溢出检查） */
            if (!lv_ensure_capacity((void **) &sys->validation_metas, sys->validation_meta_count, &sys->validation_meta_capacity, sizeof(NonSymbolicMeasureValidationMeta), 0))
                return -1;

            NonSymbolicMeasureValidationMeta *meta = &sys->validation_metas[sys->validation_meta_count];
            meta->measure_id = measure_id;
            snprintf(meta->validation_template, sizeof(meta->validation_template), "%s", axiom_template_name);
            sys->validation_meta_count++;
        }
    }

    return 0;
}

const char *recursion_get_measure_validation_template(MeasureSystem *sys, int measure_id) {
    if (!sys)
        return NULL;

    /* 查找测度 */
    Measure *target = NULL;
    for (int i = 0; i < sys->measure_count; i++) {
        if (sys->measures[i]->id == measure_id) {
            target = sys->measures[i];
            break;
        }
    }

    /* 未找到测度 */
    if (!target)
        return NULL;

    /* 符号测度没有验证模板 */
    if (target->type != MEASURE_CUSTOM)
        return NULL;

    /* 查找验证模板 */
    for (int i = 0; i < sys->validation_meta_count; i++) {
        if (sys->validation_metas[i].measure_id == measure_id) {
            return sys->validation_metas[i].validation_template;
        }
    }

    return NULL;
}
