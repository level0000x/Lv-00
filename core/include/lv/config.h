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
#define lv_GEO_COLLINEAR_EPSILON lv_EPSILON_MEDIUM /* 共线性判断容差（语义别名 = lv_EPSILON_MEDIUM，1e-9） */
#endif
#ifndef lv_GEO_DISTANCE_EPSILON
/* 距离判断容差：独立值 1e-8（分级体系无对应档），作为 1e-8 组语义常量
 * （EUCLID_CONGRUENCE_TOLERANCE、NUMERIC_VERIFY_TOLERANCE）的数值权威。 */
#define lv_GEO_DISTANCE_EPSILON 1e-8
#endif
#ifndef lv_GEO_ANGLE_EPSILON
#define lv_GEO_ANGLE_EPSILON lv_EPSILON_HIGH /* 角度相等容差（语义别名 = lv_EPSILON_HIGH，1e-10） */
#endif
#ifndef lv_GEO_LENGTH_GUARD
#define lv_GEO_LENGTH_GUARD 0.001 /* 几何长度/分母保护阈值（interop 弧偏移除数保护、贝塞尔法线归一化防护） */
#endif

/* 代数运算安全阈值 */
#ifndef lv_SINGULARITY_THRESHOLD
#define lv_SINGULARITY_THRESHOLD lv_EPSILON_ULTRA /* 矩阵奇异性判断（语义别名 = lv_EPSILON_ULTRA，1e-12） */
#endif
#ifndef lv_NORMALIZATION_THRESHOLD
#define lv_NORMALIZATION_THRESHOLD lv_EPSILON_SUPERTINY /* 向量归一化容差（语义别名 = lv_EPSILON_SUPERTINY，1e-15） */
#endif
#ifndef lv_GAPPA_BOUND_SLACK
#define lv_GAPPA_BOUND_SLACK 1e-15 /* Gappa 区间包含判定松弛量（gappa_dsl/gappa_propagate 传播精度） */
#endif

/* 数值范围极限（用于哨兵值） */
#ifndef lv_INFINITY_SENTINEL
#define lv_INFINITY_SENTINEL 1e308 /* "无穷大"哨兵 */
#endif
#ifndef lv_NEAR_INFINITY_SENTINEL
#define lv_NEAR_INFINITY_SENTINEL 1e300 /* "近无穷大"哨兵（包围盒/距离/越界上界，较 1e308 保留算术溢出余量） */
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
#ifndef lv_ZERO_GUARD_EPS
#define lv_ZERO_GUARD_EPS 1e-30 /* 接近零保护阈值（除数/值过滤守卫） */
#endif
#ifndef lv_SAFE_MIN_POSITIVE
#define lv_SAFE_MIN_POSITIVE 1e-308 /* 下溢保护哨兵（避免 log(0)/除零，DBL_MIN 附近） */
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

/* 渲染：无限直线扩展范围（将无限延伸直线近似为有限线段时，从端点沿方向扩展的单位数） */
#ifndef lv_RENDER_INFINITE_LINE_EXTENT
#define lv_RENDER_INFINITE_LINE_EXTENT 1000.0 /* 无限直线渲染扩展范围 */
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
 * 配置键 X-macro —— lvConfig 配置系统的单一事实来源
 *
 * 四元组格式: X(key, type, field, default)
 *   - key     裸标识符；#key 字符串化后即 JSON / 字符串分发的键名，
 *             同时用于生成 lv_config_get_<key> / lv_config_set_<key> 函数名
 *   - type    int 或 double（对应字段的 C 类型）
 *   - field   lvConfig 结构体成员路径（相对路径，如 engine.max_module_depth）
 *   - default 默认值字面量（C 字面量，如 100000 / 0.85 / 1e-8）
 *
 * 由宏一次性生成五件套（定义在 lv_config.c 展开，声明由 config.h 生成）：
 *   - 默认值初始化    DEFAULT_INT / DEFAULT_DBL      def.field = default;
 *   - 类型安全 getter GETTER                         type lv_config_get_<key>(void)
 *   - 类型安全 setter SETTER                         void lv_config_set_<key>(type val)
 *   - 通用 set 分发    SET_IF（lv_config_set_int / lv_config_set_double 的 strcmp 链）
 *   - JSON 读写        JLD / TOJSON_INT / TOJSON_DBL
 *
 * 用法:
 *   #define SET_IF(key, type, field, dflt) if (strcmp(k, #key)==0) { c->field = val; return true; }
 *   LV_CONFIG_INT_KEYS(SET_IF)
 * ==================================================================== */

#define LV_CONFIG_INT_KEYS(X) \
    X(solver_max_var_id, int, solver.solver_max_var_id, 100000) \
    X(solver_max_iterations, int, solver.solver_max_iterations, 10000) \
    X(max_module_depth, int, engine.max_module_depth, 32) \
    X(graph_adj_max_per_node, int, engine.graph_adj_max_per_node, 256) \
    X(default_rewrite_limit, int, engine.default_rewrite_limit, 1000) \
    X(wl_iterations, int, engine.wl_iterations, 3) \
    X(wl_history_size, int, engine.wl_history_size, 64) \
    X(vf2_max_depth, int, engine.vf2_max_depth, 100) \
    X(buchberger_max_steps, int, engine.buchberger_max_steps, 50000) \
    X(groebner_reduce_max_steps, int, engine.groebner_reduce_max_steps, 10000) \
    X(engine_max_collaboration_iterations, int, engine.engine_max_collaboration_iterations, 10000) \
    X(rewrite_default_max_iterations, int, engine.rewrite_default_max_iterations, 1000) \
    X(rewrite_engine_init_iterations, int, engine.rewrite_engine_init_iterations, 100) \
    X(stream_async_queue_capacity, int, stream.stream_async_queue_capacity, 1024) \
    X(stream_initial_callbacks, int, stream.stream_initial_callbacks, 16) \
    X(stream_max_callbacks, int, stream.stream_max_callbacks, 64) \
    X(stream_default_throttle_ms, int, stream.stream_default_throttle_ms, 50) \
    X(bit_cutoff_threshold, int, precision.bit_cutoff_threshold, 1000000) \
    X(max_precision_bits, int, precision.max_precision_bits, 100) \
    X(continued_fraction_max_iter, int, precision.continued_fraction_max_iter, 1000) \
    X(max_subintervals, int, precision.max_subintervals, 4096) \
    X(mini_kernel_max_statements, int, mini_kernel.mini_kernel_max_statements, 10000) \
    X(mini_kernel_max_proof_depth, int, mini_kernel.mini_kernel_max_proof_depth, 1000) \
    X(mini_kernel_verify_timeout_ms, int, mini_kernel.mini_kernel_verify_timeout_ms, 30000) \
    X(parser_max_input_length, int, parser.parser_max_input_length, 1048576) \
    X(parser_max_ast_depth, int, parser.parser_max_ast_depth, 256) \
    X(parser_max_ast_nodes, int, parser.parser_max_ast_nodes, 500000) \
    X(parser_max_token_length, int, parser.parser_max_token_length, 4096) \
    X(parser_max_coordinates, int, parser.parser_max_coordinates, 16) \
    X(parser_max_polygon_vertices, int, parser.parser_max_polygon_vertices, 32) \
    X(parser_max_statements, int, parser.parser_max_statements, 64) \
    X(parser_max_arguments, int, parser.parser_max_arguments, 16) \
    X(parser_max_participants, int, parser.parser_max_participants, 16) \
    X(runtime_guard_max_recurse, int, runtime_guard.runtime_guard_max_recurse, 128) \
    X(runtime_guard_spin_attempts, int, runtime_guard.runtime_guard_spin_attempts, 1024) \
    X(runtime_guard_write_warn_us, int, runtime_guard.runtime_guard_write_warn_us, 10000) \
    X(proto_max_draw_cmds, int, protocol.proto_max_draw_cmds, 4096) \
    X(proto_max_table_rows, int, protocol.proto_max_table_rows, 512) \
    X(proto_max_tree_nodes, int, protocol.proto_max_tree_nodes, 256) \
    X(proto_max_topology, int, protocol.proto_max_topology, 128) \
    X(proto_max_proof_steps, int, protocol.proto_max_proof_steps, 512) \
    X(proto_max_completions, int, protocol.proto_max_completions, 64) \
    X(proto_max_terminal_lines, int, protocol.proto_max_terminal_lines, 512) \
    X(geo_max_objects, int, geometry.geo_max_objects, 1024) \
    X(geo_max_constraints, int, geometry.geo_max_constraints, 2048) \
    X(geo_max_drag_chain, int, geometry.geo_max_drag_chain, 64) \
    X(geo_max_snapshots, int, geometry.geo_max_snapshots, 32) \
    X(geoevol_max_param_dim, int, geometry.geoevol_max_param_dim, 256) \
    X(geoevol_adams_max_order, int, geometry.geoevol_adams_max_order, 12) \
    X(geoevol_max_rejections, int, geometry.geoevol_max_rejections, 20) \
    X(proof_max_branches, int, proof.proof_max_branches, 64) \
    X(proof_max_strategies, int, proof.proof_max_strategies, 16) \
    X(max_recursion_depth, int, context.max_recursion_depth, 128) \
    X(context_default_max_depth, int, context.context_default_max_depth, 100) \
    X(context_max_recursion_depth, int, context.context_max_recursion_depth, 10000) \
    X(context_default_max_steps, int, context.context_default_max_steps, 1000000) \
    X(context_default_max_consecutive_errors, int, context.context_default_max_consecutive_errors, 10) \
    X(context_reasoning_stack_default_capacity, int, context.context_reasoning_stack_default_capacity, 8) \
    X(context_reasoning_stack_max_depth, int, context.context_reasoning_stack_max_depth, 1000) \
    X(interop_max_params, int, integration.interop_max_params, 32) \
    X(interop_max_completions, int, integration.interop_max_completions, 64) \
    X(interop_ws_default_port, int, integration.interop_ws_default_port, 8765) \
    X(interop_buffer_size, int, integration.interop_buffer_size, 65536) \
    X(interop_timeout_ms, int, integration.interop_timeout_ms, 30000) \
    X(log_max_files, int, diagnostics.log_max_files, 5) \
    X(log_max_size, int, diagnostics.log_max_size, 10485760) \
    X(log_ring_buffer_capacity, int, diagnostics.log_ring_buffer_capacity, 256) \
    X(perf_sample_max_count, int, diagnostics.perf_sample_max_count, 10000) \
    X(timer_max_depth, int, diagnostics.timer_max_depth, 32) \
    X(max_plugins, int, integration.max_plugins, 256) \
    X(max_interfaces, int, integration.max_interfaces, 128) \
    X(backend_step_limit, int, integration.backend_step_limit, 1000) \
    X(backend_timeout_ms, int, integration.backend_timeout_ms, 30000) \
    X(test_max_suites, int, test.test_max_suites, 256) \
    X(test_max_cases, int, test.test_max_cases, 4096) \
    X(smoke_test_step_limit, int, test.smoke_test_step_limit, 1000) \
    X(smoke_test_timeout_ms, int, test.smoke_test_timeout_ms, 30000) \
    X(stress_test_default_chain, int, test.stress_test_default_chain, 100) \
    X(stress_test_max_poly_degree, int, test.stress_test_max_poly_degree, 4) \
    X(circuit_overflow_threshold, int, health.circuit_overflow_threshold, 3) \
    X(max_consecutive_trips, int, health.max_consecutive_trips, 3) \
    X(value_too_large, int, health.value_too_large, 1048576) \
    X(downgrade_denominator, int, health.downgrade_denominator, 100000) \
    X(default_memory_limit_mb, int, health.default_memory_limit_mb, 0) \
    X(health_score_max, int, health.health_score_max, 100) \
    X(health_memory_warning_penalty, int, health.health_memory_warning_penalty, 10) \
    X(health_memory_leak_penalty, int, health.health_memory_leak_penalty, 20) \
    X(health_recent_error_penalty, int, health.health_recent_error_penalty, 5) \
    X(context_timeout_ms, int, context.context_timeout_ms, 30000) \
    X(context_cooldown_ms, int, context.context_cooldown_ms, 5000) \
    X(view_sync_timeout_ms, int, context.view_sync_timeout_ms, 1000) \
    X(prop_max_iterations, int, propagation.prop_max_iterations, 10000) \
    X(prop_max_backtracks, int, propagation.prop_max_backtracks, 1000) \
    X(prop_max_collaboration_iters, int, propagation.prop_max_collaboration_iters, 10000)

#define LV_CONFIG_DOUBLE_KEYS(X) \
    X(geo_min_zoom, double, geometry.geo_min_zoom, 0.01) \
    X(geo_max_zoom, double, geometry.geo_max_zoom, 100.0) \
    X(geoevol_min_step, double, geometry.geoevol_min_step, 1e-15) \
    X(geoevol_max_step, double, geometry.geoevol_max_step, 1e10) \
    X(geoevol_pi_smooth_factor, double, geometry.geoevol_pi_smooth_factor, 0.25) \
    X(health_memory_usage_ratio, double, health.health_memory_usage_ratio, 0.8) \
    X(health_memory_leak_ratio, double, health.health_memory_leak_ratio, 0.9) \
    X(high_dim_default_fidelity_threshold, double, high_dim.high_dim_default_fidelity_threshold, 0.85) \
    X(geo_sym_coord_eps, double, geometry.geo_sym_coord_eps, 1e-8)

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

/* ---- 类型安全 getter / setter（由四元组 X-macro 生成声明） ---- */
#define LV_CONFIG_DECL_GET(key, type, field, dflt) type lv_config_get_##key(void);
#define LV_CONFIG_DECL_SET(key, type, field, dflt) void lv_config_set_##key(type val);
LV_CONFIG_INT_KEYS(LV_CONFIG_DECL_GET)
LV_CONFIG_DOUBLE_KEYS(LV_CONFIG_DECL_GET)
LV_CONFIG_INT_KEYS(LV_CONFIG_DECL_SET)
LV_CONFIG_DOUBLE_KEYS(LV_CONFIG_DECL_SET)
#undef LV_CONFIG_DECL_GET
#undef LV_CONFIG_DECL_SET

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
