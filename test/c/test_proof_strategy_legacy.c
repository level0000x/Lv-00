/**
 * @file test_proof_strategy_legacy.c
 * @brief 经典策略桥接（proof_multi_strategy.c PROOF_STRATEGY_LEGACY_*）测试
 *
 * 覆盖 core/src/layer4_reasoning/proof_system/proof_multi_strategy.c：
 * - 默认状态：10 个 PROOF_STRATEGY_LEGACY_* 桥接策略 UNAVAILABLE，
 *   mse->legacy_proof_engine == NULL
 * - proof_multi_strategy_set_legacy_engine 挂载（lvProofEngine*）后
 *   全部桥接策略 AVAILABLE；卸载（NULL）后回退 UNAVAILABLE
 * - 挂载后 legacy 策略进入 try_all 可达路径（加入 fallback_order 后
 *   被尝试，缺导航器时安全失败返回 PROOF_STRATEGY_COUNT，不崩溃）
 *
 * 测试边界说明：
 * - lv_proof_engine_create(NULL) 使用默认配置即可构造完整实例
 *   （proof_engine.c：默认最大深度 50/分支 32/超时 30s），挂载是纯
 *   状态操作，不需要规则库。
 * - 未执行 legacy 桥接策略的完整证明（execute_legacy_bridge 需要
 *   nav->target_prop + nav->construction + 经典引擎规则库的完整
 *   证明上下文），本次守护到"挂载 -> 可用 -> try_all 可达且安全失败"。
 *
 * @version 1.0.0
 * @date 2026-08-06
 */

#include "lv.h" /* proof.h（ProofMultiStrategy / proof_multi_strategy_*）
                   + proof_engine_enhanced.h（lvProofEngine / lv_proof_engine_*） */
#include "test_helpers.h"

/* ============================================================
 * Global test counters
 * ============================================================ */
int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * Test: 默认未挂载 -> 全部 UNAVAILABLE
 * ============================================================ */
static void test_legacy_unavailable_by_default(void) {
    ProofMultiStrategy *mse = proof_multi_strategy_create(NULL);
    TEST_ASSERT_NOT_NULL(mse);

    TEST_ASSERT_NULL(mse->legacy_proof_engine);
    int all_unavailable = 1;
    for (ProofStrategyType t = PROOF_STRATEGY_LEGACY_DIRECT; t <= PROOF_STRATEGY_LEGACY_HYBRID; t = (ProofStrategyType)(t + 1)) {
        if (mse->strategies[t].status != PROOF_STRATEGY_UNAVAILABLE) {
            all_unavailable = 0;
            break;
        }
    }
    TEST_ASSERT_MSG(all_unavailable, "all legacy bridge strategies should be UNAVAILABLE by default");

    proof_multi_strategy_destroy(mse);
}

/* ============================================================
 * Test: 挂载 / 卸载循环
 * ============================================================ */
static void test_legacy_mount_cycle(void) {
    lvProofEngine *engine = lv_proof_engine_create(NULL);
    TEST_ASSERT_NOT_NULL(engine);

    ProofMultiStrategy *mse = proof_multi_strategy_create(NULL);
    TEST_ASSERT_NOT_NULL(mse);

    /* 挂载 -> AVAILABLE */
    proof_multi_strategy_set_legacy_engine(mse, engine);
    TEST_ASSERT_MSG(mse->legacy_proof_engine == engine, "legacy engine should be mounted");
    int all_available = 1;
    for (ProofStrategyType t = PROOF_STRATEGY_LEGACY_DIRECT; t <= PROOF_STRATEGY_LEGACY_HYBRID; t = (ProofStrategyType)(t + 1)) {
        if (mse->strategies[t].status != PROOF_STRATEGY_AVAILABLE) {
            all_available = 0;
            break;
        }
    }
    TEST_ASSERT_MSG(all_available, "all legacy bridge strategies should be AVAILABLE after mount");

    /* 卸载 -> UNAVAILABLE */
    proof_multi_strategy_set_legacy_engine(mse, NULL);
    TEST_ASSERT_NULL(mse->legacy_proof_engine);
    int all_unavailable = 1;
    for (ProofStrategyType t = PROOF_STRATEGY_LEGACY_DIRECT; t <= PROOF_STRATEGY_LEGACY_HYBRID; t = (ProofStrategyType)(t + 1)) {
        if (mse->strategies[t].status != PROOF_STRATEGY_UNAVAILABLE) {
            all_unavailable = 0;
            break;
        }
    }
    TEST_ASSERT_MSG(all_unavailable, "all legacy bridge strategies should be UNAVAILABLE after unmount");

    /* 再次挂载后销毁（引擎所有权归调用方，mse destroy 不释放引擎） */
    proof_multi_strategy_set_legacy_engine(mse, engine);
    TEST_ASSERT_MSG(mse->strategies[PROOF_STRATEGY_LEGACY_DIRECT].status == PROOF_STRATEGY_AVAILABLE,
                    "re-mount should restore AVAILABLE");

    proof_multi_strategy_destroy(mse);
    lv_proof_engine_destroy(engine);
    lv_proof_engine_destroy(NULL);
}

/* ============================================================
 * Test: 挂载后 legacy 策略 try_all 可达且安全失败
 * ============================================================ */
static void test_legacy_try_all_reachable(void) {
    lvProofEngine *engine = lv_proof_engine_create(NULL);
    TEST_ASSERT_NOT_NULL(engine);

    ProofMultiStrategy *mse = proof_multi_strategy_create(NULL);
    TEST_ASSERT_NOT_NULL(mse);

    /* 未挂载时 fallback 指向 legacy：UNAVAILABLE 被跳过，安全返回 */
    int order_unmounted[] = { PROOF_STRATEGY_LEGACY_DIRECT };
    proof_multi_strategy_set_fallback_order(mse, order_unmounted, 1);
    ProofStrategyType r1 = proof_multi_strategy_try_all(mse);
    TEST_ASSERT_EQ((int) r1, (int) PROOF_STRATEGY_COUNT);

    /* 挂载后：legacy 策略进入 try_all 可达路径（shared_navigator 为
     * NULL -> 桥接执行安全失败 -> 无崩溃，返回 PROOF_STRATEGY_COUNT） */
    proof_multi_strategy_set_legacy_engine(mse, engine);
    ProofStrategyType r2 = proof_multi_strategy_try_all(mse);
    TEST_ASSERT_EQ((int) r2, (int) PROOF_STRATEGY_COUNT);

    /* 卸载后恢复跳过语义 */
    proof_multi_strategy_set_legacy_engine(mse, NULL);
    ProofStrategyType r3 = proof_multi_strategy_try_all(mse);
    TEST_ASSERT_EQ((int) r3, (int) PROOF_STRATEGY_COUNT);

    proof_multi_strategy_destroy(mse);
    lv_proof_engine_destroy(engine);
}

/* ============================================================
 * Main
 * ============================================================ */
TEST_MAIN_BEGIN("ProofStrategyLegacyBridge")

    TEST_MAIN_RUN(test_legacy_unavailable_by_default);
    TEST_MAIN_RUN(test_legacy_mount_cycle);
    TEST_MAIN_RUN(test_legacy_try_all_reachable);

TEST_MAIN_END()
