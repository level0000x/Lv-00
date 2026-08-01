/**
 * @file engine_lifecycle.c
 * @brief 引擎生命周期管理（从 engine.c 拆分）
 *
 * @details 负责 lvEngine 实例的创建与销毁。
 *          初始化五状态机、流式上下文、主图等核心组件，
 *          并注册/分发内置模块的流式上下文 setter。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/engine.h"

#include <stdio.h>
#include <string.h>

#include "lv/lv.h"
#include "lv/stream.h"

#include "debug.h"
#include "engine_internal.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "stream_context_util.h"

/**
 * @brief 创建并初始化 lv 引擎实例
 *
 * 分配引擎结构体内存，初始化五状态机（v3.3.0 形式化）、流式上下文、
 * 主图等核心组件，并注册/分发内置模块的流式上下文 setter。
 *
 * @return 新创建的引擎实例指针；若内存不足则返回 NULL，
 *         此时可通过 engine_get_last_status() 获取 ENGINE_STATUS_OUT_OF_MEMORY 错误码。
 */
lvEngine *engine_create(void) {
    lvEngine *engine = lv_calloc(1, sizeof(lvEngine));
    if (!engine) {
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "engine_create: calloc engine failed");
    }
    engine->rewrite_step_limit = lv_DEFAULT_REWRITE_STEP_LIMIT; /* 默认重写步数限制 */
    engine->frozen_point = NULL;
    engine->context = NULL;                       /* 迁移中：暂不绑定上下文，后续可通过 engine_bind_context() 设置 */
    engine->stream_ctx = stream_context_create(); /* 创建流式上下文 */
    if (!engine->stream_ctx) {
        lv_free((void **) &engine);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "engine_create: stream_ctx creation failed");
    }

    /* 初始化五状态机（v3.3.0 形式化） */
    engine->state = ENGINE_STATE_IDLE;
    engine->previous_state = ENGINE_STATE_IDLE;
    engine->state_transition_count = 0;

    /* 通过注册/分发机制将流式上下文同步到各子模块。
     * stream_context_register_builtins() 一次性注册所有内置模块的 setter，
     * stream_context_dispatch_all() 统一分发流式上下文到所有已注册模块。
     * 新增模块时只需在 stream_context_util.c 中添加注册行，
     * 无需修改此处的引擎初始化代码。
     *
     * 注意：这两个函数当前不返回错误码（void 返回类型）。
     * 如果未来重构为返回错误码，应在此处检查返回值并做相应错误处理。
     * 当前状态：调用后无法检测失败，仅记录日志以便排查问题。 */
    stream_context_register_builtins(engine->stream_ctx);
    /* 当前状态：内置模块 setter 注册完成，无法确认是否全部成功 */
    lv_LOG_WARNING("engine_create: stream_context_register_builtins() 已调用（void 返回，无法检测错误）");

    stream_context_dispatch_all(engine->stream_ctx);
    /* 当前状态：流式上下文分发完成，无法确认是否全部成功 */
    /* lv_LOG_WARNING("engine_create: stream_context_dispatch_all() 已调用（void 返回，无法检测错误）"); */

    engine->main_graph = graph_create();
    if (!engine->main_graph) {
        stream_context_destroy(engine->stream_ctx);
        lv_free((void **) &engine);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "engine_create: main_graph creation failed");
    }
    engine->last_status = ENGINE_STATUS_OK;
    engine->last_error[0] = '\0';
    return engine;
}

/**
 * @brief 销毁引擎实例并释放所有关联资源
 *
 * 依次释放冻结点快照、流式上下文、主图、已加载模块、
 * 公理包、重写规则及引擎结构体本身。
 * 传入 NULL 时安全返回，不做任何操作。
 *
 * @param engine 待销毁的引擎实例指针
 */
void engine_destroy(lvEngine *engine) {
    if (!engine)
        return;

    /*
     * 状态检查：如果引擎正处于 REASONING 状态，拒绝销毁。
     * 推理过程中销毁会导致约束图、重写规则等共享数据结构处于不一致状态，
     * 可能引发悬垂指针、内存损坏等严重问题。
     * 调用者应先等待推理完成或通过 engine_reset() 将状态置为 IDLE/ERROR。
     */
    if (engine->state == ENGINE_STATE_REASONING) {
        engine_set_error(engine, ENGINE_STATUS_INVALID_STATE,
                         "engine_destroy: 引擎处于 REASONING 状态，无法安全销毁。"
                         "请先等待推理完成或调用 engine_reset() 重置状态。");
        return;
    }

    /* 标记引擎为销毁中状态，防止并发操作 */
    engine->state = ENGINE_STATE_IDLE;

    if (engine->frozen_point) {
        engine_destroy_frozen_point(engine->frozen_point);
        engine->frozen_point = NULL;
    }
    if (engine->stream_ctx) {
        /* 在销毁流式上下文前，清除所有已注册模块的全局指针，
         * 防止 type_system_stream_ctx、rewrite_stream_ctx 等
         * 全局变量成为悬挂指针，导致后续操作堆损坏。 */
        stream_context_clear_all();
        stream_context_destroy(engine->stream_ctx);
        engine->stream_ctx = NULL;
    }
    if (engine->main_graph) {
        graph_destroy(engine->main_graph);
        engine->main_graph = NULL;
    }
    for (int i = 0; i < engine->module_count; i++) {
        module_destroy(engine->loaded_modules[i]);
    }
    lv_free((void **) &engine->loaded_modules);
    for (int i = 0; i < engine->axiom_package_count; i++) {
        axiom_package_destroy(engine->axiom_packages[i]);
    }
    lv_free((void **) &engine->axiom_packages);
    for (int i = 0; i < engine->rewrite_rule_count; i++) {
        rewrite_rule_destroy(engine->rewrite_rules[i]);
    }
    lv_free((void **) &engine->rewrite_rules);
    lv_free((void **) &engine);
}
