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
#define lv_CONFIG_POOL_CONSTRAINT_NODE_SIZE 128
#define lv_CONFIG_POOL_CONSTRAINT_SIZE 96
#define lv_CONFIG_POOL_SYMBOLIC_COORD_SIZE 64
#define lv_CONFIG_POOL_PROOF_STEP_SIZE 128

/* ---- 内存池容量 ---- */
#define lv_CONFIG_POOL_DEFAULT_CAPACITY 1024
#define lv_CONFIG_LINEAR_ALLOCATOR_BLOCK_SIZE 65536
#define lv_CONFIG_LRU_CACHE_DEFAULT_CAPACITY 256
#define lv_CONFIG_MEM_STAT_MAX_TYPES 64

/* ---- 数据结构 ---- */
#define lv_CONFIG_INITIAL_ARRAY_CAPACITY 8
#define lv_CONFIG_INITIAL_HASH_INDEX_CAPACITY 64
#define lv_CONFIG_ARRAY_GROWTH_FACTOR 2
#define lv_CONFIG_FNV_HASH_MULTIPLIER 0x01000193U
#define lv_CONFIG_NODE_INDEX_INITIAL_SIZE 16
#define lv_CONFIG_CONSTRAINT_INDEX_INITIAL_SIZE 16
#define lv_CONFIG_INDEX_LOAD_FACTOR 0.75
#define lv_FNV64_OFFSET_BASIS 14695981039346656037ULL
#define lv_FNV64_PRIME 1099511628211ULL
#define lv_ARRAY_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))

/* ---- 缓冲区 / 字符串长度（影响 char buf[N] 声明） ---- */
#define lv_CONFIG_GRAPH_ERROR_BUFFER_SIZE 256
#define lv_CONFIG_GRAPH_SERIALIZE_BUFFER_SIZE 256
#define lv_CONFIG_ENGINE_ERROR_BUFFER_SIZE 256
#define lv_CONFIG_PARSER_MAX_BUFFER_SIZE 256
#define lv_CONFIG_PARSER_MAX_TEMP_MSG_SIZE 128

#define lv_CONFIG_INTEROP_CMD_BUFFER_SIZE 4096
#define lv_CONFIG_INTEROP_RESP_BUFFER_SIZE 65536
#define lv_CONFIG_INTEROP_MAX_PATH_LEN 512
#define lv_CONFIG_LOG_PATH_MAX 256

#define lv_CONFIG_STREAM_JSON_BUFFER_SIZE 4096

#define lv_CONFIG_PROTO_STR_LEN 64
#define lv_CONFIG_PROTO_LABEL_LEN 128
#define lv_CONFIG_PROTO_DESC_LEN 256
#define lv_CONFIG_PROTO_BUFFER_LEN 4096

#define lv_CONFIG_GEO_SCRIPT_BUFFER_SIZE 65536
#define lv_CONFIG_GEO_STATE_BUFFER_SIZE 131072

#define lv_CONFIG_LOG_MSG_MAX_LEN 4096
#define lv_CONFIG_LOG_TAG_MAX_LEN 64
#define lv_CONFIG_METRIC_NAME_MAX_LEN 128

#define lv_CONFIG_PLUGIN_NAME_MAX 64
#define lv_CONFIG_PLUGIN_DESC_MAX 256
#define lv_CONFIG_PLUGIN_AUTHOR_MAX 128
#define lv_CONFIG_PLUGIN_PATH_MAX 512

#define lv_CONFIG_BACKEND_NAME_MAX 64

#define lv_CONFIG_TEST_NAME_MAX_LEN 256
#define lv_CONFIG_TEST_MSG_MAX_LEN 512

#define lv_CONFIG_MAX_LABEL_LENGTH 256
#define lv_CONFIG_MAX_FORMULA_LENGTH 1024
#define lv_CONFIG_MAX_VARIABLE_NAME_LENGTH 128
#define lv_CONFIG_MAX_REPLACEMENT_TERM_LENGTH 512
#define lv_CONFIG_MAX_PROOF_REFS 64

/* ---- 数值常量（compile-time 表达式中使用） ---- */
#define lv_CONFIG_DEFAULT_PRECISION_BITS 52
#define lv_CONFIG_ROOT_EPSILON 1e-12

/* ---- 有理数缩放因子 ---- */
/** @brief 默认有理数缩放因子（1000000 = 6位小数精度） */
#ifndef lv_RATIONAL_SCALE_DEFAULT
#define lv_RATIONAL_SCALE_DEFAULT 1000000
#endif

/** @brief 低精度有理数缩放因子（1000 = 3位小数精度） */
#ifndef lv_RATIONAL_SCALE_LOW
#define lv_RATIONAL_SCALE_LOW 1000
#endif

/** @brief 浮点近似缩放默认精度 */
#ifndef lv_FLOAT_APPROX_SCALE
#define lv_FLOAT_APPROX_SCALE 1000
#endif

/* ---- 数值精度常量（浮点数比较容差） ---- */
/* 通用高精度容差 */
#ifndef lv_EPSILON_SUPERTINY
#define lv_EPSILON_SUPERTINY 1e-15 /* 极高精度：用于Adams/BDF步长、Groebner基 */
#endif
#ifndef lv_EPSILON_ULTRA
#define lv_EPSILON_ULTRA 1e-12 /* 超高精度：用于根求解、收敛判断 */
#endif
#ifndef lv_EPSILON_HIGH
#define lv_EPSILON_HIGH 1e-10 /* 高精度：用于角度/距离判断 */
#endif
#ifndef lv_EPSILON_MEDIUM
#define lv_EPSILON_MEDIUM 1e-9 /* 中等精度：用于几何谓词（orientation） */
#endif
#ifndef lv_EPSILON_LOW
#define lv_EPSILON_LOW 1e-6 /* 低精度：用于近似计算 */
#endif

/* 几何专用容差 */
#ifndef lv_GEO_COLLINEAR_EPSILON
#define lv_GEO_COLLINEAR_EPSILON 1e-9 /* 共线性判断容差 */
#endif
#ifndef lv_GEO_DISTANCE_EPSILON
#define lv_GEO_DISTANCE_EPSILON 1e-8 /* 距离判断容差 */
#endif
#ifndef lv_GEO_ANGLE_EPSILON
#define lv_GEO_ANGLE_EPSILON 1e-10 /* 角度相等容差 */
#endif

/* 代数运算安全阈值 */
#ifndef lv_SINGULARITY_THRESHOLD
#define lv_SINGULARITY_THRESHOLD 1e-12 /* 矩阵奇异性判断 */
#endif
#ifndef lv_NORMALIZATION_THRESHOLD
#define lv_NORMALIZATION_THRESHOLD 1e-15 /* 向量归一化容差 */
#endif

/* 数值范围极限（用于哨兵值） */
#ifndef lv_INFINITY_SENTINEL
#define lv_INFINITY_SENTINEL 1e308 /* "无穷大"哨兵 */
#endif
#ifndef lv_TINY_SENTINEL
#define lv_TINY_SENTINEL 1e-300 /* "接近零"哨兵 */
#endif
#ifndef lv_HUGE_NUMBER
#define lv_HUGE_NUMBER 1e30 /* 极大数阈值 */
#endif
#ifndef lv_LARGE_NUMBER
#define lv_LARGE_NUMBER 1e18 /* 大数阈值 */
#endif
#ifndef lv_BIG_NUMBER
#define lv_BIG_NUMBER 1e9 /* 10亿级阈值 */
#endif

/* ---- 常用数学系数与比例因子 ---- */
/* 缩放系数 */
#ifndef lv_HALF
#define lv_HALF 0.5 /* 1/2 */
#endif
#ifndef lv_THIRD
#define lv_THIRD 0.333333333333333333333 /* 1/3 */
#endif
#ifndef lv_QUARTER
#define lv_QUARTER 0.25 /* 1/4 */
#endif
#ifndef lv_TENTH
#define lv_TENTH 0.1 /* 1/10 */
#endif

/* 角度系数（弧度↔度） */
#ifndef lv_DEG_TO_RAD
#define lv_DEG_TO_RAD 0.017453292519943295769 /* π/180 */
#endif
#ifndef lv_RAD_TO_DEG
#define lv_RAD_TO_DEG 57.2957795130823208768 /* 180/π */
#endif
#ifndef lv_PI
#define lv_PI 3.1415926535897932384626 /* π */
#endif
#ifndef lv_TWO_PI
#define lv_TWO_PI 6.28318530717958647692 /* 2π */
#endif
#ifndef lv_HALF_PI
#define lv_HALF_PI 1.57079632679489661923 /* π/2 */
#endif
#ifndef lv_QUARTER_PI
#define lv_QUARTER_PI 0.78539816339744830962 /* π/4 */
#endif

/* 颜色通道（RGB 0-255 归一化） */
#ifndef lv_COLOR_CHANNEL_MAX
#define lv_COLOR_CHANNEL_MAX 255.0f /* RGB最大通道值 */
#endif
#ifndef lv_COLOR_CHANNEL_HALF
#define lv_COLOR_CHANNEL_HALF 0.5f /* 0.5f */
#endif

/* 哈希表负载因子 */
#ifndef lv_HASH_LOAD_FACTOR_MAX
#define lv_HASH_LOAD_FACTOR_MAX 0.75 /* 最大负载因子 */
#endif
#ifndef lv_HASH_LOAD_FACTOR_MIN
#define lv_HASH_LOAD_FACTOR_MIN 0.25 /* 最小负载因子 */
#endif

/* 缓冲区/块大小（2的幂次） */
#ifndef lv_KB
#define lv_KB 1024.0 /* 1KB */
#endif
#ifndef lv_MB
#define lv_MB 1048576.0 /* 1MB */
#endif
#ifndef lv_GB
#define lv_GB 1073741824.0 /* 1GB */
#endif
#ifndef lv_KB_I
#define lv_KB_I 1024 /* 1KB (int) */
#endif
#ifndef lv_MB_I
#define lv_MB_I 1048576 /* 1MB (int) */
#endif

/* == 以下全部移入 lvConfig 运行时 == */

/* ====================================================================
 * 子系统级配置结构体
 * ==================================================================== */

/** @brief 求解器配置 */
typedef struct lvCfgSolver {
    int solver_max_var_id;           /**< 求解器最大变量 ID（默认 100000） */
    int solver_max_iterations;       /**< 求解器最大迭代次数（默认 10000） */
} lvCfgSolver;

/** @brief 引擎配置（约束图 + 重写引擎） */
typedef struct lvCfgEngine {
    int max_module_depth;            /**< 最大模块深度（默认 32） */
    int graph_adj_max_per_node;      /**< 约束图每节点最大邻接数（默认 256） */
    int default_rewrite_limit;       /**< 默认重写限制（默认 1000） */
    int wl_iterations;               /**< Weisfeiler-Lehman 迭代次数（默认 3） */
    int wl_history_size;             /**< WL 历史大小（默认 64） */
    int vf2_max_depth;               /**< VF2 最大深度（默认 100） */
    int buchberger_max_steps;        /**< Buchberger 最大步数（默认 50000） */
    int groebner_reduce_max_steps;   /**< Groebner 归约最大步数（默认 10000） */
    int engine_max_collaboration_iterations; /**< 重写-求解协作最大迭代次数（默认 10000） */
    int rewrite_default_max_iterations; /**< 重写默认最大迭代次数（默认 1000） */
    int rewrite_engine_init_iterations; /**< 重写引擎初始迭代次数（默认 100） */
} lvCfgEngine;

/** @brief 解析器与类型系统配置 */
typedef struct lvCfgParser {
    int parser_max_input_length;     /**< 解析器最大输入长度（默认 1048576） */
    int parser_max_ast_depth;        /**< 解析器最大 AST 深度（默认 256） */
    int parser_max_ast_nodes;        /**< 解析器最大 AST 节点数（默认 500000） */
    int parser_max_token_length;     /**< 解析器最大 Token 长度（默认 4096） */
    int parser_max_coordinates;      /**< 解析器最大坐标数（默认 16） */
    int parser_max_polygon_vertices; /**< 解析器最大多边形顶点数（默认 32） */
    int parser_max_statements;       /**< 解析器最大语句数（默认 64） */
    int parser_max_arguments;        /**< 解析器最大参数数（默认 16） */
    int parser_max_participants;     /**< 解析器最大参与者数（默认 16） */
} lvCfgParser;

/** @brief 流式输出配置 */
typedef struct lvCfgStream {
    int stream_async_queue_capacity;     /**< 异步队列容量（默认 1024） */
    int stream_initial_callbacks;        /**< 初始回调数（默认 16） */
    int stream_max_callbacks;            /**< 最大回调数（默认 64） */
    int stream_default_throttle_ms;      /**< 默认节流毫秒数（默认 50） */
} lvCfgStream;

/** @brief 数值精度配置 */
typedef struct lvCfgPrecision {
    int bit_cutoff_threshold;            /**< 位截断阈值（默认 1000000） */
    int max_precision_bits;              /**< 最大精度位数（默认 100） */
    int continued_fraction_max_iter;     /**< 连分数最大迭代（默认 1000） */
    int max_subintervals;                /**< 最大子区间数（默认 4096） */
} lvCfgPrecision;

/** @brief MiniKernel 验证配置 */
typedef struct lvCfgMiniKernel {
    int mini_kernel_max_statements;      /**< MiniKernel 最大语句数（默认 10000） */
    int mini_kernel_max_proof_depth;     /**< MiniKernel 最大证明深度（默认 1000） */
    int mini_kernel_verify_timeout_ms;   /**< MiniKernel 验证超时毫秒（默认 30000） */
} lvCfgMiniKernel;

/** @brief UI-Kernel 通信协议配置 */
typedef struct lvCfgProtocol {
    int proto_max_draw_cmds;             /**< 协议最大绘制命令数（默认 4096） */
    int proto_max_table_rows;            /**< 协议最大表格行数（默认 512） */
    int proto_max_tree_nodes;            /**< 协议最大树节点数（默认 256） */
    int proto_max_topology;              /**< 协议最大拓扑数（默认 128） */
    int proto_max_proof_steps;           /**< 协议最大证明步数（默认 512） */
    int proto_max_completions;           /**< 协议最大补全数（默认 64） */
    int proto_max_terminal_lines;        /**< 协议最大终端行数（默认 512） */
} lvCfgProtocol;

/** @brief 几何与 ODE 配置 */
typedef struct lvCfgGeometry {
    int geo_max_objects;                 /**< 交互几何最大对象数（默认 1024） */
    int geo_max_constraints;             /**< 交互几何最大约束数（默认 2048） */
    int geo_max_drag_chain;              /**< 交互几何最大拖拽链（默认 64） */
    int geo_max_snapshots;               /**< 交互几何最大快照数（默认 32） */
    double geo_min_zoom;                 /**< 交互几何最小缩放（默认 0.01） */
    double geo_max_zoom;                 /**< 交互几何最大缩放（默认 100.0） */
    int geoevol_max_param_dim;           /**< ODE 最大参数维度（默认 256） */
    int geoevol_adams_max_order;         /**< ODE Adams 最大阶数（默认 12） */
    int geoevol_max_rejections;          /**< ODE 最大拒绝次数（默认 20） */
    double geoevol_min_step;             /**< ODE 最小步长（默认 1e-15） */
    double geoevol_max_step;             /**< ODE 最大步长（默认 1e10） */
    double geoevol_pi_smooth_factor;     /**< ODE PI 平滑因子（默认 0.25） */
    double geo_sym_coord_eps;            /**< 符号坐标计算容差（默认 1e-8） */
} lvCfgGeometry;

/** @brief 证明引擎配置 */
typedef struct lvCfgProof {
    int proof_max_branches;              /**< 证明最大分支数（默认 64） */
    int proof_max_strategies;            /**< 证明最大策略数（默认 16） */
} lvCfgProof;

/** @brief 上下文与运行时防护配置 */
typedef struct lvCfgContext {
    int max_recursion_depth;                      /**< 最大递归深度（默认 128） */
    int context_default_max_depth;                /**< 上下文默认最大深度（默认 100） */
    int context_max_recursion_depth;              /**< 上下文最大递归深度（默认 10000） */
    int context_default_max_steps;                /**< 上下文默认最大步数（默认 1000000） */
    int context_default_max_consecutive_errors;   /**< 上下文默认最大连续错误数（默认 10） */
    int context_reasoning_stack_default_capacity; /**< 推理栈默认容量（默认 8） */
    int context_reasoning_stack_max_depth;        /**< 推理栈最大深度（默认 1000） */
    int context_timeout_ms;                       /**< 上下文默认超时毫秒（默认 30000） */
    int context_cooldown_ms;                      /**< 上下文冷却时间毫秒（默认 5000） */
    int view_sync_timeout_ms;                     /**< 视图同步超时毫秒（默认 1000） */
} lvCfgContext;

/** @brief 运行时防护配置 */
typedef struct lvCfgRuntimeGuard {
    int runtime_guard_max_recurse;     /**< 运行时防护最大递归（默认 128） */
    int runtime_guard_spin_attempts;   /**< 运行时防护自旋尝试（默认 1024） */
    int runtime_guard_write_warn_us;   /**< 运行时防护写入警告微秒（默认 10000） */
} lvCfgRuntimeGuard;

/** @brief 互操作 / 插件 / 后端集成配置 */
typedef struct lvCfgIntegration {
    int interop_max_params;            /**< 互操作最大参数数（默认 32） */
    int interop_max_completions;       /**< 互操作最大补全数（默认 64） */
    int interop_ws_default_port;       /**< 互操作 WebSocket 默认端口（默认 8765） */
    int interop_buffer_size;           /**< 互操作响应缓冲区大小（默认 65536） */
    int interop_timeout_ms;            /**< 互操作超时毫秒（默认 30000） */
    int max_plugins;                   /**< 最大插件数（默认 256） */
    int max_interfaces;                /**< 最大接口数（默认 128） */
    int backend_step_limit;            /**< 数值后端步数上限（默认 1000） */
    int backend_timeout_ms;            /**< 数值后端超时毫秒（默认 30000） */
} lvCfgIntegration;

/** @brief 诊断与日志配置 */
typedef struct lvCfgDiagnostics {
    int log_max_files;                 /**< 日志最大文件数（默认 5） */
    int log_max_size;                  /**< 日志最大大小（默认 10485760） */
    int log_ring_buffer_capacity;      /**< 日志环形缓冲区容量（默认 256） */
    int perf_sample_max_count;         /**< 性能采样最大计数（默认 10000） */
    int timer_max_depth;               /**< 定时器最大深度（默认 32） */
} lvCfgDiagnostics;

/** @brief 测试与压力测试配置 */
typedef struct lvCfgTest {
    int test_max_suites;               /**< 测试最大 Suite 数（默认 256） */
    int test_max_cases;                /**< 测试最大 Case 数（默认 4096） */
    int smoke_test_step_limit;         /**< 烟测独立步数上限（默认 1000） */
    int smoke_test_timeout_ms;         /**< 烟测总时间上限毫秒（默认 30000） */
    int stress_test_default_chain;     /**< 压力测试默认链长（默认 100） */
    int stress_test_max_poly_degree;   /**< 压力测试最大多项式次数（默认 4） */
} lvCfgTest;

/** @brief 健康与系统安全配置 */
typedef struct lvCfgHealth {
    int health_score_max;              /**< 健康分最大值（默认 100） */
    double health_memory_usage_ratio;  /**< 健康内存使用率阈值（默认 0.8） */
    int health_memory_warning_penalty; /**< 健康内存警告扣分（默认 10） */
    double health_memory_leak_ratio;   /**< 健康内存泄漏比率阈值（默认 0.9） */
    int health_memory_leak_penalty;    /**< 健康内存泄漏扣分（默认 20） */
    int health_recent_error_penalty;   /**< 健康最近错误扣分（默认 5） */
    int circuit_overflow_threshold;    /**< 熔断器溢出阈值（默认 3） */
    int max_consecutive_trips;         /**< 位数熔断连续触发阈值（默认 3） */
    int value_too_large;               /**< 代数值过大阈值（默认 1048576） */
    int downgrade_denominator;         /**< 降级分母阈值（默认 100000） */
    int default_memory_limit_mb;       /**< 默认内存限制 MB（默认 0=无限制） */
} lvCfgHealth;

/** @brief 传播引擎配置 */
typedef struct lvCfgPropagation {
    int prop_max_iterations;           /**< 传播引擎最大迭代次数（默认 10000） */
    int prop_max_backtracks;           /**< 传播引擎最大回溯次数（默认 1000） */
    int prop_max_collaboration_iters;  /**< WFC 协作最大迭代次数（默认 10000） */
} lvCfgPropagation;

/** @brief 高维几何配置 */
typedef struct lvCfgHighDim {
    double high_dim_default_fidelity_threshold; /**< 保真度默认警告阈值（默认 0.85） */
} lvCfgHighDim;

/* ====================================================================
 * lvConfig —— 集中化运行时配置（嵌套子系统结构体）
 * ==================================================================== */

typedef struct lvConfig {
    lvCfgSolver solver;
    lvCfgEngine engine;
    lvCfgParser parser;
    lvCfgStream stream;
    lvCfgPrecision precision;
    lvCfgMiniKernel mini_kernel;
    lvCfgProtocol protocol;
    lvCfgGeometry geometry;
    lvCfgProof proof;
    lvCfgContext context;
    lvCfgRuntimeGuard runtime_guard;
    lvCfgIntegration integration;
    lvCfgDiagnostics diagnostics;
    lvCfgTest test;
    lvCfgHealth health;
    lvCfgPropagation propagation;
    lvCfgHighDim high_dim;
} lvConfig;

/* ====================================================================
 * 配置键 X-macro —— 用于生成 setter 分发 & JSON 加载
 *
 * 格式: X(JSON_KEY_STR, struct_path_member)
 *
 * 用法:
 *   #define SET_IF(key, field) if (strcmp(k, key)==0) { c->field = val; return true; }
 *   LV_CONFIG_INT_KEYS(SET_IF)
 * ==================================================================== */

#define LV_CONFIG_INT_KEYS(X) \
    X("solver_max_var_id", solver.solver_max_var_id) \
    X("solver_max_iterations", solver.solver_max_iterations) \
    X("max_module_depth", engine.max_module_depth) \
    X("graph_adj_max_per_node", engine.graph_adj_max_per_node) \
    X("default_rewrite_limit", engine.default_rewrite_limit) \
    X("wl_iterations", engine.wl_iterations) \
    X("wl_history_size", engine.wl_history_size) \
    X("vf2_max_depth", engine.vf2_max_depth) \
    X("buchberger_max_steps", engine.buchberger_max_steps) \
    X("groebner_reduce_max_steps", engine.groebner_reduce_max_steps) \
    X("engine_max_collaboration_iterations", engine.engine_max_collaboration_iterations) \
    X("rewrite_default_max_iterations", engine.rewrite_default_max_iterations) \
    X("rewrite_engine_init_iterations", engine.rewrite_engine_init_iterations) \
    X("stream_async_queue_capacity", stream.stream_async_queue_capacity) \
    X("stream_initial_callbacks", stream.stream_initial_callbacks) \
    X("stream_max_callbacks", stream.stream_max_callbacks) \
    X("stream_default_throttle_ms", stream.stream_default_throttle_ms) \
    X("bit_cutoff_threshold", precision.bit_cutoff_threshold) \
    X("max_precision_bits", precision.max_precision_bits) \
    X("continued_fraction_max_iter", precision.continued_fraction_max_iter) \
    X("max_subintervals", precision.max_subintervals) \
    X("mini_kernel_max_statements", mini_kernel.mini_kernel_max_statements) \
    X("mini_kernel_max_proof_depth", mini_kernel.mini_kernel_max_proof_depth) \
    X("mini_kernel_verify_timeout_ms", mini_kernel.mini_kernel_verify_timeout_ms) \
    X("parser_max_input_length", parser.parser_max_input_length) \
    X("parser_max_ast_depth", parser.parser_max_ast_depth) \
    X("parser_max_ast_nodes", parser.parser_max_ast_nodes) \
    X("parser_max_token_length", parser.parser_max_token_length) \
    X("parser_max_coordinates", parser.parser_max_coordinates) \
    X("parser_max_polygon_vertices", parser.parser_max_polygon_vertices) \
    X("parser_max_statements", parser.parser_max_statements) \
    X("parser_max_arguments", parser.parser_max_arguments) \
    X("parser_max_participants", parser.parser_max_participants) \
    X("runtime_guard_max_recurse", runtime_guard.runtime_guard_max_recurse) \
    X("runtime_guard_spin_attempts", runtime_guard.runtime_guard_spin_attempts) \
    X("runtime_guard_write_warn_us", runtime_guard.runtime_guard_write_warn_us) \
    X("proto_max_draw_cmds", protocol.proto_max_draw_cmds) \
    X("proto_max_table_rows", protocol.proto_max_table_rows) \
    X("proto_max_tree_nodes", protocol.proto_max_tree_nodes) \
    X("proto_max_topology", protocol.proto_max_topology) \
    X("proto_max_proof_steps", protocol.proto_max_proof_steps) \
    X("proto_max_completions", protocol.proto_max_completions) \
    X("proto_max_terminal_lines", protocol.proto_max_terminal_lines) \
    X("geo_max_objects", geometry.geo_max_objects) \
    X("geo_max_constraints", geometry.geo_max_constraints) \
    X("geo_max_drag_chain", geometry.geo_max_drag_chain) \
    X("geo_max_snapshots", geometry.geo_max_snapshots) \
    X("geoevol_max_param_dim", geometry.geoevol_max_param_dim) \
    X("geoevol_adams_max_order", geometry.geoevol_adams_max_order) \
    X("geoevol_max_rejections", geometry.geoevol_max_rejections) \
    X("proof_max_branches", proof.proof_max_branches) \
    X("proof_max_strategies", proof.proof_max_strategies) \
    X("max_recursion_depth", context.max_recursion_depth) \
    X("context_default_max_depth", context.context_default_max_depth) \
    X("context_max_recursion_depth", context.context_max_recursion_depth) \
    X("context_default_max_steps", context.context_default_max_steps) \
    X("context_default_max_consecutive_errors", context.context_default_max_consecutive_errors) \
    X("context_reasoning_stack_default_capacity", context.context_reasoning_stack_default_capacity) \
    X("context_reasoning_stack_max_depth", context.context_reasoning_stack_max_depth) \
    X("interop_max_params", integration.interop_max_params) \
    X("interop_max_completions", integration.interop_max_completions) \
    X("interop_ws_default_port", integration.interop_ws_default_port) \
    X("interop_buffer_size", integration.interop_buffer_size) \
    X("interop_timeout_ms", integration.interop_timeout_ms) \
    X("log_max_files", diagnostics.log_max_files) \
    X("log_max_size", diagnostics.log_max_size) \
    X("log_ring_buffer_capacity", diagnostics.log_ring_buffer_capacity) \
    X("perf_sample_max_count", diagnostics.perf_sample_max_count) \
    X("timer_max_depth", diagnostics.timer_max_depth) \
    X("max_plugins", integration.max_plugins) \
    X("max_interfaces", integration.max_interfaces) \
    X("backend_step_limit", integration.backend_step_limit) \
    X("backend_timeout_ms", integration.backend_timeout_ms) \
    X("test_max_suites", test.test_max_suites) \
    X("test_max_cases", test.test_max_cases) \
    X("smoke_test_step_limit", test.smoke_test_step_limit) \
    X("smoke_test_timeout_ms", test.smoke_test_timeout_ms) \
    X("stress_test_default_chain", test.stress_test_default_chain) \
    X("stress_test_max_poly_degree", test.stress_test_max_poly_degree) \
    X("circuit_overflow_threshold", health.circuit_overflow_threshold) \
    X("max_consecutive_trips", health.max_consecutive_trips) \
    X("value_too_large", health.value_too_large) \
    X("downgrade_denominator", health.downgrade_denominator) \
    X("default_memory_limit_mb", health.default_memory_limit_mb) \
    X("health_score_max", health.health_score_max) \
    X("health_memory_warning_penalty", health.health_memory_warning_penalty) \
    X("health_memory_leak_penalty", health.health_memory_leak_penalty) \
    X("health_recent_error_penalty", health.health_recent_error_penalty) \
    X("context_timeout_ms", context.context_timeout_ms) \
    X("context_cooldown_ms", context.context_cooldown_ms) \
    X("view_sync_timeout_ms", context.view_sync_timeout_ms) \
    X("prop_max_iterations", propagation.prop_max_iterations) \
    X("prop_max_backtracks", propagation.prop_max_backtracks) \
    X("prop_max_collaboration_iters", propagation.prop_max_collaboration_iters)

#define LV_CONFIG_DOUBLE_KEYS(X) \
    X("geo_min_zoom", geometry.geo_min_zoom) \
    X("geo_max_zoom", geometry.geo_max_zoom) \
    X("geoevol_min_step", geometry.geoevol_min_step) \
    X("geoevol_max_step", geometry.geoevol_max_step) \
    X("geoevol_pi_smooth_factor", geometry.geoevol_pi_smooth_factor) \
    X("health_memory_usage_ratio", health.health_memory_usage_ratio) \
    X("health_memory_leak_ratio", health.health_memory_leak_ratio) \
    X("high_dim_default_fidelity_threshold", high_dim.high_dim_default_fidelity_threshold) \
    X("geo_sym_coord_eps", geometry.geo_sym_coord_eps)

/* ====================================================================
 * 运行时配置 API
 * ==================================================================== */

const lvConfig *lv_config_default(void);
const lvConfig *lv_config_current(void);
int lv_config_apply(const lvConfig *cfg);
int lv_config_load_json(const char *json_path);
int lv_config_to_json(char *buf, size_t buf_size);

/* ====================================================================
 * 运行时单字段修改 API（不重编译，立即生效）
 * ==================================================================== */

/* ---- 类型安全 setter（高频字段，IDE 自动补全） ---- */
void lv_config_set_solver_max_var_id(int val);
void lv_config_set_solver_max_iterations(int val);
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
void lv_config_set_context_timeout_ms(int val);
void lv_config_set_context_cooldown_ms(int val);
void lv_config_set_prop_max_iterations(int val);
void lv_config_set_prop_max_backtracks(int val);
void lv_config_set_prop_max_collaboration_iters(int val);
void lv_config_set_high_dim_default_fidelity_threshold(double val);
void lv_config_set_geo_sym_coord_eps(double val);
void lv_config_set_engine_max_collaboration_iterations(int val);
void lv_config_set_rewrite_default_max_iterations(int val);
void lv_config_set_rewrite_engine_init_iterations(int val);
void lv_config_set_interop_buffer_size(int val);
void lv_config_set_interop_timeout_ms(int val);
void lv_config_set_view_sync_timeout_ms(int val);
void lv_config_set_max_consecutive_trips(int val);

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
#define lv_PROTO_MAX_DRAW_CMDS 4096
#endif
#ifndef lv_PROTO_MAX_TABLE_ROWS
#define lv_PROTO_MAX_TABLE_ROWS 512
#endif
#ifndef lv_PROTO_MAX_TREE_NODES
#define lv_PROTO_MAX_TREE_NODES 256
#endif
#ifndef lv_PROTO_MAX_TOPOLOGY
#define lv_PROTO_MAX_TOPOLOGY 128
#endif
#ifndef lv_PROTO_MAX_PROOF_STEPS
#define lv_PROTO_MAX_PROOF_STEPS 512
#endif
#ifndef lv_PROTO_MAX_COMPLETIONS
#define lv_PROTO_MAX_COMPLETIONS 64
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

/* ---- 超时与步数上限（编译期默认值） ---- */
#ifndef lv_DEFAULT_TIMEOUT_MS
#define lv_DEFAULT_TIMEOUT_MS 30000
#endif
#ifndef lv_DEFAULT_MAX_STEPS
#define lv_DEFAULT_MAX_STEPS 10000
#endif

/* ---- 通用缓冲区大小（编译期，用于 char buf[N] 声明） ---- */
#ifndef lv_DETAIL_BUF_SIZE
#define lv_DETAIL_BUF_SIZE 512
#endif
#ifndef lv_MSG_BUF_SIZE
#define lv_MSG_BUF_SIZE 256
#endif
#ifndef lv_LARGE_BUF_SIZE
#define lv_LARGE_BUF_SIZE 4096
#endif
#ifndef lv_MEDIUM_BUF_SIZE
#define lv_MEDIUM_BUF_SIZE 2048
#endif
#ifndef lv_PATH_BUF_SIZE
#define lv_PATH_BUF_SIZE 1024
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

/* ====================================================================
 * 字符串键注册表 —— lv_config_get_* / lv_config_set_* 使用的键名
 *
 * 集中定义所有运行时字符串配置键，使用处一律引用宏而非裸字符串字面量，
 * 以获得编译期拼写检查（LV_CFG_* 前缀，区别于 lv_CONFIG_* 编译期常量
 * 与 LV_CONFIG_INT_KEYS / LV_CONFIG_DOUBLE_KEYS 的 JSON 键 X-macro）。
 * 其中与 lvConfig 结构体系统同名的键（如 cdcl_max_steps）两侧命名一致，
 * 但分属不同的配置存储，勿混淆。
 * ==================================================================== */
#define LV_CFG_META_PROOF_TIMEOUT_MS       "meta_proof_timeout_ms"
#define LV_CFG_TYPE_INFER_MAX_DEPTH        "type_infer_max_depth"
#define LV_CFG_TYPE_EQUIV_MAX_DEPTH        "type_equiv_max_depth"
#define LV_CFG_CIRCUIT_OVERFLOW_THRESHOLD  "circuit_overflow_threshold"
#define LV_CFG_CDCL_MAX_STEPS              "cdcl_max_steps"
#define LV_CFG_CDCL_MAX_DECISIONS          "cdcl_max_decisions"
#define LV_CFG_CDCL_MAX_RESTARTS           "cdcl_max_restarts"
#define LV_CFG_SMT_SOLVER_TIMEOUT_MS       "smt_solver_timeout_ms"
#define LV_CFG_SMOKE_TEST_STEP_LIMIT       "smoke_test_step_limit"
#define LV_CFG_SMOKE_TEST_TIMEOUT_MS       "smoke_test_timeout_ms"
#define LV_CFG_BUCHBERGER_TIME_BUDGET_MS   "buchberger_time_budget_ms"
#define LV_CFG_BUCHBERGER_MAX_STEPS        "buchberger_max_steps"
#define LV_CFG_GROEBNER_REDUCE_MAX_STEPS   "groebner_reduce_max_steps"
#define LV_CFG_PCTL_VALUE_ITER_MAX         "pctl_value_iter_max"
#define LV_CFG_PCTL_POWER_ITER_MAX         "pctl_power_iter_max"

/* ---- SMT backend config keys ---- */
#define LV_CFG_SMT_DEFAULT_TIMEOUT_MS       "smt_default_timeout_ms"
#define LV_CFG_SMT_DEFAULT_MEMORY_MB        "smt_default_memory_mb"
#define LV_CFG_SMTLIB2_DEFAULT_BUFFER       "smtlib2_default_buffer"
#define LV_CFG_GROEBNER_DEFAULT_VAR_CAPACITY "groebner_default_var_capacity"
#define LV_CFG_GROEBNER_SMT_ZERO_THRESHOLD  "groebner_smt_zero_threshold"
/* ---- Groebner engine config keys ---- */
#define LV_CFG_GROEBNER_ZERO_THRESHOLD       "groebner_zero_threshold"
#define LV_CFG_GROEBNER_POLY_INIT_CAPACITY   "groebner_poly_init_capacity"
#define LV_CFG_GROEBNER_SOLVE_MAX_ITER       "groebner_solve_max_iter"
#define LV_CFG_GROEBNER_NEWTON_TOL           "groebner_newton_tol"
#define LV_CFG_GROEBNER_NEWTON_MAX_ITER      "groebner_newton_max_iter"
#define LV_CFG_GROEBNER_ROOT_SEARCH_SEGMENTS "groebner_root_search_segments"
/* ---- AABB tree config keys ---- */
#define LV_CFG_AABB_INITIAL_CAPACITY         "aabb_initial_capacity"
#define LV_CFG_AABB_DEFAULT_MAX_LEAF_SIZE    "aabb_default_max_leaf_size"
#define LV_CFG_AABB_DEFAULT_MAX_DEPTH        "aabb_default_max_depth"
#define LV_CFG_AABB_DEFAULT_USE_SAH          "aabb_default_use_sah"
/* ---- ATP backend config keys ---- */
#define LV_CFG_ATP_DEFAULT_TIMEOUT           "atp_default_timeout"
#define LV_CFG_ATP_DEFAULT_MEMORY_MB         "atp_default_memory_mb"
#define LV_CFG_ATP_TPTP_BUFFER_SIZE          "atp_tptp_buffer_size"

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
