/**
 * @file lv00_optimized.h
 * @brief Lv-00 核心库主头文件 (优化版 v3.5.1)
 * 
 * 本文件是 Lv-00 几何元编程系统的核心头文件，提供统一的 C 语言 API 接口。
 * 包含符号坐标系统、约束图管理、方程求解、图同构检测、证明验证等核心功能。
 * 
 * 模块架构：
 *   - Layer 1 (Foundation): 符号坐标、内存管理、错误处理
 *   - Layer 2 (Graph): 约束图、几何节点、约束关系
 *   - Layer 3 (Analysis): 图分析、规范化、同构检测
 *   - Layer 4 (Engine): 求解引擎、重写系统、证明验证
 *   - Layer 5 (Interface): Python/JS 绑定、WebAssembly 接口
 * 
 * 版本：3.5.1 (优化版)
 * 作者：Lv-00 开发团队
 * 许可证：MIT
 */

#ifndef LV00_OPTIMIZED_H
#define LV00_OPTIMIZED_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/* =============================================================
 * 版本信息 / Version Information
 * ============================================================= */

/** 主版本号 */
#define LV00_VERSION_MAJOR 3
/** 次版本号 */
#define LV00_VERSION_MINOR 5
/** 修订版本号 */
#define LV00_VERSION_PATCH 1
/** 完整版本字符串 */
#define LV00_VERSION_STRING "3.5.1"

/* =============================================================
 * 平台检测 / Platform Detection
 * ============================================================= */

#if defined(_WIN32) || defined(__CYGWIN__)
    #define LV00_PLATFORM_WINDOWS 1
    #ifdef LV00_BUILD_DLL
        #define LV00_API __declspec(dllexport)
    #else
        #define LV00_API __declspec(dllimport)
    #endif
#elif defined(__APPLE__)
    #define LV00_PLATFORM_MACOS 1
    #define LV00_API __attribute__((visibility("default")))
#elif defined(__linux__)
    #define LV00_PLATFORM_LINUX 1
    #define LV00_API __attribute__((visibility("default")))
#else
    #define LV00_API
#endif

/* =============================================================
 * 错误码定义 / Error Codes
 * ============================================================= */

typedef enum {
    LV00_OK = 0,                    /**< 成功 */
    LV00_ERROR = -1,                /**< 通用错误 */
    LV00_OUT_OF_MEMORY = -2,        /**< 内存不足 */
    LV00_INVALID_ARGUMENT = -3,     /**< 无效参数 */
    LV00_NULL_POINTER = -4,         /**< 空指针 */
    LV00_NOT_IMPLEMENTED = -5,      /**< 未实现 */
    LV00_NOT_FOUND = -6,            /**< 未找到 */
    LV00_ALREADY_EXISTS = -7,       /**< 已存在 */
    LV00_OVERFLOW = -8,             /**< 溢出 */
    LV00_UNDERFLOW = -9,            /**< 下溢 */
    LV00_IO_ERROR = -10,            /**< IO 错误 */
    LV00_PARSE_ERROR = -11,         /**< 解析错误 */
    LV00_CONSTRAINT_CONFLICT = -12, /**< 约束冲突 */
    LV00_SOLVER_ERROR = -13,        /**< 求解器错误 */
    LV00_TIMEOUT = -14,             /**< 超时 */
    LV00_CANCELLED = -15,           /**< 已取消 */
} Lv00Status;

/* =============================================================
 * 符号坐标系统 / Symbolic Coordinate System
 * ============================================================= */

/**
 * @brief 符号坐标类型枚举
 * 
 * 定义符号坐标支持的各种数值类型。
 */
typedef enum {
    COORD_RATIONAL = 0,         /**< 有理数 */
    COORD_ALGEBRAIC = 1,        /**< 代数数 */
    COORD_QUADRATIC = 2,        /**< 二次根式 */
    COORD_TRANSCENDENTAL = 3,   /**< 超越数 */
    COORD_SPECIAL = 4,          /**< 特殊值（无穷、未定义等） */
} Lv00CoordType;

/**
 * @brief 符号坐标结构体（不透明）
 * 
 * 符号坐标是 Lv-00 精确几何计算的基础数值类型。
 * 支持任意精度的有理数、代数数和超越数运算。
 */
typedef struct Lv00SymbolicCoord Lv00SymbolicCoord;

/**
 * @brief 创建有理数坐标
 * @param numerator 分子
 * @param denominator 分母（不能为 0）
 * @return 新创建的符号坐标，失败返回 NULL
 */
LV00_API Lv00SymbolicCoord* lv00_coord_create_rational(int64_t numerator, int64_t denominator);

/**
 * @brief 从字符串解析符号坐标
 * @param str 格式如 "3/4", "sqrt(2)", "pi" 等
 * @return 新创建的符号坐标，失败返回 NULL
 */
LV00_API Lv00SymbolicCoord* lv00_coord_parse(const char* str);

/**
 * @brief 复制符号坐标
 * @param coord 源坐标
 * @return 新创建的副本，失败返回 NULL
 */
LV00_API Lv00SymbolicCoord* lv00_coord_copy(const Lv00SymbolicCoord* coord);

/**
 * @brief 销毁符号坐标
 * @param coord 要销毁的坐标
 */
LV00_API void lv00_coord_destroy(Lv00SymbolicCoord* coord);

/**
 * @brief 获取坐标类型
 * @param coord 符号坐标
 * @return 坐标类型枚举值
 */
LV00_API Lv00CoordType lv00_coord_get_type(const Lv00SymbolicCoord* coord);

/**
 * @brief 序列化坐标为字符串
 * @param coord 符号坐标
 * @return 字符串表示（调用者负责释放），失败返回 NULL
 */
LV00_API char* lv00_coord_serialize(const Lv00SymbolicCoord* coord);

/**
 * @brief 算术运算
 */
LV00_API Lv00SymbolicCoord* lv00_coord_add(const Lv00SymbolicCoord* a, const Lv00SymbolicCoord* b);
LV00_API Lv00SymbolicCoord* lv00_coord_subtract(const Lv00SymbolicCoord* a, const Lv00SymbolicCoord* b);
LV00_API Lv00SymbolicCoord* lv00_coord_multiply(const Lv00SymbolicCoord* a, const Lv00SymbolicCoord* b);
LV00_API Lv00SymbolicCoord* lv00_coord_divide(const Lv00SymbolicCoord* a, const Lv00SymbolicCoord* b);
LV00_API Lv00SymbolicCoord* lv00_coord_negate(const Lv00SymbolicCoord* coord);
LV00_API Lv00SymbolicCoord* lv00_coord_abs(const Lv00SymbolicCoord* coord);

/**
 * @brief 幂运算
 * @param base 底数
 * @param exp 指数（整数）
 * @return 计算结果，失败返回 NULL
 */
LV00_API Lv00SymbolicCoord* lv00_coord_power(const Lv00SymbolicCoord* base, int64_t exp);

/**
 * @brief 比较运算
 * @return 负数(a<b), 0(a==b), 正数(a>b)
 */
LV00_API int lv00_coord_compare(const Lv00SymbolicCoord* a, const Lv00SymbolicCoord* b);

/**
 * @brief 类型检查
 */
LV00_API bool lv00_coord_is_zero(const Lv00SymbolicCoord* coord);
LV00_API bool lv00_coord_is_positive(const Lv00SymbolicCoord* coord);
LV00_API bool lv00_coord_is_negative(const Lv00SymbolicCoord* coord);
LV00_API bool lv00_coord_is_one(const Lv00SymbolicCoord* coord);

/**
 * @brief 转换为浮点数（可能丢失精度）
 */
LV00_API double lv00_coord_to_double(const Lv00SymbolicCoord* coord);

/**
 * @brief 获取哈希值
 */
LV00_API uint64_t lv00_coord_hash(const Lv00SymbolicCoord* coord);

/* =============================================================
 * 几何类型定义 / Geometry Types
 * ============================================================= */

/**
 * @brief 几何节点类型枚举
 */
typedef enum {
    GEOM_POINT = 0,             /**< 点 */
    GEOM_LINE_SEGMENT = 1,      /**< 线段 */
    GEOM_PORT = 2,              /**< 端口 */
    GEOM_FUNCTION_BLOCK = 3,    /**< 函数块 */
    GEOM_REGION = 4,            /**< 区域 */
    GEOM_CIRCLE = 5,            /**< 圆 */
    GEOM_ARC = 6,               /**< 圆弧 */
} Lv00GeomType;

/**
 * @brief 端口方向枚举
 */
typedef enum {
    PORT_INPUT = 0,     /**< 输入端口 */
    PORT_OUTPUT = 1,    /**< 输出端口 */
    PORT_BIDIR = 2,     /**< 双向端口 */
} Lv00PortDirection;

/**
 * @brief 约束类型枚举
 */
typedef enum {
    CONSTRAINT_INCIDENCE = 0,       /**< 关联约束 */
    CONSTRAINT_BETWEENNESS = 1,     /**< 介于约束 */
    CONSTRAINT_INTERSECTION = 2,    /**< 相交约束 */
    CONSTRAINT_CONTAINMENT = 3,     /**< 包含约束 */
    CONSTRAINT_CONNECTION = 4,      /**< 连接约束 */
    CONSTRAINT_DISTANCE = 5,        /**< 距离约束 */
    CONSTRAINT_ANGLE = 6,           /**< 角度约束 */
    CONSTRAINT_PARALLEL = 7,        /**< 平行约束 */
    CONSTRAINT_PERPENDICULAR = 8,   /**< 垂直约束 */
    CONSTRAINT_EQUAL_LENGTH = 9,    /**< 等长约束 */
    CONSTRAINT_COLLINEAR = 10,      /**< 共线约束 */
} Lv00ConstraintType;

/**
 * @brief 约束强度枚举
 */
typedef enum {
    CONSTRAINT_WEAK = 0,        /**< 弱约束 */
    CONSTRAINT_NORMAL = 1,      /**< 普通约束 */
    CONSTRAINT_STRONG = 2,      /**< 强约束 */
    CONSTRAINT_REQUIRED = 3,    /**< 必需约束 */
} Lv00ConstraintStrength;

/* =============================================================
 * 约束图 / Constraint Graph
 * ============================================================= */

/**
 * @brief 约束图结构体（不透明）
 * 
 * 约束图是 Lv-00 的核心数据结构，表示几何构造的完整约束关系。
 */
typedef struct Lv00ConstraintGraph Lv00ConstraintGraph;

/**
 * @brief 几何节点结构体（不透明）
 */
typedef struct Lv00GeomNode Lv00GeomNode;

/**
 * @brief 约束结构体（不透明）
 */
typedef struct Lv00Constraint Lv00Constraint;

/**
 * @brief 创建新的空约束图
 * @return 新创建的约束图，失败返回 NULL
 */
LV00_API Lv00ConstraintGraph* lv00_graph_create(void);

/**
 * @brief 销毁约束图
 * @param graph 要销毁的约束图
 */
LV00_API void lv00_graph_destroy(Lv00ConstraintGraph* graph);

/**
 * @brief 复制约束图
 * @param graph 源约束图
 * @return 新创建的副本，失败返回 NULL
 */
LV00_API Lv00ConstraintGraph* lv00_graph_copy(const Lv00ConstraintGraph* graph);

/**
 * @brief 清空约束图（保留图结构，移除所有节点和约束）
 * @param graph 约束图
 */
LV00_API void lv00_graph_clear(Lv00ConstraintGraph* graph);

/* ---- 节点操作 ---- */

/**
 * @brief 添加点节点
 * @param graph 约束图
 * @param x X 坐标
 * @param y Y 坐标
 * @return 新节点的 ID，失败返回 -1
 */
LV00_API int lv00_graph_add_point(Lv00ConstraintGraph* graph, 
                                   const Lv00SymbolicCoord* x, 
                                   const Lv00SymbolicCoord* y);

/**
 * @brief 添加线段节点
 * @param graph 约束图
 * @param p1_id 起点 ID
 * @param p2_id 终点 ID
 * @return 新节点的 ID，失败返回 -1
 */
LV00_API int lv00_graph_add_line_segment(Lv00ConstraintGraph* graph, int p1_id, int p2_id);

/**
 * @brief 添加端口节点
 * @param graph 约束图
 * @param parent_id 父节点 ID
 * @param direction 端口方向
 * @param x 相对 X 坐标
 * @param y 相对 Y 坐标
 * @return 新节点的 ID，失败返回 -1
 */
LV00_API int lv00_graph_add_port(Lv00ConstraintGraph* graph, int parent_id,
                                  Lv00PortDirection direction,
                                  const Lv00SymbolicCoord* x,
                                  const Lv00SymbolicCoord* y);

/**
 * @brief 添加函数块节点
 * @param graph 约束图
 * @param internal_nodes 内部节点 ID 数组
 * @param internal_count 内部节点数量
 * @param input_ports 输入端口 ID 数组
 * @param input_count 输入端口数量
 * @param output_ports 输出端口 ID 数组
 * @param output_count 输出端口数量
 * @return 新节点的 ID，失败返回 -1
 */
LV00_API int lv00_graph_add_function_block(Lv00ConstraintGraph* graph,
                                            const int* internal_nodes, size_t internal_count,
                                            const int* input_ports, size_t input_count,
                                            const int* output_ports, size_t output_count);

/**
 * @brief 移除节点
 * @param graph 约束图
 * @param node_id 节点 ID
 * @return 状态码
 */
LV00_API Lv00Status lv00_graph_remove_node(Lv00ConstraintGraph* graph, int node_id);

/**
 * @brief 根据 ID 获取节点
 * @param graph 约束图
 * @param node_id 节点 ID
 * @return 节点指针，不存在返回 NULL
 */
LV00_API Lv00GeomNode* lv00_graph_get_node(const Lv00ConstraintGraph* graph, int node_id);

/**
 * @brief 获取节点数量
 */
LV00_API size_t lv00_graph_get_node_count(const Lv00ConstraintGraph* graph);

/* ---- 约束操作 ---- */

/**
 * @brief 添加关联约束
 * @param graph 约束图
 * @param point_id 点 ID
 * @param target_id 目标（线段或区域）ID
 * @return 新约束的 ID，失败返回 -1
 */
LV00_API int lv00_graph_add_incidence(Lv00ConstraintGraph* graph, int point_id, int target_id);

/**
 * @brief 添加介于约束
 * @param graph 约束图
 * @param a 点 A ID
 * @param b 点 B ID（中间点）
 * @param c 点 C ID
 * @return 新约束的 ID，失败返回 -1
 */
LV00_API int lv00_graph_add_betweenness(Lv00ConstraintGraph* graph, int a, int b, int c);

/**
 * @brief 添加相交约束
 * @param graph 约束图
 * @param line1_id 线段 1 ID
 * @param line2_id 线段 2 ID
 * @param result_point_id 交点 ID
 * @return 新约束的 ID，失败返回 -1
 */
LV00_API int lv00_graph_add_intersection(Lv00ConstraintGraph* graph, int line1_id, int line2_id, int result_point_id);

/**
 * @brief 添加包含约束
 * @param graph 约束图
 * @param inner_id 内部区域 ID
 * @param outer_id 外部区域 ID
 * @return 新约束的 ID，失败返回 -1
 */
LV00_API int lv00_graph_add_containment(Lv00ConstraintGraph* graph, int inner_id, int outer_id);

/**
 * @brief 添加连接约束
 * @param graph 约束图
 * @param src_port_id 源端口 ID
 * @param dst_port_id 目标端口 ID
 * @return 新约束的 ID，失败返回 -1
 */
LV00_API int lv00_graph_add_connection(Lv00ConstraintGraph* graph, int src_port_id, int dst_port_id);

/**
 * @brief 添加距离约束
 * @param graph 约束图
 * @param p1_id 点 1 ID
 * @param p2_id 点 2 ID
 * @param distance 距离值
 * @return 新约束的 ID，失败返回 -1
 */
LV00_API int lv00_graph_add_distance(Lv00ConstraintGraph* graph, int p1_id, int p2_id, 
                                      const Lv00SymbolicCoord* distance);

/**
 * @brief 移除约束
 * @param graph 约束图
 * @param constraint_id 约束 ID
 * @return 状态码
 */
LV00_API Lv00Status lv00_graph_remove_constraint(Lv00ConstraintGraph* graph, int constraint_id);

/**
 * @brief 获取约束数量
 */
LV00_API size_t lv00_graph_get_constraint_count(const Lv00ConstraintGraph* graph);

/* =============================================================
 * 图分析 / Graph Analysis
 * ============================================================= */

/**
 * @brief 规范化结果结构体
 */
typedef struct {
    bool success;           /**< 是否成功 */
    int step_count;         /**< 执行的步数 */
    char* message;          /**< 结果消息（调用者释放） */
} Lv00NormalizationResult;

/**
 * @brief 规范化约束图
 * @param graph 约束图
 * @param scope_aware 是否考虑作用域
 * @return 规范化结果
 */
LV00_API Lv00NormalizationResult lv00_graph_normalize(Lv00ConstraintGraph* graph, bool scope_aware);

/**
 * @brief 释放规范化结果
 * @param result 规范化结果
 */
LV00_API void lv00_normalization_result_free(Lv00NormalizationResult* result);

/**
 * @brief 合并候选节点对
 */
typedef struct {
    int node_a;     /**< 节点 A ID */
    int node_b;     /**< 节点 B ID */
    double distance; /**< 距离 */
} Lv00MergeCandidate;

/**
 * @brief 查找可合并的节点对
 * @param graph 约束图
 * @param threshold 距离阈值
 * @param count 输出候选数量
 * @return 候选数组（调用者释放），无候选返回 NULL
 */
LV00_API Lv00MergeCandidate* lv00_graph_find_merge_candidates(
    const Lv00ConstraintGraph* graph, 
    double threshold, 
    size_t* count
);

/**
 * @brief 释放合并候选数组
 */
LV00_API void lv00_merge_candidates_free(Lv00MergeCandidate* candidates);

/**
 * @brief 检测冗余约束
 * @param graph 约束图
 * @param count 输出冗余约束数量
 * @return 冗余约束 ID 数组（调用者释放）
 */
LV00_API int* lv00_graph_detect_redundant(const Lv00ConstraintGraph* graph, size_t* count);

/**
 * @brief 检测约束冲突
 */
typedef struct {
    int constraint1;    /**< 约束 1 ID */
    int constraint2;    /**< 约束 2 ID */
    char* reason;       /**< 冲突原因（调用者释放） */
} Lv00Conflict;

/**
 * @brief 检测约束冲突
 * @param graph 约束图
 * @param count 输出冲突数量
 * @return 冲突数组（调用者释放）
 */
LV00_API Lv00Conflict* lv00_graph_detect_conflicts(const Lv00ConstraintGraph* graph, size_t* count);

/**
 * @brief 释放冲突数组
 */
LV00_API void lv00_conflicts_free(Lv00Conflict* conflicts, size_t count);

/**
 * @brief 计算自由度
 * @param graph 约束图
 * @return 自由度数值
 */
LV00_API int lv00_graph_degrees_of_freedom(const Lv00ConstraintGraph* graph);

/**
 * @brief 拓扑排序约束
 * @param graph 约束图
 * @param count 输出排序后的约束数量
 * @return 排序后的约束 ID 数组（调用者释放）
 */
LV00_API int* lv00_graph_topological_sort(const Lv00ConstraintGraph* graph, size_t* count);

/**
 * @brief 计算图哈希
 * @param graph 约束图
 * @return 哈希字符串（调用者释放）
 */
LV00_API char* lv00_graph_hash(const Lv00ConstraintGraph* graph);

/* =============================================================
 * 图同构检测 / Graph Isomorphism
 * ============================================================= */

typedef struct Lv00GraphMatcher Lv00GraphMatcher;

/**
 * @brief 创建图匹配器
 */
LV00_API Lv00GraphMatcher* lv00_matcher_create(void);

/**
 * @brief 销毁图匹配器
 */
LV00_API void lv00_matcher_destroy(Lv00GraphMatcher* matcher);

/**
 * @brief 检查两个图是否同构
 * @param matcher 图匹配器
 * @param graph1 图 1
 * @param graph2 图 2
 * @return 是否同构
 */
LV00_API bool lv00_matcher_is_isomorphic(Lv00GraphMatcher* matcher,
                                          const Lv00ConstraintGraph* graph1,
                                          const Lv00ConstraintGraph* graph2);

/**
 * @brief 查找图同构映射
 * @param matcher 图匹配器
 * @param pattern 模式图
 * @param target 目标图
 * @param mapping_count 输出映射数量
 * @return 节点 ID 映射数组（调用者释放）
 */
LV00_API int* lv00_matcher_find_mapping(Lv00GraphMatcher* matcher,
                                         const Lv00ConstraintGraph* pattern,
                                         const Lv00ConstraintGraph* target,
                                         size_t* mapping_count);

/* =============================================================
 * 求解引擎 / Solver Engine
 * ============================================================= */

typedef struct Lv00Solver Lv00Solver;

/**
 * @brief 求解器配置
 */
typedef struct {
    int max_iterations;     /**< 最大迭代次数 */
    double tolerance;       /**< 收敛容差 */
    bool use_symbolic;      /**< 使用符号计算 */
    int timeout_ms;         /**< 超时时间（毫秒） */
} Lv00SolverConfig;

/**
 * @brief 创建求解器
 * @param config 求解器配置（NULL 使用默认配置）
 * @return 新创建的求解器，失败返回 NULL
 */
LV00_API Lv00Solver* lv00_solver_create(const Lv00SolverConfig* config);

/**
 * @brief 销毁求解器
 */
LV00_API void lv00_solver_destroy(Lv00Solver* solver);

/**
 * @brief 求解约束图
 * @param solver 求解器
 * @param graph 约束图
 * @return 状态码
 */
LV00_API Lv00Status lv00_solver_solve(Lv00Solver* solver, Lv00ConstraintGraph* graph);

/**
 * @brief 获取求解器统计信息
 */
typedef struct {
    int iteration_count;        /**< 迭代次数 */
    double residual;            /**< 残差 */
    int constraint_satisfied;   /**< 满足的约束数 */
    int constraint_total;       /**< 总约束数 */
} Lv00SolverStats;

/**
 * @brief 获取求解器统计
 */
LV00_API Lv00SolverStats lv00_solver_get_stats(const Lv00Solver* solver);

/* =============================================================
 * 重写系统 / Rewrite System
 * ============================================================= */

typedef struct Lv00RewriteRule Lv00RewriteRule;
typedef struct Lv00RewriteEngine Lv00RewriteEngine;

/**
 * @brief 从字符串创建重写规则
 * @param pattern 模式字符串
 * @param replacement 替换字符串
 * @return 新创建的重写规则，失败返回 NULL
 */
LV00_API Lv00RewriteRule* lv00_rewrite_rule_create(const char* pattern, const char* replacement);

/**
 * @brief 销毁重写规则
 */
LV00_API void lv00_rewrite_rule_destroy(Lv00RewriteRule* rule);

/**
 * @brief 创建重写引擎
 */
LV00_API Lv00RewriteEngine* lv00_rewrite_engine_create(void);

/**
 * @brief 销毁重写引擎
 */
LV00_API void lv00_rewrite_engine_destroy(Lv00RewriteEngine* engine);

/**
 * @brief 添加重写规则
 */
LV00_API bool lv00_rewrite_engine_add_rule(Lv00RewriteEngine* engine, Lv00RewriteRule* rule);

/**
 * @brief 应用重写
 * @param engine 重写引擎
 * @param graph 约束图
 * @param max_steps 最大步数
 * @return 实际执行的步数，负数表示错误
 */
LV00_API int lv00_rewrite_engine_apply(Lv00RewriteEngine* engine, 
                                        Lv00ConstraintGraph* graph,
                                        int max_steps);

/* =============================================================
 * 证明验证 / Proof Verification
 * ============================================================= */

/**
 * @brief 合一结果枚举
 */
typedef enum {
    UNIFY_OK = 0,               /**< 合一成功 */
    UNIFY_FAILED = 1,           /**< 合一失败 */
    UNIFY_TYPE_MISMATCH = 2,    /**< 类型不匹配 */
    UNIFY_TIMEOUT = 3,          /**< 超时 */
} Lv00UnifyResult;

/**
 * @brief 执行合一检查
 * @param construction 构造图
 * @param proposition 命题模式图
 * @param strict 是否严格模式
 * @return 合一结果
 */
LV00_API Lv00UnifyResult lv00_proof_unify(const Lv00ConstraintGraph* construction,
                                           const Lv00ConstraintGraph* proposition,
                                           bool strict);

/**
 * @brief 验证证明
 * @param proof_graph 证明图
 * @param theorem_graph 定理图
 * @return 是否验证通过
 */
LV00_API bool lv00_proof_verify(const Lv00ConstraintGraph* proof_graph,
                                 const Lv00ConstraintGraph* theorem_graph);

/* =============================================================
 * 内存管理 / Memory Management
 * ============================================================= */

/**
 * @brief 释放指针（通用释放函数）
 * @param ptr 要释放的指针
 */
LV00_API void lv00_free(void* ptr);

/**
 * @brief 设置自定义内存分配器
 * @param malloc_fn 自定义 malloc 函数
 * @param free_fn 自定义 free 函数
 * @param realloc_fn 自定义 realloc 函数（可为 NULL）
 */
LV00_API void lv00_set_allocator(void* (*malloc_fn)(size_t),
                                  void (*free_fn)(void*),
                                  void* (*realloc_fn)(void*, size_t));

/* =============================================================
 * 错误处理 / Error Handling
 * ============================================================= */

/**
 * @brief 获取最后错误码
 */
LV00_API Lv00Status lv00_get_last_error(void);

/**
 * @brief 获取最后错误消息
 * @return 错误消息字符串（调用者释放）
 */
LV00_API char* lv00_get_last_error_message(void);

/**
 * @brief 清除错误状态
 */
LV00_API void lv00_clear_error(void);

/**
 * @brief 设置错误回调
 * @param callback 错误回调函数
 * @param user_data 用户数据
 */
LV00_API void lv00_set_error_callback(void (*callback)(Lv00Status, const char*, void*),
                                       void* user_data);

/* =============================================================
 * 日志系统 / Logging
 * ============================================================= */

/**
 * @brief 日志级别枚举
 */
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO = 1,
    LOG_LEVEL_WARN = 2,
    LOG_LEVEL_ERROR = 3,
} Lv00LogLevel;

/**
 * @brief 设置日志级别
 */
LV00_API void lv00_log_set_level(Lv00LogLevel level);

/**
 * @brief 设置日志输出回调
 */
LV00_API void lv00_log_set_callback(void (*callback)(Lv00LogLevel, const char*, void*),
                                     void* user_data);

/**
 * @brief 写入日志
 */
LV00_API void lv00_log(Lv00LogLevel level, const char* format, ...);

/* =============================================================
 * 初始化与清理 / Initialization & Cleanup
 * ============================================================= */

/**
 * @brief 初始化 Lv-00 库
 * @return 是否成功
 */
LV00_API bool lv00_init(void);

/**
 * @brief 清理 Lv-00 库
 */
LV00_API void lv00_cleanup(void);

/**
 * @brief 获取版本字符串
 * @return 版本字符串（静态内存，无需释放）
 */
LV00_API const char* lv00_get_version_string(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_OPTIMIZED_H */
