/**
 * @file engine.c
 * @brief 主引擎实现
 * @details 实现工作流编排，协调规范化、重写、求解和冲突检查。
 *          支持重写-求解协作、位电路跳闸处理和冻结点回滚。
 *
 * ============================================================
 * 迁移计划：从全局状态到 lvContext（v3.3.0）
 * ============================================================
 *
 * Lv-00 正在从"全局引擎模式"迁移到"隔离上下文模式"。迁移路线图：
 *
 *   第 1 阶段（当前）：lvEngine 持有 lvContext* 指针，两者共存。
 *     上下文中已拥有独立的错误码、错误消息、递归深度追踪。
 *     引擎级全局变量标记为 LEGACY，新代码禁止新增全局状态。
 *
 *   第 2 阶段（下一主版本）：将引擎的核心逻辑逐步下沉到 lvContext。
 *     engine_solve()、engine_rewrite_and_solve() 等函数改用 context 参数。
 *     全局变量 last_status / last_error 彻底移除。
 *
 *   第 3 阶段（远期）：lvEngine 降级为 C API 的薄封装层，
 *     所有引擎逻辑由 lvContext 统一管理。
 *
 * 设计原则：
 *   1. 新代码必须通过 context 访问引擎状态，禁止新增全局/线程局部变量。
 *   2. 现有 LEGACY 变量在迁移完成前继续工作，不影响运行时行为。
 *   3. 每个 context 实例是完全隔离的，支持并发、分支推理和资源熔断。
 * ============================================================
 */

#include "lv/engine.h"

#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/stream.h"
#include "node_deep_copy.h"
#include "stream_context_util.h"

/** 模块名称最大长度（用于 engine_extract_module_name 静态缓冲区） */
#define lv_MAX_NAME_LENGTH 256

/* lvRefCounted 及 lv_REF_* 宏已在 debug.h 中定义，此处不再重复 */

/** 全局画布的上下文深度（用于引擎初始化时的默认值） */
#define ENGINE_GLOBAL_CANVAS_DEPTH 0

/** @brief 重写-求解协作最大迭代次数 */
#define ENGINE_MAX_COLLABORATION_ITERATIONS 10000

/* ============================================================
 * 统一错误处理系统 v3.4.1
 * ============================================================
 *
 * 【设计原则】
 *   1. 引擎实例优先：所有错误信息优先存储在引擎实例中（engine->last_status/error）
 *   2. 全局状态回退：仅在引擎实例不可用时（如 engine_create 失败）使用线程局部状态
 *   3. 自动清理：成功操作后自动清除错误状态，避免错误滞留
 *   4. 线程安全：线程局部存储确保多线程环境下的错误隔离
 *
 * 【迁移状态】
 *   v3.4.1: 统一错误处理机制，消除双重错误报告问题
 *   - 新增 engine_set_error / engine_clear_error / engine_get_error 统一接口
 *   - 保留线程局部变量作为早期错误回退机制
 *   - 所有引擎操作统一使用实例级错误存储
 *
 * 【使用规范】
 *   - 引擎内部函数：使用 engine_set_error(engine, status, fmt, ...) 设置错误
 *   - 获取错误：使用 engine_get_error(engine, &status, buf, size) 获取完整错误信息
 *   - 清理错误：成功操作后自动调用 engine_clear_error(engine)
 * ============================================================ */

/* 线程局部回退状态 —— 仅在引擎实例不可用时使用 */
#define lv_ERROR_MSG_SIZE 512
static lv_THREAD_LOCAL EngineStatus g_thread_last_status = ENGINE_STATUS_OK;
static lv_THREAD_LOCAL char g_thread_last_error[lv_ERROR_MSG_SIZE] = {0};

/* 错误缓冲区大小 */
#define lv_ENGINE_ERROR_SIZE 512

/**
 * @brief 设置引擎错误状态（统一接口）
 *
 * 优先将错误信息存储在引擎实例中，如果引擎为 NULL 则存储在线程局部变量中。
 * 使用可变参数支持格式化错误消息。
 *
 * @param engine 引擎实例（可为 NULL）
 * @param status 错误状态码
 * @param fmt 格式化字符串（printf 风格）
 * @param ... 可变参数
 */
static void engine_set_error(lvEngine *engine, EngineStatus status, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    if (engine) {
        engine->last_status = status;
        vsnprintf(engine->last_error, sizeof(engine->last_error), fmt, args);
    } else {
        g_thread_last_status = status;
        vsnprintf(g_thread_last_error, sizeof(g_thread_last_error), fmt, args);
    }
    
    va_end(args);
}

/**
 * @brief 将引擎状态码转换为可读字符串（公共API实现）
 *
 * 提供所有 EngineStatus 枚举值的人类可读描述。
 *
 * @param status 引擎状态码
 * @return 状态描述字符串（静态常量，无需释放）
 */
const char *engine_status_to_string(EngineStatus status) {
    switch (status) {
        case ENGINE_STATUS_OK: return "成功";
        case ENGINE_STATUS_OUT_OF_MEMORY: return "内存不足";
        case ENGINE_STATUS_INVALID_ARGUMENT: return "无效参数";
        case ENGINE_STATUS_INVALID_STATE: return "无效状态";
        case ENGINE_STATUS_ERROR_INTERNAL: return "内部错误";
        case ENGINE_STATUS_CONSTRAINT_CONFLICT: return "约束冲突";
        case ENGINE_STATUS_MODULE_ERROR: return "模块错误";
        default: return "未知错误";
    }
}

/**
 * @brief 将引擎状态码转换为英文标识符字符串（公共API实现）
 *
 * 返回状态码的英文标识符，适合用于日志和程序逻辑判断。
 *
 * @param status 引擎状态码
 * @return 英文标识符字符串（静态常量，无需释放）
 */
const char *engine_status_to_identifier(EngineStatus status) {
    switch (status) {
        case ENGINE_STATUS_OK: return "ENGINE_STATUS_OK";
        case ENGINE_STATUS_OUT_OF_MEMORY: return "ENGINE_STATUS_OUT_OF_MEMORY";
        case ENGINE_STATUS_INVALID_ARGUMENT: return "ENGINE_STATUS_INVALID_ARGUMENT";
        case ENGINE_STATUS_INVALID_STATE: return "ENGINE_STATUS_INVALID_STATE";
        case ENGINE_STATUS_ERROR_INTERNAL: return "ENGINE_STATUS_ERROR_INTERNAL";
        case ENGINE_STATUS_CONSTRAINT_CONFLICT: return "ENGINE_STATUS_CONSTRAINT_CONFLICT";
        case ENGINE_STATUS_MODULE_ERROR: return "ENGINE_STATUS_MODULE_ERROR";
        default: return "ENGINE_STATUS_UNKNOWN";
    }
}

/**
 * @brief 获取引擎状态的详细描述（公共API实现）
 *
 * 返回包含状态码、描述和建议操作的完整信息。
 *
 * @param status 引擎状态码
 * @return 详细描述字符串（静态常量，无需释放）
 */
const char *engine_status_get_description(EngineStatus status) {
    switch (status) {
        case ENGINE_STATUS_OK: 
            return "操作成功完成。系统处于正常状态，可以继续后续操作。";
        case ENGINE_STATUS_OUT_OF_MEMORY: 
            return "内存分配失败。系统无法分配所需的内存资源。建议：检查系统内存使用情况，尝试释放不必要的资源，或减小问题规模。";
        case ENGINE_STATUS_INVALID_ARGUMENT: 
            return "传入参数无效。可能是空指针、越界值或格式错误的参数。建议：检查函数调用的参数是否符合文档要求。";
        case ENGINE_STATUS_INVALID_STATE: 
            return "引擎处于无效状态。当前操作与引擎状态不兼容。建议：检查引擎当前状态，必要时调用 engine_reset() 重置。";
        case ENGINE_STATUS_ERROR_INTERNAL: 
            return "内部错误。系统内部出现意外情况。建议：检查日志获取详细信息，如果问题持续请报告给开发团队。";
        case ENGINE_STATUS_CONSTRAINT_CONFLICT: 
            return "约束冲突。几何约束之间存在矛盾，无法满足所有约束条件。建议：检查约束定义，移除或修改冲突的约束。";
        case ENGINE_STATUS_MODULE_ERROR: 
            return "模块错误。加载或执行模块/公理包时发生错误。建议：检查模块文件路径和格式是否正确。";
        default: 
            return "未知错误。系统遇到未识别的错误状态。建议：检查日志并报告问题。";
    }
}

/* 
 * 注意：以下宏定义用于向后兼容，但会导致问题，已移除。
 * 请直接使用 engine->last_status 和 engine->last_error 访问引擎状态。
 */

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
    lvEngine *engine = lv_malloc(sizeof(lvEngine));
    if (!engine) {
        g_thread_last_status = ENGINE_STATUS_OUT_OF_MEMORY;
        return NULL;
    }
    memset(engine, 0, sizeof(lvEngine));
    engine->rewrite_step_limit = lv_DEFAULT_REWRITE_STEP_LIMIT; /* 默认重写步数限制 */
    engine->frozen_point = NULL;
    engine->context = NULL; /* 迁移中：暂不绑定上下文，后续可通过 engine_bind_context() 设置 */
    engine->stream_ctx = stream_context_create(); /* 创建流式上下文 */
    if (!engine->stream_ctx) {
        lv_free((void **) &engine);
        g_thread_last_status = ENGINE_STATUS_OUT_OF_MEMORY;
        return NULL;
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
        g_thread_last_status = ENGINE_STATUS_OUT_OF_MEMORY;
        return NULL;
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

/**
 * @brief 引擎内部数组扩容辅助函数（委托给统一的 lv_ensure_capacity）
 *
 * @param arr      当前数组指针的地址（用于 realloc）
 * @param count    当前元素数量
 * @param capacity 当前容量的地址（会被更新为新容量）
 * @param elem_size 单个元素的字节大小
 * @return true 扩容成功（或无需扩容），false 失败（内存不足或溢出）
 * @note 内部委托给 lv_ensure_capacity，最小增长量为 1
 */
static bool engine_ensure_capacity(void **arr, int count, int *capacity, size_t elem_size) {
    return lv_ensure_capacity(arr, count, capacity, elem_size, 1);
}

/**
 * @brief 向引擎添加一条重写规则
 *
 * 将指定的重写规则追加到引擎的规则列表中。
 * 内部数组采用指数增长策略自动扩容。
 *
 * @param engine 引擎实例
 * @param rule   待添加的重写规则（指针所有权转移至引擎，调用者不应再释放）
 * @return true 添加成功，false 参数为 NULL 或内存不足
 */
bool engine_add_rewrite_rule(lvEngine *engine, const RewriteRule *rule) {
    if (!engine || !rule)
        return false;

    if (!engine_ensure_capacity((void **) &engine->rewrite_rules, engine->rewrite_rule_count,
                                &engine->rewrite_rule_capacity, sizeof(RewriteRule *)))
        return false;

    engine->rewrite_rules[engine->rewrite_rule_count++] = (RewriteRule *)rule;
    return true;
}

/**
 * @brief 从文件加载几何模块
 *
 * 创建临时模块实例，从指定文件路径解析并加载模块定义，
 * 加载成功后将其追加到引擎的模块列表中。
 *
 * @param engine   引擎实例
 * @param filepath 模块文件路径
 * @return 模块加载状态码（MODULE_LOAD_OK 表示成功）
 */
/**
 * @brief 从文件路径中提取基础文件名（不含目录和扩展名）
 *
 * 例如: "/path/to/my_module.lvmod" -> "my_module"
 *       "simple.lvmod" -> "simple"
 *       "no_extension" -> "no_extension"
 *
 * @param filepath 文件路径
 * @return 静态缓冲区中的文件名字符串（注意：非线程安全，调用后应立即使用）
 */
static const char *engine_extract_module_name(const char *filepath) {
    static __thread char name_buf[lv_MAX_NAME_LENGTH];
    if (!filepath) {
        name_buf[0] = '\0';
        return name_buf;
    }

    /* 找到最后一个路径分隔符 */
    const char *last_sep = strrchr(filepath, '/');
    const char *last_bsep = strrchr(filepath, '\\');
    if (last_bsep > last_sep)
        last_sep = last_bsep;
    const char *base = last_sep ? last_sep + 1 : filepath;

    /* 复制基础文件名 */
    lv_strlcpy(name_buf, base, lv_MAX_NAME_LENGTH);

    /* 去掉扩展名（最后一个 '.' 之后的部分） */
    char *dot = strrchr(name_buf, '.');
    if (dot)
        *dot = '\0';

    /* 如果提取后为空，回退到 "temp" */
    if (name_buf[0] == '\0')
        lv_strlcpy(name_buf, "temp", lv_MAX_NAME_LENGTH);

    return name_buf;
}

ModuleLoadStatus engine_load_module(lvEngine *engine, const char *filepath) {
    if (!engine || !filepath)
        return MODULE_LOAD_ERROR_INVALID_PATH;
    const char *module_name = engine_extract_module_name(filepath);
    Module *mod = module_create(module_name, "0.0.0");
    if (!mod) {
        engine->last_status = ENGINE_STATUS_OUT_OF_MEMORY;
        snprintf(engine->last_error, sizeof(engine->last_error), "模块创建失败");
        return MODULE_LOAD_MEMORY_ERROR;
    }
    ModuleLoadStatus status = module_load(mod, filepath, engine->loaded_modules, engine->module_count);
    if (status != MODULE_LOAD_OK) {
        module_destroy(mod);
        engine->last_status = ENGINE_STATUS_MODULE_ERROR;
        snprintf(engine->last_error, sizeof(engine->last_error), "模块加载失败 [文件=%s, 状态码=%d]", filepath, status);
        return status;
    }
    /* 指数增长策略：使用通用扩容辅助函数 */
    if (!engine_ensure_capacity((void **) &engine->loaded_modules, engine->module_count, &engine->module_capacity,
                                sizeof(Module *))) {
        module_destroy(mod);
        engine->last_status = ENGINE_STATUS_OUT_OF_MEMORY;
        return MODULE_LOAD_MEMORY_ERROR;
    }
    engine->loaded_modules[engine->module_count++] = mod;
    return MODULE_LOAD_OK;
}

/**
 * @brief 从文件加载公理包
 *
 * 创建临时公理包实例，从指定文件路径解析并加载公理定义，
 * 加载成功后将其追加到引擎的公理包列表中。
 * 内部数组采用指数增长策略自动扩容。
 *
 * @param engine   引擎实例
 * @param filepath 公理包文件路径
 * @return 公理加载状态码（AXIOM_LOAD_OK 表示成功）
 */
AxiomLoadStatus engine_load_axiom_package(lvEngine *engine, const char *filepath) {
    if (!engine || !filepath)
        return AXIOM_LOAD_NULL_POINTER;
    AxiomPackage *pkg = axiom_package_create("temp", "0.0.0");
    if (!pkg) {
        engine->last_status = ENGINE_STATUS_OUT_OF_MEMORY;
        snprintf(engine->last_error, sizeof(engine->last_error), "公理包创建失败");
        return AXIOM_LOAD_MEMORY_ERROR;
    }
    AxiomLoadStatus status = axiom_package_load(pkg, filepath);
    if (status != AXIOM_LOAD_OK) {
        axiom_package_destroy(pkg);
        engine->last_status = ENGINE_STATUS_MODULE_ERROR;
        snprintf(engine->last_error, sizeof(engine->last_error), "公理包加载失败 [文件=%s, 状态码=%d]", filepath, status);
        return status;
    }
    /* 指数增长策略：使用通用扩容辅助函数 */
    if (!engine_ensure_capacity((void **) &engine->axiom_packages, engine->axiom_package_count,
                                &engine->axiom_package_capacity, sizeof(AxiomPackage *))) {
        axiom_package_destroy(pkg);
        engine->last_status = ENGINE_STATUS_OUT_OF_MEMORY;
        return AXIOM_LOAD_MEMORY_ERROR;
    }
    engine->axiom_packages[engine->axiom_package_count++] = pkg;
    return AXIOM_LOAD_OK;
}

/**
 * @brief 更新端口节点的命名空间深度
 * @param n 目标节点
 * @param new_func_block_id 新函数块ID
 * @param context_depth 上下文深度
 * @param is_input 是否为输入端口（影响 is_formal_param 设置）
 */
static void update_port_namespace_depth(GeomNode *n, int new_func_block_id,
                                         int context_depth, bool is_input) {
    if (n && n->type == GEOM_PORT && n->data.port != NULL) {
        n->data.port->parent_block_id = new_func_block_id;
        n->data.port->is_formal_param = is_input;
        n->data.port->namespace_depth = n->data.port->namespace_depth - context_depth + 1;
        n->parent_block_id = new_func_block_id;
        n->namespace_depth = n->namespace_depth - context_depth + 1;
    }
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
bool engine_pack_function(lvEngine *engine, const int *internal_node_ids, int internal_count, const int *input_port_ids,
                          int input_count, const int *output_port_ids, int output_count, int *out_func_block_id) {
    if (!engine || !engine->main_graph || (internal_count > 0 && !internal_node_ids) || (input_count > 0 && !input_port_ids) || (output_count > 0 && !output_port_ids)) {
        engine_set_error(engine, ENGINE_STATUS_INVALID_ARGUMENT, "引擎或主图为空");
        return false;
    }
    for (int i = 0; i < internal_count; i++) {
        GeomNode *n = graph_get_node(engine->main_graph, internal_node_ids[i]);
        if (!n) {
            engine_set_error(engine, ENGINE_STATUS_INVALID_STATE, "打包函数块失败: 内部节点 %d 不存在", internal_node_ids[i]);
            return false;
        }
    }
    for (int i = 0; i < input_count; i++) {
        GeomNode *n = graph_get_node(engine->main_graph, input_port_ids[i]);
        if (!n || n->type != GEOM_PORT) {
            engine_set_error(engine, ENGINE_STATUS_INVALID_STATE, "打包函数块失败: 输入端口 %d 不存在或不是端口类型",
                     input_port_ids[i]);
            return false;
        }
    }
    for (int i = 0; i < output_count; i++) {
        GeomNode *n = graph_get_node(engine->main_graph, output_port_ids[i]);
        if (!n || n->type != GEOM_PORT) {
            engine_set_error(engine, ENGINE_STATUS_INVALID_STATE, "打包函数块失败: 输出端口 %d 不存在或不是端口类型",
                     output_port_ids[i]);
            return false;
        }
    }
    /* v3.4.1 改进：使用 graph_get_last_added_node_id() 安全获取新节点 ID，
     * 避免依赖 next_node_id - 1 的推断方式，消除悬空指针风险。
     * graph_add_function_block 成功后会更新最后添加节点 ID。 */
    AddNodeResult result = graph_add_function_block(engine->main_graph, internal_node_ids, internal_count,
                                                    input_port_ids, input_count, output_port_ids, output_count);
    if (result != ADD_NODE_OK) {
        engine_set_error(engine, ENGINE_STATUS_INVALID_STATE, "创建函数块失败（图操作返回错误: %d）", result);
        return false;
    }
    
    /* 安全获取新创建的函数块 ID */
    int new_func_block_id = graph_get_last_added_node_id(engine->main_graph);
    
    /* 验证获取的 ID 有效 */
    if (new_func_block_id < 0) {
        /* lv_LOG_ERROR("engine_add_function_block: 无法获取有效的函数块 ID"); */
        engine_set_error(engine, ENGINE_STATUS_ERROR_INTERNAL, "无法获取有效的函数块 ID");
        return false;
    }
    
    /* 二次验证：确保 ID 对应的节点存在且类型正确 */
    GeomNode *new_fb_node = graph_get_node(engine->main_graph, new_func_block_id);
    if (!new_fb_node || new_fb_node->type != GEOM_FUNCTION_BLOCK) {
        lv_LOG_ERROR("engine_add_function_block: ID=%d 对应的节点不存在或类型不是函数块", new_func_block_id);
        engine_set_error(engine, ENGINE_STATUS_ERROR_INTERNAL, "函数块节点验证失败");
        return false;
    }
    if (out_func_block_id) {
        *out_func_block_id = new_func_block_id;
    }
    /* context_depth is the current canvas depth (0 for global canvas) */
    int context_depth = ENGINE_GLOBAL_CANVAS_DEPTH;
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
        update_port_namespace_depth(n, new_func_block_id, context_depth, true);
    }
    for (int i = 0; i < output_count; i++) {
        GeomNode *n = graph_get_node(engine->main_graph, output_port_ids[i]);
        update_port_namespace_depth(n, new_func_block_id, context_depth, false);
    }
    return true;
}

int *engine_instantiate_function(lvEngine *engine, int func_block_id, const int *arg_mappings, int arg_count,
                                 int *out_result_count) {
    if (!out_result_count) {
        engine_set_error(engine, ENGINE_STATUS_INVALID_ARGUMENT, "out_result_count 不能为 NULL");
        return NULL;
    }
    if (!engine || !engine->main_graph) {
        *out_result_count = 0;
        engine_set_error(engine, ENGINE_STATUS_INVALID_ARGUMENT, "引擎或主图为空");
        return NULL;
    }
    *out_result_count = 0;

    GeomNode *func_block = graph_get_node(engine->main_graph, func_block_id);
    if (!func_block || func_block->type != GEOM_FUNCTION_BLOCK) {
        engine_set_error(engine, ENGINE_STATUS_INVALID_STATE, "函数块 %d 不存在或类型不是函数块", func_block_id);
        return NULL;
    }

    /*
     * 构建独立的 FuncBlock 描述符，从 GeomNode 中提取函数块数据，
     * 以便调用 func_block_instantiate()，实现完整的 beta-归约和变量捕获解析
     * （设计文档 Section 3.3 中的 A/B/C 三种情况）。
     */
    FuncBlock *fb = func_block_create(func_block_id);
    if (!fb) {
        engine_set_error(engine, ENGINE_STATUS_OUT_OF_MEMORY, "func_block_create 分配失败");
        return NULL;
    }

    /* 拷贝内部节点ID */
    if (func_block->data.func_block.internal_node_count > 0) {
        int ic = func_block->data.func_block.internal_node_count;
        int *ids = lv_malloc((size_t) ic * sizeof(int));
        if (!ids) {
            func_block_destroy(fb);
            engine_set_error(engine, ENGINE_STATUS_OUT_OF_MEMORY, "内部节点ID数组分配失败");
            return NULL;
        }
        for (int i = 0; i < ic; i++) {
            if (!func_block->data.func_block.internal_nodes[i]) {
                func_block_destroy(fb);
                lv_free((void **) &ids);
                engine_set_error(engine, ENGINE_STATUS_INVALID_STATE, "内部节点 %d 为空", i);
                return NULL;
            }
            ids[i] = func_block->data.func_block.internal_nodes[i]->id;
        }
        func_block_set_internal_nodes(fb, ids, ic);
        lv_free((void **) &ids);
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
    InstantiateResult inst_result =
        func_block_instantiate(fb, engine->main_graph, (int *)arg_mappings, arg_count, &new_node_ids, &new_node_count);

    func_block_destroy(fb);

    if (inst_result != INSTANTIATE_OK) {
        engine_set_error(engine, ENGINE_STATUS_INVALID_STATE, "engine_instantiate_function: instantiation failed (code %d)",
                 inst_result);
        return NULL;
    }

    *out_result_count = new_node_count;
    return new_node_ids;
}

UnifyStatus engine_unify(lvEngine *engine, ConstraintGraph *construction, ConstraintGraph *proposition) {
    /* 参数校验：任一参数为 NULL 视为调用方错误 */
    if (!engine || !construction || !proposition) {
        if (engine) {
            engine->last_unify_status = lv_ERROR_INVALID_PARAM;
            engine->last_status = ENGINE_STATUS_INVALID_ARGUMENT;
            snprintf(engine->last_error, sizeof(engine->last_error),
                     "engine_unify: 空指针参数 (engine=%p, construction=%p, proposition=%p)", (void *) engine,
                     (void *) construction, (void *) proposition);
        }
        lv_set_error(lv_ERROR_INVALID_PARAM, "engine_unify: 空指针参数");
        return UNIFY_STATUS_FAILED;
    }

    /* 执行合一操作：委托给合一检查器模块 */
    UnifyStatus status = unify_construction_with_proposition(construction, proposition);

    /* 同步错误状态到引擎实例 */
    engine->last_unify_status = status;
    engine->last_status = ENGINE_STATUS_OK;

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
        engine->last_status = ENGINE_STATUS_CONSTRAINT_CONFLICT;
        snprintf(engine->last_error, sizeof(engine->last_error), "合一失败 [状态码=%d]: %s", (int) status, reason);
        lv_set_error(lv_ERROR_UNIFY_FAILED, reason);
    }

    return status;
}

/**
 * @brief 获取引擎最近一次操作的状态码
 *
 * 优先使用引擎实例级别的错误状态（每个引擎独立隔离）。
 * 若无引擎实例，回退到线程局部变量。
 * 当 engine 为 NULL 且线程局部变量未被初始化时，返回 ENGINE_STATUS_INVALID_ARGUMENT。
 *
 * @param[in] engine 引擎实例（可为 NULL，此时回退到线程局部状态）
 * @return 最近一次操作的状态码
 */
EngineStatus engine_get_last_status(const lvEngine *engine) {
    if (engine) {
        return engine->last_status;
    }
    /* 回退到线程局部变量（LEGACY 模式） */
    return g_thread_last_status;
}

/**
 * @brief 获取引擎最近一次错误的描述字符串
 *
 * @param[in] engine 引擎实例（为 NULL 时返回全局错误字符串）
 * @return 内部静态错误字符串指针。调用者不得 free。
 *         在下一次可能修改错误状态的操作前有效。
 *         如无错误，返回空字符串。
 */
const char *engine_get_last_error(const lvEngine *engine) {
    if (engine) {
        return engine->last_error;
    }
    /* 回退到线程局部变量（LEGACY 模式） */
    return g_thread_last_error;
}

/**
 * @brief engine_solve - 完整求解流水线
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
static int check_and_report_conflicts(lvEngine *engine, const char *context) {
    int conflict_count = 0;
    int **conflicts = graph_detect_conflicts(engine->main_graph, &conflict_count, NULL);
    if (conflicts) {
        for (int i = 0; i < conflict_count; i++) {
            lv_free((void **) &conflicts[i]);
        }
        lv_free((void **) &conflicts);
    }
    if (conflict_count > 0) {
        engine->last_status = ENGINE_STATUS_CONSTRAINT_CONFLICT;
        snprintf(engine->last_error, sizeof(engine->last_error), "%s: 检测到 %d 个冲突", context, conflict_count);
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
static EngineSolveResult run_solver_on_graph(lvEngine *engine, const char *context) {
    int *dirty_ids = NULL;
    int free_count = count_degrees_of_freedom(engine->main_graph, &dirty_ids);
    if (free_count < 0) {
        /* count_degrees_of_freedom 返回 -1 表示内部错误（如内存分配失败） */
        engine_set_error(engine, ENGINE_STATUS_ERROR_INTERNAL, "%s: 计算自由度失败", context);
        lv_free((void **) &dirty_ids);
        return ENGINE_SOLVE_CONFLICT;
    }
    if (free_count > 0 && dirty_ids) {
        GroebnerResult *result = NULL;
        SolverStatus sstatus = solve_algebraic_system(engine->main_graph, dirty_ids, free_count, &result);
        if (result)
            groebner_result_destroy(result);
        lv_free((void **) &dirty_ids);

        if (sstatus == SOLVER_STATUS_NO_SOLUTION || sstatus == SOLVER_STATUS_OVERCONSTRAINED) {
            engine->last_status = ENGINE_STATUS_CONSTRAINT_CONFLICT;
            snprintf(engine->last_error, sizeof(engine->last_error), "%s: 求解器检测到冲突", context);
            return ENGINE_SOLVE_CONFLICT;
        }
        if (sstatus == SOLVER_STATUS_TIMEOUT) {
            engine->last_status = ENGINE_STATUS_CONSTRAINT_CONFLICT;
            snprintf(engine->last_error, sizeof(engine->last_error), "%s: 求解器超时", context);
            return ENGINE_SOLVE_TIMEOUT;
        }
        return ENGINE_SOLVE_OK;
    }
    lv_free((void **) &dirty_ids);
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
EngineSolveResult engine_solve(lvEngine *engine) {
    /* P2修复: 迁移到 engine 实例变量，移除全局 TLS 状态依赖 */
    if (!engine) return ENGINE_SOLVE_ERROR;
    if (!engine->main_graph) {
        engine->last_status = ENGINE_STATUS_INVALID_STATE;
        snprintf(engine->last_error, sizeof(engine->last_error), "求解失败: 约束图为空");
        return ENGINE_SOLVE_ERROR;
    }

    /* 流式事件: 引擎开始 */
    engine_emit_stream_event(engine, STREAM_EVENT_ENGINE_START, "求解流程启动", 0, -1, -1);

    /* 步骤0：归一化约束图
     * 根据 design_v2.9.md Section 18.1：求解前进行归一化可消除冗余节点并规范化图结构。
     * 归一化失败时记录警告，但不中断求解流程（归一化是优化步骤，
     * 即使失败，求解器仍可尝试求解原始图）。 */
    {
        engine_emit_stream_event(engine, STREAM_EVENT_NORMALIZE_START, "开始图规范化", 0, -1, -1);

        NormalizationResult *norm = graph_normalize(engine->main_graph, false);
        if (!norm) {
            /* 归一化失败（可能内存不足或图状态异常），记录警告继续执行。
             * 求解器将在未归一化的图上运行，结果可能不如预期。 */
            snprintf(engine->last_error, sizeof(engine->last_error),
                     "engine_solve: graph_normalize 返回 NULL，将继续在未规范化的图上求解");
            engine_emit_stream_event(engine, STREAM_EVENT_WARNING, "图规范化失败，将继续在未规范化的图上求解", 0, -1,
                                     -1);
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
    int rewrite_limit = engine->rewrite_step_limit > 0 ? engine->rewrite_step_limit : lv_DEFAULT_REWRITE_STEP_LIMIT;
    if (engine->rewrite_rule_count > 0) {
        engine_emit_stream_event(engine, STREAM_EVENT_REWRITE_START, "开始重写阶段", 1, -1, -1);

        RewriteStatus rstatus =
            rewrite_with_rules(engine->main_graph, engine->rewrite_rules, engine->rewrite_rule_count, rewrite_limit,
                               false /* normalize_between_steps: 默认禁用 */
            );

        if (rstatus == REWRITE_STATUS_TERMINATED) {
            engine_emit_stream_event(engine, STREAM_EVENT_ERROR, "重写终止（可能循环）", 1, -1, -1);
            engine->last_status = ENGINE_STATUS_CONSTRAINT_CONFLICT;
            snprintf(engine->last_error, sizeof(engine->last_error), "engine_solve: 重写终止（可能存在循环）");
            return ENGINE_SOLVE_TIMEOUT;
        }
        engine_emit_stream_event(engine, STREAM_EVENT_REWRITE_DONE, "重写阶段完成", 1, -1, -1);
    }

    /* 步骤2：如果重写未能完全化简，则对剩余约束调用求解器 */
    int node_count = graph_get_node_count(engine->main_graph);
    if (node_count > 0) {
        engine_emit_stream_event(engine, STREAM_EVENT_SOLVE_START, "开始代数求解", 2, -1, -1);

        EngineSolveResult solver_result = run_solver_on_graph(engine, "engine_solve");
        if (solver_result != ENGINE_SOLVE_OK) {
            engine_emit_stream_event(engine, STREAM_EVENT_ERROR, "代数求解失败", 2, -1, -1);
            return solver_result;
        }
        engine_emit_stream_event(engine, STREAM_EVENT_SOLVE_DONE, "代数求解完成", 2, -1, -1);
    }

    /* 步骤3：检查冲突 */
    if (check_and_report_conflicts(engine, "engine_solve") != 0) {
        engine_emit_stream_event(engine, STREAM_EVENT_CONFLICT_DETECTED, "检测到约束冲突", 3, -1, -1);
        return ENGINE_SOLVE_CONFLICT;
    }

    /* 步骤4：更新自由度信息（重新计数） */
    int *free_var_ids = NULL;
    int free_count = count_degrees_of_freedom(engine->main_graph, &free_var_ids);
    if (free_count < 0) {
        /* lv_LOG_WARNING("engine_solve: 重新计算自由度失败，使用上一次的值"); */
        free_count = 0; /* 使用安全默认值 */
    }
    lv_free((void **) &free_var_ids);

    /* 流式事件: 引擎完成 */
    {
        StreamEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = STREAM_EVENT_ENGINE_DONE;
        ev.timestamp_ms = stream_timestamp_ms();
        ev.step_number = 4;
        ev.node_id = free_count; /* 自由度为附加值 */
        ev.description = "求解流程完成";
        stream_emit(engine->stream_ctx, &ev);
    }

    engine->last_status = ENGINE_STATUS_OK;
    engine->last_error[0] = '\0';
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
int engine_rewrite_and_solve(lvEngine *engine, int max_rewrite_steps, int max_solve_steps) {
    if (!engine || !engine->main_graph) {
        engine->last_status = ENGINE_STATUS_INVALID_STATE;
        snprintf(engine->last_error, sizeof(engine->last_error), "重写-求解协作失败: 引擎实例或约束图为空 (engine=%p)",
                 (void *) engine);
        return -1;
    }

    /* 流式事件: 引擎开始 */
    engine_emit_stream_event(engine, STREAM_EVENT_ENGINE_START, "重写-求解协作流程启动", 0, -1, -1);

    /* 若调用方传入0或负值，则使用引擎的可配置步数限制 */
    if (max_rewrite_steps <= 0) {
        max_rewrite_steps =
            engine->rewrite_step_limit > 0 ? engine->rewrite_step_limit : lv_DEFAULT_REWRITE_STEP_LIMIT;
    }

    int total_steps = 0;
    int remaining_rewrite = max_rewrite_steps;
    int remaining_solve = max_solve_steps;
    int iteration = 0; /* 迭代轮次计数 */

    /* 初始化WL哈希历史记录用于循环检测 */
    WLHashHistory wl_history;
    wl_history_init(&wl_history);

    /* 外层循环：交替执行重写和求解 */
    while (remaining_rewrite > 0 || remaining_solve > 0) {
        iteration++;

        /* 总迭代次数安全限制：防止重写-求解交替无限循环 */
        if (iteration > ENGINE_MAX_COLLABORATION_ITERATIONS) {
            engine_emit_stream_event(engine, STREAM_EVENT_ERROR, "重写-求解协作超过最大迭代次数限制 (" lv_TOSTRING(ENGINE_MAX_COLLABORATION_ITERATIONS) ")", iteration, -1, -1);
            engine->last_status = ENGINE_STATUS_CONSTRAINT_CONFLICT;
            snprintf(engine->last_error, sizeof(engine->last_error),
                     "engine_rewrite_and_solve: 总迭代次数超过上限 " lv_TOSTRING(ENGINE_MAX_COLLABORATION_ITERATIONS) "，终止执行");
            wl_history_destroy(&wl_history);
            return -2;
        }

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

            RewriteStatus rstatus =
                rewrite_with_rules(engine->main_graph, engine->rewrite_rules, engine->rewrite_rule_count,
                                   remaining_rewrite, true /* normalize_between_steps: 为求解循环启用 */
                );

            int after_constraints = graph_get_constraint_count(engine->main_graph);
            int rewrite_progress = before_constraints - after_constraints;
            total_steps += (rewrite_progress > 0) ? rewrite_progress : 1;
            remaining_rewrite = 0;

            if (rstatus == REWRITE_STATUS_TERMINATED) {
                engine_emit_stream_event(engine, STREAM_EVENT_ERROR, "重写终止（循环）", iteration, -1, -1);
                engine->last_status = ENGINE_STATUS_CONSTRAINT_CONFLICT;
                snprintf(engine->last_error, sizeof(engine->last_error), "engine_rewrite_and_solve: 重写终止（存在循环）");
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
                ev.node_id = rewrite_progress; /* 约束减少量 */
                ev.constraint_id = after_constraints;
                ev.description = "重写轮次完成";
                ev.progress = -1.0;
                stream_emit(engine->stream_ctx, &ev);
            }

            /* 重写阶段后的WL图哈希循环检测 */
            RewriteStatus loop_status = detect_rewrite_loop_wl(engine->main_graph, &wl_history);
            if (loop_status == REWRITE_STATUS_TERMINATED) {
                engine_emit_stream_event(engine, STREAM_EVENT_ERROR, "WL哈希循环检测触发", iteration, -1, -1);
                engine->last_status = ENGINE_STATUS_CONSTRAINT_CONFLICT;
                snprintf(engine->last_error, sizeof(engine->last_error), "engine_rewrite_and_solve: 在第 %d 步通过WL哈希检测到重写循环",
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
            engine_emit_stream_event(engine, STREAM_EVENT_SOLVE_START, "重写停滞，调用代数求解", iteration, -1, -1);

            EngineSolveResult solver_result = run_solver_on_graph(engine, "engine_rewrite_and_solve");
            if (solver_result != ENGINE_SOLVE_OK) {
                engine_emit_stream_event(engine, STREAM_EVENT_ERROR, "代数求解失败", iteration, -1, -1);
                wl_history_destroy(&wl_history);
                return -(int) solver_result - 1;
            }
            total_steps += 1;
            remaining_solve = 0;

            engine_emit_stream_event(engine, STREAM_EVENT_SOLVE_DONE, "代数求解完成，返回重写阶段", iteration, -1, -1);

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
        engine_emit_stream_event(engine, STREAM_EVENT_CONFLICT_DETECTED, "最终冲突检查失败", total_steps, -1, -1);
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

    engine->last_status = ENGINE_STATUS_OK;
    engine->last_error[0] = '\0';
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
EngineCircuitResult engine_handle_circuit_trip(lvEngine *engine) {
    if (!engine) {
        return ENGINE_CIRCUIT_IGNORE;
    }

    /* 步骤1：检查是否存在冻结点 */
    if (circuit_has_frozen_point()) {
        /* 存在冻结点，调用方可能需要回滚到该点 */
        int overflow = circuit_get_overflow_count();

        /* 步骤2：若 overflow_count >= lv_CIRCUIT_OVERFLOW_THRESHOLD，建议永久降级 */
        if (overflow >= lv_CIRCUIT_OVERFLOW_THRESHOLD) {
            snprintf(engine->last_error, sizeof(engine->last_error),
                     "engine_handle_circuit_trip: 溢出计数 %d >= %d，"
                     "建议永久降级为琥珀色",
                     overflow, lv_CIRCUIT_OVERFLOW_THRESHOLD);
            engine->last_status = ENGINE_STATUS_CONSTRAINT_CONFLICT;
            return ENGINE_CIRCUIT_DOWNGRADE;
        }

        /* 存在冻结点但溢出计数可控，建议回滚 */
        snprintf(engine->last_error, sizeof(engine->last_error),
                 "engine_handle_circuit_trip: 存在冻结点，"
                 "溢出计数 %d，建议回滚",
                 overflow);
        engine->last_status = ENGINE_STATUS_OK;
        return ENGINE_CIRCUIT_ROLLBACK;
    }

    /* 步骤3：无冻结点，仅报告警告 */
    int overflow = circuit_get_overflow_count();
    if (overflow >= lv_CIRCUIT_OVERFLOW_THRESHOLD) {
        /* 即使没有冻结点，反复溢出也建议降级 */
        snprintf(engine->last_error, sizeof(engine->last_error),
                 "engine_handle_circuit_trip: 溢出计数 %d >= %d，"
                 "建议永久降级为琥珀色",
                 overflow, lv_CIRCUIT_OVERFLOW_THRESHOLD);
        engine->last_status = ENGINE_STATUS_CONSTRAINT_CONFLICT;
        return ENGINE_CIRCUIT_DOWNGRADE;
    }

    snprintf(engine->last_error, sizeof(engine->last_error), "engine_handle_circuit_trip: 溢出计数 %d，已处理（忽略）", overflow);
    engine->last_status = ENGINE_STATUS_OK;
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
EngineCircuitResult engine_handle_circuit_trip_with_action(lvEngine *engine, EngineCircuitAction action) {
    if (!engine)
        return ENGINE_CIRCUIT_ERROR;

    SymbolicCoord *overflow_coord = circuit_get_last_result();

    switch (action) {
        case ENGINE_CIRCUIT_ACTION_IGNORE: /* 忽略：将坐标标记为琥珀色并继续 */
        if (overflow_coord) {
            symbolic_coord_set_trust(overflow_coord, TRUST_AMBER);
        }
        circuit_handle_overflow();
        g_thread_last_status = ENGINE_STATUS_OK;
        g_thread_last_error[0] = '\0';
        return ENGINE_CIRCUIT_IGNORE;

    case ENGINE_CIRCUIT_ACTION_ROLLBACK: /* 回滚：恢复到冻结点 */
        if (engine->frozen_point) {
            if (!engine_restore_frozen_point(engine, engine->frozen_point)) {
                /* lv_LOG_WARNING("engine: 回滚到冻结点失败，引擎状态可能不一致"); */
            }
            /* engine_restore_frozen_point 消费了 frozen_point，已置 NULL */
        }
        circuit_reset_context();
        g_thread_last_status = ENGINE_STATUS_OK;
            snprintf(g_thread_last_error, sizeof(g_thread_last_error), "engine: 通过回滚到冻结点处理了电路跳闸");
            return ENGINE_CIRCUIT_ROLLBACK;

        case ENGINE_CIRCUIT_ACTION_DOWNGRADE: /* 永久降级：替换为高精度浮点数近似值 */
            if (overflow_coord) {
                /* 将溢出坐标永久降级为高精度有理数近似值。
                 * 策略：创建新的有理数坐标，然后将其内容原地替换到
                 * overflow_coord 中，使得所有引用该坐标的节点都能
                 * 看到降级后的值。 */
                double approx = symbolic_coord_to_double(overflow_coord);
                if (fabs(approx) > lv_VALUE_TOO_LARGE) {
                    /* 值过大，无法用 int64_t 分子精确表示，仅标记琥珀色 */
                    symbolic_coord_set_trust(overflow_coord, TRUST_AMBER);
                } else {
                    SymbolicCoord *new_coord = symbolic_coord_create_rational(
                        (int64_t) (approx * lv_DOWNGRADE_DENOMINATOR), lv_DOWNGRADE_DENOMINATOR);
                    if (new_coord) {
                        symbolic_coord_set_trust(new_coord, TRUST_AMBER);
                        /* 原地替换：先释放 overflow_coord 的内部数据（但不释放
                         * overflow_coord 结构体本身，因为外部仍持有其指针），
                         * 再将 new_coord 的数据转移过来，最后仅释放 new_coord 外壳。
                         * 注意：不能调用 symbolic_coord_destroy(overflow_coord)，
                         * 因为它会 lv_free((void **) &coord) 整个结构体，导致后续写入为 use-after-free。 */
                        switch (overflow_coord->type) {
                            case RATIONAL:
                                rational_destroy(overflow_coord->data.rational);
                                break;
                            case ALGEBRAIC:
                                algebraic_destroy(overflow_coord->data.algebraic);
                                break;
                            case QUADRATIC:
                                quadratic_destroy(overflow_coord->data.quadratic);
                                break;
                            case TRANSCENDENTAL:
                                transcendental_destroy(overflow_coord->data.transcendental);
                                break;
                            default:
                                break;
                        }
                        overflow_coord->type = new_coord->type;
                        overflow_coord->data = new_coord->data;
                        overflow_coord->trust = new_coord->trust;
                        /* 仅释放 new_coord 的外壳，不释放其内部数据（已转移至 overflow_coord） */
                        lv_free((void **)&new_coord);
                    } else {
                        /* new_coord 创建失败，仅标记琥珀色作为降级 */
                        symbolic_coord_set_trust(overflow_coord, TRUST_AMBER);
                    }
                }
            }
            circuit_handle_overflow();
            g_thread_last_status = ENGINE_STATUS_OK;
            snprintf(g_thread_last_error, sizeof(g_thread_last_error), "engine: 通过永久降级为琥珀色处理了电路跳闸");
            return ENGINE_CIRCUIT_DOWNGRADE;

        default:
            g_thread_last_status = ENGINE_STATUS_INVALID_STATE;
            snprintf(g_thread_last_error, sizeof(g_thread_last_error), "engine_handle_circuit_trip_with_action: 无效的动作 %d", action);
            return ENGINE_CIRCUIT_ERROR;
    }
}

/* ================================================================
 * 重写步数限制配置
 * ================================================================ */

/**
 * @brief 设置引擎重写步数上限
 *
 * 若传入 limit <= 0，则自动使用默认值 lv_DEFAULT_REWRITE_STEP_LIMIT。
 *
 * @param engine 引擎实例
 * @param limit  新的步数上限（正值）
 */
void engine_set_rewrite_step_limit(lvEngine *engine, int limit) {
    if (!engine)
        return;
    if (limit <= 0)
        limit = lv_DEFAULT_REWRITE_STEP_LIMIT; /* 强制使用正数下限 */
    engine->rewrite_step_limit = limit;
}

/**
 * @brief 获取引擎重写步数上限
 *
 * 若引擎为 NULL 或未设置有效值，返回默认值 lv_DEFAULT_REWRITE_STEP_LIMIT。
 *
 * @param engine 引擎实例
 * @return 当前重写步数上限
 */
int engine_get_rewrite_step_limit(const lvEngine *engine) {
    if (!engine)
        return lv_DEFAULT_REWRITE_STEP_LIMIT;
    return engine->rewrite_step_limit > 0 ? engine->rewrite_step_limit : lv_DEFAULT_REWRITE_STEP_LIMIT;
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
    if (!src)
        return NULL;

    ConstraintGraph *dst = graph_create();
    if (!dst)
        return NULL;

    /* 构建ID映射表：old_id -> new_id */
    int max_id = 0;
    for (int i = 0; i < src->node_count; i++) {
        if (src->nodes[i]->id > max_id)
            max_id = src->nodes[i]->id;
    }
    int *id_map = NULL;
    if (max_id > 0) {
        id_map = lv_calloc((size_t) (max_id + 1), sizeof(int));
        if (!id_map) {
            graph_destroy(dst);
            return NULL;
        }
        for (int i = 0; i <= max_id; i++)
            id_map[i] = -1;
    }

    /* 第一遍：深拷贝所有节点 */
    /* 预分配足够容量以容纳所有源节点 */
    if (src->node_count > 0) {
        dst->nodes = lv_malloc((size_t) src->node_count * sizeof(GeomNode *));
        if (!dst->nodes) {
            graph_destroy(dst);
            lv_free((void **) &id_map);
            return NULL;
        }
    }
    for (int i = 0; i < src->node_count; i++) {
        GeomNode *orig = src->nodes[i];
        GeomNode *copy = node_deep_copy_geom_node(orig, NULL);
        if (!copy) {
            /* 失败时清理 */
            graph_destroy(dst);
            lv_free((void **) &id_map);
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
        Constraint *copy_c = lv_malloc(sizeof(Constraint));
        if (!copy_c) {
            /* 约束分配失败：释放已复制的所有约束和整个目标图，返回 NULL */
            for (int k = 0; k < dst->constraint_count; k++) {
                lv_free((void **) &dst->constraints[k]->participants);
                lv_free((void **) &dst->constraints[k]);
            }
            lv_free((void **) &dst->constraints);
            graph_destroy(dst);
            lv_free((void **) &id_map);
            return NULL;
        }

        copy_c->id = dst->next_constraint_id++;
        copy_c->type = orig_c->type;
        copy_c->template_id = orig_c->template_id;
        copy_c->participant_count = orig_c->participant_count;

        if (orig_c->participant_count > 0) {
            copy_c->participants = lv_malloc((size_t) orig_c->participant_count * sizeof(int));
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
            tmp_cons = lv_malloc(sizeof(Constraint *));
        } else {
            tmp_cons = lv_realloc(dst->constraints, (size_t) (dst->constraint_count + 1) * sizeof(Constraint *));
        }
        if (tmp_cons) {
            dst->constraints = tmp_cons;
            dst->constraints[dst->constraint_count++] = copy_c;
        } else {
            /* realloc 失败：释放当前约束、已复制的所有约束、整个目标图和 id_map */
            lv_free((void **) &copy_c->participants);
            lv_free((void **) &copy_c);
            for (int k = 0; k < dst->constraint_count; k++) {
                lv_free((void **) &dst->constraints[k]->participants);
                lv_free((void **) &dst->constraints[k]);
            }
            lv_free((void **) &dst->constraints);
            graph_destroy(dst);
            lv_free((void **) &id_map);
            return NULL;
        }
    }

    lv_free((void **) &id_map);

    /* ---- 重建哈希索引 ----
     * graph_create() 初始化空的哈希表（node_index, constraint_index），
     * 但上面的深拷贝绕过了 graph_alloc_node/graph_alloc_constraint，
     * 所以哈希表仍然为空。使用与 constraint_graph.c 相同的 FNV-1a
     * 开放寻址方案重建它们。
     */
    {
        /* --- 重建 node_index --- */
        if (dst->node_count > 0) {
            int ni_cap = lv_NODE_INDEX_INITIAL_SIZE;
            while (ni_cap < dst->node_count * lv_ARRAY_GROWTH_FACTOR)
                ni_cap *= lv_ARRAY_GROWTH_FACTOR; /* 保持负载率 < 0.5 */

            lv_free((void **) &dst->node_index); /* 释放 graph_create 创建的空表 */
            dst->node_index = lv_calloc((size_t) ni_cap, sizeof(GeomNode *));
            if (!dst->node_index) {
                graph_destroy(dst);
                return NULL;
            }
            dst->node_index_capacity = ni_cap;
            for (int i = 0; i < dst->node_count; i++) {
                unsigned idx = (unsigned) dst->nodes[i]->id * lv_FNV_HASH_MULTIPLIER & (unsigned) (ni_cap - 1);
                while (dst->node_index[idx] != NULL) {
                    idx = (idx + 1) & (unsigned) (ni_cap - 1);
                }
                dst->node_index[idx] = dst->nodes[i];
            }
        }

        /* --- 重建 constraint_index --- */
        if (dst->constraint_count > 0) {
            int ci_cap = lv_CONSTRAINT_INDEX_INITIAL_SIZE;
            while (ci_cap < dst->constraint_count * lv_ARRAY_GROWTH_FACTOR)
                ci_cap *= lv_ARRAY_GROWTH_FACTOR;

            lv_free((void **) &dst->constraint_index);
            dst->constraint_index = lv_calloc((size_t) ci_cap, sizeof(Constraint *));
            if (!dst->constraint_index) {
                graph_destroy(dst);
                return NULL;
            }
            dst->constraint_index_capacity = ci_cap;
            for (int i = 0; i < dst->constraint_count; i++) {
                unsigned idx =
                    (unsigned) dst->constraints[i]->id * lv_FNV_HASH_MULTIPLIER & (unsigned) (ci_cap - 1);
                while (dst->constraint_index[idx] != NULL) {
                    idx = (idx + 1) & (unsigned) (ci_cap - 1);
                }
                dst->constraint_index[idx] = dst->constraints[i];
            }
        }
    }

    return dst;
}

/** @brief 创建引擎状态冻结点 @details 保存当前引擎状态，用于后续回滚。 @param engine 引擎实例 @return 冻结点句柄，失败返回 NULL */
void *engine_create_frozen_point(lvEngine *engine) {
    if (!engine || !engine->main_graph)
        return NULL;

    ConstraintGraph *snapshot = graph_deep_copy(engine->main_graph);
    return (void *) snapshot;
}

/** @brief 恢复引擎状态到指定冻结点 @param engine 引擎实例 @param frozen_point 冻结点句柄 @return true 成功 */
bool engine_restore_frozen_point(lvEngine *engine, void *frozen_point) {
    if (!engine || !frozen_point)
        return false;

    ConstraintGraph *snapshot = (ConstraintGraph *) frozen_point;

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

/** @brief 销毁冻结点并释放关联资源 @param frozen_point 冻结点句柄 */
void engine_destroy_frozen_point(void *frozen_point) {
    if (!frozen_point)
        return;
    ConstraintGraph *snapshot = (ConstraintGraph *) frozen_point;
    graph_destroy(snapshot);
}

/* ================================================================
 * 流式输出 API
 * ================================================================ */

/** @brief 获取引擎的流式上下文 @param engine 引擎实例 @return 流式上下文指针 */
StreamContext *engine_get_stream_context(const lvEngine *engine) {
    if (!engine)
        return NULL;
    return engine->stream_ctx;
}

/** @brief 启用或禁用流式输出 @param engine 引擎实例 @param enabled true 启用 */
void engine_set_streaming_enabled(lvEngine *engine, bool enabled) {
    if (!engine)
        return;
    if (!enabled && engine->stream_ctx) {
        /* 禁用时销毁流式上下文，同步清空子模块 */
        stream_context_destroy(engine->stream_ctx);
        engine->stream_ctx = NULL;
        /* 通过分发机制统一清空所有已注册子模块的流式上下文 */
        stream_context_dispatch_all(NULL);
    } else if (enabled && !engine->stream_ctx) {
        /* 启用时重新创建流式上下文，通过分发机制同步到所有子模块 */
        engine->stream_ctx = stream_context_create();
        if (!engine->stream_ctx) {
            /* lv_LOG_ERROR("engine_set_streaming_enabled: stream_context_create() 返回 NULL，流式输出未能启用"); */
            return;
        }
        stream_context_dispatch_all(engine->stream_ctx);
    }
}

/** @brief 查询流式输出是否启用 @param engine 引擎实例 @return true 已启用 */
bool engine_is_streaming_enabled(const lvEngine *engine) {
    if (!engine)
        return false;
    return engine->stream_ctx != NULL;
}

/** @brief 发射流式事件 @param engine 引擎实例 @param event_type 事件类型 @param ... 事件数据 */
void engine_emit_stream_event(lvEngine *engine, StreamEventType type, const char *description, int step_number,
                              int node_id, int constraint_id) {
    if (!engine || !engine->stream_ctx)
        return;

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

/* ============================================================
 * 五状态机实现（v3.3.0 形式化）
 *
 * 从 context.h 中引入的状态机概念在引擎层的具体实现。
 * 引擎在其生命周期内严格遵循状态转移规则，
 * 所有状态变更通过 lv_engine_transition_state() 执行。
 * ============================================================ */

/**
 * @brief 状态转移表 —— 定义所有合法的状态转移
 *
 * 二维矩阵：engine_transition_table[from][to] = true（转移合法）/ false（非法）。
 * 数组大小为 5x5（ENGINE_STATE_IDLE..ENGINE_STATE_COMPLETE）。
 *
 * 转移规则：
 *   IDLE      → PARSING   ✓  开始接收新输入
 *   IDLE      → ERROR     ✓  初始化失败
 *   PARSING   → REASONING ✓  解析成功完成
 *   PARSING   → ERROR     ✓  解析失败
 *   PARSING   → IDLE      ✓  取消/中断
 *   REASONING → COMPLETE  ✓  证明/求解成功
 *   REASONING → ERROR     ✓  矛盾/超时/资源耗尽
 *   REASONING → IDLE      ✓  取消/中断
 *   COMPLETE  → IDLE      ✓  重置，准备新问题
 *   ERROR     → IDLE      ✓  重置，清理错误状态
 *
 * 所有其他组合均非法（如 IDLE → COMPLETE，COMPLETE → REASONING 等）。
 */
static const bool engine_transition_table[5][5] = {
    /* from \ to →        IDLE  PARSING  REASONING  ERROR  COMPLETE */
    /* IDLE      */  {  false,   true,     false,    true,   false },
    /* PARSING   */  {   true,  false,      true,    true,   false },
    /* REASONING */  {   true,  false,     false,    true,    true },
    /* ERROR     */  {   true,  false,     false,   false,   false },
    /* COMPLETE  */  {   true,  false,     false,   false,   false },
};

/**
 * @brief 检查状态转移是否合法
 *
 * 直接查转移表，O(1) 时间复杂度。
 *
 * @param from 当前状态
 * @param to   目标状态
 * @return true 合法，false 非法
 */
bool engine_is_valid_transition(EngineState from, EngineState to) {
    /* 边界检查：防止无效状态索引 */
    if (from > ENGINE_STATE_COMPLETE || to > ENGINE_STATE_COMPLETE) {
        return false;
    }
    return engine_transition_table[from][to];
}

/**
 * @brief 获取引擎状态的中文名称
 *
 * 返回静态字符串，调用者无需释放。
 *
 * @param state 引擎状态枚举值
 * @return 状态的中文描述字符串
 */
const char *engine_state_name(EngineState state) {
    switch (state) {
        case ENGINE_STATE_IDLE:
            return "空闲";       /* IDLE: 等待输入 */
        case ENGINE_STATE_PARSING:
            return "解析中";     /* PARSING: 解析输入文本 */
        case ENGINE_STATE_REASONING:
            return "推理中";     /* REASONING: 执行重写/求解/证明 */
        case ENGINE_STATE_ERROR:
            return "错误";       /* ERROR: 发生不可恢复错误 */
        case ENGINE_STATE_COMPLETE:
            return "完成";       /* COMPLETE: 推理成功完成 */
        default:
            return "未知状态";
    }
}

/**
 * @brief 尝试将引擎转移到指定状态
 *
 * 这是引擎状态机的核心 API。在执行任何可能改变引擎上下文的操作前，
 * 调用此函数来验证状态的合法性。
 *
 * 转移成功时：
 * - 记录 previous_state（用于审计和调试）
 * - 更新 state
 * - 递增 state_transition_count
 * - 返回 ENGINE_STATUS_OK
 *
 * 转移非法时：
 * - 不改变任何状态字段
 * - 设置 last_status = ENGINE_STATUS_INVALID_STATE
 * - 设置 last_error 描述尝试的非法转移
 * - 返回 ENGINE_STATUS_INVALID_STATE
 *
 * @param engine    引擎实例（非 NULL）
 * @param new_state 目标状态
 * @return ENGINE_STATUS_OK 成功，ENGINE_STATUS_INVALID_STATE 非法转移
 */
EngineStatus lv_engine_transition_state(lvEngine *engine, EngineState new_state) {
    /* 参数校验 */
    if (!engine) {
        return ENGINE_STATUS_INVALID_ARGUMENT;
    }

    EngineState current = engine->state;

    /* 边界检查：防止无效状态枚举值 */
    if (new_state < ENGINE_STATE_IDLE || new_state > ENGINE_STATE_COMPLETE) {
        engine->last_status = ENGINE_STATUS_INVALID_STATE;
        snprintf(engine->last_error, sizeof(engine->last_error),
                 "状态转移失败: 无效的目标状态 %d（合法范围: %d-%d）",
                 (int)new_state, ENGINE_STATE_IDLE, ENGINE_STATE_COMPLETE);
        return ENGINE_STATUS_INVALID_STATE;
    }

    /* 转移到相同状态 —— 允许（幂等），只记录日志不触发错误 */
    if (current == new_state) {
        /* 相同状态转移：这是一个 no-op，但计数仍然递增用于审计 */
        engine->state_transition_count++;
        return ENGINE_STATUS_OK;
    }

    /* 查转移表验证合法性 */
    if (!engine_is_valid_transition(current, new_state)) {
        engine->last_status = ENGINE_STATUS_INVALID_STATE;
        snprintf(engine->last_error, sizeof(engine->last_error),
                 "状态转移非法: 不能从 \"%s\" 转移到 \"%s\"（转移次数: %d）",
                 engine_state_name(current), engine_state_name(new_state),
                 engine->state_transition_count);
        return ENGINE_STATUS_INVALID_STATE;
    }

    /* 合法转移：记录并执行 */
    engine->previous_state = current;
    engine->state = new_state;
    engine->state_transition_count++;

    return ENGINE_STATUS_OK;
}

/**
 * @brief 获取引擎当前状态
 *
 * @param engine 引擎实例（可为 NULL）
 * @return 当前状态（NULL 时返回 ENGINE_STATE_IDLE）
 */
EngineState engine_get_state(const lvEngine *engine) {
    if (!engine) {
        return ENGINE_STATE_IDLE;
    }
    return engine->state;
}

/**
 * @brief 检查引擎是否正在忙于推理计算
 *
 * 引擎在 REASONING 状态下禁止接受新请求、修改约束图拓扑或执行销毁。
 * 此函数提供便捷的忙状态检查，供所有会修改引擎状态的公共 API 使用。
 *
 * @param engine 引擎实例（可为 NULL，返回 false 即非忙状态）
 * @return true 引擎繁忙（处于 REASONING），false 空闲
 */
bool engine_is_busy(const lvEngine *engine) {
    if (!engine)
        return false;
    return engine->state == ENGINE_STATE_REASONING;
}
