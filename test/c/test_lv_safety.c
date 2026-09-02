/**
 * @file test_lv_safety.c
 * @brief 蓝图安全宏与泄漏检测契约测试（TEN_LAYER_OPTIMIZED_PLAN §16.2/16.3，批次 G5）
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/lv_safety.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 数值安全宏 ============== */

static void test_numeric_safety(void) {
    /* validate_triangle */
    TEST_ASSERT(lv_validate_triangle(3, 4, 5), "3-4-5 有效");
    TEST_ASSERT(!lv_validate_triangle(1, 1, 3), "退化无效");
    TEST_ASSERT(!lv_validate_triangle(0, 4, 5), "零边无效");
    TEST_ASSERT(!lv_validate_triangle(-1, 4, 5), "负边无效");
    TEST_ASSERT(!lv_validate_triangle(1, 2, 3.000000000001), "近退化容差");

    /* SAFE_DIV */
    double r = lv_SAFE_DIV(10.0, 2.0, -1.0);
    TEST_ASSERT(r == 5.0, "正常除法");
    r = lv_SAFE_DIV(10.0, 0.0, -1.0);
    TEST_ASSERT(r == -1.0, "除零返回默认");
    r = lv_SAFE_DIV(10.0, 1e-13, -1.0);
    TEST_ASSERT(r == -1.0, "近零返回默认");
}

/* 独立验证 CHECK_COORD 触发（含 return 宏只能在函数内） */
static int check_coord_trigger_nan(void) {
    double v = 0.0 / 0.0; /* NaN */
    lv_CHECK_COORD(v, -7);
    return 0;
}
static int check_coord_trigger_inf(void) {
    double v = 1e308 * 10.0; /* Inf */
    lv_CHECK_COORD(v, -8);
    return 0;
}
static int check_coord_trigger_big(void) {
    double v = 1e16; /* > lv_COORD_MAX 1e15 */
    lv_CHECK_COORD(v, -9);
    return 0;
}
static int check_coord_pass(void) {
    double v = 123.0;
    lv_CHECK_COORD(v, -1);
    return 0;
}

static void test_check_coord(void) {
    TEST_ASSERT_EQ(check_coord_trigger_nan(), -7);
    TEST_ASSERT_EQ(check_coord_trigger_inf(), -8);
    TEST_ASSERT_EQ(check_coord_trigger_big(), -9);
    TEST_ASSERT_EQ(check_coord_pass(), 0);
}

/* ============== 深度守卫 ============== */

/* 独立验证 depth 进入/离开（DEPTH_ENTER 含 return，须在 int 函数内） */
static int depth_enter_leave_ok(void) {
    lvDepthGuard guard = {0, 0, 0};
    lv_DEPTH_ENTER(&guard, propagation, 4, false);
    lv_DEPTH_ENTER(&guard, propagation, 4, false);
    if (guard.propagation_depth != 2)
        return -1;
    if (guard.total_steps != 2)
        return -1;
    lv_DEPTH_LEAVE(&guard, propagation);
    return (guard.propagation_depth == 1) ? 0 : -1;
}

static void test_depth_guard(void) {
    TEST_ASSERT_EQ(depth_enter_leave_ok(), 0);
}

/* 独立验证超限 return */
static int depth_trigger(void) {
    lvDepthGuard guard = {0, 0, 0};
    for (int i = 0; i < 6; i++) {
        lv_DEPTH_ENTER(&guard, proof_search, 3, -1); /* 超过 3 层返回 -1 */
    }
    return 0;
}

static void test_depth_limit(void) {
    TEST_ASSERT_EQ(depth_trigger(), -1);
}

/* ============== 安全字符串 ============== */

static void test_safe_string(void) {
    char buf[8];
    memset(buf, 'X', sizeof(buf));
    lv_STRCPY(buf, sizeof(buf), "hello");
    TEST_ASSERT(strcmp(buf, "hello") == 0, "strcpy 正常");
    lv_STRCPY(buf, sizeof(buf), "a very long string that overflows");
    TEST_ASSERT(strlen(buf) == 7, "截断到 size-1");
    TEST_ASSERT(buf[7] == '\0', "NUL 终止");

    lv_STRCPY(buf, sizeof(buf), "ab");
    lv_STRCAT(buf, sizeof(buf), "cdefg");
    TEST_ASSERT(strcmp(buf, "abcdefg") == 0, "strcat 正常");
    lv_STRCPY(buf, sizeof(buf), "abc");
    lv_STRCAT(buf, sizeof(buf), "defghijklmnop");
    TEST_ASSERT(strlen(buf) == 7, "strcat 不越界");
}

/* ============== 引用计数 ============== */

typedef struct {
    lv_REFCOUNT_HEADER;
    int value;
    int destructor_called;
} RefObj;

static void ref_destructor(void *self) {
    RefObj *o = (RefObj *) self;
    o->destructor_called = 1;
}

static void test_refcount(void) {
    RefObj obj;
    memset(&obj, 0, sizeof(obj));
    lv_REFCOUNT_INIT(&obj, ref_destructor);
    TEST_ASSERT(obj._ref_count == 1, "初始 1");
    lv_REFCOUNT_RETAIN(&obj);
    TEST_ASSERT(obj._ref_count == 2, "retain 后 2");
    lv_REFCOUNT_RELEASE(&obj);
    TEST_ASSERT(obj._ref_count == 1, "release 后 1");
    TEST_ASSERT(obj.destructor_called == 0, "未归零不析构");
    lv_REFCOUNT_RELEASE(&obj);
    TEST_ASSERT(obj.destructor_called == 1, "归零析构");
}

/* ============== 泄漏检测 ============== */

static void test_leak_detector(void) {
    /* 快照：分配后 active 增加、释放后减少（链表跟踪） */
    lvLeakSnapshot before = lv_leak_detector_snapshot();
    uint64_t base_count = before.active_count;
    uint64_t base_bytes = before.active_bytes;

    void *p = lv_malloc(256);
    TEST_ASSERT_NOT_NULL(p);
    lvLeakSnapshot after_alloc = lv_leak_detector_snapshot();
    TEST_ASSERT(after_alloc.active_count == base_count + 1, "分配后块数 +1");
    TEST_ASSERT(after_alloc.active_bytes >= base_bytes + 256, "分配后字节增加");
    TEST_ASSERT(after_alloc.active_count > 0, "有明细");

    lv_free(&p);
    lvLeakSnapshot after_free = lv_leak_detector_snapshot();
    TEST_ASSERT(after_free.active_count == base_count, "释放后块数复原");

    /* assert_clean：报告函数可调用不崩溃 */
    lv_leak_detector_report(NULL);
    (void) lv_leak_detector_assert_clean();
}

/* ============== 入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Safety (G5) Test Suite")
    printf("=== Lv-00 Safety (G5) Test Suite ===\n\n");
    lv_init();
    TEST_MAIN_RUN(test_numeric_safety);
    TEST_MAIN_RUN(test_check_coord);
    TEST_MAIN_RUN(test_depth_guard);
    TEST_MAIN_RUN(test_depth_limit);
    TEST_MAIN_RUN(test_safe_string);
    TEST_MAIN_RUN(test_refcount);
    TEST_MAIN_RUN(test_leak_detector);
    lv_cleanup();
TEST_MAIN_END()
