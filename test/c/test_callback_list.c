/**
 * @file test_callback_list.c
 * @brief 泛型回调列表（lvCallbackList）行为测试
 *
 * 覆盖 core/src/layer2_resource/lv_callback_list.c（公共设施）：
 * - 生命周期：init / clear / cleanup（含 NULL 安全）
 * - 注册：add 返回自增 ID（>= 1），NULL 回调返回 -1
 * - 注销：remove_by_id / remove_by_fn，前移紧凑且保持注册顺序
 * - 过滤值：set_filter / get_filter（未找到返回 0 / false）
 * - 分发：filter 匹配才回调（invoke 次数断言）；遍历中注册/注销
 *   的迭代安全（回调里注销自己/其他/新增，不崩溃且行为可预期）
 * - max_entries 硬上限：超过返回 -1，0 = 无限制
 *
 * 测试边界说明：
 * - 迭代安全语义（快照 count + 越界检查，见实现注释）下，遍历中注销
 *   会导致部分后续回调被跳过（前移紧凑），断言采用实现可预期的确定性
 *   模式，不做"全部回调都恰好执行一次"的过度断言。
 *
 * @version 1.0.0
 * @date 2026-08-06
 */

#include <stdint.h>
#include <string.h>

#include "lv/lv_callback_list.h"
#include "test_helpers.h"

/* ============================================================
 * Global test counters
 * ============================================================ */
int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * 测试回调：5 个互相不同的函数指针（remove_by_fn 需要区分）
 * ============================================================ */
static void cb0(void) {}
static void cb1(void) {}
static void cb2(void) {}
static void cb3(void) {}
static void cb_extra(void) {}

/* ============================================================
 * 测试上下文：invoke 中记录调用次数/顺序，并可修改列表
 * ============================================================ */
#define CB_COUNT 4

typedef struct {
    int invoked[CB_COUNT];  /* 各回调被分发次数 */
    int order[8];           /* 分发顺序（回调索引） */
    int order_count;
    lvCallbackList *list;        /* 供回调内修改列表 */
    int remove_self_idx;         /* 被分发到该索引回调时注销自己（-1 = 不注销） */
    int remove_other_idx;        /* 被分发到该索引回调时注销 remove_other_fn（-1 = 不注销） */
    lvCallbackFn remove_other_fn;
    int add_idx;                 /* 被分发到该索引回调时新增 cb_extra（-1 = 不新增） */
} TestCtx;

static TestCtx g_ctx;

static void reset_ctx(void) {
    memset(&g_ctx, 0, sizeof(g_ctx));
    g_ctx.remove_self_idx = -1;
    g_ctx.remove_other_idx = -1;
    g_ctx.add_idx = -1;
}

/* ============================================================
 * 分发辅助：filter / invoke
 * ============================================================ */

/* 过滤：仅偶数 filter 值才分发 */
static bool filter_even(const lvCallbackEntry *entry, const void *dispatch_arg) {
    (void) dispatch_arg;
    return (entry->filter & 1u) == 0u;
}

/* invoke：记录回调调用并执行可选的"回调内修改列表"行为 */
static void test_invoke(const lvCallbackEntry *entry, const void *dispatch_arg) {
    (void) dispatch_arg;
    int idx = (int) (intptr_t) entry->user_data;
    if (idx >= 0 && idx < CB_COUNT) {
        if (g_ctx.invoked[idx] == 0 && g_ctx.order_count < (int) (sizeof(g_ctx.order) / sizeof(g_ctx.order[0])))
            g_ctx.order[g_ctx.order_count++] = idx;
        g_ctx.invoked[idx]++;
    }
    if (g_ctx.list && g_ctx.remove_self_idx == idx)
        lv_callback_list_remove_by_fn(g_ctx.list, entry->callback);
    if (g_ctx.list && g_ctx.remove_other_idx == idx && g_ctx.remove_other_fn)
        lv_callback_list_remove_by_fn(g_ctx.list, g_ctx.remove_other_fn);
    if (g_ctx.list && g_ctx.add_idx == idx)
        lv_callback_list_add(g_ctx.list, cb_extra, (void *) (intptr_t) CB_COUNT, 0);
}

/* 依次注册 cb0..cb3，user_data 分别为索引 0..3 */
static void add_four(lvCallbackList *list, uint64_t f0, uint64_t f1, uint64_t f2, uint64_t f3) {
    lv_callback_list_add(list, cb0, (void *) (intptr_t) 0, f0);
    lv_callback_list_add(list, cb1, (void *) (intptr_t) 1, f1);
    lv_callback_list_add(list, cb2, (void *) (intptr_t) 2, f2);
    lv_callback_list_add(list, cb3, (void *) (intptr_t) 3, f3);
}

/* ============================================================
 * Test: 生命周期与自增 ID
 * ============================================================ */

static void test_init_add_id(void) {
    lvCallbackList list;
    lv_callback_list_init(&list, 0, 0);
    TEST_ASSERT_EQ(lv_callback_list_count(&list), 0);

    int id1 = lv_callback_list_add(&list, cb0, NULL, 0);
    int id2 = lv_callback_list_add(&list, cb1, NULL, 0);
    int id3 = lv_callback_list_add(&list, cb2, NULL, 0);
    TEST_ASSERT_EQ(id1, 1);
    TEST_ASSERT_EQ(id2, 2);
    TEST_ASSERT_EQ(id3, 3);
    TEST_ASSERT_EQ(lv_callback_list_count(&list), 3);

    /* NULL 回调注册失败，计数不变 */
    TEST_ASSERT_EQ(lv_callback_list_add(&list, NULL, NULL, 0), -1);
    TEST_ASSERT_EQ(lv_callback_list_count(&list), 3);

    /* clear 保留容量、计数清零，next_id 继续自增 */
    lv_callback_list_clear(&list);
    TEST_ASSERT_EQ(lv_callback_list_count(&list), 0);
    TEST_ASSERT_EQ(lv_callback_list_add(&list, cb0, NULL, 0), 4);

    lv_callback_list_cleanup(&list);
    TEST_ASSERT_EQ(lv_callback_list_count(&list), 0);

    /* NULL 安全 */
    TEST_ASSERT_EQ(lv_callback_list_count(NULL), 0);
    lv_callback_list_init(NULL, 0, 0);
    lv_callback_list_cleanup(NULL);
    lv_callback_list_clear(NULL);
}

/* ============================================================
 * Test: remove_by_id
 * ============================================================ */

static void test_remove_by_id(void) {
    lvCallbackList list;
    lv_callback_list_init(&list, 0, 0);
    int id1 = lv_callback_list_add(&list, cb0, NULL, 0);
    int id2 = lv_callback_list_add(&list, cb1, NULL, 0);
    int id3 = lv_callback_list_add(&list, cb2, NULL, 0);
    TEST_ASSERT_EQ(id1, 1);
    TEST_ASSERT_EQ(id2, 2);
    TEST_ASSERT_EQ(id3, 3);

    TEST_ASSERT_MSG(lv_callback_list_remove_by_id(&list, id2), "remove existing id should succeed");
    TEST_ASSERT_EQ(lv_callback_list_count(&list), 2);

    /* 已移除的 ID 再次移除失败 */
    TEST_ASSERT_MSG(!lv_callback_list_remove_by_id(&list, id2), "removed id should not be removable again");
    /* 无效 ID / 不存在的 ID / NULL 列表 */
    TEST_ASSERT_MSG(!lv_callback_list_remove_by_id(&list, 0), "id<=0 should fail");
    TEST_ASSERT_MSG(!lv_callback_list_remove_by_id(&list, -5), "negative id should fail");
    TEST_ASSERT_MSG(!lv_callback_list_remove_by_id(&list, 999), "nonexistent id should fail");
    TEST_ASSERT_MSG(!lv_callback_list_remove_by_id(NULL, id1), "NULL list should fail");

    lv_callback_list_cleanup(&list);
}

/* ============================================================
 * Test: remove_by_fn —— 前移紧凑、顺序保持
 * ============================================================ */

static void test_remove_by_fn_order(void) {
    lvCallbackList list;
    lv_callback_list_init(&list, 0, 0);
    add_four(&list, 0, 0, 0, 0);

    /* 移除中间条目 cb1：剩余 cb0, cb2, cb3，顺序保持 */
    TEST_ASSERT_MSG(lv_callback_list_remove_by_fn(&list, cb1), "remove_by_fn cb1 should succeed");
    TEST_ASSERT_EQ(lv_callback_list_count(&list), 3);

    reset_ctx();
    g_ctx.list = &list;
    lv_callback_list_dispatch(&list, NULL, NULL, test_invoke);
    TEST_ASSERT_EQ(g_ctx.invoked[0], 1);
    TEST_ASSERT_EQ(g_ctx.invoked[1], 0);
    TEST_ASSERT_EQ(g_ctx.invoked[2], 1);
    TEST_ASSERT_EQ(g_ctx.invoked[3], 1);
    TEST_ASSERT_EQ(g_ctx.order_count, 3);
    TEST_ASSERT_EQ(g_ctx.order[0], 0);
    TEST_ASSERT_EQ(g_ctx.order[1], 2);
    TEST_ASSERT_EQ(g_ctx.order[2], 3);

    /* 移除头条目 cb0：剩余 cb2, cb3 */
    TEST_ASSERT_MSG(lv_callback_list_remove_by_fn(&list, cb0), "remove_by_fn cb0 should succeed");
    TEST_ASSERT_EQ(lv_callback_list_count(&list), 2);

    /* 不存在的函数指针移除失败 */
    TEST_ASSERT_MSG(!lv_callback_list_remove_by_fn(&list, cb_extra), "unregistered fn should fail");
    TEST_ASSERT_MSG(!lv_callback_list_remove_by_fn(&list, NULL), "NULL fn should fail");

    reset_ctx();
    g_ctx.list = &list;
    lv_callback_list_dispatch(&list, NULL, NULL, test_invoke);
    TEST_ASSERT_EQ(g_ctx.order_count, 2);
    TEST_ASSERT_EQ(g_ctx.order[0], 2);
    TEST_ASSERT_EQ(g_ctx.order[1], 3);

    lv_callback_list_cleanup(&list);
}

/* ============================================================
 * Test: set_filter / get_filter
 * ============================================================ */

static void test_set_get_filter(void) {
    lvCallbackList list;
    lv_callback_list_init(&list, 0, 0);
    int id1 = lv_callback_list_add(&list, cb0, NULL, 10);
    int id2 = lv_callback_list_add(&list, cb1, NULL, 20);
    int id3 = lv_callback_list_add(&list, cb2, NULL, 30);
    TEST_ASSERT_EQ((int) lv_callback_list_get_filter(&list, id1), 10);
    TEST_ASSERT_EQ((int) lv_callback_list_get_filter(&list, id2), 20);
    TEST_ASSERT_EQ((int) lv_callback_list_get_filter(&list, id3), 30);

    TEST_ASSERT_MSG(lv_callback_list_set_filter(&list, id1, 99), "set_filter existing id should succeed");
    TEST_ASSERT_EQ((int) lv_callback_list_get_filter(&list, id1), 99);

    /* 未找到 / 无效参数 */
    TEST_ASSERT_MSG(!lv_callback_list_set_filter(&list, 999, 1), "set_filter nonexistent id should fail");
    TEST_ASSERT_EQ((int) lv_callback_list_get_filter(&list, 999), 0);
    TEST_ASSERT_EQ((int) lv_callback_list_get_filter(&list, 0), 0);
    TEST_ASSERT_MSG(!lv_callback_list_set_filter(NULL, id1, 1), "set_filter NULL list should fail");
    TEST_ASSERT_EQ((int) lv_callback_list_get_filter(NULL, id1), 0);

    lv_callback_list_cleanup(&list);
}

/* ============================================================
 * Test: dispatch —— filter 匹配才回调
 * ============================================================ */

static void test_dispatch_filter(void) {
    lvCallbackList list;
    lv_callback_list_init(&list, 0, 0);
    add_four(&list, 1, 2, 3, 4); /* cb1 filter=2, cb3 filter=4 为偶数 */

    reset_ctx();
    lv_callback_list_dispatch(&list, NULL, filter_even, test_invoke);
    TEST_ASSERT_EQ(g_ctx.invoked[0], 0);
    TEST_ASSERT_EQ(g_ctx.invoked[1], 1);
    TEST_ASSERT_EQ(g_ctx.invoked[2], 0);
    TEST_ASSERT_EQ(g_ctx.invoked[3], 1);
    TEST_ASSERT_EQ(g_ctx.order_count, 2);
    TEST_ASSERT_EQ(g_ctx.order[0], 1);
    TEST_ASSERT_EQ(g_ctx.order[1], 3);

    /* filter = NULL 表示全部调用 */
    reset_ctx();
    lv_callback_list_dispatch(&list, NULL, NULL, test_invoke);
    TEST_ASSERT_EQ(g_ctx.invoked[0], 1);
    TEST_ASSERT_EQ(g_ctx.invoked[1], 1);
    TEST_ASSERT_EQ(g_ctx.invoked[2], 1);
    TEST_ASSERT_EQ(g_ctx.invoked[3], 1);
    TEST_ASSERT_EQ(g_ctx.order_count, 4);

    /* invoke = NULL：只遍历不调用，不崩溃 */
    lv_callback_list_dispatch(&list, NULL, NULL, NULL);
    lv_callback_list_dispatch(NULL, NULL, NULL, test_invoke);

    lv_callback_list_cleanup(&list);
}

/* ============================================================
 * Test: dispatch 迭代安全 —— 回调里注销自己
 * ============================================================ */

static void test_dispatch_remove_self(void) {
    lvCallbackList list;
    lv_callback_list_init(&list, 0, 0);
    add_four(&list, 0, 0, 0, 0);

    reset_ctx();
    g_ctx.list = &list;
    g_ctx.remove_self_idx = 2; /* cb2 被分发时注销自己 */

    lv_callback_list_dispatch(&list, NULL, NULL, test_invoke);
    /* 快照 count=4；cb2 注销后前移，i=3 处 count=3 -> 越界跳出，cb3 被跳过 */
    TEST_ASSERT_EQ(g_ctx.invoked[0], 1);
    TEST_ASSERT_EQ(g_ctx.invoked[1], 1);
    TEST_ASSERT_EQ(g_ctx.invoked[2], 1);
    TEST_ASSERT_EQ(g_ctx.invoked[3], 0);
    TEST_ASSERT_EQ(g_ctx.order_count, 3);
    TEST_ASSERT_EQ(g_ctx.order[0], 0);
    TEST_ASSERT_EQ(g_ctx.order[1], 1);
    TEST_ASSERT_EQ(g_ctx.order[2], 2);
    TEST_ASSERT_EQ(lv_callback_list_count(&list), 3);

    lv_callback_list_cleanup(&list);
}

/* ============================================================
 * Test: dispatch 迭代安全 —— 回调里注销其他回调
 * ============================================================ */

static void test_dispatch_remove_other(void) {
    lvCallbackList list;
    lv_callback_list_init(&list, 0, 0);
    add_four(&list, 0, 0, 0, 0);

    reset_ctx();
    g_ctx.list = &list;
    g_ctx.remove_other_idx = 1;    /* cb1 被分发时注销 cb0 */
    g_ctx.remove_other_fn = cb0;

    lv_callback_list_dispatch(&list, NULL, NULL, test_invoke);
    /* i=0 cb0 已调用；i=1 cb1 调用并移除 cb0 -> [cb1,cb2,cb3]；
     * i=2 命中 cb3；i=3 越界跳出 -> cb2 被跳过 */
    TEST_ASSERT_EQ(g_ctx.invoked[0], 1);
    TEST_ASSERT_EQ(g_ctx.invoked[1], 1);
    TEST_ASSERT_EQ(g_ctx.invoked[2], 0);
    TEST_ASSERT_EQ(g_ctx.invoked[3], 1);
    TEST_ASSERT_EQ(g_ctx.order_count, 3);
    TEST_ASSERT_EQ(g_ctx.order[0], 0);
    TEST_ASSERT_EQ(g_ctx.order[1], 1);
    TEST_ASSERT_EQ(g_ctx.order[2], 3);
    TEST_ASSERT_EQ(lv_callback_list_count(&list), 3);

    lv_callback_list_cleanup(&list);
}

/* ============================================================
 * Test: dispatch 迭代安全 —— 回调里新增回调（快照外不调用）
 * ============================================================ */

static void test_dispatch_add_during(void) {
    lvCallbackList list;
    lv_callback_list_init(&list, 0, 0);
    add_four(&list, 0, 0, 0, 0);

    reset_ctx();
    g_ctx.list = &list;
    g_ctx.add_idx = 1; /* cb1 被分发时新增 cb_extra */

    lv_callback_list_dispatch(&list, NULL, NULL, test_invoke);
    /* 快照 count=4：新增的 cb_extra 在快照之外，本次分发不被调用 */
    TEST_ASSERT_EQ(g_ctx.invoked[0], 1);
    TEST_ASSERT_EQ(g_ctx.invoked[1], 1);
    TEST_ASSERT_EQ(g_ctx.invoked[2], 1);
    TEST_ASSERT_EQ(g_ctx.invoked[3], 1);
    TEST_ASSERT_EQ(g_ctx.order_count, 4);
    /* 列表实际新增 1 条 */
    TEST_ASSERT_EQ(lv_callback_list_count(&list), 5);

    lv_callback_list_cleanup(&list);
}

/* ============================================================
 * Test: max_entries 硬上限
 * ============================================================ */

static void test_max_entries(void) {
    lvCallbackList list;
    lv_callback_list_init(&list, 0, 2); /* 上限 2 */
    int id1 = lv_callback_list_add(&list, cb0, NULL, 0);
    int id2 = lv_callback_list_add(&list, cb1, NULL, 0);
    TEST_ASSERT_EQ(id1, 1);
    TEST_ASSERT_EQ(id2, 2);
    TEST_ASSERT_EQ(lv_callback_list_add(&list, cb2, NULL, 0), -1);
    TEST_ASSERT_EQ(lv_callback_list_count(&list), 2);

    /* 注销腾位后可继续注册 */
    TEST_ASSERT_MSG(lv_callback_list_remove_by_id(&list, id1), "remove to free slot");
    TEST_ASSERT_EQ(lv_callback_list_add(&list, cb2, NULL, 0), 3);
    TEST_ASSERT_EQ(lv_callback_list_add(&list, cb0, NULL, 0), -1);
    lv_callback_list_cleanup(&list);

    /* max_entries = 0 表示无限制（覆盖动态扩容路径） */
    lvCallbackList unlimited;
    lv_callback_list_init(&unlimited, 0, 0);
    int ok = 1;
    for (int i = 0; i < 64; i++) {
        if (lv_callback_list_add(&unlimited, cb0, NULL, 0) < 1) {
            ok = 0;
            break;
        }
    }
    TEST_ASSERT_MSG(ok, "unlimited list should accept 64 callbacks (growth path)");
    TEST_ASSERT_EQ(lv_callback_list_count(&unlimited), 64);
    lv_callback_list_cleanup(&unlimited);
}

/* ============================================================
 * Main
 * ============================================================ */
TEST_MAIN_BEGIN("CallbackList")

    TEST_MAIN_RUN(test_init_add_id);
    TEST_MAIN_RUN(test_remove_by_id);
    TEST_MAIN_RUN(test_remove_by_fn_order);
    TEST_MAIN_RUN(test_set_get_filter);
    TEST_MAIN_RUN(test_dispatch_filter);
    TEST_MAIN_RUN(test_dispatch_remove_self);
    TEST_MAIN_RUN(test_dispatch_remove_other);
    TEST_MAIN_RUN(test_dispatch_add_during);
    TEST_MAIN_RUN(test_max_entries);

TEST_MAIN_END()
