/**
 * @file solver_result_standard.h
 * @brief 统一求解结果标准 —— 多后端结果标准化、交叉验证与一致性检查
 *
 * @details 本模块定义 Lv-00 多求解后端（SAT、SMT、Groebner、BDD、ATP）的
 *          统一结果格式和精度标准，实现结果交叉验证和一致性检查机制。
 *
 *          主要功能：
 *          1. 统一求解结果格式（SolverUnifiedResult）
 *          2. 精度标准定义（数值容差、符号等价判定）
 *          3. 结果交叉验证机制
 *          4. 一致性检查与冲突解决
 *          5. 结果置信度评估
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#ifndef LV00_SOLVER_RESULT_STANDARD_H
#define LV00_SOLVER_RESULT_STANDARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "smt_backend.h"

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 数值比较默认绝对容差 */
#define SOLVER_RESULT_EPSILON_ABSOLUTE 1e-9

/** @brief 数值比较默认相对容差 */
#define SOLVER_RESULT_EPSILON_RELATIVE 1e-6

/** @brief 有理数比较默认分母上限 */
#define SOLVER_RESULT_MAX_DENOMINATOR 1000000

/** @brief 最大支持的后端数量 */
#define SOLVER_RESULT_MAX_BACKENDS 8

/** @brief 最大变量赋值数量 */
#define SOLVER_RESULT_MAX_ASSIGNMENTS 1024

/** @brief 结果验证失败时的最大冲突记录数 */
#define SOLVER_RESULT_MAX_CONFLICTS 64

/* ============================================================
 * 统一结果类型枚举（v4.1.0：统一使用 solver.h 中的 SolverStatus）
 * ============================================================ */

/* 统一使用 solver.h 中的 SolverStatus 枚举 */
#include "solver.h"

/* 兼容性别名：将旧 SolverResultStatus 枚举值映射到 SolverStatus */
#define SOLVER_RESULT_UNKNOWN      SOLVER_STATUS_UNKNOWN
#define SOLVER_RESULT_SAT          SOLVER_STATUS_OK
#define SOLVER_RESULT_UNSAT        SOLVER_STATUS_NO_SOLUTION
#define SOLVER_RESULT_PARTIAL      SOLVER_STATUS_PARTIAL
#define SOLVER_RESULT_TIMEOUT      SOLVER_STATUS_TIMEOUT
#define SOLVER_RESULT_ERROR        SOLVER_STATUS_ERROR
#define SOLVER_RESULT_INCONSISTENT SOLVER_STATUS_INCONSISTENT
#define SOLVER_RESULT_OUT_OF_SCOPE SOLVER_STATUS_OUT_OF_SCOPE

/* SolverResultStatus 作为 SolverStatus 的类型别名，保持向后兼容 */
typedef SolverStatus SolverResultStatus;

/**
 * @brief 结果置信度等级
 */
typedef enum {
    SOLVER_CONFIDENCE_NONE = 0,      /**< 无置信度 */
    SOLVER_CONFIDENCE_LOW = 1,       /**< 低置信度（单后端结果） */
    SOLVER_CONFIDENCE_MEDIUM = 2,    /**< 中等置信度（部分验证通过） */
    SOLVER_CONFIDENCE_HIGH = 3,      /**< 高置信度（多后端一致） */
    SOLVER_CONFIDENCE_CERTAIN = 4    /**< 确定性结果（证明生成） */
} SolverConfidenceLevel;

/**
 * @brief 变量值类型
 */
typedef enum {
    SOLVER_VALUE_BOOL = 0,       /**< 布尔值 */
    SOLVER_VALUE_INT = 1,        /**< 整数值 */
    SOLVER_VALUE_RATIONAL = 2,   /**< 有理数值 */
    SOLVER_VALUE_REAL = 3,       /**< 实数值（浮点） */
    SOLVER_VALUE_ALGEBRAIC = 4,  /**< 代数数（最小多项式表示） */
    SOLVER_VALUE_SYMBOLIC = 5    /**< 符号表达式 */
} SolverValueType;

/* ============================================================
 * 统一数值表示
 * ============================================================ */

/**
 * @brief 统一数值表示结构
 *
 * 支持多种数值类型的统一表示，便于跨后端比较。
 */
typedef struct SolverUnifiedValue {
    SolverValueType type;   /**< 值类型 */
    
    union {
        /* 布尔值 */
        bool bool_val;
        
        /* 整数值 */
        int64_t int_val;
        
        /* 有理数值 */
        struct {
            int64_t numerator;
            uint64_t denominator;
        } rational;
        
        /* 实数值（浮点） */
        double real_val;
        
        /* 代数数（最小多项式系数 + 区间隔离） */
        struct {
            int64_t *coeffs;    /**< 最小多项式系数（从高次到低次） */
            int degree;         /**< 多项式次数 */
            double lower_bound; /**< 根的下界 */
            double upper_bound; /**< 根的上界 */
        } algebraic;
        
        /* 符号表达式（字符串形式） */
        char *symbolic_expr;
    } data;
} SolverUnifiedValue;

/**
 * @brief 统一变量赋值
 */
typedef struct SolverUnifiedAssignment {
    int var_id;                     /**< 变量节点ID */
    char var_name[128];             /**< 变量名称 */
    SolverUnifiedValue value;       /**< 统一值 */
    
    /* 元数据 */
    SolverBackendType source;       /**< 来源后端 */
    double confidence;              /**< 该赋值的置信度 [0,1] */
    bool is_exact;                  /**< 是否为精确值 */
} SolverUnifiedAssignment;

/* ============================================================
 * 后端能力描述
 * ============================================================ */

/**
 * @brief 求解器能力标志
 */
typedef enum {
    SOLVER_CAP_BOOL = 1 << 0,           /**< 支持布尔约束 */
    SOLVER_CAP_LINEAR = 1 << 1,         /**< 支持线性约束 */
    SOLVER_CAP_NONLINEAR = 1 << 2,      /**< 支持非线性约束 */
    SOLVER_CAP_INTEGER = 1 << 3,        /**< 支持整数约束 */
    SOLVER_CAP_BITVECTOR = 1 << 4,      /**< 支持位向量 */
    SOLVER_CAP_ARRAY = 1 << 5,          /**< 支持数组理论 */
    SOLVER_CAP_QUANTIFIER = 1 << 6,     /**< 支持量词 */
    SOLVER_CAP_PROOF = 1 << 7,          /**< 支持证明生成 */
    SOLVER_CAP_INCREMENTAL = 1 << 8,    /**< 支持增量求解 */
    SOLVER_CAP_PARALLEL = 1 << 9,       /**< 支持并行求解 */
    SOLVER_CAP_ALGEBRAIC = 1 << 10,     /**< 支持代数数 */
    SOLVER_CAP_EXACT = 1 << 11          /**< 支持精确算术 */
} SolverCapability;

/**
 * @brief 后端能力描述结构
 */
typedef struct SolverCapabilityDesc {
    SolverBackendType type;         /**< 后端类型 */
    const char *name;               /**< 后端名称 */
    uint32_t capabilities;          /**< 能力标志位组合 */
    
    /* 性能特征 */
    int max_variables;              /**< 最大变量数（0=无限制） */
    int max_constraints;            /**< 最大约束数（0=无限制） */
    int max_degree;                 /**< 最大多项式次数（0=无限制） */
    
    /* 精度特征 */
    bool exact_arithmetic;          /**< 是否使用精确算术 */
    double numerical_precision;     /**< 数值精度（如果是近似求解） */
    
    /* 适用问题类型评分 [0,1] */
    float suitability_sat;          /**< SAT问题适用性 */
    float suitability_smt_linear;   /**< 线性SMT适用性 */
    float suitability_smt_nonlinear;/**< 非线性SMT适用性 */
    float suitability_groebner;     /**< Groebner基适用性 */
    float suitability_bdd;          /**< BDD适用性 */
    float suitability_atp;          /**< ATP适用性 */
} SolverCapabilityDesc;

/* ============================================================
 * 统一结果结构
 * ============================================================ */

/**
 * @brief 后端特定结果记录
 */
typedef struct SolverBackendResult {
    SolverBackendType backend;      /**< 后端类型 */
    SolverResultStatus status;      /**< 结果状态 */
    int64_t solve_time_ms;          /**< 求解耗时（毫秒） */
    
    /* 原始结果引用（可选） */
    void *raw_result;               /**< 后端特定结果指针 */
    void (*raw_result_free)(void*); /**< 释放函数 */
    
    /* 转换后的统一赋值 */
    SolverUnifiedAssignment *assignments;
    int assignment_count;
} SolverBackendResult;

/**
 * @brief 结果冲突记录
 */
typedef struct SolverResultConflict {
    int var_id;                             /**< 冲突变量ID */
    SolverBackendType backend_a;            /**< 后端A */
    SolverBackendType backend_b;            /**< 后端B */
    SolverUnifiedValue value_a;             /**< 后端A的值 */
    SolverUnifiedValue value_b;             /**< 后端B的值 */
    double discrepancy;                     /**< 差异程度 */
} SolverResultConflict;

/**
 * @brief 统一求解结果
 *
 * 聚合多后端求解结果，提供一致的结果视图。
 */
typedef struct SolverUnifiedResult {
    /* 综合结论 */
    SolverResultStatus status;              /**< 综合结果状态 */
    SolverConfidenceLevel confidence;       /**< 结果置信度 */
    
    /* 参与的后端结果 */
    SolverBackendResult backend_results[SOLVER_RESULT_MAX_BACKENDS];
    int backend_count;                      /**< 实际后端结果数量 */
    
    /* 统一赋值（合并后的结果） */
    SolverUnifiedAssignment *assignments;   /**< 统一赋值数组 */
    int assignment_count;                   /**< 赋值数量 */
    
    /* 验证信息 */
    SolverResultConflict conflicts[SOLVER_RESULT_MAX_CONFLICTS];
    int conflict_count;                     /**< 冲突数量 */
    
    /* 元数据 */
    int64_t total_time_ms;                  /**< 总耗时（毫秒） */
    int64_t verification_time_ms;           /**< 验证耗时（毫秒） */
    char consensus_reason[256];             /**< 共识达成原因说明 */
} SolverUnifiedResult;

/* ============================================================
 * 精度标准配置
 * ============================================================ */

/**
 * @brief 精度标准配置
 */
typedef struct SolverPrecisionConfig {
    double epsilon_absolute;        /**< 绝对容差 */
    double epsilon_relative;        /**< 相对容差 */
    uint64_t max_denominator;       /**< 有理数分母上限 */
    int max_algebraic_degree;       /**< 代数数最大次数 */
    bool strict_mode;               /**< 严格模式（容差更严格） */
} SolverPrecisionConfig;

/* ============================================================
 * API 函数声明
 * ============================================================ */

/**
 * @brief 获取默认精度配置
 */
SolverPrecisionConfig solver_precision_default(void);

/**
 * @brief 获取后端能力描述
 *
 * @param type 后端类型
 * @return 能力描述指针（静态存储，无需释放）
 */
const SolverCapabilityDesc *solver_capability_get(SolverBackendType type);

/**
 * @brief 检查后端是否支持特定能力
 *
 * @param type 后端类型
 * @param cap 能力标志
 * @return true 支持，false 不支持
 */
bool solver_capability_has(SolverBackendType type, SolverCapability cap);

/**
 * @brief 根据问题特征选择最佳后端
 *
 * @param features 问题特征（来自 GraphFeatures）
 * @param preferred_caps 优先能力（可选）
 * @return 推荐的后端类型
 */
SolverBackendType solver_select_by_capability(const void *features, uint32_t preferred_caps);

/**
 * @brief 创建统一结果对象
 *
 * @return 新创建的统一结果，失败返回 NULL
 */
SolverUnifiedResult *solver_unified_result_create(void);

/**
 * @brief 销毁统一结果对象
 *
 * @param result 统一结果
 */
void solver_unified_result_destroy(SolverUnifiedResult *result);

/**
 * @brief 初始化统一结果
 *
 * @param result 统一结果结构
 */
void solver_unified_result_init(SolverUnifiedResult *result);

/**
 * @brief 释放统一结果内部资源
 *
 * @param result 统一结果结构
 */
void solver_unified_result_free(SolverUnifiedResult *result);

/**
 * @brief 将 SMT 结果转换为统一结果
 *
 * @param smt_result SMT求解结果
 * @param out_result 输出的统一结果
 * @return 成功返回 0，失败返回 -1
 */
int solver_convert_smt_to_unified(const SMTSolverResult *smt_result, SolverUnifiedResult *out_result);

/**
 * @brief 将 Groebner 结果转换为统一结果
 *
 * @param gb_result Groebner求解结果
 * @param out_result 输出的统一结果
 * @return 成功返回 0，失败返回 -1
 */
int solver_convert_groebner_to_unified(const void *gb_result, SolverUnifiedResult *out_result);

/**
 * @brief 创建统一数值（布尔）
 */
SolverUnifiedValue solver_value_bool(bool val);

/**
 * @brief 创建统一数值（整数）
 */
SolverUnifiedValue solver_value_int(int64_t val);

/**
 * @brief 创建统一数值（有理数）
 */
SolverUnifiedValue solver_value_rational(int64_t num, uint64_t den);

/**
 * @brief 创建统一数值（实数）
 */
SolverUnifiedValue solver_value_real(double val);

/**
 * @brief 释放统一数值内部资源
 */
void solver_value_free(SolverUnifiedValue *val);

/**
 * @brief 比较两个统一数值是否相等（在容差范围内）
 *
 * @param a 值A
 * @param b 值B
 * @param config 精度配置（可为NULL使用默认）
 * @return true 相等，false 不相等
 */
bool solver_value_equals(const SolverUnifiedValue *a, const SolverUnifiedValue *b, 
                         const SolverPrecisionConfig *config);

/**
 * @brief 计算两个统一数值的差异程度
 *
 * @param a 值A
 * @param b 值B
 * @return 差异程度 [0, +inf)，0表示完全相同
 */
double solver_value_discrepancy(const SolverUnifiedValue *a, const SolverUnifiedValue *b);

/**
 * @brief 添加后端结果到统一结果
 *
 * @param unified 统一结果
 * @param backend_result 后端结果
 * @return 成功返回 0，失败返回 -1
 */
int solver_unified_add_backend_result(SolverUnifiedResult *unified, 
                                       const SolverBackendResult *backend_result);

/**
 * @brief 执行交叉验证
 *
 * 比较所有后端的结果，检测冲突并计算置信度。
 *
 * @param result 统一结果
 * @param config 精度配置（可为NULL）
 * @return 验证通过的后端数量
 */
int solver_cross_validate(SolverUnifiedResult *result, const SolverPrecisionConfig *config);

/**
 * @brief 合并多后端结果（共识算法）
 *
 * 使用投票或加权平均算法合并多后端结果。
 *
 * @param result 统一结果
 * @param config 精度配置（可为NULL）
 * @return 成功返回 0，失败返回 -1
 */
int solver_merge_results(SolverUnifiedResult *result, const SolverPrecisionConfig *config);

/**
 * @brief 检查统一结果是否一致
 *
 * @param result 统一结果
 * @return true 一致，false 存在冲突
 */
bool solver_unified_is_consistent(const SolverUnifiedResult *result);

/**
 * @brief 获取变量的共识值
 *
 * @param result 统一结果
 * @param var_id 变量ID
 * @param out_value 输出的共识值
 * @return 成功返回 0，未找到返回 -1
 */
int solver_unified_get_consensus(const SolverUnifiedResult *result, int var_id, 
                                  SolverUnifiedValue *out_value);

/**
 * @brief 生成结果验证报告
 *
 * @param result 统一结果
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 实际写入的字符数
 */
int solver_unified_format_report(const SolverUnifiedResult *result, char *buffer, size_t buffer_size);

/**
 * @brief 获取结果状态字符串
 *
 * @param status 结果状态
 * @return 状态字符串
 */
const char *solver_result_status_string(SolverResultStatus status);

/**
 * @brief 获取置信度等级字符串
 *
 * @param level 置信度等级
 * @return 等级字符串
 */
const char *solver_confidence_string(SolverConfidenceLevel level);

#ifdef __cplusplus
}
#endif

#endif /* LV00_SOLVER_RESULT_STANDARD_H */
