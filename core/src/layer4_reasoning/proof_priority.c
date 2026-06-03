/**
 * @file proof_priority.c
 * @brief 推理优先级系统实现 —— 优先级标记、调度器与规则管理
 *
 * @details 实现基于四级优先级的推理规则调度系统，支持规则的注册、
 *          优先级动态调整、耗尽标记和按优先级顺序的调度。
 *          用于在自动证明搜索中优先使用高价值规则，缩小搜索空间。
 *
 *          核心功能模块：
 *          - 优先级标记：创建/销毁/应用记录/耗尽标记
 *          - 优先级调度器：四级队列管理，按 URGENT > HIGH > NORMAL > LOW 调度
 *          - 动态重优先级：运行时调整规则的优先级等级
 *          - 统计查询：查看调度器状态和规则使用情况
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#include "proof_priority.h"

#include <string.h>

#include "lv00_utils.h"

/* ============== 内部常量 ============== */

/** 队列初始容量 */
#define PRIORITY_QUEUE_CAP 16

/** 规则名称最大复制长度 */
#define PRIORITY_NAME_MAX 128

/* ============== 优先级标记 API ============== */

/**
 * @brief 创建优先级标记
 *
 * 分配并初始化优先级标记，内部复制规则名称。weight 被截断到 [0, 100]。
 *
 * @param rule_name       规则名称（不可为空）
 * @param rule_id         规则ID
 * @param priority        优先级等级
 * @param weight          权重系数（0-100）
 * @param max_applications 最大应用次数（0=无限制）
 * @param scheduler_hint  调度提示（可为 NULL）
 * @return 新分配的优先级标记，失败返回 NULL
 */
Lv00PriorityTag *lv00_priority_tag_create(const char *rule_name, int rule_id,
                                           Lv00TheoremPriority priority, int weight,
                                           int max_applications, const char *scheduler_hint) {
    if (!rule_name)
        return NULL;

    Lv00PriorityTag *tag = lv00_calloc(1, sizeof(Lv00PriorityTag));
    if (!tag)
        return NULL;

    tag->rule_id = rule_id;

    /* 安全复制规则名称 */
    size_t name_len = strlen(rule_name);
    if (name_len >= PRIORITY_NAME_MAX) {
        name_len = PRIORITY_NAME_MAX - 1;
    }
    memcpy(tag->rule_name, rule_name, name_len);
    tag->rule_name[name_len] = '\0';

    tag->priority = priority;

    /* 权重截断到 [0, 100] */
    if (weight < 0)  tag->weight = 0;
    else if (weight > 100) tag->weight = 100;
    else tag->weight = weight;

    tag->is_exhausted = false;
    tag->apply_count = 0;
    tag->max_applications = max_applications;

    if (scheduler_hint) {
        tag->scheduler_hint = lv00_strdup(scheduler_hint);
    }

    return tag;
}

/**
 * @brief 销毁优先级标记
 *
 * @param tag  优先级标记指针（可为 NULL）
 */
void lv00_priority_tag_destroy(Lv00PriorityTag *tag) {
    if (!tag)
        return;
    lv00_free((void **) &tag->scheduler_hint);
    lv00_free((void **) &tag);
}

/* ============== 调度器辅助函数 ============== */

/**
 * @brief 根据优先级等级获取对应的队列指针
 *
 * @param scheduler  调度器
 * @param priority   优先级等级
 * @return 指向对应队列数组的指针
 */
static Lv00PriorityTag ***get_queue_for_priority(Lv00PriorityScheduler *scheduler,
                                                  Lv00TheoremPriority priority) {
    switch (priority) {
        case PRIORITY_URGENT: return &scheduler->urgent_queue;
        case PRIORITY_HIGH:   return &scheduler->high_queue;
        case PRIORITY_NORMAL: return &scheduler->normal_queue;
        case PRIORITY_LOW:    return &scheduler->low_queue;
        default:              return NULL;
    }
}

/**
 * @brief 根据优先级等级获取对应的队列计数指针
 *
 * @param scheduler  调度器
 * @param priority   优先级等级
 * @return 指向对应计数字段的指针
 */
static int *get_count_for_priority(Lv00PriorityScheduler *scheduler,
                                    Lv00TheoremPriority priority) {
    switch (priority) {
        case PRIORITY_URGENT: return &scheduler->urgent_count;
        case PRIORITY_HIGH:   return &scheduler->high_count;
        case PRIORITY_NORMAL: return &scheduler->normal_count;
        case PRIORITY_LOW:    return &scheduler->low_count;
        default:              return NULL;
    }
}

/**
 * @brief 根据优先级等级获取对应的队列容量指针
 *
 * @param scheduler  调度器
 * @param priority   优先级等级
 * @return 指向对应容量字段的指针
 */
static int *get_capacity_for_priority(Lv00PriorityScheduler *scheduler,
                                       Lv00TheoremPriority priority) {
    switch (priority) {
        case PRIORITY_URGENT: return &scheduler->urgent_capacity;
        case PRIORITY_HIGH:   return &scheduler->high_capacity;
        case PRIORITY_NORMAL: return &scheduler->normal_capacity;
        case PRIORITY_LOW:    return &scheduler->low_capacity;
        default:              return NULL;
    }
}

/**
 * @brief 将优先级标记添加到指定队列
 *
 * 动态扩容队列，按初始顺序追加（不排序）。
 * 调用者需要之后调用排序（设置 sort_needed）。
 *
 * @param scheduler  调度器
 * @param tag        优先级标记
 * @param priority   目标优先级等级
 * @return true 成功
 */
static bool priority_queue_push(Lv00PriorityScheduler *scheduler,
                                 Lv00PriorityTag *tag, Lv00TheoremPriority priority) {
    int *count = get_count_for_priority(scheduler, priority);
    int *capacity = get_capacity_for_priority(scheduler, priority);
    Lv00PriorityTag ***queue = get_queue_for_priority(scheduler, priority);

    if (!count || !capacity || !queue)
        return false;

    /* 扩容 */
    if (*count >= *capacity) {
        int new_cap = (*capacity == 0) ? PRIORITY_QUEUE_CAP : (*capacity) * 2;
        Lv00PriorityTag **new_arr = lv00_realloc(*queue, new_cap * sizeof(Lv00PriorityTag *));
        if (!new_arr)
            return false;
        *queue = new_arr;
        *capacity = new_cap;
    }

    (*queue)[*count] = tag;
    (*count)++;
    return true;
}

/**
 * @brief 从队列中移除指定 rule_id 的标记
 *
 * 线性搜索队列，找到后向前移动后续元素。
 *
 * @param queue       队列数组
 * @param count       队列计数指针
 * @param rule_id     要移除的规则ID
 * @return true 找到并移除，false 未找到
 */
static bool priority_queue_remove(Lv00PriorityTag ***queue, int *count, int rule_id) {
    if (!queue || !count)
        return false;

    for (int i = 0; i < *count; i++) {
        if ((*queue)[i]->rule_id == rule_id) {
            /* 向前移动后续元素 */
            for (int j = i; j < *count - 1; j++) {
                (*queue)[j] = (*queue)[j + 1];
            }
            (*count)--;
            return true;
        }
    }
    return false;
}

/* ============== 调度器 API ============== */

/**
 * @brief 创建优先级调度器
 *
 * @return 新分配的调度器指针，失败返回 NULL
 */
Lv00PriorityScheduler *lv00_priority_scheduler_create(void) {
    Lv00PriorityScheduler *s = lv00_calloc(1, sizeof(Lv00PriorityScheduler));
    return s;
}

/**
 * @brief 销毁优先级调度器
 *
 * 遍历四个队列，销毁所有已注册的优先级标记，
 * 释放队列数组和调度器自身。
 *
 * @param scheduler  调度器指针（可为 NULL）
 */
void lv00_priority_scheduler_destroy(Lv00PriorityScheduler *scheduler) {
    if (!scheduler)
        return;

    /* 销毁所有注册的标记 */
    for (int i = 0; i < scheduler->urgent_count; i++)
        lv00_priority_tag_destroy(scheduler->urgent_queue[i]);
    for (int i = 0; i < scheduler->high_count; i++)
        lv00_priority_tag_destroy(scheduler->high_queue[i]);
    for (int i = 0; i < scheduler->normal_count; i++)
        lv00_priority_tag_destroy(scheduler->normal_queue[i]);
    for (int i = 0; i < scheduler->low_count; i++)
        lv00_priority_tag_destroy(scheduler->low_queue[i]);

    /* 释放队列数组 */
    lv00_free((void **) &scheduler->urgent_queue);
    lv00_free((void **) &scheduler->high_queue);
    lv00_free((void **) &scheduler->normal_queue);
    lv00_free((void **) &scheduler->low_queue);

    lv00_free((void **) &scheduler);
}

/**
 * @brief 向调度器注册一个优先级规则
 *
 * 根据优先级加入对应队列，更新 total_rules 计数。
 *
 * @param scheduler  调度器
 * @param tag        优先级标记
 * @return true 注册成功
 */
bool lv00_priority_scheduler_register(Lv00PriorityScheduler *scheduler, Lv00PriorityTag *tag) {
    if (!scheduler || !tag)
        return false;

    if (!priority_queue_push(scheduler, tag, tag->priority))
        return false;

    scheduler->total_rules++;
    scheduler->sort_needed = true;
    return true;
}

/**
 * @brief 获取下一个应执行的规则
 *
 * 按 URGENT > HIGH > NORMAL > LOW 顺序遍历四个队列，
 * 返回第一个未耗尽的规则。在同级别队列中，
 * 如果 sort_needed 为 true，按 weight 降序排列后再返回。
 *
 * @param scheduler  调度器
 * @return 下一个应执行的规则标记，无可用规则返回 NULL
 */
Lv00PriorityTag *lv00_priority_scheduler_next(Lv00PriorityScheduler *scheduler) {
    if (!scheduler)
        return NULL;

    /* 按优先级顺序检查四个队列 */
    static const Lv00TheoremPriority order[] = {
        PRIORITY_URGENT, PRIORITY_HIGH, PRIORITY_NORMAL, PRIORITY_LOW
    };

    for (int level = 0; level < 4; level++) {
        Lv00TheoremPriority pri = order[level];
        Lv00PriorityTag **queue = *get_queue_for_priority(scheduler, pri);
        int count = *get_count_for_priority(scheduler, pri);

        /* 简单冒泡排序：按 weight 降序 */
        if (scheduler->sort_needed && count > 1) {
            for (int i = 0; i < count - 1; i++) {
                for (int j = i + 1; j < count; j++) {
                    if (queue[i]->weight < queue[j]->weight) {
                        Lv00PriorityTag *tmp = queue[i];
                        queue[i] = queue[j];
                        queue[j] = tmp;
                    }
                }
            }
        }

        for (int i = 0; i < count; i++) {
            if (!queue[i]->is_exhausted) {
                scheduler->current_priority_level = (int) pri;
                return queue[i];
            }
        }
    }

    /* 所有规则都已耗尽 */
    scheduler->sort_needed = false;
    return NULL;
}

/**
 * @brief 标记一个规则为已耗尽
 *
 * @param tag  优先级标记
 */
void lv00_priority_tag_exhaust(Lv00PriorityTag *tag) {
    if (tag) {
        tag->is_exhausted = true;
    }
}

/**
 * @brief 记录规则被应用了一次
 *
 * 递增 apply_count，如果达到 max_applications（且不为 0）
 * 则自动标记为已耗尽。
 *
 * @param tag  优先级标记
 */
void lv00_priority_tag_record_application(Lv00PriorityTag *tag) {
    if (!tag)
        return;
    tag->apply_count++;
    if (tag->max_applications > 0 && tag->apply_count >= tag->max_applications) {
        tag->is_exhausted = true;
    }
}

/**
 * @brief 动态调整规则的优先级
 *
 * 从旧队列中移除规则，将规则的 priority 字段更新为 new_priority，
 * 然后加入新队列对应的优先级级别。
 *
 * @param scheduler   调度器
 * @param rule_id     规则ID
 * @param new_priority 新的优先级等级
 * @return true 调整成功，false 未找到规则
 */
bool lv00_priority_scheduler_reprioritize(Lv00PriorityScheduler *scheduler,
                                           int rule_id, Lv00TheoremPriority new_priority) {
    if (!scheduler)
        return false;

    /* 在所有四个队列中查找 */
    static const Lv00TheoremPriority all_priorities[] = {
        PRIORITY_URGENT, PRIORITY_HIGH, PRIORITY_NORMAL, PRIORITY_LOW
    };

    Lv00PriorityTag *found = NULL;
    Lv00TheoremPriority old_priority = PRIORITY_NORMAL;

    for (int p = 0; p < 4; p++) {
        Lv00PriorityTag **queue = *get_queue_for_priority(scheduler, all_priorities[p]);
        int count = *get_count_for_priority(scheduler, all_priorities[p]);

        for (int i = 0; i < count; i++) {
            if (queue[i]->rule_id == rule_id) {
                found = queue[i];
                old_priority = all_priorities[p];
                break;
            }
        }
        if (found)
            break;
    }

    if (!found)
        return false;

    /* 如果新旧优先级相同，无需移动 */
    if (old_priority == new_priority)
        return true;

    /* 从旧队列移除 */
    {
        int *count = get_count_for_priority(scheduler, old_priority);
        Lv00PriorityTag ***queue = get_queue_for_priority(scheduler, old_priority);
        for (int i = 0; i < *count; i++) {
            if ((*queue)[i] == found) {
                for (int j = i; j < *count - 1; j++) {
                    (*queue)[j] = (*queue)[j + 1];
                }
                (*count)--;
                break;
            }
        }
    }

    /* 更新优先级并加入新队列 */
    found->priority = new_priority;
    if (!priority_queue_push(scheduler, found, new_priority))
        return false;

    scheduler->sort_needed = true;
    return true;
}

/**
 * @brief 重置调度器中的所有规则状态
 *
 * 将所有规则的 is_exhausted 设为 false，apply_count 清零。
 * 同时也重置 current_priority_level。
 *
 * @param scheduler  调度器
 */
void lv00_priority_scheduler_reset(Lv00PriorityScheduler *scheduler) {
    if (!scheduler)
        return;

    static const Lv00PriorityTag **all_queues[4];
    static const int *all_counts[4];

    /* 收集四个队列 */
    all_queues[0] = (const Lv00PriorityTag **) scheduler->urgent_queue;
    all_counts[0] = &scheduler->urgent_count;
    all_queues[1] = (const Lv00PriorityTag **) scheduler->high_queue;
    all_counts[1] = &scheduler->high_count;
    all_queues[2] = (const Lv00PriorityTag **) scheduler->normal_queue;
    all_counts[2] = &scheduler->normal_count;
    all_queues[3] = (const Lv00PriorityTag **) scheduler->low_queue;
    all_counts[3] = &scheduler->low_count;

    for (int q = 0; q < 4; q++) {
        int count = *all_counts[q];
        for (int i = 0; i < count; i++) {
            Lv00PriorityTag *tag = (Lv00PriorityTag *) all_queues[q][i];
            tag->is_exhausted = false;
            tag->apply_count = 0;
        }
    }

    scheduler->current_priority_level = 0;
    scheduler->sort_needed = true;
}

/* ============== 统计查询 ============== */

/**
 * @brief 获取调度器的统计信息
 *
 * @param scheduler            调度器
 * @param out_total_rules      输出：注册的规则总数
 * @param out_remaining_rules  输出：尚未耗尽的规则数量
 * @param out_current_level    输出：当前正在处理的优先级等级
 */
void lv00_priority_scheduler_get_stats(const Lv00PriorityScheduler *scheduler,
                                        int *out_total_rules, int *out_remaining_rules,
                                        int *out_current_level) {
    if (!scheduler) {
        if (out_total_rules)     *out_total_rules = 0;
        if (out_remaining_rules) *out_remaining_rules = 0;
        if (out_current_level)   *out_current_level = 0;
        return;
    }

    if (out_total_rules) *out_total_rules = scheduler->total_rules;
    if (out_current_level) *out_current_level = scheduler->current_priority_level;

    /* 统计尚未耗尽的规则数 */
    if (out_remaining_rules) {
        int remaining = 0;
        for (int i = 0; i < scheduler->urgent_count; i++)
            if (!scheduler->urgent_queue[i]->is_exhausted) remaining++;
        for (int i = 0; i < scheduler->high_count; i++)
            if (!scheduler->high_queue[i]->is_exhausted) remaining++;
        for (int i = 0; i < scheduler->normal_count; i++)
            if (!scheduler->normal_queue[i]->is_exhausted) remaining++;
        for (int i = 0; i < scheduler->low_count; i++)
            if (!scheduler->low_queue[i]->is_exhausted) remaining++;
        *out_remaining_rules = remaining;
    }
}

/* ============== 字符串转换 ============== */

/**
 * @brief 将优先级等级转换为中文字符串
 *
 * @param priority  优先级等级
 * @return 中文描述字符串（静态内存）
 */
const char *lv00_priority_to_string(Lv00TheoremPriority priority) {
    switch (priority) {
        case PRIORITY_LOW:    return "低优先级（延迟执行）";
        case PRIORITY_NORMAL: return "普通优先级";
        case PRIORITY_HIGH:   return "高优先级（优先执行）";
        case PRIORITY_URGENT: return "紧急优先级（立即执行）";
        default:              return "未知优先级";
    }
}
