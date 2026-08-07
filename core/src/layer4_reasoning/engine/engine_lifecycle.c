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
#include "lv/lv_lifecycle.h"
#include "lv/stream.h"

#include "debug.h"
#include "engine_internal.h"
#include "engine_scheduler.h"
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

/* ── engine_destroy 子资源销毁适配（签名统一为 void (*)(void *)） ── */

static void destroy_engine_frozen_point(void *obj) {
    engine_destroy_frozen_point(obj);
}

static void destroy_engine_scheduler(void *obj) {
    scheduler_destroy((EngineScheduler *) obj);
}

static void destroy_engine_module(void *obj) {
    module_destroy((Module *) obj);
}

static void destroy_engine_axiom_package(void *obj) {
    axiom_package_destroy((AxiomPackage *) obj);
}

static void destroy_engine_rewrite_rule(void *obj) {
    rewrite_rule_destroy((RewriteRule *) obj);
}

static void destroy_engine_main_graph(void *obj) {
    graph_destroy((ConstraintGraph *) obj);
}

/* 流式上下文需先清除所有已注册模块的全局指针（stream_context_clear_all），
 * 再销毁流式上下文，防止 type_system_stream_ctx、rewrite_stream_ctx 等
 * 全局变量成为悬挂指针，导致后续操作堆损坏 */
static void destroy_engine_stream_ctx(void *obj, void *field_ptr) {
    lvEngine *engine = (lvEngine *) obj;
    (void) field_ptr;
    if (engine->stream_ctx) {
        stream_context_clear_all();
        stream_context_destroy(engine->stream_ctx);
        engine->stream_ctx = NULL;
    }
}

/* engine_destroy 字段描述表：释放顺序与原实现一致
 * （冻结点 → 调度器 → 流式上下文 → 主图 → 模块/公理包/重写规则数组 → 外壳），
 * 全部置 NULL 安全 */
static const lvFieldDesc s_engine_destroy_fields[] = {
    lv_FIELD_OBJECT(lvEngine, frozen_point, destroy_engine_frozen_point),
    lv_FIELD_OBJECT(lvEngine, scheduler, destroy_engine_scheduler),
    lv_FIELD_CUSTOM(lvEngine, stream_ctx, destroy_engine_stream_ctx),
    lv_FIELD_OBJECT(lvEngine, main_graph, destroy_engine_main_graph),
    lv_FIELD_ARRAY(lvEngine, loaded_modules, module_count, destroy_engine_module),
    lv_FIELD_ARRAY(lvEngine, axiom_packages, axiom_package_count, destroy_engine_axiom_package),
    lv_FIELD_ARRAY(lvEngine, rewrite_rules, rewrite_rule_count, destroy_engine_rewrite_rule),
};

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
     * 调用者应先等待推理完成，或通过 lv_engine_transition_state() 将状态转回 IDLE。
     */
    if (engine->state == ENGINE_STATE_REASONING) {
        engine_set_error(engine, ENGINE_STATUS_INVALID_STATE,
                         "engine_destroy: 引擎处于 REASONING 状态，无法安全销毁。"
                         "请先等待推理完成，或调用 lv_engine_transition_state() 将状态转回 IDLE。");
        return;
    }

    /* 标记引擎为销毁中状态，防止并发操作 */
    engine->state = ENGINE_STATE_IDLE;

    /* 解除旧版调度 API 对本线程 TLS 引擎指针的关联，防止销毁后 UAF */
    lv_engine_scheduler_shutdown(engine);

    /* 按字段描述表统一销毁全部子资源（顺序与原实现一致，全部置 NULL 安全） */
    lv_obj_destroy_fields(engine, s_engine_destroy_fields,
                          sizeof(s_engine_destroy_fields) / sizeof(s_engine_destroy_fields[0]));
    lv_free((void **) &engine);
}
