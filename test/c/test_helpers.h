/**
 * @file test_helpers.h
 * @brief Lv-00 测试套件公共辅助函数与宏
 *
 * 集中定义各测试文件重复使用的辅助函数和测试宏，避免代码重复，确保API调用一致性。
 *
 * 【v3.5.0 统一】现在包含 test_framework.h，因此所有测试文件均可使用
 * lv_test_run_all() / lv_test_report_print() 等结构化报告 API，
 * 以及 lv_ASSERT_* / lv_TEST 宏作为替代断言选项。
 *
 * 使用方法：
 *   1. 在测试文件中 #include "test_helpers.h"
 *   2. 定义全局计数器：int g_pass_count = 0; int g_fail_count = 0;
 *   3. 使用 TEST_SUITE_BEGIN / TEST_SUITE_END 包裹测试函数
 *   4. 使用 TEST_RUN 运行每个测试函数
 *   5. 使用 TEST_SUMMARY 打印汇总
 *
 * 要使用新的结构化框架，将 test_helpers.h 替换为 test_framework.h，
 * 并使用 lv_TEST / lv_ASSERT_* 宏代替。参见 test_new_modules.c 示例。
 */

#ifndef lv_TEST_HELPERS_H
#define lv_TEST_HELPERS_H

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "lv.h"
#include "test_framework.h"

/**
 * @brief 相等断言宏（带消息） - 比较两个值是否相等并显示自定义消息
 *
 * 与 TEST_ASSERT_EQ 类似，但支持自定义失败消息。
 *
 * @param actual    实际值
 * @param expected  期望值
 * @param msg       失败时显示的自定义消息
 */
#define TEST_ASSERT_EQ_MSG(actual, expected, msg)                                                        \
    do {                                                                                                 \
        intptr_t _th_actual = (intptr_t) (actual);                                                       \
        intptr_t _th_expected = (intptr_t) (expected);                                                   \
        if (_th_actual != _th_expected) {                                                                \
            fprintf(stderr, "  FAIL [%s:%d] %s (actual=%ld, expected=%ld)\n", __FILE__, __LINE__, (msg), \
                    (long) _th_actual, (long) _th_expected);                                             \
            g_fail_count++;                                                                              \
            return;                                                                                      \
        }                                                                                                \
        g_pass_count++;                                                                                  \
    } while (0)

/**
 * @brief 字符串相等断言宏 - 比较两个字符串是否相等
 *
 * 使用 strcmp 进行比较，失败时打印实际值和期望值字符串。
 * 支持 NULL 字符串安全比较。
 *
 * @param actual    实际字符串
 * @param expected  期望字符串
 */
#define TEST_ASSERT_STR_EQ(actual, expected)                                                                       \
    do {                                                                                                           \
        const char *_th_actual = (actual);                                                                         \
        const char *_th_expected = (expected);                                                                     \
        int _th_cmp = 0;                                                                                           \
        if (_th_actual == NULL && _th_expected == NULL) {                                                          \
            _th_cmp = 0;                                                                                           \
        } else if (_th_actual == NULL || _th_expected == NULL) {                                                   \
            _th_cmp = 1;                                                                                           \
        } else {                                                                                                   \
            _th_cmp = strcmp(_th_actual, _th_expected);                                                            \
        }                                                                                                          \
        if (_th_cmp != 0) {                                                                                        \
            fprintf(stderr, "  FAIL [%s:%d] %s != %s (actual='%s', expected='%s')\n", __FILE__, __LINE__, #actual, \
                    #expected, _th_actual ? _th_actual : "(null)", _th_expected ? _th_expected : "(null)");        \
            g_fail_count++;                                                                                        \
            return;                                                                                                \
        }                                                                                                          \
        g_pass_count++;                                                                                            \
    } while (0)

/* ============================================================
 * 全局测试计数器（各测试文件需定义）
 * ============================================================ */
extern int g_pass_count;
extern int g_fail_count;

/* ============================================================
 * 辅助函数
 * ============================================================ */

/**
 * @brief 在约束图中添加一个有理坐标点
 * @param g   约束图指针
 * @param xn  X坐标分子
 * @param xd  X坐标分母
 * @param yn  Y坐标分子
 * @param yd  Y坐标分母
 * @return    新添加节点的ID（g->next_node_id - 1）
 */
static inline int add_point(ConstraintGraph *g, int64_t xn, uint64_t xd, int64_t yn, uint64_t yd) {
    /* NULL检查：防止传入空图指针导致崩溃 */
    if (g == NULL) {
        fprintf(stderr, "  [ERROR] add_point: graph is NULL\n");
        return -1;
    }

    SymbolicCoord *cx = symbolic_coord_create_rational(xn, xd);
    SymbolicCoord *cy = symbolic_coord_create_rational(yn, yd);

    /* NULL检查：内存分配失败时清理已分配资源并返回 */
    if (cx == NULL || cy == NULL) {
        fprintf(stderr, "  [ERROR] add_point: failed to create symbolic coord\n");
        if (cx)
            symbolic_coord_destroy(cx);
        if (cy)
            symbolic_coord_destroy(cy);
        return -1;
    }

    SymbolicCoord *coords[] = {cx, cy};
    AddNodeResult result = graph_add_point(g, coords, 2);
    if (result != ADD_NODE_OK) {
        fprintf(stderr, "  [ERROR] add_point: graph_add_point failed (result=%d)\n", result);
        return -1;
    }
    return g->next_node_id - 1;
}

static inline int approx_eq(double a, double b) { return fabs(a - b) < 1e-10; }
static inline int approx_eq_eps(double a, double b, double eps) { return fabs(a - b) < (eps); }

/* ============================================================
 * 测试断言宏
 * ============================================================ */

/**
 * @brief 基础断言宏 - 带失败计数（向后兼容）
 *
 * 如果条件为假，打印失败信息（含文件名、行号、自定义消息），
 * 递增失败计数并立即从当前函数返回。
 * 如果条件为真，递增通过计数。
 *
 * @param cond  条件表达式
 * @param msg   失败消息字符串
 */
#define TEST_ASSERT(cond, msg)                                                 \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, "  FAIL [%s:%d] %s\n", __FILE__, __LINE__, (msg)); \
            g_fail_count++;                                                    \
            return;                                                            \
        }                                                                      \
        g_pass_count++;                                                        \
    } while (0)

/**
 * @brief 带消息的断言宏
 *
 * 与 TEST_ASSERT 功能相同，但参数顺序更直观（条件在前，消息在后）。
 * 适用于希望明确区分条件和描述消息的场景。
 *
 * @param cond  条件表达式
 * @param msg   失败时打印的描述消息
 */
#define TEST_ASSERT_MSG(cond, msg)                                             \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, "  FAIL [%s:%d] %s\n", __FILE__, __LINE__, (msg)); \
            g_fail_count++;                                                    \
            return;                                                            \
        }                                                                      \
        g_pass_count++;                                                        \
    } while (0)

/**
 * @brief 相等断言宏 - 比较两个值是否相等
 *
 * 支持整数类型和指针类型的比较。
 * 使用 == 运算符直接比较，失败时以整数形式打印实际值和期望值。
 *
 * @param actual    实际值
 * @param expected  期望值
 */
#define TEST_ASSERT_EQ(actual, expected)                                                                         \
    do {                                                                                                         \
        intptr_t _th_actual = (intptr_t) (actual);                                                               \
        intptr_t _th_expected = (intptr_t) (expected);                                                           \
        if (_th_actual != _th_expected) {                                                                        \
            fprintf(stderr, "  FAIL [%s:%d] %s != %s (actual=%ld, expected=%ld)\n", __FILE__, __LINE__, #actual, \
                    #expected, (long) _th_actual, (long) _th_expected);                                          \
            g_fail_count++;                                                                                      \
            return;                                                                                              \
        }                                                                                                        \
        g_pass_count++;                                                                                          \
    } while (0)

/**
 * @brief 不等断言宏 - 比较两个值是否不相等
 *
 * 如果两个值相等，打印失败信息并返回。
 *
 * @param actual    实际值
 * @param expected  不应等于的值
 */
#define TEST_ASSERT_NE(actual, expected)                                                                    \
    do {                                                                                                    \
        intptr_t _th_actual = (intptr_t) (actual);                                                          \
        intptr_t _th_expected = (intptr_t) (expected);                                                      \
        if (_th_actual == _th_expected) {                                                                   \
            fprintf(stderr, "  FAIL [%s:%d] %s == %s (both=%ld)\n", __FILE__, __LINE__, #actual, #expected, \
                    (long) _th_actual);                                                                     \
            g_fail_count++;                                                                                 \
            return;                                                                                         \
        }                                                                                                   \
        g_pass_count++;                                                                                     \
    } while (0)

#define TEST_ASSERT_NEAR(actual, expected, tol, msg)                                                              \
    do {                                                                                                          \
        double _ta_actual = (double) (actual);                                                                    \
        double _ta_expected = (double) (expected);                                                                \
        double _ta_diff = _ta_actual - _ta_expected;                                                              \
        if (_ta_diff < 0.0)                                                                                       \
            _ta_diff = -_ta_diff;                                                                                 \
        if (_ta_diff > (tol)) {                                                                                   \
            fprintf(stderr, "  FAIL [%s:%d] %s (actual=%.12f, expected=%.12f, diff=%.12e)\n", __FILE__, __LINE__, \
                    (msg), _ta_actual, _ta_expected, _ta_diff);                                                   \
            g_fail_count++;                                                                                       \
            return;                                                                                               \
        }                                                                                                         \
        g_pass_count++;                                                                                           \
    } while (0)

/**
 * @brief 浮点近似相等断言宏（无消息参数的简版）
 *
 * 与 TEST_ASSERT_NEAR 功能相同，但省略消息参数，用于收敛测试中
 * 大量手写的 fabs(a - b) < 1e-N 判断。失败时打印实际值、期望值和差值。
 *
 * @param actual    实际值
 * @param expected  期望值
 * @param tol       容差
 */
#define TEST_ASSERT_DOUBLE(actual, expected, tol)                                                               \
    do {                                                                                                        \
        double _td_actual = (double) (actual);                                                                  \
        double _td_expected = (double) (expected);                                                              \
        double _td_diff = _td_actual - _td_expected;                                                            \
        if (_td_diff < 0.0)                                                                                     \
            _td_diff = -_td_diff;                                                                               \
        if (_td_diff > (tol)) {                                                                                 \
            fprintf(stderr, "  FAIL [%s:%d] (actual=%.12f, expected=%.12f, diff=%.12e)\n", __FILE__, __LINE__,  \
                    _td_actual, _td_expected, _td_diff);                                                        \
            g_fail_count++;                                                                                     \
            return;                                                                                             \
        }                                                                                                       \
        g_pass_count++;                                                                                         \
    } while (0)

/**
 * @brief 失败不返回的断言宏（失败后继续执行）
 *
 * 与 TEST_ASSERT 不同：断言失败时仅打印失败信息并递增失败计数，
 * 不立即从当前函数返回。适用于"单个测试函数内连续多条断言、失败后
 * 仍希望继续执行后续断言"的场景（部分旧测试文件的既有语义）。
 *
 * @param cond  条件表达式
 * @param msg   失败消息字符串
 */
#define TEST_ASSERT_CONTINUE(cond, msg)                                                      \
    do {                                                                                     \
        if (!(cond)) {                                                                       \
            fprintf(stderr, "  FAIL [%s:%d] %s\n", __FILE__, __LINE__, (msg));               \
            g_fail_count++;                                                                  \
        } else {                                                                             \
            g_pass_count++;                                                                  \
        }                                                                                    \
    } while (0)

/**
 * @brief 相等断言宏（失败不返回，无消息参数）
 *
 * 与 TEST_ASSERT_EQ 功能相同，但断言失败时不 return，继续执行后续代码。
 *
 * @param actual    实际值
 * @param expected  期望值
 */
#define TEST_ASSERT_EQ_CONTINUE(actual, expected)                                          \
    do {                                                                                   \
        intptr_t _th_actual = (intptr_t) (actual);                                         \
        intptr_t _th_expected = (intptr_t) (expected);                                     \
        if (_th_actual != _th_expected) {                                                  \
            fprintf(stderr, "  FAIL [%s:%d] %s != %s (actual=%ld, expected=%ld)\n",        \
                    __FILE__, __LINE__, #actual, #expected, (long) _th_actual,             \
                    (long) _th_expected);                                                  \
            g_fail_count++;                                                                \
        } else {                                                                           \
            g_pass_count++;                                                                \
        }                                                                                  \
    } while (0)

/**
 * @brief 空指针断言宏 - 检查指针是否为 NULL
 *
 * 如果指针不为 NULL，打印失败信息并返回。
 *
 * @param ptr  待检查的指针
 */
#define TEST_ASSERT_NULL(ptr)                                                                     \
    do {                                                                                          \
        if ((ptr) != NULL) {                                                                      \
            fprintf(stderr, "  FAIL [%s:%d] %s is not NULL (got %p)\n", __FILE__, __LINE__, #ptr, \
                    (const void *) (ptr));                                                        \
            g_fail_count++;                                                                       \
            return;                                                                               \
        }                                                                                         \
        g_pass_count++;                                                                           \
    } while (0)

/**
 * @brief 非空指针断言宏 - 检查指针是否不为 NULL
 *
 * 如果指针为 NULL，打印失败信息并返回。
 *
 * @param ptr  待检查的指针
 */
#define TEST_ASSERT_NOT_NULL(ptr)                                                     \
    do {                                                                              \
        if ((ptr) == NULL) {                                                          \
            fprintf(stderr, "  FAIL [%s:%d] %s is NULL\n", __FILE__, __LINE__, #ptr); \
            g_fail_count++;                                                           \
            return;                                                                   \
        }                                                                             \
        g_pass_count++;                                                               \
    } while (0)

/* ============================================================
 * 测试运行与报告宏
 * ============================================================ */

/**
 * @brief 运行单个测试函数并记录结果
 *
 * 调用指定的测试函数，捕获异常（如果适用），
 * 并打印测试名称和结果（PASS/FAIL）。
 *
 * @param test_func  测试函数名（无参数、返回 void 的函数）
 */
#define TEST_RUN(test_func)                                \
    do {                                                   \
        fprintf(stderr, "  Running: %s ... ", #test_func); \
        int _prev_fail = g_fail_count;                     \
        test_func();                                       \
        if (g_fail_count == _prev_fail) {                  \
            fprintf(stderr, "PASS\n");                     \
        }                                                  \
    } while (0)

/**
 * @brief 打印测试摘要
 *
 * 输出通过/失败的测试数量，以及总数。
 * 建议在所有测试运行完毕后调用。
 */
#define TEST_SUMMARY()                                                                                  \
    do {                                                                                                \
        fprintf(stderr, "\n========================================\n");                                \
        fprintf(stderr, "  Test Summary: %d passed, %d failed, %d total\n", g_pass_count, g_fail_count, \
                g_pass_count + g_fail_count);                                                           \
        fprintf(stderr, "========================================\n");                                  \
    } while (0)

/**
 * @brief 测试套件开始标记
 *
 * 打印测试套件名称，用于在输出中标识一组相关测试。
 * 建议在 main() 函数开头调用。
 *
 * @param name  测试套件名称字符串
 */
#define TEST_SUITE_BEGIN(name)                                       \
    do {                                                             \
        fprintf(stderr, "\n====== Test Suite: %s ======\n", (name)); \
    } while (0)

/**
 * @brief 测试套件结束标记
 *
 * 打印测试套件结束信息，并调用 TEST_SUMMARY() 输出汇总。
 * 建议在 main() 函数末尾调用。
 */
#define TEST_SUITE_END()                                        \
    do {                                                        \
        fprintf(stderr, "====== Test Suite Complete ======\n"); \
        TEST_SUMMARY();                                         \
    } while (0)

/**
 * @brief 标准 main() 骨架：开始部分
 *
 * 与 TEST_MAIN_RUN / TEST_MAIN_END 配合，将大量测试文件中重复的
 * main() 骨架（TEST_SUITE_BEGIN + TEST_RUN 列表 + 汇总 + 退出码）
 * 收敛为三行调用，保持原有输出格式与退出码行为一致：
 *
 *     TEST_MAIN_BEGIN("suite name")
 *         TEST_MAIN_RUN(test_func1);
 *         TEST_MAIN_RUN(test_func2);
 *     TEST_MAIN_END()
 *
 * 注意：文件级计数器 int g_pass_count / int g_fail_count 仍需在
 * 测试文件顶部自行定义（与现有测试文件保持一致）。
 *
 * @param suite_name  测试套件名称字符串
 */
#define TEST_MAIN_BEGIN(suite_name)   \
    int main(void) {                  \
        TEST_SUITE_BEGIN(suite_name); \
        {

/**
 * @brief 标准 main() 骨架：运行单个测试函数
 *
 * TEST_RUN 的语义化别名，用法为 TEST_MAIN_RUN(test_func);
 *
 * @param test_func  测试函数名（无参数、返回 void 的函数）
 */
#define TEST_MAIN_RUN(test_func) TEST_RUN(test_func)

/**
 * @brief 标准 main() 骨架：结束部分
 *
 * 输出测试套件完成标记与测试汇总，并以失败计数决定退出码
 * （等价于 TEST_SUITE_END(); return g_fail_count > 0 ? 1 : 0;）。
 */
#define TEST_MAIN_END()                  \
        }                                \
        TEST_SUITE_END();                \
        return g_fail_count > 0 ? 1 : 0; \
    }

/* ============================================================
 * 传统 TEST/PASS/FAIL 输出宏（旧式测试文件兼容层）
 * ============================================================
 * 早期测试文件各自定义了私有的 TEST/PASS/FAIL 宏（打印
 * "  [TEST] <名称> ... "、"PASS\n" / "FAIL: <消息>"，部分文件
 * 同时递增私有计数器）。这里提供统一的兼容实现，替换各文件
 * 中的私有定义，保持原有输出与计数行为：
 *
 *   - TEST(n)        打印 "  [TEST] %s ... "（n 为测试名称）
 *   - PASS()         打印 "PASS\n"，随后执行 TEST_PASS_STATEMENT
 *   - FAIL(m)        打印 "FAIL: %s\n"，随后执行 TEST_FAIL_STATEMENT
 *
 * 需要计数递增的旧文件，在包含本头文件之前定义挂钩，例如：
 *   #define TEST_PASS_STATEMENT P++
 *   #define TEST_FAIL_STATEMENT F++
 * 纯输出风格（计数器在调用处自行递增）的文件无需定义任何挂钩。
 *
 * 若某文件在包含本头文件之前已自定义同名宏，则保留其自定义版本
 * （#ifndef 保护）。
 */
#ifndef TEST
#define TEST(n) printf("  [TEST] %s ... ", n)
#endif

#ifndef TEST_PASS_STATEMENT
#define TEST_PASS_STATEMENT ((void) 0)
#endif

#ifndef TEST_FAIL_STATEMENT
#define TEST_FAIL_STATEMENT ((void) 0)
#endif

#ifndef PASS
#define PASS()               \
    do {                     \
        printf("PASS\n");    \
        TEST_PASS_STATEMENT; \
    } while (0)
#endif

#ifndef FAIL
#define FAIL(m)                  \
    do {                         \
        printf("FAIL: %s\n", m); \
        TEST_FAIL_STATEMENT;     \
    } while (0)
#endif

#endif /* lv_TEST_HELPERS_H */
