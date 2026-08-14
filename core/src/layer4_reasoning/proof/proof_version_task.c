/**
 * @file proof_version_task.c
 * @brief 证明版本管理与序列化 —— 任务系统与备选占位
 *
 * @details 由 proof_version.c 按功能域拆分而来。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/lv.h"
#include "lv/proof.h"
#include "lv/smt_backend.h"
#include "lv/thread_pool.h"

#include "lv/debug.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/lv_str_utils.h"

#include "lv/lv_strbuf.h"

/* ============================================================================
 * Task system functions — 实现
 * ============================================================================ */

lvTaskGroup *lv_task_group_create(const char *name) {
    (void) name;
    lvTaskGroup *g = (lvTaskGroup *) lv_calloc(1, sizeof(lvTaskGroup));
    if (!g)
        return NULL;
    lv_mutex_init(&g->mutex);
    lv_cond_init(&g->cond);
    g->pending = 0;
    g->completed_count = 0;
    g->task_head = NULL;
    g->task_tail = NULL;
    return g;
}

lvTask *lv_task_create(int (*fn)(void *), void *arg, const char *name) {
    (void) name;
    lvTask *t = (lvTask *) lv_calloc(1, sizeof(lvTask));
    if (!t)
        return NULL;
    t->func = (void (*)(void *)) fn;
    t->arg = arg;
    t->group = NULL;
    t->next = NULL;
    return t;
}

void lv_task_group_add(lvTaskGroup *group, lvTask *task) {
    if (!group || !task)
        return;
    lv_mutex_lock(&group->mutex);
    task->group = group;
    task->next = NULL;
    if (group->task_tail) {
        group->task_tail->next = task;
    } else {
        group->task_head = task;
    }
    group->task_tail = task;
    group->pending++;
    lv_mutex_unlock(&group->mutex);
}

/* ---- 单任务执行：顺序执行任务函数 ---- */
static int lv_task_group_exec_one(lvTask *task) {
    if (!task || !task->func)
        return 0;
    task->func(task->arg);
    return 1;
}

/**
 * @brief 执行任务组中的全部待执行任务（顺序执行）
 *
 * 以先进先出顺序执行组内所有任务。顺序执行是安全默认：与 SLEDGE_ASYNC
 * 的并发缺陷回退先例一致，避免共享状态下的数据竞争（见
 * proof_version_sledge.c 的并发缺陷修复说明）。需要并行时由调用方
 * 自行保证每任务状态独立后再扩展为线程池模式。
 *
 * @param group 任务组
 * @return 成功执行的任务数
 */
int lv_task_group_run(lvTaskGroup *group) {
    if (!group)
        return 0;

    int done = 0;
    for (;;) {
        lv_mutex_lock(&group->mutex);
        lvTask *task = group->task_head;
        if (task) {
            group->task_head = task->next;
            if (!group->task_head)
                group->task_tail = NULL;
            group->pending--;
        }
        lv_mutex_unlock(&group->mutex);

        if (!task)
            break;

        if (lv_task_group_exec_one(task))
            done++;

        lv_free((void **) &task);

        lv_mutex_lock(&group->mutex);
        group->completed_count++;
        lv_mutex_unlock(&group->mutex);
    }

    lv_cond_broadcast(&group->cond);
    return done;
}

/**
 * @brief 等待任务组全部完成
 *
 * 未执行的任务会先按顺序补齐执行（懒执行语义），随后返回。
 */
void lv_task_group_wait(lvTaskGroup *group) {
    if (!group)
        return;
    lv_task_group_run(group);
}

void lv_task_group_destroy(lvTaskGroup *group) {
    if (!group)
        return;
    /* 先执行剩余任务，确保任务不泄漏 */
    lv_task_group_run(group);
    lv_mutex_destroy(&group->mutex);
    lv_cond_destroy(&group->cond);
    lv_free((void **) &group);
}
