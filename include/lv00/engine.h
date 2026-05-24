/**
 * @file engine.h
 * @brief Lv-00 主引擎 —— 工作流编排、模块/公理加载、重写与求解
 *
 * 提供引擎的创建/销毁、模块与公理包加载、函数打包与实例化、
 * 重写-求解协作流程、位电路跳闸处理以及冻结点快照回滚机制。
 */

#ifndef LV00_ENGINE_H
#define LV00_ENGINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "axiom_pkg.h"
#include "constraint_graph.h"
#include "func_block.h"
#include "module.h"
#include "normalization.h"
#include "rewrite.h"
#include "solver.h"
#include "stream.h"
#include "unify.h"

/* ── 引擎状态码（必须在 LV00Engine 结构体之前定义）── */
typedef enum {
    ENGINE_OK,
    ENGINE_OUT_OF_MEMORY,
    ENGINE_INVALID_STATE,
    ENGINE_INVALID_ARGUMENT,
    ENGINE_CONSTRAINT_CONFLICT,
    ENGINE_MODULE_ERROR
} EngineStatus;

typedef struct LV00Engine {
    ConstraintGraph *main_graph;
    Module **loaded_modules;
    int module_count;
    int module_capacity; /* 指数增长容量 */
    AxiomPackage **axiom_packages;
    int axiom_package_count;
    int axiom_package_capacity; /* 指数增长容量 */
    RewriteRule **rewrite_rules;
    int rewrite_rule_count;
    int rewrite_rule_capacity; /* 指数增长容量 */

    /* 可配置的重写步数上限（默认: 1000） */
    int rewrite_step_limit;

    /* 位电路跳闸回滚的冻结点快照（由引擎持有所有权） */
    void *frozen_point;

    /* 上一次合一操作的状态码 */
    int last_unify_status;

    /* ── 引擎级别的错误状态（每个引擎实例独立隔离）── */
    EngineStatus last_status; /* 最近一次操作的状态码 */
    char last_error[256];     /**< 最近一次操作的错误描述文本（固定 256 字节，长消息会被截断） */

    /* 流式输出上下文（可选，为 NULL 时不发射事件） */
    StreamContext *stream_ctx;
} LV00Engine;

LV00Engine *engine_create(void);
void engine_destroy(LV00Engine *engine);

bool engine_add_rewrite_rule(LV00Engine *engine, RewriteRule *rule);
ModuleLoadStatus engine_load_module(LV00Engine *engine, const char *filepath);
AxiomLoadStatus engine_load_axiom_package(LV00Engine *engine, const char *filepath);

bool engine_pack_function(LV00Engine *engine, int *internal_node_ids, int internal_count, int *input_port_ids,
                          int input_count, int *output_port_ids, int output_count, int *out_func_block_id);

int *engine_instantiate_function(LV00Engine *engine, int func_block_id, int *arg_mappings, int arg_count,
                                 int *out_result_count);

UnifyStatus engine_unify(LV00Engine *engine, ConstraintGraph *construction, ConstraintGraph *proposition);

typedef enum { ENGINE_SOLVE_OK, ENGINE_SOLVE_CONFLICT, ENGINE_SOLVE_TIMEOUT, ENGINE_SOLVE_ERROR } EngineSolveResult;

typedef enum {
    ENGINE_CIRCUIT_IGNORE,
    ENGINE_CIRCUIT_ROLLBACK,
    ENGINE_CIRCUIT_DOWNGRADE,
    ENGINE_CIRCUIT_ERROR
} EngineCircuitResult;

/** @brief 位电路跳闸时的用户动作 */
typedef enum {
    ENGINE_CIRCUIT_ACTION_IGNORE,   /**< 忽略，接受当前结果 */
    ENGINE_CIRCUIT_ACTION_ROLLBACK, /**< 回滚到冻结点快照 */
    ENGINE_CIRCUIT_ACTION_DOWNGRADE /**< 永久降级为 AMBER */
} EngineCircuitAction;

EngineStatus engine_get_last_status(const LV00Engine *engine);
/**
 * @brief 获取引擎最近一次错误的描述字符串
 *
 * @param[in] engine 引擎实例（当前未使用，可为 NULL）
 * @return 内部静态错误字符串指针。调用者不得 free。
 *         在下一次可能修改错误状态的操作前有效。
 *         如无错误，返回空字符串。
 */
const char *engine_get_last_error(const LV00Engine *engine);

/* ---- 工作流编排 ---- */

/* 完整求解流水线：重写 -> 求解器 -> 冲突检查 -> 自由度更新。
 * 成功返回 ENGINE_SOLVE_OK，冲突返回 ENGINE_SOLVE_CONFLICT，
 * 超时返回 ENGINE_SOLVE_TIMEOUT。 */
EngineSolveResult engine_solve(LV00Engine *engine);

/* 重写-求解工作流，实现协作协议：
 * 先重写 -> 遇停顿则求解 -> 冲突暴露。
 * 返回总执行步数，出错返回负值。 */
int engine_rewrite_and_solve(LV00Engine *engine, int max_rewrite_steps, int max_solve_steps);

/* 位电路跳闸处理器，用于 symbolic_coord 溢出事件。
 * 返回 ENGINE_CIRCUIT_IGNORE 表示已处理，ENGINE_CIRCUIT_ROLLBACK 表示需要回滚，
 * ENGINE_CIRCUIT_DOWNGRADE 表示建议降级。 */
EngineCircuitResult engine_handle_circuit_trip(LV00Engine *engine);

/* 带显式用户动作的电路跳闸处理。
 * action: EngineCircuitAction 枚举值之一。
 * 成功返回 ENGINE_CIRCUIT_IGNORE，回滚返回 ENGINE_CIRCUIT_ROLLBACK，
 * 降级返回 ENGINE_CIRCUIT_DOWNGRADE，错误返回 ENGINE_CIRCUIT_ERROR。 */
EngineCircuitResult engine_handle_circuit_trip_with_action(LV00Engine *engine, EngineCircuitAction action);

/* ---- 重写步数上限配置 ---- */

/**
 * @brief 设置引擎的重写步数上限。
 *
 * 该上限由 engine_solve() 和 engine_rewrite_and_solve() 使用，
 * 用于限制每次迭代中的重写步数。
 *
 * @param engine  引擎实例。
 * @param limit   最大重写步数（必须 > 0；默认为 1000）。
 */
void engine_set_rewrite_step_limit(LV00Engine *engine, int limit);

/**
 * @brief 获取当前重写步数上限。
 *
 * @param engine  引擎实例。
 * @return 当前重写步数上限（默认 1000）。
 */
int engine_get_rewrite_step_limit(const LV00Engine *engine);

/* ---- 冻结点快照机制 ---- */

/**
 * @brief 创建当前引擎状态的冻结点快照。
 *
 * 深拷贝约束图，用于位电路跳闸后的回滚。
 * 调用者拥有返回的快照所有权，最终须调用 engine_destroy_frozen_point() 释放。
 *
 * @param engine  引擎实例。
 * @return 指向快照的不透明指针，失败返回 NULL。
 */
void *engine_create_frozen_point(LV00Engine *engine);

/**
 * @brief 将引擎恢复到先前创建的冻结点。
 *
 * 用快照状态替换引擎的约束图。
 * 成功恢复后，快照已被消耗，不应再传递给 engine_destroy_frozen_point()。
 *
 * @param engine        引擎实例。
 * @param frozen_point  由 engine_create_frozen_point() 返回的快照。
 * @return true 成功，false 失败。
 */
bool engine_restore_frozen_point(LV00Engine *engine, void *frozen_point);

/**
 * @brief 销毁冻结点快照，释放其内存。
 *
 * @param frozen_point  要销毁的快照（可为 NULL）。
 */
void engine_destroy_frozen_point(void *frozen_point);

/* ---- 流式输出 API ---- */

/**
 * @brief 获取引擎的流式上下文
 *
 * 引擎创建时自动创建流式上下文。可通过 stream_register_callback()
 * 注册回调以接收实时事件。
 *
 * @param engine  引擎实例
 * @return 流式上下文指针，或 NULL（引擎为 NULL 时）
 */
StreamContext *engine_get_stream_context(const LV00Engine *engine);

/**
 * @brief 设置引擎的流式输出开关
 *
 * 启用后，引擎在执行 solve/rewrite/normalize 等操作时会持续
 * 发射流式事件。默认启用。
 *
 * @param engine   引擎实例
 * @param enabled  true 启用，false 禁用
 */
void engine_set_streaming_enabled(LV00Engine *engine, bool enabled);

/**
 * @brief 获取流式输出是否启用
 *
 * @param engine  引擎实例
 * @return true 启用，false 禁用或 engine 为 NULL
 */
bool engine_is_streaming_enabled(const LV00Engine *engine);

/**
 * @brief 发射引擎流式事件（便捷函数）
 *
 * 填充 StreamEvent 的通用字段后调用 stream_emit()。
 * 如果 engine->stream_ctx 为 NULL，则为空操作。
 *
 * @param engine       引擎实例
 * @param type         事件类型
 * @param description  描述文本
 * @param step_number  步骤编号
 * @param node_id      相关节点 ID（-1 表示无）
 * @param constraint_id 相关约束 ID（-1 表示无）
 */
void engine_emit_stream_event(LV00Engine *engine, StreamEventType type, const char *description, int step_number,
                              int node_id, int constraint_id);

#ifdef __cplusplus
}
#endif

#endif /* LV00_ENGINE_H */