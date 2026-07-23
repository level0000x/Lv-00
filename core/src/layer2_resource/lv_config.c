/**
 * @file lv_config.c
 * @brief Lv-00 运行时配置系统实现
 */

#include "lv/lv.h"
#include "lv/lv_utils.h"
#include "lv/lv_parse_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static lvConfig g_active_config;
static int g_config_applied = 0;

const lvConfig *lv_config_default(void) {
    static lvConfig def;
    static int initialized = 0;
    if (initialized) return &def;
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

    return &def;
}

const lvConfig *lv_config_current(void) {
    if (!g_config_applied) {
        g_active_config = *lv_config_default();
        g_config_applied = 1;
    }
    return &g_active_config;
}

int lv_config_apply(const lvConfig *cfg) {
    if (!cfg) return -1;
    g_active_config = *cfg;
    g_config_applied = 1;
    return 0;
}

/* ---- 类型安全 setter（直接改全局配置，立即生效） ---- */

static lvConfig *cfg_mut(void) {
    lv_config_current(); /* ensure initialized */
    return &g_active_config;
}

void lv_config_set_solver_max_var_id(int val)       { cfg_mut()->solver_max_var_id = val; }
void lv_config_set_solver_max_iterations(int val)    { cfg_mut()->solver_max_iterations = val; }
void lv_config_set_proof_max_depth(int val)           { cfg_mut()->proof_max_depth = val; }
void lv_config_set_proof_max_branches(int val)        { cfg_mut()->proof_max_branches = val; }
void lv_config_set_proto_max_draw_cmds(int val)       { cfg_mut()->proto_max_draw_cmds = val; }
void lv_config_set_proto_max_proof_steps(int val)     { cfg_mut()->proto_max_proof_steps = val; }
void lv_config_set_proto_max_terminal_lines(int val)  { cfg_mut()->proto_max_terminal_lines = val; }
void lv_config_set_geo_max_objects(int val)           { cfg_mut()->geo_max_objects = val; }
void lv_config_set_geo_max_constraints(int val)       { cfg_mut()->geo_max_constraints = val; }
void lv_config_set_geo_min_zoom(double val)           { cfg_mut()->geo_min_zoom = val; }
void lv_config_set_geo_max_zoom(double val)           { cfg_mut()->geo_max_zoom = val; }
void lv_config_set_parser_max_input_length(int val)   { cfg_mut()->parser_max_input_length = val; }
void lv_config_set_parser_max_ast_nodes(int val)      { cfg_mut()->parser_max_ast_nodes = val; }
void lv_config_set_max_recursion_depth(int val)       { cfg_mut()->max_recursion_depth = val; }
void lv_config_set_default_rewrite_limit(int val)     { cfg_mut()->default_rewrite_limit = val; }
void lv_config_set_geoevol_max_param_dim(int val)     { cfg_mut()->geoevol_max_param_dim = val; }
void lv_config_set_geoevol_max_rejections(int val)    { cfg_mut()->geoevol_max_rejections = val; }
void lv_config_set_stream_max_callbacks(int val)      { cfg_mut()->stream_max_callbacks = val; }
void lv_config_set_max_plugins(int val)               { cfg_mut()->max_plugins = val; }

/* ---- 通用 key-value setter ---- */

bool lv_config_set_int(const char *key, int val) {
    if (!key) return false;
    lvConfig *c = cfg_mut();

    #define SET_IF(k, f) if (strcmp(key, k) == 0) { c->f = val; return true; }
    SET_IF("solver_max_var_id",              solver_max_var_id)
    SET_IF("solver_max_iterations",          solver_max_iterations)
    SET_IF("default_rewrite_limit",          default_rewrite_limit)
    SET_IF("wl_iterations",                  wl_iterations)
    SET_IF("wl_history_size",                wl_history_size)
    SET_IF("vf2_max_depth",                  vf2_max_depth)
    SET_IF("buchberger_max_steps",           buchberger_max_steps)
    SET_IF("groebner_reduce_max_steps",      groebner_reduce_max_steps)
    SET_IF("stream_async_queue_capacity",    stream_async_queue_capacity)
    SET_IF("stream_initial_callbacks",       stream_initial_callbacks)
    SET_IF("stream_max_callbacks",           stream_max_callbacks)
    SET_IF("stream_default_throttle_ms",     stream_default_throttle_ms)
    SET_IF("bit_cutoff_threshold",           bit_cutoff_threshold)
    SET_IF("max_precision_bits",             max_precision_bits)
    SET_IF("continued_fraction_max_iter",    continued_fraction_max_iter)
    SET_IF("max_subintervals",               max_subintervals)
    SET_IF("mini_kernel_max_statements",     mini_kernel_max_statements)
    SET_IF("mini_kernel_max_proof_depth",    mini_kernel_max_proof_depth)
    SET_IF("mini_kernel_verify_timeout_ms",  mini_kernel_verify_timeout_ms)
    SET_IF("cdcl_max_steps",                 cdcl_max_steps)
    SET_IF("cdcl_max_decisions",             cdcl_max_decisions)
    SET_IF("cdcl_max_restarts",              cdcl_max_restarts)
    SET_IF("parser_max_input_length",        parser_max_input_length)
    SET_IF("parser_max_tokens",              parser_max_tokens)
    SET_IF("parser_max_ast_depth",           parser_max_ast_depth)
    SET_IF("parser_max_ast_nodes",           parser_max_ast_nodes)
    SET_IF("parser_max_token_length",        parser_max_token_length)
    SET_IF("parser_max_coordinates",         parser_max_coordinates)
    SET_IF("parser_max_vertices",            parser_max_vertices)
    SET_IF("parser_max_statements",          parser_max_statements)
    SET_IF("parser_max_arguments",           parser_max_arguments)
    SET_IF("parser_max_participants",        parser_max_participants)
    SET_IF("type_infer_max_depth",           type_infer_max_depth)
    SET_IF("type_equiv_max_depth",           type_equiv_max_depth)
    SET_IF("runtime_guard_max_recurse",      runtime_guard_max_recurse)
    SET_IF("runtime_guard_spin_attempts",    runtime_guard_spin_attempts)
    SET_IF("proto_max_draw_cmds",            proto_max_draw_cmds)
    SET_IF("proto_max_table_rows",           proto_max_table_rows)
    SET_IF("proto_max_tree_nodes",           proto_max_tree_nodes)
    SET_IF("proto_max_topology",             proto_max_topology)
    SET_IF("proto_max_proof_steps",          proto_max_proof_steps)
    SET_IF("proto_max_completions",          proto_max_completions)
    SET_IF("proto_max_terminal_lines",       proto_max_terminal_lines)
    SET_IF("geo_max_objects",                geo_max_objects)
    SET_IF("geo_max_constraints",            geo_max_constraints)
    SET_IF("geo_max_drag_chain",             geo_max_drag_chain)
    SET_IF("geo_max_snapshots",              geo_max_snapshots)
    SET_IF("geoevol_max_param_dim",          geoevol_max_param_dim)
    SET_IF("geoevol_max_rejections",         geoevol_max_rejections)
    SET_IF("proof_max_depth",                proof_max_depth)
    SET_IF("proof_max_branches",             proof_max_branches)
    SET_IF("proof_max_strategies",           proof_max_strategies)
    SET_IF("trace_tree_max_depth",           trace_tree_max_depth)
    SET_IF("max_recursion_depth",            max_recursion_depth)
    SET_IF("context_default_max_depth",      context_default_max_depth)
    SET_IF("context_default_max_steps",      context_default_max_steps)
    SET_IF("context_default_max_consecutive_errors", context_default_max_consecutive_errors)
    SET_IF("context_reasoning_stack_default_capacity", context_reasoning_stack_default_capacity)
    SET_IF("interop_max_params",             interop_max_params)
    SET_IF("interop_max_completions",        interop_max_completions)
    SET_IF("interop_ws_default_port",        interop_ws_default_port)
    SET_IF("log_max_files",                  log_max_files)
    SET_IF("log_max_size",                   log_max_size)
    SET_IF("log_ring_buffer_capacity",       log_ring_buffer_capacity)
    SET_IF("perf_sample_max_count",          perf_sample_max_count)
    SET_IF("timer_max_depth",                timer_max_depth)
    SET_IF("max_plugins",                    max_plugins)
    SET_IF("max_interfaces",                 max_interfaces)
    SET_IF("backend_step_limit",             backend_step_limit)
    SET_IF("backend_timeout_ms",             backend_timeout_ms)
    SET_IF("test_max_suites",                test_max_suites)
    SET_IF("test_max_cases",                 test_max_cases)
    SET_IF("smoke_test_step_limit",          smoke_test_step_limit)
    SET_IF("smoke_test_timeout_ms",          smoke_test_timeout_ms)
    SET_IF("circuit_overflow_threshold",     circuit_overflow_threshold)
    SET_IF("value_too_large",                value_too_large)
    SET_IF("downgrade_denominator",          downgrade_denominator)
    SET_IF("default_memory_limit_mb",        default_memory_limit_mb)
    #undef SET_IF
    return false;
}

bool lv_config_set_double(const char *key, double val) {
    if (!key) return false;
    lvConfig *c = cfg_mut();

    #define SET_IF(k, f) if (strcmp(key, k) == 0) { c->f = val; return true; }
    SET_IF("geo_min_zoom",              geo_min_zoom)
    SET_IF("geo_max_zoom",              geo_max_zoom)
    SET_IF("geoevol_min_step",          geoevol_min_step)
    SET_IF("geoevol_max_step",          geoevol_max_step)
    SET_IF("geoevol_pi_smooth_factor",  geoevol_pi_smooth_factor)
    SET_IF("health_memory_usage_ratio", health_memory_usage_ratio)
    SET_IF("health_memory_leak_ratio",  health_memory_leak_ratio)
    #undef SET_IF
    return false;
}

/* ---- 重置 ---- */

void lv_config_reset(void) {
    g_active_config = *lv_config_default();
    g_config_applied = 1;
}

/* ---- minimal JSON parser ---- */

static const char *json_skip_ws(const char *p) {
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    return p;
}

static int json_parse_int(const char **p, int *out) {
    *p = json_skip_ws(*p);
    int sign = 1;
    if (**p == '-') { sign = -1; (*p)++; }
    if (!isdigit((unsigned char)**p)) return -1;
    int val = 0;
    while (isdigit((unsigned char)**p)) { val = val * 10 + (**p - '0'); (*p)++; }
    *out = sign * val;
    return 0;
}

static int json_parse_double(const char **p, double *out) {
    *p = json_skip_ws(*p);
    char buf[64]; int i = 0;
    while (**p && (isdigit((unsigned char)**p)||**p=='.'||**p=='-'||**p=='e'||**p=='E'||**p=='+') && i<63)
        buf[i++] = *(*p)++;
    buf[i] = '\0';
    lv_parse_double(buf, out);
    return 0;
}

static const char *json_find_key(const char *p, const char *key) {
    while (*p) {
        p = json_skip_ws(p);
        if (*p == '}' || *p == '\0') return NULL;
        if (*p == ',') p++;
        p = json_skip_ws(p);
        if (*p == '"') {
            p++;
            const char *ks = key;
            while (*p && *p != '"' && *ks && *p == *ks) { p++; ks++; }
            if (*p == '"' && *ks == '\0') { p++; return json_skip_ws(p); }
            while (*p && *p != '"') p++;
            if (*p == '"') p++;
            p = json_skip_ws(p);
            if (*p == ':') { p++; p = json_skip_ws(p);
                if (*p=='{'||*p=='[') { int d=1; p++;
                    while(*p&&d>0){if(*p=='{'||*p=='[')d++;else if(*p=='}'||*p==']')d--;p++;}
                } else if (*p=='"') { p++; while(*p&&*p!='"')p++; if(*p=='"')p++; }
                else { while(*p&&*p!=','&&*p!='}')p++; }
            }
        }
    }
    return NULL;
}

static int json_config_int(const char *json, const char *key, int *out) {
    const char *p = json_find_key(json, key);
    if (!p || *p != ':') return 0;
    p++; return json_parse_int(&p, out);
}
static int json_config_double(const char *json, const char *key, double *out) {
    const char *p = json_find_key(json, key);
    if (!p || *p != ':') return 0;
    p++; return json_parse_double(&p, out);
}

#define JLD_INT(k,f)  json_config_int(json_data, k, &cfg.f)
#define JLD_DBL(k,f)  json_config_double(json_data, k, &cfg.f)

int lv_config_load_json(const char *json_path) {
    if (!json_path) return -1;
    FILE *f = fopen(json_path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > (1024*1024)) { fclose(f); return -1; }
    char *buf = (char *)lv_malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return -1; }
    size_t n = fread(buf, 1, (size_t)sz, f); fclose(f);
    buf[n] = '\0';

    lvConfig cfg = *lv_config_default();
    const char *json_data = buf;

    /* solver */           JLD_INT("solver_max_var_id", solver_max_var_id);
                           JLD_INT("solver_max_iterations", solver_max_iterations);
    /* graph */            JLD_INT("max_module_depth", max_module_depth);
                           JLD_INT("graph_adj_max_per_node", graph_adj_max_per_node);
    /* rewrite */          JLD_INT("default_rewrite_limit", default_rewrite_limit);
                           JLD_INT("vf2_max_depth", vf2_max_depth);
                           JLD_INT("buchberger_max_steps", buchberger_max_steps);
                           JLD_INT("groebner_reduce_max_steps", groebner_reduce_max_steps);
    /* stream */           JLD_INT("stream_async_queue_capacity", stream_async_queue_capacity);
                           JLD_INT("stream_max_callbacks", stream_max_callbacks);
    /* precision */        JLD_INT("max_precision_bits", max_precision_bits);
                           JLD_INT("bit_cutoff_threshold", bit_cutoff_threshold);
    /* sat */              JLD_INT("cdcl_max_steps", cdcl_max_steps);
                           JLD_INT("cdcl_max_decisions", cdcl_max_decisions);
                           JLD_INT("cdcl_max_restarts", cdcl_max_restarts);
    /* parser */           JLD_INT("parser_max_input_length", parser_max_input_length);
                           JLD_INT("parser_max_tokens", parser_max_tokens);
                           JLD_INT("parser_max_ast_depth", parser_max_ast_depth);
                           JLD_INT("parser_max_ast_nodes", parser_max_ast_nodes);
    /* type */             JLD_INT("type_infer_max_depth", type_infer_max_depth);
                           JLD_INT("type_equiv_max_depth", type_equiv_max_depth);
    /* guard */            JLD_INT("runtime_guard_max_recurse", runtime_guard_max_recurse);
    /* proto */            JLD_INT("proto_max_draw_cmds", proto_max_draw_cmds);
                           JLD_INT("proto_max_table_rows", proto_max_table_rows);
                           JLD_INT("proto_max_tree_nodes", proto_max_tree_nodes);
                           JLD_INT("proto_max_topology", proto_max_topology);
                           JLD_INT("proto_max_proof_steps", proto_max_proof_steps);
                           JLD_INT("proto_max_completions", proto_max_completions);
                           JLD_INT("proto_max_terminal_lines", proto_max_terminal_lines);
    /* geo */              JLD_INT("geo_max_objects", geo_max_objects);
                           JLD_INT("geo_max_constraints", geo_max_constraints);
                           JLD_INT("geo_max_drag_chain", geo_max_drag_chain);
                           JLD_INT("geo_max_snapshots", geo_max_snapshots);
                           JLD_DBL("geo_min_zoom", geo_min_zoom);
                           JLD_DBL("geo_max_zoom", geo_max_zoom);
    /* ode */              JLD_INT("geoevol_max_param_dim", geoevol_max_param_dim);
                           JLD_INT("geoevol_max_rejections", geoevol_max_rejections);
    /* proof */            JLD_INT("proof_max_depth", proof_max_depth);
                           JLD_INT("proof_max_branches", proof_max_branches);
                           JLD_INT("proof_max_strategies", proof_max_strategies);
    /* recursive */        JLD_INT("max_recursion_depth", max_recursion_depth);
                           JLD_INT("context_default_max_depth", context_default_max_depth);
                           JLD_INT("context_default_max_steps", context_default_max_steps);
                           JLD_INT("context_default_max_consecutive_errors", context_default_max_consecutive_errors);
    /* interop */          JLD_INT("interop_max_params", interop_max_params);
                           JLD_INT("interop_ws_default_port", interop_ws_default_port);
    /* log */              JLD_INT("log_max_files", log_max_files);
                           JLD_INT("log_max_size", log_max_size);
                           JLD_INT("log_ring_buffer_capacity", log_ring_buffer_capacity);
    /* perf */             JLD_INT("perf_sample_max_count", perf_sample_max_count);
    /* plugin */           JLD_INT("max_plugins", max_plugins);
    /* backend */          JLD_INT("backend_step_limit", backend_step_limit);
                           JLD_INT("backend_timeout_ms", backend_timeout_ms);
    /* smoke test */       JLD_INT("smoke_test_step_limit", smoke_test_step_limit);
                           JLD_INT("smoke_test_timeout_ms", smoke_test_timeout_ms);
    /* circuit */          JLD_INT("circuit_overflow_threshold", circuit_overflow_threshold);
    /* memory */           JLD_INT("default_memory_limit_mb", default_memory_limit_mb);

    lv_free((void **)&buf);
    return lv_config_apply(&cfg);
}

int lv_config_to_json(char *buf, size_t buf_size) {
    if (!buf || buf_size < 64) return -1;
    const lvConfig *c = lv_config_current();
    return snprintf(buf, buf_size,
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
        "  \"default_memory_limit_mb\": %d\n"
        "}\n",
        c->solver_max_var_id, c->solver_max_iterations,
        c->default_rewrite_limit, c->stream_async_queue_capacity,
        c->stream_max_callbacks, c->max_precision_bits,
        c->parser_max_input_length, c->parser_max_tokens,
        c->parser_max_ast_depth, c->runtime_guard_max_recurse,
        c->proto_max_draw_cmds, c->proto_max_proof_steps,
        c->geo_max_objects, c->geo_max_constraints,
        c->geo_min_zoom, c->geo_max_zoom,
        c->geoevol_max_param_dim, c->proof_max_depth,
        c->max_recursion_depth, c->interop_ws_default_port,
        c->log_max_files, c->max_plugins,
        c->backend_step_limit, c->default_memory_limit_mb);
}
