/**
 * @file engine.c
 * @brief 主引擎实现
 * @details 实现工作流编排，协调规范化、重写、求解和冲突检查。
 *          支持重写-求解协作、位电路跳闸处理和冻结点回滚。
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine.h"
#include "lv00_internal.h"
#include "lv00_utils.h"
#include "prop_verifier.h"
#include "stream.h"
#include "stream_context_util.h"
#include "node_deep_copy.h"

/* 错误消息缓冲区大小（用于 last_error 数组） */
#define LV00_ERROR_MSG_SIZE 256

/**
 * 线程局部的引擎状态（作为回退使用）
 *
 * 【设计说明】last_status 和 last_error 同时存在于两个层级：
 *   - 线程局部变量（此处）：用于 engine_create 等无实例场景的回退
 *   - 实例字段（engine->last_status / engine->last_error）：每个引擎独立隔离
 *
 * 优先使用实例级别的错误状态，线程局部变量仅在没有引擎实例可用时
 * 作为回退方案（如 engine_create 失败，尚未创建引擎实例时）。
 */
static LV00_THREAD_LOCAL EngineStatus last_status = ENGINE_OK;
static LV00_THREAD_LOCAL char last_error[LV00_ERROR_MSG_SIZE] = {0};

LV00Engine *engine_create(void) {
    LV00Engine *engine = lv00_malloc(sizeof(LV00Engine));
    if (!engine) {
        last_status = ENGINE_OUT_OF_MEMORY;
        return NULL;
    }
    memset(engine, 0, sizeof(LV00Engine));
    engine->rewrite_step_limit = LV00_DEFAULT_REWRITE_STEP_LIMIT;  /* 默认重写步数限制 */
    engine->frozen_point = NULL;
    engine->stream_ctx = stream_context_create(); /* 创建流式上下文 */

    /* 通过注册/分发机制将流式上下文同步到各子模块。
     * stream_context_register_builtins() 一次性注册所有内置模块的 setter，
     * stream_context_dispatch_all() 统一分发流式上下文到所有已注册模块。
     * 新增模块时只需在 stream_context_util.c 中添加注册行，
     * 无需修改此处的引擎初始化代码。 */
    stream_context_register_builtins();
    stream_context_dispatch_all(engine->stream_ctx);

    engine->main_graph = graph_create();
    if (!engine->main_graph) {
        stream_context_destroy(engine->stream_ctx);
        lv00_free((void **)&engine);
        last_status = ENGINE_OUT_OF_MEMORY;
        return NULL;
    }
    last_status = ENGINE_OK;
    last_error[0] = '\0';
    return engine;
}

void engine_destroy(LV00Engine *engine) {
    if (!engine) return;
    if (engine->frozen_point) {
        engine_destroy_frozen_point(engine->frozen_point);
        engine->frozen_point = NULL;
    }
    if (engine->stream_ctx) {
        stream_context_destroy(engine->stream_ctx);
        engine->stream_ctx = NULL;
    }
    if (engine->main_graph) {
        graph_destroy(engine->main_graph);
    }
    for (int i = 0; i < engine->module_count; i++) {
        module_destroy(engine->loaded_modules[i]);
    }
    lv00_free((void **)&engine->loaded_modules);
    for (int i = 0; i < engine->axiom_package_count; i++) {
        axiom_package_destroy(engine->axiom_packages[i]);
    }
    lv00_free((void **)&engine->axiom_packages);
    for (int i = 0; i < engine->rewrite_rule_count; i++) {
        rewrite_rule_destroy(engine->rewrite_rules[i]);
    }
    lv00_free((void **)&engine->rewrite_rules);
    lv00_free((void **)&engine);
}

/**
 * @brief 通用动态数组扩容辅助函数
 *
 * 当 count >= capacity 时触发扩容。扩容策略为指数增长（初始容量由
 * LV00_INITIAL_ARRAY_CAPACITY 定义，后续每次翻倍）。
 *
 * 【溢出检查逻辑】
 *   1. count <= -1: 防御性检查，捕获调用方传入负数的错误场景
 *   2. *capacity <= 0: 初始化容量分支，使用 LV00_INITIAL_ARRAY_CAPACITY
 *   3. *capacity > INT_MAX / 2: 已接近 int 最大值，无法安全扩容（扩容公式
 *      new_cap = capacity * 2 会溢出），此时直接返回 false
 *   4. (size_t)new_cap > SIZE_MAX / elem_size: 在分配内存前检查乘法是否溢出，
 *      防止 lv00_realloc 接收到超大值导致未定义行为
 *   5. new_cap < count: 极端情况下的安全钳制（例如 capacity 被外部错误修改）
 *
 * 注：第3、4条的检查顺序经过优化 —— 先检查 int 溢出（较廉价），
 * 再检查 size_t 溢出（只在需要分配时才做），避免不必要的除法运算。
 *
 * @param arr      当前数组指针的地址（用于 realloc）
 * @param count    当前元素数量
 * @param capacity 当前容量的地址（会被更新为新容量）
 * @param elem_size 单个元素的字节大小
 * @return true 扩容成功（或无需扩容），false 失败（内存不足或溢出）
 */
static bool engine_ensure_capacity(void **arr, int count, int *capacity, size_t elem_size) {
    if (count < *capacity) return true;
    /* 溢出检查：先防御负数，再防御接近 INT_MAX 的极端情况 */
    if (count < 0 || *capacity < 0) return false;
    if (*capacity > INT_MAX / 2) return false;
    int new_cap = *capacity == 0 ? LV00_INITIAL_ARRAY_CAPACITY : *capacity * LV00_ARRAY_GROWTH_FACTOR;
    /* 二次检查：确保扩容后的容量至少容纳 count 个元素 */
    if (new_cap < count) new_cap = count * LV00_ARRAY_GROWTH_FACTOR;
    /* 分配前检查：防止 new_cap * elem_size 超过 size_t 可表示范围 */
    if ((size_t)new_cap > SIZE_MAX / elem_size) return false;
    void *new_arr = lv00_realloc(*arr, (size_t)new_cap * elem_size);
    if (!new_arr) return false;
    *arr = new_arr;
    *capacity = new_cap;
    return true;
}

bool engine_add_rewrite_rule(LV00Engine *engine, RewriteRule *rule) {
    if (!engine || !rule) return false;
    
    if (!engine_ensure_capacity((void **)&engine->rewrite_rules, engine->rewrite_rule_count,
                                &engine->rewrite_rule_capacity, sizeof(RewriteRule*)))
        return false;
    
    engine->rewrite_rules[engine->rewrite_rule_count++] = rule;
    return true;
}

ModuleLoadStatus engine_load_module(LV00Engine *engine, const char *filepath) {
    Module *mod = module_create("temp", "0.0.0");
    if (!mod) {
        last_status = ENGINE_OUT_OF_MEMORY;
        snprintf(last_error, sizeof(last_error), "模块创建失败");
        return MODULE_LOAD_PARSE_ERROR;
    }
    ModuleLoadStatus status = module_load(mod, filepath, engine->loaded_modules, engine->module_count);
    if (status != MODULE_LOAD_OK) {
        module_destroy(mod);
        last_status = ENGINE_MODULE_ERROR;
        snprintf(last_error, sizeof(last_error),
                 "模块加载失败 [文件=%s, 状态码=%d]", filepath, status);
        return status;
    }
    /* 指数增长策略：使用通用扩容辅助函数 */
    if (!engine_ensure_capacity((void **)&engine->loaded_modules, engine->module_count,
                                &engine->module_capacity, sizeof(Module*))) {
        module_destroy(mod);
        last_status = ENGINE_OUT_OF_MEMORY;
        return MODULE_LOAD_PARSE_ERROR;
    }
    engine->loaded_modules[engine->module_count++] = mod;
    return MODULE_LOAD_OK;
}

AxiomLoadStatus engine_load_axiom_package(LV00Engine *engine, const char *filepath) {
    AxiomPackage *pkg = axiom_package_create("temp", "0.0.0");
    if (!pkg) {
        last_status = ENGINE_OUT_OF_MEMORY;
        snprintf(last_error, sizeof(last_error), "公理包创建失败");
        return AXIOM_LOAD_PARSE_ERROR;
    }
    AxiomLoadStatus status = axiom_package_load(pkg, filepath);
    if (status != AXIOM_LOAD_OK) {
        axiom_package_destroy(pkg);
        last_status = ENGINE_MODULE_ERROR;
        snprintf(last_error, sizeof(last_error),
                 "公理包加载失败 [文件=%s, 状态码=%d]", filepath, status);
        return status;
    }
    /* 指数增长策略：使用通用扩容辅助函数 */
    if (!engine_ensure_capacity((void **)&engine->axiom_packages, engine->axiom_package_count,
                                &engine->axiom_package_capacity, sizeof(AxiomPackage*))) {
        axiom_package_destroy(pkg);
        last_status = ENGINE_OUT_OF_MEMORY;
        return AXIOM_LOAD_PARSE_ERROR;
    }
    engine->axiom_packages[engine->axiom_package_count++] = pkg;
    return AXIOM_LOAD_OK;
}

/**
 * engine_pack_function - 将一组内部节点和端口打包为函数块。
 *
 * 将指定的内部节点、输入端口和输出端口封装为一个函数块(FunctionBlock)，
 * 并重新调整命名空间深度(namespace_depth)和父块ID(parent_block_id)。
 *
 * @param engine           引擎实例
 * @param internal_node_ids 内部节点ID数组
 * @param internal_count   内部节点数量
 * @param input_port_ids   输入端口ID数组
 * @param input_count      输入端口数量
 * @param output_port_ids  输出端口ID数组
 * @param output_count     输出端口数量
 * @param out_func_block_id [可选] 输出参数，用于接收新创建的函数块ID。
 *                         若为 NULL 则跳过赋值。
 * @return true 成功，false 失败（错误信息存入 last_error）
 */
bool engine_pack_function(LV00Engine *engine, int *internal_node_ids, int internal_count,
                         int *input_port_ids, int input_count,
                         int *output_port_ids, int output_count,
                         int *out_func_block_id) {
    for (int i = 0; i < internal_count; i++) {
        GeomNode *n = graph_get_node(engine->main_graph, internal_node_ids[i]);
        if (!n) {
            last_status = ENGINE_INVALID_STATE;
            snprintf(last_error, sizeof(last_error),
                     "打包函数块失败: 内部节点 %d 不存在", internal_node_ids[i]);
            return false;
        }
    }
    for (int i = 0; i < input_count; i++) {
        GeomNode *n = graph_get_node(engine->main_graph, input_port_ids[i]);
        if (!n || n->type != GEOM_PORT) {
            last_status = ENGINE_INVALID_STATE;
            snprintf(last_error, sizeof(last_error),
                     "打包函数块失败: 输入端口 %d 不存在或不是端口类型",
                     input_port_ids[i]);
            return false;
        }
    }
    for (int i = 0; i < output_count; i++) {
        GeomNode *n = graph_get_node(engine->main_graph, output_port_ids[i]);
        if (!n || n->type != GEOM_PORT) {
            last_status = ENGINE_INVALID_STATE;
            snprintf(last_error, sizeof(last_error),
                     "打包函数块失败: 输出端口 %d 不存在或不是端口类型",
                     output_port_ids[i]);
            return false;
        }
    }
    AddNodeResult result = graph_add_function_block(engine->main_graph, internal_node_ids, internal_count,
                                                     input_port_ids, input_count,
                                                     output_port_ids, output_count);
    if (result != ADD_NODE_OK) {
        last_status = ENGINE_INVALID_STATE;
        snprintf(last_error, sizeof(last_error), "创建函数块失败（图操作返回错误）");
        return false;
    }
    /* 记录新创建的函数块ID，供调用者使用（out_func_block_id 可以为 NULL） */
    int new_func_block_id = engine->main_graph->next_node_id - 1;
    if (out_func_block_id) {
        *out_func_block_id = new_func_block_id;
    }
    /* context_depth is the current canvas depth (0 for global canvas) */
    int context_depth = 0;
    for (int i = 0; i < internal_count; i++) {
        GeomNode *n = graph_get_node(engine->main_graph, internal_node_ids[i]);
        if (n) {
            n->parent_block_id = new_func_block_id;
            /* 重新设定 namespace_depth：new_depth = original_depth - context_depth + 1 */
            n->namespace_depth = n->namespace_depth - context_depth + 1;
        }
    }
    for (int i = 0; i < input_count; i++) {
        GeomNode *n = graph_get_node(engine->main_graph, input_port_ids[i]);
        if (n && n->type == GEOM_PORT) {
            n->data.port->parent_block_id = new_func_block_id;
            n->data.port->is_formal_param = true;
            n->data.port->namespace_depth = n->data.port->namespace_depth - context_depth + 1;
            n->parent_block_id = new_func_block_id;
            n->namespace_depth = n->namespace_depth - context_depth + 1;
        }
    }
    for (int i = 0; i < output_count; i++) {
        GeomNode *n = graph_get_node(engine->main_graph, output_port_ids[i]);
        if (n && n->type == GEOM_PORT) {
            n->data.port->parent_block_id = new_func_block_id;
            n->data.port->is_formal_param = false;
            n->data.port->namespace_depth = n->data.port->namespace_depth - context_depth + 1;
            n->parent_block_id = new_func_block_id;
            n->namespace_depth = n->namespace_depth - context_depth + 1;
        }
    }
    return true;
}

int *engine_instantiate_function(LV00Engine *engine, int func_block_id,
                                 int *arg_mappings, int arg_count,
                                 int *out_result_count) {
    if (!out_result_count) {
        last_status = ENGINE_INVALID_ARGUMENT;
        snprintf(last_error, sizeof(last_error), "out_result_count 不能为 NULL");
        return NULL;
    }
    *out_result_count = 0;

    GeomNode *func_block = graph_get_node(engine->main_graph, func_block_id);
    if (!func_block || func_block->type != GEOM_FUNCTION_BLOCK) {
        last_status = ENGINE_INVALID_STATE;
        snprintf(last_error, sizeof(last_error),
                 "函数块 %d 不存在或类型不是函数块", func_block_id);
        return NULL;
    }

    /*
     * 构建独立的 FuncBlock 描述符，从 GeomNode 中提取函数块数据，
     * 以便调用 func_block_instantiate()，实现完整的 beta-归约和变量捕获解析
     * （设计文档 Section 3.3 中的 A/B/C 三种情况）。
     */
    FuncBlock *fb = func_block_create(func_block_id);
    if (!fb) {
        last_status = ENGINE_OUT_OF_MEMORY;
        return NULL;
    }

    /* 拷贝内部节点ID */
    if (func_block->data.func_block.internal_node_count > 0) {
        int ic = func_block->data.func_block.internal_node_count;
        int *ids = lv00_malloc((size_t)ic * sizeof(int));
        if (!ids) {
            func_block_destroy(fb);
            last_status = ENGINE_OUT_OF_MEMORY;
            return NULL;
        }
        for (int i = 0; i < ic; i++) {
            if (!func_block->data.func_block.internal_nodes[i]) {
                func_block_destroy(fb);
                lv00_free((void **)&ids);
                last_status = ENGINE_INVALID_STATE;
                return NULL;
            }
            ids[i] = func_block->data.func_block.internal_nodes[i]->id;
        }
        func_block_set_internal_nodes(fb, ids, ic);
        lv00_free((void **)&ids);
    }

    /* 拷贝输入端口ID */
    if (func_block->data.func_block.input_count > 0) {
        int ic = func_block->data.func_block.input_count;
        func_block_set_input_ports(fb, func_block->data.func_block.input_port_ids, ic);
    }

    /* 拷贝输出端口ID */
    if (func_block->data.func_block.output_count > 0) {
        int oc = func_block->data.func_block.output_count;
        func_block_set_output_ports(fb, func_block->data.func_block.output_port_ids, oc);
    }

    /* 调用 func_block_instantiate：完整 beta-归约（A/B/C 三种情况） */
    int *new_node_ids = NULL;
    int new_node_count = 0;
    InstantiateResult inst_result = func_block_instantiate(
        fb, engine->main_graph, arg_mappings, arg_count,
        &new_node_ids, &new_node_count);

    func_block_destroy(fb);

    if (inst_result != INSTANTIATE_OK) {
        last_status = ENGINE_INVALID_STATE;
        snprintf(last_error, sizeof(last_error),
                 "engine_instantiate_function: instantiation failed (code %d)", inst_result);
        return NULL;
    }

    *out_result_count = new_node_count;
    return new_node_ids;
}

UnifyStatus engine_unify(LV00Engine *engine, ConstraintGraph *construction,
                        ConstraintGraph *proposition) {
    /* 参数校验：任一参数为 NULL 视为调用方错误 */
    if (!engine || !construction || !proposition) {
        if (engine) {
            engine->last_unify_status = LV00_ERROR_INVALID_PARAM;
            engine->last_status = ENGINE_INVALID_ARGUMENT;
            snprintf(engine->last_error, sizeof(engine->last_error),
                     "engine_unify: 空指针参数 (engine=%p, construction=%p, proposition=%p)",
                     (void*)engine, (void*)construction, (void*)proposition);
        }
        lv00_set_error(LV00_ERROR_INVALID_PARAM, "engine_unify: 空指针参数");
        return UNIFY_STATUS_FAILED;
    }

    /* 执行合一操作：委托给合一检查器模块 */
    UnifyStatus status = unify_construction_with_proposition(construction, proposition);

    /* 同步错误状态到引擎实例 */
    engine->last_unify_status = status;
    engine->last_status = ENGINE_OK;

    if (status == UNIFY_STATUS_OK) {
        /* 合一成功：构造图满足命题模式 */
        engine->last_error[0] = '\0';
    } else {
        /* 合一失败：根据具体失败类型设置对应的引擎错误信息 */
        const char *reason = "未知合一失败原因";
        switch (status) {
            case UNIFY_STATUS_PORT_TYPE_MISMATCH:
                reason = "端口类型不匹配";
                break;
            case UNIFY_STATUS_CONSTRAINT_MISMATCH:
                reason = "约束不匹配";
                break;
            case UNIFY_STATUS_COORD_MISMATCH:
                reason = "符号坐标不匹配";
                break;
            case UNIFY_STATUS_STRUCTURE_MISMATCH:
                reason = "图结构不匹配";
                break;
            case UNIFY_STATUS_SCOPE_MISMATCH:
                reason = "作用域不匹配";
                break;
            case UNIFY_STATUS_FAILED:
                reason = "合一检查系统内部错误";
                break;
            default:
                break;
        }
        engine->last_status = ENGINE_CONSTRAINT_CONFLICT;
        snprintf(engine->last_error, sizeof(engine->last_error),
                 "合一失败 [状态码=%d]: %s", (int)status, reason);
        lv00_set_error(LV00_ERROR_UNIFY_FAILED, reason);
    }

    return status;
}

EngineStatus engine_get_last_status(const LV00Engine *engine) {
    /* 优先使用引擎实例级别的错误状态（每个引擎独立隔离）。】
     * 若无引擎实例，回退到线程局部变量。 */
    if (engine) {
        return engine->last_status;
    }
    return last_status;
}

const char *engine_get_last_error(const LV00Engine *engine) {
    /* 优先使用引擎实例级别的错误状态。若无引擎实例，回退到线程局部变量。 */
    if (engine) {
        return engine->last_error;
    }
    return last_error;
}

/*
 * engine_solve - 完整求解流水线
 *
 * 协调执行：重写（有限步数）-> 求解器（处理剩余约束）
 *         -> 冲突检查 -> 自由度更新
 *
 * 返回 0 表示成功，-1 表示冲突，-2 表示超时
 */

/**
 * @brief 检查约束图中的冲突
 *
 * @param engine   引擎实例
 * @param context  上下文描述（用于错误消息）
 * @return 0 无冲突，-1 检测到冲突（并设置 last_status）
 */
static int check_and_report_conflicts(LV00Engine *engine, const char *context) {
    int conflict_count = 0;
    int **conflicts = graph_detect_conflicts(engine->main_graph, &conflict_count, NULL);
    if (conflicts) {
        for (int i = 0; i < conflict_count; i++) {
            lv00_free((void **)&conflicts[i]);
        }
        lv00_free((void **)&conflicts);
    }
    if (conflict_count > 0) {
        last_status = ENGINE_CONSTRAINT_CONFLICT;
        snprintf(last_error, sizeof(last_error),
                 "%s: 检测到 %d 个冲突", context, conflict_count);
        return -1;
    }
    return 0;
}

/**
 * @brief 在约束图上运行求解器
 *
 * @param engine   引擎实例
 * @param context  上下文描述（用于错误消息）
 * @return ENGINE_SOLVE_OK 成功，ENGINE_SOLVE_CONFLICT 冲突，ENGINE_SOLVE_TIMEOUT 超时
 */
static EngineSolveResult run_solver_on_graph(LV00Engine *engine, const char *context) {
    int *dirty_ids = NULL;
    int free_count = count_degrees_of_freedom(engine->main_graph, &dirty_ids);
    if (free_count > 0 && dirty_ids) {
        GroebnerResult *result = NULL;
        SolverStatus sstatus = solve_algebraic_system(
            engine->main_graph, dirty_ids, free_count, &result);
        if (result) groebner_result_free(result);
        lv00_free((void **)&dirty_ids);

        if (sstatus == SOLVER_NO_SOLUTION || sstatus == SOLVER_OVERCONSTRAINED) {
            last_status = ENGINE_CONSTRAINT_CONFLICT;
            snprintf(last_error, sizeof(last_error),
                     "%s: 求解器检测到冲突", context);
            return ENGINE_SOLVE_CONFLICT;
        }
        if (sstatus == SOLVER_TIMEOUT) {
            last_status = ENGINE_CONSTRAINT_CONFLICT;
            snprintf(last_error, sizeof(last_error),
                     "%s: 求解器超时", context);
            return ENGINE_SOLVE_TIMEOUT;
        }
        return ENGINE_SOLVE_OK;
    }
    lv00_free((void **)&dirty_ids);
    return ENGINE_SOLVE_OK;
}

/**
 * @brief 完整求解流水线
 *
 * 协调执行：重写（有限步数）-> 求解器（处理剩余约束）
 *         -> 冲突检查 -> 自由度更新
 *
 * @param engine 引擎实例
 * @return ENGINE_SOLVE_OK 成功，ENGINE_SOLVE_CONFLICT 冲突，
 *         ENGINE_SOLVE_TIMEOUT 超时，ENGINE_SOLVE_ERROR 错误
 */
EngineSolveResult engine_solve(LV00Engine *engine) {
    if (!engine || !engine->main_graph) {
        last_status = ENGINE_INVALID_STATE;
        snprintf(last_error, sizeof(last_error),
                 "求解失败: 引擎实例或约束图为空 (engine=%p)", (void*)engine);
        return ENGINE_SOLVE_ERROR;
    }

    /* 流式事件: 引擎开始 */
    engine_emit_stream_event(engine, STREAM_EVENT_ENGINE_START,
        "求解流程启动", 0, -1, -1);

    /* 步骤0：归一化约束图
     * 根据 design_v2.9.md Section 18.1：求解前进行归一化可消除冗余节点并规范化图结构。
     * 归一化失败时记录警告，但不中断求解流程（归一化是优化步骤，
     * 即使失败，求解器仍可尝试求解原始图）。 */
    {
        engine_emit_stream_event(engine, STREAM_EVENT_NORMALIZE_START,
            "开始图规范化", 0, -1, -1);

        NormalizationResult *norm = graph_normalize(engine->main_graph, false);
        if (!norm) {
            /* 归一化失败（可能内存不足或图状态异常），记录警告继续执行。
             * 求解器将在未归一化的图上运行，结果可能不如预期。 */
            snprintf(last_error, sizeof(last_error),
                     "engine_solve: graph_normalize 返回 NULL，将继续在未规范化的图上求解");
            engine_emit_stream_event(engine, STREAM_EVENT_WARNING,
                "图规范化失败，将继续在未规范化的图上求解", 0, -1, -1);
        } else {
            /* 流式事件: 归一化完成（含合并节点数） */
            {
                StreamEvent ev;
                memset(&ev, 0, sizeof(ev));
                ev.type = STREAM_EVENT_NORMALIZE_DONE;
                ev.timestamp_ms = stream_timestamp_ms();
                ev.step_number = 0;
                ev.total_steps = -1;
                ev.node_id = norm->merged_count;
                ev.constraint_id = -1;
                ev.description = "图规范化完成";
                ev.progress = -1.0;
                stream_emit(engine->stream_ctx, &ev);
            }
            normalization_result_destroy(norm);
        }
    }

    /* 步骤1：使用所有已注册规则运行重写引擎（受引擎步数限制） */
    int rewrite_limit = engine->rewrite_step_limit > 0 ? engine->rewrite_step_limit : LV00_DEFAULT_REWRITE_STEP_LIMIT;
    if (engine->rewrite_rule_count > 0) {
        engine_emit_stream_event(engine, STREAM_EVENT_REWRITE_START,
            "开始重写阶段", 1, -1, -1);

        RewriteStatus rstatus = rewrite_with_rules(
            engine->main_graph,
            engine->rewrite_rules,
            engine->rewrite_rule_count,
            rewrite_limit,
            false  /* normalize_between_steps: 默认禁用 */
        );

        if (rstatus == REWRITE_TERMINATED) {
            engine_emit_stream_event(engine, STREAM_EVENT_ERROR,
                "重写终止（可能循环）", 1, -1, -1);
            last_status = ENGINE_CONSTRAINT_CONFLICT;
            snprintf(last_error, sizeof(last_error),
                     "engine_solve: 重写终止（可能存在循环）");
            return ENGINE_SOLVE_TIMEOUT;
        }
        engine_emit_stream_event(engine, STREAM_EVENT_REWRITE_DONE,
            "重写阶段完成", 1, -1, -1);
    }

    /* 步骤2：如果重写未能完全化简，则对剩余约束调用求解器 */
    int node_count = graph_get_node_count(engine->main_graph);
    if (node_count > 0) {
        engine_emit_stream_event(engine, STREAM_EVENT_SOLVE_START,
            "开始代数求解", 2, -1, -1);

        EngineSolveResult solver_result = run_solver_on_graph(engine, "engine_solve");
        if (solver_result != ENGINE_SOLVE_OK) {
            engine_emit_stream_event(engine, STREAM_EVENT_ERROR,
                "代数求解失败", 2, -1, -1);
            return solver_result;
        }
        engine_emit_stream_event(engine, STREAM_EVENT_SOLVE_DONE,
            "代数求解完成", 2, -1, -1);
    }

    /* 步骤3：检查冲突 */
    if (check_and_report_conflicts(engine, "engine_solve") != 0) {
        engine_emit_stream_event(engine, STREAM_EVENT_CONFLICT_DETECTED,
            "检测到约束冲突", 3, -1, -1);
        return ENGINE_SOLVE_CONFLICT;
    }

    /* 步骤4：更新自由度信息（重新计数） */
    int *free_var_ids = NULL;
    int free_count = count_degrees_of_freedom(engine->main_graph, &free_var_ids);
    lv00_free((void **)&free_var_ids);

    /* 流式事件: 引擎完成 */
    {
        StreamEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = STREAM_EVENT_ENGINE_DONE;
        ev.timestamp_ms = stream_timestamp_ms();
        ev.step_number = 4;
        ev.node_id = free_count;  /* 自由度为附加值 */
        ev.description = "求解流程完成";
        stream_emit(engine->stream_ctx, &ev);
    }

    last_status = ENGINE_OK;
    last_error[0] = '\0';
    return ENGINE_SOLVE_OK;
}

/**
 * @brief 重写-求解协作工作流
 *
 * 实现 design_v2.9.md Section 3.6 中的协作协议：
 *   先重写 -> 遇停顿则求解 -> 暴露冲突
 *
 * 步骤1：运行重写引擎至 max_rewrite_steps 步
 * 步骤2：若重写有进展，返回步骤1
 * 步骤3：若重写停滞，调用求解器
 * 步骤4：若求解器发现冲突，报告并停止
 *
 * @param engine            引擎实例
 * @param max_rewrite_steps 最大重写步数
 * @param max_solve_steps  最大求解步数
 * @return 总执行步数，出错返回负值
 */
int engine_rewrite_and_solve(LV00Engine *engine, int max_rewrite_steps, int max_solve_steps) {
    if (!engine || !engine->main_graph) {
        last_status = ENGINE_INVALID_STATE;
        snprintf(last_error, sizeof(last_error),
                 "重写-求解协作失败: 引擎实例或约束图为空 (engine=%p)", (void*)engine);
        return -1;
    }

    /* 流式事件: 引擎开始 */
    engine_emit_stream_event(engine, STREAM_EVENT_ENGINE_START,
        "重写-求解协作流程启动", 0, -1, -1);

    /* 若调用方传入0或负值，则使用引擎的可配置步数限制 */
    if (max_rewrite_steps <= 0) {
        max_rewrite_steps = engine->rewrite_step_limit > 0 ? engine->rewrite_step_limit : LV00_DEFAULT_REWRITE_STEP_LIMIT;
    }

    int total_steps = 0;
    int remaining_rewrite = max_rewrite_steps;
    int remaining_solve = max_solve_steps;
    int iteration = 0;  /* 迭代轮次计数 */

    /* 初始化WL哈希历史记录用于循环检测 */
    WLHashHistory wl_history;
    wl_history_init(&wl_history);

    /* 外层循环：交替执行重写和求解 */
    while (remaining_rewrite > 0 || remaining_solve > 0) {
        iteration++;

        /* 步骤1：运行重写引擎最多 remaining_rewrite 步 */
        if (remaining_rewrite > 0 && engine->rewrite_rule_count > 0) {
            int before_constraints = graph_get_constraint_count(engine->main_graph);

            /* 流式事件: 重写轮次开始 */
            {
                StreamEvent ev;
                memset(&ev, 0, sizeof(ev));
                ev.type = STREAM_EVENT_REWRITE_START;
                ev.timestamp_ms = stream_timestamp_ms();
                ev.step_number = iteration;
                ev.total_steps = -1;
                ev.node_id = before_constraints;
                ev.description = "重写轮次开始";
                ev.progress = -1.0;
                stream_emit(engine->stream_ctx, &ev);
            }

            RewriteStatus rstatus = rewrite_with_rules(
                engine->main_graph,
                engine->rewrite_rules,
                engine->rewrite_rule_count,
                remaining_rewrite,
                true  /* normalize_between_steps: 为求解循环启用 */
            );

            int after_constraints = graph_get_constraint_count(engine->main_graph);
            int rewrite_progress = before_constraints - after_constraints;
            total_steps += (rewrite_progress > 0) ? rewrite_progress : 1;
            remaining_rewrite = 0;

            if (rstatus == REWRITE_TERMINATED) {
                engine_emit_stream_event(engine, STREAM_EVENT_ERROR,
                    "重写终止（循环）", iteration, -1, -1);
                last_status = ENGINE_CONSTRAINT_CONFLICT;
                snprintf(last_error, sizeof(last_error),
                         "engine_rewrite_and_solve: 重写终止（存在循环）");
                wl_history_destroy(&wl_history);
                return -2;
            }

            /* 流式事件: 重写轮次完成 */
            {
                StreamEvent ev;
                memset(&ev, 0, sizeof(ev));
                ev.type = STREAM_EVENT_REWRITE_DONE;
                ev.timestamp_ms = stream_timestamp_ms();
                ev.step_number = iteration;
                ev.node_id = rewrite_progress;  /* 约束减少量 */
                ev.constraint_id = after_constraints;
                ev.description = "重写轮次完成";
                ev.progress = -1.0;
                stream_emit(engine->stream_ctx, &ev);
            }

            /* 重写阶段后的WL图哈希循环检测 */
            RewriteStatus loop_status = detect_rewrite_loop_wl(engine->main_graph, &wl_history);
            if (loop_status == REWRITE_TERMINATED) {
                engine_emit_stream_event(engine, STREAM_EVENT_ERROR,
                    "WL哈希循环检测触发", iteration, -1, -1);
                last_status = ENGINE_CONSTRAINT_CONFLICT;
                snprintf(last_error, sizeof(last_error),
                         "engine_rewrite_and_solve: 在第 %d 步通过WL哈希检测到重写循环",
                         total_steps);
                wl_history_destroy(&wl_history);
                return -2;
            }

            /* 步骤2：若重写有进展，返回步骤1 */
            if (rewrite_progress > 0) {
                remaining_rewrite = max_rewrite_steps;
                continue;
            }
        } else {
            remaining_rewrite = 0;
        }

        /* 步骤3：重写停滞，调用求解器 */
        if (remaining_solve > 0) {
            engine_emit_stream_event(engine, STREAM_EVENT_SOLVE_START,
                "重写停滞，调用代数求解", iteration, -1, -1);

            EngineSolveResult solver_result = run_solver_on_graph(engine, "engine_rewrite_and_solve");
            if (solver_result != ENGINE_SOLVE_OK) {
                engine_emit_stream_event(engine, STREAM_EVENT_ERROR,
                    "代数求解失败", iteration, -1, -1);
                wl_history_destroy(&wl_history);
                return -(int)solver_result - 1;
            }
            total_steps += 1;
            remaining_solve = 0;

            engine_emit_stream_event(engine, STREAM_EVENT_SOLVE_DONE,
                "代数求解完成，返回重写阶段", iteration, -1, -1);

            /* 求解器有进展，返回重写阶段 */
            remaining_rewrite = max_rewrite_steps;
            continue;
        }

        /* 重写和求解都无法取得进展，停止 */
        break;
    }

    wl_history_destroy(&wl_history);

    /* 最终冲突检查 */
    if (check_and_report_conflicts(engine, "engine_rewrite_and_solve") != 0) {
        engine_emit_stream_event(engine, STREAM_EVENT_CONFLICT_DETECTED,
            "最终冲突检查失败", total_steps, -1, -1);
        return -1;
    }

    /* 流式事件: 引擎完成 */
    {
        StreamEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = STREAM_EVENT_ENGINE_DONE;
        ev.timestamp_ms = stream_timestamp_ms();
        ev.step_number = total_steps;
        ev.constraint_id = graph_get_constraint_count(engine->main_graph);
        ev.description = "重写-求解协作流程完成";
        stream_emit(engine->stream_ctx, &ev);
    }

    last_status = ENGINE_OK;
    last_error[0] = '\0';
    return total_steps;
}

/**
 * @brief 位电路跳闸处理器
 *
 * 当位电路跳闸时（来自 symbolic_coord 操作）：
 *   1. 检查是否存在冻结点
 *   2. 若 overflow_count >= 3，建议永久降级
 *   3. 否则，报告警告
 *
 * @param engine 引擎实例
 * @return ENGINE_CIRCUIT_IGNORE 已处理（忽略），
 *         ENGINE_CIRCUIT_ROLLBACK 需要回滚，ENGINE_CIRCUIT_DOWNGRADE 建议降级
 */
EngineCircuitResult engine_handle_circuit_trip(LV00Engine *engine) {
    if (!engine) {
        return ENGINE_CIRCUIT_IGNORE;
    }

    (void)engine;  /* 引擎可用于未来特定上下文的处理 */

    /* 步骤1：检查是否存在冻结点 */
    if (circuit_has_frozen_point()) {
        /* 存在冻结点，调用方可能需要回滚到该点 */
        int overflow = circuit_get_overflow_count();

        /* 步骤2：若 overflow_count >= LV00_CIRCUIT_OVERFLOW_THRESHOLD，建议永久降级 */
        if (overflow >= LV00_CIRCUIT_OVERFLOW_THRESHOLD) {
            snprintf(last_error, sizeof(last_error),
                     "engine_handle_circuit_trip: 溢出计数 %d >= %d，"
                     "建议永久降级为琥珀色", overflow, LV00_CIRCUIT_OVERFLOW_THRESHOLD);
            last_status = ENGINE_CONSTRAINT_CONFLICT;
            return ENGINE_CIRCUIT_DOWNGRADE;
        }

        /* 存在冻结点但溢出计数可控，建议回滚 */
        snprintf(last_error, sizeof(last_error),
                 "engine_handle_circuit_trip: 存在冻结点，"
                 "溢出计数 %d，建议回滚", overflow);
        last_status = ENGINE_OK;
        return ENGINE_CIRCUIT_ROLLBACK;
    }

    /* 步骤3：无冻结点，仅报告警告 */
    int overflow = circuit_get_overflow_count();
    if (overflow >= LV00_CIRCUIT_OVERFLOW_THRESHOLD) {
        /* 即使没有冻结点，反复溢出也建议降级 */
        snprintf(last_error, sizeof(last_error),
                 "engine_handle_circuit_trip: 溢出计数 %d >= %d，"
                 "建议永久降级为琥珀色", overflow, LV00_CIRCUIT_OVERFLOW_THRESHOLD);
        last_status = ENGINE_CONSTRAINT_CONFLICT;
        return ENGINE_CIRCUIT_DOWNGRADE;
    }

    snprintf(last_error, sizeof(last_error),
             "engine_handle_circuit_trip: 溢出计数 %d，已处理（忽略）", overflow);
    last_status = ENGINE_OK;
    return ENGINE_CIRCUIT_IGNORE;
}

/**
 * @brief 使用显式用户动作处理电路跳闸
 *
 * 根据 design_v2.9.md Section 1.5：
 * - action 0 (忽略)：将坐标标记为琥珀色，继续执行
 * - action 1 (回滚)：恢复到冻结点快照
 * - action 2 (永久降级)：替换为高精度浮点数，标记琥珀色
 *
 * @param engine 引擎实例
 * @param action 用户选择的动作（0=忽略，1=回滚，2=降级）
 * @return 引擎电路结果码
 */
EngineCircuitResult engine_handle_circuit_trip_with_action(LV00Engine *engine, EngineCircuitAction action) {
    if (!engine) return ENGINE_CIRCUIT_ERROR;

    SymbolicCoord *overflow_coord = circuit_get_last_result();

    switch (action) {
        case ENGINE_CIRCUIT_ACTION_IGNORE:  /* 忽略：将坐标标记为琥珀色并继续 */
            if (overflow_coord) {
                symbolic_coord_set_trust(overflow_coord, TRUST_AMBER);
            }
            circuit_handle_overflow();
            last_status = ENGINE_OK;
            last_error[0] = '\0';
            return ENGINE_CIRCUIT_IGNORE;

        case ENGINE_CIRCUIT_ACTION_ROLLBACK:  /* 回滚：恢复到冻结点 */
            if (engine->frozen_point) {
                engine_restore_frozen_point(engine, engine->frozen_point);
                /* engine_restore_frozen_point 消费了 frozen_point，已置 NULL */
            }
            circuit_reset_context();
            last_status = ENGINE_OK;
            snprintf(last_error, sizeof(last_error),
                     "engine: 通过回滚到冻结点处理了电路跳闸");
            return ENGINE_CIRCUIT_ROLLBACK;

        case ENGINE_CIRCUIT_ACTION_DOWNGRADE:  /* 永久降级：替换为高精度浮点数近似值 */
            if (overflow_coord) {
                /* 将溢出坐标永久降级为高精度有理数近似值。
                 * 策略：创建新的有理数坐标，然后将其内容原地替换到
                 * overflow_coord 中，使得所有引用该坐标的节点都能
                 * 看到降级后的值。 */
                double approx = symbolic_coord_to_double(overflow_coord);
                if (fabs(approx) > LV00_VALUE_TOO_LARGE) {
                    /* 值过大，无法用 int64_t 分子精确表示，仅标记琥珀色 */
                    symbolic_coord_set_trust(overflow_coord, TRUST_AMBER);
                } else {
                    SymbolicCoord *new_coord = symbolic_coord_create_rational(
                        (int64_t)(approx * LV00_DOWNGRADE_DENOMINATOR), LV00_DOWNGRADE_DENOMINATOR);
                    if (new_coord) {
                        symbolic_coord_set_trust(new_coord, TRUST_AMBER);
                        /* 原地替换：先释放 overflow_coord 的旧数据，
                         * 再将 new_coord 的数据转移过来，最后释放 new_coord 外壳。
                         * 这样所有持有 overflow_coord 指针的引用者都能看到新值。 */
                        symbolic_coord_destroy(overflow_coord);
                        overflow_coord->type = new_coord->type;
                        overflow_coord->data = new_coord->data;
                        overflow_coord->trust = new_coord->trust;
                        /* 仅释放 new_coord 的外壳，不释放其 data（已转移） */
                        lv00_free((void **)&new_coord);
                    } else {
                        /* new_coord 创建失败，仅标记琥珀色作为降级 */
                        symbolic_coord_set_trust(overflow_coord, TRUST_AMBER);
                    }
                }
            }
            circuit_handle_overflow();
            last_status = ENGINE_OK;
            snprintf(last_error, sizeof(last_error),
                     "engine: 通过永久降级为琥珀色处理了电路跳闸");
            return ENGINE_CIRCUIT_DOWNGRADE;

        default:
            last_status = ENGINE_INVALID_STATE;
            snprintf(last_error, sizeof(last_error),
                     "engine_handle_circuit_trip_with_action: 无效的动作 %d", action);
            return ENGINE_CIRCUIT_ERROR;
    }
}

/* ================================================================
 * 重写步数限制配置
 * ================================================================ */

/**
 * @brief 设置引擎重写步数上限
 *
 * 若传入 limit <= 0，则自动使用默认值 LV00_DEFAULT_REWRITE_STEP_LIMIT。
 *
 * @param engine 引擎实例
 * @param limit  新的步数上限（正值）
 */
void engine_set_rewrite_step_limit(LV00Engine *engine, int limit) {
    if (!engine) return;
    if (limit <= 0) limit = LV00_DEFAULT_REWRITE_STEP_LIMIT;  /* 强制使用正数下限 */
    engine->rewrite_step_limit = limit;
}

/**
 * @brief 获取引擎重写步数上限
 *
 * 若引擎为 NULL 或未设置有效值，返回默认值 LV00_DEFAULT_REWRITE_STEP_LIMIT。
 *
 * @param engine 引擎实例
 * @return 当前重写步数上限
 */
int engine_get_rewrite_step_limit(const LV00Engine *engine) {
    if (!engine) return LV00_DEFAULT_REWRITE_STEP_LIMIT;
    return engine->rewrite_step_limit > 0 ? engine->rewrite_step_limit : LV00_DEFAULT_REWRITE_STEP_LIMIT;
}

/* ================================================================
 * 冻结点快照机制
 *
 * 冻结点是对引擎约束图的深拷贝，在执行有风险的符号操作前创建。
 * 如果位电路跳闸，引擎可以回滚到这个快照。
 * ================================================================ */

/**
 * @brief 深拷贝整个约束图
 *
 * 返回一个新分配的 ConstraintGraph，它是源图的结构克隆。
 * 调用方拥有返回的图的所有权，必须调用 graph_destroy() 释放。
 *
 * @param src 源约束图
 * @return 深拷贝后的新图，失败返回 NULL
 */
static ConstraintGraph *graph_deep_copy(const ConstraintGraph *src) {
    if (!src) return NULL;

    ConstraintGraph *dst = graph_create();
    if (!dst) return NULL;

    /* 构建ID映射表：old_id -> new_id */
    int max_id = 0;
    for (int i = 0; i < src->node_count; i++) {
        if (src->nodes[i]->id > max_id) max_id = src->nodes[i]->id;
    }
    int *id_map = NULL;
    if (max_id > 0) {
        id_map = lv00_calloc((size_t)(max_id + 1), sizeof(int));
        if (!id_map) {
            graph_destroy(dst);
            return NULL;
        }
        for (int i = 0; i <= max_id; i++) id_map[i] = -1;
    }

    /* 第一遍：深拷贝所有节点 */
    /* 预分配足够容量以容纳所有源节点 */
    if (src->node_count > 0) {
        dst->nodes = lv00_malloc((size_t)src->node_count * sizeof(GeomNode*));
        if (!dst->nodes) {
            graph_destroy(dst);
            lv00_free((void **)&id_map);
            return NULL;
        }
    }
    for (int i = 0; i < src->node_count; i++) {
        GeomNode *orig = src->nodes[i];
        GeomNode *copy = node_deep_copy_geom_node(orig, NULL);
        if (!copy) {
            /* 失败时清理 */
            graph_destroy(dst);
            lv00_free((void **)&id_map);
            return NULL;
        }

        /* 分配新ID并记录映射 */
        int new_id = dst->next_node_id++;
        copy->id = new_id;
        if (id_map && orig->id >= 0 && orig->id <= max_id) {
            id_map[orig->id] = new_id;
        }

        /* 添加到目标图（无需realloc，已预分配） */
        dst->nodes[dst->node_count++] = copy;
    }

    /* 第二遍：更新拷贝节点中的交叉引用 */
    for (int i = 0; i < dst->node_count; i++) {
        GeomNode *copy = dst->nodes[i];
        switch (copy->type) {
            case GEOM_PORT:
                if (copy->data.port && copy->data.port->connected_to) {
                    int old_cid = copy->data.port->connected_to->id;
                    if (id_map && old_cid >= 0 && old_cid <= max_id && id_map[old_cid] >= 0) {
                        copy->data.port->connected_to = graph_get_node(dst, id_map[old_cid]);
                    } else {
                        copy->data.port->connected_to = NULL;
                    }
                }
                break;
            case GEOM_REGION:
                for (int j = 0; j < copy->data.region.segment_count; j++) {
                    if (copy->data.region.boundary_segments[j]) {
                        int old_sid = copy->data.region.boundary_segments[j]->id;
                        if (id_map && old_sid >= 0 && old_sid <= max_id && id_map[old_sid] >= 0) {
                            copy->data.region.boundary_segments[j] = graph_get_node(dst, id_map[old_sid]);
                        } else {
                            copy->data.region.boundary_segments[j] = NULL;
                        }
                    }
                }
                break;
            case GEOM_FUNCTION_BLOCK:
                for (int j = 0; j < copy->data.func_block.internal_node_count; j++) {
                    if (copy->data.func_block.internal_nodes[j]) {
                        int old_iid = copy->data.func_block.internal_nodes[j]->id;
                        if (id_map && old_iid >= 0 && old_iid <= max_id && id_map[old_iid] >= 0) {
                            copy->data.func_block.internal_nodes[j] = graph_get_node(dst, id_map[old_iid]);
                        } else {
                            copy->data.func_block.internal_nodes[j] = NULL;
                        }
                    }
                }
                for (int j = 0; j < copy->data.func_block.input_count; j++) {
                    int old_pid = copy->data.func_block.input_port_ids[j];
                    if (id_map && old_pid >= 0 && old_pid <= max_id && id_map[old_pid] >= 0) {
                        copy->data.func_block.input_port_ids[j] = id_map[old_pid];
                    }
                }
                for (int j = 0; j < copy->data.func_block.output_count; j++) {
                    int old_pid = copy->data.func_block.output_port_ids[j];
                    if (id_map && old_pid >= 0 && old_pid <= max_id && id_map[old_pid] >= 0) {
                        copy->data.func_block.output_port_ids[j] = id_map[old_pid];
                    }
                }
                break;
            default:
                break;
        }
    }

    /* 复制约束(Constraint)数组。
     * 约束是图完整性的关键部分，如果约束复制失败（内存不足），
     * 深拷贝的结果将是不完整的，可能导致后续求解产生错误结果。
     * 因此，约束分配失败时释放已分配的所有资源并返回 NULL。 */
    for (int i = 0; i < src->constraint_count; i++) {
        Constraint *orig_c = src->constraints[i];
        Constraint *copy_c = lv00_malloc(sizeof(Constraint));
        if (!copy_c) {
            /* 约束分配失败：释放已复制的所有约束和整个目标图，返回 NULL */
            for (int k = 0; k < dst->constraint_count; k++) {
                lv00_free((void **)&dst->constraints[k]->participants);
                lv00_free((void **)&dst->constraints[k]);
            }
            lv00_free((void **)&dst->constraints);
            graph_destroy(dst);
            lv00_free((void **)&id_map);
            return NULL;
        }

        copy_c->id = dst->next_constraint_id++;
        copy_c->type = orig_c->type;
        copy_c->template_id = orig_c->template_id;
        copy_c->participant_count = orig_c->participant_count;

        if (orig_c->participant_count > 0) {
            copy_c->participants = lv00_malloc((size_t)orig_c->participant_count * sizeof(int));
            if (copy_c->participants) {
                for (int j = 0; j < orig_c->participant_count; j++) {
                    int old_pid = orig_c->participants[j];
                    if (id_map && old_pid >= 0 && old_pid <= max_id && id_map[old_pid] >= 0) {
                        copy_c->participants[j] = id_map[old_pid];
                    } else {
                        copy_c->participants[j] = old_pid;
                    }
                }
            } else {
                copy_c->participant_count = 0;
            }
        } else {
            copy_c->participants = NULL;
        }

        Constraint **tmp_cons;
        if (dst->constraint_count == 0) {
            tmp_cons = lv00_malloc(sizeof(Constraint*));
        } else {
            tmp_cons = lv00_realloc(dst->constraints,
                               (size_t)(dst->constraint_count + 1) * sizeof(Constraint*));
        }
        if (tmp_cons) {
            dst->constraints = tmp_cons;
            dst->constraints[dst->constraint_count++] = copy_c;
        } else {
            lv00_free((void **)&copy_c->participants);
            lv00_free((void **)&copy_c);
        }
    }

    lv00_free((void **)&id_map);

    /* ---- 重建哈希索引 ----
     * graph_create() 初始化空的哈希表（node_index, constraint_index），
     * 但上面的深拷贝绕过了 graph_alloc_node/graph_alloc_constraint，
     * 所以哈希表仍然为空。使用与 constraint_graph.c 相同的 FNV-1a
     * 开放寻址方案重建它们。
     */
    {
        /* --- 重建 node_index --- */
        if (dst->node_count > 0) {
            int ni_cap = LV00_NODE_INDEX_INITIAL_SIZE;
            while (ni_cap < dst->node_count * LV00_ARRAY_GROWTH_FACTOR) ni_cap *= LV00_ARRAY_GROWTH_FACTOR;  /* 保持负载率 < 0.5 */

            lv00_free((void **)&dst->node_index);  /* 释放 graph_create 创建的空表 */
            dst->node_index = lv00_calloc((size_t)ni_cap, sizeof(GeomNode *));
            if (dst->node_index) {
                dst->node_index_capacity = ni_cap;
                for (int i = 0; i < dst->node_count; i++) {
                    unsigned idx = (unsigned)dst->nodes[i]->id * LV00_FNV_HASH_MULTIPLIER
                                   & (unsigned)(ni_cap - 1);
                    while (dst->node_index[idx] != NULL) {
                        idx = (idx + 1) & (unsigned)(ni_cap - 1);
                    }
                    dst->node_index[idx] = dst->nodes[i];
                }
            }
        }

        /* --- 重建 constraint_index --- */
        if (dst->constraint_count > 0) {
            int ci_cap = LV00_CONSTRAINT_INDEX_INITIAL_SIZE;
            while (ci_cap < dst->constraint_count * LV00_ARRAY_GROWTH_FACTOR) ci_cap *= LV00_ARRAY_GROWTH_FACTOR;

            lv00_free((void **)&dst->constraint_index);
            dst->constraint_index = lv00_calloc((size_t)ci_cap, sizeof(Constraint *));
            if (dst->constraint_index) {
                dst->constraint_index_capacity = ci_cap;
                for (int i = 0; i < dst->constraint_count; i++) {
                    unsigned idx = (unsigned)dst->constraints[i]->id * LV00_FNV_HASH_MULTIPLIER
                                   & (unsigned)(ci_cap - 1);
                    while (dst->constraint_index[idx] != NULL) {
                        idx = (idx + 1) & (unsigned)(ci_cap - 1);
                    }
                    dst->constraint_index[idx] = dst->constraints[i];
                }
            }
        }
    }

    return dst;
}

void *engine_create_frozen_point(LV00Engine *engine) {
    if (!engine || !engine->main_graph) return NULL;

    ConstraintGraph *snapshot = graph_deep_copy(engine->main_graph);
    return (void *)snapshot;
}

bool engine_restore_frozen_point(LV00Engine *engine, void *frozen_point) {
    if (!engine || !frozen_point) return false;

    ConstraintGraph *snapshot = (ConstraintGraph *)frozen_point;

    /* 销毁当前图 */
    if (engine->main_graph) {
        graph_destroy(engine->main_graph);
    }

    /* 用快照替换（所有权转移给引擎） */
    engine->main_graph = snapshot;

    /* 同时更新电路系统的冻结点状态 */
    circuit_set_frozen_point(NULL);
    engine->frozen_point = NULL;

    return true;
}

void engine_destroy_frozen_point(void *frozen_point) {
    if (!frozen_point) return;
    ConstraintGraph *snapshot = (ConstraintGraph *)frozen_point;
    graph_destroy(snapshot);
}

/* ================================================================
 * 流式输出 API
 * ================================================================ */

StreamContext *engine_get_stream_context(const LV00Engine *engine) {
    if (!engine) return NULL;
    return engine->stream_ctx;
}

void engine_set_streaming_enabled(LV00Engine *engine, bool enabled) {
    if (!engine) return;
    if (!enabled && engine->stream_ctx) {
        /* 禁用时销毁流式上下文，同步清空子模块 */
        stream_context_destroy(engine->stream_ctx);
        engine->stream_ctx = NULL;
        /* 通过分发机制统一清空所有已注册子模块的流式上下文 */
        stream_context_dispatch_all(NULL);
    } else if (enabled && !engine->stream_ctx) {
        /* 启用时重新创建流式上下文，通过分发机制同步到所有子模块 */
        engine->stream_ctx = stream_context_create();
        stream_context_dispatch_all(engine->stream_ctx);
    }
}

bool engine_is_streaming_enabled(const LV00Engine *engine) {
    if (!engine) return false;
    return engine->stream_ctx != NULL;
}

void engine_emit_stream_event(LV00Engine *engine, StreamEventType type,
                               const char *description, int step_number,
                               int node_id, int constraint_id) {
    if (!engine || !engine->stream_ctx) return;

    StreamEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = type;
    ev.timestamp_ms = stream_timestamp_ms();
    ev.step_number = step_number;
    ev.total_steps = -1;
    ev.node_id = node_id;
    ev.constraint_id = constraint_id;
    ev.rule_id = -1;
    ev.var_id = -1;
    ev.description = description;
    ev.progress = -1.0;
    ev.numeric_value = 0.0;

    stream_emit(engine->stream_ctx, &ev);
}
