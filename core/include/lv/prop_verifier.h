#ifndef lv_PROP_VERIFIER_H
#define lv_PROP_VERIFIER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "constraint_graph.h"
#include "stream.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef lv_PUBLIC_API
#define lv_PUBLIC_API
#endif

lv_PUBLIC_API void prop_verifier_set_stream_context(StreamContext *ctx);
lv_PUBLIC_API StreamContext *prop_verifier_get_stream_context(void);

/* ================================================================
 * 命题公式类型枚举
 * ================================================================ */
typedef enum {
    PROP_ATOM         = 0,
    PROP_CONJUNCTION  = 1,
    PROP_DISJUNCTION  = 2,
    PROP_IMPLICATION  = 3,
    PROP_NEGATION     = 4,
    PROP_BOTTOM       = 5,
    PROP_TRUE         = 6
} PropFormulaType;

/* ================================================================
 * 命题公式结构体
 * ================================================================ */
typedef struct PropFormula {
    PropFormulaType type;
    union {
        /* PROP_ATOM */
        struct { char name[64]; } atom;
        /* PROP_CONJUNCTION / PROP_DISJUNCTION / PROP_IMPLICATION */
        struct { struct PropFormula *left; struct PropFormula *right; } binary;
        /* PROP_NEGATION */
        struct { struct PropFormula *operand; } unary;
    } data;
} PropFormula;

/* ================================================================
 * 验证结果枚举
 * ================================================================ */
#ifndef VERIFY_RESULT_DEFINED
#define VERIFY_RESULT_DEFINED
typedef enum {
    VERIFY_INVALID_INPUT = 0,
    VERIFY_PROVEN        = 1,
    VERIFY_FAILED        = 2,
    VERIFY_TIMEOUT       = 3,
    VERIFY_DISPROVEN     = 4,
    VERIFY_ERROR         = 5
} VerifyResult;
#endif

/* ================================================================
 * 验证详情结构体
 * ================================================================ */
typedef struct {
    VerifyResult result;
    char error_message[256];
    char construction_summary[256];
    int steps_used;
    int max_steps;
    bool proven;
    TrustColor trust_color;
} VerifyDetail;

/* ================================================================
 * 验证器配置结构体
 * ================================================================ */
typedef struct {
    bool enable_ex_falso;      /**< 启用爆炸原理 (ex falso quodlibet) */
    bool use_intuitionistic;  /**< 使用直觉主义逻辑模式（禁止反证法）*/
    int timeout_ms;           /**< 验证超时（毫秒），0 表示无限制 */
    int max_steps;            /**< 最大证明步数，0 表示使用默认值 */
} VerifierConfig;

/** 验证器默认配置（全启用，直觉主义关闭，超时 5s） */
#define VERIFIER_CONFIG_DEFAULT \
    { true, false, 5000, 10000 }

/* ================================================================
 * BHK 验证结果结构体
 * ================================================================ */
typedef struct {
    bool verified;
    char bhk_interpretation[256];
    char geometric_mapping[256];
    int missing_constructions;
    int missing_count;
    char **missing_descriptions;
} BHKVerificationResult;

/* ================================================================
 * 不可构成性分析结果结构体
 * ================================================================ */
typedef struct {
    bool is_inconstructible;
    char reason[512];
    int failed_subgoals;
    char **subgoal_descriptions;
    int subgoal_desc_count;
} InconstructibilityAnalysis;

/* ================================================================
 * 冒烟测试结构体
 * ================================================================ */
typedef struct {
    PropFormula *premises[8];     /**< 前提公式指针数组 */
    int premise_count;             /**< 有效前提数量 */
    PropFormula *goal;             /**< 目标公式 */
    bool expected_provable;        /**< 是否预期可证 */
    const char *description;       /**< 测试描述 */
} SmokeTest;

/* ================================================================
 * 命题公式构造函数
 * ================================================================ */
PropFormula *prop_formula_create_atom(const char *name);
PropFormula *prop_formula_create_conjunction(PropFormula *left, PropFormula *right);
PropFormula *prop_formula_create_disjunction(PropFormula *left, PropFormula *right);
PropFormula *prop_formula_create_implication(PropFormula *left, PropFormula *right);
PropFormula *prop_formula_create_negation(PropFormula *operand);
PropFormula *prop_formula_create_bottom(void);
PropFormula *prop_formula_create_true(void);
PropFormula *prop_formula_copy(const PropFormula *f);
void prop_formula_destroy(PropFormula *f);
char *prop_formula_to_string(const PropFormula *f);
char *prop_formula_to_latex(const PropFormula *f);

/* ================================================================
 * 核心验证 API
 * ================================================================ */
VerifyDetail prop_verifier_verify(
    const PropFormula **premises,
    int premise_count,
    const PropFormula *goal,
    const VerifierConfig *config);

/* ================================================================
 * 冒烟测试 API
 * ================================================================ */
int prop_verifier_builtin_smoke_test_count(void);
int prop_verifier_run_builtin_smoke_tests(VerifyDetail *results);
int prop_verifier_run_smoke_tests(const SmokeTest *tests, int test_count,
                                  VerifyDetail *results);

/* ================================================================
 * BHK 验证 API
 * ================================================================ */
BHKVerificationResult prop_verifier_bhk_verify(
    const PropFormula **premises,
    int premise_count,
    const PropFormula *goal,
    const VerifierConfig *config);
void prop_verifier_free_bhk_result(BHKVerificationResult *result);

/* ================================================================
 * 不可构成性分析 API
 * ================================================================ */
InconstructibilityAnalysis prop_verifier_analyze_inconstructibility(
    const PropFormula **premises,
    int premise_count,
    const PropFormula *goal,
    const VerifierConfig *config);
void prop_verifier_free_analysis(InconstructibilityAnalysis *analysis);

/* ================================================================
 * 辅助 API
 * ================================================================ */
int prop_verifier_apply_trust_colors(
    ConstraintGraph *graph,
    const PropFormula **premises,
    int premise_count,
    const PropFormula *goal,
    const VerifierConfig *config,
    BHKVerificationResult *out_result);
bool prop_verifier_check_equivalence(const PropFormula *a, const PropFormula *b,
                                    const VerifierConfig *config);
bool prop_verifier_check_tautology(const PropFormula *f,
                                  const VerifierConfig *config);

/* ================================================================
 * 遗留简单 API（保留以兼容旧代码）
 * ================================================================ */
typedef struct PropVerifierResult { bool valid; const char *msg; } PropVerifierResult;
PropVerifierResult lv_prop_verify(const void *prop);

#ifdef __cplusplus
}
#endif

#endif /* lv_PROP_VERIFIER_H */
