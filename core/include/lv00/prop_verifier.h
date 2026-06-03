/* ========================================================================
 * 模块名称：命题逻辑验证器 (prop_verifier)
 * 功能概述：Lv-00 自举目标——基于自然演绎风格的命题逻辑证明搜索器。
 *          支持 BHK 解释下的几何构造验证：合取（积类型）、析取（和类型）、
 *          蕴涵（标准函数块）、否定（蕴涵矛盾）。当构造的外部端口通过
 *          合一匹配命题的几何模式时，命题被验证。
 *          包含不可构造性分析和 BHK 几何构造验证桥接。
 *
 * 主要 API：
 *   - prop_verifier_verify                     — 验证命题 sequent
 *   - prop_formula_create_atom / conjunction / ... — 命题构造
 *   - prop_formula_to_string / to_latex        — 命题序列化
 *   - prop_verifier_run_smoke_tests            — 烟测集
 *   - prop_verifier_analyze_inconstructibility — 不可构造性分析
 *   - prop_verifier_bhk_verify                 — BHK 几何构造验证
 *   - prop_verifier_apply_trust_colors         — 信任颜色桥接
 *   - prop_verifier_check_equivalence          — 等价性检查
 *
 * 使用示例：
 *   PropFormula *goal = prop_formula_create_atom("Pythagorean");
 *   VerifyDetail detail = prop_verifier_verify(premises, n, goal, NULL);
 *   if (detail.result == PV_VERIFY_PROVEN) { ... 证明成功 ... }
 *
 * @version 3.3.0
 * ======================================================================== */

/**
 * @file prop_verifier.h
 * @brief 命题逻辑验证器 —— Lv-00 自举目标
 */

#ifndef LV00_PROP_VERIFIER_H
#define LV00_PROP_VERIFIER_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "constraint_graph.h" /* ConstraintGraph 类型（用于信任颜色桥接） */
#include "stream.h"           /* StreamContext（用于流式输出设置） */
#include "symbolic_coord.h"   /* TrustColor 枚举（用于信任颜色桥接） */

/* 确保结构体使用默认对齐（防止 MSYS2/MinGW CRT 头文件的 pragma pack 影响） */
#pragma pack(push, 8)

/**
 * @brief 设置命题验证器的流式输出上下文
 * @param ctx  流式上下文（可为 NULL 以禁用流式输出）
 */
void prop_verifier_set_stream_context(StreamContext *ctx);

/* ============================================================
 * 命题逻辑公式类型
 * ============================================================ */

typedef enum {
    PROP_ATOM,        /* 原子命题 P, Q, R, ... */
    PROP_CONJUNCTION, /* A ∧ B */
    PROP_DISJUNCTION, /* A ∨ B */
    PROP_IMPLICATION, /* A → B */
    PROP_NEGATION,    /* ¬A */
    PROP_BOTTOM,      /* ⊥ (矛盾) */
    PROP_TRUE         /* ⊤ (真) */
} PropFormulaType;

/* 前向声明 */
typedef struct PropFormula PropFormula;

/* 命题逻辑公式 (不可变 AST) */
struct PropFormula {
    PropFormulaType type;
    union {
        struct {
            char name[64];
        } atom; /* PROP_ATOM */
        struct {
            PropFormula *left, *right;
        } binary; /* CONJ/DISJ/IMPL */
        struct {
            PropFormula *operand;
        } unary; /* NEGATION */
        /* PROP_BOTTOM, PROP_TRUE: 无额外数据 */
    } data;
};

/* ============================================================
 * 验证结果
 * ============================================================ */

typedef enum {
    PV_VERIFY_PROVEN,        /* 证明成功（合一匹配） */
    PV_VERIFY_DISPROVEN,     /* 证伪（找到反例） */
    PV_VERIFY_FAILED,        /* 未能证明（搜索空间耗尽） */
    PV_VERIFY_INVALID_INPUT, /* 输入无效 */
    PV_VERIFY_TIMEOUT,       /* 超时 */
    PV_VERIFY_ERROR          /* 内部错误 */
} PropVerifyResult;

/* 验证详情 */
typedef struct {
    PropVerifyResult result;
    int steps_used;          /* 使用的推理步数 */
    int max_steps;           /* 最大步数限制 */
    char error_message[256]; /* 错误信息 */
    /* 证明成功时的构造摘要 */
    char construction_summary[512];
} VerifyDetail;

/* ============================================================
 * 验证器配置
 * ============================================================ */

typedef struct {
    int max_steps;           /* 最大推理步数 (默认 10000) */
    bool use_intuitionistic; /* 使用直觉主义逻辑 (默认 true) */
    bool enable_ex_falso;    /* 启用爆炸原理 (默认 false) */
    int timeout_ms;          /* 超时毫秒数 (默认 30000) */
} VerifierConfig;

#define VERIFIER_CONFIG_DEFAULT {10000, true, false, 30000}

/* ============================================================
 * 核心验证函数
 * ============================================================ */

/**
 * @brief 验证命题 sequent: premises ⊢ goal
 *
 * 使用自然演绎风格的向后链接证明搜索。
 * 在直觉主义模式下，不使用 RAA（归谬法）。
 * 当 enable_ex_falso 为 true 时，允许从 ⊥ 推出任意命题。
 *
 * @param premises      前提公式数组
 * @param premise_count 前提数量
 * @param goal          目标公式
 * @param config        验证器配置（可为 NULL 使用默认值）
 * @return VerifyDetail 验证结果详情
 */
VerifyDetail prop_verifier_verify(const PropFormula **premises, int premise_count, const PropFormula *goal,
                                  const VerifierConfig *config);

/* ============================================================
 * 命题构造/销毁
 * ============================================================ */

PropFormula *prop_formula_create_atom(const char *name);
PropFormula *prop_formula_create_conjunction(PropFormula *left, PropFormula *right);
PropFormula *prop_formula_create_disjunction(PropFormula *left, PropFormula *right);
PropFormula *prop_formula_create_implication(PropFormula *left, PropFormula *right);
PropFormula *prop_formula_create_negation(PropFormula *operand);
PropFormula *prop_formula_create_bottom(void);
PropFormula *prop_formula_create_true(void);
PropFormula *prop_formula_copy(const PropFormula *f);
void prop_formula_destroy(PropFormula *f);

/* ============================================================
 * 命题序列化
 * ============================================================ */

/**
 * @brief 将公式转为可读字符串
 * @return 新分配的字符串，需调用者 free
 */
char *prop_formula_to_string(const PropFormula *f);

/**
 * @brief 将公式转为 LaTeX 字符串
 * @return 新分配的字符串，需调用者 free
 */
char *prop_formula_to_latex(const PropFormula *f);

/* ============================================================
 * 烟测集
 * ============================================================ */

typedef struct {
    const PropFormula *premises[8];
    int premise_count;
    const PropFormula *goal;
    bool expected_provable; /* 预期是否可证 */
    const char *description;
} SmokeTest;

/**
 * @brief 运行自定义烟测集
 * @param tests      测试用例数组
 * @param test_count 测试数量
 * @param results    输出结果数组（至少 test_count 个元素）
 * @return 通过的测试数
 */
int prop_verifier_run_smoke_tests(const SmokeTest *tests, int test_count, VerifyDetail *results);

/**
 * @brief 运行内置烟测集
 * @param results 输出结果数组
 * @return 通过的测试数
 */
int prop_verifier_run_builtin_smoke_tests(VerifyDetail *results);

/**
 * @brief 获取内置烟测数量
 * @return 内置烟测数量
 */
int prop_verifier_builtin_smoke_test_count(void);

/* ============================================================
 * 不可构造性分析
 * ============================================================ */

/**
 * @brief 不可构造性分析结果
 */
typedef struct {
    bool is_inconstructible;     /* 是否判定为不可构造 */
    char reason[512];            /* 不可构造原因描述 */
    int failed_subgoals;         /* 失败的子目标数量 */
    char **subgoal_descriptions; /* 各失败子目标的描述（调用者 free） */
    int subgoal_desc_count;      /* 子目标描述数量 */
} InconstructibilityAnalysis;

/**
 * @brief 分析命题的不可构造性
 *
 * 当 prop_verifier_verify 返回 PV_VERIFY_FAILED 时，可调用此函数
 * 获取详细的不可构造性分析报告。分析包括：
 *   - 失败的子目标列表及其描述
 *   - 不可构造性的原因分类（直觉主义限制、缺少前提等）
 *   - BHK 解释下的几何构造缺口分析
 *
 * @param premises      前提公式数组
 * @param premise_count 前提数量
 * @param goal          目标公式
 * @param config        验证器配置（可为 NULL 使用默认值）
 * @return InconstructibilityAnalysis 分析结果（调用者负责释放 subgoal_descriptions）
 */
InconstructibilityAnalysis prop_verifier_analyze_inconstructibility(const PropFormula **premises, int premise_count,
                                                                    const PropFormula *goal,
                                                                    const VerifierConfig *config);

/**
 * @brief 释放不可构造性分析结果
 *
 * @param analysis 分析结果指针
 */
void prop_verifier_free_analysis(InconstructibilityAnalysis *analysis);

/* ============================================================
 * BHK 几何构造验证桥接
 * ============================================================ */

/**
 * @brief BHK 构造验证结果
 *
 * 将命题逻辑验证结果映射到 Lv-00 几何构造系统的信任颜色。
 * 基于 BHK (Brouwer-Heyting-Kolmogorov) 解释：
 *   - A∧B 的证物：一对证物 (a, b)
 *   - A∨B 的证物：一个证物附带其来源标记（左/右）
 *   - A→B 的证物：一个将 A 的证物转换为 B 的证物的构造
 *   - ¬A 的证物：一个将 A 的证物转换为 ⊥ 的构造
 *   - ⊥ 的证物：不存在（不可构造）
 */
typedef struct {
    bool verified;                /* 是否通过 BHK 验证 */
    char bhk_interpretation[512]; /* BHK 解释描述 */
    char geometric_mapping[512];  /* 几何映射描述（函数块类型） */
    int missing_constructions;    /* 缺少的构造数量 */
    char **missing_descriptions;  /* 缺少构造的描述列表（调用者 free） */
    int missing_count;            /* 缺少构造描述数量 */
} BHKVerificationResult;

/**
 * @brief 执行 BHK 几何构造验证
 *
 * 在命题逻辑验证成功后，进一步验证证明是否满足 BHK 解释下的
 * 几何构造要求。此函数不执行实际的几何合一检查（那是 proof.c 的职责），
 * 而是在命题逻辑层面分析证明的构造性结构。
 *
 * @param premises      前提公式数组
 * @param premise_count 前提数量
 * @param goal          目标公式
 * @param config        验证器配置（可为 NULL 使用默认值）
 * @return BHKVerificationResult BHK 验证结果（调用者负责释放 missing_descriptions）
 */
BHKVerificationResult prop_verifier_bhk_verify(const PropFormula **premises, int premise_count, const PropFormula *goal,
                                               const VerifierConfig *config);

/**
 * @brief 释放 BHK 验证结果
 *
 * @param result BHK 验证结果指针
 */
void prop_verifier_free_bhk_result(BHKVerificationResult *result);

/* ============================================================
 * 信任颜色桥接 —— BHK验证结果 → 约束图 TrustColor
 * ============================================================ */

/**
 * @brief 将 BHK 几何构造验证结果应用到约束图的信任颜色
 *
 * 运行 BHK 验证并基于验证结果自动设置约束图中所有节点的符号坐标信任颜色。
 *
 * 映射规则（从 BHK 验证到 TrustColor）：
 *   - PV_VERIFY_PROVEN + missing_constructions == 0 → TRUST_GREEN（完全构造性，可信）
 *   - PV_VERIFY_PROVEN + missing_constructions <= 2 → TRUST_YELLOW（少量缺失，条件可信）
 *   - PV_VERIFY_PROVEN + missing_constructions >= 3 → TRUST_AMBER（显著缺失，需关注）
 *   - PV_VERIFY_FAILED（搜索耗尽但未证伪）→ TRUST_BLUE（未确定）
 *   - PV_VERIFY_DISPROVEN → TRUST_RED（已证伪，不可信）
 *   - PV_VERIFY_TIMEOUT / PV_VERIFY_ERROR → TRUST_BLUE（未确定）
 *
 * 对于每个节点，其所有坐标被设置为相同的信任颜色（节点级信任色）。
 * 每个颜色变更通过流式系统发射 STREAM_EVENT_PROOF_COLOR_UPDATE 事件。
 *
 * @param graph          目标约束图（不能为 NULL）
 * @param premises       前提公式数组
 * @param premise_count  前提数量
 * @param goal           目标公式
 * @param config         验证器配置（可为 NULL 使用默认值）
 * @param out_result     输出 BHK 验证详细结果（可为 NULL，调用者负责释放）
 * @return 成功更新的节点数（>=0），-1 表示参数错误
 */
int prop_verifier_apply_trust_colors(ConstraintGraph *graph, const PropFormula **premises, int premise_count,
                                     const PropFormula *goal, const VerifierConfig *config,
                                     BHKVerificationResult *out_result);

/* ============================================================
 * 命题等价性检查
 * ============================================================ */

/**
 * @brief 检查两个公式是否逻辑等价（双向蕴涵可证）
 *
 * @param a       第一个公式
 * @param b       第二个公式
 * @param config  验证器配置（可为 NULL 使用默认值）
 * @return true 两个公式逻辑等价
 */
bool prop_verifier_check_equivalence(const PropFormula *a, const PropFormula *b, const VerifierConfig *config);

/**
 * @brief 检查公式是否为永真式（无前提即可证）
 *
 * @param f       公式
 * @param config  验证器配置（可为 NULL 使用默认值）
 * @return true 公式是永真式
 */
bool prop_verifier_check_tautology(const PropFormula *f, const VerifierConfig *config);

/* 恢复默认对齐 */
#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif /* LV00_PROP_VERIFIER_H */
