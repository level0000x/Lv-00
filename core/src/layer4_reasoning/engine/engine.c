/**
 * @file engine.c
 * @brief 主引擎实现
 * @details 实现工作流编排，协调规范化、重写、求解和冲突检查。
 *          支持重写-求解协作、位电路跳闸处理和冻结点回滚。
 *          状态码映射已提取至 engine_status.c，状态转移已提取至 engine_state.c。
 *
 * ============================================================
 * 迁移状态：从全局状态到 lvContext（v3.3.0+）
 * ============================================================
 *
 * Lv-00 正在从"全局引擎模式"迁移到"隔离上下文模式"。迁移路线图：
 *
 *   第 1 阶段（已完成）：lvEngine 持有 lvContext* 指针，两者共存。
 *     上下文中已拥有独立的错误码、错误消息、递归深度追踪。
 *     LEGACY 线程局部变量（g_thread_last_status / g_thread_last_error）已移除。
 *
 *   第 2 阶段（进行中）：将引擎的核心逻辑逐步下沉到 lvContext。
 *     engine_solve()、engine_rewrite_and_solve() 等函数改用 context 参数。
 *     状态码映射和状态转移函数已提取到独立文件，减少主引擎文件的耦合度。
 *
 *   第 3 阶段（远期）：lvEngine 降级为 C API 的薄封装层，
 *     所有引擎逻辑由 lvContext 统一管理。
 *
 * 设计原则：
 *   1. 新代码必须通过 context 访问引擎状态，禁止新增全局/线程局部变量。
 *   2. 每个 context 实例是完全隔离的，支持并发、分支推理和资源熔断。
 * ============================================================
 *
 * ============================================================
 * 拆分子模块（Lv-00 v3.3.0+）
 * ============================================================
 *
 * 本文件已拆分为以下子模块：
 *   - engine_error.c    引擎错误管理（engine_set_error / 状态查询）
 *   - engine_lifecycle.c 引擎生命周期（engine_create / engine_destroy）
 *   - engine_resource.c  资源加载（规则/模块/公理包 + 步数限制）
 *   - engine_function.c  函数块操作（pack / instantiate / unify）
 *   - engine_solve.c     求解流水线（engine_solve / engine_rewrite_and_solve）
 *   - engine_circuit.c   位电路跳闸处理
 *   - engine_frozen.c    冻结点快照机制
 *   - engine_stream.c    流式输出 API
 *   - engine_state.c     五状态机实现（含 lv_engine_transition_state）
 *   - engine_status.c    状态码字符串映射
 */

#include "lv/engine.h"

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/bit_burning.h"
#include "lv/func_block.h"
#include "lv/lambda_to_graph.h"
#include "lv/lv_config.h"
#include "lv/normalization.h"
#include "lv/solver.h"
#include "lv/stream.h"
#include "lv/trust_color.h"

#include "lv/debug.h"
#include "engine_internal.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/node_deep_copy.h"
#include "lv/stream.h"
