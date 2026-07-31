/**
 * @file proof_multi_strategy_internal.h
 * @brief 多策略证明引擎内部共享声明（proof_search_algo.c 与 proof_multi_strategy.c 共用）
 *
 * @details 从 proof_multi_strategy.c 拆分搜索算法段后，
 *          将跨文件搜索函数声明集中于此。
 */

#ifndef lv_PROOF_MULTI_STRATEGY_INTERNAL_H
#define lv_PROOF_MULTI_STRATEGY_INTERNAL_H

#include <stdbool.h>

#include "lv/proof.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  搜索算法（proof_search_algo.c 与 proof_multi_strategy.c 共享）
 * ================================================================ */
bool proof_depth_first_search(ProofNavigator *proof, int max_steps);
bool proof_breadth_first_search(ProofNavigator *proof, int max_steps);
bool proof_best_first_search(ProofNavigator *proof, int max_steps);
bool proof_mcts_search(ProofNavigator *proof, int max_steps);

#ifdef __cplusplus
}
#endif

/* ================================================================
 *  策略执行函数（proof_strategy_exec.c 与 proof_multi_strategy.c 共享）
 * ================================================================ */
bool execute_direct_construction(ProofMultiStrategy *mse, ProofNavigator *nav);
bool execute_area_method(ProofMultiStrategy *mse, ProofNavigator *nav);
bool execute_groebner_basis(ProofMultiStrategy *mse, ProofNavigator *nav);
bool execute_vector_method(ProofMultiStrategy *mse, ProofNavigator *nav);
bool execute_full_angle_method(ProofMultiStrategy *mse, ProofNavigator *nav);
bool execute_deductive_database(ProofMultiStrategy *mse, ProofNavigator *nav);
bool execute_coordinate(ProofMultiStrategy *mse, ProofNavigator *nav);
bool execute_hol_light(ProofMultiStrategy *mse, ProofNavigator *nav);
bool execute_oracle(ProofMultiStrategy *mse, ProofNavigator *nav);

#endif /* lv_PROOF_MULTI_STRATEGY_INTERNAL_H */
