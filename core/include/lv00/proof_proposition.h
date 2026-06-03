/**
 * @file proof_proposition.h
 * @brief 命题与证明基础类型定义
 *
 * 包含：
 * - 前向声明
 * - 证明状态颜色枚举
 * - 命题类型枚举
 * - Proposition / ProofStep / ProofDependency 结构体
 * - ProofNavigator 结构体
 * - 命题管理 API
 * - 假设作用域声明
 * - 合一检查 API
 */

#ifndef LV00_PROOF_PROPOSITION_H
#define LV00_PROOF_PROPOSITION_H

#include <stdbool.h>
#include <time.h>

#include "constraint_graph.h"
#include "exact_arithmetic.h" /* LV00_TOLERATED_FLOAT for proof timing/thresholds */
#include "normalization.h"
#include "stream.h"
#include "type_system.h"
#include "unify.h"
#ifndef LV00_PUBLIC_API
#define LV00_PUBLIC_API
#endif


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 设置证明系统的流式输出上下文
 * @param ctx  流式上下文（可为 NULL 以禁用流式输出）
 */
LV00_PUBLIC_API void proof_set_stream_context(StreamContext *ctx);

/**
 * @brief 获取证明系统的流式输出上下文
 * @return 当前流式上下文（可能为 NULL）
 */
LV00_PUBLIC_API StreamContext *proof_get_stream_context(void);

/* ============== 前向声明 ============== */
typedef struct Proposition Proposition;
typedef struct ProofStep ProofStep;
typedef struct ProofNavigator ProofNavigator;
typedef struct ProofDependency ProofDependency;
typedef struct PropositionEquivalence PropositionEquivalence;
typedef struct BottomDefinition BottomDefinition;
typedef struct LV00Engine LV00Engine; /* 引擎前向声明 */

/* ============== 证明状态颜色 ==============
 * 【枚举值命名规范】所有枚举值使用 UPPER_SNAKE_CASE
 */
typedef enum {
    PROOF_COLOR_GREEN,             /**< 全构造，无任何非常规依赖 */
    PROOF_COLOR_BLUE_UNEXPLORED,   /**< 蓝色（未探索） */
    PROOF_COLOR_BLUE_RESOURCE,     /**< 蓝色（资源受限） */
    PROOF_COLOR_BLUE_OUT_OF_RANGE, /**< 蓝色（超出范围） */
    PROOF_COLOR_GREEN_VERIFIED,    /**< 绿色实框：已证不可构造 */
    PROOF_COLOR_YELLOW,            /**< 黄色虚线框：条件性不可构造 */
    PROOF_COLOR_ORANGE_ORACLE,     /**< 浅橙色实心端口：依赖非构造性oracle */
    PROOF_COLOR_ORANGE_EX_FALSO,   /**< 浅橙色虚线箭头：爆炸原理步骤 */
    PROOF_COLOR_AMBER,             /**< 橙黄色：含数值假设 */
    PROOF_COLOR_DARK_ORANGE        /**< 深橙色：非构造性依赖与数值假设叠加 */
} ProofColor;

/* ============== 命题类型 ==============
 * 【枚举值命名规范】所有枚举值使用 UPPER_SNAKE_CASE
 */
typedef enum {
    PROPOSITION_TYPE_ATOMIC,      /**< 原子命题 */
    PROPOSITION_TYPE_CONJUNCTION, /**< 合取 ∧ */
    PROPOSITION_TYPE_DISJUNCTION, /**< 析取 ∨ */
    PROPOSITION_TYPE_IMPLICATION, /**< 蕴含 → */
    PROPOSITION_TYPE_NEGATION,    /**< 否定 ¬ */
    PROPOSITION_TYPE_UNIVERSAL,   /**< 全称 ∀ */
    PROPOSITION_TYPE_EXISTENTIAL, /**< 存在 ∃ */
    PROPOSITION_TYPE_BOTTOM       /**< 矛盾 ⊥ */
} PropositionType;

/* ============== 命题模式 ============== */
struct Proposition {
    int id;               /* 命题ID */
    PropositionType type; /* 命题类型 */
    ProofColor color;     /* 证明状态颜色 */
    char *label;          /* 命题标签（可空） */
    int ref_count;        /* 引用计数（用于 proposition_ref/unref） */

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

    /* 时间戳 */
    time_t created_at;    /* 创建时间 */
    time_t last_modified; /* 最后修改时间 */

    /* ============================================================
     * 版本控制字段 (v3.5.0: 自举支持)
     * ============================================================ */
    uint16_t version_major;         /**< 主版本号 */
    uint16_t version_minor;         /**< 次版本号 */
    uint16_t version_patch;         /**< 补丁版本号 */
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

    /* 证明树结构 */
    int parent_step_id; /* 父步骤ID（-1 表示根步骤） */
    int depth;          /* 步骤在证明树中的深度 */

    /* 时间戳 */
    int64_t timestamp; /* 步骤时间戳 */

    /* ============================================================
     * 版本控制字段 (v3.5.0: 自举支持)
     * ============================================================ */
    uint16_t version_major;         /**< 主版本号 */
    uint16_t version_minor;         /**< 次版本号 */
    uint16_t version_patch;         /**< 补丁版本号 */
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
 * 【枚举值命名规范】所有枚举值使用 UPPER_SNAKE_CASE
 */
typedef enum {
    LEMMA_VIEW_STATE_EXPANDED, /**< 展开 */
    LEMMA_VIEW_STATE_COLLAPSED /**< 折叠 */
} LemmaViewState;

/**
 * @brief 证明状态
 * 【枚举值命名规范】所有枚举值使用 UPPER_SNAKE_CASE
 */
typedef enum {
    PROOF_STATE_ONGOING,       /**< 证明进行中 */
    PROOF_STATE_COMPLETED,     /**< 证明完成 */
    PROOF_STATE_CONTRADICTORY  /**< 证明矛盾：推导出了互斥结论 */
} ProofState;

/* ============== 假设作用域标识符 ============== */
/**
 * @brief 假设作用域标识符
 *
 * 用于限定反证法中的临时假设作用范围。作用域内的矛盾不得污染
 * 全局证明上下文。作用域关闭后，其下所有临时假设和条件性结论
 * 应被回收或标记为失效。
 */
typedef int Lv00ProofScopeId;

#define LV00_PROOF_SCOPE_GLOBAL 0   /**< 全局作用域（默认公理和约束） */
#define LV00_PROOF_SCOPE_INVALID -1 /**< 无效作用域标识符 */

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
    ProofState proof_state; /* 证明状态（进行中/完成/矛盾） */

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

    /* 局部假设作用域：防止局部矛盾污染全局证明上下文 */
    Lv00ProofScopeId *scope_ids;
    bool *scope_active;
    Proposition **scope_assumptions;
    int scope_count;
    int scope_capacity;
    Lv00ProofScopeId next_scope_id;

    /* ============================================================
     * 版本控制字段 (v3.5.0: 自举支持)
     * ============================================================ */
    uint16_t version_major;         /**< 主版本号 */
    uint16_t version_minor;         /**< 次版本号 */
    uint16_t version_patch;         /**< 补丁版本号 */
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
LV00_PUBLIC_API Proposition *proposition_create(int id, PropositionType type);

/**
 * 增加命题引用计数
 */
LV00_PUBLIC_API void proposition_ref(Proposition *prop);

/**
 * 减少命题引用计数，当计数为0时销毁
 */
LV00_PUBLIC_API void proposition_unref(Proposition *prop);

/**
 * 销毁命题
 */
LV00_PUBLIC_API void proposition_destroy(Proposition *prop);

/**
 * 设置输入端口
 */
LV00_PUBLIC_API bool proposition_set_input_ports(Proposition *prop, const int *port_ids, int count);

/**
 * 设置输出端口
 */
LV00_PUBLIC_API bool proposition_set_output_ports(Proposition *prop, const int *port_ids, int count);

/**
 * 设置模式图
 */
LV00_PUBLIC_API bool proposition_set_pattern(Proposition *prop, ConstraintGraph *pattern);

/**
 * 设置前置条件
 */
LV00_PUBLIC_API bool proposition_set_preconditions(Proposition *prop, const int *region_ids, int count);

/**
 * 设置后置条件
 */
LV00_PUBLIC_API bool proposition_set_postconditions(Proposition *prop, const int *constraint_ids, int count);

/**
 * 添加子命题
 */
LV00_PUBLIC_API bool proposition_add_sub_proposition(Proposition *parent, Proposition *child);

/**
 * @brief 检查两个命题是否逻辑互斥
 *
 * 通过比较命题的类型、模式图和约束关系，判断两个命题是否
 * 构成逻辑矛盾（如 P 和 ¬P 同时成立）。
 *
 * @param a  命题 A
 * @param b  命题 B
 * @return true 表示两个命题互斥，false 表示不互斥或无法判断
 */
LV00_PUBLIC_API bool proposition_contradicts(const Proposition *a, const Proposition *b);

/* ============== 假设作用域与局部矛盾隔离 ============== */
LV00_PUBLIC_API Lv00ProofScopeId proof_begin_assumption_scope(ProofNavigator *nav, const Proposition *assumption);
LV00_PUBLIC_API bool proof_close_assumption_scope(ProofNavigator *nav, Lv00ProofScopeId scope_id);
LV00_PUBLIC_API bool proof_scope_is_active(const ProofNavigator *nav, Lv00ProofScopeId scope_id);
LV00_PUBLIC_API bool proof_apply_ex_falso_scoped(ProofNavigator *nav, ConstraintGraph *bottom_proof, Proposition *target_prop,
                                 Lv00ProofScopeId scope_id);
LV00_PUBLIC_API bool proof_has_global_proposition(const ProofNavigator *nav, const Proposition *prop);

/* ============== 合一检查 ============== */

/**
 * 执行合一检查
 * @param construction 构造图
 * @param proposition 命题模式
 * @param normalize_first 是否先执行图规范化遍
 * @return 合一结果
 */
LV00_PUBLIC_API UnifyStatus proof_unify(const ConstraintGraph *construction, Proposition *proposition, bool normalize_first);

/**
 * 合一检查（详细版）
 * @param construction 构造图
 * @param proposition 命题模式
 * @param out_mismatch_info 输出不匹配信息
 * @return 合一结果
 */
LV00_PUBLIC_API UnifyStatus proof_unify_detailed(const ConstraintGraph *construction, Proposition *proposition, char **out_mismatch_info);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PROOF_PROPOSITION_H */
