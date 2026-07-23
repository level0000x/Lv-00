#ifndef lv_ENGINE_SCHEDULER_H
#define lv_ENGINE_SCHEDULER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/* 前向声明 */
struct lvEngine;

/** Initialize the scheduler with an engine instance. */
void lv_engine_scheduler_init(struct lvEngine *engine);

/** Schedule a task for the engine. */
int lv_engine_schedule(const char *task_name, int priority);

/** Execute all pending tasks, dispatching to the registered engine. */
bool lv_engine_execute_pending(void);

/** Get pending task count. */
int lv_engine_pending_count(void);

#ifdef __cplusplus
}
#endif

#endif
