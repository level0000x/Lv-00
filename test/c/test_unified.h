/**
 * @file test_unified.h
 * @brief 统一测试框架入口 —— 同时支持旧宏风格和结构化风格
 *
 * 包含此头文件即可使用两种风格的测试 API，无需再单独包含 lv.h / test_framework.h / test_helpers.h。
 *
 * ── 使用方法 ──
 *
 *   #include "test_unified.h"   // 唯一需要的头文件
 *
 *   int g_pass_count = 0;
 *   int g_fail_count = 0;
 *
 *   // 旧风格（TEST_ASSERT / TEST_RUN / TEST_SUMMARY）
 *   void test_foo(void) {
 *       TEST_ASSERT(1 + 1 == 2, "basic math");
 *       TEST_RUN(test_foo);
 *   }
 *
 *   // 新结构化风格（lv_TEST / lv_ASSERT_*）
 *   lv_TEST(Suite, Bar) {
 *       lv_ASSERT_EQ(2, 1 + 1);
 *   }
 *
 * ── API 迁移对照表 ──
 *
 *   旧风格（test_helpers.h）         新风格（test_framework.h）      说明
 *   ───────────────────────────────  ───────────────────────────  ──────────────────────────────
 *   TEST_ASSERT(cond, msg)           lv_ASSERT(cond)              新风格无自定义消息参数
 *   TEST_ASSERT_MSG(cond, msg)       lv_ASSERT_TRUE(cond)         等价用法 lv_ASSERT(cond)
 *   TEST_ASSERT_EQ(a, e)             lv_ASSERT_EQ(a, e)           功能等价
 *   TEST_ASSERT_NE(a, e)             lv_ASSERT_FALSE(a == e)      无直接等价宏，用取反组合
 *   TEST_ASSERT_NULL(ptr)            lv_ASSERT(ptr == NULL)       无专用宏
 *   TEST_ASSERT_NOT_NULL(ptr)        lv_ASSERT_NOT_NULL(ptr)      功能等价
 *   TEST_ASSERT_STR_EQ(a, e)         (无等价宏)                   旧风格专有，可通过 strcmp + lv_ASSERT 模拟
 *   TEST_RUN(func)                   lv_TEST(Suite, Name)         旧风格手动调用；新风格注册到框架
 *   TEST_SUMMARY()                   lv_test_report_print()       旧风格打印计数器；新风格输出结构化报告
 *   TEST_SUITE_BEGIN(name)           lv_test_run_all()            旧风格标记开始；新风格直接运行
 *   TEST_SUITE_END()                 lv_test_report_destroy()     旧风格标记结束；新风格清理报告
 *   add_point(g, xn, xd, yn, yd)     (无等价项)                   辅助函数，仍可用
 *
 *   推荐策略：
 *   - 新测试 → 直接用 lv_TEST / lv_ASSERT_* 结构化风格
 *   - 现有旧测试 → 保持旧宏不变，无需立即重写
 *   - 当需要重写旧测试时，参考上表逐条替换
 */

#ifndef lv_TEST_UNIFIED_H
#define lv_TEST_UNIFIED_H

/* ── 项目基础头文件 ── */
#include "lv.h"

/* ── 结构化测试框架（lvTestSuite / lvTestCase / lv_ASSERT_* / lv_TEST） ── */
#include "test_framework.h"

/* ── 旧风格兼容层（TEST_ASSERT / TEST_RUN / TEST_SUMMARY / add_point 等） ── */
#include "test_helpers.h"

#endif /* lv_TEST_UNIFIED_H */
