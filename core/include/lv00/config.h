/**
 * @file config.h
 * @brief Lv-00 集中化配置 —— 所有硬编码常量的单一事实来源
 *
 * @details 此文件收集项目中所有魔数、阈值、缓冲区大小，统一管理。
 *          支持编译期宏定义（LV00_CONFIG_*）和运行时配置（Lv00Config 结构体）。
 *
 * 使用方式:
 *   编译期: #include "lv00/config.h" → 使用 LV00_CONFIG_* 宏
 *   运行时: lv00_config_load_json("lv00.config.json"); lv00_init();
 *
 * @version 3.4.0
 */
#ifndef LV00_CONFIG_H
#define LV00_CONFIG_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* ====================================================================
 * 编译期默认值（所有可调上限的统一命名空间）
 * ==================================================================== */

/* ---- 求解器 ---- */
#define LV00_CONFIG_SOLVER_MAX_VAR_ID             100000
#define LV00_CONFIG_SOLVER_MAX_ITERATIONS         10000

/* ---- 约束图 ---- */
#define LV00_CONFIG_MAX_MODULE_DEPTH              32
#define LV00_CONFIG_GRAPH_ERROR_BUFFER_SIZE       256
#define LV00_CONFIG_GRAPH_SERIALIZE_BUFFER_SIZE   256
#define LV00_CONFIG_GRAPH_ADJ_MAX_PER_NODE        256

/* ---- 引擎 ---- */
#define LV00_CONFIG_ENGINE_ERROR_BUFFER_SIZE      256
#define LV00_CONFIG_DEFAULT_REWRITE_LIMIT         1000

/* ---- 数据结构 ---- */
#define LV00_CONFIG_INITIAL_ARRAY_CAPACITY        8
#define LV00_CONFIG_INITIAL_HASH_INDEX_CAPACITY   64

/* ---- 重写引擎 ---- */
#define LV00_CONFIG_WL_ITERATIONS                 3
#define LV00_CONFIG_WL_HISTORY_SIZE               64

/* ---- 流式输出 ---- */
#define LV00_CONFIG_STREAM_ASYNC_QUEUE_CAPACITY   1024
#define LV00_CONFIG_STREAM_JSON_BUFFER_SIZE       4096
#define LV00_CONFIG_STREAM_DEFAULT_THROTTLE_MS    50
#define LV00_CONFIG_STREAM_INITIAL_CALLBACKS      16
#define LV00_CONFIG_STREAM_MAX_CALLBACKS          64

/* ---- 数值精度 ---- */
#define LV00_CONFIG_BIT_CUTOFF_THRESHOLD          1000000
#define LV00_CONFIG_MAX_PRECISION_BITS            100
#define LV00_CONFIG_DEFAULT_PRECISION_BITS        52
#define LV00_CONFIG_ROOT_EPSILON                  1e-12
#define LV00_CONFIG_CONTINUED_FRACTION_MAX_ITER   1000
#define LV00_CONFIG_MAX_SUBINTERVALS              4096

/* ---- 压力测试 ---- */
#define LV00_CONFIG_STRESS_TEST_DEFAULT_CHAIN     100
#define LV00_CONFIG_STRESS_TEST_MAX_POLY_DEGREE   4

/* ---- MiniKernel ---- */
#define LV00_CONFIG_MINI_KERNEL_MAX_STATEMENTS    10000
#define LV00_CONFIG_MINI_KERNEL_MAX_PROOF_DEPTH   1000
#define LV00_CONFIG_MINI_KERNEL_VERIFY_TIMEOUT_MS 30000

/* ---- 解析器 ---- */
#define LV00_CONFIG_PARSER_MAX_INPUT_LENGTH       1048576
#define LV00_CONFIG_PARSER_MAX_TOKENS             100000
#define LV00_CONFIG_PARSER_MAX_AST_DEPTH          256
#define LV00_CONFIG_PARSER_MAX_AST_NODES          500000
#define LV00_CONFIG_PARSER_MAX_TOKEN_LENGTH       4096
#define LV00_CONFIG_PARSER_MAX_COORDINATES        16
#define LV00_CONFIG_PARSER_MAX_VERTICES           32
#define LV00_CONFIG_PARSER_MAX_POLYGON_VERTICES   32
#define LV00_CONFIG_PARSER_MAX_STATEMENTS         64
#define LV00_CONFIG_PARSER_MAX_ARGUMENTS          16
#define LV00_CONFIG_PARSER_MAX_PARTICIPANTS       16
#define LV00_CONFIG_PARSER_MAX_BUFFER_SIZE        256
#define LV00_CONFIG_PARSER_MAX_TEMP_MSG_SIZE      128

/* ---- 运行时防护 ---- */
#define LV00_CONFIG_RUNTIME_GUARD_MAX_RECURSE     128
#define LV00_CONFIG_RUNTIME_GUARD_SPIN_ATTEMPTS   1024
#define LV00_CONFIG_RUNTIME_GUARD_WRITE_WARN_US   10000

/* ---- 通用字符串长度 ---- */
#define LV00_CONFIG_MAX_LABEL_LENGTH              256
#define LV00_CONFIG_MAX_FORMULA_LENGTH            1024
#define LV00_CONFIG_MAX_VARIABLE_NAME_LENGTH      128
#define LV00_CONFIG_MAX_REPLACEMENT_TERM_LENGTH   512
#define LV00_CONFIG_MAX_PROOF_REFS                64

/* ---- 对象池 ---- */
#define LV00_CONFIG_POOL_CONSTRAINT_NODE_SIZE     128
#define LV00_CONFIG_POOL_CONSTRAINT_SIZE          96
#define LV00_CONFIG_POOL_SYMBOLIC_COORD_SIZE      64
#define LV00_CONFIG_POOL_PROOF_STEP_SIZE          128

/* ---- 内存池 ---- */
#define LV00_CONFIG_POOL_DEFAULT_CAPACITY         1024
#define LV00_CONFIG_LINEAR_ALLOCATOR_BLOCK_SIZE   65536
#define LV00_CONFIG_LRU_CACHE_DEFAULT_CAPACITY    256
#define LV00_CONFIG_MEM_STAT_MAX_TYPES            64

/* ---- UI-Kernel 通信协议 ---- */
#define LV00_CONFIG_PROTO_MAX_DRAW_CMDS           4096
#define LV00_CONFIG_PROTO_MAX_TABLE_ROWS           512
#define LV00_CONFIG_PROTO_MAX_TREE_NODES           256
#define LV00_CONFIG_PROTO_MAX_TOPOLOGY             128
#define LV00_CONFIG_PROTO_MAX_PROOF_STEPS          512
#define LV00_CONFIG_PROTO_MAX_COMPLETIONS           64
#define LV00_CONFIG_PROTO_MAX_TERMINAL_LINES       512
#define LV00_CONFIG_PROTO_STR_LEN                   64
#define LV00_CONFIG_PROTO_LABEL_LEN               128
#define LV00_CONFIG_PROTO_DESC_LEN                256
#define LV00_CONFIG_PROTO_BUFFER_LEN             4096

/* ---- 交互几何 ---- */
#define LV00_CONFIG_GEO_MAX_OBJECTS              1024
#define LV00_CONFIG_GEO_MAX_CONSTRAINTS          2048
#define LV00_CONFIG_GEO_MAX_DRAG_CHAIN             64
#define LV00_CONFIG_GEO_MAX_SNAPSHOTS              32
#define LV00_CONFIG_GEO_SCRIPT_BUFFER_SIZE       65536
#define LV00_CONFIG_GEO_STATE_BUFFER_SIZE       131072
#define LV00_CONFIG_GEO_MIN_ZOOM                  0.01
#define LV00_CONFIG_GEO_MAX_ZOOM                100.0

/* ---- 几何演化 (ODE) ---- */
#define LV00_CONFIG_GEOEVOL_MAX_PARAM_DIM         256
#define LV00_CONFIG_GEOEVOL_ADAMS_MAX_ORDER        12
#define LV00_CONFIG_GEOEVOL_MIN_STEP             1e-15
#define LV00_CONFIG_GEOEVOL_MAX_STEP             1e10
#define LV00_CONFIG_GEOEVOL_PI_SMOOTH_FACTOR      0.25
#define LV00_CONFIG_GEOEVOL_MAX_REJECTIONS         20

/* ---- 证明引擎 ---- */
#define LV00_CONFIG_PROOF_MAX_DEPTH               100
#define LV00_CONFIG_PROOF_MAX_BRANCHES             64
#define LV00_CONFIG_PROOF_MAX_STRATEGIES           16
#define LV00_CONFIG_TRACE_TREE_MAX_DEPTH           50

/* ---- 递归/上下文 ---- */
#define LV00_CONFIG_MAX_RECURSION_DEPTH           128
#define LV00_CONFIG_CONTEXT_DEFAULT_MAX_DEPTH     100
#define LV00_CONFIG_CONTEXT_MAX_RECURSION_DEPTH 10000
#define LV00_CONFIG_CONTEXT_DEFAULT_MAX_STEPS 1000000
#define LV00_CONFIG_CONTEXT_DEFAULT_MAX_CONSECUTIVE_ERRORS 10
#define LV00_CONFIG_CONTEXT_REASONING_STACK_DEFAULT_CAPACITY 8
#define LV00_CONFIG_CONTEXT_REASONING_STACK_MAX_DEPTH 1000

/* ---- 互操作 ---- */
#define LV00_CONFIG_INTEROP_CMD_BUFFER_SIZE      4096
#define LV00_CONFIG_INTEROP_RESP_BUFFER_SIZE    65536
#define LV00_CONFIG_INTEROP_MAX_PARAMS             32
#define LV00_CONFIG_INTEROP_WS_DEFAULT_PORT      8765
#define LV00_CONFIG_INTEROP_MAX_PATH_LEN          512
#define LV00_CONFIG_INTEROP_MAX_COMPLETIONS        64

/* ---- 调试/日志 ---- */
#define LV00_CONFIG_LOG_MAX_FILES                   5
#define LV00_CONFIG_LOG_MAX_SIZE            10485760
#define LV00_CONFIG_LOG_PATH_MAX                 256
#define LV00_CONFIG_LOG_RING_BUFFER_CAPACITY     256

/* ---- 运行时监控 ---- */
#define LV00_CONFIG_LOG_MSG_MAX_LEN             4096
#define LV00_CONFIG_LOG_TAG_MAX_LEN               64
#define LV00_CONFIG_METRIC_NAME_MAX_LEN          128
#define LV00_CONFIG_TIMER_MAX_DEPTH               32
#define LV00_CONFIG_PERF_SAMPLE_MAX_COUNT      10000

/* ---- 插件系统 ---- */
#define LV00_CONFIG_MAX_PLUGINS                  256
#define LV00_CONFIG_MAX_INTERFACES               128
#define LV00_CONFIG_PLUGIN_NAME_MAX               64
#define LV00_CONFIG_PLUGIN_DESC_MAX              256
#define LV00_CONFIG_PLUGIN_AUTHOR_MAX            128
#define LV00_CONFIG_PLUGIN_PATH_MAX              512

/* ---- 数值后端 ---- */
#define LV00_CONFIG_BACKEND_NAME_MAX              64
#define LV00_CONFIG_BACKEND_STEP_LIMIT          1000
#define LV00_CONFIG_BACKEND_TIMEOUT_MS         30000

/* ---- 测试框架 ---- */
#define LV00_CONFIG_TEST_MAX_SUITES              256
#define LV00_CONFIG_TEST_MAX_CASES              4096
#define LV00_CONFIG_TEST_NAME_MAX_LEN            256
#define LV00_CONFIG_TEST_MSG_MAX_LEN             512

/* ---- 熔断器 ---- */
#define LV00_CONFIG_CIRCUIT_OVERFLOW_THRESHOLD     3

/* ---- 代数 ---- */
#define LV00_CONFIG_VALUE_TOO_LARGE          1048576
#define LV00_CONFIG_DOWNGRADE_DENOMINATOR     100000

/* ---- 内存限制 ---- */
#define LV00_CONFIG_DEFAULT_MEMORY_LIMIT_MB        0
#define LV00_CONFIG_HEALTH_SCORE_MAX             100
#define LV00_CONFIG_HEALTH_MEMORY_USAGE_RATIO    0.8
#define LV00_CONFIG_HEALTH_MEMORY_WARNING_PENALTY 10
#define LV00_CONFIG_HEALTH_MEMORY_LEAK_RATIO     0.9
#define LV00_CONFIG_HEALTH_MEMORY_LEAK_PENALTY    20
#define LV00_CONFIG_HEALTH_RECENT_ERROR_PENALTY    5

/* ---- 内部常量（不开放运行时调整） ---- */
#define LV00_CONFIG_ARRAY_GROWTH_FACTOR            2
#define LV00_CONFIG_FNV_HASH_MULTIPLIER    0x01000193U
#define LV00_CONFIG_NODE_INDEX_INITIAL_SIZE       16
#define LV00_CONFIG_CONSTRAINT_INDEX_INITIAL_SIZE 16
#define LV00_CONFIG_INDEX_LOAD_FACTOR           0.75

/* ====================================================================
 * Lv00Config 运行时配置结构体
 * ==================================================================== */

typedef struct {
    /* ---- 求解器 ---- */
    int solver_max_var_id;
    int solver_max_iterations;
    /* ---- 重写引擎 ---- */
    int default_rewrite_limit;
    /* ---- 流式输出 ---- */
    int stream_async_queue_capacity;
    int stream_json_buffer_size;
    int stream_max_callbacks;
    /* ---- 数值精度 ---- */
    int max_precision_bits;
    double root_epsilon;
    /* ---- 解析器 ---- */
    int parser_max_input_length;
    int parser_max_tokens;
    int parser_max_ast_depth;
    int parser_max_ast_nodes;
    /* ---- 运行时防护 ---- */
    int runtime_guard_max_recurse;
    /* ---- 通信协议 ---- */
    int proto_max_draw_cmds;
    int proto_max_table_rows;
    int proto_max_tree_nodes;
    int proto_max_topology;
    int proto_max_proof_steps;
    int proto_max_completions;
    int proto_max_terminal_lines;
    /* ---- 交互几何 ---- */
    int geo_max_objects;
    int geo_max_constraints;
    int geo_max_drag_chain;
    int geo_max_snapshots;
    double geo_min_zoom;
    double geo_max_zoom;
    /* ---- 几何演化 ---- */
    int geoevol_max_param_dim;
    int geoevol_max_rejections;
    double geoevol_min_step;
    double geoevol_max_step;
    /* ---- 证明引擎 ---- */
    int proof_max_depth;
    int proof_max_branches;
    int proof_max_strategies;
    /* ---- 递归/上下文 ---- */
    int max_recursion_depth;
    int context_default_max_depth;
    int context_default_max_steps;
    int context_default_max_consecutive_errors;
    /* ---- 互操作 ---- */
    int interop_cmd_buffer_size;
    int interop_resp_buffer_size;
    int interop_max_params;
    int interop_max_path_len;
    int interop_ws_default_port;
    /* ---- 调试/日志 ---- */
    int log_max_files;
    int log_max_size;
    int log_ring_buffer_capacity;
    /* ---- 运行时监控 ---- */
    int perf_sample_max_count;
    int timer_max_depth;
    /* ---- 插件 ---- */
    int max_plugins;
    int max_interfaces;
    /* ---- 数值后端 ---- */
    int backend_step_limit;
    int backend_timeout_ms;
    /* ---- 熔断器 ---- */
    int circuit_overflow_threshold;
    /* ---- 内存 ---- */
    int default_memory_limit_mb;
} Lv00Config;

/* ====================================================================
 * 运行时配置 API
 * ==================================================================== */

/** @brief 获取默认配置（编译期宏值填充） */
const Lv00Config *lv00_config_default(void);

/** @brief 获取当前生效配置 */
const Lv00Config *lv00_config_current(void);

/** @brief 应用自定义配置 */
int lv00_config_apply(const Lv00Config *cfg);

/** @brief 从 JSON 文件加载配置 */
int lv00_config_load_json(const char *json_path);

/** @brief 将当前配置序列化为 JSON 字符串 */
int lv00_config_to_json(char *buf, size_t buf_size);

/* ====================================================================
 * 兼容层 —— 旧宏名重定向（逐个模块）
 * ==================================================================== */

/* solver.h */
#ifndef SOLVER_MAX_VAR_ID
#define SOLVER_MAX_VAR_ID LV00_CONFIG_SOLVER_MAX_VAR_ID
#endif

/* symbolic_coord.h */
#ifndef MAX_MODULE_DEPTH
#define MAX_MODULE_DEPTH LV00_CONFIG_MAX_MODULE_DEPTH
#endif
#ifndef BIT_CUTOFF_THRESHOLD
#define BIT_CUTOFF_THRESHOLD LV00_CONFIG_BIT_CUTOFF_THRESHOLD
#endif
#ifndef MAX_PRECISION_BITS
#define MAX_PRECISION_BITS LV00_CONFIG_MAX_PRECISION_BITS
#endif

/* rewrite.h */
#ifndef WL_ITERATIONS
#define WL_ITERATIONS LV00_CONFIG_WL_ITERATIONS
#endif
#ifndef WL_HISTORY_SIZE
#define WL_HISTORY_HISTORY_SIZE LV00_CONFIG_WL_HISTORY_SIZE
#endif

/* stream.h */
#ifndef STREAM_ASYNC_QUEUE_DEFAULT_CAPACITY
#define STREAM_ASYNC_QUEUE_DEFAULT_CAPACITY LV00_CONFIG_STREAM_ASYNC_QUEUE_CAPACITY
#endif
#ifndef STREAM_JSON_BUFFER_DEFAULT_SIZE
#define STREAM_JSON_BUFFER_DEFAULT_SIZE LV00_CONFIG_STREAM_JSON_BUFFER_SIZE
#endif

/* parser */
#ifndef LV00_MAX_AST_NODES
#define LV00_MAX_AST_NODES LV00_CONFIG_PARSER_MAX_AST_NODES
#endif
#ifndef LV00_MAX_TOKEN_LENGTH
#define LV00_MAX_TOKEN_LENGTH LV00_CONFIG_PARSER_MAX_TOKEN_LENGTH
#endif
#ifndef LV00_MAX_INPUT_LENGTH
#define LV00_MAX_INPUT_LENGTH LV00_CONFIG_PARSER_MAX_INPUT_LENGTH
#endif
#ifndef LV00_MAX_AST_DEPTH
#define LV00_MAX_AST_DEPTH LV00_CONFIG_PARSER_MAX_AST_DEPTH
#endif

/* plugin_system.h */
#ifndef LV00_MAX_PLUGINS
#define LV00_MAX_PLUGINS LV00_CONFIG_MAX_PLUGINS
#endif
#ifndef LV00_MAX_INTERFACES
#define LV00_MAX_INTERFACES LV00_CONFIG_MAX_INTERFACES
#endif
#ifndef LV00_PLUGIN_NAME_MAX
#define LV00_PLUGIN_NAME_MAX LV00_CONFIG_PLUGIN_NAME_MAX
#endif
#ifndef LV00_PLUGIN_DESC_MAX
#define LV00_PLUGIN_DESC_MAX LV00_CONFIG_PLUGIN_DESC_MAX
#endif
#ifndef LV00_PLUGIN_AUTHOR_MAX
#define LV00_PLUGIN_AUTHOR_MAX LV00_CONFIG_PLUGIN_AUTHOR_MAX
#endif
#ifndef LV00_PLUGIN_PATH_MAX
#define LV00_PLUGIN_PATH_MAX LV00_CONFIG_PLUGIN_PATH_MAX
#endif

/* lv00_protocol.h */
#ifndef LV00_PROTO_MAX_DRAW_CMDS
#define LV00_PROTO_MAX_DRAW_CMDS LV00_CONFIG_PROTO_MAX_DRAW_CMDS
#endif
#ifndef LV00_PROTO_MAX_TABLE_ROWS
#define LV00_PROTO_MAX_TABLE_ROWS LV00_CONFIG_PROTO_MAX_TABLE_ROWS
#endif
#ifndef LV00_PROTO_MAX_TREE_NODES
#define LV00_PROTO_MAX_TREE_NODES LV00_CONFIG_PROTO_MAX_TREE_NODES
#endif
#ifndef LV00_PROTO_MAX_TOPOLOGY
#define LV00_PROTO_MAX_TOPOLOGY LV00_CONFIG_PROTO_MAX_TOPOLOGY
#endif
#ifndef LV00_PROTO_MAX_PROOF_STEPS
#define LV00_PROTO_MAX_PROOF_STEPS LV00_CONFIG_PROTO_MAX_PROOF_STEPS
#endif
#ifndef LV00_PROTO_MAX_COMPLETIONS
#define LV00_PROTO_MAX_COMPLETIONS LV00_CONFIG_PROTO_MAX_COMPLETIONS
#endif
#ifndef LV00_PROTO_MAX_TERMINAL_LINES
#define LV00_PROTO_MAX_TERMINAL_LINES LV00_CONFIG_PROTO_MAX_TERMINAL_LINES
#endif
#ifndef LV00_PROTO_STR_LEN
#define LV00_PROTO_STR_LEN LV00_CONFIG_PROTO_STR_LEN
#endif
#ifndef LV00_PROTO_LABEL_LEN
#define LV00_PROTO_LABEL_LEN LV00_CONFIG_PROTO_LABEL_LEN
#endif
#ifndef LV00_PROTO_DESC_LEN
#define LV00_PROTO_DESC_LEN LV00_CONFIG_PROTO_DESC_LEN
#endif
#ifndef LV00_PROTO_BUFFER_LEN
#define LV00_PROTO_BUFFER_LEN LV00_CONFIG_PROTO_BUFFER_LEN
#endif

/* interactive_geo.h */
#ifndef LV00_GEO_MAX_OBJECTS
#define LV00_GEO_MAX_OBJECTS LV00_CONFIG_GEO_MAX_OBJECTS
#endif
#ifndef LV00_GEO_MAX_CONSTRAINTS
#define LV00_GEO_MAX_CONSTRAINTS LV00_CONFIG_GEO_MAX_CONSTRAINTS
#endif
#ifndef LV00_GEO_MAX_DRAG_CHAIN
#define LV00_GEO_MAX_DRAG_CHAIN LV00_CONFIG_GEO_MAX_DRAG_CHAIN
#endif
#ifndef LV00_GEO_MAX_SNAPSHOTS
#define LV00_GEO_MAX_SNAPSHOTS LV00_CONFIG_GEO_MAX_SNAPSHOTS
#endif
#ifndef LV00_GEO_SCRIPT_BUFFER_SIZE
#define LV00_GEO_SCRIPT_BUFFER_SIZE LV00_CONFIG_GEO_SCRIPT_BUFFER_SIZE
#endif
#ifndef LV00_GEO_STATE_BUFFER_SIZE
#define LV00_GEO_STATE_BUFFER_SIZE LV00_CONFIG_GEO_STATE_BUFFER_SIZE
#endif
#ifndef LV00_GEO_MIN_ZOOM
#define LV00_GEO_MIN_ZOOM LV00_CONFIG_GEO_MIN_ZOOM
#endif
#ifndef LV00_GEO_MAX_ZOOM
#define LV00_GEO_MAX_ZOOM LV00_CONFIG_GEO_MAX_ZOOM
#endif

/* geom_evol.h */
#ifndef GEOEVOL_MAX_PARAM_DIM
#define GEOEVOL_MAX_PARAM_DIM LV00_CONFIG_GEOEVOL_MAX_PARAM_DIM
#endif
#ifndef GEOEVOL_ADAMS_MAX_ORDER
#define GEOEVOL_ADAMS_MAX_ORDER LV00_CONFIG_GEOEVOL_ADAMS_MAX_ORDER
#endif
#ifndef GEOEVOL_MIN_STEP
#define GEOEVOL_MIN_STEP LV00_CONFIG_GEOEVOL_MIN_STEP
#endif
#ifndef GEOEVOL_MAX_STEP
#define GEOEVOL_MAX_STEP LV00_CONFIG_GEOEVOL_MAX_STEP
#endif
#ifndef GEOEVOL_PI_SMOOTH_FACTOR
#define GEOEVOL_PI_SMOOTH_FACTOR LV00_CONFIG_GEOEVOL_PI_SMOOTH_FACTOR
#endif
#ifndef GEOEVOL_MAX_REJECTIONS
#define GEOEVOL_MAX_REJECTIONS LV00_CONFIG_GEOEVOL_MAX_REJECTIONS
#endif

/* proof_engine_enhanced.h */
#ifndef LV00_PROOF_MAX_DEPTH
#define LV00_PROOF_MAX_DEPTH LV00_CONFIG_PROOF_MAX_DEPTH
#endif
#ifndef LV00_PROOF_MAX_BRANCHES
#define LV00_PROOF_MAX_BRANCHES LV00_CONFIG_PROOF_MAX_BRANCHES
#endif
#ifndef LV00_PROOF_MAX_STRATEGIES
#define LV00_PROOF_MAX_STRATEGIES LV00_CONFIG_PROOF_MAX_STRATEGIES
#endif
#ifndef LV00_TRACE_TREE_MAX_DEPTH
#define LV00_TRACE_TREE_MAX_DEPTH LV00_CONFIG_TRACE_TREE_MAX_DEPTH
#endif

/* recursion.h */
#ifndef LV00_MAX_RECURSION_DEPTH
#define LV00_MAX_RECURSION_DEPTH LV00_CONFIG_MAX_RECURSION_DEPTH
#endif

/* context.h */
#ifndef LV00_CONTEXT_DEFAULT_MAX_DEPTH
#define LV00_CONTEXT_DEFAULT_MAX_DEPTH LV00_CONFIG_CONTEXT_DEFAULT_MAX_DEPTH
#endif
#ifndef LV00_CONTEXT_MAX_RECURSION_DEPTH
#define LV00_CONTEXT_MAX_RECURSION_DEPTH LV00_CONFIG_CONTEXT_MAX_RECURSION_DEPTH
#endif
#ifndef LV00_CONTEXT_DEFAULT_MAX_STEPS
#define LV00_CONTEXT_DEFAULT_MAX_STEPS LV00_CONFIG_CONTEXT_DEFAULT_MAX_STEPS
#endif
#ifndef LV00_CONTEXT_DEFAULT_MAX_CONSECUTIVE_ERRORS
#define LV00_CONTEXT_DEFAULT_MAX_CONSECUTIVE_ERRORS LV00_CONFIG_CONTEXT_DEFAULT_MAX_CONSECUTIVE_ERRORS
#endif
#ifndef LV00_CONTEXT_REASONING_STACK_DEFAULT_CAPACITY
#define LV00_CONTEXT_REASONING_STACK_DEFAULT_CAPACITY LV00_CONFIG_CONTEXT_REASONING_STACK_DEFAULT_CAPACITY
#endif
#ifndef LV00_CONTEXT_REASONING_STACK_MAX_DEPTH
#define LV00_CONTEXT_REASONING_STACK_MAX_DEPTH LV00_CONFIG_CONTEXT_REASONING_STACK_MAX_DEPTH
#endif

/* runtime_guard.h */
#ifndef LV00_RUNTIME_GUARD_MAX_RECURSE
#define LV00_RUNTIME_GUARD_MAX_RECURSE LV00_CONFIG_RUNTIME_GUARD_MAX_RECURSE
#endif

/* interop.h */
#ifndef INTEROP_CMD_BUFFER_SIZE
#define INTEROP_CMD_BUFFER_SIZE LV00_CONFIG_INTEROP_CMD_BUFFER_SIZE
#endif
#ifndef INTEROP_RESP_BUFFER_SIZE
#define INTEROP_RESP_BUFFER_SIZE LV00_CONFIG_INTEROP_RESP_BUFFER_SIZE
#endif
#ifndef INTEROP_MAX_PARAMS
#define INTEROP_MAX_PARAMS LV00_CONFIG_INTEROP_MAX_PARAMS
#endif
#ifndef INTEROP_WS_DEFAULT_PORT
#define INTEROP_WS_DEFAULT_PORT LV00_CONFIG_INTEROP_WS_DEFAULT_PORT
#endif
#ifndef INTEROP_MAX_PATH_LEN
#define INTEROP_MAX_PATH_LEN LV00_CONFIG_INTEROP_MAX_PATH_LEN
#endif
#ifndef INTEROP_MAX_COMPLETIONS
#define INTEROP_MAX_COMPLETIONS LV00_CONFIG_INTEROP_MAX_COMPLETIONS
#endif

/* debug.h */
#ifndef LV00_LOG_MAX_FILES
#define LV00_LOG_MAX_FILES LV00_CONFIG_LOG_MAX_FILES
#endif
#ifndef LV00_LOG_MAX_SIZE
#define LV00_LOG_MAX_SIZE LV00_CONFIG_LOG_MAX_SIZE
#endif
#ifndef LV00_LOG_PATH_MAX
#define LV00_LOG_PATH_MAX LV00_CONFIG_LOG_PATH_MAX
#endif
#ifndef LV00_LOG_RING_BUFFER_DEFAULT_CAPACITY
#define LV00_LOG_RING_BUFFER_DEFAULT_CAPACITY LV00_CONFIG_LOG_RING_BUFFER_CAPACITY
#endif

/* runtime_monitor.h */
#ifndef LV00_LOG_MSG_MAX_LEN
#define LV00_LOG_MSG_MAX_LEN LV00_CONFIG_LOG_MSG_MAX_LEN
#endif
#ifndef LV00_LOG_TAG_MAX_LEN
#define LV00_LOG_TAG_MAX_LEN LV00_CONFIG_LOG_TAG_MAX_LEN
#endif
#ifndef LV00_METRIC_NAME_MAX_LEN
#define LV00_METRIC_NAME_MAX_LEN LV00_CONFIG_METRIC_NAME_MAX_LEN
#endif
#ifndef LV00_TIMER_MAX_DEPTH
#define LV00_TIMER_MAX_DEPTH LV00_CONFIG_TIMER_MAX_DEPTH
#endif
#ifndef LV00_PERF_SAMPLE_MAX_COUNT
#define LV00_PERF_SAMPLE_MAX_COUNT LV00_CONFIG_PERF_SAMPLE_MAX_COUNT
#endif

/* memory_pool.h */
#ifndef LV00_POOL_DEFAULT_CAPACITY
#define LV00_POOL_DEFAULT_CAPACITY LV00_CONFIG_POOL_DEFAULT_CAPACITY
#endif
#ifndef LV00_LINEAR_ALLOCATOR_BLOCK_SIZE
#define LV00_LINEAR_ALLOCATOR_BLOCK_SIZE LV00_CONFIG_LINEAR_ALLOCATOR_BLOCK_SIZE
#endif
#ifndef LV00_LRU_CACHE_DEFAULT_CAPACITY
#define LV00_LRU_CACHE_DEFAULT_CAPACITY LV00_CONFIG_LRU_CACHE_DEFAULT_CAPACITY
#endif
#ifndef LV00_MEM_STAT_MAX_TYPES
#define LV00_MEM_STAT_MAX_TYPES LV00_CONFIG_MEM_STAT_MAX_TYPES
#endif

/* test_framework.h */
#ifndef LV00_TEST_MAX_SUITES
#define LV00_TEST_MAX_SUITES LV00_CONFIG_TEST_MAX_SUITES
#endif
#ifndef LV00_TEST_MAX_CASES
#define LV00_TEST_MAX_CASES LV00_CONFIG_TEST_MAX_CASES
#endif
#ifndef LV00_TEST_NAME_MAX_LEN
#define LV00_TEST_NAME_MAX_LEN LV00_CONFIG_TEST_NAME_MAX_LEN
#endif
#ifndef LV00_TEST_MSG_MAX_LEN
#define LV00_TEST_MSG_MAX_LEN LV00_CONFIG_TEST_MSG_MAX_LEN
#endif

/* numerical_backend.h */
#ifndef LV00_BACKEND_NAME_MAX
#define LV00_BACKEND_NAME_MAX LV00_CONFIG_BACKEND_NAME_MAX
#endif

/* ---- 通用 ---- */
#ifndef LV00_PUBLIC_API
#define LV00_PUBLIC_API
#endif
#ifndef LV00_DEFAULT_REWRITE_STEP_LIMIT
#define LV00_DEFAULT_REWRITE_STEP_LIMIT LV00_CONFIG_DEFAULT_REWRITE_LIMIT
#endif
#ifndef LV00_DEFAULT_MAX_ITERATIONS
#define LV00_DEFAULT_MAX_ITERATIONS LV00_CONFIG_SOLVER_MAX_ITERATIONS
#endif
#ifndef LV00_DEFAULT_PRECISION_BITS
#define LV00_DEFAULT_PRECISION_BITS LV00_CONFIG_DEFAULT_PRECISION_BITS
#endif
#ifndef LV00_DEFAULT_MEMORY_LIMIT_MB
#define LV00_DEFAULT_MEMORY_LIMIT_MB LV00_CONFIG_DEFAULT_MEMORY_LIMIT_MB
#endif

/* 内存池短别名 */
#ifndef LV00_CONSTRAINT_NODE_SIZE
#define LV00_CONSTRAINT_NODE_SIZE LV00_CONFIG_POOL_CONSTRAINT_NODE_SIZE
#endif
#ifndef LV00_SYMBOLIC_COORD_SIZE
#define LV00_SYMBOLIC_COORD_SIZE LV00_CONFIG_POOL_SYMBOLIC_COORD_SIZE
#endif
#ifndef LV00_PROOF_STEP_SIZE
#define LV00_PROOF_STEP_SIZE LV00_CONFIG_POOL_PROOF_STEP_SIZE
#endif
#ifndef LV00_CONSTRAINT_SIZE
#define LV00_CONSTRAINT_SIZE LV00_CONFIG_POOL_CONSTRAINT_SIZE
#endif

/* 内部常量 */
#ifndef LV00_ARRAY_GROWTH_FACTOR
#define LV00_ARRAY_GROWTH_FACTOR LV00_CONFIG_ARRAY_GROWTH_FACTOR
#endif
#ifndef LV00_FNV_HASH_MULTIPLIER
#define LV00_FNV_HASH_MULTIPLIER LV00_CONFIG_FNV_HASH_MULTIPLIER
#endif
#ifndef LV00_NODE_INDEX_INITIAL_SIZE
#define LV00_NODE_INDEX_INITIAL_SIZE LV00_CONFIG_NODE_INDEX_INITIAL_SIZE
#endif
#ifndef LV00_CONSTRAINT_INDEX_INITIAL_SIZE
#define LV00_CONSTRAINT_INDEX_INITIAL_SIZE LV00_CONFIG_CONSTRAINT_INDEX_INITIAL_SIZE
#endif
#ifndef LV00_INDEX_LOAD_FACTOR
#define LV00_INDEX_LOAD_FACTOR LV00_CONFIG_INDEX_LOAD_FACTOR
#endif
#ifndef LV00_INITIAL_ARRAY_CAPACITY
#define LV00_INITIAL_ARRAY_CAPACITY LV00_CONFIG_INITIAL_ARRAY_CAPACITY
#endif
#ifndef LV00_FNV64_OFFSET_BASIS
#define LV00_FNV64_OFFSET_BASIS 14695981039346656037ULL
#endif
#ifndef LV00_FNV64_PRIME
#define LV00_FNV64_PRIME 1099511628211ULL
#endif
#ifndef LV00_ARRAY_COUNT
#define LV00_ARRAY_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

/* 根/位/降级常量 */
#ifndef LV00_MAX_PRECISION_BITS
#define LV00_MAX_PRECISION_BITS           LV00_CONFIG_MAX_PRECISION_BITS
#endif
#ifndef LV00_BIT_CUTOFF_THRESHOLD
#define LV00_BIT_CUTOFF_THRESHOLD         LV00_CONFIG_BIT_CUTOFF_THRESHOLD
#endif
#ifndef LV00_CONTINUED_FRACTION_MAX_ITER
#define LV00_CONTINUED_FRACTION_MAX_ITER LV00_CONFIG_CONTINUED_FRACTION_MAX_ITER
#endif
#ifndef LV00_MAX_SUBINTERVALS
#define LV00_MAX_SUBINTERVALS LV00_CONFIG_MAX_SUBINTERVALS
#endif
#ifndef LV00_ROOT_EPSILON
#define LV00_ROOT_EPSILON LV00_CONFIG_ROOT_EPSILON
#endif
#ifndef LV00_CIRCUIT_OVERFLOW_THRESHOLD
#define LV00_CIRCUIT_OVERFLOW_THRESHOLD LV00_CONFIG_CIRCUIT_OVERFLOW_THRESHOLD
#endif
#ifndef LV00_VALUE_TOO_LARGE
#define LV00_VALUE_TOO_LARGE LV00_CONFIG_VALUE_TOO_LARGE
#endif
#ifndef LV00_DOWNGRADE_DENOMINATOR
#define LV00_DOWNGRADE_DENOMINATOR LV00_CONFIG_DOWNGRADE_DENOMINATOR
#endif

/* 路径分隔符 */
#ifndef LV00_PATH_SEPARATOR
#ifdef _WIN32
#define LV00_PATH_SEPARATOR '\\'
#else
#define LV00_PATH_SEPARATOR '/'
#endif
#endif
/* localtime */
#ifndef LV00_LOCALTIME
#ifdef _WIN32
#define LV00_LOCALTIME(num, p) localtime_s(p, num)
#else
#define LV00_LOCALTIME(num, p) localtime_r(num, p)
#endif
#endif
/* 健康检查 */
#ifndef LV00_HEALTH_SCORE_MAX
#define LV00_HEALTH_SCORE_MAX              LV00_CONFIG_HEALTH_SCORE_MAX
#endif
#ifndef LV00_HEALTH_MEMORY_USAGE_RATIO
#define LV00_HEALTH_MEMORY_USAGE_RATIO     LV00_CONFIG_HEALTH_MEMORY_USAGE_RATIO
#endif
#ifndef LV00_HEALTH_MEMORY_WARNING_PENALTY
#define LV00_HEALTH_MEMORY_WARNING_PENALTY LV00_CONFIG_HEALTH_MEMORY_WARNING_PENALTY
#endif
#ifndef LV00_HEALTH_MEMORY_LEAK_RATIO
#define LV00_HEALTH_MEMORY_LEAK_RATIO      LV00_CONFIG_HEALTH_MEMORY_LEAK_RATIO
#endif
#ifndef LV00_HEALTH_MEMORY_LEAK_PENALTY
#define LV00_HEALTH_MEMORY_LEAK_PENALTY    LV00_CONFIG_HEALTH_MEMORY_LEAK_PENALTY
#endif
#ifndef LV00_HEALTH_RECENT_ERROR_PENALTY
#define LV00_HEALTH_RECENT_ERROR_PENALTY   LV00_CONFIG_HEALTH_RECENT_ERROR_PENALTY
#endif

#ifdef __cplusplus
}
#endif
#endif /* LV00_CONFIG_H */
