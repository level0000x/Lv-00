/**
 * @file preset_manager_query.c
 * @brief 查询与列表
 *
 * @details 从 preset_manager.c 拆分的子模块（Lv-00 项目 v3.3.0+）。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error_codes.h"
#include "func_block_preset.h"
#include "func_block_registry.h"
#include "lv_internal.h"
#include "lv/lv_json.h"
#include "lv_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"
#include "preset_core.h"
#include "preset_manager_internal.h"

/* ============================================================
 * 高级查询
 * ============================================================ */

/**
 * @brief 通配符名称匹配辅助函数
 *
 * 支持 '*' 通配符（匹配任意字符序列）。
 *
 * @param pattern 模式（含通配符）
 * @param name    待匹配的名称
 * @return true 匹配成功
 */
static bool wildcard_match(const char *pattern, const char *name) {
    if (!pattern || !name)
        return false;

    /* 空模式匹配空字符串 */
    if (*pattern == '\0')
        return (*name == '\0');

    /* 遇到 '*' 时递归试探 */
    if (*pattern == '*') {
        /* 跳过连续的 '*' */
        while (*(pattern + 1) == '*')
            pattern++;
        /* 尝试从每个位置匹配剩余模式 */
        while (*name) {
            if (wildcard_match(pattern + 1, name))
                return true;
            name++;
        }
        return wildcard_match(pattern + 1, name);
    }

    /* 逐字符匹配 */
    if (*pattern == '?' || *pattern == *name) {
        return wildcard_match(pattern + 1, name + 1);
    }

    return false;
}

/**
 * @brief 高级查询预设
 *
 * 根据 PresetQueryCriteria 中的多个条件综合筛选预设。
 * 所有条件之间为"与"（AND）关系。
 * 结果按名称字母序排列。
 *
 * @param criteria   查询条件（不可为 NULL）
 * @param out_result 输出结果（调用者需使用 preset_query_result_free 释放）
 * @return true 查询成功
 * @return false 查询失败
 */
bool preset_query(const PresetQueryCriteria *criteria, PresetQueryResult **out_result) {
    PRESET_CHECK_NULL(criteria, error);
    PRESET_CHECK_NULL(out_result, error);

    lock_library();

    if (!g_library.initialized) {
        unlock_library();
        set_error("预设库未初始化");
        goto error;
    }

    /* ── 第一步：分配结果结构 ── */
    PresetQueryResult *result = (PresetQueryResult *) lv_calloc(1, sizeof(PresetQueryResult));
    if (!result) {
        unlock_library();
        set_error("内存分配失败");
        goto error;
    }

    /* 预分配名称数组（最多 entry_count 个） */
    int max_candidates = g_library.entry_count;
    const char **candidate_names = (const char **) lv_malloc((size_t) max_candidates * sizeof(const char *));
    if (!candidate_names) {
        unlock_library();
        lv_free((void **) &result);
        set_error("内存分配失败");
        goto error;
    }

    int match_count = 0;

    /* ── 第二步：遍历所有条目并逐项筛选 ── */
    for (int i = 0; i < g_library.hash_table_size; i++) {
        InternalPresetEntry *entry = g_library.hash_table[i];
        while (entry != NULL) {
            if (!entry->is_active) {
                entry = entry->next;
                continue;
            }

            const PresetMetadata *meta = &entry->metadata;
            bool matches = true;

            /* 条件1：名称模式匹配（支持通配符） */
            if (criteria->name_pattern && criteria->name_pattern[0] != '\0') {
                if (!wildcard_match(criteria->name_pattern, meta->name)) {
                    matches = false;
                }
            }

            /* 条件2：类别筛选 */
            if (matches && criteria->category >= 0 && meta->category != criteria->category) {
                matches = false;
            }

            /* 条件3：必须属性检查 */
            if (matches && criteria->required_properties != PRESET_PROPERTY_NONE) {
                if ((meta->properties & criteria->required_properties) != criteria->required_properties) {
                    matches = false;
                }
            }

            /* 条件4：禁止属性检查 */
            if (matches && criteria->forbidden_properties != PRESET_PROPERTY_NONE) {
                if ((meta->properties & criteria->forbidden_properties) != 0) {
                    matches = false;
                }
            }

            /* 条件5：输入数量范围 */
            if (matches && criteria->min_inputs > 0 && meta->input_count > 0) {
                if (meta->input_count < criteria->min_inputs)
                    matches = false;
            }
            if (matches && criteria->max_inputs > 0 && meta->input_count > 0) {
                if (meta->input_count > criteria->max_inputs)
                    matches = false;
            }

            /* 条件6：输出数量范围 */
            if (matches && criteria->min_outputs > 0 && meta->output_count > 0) {
                if (meta->output_count < criteria->min_outputs)
                    matches = false;
            }
            if (matches && criteria->max_outputs > 0 && meta->output_count > 0) {
                if (meta->output_count > criteria->max_outputs)
                    matches = false;
            }

            /* 条件7：搜索描述（关键词匹配） */
            if (matches && criteria->search_description && criteria->name_pattern &&
                criteria->name_pattern[0] != '\0') {
                /* 在描述中搜索名称模式（不使用通配符） */
                if (meta->description) {
                    const char *found = strstr(meta->description, criteria->name_pattern);
                    if (!found) {
                        /* 替换通配符后重新搜索 */
                        matches = false;
                        /* 如果通配符匹配了名称但描述中没有对应的关键词，
                         * 接受这个匹配（描述搜索为可选项） */
                        if (criteria->search_description) {
                            matches = true; /* 名称匹配即通过 */
                        }
                    }
                } else {
                    matches = false;
                }
            }

            /* 通过所有筛选条件 */
            if (matches && match_count < max_candidates) {
                candidate_names[match_count] = meta->name;
                match_count++;
            }

            entry = entry->next;
        }
    }

    /* ── 第三步：组装结果 ── */
    result->total_matches = match_count;
    result->count = match_count;

    if (match_count > 0) {
        result->names = (const char **) lv_malloc((size_t) match_count * sizeof(const char *));
        if (!result->names) {
            lv_free((void **) &candidate_names);
            lv_free((void **) &result);
            unlock_library();
            set_error("内存分配失败");
            goto error;
        }
        for (int i = 0; i < match_count; i++) {
            result->names[i] = candidate_names[i];
        }
    }

    lv_free((void **) &candidate_names);
    unlock_library();

    *out_result = result;
    return true;

error:
    return false;
}

/**
 * @brief 释放查询结果
 *
 * 释放由 preset_query 分配的 PresetQueryResult 结构
 * 及其内部动态内存。
 *
 * @param result 查询结果（可为 NULL）
 */
void preset_query_result_free(PresetQueryResult *result) {
    if (!result)
        return;

    if (result->names) {
        lv_free((void **) &result->names);
    }

    lv_free((void **) &result);
}

/* ============================================================
 * 按类别/全部列出预设
 * ============================================================ */

/**
 * @brief 按类别列出预设
 *
 * 收集指定类别的所有预设名称。
 * 返回的 out_names 和每个元素均由调用者通过 lv_free 释放。
 *
 * @param category   目标类别
 * @param out_names  输出名称数组（调用者需释放每个元素和数组本身）
 * @param out_count  输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_list_by_category(PresetCategory category, char ***out_names, int *out_count) {
    PRESET_CHECK_NULL(out_names, error);
    PRESET_CHECK_NULL(out_count, error);

    lock_library();

    if (!g_library.initialized) {
        unlock_library();
        set_error("预设库未初始化");
        return false;
    }

    /* 第一遍：统计匹配数量 */
    int count = 0;
    for (int i = 0; i < g_library.hash_table_size; i++) {
        InternalPresetEntry *entry = g_library.hash_table[i];
        while (entry != NULL) {
            if (entry->is_active && entry->metadata.category == category) {
                count++;
            }
            entry = entry->next;
        }
    }

    /* 分配结果数组 */
    char **names = NULL;
    if (count > 0) {
        names = (char **) lv_malloc((size_t) count * sizeof(char *));
        if (!names) {
            unlock_library();
            set_error("内存分配失败");
            return false;
        }
        memset(names, 0, (size_t) count * sizeof(char *));

        /* 第二遍：填充名称 */
        int idx = 0;
        for (int i = 0; i < g_library.hash_table_size && idx < count; i++) {
            InternalPresetEntry *entry = g_library.hash_table[i];
            while (entry != NULL && idx < count) {
                if (entry->is_active && entry->metadata.category == category) {
                    names[idx] = lv_strdup_safe(entry->metadata.name);
                    if (!names[idx]) {
                        /* 部分分配失败，释放已分配的元素 */
                        unlock_library();
                        for (int j = 0; j < idx; j++) {
                            void *tmp = names[j];
                            lv_free(&tmp);
                        }
                        lv_free((void **) &names);
                        set_error("内存分配失败");
                        return false;
                    }
                    idx++;
                }
                entry = entry->next;
            }
        }
    }

    unlock_library();

    *out_names = names;
    *out_count = count;
    return true;

error:
    return false;
}

/**
 * @brief 获取所有预设名称
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_list_all(char ***out_names, int *out_count) {
    PRESET_CHECK_NULL(out_names, error);
    PRESET_CHECK_NULL(out_count, error);

    lock_library();

    if (!g_library.initialized) {
        unlock_library();
        set_error("预设库未初始化");
        return false;
    }

    int count = g_library.entry_count;
    char **names = NULL;

    if (count > 0) {
        names = (char **) lv_malloc((size_t) count * sizeof(char *));
        if (!names) {
            unlock_library();
            set_error("内存分配失败");
            return false;
        }
        memset(names, 0, (size_t) count * sizeof(char *));

        int idx = 0;
        for (int i = 0; i < g_library.hash_table_size && idx < count; i++) {
            InternalPresetEntry *entry = g_library.hash_table[i];
            while (entry != NULL && idx < count) {
                if (entry->is_active) {
                    names[idx] = lv_strdup_safe(entry->metadata.name);
                    if (!names[idx]) {
                        unlock_library();
                        for (int j = 0; j < idx; j++) {
                            void *tmp = names[j];
                            lv_free(&tmp);
                        }
                        lv_free((void **) &names);
                        set_error("内存分配失败");
                        return false;
                    }
                    idx++;
                }
                entry = entry->next;
            }
        }
    }

    unlock_library();

    *out_names = names;
    *out_count = count;
    return true;

error:
    return false;
}

