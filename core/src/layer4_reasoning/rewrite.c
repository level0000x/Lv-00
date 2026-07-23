/**
 * @file rewrite.c
 * @brief 图重写引擎实现
 * @details 实现 VF2 子图同构匹配算法和 Weisfeiler-Lehman 图核哈希。
 *          支持图快照/回滚机制、规则热加载和 .lvz 规则文件解析。
 *          提供非重叠匹配和多步重写功能。
 *
 * 【重构计划】
 *   以下模块适合提取为独立文件，以降低本文件的复杂度：
 *   1. VF2 子图同构匹配引擎
 *      - 来源：vf2_match_recursive / vf2_feasible / vf2_lookahead / VF2State
 *      - 建议文件：src/vf2_matcher.c + src/vf2_matcher.h
 *      - 原因：VF2 是独立图算法，与重写规则引擎正交；
 *              提取后可与 constraint_graph 模块形成清晰的依赖关系
 *   2. WL 图核哈希（Weisfeiler-Lehman Graph Kernel）
 *      - 来源：wl_hash_graph / wl_hash_* 系列函数
 *      - 建议文件：src/wl_hash.c + src/wl_hash.h
 *      - 原因：图核哈希是通用图算法，不依赖重写语义
 *   3. 图快照/回滚（Graph Snapshot / Rollback）
 *      - 来源：snapshot_save / snapshot_restore / graph_snapshot_* 系列
 *      - 建议文件：src/graph_snapshot.c + src/graph_snapshot.h
 *      - 原因：快照机制与重写引擎逻辑独立，可复用于 undo/redo 场景
 *   4. .lvz 规则文件解析
 *      - 来源：parse_lvz_rule / parse_rule_file / lvz_rule_validate
 *      - 建议文件：src/lvz_rule_parser.c + src/lvz_rule_parser.h
 *      - 原因：解析器与引擎分离，便于单独测试和格式版本演进
 *
 *   注意：此计划仅记录结构优化方向，实际拆分需配合接口稳定性评估和
 *   回归测试，避免破坏现有的 VF2 -> 重写 -> 规范化流水线。
 *
 * @author Lv-00 Project
 * @version 3.0.1
 *
 * @dependencies
 *   - rewrite.h            : 图重写引擎公共接口定义
 *   - lv_internal.h      : 内部数据结构、常量、FNV 哈希基础
 *   - lv_utils.h         : 统一内存分配器
 *   - constraint_graph.h   : 约束图接口
 *   - normalization.h      : 图规范化引擎（规范化间步）
 *   - stream.h             : 流式事件输出
 */

#include "rewrite.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h" /* lv_malloc / lv_free —— 统一内存分配器 */
#include "normalization.h"
#include "stream.h"
#include "stream_context_util.h"

/* ==================== 命名常量 ==================== */

/** 哈希计算批次大小（用于 WL 图核哈希的增量计算） */
#define REWRITE_HASH_BATCH_SIZE 64

lv_DECLARE_STREAM_CTX(rewrite);

void rewrite_set_stream_context(StreamContext *ctx) {
    rewrite_stream_ctx = ctx;
}

/* ── 前向声明 ── */

/* ── 子模块已拆分至 rewrite/ ── */
