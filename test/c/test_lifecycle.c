/**
 * @file test_lifecycle.c
 * @brief 测试 lv_lifecycle 公共基元：字段销毁描述表 + 作用域守卫宏族
 *
 * 覆盖：
 *   (a) lv_obj_destroy_fields 释放顺序与 NULL 安全（含嵌套字段复合 offsetof）
 *   (b) lv_DEFER / lv_SCOPE_EXIT 在函数多出口（return / goto）下都执行，
 *       且逆序（LIFO）执行；lv_DEFER_FREE_MANY 批量释放
 *   (c) graph_destroy 字段描述表重写后的图对象生命周期往返（无泄漏/无崩溃）
 *   (d) 堆上复合对象生命周期往返（tracked 分配全部归还）
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/debug.h" /* RefCounted / ref_count_inc / ref_count_dec / ref_count_get */
#include "lv/lv_lifecycle.h"
#include "lv/magic.h"

#include "test_helpers.h"

/* test_helpers.h 要求的文件级计数器 */
int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * 测试用复合对象模型（覆盖全部字段释放方式）
 * ============================================================ */

typedef struct {
    char *label;
} TestSub;

typedef struct {
    char *tag;
} TestRecord;

typedef struct {
    char *reason;
} TestNested;

typedef struct {
    char *name;       /* LV_FIELD_PLAIN_FREE */
    char *desc;       /* LV_FIELD_PLAIN_FREE */
    lvDArray ints;    /* LV_FIELD_DARRAY_FREE（元素为 int，平凡值） */
    lvDArray records; /* LV_FIELD_DARRAY_ELEMS（元素含字符串） */
    TestSub *sub;     /* LV_FIELD_OBJECT */
    TestSub **list;   /* LV_FIELD_ARRAY_ELEMS */
    int list_count;
    int custom_calls; /* LV_FIELD_CUSTOM 标记 */
    TestNested nested; /* 嵌套字段（复合 offsetof） */
} TestComposite;

/* 销毁回调顺序记录（1=sub, 2=record 元素, 3=list 元素, 4=custom） */
static int g_order_buf[64];
static int g_order_idx = 0;

static void destroy_test_sub(void *obj) {
    TestSub *s = (TestSub *) obj;
    lv_free((void **) &s->label);
    lv_free((void **) &s);
    g_order_buf[g_order_idx++] = 1;
}

static void destroy_test_record(void *elem) {
    TestRecord *r = (TestRecord *) elem;
    lv_free((void **) &r->tag);
    g_order_buf[g_order_idx++] = 2;
}

static void destroy_test_list_elem(void *elem) {
    TestSub *s = (TestSub *) elem;
    lv_free((void **) &s->label);
    lv_free((void **) &s);
    g_order_buf[g_order_idx++] = 3;
}

static void destroy_test_custom(void *obj, void *field_ptr) {
    TestComposite *c = (TestComposite *) obj;
    (void) field_ptr;
    c->custom_calls++;
    g_order_buf[g_order_idx++] = 4;
}

static const lvFieldDesc s_test_composite_fields[] = {
    lv_FIELD_PLAIN(TestComposite, name),
    lv_FIELD_PLAIN(TestComposite, desc),
    lv_FIELD_DARRAY(TestComposite, ints),
    lv_FIELD_DARRAY_ELEMS(TestComposite, records, destroy_test_record),
    lv_FIELD_OBJECT(TestComposite, sub, destroy_test_sub),
    lv_FIELD_ARRAY(TestComposite, list, list_count, destroy_test_list_elem),
    lv_FIELD_CUSTOM(TestComposite, custom_calls, destroy_test_custom),
    /* 嵌套字段：复合 offsetof（nested.reason 为 TestNested 首字段） */
    {"nested.reason", LV_FIELD_PLAIN_FREE,
     offsetof(TestComposite, nested) + offsetof(TestNested, reason), 0, {NULL}},
};

#define TEST_COMPOSITE_FIELD_COUNT \
    (sizeof(s_test_composite_fields) / sizeof(s_test_composite_fields[0]))

/* 用 tracked 分配填充复合对象（便于泄漏检测） */
static void build_composite(TestComposite *c) {
    memset(c, 0, sizeof(*c));
    c->name = lv_TRACKED_MALLOC(16);
    strcpy(c->name, "name");
    c->desc = lv_TRACKED_MALLOC(16);
    strcpy(c->desc, "desc");

    lv_darray_init(&c->ints, sizeof(int));
    int v = 42;
    lv_darray_push(&c->ints, &v);

    lv_darray_init(&c->records, sizeof(TestRecord));
    TestRecord r;
    r.tag = lv_TRACKED_MALLOC(8);
    strcpy(r.tag, "rec");
    lv_darray_push(&c->records, &r);

    c->sub = lv_TRACKED_MALLOC(sizeof(TestSub));
    c->sub->label = lv_TRACKED_MALLOC(8);
    strcpy(c->sub->label, "sub");

    c->list = lv_TRACKED_MALLOC(2 * sizeof(TestSub *));
    c->list[0] = lv_TRACKED_MALLOC(sizeof(TestSub));
    c->list[0]->label = lv_TRACKED_MALLOC(8);
    strcpy(c->list[0]->label, "l0");
    c->list[1] = lv_TRACKED_MALLOC(sizeof(TestSub));
    c->list[1]->label = lv_TRACKED_MALLOC(8);
    strcpy(c->list[1]->label, "l1");
    c->list_count = 2;

    c->nested.reason = lv_TRACKED_MALLOC(8);
    strcpy(c->nested.reason, "why");
}

/* ============================================================
 * (a) lv_obj_destroy_fields 释放顺序与 NULL 安全
 * ============================================================ */

static void test_destroy_fields_order_and_null(void) {
    /* 以操作前追踪分配数为基线（lv_init 可能持有少量全局分配） */
    TEST_LEAK_BASELINE();
    TestComposite c;
    build_composite(&c);

    g_order_idx = 0;
    lv_obj_destroy_fields(&c, s_test_composite_fields, TEST_COMPOSITE_FIELD_COUNT);

    /* 释放顺序 = 描述表声明顺序：records 元素(2) → sub(1) → list 元素(3,3) → custom(4) */
    TEST_ASSERT_EQ(g_order_idx, 5);
    TEST_ASSERT_EQ(g_order_buf[0], 2);
    TEST_ASSERT_EQ(g_order_buf[1], 1);
    TEST_ASSERT_EQ(g_order_buf[2], 3);
    TEST_ASSERT_EQ(g_order_buf[3], 3);
    TEST_ASSERT_EQ(g_order_buf[4], 4);

    /* 全部指针字段置 NULL */
    TEST_ASSERT_NULL(c.name);
    TEST_ASSERT_NULL(c.desc);
    TEST_ASSERT_NULL(c.records.data);
    TEST_ASSERT_NULL(c.sub);
    TEST_ASSERT_NULL(c.list);
    TEST_ASSERT_NULL(c.nested.reason);
    TEST_ASSERT_EQ(c.custom_calls, 1);

    /* tracked 分配全部归还，无泄漏 */
    TEST_LEAK_NO_DELTA();

    /* 重复销毁安全（NULL 字段全部跳过，仅 custom 再次执行） */
    g_order_idx = 0;
    lv_obj_destroy_fields(&c, s_test_composite_fields, TEST_COMPOSITE_FIELD_COUNT);
    TEST_ASSERT_EQ(c.custom_calls, 2);
    TEST_ASSERT_EQ(g_order_idx, 1);
    TEST_ASSERT_EQ(g_order_buf[0], 4);
}

/* ============================================================
 * (b) defer 宏族：多出口执行 + LIFO 顺序 + 批量释放
 * ============================================================ */

static int g_defer_sum = 0;
static int g_defer_order[8];
static int g_defer_order_idx = 0;

static void defer_add_sum(void *arg) {
    g_defer_sum += (int) (intptr_t) arg;
}

static void defer_record(void *arg) {
    g_defer_order[g_defer_order_idx++] = (int) (intptr_t) arg;
}

/* 三个出口：return ×2 + goto */
static int defer_multi_exit_helper(int mode) {
    lv_DEFER(defer_add_sum, (void *) (intptr_t) 1);
    lv_DEFER(defer_add_sum, (void *) (intptr_t) 2);
    if (mode == 0)
        return 0; /* 出口 1 */
    if (mode == 1)
        return 1; /* 出口 2 */
    goto defer_out; /* 出口 3 */
defer_out:
    return 2;
}

static int defer_lifo_helper(void) {
    lv_DEFER(defer_record, (void *) (intptr_t) 1);
    lv_DEFER(defer_record, (void *) (intptr_t) 2);
    lv_DEFER(defer_record, (void *) (intptr_t) 3);
    return 0;
}

static void *g_defer_free_a = NULL;
static void *g_defer_free_b = NULL;

static int defer_free_many_helper(int mode) {
    g_defer_free_a = lv_TRACKED_MALLOC(32);
    g_defer_free_b = lv_TRACKED_MALLOC(32);
    lv_DEFER_FREE_MANY(&g_defer_free_a, &g_defer_free_b);
    if (mode == 0)
        return 0;
    return 1;
}

static int scope_exit_alias_helper(void) {
    lv_SCOPE_EXIT(defer_add_sum, (void *) (intptr_t) 7);
    return 0;
}

static void test_defer_multi_exit(void) {
    g_defer_sum = 0;
    TEST_ASSERT_EQ(defer_multi_exit_helper(0), 0);
    TEST_ASSERT_EQ(g_defer_sum, 3);
    g_defer_sum = 0;
    TEST_ASSERT_EQ(defer_multi_exit_helper(1), 1);
    TEST_ASSERT_EQ(g_defer_sum, 3);
    g_defer_sum = 0;
    TEST_ASSERT_EQ(defer_multi_exit_helper(2), 2);
    TEST_ASSERT_EQ(g_defer_sum, 3);
}

static void test_defer_lifo_order(void) {
    g_defer_order_idx = 0;
    TEST_ASSERT_EQ(defer_lifo_helper(), 0);
    TEST_ASSERT_EQ(g_defer_order_idx, 3);
    /* 逆序执行：后注册的先执行 */
    TEST_ASSERT_EQ(g_defer_order[0], 3);
    TEST_ASSERT_EQ(g_defer_order[1], 2);
    TEST_ASSERT_EQ(g_defer_order[2], 1);
}

static void test_defer_free_many(void) {
    int leaks_before = lv_memory_leak_report(NULL);
    for (int i = 0; i < 4; i++) {
        g_defer_free_a = NULL;
        g_defer_free_b = NULL;
        TEST_ASSERT_EQ(defer_free_many_helper(i % 2), i % 2);
        TEST_ASSERT_NULL(g_defer_free_a);
        TEST_ASSERT_NULL(g_defer_free_b);
    }
    TEST_ASSERT_EQ(lv_memory_leak_report(NULL), leaks_before);
}

static void test_scope_exit_alias(void) {
    g_defer_sum = 0;
    TEST_ASSERT_EQ(scope_exit_alias_helper(), 0);
    TEST_ASSERT_EQ(g_defer_sum, 7);
}

/* ============================================================
 * (c) graph_destroy 生命周期往返（字段描述表重写后无泄漏/无崩溃）
 * ============================================================ */

static void test_graph_destroy_roundtrip(void) {
    int leaks_before = lv_memory_leak_report(NULL);
    for (int iter = 0; iter < 20; iter++) {
        ConstraintGraph *g = graph_create();
        TEST_ASSERT_NOT_NULL(g);

        SymbolicCoord *x1 = mk_rat(0, 1);
        SymbolicCoord *y1 = mk_rat(0, 1);
        SymbolicCoord *x2 = mk_rat(1, 1);
        SymbolicCoord *y2 = mk_rat(0, 1);
        SymbolicCoord *coords1[2] = {x1, y1};
        SymbolicCoord *coords2[2] = {x2, y2};

        TEST_ASSERT_EQ(graph_add_point(g, coords1, 2), ADD_NODE_OK);
        TEST_ASSERT_EQ(graph_add_point(g, coords2, 2), ADD_NODE_OK);
        TEST_ASSERT_EQ(graph_add_line_segment(g, 0, 1), ADD_NODE_OK);
        TEST_ASSERT_EQ(graph_add_incidence(g, 0, 2), ADD_CONSTRAINT_OK);

        graph_destroy(g);

        /* graph_add_point 深拷贝坐标，归还测试侧的原坐标 */
        symbolic_coord_destroy(x1);
        symbolic_coord_destroy(y1);
        symbolic_coord_destroy(x2);
        symbolic_coord_destroy(y2);
    }
    TEST_ASSERT_EQ(lv_memory_leak_report(NULL), leaks_before);
}

/* ============================================================
 * (d) 堆上复合对象生命周期往返
 * ============================================================ */

static void test_heap_composite_roundtrip(void) {
    int leaks_before = lv_memory_leak_report(NULL);
    for (int i = 0; i < 5; i++) {
        TestComposite *c = lv_TRACKED_MALLOC(sizeof(TestComposite));
        TEST_ASSERT_NOT_NULL(c);
        build_composite(c);

        lv_obj_destroy_fields(c, s_test_composite_fields, TEST_COMPOSITE_FIELD_COUNT);
        lv_free((void **) &c);
        TEST_ASSERT_NULL(c);
    }
    TEST_ASSERT_EQ(lv_memory_leak_report(NULL), leaks_before);
}

/* ============================================================
 * (e) ref_count_dec 锁内 destructor 语义（D3 终审二回归测试）
 * ============================================================ */

static int g_dtor_calls = 0;

static void test_refcount_dtor_count(void *obj) {
    (void) obj;
    g_dtor_calls++;
}

static void test_refcount_dtor_free(void *obj) {
    lv_free((void **) &obj);
}

/* destructor 仅在引用计数降为 0 时于锁内被调用一次；重复 dec 不再触发 */
static void test_refcount_destructor_once(void) {
    /* 场景 1：计数 1 → 0 触发 destructor 一次，destructor 置空防重入，重复 dec 返回 false */
    lvRefCounted *obj = (lvRefCounted *) lv_calloc(1, sizeof(lvRefCounted));
    TEST_ASSERT_NOT_NULL(obj);
    obj->ref_count = 1;
    obj->destructor = test_refcount_dtor_count;

    g_dtor_calls = 0;
    TEST_ASSERT_EQ(ref_count_dec(obj), true);
    TEST_ASSERT_EQ(g_dtor_calls, 1);
    TEST_ASSERT_NULL(obj->destructor); /* 已置空，防止重复调用 */
    TEST_ASSERT_EQ(ref_count_dec(obj), false); /* 计数为 0：不执行任何操作 */
    TEST_ASSERT_EQ(g_dtor_calls, 1);           /* destructor 未被再次调用 */
    lv_free((void **) &obj);

    /* 场景 2：计数 2 → 1 → 0，destructor 仅在归零时调用一次 */
    lvRefCounted *o2 = (lvRefCounted *) lv_calloc(1, sizeof(lvRefCounted));
    TEST_ASSERT_NOT_NULL(o2);
    o2->ref_count = 2;
    o2->destructor = test_refcount_dtor_count;
    g_dtor_calls = 0;
    TEST_ASSERT_EQ(ref_count_dec(o2), false); /* 2 → 1，存活 */
    TEST_ASSERT_EQ(g_dtor_calls, 0);
    TEST_ASSERT_EQ(ref_count_dec(o2), true); /* 1 → 0，触发 */
    TEST_ASSERT_EQ(g_dtor_calls, 1);
    lv_free((void **) &o2);

    /* 场景 3：inc/dec 往返 + destructor 实际释放对象（锁内调用后对象失效） */
    lvRefCounted *o3 = (lvRefCounted *) lv_calloc(1, sizeof(lvRefCounted));
    TEST_ASSERT_NOT_NULL(o3);
    o3->ref_count = 1;
    o3->destructor = test_refcount_dtor_free;
    ref_count_inc(o3);
    TEST_ASSERT_EQ(ref_count_get(o3), 2);
    TEST_ASSERT_EQ(ref_count_dec(o3), false); /* 2 → 1，存活 */
    TEST_ASSERT_EQ(ref_count_dec(o3), true);  /* 1 → 0，destructor 在锁内释放 o3 */
}

/* ============================================================
 * (f) K7: LV_DESTROY_SHIM 收敛后字段销毁等价
 * （magic_array / spell / domain 子资源销毁无泄漏）
 * ============================================================ */
static void test_k7_shim_destroy_equivalence(void) {
    TEST_LEAK_BASELINE();

    /* MagicArray：runes / graph / constraints 子资源销毁（含扩容路径） */
    MagicArray *array = magic_array_create();
    TEST_ASSERT_NOT_NULL(array);
    for (int i = 0; i < 40; i++) {
        Rune *r = rune_create_rational(i + 1, 1, ELEMENT_FIRE);
        TEST_ASSERT_NOT_NULL(r);
        /* magic_array_add_rune 返回图中节点索引（成功 >= 0，失败 -1）；
         * 原符文所有权归调用者，add 内部已复制，此处必须释放 */
        TEST_ASSERT_MSG(magic_array_add_rune(array, r) >= 0, "magic_array_add_rune 失败");
        rune_destroy(r);
    }
    TEST_ASSERT_EQ(magic_array_get_rune_count(array), 40);
    magic_array_destroy(array);

    /* Spell：molding 子序列销毁 */
    Spell *spell = spell_create("K7ShimSpell");
    TEST_ASSERT_NOT_NULL(spell);
    spell_destroy(spell);

    /* Domain：center / rules 子资源销毁 */
    Domain *domain = domain_create("K7ShimDomain", 10);
    TEST_ASSERT_NOT_NULL(domain);
    domain_destroy(domain);

    TEST_LEAK_NO_DELTA();
}

/* ============================================================
 * 测试入口
 * ============================================================ */

TEST_MAIN_BEGIN("生命周期管理")
    printf("=== Lv-00 复合对象生命周期 / 作用域守卫测试 ===\n\n");

    TEST_MAIN_RUN(test_destroy_fields_order_and_null);
    TEST_MAIN_RUN(test_defer_multi_exit);
    TEST_MAIN_RUN(test_defer_lifo_order);
    TEST_MAIN_RUN(test_defer_free_many);
    TEST_MAIN_RUN(test_scope_exit_alias);
    TEST_MAIN_RUN(test_graph_destroy_roundtrip);
    TEST_MAIN_RUN(test_heap_composite_roundtrip);
    TEST_MAIN_RUN(test_refcount_destructor_once);
    TEST_MAIN_RUN(test_k7_shim_destroy_equivalence);

TEST_MAIN_END()