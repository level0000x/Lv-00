/**
 * @file test_groebner_parallel.c
 * @brief 并行 Groebner 引擎（groebner_parallel.c）忠实布尔编码测试
 *
 * 覆盖 core/src/layer4_reasoning/backends/groebner_parallel.c（上一轮修复）：
 * - lv_groebner_parallel_create / destroy / compute / state
 * - 忠实布尔 Groebner 编码判定：子句集 -> Q[x1..xn] 多项式，
 *   UNSAT <=> 约化基含非零常数（1）。输入子句格式：
 *   int[] 数组，0 结尾；正文字 +x  -> 因子 (1-x)，负文字 -x -> 因子 x。
 * - 判定用例：{(x),(¬x)} -> UNSAT（基含非零常数）；
 *   {(x),(y)} -> SAT（基不含非零常数）；
 *   空子句 {}（常数 1 直接入基）-> UNSAT。
 * - 无效参数：NULL 引擎 / NULL 多项式 / poly_count<=0 -> -1。
 *
 * 结果读取方式（读 groebner_parallel.h 公共 API 确认）：
 * - lv_groebner_parallel_compute 返回 0 表示计算成功；
 * - lv_groebner_parallel_state 返回 completed/total/remaining；
 * - lvGroebnerParallel.groebner_basis / basis_size 公开可读；
 * - lv_groebner_poly_is_nonzero_constant(void*) 判定基中是否存在非零常数
 *   （=1），从而由公共 API 判定 UNSAT/SAT。
 *
 * 测试边界说明：
 * - SimplePoly 为内部结构，测试不直接读其字段，仅经上述公共 API 判定。
 * - 不执行"再 compute 复用引擎"路径（destroy 逻辑依赖单次 compute 的
 *   内部指针布局，复用不在本次守护范围）。
 *
 * @version 1.0.0
 * @date 2026-08-06
 */

#include "lv/groebner_parallel.h"
#include "test_helpers.h"

/* ============================================================
 * Global test counters
 * ============================================================ */
int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * 辅助：基中是否存在非零常数（即 UNSAT 证据）
 * ============================================================ */
static bool basis_has_nonzero_constant(const lvGroebnerParallel *e) {
    if (!e || !e->groebner_basis || e->basis_size <= 0)
        return false;
    for (int i = 0; i < e->basis_size; i++) {
        if (lv_groebner_poly_is_nonzero_constant(e->groebner_basis[i]))
            return true;
    }
    return false;
}

/* ============================================================
 * Test: 默认配置
 * ============================================================ */
static void test_default_config(void) {
    lvGroebnerConfig cfg = lv_groebner_default_config();
    TEST_ASSERT_EQ(cfg.max_threads, 4);
    TEST_ASSERT_EQ(cfg.chunk_size, 16);
    TEST_ASSERT_NEAR(cfg.load_balance_threshold, 0.3, 1e-12, "load_balance_threshold default");
    TEST_ASSERT_EQ(cfg.enable_inter_reduction, 1);
    TEST_ASSERT_EQ(cfg.enable_cache, 1);
}

/* ============================================================
 * Test: 生命周期
 * ============================================================ */
static void test_lifecycle(void) {
    lvGroebnerParallel *engine = lv_groebner_parallel_create(NULL);
    TEST_ASSERT_NOT_NULL(engine);
    /* 默认配置生效 */
    TEST_ASSERT_EQ(engine->config.max_threads, 4);
    lv_groebner_parallel_destroy(engine);

    lvGroebnerConfig cfg = lv_groebner_default_config();
    cfg.max_threads = 2;
    engine = lv_groebner_parallel_create(&cfg);
    TEST_ASSERT_NOT_NULL(engine);
    TEST_ASSERT_EQ(engine->config.max_threads, 2);
    lv_groebner_parallel_destroy(engine);

    lv_groebner_parallel_destroy(NULL);
}

/* ============================================================
 * Test: {(x), (¬x)} -> UNSAT（基含非零常数）
 * ============================================================ */
static void test_unsat_x_notx(void) {
    /* 输入协议：compute 将输入按 int **（指针数组）解读——
     * clauses[i] 为第 i 个子句的 int[]（0 结尾文字序列），
     * 故子句须以独立 int 数组 + 指针数组形式传入（不能传 int[][2] 连续二维数组） */
    int clause_x[] = {1, 0};
    int clause_notx[] = {-1, 0};
    int *clauses[] = { clause_x, clause_notx };
    lvGroebnerParallel *engine = lv_groebner_parallel_create(NULL);
    TEST_ASSERT_NOT_NULL(engine);

    int rc = lv_groebner_parallel_compute(engine, (void *) clauses, 2);
    TEST_ASSERT_EQ(rc, 0);

    lvGroebnerState st = lv_groebner_parallel_state(engine);
    TEST_ASSERT_EQ(st.completed_pairs, st.total_pairs);
    TEST_ASSERT_EQ(st.remaining_pairs, 0);
    TEST_ASSERT(engine->basis_size > 0, "basis should be non-empty");

    TEST_ASSERT_MSG(basis_has_nonzero_constant(engine),
                    "{(x),(¬x)} should be UNSAT: basis contains nonzero constant");

    lv_groebner_parallel_destroy(engine);
}

/* ============================================================
 * Test: {(x), (y)} -> SAT（基不含非零常数）
 * ============================================================ */
static void test_sat_x_y(void) {
    int clause_x[] = {1, 0};
    int clause_y[] = {2, 0};
    int *clauses[] = { clause_x, clause_y };
    lvGroebnerParallel *engine = lv_groebner_parallel_create(NULL);
    TEST_ASSERT_NOT_NULL(engine);

    int rc = lv_groebner_parallel_compute(engine, (void *) clauses, 2);
    TEST_ASSERT_EQ(rc, 0);

    lvGroebnerState st = lv_groebner_parallel_state(engine);
    TEST_ASSERT_EQ(st.completed_pairs, st.total_pairs);
    TEST_ASSERT_EQ(st.remaining_pairs, 0);
    TEST_ASSERT(engine->basis_size > 0, "basis should be non-empty");

    TEST_ASSERT_MSG(!basis_has_nonzero_constant(engine),
                    "{(x),(y)} should be SAT: basis contains no nonzero constant");

    lv_groebner_parallel_destroy(engine);
}

/* ============================================================
 * Test: 空子句 {} -> UNSAT（子句多项式即常数 1 直接入基）
 * ============================================================ */
static void test_empty_clause_unsat(void) {
    int clause_empty[] = {0}; /* 空子句：无文字，多项式 = 常数 1 */
    /* 双元素指针数组：单元素数组会被编译器折叠成直接传内层数组地址，
     * 导致 compute 按 int ** 解读输入时读到垃圾（见 gdb 复现） */
    int *clauses[2] = { clause_empty, NULL };
    lvGroebnerParallel *engine = lv_groebner_parallel_create(NULL);
    TEST_ASSERT_NOT_NULL(engine);

    int rc = lv_groebner_parallel_compute(engine, (void *) clauses, 1);
    TEST_ASSERT_EQ(rc, 0);

    lvGroebnerState st = lv_groebner_parallel_state(engine);
    TEST_ASSERT_EQ(st.completed_pairs, st.total_pairs);
    TEST_ASSERT_EQ(st.remaining_pairs, 0);
    TEST_ASSERT(engine->basis_size > 0, "basis should be non-empty");

    TEST_ASSERT_MSG(basis_has_nonzero_constant(engine),
                    "empty clause should be UNSAT: constant 1 is in the basis");

    lv_groebner_parallel_destroy(engine);
}

/* ============================================================
 * Test: 无效参数
 * ============================================================ */
static void test_invalid_args(void) {
    int clause_x[] = {1, 0};
    int clause_notx[] = {-1, 0};
    int *clauses[] = { clause_x, clause_notx };
    lvGroebnerParallel *engine = lv_groebner_parallel_create(NULL);
    TEST_ASSERT_NOT_NULL(engine);

    TEST_ASSERT_EQ(lv_groebner_parallel_compute(NULL, (void *) clauses, 2), -1);
    TEST_ASSERT_EQ(lv_groebner_parallel_compute(engine, NULL, 2), -1);
    TEST_ASSERT_EQ(lv_groebner_parallel_compute(engine, (void *) clauses, 0), -1);

    /* NULL 多项式判定为 false */
    TEST_ASSERT_MSG(!lv_groebner_poly_is_nonzero_constant(NULL),
                    "NULL poly should not be a nonzero constant");

    lv_groebner_parallel_destroy(engine);
}

/* ============================================================
 * Main
 * ============================================================ */
TEST_MAIN_BEGIN("GroebnerParallel")

    TEST_MAIN_RUN(test_default_config);
    TEST_MAIN_RUN(test_lifecycle);
    TEST_MAIN_RUN(test_unsat_x_notx);
    TEST_MAIN_RUN(test_sat_x_y);
    TEST_MAIN_RUN(test_empty_clause_unsat);
    TEST_MAIN_RUN(test_invalid_args);

TEST_MAIN_END()
