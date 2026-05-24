/**
 * @file proof.h
 * @brief 命题与证明系统 - 合一检查、证明导航器、证明步骤
 *
 * 根据 Lv-00 设计文档第10节实现：
 * - 命题模式定义
 * - 合一检查（Unify）
 * - 命题的等价变换
 * - 命题的实例化
 * - ⊥的公理包可定义性
 * - 爆炸原理
 * - 证明导航器
 *
 * 【中文模块说明】
 * proof.h 是 Lv-00 系统的证明引擎核心模块，提供完整的几何证明框架。
 * 主要功能包括：
 * - 命题管理：创建、销毁、设置端口/模式/前置条件/后置条件
 * - 合一检查：将构造图与命题模式进行匹配验证
 * - 证明导航器：管理证明步骤的添加、导航、断点管理
 * - 证明依赖链：追踪证明步骤间的依赖关系和信任颜色
 * - 爆炸原理（Ex Falso）：从矛盾推导任意命题
 * - 反证法证明：假设目标否定，推导矛盾以证明原命题
 * - 自然语言输出：AlphaGeometry 风格的人类可读证明文本
 * - 策略注释：LeanGeo 风格的"先展示总体策略，再展开细节"
 * - 回溯搜索树：Newclid 风格的证明搜索可视化
 * - 多策略引擎：JGEX 风格的多证明方法共存（面积法、Groebner基法、向量法等）
 * - 不可构造性证明：三等分角、倍立方等经典不可构造问题的检测
 * - 参考项目 API：借鉴 Agda（洞填充）、Idris 2（QTT）、Isabelle（Sledgehammer）、
 *   HOL Light（微内核验证）、F*（精化类型）的证明功能
 */

#ifndef LV00_PROOF_H
#define LV00_PROOF_H

#include <stdbool.h>

#include "constraint_graph.h"
#include "exact_arithmetic.h" /* LV00_TOLERATED_FLOAT for proof timing/thresholds */
#include "normalization.h"
#include "stream.h"
#include "type_system.h"
#include "unify.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 设置证明系统的流式输出上下文
 * @param ctx  流式上下文（可为 NULL 以禁用流式输出）
 */
void proof_set_stream_context(StreamContext *ctx);

/* ============== 前向声明 ============== */
typedef struct Proposition Proposition;
typedef struct ProofStep ProofStep;
typedef struct ProofNavigator ProofNavigator;
typedef struct ProofDependency ProofDependency;
typedef struct PropositionEquivalence PropositionEquivalence;
typedef struct BottomDefinition BottomDefinition;
typedef struct LV00Engine LV00Engine; /* 引擎前向声明 */

/* ============== 证明状态颜色 ============== */
typedef enum {
    PROOF_COLOR_GREEN,             /* 全构造，无任何非常规依赖 */
    PROOF_COLOR_BLUE_UNEXPLORED,   /* 蓝色（未探索） */
    PROOF_COLOR_BLUE_RESOURCE,     /* 蓝色（资源受限） */
    PROOF_COLOR_BLUE_OUT_OF_RANGE, /* 蓝色（超出范围） */
    PROOF_COLOR_GREEN_VERIFIED,    /* 绿色实框：已证不可构造 */
    PROOF_COLOR_YELLOW,            /* 黄色虚线框：条件性不可构造 */
    PROOF_COLOR_ORANGE_ORACLE,     /* 浅橙色实心端口：依赖非构造性oracle */
    PROOF_COLOR_ORANGE_EX_FALSO,   /* 浅橙色虚线箭头：爆炸原理步骤 */
    PROOF_COLOR_AMBER,             /* 橙黄色：含数值假设 */
    PROOF_COLOR_DARK_ORANGE        /* 深橙色：非构造性依赖与数值假设叠加 */
} ProofColor;

/* ============== 命题类型 ============== */
typedef enum {
    PROPOSITION_ATOMIC,      /* 原子命题 */
    PROPOSITION_CONJUNCTION, /* 合取 ∧ */
    PROPOSITION_DISJUNCTION, /* 析取 ∨ */
    PROPOSITION_IMPLICATION, /* 蕴含 → */
    PROPOSITION_NEGATION,    /* 否定 ¬ */
    PROPOSITION_UNIVERSAL,   /* 全称 ∀ */
    PROPOSITION_EXISTENTIAL, /* 存在 ∃ */
    PROPOSITION_BOTTOM       /* 矛盾 ⊥ */
} PropositionType;

/* ============== 命题模式 ============== */
struct Proposition {
    int id;               /* 命题ID */
    PropositionType type; /* 命题类型 */
    ProofColor color;     /* 证明状态颜色 */
    char *label;          /* 命题标签（可空） */

    /* 输入/输出端口 */
    int *input_port_ids;  /* 输入端口ID数组 */
    int input_count;      /* 输入端口数量 */
    int *output_port_ids; /* 输出端口ID数组 */
    int output_count;     /* 输出端口数量 */

    /* 几何模式（虚线框内的约束骨架） */
    ConstraintGraph *pattern; /* 命题模式图 */

    /* 前置条件区域 */
    int *precondition_region_ids; /* 前置条件区域ID */
    int precondition_count;       /* 前置条件数量 */

    /* 后置条件 */
    int *postcondition_constraint_ids; /* 后置条件约束ID */
    int postcondition_count;           /* 后置条件数量 */

    /* 子命题（用于复合命题） */
    Proposition **sub_props; /* 子命题数组 */
    int sub_prop_count;      /* 子命题数量 */

    /* 类型信息 */
    TypeRegion *prop_type; /* 命题类型 */

    /* 元数据 */
    char *name;        /* 命题名称 */
    char *description; /* 描述 */
};

/* ============== 证明步骤类型 ============== */
typedef enum {
    PROOF_STEP_ADD_NODE,       /* 添加节点 */
    PROOF_STEP_ADD_CONSTRAINT, /* 添加约束 */
    PROOF_STEP_REWRITE,        /* 重写步骤 */
    PROOF_STEP_FUNCTION_APP,   /* 函数应用 */
    PROOF_STEP_PACK_FUNCTION,  /* 打包函数块 */
    PROOF_STEP_NORMALIZATION,  /* 自动规范化 */
    PROOF_STEP_UNIFY,          /* 合一检查 */
    PROOF_STEP_EX_FALSO,       /* 爆炸原理步骤 */
    PROOF_STEP_ORACLE          /* Oracle依赖 */
} ProofStepType;

/* ============== 证明步骤 ============== */
struct ProofStep {
    int id;             /* 步骤ID */
    ProofStepType type; /* 步骤类型 */
    ProofColor color;   /* 步骤颜色 */

    /* 步骤数据 */
    int node_id;       /* 相关节点ID */
    int constraint_id; /* 相关约束ID */
    int rule_id;       /* 相关规则ID */
    int func_block_id; /* 相关函数块ID */

    /* 规范化步骤数据 */
    int *merged_node_ids; /* 被合并的节点ID */
    int merged_count;     /* 被合并的节点数量 */
    int retained_node_id; /* 保留的节点ID */

    /* 依赖关系 */
    int *dependency_step_ids; /* 依赖的前驱步骤ID */
    int dependency_count;     /* 依赖数量 */
    int *dependent_step_ids;  /* 被依赖的后继步骤ID */
    int dependent_count;      /* 被依赖数量 */

    /* 状态 */
    bool is_breakpoint; /* 是否为断点 */
    bool is_completed;  /* 是否完成 */
    char *note;         /* 用户注释 */

    /* 时间戳 */
    int64_t timestamp; /* 步骤时间戳 */
};

/* ============== 证明依赖链 ============== */
struct ProofDependency {
    int id;           /* 依赖ID */
    ProofColor color; /* 依赖颜色 */

    /* 依赖来源 */
    enum {
        DEP_SOURCE_DIRECT,   /* 直接构造 */
        DEP_SOURCE_LEMMA,    /* 引理引用 */
        DEP_SOURCE_ORACLE,   /* 非构造性Oracle */
        DEP_SOURCE_EX_FALSO, /* 爆炸原理 */
        DEP_SOURCE_NUMERIC   /* 数值假设 */
    } source;

    /* 引理引用 */
    int lemma_id;       /* 引理ID */
    char *content_hash; /* 内容哈希 */

    /* 外部引用 */
    char *external_ref; /* 外部引用字符串 */

    /* 数值假设声明 */
    char *numeric_declaration;  /* 数值假设声明 */
    double LV00_TOLERATED_FLOAT(precision_threshold); /* 精度阈值
                                                        * @note LV00_TOLERATED_FLOAT:
                                                        * 阈值用于证明规则参数化，不参与代数计算 */

    /* 子依赖 */
    ProofDependency **sub_deps; /* 子依赖数组 */
    int sub_dep_count;          /* 子依赖数量 */
};

/* ============== 引理块折叠 ============== */

/**
 * @brief 引理视图状态
 */
typedef enum {
    LEMMA_VIEW_EXPANDED, /* 展开 */
    LEMMA_VIEW_COLLAPSED /* 折叠 */
} LemmaViewState;

/* ============== 证明导航器 ============== */
struct ProofNavigator {
    ProofStep **steps; /* 证明步骤数组 */
    int step_count;    /* 步骤数量 */
    int current_step;  /* 当前步骤索引 */

    Proposition *target_prop;      /* 目标命题 */
    ConstraintGraph *construction; /* 构造图 */

    ProofDependency *dep_tree; /* 依赖树 */

    /* 导航状态 */
    bool is_complete;       /* 证明是否完成 */
    ProofColor final_color; /* 最终颜色 */

    /* 断点管理 */
    int *breakpoint_indices; /* 断点索引数组 */
    int breakpoint_count;    /* 断点数量 */

    /* 命题等价表 */
    PropositionEquivalence *equivalences; /* 等价命题数组 */
    int equivalence_count;                /* 等价命题数量 */
    int equivalence_capacity;             /* 等价命题容量 */

    /* ⊥ 的定义 */
    BottomDefinition *bottom_def; /* 矛盾定义（动态分配） */

    /* 引理视图状态 */
    int *lemma_view_step_ids;          /* 引理步骤ID数组 */
    LemmaViewState *lemma_view_states; /* 引理视图状态数组 */
    int lemma_view_count;              /* 引理视图状态数量 */
    int lemma_view_capacity;           /* 引理视图状态容量 */

    /* 引擎上下文（用于访问已加载的公理包等） */
    LV00Engine *engine;

    /* 证明策略注释（LeanGeo风格：先展示总体策略，再展开细节） */
    char *strategy_note; /* 总体策略描述 */
};

/* Proof 类型——与 ProofNavigator 相同 */
typedef ProofNavigator Proof;

/* ============== 命题管理API ============== */

/**
 * @brief 创建命题
 *
 * 分配并初始化一个新的命题实例。新命题的所有字段均初始化为零/NULL，
 * 调用者需通过 proposition_set_* 系列函数设置具体内容。
 *
 * @param[in] id    命题ID（唯一标识符）
 * @param[in] type  命题类型（原子、合取、析取等）
 * @return 新创建的命题指针，失败返回 NULL
 */
Proposition *proposition_create(int id, PropositionType type);

/**
 * 销毁命题
 */
void proposition_destroy(Proposition *prop);

/**
 * 设置输入端口
 */
bool proposition_set_input_ports(Proposition *prop, const int *port_ids, int count);

/**
 * 设置输出端口
 */
bool proposition_set_output_ports(Proposition *prop, const int *port_ids, int count);

/**
 * 设置模式图
 */
bool proposition_set_pattern(Proposition *prop, ConstraintGraph *pattern);

/**
 * 设置前置条件
 */
bool proposition_set_preconditions(Proposition *prop, const int *region_ids, int count);

/**
 * 设置后置条件
 */
bool proposition_set_postconditions(Proposition *prop, const int *constraint_ids, int count);

/**
 * 添加子命题
 */
bool proposition_add_sub_proposition(Proposition *parent, Proposition *child);

/* ============== 合一检查 ============== */

/**
 * 执行合一检查
 * @param construction 构造图
 * @param proposition 命题模式
 * @param normalize_first 是否先执行图规范化遍
 * @return 合一结果
 */
UnifyStatus proof_unify(const ConstraintGraph *construction, const Proposition *proposition, bool normalize_first);

/**
 * 合一检查（详细版）
 * @param construction 构造图
 * @param proposition 命题模式
 * @param out_mismatch_info 输出不匹配信息
 * @return 合一结果
 */
UnifyStatus proof_unify_detailed(const ConstraintGraph *construction, const Proposition *proposition, char **out_mismatch_info);

/* ============== 证明步骤管理 ============== */

/**
 * @brief 创建证明步骤
 *
 * 分配并初始化一个新的证明步骤实例，类型由参数指定。
 * 新步骤的所有字段均初始化为零/NULL。
 *
 * @param[in] type 证明步骤类型（添加节点、重写、合一检查等）
 * @return 新创建的证明步骤指针，失败返回 NULL
 */
ProofStep *proof_step_create(ProofStepType type);

/**
 * @brief 销毁证明步骤
 *
 * 释放证明步骤及其所有动态分配的资源（依赖数组、合并节点数组、注释等）。
 *
 * @param[in] step 证明步骤指针（可为 NULL，此时函数无操作）
 */
void proof_step_destroy(ProofStep *step);

/**
 * 添加依赖关系
 */
bool proof_step_add_dependency(ProofStep *step, int dep_step_id);

/**
 * 设置断点
 */
void proof_step_set_breakpoint(ProofStep *step, bool is_breakpoint);

/* ============== 证明导航器 ============== */

/**
 * 创建证明导航器
 * @param target 目标命题
 * @param engine 引擎上下文（可为NULL，但推荐提供以支持完整功能）
 */
ProofNavigator *proof_navigator_create(Proposition *target, LV00Engine *engine);

/**
 * 销毁证明导航器
 */
void proof_navigator_destroy(ProofNavigator *nav);

/**
 * 添加证明步骤
 */
bool proof_navigator_add_step(ProofNavigator *nav, ProofStep *step);

/**
 * 导航到下一步
 */
bool proof_navigator_next(ProofNavigator *nav);

/**
 * 导航到上一步
 */
bool proof_navigator_prev(ProofNavigator *nav);

/**
 * 跳转到指定步骤
 */
bool proof_navigator_goto(ProofNavigator *nav, int step_index);

/**
 * 跳转到下一个断点
 */
bool proof_navigator_next_breakpoint(ProofNavigator *nav);

/**
 * 获取当前步骤
 */
ProofStep *proof_navigator_current_step(ProofNavigator *nav);

/**
 * 计算最终颜色
 */
ProofColor proof_navigator_compute_final_color(ProofNavigator *nav);

/* ============== 证明依赖链 ============== */

/**
 * 创建证明依赖
 */
ProofDependency *proof_dependency_create(ProofColor color);

/**
 * 销毁证明依赖
 */
void proof_dependency_destroy(ProofDependency *dep);

/**
 * 添加子依赖
 */
bool proof_dependency_add_sub(ProofDependency *parent, ProofDependency *child);

/**
 * 计算依赖链颜色
 */
ProofColor proof_dependency_compute_color(ProofDependency *dep);

/* ============== 爆炸原理 ============== */

/**
 * 创建爆炸原理函数块
 * @param graph 约束图
 * @param out_block_id 输出的函数块ID
 * @return 是否成功
 */
bool proof_create_ex_falso_block(ConstraintGraph *graph, int *out_block_id);

/**
 * 应用爆炸原理
 * @param nav 证明导航器
 * @param bottom_proof ⊥的证物
 * @param target_prop 目标命题
 * @return 是否成功
 */
bool proof_apply_ex_falso(ProofNavigator *nav, ConstraintGraph *bottom_proof, Proposition *target_prop);

/* ============== 反证法证明 ============== */

/**
 * @brief 反证法证明结果 —— 包含矛盾路径和证明追踪树
 *
 * 当反证法成功时，该结构记录完整的矛盾推导路径。
 * 失败的证明也记录尝试的路径，用于调试和学习。
 */
typedef struct Lv00ProofTree Lv00ProofTree; /* 前向声明，完整定义见 proof_trace.h */

typedef struct {
    bool              success;           /**< 反证法是否成功 */
    char             *contradiction_desc; /**< 矛盾的描述（如"P ∧ ¬P 同时成立"） */
    int               contradiction_step; /**< 发现矛盾的步骤索引（-1 = 未发现） */
    struct Lv00ProofTree *proof_trace;    /**< 完整的证明追踪树（成功时记录完整路径，失败时也记录已探索路径） */
    int               total_steps;       /**< 反证法证明的总步骤数 */
    int               forward_steps;     /**< 正向推理步骤数 */
    char             *error_message;     /**< 错误消息（失败时有效，可为 NULL） */
} Lv00ContradictionResult;

/**
 * @brief 执行反证法证明
 *
 * 反证法（归谬法）工作流程：
 * 1. 假设目标命题的否定成立（¬goal）
 * 2. 将否定假设作为临时前提加入证明环境
 * 3. 正向推理：从否定的假设出发，尽可能多地推导出结论
 * 4. 矛盾检测：检查推导出的结论是否与已知公理或已证定理冲突
 * 5. 如果发现矛盾，则反证法成功，原命题得证
 * 6. 记录整个矛盾推导路径到 proof_trace 中
 *
 * 关键设计原则：
 * - 矛盾分支与主证明隔离：否定假设只在矛盾分支内有效，
 *   不会污染主证明上下文。使用独立的 ProofNavigator 实例。
 * - 记录完整路径：成功和失败的情况都记录已探索的推导路径，
 *   方便用户理解证明过程和排查失败原因。
 *
 * @param nav         主证明导航器（不会被修改，仅用于获取引擎上下文和已证定理）
 * @param goal_prop   待证明的目标命题
 * @param max_steps   最大允许的正向推理步骤数（0 = 无限制）
 * @return 反证法结果，包含成功标志和矛盾路径。调用者需用 lv00_contradiction_result_destroy 释放。
 */
Lv00ContradictionResult *lv00_proof_by_contradiction(ProofNavigator *nav, const Proposition *goal_prop, int max_steps);

/**
 * @brief 释放反证法结果
 *
 * 释放 Lv00ContradictionResult 中所有动态分配的内存，
 * 包括证明追踪树、矛盾描述和错误消息。
 *
 * @param result  反证法结果（可为 NULL）
 */
void lv00_contradiction_result_destroy(Lv00ContradictionResult *result);

/**
 * 交互式证明步骤
 * 允许用户引导证明构建
 * @param nav 证明导航器
 * @param step_type 步骤类型
 * @param step_data 步骤数据（类型取决于 step_type）
 * @return true 成功，false 验证失败
 */
bool proof_interactive_step(ProofNavigator *nav, ProofStepType step_type, const void *step_data);

/**
 * 保存证明断点
 * 在指定断点ID处保存当前证明状态，以便后续继续
 * @param nav 证明导航器
 * @param breakpoint_id 断点ID
 * @return true 成功，false 失败
 */
bool proof_save_breakpoint(ProofNavigator *nav, int breakpoint_id);

/**
 * 恢复证明断点
 * 从指定断点ID处恢复之前保存的证明状态
 * @param nav 证明导航器
 * @param breakpoint_id 断点ID
 * @return true 成功，false 失败
 */
bool proof_restore_breakpoint(ProofNavigator *nav, int breakpoint_id);

/* ============== 导出功能 ============== */

/**
 * 导出证明为HTML（含交互式导航、SVG时间线、自然语言描述）
 */
bool proof_export_html(ProofNavigator *nav, const char *filepath);

/**
 * 导出证明为LaTeX
 */
bool proof_export_latex(ProofNavigator *nav, const char *filepath);

/**
 * 导出证明为Coq调用序列
 */
bool proof_export_coq(ProofNavigator *nav, const char *filepath);

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
char *proof_step_get_natural_language(const ProofStep *step, ProofNaturalLanguage lang);

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
bool proof_export_natural_language(ProofNavigator *nav, const char *filepath, ProofNaturalLanguage lang);

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
bool proof_navigator_set_strategy_note(ProofNavigator *nav, const char *strategy_note);

/**
 * @brief 获取证明的总体策略描述
 *
 * @param nav  证明导航器
 * @return 策略描述字符串（属于导航器，不要释放），未设置返回NULL
 */
const char *proof_navigator_get_strategy_note(const ProofNavigator *nav);

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
bool proof_step_set_note(ProofStep *step, const char *note);

/* ============== 命题的等价变换 ============== */

/**
 * @brief 命题等价声明
 */
typedef struct PropositionEquivalence {
    int prop_a_id;
    int prop_b_id;
    ConstraintGraph *transformation; /* 双向变换规则 */
} PropositionEquivalence;

/**
 * @brief 声明两个命题等价
 */
void proof_declare_proposition_equivalence(ProofNavigator *nav, int prop_a_id, int prop_b_id);

/**
 * @brief 查找命题的等价命题
 */
int proof_find_equivalent_proposition(const ProofNavigator *nav, int prop_id, int *equivalent_ids, int max_count);

/* ============== 依赖链断裂自动降级 ============== */

/**
 * @brief 依赖更新结果
 *
 * 当公理包升级后，重新验证所有内引用。
 * 若内容哈希变化，自动降级信任颜色。
 */
typedef struct {
    int dependency_id;
    ProofColor old_color;
    ProofColor new_color;
    bool hash_changed;
} DependencyUpdateResult;

/**
 * @brief 验证并更新所有依赖链
 */
int proof_validate_dependencies(ProofNavigator *nav, DependencyUpdateResult *results, int max_results);

/* ============== ⊥ 的公理包可定义性 ============== */

/**
 * @brief 配置矛盾（⊥）的定义
 */
typedef struct BottomDefinition {
    bool has_input_ports; /* 是否有输入端口 */
    int input_port_count;
    bool allow_explosion; /* 是否允许爆炸原理 */
} BottomDefinition;

/**
 * @brief 设置 ⊥ 的定义
 */
void proof_set_bottom_definition(ProofNavigator *nav, const BottomDefinition *def);

/**
 * @brief 获取 ⊥ 的定义
 */
const BottomDefinition *proof_get_bottom_definition(const ProofNavigator *nav);

/* ============== 引理块折叠 ============== */

/**
 * @brief 设置引理的视图状态
 */
void proof_set_lemma_view_state(ProofNavigator *nav, int step_id, LemmaViewState state);

/**
 * @brief 获取引理的视图状态
 */
LemmaViewState proof_get_lemma_view_state(const ProofNavigator *nav, int step_id);

/* ============== 辅助函数 ============== */

/**
 * 证明颜色转字符串
 */
const char *proof_color_to_string(ProofColor color);

/**
 * 命题类型转字符串
 */
const char *proposition_type_to_string(PropositionType type);

/**
 * 步骤类型转字符串
 */
const char *proof_step_type_to_string(ProofStepType type);

/**
 * 合一结果转字符串
 */
const char *unify_result_to_string(UnifyStatus result);

/* ============== 命题实例化 ============== */

/**
 * @brief 检查命题是否包含未实例化的类型变量
 *
 * 扫描命题的模式图中所有端口节点，检查其 type_region 是否为
 * TYPE_KIND_VARIABLE 类型。同时递归检查子命题。
 *
 * @param prop  要检查的命题
 * @return true 如果存在未实例化的类型变量，false 否则
 */
bool proof_has_type_variables(const Proposition *prop);

/**
 * @brief 实例化多态命题
 *
 * 将命题中的类型变量节点替换为具体的类型区域节点。
 * 创建命题的深拷贝，在副本上执行替换，不影响原始命题。
 *
 * @param prop               原始命题（不会被修改）
 * @param type_var_to_concrete  映射数组，交替存放 [type_var_node_id, concrete_node_id, ...]
 * @param mapping_count      映射条目数量（数组长度 = mapping_count * 2）
 * @return 新的已实例化命题，失败返回 NULL
 */
Proposition *proof_instantiate_proposition(const Proposition *prop, const int *type_var_to_concrete, int mapping_count);

/* ============== 不可构造性证明流程 ============== */

/**
 * @brief 不可构造性证明结果
 */
typedef enum {
    UNCONSTRUCT_PROVED,         /**< 证明成功：构造不可行 */
    UNCONSTRUCT_NOT_PROVED,     /**< 未能证明不可构造 */
    UNCONSTRUCT_MAYBE_POSSIBLE, /**< 可能可构造（不在已知不可构造列表中） */
    UNCONSTRUCT_ERROR           /**< 检查过程中出错 */
} UnconstructResult;

/**
 * @brief 不可构造性证明详细信息
 */
typedef struct {
    UnconstructResult result;    /**< 证明结果 */
    const char *matched_problem; /**< 匹配到的已知不可构造问题名 */
    const char *matched_theory;  /**< 匹配到的问题所属理论域 */
    const char *proof_strategy;  /**< 使用的证明策略描述 */
    char *detailed_report;       /**< 详细报告字符串（调用者需用lv00_free释放） */
    int reduction_steps;         /**< 归约步数 */
} UnconstructInfo;

/**
 * @brief 检查构造是否已知不可构造
 *
 * 在已加载的公理包中搜索匹配的不可构造性问题。
 * 检查构造的结构特征是否与已知的不可构造问题（如三等分角、
 * 倍立方、化圆为方等）相匹配。
 *
 * @param nav    证明导航器（含已加载的公理包引用）
 * @param graph  要检查的构造图
 * @param prop   相关的命题（可为NULL）
 * @param info   输出：不可构造性信息（调用者需用 unconstruct_info_destroy 释放）
 * @return 检查结果
 */
UnconstructResult proof_check_unconstructibility(ProofNavigator *nav, const ConstraintGraph *graph,
                                                 const Proposition *prop, UnconstructInfo *info);

/**
 * @brief 尝试系统性地证明不可构造性
 *
 * 使用多策略方法尝试证明构造不可行：
 * 1. 检查已知不可构造问题列表
 * 2. 尝试归约到已知不可构造问题
 * 3. 分析代数方程的可解性
 * 4. 检查是否超出特定几何系统范围
 *
 * @param nav      证明导航器
 * @param graph    要检查的构造图
 * @param prop     相关命题（可为NULL）
 * @param info     输出：不可构造性信息
 * @return 证明结果
 */
UnconstructResult proof_attempt_unconstructibility(ProofNavigator *nav, const ConstraintGraph *graph,
                                                   const Proposition *prop, UnconstructInfo *info);

/**
 * @brief 释放不可构造性信息结构体
 *
 * 释放 UnconstructInfo 中的动态分配内存（detailed_report）。
 * 注意：matched_problem、matched_theory、proof_strategy 指向静态字符串，无需释放。
 *
 * @param info 要释放的不可构造性信息
 */
void unconstruct_info_destroy(UnconstructInfo *info);

/* ============== 证明回溯与搜索树可视化（Newclid风格） ============== */

/**
 * @brief 回溯点类型
 */
typedef enum {
    BACKTRACK_CHOICE_POINT, /**< 选择点：多个策略分支 */
    BACKTRACK_FAILURE,      /**< 失败点：此路径不可行 */
    BACKTRACK_SUCCESS,      /**< 成功点：此路径到达目标 */
    BACKTRACK_PRUNE         /**< 剪枝点：启发式跳过 */
} BacktrackNodeType;

/**
 * @brief 证明搜索树节点（Newclid风格）
 *
 * 借鉴 Newclid 的证明搜索树可视化：
 * - 展示证明搜索过程中尝试了哪些路径
 * - 标注在哪些节点进行了回溯
 * - 支持在不同搜索策略之间切换观察效果
 */
typedef struct BacktrackNode {
    int id;                  /**< 节点ID */
    BacktrackNodeType type;  /**< 节点类型 */
    int step_index;          /**< 关联的证明步骤索引（-1 = 无关联） */
    char *label;             /**< 节点标签（如"尝试辅助线AD"） */
    char *strategy_name;     /**< 使用的策略名称 */
    bool is_backtrack_point; /**< 是否为回溯点 */

    /* 树结构 */
    struct BacktrackNode *parent;    /**< 父节点 */
    struct BacktrackNode **children; /**< 子节点数组 */
    int child_count;                 /**< 子节点数量 */
    int child_capacity;              /**< 子节点容量 */

    /* 颜色/状态 */
    bool explored;    /**< 是否已探索 */
    ProofColor color; /**< 节点信任颜色 */
} BacktrackNode;

/**
 * @brief 证明搜索树（Newclid风格）
 */
typedef struct {
    BacktrackNode *root;       /**< 根节点 */
    BacktrackNode **all_nodes; /**< 所有节点（用于遍历） */
    int node_count;            /**< 节点总数 */
    int node_capacity;         /**< 节点容量 */

    /* 统计信息 */
    int success_paths;   /**< 成功路径数 */
    int failure_paths;   /**< 失败路径数 */
    int backtrack_count; /**< 回溯次数 */
    int pruned_branches; /**< 剪枝分支数 */
    int max_depth;       /**< 最大搜索深度 */

    /* 策略信息 */
    char *current_strategy;      /**< 当前使用的搜索策略名称 */
    char **available_strategies; /**< 可用策略名称列表 */
    int strategy_count;          /**< 策略数量 */
} ProofSearchTree;

/* --- API --- */

/**
 * @brief 创建证明搜索树
 * @return 新分配的搜索树，失败返回NULL
 */
ProofSearchTree *proof_search_tree_create(void);

/**
 * @brief 销毁证明搜索树（递归释放所有节点）
 */
void proof_search_tree_destroy(ProofSearchTree *tree);

/**
 * @brief 创建回溯节点
 * @param type   节点类型
 * @param label  节点标签
 * @return 新分配的节点，失败返回NULL
 */
BacktrackNode *backtrack_node_create(BacktrackNodeType type, const char *label);

/**
 * @brief 向搜索树添加子节点
 * @param tree   搜索树
 * @param parent 父节点（传NULL则设为根节点）
 * @param child  子节点
 * @return 是否成功
 */
bool proof_search_tree_add_child(ProofSearchTree *tree, BacktrackNode *parent, BacktrackNode *child);

/**
 * @brief 标记回溯点
 * @param node         要标记的节点
 * @param strategy_name 使用的策略名称
 */
void backtrack_node_mark_backtrack(BacktrackNode *node, const char *strategy_name);

/**
 * @brief 注册可用策略
 * @param tree          搜索树
 * @param strategy_name 策略名称
 */
void proof_search_tree_register_strategy(ProofSearchTree *tree, const char *strategy_name);

/**
 * @brief 设置当前策略
 * @param tree          搜索树
 * @param strategy_name 策略名称
 */
void proof_search_tree_set_strategy(ProofSearchTree *tree, const char *strategy_name);

/**
 * @brief 导出搜索树为JSON（用于Web GUI可视化）
 * @param tree      搜索树
 * @param filepath   输出文件路径
 * @return 是否成功
 */
bool proof_search_tree_export_json(const ProofSearchTree *tree, const char *filepath);

/**
 * @brief 导出搜索树为DOT格式（Graphviz）
 * @param tree      搜索树
 * @param filepath   输出文件路径
 * @return 是否成功
 */
bool proof_search_tree_export_dot(const ProofSearchTree *tree, const char *filepath);

/* ========================================================================
 * 多证明方法并存引擎（v3.2.0 新增，借鉴 JGEX/GEX 架构）
 *
 * 借鉴 JGEX（中科院张景中团队）的多证明方法共存设计：
 * - 在同一系统中集成多种独立的证明方法
 * - 用户可在不同策略之间切换
 * - 每种方法有独立的搜索空间和输出格式
 *
 * JGEX 集成了六种证明方法：
 *   Wu's Method, Area Method, Groebner Basis, Vector Method,
 *   Full-Angle Method, Deductive Database
 *
 * Lv-00 将其适配为几何元语言环境下的多策略架构。
 * ======================================================================== */

/**
 * @brief 证明策略类型（借鉴 JGEX 的六种方法）
 */
typedef enum {
    PROOF_STRATEGY_DIRECT_CONSTRUCTION, /**< 直接构造法：通过几何构造直接满足命题模式 */
    PROOF_STRATEGY_AREA_METHOD,         /**< 面积法：利用面积关系和消点法（借鉴 JGEX Area Method） */
    PROOF_STRATEGY_GROEBNER_BASIS,      /**< Groebner 基法：代数方程求解（借鉴 JGEX/Wu's Method） */
    PROOF_STRATEGY_VECTOR_METHOD,       /**< 向量法：矢量代数推导 */
    PROOF_STRATEGY_FULL_ANGLE_METHOD,   /**< 全角法：利用全角关系进行角度推理 */
    PROOF_STRATEGY_DEDUCTIVE_DATABASE,  /**< 演绎数据库法：前向链推理 */
    PROOF_STRATEGY_COORDINATE,          /**< 坐标法：解析几何坐标计算 */
    PROOF_STRATEGY_ORACLE,              /**< Oracle 法：外部求解器辅助（不可构造性） */
    PROOF_STRATEGY_COUNT                /**< 策略总数（用于数组大小） */
} ProofStrategyType;

/* Forward declaration: ProofMultiStrategy 结构体在下方完整定义 */
struct ProofMultiStrategy;

/**
 * @brief 证明策略状态
 */
typedef enum {
    PROOF_STRATEGY_AVAILABLE,   /**< 可用（已加载所需公理包） */
    PROOF_STRATEGY_UNAVAILABLE, /**< 不可用（缺少公理包） */
    PROOF_STRATEGY_ACTIVE,      /**< 当前激活 */
    PROOF_STRATEGY_COMPLETED,   /**< 已完成 */
    PROOF_STRATEGY_FAILED       /**< 失败 */
} ProofStrategyStatus;

/**
 * @brief 证明策略描述符
 *
 * 每种证明方法对应一个策略描述符，记录其：
 * - 基本元数据（名称、描述）
 * - 依赖的公理包
 * - 适用的问题类型
 * - 产生的证明步骤
 */
typedef struct ProofStrategyDescriptor {
    ProofStrategyType type;         /**< 策略类型 */
    ProofStrategyStatus status;     /**< 当前状态 */
    char *name;                     /**< 策略名称（如"面积法"） */
    char *description;              /**< 策略描述 */
    char **required_axiom_packages; /**< 依赖的公理包名称列表 */
    int axiom_package_count;        /**< 公理包数量 */

    /* 适用性评估 */
    bool (*applicability_check)(/**< 适用性检查函数 */
                                const struct ProofMultiStrategy *mse, const ConstraintGraph *graph,
                                const Proposition *prop);

    /* 策略执行 */
    bool (*execute)(/**< 策略执行函数 */
                    struct ProofMultiStrategy *mse, ProofNavigator *nav);

    /* 生成的证明步骤 */
    int generated_step_count; /**< 生成的步骤数 */
    int *generated_step_ids;  /**< 生成的步骤ID列表 */
} ProofStrategyDescriptor;

/**
 * @brief 多策略证明引擎（借鉴 JGEX 架构）
 *
 * 管理多种证明方法的注册、切换、组合执行。
 * 支持：
 * - 策略注册与发现
 * - 策略切换（运行时）
 * - 策略组合（流水线：一个方法的输出作为另一个的输入）
 * - 策略竞争（多方法并行，取最先成功者）
 * - 策略适用性自动评估
 */
typedef struct ProofMultiStrategy {
    ProofStrategyDescriptor strategies[PROOF_STRATEGY_COUNT]; /**< 策略数组 */
    int active_strategy_index;                                /**< 当前激活的策略索引（-1 = 未选择） */
    ProofNavigator *shared_navigator;                         /**< 共享的证明导航器 */

    /* 策略组合配置 */
    bool enable_fallback; /**< 是否启用回退（主策略失败后尝试其他） */
    int *fallback_order;  /**< 回退顺序（策略索引数组） */
    int fallback_count;   /**< 回退策略数量 */

    /* 执行统计 */
    int total_attempts;           /**< 总尝试次数 */
    int success_count;            /**< 成功次数 */
    int64_t *strategy_timings_ms; /**< 每种策略的耗时（毫秒） */
} ProofMultiStrategy;

/* --- 多策略引擎 API --- */

/**
 * @brief 创建多策略证明引擎
 * @param nav  共享的证明导航器（可为NULL，稍后设置）
 * @return 新分配的多策略引擎，失败返回NULL
 */
ProofMultiStrategy *proof_multi_strategy_create(ProofNavigator *nav);

/**
 * @brief 销毁多策略证明引擎
 */
void proof_multi_strategy_destroy(ProofMultiStrategy *mse);

/**
 * @brief 注册证明策略
 * @param mse        多策略引擎
 * @param descriptor 策略描述符
 * @return 是否成功
 */
bool proof_multi_strategy_register(ProofMultiStrategy *mse, const ProofStrategyDescriptor *descriptor);

/**
 * @brief 激活指定策略
 * @param mse           多策略引擎
 * @param strategy_type 要激活的策略类型
 * @return 是否成功
 */
bool proof_multi_strategy_activate(ProofMultiStrategy *mse, ProofStrategyType strategy_type);

/**
 * @brief 获取当前激活的策略
 * @return 策略描述符指针（不可修改），无激活策略返回NULL
 */
const ProofStrategyDescriptor *proof_multi_strategy_get_active(const ProofMultiStrategy *mse);

/**
 * @brief 评估所有可用策略的适用性
 *
 * 遍历所有已注册的策略，调用其 applicability_check 函数，
 * 返回适用策略的列表，按适用性评分排序。
 *
 * @param mse    多策略引擎
 * @param graph  目标构造图
 * @param prop   目标命题
 * @param out_applicable_types 输出：适用的策略类型数组
 * @param max_count            最多返回数量
 * @return 实际返回的适用策略数量
 */
int proof_multi_strategy_evaluate_applicability(ProofMultiStrategy *mse, const ConstraintGraph *graph,
                                                const Proposition *prop, ProofStrategyType *out_applicable_types,
                                                int max_count);

/**
 * @brief 使用当前策略执行证明
 * @return 是否成功
 */
bool proof_multi_strategy_execute(ProofMultiStrategy *mse);

/**
 * @brief 尝试所有可用策略（竞争模式）
 *
 * 按回退顺序依次尝试每个可用策略，直到某个策略成功或全部失败。
 * 借鉴 JGEX 的用户可选策略机制。
 *
 * @return 成功的策略类型，失败返回 PROOF_STRATEGY_COUNT
 */
ProofStrategyType proof_multi_strategy_try_all(ProofMultiStrategy *mse);

/**
 * @brief 使用多个策略组合证明（流水线模式）
 *
 * 将多个策略按顺序串联：前一个策略的输出作为后一个策略的输入。
 * 例如：先用面积法建立引理，再用直接构造法完成主证明。
 *
 * @param mse            多策略引擎
 * @param pipeline       策略类型流水线（按顺序执行）
 * @param pipeline_count 流水线长度
 * @return 是否全部成功
 */
bool proof_multi_strategy_pipeline(ProofMultiStrategy *mse, const ProofStrategyType *pipeline, int pipeline_count);

/**
 * @brief 设置回退顺序
 * @param mse             多策略引擎
 * @param fallback_order  策略索引数组（按优先级排序）
 * @param count           回退策略数量
 */
void proof_multi_strategy_set_fallback_order(ProofMultiStrategy *mse, const int *fallback_order, int count);

/**
 * @brief 切换策略（保存当前策略状态后切换）
 * @param mse           多策略引擎
 * @param strategy_type 目标策略类型
 * @return 是否成功
 */
bool proof_multi_strategy_switch(ProofMultiStrategy *mse, ProofStrategyType strategy_type);

/**
 * @brief 获取策略执行统计
 * @param mse              多策略引擎
 * @param out_total_attempts  输出：总尝试次数
 * @param out_success_count   输出：成功次数
 */
void proof_multi_strategy_get_stats(const ProofMultiStrategy *mse, int *out_total_attempts, int *out_success_count);

/**
 * @brief 策略类型转字符串
 */
const char *proof_strategy_type_to_string(ProofStrategyType type);

/**
 * @brief 策略状态转字符串
 */
const char *proof_strategy_status_to_string(ProofStrategyStatus status);


/* ================================================================
 * === 第六梯队参考项目落地 (P1) — 2026-05-24 ======================
 * 新增 API 声明来自 Agda/Idris2/Isabelle/HOL Light/F* 五个项目
 * ================================================================ */

/* --- 前向声明 --- */
typedef struct ConstraintSolver ConstraintSolver;

/* ================================================================
 * 1. Agda — hole-driven 证明编辑
 *    借鉴：逐"洞"填充的交互式证明方式，Lv-00 Web GUI 对应功能
 * ================================================================ */

/** @brief 填充建议结构体 — Agda hole-driven 证明编辑 */
typedef enum { FILL_EXACT, FILL_LAMBDA, FILL_CONSTRUCTOR, FILL_CASE_SPLIT, FILL_REFINE } FillKind;

typedef struct FillSuggestion {
    FillKind kind;
    char *label;        /* 建议描述 */
    char *code_snippet; /* 填充代码片段 */
    int arity;          /* 构造器元数 */
    struct FillSuggestion *next;
} FillSuggestion;

/**
 * @brief 引导式洞填充 — 声明几何命题后，系统引导用户逐步填充证明
 * @param solver      约束求解器上下文
 * @param goal_type   目标几何命题的类型声明（如"等腰三角形面积公式"）
 * @param goal_dim    维度
 * @return 填充建议链表，调用者用 fill_suggestions_destroy() 释放
 */
FillSuggestion *proof_guided_fill(ConstraintSolver *solver, const char *goal_type, int goal_dim);
void fill_suggestions_destroy(FillSuggestion *list);

/* ================================================================
 * 2. Idris 2 — QTT 线性类型标记（0/1/ω），证明仅编译期
 * ================================================================ */

/** @brief QTT 用量标注（借鉴 Idris 2 Quantitative Type Theory） */
typedef enum { PROOF_QTT_ERASED = 0, PROOF_QTT_LINEAR = 1, PROOF_QTT_UNRESTRICTED = 2 } ProofQuantifier;

/**
 * @brief 标记构造步骤为 Ghost（仅编译期存在，运行时擦除）
 * @param step_id  证明步骤 ID
 * @param quant    用量标注（ERASED=仅证明，LINEAR=精确一次，UNRESTRICTED=可多次）
 * @return 是否成功
 */
bool proof_mark_ghost(int step_id, ProofQuantifier quant);

/**
 * @brief 检查 Ghost 冲突 — 确认被运行时计算依赖的步骤未被标记为 ERASED
 * @return 冲突数量（0 = 无冲突）
 */
int proof_check_ghost_conflicts(void);

/* ================================================================
 * 3. Isabelle/HOL — Sledgehammer 自动证明策略调度
 * ================================================================ */

/** @brief Sledgehammer 调用模式 */
typedef enum { SLEDGE_SYNC, SLEDGE_ASYNC, SLEDGE_TIMEOUT } SledgehammerMode;

/** @brief Isar 结构化证明层级 */
typedef enum { ISAR_LEMMA, ISAR_HAVE, ISAR_SHOW, ISAR_QED } IsarStructureLevel;

/** @brief Sledgehammer 单个策略执行结果 */
typedef struct {
    ProofStrategyType strategy;
    bool success;
    double LV00_TOLERATED_FLOAT(elapsed_sec); /* @note tolerated: timing only */
    char *isar_proof_script; /* 自动生成的 Isar 证明脚本 */
} SledgehammerStrategyResult;

/** @brief Sledgehammer 批量调度报告 */
typedef struct {
    SledgehammerStrategyResult *results;
    int result_count;
    int best_index; /* 最优（最简）证明的索引 */
    double LV00_TOLERATED_FLOAT(total_time_sec); /* @note tolerated: timing only */
    const char *error_msg;
} SledgehammerReport;

/**
 * @brief Sledgehammer 风格 — 自动尝试多个证明策略，返回最优结果
 * @param mse          多策略引擎
 * @param mode         调度模式（同步/异步/超时）
 * @param timeout_ms   超时毫秒（0 = 不限）
 * @return 调度报告，调用者用 sledgehammer_report_destroy() 释放
 */
SledgehammerReport *proof_sledgehammer_dispatch(ProofMultiStrategy *mse, SledgehammerMode mode, int timeout_ms);
void sledgehammer_report_destroy(SledgehammerReport *report);

/**
 * @brief 将证明步骤导出为 Isar 结构化证明文本
 * @param props       命题列表
 * @param prop_count  命题数量
 * @return Isar 格式证明文本（调用者释放）
 */
char *proof_export_isar(const Proposition **props, int prop_count);

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
VerifyResult proof_minimal_verify(VerifyRuleType rule, const char **premises, const char *conclusion, char **out_trace);

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
RefinementCheckReport *proof_refinement_check(ConstraintSolver *solver, RefinementCheckEntry *entries, int count);
void refinement_check_report_destroy(RefinementCheckReport *report);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PROOF_H */
