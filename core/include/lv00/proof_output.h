/**
 * @file proof_output.h
 * @brief 证明输出、格式化、可视化与验证接口
 *
 * 包含：
 * - 导出功能（HTML/LaTeX/Coq）
 * - 自然语言证明输出（AlphaGeometry 风格）
 * - 证明策略注释（LeanGeo 风格）
 * - Isar 结构化证明导出
 * - HOL Light 微内核验证
 * - F* 精化类型 + SMT 混合验证
 */

#ifndef LV00_PROOF_OUTPUT_H
#define LV00_PROOF_OUTPUT_H

#include "proof_search.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============== 导出功能 ============== */

/**
 * 导出证明为HTML（含交互式导航、SVG时间线、自然语言描述）
 */
LV00_PUBLIC_API bool proof_export_html(ProofNavigator *nav, const char *filepath);

/**
 * 导出证明为LaTeX
 */
LV00_PUBLIC_API bool proof_export_latex(ProofNavigator *nav, const char *filepath);

/**
 * 导出证明为Coq调用序列
 */
LV00_PUBLIC_API bool proof_export_coq(ProofNavigator *nav, const char *filepath);

/* ============== 自然语言证明输出（AlphaGeometry风格） ============== */

/**
 * @brief 自然语言证明输出语言
 */
typedef enum {
    PROOF_NL_LANG_ZH_CN, /**< 简体中文 */
    PROOF_NL_LANG_EN_US  /**< 英文 */
} ProofNaturalLanguage;

/**
 * @brief 将单个证明步骤转换为自然语言描述
 *
 * 借鉴 AlphaGeometry 的人类可读证明输出设计，
 * 每一步都生成完整的自然语言描述，包括：
 * - 应用了什么推理规则
 * - 涉及哪些几何对象
 * - 为什么可以进行这一步
 *
 * @param step        证明步骤
 * @param lang        输出语言
 * @return 新分配的自然语言描述字符串（调用者需用lv00_free释放），失败返回NULL
 */
LV00_PUBLIC_API char *proof_step_get_natural_language(const ProofStep *step, ProofNaturalLanguage lang);

/**
 * @brief 导出完整证明为自然语言文本
 *
 * 生成 AlphaGeometry 风格的人类可读证明：
 * - 首先说明总体证明策略
 * - 然后逐步展开，每一步只应用一条推理规则
 * - 辅助构造附带"为什么"的解释
 * - 从已知条件出发，逐步推导到结论
 *
 * @param nav        证明导航器
 * @param filepath   输出文件路径
 * @param lang       输出语言
 * @return 是否成功
 */
LV00_PUBLIC_API bool proof_export_natural_language(ProofNavigator *nav, const char *filepath, ProofNaturalLanguage lang);

/* ============== 证明策略注释（LeanGeo风格） ============== */

/**
 * @brief 设置证明的总体策略描述
 *
 * 借鉴 LeanGeo 的"先展示总体策略，再展开细节"的呈现方式。
 * 例如："通过作辅助线构造相似三角形，利用角平分线性质完成证明"
 *
 * @param nav            证明导航器
 * @param strategy_note  策略描述（会内部复制）
 * @return 是否成功
 */
LV00_PUBLIC_API bool proof_navigator_set_strategy_note(ProofNavigator *nav, const char *strategy_note);

/**
 * @brief 获取证明的总体策略描述
 *
 * @param nav  证明导航器
 * @return 策略描述字符串（属于导航器，不要释放），未设置返回NULL
 */
LV00_PUBLIC_API const char *proof_navigator_get_strategy_note(const ProofNavigator *nav);

/**
 * @brief 为证明步骤设置自然语言注释
 *
 * 在自动生成的描述之外，允许用户为每个步骤添加自定义注释。
 * 注释在HTML导出和自然语言导出中都会显示。
 *
 * @param step  证明步骤
 * @param note  注释字符串（会内部复制），传NULL清除
 * @return 是否成功
 */
LV00_PUBLIC_API bool proof_step_set_note(ProofStep *step, const char *note);

/* ================================================================
 * Isabelle/HOL — Isar 结构化证明导出
 * ================================================================ */

/**
 * @brief 将证明步骤导出为 Isar 结构化证明文本
 * @param props       命题列表
 * @param prop_count  命题数量
 * @return Isar 格式证明文本（调用者释放）
 */
LV00_PUBLIC_API char *proof_export_isar(const Proposition **props, int prop_count);

/* ================================================================
 * 4. HOL Light — 500 行微内核验证
 * ================================================================ */

/** @brief 验证规则类型（对应 HOL Light 10 条基本推理规则） */
typedef enum {
    VERIFY_ASSUME,    /* ASSUME: t |- t */
    VERIFY_REFL,      /* REFL:   |- t = t */
    VERIFY_BETA_CONV, /* BETA_CONV: |- (\x.t) s = t[s/x] */
    VERIFY_MK_COMB,   /* MK_COMB:  f=g, x=y => f x = g y */
    VERIFY_ABS,       /* ABS:     x not free in Γ => Γ|-s=t => Γ|-(\x.s)=(\x.t) */
    VERIFY_TRANS,     /* TRANS:   s=t, t=u => s=u */
    VERIFY_SUBST,     /* SUBST:   substitution */
    VERIFY_INST_TYPE, /* INST_TYPE: type instantiation */
    VERIFY_INST,      /* INST:    term instantiation */
    VERIFY_DISCH      /* DISCH:   discharge assumption */
} VerifyRuleType;

/** @brief 验证结果 */
typedef enum { VERIFY_VALID, VERIFY_INVALID, VERIFY_UNDECIDED } VerifyResult;

/**
 * @brief 极简验证 — 仅用不超过 10 条基本规则验证一个证明步骤
 * @param rule        应用的推理规则
 * @param premises    前提列表（terminated by NULL）
 * @param conclusion  结论
 * @param out_trace   输出：验证追溯（可选，成功时给出规则链）
 * @return VERIFY_VALID 如果结论可从前提通过给定规则合法推导
 */
LV00_PUBLIC_API VerifyResult proof_minimal_verify(VerifyRuleType rule, const char **premises, const char *conclusion, char **out_trace);

/* ================================================================
 * 5. F* — 精化类型 + SMT 混合验证
 * ================================================================ */

/** @brief 精化检查结果 */
typedef enum { REFINE_OK, REFINE_SMT_UNSAT, REFINE_TYPE_ERROR, REFINE_TIMEOUT } RefinementCheckResult;

/** @brief 精化类型检查条目 */
typedef struct {
    const char *geom_object;     /* 几何对象名 */
    const char *base_type;       /* 基础类型（如 Triangle） */
    const char *refinement_pred; /* 精化谓词（如 "is_right && area > 0"） */
    RefinementCheckResult result;
    char *smt_counterexample; /* SMT 反例（失败时） */
    double LV00_TOLERATED_FLOAT(elapsed_sec); /* @note tolerated: timing only */
} RefinementCheckEntry;

/** @brief 精化类型批量检查报告 */
typedef struct {
    RefinementCheckEntry *entries;
    int entry_count;
    int passed_count;
    int failed_count;
} RefinementCheckReport;

/**
 * @brief 精化类型检查 — 验证几何体是否同时满足类型条件（struct）和精化谓词（SMT）
 * @param solver     约束求解器
 * @param entries    检查条目列表
 * @param count      条目数量
 * @return 批量检查报告
 */
LV00_PUBLIC_API RefinementCheckReport *proof_refinement_check(ConstraintSolver *solver, RefinementCheckEntry *entries, int count);
LV00_PUBLIC_API void refinement_check_report_destroy(RefinementCheckReport *report);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PROOF_OUTPUT_H */
