#ifndef LV00_TEST_FRAMEWORK_H
#define LV00_TEST_FRAMEWORK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/* ── Macros ── */ 
#define LV00_TEST_MAX_SUITES    256
#define LV00_TEST_MAX_CASES     4096
#define LV00_TEST_MAX_TAGS      16
#define LV00_TEST_MSG_MAX_LEN   512
#define LV00_TEST_BENCH_RUNS    100

/* ── Status ── */
typedef enum {
    TEST_STATUS_PASSED  = 0,
    TEST_STATUS_FAILED  = 1,
    TEST_STATUS_SKIPPED = 2
} Lv00TestStatus;

/* ── Forward decls ── */
typedef struct Lv00TestCase   Lv00TestCase;
typedef struct Lv00TestSuite  Lv00TestSuite;
typedef struct Lv00TestResult Lv00TestResult;
typedef struct Lv00TestReport Lv00TestReport;
typedef struct Lv00Benchmark  Lv00Benchmark;
typedef struct Lv00TestMutex  Lv00TestMutex;

/* ── Function pointer types ── */
typedef void (*Lv00TestFunc)(void);
typedef void (*Lv00TestSetupFunc)(void);
typedef void (*Lv00TestTeardownFunc)(void);
typedef void (*Lv00TestDataGenerator)(void *data, int index);
typedef void (*Lv00BenchmarkFunc)(int runs);

/* ── TestCase ── */
struct Lv00TestCase {
    char *name;
    Lv00TestFunc func;
    Lv00TestFunc run;
    Lv00TestStatus status;
    char message[LV00_TEST_MSG_MAX_LEN];
    const char *file;
    int line;
    Lv00TestSetupFunc setup;
    Lv00TestTeardownFunc teardown;
    void *test_data;
    int data_index;
    int tag_count;
    const char *tags[LV00_TEST_MAX_TAGS];
    uint64_t elapsed_ns;
};

/* ── TestSuite ── */
struct Lv00TestSuite {
    char *name;
    Lv00TestCase *cases;
    int case_count;
    int case_capacity;
    int passed_count;
    int failed_count;
    int skipped_count;
    Lv00TestSetupFunc suite_setup;
    Lv00TestTeardownFunc suite_teardown;
};

/* ── TestResult ── */
struct Lv00TestResult {
    Lv00TestSuite *suites;
    int suite_count;
    int total_tests;
    int passed_count;
    int failed_count;
    int skipped_count;
    uint64_t start_time_ns;
    uint64_t end_time_ns;
    uint64_t total_time_ns;
};

/* ── TestReport ── */
struct Lv00TestReport {
    Lv00TestResult *result;
    char *json_payload;
};

/* ── Benchmark ── */
struct Lv00Benchmark {
    char *name;
    Lv00BenchmarkFunc func;
    int runs;
    uint64_t elapsed_ns;
};

/* ── Mutex stub ── */
struct Lv00TestMutex { int _; };

/* ── API ── */
Lv00TestSuite *lv00_test_suite_create(const char *name);
void lv00_test_suite_destroy(Lv00TestSuite *suite);
int lv00_test_suite_register_case(Lv00TestSuite *suite, const char *name, Lv00TestFunc func);
int lv00_test_suite_register_case_full(Lv00TestSuite *suite, const char *name, Lv00TestFunc func, const char *file, int line);
Lv00TestResult *lv00_test_run_all(void);
Lv00TestResult *lv00_test_run_suite(const Lv00TestSuite *suite);
Lv00TestResult *lv00_test_run_by_tag(const char *tag);
void lv00_test_result_destroy(Lv00TestResult *result);
Lv00TestReport *lv00_test_report_create_json(Lv00TestResult *result);
Lv00TestReport *lv00_test_report_create_verbose(Lv00TestResult *result);
Lv00TestReport *lv00_test_report_create_summary(Lv00TestResult *result);
Lv00TestReport *lv00_test_report_create_xml(Lv00TestResult *result);
void lv00_test_report_destroy(Lv00TestReport *report);
char *lv00_test_report_to_json(Lv00TestReport *report);
Lv00Benchmark *lv00_benchmark_create(const char *name, Lv00BenchmarkFunc func, int runs);
void lv00_benchmark_run(Lv00Benchmark *bench);
void lv00_benchmark_destroy(Lv00Benchmark *bench);

#ifdef __cplusplus
}
#endif
#endif
