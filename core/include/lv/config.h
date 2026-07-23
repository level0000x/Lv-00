/**
 * @file config.h
 * @brief Lv-00 集中化配置
 *
 * 编译期常量（仅 ~30 个，影响 sizeof / 数组维度）→ lv_CONFIG_* 宏
 * 运行时配置（~80 个限制）→ lvConfig 结构体 + lv_config_current()
 *
 * 用法:
 *   // 不重编译就能调上限
 *   const lvConfig *cfg = lv_config_current();
 *   if (step_count > cfg->proto_max_proof_steps) break;
 *
 *   // JSON 文件改配置，重启即生效
 *   lv_config_load_json("lv.config.json"); lv_init();
 *
 * @version 1.1.0
 */
#ifndef lv_CONFIG_H
#define lv_CONFIG_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* ====================================================================
 * 编译期常量 —— 仅影响 sizeof / 栈数组 / #if 判断
 * ==================================================================== */

/* ---- 对象池块尺寸（影响结构体 sizeof） ---- */
#define lv_CONFIG_POOL_CONSTRAINT_NODE_SIZE     128
#define lv_CONFIG_POOL_CONSTRAINT_SIZE           96
#define lv_CONFIG_POOL_SYMBOLIC_COORD_SIZE       64
#define lv_CONFIG_POOL_PROOF_STEP_SIZE          128

/* ---- 内存池容量 ---- */
#define lv_CONFIG_POOL_DEFAULT_CAPACITY        1024
#define lv_CONFIG_LINEAR_ALLOCATOR_BLOCK_SIZE 65536
#define lv_CONFIG_LRU_CACHE_DEFAULT_CAPACITY    256
#define lv_CONFIG_MEM_STAT_MAX_TYPES             64

/* ---- 数据结构 ---- */
#define lv_CONFIG_INITIAL_ARRAY_CAPACITY          8
#define lv_CONFIG_INITIAL_HASH_INDEX_CAPACITY    64
#define lv_CONFIG_ARRAY_GROWTH_FACTOR             2
#define lv_CONFIG_FNV_HASH_MULTIPLIER    0x01000193U
#define lv_CONFIG_NODE_INDEX_INITIAL_SIZE        16
#define lv_CONFIG_CONSTRAINT_INDEX_INITIAL_SIZE  16
#define lv_CONFIG_INDEX_LOAD_FACTOR            0.75
#define lv_FNV64_OFFSET_BASIS          14695981039346656037ULL
#define lv_FNV64_PRIME                  1099511628211ULL
#define lv_ARRAY_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))

/* ---- 缓冲区 / 字符串长度（影响 char buf[N] 声明） ---- */
#define lv_CONFIG_GRAPH_ERROR_BUFFER_SIZE        256
#define lv_CONFIG_GRAPH_SERIALIZE_BUFFER_SIZE    256
#define lv_CONFIG_ENGINE_ERROR_BUFFER_SIZE       256
#define lv_CONFIG_PARSER_MAX_BUFFER_SIZE         256
#define lv_CONFIG_PARSER_MAX_TEMP_MSG_SIZE       128

#define lv_CONFIG_INTEROP_CMD_BUFFER_SIZE       4096
#define lv_CONFIG_INTEROP_RESP_BUFFER_SIZE     65536
#define lv_CONFIG_INTEROP_MAX_PATH_LEN           512
#define lv_CONFIG_LOG_PATH_MAX                  256

#define lv_CONFIG_STREAM_JSON_BUFFER_SIZE       4096

#define lv_CONFIG_PROTO_STR_LEN                  64
#define lv_CONFIG_PROTO_LABEL_LEN               128
#define lv_CONFIG_PROTO_DESC_LEN                256
#define lv_CONFIG_PROTO_BUFFER_LEN             4096

#define lv_CONFIG_GEO_SCRIPT_BUFFER_SIZE       65536
#define lv_CONFIG_GEO_STATE_BUFFER_SIZE       131072

#define lv_CONFIG_LOG_MSG_MAX_LEN              4096
#define lv_CONFIG_LOG_TAG_MAX_LEN                64
#define lv_CONFIG_METRIC_NAME_MAX_LEN           128

#define lv_CONFIG_PLUGIN_NAME_MAX                64
#define lv_CONFIG_PLUGIN_DESC_MAX               256
#define lv_CONFIG_PLUGIN_AUTHOR_MAX             128
#define lv_CONFIG_PLUGIN_PATH_MAX               512

#define lv_CONFIG_BACKEND_NAME_MAX               64

#define lv_CONFIG_TEST_NAME_MAX_LEN             256
#define lv_CONFIG_TEST_MSG_MAX_LEN              512

#define lv_CONFIG_MAX_LABEL_LENGTH              256
#define lv_CONFIG_MAX_FORMULA_LENGTH           1024
#define lv_CONFIG_MAX_VARIABLE_NAME_LENGTH      128
#define lv_CONFIG_MAX_REPLACEMENT_TERM_LENGTH   512
#define lv_CONFIG_MAX_PROOF_REFS                 64

/* ---- 数值常量（compile-time 表达式中使用） ---- */
#define lv_CONFIG_DEFAULT_PRECISION_BITS         52
#define lv_CONFIG_ROOT_EPSILON                 1e-12

/* ---- 数值精度常量（浮点数比较容差） ---- */
/* 通用高精度容差 */
#ifndef lv_EPSILON_SUPERTINY
#define lv_EPSILON_SUPERTINY 1e-15   /* 极高精度：用于Adams/BDF步长、Groebner基 */
#endif
#ifndef lv_EPSILON_ULTRA
#define lv_EPSILON_ULTRA     1e-12   /* 超高精度：用于根求解、收敛判断 */
#endif
#ifndef lv_EPSILON_HIGH
#define lv_EPSILON_HIGH      1e-10   /* 高精度：用于角度/距离判断 */
#endif
#ifndef lv_EPSILON_MEDIUM
#define lv_EPSILON_MEDIUM    1e-9    /* 中等精度：用于几何谓词（orientation） */
#endif
#ifndef lv_EPSILON_LOW
#define lv_EPSILON_LOW       1e-6    /* 低精度：用于近似计算 */
#endif

/* 几何专用容差 */
#ifndef lv_GEO_COLLINEAR_EPSILON
#define lv_GEO_COLLINEAR_EPSILON 1e-9   /* 共线性判断容差 */
#endif
#ifndef lv_GEO_DISTANCE_EPSILON
#define lv_GEO_DISTANCE_EPSILON 1e-8    /* 距离判断容差 */
#endif
#ifndef lv_GEO_ANGLE_EPSILON
#define lv_GEO_ANGLE_EPSILON 1e-10      /* 角度相等容差 */
#endif

/* 代数运算安全阈值 */
#ifndef lv_SINGULARITY_THRESHOLD
#define lv_SINGULARITY_THRESHOLD 1e-12   /* 矩阵奇异性判断 */
#endif
#ifndef lv_NORMALIZATION_THRESHOLD
#define lv_NORMALIZATION_THRESHOLD 1e-15  /* 向量归一化容差 */
#endif

/* 数值范围极限（用于哨兵值） */
#ifndef lv_INFINITY_SENTINEL
#define lv_INFINITY_SENTINEL 1e308      /* "无穷大"哨兵 */
#endif
#ifndef lv_TINY_SENTINEL
#define lv_TINY_SENTINEL  1e-300        /* "接近零"哨兵 */
#endif
#ifndef lv_HUGE_NUMBER
#define lv_HUGE_NUMBER     1e30        /* 极大数阈值 */
#endif
#ifndef lv_LARGE_NUMBER
#define lv_LARGE_NUMBER    1e18        /* 大数阈值 */
#endif
#ifndef lv_BIG_NUMBER
#define lv_BIG_NUMBER      1e9         /* 10亿级阈值 */
#endif

/* ---- 常用数学系数与比例因子 ---- */
/* 缩放系数 */
#ifndef lv_HALF
#define lv_HALF         0.5         /* 1/2 */
#endif
#ifndef lv_THIRD
#define lv_THIRD        0.333333333333333333333   /* 1/3 */
#endif
#ifndef lv_QUARTER
#define lv_QUARTER      0.25        /* 1/4 */
#endif
#ifndef lv_TENTH
#define lv_TENTH        0.1         /* 1/10 */
#endif

/* 角度系数（弧度↔度） */
#ifndef lv_DEG_TO_RAD
#define lv_DEG_TO_RAD   0.017453292519943295769   /* π/180 */
#endif
#ifndef lv_RAD_TO_DEG
#define lv_RAD_TO_DEG   57.2957795130823208768    /* 180/π */
#endif
#ifndef lv_PI
#define lv_PI           3.1415926535897932384626  /* π */
#endif
#ifndef lv_TWO_PI
#define lv_TWO_PI       6.28318530717958647692   /* 2π */
#endif
#ifndef lv_HALF_PI
#define lv_HALF_PI      1.57079632679489661923   /* π/2 */
#endif
#ifndef lv_QUARTER_PI
#define lv_QUARTER_PI   0.78539816339744830962   /* π/4 */
#endif

/* 颜色通道（RGB 0-255 归一化） */
#ifndef lv_COLOR_CHANNEL_MAX
#define lv_COLOR_CHANNEL_MAX 255.0f   /* RGB最大通道值 */
#endif
#ifndef lv_COLOR_CHANNEL_HALF
#define lv_COLOR_CHANNEL_HALF 0.5f    /* 0.5f */
#endif

/* 哈希表负载因子 */
#ifndef lv_HASH_LOAD_FACTOR_MAX
#define lv_HASH_LOAD_FACTOR_MAX 0.75  /* 最大负载因子 */
#endif
#ifndef lv_HASH_LOAD_FACTOR_MIN
#define lv_HASH_LOAD_FACTOR_MIN 0.25  /* 最小负载因子 */
#endif

/* 缓冲区/块大小（2的幂次） */
#ifndef lv_KB
#define lv_KB           1024.0       /* 1KB */
#endif
#ifndef lv_MB
#define lv_MB           1048576.0    /* 1MB */
#endif
#ifndef lv_GB
#define lv_GB           1073741824.0 /* 1GB */
#endif
#ifndef lv_KB_I
#define lv_KB_I         1024         /* 1KB (int) */
#endif
#ifndef lv_MB_I
#define lv_MB_I         1048576      /* 1MB (int) */
#endif

/* == 以下全部移入 lvConfig 运行时 == */

typedef struct {
    /* ---- 求解器 ---- */
    int solver_max_var_id;
    int solver_max_iterations;

    /* ---- 约束图 ---- */
    int max_module_depth;
    int graph_adj_max_per_node;

    /* ---- 重写引擎 ---- */
    int default_rewrite_limit;
    int wl_iterations;
    int wl_history_size;

    /* ---- 流式输出 ---- */
    int stream_async_queue_capacity;
    int stream_initial_callbacks;
    int stream_max_callbacks;
    int stream_default_throttle_ms;

    /* ---- 数值精度 ---- */
    int bit_cutoff_threshold;
    int max_precision_bits;
    int continued_fraction_max_iter;
    int max_subintervals;

    /* ---- MiniKernel ---- */
    int mini_kernel_max_statements;
    int mini_kernel_max_proof_depth;
    int mini_kernel_verify_timeout_ms;

    /* ---- 压力测试 ---- */
    int stress_test_default_chain;
    int stress_test_max_poly_degree;

    /* ---- 解析器 ---- */
    int parser_max_input_length;
    int parser_max_tokens;
    int parser_max_ast_depth;
    int parser_max_ast_nodes;
    int parser_max_token_length;
    int parser_max_coordinates;
    int parser_max_vertices;
    int parser_max_polygon_vertices;
    int parser_max_statements;
    int parser_max_arguments;
    int parser_max_participants;

    /* ---- 运行时防护 ---- */
    int runtime_guard_max_recurse;
    int runtime_guard_spin_attempts;
    int runtime_guard_write_warn_us;

    /* ---- UI-Kernel 通信协议 ---- */
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

    /* ---- 几何演化 (ODE) ---- */
    int geoevol_max_param_dim;
    int geoevol_adams_max_order;
    int geoevol_max_rejections;
    double geoevol_min_step;
    double geoevol_max_step;
    double geoevol_pi_smooth_factor;

    /* ---- 证明引擎 ---- */
    int proof_max_depth;
    int proof_max_branches;
    int proof_max_strategies;
    int trace_tree_max_depth;

    /* ---- 递归 / 上下文 ---- */
    int max_recursion_depth;
    int context_default_max_depth;
    int context_max_recursion_depth;
    int context_default_max_steps;
    int context_default_max_consecutive_errors;
    int context_reasoning_stack_default_capacity;
    int context_reasoning_stack_max_depth;

    /* ---- 互操作 ---- */
    int interop_max_params;
    int interop_max_completions;
    int interop_ws_default_port;

    /* ---- 调试 / 日志 ---- */
    int log_max_files;
    int log_max_size;
    int log_ring_buffer_capacity;

    /* ---- 运行时监控 ---- */
    int perf_sample_max_count;
    int timer_max_depth;

    /* ---- 插件系统 ---- */
    int max_plugins;
    int max_interfaces;

    /* ---- 数值后端 ---- */
    int backend_step_limit;
    int backend_timeout_ms;

    /* ---- 测试框架 ---- */
    int test_max_suites;
    int test_max_cases;

    /* ---- 熔断器 ---- */
    int circuit_overflow_threshold;

    /* ---- 代数 ---- */
    int value_too_large;
    int downgrade_denominator;

    /* ---- 内存 ---- */
    int default_memory_limit_mb;

    /* ---- 健康检查 ---- */
    int health_score_max;
    double health_memory_usage_ratio;
    int health_memory_warning_penalty;
    double health_memory_leak_ratio;
    int health_memory_leak_penalty;
    int health_recent_error_penalty;
} lvConfig;

/* ====================================================================
 * 运行时配置 API
 * ==================================================================== */

const lvConfig *lv_config_default(void);
const lvConfig *lv_config_current(void);
int               lv_config_apply(const lvConfig *cfg);
int               lv_config_load_json(const char *json_path);
int               lv_config_to_json(char *buf, size_t buf_size);

/* ====================================================================
 * 运行时单字段修改 API（不重编译，立即生效）
 * ==================================================================== */

/* ---- 类型安全 setter（高频字段，IDE 自动补全） ---- */
void lv_config_set_solver_max_var_id(int val);
void lv_config_set_solver_max_iterations(int val);
void lv_config_set_proof_max_depth(int val);
void lv_config_set_proof_max_branches(int val);
void lv_config_set_proto_max_draw_cmds(int val);
void lv_config_set_proto_max_proof_steps(int val);
void lv_config_set_proto_max_terminal_lines(int val);
void lv_config_set_geo_max_objects(int val);
void lv_config_set_geo_max_constraints(int val);
void lv_config_set_geo_min_zoom(double val);
void lv_config_set_geo_max_zoom(double val);
void lv_config_set_parser_max_input_length(int val);
void lv_config_set_parser_max_ast_nodes(int val);
void lv_config_set_max_recursion_depth(int val);
void lv_config_set_default_rewrite_limit(int val);
void lv_config_set_geoevol_max_param_dim(int val);
void lv_config_set_geoevol_max_rejections(int val);
void lv_config_set_stream_max_callbacks(int val);
void lv_config_set_max_plugins(int val);

/* ---- 通用 key-value setter（低频字段用，一次调用不改编译） ---- */
bool lv_config_set_int(const char *key, int val);
bool lv_config_set_double(const char *key, double val);

/* ---- 重置 ---- */
void lv_config_reset(void);

/* ====================================================================
 * 兼容层 —— 旧宏名 → 现在全部走 lvConfig 或保留的编译期常量
 * ==================================================================== */

/* ---- 编译期保留的兼容 ---- */
#define lv_PUBLIC_API

/* ---- 运行时兼容（from config）---- */
/* 这些宏仍然有效，值固定；代码应逐步迁移到 cfg->field */

/* solver.h compat */
#ifndef SOLVER_MAX_VAR_ID
#define SOLVER_MAX_VAR_ID 100000
#endif

/* symbolic_coord.h compat */
#ifndef MAX_MODULE_DEPTH
#define MAX_MODULE_DEPTH 32
#endif
#ifndef BIT_CUTOFF_THRESHOLD
#define BIT_CUTOFF_THRESHOLD 1000000
#endif
#ifndef MAX_PRECISION_BITS
#define MAX_PRECISION_BITS 100
#endif

/* rewrite.h compat */
#ifndef WL_ITERATIONS
#define WL_ITERATIONS 3
#endif
#ifndef WL_HISTORY_SIZE
#define WL_HISTORY_HISTORY_SIZE 64
#endif

/* stream.h compat */
#ifndef STREAM_ASYNC_QUEUE_DEFAULT_CAPACITY
#define STREAM_ASYNC_QUEUE_DEFAULT_CAPACITY 1024
#endif
#ifndef STREAM_JSON_BUFFER_DEFAULT_SIZE
#define STREAM_JSON_BUFFER_DEFAULT_SIZE lv_CONFIG_STREAM_JSON_BUFFER_SIZE
#endif

/* parser compat */
#ifndef lv_MAX_AST_NODES
#define lv_MAX_AST_NODES 500000
#endif
#ifndef lv_MAX_TOKEN_LENGTH
#define lv_MAX_TOKEN_LENGTH 4096
#endif
#ifndef lv_MAX_INPUT_LENGTH
#define lv_MAX_INPUT_LENGTH 1048576
#endif
#ifndef lv_MAX_AST_DEPTH
#define lv_MAX_AST_DEPTH 256
#endif

/* lv_protocol.h 长度（编译期，struct 字段维度） */
#ifndef lv_PROTO_STR_LEN
#define lv_PROTO_STR_LEN lv_CONFIG_PROTO_STR_LEN
#endif
#ifndef lv_PROTO_LABEL_LEN
#define lv_PROTO_LABEL_LEN lv_CONFIG_PROTO_LABEL_LEN
#endif
#ifndef lv_PROTO_DESC_LEN
#define lv_PROTO_DESC_LEN lv_CONFIG_PROTO_DESC_LEN
#endif
#ifndef lv_PROTO_BUFFER_LEN
#define lv_PROTO_BUFFER_LEN lv_CONFIG_PROTO_BUFFER_LEN
#endif

/* lv_protocol.h 上限（运行时，兼容旧宏） */
#ifndef lv_PROTO_MAX_DRAW_CMDS
#define lv_PROTO_MAX_DRAW_CMDS    4096
#endif
#ifndef lv_PROTO_MAX_TABLE_ROWS
#define lv_PROTO_MAX_TABLE_ROWS    512
#endif
#ifndef lv_PROTO_MAX_TREE_NODES
#define lv_PROTO_MAX_TREE_NODES    256
#endif
#ifndef lv_PROTO_MAX_TOPOLOGY
#define lv_PROTO_MAX_TOPOLOGY      128
#endif
#ifndef lv_PROTO_MAX_PROOF_STEPS
#define lv_PROTO_MAX_PROOF_STEPS   512
#endif
#ifndef lv_PROTO_MAX_COMPLETIONS
#define lv_PROTO_MAX_COMPLETIONS    64
#endif
#ifndef lv_PROTO_MAX_TERMINAL_LINES
#define lv_PROTO_MAX_TERMINAL_LINES 512
#endif

/* interactive_geo.h compat */
#ifndef lv_GEO_MAX_OBJECTS
#define lv_GEO_MAX_OBJECTS 1024
#endif
#ifndef lv_GEO_MAX_CONSTRAINTS
#define lv_GEO_MAX_CONSTRAINTS 2048
#endif
#ifndef lv_GEO_MAX_DRAG_CHAIN
#define lv_GEO_MAX_DRAG_CHAIN 64
#endif
#ifndef lv_GEO_MAX_SNAPSHOTS
#define lv_GEO_MAX_SNAPSHOTS 32
#endif
#ifndef lv_GEO_SCRIPT_BUFFER_SIZE
#define lv_GEO_SCRIPT_BUFFER_SIZE lv_CONFIG_GEO_SCRIPT_BUFFER_SIZE
#endif
#ifndef lv_GEO_STATE_BUFFER_SIZE
#define lv_GEO_STATE_BUFFER_SIZE lv_CONFIG_GEO_STATE_BUFFER_SIZE
#endif
#ifndef lv_GEO_MIN_ZOOM
#define lv_GEO_MIN_ZOOM 0.01
#endif
#ifndef lv_GEO_MAX_ZOOM
#define lv_GEO_MAX_ZOOM 100.0
#endif

/* geom_evol.h compat */
#ifndef GEOEVOL_MAX_PARAM_DIM
#define GEOEVOL_MAX_PARAM_DIM 256
#endif
#ifndef GEOEVOL_ADAMS_MAX_ORDER
#define GEOEVOL_ADAMS_MAX_ORDER 12
#endif
#ifndef GEOEVOL_MIN_STEP
#define GEOEVOL_MIN_STEP 1e-15
#endif
#ifndef GEOEVOL_MAX_STEP
#define GEOEVOL_MAX_STEP 1e10
#endif
#ifndef GEOEVOL_PI_SMOOTH_FACTOR
#define GEOEVOL_PI_SMOOTH_FACTOR 0.25
#endif
#ifndef GEOEVOL_MAX_REJECTIONS
#define GEOEVOL_MAX_REJECTIONS 20
#endif

/* proof_engine_enhanced.h compat */
#ifndef lv_PROOF_MAX_DEPTH
#define lv_PROOF_MAX_DEPTH 100
#endif
#ifndef lv_PROOF_MAX_BRANCHES
#define lv_PROOF_MAX_BRANCHES 64
#endif
#ifndef lv_PROOF_MAX_STRATEGIES
#define lv_PROOF_MAX_STRATEGIES 16
#endif
#ifndef lv_TRACE_TREE_MAX_DEPTH
#define lv_TRACE_TREE_MAX_DEPTH 50
#endif

/* recursion.h compat */
#ifndef lv_MAX_RECURSION_DEPTH
#define lv_MAX_RECURSION_DEPTH 128
#endif

/* context.h compat */
#ifndef lv_CONTEXT_DEFAULT_MAX_DEPTH
#define lv_CONTEXT_DEFAULT_MAX_DEPTH 100
#endif
#ifndef lv_CONTEXT_MAX_RECURSION_DEPTH
#define lv_CONTEXT_MAX_RECURSION_DEPTH 10000
#endif
#ifndef lv_CONTEXT_DEFAULT_MAX_STEPS
#define lv_CONTEXT_DEFAULT_MAX_STEPS 1000000
#endif
#ifndef lv_CONTEXT_DEFAULT_MAX_CONSECUTIVE_ERRORS
#define lv_CONTEXT_DEFAULT_MAX_CONSECUTIVE_ERRORS 10
#endif
#ifndef lv_CONTEXT_REASONING_STACK_DEFAULT_CAPACITY
#define lv_CONTEXT_REASONING_STACK_DEFAULT_CAPACITY 8
#endif
#ifndef lv_CONTEXT_REASONING_STACK_MAX_DEPTH
#define lv_CONTEXT_REASONING_STACK_MAX_DEPTH 1000
#endif

/* runtime_guard.h compat */
#ifndef lv_RUNTIME_GUARD_MAX_RECURSE
#define lv_RUNTIME_GUARD_MAX_RECURSE 128
#endif

/* interop.h compat */
#ifndef INTEROP_CMD_BUFFER_SIZE
#define INTEROP_CMD_BUFFER_SIZE lv_CONFIG_INTEROP_CMD_BUFFER_SIZE
#endif
#ifndef INTEROP_RESP_BUFFER_SIZE
#define INTEROP_RESP_BUFFER_SIZE lv_CONFIG_INTEROP_RESP_BUFFER_SIZE
#endif
#ifndef INTEROP_MAX_PARAMS
#define INTEROP_MAX_PARAMS 32
#endif
#ifndef INTEROP_WS_DEFAULT_PORT
#define INTEROP_WS_DEFAULT_PORT 8765
#endif
#ifndef INTEROP_MAX_PATH_LEN
#define INTEROP_MAX_PATH_LEN lv_CONFIG_INTEROP_MAX_PATH_LEN
#endif
#ifndef INTEROP_MAX_COMPLETIONS
#define INTEROP_MAX_COMPLETIONS 64
#endif

/* debug.h compat */
#ifndef lv_LOG_MAX_FILES
#define lv_LOG_MAX_FILES 5
#endif
#ifndef lv_LOG_MAX_SIZE
#define lv_LOG_MAX_SIZE 10485760
#endif
#ifndef lv_LOG_PATH_MAX
#define lv_LOG_PATH_MAX lv_CONFIG_LOG_PATH_MAX
#endif
#ifndef lv_LOG_RING_BUFFER_DEFAULT_CAPACITY
#define lv_LOG_RING_BUFFER_DEFAULT_CAPACITY 256
#endif

/* runtime_monitor.h compat */
#ifndef lv_LOG_MSG_MAX_LEN
#define lv_LOG_MSG_MAX_LEN lv_CONFIG_LOG_MSG_MAX_LEN
#endif
#ifndef lv_LOG_TAG_MAX_LEN
#define lv_LOG_TAG_MAX_LEN lv_CONFIG_LOG_TAG_MAX_LEN
#endif
#ifndef lv_METRIC_NAME_MAX_LEN
#define lv_METRIC_NAME_MAX_LEN lv_CONFIG_METRIC_NAME_MAX_LEN
#endif
#ifndef lv_TIMER_MAX_DEPTH
#define lv_TIMER_MAX_DEPTH 32
#endif
#ifndef lv_PERF_SAMPLE_MAX_COUNT
#define lv_PERF_SAMPLE_MAX_COUNT 10000
#endif

/* memory_pool.h compat */
#ifndef lv_POOL_DEFAULT_CAPACITY
#define lv_POOL_DEFAULT_CAPACITY lv_CONFIG_POOL_DEFAULT_CAPACITY
#endif
#ifndef lv_LINEAR_ALLOCATOR_BLOCK_SIZE
#define lv_LINEAR_ALLOCATOR_BLOCK_SIZE lv_CONFIG_LINEAR_ALLOCATOR_BLOCK_SIZE
#endif
#ifndef lv_LRU_CACHE_DEFAULT_CAPACITY
#define lv_LRU_CACHE_DEFAULT_CAPACITY lv_CONFIG_LRU_CACHE_DEFAULT_CAPACITY
#endif
#ifndef lv_MEM_STAT_MAX_TYPES
#define lv_MEM_STAT_MAX_TYPES lv_CONFIG_MEM_STAT_MAX_TYPES
#endif

/* test_framework.h compat */
#ifndef lv_TEST_MAX_SUITES
#define lv_TEST_MAX_SUITES 256
#endif
#ifndef lv_TEST_MAX_CASES
#define lv_TEST_MAX_CASES 4096
#endif
#ifndef lv_TEST_NAME_MAX_LEN
#define lv_TEST_NAME_MAX_LEN lv_CONFIG_TEST_NAME_MAX_LEN
#endif
#ifndef lv_TEST_MSG_MAX_LEN
#define lv_TEST_MSG_MAX_LEN lv_CONFIG_TEST_MSG_MAX_LEN
#endif

/* numerical_backend.h compat */
#ifndef lv_BACKEND_NAME_MAX
#define lv_BACKEND_NAME_MAX lv_CONFIG_BACKEND_NAME_MAX
#endif

/* ---- 通用 ---- */
#ifndef lv_DEFAULT_REWRITE_STEP_LIMIT
#define lv_DEFAULT_REWRITE_STEP_LIMIT 1000
#endif
#ifndef lv_DEFAULT_MAX_ITERATIONS
#define lv_DEFAULT_MAX_ITERATIONS 10000
#endif
#ifndef lv_DEFAULT_PRECISION_BITS
#define lv_DEFAULT_PRECISION_BITS 52
#endif
#ifndef lv_DEFAULT_MEMORY_LIMIT_MB
#define lv_DEFAULT_MEMORY_LIMIT_MB 0
#endif

/* 内存池短别名 */
#ifndef lv_CONSTRAINT_NODE_SIZE
#define lv_CONSTRAINT_NODE_SIZE lv_CONFIG_POOL_CONSTRAINT_NODE_SIZE
#endif
#ifndef lv_SYMBOLIC_COORD_SIZE
#define lv_SYMBOLIC_COORD_SIZE lv_CONFIG_POOL_SYMBOLIC_COORD_SIZE
#endif
#ifndef lv_PROOF_STEP_SIZE
#define lv_PROOF_STEP_SIZE lv_CONFIG_POOL_PROOF_STEP_SIZE
#endif
#ifndef lv_CONSTRAINT_SIZE
#define lv_CONSTRAINT_SIZE lv_CONFIG_POOL_CONSTRAINT_SIZE
#endif

/* 内部常量 */
#ifndef lv_ARRAY_GROWTH_FACTOR
#define lv_ARRAY_GROWTH_FACTOR 2
#endif
#ifndef lv_FNV_HASH_MULTIPLIER
#define lv_FNV_HASH_MULTIPLIER 0x01000193U
#endif
#ifndef lv_NODE_INDEX_INITIAL_SIZE
#define lv_NODE_INDEX_INITIAL_SIZE 16
#endif
#ifndef lv_CONSTRAINT_INDEX_INITIAL_SIZE
#define lv_CONSTRAINT_INDEX_INITIAL_SIZE 16
#endif
#ifndef lv_INDEX_LOAD_FACTOR
#define lv_INDEX_LOAD_FACTOR 0.75
#endif
#ifndef lv_INITIAL_ARRAY_CAPACITY
#define lv_INITIAL_ARRAY_CAPACITY 8
#endif

/* 根/位/降级 */
#ifndef lv_MAX_PRECISION_BITS
#define lv_MAX_PRECISION_BITS 100
#endif
#ifndef lv_BIT_CUTOFF_THRESHOLD
#define lv_BIT_CUTOFF_THRESHOLD 1000000
#endif
#ifndef lv_CONTINUED_FRACTION_MAX_ITER
#define lv_CONTINUED_FRACTION_MAX_ITER 1000
#endif
#ifndef lv_MAX_SUBINTERVALS
#define lv_MAX_SUBINTERVALS 4096
#endif
#ifndef lv_ROOT_EPSILON
#define lv_ROOT_EPSILON lv_CONFIG_ROOT_EPSILON
#endif
#ifndef lv_CIRCUIT_OVERFLOW_THRESHOLD
#define lv_CIRCUIT_OVERFLOW_THRESHOLD 3
#endif
#ifndef lv_VALUE_TOO_LARGE
#define lv_VALUE_TOO_LARGE 1048576
#endif
#ifndef lv_DOWNGRADE_DENOMINATOR
#define lv_DOWNGRADE_DENOMINATOR 100000
#endif

/* 健康检查 */
#ifndef lv_HEALTH_SCORE_MAX
#define lv_HEALTH_SCORE_MAX 100
#endif
#ifndef lv_HEALTH_MEMORY_USAGE_RATIO
#define lv_HEALTH_MEMORY_USAGE_RATIO 0.8
#endif
#ifndef lv_HEALTH_MEMORY_WARNING_PENALTY
#define lv_HEALTH_MEMORY_WARNING_PENALTY 10
#endif
#ifndef lv_HEALTH_MEMORY_LEAK_RATIO
#define lv_HEALTH_MEMORY_LEAK_RATIO 0.9
#endif
#ifndef lv_HEALTH_MEMORY_LEAK_PENALTY
#define lv_HEALTH_MEMORY_LEAK_PENALTY 20
#endif
#ifndef lv_HEALTH_RECENT_ERROR_PENALTY
#define lv_HEALTH_RECENT_ERROR_PENALTY 5
#endif

/* localtime */
#ifndef lv_LOCALTIME
#ifdef _WIN32
#define lv_LOCALTIME(num, p) localtime_s(p, num)
#else
#define lv_LOCALTIME(num, p) localtime_r(num, p)
#endif
#endif

/* 路径分隔符 */
#ifndef lv_PATH_SEPARATOR
#ifdef _WIN32
#define lv_PATH_SEPARATOR '\\'
#else
#define lv_PATH_SEPARATOR '/'
#endif
#endif

#ifdef __cplusplus
}
#endif
#endif /* lv_CONFIG_H */
