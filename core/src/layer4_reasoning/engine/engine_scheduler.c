/**
 * @file engine_scheduler.c
 * @brief 引擎调度器 —— 管理多后端求解引擎的任务调度
 *
 * @details 实现基于优先级的任务调度队列，
 *          按优先级顺序调度引擎执行任务，
 *          支持任务提交、批量执行和状态查询。
 *
 * @version 1.1.0
 */

#include "engine_scheduler.h"
#include "lv_internal.h"

#include <stdlib.h>
#include <string.h>

/* ---- 调度任务 ---- */
typedef struct {
    char name[256];
    int  priority;
    bool pending;
} SchedulerTask;

/* ---- 内部状态 ---- */
static SchedulerTask *g_tasks = NULL;
static int            g_task_count = 0;
static int            g_task_capacity = 0;

/** 按优先级排序（高优先级在前）*/
static int task_compare(const void *a, const void *b) {
    const SchedulerTask *ta = (const SchedulerTask *)a;
    const SchedulerTask *tb = (const SchedulerTask *)b;
    return tb->priority - ta->priority;
}

int lv_engine_schedule(const char *task_name, int priority) {
    if (!task_name) return -1;

    /* 扩容 */
    if (g_task_count >= g_task_capacity) {
        int new_cap = (g_task_capacity == 0) ? 16 : g_task_capacity * 2;
        SchedulerTask *new_tasks = lv_realloc(g_tasks, sizeof(SchedulerTask) * new_cap);
        if (!new_tasks) return -1;
        g_tasks = new_tasks;
        g_task_capacity = new_cap;
    }

    SchedulerTask *t = &g_tasks[g_task_count];
    memset(t, 0, sizeof(*t));
    strncpy(t->name, task_name, sizeof(t->name) - 1);
    t->name[sizeof(t->name) - 1] = '\0';
    t->priority = priority;
    t->pending = true;
    g_task_count++;

    /* 按优先级排序 */
    qsort(g_tasks, (size_t)g_task_count, sizeof(SchedulerTask), task_compare);

    return g_task_count - 1;
}

bool lv_engine_execute_pending(void) {
    bool any_executed = false;

    for (int i = 0; i < g_task_count; i++) {
        if (g_tasks[i].pending) {
            g_tasks[i].pending = false;
            any_executed = true;
            /* 实际执行逻辑预留：调度对应引擎后端处理 */
        }
    }

    return any_executed;
}

int lv_engine_pending_count(void) {
    int count = 0;
    for (int i = 0; i < g_task_count; i++) {
        if (g_tasks[i].pending) count++;
    }
    return count;
}
