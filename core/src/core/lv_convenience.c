/**
 * @file lv_convenience.c
 * @brief Lv-00 高层便捷 API 包装层
 *
 * @details 实现 API_REFERENCE.md 中定义的高层便捷函数，
 *          这些函数是底层 API 的薄封装，提供更简洁的使用接口。
 *          内部调用已有的 engine、context、preset 系统函数。
 *
 * 提供的便捷 API：
 *   - lv_prove():          执行证明
 *   - lv_preset_load():    加载预设
 *   - lv_preset_unload():  卸载预设
 *   - lv_preset_apply():   应用预设
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "lv/lv_convenience.h" /* 本文件接口头：lv_prove / lv_preset_load 等便捷 API 声明 */
#include "lv/constraint_graph.h"
#include "lv/context.h"
#include "lv/dsl_compiler.h"
#include "lv/engine.h"
#include "lv/lv_check.h"
#include "lv/stream.h"

#include "lv/func_block_preset.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/lv_str_utils.h"
#include "lv/preset_core.h"

/* ── 错误码兼容：未在 error_codes.h 中定义的便捷 API 专用码 ── */
#ifndef lv_ERROR_PROOF_FAILED
#define lv_ERROR_PROOF_FAILED lv_ERROR_PROOF_INVALID
#endif
#ifndef lv_ERROR_MODULE_ERROR
#define lv_ERROR_MODULE_ERROR lv_ERROR_INTERNAL
#endif

/* ── 预设系统前向声明（定义在 preset_manager.c） ── */
typedef struct InternalPresetEntry *PresetEntryHandle;
bool preset_library_is_initialized(void);
PresetEntryHandle preset_find(const char *name);
void preset_release(PresetEntryHandle entry);
const PresetMetadata *preset_get_metadata(PresetEntryHandle entry);

/* ============================================================
 * 内部辅助：状态转移安全检查
 * ============================================================ */

/**
 * @brief 安全地将上下文转入指定状态
 *
 * 如果转移合法则执行并返回 true，否则设置错误信息并返回 false。
 *
 * @param ctx       上下文指针（非 NULL）
 * @param new_state 目标状态
 * @param op_name   操作名称（用于错误消息）
 * @return true 转移成功，false 转移失败
 */
static bool safe_transition(lvContext *ctx, lvContextState new_state, const char *op_name) {
    if (!ctx)
        return false;

    lvErrorCode ec = lv_context_set_state(ctx, new_state);
    if (ec != lv_OK) {
        /* 转移失败时设置上下文错误信息 */
        ctx->error_code = ec;
        lv_snprintf(ctx->error_message, sizeof(ctx->error_message), "%s: 状态转移失败（当前状态 %s -> 目标状态 %s）",
                 op_name, lv_context_state_name(ctx->state), lv_context_state_name(new_state));
        return false;
    }
    return true;
}

/* ============================================================
 * 第一部分：证明执行 API
 * ============================================================ */

/**
 * @brief 执行证明 -- 高层便捷接口
 *
 * 封装完整的证明工作流：
 *   1. 参数校验
 *   2. 转入 PARSING 状态
 *   3. 将 goal 字符串解析为内部约束（委托给解析器）
 *   4. 转入 REASONING 状态
 *   5. 调用引擎的重写-求解流水线
 *   6. 转入 COMPLETE 或 ERROR 状态
 *
 * @param ctx   上下文指针（非 NULL，必须处于 IDLE 或 COMPLETE 状态）
 * @param goal  证明目标的文本描述（如 "triangle ABC is equilateral"）
 *              如果为 NULL，使用上下文中已有的约束图进行证明
 * @return 0    证明成功
 * @return -1   参数无效（ctx 为 NULL）
 * @return -2   上下文状态不合法（不在 IDLE/COMPLETE 状态）
 * @return -3   解析阶段失败
 * @return -4   推理阶段失败（矛盾、超时或熔断）
 */
int lv_prove(lvContext *ctx, const char *goal) {
    /* ---- 参数校验 ---- */
    lv_CHECK_NOT_NULL(ctx);

    /* 只有 IDLE 和 COMPLETE 状态允许开始新的证明 */
    if (ctx->state != lv_CONTEXT_IDLE && ctx->state != lv_CONTEXT_COMPLETE) {
        ctx->error_code = lv_ERROR_INVALID_STATE;
        lv_snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "lv_prove: 当前状态 %s 不允许开始新证明（需要 IDLE 或 COMPLETE）", lv_context_state_name(ctx->state));
        return -2;
    }

    /* 如果从 COMPLETE 重入，先重置上下文 */
    if (ctx->state == lv_CONTEXT_COMPLETE) {
        lv_context_reset(ctx);
    }

    /* 确保上下文中存在主约束图：新建的 lvContext 在首次 lv_context_reset()
     * 之前 main_graph 为 NULL，缺图会导致后续解析和引擎推理无法进行。 */
    if (ctx->main_graph == NULL) {
        ctx->main_graph = graph_create();
        if (!ctx->main_graph) {
            ctx->error_code = lv_ERROR_OUT_OF_MEMORY;
            lv_snprintf(ctx->error_message, sizeof(ctx->error_message), "lv_prove: 创建约束图失败");
            return -3;
        }
    }

    /* ---- 解析阶段 ---- */
    if (!safe_transition(ctx, lv_CONTEXT_PARSING, "lv_prove")) {
        return -2;
    }

    /* goal 非空时，解析目标字符串为约束图 */
    if (lv_str_nonempty(goal)) {
        DslCompileConfig cfg;
        dsl_compile_config_default(&cfg);
        if (!dsl_compile_and_load(goal, &cfg, ctx->main_graph)) {
            ctx->error_code = lv_ERROR_PARSE;
            lv_RETURN_ERROR(lv_ERROR_PARSE, "parse failed in lv_prove");
        }
    }

    /* ---- 推理阶段 ---- */
    if (!safe_transition(ctx, lv_CONTEXT_REASONING, "lv_prove")) {
        /* 解析到推理的转移失败，标记为解析错误 */
        ctx->state = lv_CONTEXT_ERROR;
        return -3;
    }

    /* 调用引擎重写-求解流水线。
     * 这里使用重写-求解协作模式：先重写简化约束，再调用求解器。
     * max_rewrite_steps=1000, max_solve_steps=1000 为默认值。
     *
     * 引擎实例在此本地构造：engine_create() 会自建一个空白的 main_graph，
     * 这里将其释放并替换为 ctx->main_graph，使引擎直接作用于上下文中
     * 已解析好的约束图。引擎不拥有该图的所有权，因此求解结束后先把
     * engine->main_graph 置回 NULL，再调用 engine_destroy()，避免引擎
     * 销毁时释放本应属于上下文的约束图（双重释放）。 */
    lvEngine *engine = engine_create();
    if (!engine) {
        ctx->state = lv_CONTEXT_ERROR;
        ctx->error_code = lv_ERROR_OUT_OF_MEMORY;
        lv_snprintf(ctx->error_message, sizeof(ctx->error_message), "lv_prove: 创建引擎失败");
        return -4;
    }

    graph_destroy(engine->main_graph);
    engine->main_graph = ctx->main_graph;

    int steps = engine_rewrite_and_solve(engine, 1000, 1000);

    /* 解除引擎对上下文约束图的引用后再销毁引擎 */
    engine->main_graph = NULL;
    engine_destroy(engine);

    /* ---- 结果判定 ---- */
    if (steps >= 0) {
        /* 证明成功 */
        ctx->state = lv_CONTEXT_COMPLETE;
        ctx->error_code = lv_OK;
        ctx->error_message[0] = '\0';
        ctx->problems_processed++;
        return 0;
    } else {
        /* 证明失败：矛盾、超时或资源耗尽 */
        ctx->state = lv_CONTEXT_ERROR;
        ctx->error_code = lv_ERROR_PROOF_FAILED;
        lv_snprintf(ctx->error_message, sizeof(ctx->error_message), "lv_prove: 证明失败（重写-求解流水线返回 %d）", steps);
        return -4;
    }
}

/* ============================================================
 * 第二部分：预设管理 API
 * ============================================================ */

/**
 * @brief 加载预设 -- 将指定名称的预设注册到上下文
 *
 * 从预设库中查找指定名称的预设，如果找到则将其标记为已加载。
 * 预设的模板函数块不会立即实例化，仅记录加载状态。
 *
 * @param ctx  上下文指针（非 NULL）
 * @param name 预设名称（如 "midpoint", "circumcenter", "rotate" 等）
 * @return 0   加载成功
 * @return -1  参数无效（ctx 或 name 为 NULL）
 * @return -2  预设库未初始化
 * @return -3  指定名称的预设不存在
 * @return -4  内存分配失败
 */
int lv_preset_load(lvContext *ctx, const char *name) {
    /* ---- 参数校验 ---- */
    lv_CHECK_NOT_NULL(ctx);
    lv_CHECK_ARG(name && name[0] != '\0', lv_ERROR_NULL_POINTER, "NULL or empty name in lv_preset_load");

    /* ---- 检查预设库状态 ---- */
    if (!preset_library_is_initialized()) {
        ctx->error_code = lv_ERROR_MODULE_ERROR;
        lv_snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "lv_preset_load: 预设库未初始化，请先调用 preset_library_init()");
        return -2;
    }

    /* ---- 查找预设 ---- */
    PresetEntryHandle entry = preset_find(name);
    if (!entry) {
        ctx->error_code = lv_ERROR_NOT_FOUND;
        lv_snprintf(ctx->error_message, sizeof(ctx->error_message), "lv_preset_load: 预设 '%s' 不存在", name);
        return -3;
    }

    /* ---- 注册到上下文的模块引用 ----
     * 将预设条目作为模块引用记录在上下文中，
     * 方便后续 lv_preset_apply() 使用。
     * module_refs 为动态数组：写入前按需扩容（参照 dsl_compiler_load.c 的做法）。 */
    {
        /* 倍增扩容：委托 lv_ensure_capacity（初始 8，此后每次倍增；失败语义与原来一致：返回 -4） */
        if (!lv_ensure_capacity((void **) &ctx->module_refs, ctx->module_ref_count, &ctx->module_ref_capacity,
                                sizeof(void *), 1)) {
            preset_release(entry);
            ctx->error_code = lv_ERROR_OUT_OF_MEMORY;
            lv_snprintf(ctx->error_message, sizeof(ctx->error_message), "lv_preset_load: 模块引用数组扩容失败");
            return -4;
        }
    }
    ctx->module_refs[ctx->module_ref_count] = (void *) entry;
    ctx->module_ref_count++;

    ctx->error_code = lv_OK;
    ctx->error_message[0] = '\0';
    return 0;
}

/**
 * @brief 卸载预设 -- 从上下文中移除指定预设的加载标记
 *
 * 查找并移除上下文模块引用列表中的指定预设。
 * 不影响预设库中预设本身的注册状态。
 *
 * @param ctx  上下文指针（非 NULL）
 * @param name 预设名称
 * @return 0   卸载成功
 * @return -1  参数无效（ctx 或 name 为 NULL）
 * @return -3  上下文中未找到该预设的加载记录
 */
int lv_preset_unload(lvContext *ctx, const char *name) {
    /* ---- 参数校验 ---- */
    lv_CHECK_NOT_NULL(ctx);
    lv_CHECK_ARG(name && name[0] != '\0', lv_ERROR_NULL_POINTER, "NULL or empty name in lv_preset_unload");

    /* ---- 在模块引用列表中查找匹配的预设 ---- */
    int found_idx = -1;
    for (int i = 0; i < ctx->module_ref_count; i++) {
        PresetEntryHandle entry = (PresetEntryHandle) ctx->module_refs[i];
        if (entry) {
            const PresetMetadata *meta = preset_get_metadata(entry);
            if (meta && meta->name && lv_str_eq(meta->name, name)) {
                found_idx = i;
                break;
            }
        }
    }

    if (found_idx < 0) {
        ctx->error_code = lv_ERROR_NOT_FOUND;
        lv_snprintf(ctx->error_message, sizeof(ctx->error_message), "lv_preset_unload: 上下文中未加载预设 '%s'", name);
        return -3;
    }

    /* ---- 移除并释放引用 ---- */
    PresetEntryHandle entry = (PresetEntryHandle) ctx->module_refs[found_idx];
    preset_release(entry);

    /* 将最后一个元素移到空位，保持紧凑排列 */
    ctx->module_refs[found_idx] = ctx->module_refs[ctx->module_ref_count - 1];
    ctx->module_refs[ctx->module_ref_count - 1] = NULL;
    ctx->module_ref_count--;

    ctx->error_code = lv_OK;
    ctx->error_message[0] = '\0';
    return 0;
}

/**
 * @brief 应用预设 -- 将指定预设实例化并应用到当前约束图
 *
 * 从上下文的已加载预设中查找目标预设，实例化其函数块模板，
 * 并将产生的节点和约束合并到上下文的约束图中。
 *
 * @param ctx  上下文指针（非 NULL，应处于 IDLE 或 PARSING 状态）
 * @param name 预设名称（必须已通过 lv_preset_load 加载）
 * @return 0   应用成功
 * @return -1  参数无效（ctx 或 name 为 NULL）
 * @return -2  上下文状态不允许应用预设
 * @return -3  指定预设未加载
 * @return -4  实例化失败
 */
int lv_preset_apply(lvContext *ctx, const char *name) {
    /* ---- 参数校验 ---- */
    lv_CHECK_NOT_NULL(ctx);
    lv_CHECK_ARG(name && name[0] != '\0', lv_ERROR_NULL_POINTER, "NULL or empty name in lv_preset_apply");

    /* 仅允许在 IDLE 或 PARSING 状态下应用预设 */
    if (ctx->state != lv_CONTEXT_IDLE && ctx->state != lv_CONTEXT_PARSING) {
        ctx->error_code = lv_ERROR_INVALID_STATE;
        lv_snprintf(ctx->error_message, sizeof(ctx->error_message), "lv_preset_apply: 当前状态 %s 不允许应用预设",
                 lv_context_state_name(ctx->state));
        return -2;
    }

    /* ---- 在已加载列表中查找 ---- */
    PresetEntryHandle target = NULL;
    for (int i = 0; i < ctx->module_ref_count; i++) {
        PresetEntryHandle entry = (PresetEntryHandle) ctx->module_refs[i];
        if (entry) {
            const PresetMetadata *meta = preset_get_metadata(entry);
            if (meta && meta->name && lv_str_eq(meta->name, name)) {
                target = entry;
                break;
            }
        }
    }

    if (!target) {
        ctx->error_code = lv_ERROR_NOT_FOUND;
        lv_snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "lv_preset_apply: 预设 '%s' 未加载，请先调用 lv_preset_load()", name);
        return -3;
    }

    /* ---- 获取预设元数据 ---- */
    const PresetMetadata *meta = preset_get_metadata(target);
    if (!meta) {
        ctx->error_code = lv_ERROR_INTERNAL;
        lv_snprintf(ctx->error_message, sizeof(ctx->error_message), "lv_preset_apply: 无法获取预设 '%s' 的元数据", name);
        return -4;
    }

    /* ---- 自动转入 PARSING 状态（如果当前是 IDLE） ---- */
    if (ctx->state == lv_CONTEXT_IDLE) {
        if (!safe_transition(ctx, lv_CONTEXT_PARSING, "lv_preset_apply")) {
            return -2;
        }
    }

    /* ---- 实例化预设并应用到约束图 ---- */
    FuncBlock *fb = NULL;
    InstantiateResult ir = func_block_preset_instantiate(meta->name, NULL, 0, ctx->main_graph, &fb);

    if (ir != INSTANTIATE_OK) {
        ctx->error_code = lv_ERROR_INTERNAL;
        lv_snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "lv_preset_apply: 预设 '%s' 实例化失败 (result=%d)", name, (int) ir);
        return -4;
    }

    /* 将实例化的输出记录到流（如果可用） */
    if (fb) {
        struct StreamContext *sc = lv_context_get_stream(ctx);
        if (sc) {
            int fb_id = 0;
            if (ctx->main_graph && ctx->main_graph->node_count > 0) {
                GeomNode *last = ctx->main_graph->nodes[ctx->main_graph->node_count - 1];
                if (last)
                    fb_id = last->id;
            }
            stream_emit_preset_instantiate(sc, name, fb_id, ctx->rewrite_step_limit);
        }
    }

    ctx->error_code = lv_OK;
    ctx->error_message[0] = '\0';
    return 0;
}
