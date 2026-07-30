/**
 * @file lv_config.c
 * @brief Lv-00 运行时配置系统实现
 *
 * @details 提供运行时配置的集中管理，包含以下功能：
 *          - 默认配置（lv_config_default）：所有子系统参数的默认值定义
 *          - 当前配置管理（lv_config_current / lv_config_apply）：全局状态读写
 *          - 类型安全的 setter 函数（lv_config_set_*）：直接修改全局配置
 *          - 通用 key-value setter（lv_config_set_int / lv_config_set_double）
 *          - JSON 配置文件加载（lv_config_load_json）和导出（lv_config_to_json）
 *
 * 配置覆盖范围：求解器、约束图、重写、流式、精度、MiniKernel、SAT、
 * 压力测试、解析器、类型系统、运行时防护、协议、交互几何、ODE、
 * 证明、递归/上下文、互操作、日志、监控、插件、后端、测试、内存、健康等。
 *
 * @author Lv-00 Project
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_file.h"

#include "lv/lv.h"
#include "lv/lv_internal.h"
#include "lv/lv_json.h"
#include "lv/lv_parse_utils.h"
#include "lv/lv_utils.h"


static lvConfig g_active_config;
static int g_config_applied = 0;

/**
 * @brief 获取默认配置
 *
 * 返回静态默认配置结构体。所有字段预置为安全的默认值。
 * 函数内部使用 static initialized 标志实现线程安全的一次性初始化。
 *
 * @return 指向默认配置的常量指针
 */
const lvConfig *lv_config_default(void) {
    static lvConfig def;
    static int initialized = 0;
    if (initialized)
        return &def;
    initialized = 1;
    memset(&def, 0, sizeof(def));

    /* 求解器 */
    def.solver_max_var_id = 100000;
    def.solver_max_iterations = 10000;
    /* 约束图 */
    def.max_module_depth = 32;
    def.graph_adj_max_per_node = 256;
    /* 重写 */
    def.default_rewrite_limit = 1000;
    def.wl_iterations = 3;
    def.wl_history_size = 64;
    def.vf2_max_depth = 100;
    def.buchberger_max_steps = 50000;
    def.groebner_reduce_max_steps = 10000;
    /* 流式 */
    def.stream_async_queue_capacity = 1024;
    def.stream_initial_callbacks = 16;
    def.stream_max_callbacks = 64;
    def.stream_default_throttle_ms = 50;
    /* 精度 */
    def.bit_cutoff_threshold = 1000000;
    def.max_precision_bits = 100;
    def.continued_fraction_max_iter = 1000;
    def.max_subintervals = 4096;
    /* MiniKernel */
    def.mini_kernel_max_statements = 10000;
    def.mini_kernel_max_proof_depth = 1000;
    def.mini_kernel_verify_timeout_ms = 30000;
    /* SAT 求解器 */
    def.cdcl_max_steps = 1000;
    def.cdcl_max_decisions = 1000;
    def.cdcl_max_restarts = 10;
    /* 压力测试 */
    def.stress_test_default_chain = 100;
    def.stress_test_max_poly_degree = 4;
    /* 解析器 */
    def.parser_max_input_length = 1048576;
    def.parser_max_tokens = 100000;
    def.parser_max_ast_depth = 256;
    def.parser_max_ast_nodes = 500000;
    def.parser_max_token_length = 4096;
    def.parser_max_coordinates = 16;
    def.parser_max_vertices = 32;
    def.parser_max_polygon_vertices = 32;
    def.parser_max_statements = 64;
    def.parser_max_arguments = 16;
    def.parser_max_participants = 16;
    /* 类型系统 */
    def.type_infer_max_depth = 100;
    def.type_equiv_max_depth = 16;
    /* 防护 */
    def.runtime_guard_max_recurse = 128;
    def.runtime_guard_spin_attempts = 1024;
    def.runtime_guard_write_warn_us = 10000;
    /* 协议 */
    def.proto_max_draw_cmds = 4096;
    def.proto_max_table_rows = 512;
    def.proto_max_tree_nodes = 256;
    def.proto_max_topology = 128;
    def.proto_max_proof_steps = 512;
    def.proto_max_completions = 64;
    def.proto_max_terminal_lines = 512;
    /* 交互几何 */
    def.geo_max_objects = 1024;
    def.geo_max_constraints = 2048;
    def.geo_max_drag_chain = 64;
    def.geo_max_snapshots = 32;
    def.geo_min_zoom = 0.01;
    def.geo_max_zoom = 100.0;
    /* ODE */
    def.geoevol_max_param_dim = 256;
    def.geoevol_adams_max_order = 12;
    def.geoevol_max_rejections = 20;
    def.geoevol_min_step = 1e-15;
    def.geoevol_max_step = 1e10;
    def.geoevol_pi_smooth_factor = 0.25;
    /* 证明 */
    def.proof_max_depth = 100;
    def.proof_max_branches = 64;
    def.proof_max_strategies = 16;
    def.trace_tree_max_depth = 50;
    /* 递归/上下文 */
    def.max_recursion_depth = 128;
    def.context_default_max_depth = 100;
    def.context_max_recursion_depth = 10000;
    def.context_default_max_steps = 1000000;
    def.context_default_max_consecutive_errors = 10;
    def.context_reasoning_stack_default_capacity = 8;
    def.context_reasoning_stack_max_depth = 1000;
    /* 互操作 */
    def.interop_max_params = 32;
    def.interop_max_completions = 64;
    def.interop_ws_default_port = 8765;
    /* 日志 */
    def.log_max_files = 5;
    def.log_max_size = 10485760;
    def.log_ring_buffer_capacity = 256;
    /* 监控 */
    def.perf_sample_max_count = 10000;
    def.timer_max_depth = 32;
    /* 插件 */
    def.max_plugins = 256;
    def.max_interfaces = 128;
    /* 后端 */
    def.backend_step_limit = 1000;
    def.backend_timeout_ms = 30000;
    /* 测试 */
    def.test_max_suites = 256;
    def.test_max_cases = 4096;
    /* 烟测保护 */
    def.smoke_test_step_limit = 1000;
    def.smoke_test_timeout_ms = 30000;
    /* 熔断 */
    def.circuit_overflow_threshold = 3;
    /* 代数 */
    def.value_too_large = 1048576;
    def.downgrade_denominator = 100000;
    /* 内存 */
    def.default_memory_limit_mb = 0;
    /* 健康 */
    def.health_score_max = 100;
    def.health_memory_usage_ratio = 0.8;
    def.health_memory_warning_penalty = 10;
    def.health_memory_leak_ratio = 0.9;
    def.health_memory_leak_penalty = 20;
    def.health_recent_error_penalty = 5;
    def.context_timeout_ms = 30000;
    def.context_cooldown_ms = 5000;
    def.prop_max_iterations = 10000;
    def.prop_max_backtracks = 1000;
    def.prop_max_collaboration_iters = 10000;

    /* 高维几何 */
    def.high_dim_max_dimensions = 32;
    def.high_dim_max_depth = 32;
    def.high_dim_max_projection_presets = 64;
    def.high_dim_max_active_views = 16;
    def.high_dim_default_fidelity_threshold = 0.85;

    return &def;
}

/**
 * @brief 获取当前生效的配置
 *
 * 首次调用时从默认配置初始化全局配置。
 *
 * @return 指向当前配置的常量指针
 */
const lvConfig *lv_config_current(void) {
    if (!g_config_applied) {
        g_active_config = *lv_config_default();
        g_config_applied = 1;
    }
    return &g_active_config;
}

/**
 * @brief 应用新的配置
 *
 * 用传入配置覆盖全局配置，立即生效。
 *
 * @param cfg 新配置指针，不能为 NULL
 * @return 0 成功，-1 失败（cfg 为 NULL）
 */
int lv_config_apply(const lvConfig *cfg) {
    if (!cfg)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "cfg is NULL");
    g_active_config = *cfg;
    g_config_applied = 1;
    return 0;
}

/* ---- 类型安全 setter（直接改全局配置，立即生效） ---- */

/** @brief 获取可变的全局配置指针（内部辅助） */
static lvConfig *cfg_mut(void) {
    lv_config_current(); /* ensure initialized */
    return &g_active_config;
}

/**
 * @brief 设置求解器最大变量 ID
 * @param val 新值
 */
void lv_config_set_solver_max_var_id(int val) {
    cfg_mut()->solver_max_var_id = val;
}
/**
 * @brief 设置求解器最大迭代次数
 * @param val 新值
 */
void lv_config_set_solver_max_iterations(int val) {
    cfg_mut()->solver_max_iterations = val;
}
/**
 * @brief 设置证明最大深度
 * @param val 新值
 */
void lv_config_set_proof_max_depth(int val) {
    cfg_mut()->proof_max_depth = val;
}
/**
 * @brief 设置证明最大分支数
 * @param val 新值
 */
void lv_config_set_proof_max_branches(int val) {
    cfg_mut()->proof_max_branches = val;
}
/**
 * @brief 设置协议最大绘制命令数
 * @param val 新值
 */
void lv_config_set_proto_max_draw_cmds(int val) {
    cfg_mut()->proto_max_draw_cmds = val;
}
/**
 * @brief 设置协议最大证明步数
 * @param val 新值
 */
void lv_config_set_proto_max_proof_steps(int val) {
    cfg_mut()->proto_max_proof_steps = val;
}
/**
 * @brief 设置协议最大终端行数
 * @param val 新值
 */
void lv_config_set_proto_max_terminal_lines(int val) {
    cfg_mut()->proto_max_terminal_lines = val;
}
/**
 * @brief 设置交互几何最大对象数
 * @param val 新值
 */
void lv_config_set_geo_max_objects(int val) {
    cfg_mut()->geo_max_objects = val;
}
/**
 * @brief 设置交互几何最大约束数
 * @param val 新值
 */
void lv_config_set_geo_max_constraints(int val) {
    cfg_mut()->geo_max_constraints = val;
}
/**
 * @brief 设置交互几何最小缩放
 * @param val 新值
 */
void lv_config_set_geo_min_zoom(double val) {
    cfg_mut()->geo_min_zoom = val;
}
/**
 * @brief 设置交互几何最大缩放
 * @param val 新值
 */
void lv_config_set_geo_max_zoom(double val) {
    cfg_mut()->geo_max_zoom = val;
}
/**
 * @brief 设置解析器最大输入长度
 * @param val 新值
 */
void lv_config_set_parser_max_input_length(int val) {
    cfg_mut()->parser_max_input_length = val;
}
/**
 * @brief 设置解析器最大 AST 节点数
 * @param val 新值
 */
void lv_config_set_parser_max_ast_nodes(int val) {
    cfg_mut()->parser_max_ast_nodes = val;
}
/**
 * @brief 设置最大递归深度
 * @param val 新值
 */
void lv_config_set_max_recursion_depth(int val) {
    cfg_mut()->max_recursion_depth = val;
}
/**
 * @brief 设置默认重写限制
 * @param val 新值
 */
void lv_config_set_default_rewrite_limit(int val) {
    cfg_mut()->default_rewrite_limit = val;
}
/**
 * @brief 设置 ODE 最大参数维度
 * @param val 新值
 */
void lv_config_set_geoevol_max_param_dim(int val) {
    cfg_mut()->geoevol_max_param_dim = val;
}
/**
 * @brief 设置 ODE 最大拒绝次数
 * @param val 新值
 */
void lv_config_set_geoevol_max_rejections(int val) {
    cfg_mut()->geoevol_max_rejections = val;
}
/**
 * @brief 设置流式最大回调数
 * @param val 新值
 */
void lv_config_set_stream_max_callbacks(int val) {
    cfg_mut()->stream_max_callbacks = val;
}
/**
 * @brief 设置最大插件数
 * @param val 新值
 */
void lv_config_set_max_plugins(int val) {
    cfg_mut()->max_plugins = val;
}
void lv_config_set_context_timeout_ms(int val) {
    cfg_mut()->context_timeout_ms = val;
}
void lv_config_set_context_cooldown_ms(int val) {
    cfg_mut()->context_cooldown_ms = val;
}
void lv_config_set_prop_max_iterations(int val) {
    cfg_mut()->prop_max_iterations = val;
}
void lv_config_set_prop_max_backtracks(int val) {
    cfg_mut()->prop_max_backtracks = val;
}
void lv_config_set_prop_max_collaboration_iters(int val) {
    cfg_mut()->prop_max_collaboration_iters = val;
}

void lv_config_set_high_dim_max_dimensions(int val) {
    cfg_mut()->high_dim_max_dimensions = val;
}
void lv_config_set_high_dim_max_depth(int val) {
    cfg_mut()->high_dim_max_depth = val;
}
void lv_config_set_high_dim_max_projection_presets(int val) {
    cfg_mut()->high_dim_max_projection_presets = val;
}
void lv_config_set_high_dim_max_active_views(int val) {
    cfg_mut()->high_dim_max_active_views = val;
}
void lv_config_set_high_dim_default_fidelity_threshold(double val) {
    cfg_mut()->high_dim_default_fidelity_threshold = val;
}

/* ---- 通用 key-value setter ---- */

/**
 * @brief 通过字符串键设置整型配置项
 *
 * 在全局配置中查找与 key 匹配的整型字段并设置值。
 * 若未找到匹配项，返回 false 但不报错。
 *
 * @param key 配置项名称（字符串）
 * @param val 新值
 * @return true 设置成功，false 未找到匹配项或 key 为 NULL
 */
bool lv_config_set_int(const char *key, int val) {
    if (!key)
        return false;
    lvConfig *c = cfg_mut();

#define SET_IF(k, f)           \
    if (strcmp(key, k) == 0) { \
        c->f = val;            \
        return true;           \
    }
    SET_IF("solver_max_var_id", solver_max_var_id)
    SET_IF("solver_max_iterations", solver_max_iterations)
    SET_IF("default_rewrite_limit", default_rewrite_limit)
    SET_IF("wl_iterations", wl_iterations)
    SET_IF("wl_history_size", wl_history_size)
    SET_IF("vf2_max_depth", vf2_max_depth)
    SET_IF("buchberger_max_steps", buchberger_max_steps)
    SET_IF("groebner_reduce_max_steps", groebner_reduce_max_steps)
    SET_IF("stream_async_queue_capacity", stream_async_queue_capacity)
    SET_IF("stream_initial_callbacks", stream_initial_callbacks)
    SET_IF("stream_max_callbacks", stream_max_callbacks)
    SET_IF("stream_default_throttle_ms", stream_default_throttle_ms)
    SET_IF("bit_cutoff_threshold", bit_cutoff_threshold)
    SET_IF("max_precision_bits", max_precision_bits)
    SET_IF("continued_fraction_max_iter", continued_fraction_max_iter)
    SET_IF("max_subintervals", max_subintervals)
    SET_IF("mini_kernel_max_statements", mini_kernel_max_statements)
    SET_IF("mini_kernel_max_proof_depth", mini_kernel_max_proof_depth)
    SET_IF("mini_kernel_verify_timeout_ms", mini_kernel_verify_timeout_ms)
    SET_IF("cdcl_max_steps", cdcl_max_steps)
    SET_IF("cdcl_max_decisions", cdcl_max_decisions)
    SET_IF("cdcl_max_restarts", cdcl_max_restarts)
    SET_IF("parser_max_input_length", parser_max_input_length)
    SET_IF("parser_max_tokens", parser_max_tokens)
    SET_IF("parser_max_ast_depth", parser_max_ast_depth)
    SET_IF("parser_max_ast_nodes", parser_max_ast_nodes)
    SET_IF("parser_max_token_length", parser_max_token_length)
    SET_IF("parser_max_coordinates", parser_max_coordinates)
    SET_IF("parser_max_vertices", parser_max_vertices)
    SET_IF("parser_max_statements", parser_max_statements)
    SET_IF("parser_max_arguments", parser_max_arguments)
    SET_IF("parser_max_participants", parser_max_participants)
    SET_IF("type_infer_max_depth", type_infer_max_depth)
    SET_IF("type_equiv_max_depth", type_equiv_max_depth)
    SET_IF("runtime_guard_max_recurse", runtime_guard_max_recurse)
    SET_IF("runtime_guard_spin_attempts", runtime_guard_spin_attempts)
    SET_IF("proto_max_draw_cmds", proto_max_draw_cmds)
    SET_IF("proto_max_table_rows", proto_max_table_rows)
    SET_IF("proto_max_tree_nodes", proto_max_tree_nodes)
    SET_IF("proto_max_topology", proto_max_topology)
    SET_IF("proto_max_proof_steps", proto_max_proof_steps)
    SET_IF("proto_max_completions", proto_max_completions)
    SET_IF("proto_max_terminal_lines", proto_max_terminal_lines)
    SET_IF("geo_max_objects", geo_max_objects)
    SET_IF("geo_max_constraints", geo_max_constraints)
    SET_IF("geo_max_drag_chain", geo_max_drag_chain)
    SET_IF("geo_max_snapshots", geo_max_snapshots)
    SET_IF("geoevol_max_param_dim", geoevol_max_param_dim)
    SET_IF("geoevol_max_rejections", geoevol_max_rejections)
    SET_IF("proof_max_depth", proof_max_depth)
    SET_IF("proof_max_branches", proof_max_branches)
    SET_IF("proof_max_strategies", proof_max_strategies)
    SET_IF("trace_tree_max_depth", trace_tree_max_depth)
    SET_IF("max_recursion_depth", max_recursion_depth)
    SET_IF("context_default_max_depth", context_default_max_depth)
    SET_IF("context_default_max_steps", context_default_max_steps)
    SET_IF("context_default_max_consecutive_errors", context_default_max_consecutive_errors)
    SET_IF("context_reasoning_stack_default_capacity", context_reasoning_stack_default_capacity)
    SET_IF("interop_max_params", interop_max_params)
    SET_IF("interop_max_completions", interop_max_completions)
    SET_IF("interop_ws_default_port", interop_ws_default_port)
    SET_IF("log_max_files", log_max_files)
    SET_IF("log_max_size", log_max_size)
    SET_IF("log_ring_buffer_capacity", log_ring_buffer_capacity)
    SET_IF("perf_sample_max_count", perf_sample_max_count)
    SET_IF("timer_max_depth", timer_max_depth)
    SET_IF("max_plugins", max_plugins)
    SET_IF("max_interfaces", max_interfaces)
    SET_IF("backend_step_limit", backend_step_limit)
    SET_IF("backend_timeout_ms", backend_timeout_ms)
    SET_IF("test_max_suites", test_max_suites)
    SET_IF("test_max_cases", test_max_cases)
    SET_IF("smoke_test_step_limit", smoke_test_step_limit)
    SET_IF("smoke_test_timeout_ms", smoke_test_timeout_ms)
    SET_IF("circuit_overflow_threshold", circuit_overflow_threshold)
    SET_IF("value_too_large", value_too_large)
    SET_IF("downgrade_denominator", downgrade_denominator)
    SET_IF("default_memory_limit_mb", default_memory_limit_mb)
    SET_IF("context_timeout_ms", context_timeout_ms)
    SET_IF("context_cooldown_ms", context_cooldown_ms)
    SET_IF("prop_max_iterations", prop_max_iterations)
    SET_IF("prop_max_backtracks", prop_max_backtracks)
    SET_IF("prop_max_collaboration_iters", prop_max_collaboration_iters)
    SET_IF("high_dim_max_dimensions", high_dim_max_dimensions)
    SET_IF("high_dim_max_depth", high_dim_max_depth)
    SET_IF("high_dim_max_projection_presets", high_dim_max_projection_presets)
    SET_IF("high_dim_max_active_views", high_dim_max_active_views)
#undef SET_IF
    return false;
}

/**
 * @brief 通过字符串键设置浮点型配置项
 *
 * @param key 配置项名称
 * @param val 新值
 * @return true 设置成功，false 未找到匹配项或 key 为 NULL
 */
bool lv_config_set_double(const char *key, double val) {
    if (!key)
        return false;
    lvConfig *c = cfg_mut();

#define SET_IF(k, f)           \
    if (strcmp(key, k) == 0) { \
        c->f = val;            \
        return true;           \
    }
    SET_IF("geo_min_zoom", geo_min_zoom)
    SET_IF("geo_max_zoom", geo_max_zoom)
    SET_IF("geoevol_min_step", geoevol_min_step)
    SET_IF("geoevol_max_step", geoevol_max_step)
    SET_IF("geoevol_pi_smooth_factor", geoevol_pi_smooth_factor)
    SET_IF("health_memory_usage_ratio", health_memory_usage_ratio)
    SET_IF("health_memory_leak_ratio", health_memory_leak_ratio)
    SET_IF("high_dim_default_fidelity_threshold", high_dim_default_fidelity_threshold)
#undef SET_IF
    return false;
}

/* ---- 重置 ---- */

/**
 * @brief 重置配置为默认值
 */
void lv_config_reset(void) {
    g_active_config = *lv_config_default();
    g_config_applied = 1;
}

/* ---- JSON 配置加载辅助 —— 基于 lv_json.h ---- */

/**
 * @brief 从 JSON 字符串中解析指定键的整数值
 * @param json JSON 字符串
 * @param key  键名
 * @param out  输出值
 */
static void json_config_int(const char *json, const char *key, int *out) {
    const char *val = lv_json_find_key(json, key, strlen(key));
    if (!val) return;
    size_t remaining = strlen(val);
    lvJsonParser p;
    lv_json_parser_init(&p, val, remaining);
    lv_json_parse_int(&p, out);
}

/**
 * @brief 从 JSON 字符串中解析指定键的浮点数值
 * @param json JSON 字符串
 * @param key  键名
 * @param out  输出值
 */
static void json_config_double(const char *json, const char *key, double *out) {
    const char *val = lv_json_find_key(json, key, strlen(key));
    if (!val) return;
    size_t remaining = strlen(val);
    lvJsonParser p;
    lv_json_parser_init(&p, val, remaining);
    lv_json_parse_double(&p, out);
}

#define JLD_INT(k, f) json_config_int(json_data, k, &cfg.f)
#define JLD_DBL(k, f) json_config_double(json_data, k, &cfg.f)

/**
 * @brief 从 JSON 文件加载配置
 *
 * 读取 JSON 格式的配置文件，解析其中的键值对并应用到全局配置。
 * 仅解析预定义的配置项，未知键将被忽略。
 * 文件大小限制为 1MB。
 *
 * @param json_path JSON 文件路径
 * @return 0 成功，-1 失败（文件无法打开、过大或解析错误）
 */
int lv_config_load_json(const char *json_path) {
    if (!json_path)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "json_path is NULL");
    FILE *f = lv_file_open(json_path, "rb");
    if (!f)
        lv_RETURN_ERROR(lv_ERROR_IO, "failed to open config file");
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > (1024 * 1024)) {
        lv_file_close(f);
        lv_RETURN_ERROR(lv_ERROR_PARSE, "invalid config file size");
    }
    char *buf = (char *) lv_malloc((size_t) sz + 1);
    if (!buf) {
        lv_file_close(f);
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "failed to allocate JSON buffer");
    }
    size_t n = fread(buf, 1, (size_t) sz, f);
    lv_file_close(f);
    buf[n] = '\0';

    lvConfig cfg = *lv_config_default();
    const char *json_data = buf;

    /* solver */ JLD_INT("solver_max_var_id", solver_max_var_id);
    JLD_INT("solver_max_iterations", solver_max_iterations);
    /* graph */ JLD_INT("max_module_depth", max_module_depth);
    JLD_INT("graph_adj_max_per_node", graph_adj_max_per_node);
    /* rewrite */ JLD_INT("default_rewrite_limit", default_rewrite_limit);
    JLD_INT("vf2_max_depth", vf2_max_depth);
    JLD_INT("buchberger_max_steps", buchberger_max_steps);
    JLD_INT("groebner_reduce_max_steps", groebner_reduce_max_steps);
    /* stream */ JLD_INT("stream_async_queue_capacity", stream_async_queue_capacity);
    JLD_INT("stream_max_callbacks", stream_max_callbacks);
    /* precision */ JLD_INT("max_precision_bits", max_precision_bits);
    JLD_INT("bit_cutoff_threshold", bit_cutoff_threshold);
    /* sat */ JLD_INT("cdcl_max_steps", cdcl_max_steps);
    JLD_INT("cdcl_max_decisions", cdcl_max_decisions);
    JLD_INT("cdcl_max_restarts", cdcl_max_restarts);
    /* parser */ JLD_INT("parser_max_input_length", parser_max_input_length);
    JLD_INT("parser_max_tokens", parser_max_tokens);
    JLD_INT("parser_max_ast_depth", parser_max_ast_depth);
    JLD_INT("parser_max_ast_nodes", parser_max_ast_nodes);
    /* type */ JLD_INT("type_infer_max_depth", type_infer_max_depth);
    JLD_INT("type_equiv_max_depth", type_equiv_max_depth);
    /* guard */ JLD_INT("runtime_guard_max_recurse", runtime_guard_max_recurse);
    /* proto */ JLD_INT("proto_max_draw_cmds", proto_max_draw_cmds);
    JLD_INT("proto_max_table_rows", proto_max_table_rows);
    JLD_INT("proto_max_tree_nodes", proto_max_tree_nodes);
    JLD_INT("proto_max_topology", proto_max_topology);
    JLD_INT("proto_max_proof_steps", proto_max_proof_steps);
    JLD_INT("proto_max_completions", proto_max_completions);
    JLD_INT("proto_max_terminal_lines", proto_max_terminal_lines);
    /* geo */ JLD_INT("geo_max_objects", geo_max_objects);
    JLD_INT("geo_max_constraints", geo_max_constraints);
    JLD_INT("geo_max_drag_chain", geo_max_drag_chain);
    JLD_INT("geo_max_snapshots", geo_max_snapshots);
    JLD_DBL("geo_min_zoom", geo_min_zoom);
    JLD_DBL("geo_max_zoom", geo_max_zoom);
    /* ode */ JLD_INT("geoevol_max_param_dim", geoevol_max_param_dim);
    JLD_INT("geoevol_max_rejections", geoevol_max_rejections);
    /* proof */ JLD_INT("proof_max_depth", proof_max_depth);
    JLD_INT("proof_max_branches", proof_max_branches);
    JLD_INT("proof_max_strategies", proof_max_strategies);
    /* recursive */ JLD_INT("max_recursion_depth", max_recursion_depth);
    JLD_INT("context_default_max_depth", context_default_max_depth);
    JLD_INT("context_default_max_steps", context_default_max_steps);
    JLD_INT("context_default_max_consecutive_errors", context_default_max_consecutive_errors);
    /* interop */ JLD_INT("interop_max_params", interop_max_params);
    JLD_INT("interop_ws_default_port", interop_ws_default_port);
    /* log */ JLD_INT("log_max_files", log_max_files);
    JLD_INT("log_max_size", log_max_size);
    JLD_INT("log_ring_buffer_capacity", log_ring_buffer_capacity);
    /* perf */ JLD_INT("perf_sample_max_count", perf_sample_max_count);
    /* plugin */ JLD_INT("max_plugins", max_plugins);
    /* backend */ JLD_INT("backend_step_limit", backend_step_limit);
    JLD_INT("backend_timeout_ms", backend_timeout_ms);
    /* smoke test */ JLD_INT("smoke_test_step_limit", smoke_test_step_limit);
    JLD_INT("smoke_test_timeout_ms", smoke_test_timeout_ms);
    /* circuit */ JLD_INT("circuit_overflow_threshold", circuit_overflow_threshold);
    /* memory */ JLD_INT("default_memory_limit_mb", default_memory_limit_mb);

    lv_free((void **) &buf);
    return lv_config_apply(&cfg);
}

/**
 * @brief 将当前配置导出为 JSON 字符串
 *
 * @param buf      输出缓冲区
 * @param buf_size 缓冲区大小
 * @return 写入的字符数（不含结尾 null），失败返回 -1
 */
int lv_config_to_json(char *buf, size_t buf_size) {
    if (!buf)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "buf is NULL");
    if (buf_size < 64)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "buf_size < 64");
    const lvConfig *c = lv_config_current();
    return snprintf(
        buf, buf_size,
        "{\n"
        "  \"solver_max_var_id\": %d,\n"
        "  \"solver_max_iterations\": %d,\n"
        "  \"default_rewrite_limit\": %d,\n"
        "  \"stream_async_queue_capacity\": %d,\n"
        "  \"stream_max_callbacks\": %d,\n"
        "  \"max_precision_bits\": %d,\n"
        "  \"parser_max_input_length\": %d,\n"
        "  \"parser_max_tokens\": %d,\n"
        "  \"parser_max_ast_depth\": %d,\n"
        "  \"runtime_guard_max_recurse\": %d,\n"
        "  \"proto_max_draw_cmds\": %d,\n"
        "  \"proto_max_proof_steps\": %d,\n"
        "  \"geo_max_objects\": %d,\n"
        "  \"geo_max_constraints\": %d,\n"
        "  \"geo_min_zoom\": %.2f,\n"
        "  \"geo_max_zoom\": %.1f,\n"
        "  \"geoevol_max_param_dim\": %d,\n"
        "  \"proof_max_depth\": %d,\n"
        "  \"max_recursion_depth\": %d,\n"
        "  \"interop_ws_default_port\": %d,\n"
        "  \"log_max_files\": %d,\n"
        "  \"max_plugins\": %d,\n"
        "  \"backend_step_limit\": %d,\n"
        "  \"default_memory_limit_mb\": %d,\n"
        "  \"vf2_max_depth\": %d,\n"
        "  \"buchberger_max_steps\": %d,\n"
        "  \"groebner_reduce_max_steps\": %d,\n"
        "  \"cdcl_max_steps\": %d,\n"
        "  \"cdcl_max_decisions\": %d,\n"
        "  \"cdcl_max_restarts\": %d,\n"
        "  \"type_infer_max_depth\": %d,\n"
        "  \"type_equiv_max_depth\": %d,\n"
        "  \"circuit_overflow_threshold\": %d,\n"
        "  \"smoke_test_step_limit\": %d,\n"
        "  \"smoke_test_timeout_ms\": %d,\n"
        "  \"context_timeout_ms\": %d,\n"
        "  \"context_cooldown_ms\": %d,\n"
        "  \"prop_max_iterations\": %d,\n"
        "  \"prop_max_backtracks\": %d,\n"
        "  \"prop_max_collaboration_iters\": %d,\n"
        "  \"high_dim_max_dimensions\": %d,\n"
        "  \"high_dim_max_depth\": %d,\n"
        "  \"high_dim_max_projection_presets\": %d,\n"
        "  \"high_dim_max_active_views\": %d,\n"
        "  \"high_dim_default_fidelity_threshold\": %.2f\n"
        "}\n",
        c->solver_max_var_id, c->solver_max_iterations, c->default_rewrite_limit, c->stream_async_queue_capacity,
        c->stream_max_callbacks, c->max_precision_bits, c->parser_max_input_length, c->parser_max_tokens,
        c->parser_max_ast_depth, c->runtime_guard_max_recurse, c->proto_max_draw_cmds, c->proto_max_proof_steps,
        c->geo_max_objects, c->geo_max_constraints, c->geo_min_zoom, c->geo_max_zoom, c->geoevol_max_param_dim,
        c->proof_max_depth, c->max_recursion_depth, c->interop_ws_default_port, c->log_max_files, c->max_plugins,
        c->backend_step_limit, c->default_memory_limit_mb, c->vf2_max_depth, c->buchberger_max_steps,
        c->groebner_reduce_max_steps, c->cdcl_max_steps, c->cdcl_max_decisions, c->cdcl_max_restarts,
        c->type_infer_max_depth, c->type_equiv_max_depth, c->circuit_overflow_threshold, c->smoke_test_step_limit,
        c->smoke_test_timeout_ms, c->context_timeout_ms, c->context_cooldown_ms, c->prop_max_iterations,
        c->prop_max_backtracks, c->prop_max_collaboration_iters, c->high_dim_max_dimensions, c->high_dim_max_depth,
        c->high_dim_max_projection_presets, c->high_dim_max_active_views, c->high_dim_default_fidelity_threshold);
}
