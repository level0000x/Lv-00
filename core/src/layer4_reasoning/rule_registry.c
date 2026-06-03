/**
 * @file rule_registry.c
 * @brief 动态定理/规则注册表实现
 *
 * @details 使用动态数组实现规则注册表，支持增删查改和批量应用。
 *          规则按优先级排序存储，批量应用时按优先级顺序依次尝试。
 *
 * @author Lv-00 Project
 * @version 3.5.0
 */

#include "rule_registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * 内部数据结构
 * ================================================================ */

/**
 * @brief 注册表内部结构
 */
struct Lv00RuleRegistry {
    Lv00Rule *rules;  /**< 规则动态数组 */
    int count;        /**< 当前规则数量 */
    int capacity;     /**< 数组容量 */
};

/** @brief 初始容量 */
#define RULE_REGISTRY_INIT_CAPACITY 16

/** @brief 容量增长因子 */
#define RULE_REGISTRY_GROW_FACTOR 2

/* ================================================================
 * 内部辅助函数
 * ================================================================ */

/**
 * @brief 按优先级排序规则数组（插入排序，适用于小规模数据）
 *
 * @param rules 规则数组
 * @param count 规则数量
 */
static void rule_registry_sort_by_priority(Lv00Rule *rules, int count) {
    for (int i = 1; i < count; i++) {
        Lv00Rule key = rules[i];
        int j = i - 1;
        while (j >= 0 && rules[j].priority > key.priority) {
            rules[j + 1] = rules[j];
            j--;
        }
        rules[j + 1] = key;
    }
}

/**
 * @brief 深拷贝一条规则
 *
 * 复制所有字段，字符串通过 strdup 复制。
 *
 * @param src 源规则
 * @param dst 目标规则
 * @return 0 成功，-1 内存不足
 */
static int rule_deep_copy(const Lv00Rule *src, Lv00Rule *dst) {
    dst->name = src->name ? strdup(src->name) : NULL;
    dst->description = src->description ? strdup(src->description) : NULL;
    dst->priority = src->priority;
    dst->enabled = src->enabled;
    dst->can_apply = src->can_apply;
    dst->apply = src->apply;
    dst->user_data = src->user_data;

    if (src->name && !dst->name) {
        return -1;
    }
    if (src->description && !dst->description) {
        free((void *)dst->name);
        dst->name = NULL;
        return -1;
    }
    return 0;
}

/**
 * @brief 释放规则中动态分配的字符串
 *
 * @param rule 规则指针
 */
static void rule_cleanup(Lv00Rule *rule) {
    if (rule->name) {
        free((void *)rule->name);
        rule->name = NULL;
    }
    if (rule->description) {
        free((void *)rule->description);
        rule->description = NULL;
    }
}

/**
 * @brief 确保注册表有足够的容量
 *
 * @param registry 注册表
 * @param needed   需要的最小容量
 * @return 0 成功，-1 内存不足
 */
static int rule_registry_ensure_capacity(Lv00RuleRegistry *registry, int needed) {
    if (needed <= registry->capacity) {
        return 0;
    }

    int new_capacity = registry->capacity * RULE_REGISTRY_GROW_FACTOR;
    if (new_capacity < needed) {
        new_capacity = needed;
    }

    Lv00Rule *new_rules = (Lv00Rule *)realloc(registry->rules,
                                               (size_t)new_capacity * sizeof(Lv00Rule));
    if (!new_rules) {
        return -1;
    }

    registry->rules = new_rules;
    registry->capacity = new_capacity;
    return 0;
}

/* ================================================================
 * 公开 API 实现
 * ================================================================ */

Lv00RuleRegistry *lv00_rule_registry_create(void) {
    Lv00RuleRegistry *registry = (Lv00RuleRegistry *)calloc(1, sizeof(Lv00RuleRegistry));
    if (!registry) {
        return NULL;
    }

    registry->capacity = RULE_REGISTRY_INIT_CAPACITY;
    registry->rules = (Lv00Rule *)calloc((size_t)registry->capacity, sizeof(Lv00Rule));
    if (!registry->rules) {
        free(registry);
        return NULL;
    }

    registry->count = 0;
    return registry;
}

void lv00_rule_registry_destroy(Lv00RuleRegistry *registry) {
    if (!registry) {
        return;
    }

    /* 释放每条规则的动态字符串 */
    for (int i = 0; i < registry->count; i++) {
        rule_cleanup(&registry->rules[i]);
    }

    free(registry->rules);
    registry->rules = NULL;
    free(registry);
}

int lv00_rule_registry_add(Lv00RuleRegistry *registry, const Lv00Rule *rule) {
    if (!registry || !rule || !rule->name) {
        return -1;
    }

    /* 检查同名规则是否已存在 */
    for (int i = 0; i < registry->count; i++) {
        if (registry->rules[i].name && strcmp(registry->rules[i].name, rule->name) == 0) {
            return -2; /* 同名规则已存在 */
        }
    }

    /* 扩容 */
    if (rule_registry_ensure_capacity(registry, registry->count + 1) != 0) {
        return -3; /* 内存不足 */
    }

    /* 深拷贝规则 */
    if (rule_deep_copy(rule, &registry->rules[registry->count]) != 0) {
        return -3; /* 内存不足 */
    }

    registry->count++;

    /* 按优先级重新排序 */
    rule_registry_sort_by_priority(registry->rules, registry->count);

    return 0;
}

bool lv00_rule_registry_remove(Lv00RuleRegistry *registry, const char *name) {
    if (!registry || !name) {
        return false;
    }

    for (int i = 0; i < registry->count; i++) {
        if (registry->rules[i].name && strcmp(registry->rules[i].name, name) == 0) {
            /* 释放该规则的动态字符串 */
            rule_cleanup(&registry->rules[i]);

            /* 将后续规则前移 */
            for (int j = i; j < registry->count - 1; j++) {
                registry->rules[j] = registry->rules[j + 1];
            }

            registry->count--;

            /* 清零最后一个槽位（防止悬垂指针） */
            memset(&registry->rules[registry->count], 0, sizeof(Lv00Rule));
            return true;
        }
    }

    return false; /* 未找到 */
}

int lv00_rule_registry_count(const Lv00RuleRegistry *registry) {
    if (!registry) {
        return 0;
    }
    return registry->count;
}

const Lv00Rule *lv00_rule_registry_get(const Lv00RuleRegistry *registry, int index) {
    if (!registry || index < 0 || index >= registry->count) {
        return NULL;
    }
    return &registry->rules[index];
}

const Lv00Rule *lv00_rule_registry_find(const Lv00RuleRegistry *registry, const char *name) {
    if (!registry || !name) {
        return NULL;
    }

    for (int i = 0; i < registry->count; i++) {
        if (registry->rules[i].name && strcmp(registry->rules[i].name, name) == 0) {
            return &registry->rules[i];
        }
    }

    return NULL;
}

bool lv00_rule_registry_enable(Lv00RuleRegistry *registry, const char *name, bool enabled) {
    if (!registry || !name) {
        return false;
    }

    for (int i = 0; i < registry->count; i++) {
        if (registry->rules[i].name && strcmp(registry->rules[i].name, name) == 0) {
            registry->rules[i].enabled = enabled;
            return true;
        }
    }

    return false;
}

int lv00_rule_registry_apply_all(Lv00RuleRegistry *registry, void *context, void *proposition,
                                  void **results, int max_results) {
    if (!registry || !proposition || !results || max_results <= 0) {
        return -1;
    }

    int total_results = 0;

    /* 规则已按优先级排序，直接遍历即可 */
    for (int i = 0; i < registry->count; i++) {
        Lv00Rule *rule = &registry->rules[i];

        /* 跳过已禁用的规则 */
        if (!rule->enabled) {
            continue;
        }

        /* 跳过缺少回调的规则 */
        if (!rule->can_apply || !rule->apply) {
            continue;
        }

        /* 检查规则是否适用 */
        if (!rule->can_apply(context, proposition)) {
            continue;
        }

        /* 应用规则 */
        void *rule_results = NULL;
        int rule_result_count = 0;

        int rc = rule->apply(context, proposition, &rule_results, &rule_result_count);
        if (rc != 0 || rule_result_count <= 0) {
            continue;
        }

        /* 收集结果（rule_results 是一个指针数组，由 apply 回调分配） */
        void **rule_result_array = (void **)rule_results;
        for (int j = 0; j < rule_result_count && total_results < max_results; j++) {
            results[total_results++] = rule_result_array[j];
        }

        /* 释放回调分配的结果数组容器（不释放各个结果指针，由调用者管理） */
        free(rule_result_array);

        if (total_results >= max_results) {
            break; /* 结果数组已满 */
        }
    }

    return total_results;
}
