/**
 * @file proof_engine_enhanced_internal.h
 * @brief 增强证明引擎内部共享声明（proof_trace_tree.c 与 proof_engine_enhanced.c 共用）
 */

#ifndef lv_PROOF_ENGINE_ENHANCED_INTERNAL_H
#define lv_PROOF_ENGINE_ENHANCED_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lv/proof_engine_enhanced.h"

/** @brief 矛盾路径初始容量 */
#define CONTRADICTION_PATH_INITIAL_CAPACITY 32

#ifdef __cplusplus
extern "C" {
#endif

/* 内部辅助（proof_trace_tree.c 与 proof_engine_enhanced.c 共享） */
void safe_strncpy(char *dest, const char *src, size_t max_len);
int64_t get_time_ns(void);
bool trace_tree_register_node(lvProofTraceTree *tree, lvProofTraceNode *node);
void trace_tree_update_stats(lvProofTraceTree *tree);

#ifdef __cplusplus
}
#endif

#endif /* lv_PROOF_ENGINE_ENHANCED_INTERNAL_H */
