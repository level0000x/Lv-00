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
 */

#ifndef LV00_PROOF_H
#define LV00_PROOF_H

#include "constraint_graph.h"
#include "normalization.h"
#include "type_system.h"
#include "unify.h"
#include "stream.h"
#include <stdbool.h>

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
typedef struct LV00Engine LV00Engine;  /* 引擎前向声明 */

/* ============== 证明状态颜色 ============== */
typedef enum {
    PROOF_COLOR_GREEN,          /* 全构造，无任何非常规依赖 */
    PROOF_COLOR_BLUE_UNEXPLORED, /* 蓝色（未探索） */
    PROOF_COLOR_BLUE_RESOURCE,   /* 蓝色（资源受限） */
    PROOF_COLOR_BLUE_OUT_OF_RANGE, /* 蓝色（超出范围） */
    PROOF_COLOR_GREEN_VERIFIED,  /* 绿色实框：已证不可构造 */
    PROOF_COLOR_YELLOW,          /* 黄色虚线框：条件性不可构造 */
    PROOF_COLOR_ORANGE_ORACLE,   /* 浅橙色实心端口：依赖非构造性oracle */
    PROOF_COLOR_ORANGE_EX_FALSO, /* 浅橙色虚线箭头：爆炸原理步骤 */
    PROOF_COLOR_AMBER,           /* 橙黄色：含数值假设 */
    PROOF_COLOR_DARK_ORANGE      /* 深橙色：非构造性依赖与数值假设叠加 */
} ProofColor;

/* ============== 命题类型 ============== */
typedef enum {
    PROPOSITION_ATOMIC,     /* 原子命题 */
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
    int id;                     /* 命题ID */
    PropositionType type;       /* 命题类型 */
    ProofColor color;           /* 证明状态颜色 */

    /* 输入/输出端口 */
    int *input_port_ids;        /* 输入端口ID数组 */
    int input_count;            /* 输入端口数量 */
    int *output_port_ids;       /* 输出端口ID数组 */
    int output_count;           /* 输出端口数量 */

    /* 几何模式（虚线框内的约束骨架） */
    ConstraintGraph *pattern;   /* 命题模式图 */

    /* 前置条件区域 */
    int *precondition_region_ids; /* 前置条件区域ID */
    int precondition_count;       /* 前置条件数量 */

    /* 后置条件 */
    int *postcondition_constraint_ids; /* 后置条件约束ID */
    int postcondition_count;          /* 后置条件数量 */

    /* 子命题（用于复合命题） */
    Proposition **sub_props;    /* 子命题数组 */
    int sub_prop_count;         /* 子命题数量 */

    /* 类型信息 */
    TypeRegion *prop_type;      /* 命题类型 */

    /* 元数据 */
    char *name;                 /* 命题名称 */
    char *description;          /* 描述 */
};

/* ============== 证明步骤类型 ============== */
typedef enum {
    PROOF_STEP_ADD_NODE,        /* 添加节点 */
    PROOF_STEP_ADD_CONSTRAINT,  /* 添加约束 */
    PROOF_STEP_REWRITE,         /* 重写步骤 */
    PROOF_STEP_FUNCTION_APP,    /* 函数应用 */
    PROOF_STEP_PACK_FUNCTION,   /* 打包函数块 */
    PROOF_STEP_NORMALIZATION,   /* 自动规范化 */
    PROOF_STEP_UNIFY,           /* 合一检查 */
    PROOF_STEP_EX_FALSO,        /* 爆炸原理步骤 */
    PROOF_STEP_ORACLE           /* Oracle依赖 */
} ProofStepType;

/* ============== 证明步骤 ============== */
struct ProofStep {
    int id;                     /* 步骤ID */
    ProofStepType type;         /* 步骤类型 */
    ProofColor color;           /* 步骤颜色 */

    /* 步骤数据 */
    int node_id;                /* 相关节点ID */
    int constraint_id;          /* 相关约束ID */
    int rule_id;                /* 相关规则ID */
    int func_block_id;          /* 相关函数块ID */

    /* 规范化步骤数据 */
    int *merged_node_ids;       /* 被合并的节点ID */
    int merged_count;           /* 被合并的节点数量 */
    int retained_node_id;       /* 保留的节点ID */

    /* 依赖关系 */
    int *dependency_step_ids;   /* 依赖的前驱步骤ID */
    int dependency_count;       /* 依赖数量 */
    int *dependent_step_ids;    /* 被依赖的后继步骤ID */
    int dependent_count;        /* 被依赖数量 */

    /* 状态 */
    bool is_breakpoint;         /* 是否为断点 */
    bool is_completed;          /* 是否完成 */
    char *note;                 /* 用户注释 */

    /* 时间戳 */
    int64_t timestamp;          /* 步骤时间戳 */
};

/* ============== 证明依赖链 ============== */
struct ProofDependency {
    int id;                     /* 依赖ID */
    ProofColor color;           /* 依赖颜色 */

    /* 依赖来源 */
    enum {
        DEP_SOURCE_DIRECT,      /* 直接构造 */
        DEP_SOURCE_LEMMA,       /* 引理引用 */
        DEP_SOURCE_ORACLE,      /* 非构造性Oracle */
        DEP_SOURCE_EX_FALSO,    /* 爆炸原理 */
        DEP_SOURCE_NUMERIC      /* 数值假设 */
    } source;

    /* 引理引用 */
    int lemma_id;               /* 引理ID */
    char *content_hash;         /* 内容哈希 */

    /* 外部引用 */
    char *external_ref;         /* 外部引用字符串 */

    /* 数值假设声明 */
    char *numeric_declaration;  /* 数值假设声明 */
    double precision_threshold; /* 精度阈值 */

    /* 子依赖 */
    ProofDependency **sub_deps; /* 子依赖数组 */
    int sub_dep_count;          /* 子依赖数量 */
};

/* ============== 引理块折叠 ============== */

/**
 * @brief 引理视图状态
 */
typedef enum {
    LEMMA_VIEW_EXPANDED,
    LEMMA_VIEW_COLLAPSED
} LemmaViewState;

/* ============== 证明导航器 ============== */
struct ProofNavigator {
    ProofStep **steps;          /* 证明步骤数组 */
    int step_count;             /* 步骤数量 */
    int current_step;           /* 当前步骤索引 */

    Proposition *target_prop;   /* 目标命题 */
    ConstraintGraph *construction; /* 构造图 */

    ProofDependency *dep_tree;  /* 依赖树 */

    /* 导航状态 */
    bool is_complete;           /* 证明是否完成 */
    ProofColor final_color;     /* 最终颜色 */

    /* 断点管理 */
    int *breakpoint_indices;    /* 断点索引数组 */
    int breakpoint_count;       /* 断点数量 */

    /* 命题等价表 */
    PropositionEquivalence *equivalences; /* 等价命题数组 */
    int equivalence_count;      /* 等价命题数量 */
    int equivalence_capacity;   /* 等价命题容量 */

    /* ⊥ 的定义 */
    BottomDefinition *bottom_def; /* 矛盾定义（动态分配） */

    /* 引理视图状态 */
    int *lemma_view_step_ids;   /* 引理步骤ID数组 */
    LemmaViewState *lemma_view_states; /* 引理视图状态数组 */
    int lemma_view_count;       /* 引理视图状态数量 */
    int lemma_view_capacity;    /* 引理视图状态容量 */

    /* 引擎上下文（用于访问已加载的公理包等） */
    LV00Engine *engine;

    /* 证明策略注释（LeanGeo风格：先展示总体策略，再展开细节） */
    char *strategy_note;        /* 总体策略描述 */
};

/* ============== 命题管理API ============== */

/**
 * 创建命题
 */
Proposition *proposition_create(int id, PropositionType type);

/**
 * 销毁命题
 */
void proposition_destroy(Proposition *prop);

/**
 * 设置输入端口
 */
bool proposition_set_input_ports(Proposition *prop, int *port_ids, int count);

/**
 * 设置输出端口
 */
bool proposition_set_output_ports(Proposition *prop, int *port_ids, int count);

/**
 * 设置模式图
 */
bool proposition_set_pattern(Proposition *prop, ConstraintGraph *pattern);

/**
 * 设置前置条件
 */
bool proposition_set_preconditions(Proposition *prop, int *region_ids, int count);

/**
 * 设置后置条件
 */
bool proposition_set_postconditions(Proposition *prop, int *constraint_ids, int count);

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
UnifyStatus proof_unify(
    ConstraintGraph *construction,
    Proposition *proposition,
    bool normalize_first
);

/**
 * 合一检查（详细版）
 * @param construction 构造图
 * @param proposition 命题模式
 * @param out_mismatch_info 输出不匹配信息
 * @return 合一结果
 */
UnifyStatus proof_unify_detailed(
    ConstraintGraph *construction,
    Proposition *proposition,
    char **out_mismatch_info
);

/* ============== 证明步骤管理 ============== */

/**
 * 创建证明步骤
 */
ProofStep *proof_step_create(ProofStepType type);

/**
 * 销毁证明步骤
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
bool proof_apply_ex_falso(
    ProofNavigator *nav,
    ConstraintGraph *bottom_proof,
    Proposition *target_prop
);

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
    PROOF_NL_LANG_ZH_CN,    /**< 简体中文 */
    PROOF_NL_LANG_EN_US     /**< 英文 */
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
    ConstraintGraph *transformation;  /* 双向变换规则 */
} PropositionEquivalence;

/**
 * @brief 声明两个命题等价
 */
void proof_declare_proposition_equivalence(
    ProofNavigator *nav,
    int prop_a_id, int prop_b_id);

/**
 * @brief 查找命题的等价命题
 */
int proof_find_equivalent_proposition(
    const ProofNavigator *nav,
    int prop_id,
    int *equivalent_ids,
    int max_count);

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
int proof_validate_dependencies(
    ProofNavigator *nav,
    DependencyUpdateResult *results,
    int max_results);

/* ============== ⊥ 的公理包可定义性 ============== */

/**
 * @brief 配置矛盾（⊥）的定义
 */
typedef struct BottomDefinition {
    bool has_input_ports;          /* 是否有输入端口 */
    int input_port_count;
    bool allow_explosion;          /* 是否允许爆炸原理 */
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
Proposition *proof_instantiate_proposition(
    const Proposition *prop,
    const int *type_var_to_concrete,
    int mapping_count);

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
    UnconstructResult result;           /**< 证明结果 */
    const char *matched_problem;        /**< 匹配到的已知不可构造问题名 */
    const char *matched_theory;         /**< 匹配到的问题所属理论域 */
    const char *proof_strategy;         /**< 使用的证明策略描述 */
    char *detailed_report;              /**< 详细报告字符串（调用者需用lv00_free释放） */
    int reduction_steps;                /**< 归约步数 */
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
UnconstructResult proof_check_unconstructibility(
    ProofNavigator *nav,
    const ConstraintGraph *graph,
    const Proposition *prop,
    UnconstructInfo *info);

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
UnconstructResult proof_attempt_unconstructibility(
    ProofNavigator *nav,
    const ConstraintGraph *graph,
    const Proposition *prop,
    UnconstructInfo *info);

/**
 * @brief 释放不可构造性信息结构体
 *
 * 释放 UnconstructInfo 中的动态分配内存（detailed_report）。
 * 注意：matched_problem、matched_theory、proof_strategy 指向静态字符串，无需释放。
 *
 * @param info 要释放的不可构造性信息
 */
void unconstruct_info_destroy(UnconstructInfo *info);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PROOF_H */
