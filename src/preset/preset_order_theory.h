/**
 * @file preset_order_theory.h
 * @brief 序理论预设函数块 - 头文件
 *
 * 提供理论数学研究项目Lv-00中序理论领域的预设函数块，包括：
 *   - 偏序与全序：偏序关系构造、全序判定
 *   - 格论：格的上确界（join/并）、格的下确界（meet/交）
 *   - 分解定理：链分解（Dilworth定理）、反链分解
 *   - 选择公理应用：Zorn引理应用、良序定理
 *   - 不动点理论：Tarski/Knaster不动点定理
 *   - Galois连接与完备化：Galois连接、完备化
 *
 * @module OrderTheory
 * @category PRESET_CATEGORY_LOGIC
 * @version 1.0.0
 * @author Lv-00 开发团队
 */

#ifndef PRESET_ORDER_THEORY_H
#define PRESET_ORDER_THEORY_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义
 * ============================================================ */

/* -------------------- 偏序与全序 -------------------- */

/** 偏序关系构造：给定集合和关系对，构造偏序集 (P, ≤) */
#define PRESET_ORDER_PARTIAL_ORDER_RELATION  "partial_order_relation"

/** 格的上确界（join/并）：a ∨ b */
#define PRESET_ORDER_LATTICE_JOIN            "lattice_join"

/** 格的下确界（meet/交）：a ∧ b */
#define PRESET_ORDER_LATTICE_MEET            "lattice_meet"

/** 链分解（Dilworth定理）：将偏序集分解为链的最小划分 */
#define PRESET_ORDER_CHAIN_DECOMPOSITION     "chain_decomposition"

/** Zorn引理应用：在偏序集中应用Zorn引理证明极大元存在 */
#define PRESET_ORDER_ZORN_LEMMA_APPLICATION  "zorn_lemma_application"

/** 不动点定理（Tarski/Knaster）：完备格上保序映射的不动点 */
#define PRESET_ORDER_FIXED_POINT_THEOREM     "fixed_point_theorem"

/** Galois连接：构造或判定两个偏序集之间的Galois连接 */
#define PRESET_ORDER_GALOIS_CONNECTION       "galois_connection"

/** 完备化：将偏序集嵌入到其Dedekind-MacNeille完备化中 */
#define PRESET_ORDER_COMPLETE_LATTICE_COMPLETION "complete_lattice_completion"

/* ============================================================
 * 模块注册函数
 * ============================================================ */

/**
 * @brief 注册所有序理论预设函数块
 *
 * 将序理论模块的全部8个预设函数块注册到全局预设库中。
 * 此函数由 preset_blocks_init() 自动调用。
 *
 * @return true 全部注册成功
 * @return false 部分注册失败
 */
bool preset_order_theory_register(void);

/**
 * @brief 获取序理论预设函数块数量
 *
 * @return int 序理论模块预设函数块总数（8）
 */
int preset_order_theory_count(void);

#ifdef __cplusplus
}
#endif

#endif /* PRESET_ORDER_THEORY_H */
