/**
 * @file test_gappa_propagate_ext.c
 * @brief Gappa 区间传播契约测试（批次 C-㊺续30：gappa_propagate.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（7 个）：
 *   lv_gappa_pred_set_init / add / find / clear
 *   lv_gappa_propagate_config_default
 *   lv_gappa_propagate_set
 *   lv_gappa_propagate
 *
 * 契约要点（与 gappa_propagate.h / gappa_propagate.c 核对）：
 *   - pred_set：以 expr_lhs 为名称在集合内查重（注册表 key "<set>:<expr_lhs>"），
 *     不同集合允许同名谓词；find 返回索引（未找到 -1）并输出谓词副本。
 *   - config_default：max_iterations=1，precision=lv_GAPPA_BOUND_SLACK(1e-15)，
 *     backward=false。
 *   - propagate_set：复制输入谓词到输出并迭代推导（和/差、乘/除/平方规则），
 *     返回值 = 成功推导数（>0）；输入输出须为不同集合（输出会被 init 清空）。
 *   - propagate：Gappa DSL 表达式正向传播，变量默认区间 [-1,1]；
 *     NULL 参数返回 -1。
 *
 * @author Lv-00 Project
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "lv/gappa_dsl.h"
#include "lv/gappa_propagate.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

#define TOL 1e-9

/** @brief 构造一个 BND 谓词 */
static lvGappaPredicate make_bnd(const char *lhs, double lo, double hi) {
    lvGappaPredicate p;
    memset(&p, 0, sizeof(p));
    p.type = lv_PRED_BND;
    strncpy(p.expr_lhs, lhs, sizeof(p.expr_lhs) - 1);
    p.bound_lo = lo;
    p.bound_hi = hi;
    p.is_hypothesis = true;
    return p;
}

/* ============== 测试：谓词集 init/add/find/clear ============== */

static void test_pred_set_api(void) {
    lvGappaPredSet set;
    lv_gappa_pred_set_init(&set);
    TEST_ASSERT_EQ(set.count, 0);
    TEST_ASSERT_EQ(set.capacity, 0);

    /* init(NULL) 安全 */
    lv_gappa_pred_set_init(NULL);

    /* 添加谓词 */
    lvGappaPredicate p = make_bnd("x", 1.0, 2.0);
    TEST_ASSERT(lv_gappa_pred_set_add(&set, &p), "add x");
    TEST_ASSERT_EQ(set.count, 1);

    /* 重复 expr_lhs 被拒绝，count 不变 */
    lvGappaPredicate p2 = make_bnd("x", 5.0, 6.0);
    TEST_ASSERT(!lv_gappa_pred_set_add(&set, &p2), "duplicate x rejected");
    TEST_ASSERT_EQ(set.count, 1);

    /* 不同名称共存 */
    lvGappaPredicate q = make_bnd("y", 3.0, 4.0);
    TEST_ASSERT(lv_gappa_pred_set_add(&set, &q), "add y");
    TEST_ASSERT_EQ(set.count, 2);

    /* find：返回索引并输出副本 */
    lvGappaPredicate found;
    memset(&found, 0, sizeof(found));
    int idx = lv_gappa_pred_set_find(&set, "x", &found);
    TEST_ASSERT_EQ(idx, 0);
    TEST_ASSERT_STR_EQ(found.expr_lhs, "x");
    TEST_ASSERT_DOUBLE(found.bound_lo, 1.0, TOL);
    TEST_ASSERT_DOUBLE(found.bound_hi, 2.0, TOL);
    TEST_ASSERT(found.is_hypothesis, "is_hypothesis copied");

    idx = lv_gappa_pred_set_find(&set, "y", NULL);
    TEST_ASSERT_EQ(idx, 1);

    /* 未找到：-1；NULL 参数安全 */
    TEST_ASSERT_EQ(lv_gappa_pred_set_find(&set, "z", NULL), -1);
    TEST_ASSERT_EQ(lv_gappa_pred_set_find(NULL, "x", NULL), -1);
    TEST_ASSERT_EQ(lv_gappa_pred_set_find(&set, NULL, NULL), -1);

    /* clear：清空并释放，可继续 add */
    lv_gappa_pred_set_clear(&set);
    TEST_ASSERT_EQ(set.count, 0);
    TEST_ASSERT_NULL(set.preds);
    TEST_ASSERT_EQ(lv_gappa_pred_set_find(&set, "x", NULL), -1);
    TEST_ASSERT(lv_gappa_pred_set_add(&set, &p), "re-add after clear");
    TEST_ASSERT_EQ(set.count, 1);

    lv_gappa_pred_set_clear(&set);
    lv_gappa_pred_set_clear(NULL);
}

/* ============== 测试：不同集合同名谓词隔离 ============== */

static void test_pred_set_isolation(void) {
    lvGappaPredSet s1, s2;
    lv_gappa_pred_set_init(&s1);
    lv_gappa_pred_set_init(&s2);

    lvGappaPredicate p = make_bnd("x", 1.0, 2.0);
    TEST_ASSERT(lv_gappa_pred_set_add(&s1, &p), "s1 add x");
    TEST_ASSERT(lv_gappa_pred_set_add(&s2, &p), "s2 add x (independent)");
    TEST_ASSERT_EQ(lv_gappa_pred_set_find(&s1, "x", NULL), 0);
    TEST_ASSERT_EQ(lv_gappa_pred_set_find(&s2, "x", NULL), 0);

    lv_gappa_pred_set_clear(&s1);
    lv_gappa_pred_set_clear(&s2);
}

/* ============== 测试：默认配置 ============== */

static void test_config_default_api(void) {
    lvGappaPropagateConfig cfg = lv_gappa_propagate_config_default();
    TEST_ASSERT_EQ(cfg.max_iterations, 1);
    TEST_ASSERT_DOUBLE(cfg.precision, 1e-15, 1e-30);
    TEST_ASSERT(!cfg.backward, "backward default false");
}

/* ============== 测试：结构化传播 propagate_set ============== */

static void test_propagate_set_api(void) {
    lvGappaPredSet in, out;
    lv_gappa_pred_set_init(&in);
    lv_gappa_pred_set_init(&out);

    /* 输入：x ∈ [1,2]，y ∈ [3,4] */
    lvGappaPredicate px = make_bnd("x", 1.0, 2.0);
    lvGappaPredicate py = make_bnd("y", 3.0, 4.0);
    TEST_ASSERT(lv_gappa_pred_set_add(&in, &px), "in x");
    TEST_ASSERT(lv_gappa_pred_set_add(&in, &py), "in y");

    lvGappaPropagateConfig cfg = lv_gappa_propagate_config_default();
    int derived = lv_gappa_propagate_set(&in, &out, &cfg);
    TEST_ASSERT(derived > 0, "derived > 0");
    TEST_ASSERT(out.count > in.count, "output grew");

    /* x + y ∈ [4, 6] */
    lvGappaPredicate f;
    memset(&f, 0, sizeof(f));
    TEST_ASSERT(lv_gappa_pred_set_find(&out, "x + y", &f) >= 0, "found x + y");
    TEST_ASSERT_DOUBLE(f.bound_lo, 4.0, TOL);
    TEST_ASSERT_DOUBLE(f.bound_hi, 6.0, TOL);

    /* y - x ∈ [3-2, 4-1] = [1, 3] */
    memset(&f, 0, sizeof(f));
    TEST_ASSERT(lv_gappa_pred_set_find(&out, "y - x", &f) >= 0, "found y - x");
    TEST_ASSERT_DOUBLE(f.bound_lo, 1.0, TOL);
    TEST_ASSERT_DOUBLE(f.bound_hi, 3.0, TOL);

    /* (x)^2 ∈ [1, 4] */
    memset(&f, 0, sizeof(f));
    TEST_ASSERT(lv_gappa_pred_set_find(&out, "(x)^2", &f) >= 0, "found (x)^2");
    TEST_ASSERT_DOUBLE(f.bound_lo, 1.0, TOL);
    TEST_ASSERT_DOUBLE(f.bound_hi, 4.0, TOL);

    /* x * y ∈ [3, 8] */
    memset(&f, 0, sizeof(f));
    TEST_ASSERT(lv_gappa_pred_set_find(&out, "x * y", &f) >= 0, "found x * y");
    TEST_ASSERT_DOUBLE(f.bound_lo, 3.0, TOL);
    TEST_ASSERT_DOUBLE(f.bound_hi, 8.0, TOL);

    /* x / y ∈ [1/4, 2/3] */
    memset(&f, 0, sizeof(f));
    TEST_ASSERT(lv_gappa_pred_set_find(&out, "x / y", &f) >= 0, "found x / y");
    TEST_ASSERT_DOUBLE(f.bound_lo, 0.25, TOL);
    TEST_ASSERT_DOUBLE(f.bound_hi, 2.0 / 3.0, TOL);

    /* NULL 参数安全 */
    TEST_ASSERT_EQ(lv_gappa_propagate_set(NULL, &out, &cfg), 0);
    TEST_ASSERT_EQ(lv_gappa_propagate_set(&in, NULL, &cfg), 0);

    lv_gappa_pred_set_clear(&in);
    lv_gappa_pred_set_clear(&out);
}

/* ============== 测试：表达式传播 propagate ============== */

static void test_propagate_expr_api(void) {
    double lo = 0.0, hi = 0.0;

    /* x + 1：默认变量 x ∈ [-1,1] -> [0,2] */
    TEST_ASSERT_EQ(lv_gappa_propagate("x + 1", &lo, &hi), 0);
    TEST_ASSERT_DOUBLE(lo, 0.0, TOL);
    TEST_ASSERT_DOUBLE(hi, 2.0, TOL);

    /* 2 * x -> [-2,2] */
    TEST_ASSERT_EQ(lv_gappa_propagate("2 * x", &lo, &hi), 0);
    TEST_ASSERT_DOUBLE(lo, -2.0, TOL);
    TEST_ASSERT_DOUBLE(hi, 2.0, TOL);

    /* sqrt(x)：x ∈ [-1,1] 负下界截断 -> [0,1] */
    TEST_ASSERT_EQ(lv_gappa_propagate("sqrt(x)", &lo, &hi), 0);
    TEST_ASSERT_DOUBLE(lo, 0.0, TOL);
    TEST_ASSERT_DOUBLE(hi, 1.0, TOL);

    /* log(x)：非正下界 -> lo = -HUGE_VAL, hi = log(1) = 0 */
    TEST_ASSERT_EQ(lv_gappa_propagate("log(x)", &lo, &hi), 0);
    TEST_ASSERT(lo == -HUGE_VAL, "log lo -HUGE_VAL");
    TEST_ASSERT_DOUBLE(hi, 0.0, TOL);

    /* NULL 参数 */
    TEST_ASSERT_EQ(lv_gappa_propagate(NULL, &lo, &hi), -1);
    TEST_ASSERT_EQ(lv_gappa_propagate("x + 1", NULL, &hi), -1);
    TEST_ASSERT_EQ(lv_gappa_propagate("x + 1", &lo, NULL), -1);
}

/* ============== 测试：反向传播 backward（批次 C-㊺续36 追加） ============== */

static void test_propagate_backward_api(void) {
    lvGappaPredSet out;
    lv_gappa_pred_set_init(&out);

    /* NULL 契约 */
    TEST_ASSERT_EQ(lv_gappa_propagate_backward(NULL, NULL, &out, NULL), 0);

    /* BND 目标：复制目标区间为 hypothesis */
    lvGappaPredicate goal = make_bnd("z", 1.0, 2.0);
    int needed = lv_gappa_propagate_backward(&goal, NULL, &out, NULL);
    TEST_ASSERT_EQ(needed, 1);
    lvGappaPredicate f;
    memset(&f, 0, sizeof(f));
    TEST_ASSERT(lv_gappa_pred_set_find(&out, "z", &f) >= 0, "found z");
    TEST_ASSERT_DOUBLE(f.bound_lo, 1.0, 1e-9);
    TEST_ASSERT_DOUBLE(f.bound_hi, 2.0, 1e-9);
    TEST_ASSERT(f.is_hypothesis, "hypothesis flag");
    lv_gappa_pred_set_clear(&out);

    /* ABS 目标：|x - 0.5| <= 0.25 → x ∈ [0.25, 0.75] */
    memset(&goal, 0, sizeof(goal));
    goal.type = lv_PRED_ABS;
    strncpy(goal.expr_lhs, "x - 0.5", sizeof(goal.expr_lhs) - 1);
    strncpy(goal.expr_rhs, "0.5", sizeof(goal.expr_rhs) - 1);
    goal.bound_abs = 0.25;
    needed = lv_gappa_propagate_backward(&goal, NULL, &out, NULL);
    TEST_ASSERT_EQ(needed, 1);
    memset(&f, 0, sizeof(f));
    TEST_ASSERT(lv_gappa_pred_set_find(&out, "x", &f) >= 0, "derived x");
    TEST_ASSERT_DOUBLE(f.bound_lo, 0.25, 1e-9);
    TEST_ASSERT_DOUBLE(f.bound_hi, 0.75, 1e-9);

    lv_gappa_pred_set_clear(&out);
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("GappaPropagateExt")

    printf("\n--- gappa_propagate (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_pred_set_api);
    TEST_MAIN_RUN(test_pred_set_isolation);
    TEST_MAIN_RUN(test_config_default_api);
    TEST_MAIN_RUN(test_propagate_set_api);
    TEST_MAIN_RUN(test_propagate_expr_api);
    TEST_MAIN_RUN(test_propagate_backward_api);

TEST_MAIN_END()
