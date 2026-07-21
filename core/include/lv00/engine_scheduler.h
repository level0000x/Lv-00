#ifndef LV00_ENGINE_SCHEDULER_H
#define LV00_ENGINE_SCHEDULER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/** Schedule a task for the engine. */
int lv00_engine_schedule(const char *task_name, int priority);
/** Execute all pending tasks. */
int lv00_engine_execute_pending(void);
/** Get pending task count. */
int lv00_engine_pending_count(void);

#ifdef __cplusplus
}
#endif

#endif
