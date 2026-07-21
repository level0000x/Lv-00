/**
 * @file config.h
 * @brief Lv-00 集中化配置
 *
 * 编译期常量（仅 ~30 个，影响 sizeof / 数组维度）→ LV00_CONFIG_* 宏
 * 运行时配置（~80 个限制）→ Lv00Config 结构体 + lv00_config_current()
 *
 * 用法:
 *   // 不重编译就能调上限
 *   const Lv00Config *cfg = lv00_config_current();
 *   if (step_count > cfg->proto_max_proof_steps) break;
 *
 *   // JSON 文件改配置，重启即生效
 *   lv00_config_load_json("lv00.config.json"); lv00_init();
 *
 * @version 1.1.0
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
 * 编译期常量 —— 仅影响 sizeof / 栈数组 / #if 判断
 * ==================================================================== */

/* ---- 对象池块尺寸（影响结构体 sizeof） ---- */
#define LV00_CONFIG_POOL_CONSTRAINT_NODE_SIZE     128
#define LV00_CONFIG_POOL_CONSTRAINT_SIZE           96
#define LV00_CONFIG_POOL_SYMBOLIC_COORD_SIZE       64
#define LV00_CONFIG_POOL_PROOF_STEP_SIZE          128

/* ---- 内存池容量 ---- */
#define LV00_CONFIG_POOL_DEFAULT_CAPACITY        1024
#define LV00_CONFIG_LINEAR_ALLOCATOR_BLOCK_SIZE 65536
#define LV00_CONFIG_LRU_CACHE_DEFAULT_CAPACITY    256
#define LV00_CONFIG_MEM_STAT_MAX_TYPES             64

/* ---- 数据结构 ---- */
#define LV00_CONFIG_INITIAL_ARRAY_CAPACITY          8
#define LV00_CONFIG_INITIAL_HASH_INDEX_CAPACITY    64
#define LV00_CONFIG_ARRAY_GROWTH_FACTOR             2
#define LV00_CONFIG_FNV_HASH_MULTIPLIER    0x01000193U
#define LV00_CONFIG_NODE_INDEX_INITIAL_SIZE        16
#define LV00_CONFIG_CONSTRAINT_INDEX_INITIAL_SIZE  16
#define LV00_CONFIG_INDEX_LOAD_FACTOR            0.75
#define LV00_FNV64_OFFSET_BASIS          14695981039346656037ULL
#define LV00_FNV64_PRIME                  1099511628211ULL
#define LV00_ARRAY_COUNT(arr) ((arr) ? sizeof(arr) / sizeof((arr)[0]) : 0)

/* ---- 缓冲区 / 字符串长度（影响 char buf[N] 声明） ---- */
#define LV00_CONFIG_GRAPH_ERROR_BUFFER_SIZE        256
#define LV00_CONFIG_GRAPH_SERIALIZE_BUFFER_SIZE    256
#define LV00_CONFIG_ENGINE_ERROR_BUFFER_SIZE       256
#define LV00_CONFIG_PARSER_MAX_BUFFER_SIZE         256
#define LV00_CONFIG_PARSER_MAX_TEMP_MSG_SIZE       128

#define LV00_CONFIG_INTEROP_CMD_BUFFER_SIZE       4096
#define LV00_CONFIG_INTEROP_RESP_BUFFER_SIZE     65536
#define LV00_CONFIG_INTEROP_MAX_PATH_LEN           512
#define LV00_CONFIG_LOG_PATH_MAX                  256

#define LV00_CONFIG_STREAM_JSON_BUFFER_SIZE       4096

#define LV00_CONFIG_PROTO_STR_LEN                  64
#define LV00_CONFIG_PROTO_LABEL_LEN               128
#define LV00_CONFIG_PROTO_DESC_LEN                256
#define LV00_CONFIG_PROTO_BUFFER_LEN             4096

#define LV00_CONFIG_GEO_SCRIPT_BUFFER_SIZE       65536
#define LV00_CONFIG_GEO_STATE_BUFFER_SIZE       131072

#define LV00_CONFIG_LOG_MSG_MAX_LEN              4096
#define LV00_CONFIG_LOG_TAG_MAX_LEN                64
#define LV00_CONFIG_METRIC_NAME_MAX_LEN           128

#define LV00_CONFIG_PLUGIN_NAME_MAX                64
#define LV00_CONFIG_PLUGIN_DESC_MAX               256
#define LV00_CONFIG_PLUGIN_AUTHOR_MAX             128
#define LV00_CONFIG_PLUGIN_PATH_MAX               512

#define LV00_CONFIG_BACKEND_NAME_MAX               64

#define LV00_CONFIG_TEST_NAME_MAX_LEN             256
#define LV00_CONFIG_TEST_MSG_MAX_LEN              512

#define LV00_CONFIG_MAX_LABEL_LENGTH              256
#define LV00_CONFIG_MAX_FORMULA_LENGTH           1024
#define LV00_CONFIG_MAX_VARIABLE_NAME_LENGTH      128
#define LV00_CONFIG_MAX_REPLACEMENT_TERM_LENGTH   512
#define LV00_CONFIG_MAX_PROOF_REFS                 64

/* ---- 数值常量（compile-time 表达式中使用） ---- */
#define LV00_CONFIG_DEFAULT_PRECISION_BITS         52
#define LV00_CONFIG_ROOT_EPSILON                 1e-12

/* ---- 数值精度常量（浮点数比较容差） ---- */
/* 通用高精度容差 */
#ifndef LV00_EPSILON_SUPERTINY
#define LV00_EPSILON_SUPERTINY 1e-15   /* 极高精度：用于Adams/BDF步长、Groebner基 */
#endif
#ifndef LV00_EPSILON_ULTRA
#define LV00_EPSILON_ULTRA     1e-12   /* 超高精度：用于根求解、收敛判断 */
#endif
#ifndef LV00_EPSILON_HIGH
#define LV00_EPSILON_HIGH      1e-10   /* 高精度：用于角度/距离判断 */
#endif
#ifndef LV00_EPSILON_MEDIUM
#define LV00_EPSILON_MEDIUM    1e-9    /* 中等精度：用于几何谓词（orientation） */
#endif
#ifndef LV00_EPSILON_LOW
#define LV00_EPSILON_LOW       1e-6    /* 低精度：用于近似计算 */
#endif

/* 几何专用容差 */
#ifndef LV00_GEO_COLLINEAR_EPSILON
#define LV00_GEO_COLLINEAR_EPSILON 1e-9   /* 共线性判断容差 */
#endif
#ifndef LV00_GEO_DISTANCE_EPSILON
#define LV00_GEO_DISTANCE_EPSILON 1e-8    /* 距离判断容差 */
#endif
#ifndef LV00_GEO_ANGLE_EPSILON
#define LV00_GEO_ANGLE_EPSILON 1e-10      /* 角度相等容差 */
#endif

/* 代数运算安全阈值 */
#ifndef LV00_SINGULARITY_THRESHOLD
#define LV00_SINGULARITY_THRESHOLD 1e-12   /* 矩阵奇异性判断 */
#endif
#ifndef LV00_NORMALIZATION_THRESHOLD
#define LV00_NORMALIZATION_THRESHOLD 1e-15  /* 向量归一化容差 */
#endif

/* 数值范围极限（用于哨兵值） */
#ifndef LV00_INFINITY_SENTINEL
#define LV00_INFINITY_SENTINEL 1e308      /* "无穷大"哨兵 */
#endif
#ifndef LV00_TINY_SENTINEL
#define LV00_TINY_SENTINEL  1e-300        /* "接近零"哨兵 */
#endif
#ifndef LV00_HUGE_NUMBER
#define LV00_HUGE_NUMBER     1e30        /* 极大数阈值 */
#endif
#ifndef LV00_LARGE_NUMBER
#define LV00_LARGE_NUMBER    1e18        /* 大数阈值 */
#endif
#ifndef LV00_BIG_NUMBER
#define LV00_BIG_NUMBER      1e9         /* 10亿级阈值 */
#endif

/* ---- 常用数学系数与比例因子 ---- */
/* 缩放系数 */
#ifndef LV00_HALF
#define LV00_HALF         0.5         /* 1/2 */
#endif
#ifndef LV00_THIRD
#define LV00_THIRD        0.333333333333333333333   /* 1/3 */
#endif
#ifndef LV00_QUARTER
#define LV00_QUARTER      0.25        /* 1/4 */
#endif
#ifndef LV00_TENTH
#define LV00_TENTH        0.1         /* 1/10 */
#endif

/* 角度系数（弧度↔度） */
#ifndef LV00_DEG_TO_RAD
#define LV00_DEG_TO_RAD   0.017453292519943295769   /* π/180 */
#endif
#ifndef LV00_RAD_TO_DEG
#define LV00_RAD_TO_DEG   57.2957795130823208768    /* 180/π */
#endif
#ifndef LV00_PI
#define LV00_PI           3.1415926535897932384626  /* π */
#endif
#ifndef LV00_TWO_PI
#define LV00_TWO_PI       6.28318530717958647692   /* 2π */
#endif
#ifndef LV00_HALF_PI
#define LV00_HALF_PI      1.57079632679489661923   /* π/2 */
#endif
#ifndef LV00_QUARTER_PI
#define LV00_QUARTER_PI   0.78539816339744830962   /* π/4 */
#endif

/* 颜色通道（RGB 0-255 归一化） */
#ifndef LV00_COLOR_CHANNEL_MAX
#define LV00_COLOR_CHANNEL_MAX 255.0f   /* RGB最大通道值 */
#endif
#ifndef LV00_COLOR_CHANNEL_HALF
#define LV00_COLOR_CHANNEL_HALF 0.5f    /* 0.5f */
#endif

/* 哈希表负载因子 */
#ifndef LV00_HASH_LOAD_FACTOR_MAX
#define LV00_HASH_LOAD_FACTOR_MAX 0.75  /* 最大负载因子 */
#endif
#ifndef LV00_HASH_LOAD_FACTOR_MIN
#define LV00_HASH_LOAD_FACTOR_MIN 0.25  /* 最小负载因子 */
#endif

/* 缓冲区/块大小（2的幂次） */
#ifndef LV00_KB
#define LV00_KB           1024.0       /* 1KB */
#endif
#ifndef LV00_MB
#define LV00_MB           1048576.0    /* 1MB */
#endif
#ifndef LV00_GB
#define LV00_GB           1073741824.0 /* 1GB */
#endif
#ifndef LV00_KB_I
#define LV00_KB_I         1024         /* 1KB (int) */
#endif
#ifndef LV00_MB_I
#define LV00_MB_I         1048576      /* 1MB (int) */
#endif

/* == 以下全部移入 Lv00Config 运行时 == */

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
} Lv00Config;

/* ====================================================================
 * 运行时配置 API
 * ==================================================================== */

const Lv00Config *lv00_config_default(void);
const Lv00Config *lv00_config_current(void);
int               lv00_config_apply(const Lv00Config *cfg);
int               lv00_config_load_json(const char *json_path);
int               lv00_config_to_json(char *buf, size_t buf_size);

/* ====================================================================
 * 运行时单字段修改 API（不重编译，立即生效）
 * ==================================================================== */

/* ---- 类型安全 setter（高频字段，IDE 自动补全） ---- */
void lv00_config_set_solver_max_var_id(int val);
void lv00_config_set_solver_max_iterations(int val);
void lv00_config_set_proof_max_depth(int val);
void lv00_config_set_proof_max_branches(int val);
void lv00_config_set_proto_max_draw_cmds(int val);
void lv00_config_set_proto_max_proof_steps(int val);
void lv00_config_set_proto_max_terminal_lines(int val);
void lv00_config_set_geo_max_objects(int val);
void lv00_config_set_geo_max_constraints(int val);
void lv00_config_set_geo_min_zoom(double val);
void lv00_config_set_geo_max_zoom(double val);
void lv00_config_set_parser_max_input_length(int val);
void lv00_config_set_parser_max_ast_nodes(int val);
void lv00_config_set_max_recursion_depth(int val);
void lv00_config_set_default_rewrite_limit(int val);
void lv00_config_set_geoevol_max_param_dim(int val);
void lv00_config_set_geoevol_max_rejections(int val);
void lv00_config_set_stream_max_callbacks(int val);
void lv00_config_set_max_plugins(int val);

/* ---- 通用 key-value setter（低频字段用，一次调用不改编译） ---- */
bool lv00_config_set_int(const char *key, int val);
bool lv00_config_set_double(const char *key, double val);

/* ---- 重置 ---- */
void lv00_config_reset(void);

/* ====================================================================
 * 兼容层 —— 旧宏名 → 现在全部走 Lv00Config 或保留的编译期常量
 * ==================================================================== */

/* ---- 编译期保留的兼容 ---- */
#define LV00_PUBLIC_API

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
#define STREAM_JSON_BUFFER_DEFAULT_SIZE LV00_CONFIG_STREAM_JSON_BUFFER_SIZE
#endif

/* parser compat */
#ifndef LV00_MAX_AST_NODES
#define LV00_MAX_AST_NODES 500000
#endif
#ifndef LV00_MAX_TOKEN_LENGTH
#define LV00_MAX_TOKEN_LENGTH 4096
#endif
#ifndef LV00_MAX_INPUT_LENGTH
#define LV00_MAX_INPUT_LENGTH 1048576
#endif
#ifndef LV00_MAX_AST_DEPTH
#define LV00_MAX_AST_DEPTH 256
#endif

/* lv00_protocol.h 长度（编译期，struct 字段维度） */
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

/* lv00_protocol.h 上限（运行时，兼容旧宏） */
#ifndef LV00_PROTO_MAX_DRAW_CMDS
#define LV00_PROTO_MAX_DRAW_CMDS    4096
#endif
#ifndef LV00_PROTO_MAX_TABLE_ROWS
#define LV00_PROTO_MAX_TABLE_ROWS    512
#endif
#ifndef LV00_PROTO_MAX_TREE_NODES
#define LV00_PROTO_MAX_TREE_NODES    256
#endif
#ifndef LV00_PROTO_MAX_TOPOLOGY
#define LV00_PROTO_MAX_TOPOLOGY      128
#endif
#ifndef LV00_PROTO_MAX_PROOF_STEPS
#define LV00_PROTO_MAX_PROOF_STEPS   512
#endif
#ifndef LV00_PROTO_MAX_COMPLETIONS
#define LV00_PROTO_MAX_COMPLETIONS    64
#endif
#ifndef LV00_PROTO_MAX_TERMINAL_LINES
#define LV00_PROTO_MAX_TERMINAL_LINES 512
#endif

/* interactive_geo.h compat */
#ifndef LV00_GEO_MAX_OBJECTS
#define LV00_GEO_MAX_OBJECTS 1024
#endif
#ifndef LV00_GEO_MAX_CONSTRAINTS
#define LV00_GEO_MAX_CONSTRAINTS 2048
#endif
#ifndef LV00_GEO_MAX_DRAG_CHAIN
#define LV00_GEO_MAX_DRAG_CHAIN 64
#endif
#ifndef LV00_GEO_MAX_SNAPSHOTS
#define LV00_GEO_MAX_SNAPSHOTS 32
#endif
#ifndef LV00_GEO_SCRIPT_BUFFER_SIZE
#define LV00_GEO_SCRIPT_BUFFER_SIZE LV00_CONFIG_GEO_SCRIPT_BUFFER_SIZE
#endif
#ifndef LV00_GEO_STATE_BUFFER_SIZE
#define LV00_GEO_STATE_BUFFER_SIZE LV00_CONFIG_GEO_STATE_BUFFER_SIZE
#endif
#ifndef LV00_GEO_MIN_ZOOM
#define LV00_GEO_MIN_ZOOM 0.01
#endif
#ifndef LV00_GEO_MAX_ZOOM
#define LV00_GEO_MAX_ZOOM 100.0
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
#ifndef LV00_PROOF_MAX_DEPTH
#define LV00_PROOF_MAX_DEPTH 100
#endif
#ifndef LV00_PROOF_MAX_BRANCHES
#define LV00_PROOF_MAX_BRANCHES 64
#endif
#ifndef LV00_PROOF_MAX_STRATEGIES
#define LV00_PROOF_MAX_STRATEGIES 16
#endif
#ifndef LV00_TRACE_TREE_MAX_DEPTH
#define LV00_TRACE_TREE_MAX_DEPTH 50
#endif

/* recursion.h compat */
#ifndef LV00_MAX_RECURSION_DEPTH
#define LV00_MAX_RECURSION_DEPTH 128
#endif

/* context.h compat */
#ifndef LV00_CONTEXT_DEFAULT_MAX_DEPTH
#define LV00_CONTEXT_DEFAULT_MAX_DEPTH 100
#endif
#ifndef LV00_CONTEXT_MAX_RECURSION_DEPTH
#define LV00_CONTEXT_MAX_RECURSION_DEPTH 10000
#endif
#ifndef LV00_CONTEXT_DEFAULT_MAX_STEPS
#define LV00_CONTEXT_DEFAULT_MAX_STEPS 1000000
#endif
#ifndef LV00_CONTEXT_DEFAULT_MAX_CONSECUTIVE_ERRORS
#define LV00_CONTEXT_DEFAULT_MAX_CONSECUTIVE_ERRORS 10
#endif
#ifndef LV00_CONTEXT_REASONING_STACK_DEFAULT_CAPACITY
#define LV00_CONTEXT_REASONING_STACK_DEFAULT_CAPACITY 8
#endif
#ifndef LV00_CONTEXT_REASONING_STACK_MAX_DEPTH
#define LV00_CONTEXT_REASONING_STACK_MAX_DEPTH 1000
#endif

/* runtime_guard.h compat */
#ifndef LV00_RUNTIME_GUARD_MAX_RECURSE
#define LV00_RUNTIME_GUARD_MAX_RECURSE 128
#endif

/* interop.h compat */
#ifndef INTEROP_CMD_BUFFER_SIZE
#define INTEROP_CMD_BUFFER_SIZE LV00_CONFIG_INTEROP_CMD_BUFFER_SIZE
#endif
#ifndef INTEROP_RESP_BUFFER_SIZE
#define INTEROP_RESP_BUFFER_SIZE LV00_CONFIG_INTEROP_RESP_BUFFER_SIZE
#endif
#ifndef INTEROP_MAX_PARAMS
#define INTEROP_MAX_PARAMS 32
#endif
#ifndef INTEROP_WS_DEFAULT_PORT
#define INTEROP_WS_DEFAULT_PORT 8765
#endif
#ifndef INTEROP_MAX_PATH_LEN
#define INTEROP_MAX_PATH_LEN LV00_CONFIG_INTEROP_MAX_PATH_LEN
#endif
#ifndef INTEROP_MAX_COMPLETIONS
#define INTEROP_MAX_COMPLETIONS 64
#endif

/* debug.h compat */
#ifndef LV00_LOG_MAX_FILES
#define LV00_LOG_MAX_FILES 5
#endif
#ifndef LV00_LOG_MAX_SIZE
#define LV00_LOG_MAX_SIZE 10485760
#endif
#ifndef LV00_LOG_PATH_MAX
#define LV00_LOG_PATH_MAX LV00_CONFIG_LOG_PATH_MAX
#endif
#ifndef LV00_LOG_RING_BUFFER_DEFAULT_CAPACITY
#define LV00_LOG_RING_BUFFER_DEFAULT_CAPACITY 256
#endif

/* runtime_monitor.h compat */
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
#define LV00_TIMER_MAX_DEPTH 32
#endif
#ifndef LV00_PERF_SAMPLE_MAX_COUNT
#define LV00_PERF_SAMPLE_MAX_COUNT 10000
#endif

/* memory_pool.h compat */
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

/* test_framework.h compat */
#ifndef LV00_TEST_MAX_SUITES
#define LV00_TEST_MAX_SUITES 256
#endif
#ifndef LV00_TEST_MAX_CASES
#define LV00_TEST_MAX_CASES 4096
#endif
#ifndef LV00_TEST_NAME_MAX_LEN
#define LV00_TEST_NAME_MAX_LEN LV00_CONFIG_TEST_NAME_MAX_LEN
#endif
#ifndef LV00_TEST_MSG_MAX_LEN
#define LV00_TEST_MSG_MAX_LEN LV00_CONFIG_TEST_MSG_MAX_LEN
#endif

/* numerical_backend.h compat */
#ifndef LV00_BACKEND_NAME_MAX
#define LV00_BACKEND_NAME_MAX LV00_CONFIG_BACKEND_NAME_MAX
#endif

/* ---- 通用 ---- */
#ifndef LV00_DEFAULT_REWRITE_STEP_LIMIT
#define LV00_DEFAULT_REWRITE_STEP_LIMIT 1000
#endif
#ifndef LV00_DEFAULT_MAX_ITERATIONS
#define LV00_DEFAULT_MAX_ITERATIONS 10000
#endif
#ifndef LV00_DEFAULT_PRECISION_BITS
#define LV00_DEFAULT_PRECISION_BITS 52
#endif
#ifndef LV00_DEFAULT_MEMORY_LIMIT_MB
#define LV00_DEFAULT_MEMORY_LIMIT_MB 0
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
#define LV00_ARRAY_GROWTH_FACTOR 2
#endif
#ifndef LV00_FNV_HASH_MULTIPLIER
#define LV00_FNV_HASH_MULTIPLIER 0x01000193U
#endif
#ifndef LV00_NODE_INDEX_INITIAL_SIZE
#define LV00_NODE_INDEX_INITIAL_SIZE 16
#endif
#ifndef LV00_CONSTRAINT_INDEX_INITIAL_SIZE
#define LV00_CONSTRAINT_INDEX_INITIAL_SIZE 16
#endif
#ifndef LV00_INDEX_LOAD_FACTOR
#define LV00_INDEX_LOAD_FACTOR 0.75
#endif
#ifndef LV00_INITIAL_ARRAY_CAPACITY
#define LV00_INITIAL_ARRAY_CAPACITY 8
#endif

/* 根/位/降级 */
#ifndef LV00_MAX_PRECISION_BITS
#define LV00_MAX_PRECISION_BITS 100
#endif
#ifndef LV00_BIT_CUTOFF_THRESHOLD
#define LV00_BIT_CUTOFF_THRESHOLD 1000000
#endif
#ifndef LV00_CONTINUED_FRACTION_MAX_ITER
#define LV00_CONTINUED_FRACTION_MAX_ITER 1000
#endif
#ifndef LV00_MAX_SUBINTERVALS
#define LV00_MAX_SUBINTERVALS 4096
#endif
#ifndef LV00_ROOT_EPSILON
#define LV00_ROOT_EPSILON LV00_CONFIG_ROOT_EPSILON
#endif
#ifndef LV00_CIRCUIT_OVERFLOW_THRESHOLD
#define LV00_CIRCUIT_OVERFLOW_THRESHOLD 3
#endif
#ifndef LV00_VALUE_TOO_LARGE
#define LV00_VALUE_TOO_LARGE 1048576
#endif
#ifndef LV00_DOWNGRADE_DENOMINATOR
#define LV00_DOWNGRADE_DENOMINATOR 100000
#endif

/* 健康检查 */
#ifndef LV00_HEALTH_SCORE_MAX
#define LV00_HEALTH_SCORE_MAX 100
#endif
#ifndef LV00_HEALTH_MEMORY_USAGE_RATIO
#define LV00_HEALTH_MEMORY_USAGE_RATIO 0.8
#endif
#ifndef LV00_HEALTH_MEMORY_WARNING_PENALTY
#define LV00_HEALTH_MEMORY_WARNING_PENALTY 10
#endif
#ifndef LV00_HEALTH_MEMORY_LEAK_RATIO
#define LV00_HEALTH_MEMORY_LEAK_RATIO 0.9
#endif
#ifndef LV00_HEALTH_MEMORY_LEAK_PENALTY
#define LV00_HEALTH_MEMORY_LEAK_PENALTY 20
#endif
#ifndef LV00_HEALTH_RECENT_ERROR_PENALTY
#define LV00_HEALTH_RECENT_ERROR_PENALTY 5
#endif

/* localtime */
#ifndef LV00_LOCALTIME
#ifdef _WIN32
#define LV00_LOCALTIME(num, p) localtime_s(p, num)
#else
#define LV00_LOCALTIME(num, p) localtime_r(num, p)
#endif
#endif

/* 路径分隔符 */
#ifndef LV00_PATH_SEPARATOR
#ifdef _WIN32
#define LV00_PATH_SEPARATOR '\\'
#else
#define LV00_PATH_SEPARATOR '/'
#endif
#endif

#ifdef __cplusplus
}
#endif
#endif /* LV00_CONFIG_H */
