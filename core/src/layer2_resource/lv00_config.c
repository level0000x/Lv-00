/**
 * @file lv00_config.c
 * @brief Lv-00 运行时配置系统实现
 */

#include "lv00/lv00.h"
#include "lv00/lv00_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static Lv00Config g_active_config;
static int g_config_applied = 0;

/* ---- 默认值填充 ---- */

const Lv00Config *lv00_config_default(void) {
    static Lv00Config def;
    static int initialized = 0;
    if (initialized) return &def;
    initialized = 1;

    memset(&def, 0, sizeof(def));
    def.solver_max_var_id               = LV00_CONFIG_SOLVER_MAX_VAR_ID;
    def.solver_max_iterations           = LV00_CONFIG_SOLVER_MAX_ITERATIONS;
    def.default_rewrite_limit           = LV00_CONFIG_DEFAULT_REWRITE_LIMIT;
    def.stream_async_queue_capacity     = LV00_CONFIG_STREAM_ASYNC_QUEUE_CAPACITY;
    def.stream_json_buffer_size         = LV00_CONFIG_STREAM_JSON_BUFFER_SIZE;
    def.stream_max_callbacks            = LV00_CONFIG_STREAM_MAX_CALLBACKS;
    def.max_precision_bits              = LV00_CONFIG_MAX_PRECISION_BITS;
    def.root_epsilon                    = LV00_CONFIG_ROOT_EPSILON;
    def.parser_max_input_length         = LV00_CONFIG_PARSER_MAX_INPUT_LENGTH;
    def.parser_max_tokens               = LV00_CONFIG_PARSER_MAX_TOKENS;
    def.parser_max_ast_depth            = LV00_CONFIG_PARSER_MAX_AST_DEPTH;
    def.parser_max_ast_nodes            = LV00_CONFIG_PARSER_MAX_AST_NODES;
    def.runtime_guard_max_recurse       = LV00_CONFIG_RUNTIME_GUARD_MAX_RECURSE;
    def.proto_max_draw_cmds             = LV00_CONFIG_PROTO_MAX_DRAW_CMDS;
    def.proto_max_table_rows            = LV00_CONFIG_PROTO_MAX_TABLE_ROWS;
    def.proto_max_tree_nodes            = LV00_CONFIG_PROTO_MAX_TREE_NODES;
    def.proto_max_topology              = LV00_CONFIG_PROTO_MAX_TOPOLOGY;
    def.proto_max_proof_steps           = LV00_CONFIG_PROTO_MAX_PROOF_STEPS;
    def.proto_max_completions           = LV00_CONFIG_PROTO_MAX_COMPLETIONS;
    def.proto_max_terminal_lines        = LV00_CONFIG_PROTO_MAX_TERMINAL_LINES;
    def.geo_max_objects                 = LV00_CONFIG_GEO_MAX_OBJECTS;
    def.geo_max_constraints             = LV00_CONFIG_GEO_MAX_CONSTRAINTS;
    def.geo_max_drag_chain              = LV00_CONFIG_GEO_MAX_DRAG_CHAIN;
    def.geo_max_snapshots               = LV00_CONFIG_GEO_MAX_SNAPSHOTS;
    def.geo_min_zoom                    = LV00_CONFIG_GEO_MIN_ZOOM;
    def.geo_max_zoom                    = LV00_CONFIG_GEO_MAX_ZOOM;
    def.geoevol_max_param_dim           = LV00_CONFIG_GEOEVOL_MAX_PARAM_DIM;
    def.geoevol_max_rejections          = LV00_CONFIG_GEOEVOL_MAX_REJECTIONS;
    def.geoevol_min_step                = LV00_CONFIG_GEOEVOL_MIN_STEP;
    def.geoevol_max_step                = LV00_CONFIG_GEOEVOL_MAX_STEP;
    def.proof_max_depth                 = LV00_CONFIG_PROOF_MAX_DEPTH;
    def.proof_max_branches              = LV00_CONFIG_PROOF_MAX_BRANCHES;
    def.proof_max_strategies            = LV00_CONFIG_PROOF_MAX_STRATEGIES;
    def.max_recursion_depth             = LV00_CONFIG_MAX_RECURSION_DEPTH;
    def.context_default_max_depth       = LV00_CONFIG_CONTEXT_DEFAULT_MAX_DEPTH;
    def.context_default_max_steps       = LV00_CONFIG_CONTEXT_DEFAULT_MAX_STEPS;
    def.context_default_max_consecutive_errors = LV00_CONFIG_CONTEXT_DEFAULT_MAX_CONSECUTIVE_ERRORS;
    def.interop_cmd_buffer_size         = LV00_CONFIG_INTEROP_CMD_BUFFER_SIZE;
    def.interop_resp_buffer_size        = LV00_CONFIG_INTEROP_RESP_BUFFER_SIZE;
    def.interop_max_params              = LV00_CONFIG_INTEROP_MAX_PARAMS;
    def.interop_max_path_len            = LV00_CONFIG_INTEROP_MAX_PATH_LEN;
    def.interop_ws_default_port         = LV00_CONFIG_INTEROP_WS_DEFAULT_PORT;
    def.log_max_files                   = LV00_CONFIG_LOG_MAX_FILES;
    def.log_max_size                    = LV00_CONFIG_LOG_MAX_SIZE;
    def.log_ring_buffer_capacity        = LV00_CONFIG_LOG_RING_BUFFER_CAPACITY;
    def.perf_sample_max_count           = LV00_CONFIG_PERF_SAMPLE_MAX_COUNT;
    def.timer_max_depth                 = LV00_CONFIG_TIMER_MAX_DEPTH;
    def.max_plugins                     = LV00_CONFIG_MAX_PLUGINS;
    def.max_interfaces                  = LV00_CONFIG_MAX_INTERFACES;
    def.backend_step_limit              = LV00_CONFIG_BACKEND_STEP_LIMIT;
    def.backend_timeout_ms              = LV00_CONFIG_BACKEND_TIMEOUT_MS;
    def.circuit_overflow_threshold      = LV00_CONFIG_CIRCUIT_OVERFLOW_THRESHOLD;
    def.default_memory_limit_mb         = LV00_CONFIG_DEFAULT_MEMORY_LIMIT_MB;

    return &def;
}

/* ---- 当前配置 ---- */

const Lv00Config *lv00_config_current(void) {
    if (!g_config_applied) {
        g_active_config = *lv00_config_default();
        g_config_applied = 1;
    }
    return &g_active_config;
}

int lv00_config_apply(const Lv00Config *cfg) {
    if (!cfg) return -1;
    g_active_config = *cfg;
    g_config_applied = 1;
    return 0;
}

/* ---- 最小 JSON 解析（无第三方依赖） ---- */

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
    while (isdigit((unsigned char)**p)) {
        val = val * 10 + (**p - '0');
        (*p)++;
    }
    *out = sign * val;
    return 0;
}

static int json_parse_double(const char **p, double *out) {
    *p = json_skip_ws(*p);
    char buf[64]; int i = 0;
    while (**p && (isdigit((unsigned char)**p) || **p == '.' || **p == '-' || **p == 'e' || **p == 'E' || **p == '+') && i < 63) {
        buf[i++] = *(*p)++;
    }
    buf[i] = '\0';
    *out = atof(buf);
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
            if (*p == '"' && *ks == '\0') {
                p++;
                return json_skip_ws(p);
            }
            while (*p && *p != '"') p++;
            if (*p == '"') p++;
            p = json_skip_ws(p);
            if (*p == ':') { p++; p = json_skip_ws(p);
                if (*p == '{' || *p == '[') { int d = 1; p++;
                    while (*p && d > 0) { if (*p == '{' || *p == '[') d++; else if (*p == '}' || *p == ']') d--; p++; }
                } else if (*p == '"') { p++; while (*p && *p != '"') p++; if (*p == '"') p++; }
                else { while (*p && *p != ',' && *p != '}') p++; }
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

#define JSON_LOAD_INT(key, field) json_config_int(json_data, key, &cfg.field)
#define JSON_LOAD_DBL(key, field)  json_config_double(json_data, key, &cfg.field)

int lv00_config_load_json(const char *json_path) {
    if (!json_path) return -1;
    FILE *f = fopen(json_path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > (1024 * 1024)) { fclose(f); return -1; }
    char *buf = (char *)lv00_malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return -1; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';

    Lv00Config cfg = *lv00_config_default();
    const char *json_data = buf;

    /* ---- 求解器 ---- */
    JSON_LOAD_INT("solver_max_var_id", solver_max_var_id);
    JSON_LOAD_INT("solver_max_iterations", solver_max_iterations);
    /* ---- 重写 ---- */
    JSON_LOAD_INT("default_rewrite_limit", default_rewrite_limit);
    /* ---- 流式 ---- */
    JSON_LOAD_INT("stream_async_queue_capacity", stream_async_queue_capacity);
    JSON_LOAD_INT("stream_json_buffer_size", stream_json_buffer_size);
    JSON_LOAD_INT("stream_max_callbacks", stream_max_callbacks);
    /* ---- 精度 ---- */
    JSON_LOAD_INT("max_precision_bits", max_precision_bits);
    JSON_LOAD_DBL("root_epsilon", root_epsilon);
    /* ---- 解析器 ---- */
    JSON_LOAD_INT("parser_max_input_length", parser_max_input_length);
    JSON_LOAD_INT("parser_max_tokens", parser_max_tokens);
    JSON_LOAD_INT("parser_max_ast_depth", parser_max_ast_depth);
    JSON_LOAD_INT("parser_max_ast_nodes", parser_max_ast_nodes);
    /* ---- 防护 ---- */
    JSON_LOAD_INT("runtime_guard_max_recurse", runtime_guard_max_recurse);
    /* ---- 协议 ---- */
    JSON_LOAD_INT("proto_max_draw_cmds", proto_max_draw_cmds);
    JSON_LOAD_INT("proto_max_table_rows", proto_max_table_rows);
    JSON_LOAD_INT("proto_max_tree_nodes", proto_max_tree_nodes);
    JSON_LOAD_INT("proto_max_topology", proto_max_topology);
    JSON_LOAD_INT("proto_max_proof_steps", proto_max_proof_steps);
    JSON_LOAD_INT("proto_max_completions", proto_max_completions);
    JSON_LOAD_INT("proto_max_terminal_lines", proto_max_terminal_lines);
    /* ---- 几何 ---- */
    JSON_LOAD_INT("geo_max_objects", geo_max_objects);
    JSON_LOAD_INT("geo_max_constraints", geo_max_constraints);
    JSON_LOAD_INT("geo_max_drag_chain", geo_max_drag_chain);
    JSON_LOAD_INT("geo_max_snapshots", geo_max_snapshots);
    JSON_LOAD_DBL("geo_min_zoom", geo_min_zoom);
    JSON_LOAD_DBL("geo_max_zoom", geo_max_zoom);
    /* ---- 演化 ---- */
    JSON_LOAD_INT("geoevol_max_param_dim", geoevol_max_param_dim);
    JSON_LOAD_INT("geoevol_max_rejections", geoevol_max_rejections);
    JSON_LOAD_DBL("geoevol_min_step", geoevol_min_step);
    JSON_LOAD_DBL("geoevol_max_step", geoevol_max_step);
    /* ---- 证明 ---- */
    JSON_LOAD_INT("proof_max_depth", proof_max_depth);
    JSON_LOAD_INT("proof_max_branches", proof_max_branches);
    JSON_LOAD_INT("proof_max_strategies", proof_max_strategies);
    /* ---- 递归 ---- */
    JSON_LOAD_INT("max_recursion_depth", max_recursion_depth);
    JSON_LOAD_INT("context_default_max_depth", context_default_max_depth);
    JSON_LOAD_INT("context_default_max_steps", context_default_max_steps);
    JSON_LOAD_INT("context_default_max_consecutive_errors", context_default_max_consecutive_errors);
    /* ---- 互操作 ---- */
    JSON_LOAD_INT("interop_cmd_buffer_size", interop_cmd_buffer_size);
    JSON_LOAD_INT("interop_resp_buffer_size", interop_resp_buffer_size);
    JSON_LOAD_INT("interop_max_params", interop_max_params);
    JSON_LOAD_INT("interop_max_path_len", interop_max_path_len);
    JSON_LOAD_INT("interop_ws_default_port", interop_ws_default_port);
    /* ---- 日志 ---- */
    JSON_LOAD_INT("log_max_files", log_max_files);
    JSON_LOAD_INT("log_max_size", log_max_size);
    JSON_LOAD_INT("log_ring_buffer_capacity", log_ring_buffer_capacity);
    /* ---- 监控 ---- */
    JSON_LOAD_INT("perf_sample_max_count", perf_sample_max_count);
    JSON_LOAD_INT("timer_max_depth", timer_max_depth);
    /* ---- 插件 ---- */
    JSON_LOAD_INT("max_plugins", max_plugins);
    JSON_LOAD_INT("max_interfaces", max_interfaces);
    /* ---- 后端 ---- */
    JSON_LOAD_INT("backend_step_limit", backend_step_limit);
    JSON_LOAD_INT("backend_timeout_ms", backend_timeout_ms);
    /* ---- 熔断 ---- */
    JSON_LOAD_INT("circuit_overflow_threshold", circuit_overflow_threshold);
    /* ---- 内存 ---- */
    JSON_LOAD_INT("default_memory_limit_mb", default_memory_limit_mb);

    lv00_free((void **)&buf);
    return lv00_config_apply(&cfg);
}

int lv00_config_to_json(char *buf, size_t buf_size) {
    if (!buf || buf_size < 32) return -1;
    const Lv00Config *c = lv00_config_current();
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
        "  \"geoevol_min_step\": %.1e,\n"
        "  \"proof_max_depth\": %d,\n"
        "  \"proof_max_branches\": %d,\n"
        "  \"max_recursion_depth\": %d,\n"
        "  \"interop_cmd_buffer_size\": %d,\n"
        "  \"interop_ws_default_port\": %d,\n"
        "  \"log_max_files\": %d,\n"
        "  \"log_max_size\": %d,\n"
        "  \"max_plugins\": %d,\n"
        "  \"backend_step_limit\": %d,\n"
        "  \"backend_timeout_ms\": %d,\n"
        "  \"circuit_overflow_threshold\": %d,\n"
        "  \"default_memory_limit_mb\": %d\n"
        "}\n",
        c->solver_max_var_id, c->solver_max_iterations,
        c->default_rewrite_limit,
        c->stream_async_queue_capacity, c->stream_max_callbacks,
        c->max_precision_bits,
        c->parser_max_input_length, c->parser_max_tokens,
        c->parser_max_ast_depth, c->runtime_guard_max_recurse,
        c->proto_max_draw_cmds, c->proto_max_proof_steps,
        c->geo_max_objects, c->geo_max_constraints,
        c->geo_min_zoom, c->geo_max_zoom,
        c->geoevol_max_param_dim, c->geoevol_min_step,
        c->proof_max_depth, c->proof_max_branches,
        c->max_recursion_depth,
        c->interop_cmd_buffer_size, c->interop_ws_default_port,
        c->log_max_files, c->log_max_size,
        c->max_plugins,
        c->backend_step_limit, c->backend_timeout_ms,
        c->circuit_overflow_threshold, c->default_memory_limit_mb
    );
}
