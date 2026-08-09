#ifndef lv_TEST_FRAMEWORK_H
#define lv_TEST_FRAMEWORK_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wconversion"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h> /* FILE */

/* ── Size constants ──
 * 与 config.h 的 "test_framework.h compat" 段（#ifndef 保护）保持一致。
 * 测试 TU 通常先包含 lv.h（→config.h）再包含本头，若此处无 #ifndef 保护
 * 且替换文本不同，会触发 -Wmacro-redefined，故与 config.h 相同加 #ifndef 保护。 */
#ifndef lv_TEST_MAX_SUITES
#define lv_TEST_MAX_SUITES 256
#endif
#ifndef lv_TEST_MAX_CASES
#define lv_TEST_MAX_CASES 4096
#endif
#ifndef lv_TEST_NAME_MAX_LEN
#ifdef lv_CONFIG_TEST_NAME_MAX_LEN
#define lv_TEST_NAME_MAX_LEN lv_CONFIG_TEST_NAME_MAX_LEN
#else
#define lv_TEST_NAME_MAX_LEN 256
#endif
#endif
#ifndef lv_TEST_MSG_MAX_LEN
#ifdef lv_CONFIG_TEST_MSG_MAX_LEN
#define lv_TEST_MSG_MAX_LEN lv_CONFIG_TEST_MSG_MAX_LEN
#else
#define lv_TEST_MSG_MAX_LEN 512
#endif
#endif

/* ── Test status ── */
typedef enum {
    TEST_STATUS_PASSED = 0,
    TEST_STATUS_FAILED = 1,
    TEST_STATUS_SKIPPED = 2,
    TEST_STATUS_PENDING = 3,
    TEST_STATUS_RUNNING = 4
} lvTestStatus;

/* ── Forward decls ── */
typedef struct lvTestCase lvTestCase;
typedef struct lvTestSuite lvTestSuite;
typedef struct lvTestResult lvTestResult;
typedef struct lvTestReport lvTestReport;
typedef struct lvBenchmark lvBenchmark;

/* ── Function pointer types ── */
typedef void (*lvTestFunc)(void);
typedef void (*lvTestSetupFunc)(void);
typedef void (*lvTestTeardownFunc)(void);
typedef void *(*lvTestDataGenerator)(int index);
typedef void (*lvBenchmarkFunc)(void);

/* ── TestCase ── */
struct lvTestCase {
    char name[lv_TEST_NAME_MAX_LEN];
    char suite[lv_TEST_NAME_MAX_LEN];
    lvTestFunc func;
    lvTestSetupFunc setup;
    lvTestTeardownFunc teardown;
    lvTestStatus status;
    char **tags;
    int tag_count;
    char message[lv_TEST_MSG_MAX_LEN];
    const char *file;
    int line;
    void *test_data;
    int data_index;
    uint64_t elapsed_ns;
};

/* ── TestSuite ── */
struct lvTestSuite {
    char name[lv_TEST_NAME_MAX_LEN];
    lvTestCase *cases;
    int case_count;
    int case_capacity;
    int passed_count;
    int failed_count;
    int skipped_count;
    lvTestSetupFunc suite_setup;
    lvTestTeardownFunc suite_teardown;
};

/* ── TestResult ── */
struct lvTestResult {
    lvTestCase *test_case;
    lvTestStatus status;
    uint64_t elapsed_ns;
    lvTestCase **failures; /* list of failing cases */
};

/* ── TestReport ── */
struct lvTestReport {
    uint64_t start_time_ns;
    lvTestSuite *suites;
    int suite_count;
    int total_tests;
    int passed_count;
    int failed_count;
    int skipped_count;
    uint64_t end_time_ns;
    uint64_t total_time_ns;
    char *json_output;
    char *xml_output;
    char *html_output;
};

/* ── Benchmark ── */
struct lvBenchmark {
    char *name;
    lvBenchmarkFunc func;
    uint64_t iterations;
    uint64_t total_time_ns;
    double avg_time_ns;
    uint64_t min_time_ns;
    uint64_t max_time_ns;
    double ops_per_sec;
    double std_dev_ns;
};

/* ── API ── */
bool lv_test_register(const char *suite_name, const char *test_name, lvTestFunc func);
bool lv_test_register_with_fixture(const char *suite_name, const char *test_name, lvTestFunc func,
                                   lvTestSetupFunc setup, lvTestTeardownFunc teardown);
bool lv_test_register_suite_fixture(const char *suite_name, lvTestSetupFunc setup, lvTestTeardownFunc teardown);
bool lv_test_register_data_driven(const char *suite_name, const char *test_name, lvTestFunc func,
                                  lvTestDataGenerator generator, int data_count);
bool lv_test_register_tag(const char *suite_name, const char *test_name, const char *tag);

lvTestReport *lv_test_run_all(void);
lvTestReport *lv_test_run_suite(const char *suite_name);
lvTestReport *lv_test_run_by_tag(const char *tag);

void lv_test_result_destroy(lvTestResult *result);
void lv_test_report_destroy(lvTestReport *report);
void lv_test_report_print(const lvTestReport *report, FILE *stream);
char *lv_test_report_to_json(const lvTestReport *report);
bool lv_test_report_write_file(const lvTestReport *report, const char *path, const char *format);

/*
 * 基准统计口径豁免约定（P2-1）：
 * 本组基准 API 采用「固定迭代次数 + 逐次计时 + 简单均值/标准差」口径，
 * 与 performance_profiler.c 的 lv_perf_benchmark_run（预热 + 100ms 校准 +
 * Welford 在线统计）统计口径不同，二者刻意保持独立、不做收敛复用；
 * 如需统一口径，应先评审两侧消费方对计时确定性与采样方法的依赖。
 */
bool lv_benchmark_register(const char *name, lvBenchmarkFunc func, uint64_t iterations);
lvBenchmark *lv_benchmark_run(const char *name);
void lv_benchmark_destroy(lvBenchmark *bench);

/* data-driven helpers */
void *lv_test_get_data(void);
uint32_t lv_test_get_data_index(void);

/* ── Test assertion macros (lv_ prefix) ── */
#ifndef lv_TEST_MACROS_DEFINED
#define lv_TEST_MACROS_DEFINED

#include <stdio.h>
#include <string.h>

/* ── 断言宏共享的全局计数器 ──
 * lv_ASSERT_* 失败时递增 g_fail_count 并 return（在 void 测试函数内）；
 * 成功时递增 g_pass_count。与 test_helpers.h 的 TEST_ASSERT_* 使用同一对
 * 符号（g_pass_count/g_fail_count），使两套断言体系的计数统一：
 * TEST_MAIN_END() 可依据 g_fail_count 正确决定退出码，杜绝"失败但退出码为 0"。
 * 使用 lv_ASSERT_* 的测试 TU 必须定义这两个变量（仅声明 extern 于此）。 */
extern int g_pass_count;
extern int g_fail_count;

#define lv_TEST(suite, name)                 \
    static void test_##suite##_##name(void); \
    static void test_##suite##_##name(void)

#define lv_ASSERT(cond)                                                        \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, "  FAIL [%s:%d] %s\n", __FILE__, __LINE__, #cond); \
            g_fail_count++;                                                    \
            return;                                                            \
        }                                                                      \
        g_pass_count++;                                                        \
    } while (0)

#define lv_ASSERT_NOT_NULL(ptr)                                                       \
    do {                                                                              \
        if ((ptr) == NULL) {                                                          \
            fprintf(stderr, "  FAIL [%s:%d] %s is NULL\n", __FILE__, __LINE__, #ptr); \
            g_fail_count++;                                                           \
            return;                                                                   \
        }                                                                             \
        g_pass_count++;                                                               \
    } while (0)

#define lv_ASSERT_TRUE(expr) lv_ASSERT(expr)
#define lv_ASSERT_FALSE(expr)                                                          \
    do {                                                                               \
        if ((expr)) {                                                                  \
            fprintf(stderr, "  FAIL [%s:%d] %s is true\n", __FILE__, __LINE__, #expr); \
            g_fail_count++;                                                            \
            return;                                                                    \
        }                                                                              \
        g_pass_count++;                                                                \
    } while (0)

#define lv_ASSERT_EQ(expected, actual)                                                                             \
    do {                                                                                                           \
        if ((intptr_t) (expected) != (intptr_t) (actual)) {                                                        \
            fprintf(stderr, "  FAIL [%s:%d] %s != %s (expected=%ld, actual=%ld)\n", __FILE__, __LINE__, #expected, \
                    #actual, (long) (intptr_t) (expected), (long) (intptr_t) (actual));                            \
            g_fail_count++;                                                                                        \
            return;                                                                                                \
        }                                                                                                          \
        g_pass_count++;                                                                                            \
    } while (0)

#define lv_ASSERT_STR_EQ(actual, expected)                                                                      \
    do {                                                                                                        \
        const char *_lv_actual = (actual);                                                                      \
        const char *_lv_expected = (expected);                                                                  \
        int _lv_cmp = 0;                                                                                        \
        if (_lv_actual == NULL && _lv_expected == NULL) {                                                       \
            _lv_cmp = 0;                                                                                        \
        } else if (_lv_actual == NULL || _lv_expected == NULL) {                                                \
            _lv_cmp = 1;                                                                                        \
        } else {                                                                                                \
            _lv_cmp = strcmp(_lv_actual, _lv_expected);                                                         \
        }                                                                                                       \
        if (_lv_cmp != 0) {                                                                                     \
            fprintf(stderr, "  FAIL [%s:%d] %s != %s (actual='%s', expected='%s')\n", __FILE__, __LINE__,       \
                    #actual, #expected, _lv_actual ? _lv_actual : "(null)",                                     \
                    _lv_expected ? _lv_expected : "(null)");                                                    \
            g_fail_count++;                                                                                     \
            return;                                                                                             \
        }                                                                                                       \
        g_pass_count++;                                                                                         \
    } while (0)

#define lv_ASSERT_NE(a, e)                                                                                      \
    do {                                                                                                        \
        if ((intptr_t) (a) == (intptr_t) (e)) {                                                                 \
            fprintf(stderr, "  FAIL [%s:%d] %s == %s (value=%ld)\n", __FILE__, __LINE__, #a, #e,                 \
                    (long) (intptr_t) (a));                                                                     \
            g_fail_count++;                                                                                     \
            return;                                                                                             \
        }                                                                                                       \
        g_pass_count++;                                                                                         \
    } while (0)

#define lv_ASSERT_FLOAT_EQ(expected, actual, tol)                                                             \
    do {                                                                                                      \
        double _e = (double) (expected), _a = (double) (actual), _t = (double) (tol);                         \
        double _d = _e > _a ? _e - _a : _a - _e;                                                              \
        if (_d > _t) {                                                                                        \
            fprintf(stderr, "  FAIL [%s:%d] %s ~= %s (expected=%f, actual=%f, tol=%f)\n", __FILE__, __LINE__, \
                    #expected, #actual, _e, _a, _t);                                                          \
            g_fail_count++;                                                                                   \
            return;                                                                                           \
        }                                                                                                     \
        g_pass_count++;                                                                                       \
    } while (0)

#endif /* lv_TEST_MACROS_DEFINED */

#ifdef __cplusplus
}
#endif

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
