/**
 * @file constraint_graph.c
 * @brief 约束图核心实现
 * @details 实现几何约束图的数据结构和操作，包括点、线段、区域、端口和函数块节点。
 *          支持 5 种约束类型（关联、中间、等距、角度、正交），
 *          提供哈希索引、冗余检测和冲突检测功能。
 *
 * 【错误系统迁移说明】
 * 本文件已完成从旧版 g_internal_error[256] 全局字符数组 + set_error()/clear_error()
 * 兼容层到统一错误系统（lv_set_error / lv_clear_error / lv_get_error）的迁移。
 * 所有错误报告均已直接调用 lv_set_error()，错误清除调用 lv_clear_error()。
 * 旧的双轨错误系统已被完全移除，不再保留兼容层。
 */

/* ============================================================================
 * 魔法数字常量定义
 * ============================================================================ */

/**
 * @brief 每个节点的最大邻接约束数量
 * @details 用于邻接表的内存分配，超过此限制的约束将被静默忽略并记录警告
 */
#define lv_ADJ_MAX_PER_NODE 256

/**
 * @brief 冲突检测中每个点的最大约束数量
 */
#define lv_POINT_CONSTRAINT_ARRAY_SIZE 64

/**
 * @brief 连接图邻接矩阵的列步长
 */
#define lv_MAX_CONN_ADJ_STRIDE 256

/**
 * @brief JSON 序列化缓冲区初始大小
 */
#define lv_JSON_BUFFER_INITIAL_SIZE 1024

/**
 * @brief 节点/约束描述字符串缓冲区大小
 */
#define lv_DESC_BUFFER_SIZE 128

#include "lv/constraint_graph.h"

#include <assert.h>
#include <gmp.h>
#include <math.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/config.h"  /* lv_ARRAY_GROWTH_FACTOR etc. */
#include "lv/context.h" /* v3.4.0: lvContext 用于统一错误系统 */
#include "lv/debug.h"
#include "lv/error_codes.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h" /* lv_malloc / lv_free —— 统一内存分配器 */
#include "lv/stream.h"
#include "lv/symbolic_coord.h" /* SymbolicCoord, TrustColor (brings rational.h) */

/** @brief graph 模块全局流式上下文定义（供所有 graph 子模块通过 constraint_graph.h 的 extern 引用） */
LV_STREAM_CTX_DEFINE(graph);

/* ── 子模块已拆分至 constraint_graph/ 子目录 ──
 *
 * graph_ensure_capacity / graph_alloc_node / graph_alloc_constraint
 * 已迁移至 constraint_graph/graph_node.c（canonical 版本），
 * 本文件仅保留 graph_set_stream_context。
 *
 * graph_node_index_insert / graph_constraint_index_insert 的前向声明
 * 亦不再需要，因为调用它们的函数已迁移。
 */
