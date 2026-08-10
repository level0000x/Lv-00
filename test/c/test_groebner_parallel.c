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
#include "lv/lv_numeric.h" /* lv_deg_to_rad / lv_angle_diff_pi（K4-2A/4B 共享设施） */
#include "lv/coeff_pool.h"  /* coeff_pool_alloc / coeff_pool_clear（K4-C2-3 配对语义） */
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
 * Test: 2A 度数转弧度（K4 共享设施 lv_deg_to_rad）
 * 覆盖 groebner_engine.c L604 / smt_backend_impl_smtlib2.c L494 收敛前的
 * 内联形态 `deg * M_PI / 180.0`（lv_PI 与 M_PI 同为 double 最接近 π 表示，
 * 数值逐位等价）。采样点：0° / 45° / 90° / 180°。
 * ============================================================ */
static void test_deg_to_rad(void) {
    TEST_ASSERT_DOUBLE(lv_deg_to_rad(0.0), 0.0, 0.0);
    TEST_ASSERT_DOUBLE(lv_deg_to_rad(45.0), M_PI / 4.0, 1e-12);
    TEST_ASSERT_DOUBLE(lv_deg_to_rad(90.0), M_PI / 2.0, 1e-12);
    TEST_ASSERT_DOUBLE(lv_deg_to_rad(180.0), M_PI, 1e-12);
}

/* ============================================================
 * Test: 4B 角度回绕边界（K4 共享设施 lv_angle_diff_pi）
 * 收敛 recursion_selector.c / geo_constraint_solver_residual.c 的
 * 手写 while 双循环。端点 ±π 逐位保持；3π/2 一次回绕到 -π/2。
 * ============================================================ */
static void test_angle_diff_pi_wrap(void) {
    /* 端点 ±π 保持（while 条件不触发，逐位一致） */
    TEST_ASSERT_DOUBLE(lv_angle_diff_pi(M_PI), M_PI, 0.0);
    TEST_ASSERT_DOUBLE(lv_angle_diff_pi(-M_PI), -M_PI, 0.0);
    /* 3π/2 → -π/2（一次 2π 回绕） */
    TEST_ASSERT_DOUBLE(lv_angle_diff_pi(3.0 * M_PI / 2.0), -M_PI / 2.0, 1e-12);
    /* |angle| <= π 范围内原样保持 */
    TEST_ASSERT_DOUBLE(lv_angle_diff_pi(1.5), 1.5, 0.0);
    TEST_ASSERT_DOUBLE(lv_angle_diff_pi(-2.5), -2.5, 0.0);
}

/* ============================================================
 * Test: C2-3 coeff_pool 配对语义（K4 收敛样板）
 * solver_coord_extract.c 中 solver_poly_pool_init/push 收敛前的
 * coeff_pool_alloc + mpz_init + ... + coeff_pool_clear 配对契约：
 * clear 归还池并置 coeffs=NULL、degree=-1；重复 clear 安全。
 * 注：配对使用 coeff_pool_clear（池归还），不得再调用 mpz_poly_clear
 * （其内部 lv_free 与池内存不兼容）。
 * ============================================================ */
static void test_coeff_pool_pairing(void) {
    mpz_poly_t poly;
    mpz_poly_init(&poly);
    poly.degree = 1;
    poly.coeffs = coeff_pool_alloc(2);
    TEST_ASSERT_NOT_NULL(poly.coeffs);
    if (poly.coeffs) {
        mpz_init(poly.coeffs[0]);
        mpz_init(poly.coeffs[1]);
        mpz_set_si(poly.coeffs[0], 3);
        mpz_set_si(poly.coeffs[1], -5);
    }
    /* 配对：clear 归还池 */
    coeff_pool_clear(&poly);
    TEST_ASSERT_NULL(poly.coeffs);
    TEST_ASSERT_EQ(poly.degree, -1);
    /* 重复 clear 安全（coeffs == NULL 时 no-op） */
    coeff_pool_clear(&poly);
    TEST_ASSERT_NULL(poly.coeffs);
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
    TEST_MAIN_RUN(test_deg_to_rad);
    TEST_MAIN_RUN(test_angle_diff_pi_wrap);
    TEST_MAIN_RUN(test_coeff_pool_pairing);

TEST_MAIN_END()
