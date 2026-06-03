/**
 * @file test_framework.h
 * @brief 增强单元测试框架
 *
 * @details 提供完整的单元测试功能：
 *   1. 测试用例管理：注册、发现、执行
 *   2. 断言宏：丰富的断言类型
 *   3. 测试夹具：setup/teardown 支持
 *   4. 参数化测试：数据驱动测试
 *   5. 测试报告：多种格式输出
 *   6. 性能基准：微基准测试
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#ifndef LV00_TEST_FRAMEWORK_H
#define LV00_TEST_FRAMEWORK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* ============== 配置常量 ============== */

/** 测试名称最大长度 */
#define LV00_TEST_NAME_MAX_LEN 128

/** 测试套件名称最大长度 */
#define LV00_TEST_SUITE_MAX_LEN 64

/** 错误消息最大长度 */
#define LV00_TEST_MSG_MAX_LEN 512

/** 最大测试用例数 */
#define LV00_TEST_MAX_CASES 1000

/** 最大测试套件数 */
#define LV00_TEST_MAX_SUITES 100

/** 基准测试最大迭代数 */
#define LV00_BENCH_MAX_ITERATIONS 1000000

/* ============== 前向声明 ============== */

typedef struct Lv00TestCase Lv00TestCase;
typedef struct Lv00TestSuite Lv00TestSuite;
typedef struct Lv00TestResult Lv00TestResult;
typedef struct Lv00TestReport Lv00TestReport;
typedef struct Lv00Benchmark Lv00Benchmark;

/* ============== 测试状态 ============== */

/**
 * @brief 测试结果状态
 */
typedef enum {
    TEST_STATUS_PENDING,        /**< 待执行 */
    TEST_STATUS_RUNNING,        /**< 执行中 */
    TEST_STATUS_PASSED,         /**< 通过 */
    TEST_STATUS_FAILED,         /**< 失败 */
    TEST_STATUS_SKIPPED,        /**< 跳过 */
    TEST_STATUS_ERROR,          /**< 错误 */
    TEST_STATUS_TIMEOUT         /**< 超时 */
} Lv00TestStatus;

/**
 * @brief 测试严重性
 */
typedef enum {
    TEST_SEVERITY_INFO,         /**< 信息 */
    TEST_SEVERITY_WARNING,      /**< 警告 */
    TEST_SEVERITY_ERROR,        /**< 错误 */
    TEST_SEVERITY_FATAL         /**< 致命 */
} Lv00TestSeverity;

/* ============== 测试用例 ============== */

/**
 * @brief 测试函数类型
 */
typedef void (*Lv00TestFunc)(void);

/**
 * @brief Setup/Teardown 函数类型
 */
typedef void (*Lv00TestSetupFunc)(void);
typedef void (*Lv00TestTeardownFunc)(void);

/**
 * @brief 测试用例结构 —— 描述单个测试的名称、函数和生命周期回调
 */
struct Lv00TestCase {
    char name[LV00_TEST_NAME_MAX_LEN];  /**< 测试名称 */
    char suite[LV00_TEST_SUITE_MAX_LEN];/**< 所属套件 */
    Lv00TestFunc func;                  /**< 测试函数 */
    Lv00TestSetupFunc setup;            /**< Setup 函数 */
    Lv00TestTeardownFunc teardown;      /**< Teardown 函数 */

    /* 标签 */
    char **tags;                        /**< 标签数组 */
    uint32_t tag_count;                 /**< 标签数量 */

    /* 状态 */
    Lv00TestStatus status;              /**< 执行状态 */
    char message[LV00_TEST_MSG_MAX_LEN];/**< 结果消息 */
    char file[256];                     /**< 源文件 */
    int line;                           /**< 行号 */

    /* 性能 */
    int64_t elapsed_ns;                 /**< 执行时间（纳秒） */
    uint64_t memory_used;               /**< 内存使用 */

    /* 参数化 */
    void *test_data;                    /**< 测试数据 */
    uint32_t data_index;                /**< 数据索引 */
};

/**
 * @brief 测试套件结构 —— 包含多个相关测试用例的集合
 */
struct Lv00TestSuite {
    char name[LV00_TEST_SUITE_MAX_LEN]; /**< 套件名称 */
    Lv00TestCase *cases;                /**< 测试用例数组 */
    uint32_t case_count;                /**< 用例数量 */
    uint32_t case_capacity;             /**< 用例容量 */

    Lv00TestSetupFunc suite_setup;      /**< 套件 Setup */
    Lv00TestTeardownFunc suite_teardown;/**< 套件 Teardown */

    /* 统计 */
    uint32_t passed_count;              /**< 通过数 */
    uint32_t failed_count;              /**< 失败数 */
    uint32_t skipped_count;             /**< 跳过数 */
};

/* ============== 测试结果 ============== */

/**
 * @brief 断言失败记录
 */
typedef struct {
    char expression[256];       /**< 断言表达式 */
    char file[256];             /**< 源文件 */
    int line;                   /**< 行号 */
    char message[LV00_TEST_MSG_MAX_LEN]; /**< 消息 */
    Lv00TestSeverity severity;  /**< 严重性 */
} Lv00AssertionFailure;

/**
 * @brief 测试结果结构 —— 记录单个测试用例的执行状态和失败详情
 */
struct Lv00TestResult {
    Lv00TestCase *test_case;            /**< 关联测试用例 */
    Lv00TestStatus status;              /**< 状态 */
    int64_t elapsed_ns;                 /**< 执行时间 */

    Lv00AssertionFailure *failures;     /**< 失败记录 */
    uint32_t failure_count;             /**< 失败数量 */

    char output[4096];                  /**< 标准输出 */
    char error[4096];                   /**< 错误输出 */
};

/**
 * @brief 测试报告
 */
struct Lv00TestReport {
    Lv00TestSuite *suites;              /**< 套件数组 */
    uint32_t suite_count;               /**< 套件数量 */

    uint32_t total_tests;               /**< 总测试数 */
    uint32_t passed_count;              /**< 通过数 */
    uint32_t failed_count;              /**< 失败数 */
    uint32_t skipped_count;             /**< 跳过数 */
    uint32_t error_count;               /**< 错误数 */

    int64_t total_time_ns;              /**< 总时间 */
    int64_t start_time_ns;              /**< 开始时间 */
    int64_t end_time_ns;                /**< 结束时间 */

    char *json_output;                  /**< JSON 输出 */
    char *xml_output;                   /**< XML 输出 */
    char *html_output;                  /**< HTML 输出 */
};

/* ============== 测试注册 ============== */

/**
 * @brief 注册测试用例
 * @param suite_name 套件名称
 * @param test_name 测试名称
 * @param func 测试函数
 * @return 是否成功
 */
bool lv00_test_register(const char *suite_name, const char *test_name, Lv00TestFunc func);

/**
 * @brief 注册带夹具的测试用例
 * @param suite_name 套件名称
 * @param test_name 测试名称
 * @param func 测试函数
 * @param setup Setup 函数
 * @param teardown Teardown 函数
 * @return 是否成功
 */
bool lv00_test_register_with_fixture(const char *suite_name, const char *test_name,
                                      Lv00TestFunc func,
                                      Lv00TestSetupFunc setup,
                                      Lv00TestTeardownFunc teardown);

/**
 * @brief 注册测试套件夹具
 * @param suite_name 套件名称
 * @param setup 套件 Setup
 * @param teardown 套件 Teardown
 * @return 是否成功
 */
bool lv00_test_register_suite_fixture(const char *suite_name,
                                       Lv00TestSetupFunc setup,
                                       Lv00TestTeardownFunc teardown);

/**
 * @brief 为测试添加标签
 * @param suite_name 套件名称
 * @param test_name 测试名称
 * @param tag 标签
 * @return 是否成功
 */
bool lv00_test_add_tag(const char *suite_name, const char *test_name, const char *tag);

/* ============== 测试执行 ============== */

/**
 * @brief 运行所有测试
 * @return 测试报告
 */
Lv00TestReport *lv00_test_run_all(void);

/**
 * @brief 运行指定套件的测试
 * @param suite_name 套件名称
 * @return 测试报告
 */
Lv00TestReport *lv00_test_run_suite(const char *suite_name);

/**
 * @brief 运行指定测试
 * @param suite_name 套件名称
 * @param test_name 测试名称
 * @return 测试结果
 */
Lv00TestResult *lv00_test_run_single(const char *suite_name, const char *test_name);

/**
 * @brief 运行带标签的测试
 * @param tag 标签
 * @return 测试报告
 */
Lv00TestReport *lv00_test_run_by_tag(const char *tag);

/**
 * @brief 运行匹配模式的测试
 * @param pattern 匹配模式（支持通配符）
 * @return 测试报告
 */
Lv00TestReport *lv00_test_run_by_pattern(const char *pattern);

/**
 * @brief 设置测试超时
 * @param timeout_ms 超时时间（毫秒）
 */
void lv00_test_set_timeout(uint32_t timeout_ms);

/**
 * @brief 设置并行执行
 * @param enable 是否启用
 * @param max_threads 最大线程数
 */
void lv00_test_set_parallel(bool enable, uint32_t max_threads);

/* ============== 断言宏 ============== */

/**
 * @brief 内部断言函数
 */
void lv00_assert_fail(const char *expr, const char *file, int line, const char *fmt, ...);
void lv00_assert_pass(const char *file, int line);

/* 基本断言 */
#define LV00_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            lv00_assert_fail(#expr, __FILE__, __LINE__, NULL); \
            return; \
        } \
    } while (0)

#define LV00_ASSERT_TRUE(expr) LV00_ASSERT(expr)
#define LV00_ASSERT_FALSE(expr) LV00_ASSERT(!(expr))

/* 相等断言 */
#define LV00_ASSERT_EQ(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            lv00_assert_fail(#expected " == " #actual, __FILE__, __LINE__, \
                             "Expected: %lld, Actual: %lld", (long long)(expected), (long long)(actual)); \
            return; \
        } \
    } while (0)

#define LV00_ASSERT_NE(expected, actual) \
    do { \
        if ((expected) == (actual)) { \
            lv00_assert_fail(#expected " != " #actual, __FILE__, __LINE__, \
                             "Values are equal: %lld", (long long)(expected)); \
            return; \
        } \
    } while (0)

#define LV00_ASSERT_STR_EQ(expected, actual) \
    do { \
        if (strcmp((expected), (actual)) != 0) { \
            lv00_assert_fail(#expected " == " #actual, __FILE__, __LINE__, \
                             "Expected: '%s', Actual: '%s'", (expected), (actual)); \
            return; \
        } \
    } while (0)

/* 浮点断言 */
#define LV00_ASSERT_FLOAT_EQ(expected, actual, tolerance) \
    do { \
        double _e = (expected), _a = (actual), _t = (tolerance); \
        if (fabs(_e - _a) > _t) { \
            lv00_assert_fail(#expected " == " #actual, __FILE__, __LINE__, \
                             "Expected: %g, Actual: %g (tolerance: %g)", _e, _a, _t); \
            return; \
        } \
    } while (0)

/* 指针断言 */
#define LV00_ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            lv00_assert_fail(#ptr " != NULL", __FILE__, __LINE__, "Pointer is NULL"); \
            return; \
        } \
    } while (0)

#define LV00_ASSERT_NULL(ptr) \
    do { \
        if ((ptr) != NULL) { \
            lv00_assert_fail(#ptr " == NULL", __FILE__, __LINE__, "Pointer is not NULL: %p", (ptr)); \
            return; \
        } \
    } while (0)

/* 范围断言 */
#define LV00_ASSERT_IN_RANGE(value, min, max) \
    do { \
        if ((value) < (min) || (value) > (max)) { \
            lv00_assert_fail(#value " in [" #min ", " #max "]", __FILE__, __LINE__, \
                             "Value %lld not in range [%lld, %lld]", \
                             (long long)(value), (long long)(min), (long long)(max)); \
            return; \
        } \
    } while (0)

/* 消息断言 */
#define LV00_ASSERT_MSG(expr, fmt, ...) \
    do { \
        if (!(expr)) { \
            lv00_assert_fail(#expr, __FILE__, __LINE__, fmt, ##__VA_ARGS__); \
            return; \
        } \
    } while (0)

/* 跳过测试 */
#define LV00_SKIP(fmt, ...) \
    do { \
        lv00_assert_fail("SKIP", __FILE__, __LINE__, fmt, ##__VA_ARGS__); \
        return; \
    } while (0)

/* ============== 参数化测试 ============== */

/**
 * @brief 参数化测试数据生成器
 */
typedef void *(*Lv00TestDataGenerator)(uint32_t index);

/**
 * @brief 注册参数化测试
 * @param suite_name 套件名称
 * @param test_name 测试名称
 * @param func 测试函数
 * @param generator 数据生成器
 * @param data_count 数据数量
 * @return 是否成功
 */
bool lv00_test_register_parameterized(const char *suite_name, const char *test_name,
                                       Lv00TestFunc func,
                                       Lv00TestDataGenerator generator,
                                       uint32_t data_count);

/**
 * @brief 获取当前测试数据
 * @return 测试数据指针
 */
void *lv00_test_get_data(void);

/**
 * @brief 获取当前测试数据索引
 * @return 数据索引
 */
uint32_t lv00_test_get_data_index(void);

/* ============== 性能基准测试 ============== */

/**
 * @brief 基准测试结果
 */
struct Lv00Benchmark {
    char name[LV00_TEST_NAME_MAX_LEN];  /**< 基准名称 */
    uint64_t iterations;                /**< 迭代次数 */
    int64_t total_time_ns;              /**< 总时间 */
    double avg_time_ns;                 /**< 平均时间 */
    double min_time_ns;                 /**< 最小时间 */
    double max_time_ns;                 /**< 最大时间 */
    double std_dev_ns;                  /**< 标准差 */
    double ops_per_sec;                 /**< 每秒操作数 */
};

/**
 * @brief 基准测试函数类型
 */
typedef void (*Lv00BenchmarkFunc)(void);

/**
 * @brief 注册基准测试
 * @param name 基准名称
 * @param func 基准函数
 * @param iterations 迭代次数
 * @return 是否成功
 */
bool lv00_benchmark_register(const char *name, Lv00BenchmarkFunc func, uint64_t iterations);

/**
 * @brief 运行基准测试
 * @param name 基准名称
 * @return 基准结果
 */
Lv00Benchmark *lv00_benchmark_run(const char *name);

/**
 * @brief 运行所有基准测试
 * @param out_results 输出结果数组
 * @param max_count 最大数量
 * @return 实际数量
 */
uint32_t lv00_benchmark_run_all(Lv00Benchmark **out_results, uint32_t max_count);

/**
 * @brief 销毁基准结果
 * @param bench 结果指针
 */
void lv00_benchmark_destroy(Lv00Benchmark *bench);

/* ============== 测试报告 ============== */

/**
 * @brief 销毁测试报告
 * @param report 报告指针
 */
void lv00_test_report_destroy(Lv00TestReport *report);

/**
 * @brief 销毁测试结果
 * @param result 结果指针
 */
void lv00_test_result_destroy(Lv00TestResult *result);

/**
 * @brief 打印测试报告
 * @param report 报告
 * @param stream 输出流
 */
void lv00_test_report_print(const Lv00TestReport *report, FILE *stream);

/**
 * @brief 导出测试报告为 JSON
 * @param report 报告
 * @return JSON 字符串
 */
char *lv00_test_report_to_json(const Lv00TestReport *report);

/**
 * @brief 导出测试报告为 XML (JUnit 格式)
 * @param report 报告
 * @return XML 字符串
 */
char *lv00_test_report_to_xml(const Lv00TestReport *report);

/**
 * @brief 导出测试报告为 HTML
 * @param report 报告
 * @return HTML 字符串
 */
char *lv00_test_report_to_html(const Lv00TestReport *report);

/**
 * @brief 将测试报告写入文件
 * @param report 报告
 * @param path 文件路径
 * @param format 格式（"json", "xml", "html"）
 * @return 是否成功
 */
bool lv00_test_report_write_file(const Lv00TestReport *report,
                                  const char *path,
                                  const char *format);

/* ============== 便捷宏 ============== */

/**
 * @brief 定义测试用例
 */
#define LV00_TEST(suite, name) \
    static void lv00_test_##suite##_##name(void); \
    __attribute__((constructor)) static void lv00_register_##suite##_##name(void) { \
        lv00_test_register(#suite, #name, lv00_test_##suite##_##name); \
    } \
    static void lv00_test_##suite##_##name(void)

/**
 * @brief 定义带夹具的测试用例
 */
#define LV00_TEST_F(suite, name) \
    static void lv00_test_##suite##_##name(void); \
    static void lv00_setup_##suite(void); \
    static void lv00_teardown_##suite(void); \
    __attribute__((constructor)) static void lv00_register_##suite##_##name(void) { \
        lv00_test_register_with_fixture(#suite, #name, \
                                        lv00_test_##suite##_##name, \
                                        lv00_setup_##suite, \
                                        lv00_teardown_##suite); \
    } \
    static void lv00_test_##suite##_##name(void)

/**
 * @brief 定义基准测试
 */
#define LV00_BENCHMARK(name, iterations) \
    static void lv00_bench_##name(void); \
    __attribute__((constructor)) static void lv00_register_bench_##name(void) { \
        lv00_benchmark_register(#name, lv00_bench_##name, iterations); \
    } \
    static void lv00_bench_##name(void)

/* ============== 主函数 ============== */

/**
 * @brief 测试主函数
 * @param argc 参数数量
 * @param argv 参数数组
 * @return 退出码
 */
int lv00_test_main(int argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* LV00_TEST_FRAMEWORK_H */
