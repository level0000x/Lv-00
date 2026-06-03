/* ========================================================================
 * 模块名称：递归与条件系统 (recursion)
 * 功能概述：根据 Lv-00 设计文档第9节实现递归构造系统，包含测度系统、
 *          选择器块、递归深度监控、互递归支持和全局熔断器。
 *          测度系统支持符号测度（长度/面积/角度/深度）和非符号测度，
 *          用于验证递归的良基性（终止性）。
 *
 * 主要 API：
 *   - measure_system_create / destroy / add  — 测度系统管理
 *   - measure_create_symbolic / custom       — 创建测度
 *   - measure_compute_value / compare        — 计算和比较测度
 *   - recursion_context_create / enter / exit — 递归上下文
 *   - recursion_context_check_decreasing     — 验证测度递减性
 *   - lv00_recursion_enter / leave / reset   — 全局深度保护（熔断器）
 *   - selector_block_create / evaluate       — 选择器块
 *   - recursion_check_mutual                 — 互递归验证
 *
 * 使用示例：
 *   RecursionContext *ctx = recursion_context_create(10000);
 *   Measure *m = measure_create_symbolic("depth", MEASURE_KIND_DEPTH, -1);
 *   recursion_context_set_measure(ctx, m);
 *   RecursionCheckResult r = recursion_context_enter(ctx, fb_id, input, graph);
 *
 * ======================================================================== */

/**
 * @file recursion.h
 * @brief 递归与条件系统 - 测度系统、选择器块、递归深度监控
 */

#ifndef LV00_RECURSION_H
#define LV00_RECURSION_H

#include <stdbool.h>

#include "constraint_graph.h"
#include "stream.h"
#include "symbolic_coord.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============== 递归深度保护常量 ============== */

/**
 * @brief 全局递归深度上限 —— 防止无限递归导致栈溢出
 *
 * 比上下文级 max_depth (10000) 更严格的全局硬限制。
 * 当任何递归调用深度超过此值时，触发熔断机制（circuit breaker），
 * 自动终止当前递归链。默认值 128，足够覆盖绝大多数合法递归场景。
 *
 * 与 LV00_MAX_RECURSION_DEPTH_LIMIT (100000, 定义在 lv00_internal.h) 的关系：
 * - LV00_MAX_RECURSION_DEPTH (128): 全局熔断阈值，由 lv00_recursion_enter/leave 管理
 * - LV00_MAX_RECURSION_DEPTH_LIMIT (100000): 单个 RecursionContext 的硬上限
 */
#define LV00_MAX_RECURSION_DEPTH 128

void recursion_set_stream_context(StreamContext *ctx);

/* ============== 前向声明 ============== */
typedef struct Measure Measure;
typedef struct MeasureSystem MeasureSystem;
typedef struct SelectorBlock SelectorBlock;
typedef struct RecursionContext RecursionContext;

/* ============== 测度类型 ==============
 * 【枚举值命名规范】所有枚举值使用 UPPER_SNAKE_CASE
 */
typedef enum {
    MEASURE_TYPE_SYMBOLIC, /**< 符号测度：可归约到符号坐标上的代数表达式 */
    MEASURE_TYPE_CUSTOM    /**< 非符号测度：公理包定义的抽象序结构 */
} MeasureType;

/* 向后兼容别名：保留旧测试与外部调用方使用的测度枚举名称 */
#ifndef MEASURE_SYMBOLIC
#define MEASURE_SYMBOLIC MEASURE_TYPE_SYMBOLIC
#endif
#ifndef MEASURE_CUSTOM
#define MEASURE_CUSTOM MEASURE_TYPE_CUSTOM
#endif

/* ============== 测度比较结果 ==============
 * 【枚举值命名规范】所有枚举值使用 UPPER_SNAKE_CASE
 */
typedef enum {
    MEASURE_COMPARE_LESS,    /**< a < b */
    MEASURE_COMPARE_EQUAL,   /**< a = b */
    MEASURE_COMPARE_GREATER, /**< a > b */
    MEASURE_COMPARE_UNKNOWN, /**< 无法比较 */
    MEASURE_COMPARE_ERROR    /**< 比较出错 */
} MeasureCompareResult;

/* ============== 测度定义 ============== */
/**
 * @brief 测度 —— 定义递归终止判定的度量方式
 *
 * 每个测度包含名称、类型（符号/自定义）、参考节点等信息。
 * 符号测度基于几何实体的直接度量（如长度、角度、面积、深度），
 * 自定义测度使用用户提供的比较函数进行非几何递归终止判定。
 */
struct Measure {
    int id;           /* 测度ID */
    MeasureType type; /* 测度类型 */
    char *name;       /* 测度名称 */

    /* 符号测度 —— 基于几何实体的直接度量 */
    int reference_node_id; /* 参考节点 ID */

    /** @brief 测度种类枚举
     *  描述符号测度的具体归类类型，决定 compare 语义。 */
    enum {
        MEASURE_KIND_LENGTH, /* 线段长度 —— 端点间的欧氏距离 */
        MEASURE_KIND_AREA,   /* 区域面积 —— 封闭几何图形的面积 */
        MEASURE_KIND_ANGLE,  /* 角度 —— 两条线段之间的夹角 */
        MEASURE_KIND_DEPTH,  /* 嵌套深度 —— 几何体在构造树中的层级 */
        MEASURE_KIND_CUSTOM  /* 自定义 —— 用户定义的测度函数 */
    } kind;

    /* 非符号测度 */
    int (*compare_func)(GeomNode *a, GeomNode *b, void *user_data);
    void *user_data;

    /* 良基关系 */
    bool is_well_founded; /* 是否为良基关系 */
};

/* ============== 非符号测度元数据（修改6） ============== */

/**
 * @brief 非符号测度比较器类型
 * @param a 第一个符号坐标
 * @param b 第二个符号坐标
 * @return true 表示 a < b
 */
typedef bool (*NonSymbolicComparator)(const SymbolicCoord *a, const SymbolicCoord *b);

/**
 * @brief 非符号测度元数据结构
 */
typedef struct {
    int measure_type_id;              /* 测度类型ID */
    NonSymbolicComparator comparator; /* 比较函数 */
    bool is_well_founded;             /* 是否为良基关系 */
} NonSymbolicMeasureMeta;

/**
 * @brief 非符号测度验证模板元数据（Feature 2）
 * 用于将非符号测度与公理包模板展开关联
 */
typedef struct {
    int measure_id;                /* 测度ID */
    char validation_template[128]; /* 验证模板名称 */
} NonSymbolicMeasureValidationMeta;

/* ============== 测度系统 ============== */
/**
 * @brief 测度系统 —— 管理递归终止条件的测度集合
 *
 * 测度系统包含多个测度（Measure），每个测度定义了一种递归终止判定方式。
 * 支持符号测度（基于几何性质）和自定义测度（用户提供的比较函数）。
 * 每次递归调用必须使至少一个测度值严格递减，以保证递归终止。
 */
struct MeasureSystem {
    Measure **measures;   /* 测度数组 */
    int measure_count;    /* 测度数量 */
    int measure_capacity; /* 测度数组容量（用于指数增长策略） */

    /* 默认测度 */
    Measure *default_measure; /* 默认测度 */

    /* 非符号测度提示 */
    bool has_non_symbolic; /* 是否包含非符号测度 */

    /* 非符号测度元数据（修改6） */
    NonSymbolicMeasureMeta *non_symbolic_metas; /* 非符号测度元数据数组 */
    int non_symbolic_meta_count;                /* 非符号测度元数据数量 */

    /* 非符号测度验证模板（Feature 2） */
    NonSymbolicMeasureValidationMeta *validation_metas; /* 验证模板元数据数组 */
    int validation_meta_count;                          /* 验证模板元数据数量 */
};

/* ============== 选择器块分支状态 ============== */
typedef enum {
    BRANCH_INACTIVE, /* 不活跃（灰色虚影） */
    BRANCH_ACTIVE_SELECTED,   /* 活跃（实线） */
    BRANCH_PENDING,  /* 待定（半透明） */
    BRANCH_SHADOWED  /* 虚影状态（被遮蔽的分支） */
} BranchState;

/* ============== 选择器块 ============== */
struct SelectorBlock {
    int id; /* 选择器块ID */

    /* 判定条件 */
    int test_point_id;  /* 测试点ID */
    int test_region_id; /* 测试区域ID */

    /* 分支 */
    int true_branch_root_id;  /* 真分支根节点ID */
    int false_branch_root_id; /* 假分支根节点ID */

    BranchState true_state;  /* 真分支状态 */
    BranchState false_state; /* 假分支状态 */

    /* 分支子图节点管理（修改3） */
    int *true_branch_node_ids;   /* 真分支子图的节点ID数组 */
    int true_branch_node_count;  /* 真分支节点数量 */
    int *false_branch_node_ids;  /* 假分支子图的节点ID数组 */
    int false_branch_node_count; /* 假分支节点数量 */

    /* 约束图引用 */
    ConstraintGraph *graph; /* 所属约束图 */
};

/* ============== 递归深度超限回调（修改5） ============== */

/**
 * @brief 递归深度超限时的动作
 * 【枚举值命名规范】所有枚举值使用 UPPER_SNAKE_CASE
 */
typedef enum {
    RECURSION_DEPTH_ACTION_CONTINUE, /**< 继续执行 */
    RECURSION_DEPTH_ACTION_STOP      /**< 停止递归 */
} RecursionAction;

/**
 * @brief 递归深度超限回调函数类型
 * @param current_depth 当前深度
 * @param max_depth 最大深度
 * @param user_data 用户数据
 * @return 是否继续执行
 */
typedef RecursionAction (*RecursionDepthCallback)(int current_depth, int max_depth, void *user_data);

/* ============== 递归上下文 ============== */
struct RecursionContext {
    int current_depth; /* 当前递归深度 */
    int max_depth;     /* 最大递归深度（默认10000） */

    Measure *active_measure;        /* 活动测度 */
    SymbolicCoord **measure_values; /* 测度值历史 */
    int measure_value_count;        /* 测度值数量 */

    /* 递归调用栈 */
    int *call_stack;     /* 调用栈（函数块ID） */
    int call_stack_size; /* 调用栈大小 */

    /* 状态 */
    bool is_terminated;       /* 是否已终止 */
    char *termination_reason; /* 终止原因 */

    /* 深度超限回调（修改5） */
    RecursionDepthCallback depth_callback; /* 深度超限回调函数 */
    void *depth_callback_user_data;        /* 回调用户数据 */
};

/* ============== 递归检查结果 ==============
 * 【枚举值命名规范】所有枚举值使用 UPPER_SNAKE_CASE
 */
typedef enum {
    RECURSION_CHECK_RESULT_OK,              /**< 递归有效 */
    RECURSION_CHECK_RESULT_NOT_DECREASING,  /**< 测度未递减 */
    RECURSION_CHECK_RESULT_DEPTH_EXCEEDED,  /**< 深度超限 */
    RECURSION_CHECK_RESULT_CYCLE_DETECTED,  /**< 检测到循环 */
    RECURSION_CHECK_RESULT_MEASURE_UNKNOWN, /**< 测度未知 */
    RECURSION_CHECK_RESULT_ERROR            /**< 检查出错 */
} RecursionCheckResult;

/* ============== 测度系统API ============== */

/**
 * 创建测度系统
 */
MeasureSystem *measure_system_create(void);

/**
 * 销毁测度系统
 */
void measure_system_destroy(MeasureSystem *ms);

/**
 * 创建符号测度
 */
Measure *measure_create_symbolic(const char *name, int kind, int ref_node_id);

/**
 * 创建非符号测度
 */
Measure *measure_create_custom(const char *name, int (*compare_func)(GeomNode *a, GeomNode *b, void *user_data),
                               void *user_data);

/**
 * 销毁测度
 */
void measure_destroy(Measure *m);

/**
 * 添加测度到系统
 */
bool measure_system_add(MeasureSystem *ms, Measure *m);

/**
 * 设置默认测度
 */
void measure_system_set_default(MeasureSystem *ms, Measure *m);

/**
 * 计算节点测度值
 */
SymbolicCoord *measure_compute_value(Measure *m, GeomNode *node, ConstraintGraph *graph);

/**
 * 纯符号计算节点测度值（修改4：面积测度使用符号运算）
 * @param m 测度
 * @param node 几何节点
 * @param graph 约束图
 * @return 符号坐标表示的测度值，失败返回 NULL
 */
SymbolicCoord *measure_compute_value_symbolic(Measure *m, GeomNode *node, ConstraintGraph *graph);

/**
 * 比较两个测度值
 */
MeasureCompareResult measure_compare(Measure *m, SymbolicCoord *a, SymbolicCoord *b);

/**
 * 比较两个节点的测度
 */
MeasureCompareResult measure_compare_nodes(Measure *m, GeomNode *a, GeomNode *b, const ConstraintGraph *graph);

/* ============== 递归上下文API ============== */

/**
 * 创建递归上下文
 */
RecursionContext *recursion_context_create(int max_depth);

/**
 * 销毁递归上下文
 */
void recursion_context_destroy(RecursionContext *ctx);

/**
 * 设置活动测度
 */
void recursion_context_set_measure(RecursionContext *ctx, Measure *m);

/**
 * 进入递归调用
 */
RecursionCheckResult recursion_context_enter(RecursionContext *ctx, int func_block_id, const GeomNode *input,
                                             ConstraintGraph *graph);

/**
 * 退出递归调用
 */
void recursion_context_exit(RecursionContext *ctx);

/**
 * 检查测度递减性（修改1：验证整条调用链的单调递减）
 * 遍历整个 measure_values 数组，验证严格单调递减
 */
RecursionCheckResult recursion_context_check_decreasing(const RecursionContext *ctx, SymbolicCoord *new_value);

/**
 * 获取当前深度
 */
int recursion_context_get_depth(RecursionContext *ctx);

/**
 * 重置递归上下文
 */
void recursion_context_reset(RecursionContext *ctx);

/**
 * 注册深度超限回调（修改5）
 * @param ctx 递归上下文
 * @param callback 回调函数
 * @param user_data 用户数据
 */
void recursion_context_set_depth_callback(RecursionContext *ctx, RecursionDepthCallback callback, void *user_data);

/* ============== 全局递归深度保护（熔断器） ============== */

/**
 * @brief 进入递归调用 —— 全局深度保护入口
 *
 * 维护一个全局（线程局部）的递归深度计数器。
 * 每次递归调用前调用此函数：
 * - 深度 +1
 * - 如果 depth > LV00_MAX_RECURSION_DEPTH，触发熔断器（circuit breaker）
 *
 * 熔断器机制：
 * - 自动将全局 circuit_breaker_triggered 标志设为 true
 * - 后续所有 lv00_recursion_enter() 调用都会返回 false
 * - 必须调用 lv00_recursion_reset() 才能恢复
 *
 * 与 recursion_context_enter() 的区别：
 * - lv00_recursion_enter/leave 是轻量级全局保护，无上下文依赖
 * - recursion_context_enter/exit 是上下文相关的完整测度验证
 *
 * @return true  进入成功（深度在安全范围内）
 * @return false 熔断器已触发或深度超限
 */
bool lv00_recursion_enter(void);

/**
 * @brief 退出递归调用 —— 全局深度保护出口
 *
 * 全局递归深度计数器 -1。
 * 每次递归调用返回后调用此函数。
 * 如果当前深度回到 0 且熔断器未被触发，自动重置熔断器状态。
 */
void lv00_recursion_leave(void);

/**
 * @brief 检查熔断器是否已触发
 *
 * @return true  熔断器已触发（应停止当前递归链）
 * @return false 熔断器未触发（可以继续递归）
 */
bool lv00_recursion_circuit_breaker_triggered(void);

/**
 * @brief 重置全局递归深度保护状态
 *
 * 将深度计数器置零，清除熔断器标志。
 * 应在开始新的递归链之前调用。
 */
void lv00_recursion_reset(void);

/**
 * @brief 获取当前全局递归深度
 *
 * @return 当前递归深度（0 = 未在递归中）
 */
int lv00_recursion_get_depth(void);

/* ============== 选择器块API ============== */

/**
 * 创建选择器块
 */
SelectorBlock *selector_block_create(int id, ConstraintGraph *graph);

/**
 * 销毁选择器块
 */
void selector_block_destroy(SelectorBlock *sb);

/**
 * 设置测试条件
 */
bool selector_block_set_condition(SelectorBlock *sb, int point_id, int region_id);

/**
 * 设置分支
 */
bool selector_block_set_branches(SelectorBlock *sb, int true_root, int false_root);

/**
 * 设置选择器块的分支子图（修改3）
 * @param sb 选择器块
 * @param true_ids 真分支子图的节点ID数组
 * @param true_count 真分支节点数量
 * @param false_ids 假分支子图的节点ID数组
 * @param false_count 假分支节点数量
 */
void selector_block_set_branch_nodes(SelectorBlock *sb, int *true_ids, int true_count, int *false_ids, int false_count);

/**
 * 获取指定分支的节点列表（修改3）
 * @param sb 选择器块
 * @param is_true_branch 是否为真分支
 * @param out_count 输出节点数量
 * @return 节点ID数组（只读），失败返回 NULL
 */
const int *selector_block_get_branch_nodes(SelectorBlock *sb, bool is_true_branch, int *out_count);

/**
 * 评估选择器块
 */
bool selector_block_evaluate(SelectorBlock *sb, ConstraintGraph *graph);

/**
 * 获取活跃分支
 */
int selector_block_get_active_branch(SelectorBlock *sb);

/**
 * 更新分支状态
 */
void selector_block_update_states(SelectorBlock *sb, BranchState true_state, BranchState false_state);

/**
 * 符号测度验证
 * 计算给定节点的测度值，与上下文中的前一个测度值比较
 * @param ctx 递归上下文
 * @param measure 测度定义
 * @param graph 约束图
 * @param node_id 要计算测度的节点ID
 * @return RECURSION_CHECK_RESULT_OK 严格递减，RECURSION_CHECK_RESULT_NOT_DECREASING 未递减，RECURSION_CHECK_RESULT_ERROR 出错
 */
RecursionCheckResult recursion_validate_measure(const RecursionContext *ctx, const Measure *measure,
                                                const ConstraintGraph *graph, int node_id);

/**
 * 统计选择器块各分支的节点数量
 * @param sb 选择器块
 * @param out_true_count 输出真分支节点数量
 * @param out_false_count 输出假分支节点数量
 * @return 0 成功，-1 失败
 */
int selector_block_count_branch_nodes(const SelectorBlock *sb, int *out_true_count, int *out_false_count);

/**
 * 验证选择器块的两个分支是否互斥
 * 检查真分支和假分支的节点ID集合是否有交集
 * @param sb 选择器块
 * @return true 互斥（无交集），false 不互斥或有错误
 */
bool selector_block_validate_branches(const SelectorBlock *sb);

/* ============== 互递归支持 ============== */

/**
 * 检查互递归测度一致性（修改2：完整测度验证）
 * @param func_ids 互递归函数块ID数组
 * @param count 函数块数量
 * @param ms 测度系统
 * @return 是否一致
 */
bool recursion_check_mutual(int *func_ids, int count, MeasureSystem *ms);

/**
 * 检查互递归测度一致性（修改2：完整测度验证，带上下文）
 * 验证两个递归上下文在同一个全局测度下各自递减，且合并后交叉递减
 * @param ctx_a 第一个函数块的递归上下文
 * @param ctx_b 第二个函数块的递归上下文
 * @return 是否通过验证
 */
bool recursion_check_mutual_with_contexts(RecursionContext *ctx_a, RecursionContext *ctx_b);

/* ============== 非符号测度验证（修改6） ============== */

/**
 * 注册非符号测度元数据
 * @param ms 测度系统
 * @param measure_type_id 测度类型ID
 * @param comparator 比较函数
 * @param is_well_founded 是否为良基关系
 * @return 注册是否成功
 */
bool measure_system_register_non_symbolic(MeasureSystem *ms, int measure_type_id, NonSymbolicComparator comparator,
                                          bool is_well_founded);

/**
 * 验证所有已注册的非符号测度
 * @param ms 测度系统
 * @return 是否全部验证通过
 */
bool measure_system_validate_non_symbolic(MeasureSystem *ms);

/* ============== 非符号测度的模板展开机制 ============== */

/**
 * @brief 非符号测度验证——通过模板展开验证递减性
 *
 * 当测度为非符号类型时，通过公理包提供的模板展开
 * 验证测度递减性。
 * @param measure 测度定义
 * @param before_value 递归前的测度值
 * @param after_value 递归后的测度值
 * @param comparator 非符号测度比较器（由公理包提供）
 * @return RECURSION_CHECK_RESULT_OK 递减，RECURSION_CHECK_RESULT_NOT_DECREASING 未递减，RECURSION_CHECK_RESULT_MEASURE_UNKNOWN 未知
 */
RecursionCheckResult recursion_validate_non_symbolic_measure(const Measure *measure, SymbolicCoord *before_value,
                                                             SymbolicCoord *after_value,
                                                             NonSymbolicComparator comparator);

/* ============== 递归内置测试结果 ============== */

/**
 * @brief 递归模块内置测试的结果
 */
typedef struct {
    char name[64];       /* 测试名称 */
    bool passed;         /* 是否通过 */
    char error_msg[128]; /* 错误信息（passed为false时有效） */
} RecursionTestResult;

/* ============== 加载时验证的完整测试集 ============== */

/**
 * @brief 测度验证测试结果
 */
typedef struct {
    bool passed;         /* 是否通过 */
    char *test_name;     /* 测试名称 */
    char *error_message; /* 错误信息（passed为false时有效） */
} MeasureTestResult;

/**
 * @brief 运行测度验证测试集
 *
 * 在公理包加载时，运行包含测度递减性验证用例的测试集。
 * @param measure 测度定义
 * @param test_count 测试用例数量
 * @param test_before_values 递归前测度值数组（每个元素为 SymbolicCoord* 数组）
 * @param test_after_values 递归后测度值数组（每个元素为 SymbolicCoord* 数组）
 * @param results 测试结果输出数组（由调用者分配，至少 test_count 个元素）
 * @return 是否全部通过
 */
bool recursion_run_measure_tests(const Measure *measure, int test_count, SymbolicCoord ***test_before_values,
                                 SymbolicCoord ***test_after_values, MeasureTestResult *results);

/* ============== 辅助函数 ============== */

/**
 * 测度类型转字符串
 */
const char *measure_type_to_string(MeasureType type);

/**
 * 测度比较结果转字符串
 */
const char *measure_compare_result_to_string(MeasureCompareResult result);

/**
 * 递归检查结果转字符串
 */
const char *recursion_check_result_to_string(RecursionCheckResult result);

/**
 * 分支状态转字符串
 */
const char *branch_state_to_string(BranchState state);

/* ============== Feature 1: 内置测试套件 ============== */

/**
 * @brief 运行递归模块的内置测试套件
 *
 * 在模块加载时运行综合测试，验证测度系统、递归深度限制、
 * 互递归、选择器块和角度测度等核心功能。
 * @param sys 测度系统（如果为NULL则创建临时系统）
 * @param results 输出：测试结果数组（由调用者释放，free即可）
 * @param result_count 输出：测试结果数量
 * @return 通过的测试数量（负数表示致命错误）
 */
int recursion_run_builtin_tests(MeasureSystem *sys, RecursionTestResult **results, int *result_count);

/* ============== Feature 2: 非符号测度模板展开集成 ============== */

/**
 * @brief 使用公理包模板展开验证非符号测度
 *
 * 将非符号测度与公理包模板展开关联，建立验证模板。
 * @param sys 测度系统
 * @param measure_id 测度ID
 * @param axiom_template_name 验证模板名称
 * @param axiom_pkg AxiomPackage指针（不透明指针，避免循环包含）
 * @return 0 成功，-1 测度未找到或非非符号测度
 */
int recursion_validate_non_symbolic_with_axiom(MeasureSystem *sys, int measure_id, const char *axiom_template_name,
                                               void *axiom_pkg);

/**
 * @brief 获取与非符号测度关联的公理模板名称
 *
 * @param sys 测度系统
 * @param measure_id 测度ID
 * @return 模板名称字符串（只读），未设置或测度为符号测度时返回NULL
 */
const char *recursion_get_measure_validation_template(MeasureSystem *sys, int measure_id);

#ifdef __cplusplus
}
#endif

#endif /* LV00_RECURSION_H */
