#ifndef LV00_TEST_FRAMEWORK_H
#define LV00_TEST_FRAMEWORK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>   /* FILE */

/* ── Size constants ── */
#define LV00_TEST_MAX_SUITES      256
#define LV00_TEST_MAX_CASES       4096
#define LV00_TEST_NAME_MAX_LEN    256
#define LV00_TEST_MSG_MAX_LEN     512

/* ── Test status ── */
typedef enum {
    TEST_STATUS_PASSED  = 0,
    TEST_STATUS_FAILED  = 1,
    TEST_STATUS_SKIPPED = 2,
    TEST_STATUS_PENDING = 3,
    TEST_STATUS_RUNNING = 4
} Lv00TestStatus;

/* ── Forward decls ── */
typedef struct Lv00TestCase   Lv00TestCase;
typedef struct Lv00TestSuite  Lv00TestSuite;
typedef struct Lv00TestResult Lv00TestResult;
typedef struct Lv00TestReport Lv00TestReport;
typedef struct Lv00Benchmark  Lv00Benchmark;

/* ── Function pointer types ── */
typedef void (*Lv00TestFunc)(void);
typedef void (*Lv00TestSetupFunc)(void);
typedef void (*Lv00TestTeardownFunc)(void);
typedef void *(*Lv00TestDataGenerator)(int index);
typedef void (*Lv00BenchmarkFunc)(void);

/* ── TestCase ── */
struct Lv00TestCase {
    char            name[LV00_TEST_NAME_MAX_LEN];
    char            suite[LV00_TEST_NAME_MAX_LEN];
    Lv00TestFunc    func;
    Lv00TestSetupFunc setup;
    Lv00TestTeardownFunc teardown;
    Lv00TestStatus  status;
    char           **tags;
    int             tag_count;
    char            message[LV00_TEST_MSG_MAX_LEN];
    const char     *file;
    int             line;
    void           *test_data;
    int             data_index;
    uint64_t        elapsed_ns;
};

/* ── TestSuite ── */
struct Lv00TestSuite {
    char            name[LV00_TEST_NAME_MAX_LEN];
    Lv00TestCase   *cases;
    int             case_count;
    int             case_capacity;
    int             passed_count;
    int             failed_count;
    int             skipped_count;
    Lv00TestSetupFunc    suite_setup;
    Lv00TestTeardownFunc suite_teardown;
};

/* ── TestResult ── */
struct Lv00TestResult {
    Lv00TestCase   *test_case;
    Lv00TestStatus  status;
    uint64_t        elapsed_ns;
    Lv00TestCase  **failures;       /* list of failing cases */
};

/* ── TestReport ── */
struct Lv00TestReport {
    uint64_t        start_time_ns;
    Lv00TestSuite  *suites;
    int             suite_count;
    int             total_tests;
    int             passed_count;
    int             failed_count;
    int             skipped_count;
    uint64_t        end_time_ns;
    uint64_t        total_time_ns;
    char           *json_output;
    char           *xml_output;
    char           *html_output;
};

/* ── Benchmark ── */
struct Lv00Benchmark {
    char                *name;
    Lv00BenchmarkFunc    func;
    uint64_t             iterations;
    uint64_t             total_time_ns;
    double               avg_time_ns;
    uint64_t             min_time_ns;
    uint64_t             max_time_ns;
    double               ops_per_sec;
    double               std_dev_ns;
};

/* ── API ── */
Lv00TestSuite *lv00_test_suite_create(const char *name);
void lv00_test_suite_destroy(Lv00TestSuite *suite);

bool lv00_test_register(const char *suite_name, const char *test_name,
                        Lv00TestFunc func);
bool lv00_test_register_with_fixture(const char *suite_name, const char *test_name,
                                     Lv00TestFunc func,
                                     Lv00TestSetupFunc setup,
                                     Lv00TestTeardownFunc teardown);
bool lv00_test_register_suite_fixture(const char *suite_name,
                                      Lv00TestSetupFunc setup,
                                      Lv00TestTeardownFunc teardown);
bool lv00_test_register_data_driven(const char *suite_name, const char *test_name,
                                    Lv00TestFunc func,
                                    Lv00TestDataGenerator generator,
                                    int data_count);
bool lv00_test_register_tag(const char *suite_name, const char *test_name,
                            const char *tag);

Lv00TestReport *lv00_test_run_all(void);
Lv00TestReport *lv00_test_run_suite(const char *suite_name);
Lv00TestReport *lv00_test_run_by_tag(const char *tag);

void lv00_test_result_destroy(Lv00TestResult *result);
void lv00_test_report_destroy(Lv00TestReport *report);
void lv00_test_report_print(const Lv00TestReport *report, FILE *stream);
char *lv00_test_report_to_json(const Lv00TestReport *report);

bool lv00_benchmark_register(const char *name, Lv00BenchmarkFunc func,
                             uint64_t iterations);
Lv00Benchmark *lv00_benchmark_run(const char *name);
void lv00_benchmark_destroy(Lv00Benchmark *bench);

/* data-driven helpers */
void *lv00_test_get_data(void);
uint32_t lv00_test_get_data_index(void);

#ifdef __cplusplus
}
#endif
#endif
