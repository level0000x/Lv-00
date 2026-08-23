/**
 * @file test_equiv_class_ext.c
 * @brief 等价类 Legacy 别名契约测试（批次 C-㊺续31：equiv_class.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（3 个 + 1 个补全）：
 *   lv_equiv_class_create / union / find / destroy（本批补齐实现并补 destroy 声明）
 *
 * 契约要点（与 equiv_class.c 本批新增实现核对）：
 *   - create(n)：n 个元素（0..n-1）的并查集；n == 0 或 n > INT_MAX 返回 NULL。
 *   - find(a)：返回代表元（路径压缩）；越界/无效返回 -1。
 *   - union(a,b)：已同一集合或 a == b 返回 0；合并成功返回 1；无效返回 -1。
 *   - destroy：NULL 安全；释放并查集数组与结构。
 *
 * 修复记录：这三个 Legacy 别名此前声明无实现（全库仅头文件声明，调用即链接
 * 错误），本批基于共享并查集工具补齐，并补配对销毁入口。
 *
 * @author Lv-00 Project
 */

#include <limits.h>
#include <stdint.h>
#include <stdio.h>

#include "lv/equiv_class.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：create / destroy ============== */

static void test_create_destroy(void) {
    /* 正常创建 */
    lvEquivClass *ec = lv_equiv_class_create(8);
    TEST_ASSERT_NOT_NULL(ec);

    /* 空并查集：n == 0 拒绝 */
    lvEquivClass *empty = lv_equiv_class_create(0);
    TEST_ASSERT_NULL(empty);

    /* 超 INT_MAX 拒绝（不分配） */
    lvEquivClass *huge = lv_equiv_class_create((size_t) INT_MAX + (size_t) 1);
    TEST_ASSERT_NULL(huge);

    lv_equiv_class_destroy(ec);
    lv_equiv_class_destroy(NULL);
}

/* ============== 测试：find ============== */

static void test_find(void) {
    lvEquivClass *ec = lv_equiv_class_create(8);
    TEST_ASSERT_NOT_NULL(ec);

    /* 初始自代表 */
    TEST_ASSERT_EQ(lv_equiv_class_find(ec, 0), 0);
    TEST_ASSERT_EQ(lv_equiv_class_find(ec, 7), 7);

    /* 越界 / 无效 */
    TEST_ASSERT_EQ(lv_equiv_class_find(ec, 8), -1);
    TEST_ASSERT_EQ(lv_equiv_class_find(ec, -1), -1);
    TEST_ASSERT_EQ(lv_equiv_class_find(NULL, 0), -1);

    lv_equiv_class_destroy(ec);
}

/* ============== 测试：union ============== */

static void test_union(void) {
    lvEquivClass *ec = lv_equiv_class_create(8);
    TEST_ASSERT_NOT_NULL(ec);

    /* 合并 0 与 1：返回 1，find 一致 */
    TEST_ASSERT_EQ(lv_equiv_class_union(ec, 0, 1), 1);
    TEST_ASSERT(lv_equiv_class_find(ec, 0) == lv_equiv_class_find(ec, 1), "0 and 1 same class");

    /* 已同一集合：返回 0 */
    TEST_ASSERT_EQ(lv_equiv_class_union(ec, 0, 1), 0);

    /* a == b：返回 0 */
    TEST_ASSERT_EQ(lv_equiv_class_union(ec, 3, 3), 0);

    /* 传递合并：union(1,2) 后 0、1、2 同集合 */
    TEST_ASSERT_EQ(lv_equiv_class_union(ec, 1, 2), 1);
    TEST_ASSERT(lv_equiv_class_find(ec, 0) == lv_equiv_class_find(ec, 2), "transitive 0 == 2");

    /* 不同集合保持独立 */
    TEST_ASSERT(lv_equiv_class_find(ec, 0) != lv_equiv_class_find(ec, 4), "4 independent");

    /* 无效参数：-1 */
    TEST_ASSERT_EQ(lv_equiv_class_union(ec, 0, 99), -1);
    TEST_ASSERT_EQ(lv_equiv_class_union(ec, -1, 0), -1);
    TEST_ASSERT_EQ(lv_equiv_class_union(NULL, 0, 1), -1);

    lv_equiv_class_destroy(ec);
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("EquivClassExt")

    printf("\n--- equiv_class legacy aliases (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_create_destroy);
    TEST_MAIN_RUN(test_find);
    TEST_MAIN_RUN(test_union);

TEST_MAIN_END()
