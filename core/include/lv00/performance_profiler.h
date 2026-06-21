#ifndef LV00_PERFORMANCE_PROFILER_H
#define LV00_PERFORMANCE_PROFILER_H
/* TODO: Performance profiler module stub */

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/** Benchmark result. */
typedef struct { const char *name; double elapsed_us; } Lv00PerfBenchResult;
#define lv00_perf_benchmark_run(name, fn, n) ((Lv00PerfBenchResult){(name),0.0})
#define lv00_perf_benchmark_print_result(r) ((void)0)

/** Start profiling a section. */
void lv00_profiler_start(const char *name);
/** End profiling a section. */
void lv00_profiler_end(const char *name);
/** Print profiling report. */
void lv00_profiler_report(void);

#ifdef __cplusplus
}
#endif

#endif
