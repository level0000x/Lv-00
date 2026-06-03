# Lean 4 元编程框架核心借鉴设计

> **借鉴项目**：Lean 4（github.com/leanprover/lean4）
> **核心借鉴点**：Elaborator Monad 元编程框架、卫生宏系统（Hygienic Macro）、Grind SMT 集成策略、`by` 策略块语法
> **分类**：P1 高优先级 / 证明引擎与DSL设计
> **日期**：2026-05-24

---

## 1. 概述

Lean 4 是由 Leonardo de Moura 主导开发的新一代定理证明器，其最根本的架构革新在于将整个证明引擎建构在一个**可编程元编程框架**之上。Lean 4 不再将元编程视为"附加功能"，而是把 Elaborator Monad（`Lean.Elab.Term.ElabM`）作为核心抽象层——类型检查、证明项构造、策略执行、宏展开全部运行在同一元编程框架内。这种"元编程即内核"的设计哲学对 Lv-00 的几何 DSL 和证明引擎架构具有极高的借鉴价值。

Lean 4 元编程框架的三个核心组件——Elaborator 多阶段翻译管道、卫生宏系统（Hygienic Macro）和 `Grind` SMT 集成策略——分别对应 Lv-00 几何元语言的三层需求：（1）几何 DSL 到核心证明项的多阶段翻译；（2）用户友好的语法糖（如 `let point A = intersection(l1, l2)` 自动展开）；（3）符号与数值双轨验证的自动调度。通过将 Lean 4 的元编程模式适配到 C 语言的约束图架构上，Lv-00 可以在不引入完整依赖类型系统的情况下，获得接近定理证明器级别的符号操作灵活性。

在工程层面，Lean 4 的元编程框架基于 Lean 语言自身实现（自举），这为 Lv-00 提供了一个重要启示：几何 DSL 的翻译管道不应作为外部工具实现，而应作为 Lv-00 引擎的内部一等组件，使几何构造、命题模式和证明策略三者在统一的元编程框架内协同工作。本章将详细阐述 Elaborator Monad 管道映射、卫生宏系统对语法糖设计的启发、以及元编程策略框架如何支持用户自定义几何证明策略。

---

## 2. 核心借鉴 → Lv-00 映射

### 2.1 Elaborator 多阶段翻译管道

Lean 4 的 Elaborator 将表面语法（用户输入的 Lean 代码）通过多阶段管道翻译为核心类型论项（kernel terms）。管道结构如下：

```
表面语法（Surface Syntax）
    ↓ [Parser — 语法解析]
语法树（Syntax AST）
    ↓ [Elaborator — 类型推演]
    ├─ 宏展开（Macro Expansion）
    ├─ 类型推导（Type Inference）
    ├─ 类型类解析（Typeclass Resolution）
    ├─ 隐式参数填充（Implicit Argument Synthesis）
    ├─ 策略执行（Tactic Execution）
    └─ 证明项构造（Proof Term Construction）
    ↓
核心项（Kernel Term）
    ↓ [Kernel — 类型检查]
通过 / 失败
```

这一多阶段翻译管道在 Lv-00 中存在精确的对应需求：用户用几何元语言（近似数学自然语言）描述几何构造，系统需要将其翻译为约束图（节点和约束边）。具体映射关系如下：

| Lean 4 Elaborator 阶段 | Lv-00 对应阶段 | 说明 |
|:---|:---|:---|
| Parser / 语法解析 | `diagram_parse()` — 几何声明解析器 | 将几何元语言解析为声明列表 |
| Macro Expansion / 宏展开 | `syntax_expand()` — 语法糖展开遍 | `intersection(l1, l2)` → 两步构造 |
| Type Inference / 类型推导 | `type_infer_region()` — 几何类型区域推导 | 点为 Point，线为 Line，自动推导 |
| Implicit Argument / 隐式参数 | `implicit_bind()` — 隐式绑定遍 | 自动补全未指定的参数约束 |
| Tactic Execution / 策略执行 | `proof_strategy_exec()` — 策略引擎 | 执行用户或系统策略 |
| Proof Term / 证明项构造 | `proof_block_construct()` — 证明块构造 | 将策略序列编译为约束图节点 |
| Kernel Typecheck / 内核类型检查 | `proof_unify()` — 合一检查 | 验证约束图与命题模式的一致性 |

#### Lv-00 几何 DSL 的多阶段翻译管道（C 代码框架）

```c
/**
 * @brief 几何元语言的多阶段翻译管道 — 借鉴 Lean 4 Elaborator Monad
 *
 * 将用户级别的几何声明（语法糖友好的表面语言）
 * 翻译为核心约束图节点（约束图内部表示）。
 *
 * 管道阶段：
 *  1. PARSE    — 语法解析：文本 → 声明 AST
 *  2. EXPAND   — 宏/语法糖展开：复合声明 → 原子操作序列
 *  3. INFER    — 类型推导：推导每个节点的几何类型区域
 *  4. BIND     — 隐式绑定：推导等式中未指定的参数
 *  5. ELAB     — 精化构造：构造约束图节点并建立约束边
 *  6. STRATEGY — 策略执行：运行用户指定或默认的证明策略
 *  7. UNIFY    — 合一检查：验证约束图与命题模式的一致性
 *
 * 类比 Lean 4 的 ElabM monad，每个阶段都运行在相同的上下文
 * (GeomElabContext) 中，阶段之间通过约束图传递中间结果。
 */
typedef enum {
    GEOM_ELAB_STAGE_PARSE,       /**< 语法解析阶段 */
    GEOM_ELAB_STAGE_EXPAND,      /**< 宏/语法糖展开阶段 */
    GEOM_ELAB_STAGE_INFER,       /**< 类型推导阶段 */
    GEOM_ELAB_STAGE_BIND,        /**< 隐式绑定阶段 */
    GEOM_ELAB_STAGE_ELAB,        /**< 精化构造阶段 */
    GEOM_ELAB_STAGE_STRATEGY,    /**< 策略执行阶段 */
    GEOM_ELAB_STAGE_UNIFY        /**< 合一检查阶段 */
} GeomElabStage;

/**
 * @brief 几何精化上下文 — Lv-00 的 ElabM 等价物
 *
 * 贯穿整个翻译管道的共享上下文，借鉴 Lean 4 的 ElabM monad 设计。
 * 每个阶段可以读写上下文的状态，阶段之间通过约束图传递结果。
 */
typedef struct {
    GeomElabStage           current_stage;      /**< 当前翻译阶段 */
    ConstraintGraph        *graph;              /**< 正在构建的约束图 */
    TypeSystem             *type_system;        /**< 类型系统引用 */
    SyntaxMacroTable       *macro_table;        /**< 语法宏表 */
    ProofStrategyRegistry  *strategy_registry;  /**< 证明策略注册表 */
    DiagnosticList         *diagnostics;        /**< 诊断信息列表 */
    bool                    trace_enabled;      /**< 是否启用追踪 */
    void                   *user_data;          /**< 用户扩展数据 */
} GeomElabContext;

/**
 * @brief 运行完整的多阶段翻译管道
 *
 * 对应 Lean 4 中 `runTermElabM` 等顶层入口函数。
 *
 * @param source        用户输入的几何声明文本
 * @param target_graph  目标约束图（已初始化的空图）
 * @param ts            类型系统
 * @param out_result    输出：翻译后的核心构造列表
 * @return 翻译状态码
 */
int geom_elaborate_full_pipeline(
    const char *source,
    ConstraintGraph *target_graph,
    TypeSystem *ts,
    GeomElabResult *out_result
);
```

### 2.2 卫生宏系统（Hygienic Macro）→ Lv-00 语法糖设计

Lean 4 的宏系统基于 **卫生宏**（Hygienic Macro）概念，核心特性包括：

1. **语法引号（Syntax Quotation）**：用 `` `( ) `` 在宏定义中引用 Lean 语法片段，系统保证语法正确性
2. **自动卫生**：宏展开后自动对绑定的名称进行 alpha 转换，避免名称捕获
3. **递归宏**：宏可以在展开时递归调用自身（如 `simp` 策略的重复应用）
4. **策略宏**：宏可以生成策略序列，实现"声明式"→"过程式"的转换

这些特性直接启发 Lv-00 的几何语法糖设计。考虑以下用户友好的几何声明：

```
(* Lv-00 几何元语言示例 *)
let point A = intersection(line(1, 2), line(3, 4))
let segment BC = connect(B, C)
let point D = foot_of_perpendicular(A, BC)
```

这些声明在 Lv-00 内部需要被展开为原子操作序列。卫生宏系统提供了一套系统的展开框架：

| Lean 4 宏特性 | Lv-00 语法糖映射 | 功能 |
|:---|:---|:---|
| 语法引号 `` `() `` | `SYNTAX_MACRO_PATTERN` — 模式匹配模板 | 定义语法糖的匹配模式 |
| 自动卫生 | `bind_variable_macro()` — 变量卫生绑定 | 避免名称捕获（同名变量在不同作用域） |
| 递归宏 | `MACRO_FLAG_RECURSIVE` — 递归展开标记 | `intersection` 的多步展开 |
| 策略宏 | `MACRO_FLAG_TACTIC` — 策略生成宏 | 声明式语法展开为策略序列 |
| `syntax ... :=` | `macro_register_pattern()` — 模式注册 | 注册新的语法糖模式 |

#### 语法糖展开的 C 实现

```c
/**
 * @brief 语法宏表 —— Lv-00 的卫生宏注册系统
 *
 * 借鉴 Lean 4 的 `declare_syntax_cat` + `syntax ... :=` 设计，
 * 允许用户和系统注册几何语法糖模式及其展开规则。
 */
typedef struct SyntaxMacroTable SyntaxMacroTable;

/**
 * @brief 语法宏模式：
 *  pattern  — 匹配的语法模式（如 "let point $name = intersection($l1, $l2)"）
 *  expansion — 展开后的原子操作序列（核心构造 API 调用序列）
 *  flags     — 宏属性标记
 */
typedef struct {
    char     *pattern;           /**< 语法模式（带 $meta 占位符） */
    char     *expansion;         /**< 展开模板 */
    int       flags;             /**< 宏属性标记（MACRO_FLAG_*） */
    char     *category;          /**< 语法类别（"declaration" / "tactic"） */
    void    (*custom_expand)(    /**< 自定义展开函数（可选） */
        GeomElabContext *ctx,
        SyntaxNode *node,
        ConstraintGraph *graph
    );
} SyntaxMacro;

/**
 * @brief 注册语法宏
 *
 * 对应 Lean 4 的 `syntax` / `macro_rules` 声明。
 * 示例：
 *   macro_register_pattern(table,
 *       "let point $name = intersection($l1, $l2)",
 *       "let line $l1_id = bind_line($l1.geo_id);"
 *       "let line $l2_id = bind_line($l2.geo_id);"
 *       "let point $name = point_intersection($l1_id, $l2_id);"
 *       0);
 */
SyntaxMacro *macro_register_pattern(
    SyntaxMacroTable *table,
    const char *pattern,
    const char *expansion,
    int flags
);

/**
 * @brief 展开所有匹配的语法宏 —— 卫生展开阶段
 *
 * 对应 Lean 4 的 elaboration 中的 macro expansion 阶段。
 * 每轮扫描中：
 *  1. 按注册顺序尝试匹配所有宏的 pattern
 *  2. 对每个匹配节点，在同作用域内进行 alpha 转换（卫生）
 *  3. 用 expanded 的模板替换匹配节点
 *  4. 如果有 MACRO_FLAG_RECURSIVE 标记，重复该过程直到固定点
 *
 * @param ctx          精化上下文
 * @param ast_root     语法树的根节点
 * @return 展开后的语法树根节点
 */
SyntaxNode *macro_expand_all(
    GeomElabContext *ctx,
    SyntaxNode *ast_root
);
```

### 2.3 元编程策略框架 → 用户自定义几何证明策略

Lean 4 的策略系统允许用户在元编程层面定义新的证明策略。一个策略本质上是 `TacticM` monad 中的操作，可以查询目标、应用引理、生成子目标，并递归地在子目标上运行策略。Lv-00 借鉴此模式，将几何证明策略定义为一等实体：

```
Lean 4 策略模型:
  TacticM α ≈ Goal → List (Goal × (α → Term))
  策略接收一个目标，返回一组子目标 + 证明项的构造函数

Lv-00 策略模型:
  ProofStrategy ≈ ConstraintGraph → List (Subproof × (Subproof → ProofStep))
  策略接收约束图的当前状态，返回一组子证明 + 证明步骤的构造函数
```

#### 对照表：Lean4 ElabM → Lv-00 proof.h 映射

| Lean 4 构造 | Lv-00 映射 | 说明 |
|:---|:---|:---|
| `ElabM α` | `GeomElabContext` | 精化上下文，携带所有翻译状态 |
| `TacticM α` | `ProofStrategy` 结构体 | 策略抽象，接收上下文返回子证明 |
| `elabTerm` | `geom_elaborate_full_pipeline()` | 完整的精化管道 |
| `mkAppM` | `proof_block_apply_lemma()` | 应用已知引理到当前目标 |
| `mkFreshExprMVar` | `proof_block_new_subgoal()` | 创建新的子证明目标 |
| `tryTactic` / `<\|>` | `STRATEGY_SEQ_ORELSE` 组合子 | 策略的 OR 组合（尝试-回退） |
| `andThenOnSubgoals` | `STRATEGY_SEQ_THEN` | 策略的 THEN 组合（串行应用） |
| `repeat` | `STRATEGY_FLAG_REPEAT` | 重复应用直到固定点 |
| `focus` | `proof_focus_subgoal()` | 在当前子目标上集中执行 |
| `save` | `proof_save()` | 保存已证明的子目标（对应 Qed） |
| `runTactic` | `proof_strategy_exec()` | 运行一个策略 |

#### `by` 策略块在 Lv-00 中的 C API 实现

```c
/**
 * @brief Lv-00 的 "by" 策略块 —— 借鉴 Lean 4 的 tactic block
 *
 * 在 Lv-00 几何元语言中，`by` 块用于标记一个命题需要被策略证明：
 *
 *   proposition[angle_sum ABC = PI] by {
 *       apply(parallel_angle_lemma);      (* 应用平行线角度引理 *)
 *       simplify;                          (* 代数化简 *)
 *       area_method;                       (* 面积法判定 *)
 *       done;                              (* 策略证毕 *)
 *   }
 *
 * 对应 Lean 4 的：
 *   example : angle_sum ABC = PI := by
 *     apply parallel_angle_lemma
 *     simp
 *     area_method
 *     done
 */

/**
 * @brief 证明策略 —— Lv-00 中策略的基本单元
 *
 * 每个策略是一个函数指针，接收当前约束图状态和"焦点"（当前需证明的命题），
 * 返回一组子目标（子证明），或表明证明已完成。
 */
typedef struct ProofStrategy ProofStrategy;

typedef enum {
    STRATEGY_RESULT_DONE,           /**< 策略完成，当前目标已证明 */
    STRATEGY_RESULT_SUBGOALS,       /**< 策略生成子目标（列表） */
    STRATEGY_RESULT_PARTIAL,        /**< 策略部分完成（需交互式介入） */
    STRATEGY_RESULT_FAILED,         /**< 策略无法应用 */
    STRATEGY_RESULT_ERROR           /**< 策略执行错误 */
} StrategyResultKind;

/**
 * @brief 策略执行结果
 */
typedef struct {
    StrategyResultKind  kind;           /**< 结果类型 */
    SubgoalList        *subgoals;       /**< 子目标列表（SUBGOALS 时） */
    ProofStep          *constructed;    /**< 构造的证明步骤（DONE 时） */
    char               *diagnostic;     /**< 诊断信息 */
    int                 depth;          /**< 策略执行的递归深度 */
} StrategyResult;

/**
 * @brief 策略函数签名
 *
 * 对应 Lean 4 中 `TacticM Unit` 的概念：
 *   ctx    — 精化上下文（提供符号表、宏表、类型系统等）
 *   goal   — 当前证明目标（需要验证的命题）
 *   graph  — 约束图（可读写，策略可能添加辅助构造）
 *   返回值 — 策略结果
 */
typedef StrategyResult (*StrategyFn)(
    GeomElabContext *ctx,
    ProofGoal *goal,
    ConstraintGraph *graph
);

/**
 * @brief 策略结构体
 *
 * 除了 StrategyFn，还携带元数据用于组合、显示和调试。
 */
struct ProofStrategy {
    char        *name;              /**< 策略名称（如 "area_method"） */
    char        *description;       /**< 策略描述 */
    StrategyFn   execute;           /**< 策略执行函数 */
    int          flags;             /**< 策略标志（STRATEGY_FLAG_*） */
    int          priority;          /**< 优先级（低值 = 高优先） */
    StrategyFn  *preconditions;     /**< 前置条件检查（可选，NULL = 总是可应用） */
};

/** 策略标志 */
#define STRATEGY_FLAG_REPEAT       (1 << 0)  /**< 重复应用直到固定点 */
#define STRATEGY_FLAG_BACKTRACK    (1 << 1)  /**< 失败时自动回退 */
#define STRATEGY_FLAG_ORACLE       (1 << 2)  /**< 依赖外部求解器（非构造性） */
#define STRATEGY_FLAG_AXIOM        (1 << 3)  /**< 公理级策略（TCB 可审计） */

/**
 * @brief 策略序列组合器 —— THEN 组合
 *
 * 依次执行 strats[0], strats[1], ..., strats[n-1]。
 * 每个策略的输出子目标作为下一个策略的输入目标。
 *
 * 对应 Lean 4 中策略脚本的顺序执行：
 *    tactic1
 *    tactic2
 *    ...
 *
 * @return 组合后的策略（调用者需用 proof_strategy_destroy 释放）
 */
ProofStrategy *proof_strategy_seq_then(
    ProofStrategy *strategies[],
    int count
);

/**
 * @brief 策略选择组合器 —— ORELSE 组合
 *
 * 先尝试 strats[0]，如果失败则回退并尝试 strats[1]，依此类推。
 * 一旦某个策略返回 DONE/SUBGOALS 则停止。
 *
 * 对应 Lean 4 的 <|> 组合子（try-then-backtrack）。
 *
 * @return 组合后的策略
 */
ProofStrategy *proof_strategy_seq_orelse(
    ProofStrategy *strategies[],
    int count
);

/**
 * @brief 创建 `by` 策略块 —— 执行一系列策略来证明一个目标
 *
 * 这是 Lv-00 中对应 Lean 4 `by { tactic1; tactic2; ... }` 的核心 API。
 *
 * 工作流程：
 *  1. 创建 ProofGoal 表示需要证明的命题
 *  2. 构建策略序列（按用户指定的策略列表）
 *  3. 在当前约束图上执行策略序列
 *  4. 检查最终结果：DONE（证毕）/ SUBGOALS（有剩余子目标）
 *  5. 将构造的证明步骤附加到约束图上
 *
 * @param ctx           精化上下文
 * @param proposition   需要证明的命题（约束图上的约束模式）
 * @param strategies    策略列表
 * @param strat_count   策略数量
 * @param graph         约束图
 * @param out_proof_id  输出：构造的证明块 ID
 * @return 策略执行结果
 */
StrategyResult proof_by_tactic_block(
    GeomElabContext *ctx,
    ProofGoal *proposition,
    ProofStrategy **strategies,
    int strat_count,
    ConstraintGraph *graph,
    int *out_proof_id
);

/**
 * @brief 注册用户自定义策略
 *
 * 对应 Lean 4 中用户使用 `elab` 命令定义自定义策略。
 *
 * 示例：
 *   // 注册用户定义的"等腰三角形判定"策略
 *   ProofStrategy isosceles_strat = {
 *       .name = "isosceles_check",
 *       .description = "通过角度/边长等式判定等腰三角形",
 *       .execute = isosceles_check_fn,
 *       .flags = STRATEGY_FLAG_BACKTRACK,
 *       .priority = 50
 *   };
 *   proof_register_strategy(registry, &isosceles_strat);
 *
 * @param registry  策略注册表
 * @param strategy  要注册的策略
 * @return 0 成功，-1 失败（名称冲突）
 */
int proof_register_strategy(
    ProofStrategyRegistry *registry,
    ProofStrategy *strategy
);

/**
 * @brief 重置策略执行的回退栈
 *
 * 当使用 ORELSE 组合子时，每次尝试失败后需要回退约束图状态。
 * 该函数将约束图恢复到最近的保存点。
 */
void proof_strategy_backtrack(GeomElabContext *ctx);
```

---

## 3. 实现方案

### 3.1 第一阶段：几何 DSL 翻译管道基础设施（P1-1）

- [ ] 在 `type_system.h` 附近创建 `geom_elab.h` —— 几何精化上下文和阶段定义
- [ ] 实现 `geom_elaborate_full_pipeline()` 的主循环框架（解析→展开→推导→绑定→精化→策略→合一）
- [ ] 实现 `GeomElabContext` 的初始化和销毁
- [ ] 实现阶段间的上下文传递机制（每阶段读写 `ConstraintGraph`）
- [ ] 为每个阶段定义明确的输入/输出契约（前置条件 + 后置条件断言）
- [ ] 编写管道流程的单元测试（空输入、单声明、错误恢复）

### 3.2 第二阶段：语法宏展开系统（P1-2）

- [ ] 在设计文档中定义 Lv-00 几何元语言的语法类别（declaration, proposition, strategy）
- [ ] 实现 `SyntaxMacroTable` 数据结构（哈希表，按语法类别分组）
- [ ] 实现 `macro_register_pattern()` —— 模式注册
- [ ] 实现 `macro_expand_all()` —— 卫生展开遍（递归至固定点）
- [ ] 实现内置语法糖：`intersection`, `midpoint`, `perpendicular_bisector`, `foot_of_perpendicular` 等
- [ ] 实现变量卫生绑定（`bind_variable_macro()`）—— 宏展开时自动 alpha 转换局部变量名
- [ ] 实现递归宏的正确终止检查（防止无限展开）
- [ ] 编写宏系统的单元测试（展开正确性、卫生性、终止性）

### 3.3 第三阶段：策略引擎核心（P1-3）

- [ ] 实现 `ProofStrategy` 结构体和 `StrategyFn` 函数指针类型
- [ ] 实现 `ProofGoal` —— 证明目标的定义（命题 + 上下文）
- [ ] 实现 `proof_strategy_seq_then()` —— 策略 THEN 组合器
- [ ] 实现 `proof_strategy_seq_orelse()` —— 策略 ORELSE 组合器（带回退）
- [ ] 实现 `proof_by_tactic_block()` —— `by {}` 块的执行引擎
- [ ] 实现 `ProofStrategyRegistry` —— 策略注册表（哈希表，按名称和优先级索引）
- [ ] 实现 `proof_register_strategy()` —— 策略注册
- [ ] 实现 `proof_strategy_backtrack()` —— 约束图状态回退

### 3.4 第四阶段：内置策略集（P1-4）

- [ ] 实现 `area_method` 策略 —— 面积法几何判定
- [ ] 实现 `angle_chase` 策略 —— 角度追踪法
- [ ] 实现 `congruence` 策略 —— 三角形全等/相似判定
- [ ] 实现 `coordinate_bash` 策略 —— 坐标法暴力计算（数值验证）
- [ ] 实现 `simplify` 策略 —— 代数表达式化简
- [ ] 实现 `try_lemma` 策略 —— 已知引理库查找
- [ ] 实现 `auto` 策略 —— 组合所有内置策略的自动证明尝试
- [ ] 编写策略集的集成测试（标准几何题目的自动证明）

### 3.5 第五阶段：Grind 式 SMT 集成（P1-5）

- [ ] 借鉴 Lean 4 `Grind` 的 SMT 集成策略——符号推导 + SMT 求解的自动调度
- [ ] 实现几何约束到 SMT-LIB 的编码（参考 `fstar_refinement_smt.md` 中的编码方案）
- [ ] 实现 SMT 求解器（Z3）的异步调用和超时管理
- [ ] 实现 SMT 结果到几何策略的反编码（SAT → 反例坐标，UNSAT → 证毕确认）
- [ ] 实现 Grind 启发式：优先符号策略，SMT 仅在符号策略失败时调用

---

## 4. 设计决策与权衡

### 4.1 元编程级别的选择：C 语言 vs 嵌入式 ML

Lean 4 的元编程框架得益于 Lean 语言本身是 ML 家族语言，支持代数数据类型、模式匹配和高阶函数。Lv-00 选择 C 语言作为实现语言，这意味着元编程框架（精化上下文、策略函数指针、语法宏表）的抽象层次较低。

**对策**：通过结构化的函数指针表（vtable）模拟高阶函数；通过 tagged union + switch 模式模拟代数数据类型的模式匹配（类似 Lean 4 C 内核的做法）。虽然代码量较大，但保证了性能（零开销抽象）和与现有约束图 API 的集成便利性。

### 4.2 卫生宏 vs. 预处理宏

Lv-00 的几何 DSL 语法糖选择**卫生宏**而非 C 预处理器式宏的原因：
- 卫生宏在展开时保证名称不捕获（避免 `let point A = ...` 与外部已有的 `A` 冲突）
- 卫生宏支持递归展开（`intersection` → 多步原子操作的级联）
- 卫生宏基于同作用域的 alpha 转换，语义清晰

代价是实现复杂度高于简单的字符串替换。但考虑到几何 DSL 的用户友好性目标，这一代价是可接受的。

### 4.3 策略引擎的回退机制

`ORELSE` 组合器需要约束图状态回退——这是一个对 C 风格的约束图不太自然的操作。解决方案：
- 在执行可能失败的策略分支前，对约束图做浅拷贝（copy-on-write）
- 如果策略返回 FAILED，丢弃浅拷贝并恢复到原图
- 如果策略返回 DONE/SUBGOALS，将浅拷贝的变更合并回原图（冲突时提示）

这一机制借鉴了 Lean 4 的 `withNewMCtxDepth` —— 在尝试性策略执行时创建新的元变量上下文深度，失败时回退。

---

## 5. 参考资源

- Lean 4 官方仓库：https://github.com/leanprover/lean4
- Lean 4 元编程文档：https://lean-lang.org/lean4/doc/metaprogramming.html
- Lean 4 定理证明：https://lean-lang.org/theorem_proving_in_lean4/
- "The Lean 4 Theorem Proving Language" — Leonardo de Moura, Sebastian Ullrich (2021)
- "Elaboration in Dependent Type Theory" — de Moura et al. (2015)
- "Grind: A Tactic for SMT-based Proof Automation in Lean" — Lean 4 documentation
- "Hygienic Macro Technology" — Kohlbecker et al. (1986) / Flatt (2016) Racket 实现
- Lv-00 已有借鉴文档：`fstar_refinement_smt.md`（SMT 编码方案）、`proof.h`（证明 API 定义）

---

## 6. 总结

Lean 4 的元编程框架为 Lv-00 提供了三个清晰的设计模式：（1）**多阶段翻译管道**——将几何元语言从表面语法到核心约束图的翻译过程形式化为七个阶段，每阶段有明确定义的输入/输出契约；（2）**卫生宏系统**——为 Lv-00 的几何语法糖提供语义安全的展开机制，避免名称捕获并支持递归定义；（3）**策略组合器框架**——通过 THEN 和 ORELSE 组合子使几何证明策略的构造模块化、可组合，最终通过 `by` 策略块提供用户友好的证明入口。这三者共同构成了 Lv-00 "元编程即引擎"的设计基础，使得几何 DSL 的翻译、证明策略的定义和执行在统一的框架内协同工作。
