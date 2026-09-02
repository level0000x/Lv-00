/**
 * @file test_blueprint_g4.c
 * @brief 蓝图 G4 组件契约测试
 *
 * 覆盖：约束元数据（lv_constraint_get_meta / type_from_name /
 * type_from_python_class）、符号常量池（lv_symbolic_coord_init/free_constants
 * + lv_SYM_*）、符号缓存（lv_cache_*）。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/constraint_graph.h"
#include "lv/symbolic_coord.h"
#include "lv/symbolic_cache.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 约束元数据 ============== */

static void test_constraint_meta(void) {
    /* get_meta：PARALLEL 元数据 */
    const lvConstraintMeta *m = lv_constraint_get_meta(PARALLEL);
    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_EQ((int) m->type, (int) PARALLEL);
    TEST_ASSERT(strcmp(m->name, "PARALLEL") == 0, "规范名");
    TEST_ASSERT(strcmp(m->alias, "parallel") == 0, "小写别名");
    TEST_ASSERT_EQ(m->min_participants, 2);
    /* ANGLE requires_parameters */
    const lvConstraintMeta *am = lv_constraint_get_meta(ANGLE);
    TEST_ASSERT_NOT_NULL(am);
    TEST_ASSERT(am->requires_parameters, "ANGLE 需参数");
    /* 越界 → NULL */
    TEST_ASSERT_NULL(lv_constraint_get_meta((ConstraintType) 99));

    /* from_name：规范名 / 小写 / 大小写不敏感 */
    TEST_ASSERT_EQ(lv_constraint_type_from_name("PARALLEL"), (int) PARALLEL);
    TEST_ASSERT_EQ(lv_constraint_type_from_name("parallel"), (int) PARALLEL);
    TEST_ASSERT_EQ(lv_constraint_type_from_name("Perpendicular"), (int) PERPENDICULAR);
    TEST_ASSERT_EQ(lv_constraint_type_from_name("no_such"), -1);
    TEST_ASSERT_EQ(lv_constraint_type_from_name(NULL), -1);

    /* from_python_class */
    TEST_ASSERT_EQ(lv_constraint_type_from_python_class("Parallel"), (int) PARALLEL);
    TEST_ASSERT_EQ(lv_constraint_type_from_python_class("ConstraintParallel"), (int) PARALLEL);
    TEST_ASSERT_EQ(lv_constraint_type_from_python_class("IncidenceConstraint"), (int) INCIDENCE);
    TEST_ASSERT_EQ(lv_constraint_type_from_python_class("NoSuch"), -1);
    TEST_ASSERT_EQ(lv_constraint_type_from_python_class(NULL), -1);
}

/* ============== 符号常量池 ============== */

static void test_symbolic_constants(void) {
    /* 默认未初始化 */
    TEST_ASSERT_NULL(lv_SYM_ZERO);
    TEST_ASSERT_NULL(lv_SYM_ONE);

    /* init：幂等 */
    lv_symbolic_coord_init_constants();
    lv_symbolic_coord_init_constants(); /* 再次调用安全 */

    TEST_ASSERT_NOT_NULL(lv_SYM_ZERO);
    TEST_ASSERT_NOT_NULL(lv_SYM_ONE);
    TEST_ASSERT_NOT_NULL(lv_SYM_TWO);
    TEST_ASSERT_NOT_NULL(lv_SYM_THREE);
    TEST_ASSERT_NOT_NULL(lv_SYM_HALF);
    TEST_ASSERT_NOT_NULL(lv_SYM_NEG_ONE);

    /* 值检查（double 近似） */
    TEST_ASSERT(symbolic_coord_to_double(lv_SYM_ZERO) == 0.0, "ZERO=0");
    TEST_ASSERT(symbolic_coord_to_double(lv_SYM_ONE) == 1.0, "ONE=1");
    TEST_ASSERT(symbolic_coord_to_double(lv_SYM_TWO) == 2.0, "TWO=2");
    TEST_ASSERT(symbolic_coord_to_double(lv_SYM_HALF) == 0.5, "HALF=0.5");
    TEST_ASSERT(symbolic_coord_to_double(lv_SYM_NEG_ONE) == -1.0, "NEG_ONE=-1");

    /* √2 / √3 / π：创建 + 值 */
    TEST_ASSERT(lv_SYM_SQRT2 != NULL, "SQRT2 创建");
    TEST_ASSERT(lv_SYM_SQRT3 != NULL, "SQRT3 创建");
    TEST_ASSERT(lv_SYM_PI != NULL, "PI 创建");
    double s2 = symbolic_coord_to_double(lv_SYM_SQRT2);
    double s3 = symbolic_coord_to_double(lv_SYM_SQRT3);
    double pi = symbolic_coord_to_double(lv_SYM_PI);
    TEST_ASSERT(s2 > 1.41 && s2 < 1.42, "SQRT2≈1.414");
    TEST_ASSERT(s3 > 1.73 && s3 < 1.74, "SQRT3≈1.732");
    TEST_ASSERT(pi > 3.14 && pi < 3.15, "PI≈3.141");

    /* free：置 NULL 后可重建 */
    lv_symbolic_coord_free_constants();
    TEST_ASSERT_NULL(lv_SYM_ZERO);
    lv_symbolic_coord_init_constants();
    TEST_ASSERT_NOT_NULL(lv_SYM_ONE);
    lv_symbolic_coord_free_constants();
    TEST_ASSERT_NULL(lv_SYM_PI);
}

/* ============== 符号缓存 ============== */

static void test_symbolic_cache(void) {
    lvSymbolicCache *cache = lv_cache_create(4);
    TEST_ASSERT_NOT_NULL(cache);
    lvSymbolicCache *defcache = lv_cache_create(0); /* capacity<=0 → 默认容量，仍有效 */
    TEST_ASSERT_NOT_NULL(defcache);
    lv_cache_destroy(defcache);

    SymbolicCoord *a = symbolic_coord_create_rational(2, 1);
    SymbolicCoord *b = symbolic_coord_create_rational(3, 1);
    SymbolicCoord *r1 = symbolic_coord_create_rational(5, 1);
    SymbolicCoord *r2 = symbolic_coord_create_rational(6, 1);
    const SymbolicCoord *inputs[2] = {a, b};

    /* 未命中 → 插入 → 命中 */
    TEST_ASSERT_NULL(lv_cache_lookup(cache, "add", inputs, 2));
    lv_cache_insert(cache, "add", inputs, 2, r1);
    SymbolicCoord *hit = lv_cache_lookup(cache, "add", inputs, 2);
    TEST_ASSERT(hit == r1, "命中返回同一结果");
    TEST_ASSERT_NULL(lv_cache_lookup(cache, "mul", inputs, 2));

    /* 命中率：1 命中 / 3 访问 = 1/3 */
    double hr = lv_cache_hit_rate(cache);
    TEST_ASSERT(hr > 0.3 && hr < 0.4, "命中率≈1/3");

    /* invalidate 全清 */
    lv_cache_invalidate(cache);
    TEST_ASSERT_NULL(lv_cache_lookup(cache, "add", inputs, 2));

    /* by_node */
    lv_cache_insert_for_node(cache, "op1", inputs, 2, 7, r2);
    TEST_ASSERT_NOT_NULL(lv_cache_lookup(cache, "op1", inputs, 2));
    lv_cache_invalidate_by_node(cache, 7);
    TEST_ASSERT_NULL(lv_cache_lookup(cache, "op1", inputs, 2));

    /* 淘汰：容量 4 插 5 个不同键（r2 已被 by_node 释放——新建） */
    SymbolicCoord *c0 = symbolic_coord_create_rational(1, 1);
    SymbolicCoord *c1 = symbolic_coord_create_rational(2, 1);
    SymbolicCoord *c2 = symbolic_coord_create_rational(3, 1);
    SymbolicCoord *c3 = symbolic_coord_create_rational(4, 1);
    SymbolicCoord *c4 = symbolic_coord_create_rational(5, 1);
    const SymbolicCoord *empty[1] = {NULL};
    lv_cache_insert(cache, "k0", empty, 0, c0);
    lv_cache_insert(cache, "k1", empty, 0, c1);
    lv_cache_insert(cache, "k2", empty, 0, c2);
    lv_cache_insert(cache, "k3", empty, 0, c3);
    /* 访问 k0 提高其计数，然后插 k4 淘汰最冷（k1） */
    lv_cache_lookup(cache, "k0", empty, 0);
    lv_cache_insert(cache, "k4", empty, 0, c4);
    TEST_ASSERT(lv_cache_lookup(cache, "k0", empty, 0) != NULL, "k0 仍在");
    TEST_ASSERT(lv_cache_lookup(cache, "k1", empty, 0) == NULL, "k1 被淘汰");

    lv_cache_destroy(cache);
    lv_cache_destroy(NULL); /* NULL 安全 */

    symbolic_coord_destroy(a);
    symbolic_coord_destroy(b);
}

/* ============== 入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Blueprint G4 Test Suite")
    printf("=== Lv-00 Blueprint G4 Test Suite ===\n\n");
    lv_init();
    TEST_MAIN_RUN(test_constraint_meta);
    TEST_MAIN_RUN(test_symbolic_constants);
    TEST_MAIN_RUN(test_symbolic_cache);
    lv_cleanup();
TEST_MAIN_END()
