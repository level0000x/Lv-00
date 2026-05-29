# TLA+ 时序逻辑动作规约与模型检查核心借鉴设计

> **借鉴项目**：TLA+（github.com/tlaplus/tlaplus）
> **核心借鉴点**：时序逻辑动作规约范式（Init/Next/Spec）、TLC 模型检查器穷举状态搜索、TLAPS 证明管理器、PlusCal 编译管线、不变式检查、对称性归约
> **分类**：P1 高优先级 / 形式化规约与状态空间搜索
> **日期**：2026-05-24

---

## 1. 概述

TLA+（Temporal Logic of Actions）是由图灵奖得主 Leslie Lamport 开发的形式化规约语言，专门用于设计、建模、文档化和验证并发系统与反应式系统。TLA+ 的理论基础是**时序逻辑动作**（Temporal Logic of Actions），它将系统行为建模为状态的无穷序列，并通过时序公式约束这些序列的合法性。

TLA+ 在工业界有广泛的应用验证记录。Amazon 使用 TLA+ 验证了 DynamoDB、S3、EBS 等关键服务的核心算法，发现了多处深层的并发设计缺陷；Microsoft 使用 TLA+ 验证了 Azure Cosmos DB 的一致性协议；Elasticsearch 使用 TLA+ 建模了其分布式协调协议。这些成功案例充分证明了形式化规约在复杂系统设计中的实用价值。

TLA+ 工具链采用 Java 实现，主要包括以下核心组件：

- **SANY**：规约解析器，将 TLA+ 源码解析为抽象语法树，执行语法和静态语义检查。
- **TLC**：显式状态模型检查器，在有限状态空间内穷举搜索，验证系统规约是否满足指定的不变式和时序属性。
- **TLAPS**（TLA+ Proof System）：证明管理器，支持结构化、层次化的数学证明，每个证明步骤可委托给不同的后端求解器（SMT、Zenon、Isabelle）。
- **PlusCal**：算法描述语言，编译为 TLA+ 规约，使得熟悉编程语言的工程师能够更自然地描述系统行为。
- **TLA+ Toolbox**：集成开发环境，提供规约编辑、模型检查配置、结果可视化等图形化功能。

TLA+ 最核心的设计理念是**动作规约三段式**（Init / Next / Spec），这一范式将系统行为规约天然地分解为三个正交维度：

1. **Init**：初始状态谓词，定义系统在时间零点必须满足的条件。
2. **Next**：状态转移关系（动作），描述系统从当前状态到下一状态的所有合法变迁。
3. **Spec**：完整时序规约，将 Init 和 Next 组合为时态公式 `Spec == Init /\ [][Next]_vars`，表示系统始于合法初始状态，且每一步都满足合法的状态转移。

这种三段式分解与几何构造问题具有天然的对应关系：几何构造的初始元素（给定点、给定线段）、构造步骤（各种尺规操作）、以及构造命题（要证明的几何属性）恰好可以映射为 Init、Next 和 Invariant。

Lv-00 是一个面向几何证明的交互式构造与验证系统，其核心挑战在于：如何在构造过程中自动维护约束一致性、如何高效搜索证明空间、以及如何对证明步骤进行可靠的层次化管理。TLA+ 在状态空间建模、穷举搜索和层次化证明方面的成功实践，为 Lv-00 的架构设计提供了极为重要的方法论参考。

以下将从六个核心维度详细阐述 TLA+ 对 Lv-00 的借鉴映射。

---

## 2. 核心借鉴 → Lv-00 映射

### 2.1 时序逻辑的动作规约范式（Init/Next/Spec）→ Lv-00 几何构造规约

TLA+ 的 Init/Next/Spec 三段式将系统行为规约分解为状态空间的初始化、转移和全局约束。这一范式是形式化规约领域最成功的实践之一，已被广泛验证。在 Lv-00 中，几何构造的过程同样可以建模为状态转移系统：每个构造步骤向约束图中引入新的节点和约束边，从而改变约束图的状态。

| TLA+ 概念 | 语义 | Lv-00 映射 | 函数/结构 |
|:---|:---|:---|:---|
| `Init` | 初始状态谓词，定义时间 0 的合法状态 | `geo_init_state` | `geometry_init_graph()` |
| `Next` | 状态转移关系（动作），描述合法变迁 | `geo_construction_step` | `constraint_graph_apply_step()` |
| `Spec` | 完整时序规约：`Init /\ [][Next]_vars` | `geo_full_spec` | `construction_spec_verify()` |
| `vars` | 状态变量元组 | `GeoState` 结构体 | 约束图节点集 + 约束集 |
| `[][Next]_vars` | Next 在所有步骤成立，或 vars 不变（stuttering） | 构造步骤的幂等性 | `step_is_stuttering_check()` |
| `WF_vars(A)` | 弱公平性：如果动作 A 持续使能，则最终必须发生 | 构造可达性约束 | `weak_fairness_for_construction()` |
| `SF_vars(A)` | 强公平性：如果动作 A 无限频繁使能，则必须无限频繁发生 | 强公平性不用于几何构造 | N/A |
| `ENABLED A` | 谓词：动作 A 在当前状态下是否使能 | 构造前提条件检查 | `step_precondition_check()` |

#### Init/Next/Spec 三段式在 Lv-00 中的形式化映射

TLA+ 规约的经典形式：

```tla
---- MODULE Geometry ----
EXTENDS Naturals, Reals

(* 状态变量：几何元素的坐标与关系 *)
VARIABLES points, segments, circles, constraints

(* Init：初始几何元素 *)
Init ==
    /\ points = {}
    /\ segments = {}
    /\ circles = {}
    /\ constraints = {}

(* 构造动作：添加一个点 *)
AddPoint(p) ==
    /\ p \notin points
    /\ points' = points \cup {p}
    /\ UNCHANGED <<segments, circles, constraints>>

(* 构造动作：添加一条约束 —— 两点间距离等值 *)
AddDistanceConstraint(a, b, c, d) ==
    /\ a \in points /\ b \in points
    /\ c \in points /\ d \in points
    /\ constraints' = constraints \cup {<<"dist_eq", a, b, c, d>>}
    /\ UNCHANGED <<points, segments, circles>>

(* Next：所有可能的下一步动作 *)
Next ==
    \E p : AddPoint(p)
    \/ \E a,b,c,d : AddDistanceConstraint(a, b, c, d)
    \/ (* ... 更多构造动作 ... *)

(* Spec：完整时序规约 *)
Spec == Init /\ [][Next]_<<points, segments, circles, constraints>>

(* 不变式：构造一致性 *)
Consistency ==
    \A a,b,c,d: <<"dist_eq", a, b, c, d>> \in constraints
        => distance(a,b) = distance(c,d)
====
```

在 Lv-00 的 C 实现中，这套三段式被映射为结构化的构造验证流程：

```c
/**
 * @brief 几何构造状态 —— 对应 TLA+ 的状态变量 vars
 *
 * 在 TLA+ 中，状态由 VARIABLES 声明的变量元组定义。
 * TLA+ 中的每一步 [Next]_vars 要么改变 vars，要么保持 vars 不变（stuttering）。
 * Lv-00 中，约束图本身就是完整的状态快照。
 */
typedef struct {
    SymbolicCoord    **points;       /**< 符号坐标点集 —— 对应 TLA+ 的 points */
    int               point_count;
    Segment          **segments;     /**< 线段集 */
    int               segment_count;
    Circle           **circles;      /**< 圆集 */
    int               circle_count;
    ConstraintGraph   *graph;        /**< 约束图 —— 包含所有约束关系 */
    int               step_index;    /**< 当前构造步骤索引（对应 TLA+ 中的时间） */
} GeoConstructionState;

/**
 * @brief 构造步骤 —— 对应 TLA+ 中的动作（Action）
 *
 * TLA+ 动作是一个将当前状态映射到下一状态的公式。
 * 在 Lv-00 中，每个构造步骤是一个对约束图的增量变换操作。
 */
typedef enum {
    GEO_STEP_INIT,                  /**< 初始化步骤 —— 对应 Init */
    GEO_STEP_ADD_POINT_FREE,        /**< 添加自由点 */
    GEO_STEP_ADD_POINT_ON_SEGMENT,  /**< 添加线段上的点 */
    GEO_STEP_ADD_POINT_ON_CIRCLE,   /**< 添加圆上的点 */
    GEO_STEP_ADD_POINT_INTERSECTION,/**< 添加两线交点 */
    GEO_STEP_ADD_SEGMENT,           /**< 添加线段 */
    GEO_STEP_ADD_CIRCLE,            /**< 添加圆 */
    GEO_STEP_ADD_DISTANCE_EQ,       /**< 添加距离等值约束 */
    GEO_STEP_ADD_ANGLE_EQ,          /**< 添加角度等值约束 */
    GEO_STEP_ADD_PARALLEL,          /**< 添加平行约束 */
    GEO_STEP_ADD_PERPENDICULAR,     /**< 添加垂直约束 */
    GEO_STEP_ADD_COLLINEAR,         /**< 添加共线约束 */
    GEO_STEP_ADD_CONCYCLIC,         /**< 添加共圆约束 */
    GEO_STEP_TYPE_COUNT
} GeoStepType;

/**
 * @brief 单个构造步骤的描述 —— 对应 TLA+ 中的单一动作
 */
typedef struct {
    GeoStepType       type;         /**< 步骤类型 */
    int               params[8];    /**< 步骤参数（引用的节点 ID） */
    int               param_count;  /**< 参数数量 */
    char             *label;        /**< 步骤标签（可选的用户注释） */
} GeoStep;

/**
 * @brief 几何构造规约 —— 对应 TLA+ 的完整 Spec
 *
 * 借鉴 TLA+ 的 Init/Next/Spec 三段式：
 *   Spec == Init /\ [][Next]_vars /\ Liveness
 */
typedef struct {
    GeoConstructionState  initial;      /**< 初始状态 —— 对应 Init */
    GeoStep              *steps;        /**< 构造步骤序列 —— 对应 Next 的合取分支 */
    int                   step_count;
    ConstraintGraph      *invariants;   /**< 待验证的不变式列表 */
    int                   invariant_count;
} GeoConstructionSpec;

/**
 * @brief 验证整个构造规约 —— 对应 TLC 的模型检查
 *
 * 对给定的几何构造规约执行穷举或符号化验证。
 * 依次执行每个构造步骤，并检查每一步后是否满足所有不变式。
 *
 * @param spec  几何构造规约
 * @param opts  验证选项（搜索深度、对称性归约策略等）
 * @return 验证结果
 *
 * @note 借鉴 TLC 的模型检查流程：
 *   1. 从 Init 计算初始状态集合
 *   2. 从每个状态出发，应用所有使能的 Next 分支
 *   3. 对每个新状态，检查所有 Invariant
 *   4. 若发现违反，报告反例（状态序列）
 */
GeoVerificationResult geo_spec_verify(
    GeoConstructionSpec *spec,
    GeoVerifOptions     *opts
);
```

### 2.2 TLC 模型检查器的穷举状态搜索 → Lv-00 证明搜索策略

TLC 是 TLA+ 生态系统中最关键的验证工具。它采用显式状态模型检查算法，在有限状态空间内进行广度优先或深度优先搜索，穷举遍历所有可达状态，并在每个状态处检查不变式和时序属性。

TLC 的核心技术对 Lv-00 的证明搜索策略具有直接指导意义：

| TLC 特性 | 说明 | Lv-00 映射 | 实现函数 |
|:---|:---|:---|:---|
| BFS 状态搜索 | 广度优先遍历状态空间，确保找到最短反例 | `GRAPH_SEARCH_BFS` | `proof_search_bfs()` |
| DFS 状态搜索 | 深度优先遍历，适合深层构造路径 | `GRAPH_SEARCH_DFS` | `proof_search_dfs()` |
| 指纹（Fingerprint） | 状态的哈希摘要，用于快速判重 | `GeoStateFingerprint` | `geo_state_fingerprint()` |
| 状态队列 | 待处理状态的 FIFO 队列（BFS）或栈（DFS） | `StateQueue` | `state_queue_push/pop()` |
| 不变式检查 | 在每个状态检查不变式是否成立 | `invariant_check_at_state()` | `geo_invariant_check()` |
| 对称性归约 | 利用对称性减少需探索的状态数 | 约束图等价类归约 | `symmetry_reduce_states()` |
| 反例生成 | 发现不变式违反时，生成从初始状态到错误状态的反例路径 | `CounterExample` | `counterexample_generate()` |
| 检查点/恢复 | 大规模验证的断点续传机制 | `Checkpoint` | `checkpoint_save/load()` |

#### 状态空间广度优先搜索的核心实现

```c
/**
 * @brief 状态队列节点 —— 对应 TLC 的状态队列条目
 *
 * 借鉴 TLC 的显式状态搜索：每个发现的新状态都入队，
 * 并记录其前驱状态用于反例重构。
 */
typedef struct StateNode {
    GeoConstructionState  state;        /**< 当前状态快照 */
    uint64_t              fingerprint;  /**< 状态指纹（哈希摘要） */
    int                   depth;        /**< 从 Init 到当前状态的步数 */
    struct StateNode     *parent;       /**< 前驱状态（用于反例路径重构） */
    GeoStep               arriving_step;/**< 从前驱到当前状态执行的步骤 */
} StateNode;

/**
 * @brief TLC 风格的状态空间搜索器
 *
 * 借鉴 TLC 模型检查器的核心搜索循环：
 *
 *   while (queue not empty) {
 *       state = dequeue();
 *       check_invariants(state);
 *       for each enabled action a in Next:
 *           next_state = apply(state, a);
 *           if not seen(next_state):
 *               mark_as_seen(next_state);
 *               enqueue(next_state);
 *   }
 */
typedef struct {
    StateNode        **queue;           /**< BFS 队列（或 DFS 栈） */
    int                queue_head;      /**< 队首索引 */
    int                queue_tail;      /**< 队尾索引 */
    int                queue_capacity;
    uint64_t          *seen_fingerprints;/**< 已访问状态指纹集（判重） */
    int                seen_count;
    int                seen_capacity;
    int                total_states_explored;
    int                max_depth;
    GeoSearchStrategy  strategy;        /**< BFS 或 DFS */
} StateSpaceExplorer;

/**
 * @brief 核心搜索循环 —— TLC 风格的穷举状态探索
 *
 * @param explorer     状态空间搜索器
 * @param spec         几何构造规约
 * @param invariants   需要验证的不变式列表
 * @param inv_count    不变式数量
 * @param out_counter  输出：发现的反例（如果存在）
 * @return true 如果所有不变式在所有可达状态上成立，否则 false
 *
 * 算法流程（直接映射自 TLC）：
 *   1. 从 spec->initial 计算初始状态，入队
 *   2. 循环出队当前状态
 *   3. 对每个不变式调用 invariant_check_at_state()
 *   4. 对每个使能的 GeoStepType，调用 geo_step_apply() 生成后继状态
 *   5. 计算后继状态的指纹，查重后入队
 *   6. 队列空时终止，或发现反例时提前终止
 */
bool geo_model_check(
    StateSpaceExplorer   *explorer,
    GeoConstructionSpec  *spec,
    GeoInvariant         *invariants,
    int                   inv_count,
    CounterExample       *out_counter
);
```

### 2.3 TLAPS 证明管理器的层次化证明结构 → Lv-00 ProofNavigator

TLAPS（TLA+ Proof System）是 TLA+ 的证明管理器，支持将复杂的时序逻辑证明分解为层次化的子证明树。TLAPS 的核心设计原则是：

- **层次化分解**：顶层定理被分解为若干引理，每个引理再递归分解为更小的子目标，直到每个叶节点可以由后端求解器自动处理。
- **多后端调度**：不同性质的证明步骤委托给不同的后端——命题逻辑给 SMT、等式推理给 Zenon、集合论给 Isabelle/TLA+。
- **证明独立性**：每个证明步骤是自包含的（self-contained），不依赖外部上下文，这使得并行验证和增量重检成为可能。

这些设计对 Lv-00 的证明导航器（ProofNavigator）架构提供了直接参考：

| TLAPS 概念 | 语义 | Lv-00 映射 | 数据结构 |
|:---|:---|:---|:---|
| Theorem | 顶层待证定理 | `ProofTheorem` | `proof_theorem_t` |
| Lemma | 中间引理（辅助证明顶层定理） | `ProofLemma` | `proof_lemma_t` |
| Proof Step | 单个证明步骤（可含多个子证明义务） | `ProofStep` | `proof_step_t` |
| Proof Obligation | 证明义务（步骤中需验证的子公式） | `ProofObligation` | `proof_obligation_t` |
| Backend Solver | 后端求解器（SMT/Zenon/Isabelle） | `ProofBackend` | `proof_backend_t` |
| Proof Tree | 整个证明的树状结构 | `ProofTree` | `proof_tree_t` |
| QED | 证明完成标记 | `PROOF_STATUS_QED` | proof_status 枚举值 |
| Omitted | 省略的证明（标记为"相信"） | `PROOF_STATUS_OMITTED` | proof_status 枚举值 |

```c
/**
 * @brief 证明义务 —— 借鉴 TLAPS 的 Proof Obligation
 *
 * TLAPS 中的每个证明步骤可能产生多个证明义务（Proof Obligation）。
 * 例如，使用蕴含引入规则 "A => B" 时，产生义务 A |- B。
 */
typedef struct {
    int              obligation_id;      /**< 义务唯一标识 */
    ConstraintGraph *hypotheses;         /**< 假设集（约束图形式） */
    ConstraintGraph *goal;               /**< 目标（待证明的约束） */
    ProofBackendType assigned_backend;   /**< 分配的后端求解器 */
    ProofStatus      status;             /**< 当前验证状态 */
} ProofObligation;

/**
 * @brief 证明步骤 —— 借鉴 TLAPS 的 Proof Step
 *
 * TLAPS 的每个证明步骤是一个自包含的推理单元。
 * 步骤内部可以有子步骤，形成层次化结构。
 */
typedef struct {
    int              step_id;
    char            *label;              /**< 步骤标签（如 <1>1） */
    ProofObligation *obligations;        /**< 该步骤产生的证明义务 */
    int              obligation_count;
    struct ProofStep **sub_steps;        /**< 子步骤（层次化分解） */
    int              sub_step_count;
    ConstraintGraph  *justification;     /**< 步骤的理由 / 使用的引理 */
    ProofStatus       status;
} ProofStep;

/**
 * @brief 证明树 —— 借鉴 TLAPS 的层次化证明结构
 *
 * 整个证明被组织为一棵树：
 *   Theorem（根节点）
 *     └── Lemma 1（第一层引理）
 *     │     ├── Step <1>1
 *     │     │     ├── Obligation 1  →  SMT Backend
 *     │     │     └── Obligation 2  →  Zenon Backend（Lv-00: Rewrite Engine）
 *     │     └── Step <1>2
 *     └── Lemma 2（第二层引理）
 *           └── ...
 *
 * 最终 QED 状态：所有叶节点的 ProofObligation 都被各自的后端验证通过。
 */
typedef struct {
    ProofObligation  *root_theorem;      /**< 根定理 */
    ProofLemma       *lemmas;            /**< 引理列表 */
    int               lemma_count;
    ProofStep        *top_level_steps;   /**< 顶层证明步骤 */
    int               top_level_step_count;
    ProofBackend     **backends;         /**< 注册的后端求解器列表 */
    int               backend_count;
    ProofStatus        overall_status;   /**< 整个证明树的状态 */
} ProofTree;

/**
 * @brief 证明树验证 —— 借鉴 TLAPS 的证明检查流程
 *
 * 递归遍历证明树，从叶节点向上验证每个证明义务。
 * 类似于 TLAPS，支持"信任后端求解器"模式：
 *   如果某个叶义务由已验证的后端求解器返回 SAT/UNSAT，
 *   则该义务被视为已满足，不需要再次检查。
 *
 * @param tree  证明树
 * @return true 如果所有证明义务均已通过
 */
bool proof_tree_verify(ProofTree *tree);
```

### 2.4 PlusCal 算法语言 → TLA+ 的翻译管道 → Lv-00 DSL → 内核编译管线

PlusCal 是一种类 C 的算法描述语言，可以编译为 TLA+ 规约。这一翻译管道的存在使得非形式化专家也能用熟悉的编程范式描述系统行为。

PlusCal 的翻译管道结构：

```
PlusCal 源码（算法描述）
    → PCalParser（语法分析，生成 AST）
    → PCalTranslator（语义翻译，展开为 TLA+ 动作公式）
    → TLA+ 规约（标准 Init/Next/Spec 形式）
    → SANY（TLA+ 解析器）
    → TLC 模型检查器 / TLAPS 证明管理器
```

在 Lv-00 中，同样存在一条从用户友好的 DSL 到底层内核表示的编译管线。借鉴 PlusCal 的分层翻译架构，Lv-00 的 DSL 编译管线被设计为多阶段流水线：

| PlusCal 翻译阶段 | 功能 | Lv-00 映射 | 对应模块 |
|:---|:---|:---|:---|
| 词法分析 | 将源码分解为 token 流 | `dsl_tokenize()` | `lexer_shared.h` |
| 语法分析 | 构建抽象语法树（AST） | `dsl_parse()` | `formula_parser.h` |
| 语义分析 | 类型检查 + 符号解析 | `dsl_semantic_check()` | `type_system.h` |
| 翻译（Translation） | PlusCal→TLA+ 展开动作公式 | `dsl_to_kernel_ir()` | `formula_converter.h` |
| 优化 | 常量折叠、死代码消除 | `kernel_ir_optimize()` | `normalization.h` |
| 代码生成 | 输出 TLA+ .tla 文件 | `kernel_ir_to_constraint_graph()` | `constraint_graph.h` |

```c
/**
 * @brief DSL 编译管线阶段 —— 借鉴 PlusCal 翻译管道架构
 *
 * 借鉴 PlusCal 的分阶段翻译思想，每条管线阶段都有明确定义
 * 的输入/输出接口，使得任意阶段都可以独立测试和替换。
 */
typedef enum {
    PIPELINE_STAGE_TOKENIZE,        /**< 词法分析：源码 → Token 流 */
    PIPELINE_STAGE_PARSE,           /**< 语法分析：Token 流 → AST */
    PIPELINE_STAGE_RESOLVE,         /**< 符号解析：AST → 标注 AST */
    PIPELINE_STAGE_TYPE_CHECK,      /**< 类型检查：标注 AST → 类型化 AST */
    PIPELINE_STAGE_LOWER,           /**< 降层：DSL AST → 内核 IR */
    PIPELINE_STAGE_OPTIMIZE,        /**< 优化：内核 IR → 优化后 IR */
    PIPELINE_STAGE_EMIT,            /**< 生成：内核 IR → 约束图 */
    PIPELINE_STAGE_COUNT
} PipelineStage;

/**
 * @brief 编译管线上下文
 */
typedef struct {
    PipelineStage     current_stage;
    const char       *source_text;       /**< 原始 DSL 源码 */
    TokenStream      *tokens;            /**< Stage 1 输出 */
    AstNode          *ast_root;          /**< Stage 2 输出 */
    TypedAst         *typed_ast;         /**< Stage 4 输出 */
    KernelIR         *kernel_ir;         /**< Stage 5 输出 */
    ConstraintGraph  *final_graph;       /**< Stage 7 输出（最终产物） */
    DiagnosticList   *diagnostics;       /**< 所有阶段的诊断信息汇总 */
} PipelineContext;

/**
 * @brief 运行完整的 DSL 编译管线
 *
 * 借鉴 PlusCal 的翻译管道流程，顺序执行所有编译阶段。
 * 每个阶段的输出作为下一阶段的输入。
 * 任何阶段的失败都会中止后续阶段并收集诊断信息。
 *
 * @param ctx      管线上下文
 * @param source   DSL 源码字符串
 * @return true 如果所有阶段成功，否则 false
 */
bool pipeline_run_full(PipelineContext *ctx, const char *source);
```

### 2.5 不变式（Invariant）检查机制 → Lv-00 构造不变量验证

TLA+ 的不变式（Invariant）是指系统在所有可达状态下都必须成立的谓词。TLC 在搜索每个状态时，都会检查该状态是否满足所有注册的不变式。不变式是 TLA+ 中最常用的验证手段——Amazon 的工程师报告称，他们 90% 以上的验证需求仅通过不变式即可满足。

在 Lv-00 中，几何构造中的不变式对应于"构造不变量"——在构造的每一步都必须保持成立的几何属性，如：所有点的坐标一致性、约束图的无环性、构造封闭性等。

| TLA+ 不变式概念 | Lv-00 构造不变量 | 检查函数 |
|:---|:---|:---|
| 类型不变式（Type Invariant）：状态变量的类型约束 | 类型一致性：符号坐标类型正确 | `type_invariant_check()` |
| 状态不变式（State Invariant）：单一状态谓词 | 约束图无环性：无约束循环 | `graph_acyclicity_check()` |
| 数据完整性不变式 | 构造封闭性：无悬空引用 | `construction_closure_check()` |
| 业务逻辑不变式 | 几何一致性：共线/共圆的传递性 | `geo_consistency_check()` |
| 互斥不变式 | 约束互斥：互斥约束不会同时成立 | `constraint_exclusion_check()` |
| 归纳不变式 | 归纳构造不变量：施归纳于构造步数 | `inductive_invariant_check()` |

```c
/**
 * @brief 不变式类型 —— 借鉴 TLA+ 的不变式分类
 */
typedef enum {
    INVARIANT_TYPE_CONSTRAINT,     /**< 类型不变式 —— 类比 TLA+ TypeInvariant */
    INVARIANT_STATE_CONSTRAINT,    /**< 状态不变式 —— 类比 TLA+ StateInvariant */
    INVARIANT_INTEGRITY,           /**< 完整性不变式 */
    INVARIANT_GEOMETRIC,           /**< 几何一致性不变式 */
    INVARIANT_EXCLUSION,           /**< 互斥不变式 */
    INVARIANT_INDUCTIVE            /**< 归纳不变式 */
} InvariantKind;

/**
 * @brief 构造不变量 —— 借鉴 TLA+ Invariant 概念
 */
typedef struct {
    int            invariant_id;
    char          *name;               /**< 不变量名称 */
    InvariantKind  kind;
    char          *description;        /**< 人类可读的描述 */
    bool (*check_fn)(                  /**< 不变量检查函数 */
        GeoConstructionState *state
    );
    int            violation_step;     /**< 如果违反，记录违反时的步骤索引（-1 未违反） */
    char          *violation_detail;   /**< 违反时的详细描述 */
} GeoInvariant;

/**
 * @brief 注册并检查所有不变式
 *
 * 在每个构造步骤完成后调用，对所有注册的不变式进行检查。
 * 类比 TLC 在每个状态的 "Invariant checking" 阶段。
 *
 * @param state       当前构造状态
 * @param invariants  不变式列表
 * @param inv_count   不变式数量
 * @return 第一个被违反的不变式索引，-1 表示全部通过
 */
int geo_invariants_check_all(
    GeoConstructionState *state,
    GeoInvariant         *invariants,
    int                   inv_count
);
```

### 2.6 状态空间爆炸的对称性归约技术 → Lv-00 约束图等价类归约

状态空间爆炸（State Space Explosion）是模型检查面临的最根本挑战。TLA+ 社区发展了一系列对称性归约技术：当系统状态中存在对称性（如多个同构的节点可以互换而不改变系统行为）时，可以将对称状态合并为等价类，仅探索每个等价类的一个代表状态，从而大幅缩减需探索的状态空间。

在 Lv-00 中，几何构造的约束图同样存在大量结构对称性。例如：一个三角形的三个顶点在约束图上可能是"同构"的（在没有额外标注的情况下），等边三角形的三个对称旋转产生状态上不同的约束图但几何上等价的构造。借鉴 TLA+ 的对称性归约，Lv-00 可以识别并归约这些等价状态。

| 对称性类型 | 描述 | Lv-00 归约策略 | 实现 |
|:---|:---|:---|:---|
| 顶点置换对称性 | 交换两个同构顶点的标签不改变几何意义 | 顶点标签规范化 | `vertex_label_canonize()` |
| 旋转对称性 | 等边三角形/正方形等旋转不变结构 | 旋转等价类合并 | `rotation_equivalence_reduce()` |
| 镜像对称性 | 左右镜像等价的构造 | 镜像等价类合并 | `mirror_equivalence_reduce()` |
| 子图同构对称性 | 约束图中存在同构子图 | 约束图同构检测 | `subgraph_isomorphism_detect()` |
| 参数次序对称性 | `collinear(a,b,c)` 与 `collinear(b,c,a)` 等价 | 约束参数规范化 | `constraint_param_canonize()` |

```c
/**
 * @brief 约束图等价类 —— 借鉴 TLA+ 的对称性归约
 *
 * 将对称的约束图状态归入等价类，
 * 在状态空间搜索中仅探索每个等价类的一个代表。
 */
typedef struct {
    uint64_t         canonical_fingerprint; /**< 等价类的规范指纹 */
    StateNode       *representative;        /**< 该等价类的代表状态 */
    int              equivalent_count;      /**< 该等价类中的状态数量 */
} StateEquivalenceClass;

/**
 * @brief 对称性归约的状态搜索
 *
 * 在 TLC 的标准搜索循环中插入对称性归约步骤：
 *
 *   for each generated next_state:
 *       canonical = symmetry_canonize(next_state);
 *       if canonical already seen:
 *           skip;  // 该等价类已探索
 *       else:
 *           mark canonical as seen;
 *           enqueue(next_state);
 *
 * @param explorer   状态空间搜索器
 * @param canonizer  规范形式计算函数
 * @return 归约后的状态数量（用于统计归约效果）
 */
int symmetry_reduce_search(
    StateSpaceExplorer         *explorer,
    GeoStateFingerprint (*canonizer)(GeoConstructionState *)
);
```

---

## 3. Lv-00 映射方案（C 代码级详细设计）

### 3.1 总体架构：TLA+ 三层映射

本节给出将 TLA+ 方法论映射到 Lv-00 C 代码库的完整架构设计。整体分为三层：

```
+------------------------------------------------------------------+
|                    第 3 层：策略与 UI 层                            |
|  proof_strategy.h    |  用户交互策略注册与组合                      |
|  strategy_engine.c   |  策略执行引擎（Ltac 风格的策略组合器）        |
+------------------------------------------------------------------+
|                    第 2 层：验证与搜索层                            |
|  state_space_search.c | TLC 风格的穷举状态搜索                     |
|  invariant_checker.c  | 不变式检查器                               |
|  symmetry_reduce.c    | 对称性归约                                 |
|  proof_tree.c         | TLAPS 风格的层次化证明树                   |
+------------------------------------------------------------------+
|                    第 1 层：规约与构造层                            |
|  geo_spec.h          | Init/Next/Spec 三段式规约定义               |
|  constraint_graph.h  | 约束图（状态表示）                          |
|  pipeline.c          | PlusCal 风格的 DSL 编译管线                 |
+------------------------------------------------------------------+
```

### 3.2 规约层核心结构：Init/Next/Spec 三段式

```c
/**
 * @file geo_spec.h
 * @brief TLA+ 风格的三段式几何构造规约
 *
 * 本模块将 TLA+ 的 Init/Next/Spec 三件套映射到 Lv-00 的几何构造领域。
 * 核心映射关系：
 *   Init   → 初始几何元素的定义
 *   Next   → 允许的构造操作集合
 *   Spec   → 构造有效性约束 + 待证命题
 */

#ifndef LV00_GEO_SPEC_H
#define LV00_GEO_SPEC_H

#include "lv00/constraint_graph.h"
#include "lv00/symbolic_coord.h"
#include "lv00/type_system.h"

/* ================================================================
 * 第 1 部分：Init —— 初始状态的描述
 * ================================================================ */

/**
 * @brief 初始几何元素的声明
 *
 * 对应于 TLA+ 中的 Init 谓词：
 *   Init == /\ points = {p0, p1, ...}
 *           /\ constraints = {c0, c1, ...}
 */
typedef struct {
    /* 自由点：给定但坐标自由的点 */
    SymbolicCoord   **free_points;
    int               free_point_count;

    /* 固定点：坐标完全确定的点（如原点 O(0,0)、单位点 U(1,0)） */
    SymbolicCoord   **fixed_points;
    int               fixed_point_count;

    /* 初始约束：在构造开始前就存在的约束 */
    ConstraintEdge   *initial_constraints;
    int               initial_constraint_count;

    /* 初始线段 / 初始圆 */
    Segment          *initial_segments;
    int               initial_segment_count;
    Circle           *initial_circles;
    int               initial_circle_count;
} GeoInitState;

/**
 * @brief 从初始声明构建初始约束图状态
 *
 * 对应于 TLA+ 中从 Init 公式计算初始状态集合的过程。
 * TLC 的初始状态计算是直接的：Init 是状态谓词，直接求值即得初始状态。
 */
GeoConstructionState *geo_init_from_spec(GeoInitState *init);

/* ================================================================
 * 第 2 部分：Next —— 允许的构造操作
 * ================================================================ */

/**
 * @brief 构造操作的前提条件检查函数类型
 *
 * 对应 TLA+ 动作的 ENABLED 谓词：
 *   ENABLED AddPoint(p) == p \notin points
 */
typedef bool (*GeoPreconditionFn)(
    GeoConstructionState *state,
    const GeoStep        *step
);

/**
 * @brief 构造操作的效果函数类型
 *
 * 对应 TLA+ 动作的 primed 变量赋值：
 *   points' = points \cup {p}
 */
typedef GeoConstructionState *(*GeoEffectFn)(
    GeoConstructionState *state,
    const GeoStep        *step
);

/**
 * @brief 构造操作注册表 —— 对应 TLA+ 的 Next 所有分支
 *
 * 每一个 GeoStepType 在注册表中都有一个条目，
 * 包含前提条件函数和效果函数。
 * 这直接对应 TLA+ 中 Next 的析取（\/）展开：
 *   Next == Action_1 \/ Action_2 \/ ... \/ Action_N
 */
typedef struct {
    GeoStepType          step_type;
    char                *name;            /**< 操作名称 */
    GeoPreconditionFn    precondition;    /**< ENABLED 谓词 */
    GeoEffectFn          effect;          /**< 状态变换函数 */
    char                *description;
} GeoActionEntry;

/* 全局注册表（编译时初始化） */
extern const GeoActionEntry GEO_ACTION_REGISTRY[];
extern const int            GEO_ACTION_REGISTRY_SIZE;

/**
 * @brief 获取当前状态下所有使能的构造操作
 *
 * 对应于 TLA+ 的 ENABLED 谓词批量求值：
 *   对所有 Action_i 求值 ENABLED Action_i
 *
 * @param state       当前构造状态
 * @param out_enabled 输出：使能的步骤类型数组
 * @param max_count   最大输出数量
 * @return 实际使能的步骤数量
 */
int geo_get_enabled_actions(
    GeoConstructionState *state,
    GeoStepType          *out_enabled,
    int                   max_count
);

/* ================================================================
 * 第 3 部分：Spec —— 完整构造规约
 * ================================================================ */

/**
 * @brief 完整几何构造规约
 *
 * 对应 TLA+ 的：
 *   Spec == Init /\ [][Next]_vars /\ WF_vars(Next)
 *
 * Lv-00 中的验证流程自然遵循这一格式：
 *   1. geo_init_from_spec(spec->init)  → 初始状态
 *   2. 从初始状态出发，递归应用 spec->actions 中的使能动作
 *   3. 在每一步检查 spec->invariants 中的不变式
 *   4. 对 spec->properties 中的时序属性执行模型检查
 */
typedef struct {
    GeoInitState        init;            /**< Init 初始状态 */
    GeoActionEntry     *actions;         /**< Next 允许的构造操作 */
    int                 action_count;
    GeoInvariant       *invariants;      /**< 不变式列表 */
    int                 invariant_count;
    TemporalProperty   *temporal_props;  /**< 时序属性（可选） */
    int                 temporal_prop_count;
} GeoSpec;

/**
 * @brief 时序属性 —— 对应 TLA+ 的 Temporal Property
 *
 * 时序属性比不变式更强，描述"最终/总是/直到"等时间模态。
 * 例如："构造最终会终止" → <>[](NoEnabledActions)。
 */
typedef enum {
    TEMPORAL_ALWAYS,        /**< []P：P 在所有状态成立 */
    TEMPORAL_EVENTUALLY,    /**< <>P：P 最终成立 */
    TEMPORAL_LEADS_TO,      /**< P ~> Q：P 最终导致 Q */
    TEMPORAL_UNTIL          /**< P U Q：P 成立直到 Q 成立 */
} TemporalOp;

typedef struct {
    TemporalOp          op;
    GeoInvariant       *lhs;             /**< 左侧不变式（对于一元操作，仅为 lhs） */
    GeoInvariant       *rhs;             /**< 右侧不变式（仅对二元操作有效） */
} TemporalProperty;

/**
 * @brief 对完整规约执行 TLC 风格的模型检查
 *
 * 这是 Lv-00 中最顶层的验证入口。
 * 完整实现了 TLA+ / TLC 的验证循环。
 *
 * @param spec  几何构造规约
 * @param opts  模型检查选项
 * @param out   输出：验证报告
 * @return 验证是否通过
 */
bool geo_spec_model_check(
    GeoSpec             *spec,
    ModelCheckOptions   *opts,
    VerificationReport  *out
);

#endif /* LV00_GEO_SPEC_H */
```

### 3.3 搜索层核心结构：TLC 穷举搜索

```c
/**
 * @file state_space_search.h
 * @brief TLC 风格的显式状态空间搜索器
 *
 * 直接借鉴 TLC 的模型检查算法。
 * 支持 BFS/DFS 两种搜索策略，指纹判重，对称性归约，反例生成。
 */

#ifndef LV00_STATE_SPACE_SEARCH_H
#define LV00_STATE_SPACE_SEARCH_H

#include "lv00/constraint_graph.h"

/* ------ 搜索配置 ------ */

typedef enum {
    SEARCH_STRATEGY_BFS,            /**< 广度优先（默认）—— 找到最短反例 */
    SEARCH_STRATEGY_DFS,            /**< 深度优先 —— 适合深层构造路径 */
    SEARCH_STRATEGY_BEST_FIRST      /**< 启发式最优优先 —— 利用几何启发式 */
} SearchStrategy;

typedef struct {
    SearchStrategy      strategy;
    int                 max_depth;          /**< 最大搜索深度（防止无限搜索） */
    int                 max_states;         /**< 最大探索状态数（资源限制） */
    bool                enable_symmetry_reduce; /**< 启用对称性归约 */
    bool                enable_checkpoint;      /**< 启用断点续传 */
    const char         *checkpoint_path;        /**< 断点文件路径 */
    int                 num_workers;            /**< 并行工作线程数（0=单线程） */
} ModelCheckOptions;

/* ------ 状态指纹 ------ */

/**
 * @brief 状态指纹 —— 借鉴 TLC 的 Fingerprint
 *
 * TLC 使用 64 位指纹（哈希值）来快速判重。
 * 真正的状态比较仅在指纹冲突时进行。
 *
 * 指纹计算方法：对约束图中的所有几何关系进行哈希混合。
 * 相同的约束图（不考虑节点内部 ID）产生相同的指纹。
 */
typedef uint64_t GeoStateFingerprint;

/**
 * @brief 计算约束图状态指纹
 *
 * 使用图规范形式 + 哈希函数，确保对称的图产生相同指纹。
 * 指纹算法必须满足：
 *   1. geo_state_fingerprint(s1) == geo_state_fingerprint(s2)  如果 s1 和 s2 同构
 *   2. geo_state_fingerprint(s1) != geo_state_fingerprint(s2)  绝大多数情况下如果不同构
 */
GeoStateFingerprint geo_state_fingerprint(GeoConstructionState *state);

/* ------ 反例 ------ */

/**
 * @brief 反例 —— TLC 风格的错误追踪
 *
 * 当不变式被违反时，从初始状态到违反状态的完整路径。
 * 包括每个中间状态和执行的构造步骤。
 */
typedef struct {
    int              path_length;            /**< 反例路径长度（步数） */
    GeoStep         *step_sequence;          /**< 导致违反的构造步骤序列 */
    GeoConstructionState **state_sequence;   /**< 反例路径上的状态序列 */
    int              violated_invariant_id;  /**< 被违反的不变式 ID */
    char            *violation_message;      /**< 违反的详细描述 */
} CounterExample;

/* ------ 验证报告 ------ */

typedef struct {
    bool             all_passed;             /**< 所有不变式是否在所有状态成立 */
    int              total_states_explored;  /**< 探索的状态总数 */
    int              total_states_unique;    /**< 唯一个数（去重后） */
    int              max_depth_reached;      /**< 达到的最大深度 */
    double           time_seconds;           /**< 验证耗时 */
    CounterExample  *counterexamples;        /**< 发现的反例列表 */
    int              counterexample_count;
    int              symmetry_reduced;       /**< 对称性归约消除的状态数 */
} VerificationReport;

#endif /* LV00_STATE_SPACE_SEARCH_H */
```

---

## 4. 实现路线图

### 阶段 I：基础规约层（第 1-2 周）

**目标**：完成 Init/Next/Spec 三段式的基本建模能力。

- [ ] 实现 `GeoInitState` 和 `GeoConstructionState` 结构体及序列化
- [ ] 实现 `GeoActionEntry` 注册表，覆盖全部几何构造操作类型
- [ ] 实现 `geo_get_enabled_actions()` —— 前提条件检查
- [ ] 实现 `geo_init_from_spec()` —— 从初始声明构建初始状态
- [ ] 编写单元测试：验证 Init/Next 的基本正确性

**交付物**：`geo_spec.h`、`geo_spec.c` 及对应测试。

### 阶段 II：搜索与模型检查层（第 3-5 周）

**目标**：完成 TLC 风格的穷举状态搜索。

- [ ] 实现状态指纹函数 `geo_state_fingerprint()`
- [ ] 实现 `StateSpaceExplorer` BFS/DFS 搜索循环
- [ ] 实现不变式检查框架 `geo_invariants_check_all()`
- [ ] 实现反例生成 `counterexample_generate()`
- [ ] 实现断点续传 `checkpoint_save/load()`
- [ ] 编写集成测试：小规模几何构造的穷举验证（如三角形构造全部可能性）

**交付物**：`state_space_search.h`、`state_space_search.c`、`invariant_checker.c` 及测试套件。

### 阶段 III：对称性归约与性能优化（第 6-7 周）

**目标**：解决状态空间爆炸问题，实现实用规模的验证。

- [ ] 实现顶点置换对称性归约（顶点标签规范化）
- [ ] 实现约束参数次序规范化 `constraint_param_canonize()`
- [ ] 实现旋转/镜像等价类归约
- [ ] 实现并行搜索（多线程 BFS 分割状态队列）
- [ ] 性能基准测试：对称性归约前后的状态探索数量对比

**交付物**：`symmetry_reduce.h`、`symmetry_reduce.c`、性能基准报告。

### 阶段 IV：证明管理器与集成（第 8-9 周）

**目标**：完成 TLAPS 风格的层次化证明管理与 DSL 编译管线。

- [ ] 实现 `ProofTree` 层次化证明结构
- [ ] 实现证明义务（ProofObligation）到后端求解器的调度
- [ ] 实现 `ProofNavigator` 交互式证明导航
- [ ] 实现 DSL 编译管线 `pipeline_run_full()`
- [ ] 端到端集成测试：从 DSL 源码到完整模型检查

**交付物**：`proof_tree.h`、`proof_tree.c`、`pipeline.c`、端到端测试套件。

---

## 5. 附录：参考文献与相关项目

### 5.1 核心参考文献

1. Lamport, L. (2002). *Specifying Systems: The TLA+ Language and Tools for Hardware and Software Engineers*. Addison-Wesley. — TLA+ 的权威著作，涵盖语言设计哲学和工具链。

2. Lamport, L. (1994). "The Temporal Logic of Actions." *ACM Transactions on Programming Languages and Systems*, 16(3), 872-923. — TLA 的理论基础论文。

3. Newcombe, C., et al. (2015). "How Amazon Web Services Uses Formal Methods." *Communications of the ACM*, 58(4), 66-73. — TLA+ 在工业界最著名的成功案例。

4. Chakraborty, S., et al. (2022). "TLAPS: The TLA+ Proof System." — TLAPS 证明管理器的技术文档。

5. Merz, S. (2008). "The Specification Language TLA+." *Logics of Specification Languages*, 401-451. — TLA+ 语言参考。

### 5.2 相关开源项目

| 项目 | 仓库 | 关联 |
|:---|:---|:---|
| TLA+ 主仓库 | `github.com/tlaplus/tlaplus` | 本报告借鉴对象 |
| Apalache | `github.com/apalache-mc/apalache` | 符号化 TLA+ 模型检查器（SMT-based） |
| TLA+ Community Modules | `github.com/tlaplus/CommunityModules` | TLA+ 标准库扩展 |
| tla-web | `github.com/will62794/tla-web` | TLA+ 的 Web IDE |
| TLAPS | 随 TLA+ 工具链分发 | 层次化证明管理器 |
| PlusCal | 随 TLA+ 工具链分发 | 算法语言前端 |

### 5.3 Lv-00 项目内关联文档

- `docs/reference/lean4_metaprogramming.md` — 证明策略元编程
- `docs/reference/dafny_layered_verification.md` — 层次化验证
- `docs/reference/isabelle_sledgehammer_integration.md` — 多后端调度
- `docs/reference/souffle_datalog_engine.md` — Datalog 关系引擎
- `docs/reference/minizinc_model_data_separation.md` — 模型-数据分离
- `docs/architecture_v3.2.md` — Lv-00 系统架构文档
- `docs/design_v2.9.md` — Lv-00 设计文档
- `include/lv00/constraint_graph.h` — 约束图核心头文件
- `include/lv00/proof.h` — 证明系统核心头文件

---

> **文档版本**：v1.0
> **最后更新**：2026-05-24
> **维护者**：Lv-00 项目组
> **许可证**：MIT
