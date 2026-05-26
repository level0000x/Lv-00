/**
 * @file test_framework.c
 * @brief 增强单元测试框架实现
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "test_framework.h"

#include "lv00_utils.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#endif

/* ============== 内部常量 ============== */

/** 初始测试套件容量 */
#define TEST_SUITE_INIT_CAPACITY 16

/** 初始测试用例容量 */
#define TEST_CASE_INIT_CAPACITY 64

/* ============== 平台抽象层 ============== */

#ifdef _WIN32
typedef CRITICAL_SECTION Lv00TestMutex;
#define MUTEX_INIT(m) InitializeCriticalSection(&(m))
#define MUTEX_DESTROY(m) DeleteCriticalSection(&(m))
#define MUTEX_LOCK(m) EnterCriticalSection(&(m))
#define MUTEX_UNLOCK(m) LeaveCriticalSection(&(m))

static int64_t get_time_ns(void) {
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (int64_t)((double)count.QuadPart / (double)freq.QuadPart * 1e9);
}
#else
typedef pthread_mutex_t Lv00TestMutex;
#define MUTEX_INIT(m) pthread_mutex_init(&(m), NULL)
#define MUTEX_DESTROY(m) pthread_mutex_destroy(&(m))
#define MUTEX_LOCK(m) pthread_mutex_lock(&(m))
#define MUTEX_UNLOCK(m) pthread_mutex_unlock(&(m))

static int64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}
#endif

/* ============== 全局状态 ============== */

static struct {
    Lv00TestSuite *suites[LV00_TEST_MAX_SUITES];
    uint32_t suite_count;
    Lv00TestMutex mutex;
    bool initialized;

    /* 当前测试上下文 */
    Lv00TestCase *current_test;
    Lv00TestResult *current_result;

    /* 配置 */
    uint32_t timeout_ms;
    bool parallel_enabled;
    uint32_t max_threads;
} g_test_system = {0};

/* ============== 内部函数 ============== */

static Lv00TestSuite *find_or_create_suite(const char *name) {
    /* 查找现有套件 */
    for (uint32_t i = 0; i < g_test_system.suite_count; i++) {
        if (strcmp(g_test_system.suites[i]->name, name) == 0) {
            return g_test_system.suites[i];
        }
    }

    /* 创建新套件 */
    if (g_test_system.suite_count >= LV00_TEST_MAX_SUITES) {
        return NULL;
    }

    Lv00TestSuite *suite = (Lv00TestSuite *)lv00_calloc(1, sizeof(Lv00TestSuite));
    if (!suite) {
        return NULL;
    }

    strncpy(suite->name, name, sizeof(suite->name) - 1);
    suite->cases = (Lv00TestCase *)lv00_malloc(TEST_CASE_INIT_CAPACITY * sizeof(Lv00TestCase));
    if (!suite->cases) {
        lv00_free((void **) &suite);
        return NULL;
    }
    suite->case_capacity = TEST_CASE_INIT_CAPACITY;

    g_test_system.suites[g_test_system.suite_count++] = suite;
    return suite;
}

static void init_test_system(void) {
    if (g_test_system.initialized) {
        return;
    }

    memset(&g_test_system, 0, sizeof(g_test_system));
    MUTEX_INIT(g_test_system.mutex);
    g_test_system.timeout_ms = 30000; /* 默认 30 秒超时 */
    g_test_system.initialized = true;
}

/* ============== 测试注册实现 ============== */

/**
 * @brief 注册测试用例
 * @details 将一个测试用例注册到指定测试套件中。若套件不存在则自动创建。
 *          不带 setup/teardown 夹具的简化版本，等价于调用 lv00_test_register_with_fixture
 *          并将 setup 和 teardown 设为 NULL。同名测试用例不会被重复注册。
 * @param suite_name 测试套件名称
 * @param test_name  测试用例名称
 * @param func       测试函数指针
 * @return 注册成功返回 true，参数无效或已存在同名用例时返回 false
 */
bool lv00_test_register(const char *suite_name, const char *test_name, Lv00TestFunc func) {
    return lv00_test_register_with_fixture(suite_name, test_name, func, NULL, NULL);
}

bool lv00_test_register_with_fixture(const char *suite_name, const char *test_name,
                                      Lv00TestFunc func,
                                      Lv00TestSetupFunc setup,
                                      Lv00TestTeardownFunc teardown) {
    init_test_system();

    if (!suite_name || !test_name || !func) {
        return false;
    }

    MUTEX_LOCK(g_test_system.mutex);

    Lv00TestSuite *suite = find_or_create_suite(suite_name);
    if (!suite) {
        MUTEX_UNLOCK(g_test_system.mutex);
        return false;
    }

    /* 检查是否已存在 */
    for (uint32_t i = 0; i < suite->case_count; i++) {
        if (strcmp(suite->cases[i].name, test_name) == 0) {
            MUTEX_UNLOCK(g_test_system.mutex);
            return false;
        }
    }

    /* 扩容 */
    if (suite->case_count >= suite->case_capacity) {
        /* 检查乘法溢出 */
        if (suite->case_capacity > UINT32_MAX / 2) {
            MUTEX_UNLOCK(g_test_system.mutex);
            return false;  /* 容量过大，无法扩展 */
        }
        uint32_t new_cap = suite->case_capacity * 2;
        Lv00TestCase *new_cases = (Lv00TestCase *)lv00_realloc(suite->cases,
                                                          new_cap * sizeof(Lv00TestCase));
        if (!new_cases) {
            MUTEX_UNLOCK(g_test_system.mutex);
            return false;
        }
        suite->cases = new_cases;
        suite->case_capacity = new_cap;
    }

    /* 添加测试用例 */
    Lv00TestCase *test_case = &suite->cases[suite->case_count++];
    memset(test_case, 0, sizeof(Lv00TestCase));

    strncpy(test_case->name, test_name, sizeof(test_case->name) - 1);
    strncpy(test_case->suite, suite_name, sizeof(test_case->suite) - 1);
    test_case->func = func;
    test_case->setup = setup;
    test_case->teardown = teardown;
    test_case->status = TEST_STATUS_PENDING;

    MUTEX_UNLOCK(g_test_system.mutex);
    return true;
}

bool lv00_test_register_suite_fixture(const char *suite_name,
                                       Lv00TestSetupFunc setup,
                                       Lv00TestTeardownFunc teardown) {
    init_test_system();

    if (!suite_name) {
        return false;
    }

    MUTEX_LOCK(g_test_system.mutex);

    Lv00TestSuite *suite = find_or_create_suite(suite_name);
    if (!suite) {
        MUTEX_UNLOCK(g_test_system.mutex);
        return false;
    }

    suite->suite_setup = setup;
    suite->suite_teardown = teardown;

    MUTEX_UNLOCK(g_test_system.mutex);
    return true;
}

bool lv00_test_add_tag(const char *suite_name, const char *test_name, const char *tag) {
    if (!suite_name || !test_name || !tag) {
        return false;
    }

    MUTEX_LOCK(g_test_system.mutex);

    for (uint32_t i = 0; i < g_test_system.suite_count; i++) {
        Lv00TestSuite *suite = g_test_system.suites[i];
        if (strcmp(suite->name, suite_name) == 0) {
            for (uint32_t j = 0; j < suite->case_count; j++) {
                Lv00TestCase *test_case = &suite->cases[j];
                if (strcmp(test_case->name, test_name) == 0) {
                    /* 添加标签（使用安全的字符串复制函数 lv00_strlcpy） */
                    if (test_case->tag_count < 8) {
                        if (test_case->tag_count == 0) {
                            test_case->tags = (char **)lv00_malloc(8 * sizeof(char *));
                        }
                        test_case->tags[test_case->tag_count] = (char *)lv00_malloc(strlen(tag) + 1);
                        if (test_case->tags[test_case->tag_count]) {
                            /* 使用安全的字符串复制函数，自动保证零终止 */
                            lv00_strlcpy(test_case->tags[test_case->tag_count], tag, strlen(tag) + 1);
                            test_case->tag_count++;
                        }
                    }
                    MUTEX_UNLOCK(g_test_system.mutex);
                    return true;
                }
            }
        }
    }

    MUTEX_UNLOCK(g_test_system.mutex);
    return false;
}

/* ============== 断言实现 ============== */

void lv00_assert_fail(const char *expr, const char *file, int line, const char *fmt, ...) {
    Lv00TestCase *test = g_test_system.current_test;
    if (!test) {
        return;
    }

    test->status = TEST_STATUS_FAILED;

    /* 记录失败信息 */
    char message[LV00_TEST_MSG_MAX_LEN];
    if (fmt) {
        va_list args;
        va_start(args, fmt);
        vsnprintf(message, sizeof(message), fmt, args);
        va_end(args);
    } else {
        strncpy(message, expr, sizeof(message) - 1);
    }

    strncpy(test->message, message, sizeof(test->message) - 1);
    strncpy(test->file, file, sizeof(test->file) - 1);
    test->line = line;
}

void lv00_assert_pass(const char *file, int line) {
    (void)file;
    (void)line;
    /* 默认通过，无需操作 */
}

/* ============== 测试执行实现 ============== */

/**
 * @brief 运行单个测试
 * @details 执行单个测试用例的完整生命周期：setup -> 测试函数 -> teardown。
 *          记录执行时间和结果状态。若测试函数中调用了断言失败，状态会被标记为 FAILED；
 *          若未触发任何断言失败，状态标记为 PASSED。
 * @param test_case 测试用例指针
 * @param suite     所属测试套件指针（用于更新套件统计）
 * @return 测试结果指针（调用者需使用 lv00_test_result_destroy 释放），失败时返回 NULL
 */
static Lv00TestResult *run_single_test(Lv00TestCase *test_case, Lv00TestSuite *suite) {
    Lv00TestResult *result = (Lv00TestResult *)lv00_calloc(1, sizeof(Lv00TestResult));
    if (!result) {
        return NULL;
    }

    result->test_case = test_case;
    result->status = TEST_STATUS_RUNNING;

    g_test_system.current_test = test_case;
    g_test_system.current_result = result;

    int64_t start_time = get_time_ns();

    /* 执行 setup */
    if (test_case->setup) {
        test_case->setup();
    }

    /* 执行测试 */
    test_case->status = TEST_STATUS_RUNNING;
    test_case->elapsed_ns = 0;

    if (test_case->func) {
        test_case->func();
    }

    /* 执行 teardown */
    if (test_case->teardown) {
        test_case->teardown();
    }

    int64_t end_time = get_time_ns();
    test_case->elapsed_ns = end_time - start_time;
    result->elapsed_ns = test_case->elapsed_ns;

    /* 更新状态 */
    if (test_case->status == TEST_STATUS_RUNNING) {
        test_case->status = TEST_STATUS_PASSED;
        result->status = TEST_STATUS_PASSED;
        suite->passed_count++;
    } else if (test_case->status == TEST_STATUS_FAILED) {
        result->status = TEST_STATUS_FAILED;
        suite->failed_count++;
    }

    g_test_system.current_test = NULL;
    g_test_system.current_result = NULL;

    return result;
}

/**
 * @brief 运行所有测试
 * @details 依次运行所有已注册的测试套件中的所有测试用例。对每个套件会执行：
 *          套件级 setup -> 逐个运行测试用例 -> 套件级 teardown。
 *          汇总所有套件的通过/失败/跳过计数，生成测试报告。
 * @return 测试报告指针（调用者需使用 lv00_test_report_destroy 释放），失败时返回 NULL
 */
Lv00TestReport *lv00_test_run_all(void) {
    init_test_system();

    Lv00TestReport *report = (Lv00TestReport *)lv00_calloc(1, sizeof(Lv00TestReport));
    if (!report) {
        return NULL;
    }

    report->start_time_ns = get_time_ns();

    MUTEX_LOCK(g_test_system.mutex);

    /* 分配套件数组 */
    report->suites = (Lv00TestSuite *)lv00_malloc(g_test_system.suite_count * sizeof(Lv00TestSuite));
    if (!report->suites) {
        MUTEX_UNLOCK(g_test_system.mutex);
        lv00_free((void **) &report);
        return NULL;
    }
    report->suite_count = g_test_system.suite_count;

    /* 运行所有测试 */
    for (uint32_t i = 0; i < g_test_system.suite_count; i++) {
        Lv00TestSuite *suite = g_test_system.suites[i];

        /* 重置统计 */
        suite->passed_count = 0;
        suite->failed_count = 0;
        suite->skipped_count = 0;

        /* 执行套件 setup */
        if (suite->suite_setup) {
            suite->suite_setup();
        }

        /* 运行套件中的所有测试 */
        for (uint32_t j = 0; j < suite->case_count; j++) {
            Lv00TestResult *result = run_single_test(&suite->cases[j], suite);
            if (result) {
                lv00_test_result_destroy(result);
            }

            report->total_tests++;
        }

        /* 执行套件 teardown */
        if (suite->suite_teardown) {
            suite->suite_teardown();
        }

        /* 复制统计 */
        report->suites[i] = *suite;
        report->passed_count += suite->passed_count;
        report->failed_count += suite->failed_count;
        report->skipped_count += suite->skipped_count;
    }

    MUTEX_UNLOCK(g_test_system.mutex);

    report->end_time_ns = get_time_ns();
    report->total_time_ns = report->end_time_ns - report->start_time_ns;

    return report;
}

Lv00TestReport *lv00_test_run_suite(const char *suite_name) {
    init_test_system();

    if (!suite_name) {
        return NULL;
    }

    MUTEX_LOCK(g_test_system.mutex);

    Lv00TestSuite *suite = NULL;
    for (uint32_t i = 0; i < g_test_system.suite_count; i++) {
        if (strcmp(g_test_system.suites[i]->name, suite_name) == 0) {
            suite = g_test_system.suites[i];
            break;
        }
    }

    if (!suite) {
        MUTEX_UNLOCK(g_test_system.mutex);
        return NULL;
    }

    Lv00TestReport *report = (Lv00TestReport *)lv00_calloc(1, sizeof(Lv00TestReport));
    if (!report) {
        MUTEX_UNLOCK(g_test_system.mutex);
        return NULL;
    }

    report->start_time_ns = get_time_ns();
    report->suites = (Lv00TestSuite *)lv00_malloc(sizeof(Lv00TestSuite));
    if (!report->suites) {
        MUTEX_UNLOCK(g_test_system.mutex);
        lv00_test_report_destroy(report);
        return NULL;
    }
    report->suite_count = 1;

    /* 重置统计 */
    suite->passed_count = 0;
    suite->failed_count = 0;
    suite->skipped_count = 0;

    /* 执行套件 setup */
    if (suite->suite_setup) {
        suite->suite_setup();
    }

    /* 运行测试 */
    for (uint32_t j = 0; j < suite->case_count; j++) {
        Lv00TestResult *result = run_single_test(&suite->cases[j], suite);
        if (result) {
            lv00_test_result_destroy(result);
        }
        report->total_tests++;
    }

    /* 执行套件 teardown */
    if (suite->suite_teardown) {
        suite->suite_teardown();
    }

    report->suites[0] = *suite;
    report->passed_count = suite->passed_count;
    report->failed_count = suite->failed_count;
    report->skipped_count = suite->skipped_count;

    MUTEX_UNLOCK(g_test_system.mutex);

    report->end_time_ns = get_time_ns();
    report->total_time_ns = report->end_time_ns - report->start_time_ns;

    return report;
}

Lv00TestResult *lv00_test_run_single(const char *suite_name, const char *test_name) {
    init_test_system();

    if (!suite_name || !test_name) {
        return NULL;
    }

    MUTEX_LOCK(g_test_system.mutex);

    Lv00TestResult *result = NULL;

    for (uint32_t i = 0; i < g_test_system.suite_count; i++) {
        Lv00TestSuite *suite = g_test_system.suites[i];
        if (strcmp(suite->name, suite_name) == 0) {
            for (uint32_t j = 0; j < suite->case_count; j++) {
                Lv00TestCase *test_case = &suite->cases[j];
                if (strcmp(test_case->name, test_name) == 0) {
                    result = run_single_test(test_case, suite);
                    break;
                }
            }
            break;
        }
    }

    MUTEX_UNLOCK(g_test_system.mutex);

    return result;
}

Lv00TestReport *lv00_test_run_by_tag(const char *tag) {
    init_test_system();

    if (!tag) {
        return NULL;
    }

    Lv00TestReport *report = (Lv00TestReport *)lv00_calloc(1, sizeof(Lv00TestReport));
    if (!report) {
        return NULL;
    }

    report->start_time_ns = get_time_ns();

    MUTEX_LOCK(g_test_system.mutex);

    for (uint32_t i = 0; i < g_test_system.suite_count; i++) {
        Lv00TestSuite *suite = g_test_system.suites[i];

        for (uint32_t j = 0; j < suite->case_count; j++) {
            Lv00TestCase *test_case = &suite->cases[j];

            /* 检查标签 */
            bool has_tag = false;
            for (uint32_t k = 0; k < test_case->tag_count; k++) {
                if (strcmp(test_case->tags[k], tag) == 0) {
                    has_tag = true;
                    break;
                }
            }

            if (has_tag) {
                Lv00TestResult *result = run_single_test(test_case, suite);
                if (result) {
                    report->passed_count += (result->status == TEST_STATUS_PASSED) ? 1 : 0;
                    report->failed_count += (result->status == TEST_STATUS_FAILED) ? 1 : 0;
                    report->total_tests++;
                    lv00_test_result_destroy(result);
                }
            }
        }
    }

    MUTEX_UNLOCK(g_test_system.mutex);

    report->end_time_ns = get_time_ns();
    report->total_time_ns = report->end_time_ns - report->start_time_ns;

    return report;
}

Lv00TestReport *lv00_test_run_by_pattern(const char *pattern) {
    init_test_system();

    if (!pattern) {
        return NULL;
    }

    Lv00TestReport *report = (Lv00TestReport *)lv00_calloc(1, sizeof(Lv00TestReport));
    if (!report) {
        return NULL;
    }

    report->start_time_ns = get_time_ns();

    MUTEX_LOCK(g_test_system.mutex);

    /* 简单的模式匹配（仅支持 * 通配符） */
    for (uint32_t i = 0; i < g_test_system.suite_count; i++) {
        Lv00TestSuite *suite = g_test_system.suites[i];

        for (uint32_t j = 0; j < suite->case_count; j++) {
            Lv00TestCase *test_case = &suite->cases[j];

            /* 构建完整测试名 */
            char full_name[256];
            snprintf(full_name, sizeof(full_name), "%s.%s", suite->name, test_case->name);

            /* 简单匹配（仅检查前缀） */
            bool matches = false;
            const char *star = strchr(pattern, '*');
            if (star) {
                size_t prefix_len = star - pattern;
                if (prefix_len == 0 || strncmp(full_name, pattern, prefix_len) == 0) {
                    matches = true;
                }
            } else {
                matches = (strcmp(full_name, pattern) == 0);
            }

            if (matches) {
                Lv00TestResult *result = run_single_test(test_case, suite);
                if (result) {
                    report->passed_count += (result->status == TEST_STATUS_PASSED) ? 1 : 0;
                    report->failed_count += (result->status == TEST_STATUS_FAILED) ? 1 : 0;
                    report->total_tests++;
                    lv00_test_result_destroy(result);
                }
            }
        }
    }

    MUTEX_UNLOCK(g_test_system.mutex);

    report->end_time_ns = get_time_ns();
    report->total_time_ns = report->end_time_ns - report->start_time_ns;

    return report;
}

void lv00_test_set_timeout(uint32_t timeout_ms) {
    g_test_system.timeout_ms = timeout_ms;
}

void lv00_test_set_parallel(bool enable, uint32_t max_threads) {
    g_test_system.parallel_enabled = enable;
    g_test_system.max_threads = max_threads;
}

/* ============== 参数化测试实现 ============== */

void *lv00_test_get_data(void) {
    Lv00TestCase *test = g_test_system.current_test;
    return test ? test->test_data : NULL;
}

uint32_t lv00_test_get_data_index(void) {
    Lv00TestCase *test = g_test_system.current_test;
    return test ? test->data_index : 0;
}

bool lv00_test_register_parameterized(const char *suite_name, const char *test_name,
                                       Lv00TestFunc func,
                                       Lv00TestDataGenerator generator,
                                       uint32_t data_count) {
    if (!suite_name || !test_name || !func || !generator || data_count == 0) {
        return false;
    }

    /* 为每个数据点注册一个测试 */
    for (uint32_t i = 0; i < data_count; i++) {
        char name[LV00_TEST_NAME_MAX_LEN];
        snprintf(name, sizeof(name), "%s/%u", test_name, i);

        if (!lv00_test_register(suite_name, name, func)) {
            return false;
        }

        /* 存储数据索引 */
        MUTEX_LOCK(g_test_system.mutex);
        for (uint32_t j = 0; j < g_test_system.suite_count; j++) {
            Lv00TestSuite *suite = g_test_system.suites[j];
            if (strcmp(suite->name, suite_name) == 0) {
                if (suite->case_count > 0) {
                    Lv00TestCase *test_case = &suite->cases[suite->case_count - 1];
                    test_case->test_data = generator(i);
                    test_case->data_index = i;
                }
                break;
            }
        }
        MUTEX_UNLOCK(g_test_system.mutex);
    }

    return true;
}

/* ============== 性能基准测试实现 ============== */

static struct {
    Lv00Benchmark benchmarks[LV00_TEST_MAX_CASES];
    uint32_t count;
} g_benchmarks = {0};

bool lv00_benchmark_register(const char *name, Lv00BenchmarkFunc func, uint64_t iterations) {
    if (!name || !func || g_benchmarks.count >= LV00_TEST_MAX_CASES) {
        return false;
    }

    Lv00Benchmark *bench = &g_benchmarks.benchmarks[g_benchmarks.count++];
    strncpy(bench->name, name, sizeof(bench->name) - 1);
    bench->iterations = iterations;

    /* 运行基准测试 */
    int64_t *times = (int64_t *)lv00_malloc(iterations * sizeof(int64_t));
    if (!times) {
        return false;
    }

    int64_t total_ns = 0;
    double min_ns = 1e18;
    double max_ns = 0;

    for (uint64_t i = 0; i < iterations; i++) {
        int64_t start = get_time_ns();
        func();
        int64_t end = get_time_ns();
        times[i] = end - start;
        total_ns += times[i];

        if (times[i] < min_ns) min_ns = times[i];
        if (times[i] > max_ns) max_ns = times[i];
    }

    bench->total_time_ns = total_ns;
    bench->avg_time_ns = (double)total_ns / iterations;
    bench->min_time_ns = min_ns;
    bench->max_time_ns = max_ns;
    bench->ops_per_sec = (double)iterations / ((double)total_ns / 1e9);

    /* 计算标准差 */
    double sum_sq = 0;
    for (uint64_t i = 0; i < iterations; i++) {
        double diff = times[i] - bench->avg_time_ns;
        sum_sq += diff * diff;
    }
    bench->std_dev_ns = sqrt(sum_sq / iterations);

    lv00_free((void **) &times);
    return true;
}

Lv00Benchmark *lv00_benchmark_run(const char *name) {
    for (uint32_t i = 0; i < g_benchmarks.count; i++) {
        if (strcmp(g_benchmarks.benchmarks[i].name, name) == 0) {
            return &g_benchmarks.benchmarks[i];
        }
    }
    return NULL;
}

uint32_t lv00_benchmark_run_all(Lv00Benchmark **out_results, uint32_t max_count) {
    if (!out_results) {
        return 0;
    }

    uint32_t count = g_benchmarks.count < max_count ? g_benchmarks.count : max_count;
    for (uint32_t i = 0; i < count; i++) {
        out_results[i] = &g_benchmarks.benchmarks[i];
    }

    return count;
}

void lv00_benchmark_destroy(Lv00Benchmark *bench) {
    /* 基准测试结果存储在静态数组中，无需单独释放 */
    (void)bench;
}

/* ============== 测试报告实现 ============== */

void lv00_test_report_destroy(Lv00TestReport *report) {
    if (!report) {
        return;
    }
    lv00_free((void **) &report->suites);
    lv00_free((void **) &report->json_output);
    lv00_free((void **) &report->xml_output);
    lv00_free((void **) &report->html_output);
    lv00_free((void **) &report);
}

void lv00_test_result_destroy(Lv00TestResult *result) {
    if (!result) {
        return;
    }
    lv00_free((void **) &result->failures);
    lv00_free((void **) &result);
}

void lv00_test_report_print(const Lv00TestReport *report, FILE *stream) {
    if (!report || !stream) {
        return;
    }

    fprintf(stream, "\n========== Test Results ==========\n");
    fprintf(stream, "Total: %u tests\n", report->total_tests);
    fprintf(stream, "Passed: %u\n", report->passed_count);
    fprintf(stream, "Failed: %u\n", report->failed_count);
    fprintf(stream, "Skipped: %u\n", report->skipped_count);
    fprintf(stream, "Time: %.3f ms\n", (double)report->total_time_ns / 1e6);
    fprintf(stream, "==================================\n\n");

    /* 打印失败详情 */
    if (report->failed_count > 0) {
        fprintf(stream, "Failed Tests:\n");
        for (uint32_t i = 0; i < report->suite_count; i++) {
            const Lv00TestSuite *suite = &report->suites[i];
            for (uint32_t j = 0; j < suite->case_count; j++) {
                const Lv00TestCase *test = &suite->cases[j];
                if (test->status == TEST_STATUS_FAILED) {
                    fprintf(stream, "  - %s.%s: %s\n", suite->name, test->name, test->message);
                }
            }
        }
        fprintf(stream, "\n");
    }
}

/**
 * @brief 测试报告转JSON
 * @details 将测试报告序列化为 JSON 格式字符串，包含总测试数、通过数、失败数、跳过数、
 *          总耗时以及各测试套件的统计信息。
 * @param report 测试报告指针，为 NULL 时返回 NULL
 * @return 新分配的 JSON 字符串（调用者需使用 lv00_free 释放），失败时返回 NULL
 */
char *lv00_test_report_to_json(const Lv00TestReport *report) {
    if (!report) {
        return NULL;
    }

    char *json = (char *)lv00_malloc(8192);
    if (!json) {
        return NULL;
    }

    int pos = snprintf(json, 8192,
                       "{"
                       "\"total\":%u,"
                       "\"passed\":%u,"
                       "\"failed\":%u,"
                       "\"skipped\":%u,"
                       "\"time_ns\":%lld,"
                       "\"suites\":[",
                       report->total_tests,
                       report->passed_count,
                       report->failed_count,
                       report->skipped_count,
                       (long long)report->total_time_ns);
    if (pos >= 8192) {
        pos = 8192 - 1;
    }

    for (uint32_t i = 0; i < report->suite_count; i++) {
        const Lv00TestSuite *suite = &report->suites[i];
        pos += snprintf(json + pos, 8192 - pos,
                        "{\"name\":\"%s\",\"passed\":%u,\"failed\":%u,\"skipped\":%u}",
                        suite->name, suite->passed_count, suite->failed_count, suite->skipped_count);
        if (pos >= 8192) {
            pos = 8192 - 1;
            break;
        }
        if (i < report->suite_count - 1) {
            pos += snprintf(json + pos, 8192 - pos, ",");
            if (pos >= 8192) {
                pos = 8192 - 1;
                break;
            }
        }
    }

    snprintf(json + pos, 8192 - pos, "]}");

    return json;
}

/**
 * @brief 测试报告转XML
 * @details 将测试报告序列化为 JUnit 兼容的 XML 格式字符串，包含测试套件、
 *          测试用例、执行时间、失败信息等。可用于 CI/CD 系统的测试报告集成。
 * @param report 测试报告指针，为 NULL 时返回 NULL
 * @return 新分配的 XML 字符串（调用者需使用 lv00_free 释放），失败时返回 NULL
 */
char *lv00_test_report_to_xml(const Lv00TestReport *report) {
    if (!report) {
        return NULL;
    }

    char *xml = (char *)lv00_malloc(16384);
    if (!xml) {
        return NULL;
    }

    int pos = snprintf(xml, 16384,
                       "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                       "<testsuites tests=\"%u\" failures=\"%u\" skipped=\"%u\" time=\"%.3f\">\n",
                       report->total_tests, report->failed_count, report->skipped_count,
                       (double)report->total_time_ns / 1e9);
    if (pos >= 16384) {
        pos = 16384 - 1;
    }

    for (uint32_t i = 0; i < report->suite_count; i++) {
        const Lv00TestSuite *suite = &report->suites[i];
        pos += snprintf(xml + pos, 16384 - pos,
                        "  <testsuite name=\"%s\" tests=\"%u\" failures=\"%u\" skipped=\"%u\">\n",
                        suite->name, suite->case_count, suite->failed_count, suite->skipped_count);
        if (pos >= 16384) {
            pos = 16384 - 1;
            break;
        }

        for (uint32_t j = 0; j < suite->case_count; j++) {
            const Lv00TestCase *test = &suite->cases[j];
            pos += snprintf(xml + pos, 16384 - pos,
                            "    <testcase name=\"%s\" time=\"%.6f\"",
                            test->name, (double)test->elapsed_ns / 1e9);
            if (pos >= 16384) {
                pos = 16384 - 1;
                break;
            }

            if (test->status == TEST_STATUS_FAILED) {
                pos += snprintf(xml + pos, 16384 - pos,
                                ">\n      <failure message=\"%s\"/>\n    </testcase>\n",
                                test->message);
            } else if (test->status == TEST_STATUS_SKIPPED) {
                pos += snprintf(xml + pos, 16384 - pos,
                                ">\n      <skipped/>\n    </testcase>\n");
            } else {
                pos += snprintf(xml + pos, 16384 - pos, "/>\n");
            }
            if (pos >= 16384) {
                pos = 16384 - 1;
                break;
            }
        }

        pos += snprintf(xml + pos, 16384 - pos, "  </testsuite>\n");
        if (pos >= 16384) {
            pos = 16384 - 1;
            break;
        }
    }

    snprintf(xml + pos, 16384 - pos, "</testsuites>\n");

    return xml;
}

/**
 * @brief 测试报告转HTML
 * @details 将测试报告序列化为自包含的 HTML 页面，包含内联 CSS 样式、
 *          测试汇总统计表格以及每个测试用例的状态和执行时间。
 *          通过/失败/跳过分别用绿色/红色/橙色标识。
 * @param report 测试报告指针，为 NULL 时返回 NULL
 * @return 新分配的 HTML 字符串（调用者需使用 lv00_free 释放），失败时返回 NULL
 */
char *lv00_test_report_to_html(const Lv00TestReport *report) {
    if (!report) {
        return NULL;
    }

    char *html = (char *)lv00_malloc(32768);
    if (!html) {
        return NULL;
    }

    int pos = snprintf(html, 32768,
                       "<!DOCTYPE html>\n"
                       "<html><head><title>Test Results</title>\n"
                       "<style>body{font-family:Arial,sans-serif;margin:20px;}"
                       ".passed{color:green;}.failed{color:red;}.skipped{color:orange;}"
                       "table{border-collapse:collapse;width:100%%;}"
                       "th,td{border:1px solid #ddd;padding:8px;text-align:left;}"
                       "th{background-color:#4CAF50;color:white;}</style></head>\n"
                       "<body><h1>Test Results</h1>\n"
                       "<p>Total: %u | Passed: <span class=\"passed\">%u</span> | "
                       "Failed: <span class=\"failed\">%u</span> | Skipped: <span class=\"skipped\">%u</span></p>\n"
                       "<table><tr><th>Suite</th><th>Test</th><th>Status</th><th>Time (ms)</th></tr>\n",
                       report->total_tests, report->passed_count, report->failed_count, report->skipped_count);
    if (pos >= 32768) {
        pos = 32768 - 1;
    }

    for (uint32_t i = 0; i < report->suite_count; i++) {
        const Lv00TestSuite *suite = &report->suites[i];
        for (uint32_t j = 0; j < suite->case_count; j++) {
            const Lv00TestCase *test = &suite->cases[j];
            const char *status_class = test->status == TEST_STATUS_PASSED ? "passed" :
                                       test->status == TEST_STATUS_FAILED ? "failed" : "skipped";
            const char *status_text = test->status == TEST_STATUS_PASSED ? "PASSED" :
                                      test->status == TEST_STATUS_FAILED ? "FAILED" : "SKIPPED";

            pos += snprintf(html + pos, 32768 - pos,
                            "<tr><td>%s</td><td>%s</td><td class=\"%s\">%s</td><td>%.3f</td></tr>\n",
                            suite->name, test->name, status_class, status_text,
                            (double)test->elapsed_ns / 1e6);
            if (pos >= 32768) {
                pos = 32768 - 1;
                break;
            }
        }
        if (pos >= 32768) {
            break;
        }
    }

    snprintf(html + pos, 32768 - pos, "</table></body></html>\n");

    return html;
}

bool lv00_test_report_write_file(const Lv00TestReport *report,
                                  const char *path,
                                  const char *format) {
    if (!report || !path || !format) {
        return false;
    }

    FILE *fp = fopen(path, "w");
    if (!fp) {
        return false;
    }

    char *content = NULL;
    if (strcmp(format, "json") == 0) {
        content = lv00_test_report_to_json(report);
    } else if (strcmp(format, "xml") == 0) {
        content = lv00_test_report_to_xml(report);
    } else if (strcmp(format, "html") == 0) {
        content = lv00_test_report_to_html(report);
    }

    if (content) {
        fputs(content, fp);
        lv00_free((void **) &content);
    }

    fclose(fp);
    return content != NULL;
}

/* ============== 主函数实现 ============== */

/**
 * @brief 测试主入口
 * @details 运行所有已注册的测试用例，将结果打印到标准输出，并根据失败数量返回退出码。
 *          适用于作为 main() 函数的简单包装，方便快速执行全部测试。
 * @param argc 命令行参数数量（当前未使用）
 * @param argv 命令行参数数组（当前未使用）
 * @return 所有测试通过返回 0，存在失败测试返回 1
 */
int lv00_test_main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    Lv00TestReport *report = lv00_test_run_all();
    if (!report) {
        fprintf(stderr, "Failed to run tests\n");
        return 1;
    }

    lv00_test_report_print(report, stdout);

    int exit_code = (report->failed_count > 0) ? 1 : 0;

    lv00_test_report_destroy(report);

    return exit_code;
}
