# 逻辑验证与浮点证明工具链 (Logic Verification and Floating-Point Proof Toolchain)

## 模块概述

逻辑验证与浮点证明工具链是 Lv-00 推理层的核心子系统，覆盖从基础三值逻辑到模态逻辑扩展、从量词系统到命题验证、从元证明到浮点误差严格证明的完整工具链。该模块将形式逻辑推理与浮点数值验证统一在同一个框架下，为几何约束系统的证明提供逻辑基础和数值可信度保障。

前端逻辑子系统（three_valued_logic、modal_operators、quantifier、logic_check、prop_verifier、meta_proof）处理命题和证明的逻辑结构验证；后端浮点证明子系统（herbie_eval、fptaylor_eval、gappa_dsl、gappa_propagate）借鉴 Herbie、FPTaylor、Gappa 等成熟工具的设计理念，提供从采样评估到严格误差界证明的多层次浮点精度分析能力。

## 核心设计原则

1. **Kleene 三值逻辑基础**：所有逻辑子系统建立在 Kleene 强三值逻辑之上，真值扩展为 TRUE/FALSE/UNKNOWN 三态，适应证明子目标未解决时的标注需求
2. **构造性语义**：命题验证遵循 BHK 解释，证明即构造，几何构造的外部端口通过合一匹配验证命题
3. **Kripke 语义模态扩展**：模态逻辑基于 Kripke 框架，世界对应几何配置，可达关系对应几何变换的可允许性
4. **多层次浮点证明**：从 Herbie 风格的采样评估到 FPTaylor 风格的严格误差界，再到 Gappa 风格的形式化证明，形成逐层增强的信任链
5. **元证明完备性**：剪枝合法性通过三层元证明策略（直接矛盾/传播矛盾/代数排除）保障完备性

## 1. three_valued_logic.h —— Kleene 强三值逻辑

### 设计借鉴

引入 Kleene 强三值逻辑（Strong Kleene Three-Valued Logic），将 Lv-00 原有的 TRUE/FALSE 二值系统扩展为三值系统。第三真值 UNKNOWN 表示"未确定"状态，应用于证明子目标未解决时的真值标注和无限域量词评估。

### 三值真值枚举

```c
typedef enum {
    LV00_TRUE    = 0,  /**< 真（已证构造性） */
    LV00_FALSE   = 1,  /**< 伪（有反例/矛盾） */
    LV00_UNKNOWN = 2   /**< 未知（未确定） */
} Lv00TruthValue;
```

### 真值表运算

提供完整的 Kleene 强三值逻辑真值表运算：

| 运算 | API | 说明 |
|------|-----|------|
| AND | `lv00_tvl_and(a, b)` | 遇 FALSE 即返回 FALSE |
| OR | `lv00_tvl_or(a, b)` | 遇 TRUE 即返回 TRUE |
| NOT | `lv00_tvl_not(v)` | UNKNOWN 取反仍为 UNKNOWN |
| IMPLIES | `lv00_tvl_implies(a, b)` | 等价于 OR(NOT(a), b) |
| EQUIV | `lv00_tvl_equiv(a, b)` | 双向蕴涵的合取 |

### 批量短路操作

```c
/* 批量 AND 归约：遇 FALSE 立即返回 */
Lv00TruthValue lv00_tvl_and_all(const Lv00TruthValue *values, int count);

/* 批量 OR 归约：遇 TRUE 立即返回 */
Lv00TruthValue lv00_tvl_or_all(const Lv00TruthValue *values, int count);
```

短路语义确保批量操作在确定结果后立即终止，避免不必要的遍历。

### 判定辅助与转换

```c
/* 判定辅助 */
static inline bool lv00_tvl_is_known(Lv00TruthValue v);    /* 非 UNKNOWN */
static inline bool lv00_tvl_is_true(Lv00TruthValue v);     /* == TRUE */
static inline bool lv00_tvl_is_false(Lv00TruthValue v);    /* == FALSE */

/* 转布尔值 */
static inline bool lv00_tvl_to_bool_conservative(Lv00TruthValue v); /* 仅 TRUE → true */
static inline bool lv00_tvl_to_bool_optimistic(Lv00TruthValue v);   /* 非 FALSE → true */

/* 二值/三值互转 */
static inline Lv00TruthValue lv00_tvl_from_bool(bool b);
```

保守策略仅将已证真视为真，适用于"是否可以确定使用此引理？"等场景；乐观策略将未被证伪视为真，适用于"此方向是否至少可能成功？"等探索性场景。

## 2. modal_operators.h —— 模态逻辑扩展

### 设计借鉴

为 Lv-00 几何证明系统引入基本模态逻辑，提供必然算子（□）和可能算子（◇），基于 Kripke 语义的几何约束可达关系框架。使用基本模态逻辑 K 系统。

### 模态操作符

```c
typedef enum {
    LV00_MODALOP_NECESSARY = 0, /**< □ 必然："在所有可达世界中成立" */
    LV00_MODALOP_POSSIBLE  = 1  /**< ◇ 可能："在某个可达世界中成立" */
} Lv00ModalOperator;
```

对偶关系：◇A = ¬□¬A，□A = ¬◇¬A。

### 可达关系类型

```c
typedef enum {
    LV00_REACH_GEOMETRIC_IDENTITY,   /**< 恒等变换：世界等于自身 */
    LV00_REACH_RIGID_TRANSFORM,      /**< 刚性变换：平移、旋转、反射 */
    LV00_REACH_SIMILARITY_TRANSFORM, /**< 相似变换：缩放 + 刚体 */
    LV00_REACH_AFFINE_TRANSFORM,     /**< 仿射变换：保持平行性 */
    LV00_REACH_PROJECTIVE_TRANSFORM, /**< 射影变换 */
    LV00_REACH_CONSTRAINT_INHERIT,   /**< 约束继承：子图可达 */
    LV00_REACH_CUSTOM                /**< 自定义可达关系 */
} Lv00ReachabilityType;
```

在几何域中，7 种可达关系对应从恒等到射影的不同几何变换层次，构成从强到弱的可达性谱系。

### Kripke 框架结构

```c
/* 模态世界：代表一种几何构造配置 */
struct Lv00ModalWorld {
    int id;                         /**< 世界 ID */
    char *world_name;               /**< 世界名称 */
    ConstraintGraph *configuration; /**< 该世界的几何构造图 */
    Proposition **true_props;       /**< 在该世界中成立的命题 */
    int true_prop_count;
};

/* Kripke 模态框架 <W, R> */
struct Lv00ModalFrame {
    Lv00ModalWorld **worlds;        /**< 世界数组 */
    int world_count;
    Lv00ReachabilityType **reach_matrix; /**< 可达关系类型矩阵 */
    int reach_dimension;
};
```

### 模态公式与评估

```c
/* 模态命题公式（支持嵌套，如 □◇P） */
struct Lv00ModalFormula {
    Lv00ModalOperator op;           /**< 最外层模态算子 */
    struct Proposition *inner_prop; /**< 内层命题 */
    struct Lv00ModalFormula *sub;   /**< 子模态公式（嵌套时使用） */
};

/* 模态评估结果 */
struct Lv00ModalEvalResult {
    Lv00TruthValue truth_value;     /**< 评估真值 */
    int witness_world_id;           /**< 目击世界 ID（◇ 算子时有效） */
    char *explanation;              /**< 解释字符串 */
};
```

核心评估 API：

```c
/* 在给定框架和世界中评估模态公式 */
int lv00_modal_evaluate(const Lv00ModalFrame *frame,
                        const Lv00ModalFormula *formula,
                        int world_id, Lv00ModalEvalResult *result);

/* 检查公式是否为框架中的有效式 */
Lv00TruthValue lv00_modal_check_validity(const Lv00ModalFrame *frame,
                                         const Lv00ModalFormula *formula);

/* 模态对偶转换 */
Lv00ModalFormula *lv00_modal_possible_to_necessary_not(const Lv00ModalFormula *formula);
Lv00ModalFormula *lv00_modal_necessary_to_not_possible(const Lv00ModalFormula *formula);
```

### 几何应用辅助

```c
/* 创建默认几何模态框架（刚性变换可达） */
Lv00ModalFrame *lv00_modal_frame_create_geometric_default(void);

/* 创建几何模态断言 */
Lv00ModalFormula *lv00_modal_assert_point_must_on_line(Lv00ModalFrame *frame,
                                                       int point_id, int line_id);
Lv00ModalFormula *lv00_modal_assert_point_can_on_line(Lv00ModalFrame *frame,
                                                      int point_id, int line_id);
```

## 3. quantifier.h —— 量词系统

### 设计借鉴

提供全称量词（forall）、存在量词（exists）和唯一存在量词（exists_unique）的形式化处理。支持量词实例化/泛化操作、有限域上的量词消去、与约束图的双向映射，以及三值逻辑真值评估。遵循构造性/BHK 解释语义。

### 量词类型

```c
typedef enum {
    LV00_FORALL        = 0, /**< ∀  全称量词  "对所有...都成立" */
    LV00_EXISTS        = 1, /**< ∃  存在量词  "存在一个...使得..." */
    LV00_EXISTS_UNIQUE = 2  /**< ∃! 唯一存在  "存在唯一一个...使得..." */
} Lv00Quantifier;
```

### 量化域与表达式

```c
/* 量化域：变量取值范围 */
struct Lv00Domain {
    int id;
    char *domain_name;         /**< 域名（如 "R", "Triangle", "Point"） */
    int *domain_elements;      /**< 有限枚举域元素列表 */
    int element_count;
    ConstraintGraph *subgraph; /**< 约束图子图域 */
    bool is_finite;            /**< 是否为有限域 */
    int estimated_cardinality; /**< 估计基数（-1 = 未知/无限） */
};

/* 量化命题表达式 */
struct Lv00QuantifiedExpr {
    int id;
    Lv00Quantifier quantifier;
    char *variable_name;       /**< 绑定变量名 */
    int variable_node_id;      /**< 绑定变量对应的约束图节点 ID */
    Lv00Domain *domain;        /**< 量化域 */
    struct Proposition *body_proposition; /**< 体命题 */
    Lv00TruthValue cached_truth;  /**< 真值缓存 */
    bool truth_cache_valid;
};
```

### 量词运算结果

```c
typedef enum {
    LV00_QUANT_OK,                 /**< 操作成功 */
    LV00_QUANT_DOMAIN_EMPTY,       /**< 域为空 */
    LV00_QUANT_DOMAIN_INFINITE,    /**< 域为无限，消去不可能 */
    LV00_QUANT_INVALID_VARIABLE,   /**< 变量无效 */
    LV00_QUANT_BODY_UNDEFINED,     /**< 体命题未定义 */
    LV00_QUANT_INSTANTIATE_FAILED, /**< 实例化失败 */
    LV00_QUANT_GENERALIZE_FAILED,  /**< 泛化失败 */
    LV00_QUANT_COUNTEREXAMPLE,     /**< 找到反例 */
    LV00_QUANT_ERROR               /**< 一般性错误 */
} Lv00QuantResult;
```

### 量词推理规则

| 规则 | API | 逻辑含义 |
|------|-----|----------|
| ∀-消去 | `lv00_quantifier_instantiate` | 从 ∀x.P(x) 推导 P(t) |
| ∀-引入 | `lv00_quantifier_generalize` | 从 P(x) 对任意 x 推导 ∀x.P(x) |
| ∃-引入 | `lv00_quant_exists_introduce` | 从 P(t) 推导 ∃x.P(x) |
| ∃-消去 | `lv00_quant_exists_eliminate` | 从 ∃x.P(x) 和 ∀y.(P(y)→Q) 推导 Q |

### 有限域量词消去

```c
/* 全称消去：∀x∈D.P(x) → P(d1) ∧ ... ∧ P(dn) */
Lv00QuantResult lv00_quant_eliminate_forall_finite(const Lv00QuantifiedExpr *expr,
                                                    Lv00QuantifiedResult *out_result);

/* 存在消去：∃x∈D.P(x) → P(d1) ∨ ... ∨ P(dn) */
Lv00QuantResult lv00_quant_eliminate_exists_finite(const Lv00QuantifiedExpr *expr,
                                                    Lv00QuantifiedResult *out_result);

/* 唯一存在消去：∃!x.P(x) → 恰好一个元素满足 P */
Lv00QuantResult lv00_quant_eliminate_exists_unique_finite(const Lv00QuantifiedExpr *expr,
                                                           Lv00QuantifiedResult *out_result);
```

三值逻辑评估：对有限域尝试完全评估，对无限域返回 `LV00_UNKNOWN`。

## 4. logic_check.h —— 逻辑自检

### 设计借鉴

提供三个维度的证明质量自动审查：一致性检查（矛盾检测）、循环性检查（三色 DFS）、完备性检查（断言来源追溯）。所有检查结果汇总到 `Lv00LogicReport` 中。

### 检查级别

```c
typedef enum {
    LV00_LOGIC_ISSUE_INFO,     /**< 信息性：建议性提示 */
    LV00_LOGIC_ISSUE_WARNING,  /**< 警告：可能有问题，但不阻塞 */
    LV00_LOGIC_ISSUE_ERROR,    /**< 错误：确定性问题，需修复 */
    LV00_LOGIC_ISSUE_FATAL     /**< 致命错误：证明确实无效 */
} Lv00LogicIssueLevel;
```

### 逻辑检查上下文

```c
struct Lv00LogicContext {
    ProofNavigator *nav;        /**< 证明导航器（只读访问） */
    int max_issues;             /**< 最大问题数上限 */
    bool verbose;               /**< 是否输出详细信息 */
    bool stop_on_fatal;         /**< 是否在致命错误处停止 */
    int total_steps_checked;
    int total_issues_found;
};
```

### 逻辑完整性报告

```c
struct Lv00LogicReport {
    /* 总体评估 */
    bool is_consistent;         /**< 整体是否一致 */
    bool is_non_circular;       /**< 整体是否无循环推理 */
    bool is_complete;           /**< 整体是否完备 */
    Lv00TruthValue overall_health; /**< 总体健康度（三值逻辑） */

    /* 分项问题列表 */
    Lv00LogicIssue **consistency_issues;
    Lv00LogicIssue **circularity_issues;
    Lv00LogicIssue **completeness_issues;

    /* 统计摘要 */
    int total_issues;
    int error_count, warning_count, info_count, fatal_count;
    double check_time_sec;
};
```

### 三维检查策略

**一致性检查** (`lv00_logic_check_consistency`)：
- 遍历证明中所有断言，检测互补对（A 和 ¬A 同时成立）
- 检查约束图中的几何约束矛盾（如同一线段被同时要求相等和不等）
- 对多态/依赖类型的断言考虑类型实例化后的等价性

**循环性检查** (`lv00_logic_check_circularity`)：
- 构建证明步骤的依赖有向图
- 三色 DFS（白/灰/黑）检测环：灰色节点再次被访问即为环
- 检测自循环和间接循环

**完备性检查** (`lv00_logic_check_completeness`)：
- 验证每个被使用的断言可追溯到：公理包、已证引理、明确前提或推理规则
- 无来源的断言标记为"未论证"

### 综合检查与报告导出

```c
/* 综合检查（一致性 + 循环性 + 完备性） */
int lv00_logic_check_all(Lv00LogicContext *ctx, Lv00LogicReport *report);

/* 报告导出 */
char *lv00_logic_report_to_text(const Lv00LogicReport *report, bool verbose);
char *lv00_logic_report_to_json(const Lv00LogicReport *report);
```

## 5. prop_verifier.h —— 命题逻辑验证器

### 设计借鉴

Lv-00 自举目标——基于自然演绎风格的命题逻辑证明搜索器。支持 BHK 解释下的几何构造验证：合取（积类型）、析取（和类型）、蕴涵（标准函数块）、否定（蕴涵矛盾）。

### 命题逻辑公式

```c
typedef enum {
    PROP_ATOM,        /* 原子命题 P, Q, R, ... */
    PROP_CONJUNCTION, /* A ∧ B */
    PROP_DISJUNCTION, /* A ∨ B */
    PROP_IMPLICATION, /* A → B */
    PROP_NEGATION,    /* ¬A */
    PROP_BOTTOM,      /* ⊥ (矛盾) */
    PROP_TRUE         /* ⊤ (真) */
} PropFormulaType;

struct PropFormula {
    PropFormulaType type;
    union {
        struct { char name[64]; } atom;
        struct { PropFormula *left, *right; } binary;
        struct { PropFormula *operand; } unary;
    } data;
};
```

### 验证结果与配置

```c
typedef enum {
    PV_VERIFY_PROVEN,        /* 证明成功（合一匹配） */
    PV_VERIFY_DISPROVEN,     /* 证伪（找到反例） */
    PV_VERIFY_FAILED,        /* 未能证明（搜索空间耗尽） */
    PV_VERIFY_INVALID_INPUT, /* 输入无效 */
    PV_VERIFY_TIMEOUT,       /* 超时 */
    PV_VERIFY_ERROR          /* 内部错误 */
} PropVerifyResult;

typedef struct {
    int max_steps;           /* 最大推理步数 (默认 10000) */
    bool use_intuitionistic; /* 使用直觉主义逻辑 (默认 true) */
    bool enable_ex_falso;    /* 启用爆炸原理 (默认 false) */
    int timeout_ms;          /* 超时毫秒数 (默认 30000) */
} VerifierConfig;
```

### 核心验证 API

```c
/* 验证命题 sequent: premises ⊢ goal */
VerifyDetail prop_verifier_verify(const PropFormula **premises, int premise_count,
                                  const PropFormula *goal, const VerifierConfig *config);

/* 等价性检查 */
bool prop_verifier_check_equivalence(const PropFormula *a, const PropFormula *b,
                                     const VerifierConfig *config);

/* 永真式检查 */
bool prop_verifier_check_tautology(const PropFormula *f, const VerifierConfig *config);
```

### BHK 几何构造验证桥接

基于 BHK (Brouwer-Heyting-Kolmogorov) 解释，将命题逻辑验证结果映射到几何构造系统的信任颜色：

| BHK 验证结果 | TrustColor 映射 | 含义 |
|-------------|-----------------|------|
| PROVEN + 无缺失构造 | TRUST_GREEN | 完全构造性，可信 |
| PROVED + 少量缺失 (<=2) | TRUST_YELLOW | 条件可信 |
| PROVED + 显著缺失 (>=3) | TRUST_AMBER | 需关注 |
| FAILED（搜索耗尽） | TRUST_BLUE | 未确定 |
| DISPROVEN | TRUST_RED | 已证伪 |

```c
/* BHK 几何构造验证 */
BHKVerificationResult prop_verifier_bhk_verify(const PropFormula **premises, int premise_count,
                                               const PropFormula *goal, const VerifierConfig *config);

/* 信任颜色桥接：BHK 验证 → 约束图 TrustColor */
int prop_verifier_apply_trust_colors(ConstraintGraph *graph, const PropFormula **premises,
                                     int premise_count, const PropFormula *goal,
                                     const VerifierConfig *config, BHKVerificationResult *out_result);
```

### 不可构造性分析

```c
/* 当验证失败时，获取详细的不可构造性分析 */
InconstructibilityAnalysis prop_verifier_analyze_inconstructibility(
    const PropFormula **premises, int premise_count,
    const PropFormula *goal, const VerifierConfig *config);
```

## 6. meta_proof.h —— 剪枝合法性元证明

### 设计借鉴

在证明系统之上增加元证明层，证明"被排除的状态空间确实不包含合法解"。这是 WFC 范式数学严格化的基础。

数学基础：剪枝操作 π = (v, R, φ)，合法性条件为 ∀r ∈ R, ∀σ* ∈ Σ_global : σ*(v) ≠ r。完备性定理保证：若每步剪枝合法且 Σ_global ≠ ∅，则 Σ_global 中的所有解都是原问题的合法解。

### 剪枝策略枚举

```c
typedef enum {
    PRUNE_DIRECT_CONTRADICTION,        /**< L1: 直接矛盾（候选与约束直接冲突） */
    PRUNE_PROPAGATION_CONTRADICTION,   /**< L2: 传播矛盾（选择后传播导致死路） */
    PRUNE_ALGEBRAIC_EXCLUSION          /**< L3: 代数排除（不在 Gröbner 基解集中） */
} PruneStrategy;
```

### 元证明结果

```c
typedef enum {
    META_PROVE_VALID,          /**< 剪枝合法（已证明） */
    META_PROVE_INVALID,        /**< 剪枝非法（候选可能是合法解） */
    META_PROVE_INCONCLUSIVE,   /**< 无法确定 */
    META_PROVE_TIMEOUT         /**< 证明超时 */
} MetaProofResult;
```

### 三层证明策略

**L1 直接矛盾** (`meta_prove_direct_contradiction`)：
- 将候选状态 r 代入约束的代数表达式
- 若结果非零则矛盾成立
- 记录矛盾约束 ID

**L2 传播矛盾** (`meta_prove_propagation_contradiction`)：
- 临时坍缩节点为候选状态 r
- 运行约束传播引擎
- 若到达 CONTRADICTION 状态则证明成立
- 记录传播路径和步数

**L3 代数排除** (`meta_prove_algebraic_exclusion`)：
- 从约束图提取 Gröbner 基
- 将候选状态 r 代入验证
- 记录违反的多项式数量

### 自动策略选择与完备性验证

```c
/* 自动选择策略：按优先级尝试 L1 → L2 → L3 */
MetaProofResult meta_prove_pruning(MetaProofContext *ctx,
                                    int node_id, const SymbolicCoord *candidate);

/* 完备性验证：检查所有被移除的状态是否都有合法的剪枝证明 */
CompletenessReport *meta_prove_completeness(MetaProofContext *ctx);
```

### 元证明上下文

```c
typedef struct MetaProofContext {
    ConstraintGraph *graph;
    PropagationContext *prop_ctx;
    EquivClassManager *equiv_mgr;
    ProofNavigator *navigator;
    PruningRecord *record;

    /* 配置 */
    int max_propagation_steps;  /**< L2 传播矛盾最大步数 */
    int timeout_ms;             /**< 单次证明超时 */
    bool enable_l1, enable_l2, enable_l3;

    /* 统计 */
    int64_t l1_proofs, l2_proofs, l3_proofs, inconclusive_count;

    StreamContext *stream_ctx;
} MetaProofContext;
```

## 7. herbie_eval.h —— Herbie 风格浮点精度评估

### 设计借鉴

借鉴 Herbie (herbie.uwplse.org) 的浮点精度改进方法论，提供采样评估、位误差计算和 AMBER 评分。设计参考包括 FPBench 浮点基准测试和 Rosa 范围分析工具。

### 评估结果结构

```c
typedef struct {
    char expression[256];          /**< 表达式字符串 */
    double max_bit_error;          /**< 最大位误差 */
    double avg_bit_error;          /**< 平均位误差 */
    double max_relative_error;     /**< 最大相对误差 */
    double avg_relative_error;     /**< 平均相对误差 */
    double amber_score;            /**< AMBER 精度评分 (0.0=最差, 1.0=最佳) */
    int sample_count;              /**< 采样数量 */
    int valid_samples;             /**< 有效样本数（非 NaN/Inf） */
} Lv00HerbieResult;
```

### 输入域分区

```c
typedef struct {
    Lv00Interval bounds[LV00_TAYLOR_MAX_VARS]; /**< 变量边界 */
    int var_count;
    double weight;                              /**< 相对权重 */
    char description[128];
} Lv00HerbieRegime;

typedef struct {
    Lv00HerbieRegime regimes[LV00_HERBIE_MAX_REGIMES];
    int regime_count;
    double total_weight;
} Lv00HerbiePartitionResult;
```

### 核心 API

```c
/* 评估单个表达式的浮点精度 */
bool herbie_evaluate(const char *expr, const char **var_names,
                     const Lv00Interval *var_bounds, int var_count,
                     const Lv00HerbieConfig *config, Lv00HerbieResult *out);

/* 比较多个表达式变体，选择最优 */
bool herbie_compare(const char **exprs, int expr_count, const char **var_names,
                    const Lv00Interval *var_bounds, int var_count,
                    const Lv00HerbieConfig *config,
                    Lv00HerbieResult *results, int *best_index);

/* 输入域分区（识别不同变体更精确的子域） */
bool herbie_partition_regimes(const char *expr, const char **var_names,
                              const Lv00Interval *var_bounds, int var_count,
                              const Lv00HerbieConfig *config,
                              Lv00HerbiePartitionResult *out);

/* 为每个分区选择最优表达式变体 */
bool herbie_select_path(const char **exprs, int expr_count, const char **var_names,
                        const Lv00HerbiePartitionResult *partition, int var_count,
                        const Lv00HerbieConfig *config, int *best_indices);

/* AMBER 评分计算 */
double herbie_compute_amber(const double *errors, int error_count,
                            double alpha, double beta);
```

AMBER (Accuracy Measure Based on Error Range) 提供归一化精度评分 [0, 1]，其中 1.0 表示完美精度。

## 8. fptaylor_eval.h —— FPTaylor 风格浮点误差分析

### 设计借鉴

借鉴 FPTaylor (github.com/soarlab/FPTaylor) 的浮点误差分析方法论，将浮点表达式分解为实数值表达式、舍入误差和总误差界三部分，通过泰勒展开结合区间算术计算严格误差界。

### 泰勒形式结构

```c
typedef struct {
    double center;                   /**< 中心点值 f(center) */
    double vars_center[LV00_TAYLOR_MAX_VARS]; /**< 变量中心值 */
    double derivs[LV00_TAYLOR_MAX_VARS];      /**< 偏导数 df/dx_i */
    int var_count;
    double rem_lo, rem_hi;           /**< 余项区间 [rem_lo, rem_hi] */
    int order;                       /**< 泰勒展开阶数（1 或 2） */
} Lv00TaylorForm;
```

### 误差界结构

```c
typedef struct {
    double absolute_error;   /**< 绝对误差上界 */
    double relative_error;   /**< 相对误差上界 */
    double roundoff_error;   /**< 舍入误差贡献 */
    double truncation_error; /**< 截断（泰勒余项）误差 */
    int is_valid;            /**< 误差界是否有效 */
    char proof_text[1024];   /**< 人类可读的证明摘要 */
} Lv00ErrorBound;
```

### 核心 API

```c
/* 评估浮点表达式的误差界 */
bool fptaylor_evaluate(const char *expr, const char **var_names,
                       const Lv00Interval *var_bounds, int var_count,
                       const Lv00FPTaylorConfig *config, Lv00ErrorBound *out);

/* 分析复合表达式（多步计算中误差累积） */
bool fptaylor_analyze_expression(const char *expr, const char **var_names,
                                 const Lv00Interval *var_bounds, int var_count,
                                 const Lv00FPTaylorConfig *config,
                                 Lv00ErrorBound *out, Lv00TaylorForm *taylor_out);

/* 创建简单算术表达式的泰勒形式 */
bool fptaylor_taylor_form(const char *expr, const char **var_names,
                          const double *var_centers, int var_count,
                          int order, Lv00TaylorForm *out);
```

### 配置参数

```c
typedef struct {
    int taylor_order;            /**< 泰勒展开阶数（1 或 2） */
    double branch_threshold;     /**< 区间宽度二分阈值 */
    int max_bisections;          /**< 最大二分步数 */
    int enable_optimization;     /**< 启用仿射松弛优化 */
    double rounding_unit;        /**< 舍入单位（目标格式的机器 epsilon） */
} Lv00FPTaylorConfig;
```

## 9. gappa_dsl.h —— Gappa 风格浮点证明 DSL

### 设计借鉴

借鉴 Gappa (gappa.gitlabpages.inria.fr) 的形式化浮点证明生成工具，提供领域特定语言用于指定和证明浮点计算的性质。支持浮点格式声明、假设声明、证明目标设定和自动化区间传播证明。

### 舍入模式

```c
typedef enum {
    LV00_ROUND_NE = 0,  /**< 最近舍入（偶数优先） */
    LV00_ROUND_NA,      /**< 最近舍入（远离零优先） */
    LV00_ROUND_ZR,      /**< 向零截断 */
    LV00_ROUND_DN,      /**< 向 -∞ 舍入 */
    LV00_ROUND_UP,      /**< 向 +∞ 舍入 */
    LV00_ROUND_COUNT
} Lv00GappaRounding;
```

### 浮点格式

```c
typedef struct {
    int precision_bits;      /**< 总精度位数（如 binary32 为 24） */
    int exponent_bits;       /**< 指数位数（如 binary32 为 8） */
    Lv00GappaRounding rounding;
    char name[64];           /**< 格式名称 */
} Lv00GappaFormat;
```

预定义格式通过 `gappa_format_predefined` 获取，支持 "binary16"、"binary32"、"binary64"、"binary128"。

### 谓词类型

```c
typedef enum {
    LV00_PRED_BND = 0,  /**< 有界：x ∈ [lo, hi] */
    LV00_PRED_ABS,      /**< 绝对值：|x| ≤ bound */
    LV00_PRED_REL,      /**< 相对误差：|x - y| / |y| ≤ bound */
    LV00_PRED_LIN,      /**< 线性组合：a*x + b*y + ... ∈ [lo, hi] */
    LV00_PRED_FIX,      /**< 定点：x = 精确值 */
    LV00_PRED_FLT,      /**< 浮点：x = round(exact) */
    LV00_PRED_NZR,      /**< 非零：|x| > bound */
    LV00_PRED_EQL       /**< 等式：x = y */
} Lv00GappaPredType;
```

### DSL 解析与证明引擎

```c
/* 解析 Gappa DSL 字符串 */
bool gappa_parse(const char *dsl_string,
                 Lv00GappaPredicate **hypotheses, int *hyp_count,
                 Lv00GappaProofGoal **goals, int *goal_count);

/* 从假设出发证明目标 */
Lv00GappaProofResult gappa_prove(const Lv00GappaPredicate *hypotheses, int hyp_count,
                                  Lv00GappaProofGoal *goals, int goal_count,
                                  const Lv00GappaFormat *fmt);
```

DSL 语法示例：
```
x in [0, 1] -> |x - 0.5| <= 0.5
y in [-1, 1] -> |x - 0.5| <= 0.5 -> |y| <= 1
```

## 10. gappa_propagate.h —— 谓词传播引擎

### 设计借鉴

实现 Gappa 风格的前向和后向区间谓词传播，借鉴抽象解释的前向/后向数据流分析思想。前向传播从已知假设推导新谓词，后向传播从目标确定所需附加假设。

### 谓词集合

```c
#define LV00_PRED_SET_MAX_SIZE 256

typedef struct {
    Lv00GappaPredicate preds[LV00_PRED_SET_MAX_SIZE];
    int count;
} Lv00GappaPredSet;
```

### 传播配置

```c
typedef struct {
    int max_iterations;       /**< 最大前向传播迭代次数 */
    int max_backward_depth;   /**< 最大后向推理深度 */
    double contraction_eps;   /**< 区间收缩 epsilon */
    int enable_backward;      /**< 启用后向推理 */
} Lv00GappaPropagateConfig;
```

### 核心 API

```c
/* 前向传播：从假设推导新谓词 */
int gappa_propagate(const Lv00GappaPredSet *input_set,
                    Lv00GappaPredSet *output_set,
                    const Lv00GappaPropagateConfig *config);

/* 后向传播：从目标确定所需假设 */
int gappa_propagate_backward(const Lv00GappaPredicate *goal,
                             const Lv00GappaPredSet *known_facts,
                             Lv00GappaPredSet *output_set,
                             const Lv00GappaPropagateConfig *config);
```

### 谓词集合操作

```c
void gappa_pred_set_init(Lv00GappaPredSet *set);
bool gappa_pred_set_add(Lv00GappaPredSet *set, const Lv00GappaPredicate *pred);
int gappa_pred_set_find(Lv00GappaPredSet *set, const char *var_name,
                        Lv00GappaPredicate *out);
void gappa_pred_set_clear(Lv00GappaPredSet *set);
```

### 重写规则

```c
typedef struct {
    char match_pattern[256];    /**< 匹配模式 */
    char replace_pattern[256];  /**< 替换模式 */
    char description[128];      /**< 描述 */
} Lv00GappaRewriteRule;

bool gappa_register_rewrite_rules(const Lv00GappaRewriteRule *rules, int rule_count);
```

重写规则在传播过程中应用，用于简化谓词并启用更多推导。

## 模块间依赖关系

```
three_valued_logic.h
    ├── modal_operators.h  (依赖 proof.h, three_valued_logic.h)
    ├── quantifier.h       (依赖 constraint_graph.h, three_valued_logic.h)
    ├── logic_check.h      (依赖 proof.h, three_valued_logic.h)
    └── prop_verifier.h    (依赖 constraint_graph.h, symbolic_coord.h)

meta_proof.h              (依赖 constraint_graph.h, propagation.h, symbolic_coord.h)

interval_arithmetic.h     (统一区间算术基础)
    ├── herbie_eval.h     (依赖 interval_arithmetic.h)
    ├── fptaylor_eval.h   (依赖 interval_arithmetic.h)
    ├── gappa_dsl.h       (独立浮点格式定义)
    └── gappa_propagate.h (依赖 gappa_dsl.h)
```

## 设计参考索引

| 模块 | 借鉴来源 |
|------|----------|
| three_valued_logic | Kleene 强三值逻辑 (1952) |
| modal_operators | Kripke 语义框架 (1963), 基本模态逻辑 K 系统 |
| quantifier | BHK 解释, 自然演绎量词规则 |
| logic_check | 抽象解释数据流分析 |
| prop_verifier | 自然演绎证明搜索, BHK 构造性解释 |
| meta_proof | WFC 剪枝合法性, Gröbner 基理论 |
| herbie_eval | Herbie (herbie.uwplse.org), FPBench (fpbench.org) |
| fptaylor_eval | FPTaylor (github.com/soarlab/FPTaylor), IEEE 1788 |
| gappa_dsl | Gappa (gappa.gitlabpages.inria.fr), IEEE 754 |
| gappa_propagate | Gappa 谓词传播, 抽象解释 |
