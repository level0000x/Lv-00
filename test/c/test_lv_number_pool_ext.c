/**
 * @file test_lv_number_pool_ext.c
 * @brief 数值抽象层 0b/0c 契约测试：强制池化（帧池/常驻池）+ 跨类型精确提升 + 保真
 *
 * 契约要点（design: number-abstraction-layer-design.md ND-1/ND-2/0b）：
 *   - 帧池：frame_begin/end 之间创建的对象随 end 整体回收（含 mpq 内部存储）；
 *     帧对象 destroy 为无害空操作；end 后可继续创建常驻对象。
 *   - 常驻池：destroy 归还 free-list，可大量创建/销毁循环复用（无泄漏）。
 *   - 跨类型精确提升：int 与 rational 的 ±/×/÷ → rational（mpq 精确，
 *     不再降级 double）；rational 表示 from_string("3/4") → "3/4" 规范形保真。
 *   - int ÷ int 保持整数截断（旧契约）；任一 float 参与 → float 语义。
 *   - clone 独立深拷贝；hash 同值同哈希；is_integer(rational 分母 1) = true。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/lv_number.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 帧池生命周期 ============== */

static void test_frame_pool_lifecycle(void) {
    lvNumberFrame *f = lv_number_frame_begin();
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT(lv_number_in_frame(), "帧内标记");

    /* 帧内批量创建（含 rational，mpq 内部存储随帧回收） */
    for (int i = 0; i < 2000; i++) {
        lvNumber *r = lv_number_from_rational(i + 1, 3);
        TEST_ASSERT_NOT_NULL(r);
        TEST_ASSERT_EQ((int) lv_number_type(r), (int) lv_NUMBER_RATIONAL);
        lvNumber *s = lv_number_add(r, lv_number_from_int(1));
        TEST_ASSERT_NOT_NULL(s);
        lv_number_destroy(s); /* 帧对象 destroy 无害空操作 */
    }

    /* 嵌套帧 */
    lvNumberFrame *inner = lv_number_frame_begin();
    TEST_ASSERT_NOT_NULL(inner);
    lvNumber *x = lv_number_from_int(42);
    TEST_ASSERT_NOT_NULL(x);
    lv_number_frame_end(NULL); /* 结束最内层 */
    TEST_ASSERT(lv_number_in_frame(), "外层帧仍活跃");

    lv_number_frame_end(f); /* 回收全部帧对象 */
    TEST_ASSERT(!lv_number_in_frame(), "帧已全部关闭");

    /* 帧关闭后常驻对象照常工作 */
    lvNumber *a = lv_number_from_rational(1, 2);
    TEST_ASSERT_NOT_NULL(a);
    lv_number_destroy(a);
    printf("  test_frame_pool_lifecycle: PASSED\n");
}

static void test_resident_pool_reuse(void) {
    /* 大量创建/销毁循环：验证常驻池 free-list 复用（无泄漏/无崩溃） */
    for (int i = 0; i < 20000; i++) {
        lvNumber *r = lv_number_from_rational(i, 7);
        TEST_ASSERT_NOT_NULL(r);
        lvNumber *d = lv_number_from_double(0.5);
        TEST_ASSERT_NOT_NULL(d);
        lvNumber *s = lv_number_add(r, d);
        TEST_ASSERT_NOT_NULL(s);
        lv_number_destroy(s);
        lv_number_destroy(d);
        lv_number_destroy(r);
    }
    lv_number_destroy(NULL); /* NULL 安全 */
    printf("  test_resident_pool_reuse: PASSED\n");
}

/* ============== 跨类型精确提升 ============== */

static void test_cross_type_exact(void) {
    /* int + rational → rational（精确，非 double 降级） */
    lvNumber *i = lv_number_from_int(1);
    lvNumber *r = lv_number_from_rational(1, 2);
    lvNumber *s = lv_number_add(i, r);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQ((int) lv_number_type(s), (int) lv_NUMBER_RATIONAL);
    char *str = lv_number_to_string(s);
    TEST_ASSERT_NOT_NULL(str);
    TEST_ASSERT(strcmp(str, "3/2") == 0, "1 + 1/2 = 3/2 精确");
    lv_free((void **) &str);
    lv_number_destroy(s);

    /* int * rational → rational */
    lvNumber *m = lv_number_mul(lv_number_from_int(2), lv_number_from_rational(3, 4));
    TEST_ASSERT_NOT_NULL(m);
    str = lv_number_to_string(m);
    TEST_ASSERT_NOT_NULL(str);
    TEST_ASSERT(strcmp(str, "3/2") == 0, "2 * 3/4 = 3/2");
    lv_free((void **) &str);
    lv_number_destroy(m);

    /* int ÷ rational → rational（精确倒数） */
    lvNumber *q = lv_number_div(lv_number_from_int(1), lv_number_from_rational(1, 3));
    TEST_ASSERT_NOT_NULL(q);
    str = lv_number_to_string(q);
    TEST_ASSERT_NOT_NULL(str);
    TEST_ASSERT(strcmp(str, "3") == 0, "1 / (1/3) = 3");
    lv_free((void **) &str);
    lv_number_destroy(q);

    /* rational ÷ int → rational */
    lvNumber *q2 = lv_number_div(lv_number_from_rational(3, 2), lv_number_from_int(2));
    TEST_ASSERT_NOT_NULL(q2);
    TEST_ASSERT_DOUBLE(lv_number_to_double(q2), 0.75, 1e-12);
    lv_number_destroy(q2);

    /* 精确比较：int 2 == rational 6/3；rational 3/2 < int 2 */
    lvNumber *two = lv_number_from_int(2);
    lvNumber *six3 = lv_number_from_rational(6, 3);
    TEST_ASSERT(lv_number_eq(two, six3), "2 == 6/3");
    TEST_ASSERT_EQ(lv_number_compare(two, six3), 0);
    lvNumber *h = lv_number_from_rational(3, 2);
    TEST_ASSERT(lv_number_lt(h, two), "3/2 < 2");
    lv_number_destroy(h);
    lv_number_destroy(six3);

    /* int ÷ int：整数截断（旧契约钉住） */
    lvNumber *tr = lv_number_div(lv_number_from_int(6), lv_number_from_int(4));
    TEST_ASSERT_NOT_NULL(tr);
    TEST_ASSERT_EQ((int) lv_number_type(tr), (int) lv_NUMBER_INTEGER);
    TEST_ASSERT_DOUBLE(lv_number_to_double(tr), 1, 1e-12);
    lv_number_destroy(tr);

    /* 任一 float → float 语义 */
    lvNumber *fz = lv_number_add(lv_number_from_int(1), lv_number_from_double(0.5));
    TEST_ASSERT_NOT_NULL(fz);
    TEST_ASSERT_EQ((int) lv_number_type(fz), (int) lv_NUMBER_FLOAT);
    TEST_ASSERT_DOUBLE(lv_number_to_double(fz), 1.5, 1e-12);
    lv_number_destroy(fz);

    lv_number_destroy(two);
    lv_number_destroy(r);
    lv_number_destroy(i);
    printf("  test_cross_type_exact: PASSED\n");
}

/* ============== 保真与杂项 ============== */

static void test_fidelity_misc(void) {
    /* from_string("7") → RATIONAL（规范形 "7"，分母 1 省略） */
    lvNumber *s = lv_number_from_string("7");
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQ((int) lv_number_type(s), (int) lv_NUMBER_RATIONAL);
    TEST_ASSERT(lv_number_is_integer(s), "7/1 为整值");
    char *str = lv_number_to_string(s);
    TEST_ASSERT_NOT_NULL(str);
    TEST_ASSERT(strcmp(str, "7") == 0, "有理数整值规范形省略分母");
    lv_free((void **) &str);
    lv_number_destroy(s);

    /* from_rational(3,2) 规范形 "3/2" */
    lvNumber *r = lv_number_from_rational(3, 2);
    str = lv_number_to_string(r);
    TEST_ASSERT_NOT_NULL(str);
    TEST_ASSERT(strcmp(str, "3/2") == 0, "3/2 规范形");
    lv_free((void **) &str);

    /* clone 独立 + hash 同值同哈希（跨帧也稳定：值哈希） */
    lvNumber *c = lv_number_clone(r);
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT(c != (lvNumber *) r, "克隆独立句柄");
    TEST_ASSERT_EQ(lv_number_hash(r), lv_number_hash(c));
    lv_number_destroy(c);
    lv_number_destroy(r);

    /* 帧内 hash 与常驻同值同哈希 */
    lvNumberFrame *f = lv_number_frame_begin();
    TEST_ASSERT_NOT_NULL(f);
    lvNumber *fr = lv_number_from_rational(6, 3);
    lvNumber *fi = lv_number_from_int(2);
    TEST_ASSERT_NOT_NULL(fr);
    TEST_ASSERT_NOT_NULL(fi);
    TEST_ASSERT_EQ(lv_number_hash(fr), lv_number_hash(fi)); /* 6/3 与 2 同值 */
    TEST_ASSERT(lv_number_eq(fr, fi), "帧内 6/3 == 2");
    lv_number_frame_end(f);

    /* div by zero：int÷0 → NULL；rational÷0 → NULL */
    TEST_ASSERT_NULL(lv_number_div(lv_number_from_int(5), lv_number_from_int(0)));
    TEST_ASSERT_NULL(lv_number_div(lv_number_from_rational(1, 2), lv_number_from_int(0)));

    printf("  test_fidelity_misc: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Number Pool Ext Test Suite")
    printf("=== Lv-00 Number Pool Ext Test Suite (0b/0c: pooling + exact promotion) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_frame_pool_lifecycle);
    TEST_MAIN_RUN(test_resident_pool_reuse);
    TEST_MAIN_RUN(test_cross_type_exact);
    TEST_MAIN_RUN(test_fidelity_misc);

    lv_cleanup();
TEST_MAIN_END()
