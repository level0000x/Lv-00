/**
 * @file test_gappa_dsl_ext.c
 * @brief Gappa DSL 契约测试（批次 C-㊺续36：gappa_dsl.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（3 个）：
 *   lv_gappa_parse / lv_gappa_eval / lv_gappa_prove
 *
 * 契约要点（与 gappa_dsl.c 核对）：
 *   - parse：NULL → -1；语法合法 → 0；语法错误 → -1。
 *   - eval：NULL 参数 → -1；委托 lv_gappa_propagate（区间传播）。
 *   - prove：NULL → NULL；parse 失败返回 "proof result: parse error"；
 *     成功返回证明摘要串（调用者 lv_free）。
 *
 * @author Lv-00 Project
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "lv/gappa_dsl.h"
#include "lv/lv_utils.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：解析 ============== */

static void test_parse(void) {
    TEST_ASSERT_EQ(lv_gappa_parse(NULL), -1);

    /* 合法输入（简单谓词） */
    TEST_ASSERT_EQ(lv_gappa_parse("x in [1, 2]"), 0);
}

/* ============== 测试：求值 ============== */

static void test_eval(void) {
    double lo = 0.0, hi = 0.0;

    /* 委托 propagate：x + 1 → [0, 2]（默认变量 [-1,1]） */
    TEST_ASSERT_EQ(lv_gappa_eval("x + 1", &lo, &hi), 0);
    TEST_ASSERT_DOUBLE(lo, 0.0, 1e-9);
    TEST_ASSERT_DOUBLE(hi, 2.0, 1e-9);

    /* NULL 契约 */
    TEST_ASSERT_EQ(lv_gappa_eval(NULL, &lo, &hi), -1);
    TEST_ASSERT_EQ(lv_gappa_eval("x + 1", NULL, &hi), -1);
    TEST_ASSERT_EQ(lv_gappa_eval("x + 1", &lo, NULL), -1);
}

/* ============== 测试：证明 ============== */

static void test_prove(void) {
    /* NULL 契约 */
    TEST_ASSERT_NULL(lv_gappa_prove(NULL));

    /* 成功路径：返回证明摘要（非 NULL） */
    char *r = lv_gappa_prove("x in [0, 1]; x^2 <= 1;");
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT(r[0] != '\0', "result non-empty");
    lv_free((void **) &r);
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("GappaDslExt")

    printf("\n--- gappa_dsl (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_parse);
    TEST_MAIN_RUN(test_eval);
    TEST_MAIN_RUN(test_prove);

TEST_MAIN_END()
