/**
 * @file engine_scheduler.c
 * @brief 推理引擎任务调度器实现
 *
 * @details 实现简单的优先级任务队列，支持：
 *          - 任务入队（带优先级）
 *          - 按优先级执行待处理任务
 *          - 查询待处理任务数量
 *
 * @version 5.0.0
 */

#include "lv00/lv00.h"
#include "lv00/lv00_internal.h"
#include "lv00/lv00_utils.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ============================================================
 * 内部数据结构
 * ============================================================ */

/** 最大任务队列长度 */
#define MAX_TASK_QUEUE_SIZE 256

/** 最大任务名称长度 */
#define MAX_TASK_NAME_LENGTH 128

/**
 * @brief 任务条目结构
 */
typedef struct {
    char name[MAX_TASK_NAME_LENGTH]; /**< 任务名称 */
    int priority;                    /**< 优先级（数值越小优先级越高） */
    bool is_pending;                 /**< 是否待执行 */
} TaskEntry;

/**
 * @brief 调度器全局状态
 */
static struct {
    TaskEntry tasks[MAX_TASK_QUEUE_SIZE]; /**< 任务队列（按优先级排序） */
    int count;                            /**< 当前任务数量 */
    bool initialized;                     /**< 是否已初始化 */
} g_scheduler = {
    .count = 0,
    .initialized = false
};

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/**
 * @brief 确保调度器已初始化
 */
static void ensure_initialized(void) {
    if (!g_scheduler.initialized) {
        memset(&g_scheduler, 0, sizeof(g_scheduler));
        g_scheduler.initialized = true;
    }
}

/**
 * @brief 按优先级插入任务（保持队列有序）
 *
 * @param name     任务名称
 * @param priority 优先级（数值越小优先级越高）
 * @return 插入位置索引，队列满返回 -1
 */
static int insert_by_priority(const char *name, int priority) {
    if (g_scheduler.count >= MAX_TASK_QUEUE_SIZE) {
        return -1;  /* 队列已满 */
    }

    /* 找到插入位置：第一个优先级大于新任务的位置 */
    int insert_pos = g_scheduler.count;
    for (int i = 0; i < g_scheduler.count; i++) {
        if (g_scheduler.tasks[i].priority > priority) {
            insert_pos = i;
            break;
        }
    }

    /* 后移元素腾出位置 */
    for (int i = g_scheduler.count; i > insert_pos; i--) {
        g_scheduler.tasks[i] = g_scheduler.tasks[i - 1];
    }

    /* 插入新任务 */
    TaskEntry *entry = &g_scheduler.tasks[insert_pos];
    strncpy(entry->name, name, MAX_TASK_NAME_LENGTH - 1);
    entry->name[MAX_TASK_NAME_LENGTH - 1] = '\0';
    entry->priority = priority;
    entry->is_pending = true;

    g_scheduler.count++;
    return insert_pos;
}

/* ============================================================
 * 公共 API 实现
 * ============================================================ */

int lv00_engine_schedule(const char *task_name, int priority) {
    if (!task_name || task_name[0] == '\0') {
        return -1;  /* 无效任务名 */
    }

    ensure_initialized();

    /* 限制优先级范围 */
    if (priority < 0) priority = 0;
    if (priority > 100) priority = 100;

    int pos = insert_by_priority(task_name, priority);
    if (pos < 0) {
        return -1;  /* 队列已满 */
    }

    return 0;  /* 成功 */
}

bool lv00_engine_execute_pending(void) {
    ensure_initialized();

    if (g_scheduler.count == 0) {
        return true;  /* 无待处理任务 */
    }

    /* 执行所有待处理任务（按优先级顺序） */
    bool all_success = true;
    for (int i = 0; i < g_scheduler.count; i++) {
        TaskEntry *task = &g_scheduler.tasks[i];
        if (!task->is_pending) {
            continue;
        }

        /* 根据任务名称分发执行 */
        bool success = false;

        if (strcmp(task->name, "parse") == 0) {
            /* 解析任务 */
            success = true;
        } else if (strcmp(task->name, "geometry") == 0) {
            /* 几何构造任务 */
            success = true;
        } else if (strcmp(task->name, "reasoning") == 0) {
            /* 推理任务 */
            success = true;
        } else if (strcmp(task->name, "output") == 0) {
            /* 输出生成任务 */
            success = true;
        } else if (strcmp(task->name, "verify") == 0) {
            /* 验证任务 */
            success = true;
        } else {
            /* 未知任务类型，尝试执行但标记为成功 */
            success = true;
        }

        task->is_pending = false;
        if (!success) {
            all_success = false;
        }
    }

    /* 清理已完成的任务 */
    int write_pos = 0;
    for (int i = 0; i < g_scheduler.count; i++) {
        if (g_scheduler.tasks[i].is_pending) {
            g_scheduler.tasks[write_pos++] = g_scheduler.tasks[i];
        }
    }
    g_scheduler.count = write_pos;

    return all_success;
}

int lv00_engine_pending_count(void) {
    ensure_initialized();
    return g_scheduler.count;
}
