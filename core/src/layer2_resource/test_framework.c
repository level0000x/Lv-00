/**
 * @file test_framework.c
 * @brief 增强单元测试框架实现
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/lv_platform.h"

#include "lv/lv_file.h"
#include "lv/lv_lifecycle.h"

#include "test_framework.h"


#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


#include "lv_utils.h"


#include "lv/lv_json.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_thread.h"
#include "lv/lv_registry.h"


/* ============== 内部常量 ============== */

/** 初始测试套件容量 */
#define TEST_SUITE_INIT_CAPACITY 16

/** 初始测试用例容量 */
#define TEST_CASE_INIT_CAPACITY 64

/* ============== 全局状态 ============== */

/** @brief 套件注册表（通用注册表设施：key = 套件名，value = lvTestSuite*）。
 *  保持注册顺序（get_at 遍历顺序 = 注册顺序）；strcmp 查重由 lv_registry 承担。 */
lv_REGISTRY_STATIC(suite_registry, TEST_SUITE_INIT_CAPACITY);

/** @brief 用例注册表（通用注册表设施：key = "<suite>\x1f<case>"，value = boxed int 用例索引）。
 *  用例数据仍存储于 suite->cases 数组（报告/遍历按索引访问），注册表仅承担
 *  name→索引 查重与查找；索引在数组扩容 realloc 后保持有效。 */
lv_REGISTRY_STATIC(case_registry, TEST_CASE_INIT_CAPACITY);

static struct {
    lv_mutex_t mutex;
    bool initialized;

    /* 当前测试上下文 */
    lvTestCase *current_test;
    lvTestResult *current_result;

    /* 配置 */
    uint32_t timeout_ms;
    bool parallel_enabled;
    uint32_t max_threads;
} g_test_system = {0};

/* ============== 内部函数 ============== */

/** @brief 构造用例注册表 key（"<suite>\x1f<case>"，\x1f 为极少出现在名称中的控制字符） */
static void build_case_key(const char *suite_name, const char *test_name, char *buf, size_t bufsz) {
    snprintf(buf, bufsz, "%s\x1f%s", suite_name, test_name);
}

/** @brief 装箱用例索引（注册表 value；索引在 cases 数组扩容 realloc 后保持有效） */
static void *box_case_index(int idx) {
    int *p = (int *) lv_malloc(sizeof(int));
    if (p) {
        *p = idx;
    }
    return p;
}

/** @brief boxed 索引的注册表 destroy 回调适配器（void(*)(void*) 形态） */
static void box_case_index_destroy(void *value) {
    lv_free((void **) &value);
}

/** @brief 解箱用例索引 */
static int unbox_case_index(void *value) {
    return value ? *(int *) value : -1;
}

static lvTestSuite *find_or_create_suite(const char *name) {
    /* 查找现有套件（委托注册表 strcmp 查找） */
    lvTestSuite *existing = (lvTestSuite *) lv_registry_get(&g_suite_registry, name);
    if (existing) {
        return existing;
    }

    /* 创建新套件 */
    if (lv_registry_count(&g_suite_registry) >= lv_TEST_MAX_SUITES) {
        return NULL;
    }

    lvTestSuite *suite = (lvTestSuite *) lv_calloc(1, sizeof(lvTestSuite));
    if (!suite) {
        return NULL;
    }

    strncpy(suite->name, name, sizeof(suite->name) - 1);
    suite->cases = (lvTestCase *) lv_malloc(TEST_CASE_INIT_CAPACITY * sizeof(lvTestCase));
    if (!suite->cases) {
        lv_free((void **) &suite);
        return NULL;
    }
    suite->case_capacity = TEST_CASE_INIT_CAPACITY;

    /* 写入套件注册表（内部查重 + 尾部追加，保持注册顺序；失败则回滚） */
    if (!lv_registry_put(&g_suite_registry, name, suite)) {
        lv_free((void **) &suite->cases);
        lv_free((void **) &suite);
        return NULL;
    }
    return suite;
}

lv_LAZY_LOCK_DEFINE(g_test_init_lock);

static void init_test_system(void) {
    lv_lazy_lock_lock(&g_test_init_lock, g_test_init_lock_init_once);
    if (g_test_system.initialized) {
        lv_lazy_lock_unlock(&g_test_init_lock);
        return;
    }

    memset(&g_test_system, 0, sizeof(g_test_system));
    lv_mutex_init(&g_test_system.mutex);
    suite_registry_ensure();
    case_registry_ensure();
    g_test_system.timeout_ms = 30000; /* 默认 30 秒超时 */
    g_test_system.initialized = true;
    lv_lazy_lock_unlock(&g_test_init_lock);
}

/* ============== 测试注册实现 ============== */

bool lv_test_register(const char *suite_name, const char *test_name, lvTestFunc func) {
    return lv_test_register_with_fixture(suite_name, test_name, func, NULL, NULL);
}

bool lv_test_register_with_fixture(const char *suite_name, const char *test_name, lvTestFunc func,
                                   lvTestSetupFunc setup, lvTestTeardownFunc teardown) {
    init_test_system();

    if (!suite_name || !test_name || !func) {
        return false;
    }

    lv_mutex_lock(&g_test_system.mutex);

    lvTestSuite *suite = find_or_create_suite(suite_name);
    if (!suite) {
        lv_mutex_unlock(&g_test_system.mutex);
        return false;
    }

    /* 检查是否已存在（委托注册表 strcmp 查重，key = "<suite>\x1f<case>"） */
    char case_key[2 * lv_TEST_NAME_MAX_LEN + 2];
    build_case_key(suite_name, test_name, case_key, sizeof(case_key));
    if (lv_registry_get(&g_case_registry, case_key) != NULL) {
        lv_mutex_unlock(&g_test_system.mutex);
        return false;
    }

    /* 扩容（统一委托 lv_ensure_capacity；case_capacity 为 uint32_t，经局部 int 桥接） */
    if (suite->case_count >= suite->case_capacity) {
        int case_cap = (int) suite->case_capacity;
        if (!lv_ensure_capacity((void **) &suite->cases, (int) suite->case_count, &case_cap, sizeof(lvTestCase), 0)) {
            lv_mutex_unlock(&g_test_system.mutex);
            return false;
        }
        suite->case_capacity = (uint32_t) case_cap;
    }

    /* 添加测试用例 */
    lvTestCase *test_case = &suite->cases[suite->case_count++];
    memset(test_case, 0, sizeof(lvTestCase));

    strncpy(test_case->name, test_name, sizeof(test_case->name) - 1);
    strncpy(test_case->suite, suite_name, sizeof(test_case->suite) - 1);
    test_case->func = func;
    test_case->setup = setup;
    test_case->teardown = teardown;
    test_case->status = TEST_STATUS_PENDING;

    /* 登记到用例注册表（value = boxed 用例索引；失败则回滚用例，保持一致性） */
    void *boxed = box_case_index(suite->case_count - 1);
    if (!boxed || !lv_registry_put_ex(&g_case_registry, case_key, boxed, box_case_index_destroy)) {
        if (boxed) {
            lv_free((void **) &boxed);
        }
        suite->case_count--;
        lv_mutex_unlock(&g_test_system.mutex);
        return false;
    }

    lv_mutex_unlock(&g_test_system.mutex);
    return true;
}

bool lv_test_register_suite_fixture(const char *suite_name, lvTestSetupFunc setup, lvTestTeardownFunc teardown) {
    init_test_system();

    if (!suite_name) {
        return false;
    }

    lv_mutex_lock(&g_test_system.mutex);

    lvTestSuite *suite = find_or_create_suite(suite_name);
    if (!suite) {
        lv_mutex_unlock(&g_test_system.mutex);
        return false;
    }

    suite->suite_setup = setup;
    suite->suite_teardown = teardown;

    lv_mutex_unlock(&g_test_system.mutex);
    return true;
}

bool lv_test_add_tag(const char *suite_name, const char *test_name, const char *tag) {
    if (!suite_name || !test_name || !tag) {
        return false;
    }

    /* 确保注册表已初始化（原实现依赖系统已初始化；此处显式确保，幂等） */
    init_test_system();

    lv_mutex_lock(&g_test_system.mutex);

    lvTestSuite *suite = (lvTestSuite *) lv_registry_get(&g_suite_registry, suite_name);
    if (suite) {
        char case_key[2 * lv_TEST_NAME_MAX_LEN + 2];
        build_case_key(suite_name, test_name, case_key, sizeof(case_key));
        void *boxed = lv_registry_get(&g_case_registry, case_key);
        if (boxed) {
            int case_index = unbox_case_index(boxed);
            if (case_index >= 0 && case_index < suite->case_count) {
                lvTestCase *test_case = &suite->cases[case_index];
                /* 添加标签（使用安全的字符串复制函数 lv_strlcpy） */
                if (test_case->tag_count < 8) {
                    if (test_case->tag_count == 0) {
                        test_case->tags = (char **) lv_malloc(8 * sizeof(char *));
                    }
                    test_case->tags[test_case->tag_count] = (char *) lv_malloc(strlen(tag) + 1);
                    if (test_case->tags[test_case->tag_count]) {
                        /* 使用安全的字符串复制函数，自动保证零终止 */
                        lv_strlcpy(test_case->tags[test_case->tag_count], tag, strlen(tag) + 1);
                        test_case->tag_count++;
                    }
                }
                lv_mutex_unlock(&g_test_system.mutex);
                return true;
            }
        }
    }

    lv_mutex_unlock(&g_test_system.mutex);
    return false;
}

bool lv_test_register_tag(const char *suite_name, const char *test_name, const char *tag) {
    return lv_test_add_tag(suite_name, test_name, tag);
}

/* ============== 断言实现 ============== */

void lv_assert_fail(const char *expr, const char *file, int line, const char *fmt, ...) {
    lvTestCase *test = g_test_system.current_test;
    if (!test) {
        return;
    }

    test->status = TEST_STATUS_FAILED;

    /* 记录失败信息 */
    char message[lv_TEST_MSG_MAX_LEN];
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

void lv_assert_pass(const char *file, int line) {
    (void) file;
    (void) line;
    /* 默认通过，无需操作 */
}

/* ============== 测试执行实现 ============== */

static lvTestResult *run_single_test(lvTestCase *test_case, lvTestSuite *suite) {
    lvTestResult *result = (lvTestResult *) lv_calloc(1, sizeof(lvTestResult));
    if (!result) {
        return NULL;
    }

    result->test_case = test_case;
    result->status = TEST_STATUS_RUNNING;

    g_test_system.current_test = test_case;
    g_test_system.current_result = result;

    int64_t start_time = (int64_t) lv_get_time_ns();

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

    int64_t end_time = (int64_t) lv_get_time_ns();
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

lvTestReport *lv_test_run_all(void) {
    init_test_system();

    lvTestReport *report = (lvTestReport *) lv_calloc(1, sizeof(lvTestReport));
    if (!report) {
        return NULL;
    }

    report->start_time_ns = (int64_t) lv_get_time_ns();

    lv_mutex_lock(&g_test_system.mutex);

    /* 分配套件数组 */
    int suite_total = lv_registry_count(&g_suite_registry);
    report->suites = (lvTestSuite *) lv_calloc((size_t) suite_total, sizeof(lvTestSuite));
    if (!report->suites) {
        lv_mutex_unlock(&g_test_system.mutex);
        lv_free((void **) &report);
        return NULL;
    }
    report->suite_count = (uint32_t) suite_total;

    /* 运行所有测试（按注册顺序遍历注册表条目，get_at 语义与原数组遍历一致） */
    for (int i = 0; i < suite_total; i++) {
        const char *suite_reg_name = NULL;
        void *suite_reg_value = NULL;
        if (!lv_registry_get_at(&g_suite_registry, i, &suite_reg_name, &suite_reg_value)) {
            continue;
        }
        lvTestSuite *suite = (lvTestSuite *) suite_reg_value;

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
            lvTestResult *result = run_single_test(&suite->cases[j], suite);
            if (result) {
                lv_test_result_destroy(result);
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

    lv_mutex_unlock(&g_test_system.mutex);

    report->end_time_ns = (int64_t) lv_get_time_ns();
    report->total_time_ns = report->end_time_ns - report->start_time_ns;

    return report;
}

lvTestReport *lv_test_run_suite(const char *suite_name) {
    init_test_system();

    if (!suite_name) {
        return NULL;
    }

    lv_mutex_lock(&g_test_system.mutex);

    lvTestSuite *suite = (lvTestSuite *) lv_registry_get(&g_suite_registry, suite_name);

    if (!suite) {
        lv_mutex_unlock(&g_test_system.mutex);
        return NULL;
    }

    lvTestReport *report = (lvTestReport *) lv_calloc(1, sizeof(lvTestReport));
    if (!report) {
        lv_mutex_unlock(&g_test_system.mutex);
        return NULL;
    }

    report->start_time_ns = (int64_t) lv_get_time_ns();
    report->suites = (lvTestSuite *) lv_calloc(1, sizeof(lvTestSuite));
    if (!report->suites) {
        lv_free((void **) &report);
        lv_mutex_unlock(&g_test_system.mutex);
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
        lvTestResult *result = run_single_test(&suite->cases[j], suite);
        if (result) {
            lv_test_result_destroy(result);
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

    lv_mutex_unlock(&g_test_system.mutex);

    report->end_time_ns = (int64_t) lv_get_time_ns();
    report->total_time_ns = report->end_time_ns - report->start_time_ns;

    return report;
}

lvTestResult *lv_test_run_single(const char *suite_name, const char *test_name) {
    init_test_system();

    if (!suite_name || !test_name) {
        return NULL;
    }

    lv_mutex_lock(&g_test_system.mutex);

    lvTestResult *result = NULL;

    lvTestSuite *suite = (lvTestSuite *) lv_registry_get(&g_suite_registry, suite_name);
    if (suite) {
        char case_key[2 * lv_TEST_NAME_MAX_LEN + 2];
        build_case_key(suite_name, test_name, case_key, sizeof(case_key));
        void *boxed = lv_registry_get(&g_case_registry, case_key);
        if (boxed) {
            int case_index = unbox_case_index(boxed);
            if (case_index >= 0 && case_index < suite->case_count) {
                result = run_single_test(&suite->cases[case_index], suite);
            }
        }
    }

    lv_mutex_unlock(&g_test_system.mutex);

    return result;
}

lvTestReport *lv_test_run_by_tag(const char *tag) {
    init_test_system();

    if (!tag) {
        return NULL;
    }

    lvTestReport *report = (lvTestReport *) lv_calloc(1, sizeof(lvTestReport));
    if (!report) {
        return NULL;
    }

    report->start_time_ns = (int64_t) lv_get_time_ns();

    lv_mutex_lock(&g_test_system.mutex);

    int suite_total = lv_registry_count(&g_suite_registry);
    for (int i = 0; i < suite_total; i++) {
        const char *suite_reg_name = NULL;
        void *suite_reg_value = NULL;
        if (!lv_registry_get_at(&g_suite_registry, i, &suite_reg_name, &suite_reg_value)) {
            continue;
        }
        lvTestSuite *suite = (lvTestSuite *) suite_reg_value;

        for (uint32_t j = 0; j < suite->case_count; j++) {
            lvTestCase *test_case = &suite->cases[j];

            /* 检查标签 */
            bool has_tag = false;
            for (uint32_t k = 0; k < test_case->tag_count; k++) {
                if (strcmp(test_case->tags[k], tag) == 0) {
                    has_tag = true;
                    break;
                }
            }

            if (has_tag) {
                lvTestResult *result = run_single_test(test_case, suite);
                if (result) {
                    report->passed_count += (result->status == TEST_STATUS_PASSED) ? 1 : 0;
                    report->failed_count += (result->status == TEST_STATUS_FAILED) ? 1 : 0;
                    report->total_tests++;
                    lv_test_result_destroy(result);
                }
            }
        }
    }

    lv_mutex_unlock(&g_test_system.mutex);

    report->end_time_ns = (int64_t) lv_get_time_ns();
    report->total_time_ns = report->end_time_ns - report->start_time_ns;

    return report;
}

lvTestReport *lv_test_run_by_pattern(const char *pattern) {
    init_test_system();

    if (!pattern) {
        return NULL;
    }

    lvTestReport *report = (lvTestReport *) lv_calloc(1, sizeof(lvTestReport));
    if (!report) {
        return NULL;
    }

    report->start_time_ns = (int64_t) lv_get_time_ns();

    lv_mutex_lock(&g_test_system.mutex);

    /* 简单的模式匹配（仅支持 * 通配符） */
    int suite_total = lv_registry_count(&g_suite_registry);
    for (int i = 0; i < suite_total; i++) {
        const char *suite_reg_name = NULL;
        void *suite_reg_value = NULL;
        if (!lv_registry_get_at(&g_suite_registry, i, &suite_reg_name, &suite_reg_value)) {
            continue;
        }
        lvTestSuite *suite = (lvTestSuite *) suite_reg_value;

        for (uint32_t j = 0; j < suite->case_count; j++) {
            lvTestCase *test_case = &suite->cases[j];

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
                lvTestResult *result = run_single_test(test_case, suite);
                if (result) {
                    report->passed_count += (result->status == TEST_STATUS_PASSED) ? 1 : 0;
                    report->failed_count += (result->status == TEST_STATUS_FAILED) ? 1 : 0;
                    report->total_tests++;
                    lv_test_result_destroy(result);
                }
            }
        }
    }

    lv_mutex_unlock(&g_test_system.mutex);

    report->end_time_ns = (int64_t) lv_get_time_ns();
    report->total_time_ns = report->end_time_ns - report->start_time_ns;

    return report;
}

void lv_test_set_timeout(uint32_t timeout_ms) {
    g_test_system.timeout_ms = timeout_ms;
}

void lv_test_set_parallel(bool enable, uint32_t max_threads) {
    g_test_system.parallel_enabled = enable;
    g_test_system.max_threads = max_threads;
}

/* ============== 参数化测试实现 ============== */

void *lv_test_get_data(void) {
    lvTestCase *test = g_test_system.current_test;
    return test ? test->test_data : NULL;
}

uint32_t lv_test_get_data_index(void) {
    lvTestCase *test = g_test_system.current_test;
    return test ? test->data_index : 0;
}

bool lv_test_register_parameterized(const char *suite_name, const char *test_name, lvTestFunc func,
                                    lvTestDataGenerator generator, uint32_t data_count) {
    if (!suite_name || !test_name || !func || !generator || data_count == 0) {
        return false;
    }

    /* 为每个数据点注册一个测试 */
    for (uint32_t i = 0; i < data_count; i++) {
        char name[lv_TEST_NAME_MAX_LEN];
        snprintf(name, sizeof(name), "%s/%u", test_name, i);

        if (!lv_test_register(suite_name, name, func)) {
            return false;
        }

        /* 存储数据索引 */
        lv_mutex_lock(&g_test_system.mutex);
        lvTestSuite *suite = (lvTestSuite *) lv_registry_get(&g_suite_registry, suite_name);
        if (suite && suite->case_count > 0) {
            lvTestCase *test_case = &suite->cases[suite->case_count - 1];
            test_case->test_data = generator(i);
            test_case->data_index = i;
        }
        lv_mutex_unlock(&g_test_system.mutex);
    }

    return true;
}

bool lv_test_register_data_driven(const char *suite_name, const char *test_name, lvTestFunc func,
                                  lvTestDataGenerator generator, int data_count) {
    if (data_count < 0) {
        return false;
    }
    return lv_test_register_parameterized(suite_name, test_name, func, generator, (uint32_t) data_count);
}

/* ============== 性能基准测试实现 ============== */

static struct {
    lvBenchmark benchmarks[lv_TEST_MAX_CASES];
    uint32_t count;
} g_benchmarks = {0};

bool lv_benchmark_register(const char *name, lvBenchmarkFunc func, uint64_t iterations) {
    if (!name || !func || g_benchmarks.count >= lv_TEST_MAX_CASES) {
        return false;
    }
    if (iterations == 0) {
        fprintf(stderr, "Error: benchmark '%s' registered with zero iterations\n", name);
        return false;
    }

    lvBenchmark *bench = &g_benchmarks.benchmarks[g_benchmarks.count++];
    strncpy(bench->name, name, sizeof(bench->name) - 1);
    bench->iterations = iterations;

    /* 运行基准测试 */
    int64_t *times = (int64_t *) lv_calloc((size_t) iterations, sizeof(int64_t));
    if (!times) {
        return false;
    }

    int64_t total_ns = 0;
    double min_ns = 1e18;
    double max_ns = 0;

    for (uint64_t i = 0; i < iterations; i++) {
        int64_t start = (int64_t) lv_get_time_ns();
        func();
        int64_t end = (int64_t) lv_get_time_ns();
        times[i] = end - start;
        total_ns += times[i];

        if (times[i] < min_ns)
            min_ns = times[i];
        if (times[i] > max_ns)
            max_ns = times[i];
    }

    bench->total_time_ns = total_ns;
    bench->avg_time_ns = (double) total_ns / iterations;
    bench->min_time_ns = min_ns;
    bench->max_time_ns = max_ns;
    bench->ops_per_sec = (double) iterations / ((double) total_ns / 1e9);

    /* 计算标准差 */
    double sum_sq = 0;
    for (uint64_t i = 0; i < iterations; i++) {
        double diff = times[i] - bench->avg_time_ns;
        sum_sq += diff * diff;
    }
    bench->std_dev_ns = sqrt(sum_sq / iterations);

    lv_free((void **) &times);
    return true;
}

lvBenchmark *lv_benchmark_run(const char *name) {
    for (uint32_t i = 0; i < g_benchmarks.count; i++) {
        if (strcmp(g_benchmarks.benchmarks[i].name, name) == 0) {
            return &g_benchmarks.benchmarks[i];
        }
    }
    return NULL;
}

uint32_t lv_benchmark_run_all(lvBenchmark **out_results, uint32_t max_count) {
    if (!out_results) {
        return 0;
    }

    uint32_t count = g_benchmarks.count < max_count ? g_benchmarks.count : max_count;
    for (uint32_t i = 0; i < count; i++) {
        out_results[i] = &g_benchmarks.benchmarks[i];
    }

    return count;
}

void lv_benchmark_destroy(lvBenchmark *bench) {
    /* 基准测试结果存储在静态数组中，无需单独释放 */
    (void) bench;
}

/* ============== 测试报告实现 ============== */

/* lv_test_report_destroy 字段描述表：4 个纯指针字段，全部置 NULL 安全 */
static const lvFieldDesc s_test_report_destroy_fields[] = {
    lv_FIELD_PLAIN(lvTestReport, suites),
    lv_FIELD_PLAIN(lvTestReport, json_output),
    lv_FIELD_PLAIN(lvTestReport, xml_output),
    lv_FIELD_PLAIN(lvTestReport, html_output),
};

void lv_test_report_destroy(lvTestReport *report) {
    if (!report) {
        return;
    }
    lv_obj_destroy_fields(report, s_test_report_destroy_fields,
                          sizeof(s_test_report_destroy_fields) / sizeof(s_test_report_destroy_fields[0]));
    lv_free((void **) &report);
}

void lv_test_result_destroy(lvTestResult *result) {
    if (!result) {
        return;
    }
    lv_free((void **) &result->failures);
    lv_free((void **) &result);
}

void lv_test_report_print(const lvTestReport *report, FILE *stream) {
    if (!report || !stream) {
        return;
    }

    fprintf(stream, "\n========== Test Results ==========\n");
    fprintf(stream, "Total: %u tests\n", report->total_tests);
    fprintf(stream, "Passed: %u\n", report->passed_count);
    fprintf(stream, "Failed: %u\n", report->failed_count);
    fprintf(stream, "Skipped: %u\n", report->skipped_count);
    fprintf(stream, "Time: %.3f ms\n", (double) report->total_time_ns / 1e6);
    fprintf(stream, "==================================\n\n");

    /* 打印失败详情 */
    if (report->failed_count > 0) {
        fprintf(stream, "Failed Tests:\n");
        for (uint32_t i = 0; i < report->suite_count; i++) {
            const lvTestSuite *suite = &report->suites[i];
            for (uint32_t j = 0; j < suite->case_count; j++) {
                const lvTestCase *test = &suite->cases[j];
                if (test->status == TEST_STATUS_FAILED) {
                    fprintf(stream, "  - %s.%s: %s\n", suite->name, test->name, test->message);
                }
            }
        }
        fprintf(stream, "\n");
    }
}

char *lv_test_report_to_json(const lvTestReport *report) {
    if (!report) {
        return NULL;
    }

    lvJsonBuf buf;
    if (!lv_json_buf_init(&buf, 1024)) {
        return NULL;
    }

    lv_json_buf_append_fmt(&buf,
                       "{"
                       "\"total\":%u,"
                       "\"passed\":%u,"
                       "\"failed\":%u,"
                       "\"skipped\":%u,"
                       "\"time_ns\":%lld,"
                       "\"suites\":[",
                       report->total_tests, report->passed_count, report->failed_count, report->skipped_count,
                       (long long) report->total_time_ns);

    for (uint32_t i = 0; i < report->suite_count; i++) {
        const lvTestSuite *suite = &report->suites[i];
        /* suite->name 经 append_string 自动 JSON 转义 */
        lv_json_buf_append_raw(&buf, "{\"name\":");
        lv_json_buf_append_string(&buf, suite->name);
        lv_json_buf_append_fmt(&buf, ",\"passed\":%u,\"failed\":%u,\"skipped\":%u}",
                        suite->passed_count, suite->failed_count, suite->skipped_count);
        if (i < report->suite_count - 1) {
            lv_json_buf_append_char(&buf, ',');
        }
    }

    lv_json_buf_append_raw(&buf, "]}");

    return lv_json_buf_finalize(&buf);
}

char *lv_test_report_to_xml(const lvTestReport *report) {
    if (!report) {
        return NULL;
    }

    /* lvStrBuf 动态构建（自动扩容），消除原 16KB 固定缓冲的报告截断 */
    lvStrBuf xml;
    lv_strbuf_init(&xml);

    lv_strbuf_printf(&xml,
                     "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                     "<testsuites tests=\"%u\" failures=\"%u\" skipped=\"%u\" time=\"%.3f\">\n",
                     report->total_tests, report->failed_count, report->skipped_count,
                     (double) report->total_time_ns / 1e9);

    for (uint32_t i = 0; i < report->suite_count; i++) {
        const lvTestSuite *suite = &report->suites[i];
        /* suite->name 经 XML 实体转义后写入属性（防止 XML 注入） */
        lvStrBuf esc_name = {0};
        lv_str_escape_xml(&esc_name, suite->name, suite->name ? strlen(suite->name) : 0);
        lv_strbuf_printf(&xml, "  <testsuite name=\"%s\" tests=\"%u\" failures=\"%u\" skipped=\"%u\">\n",
                         lv_strbuf_cstr(&esc_name), suite->case_count, suite->failed_count, suite->skipped_count);
        lv_strbuf_destroy(&esc_name);

        for (uint32_t j = 0; j < suite->case_count; j++) {
            const lvTestCase *test = &suite->cases[j];
            /* test->name 经 XML 实体转义后写入属性 */
            lvStrBuf esc_tname = {0};
            lv_str_escape_xml(&esc_tname, test->name, test->name ? strlen(test->name) : 0);
            lv_strbuf_printf(&xml, "    <testcase name=\"%s\" time=\"%.6f\"",
                             lv_strbuf_cstr(&esc_tname), (double) test->elapsed_ns / 1e9);
            lv_strbuf_destroy(&esc_tname);

            if (test->status == TEST_STATUS_FAILED) {
                /* test->message 经 XML 实体转义后写入 <failure message> */
                lvStrBuf esc_msg = {0};
                lv_str_escape_xml(&esc_msg, test->message, test->message ? strlen(test->message) : 0);
                lv_strbuf_printf(&xml, ">\n      <failure message=\"%s\"/>\n    </testcase>\n",
                                 lv_strbuf_cstr(&esc_msg));
                lv_strbuf_destroy(&esc_msg);
            } else if (test->status == TEST_STATUS_SKIPPED) {
                lv_strbuf_printf(&xml, ">\n      <skipped/>\n    </testcase>\n");
            } else {
                lv_strbuf_printf(&xml, "/>\n");
            }
        }

        lv_strbuf_printf(&xml, "  </testsuite>\n");
    }

    lv_strbuf_printf(&xml, "</testsuites>\n");

    return lv_strbuf_to_string(&xml);
}

char *lv_test_report_to_html(const lvTestReport *report) {
    if (!report) {
        return NULL;
    }

    /* lvStrBuf 动态构建（自动扩容），消除原 32KB 固定缓冲的报告截断 */
    lvStrBuf html;
    lv_strbuf_init(&html);

    lv_strbuf_printf(&html,
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

    for (uint32_t i = 0; i < report->suite_count; i++) {
        const lvTestSuite *suite = &report->suites[i];
        for (uint32_t j = 0; j < suite->case_count; j++) {
            const lvTestCase *test = &suite->cases[j];
            const char *status_class = test->status == TEST_STATUS_PASSED   ? "passed"
                                       : test->status == TEST_STATUS_FAILED ? "failed"
                                                                            : "skipped";
            const char *status_text = test->status == TEST_STATUS_PASSED   ? "PASSED"
                                      : test->status == TEST_STATUS_FAILED ? "FAILED"
                                                                           : "SKIPPED";

            /* suite->name / test->name 经 HTML 实体转义（lv_str_html_escape 两遍法，防止 HTML 注入） */
            size_t esc_slen = suite->name ? strlen(suite->name) : 0;
            size_t need_s = lv_str_html_escape(suite->name, esc_slen, NULL, 0);
            char *esc_suite = (char *) lv_malloc(need_s + 1);
            size_t esc_tlen = test->name ? strlen(test->name) : 0;
            size_t need_t = lv_str_html_escape(test->name, esc_tlen, NULL, 0);
            char *esc_test = (char *) lv_malloc(need_t + 1);
            if (esc_suite)
                lv_str_html_escape(suite->name, esc_slen, esc_suite, need_s + 1);
            if (esc_test)
                lv_str_html_escape(test->name, esc_tlen, esc_test, need_t + 1);

            lv_strbuf_printf(&html,
                             "<tr><td>%s</td><td>%s</td><td class=\"%s\">%s</td><td>%.3f</td></tr>\n",
                             esc_suite ? esc_suite : (suite->name ? suite->name : ""),
                             esc_test ? esc_test : (test->name ? test->name : ""), status_class, status_text,
                             (double) test->elapsed_ns / 1e6);
            lv_free((void **) &esc_suite);
            lv_free((void **) &esc_test);
        }
    }

    lv_strbuf_printf(&html, "</table></body></html>\n");

    return lv_strbuf_to_string(&html);
}

bool lv_test_report_write_file(const lvTestReport *report, const char *path, const char *format) {
    if (!report || !path || !format) {
        return false;
    }

    FILE *fp = lv_file_open(path, "w");
    if (!fp) {
        return false;
    }

    char *content = NULL;
    if (strcmp(format, "json") == 0) {
        content = lv_test_report_to_json(report);
    } else if (strcmp(format, "xml") == 0) {
        content = lv_test_report_to_xml(report);
    } else if (strcmp(format, "html") == 0) {
        content = lv_test_report_to_html(report);
    }

    if (content) {
        fputs(content, fp);
        lv_free((void **) &content);
    }

    lv_file_close(fp);
    return content != NULL;
}

/* ============== 主函数实现 ============== */

int lv_test_main(int argc, char **argv) {
    (void) argc;
    (void) argv;

    lvTestReport *report = lv_test_run_all();
    if (!report) {
        fprintf(stderr, "Failed to run tests\n");
        return 1;
    }

    lv_test_report_print(report, stdout);

    int exit_code = (report->failed_count > 0) ? 1 : 0;

    lv_test_report_destroy(report);

    return exit_code;
}
