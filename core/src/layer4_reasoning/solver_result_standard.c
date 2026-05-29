/**
 * @file solver_result_standard.c
 * @brief 统一求解结果标准 —— 多后端结果标准化、交叉验证与一致性检查
 *
 * 实现所有后端求解结果的统一格式转换、精度比较、交叉验证、
 * 共识合并与报告生成等功能。
 *
 * @version 3.3.0
 */

#include "solver_result_standard.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "lv00.h"
#include "lv00_utils.h"

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/**
 * @brief 获取默认精度配置（内部静态版本）
 */
static SolverPrecisionConfig precision_default_internal(void)
{
    SolverPrecisionConfig cfg;
    cfg.epsilon_absolute    = SOLVER_RESULT_EPSILON_ABSOLUTE;
    cfg.epsilon_relative    = SOLVER_RESULT_EPSILON_RELATIVE;
    cfg.max_denominator     = SOLVER_RESULT_MAX_DENOMINATOR;
    cfg.max_algebraic_degree = 10;
    cfg.strict_mode         = false;
    return cfg;
}

/**
 * @brief 安全获取精度配置指针（若传入 NULL 则使用默认值）
 */
static const SolverPrecisionConfig *safe_precision(const SolverPrecisionConfig *config)
{
    static SolverPrecisionConfig s_default;
    static bool s_initialized = false;
    if (!s_initialized) {
        s_default = precision_default_internal();
        s_initialized = true;
    }
    return config ? config : &s_default;
}

/* ============================================================
 * 后端能力描述表（静态数据）
 * ============================================================ */

/** @brief 各后端的能力描述（静态存储，只读） */
static const SolverCapabilityDesc s_capability_table[] = {
    /* GROEBNER */
    {
        .type = GROEBNER,
        .name = "Groebner",
        .capabilities = (uint32_t)(
            SOLVER_CAP_NONLINEAR | SOLVER_CAP_ALGEBRAIC | SOLVER_CAP_EXACT),
        .max_variables   = 0,
        .max_constraints = 0,
        .max_degree      = 0,
        .exact_arithmetic = true,
        .numerical_precision = 0.0,
        .suitability_sat = 0.3f,
        .suitability_smt_linear = 0.4f,
        .suitability_smt_nonlinear = 0.9f,
        .suitability_groebner = 1.0f,
        .suitability_bdd = 0.1f,
        .suitability_atp = 0.2f
    },
    /* SMT_Z3 */
    {
        .type = SMT_Z3,
        .name = "Z3",
        .capabilities = (uint32_t)(
            SOLVER_CAP_BOOL | SOLVER_CAP_LINEAR | SOLVER_CAP_NONLINEAR |
            SOLVER_CAP_INTEGER | SOLVER_CAP_BITVECTOR | SOLVER_CAP_ARRAY |
            SOLVER_CAP_QUANTIFIER | SOLVER_CAP_PROOF | SOLVER_CAP_INCREMENTAL |
            SOLVER_CAP_PARALLEL),
        .max_variables   = 0,
        .max_constraints = 0,
        .max_degree      = 0,
        .exact_arithmetic = false,
        .numerical_precision = 1e-15,
        .suitability_sat = 0.9f,
        .suitability_smt_linear = 1.0f,
        .suitability_smt_nonlinear = 0.8f,
        .suitability_groebner = 0.5f,
        .suitability_bdd = 0.6f,
        .suitability_atp = 0.4f
    },
    /* SMT_CVC5 */
    {
        .type = SMT_CVC5,
        .name = "cvc5",
        .capabilities = (uint32_t)(
            SOLVER_CAP_BOOL | SOLVER_CAP_LINEAR | SOLVER_CAP_NONLINEAR |
            SOLVER_CAP_INTEGER | SOLVER_CAP_BITVECTOR | SOLVER_CAP_ARRAY |
            SOLVER_CAP_QUANTIFIER | SOLVER_CAP_PROOF | SOLVER_CAP_INCREMENTAL),
        .max_variables   = 0,
        .max_constraints = 0,
        .max_degree      = 0,
        .exact_arithmetic = false,
        .numerical_precision = 1e-15,
        .suitability_sat = 0.9f,
        .suitability_smt_linear = 1.0f,
        .suitability_smt_nonlinear = 0.7f,
        .suitability_groebner = 0.4f,
        .suitability_bdd = 0.5f,
        .suitability_atp = 0.3f
    },
    /* SMT_SINGULAR */
    {
        .type = SMT_SINGULAR,
        .name = "Singular",
        .capabilities = (uint32_t)(
            SOLVER_CAP_NONLINEAR | SOLVER_CAP_ALGEBRAIC | SOLVER_CAP_EXACT),
        .max_variables   = 0,
        .max_constraints = 0,
        .max_degree      = 0,
        .exact_arithmetic = true,
        .numerical_precision = 0.0,
        .suitability_sat = 0.2f,
        .suitability_smt_linear = 0.3f,
        .suitability_smt_nonlinear = 0.9f,
        .suitability_groebner = 0.8f,
        .suitability_bdd = 0.1f,
        .suitability_atp = 0.3f
    }
};

/** @brief 能力表条目数 */
#define CAPABILITY_TABLE_SIZE \
    (sizeof(s_capability_table) / sizeof(s_capability_table[0]))

/* ============================================================
 * 精度配置 API
 * ============================================================ */

/**
 * @brief 获取默认精度配置
 */
LV00_PUBLIC_API SolverPrecisionConfig solver_precision_default(void)
{
    return precision_default_internal();
}

/* ============================================================
 * 后端能力查询 API
 * ============================================================ */

/**
 * @brief 获取后端能力描述
 */
LV00_PUBLIC_API const SolverCapabilityDesc *solver_capability_get(SolverBackendType type)
{
    for (int i = 0; i < (int)CAPABILITY_TABLE_SIZE; i++) {
        if (s_capability_table[i].type == type) {
            return &s_capability_table[i];
        }
    }
    return NULL;
}

/**
 * @brief 检查后端是否支持特定能力
 */
LV00_PUBLIC_API bool solver_capability_has(SolverBackendType type, SolverCapability cap)
{
    const SolverCapabilityDesc *desc = solver_capability_get(type);
    if (!desc) return false;
    return (desc->capabilities & (uint32_t)cap) != 0;
}

/**
 * @brief 根据问题特征选择最佳后端
 *
 * 简化实现：当 features 为 NULL 或 preferred_caps 为 0 时，
 * 默认返回 GROEBNER 后端。否则遍历能力表选择匹配度最高的后端。
 */
LV00_PUBLIC_API SolverBackendType solver_select_by_capability(const void *features,
                                                             uint32_t preferred_caps)
{
    (void)features; /* 暂不使用问题特征 */

    /* 无优先能力要求时返回默认后端 */
    if (preferred_caps == 0) {
        return GROEBNER;
    }

    /* 遍历能力表，选择能力匹配度最高的后端 */
    int best_idx   = 0;
    int best_score = 0;

    for (int i = 0; i < (int)CAPABILITY_TABLE_SIZE; i++) {
        uint32_t match = s_capability_table[i].capabilities & preferred_caps;
        int score = 0;
        /* 计算匹配的位数作为得分 */
        while (match) {
            score += match & 1;
            match >>= 1;
        }
        if (score > best_score) {
            best_score = score;
            best_idx = i;
        }
    }

    return s_capability_table[best_idx].type;
}

/* ============================================================
 * 统一数值构造 API
 * ============================================================ */

/**
 * @brief 创建统一数值（布尔）
 */
LV00_PUBLIC_API SolverUnifiedValue solver_value_bool(bool val)
{
    SolverUnifiedValue v;
    memset(&v, 0, sizeof(v));
    v.type = SOLVER_VALUE_BOOL;
    v.data.bool_val = val;
    return v;
}

/**
 * @brief 创建统一数值（整数）
 */
LV00_PUBLIC_API SolverUnifiedValue solver_value_int(int64_t val)
{
    SolverUnifiedValue v;
    memset(&v, 0, sizeof(v));
    v.type = SOLVER_VALUE_INT;
    v.data.int_val = val;
    return v;
}

/**
 * @brief 创建统一数值（有理数）
 */
LV00_PUBLIC_API SolverUnifiedValue solver_value_rational(int64_t num, uint64_t den)
{
    SolverUnifiedValue v;
    memset(&v, 0, sizeof(v));
    v.type = SOLVER_VALUE_RATIONAL;
    v.data.rational.numerator   = num;
    v.data.rational.denominator = (den == 0) ? 1 : den;
    return v;
}

/**
 * @brief 创建统一数值（实数）
 */
LV00_PUBLIC_API SolverUnifiedValue solver_value_real(double val)
{
    SolverUnifiedValue v;
    memset(&v, 0, sizeof(v));
    v.type = SOLVER_VALUE_REAL;
    v.data.real_val = val;
    return v;
}

/**
 * @brief 释放统一数值内部资源
 */
LV00_PUBLIC_API void solver_value_free(SolverUnifiedValue *val)
{
    if (!val) return;

    if (val->type == SOLVER_VALUE_ALGEBRAIC && val->data.algebraic.coeffs) {
        { void *_tmp = (void *)val->data.algebraic.coeffs; lv00_free(&_tmp); }
        val->data.algebraic.coeffs = NULL;
    }

    if (val->type == SOLVER_VALUE_SYMBOLIC && val->data.symbolic_expr) {
        { void *_tmp = (void *)val->data.symbolic_expr; lv00_free(&_tmp); }
        val->data.symbolic_expr = NULL;
    }

    val->type = SOLVER_VALUE_BOOL;
}

/* ============================================================
 * 统一数值比较 API
 * ============================================================ */

/**
 * @brief 比较两个统一数值是否相等（在容差范围内）
 */
LV00_PUBLIC_API bool solver_value_equals(const SolverUnifiedValue *a,
                                         const SolverUnifiedValue *b,
                                         const SolverPrecisionConfig *config)
{
    if (!a || !b) return false;

    /* 类型不同则直接不等（bool 与 int 之间允许隐式转换） */
    if (a->type != b->type) {
        /* bool 和 int 之间允许比较 */
        if ((a->type == SOLVER_VALUE_BOOL && b->type == SOLVER_VALUE_INT) ||
            (a->type == SOLVER_VALUE_INT  && b->type == SOLVER_VALUE_BOOL)) {
            bool a_bool = (a->type == SOLVER_VALUE_BOOL) ? a->data.bool_val : (a->data.int_val != 0);
            bool b_bool = (b->type == SOLVER_VALUE_BOOL) ? b->data.bool_val : (b->data.int_val != 0);
            return a_bool == b_bool;
        }
        /* int 和 real 之间允许比较 */
        if ((a->type == SOLVER_VALUE_INT && b->type == SOLVER_VALUE_REAL) ||
            (a->type == SOLVER_VALUE_REAL && b->type == SOLVER_VALUE_INT)) {
            double a_d = (a->type == SOLVER_VALUE_INT) ? (double)a->data.int_val : a->data.real_val;
            double b_d = (b->type == SOLVER_VALUE_INT) ? (double)b->data.int_val : b->data.real_val;
            const SolverPrecisionConfig *cfg = safe_precision(config);
            double diff = fabs(a_d - b_d);
            double rel  = (fabs(a_d) > fabs(b_d)) ? fabs(a_d) : fabs(b_d);
            return diff <= cfg->epsilon_absolute ||
                   (rel > 0.0 && diff / rel <= cfg->epsilon_relative);
        }
        return false;
    }

    const SolverPrecisionConfig *cfg = safe_precision(config);

    switch (a->type) {
    case SOLVER_VALUE_BOOL:
        return a->data.bool_val == b->data.bool_val;

    case SOLVER_VALUE_INT:
        return a->data.int_val == b->data.int_val;

    case SOLVER_VALUE_RATIONAL: {
        /* 有理数比较：a/b == c/d  <=>  a*d == c*b */
        int64_t lhs = a->data.rational.numerator * (int64_t)b->data.rational.denominator;
        int64_t rhs = b->data.rational.numerator * (int64_t)a->data.rational.denominator;
        return lhs == rhs;
    }

    case SOLVER_VALUE_REAL: {
        double diff = fabs(a->data.real_val - b->data.real_val);
        double rel  = (fabs(a->data.real_val) > fabs(b->data.real_val))
                      ? fabs(a->data.real_val) : fabs(b->data.real_val);
        return diff <= cfg->epsilon_absolute ||
               (rel > 0.0 && diff / rel <= cfg->epsilon_relative);
    }

    case SOLVER_VALUE_ALGEBRAIC:
        /* 代数数简化比较：次数相同且区间重叠 */
        if (a->data.algebraic.degree != b->data.algebraic.degree) return false;
        if (a->data.algebraic.lower_bound > b->data.algebraic.upper_bound + cfg->epsilon_absolute)
            return false;
        if (b->data.algebraic.lower_bound > a->data.algebraic.upper_bound + cfg->epsilon_absolute)
            return false;
        return true;

    case SOLVER_VALUE_SYMBOLIC:
        /* 符号表达式：字符串精确比较 */
        if (a->data.symbolic_expr == b->data.symbolic_expr) return true;
        if (!a->data.symbolic_expr || !b->data.symbolic_expr) return false;
        return strcmp(a->data.symbolic_expr, b->data.symbolic_expr) == 0;

    default:
        return false;
    }
}

/**
 * @brief 计算两个统一数值的差异程度
 */
LV00_PUBLIC_API double solver_value_discrepancy(const SolverUnifiedValue *a,
                                               const SolverUnifiedValue *b)
{
    if (!a || !b) return INFINITY;

    /* 类型完全不同时差异为无穷大 */
    if (a->type != b->type) {
        /* 允许 bool/int 和 int/real 的跨类型比较 */
        if ((a->type == SOLVER_VALUE_BOOL && b->type == SOLVER_VALUE_INT) ||
            (a->type == SOLVER_VALUE_INT  && b->type == SOLVER_VALUE_BOOL)) {
            bool a_bool = (a->type == SOLVER_VALUE_BOOL) ? a->data.bool_val : (a->data.int_val != 0);
            bool b_bool = (b->type == SOLVER_VALUE_BOOL) ? b->data.bool_val : (b->data.int_val != 0);
            return (a_bool == b_bool) ? 0.0 : 1.0;
        }
        if ((a->type == SOLVER_VALUE_INT && b->type == SOLVER_VALUE_REAL) ||
            (a->type == SOLVER_VALUE_REAL && b->type == SOLVER_VALUE_INT)) {
            double a_d = (a->type == SOLVER_VALUE_INT) ? (double)a->data.int_val : a->data.real_val;
            double b_d = (b->type == SOLVER_VALUE_INT) ? (double)b->data.int_val : b->data.real_val;
            double rel = (fabs(a_d) > fabs(b_d)) ? fabs(a_d) : fabs(b_d);
            return (rel > 0.0) ? fabs(a_d - b_d) / rel : fabs(a_d - b_d);
        }
        return INFINITY;
    }

    switch (a->type) {
    case SOLVER_VALUE_BOOL:
        return (a->data.bool_val == b->data.bool_val) ? 0.0 : 1.0;

    case SOLVER_VALUE_INT:
        if (a->data.int_val == b->data.int_val) return 0.0;
        {
            double rel = (fabs((double)a->data.int_val) > fabs((double)b->data.int_val))
                         ? fabs((double)a->data.int_val) : fabs((double)b->data.int_val);
            return (rel > 0.0)
                   ? fabs((double)(a->data.int_val - b->data.int_val)) / rel
                   : fabs((double)(a->data.int_val - b->data.int_val));
        }

    case SOLVER_VALUE_RATIONAL: {
        if (a->data.rational.denominator == 0 || b->data.rational.denominator == 0)
            return INFINITY;
        double va = (double)a->data.rational.numerator / (double)a->data.rational.denominator;
        double vb = (double)b->data.rational.numerator / (double)b->data.rational.denominator;
        double rel = (fabs(va) > fabs(vb)) ? fabs(va) : fabs(vb);
        return (rel > 0.0) ? fabs(va - vb) / rel : fabs(va - vb);
    }

    case SOLVER_VALUE_REAL: {
        double rel = (fabs(a->data.real_val) > fabs(b->data.real_val))
                     ? fabs(a->data.real_val) : fabs(b->data.real_val);
        return (rel > 0.0) ? fabs(a->data.real_val - b->data.real_val) / rel
                             : fabs(a->data.real_val - b->data.real_val);
    }

    case SOLVER_VALUE_ALGEBRAIC: {
        double gap = 0.0;
        if (a->data.algebraic.lower_bound > b->data.algebraic.upper_bound)
            gap = a->data.algebraic.lower_bound - b->data.algebraic.upper_bound;
        else if (b->data.algebraic.lower_bound > a->data.algebraic.upper_bound)
            gap = b->data.algebraic.lower_bound - a->data.algebraic.upper_bound;
        return gap;
    }

    case SOLVER_VALUE_SYMBOLIC:
        if (a->data.symbolic_expr == b->data.symbolic_expr) return 0.0;
        if (!a->data.symbolic_expr || !b->data.symbolic_expr) return INFINITY;
        return (strcmp(a->data.symbolic_expr, b->data.symbolic_expr) == 0) ? 0.0 : 1.0;

    default:
        return INFINITY;
    }
}

/* ============================================================
 * 统一结果生命周期管理 API
 * ============================================================ */

/**
 * @brief 初始化统一结果
 */
LV00_PUBLIC_API void solver_unified_result_init(SolverUnifiedResult *result)
{
    if (!result) return;
    memset(result, 0, sizeof(SolverUnifiedResult));
}

/**
 * @brief 释放统一结果内部资源
 */
LV00_PUBLIC_API void solver_unified_result_free(SolverUnifiedResult *result)
{
    if (!result) return;

    /* 释放各后端结果中的赋值数组 */
    for (int i = 0; i < result->backend_count && i < SOLVER_RESULT_MAX_BACKENDS; i++) {
        SolverBackendResult *br = &result->backend_results[i];
        if (br->assignments) {
            /* 释放每个赋值中的动态资源 */
            for (int j = 0; j < br->assignment_count; j++) {
                solver_value_free(&br->assignments[j].value);
            }
            { void *_tmp = (void *)br->assignments; lv00_free(&_tmp); }
            br->assignments = NULL;
        }
        br->assignment_count = 0;
        /* 释放原始结果（如有释放函数） */
        if (br->raw_result && br->raw_result_free) {
            br->raw_result_free(br->raw_result);
            br->raw_result = NULL;
            br->raw_result_free = NULL;
        }
    }

    /* 释放统一赋值数组 */
    if (result->assignments) {
        for (int i = 0; i < result->assignment_count; i++) {
            solver_value_free(&result->assignments[i].value);
        }
        { void *_tmp = (void *)result->assignments; lv00_free(&_tmp); }
        result->assignments = NULL;
    }
    result->assignment_count = 0;

    /* 冲突记录中的值资源释放 */
    for (int i = 0; i < result->conflict_count && i < SOLVER_RESULT_MAX_CONFLICTS; i++) {
        solver_value_free(&result->conflicts[i].value_a);
        solver_value_free(&result->conflicts[i].value_b);
    }
    result->conflict_count = 0;

    result->backend_count = 0;
    result->status = SOLVER_RESULT_UNKNOWN;
    result->confidence = SOLVER_CONFIDENCE_NONE;
}

/**
 * @brief 创建统一结果对象
 */
LV00_PUBLIC_API SolverUnifiedResult *solver_unified_result_create(void)
{
    SolverUnifiedResult *result = (SolverUnifiedResult *)lv00_calloc(1, sizeof(SolverUnifiedResult));
    if (!result) return NULL;

    solver_unified_result_init(result);
    return result;
}

/**
 * @brief 销毁统一结果对象
 */
LV00_PUBLIC_API void solver_unified_result_destroy(SolverUnifiedResult *result)
{
    if (!result) return;
    solver_unified_result_free(result);
    { void *_tmp = (void *)result; lv00_free(&_tmp); }
}

/* ============================================================
 * 结果转换 API
 * ============================================================ */

/**
 * @brief 将 SMT 结果转换为统一结果
 *
 * 遍历 SMT 求解结果中的变量赋值，逐一转换为统一赋值格式。
 */
LV00_PUBLIC_API int solver_convert_smt_to_unified(const SMTSolverResult *smt_result,
                                                  SolverUnifiedResult *out_result)
{
    if (!smt_result || !out_result) return -1;

    /* 映射 SMT 可满足性结果到统一状态 */
    switch (smt_result->sat_result) {
    case SMT_RESULT_SAT:
        out_result->status = SOLVER_RESULT_SAT;
        break;
    case SMT_RESULT_UNSAT:
        out_result->status = SOLVER_RESULT_UNSAT;
        break;
    case SMT_RESULT_UNKNOWN:
        out_result->status = SOLVER_RESULT_UNKNOWN;
        break;
    case SMT_RESULT_ERROR:
        out_result->status = SOLVER_RESULT_ERROR;
        break;
    default:
        out_result->status = SOLVER_RESULT_UNKNOWN;
        break;
    }

    /* 添加后端结果记录 */
    if (out_result->backend_count < SOLVER_RESULT_MAX_BACKENDS) {
        SolverBackendResult *br = &out_result->backend_results[out_result->backend_count];
        br->backend         = smt_result->backend_used;
        br->status          = out_result->status;
        br->solve_time_ms   = smt_result->solve_time_ms;
        br->raw_result      = NULL;
        br->raw_result_free = NULL;
        br->assignments     = NULL;
        br->assignment_count = 0;
        out_result->backend_count++;
    }

    /* 转换变量赋值 */
    if (smt_result->assignments && smt_result->assignment_count > 0) {
        int count = smt_result->assignment_count;
        if (count > SOLVER_RESULT_MAX_ASSIGNMENTS) count = SOLVER_RESULT_MAX_ASSIGNMENTS;

        out_result->assignments = (SolverUnifiedAssignment *)lv00_calloc(
            (size_t)count, sizeof(SolverUnifiedAssignment));
        if (!out_result->assignments) return -1;

        for (int i = 0; i < count; i++) {
            const SMTVariableAssignment *src = &smt_result->assignments[i];
            SolverUnifiedAssignment *dst = &out_result->assignments[i];

            dst->var_id   = src->var_node_id;
            dst->source   = smt_result->backend_used;
            dst->confidence = 1.0;
            dst->is_exact = true;

            /* 安全复制变量名 */
            strncpy(dst->var_name, src->var_name, sizeof(dst->var_name) - 1);
            dst->var_name[sizeof(dst->var_name) - 1] = '\0';

            /* 转换值类型 */
            if (src->is_boolean) {
                dst->value = solver_value_bool(src->value.bool_value);
                dst->is_exact = true;
            } else if (src->value.rational.denominator == 0) {
                /* 近似十进制值 */
                dst->value = solver_value_real(src->value.rational.approx_value);
                dst->is_exact = false;
            } else {
                /* 有理数精确值 */
                dst->value = solver_value_rational(src->value.rational.numerator,
                                                   src->value.rational.denominator);
                dst->is_exact = true;
            }
        }
        out_result->assignment_count = count;
    }

    out_result->total_time_ms = smt_result->solve_time_ms;
    out_result->confidence = SOLVER_CONFIDENCE_LOW;

    /* 设置共识原因 */
    snprintf(out_result->consensus_reason, sizeof(out_result->consensus_reason),
             "SMT backend %s single result", smtsolver_backend_type_name(smt_result->backend_used));

    return 0;
}

/**
 * @brief 将 Groebner 结果转换为统一结果
 *
 * 简化实现：当 gb_result 为 NULL 时设置状态为 UNKNOWN。
 * 完整实现需要解析 Groebner 引擎的内部结果格式。
 */
LV00_PUBLIC_API int solver_convert_groebner_to_unified(const void *gb_result,
                                                       SolverUnifiedResult *out_result)
{
    if (!out_result) return -1;

    if (!gb_result) {
        out_result->status = SOLVER_RESULT_UNKNOWN;
        out_result->confidence = SOLVER_CONFIDENCE_NONE;
        return -1;
    }

    /* 简化实现：标记为部分求解，等待完整 Groebner 结果解析 */
    out_result->status = SOLVER_RESULT_PARTIAL;
    out_result->confidence = SOLVER_CONFIDENCE_LOW;

    /* 添加 Groebner 后端记录 */
    if (out_result->backend_count < SOLVER_RESULT_MAX_BACKENDS) {
        SolverBackendResult *br = &out_result->backend_results[out_result->backend_count];
        br->backend         = GROEBNER;
        br->status          = SOLVER_RESULT_PARTIAL;
        br->solve_time_ms   = 0;
        br->raw_result      = NULL;
        br->raw_result_free = NULL;
        br->assignments     = NULL;
        br->assignment_count = 0;
        out_result->backend_count++;
    }

    snprintf(out_result->consensus_reason, sizeof(out_result->consensus_reason),
             "Groebner backend partial result (simplified conversion)");

    return 0;
}

/* ============================================================
 * 后端结果添加 API
 * ============================================================ */

/**
 * @brief 添加后端结果到统一结果
 */
LV00_PUBLIC_API int solver_unified_add_backend_result(SolverUnifiedResult *unified,
                                                       const SolverBackendResult *backend_result)
{
    if (!unified || !backend_result) return -1;

    if (unified->backend_count >= SOLVER_RESULT_MAX_BACKENDS) {
        return -1; /* 后端数量已达上限 */
    }

    SolverBackendResult *dst = &unified->backend_results[unified->backend_count];

    /* 复制基本字段 */
    dst->backend       = backend_result->backend;
    dst->status        = backend_result->status;
    dst->solve_time_ms = backend_result->solve_time_ms;
    dst->raw_result    = NULL; /* 不转移原始结果所有权 */
    dst->raw_result_free = NULL;

    /* 深拷贝赋值数组 */
    dst->assignments = NULL;
    dst->assignment_count = 0;

    if (backend_result->assignments && backend_result->assignment_count > 0) {
        int count = backend_result->assignment_count;
        dst->assignments = (SolverUnifiedAssignment *)lv00_calloc(
            (size_t)count, sizeof(SolverUnifiedAssignment));
        if (!dst->assignments) return -1;

        for (int i = 0; i < count; i++) {
            dst->assignments[i] = backend_result->assignments[i];
            /* 深拷贝符号表达式 */
            if (backend_result->assignments[i].value.type == SOLVER_VALUE_SYMBOLIC &&
                backend_result->assignments[i].value.data.symbolic_expr) {
                dst->assignments[i].value.data.symbolic_expr =
                    lv00_strdup(backend_result->assignments[i].value.data.symbolic_expr);
            }
            /* 深拷贝代数数系数 */
            if (backend_result->assignments[i].value.type == SOLVER_VALUE_ALGEBRAIC &&
                backend_result->assignments[i].value.data.algebraic.coeffs) {
                int deg = backend_result->assignments[i].value.data.algebraic.degree;
                size_t coeff_size = (size_t)(deg + 1) * sizeof(int64_t);
                dst->assignments[i].value.data.algebraic.coeffs =
                    (int64_t *)lv00_malloc(coeff_size);
                if (dst->assignments[i].value.data.algebraic.coeffs) {
                    memcpy(dst->assignments[i].value.data.algebraic.coeffs,
                           backend_result->assignments[i].value.data.algebraic.coeffs,
                           coeff_size);
                }
            }
        }
        dst->assignment_count = count;
    }

    unified->backend_count++;
    return 0;
}

/* ============================================================
 * 交叉验证 API
 * ============================================================ */

/**
 * @brief 执行交叉验证
 *
 * 简化实现：比较所有后端对的赋值结果，记录冲突并计算置信度。
 * 仅比较相同 var_id 的赋值。
 */
LV00_PUBLIC_API int solver_cross_validate(SolverUnifiedResult *result,
                                          const SolverPrecisionConfig *config)
{
    if (!result) return 0;

    const SolverPrecisionConfig *cfg = safe_precision(config);
    int verified_count = 0;
    result->conflict_count = 0;

    /* 至少需要两个后端才能交叉验证 */
    if (result->backend_count < 2) {
        /* 单后端：标记为低置信度 */
        result->confidence = SOLVER_CONFIDENCE_LOW;
        return (result->backend_count > 0) ? 1 : 0;
    }

    /* 比较每对后端 */
    for (int i = 0; i < result->backend_count; i++) {
        SolverBackendResult *br_a = &result->backend_results[i];
        if (br_a->assignment_count == 0) continue;

        for (int j = i + 1; j < result->backend_count; j++) {
            SolverBackendResult *br_b = &result->backend_results[j];
            if (br_b->assignment_count == 0) continue;

            /* 逐变量比较 */
            for (int ai = 0; ai < br_a->assignment_count; ai++) {
                for (int bi = 0; bi < br_b->assignment_count; bi++) {
                    if (br_a->assignments[ai].var_id != br_b->assignments[bi].var_id)
                        continue;

                    bool equal = solver_value_equals(
                        &br_a->assignments[ai].value,
                        &br_b->assignments[bi].value,
                        cfg);

                    if (!equal && result->conflict_count < SOLVER_RESULT_MAX_CONFLICTS) {
                        SolverResultConflict *conflict =
                            &result->conflicts[result->conflict_count];
                        conflict->var_id    = br_a->assignments[ai].var_id;
                        conflict->backend_a = br_a->backend;
                        conflict->backend_b = br_b->backend;
                        conflict->value_a   = br_a->assignments[ai].value;
                        conflict->value_b   = br_b->assignments[bi].value;
                        conflict->discrepancy = solver_value_discrepancy(
                            &br_a->assignments[ai].value,
                            &br_b->assignments[bi].value);
                        result->conflict_count++;
                    }
                }
            }
        }
    }

    /* 根据冲突数量设置置信度 */
    if (result->conflict_count == 0) {
        result->confidence = SOLVER_CONFIDENCE_HIGH;
        verified_count = result->backend_count;
        snprintf(result->consensus_reason, sizeof(result->consensus_reason),
                 "All %d backends agree", result->backend_count);
    } else {
        result->confidence = SOLVER_CONFIDENCE_MEDIUM;
        verified_count = result->backend_count - result->conflict_count;
        if (verified_count < 0) verified_count = 0;
        snprintf(result->consensus_reason, sizeof(result->consensus_reason),
                 "%d conflicts detected among %d backends",
                 result->conflict_count, result->backend_count);
    }

    return verified_count;
}

/* ============================================================
 * 结果合并 API
 * ============================================================ */

/**
 * @brief 合并多后端结果（共识算法）
 *
 * 简化实现：采用简单投票策略 —— 对于每个变量，选择出现次数最多的值。
 * 若所有后端一致则直接采用；若存在冲突则选择置信度最高的赋值。
 */
LV00_PUBLIC_API int solver_merge_results(SolverUnifiedResult *result,
                                        const SolverPrecisionConfig *config)
{
    if (!result) return -1;

    const SolverPrecisionConfig *cfg = safe_precision(config);

    /* 先执行交叉验证 */
    solver_cross_validate(result, cfg);

    /* 若只有一个后端，直接使用其赋值 */
    if (result->backend_count <= 1) {
        if (result->backend_count == 1 && result->backend_results[0].assignments) {
            /* 转移赋值到统一结果 */
            int count = result->backend_results[0].assignment_count;
            result->assignments = result->backend_results[0].assignments;
            result->assignment_count = count;
            result->backend_results[0].assignments = NULL;
            result->backend_results[0].assignment_count = 0;
        }
        return 0;
    }

    /* 收集所有唯一的 var_id */
    int var_ids[SOLVER_RESULT_MAX_ASSIGNMENTS];
    int var_count = 0;

    for (int i = 0; i < result->backend_count; i++) {
        SolverBackendResult *br = &result->backend_results[i];
        for (int j = 0; j < br->assignment_count; j++) {
            int vid = br->assignments[j].var_id;
            /* 检查是否已存在 */
            bool found = false;
            for (int k = 0; k < var_count; k++) {
                if (var_ids[k] == vid) { found = true; break; }
            }
            if (!found && var_count < SOLVER_RESULT_MAX_ASSIGNMENTS) {
                var_ids[var_count++] = vid;
            }
        }
    }

    if (var_count == 0) return 0;

    /* 为每个变量选择共识值 */
    result->assignments = (SolverUnifiedAssignment *)lv00_calloc(
        (size_t)var_count, sizeof(SolverUnifiedAssignment));
    if (!result->assignments) return -1;

    for (int vi = 0; vi < var_count; vi++) {
        SolverUnifiedAssignment *best = NULL;
        double best_conf = -1.0;
        int vote_count = 0;

        for (int i = 0; i < result->backend_count; i++) {
            SolverBackendResult *br = &result->backend_results[i];
            for (int j = 0; j < br->assignment_count; j++) {
                if (br->assignments[j].var_id != var_ids[vi]) continue;

                vote_count++;
                if (br->assignments[j].confidence > best_conf) {
                    best_conf = br->assignments[j].confidence;
                    best = &br->assignments[j];
                }
            }
        }

        if (best) {
            result->assignments[vi] = *best;
            /* 深拷贝动态资源 */
            if (best->value.type == SOLVER_VALUE_SYMBOLIC && best->value.data.symbolic_expr) {
                result->assignments[vi].value.data.symbolic_expr =
                    lv00_strdup(best->value.data.symbolic_expr);
            }
            if (best->value.type == SOLVER_VALUE_ALGEBRAIC && best->value.data.algebraic.coeffs) {
                int deg = best->value.data.algebraic.degree;
                size_t coeff_size = (size_t)(deg + 1) * sizeof(int64_t);
                result->assignments[vi].value.data.algebraic.coeffs =
                    (int64_t *)lv00_malloc(coeff_size);
                if (result->assignments[vi].value.data.algebraic.coeffs) {
                    memcpy(result->assignments[vi].value.data.algebraic.coeffs,
                           best->value.data.algebraic.coeffs, coeff_size);
                }
            }
            /* 多后端一致时提高置信度 */
            if (vote_count > 1) {
                result->assignments[vi].confidence = 1.0;
                result->assignments[vi].is_exact = true;
            }
        }
    }

    result->assignment_count = var_count;
    return 0;
}

/* ============================================================
 * 一致性检查 API
 * ============================================================ */

/**
 * @brief 检查统一结果是否一致
 */
LV00_PUBLIC_API bool solver_unified_is_consistent(const SolverUnifiedResult *result)
{
    if (!result) return false;
    return result->conflict_count == 0;
}

/* ============================================================
 * 共识值查询 API
 * ============================================================ */

/**
 * @brief 获取变量的共识值
 */
LV00_PUBLIC_API int solver_unified_get_consensus(const SolverUnifiedResult *result,
                                                  int var_id,
                                                  SolverUnifiedValue *out_value)
{
    if (!result || !out_value) return -1;

    /* 在统一赋值中查找 */
    for (int i = 0; i < result->assignment_count; i++) {
        if (result->assignments[i].var_id == var_id) {
            *out_value = result->assignments[i].value;
            /* 浅拷贝：调用者不应释放 out_value 中的指针 */
            return 0;
        }
    }

    /* 在后端结果中查找（回退） */
    for (int i = 0; i < result->backend_count && i < SOLVER_RESULT_MAX_BACKENDS; i++) {
        const SolverBackendResult *br = &result->backend_results[i];
        for (int j = 0; j < br->assignment_count; j++) {
            if (br->assignments[j].var_id == var_id) {
                *out_value = br->assignments[j].value;
                return 0;
            }
        }
    }

    return -1; /* 未找到 */
}

/* ============================================================
 * 报告生成 API
 * ============================================================ */

/**
 * @brief 生成结果验证报告
 */
LV00_PUBLIC_API int solver_unified_format_report(const SolverUnifiedResult *result,
                                                  char *buffer, size_t buffer_size)
{
    if (!result || !buffer || buffer_size == 0) return 0;

    int written = 0;
    int remaining = (int)buffer_size;

    /* 结果状态 */
    written += snprintf(buffer + written, (size_t)remaining,
        "=== Solver Result Report ===\n"
        "Status: %s\n"
        "Confidence: %s\n"
        "Backends: %d\n"
        "Assignments: %d\n"
        "Conflicts: %d\n"
        "Total time: %lld ms\n"
        "Verification time: %lld ms\n"
        "Reason: %s\n",
        solver_result_status_string(result->status),
        solver_confidence_string(result->confidence),
        result->backend_count,
        result->assignment_count,
        result->conflict_count,
        (long long)result->total_time_ms,
        (long long)result->verification_time_ms,
        result->consensus_reason);

    if (written >= (int)buffer_size) return (int)buffer_size;

    /* 各后端详情 */
    for (int i = 0; i < result->backend_count && i < SOLVER_RESULT_MAX_BACKENDS; i++) {
        const SolverBackendResult *br = &result->backend_results[i];
        int n = snprintf(buffer + written, (size_t)(buffer_size - (size_t)written),
            "  Backend[%d]: %s, status=%s, time=%lld ms, assignments=%d\n",
            i,
            smtsolver_backend_type_name(br->backend),
            solver_result_status_string(br->status),
            (long long)br->solve_time_ms,
            br->assignment_count);
        written += n;
        if (written >= (int)buffer_size) return (int)buffer_size;
    }

    /* 冲突详情 */
    for (int i = 0; i < result->conflict_count && i < SOLVER_RESULT_MAX_CONFLICTS; i++) {
        const SolverResultConflict *c = &result->conflicts[i];
        int n = snprintf(buffer + written, (size_t)(buffer_size - (size_t)written),
            "  Conflict[%d]: var=%d, %s vs %s, discrepancy=%.6e\n",
            i, c->var_id,
            smtsolver_backend_type_name(c->backend_a),
            smtsolver_backend_type_name(c->backend_b),
            c->discrepancy);
        written += n;
        if (written >= (int)buffer_size) return (int)buffer_size;
    }

    return written;
}

/* ============================================================
 * 字符串转换 API
 * ============================================================ */

/**
 * @brief 获取结果状态字符串
 */
LV00_PUBLIC_API const char *solver_result_status_string(SolverResultStatus status)
{
    switch (status) {
    case SOLVER_RESULT_UNKNOWN:      return "UNKNOWN";
    case SOLVER_RESULT_SAT:          return "SAT";
    case SOLVER_RESULT_UNSAT:        return "UNSAT";
    case SOLVER_RESULT_PARTIAL:      return "PARTIAL";
    case SOLVER_RESULT_TIMEOUT:      return "TIMEOUT";
    case SOLVER_RESULT_ERROR:        return "ERROR";
    case SOLVER_RESULT_INCONSISTENT: return "INCONSISTENT";
    case SOLVER_RESULT_OUT_OF_SCOPE: return "OUT_OF_SCOPE";
    default:                         return "INVALID";
    }
}

/**
 * @brief 获取置信度等级字符串
 */
LV00_PUBLIC_API const char *solver_confidence_string(SolverConfidenceLevel level)
{
    switch (level) {
    case SOLVER_CONFIDENCE_NONE:    return "NONE";
    case SOLVER_CONFIDENCE_LOW:     return "LOW";
    case SOLVER_CONFIDENCE_MEDIUM:  return "MEDIUM";
    case SOLVER_CONFIDENCE_HIGH:    return "HIGH";
    case SOLVER_CONFIDENCE_CERTAIN: return "CERTAIN";
    default:                        return "INVALID";
    }
}
