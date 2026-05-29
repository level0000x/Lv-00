# Rocq/Coq 微内核架构与 Ltac 策略语言核心借鉴设计

> **借鉴项目**：Rocq/Coq（github.com/rocq-prover/rocq）
> **核心借鉴点**：内核-外围微内核架构（TCB 最小化）、Ltac 策略语言（模式匹配+回溯+组合器）、SSReflect 反射证明、证明检查器与策略引擎分离
> **分类**：P1 高优先级 / 证明引擎架构与策略系统
> **日期**：2026-05-24

---

## 1. 概述

Rocq（原 Coq）是历史最悠久、工程化程度最高的定理证明器之一，其最核心的架构原则——**内核-外围微内核架构**（Microkernel Architecture）——深刻影响了整个交互式定理证明领域。Coq 将信任计算基（Trusted Computing Base, TCB）严格限制在约 2000 行 OCaml 代码的"内核"（Kernel）中，负责核心类型检查；而策略语言（Ltac）、自动化证明（auto/eauto）、SSReflect 反射证明等复杂功能全部位于"外围"——即使外围代码存在漏洞，也无法构造虚假证明，因为任何证明项都必须通过内核的独立类型检查。

Coq 的 **Ltac 策略语言**是交互式定理证明中最为成熟的策略 DSL 之一。Ltac 将策略（Tactic）视为一等公民，提供了模式匹配（`match goal with`）、回溯执行（`try` / `||`）、组合器（`;` 串行、`||` 选择、`repeat` 循环）和用户自定义策略（`Ltac` 宏）。这种"策略即程序"的设计范式为 Lv-00 的几何证明策略引擎提供了直接的设计蓝图：几何证明策略（如面积法、吴方法、角度追踪法）可以映射为 Ltac 风格的可组合策略，支持回溯和模式匹配。

**SSReflect**（Small Scale Reflection）是 Coq 中广受赞誉的证明语言扩展，其核心思想是将证明项本身作为计算对象进行"反射"操作——通过小型反射计算来简化证明步骤。这一思想对 Lv-00 的几何证明有特殊价值：几何证明中的大量步骤（如坐标代入、边长计算、角度求和）本质上是"计算"而非"推理"，SSReflect 的反射策略可以将这些计算步骤自动化为 rewriter/calculator 的调用。

---

## 2. 核心借鉴 → Lv-00 映射

### 2.1 微内核架构 → Lv-00 proof.h TCB 设计

Coq 的微内核架构将整个系统分为两层：

```
+-----------------------------------------------+
|          外围（非信任）                          |
|  Ltac策略 | SSReflect | auto/eauto | ring/field |
|  提取(Extraction) | 文档生成 | IDE 交互         |
+-----------------------------------------------+
            ↕ 仅通过 type-check 接口
+-----------------------------------------------+
|          内核（信任计算基 TCB）                   |
|  归纳构造演算(CIC) | 类型检查 | 宇宙检查          |
|  约2000行 OCaml | 独立于所有外围组件              |
+-----------------------------------------------+
```

任何证明项（即使是自动生成的）都必须通过内核的类型检查器验证。外围策略引擎的漏洞不能产生虚假证明，因为内核是唯一的信任锚点。

Lv-00 可以借鉴这一架构，将 `proof.h` 中的**证明检查器**（`proof_unify` / `proof_minimal_verify`）设计为最小的 TCB：

| Coq 内核组件 | Lv-00 proof.h TCB 映射 | 功能 |
|:---|:---|:---|
| 归纳构造演算 (CIC) | 约束图合一规则 (Unification Rules) | 类型论 → 约束图论 |
| 类型检查器 (type_check) | `proof_unify()` | 合一检查，验证约束图与命题模式一致 |
| 宇宙层级检查 | `type_universe_check()` | 几何类型宇宙一致性 |
| 规范性检查 | `proof_normalize()` | 约束图的范式计算 |
| 转换检查 (convertibility) | `type_rewrite_find_path()` | 重写路径探索实现类型等价 |
| 归纳类型 | `TypeRegion` 系统结构 | 类型的归纳定义 |
| 内核模块 | `proof.h` 中的 ~500 行核心 API | TCB 总行数目标：~600 行 C |

#### 微内核架构在 Lv-00 中的 C 实现设计

```c
/**
 * @file proof.h — Lv-00 的"微内核"
 *
 * 借鉴 Rocq/Coq 的微内核架构，将 proof.h 设计为 Lv-00 的最小信任计算基（TCB）。
 *
 * 设计原则：
 *  1. 最小化：TCB 代码量控制在 ~600 行 C 代码以内
 *  2. 自包含：不依赖任何外部库（只依赖标准约束图结构）
 *  3. 可审计：每个 TCB 函数都有明确的前置/后置条件注释
 *  4. 独立验证：TCB 中的检查可被外部验证器复现
 *
 * TCB 组成（按信任级别排序）：
 *  Level 0（最高信任）：合一规则（Unification Rules）—— 约8条核心规则
 *  Level 1（推导信任）：重写路径探索（Rewrite Path Finding）—— 约6条规则
 *  Level 2（结构信任）：类型区域系统（TypeRegion）—— 约10条规则
 *
 * 任何外围组件（策略引擎、自动化证明、SMT 集成）的最终输出
 * 必须通过 TCB 的 proof_unify() 检查才能被接受。
 */

/**
 * @brief Lv-00 TCB 验证结果
 *
 * 与 Coq 内核的 type_check 返回值对应：
 *   - 类型检查通过 → 证明项被接受
 *   - 类型检查失败 → 证明项被拒绝（不关心是哪条外围策略生成的）
 */
typedef enum {
    TCB_CHECK_PASS,           /**< TCB 验证通过 */
    TCB_CHECK_FAIL,           /**< TCB 验证失败 — 类型不匹配 */
    TCB_CHECK_UNIVERSE_ERR,   /**< 宇宙层级冲突 */
    TCB_CHECK_CYCLE,          /**< 约束图中的非法循环 */
    TCB_CHECK_INTERNAL_ERR    /**< TCB 内部错误（应触发 assert） */
} TcbCheckResult;

/**
 * @brief TCB 验证入口 —— 最小的信任锚点
 *
 * 这是 Lv-00 中唯一必须信任的函数（以及它的直接依赖）。
 * 所有策略引擎、自动化证明的输出都必须通过此入口验证。
 *
 * @param graph       约束图（包含要验证的构造和命题）
 * @param proof_block 证明块（策略引擎生成的证明步骤序列）
 * @param out_report  输出：验证报告（可选的详细诊断）
 * @return TCB 验证结果
 *
 * @note 这是 TCB 的核心——其正确性定义了 Lv-00 证明系统的可靠性。
 *       任何对此函数的修改都必须经过形式化审查。
 */
TcbCheckResult proof_tcb_verify(
    ConstraintGraph *graph,
    ProofBlock *proof_block,
    TcbVerificationReport *out_report
);
```

### 2.2 Ltac 策略语言 → Lv-00 ProofStrategy 引擎

Ltac 的核心构造及其在 Lv-00 中的映射关系：

| Ltac 构造 | 语义 | Lv-00 映射 | 函数 |
|:---|:---|:---|:---|
| `t1; t2` | 串行：先执行 t1，在 t1 的每个子目标上执行 t2 | `STRATEGY_SEQ_THEN` | `proof_strategy_seq_then()` |
| `t1 \|\| t2` | 选择：先尝试 t1，失败则回溯并尝试 t2 | `STRATEGY_SEQ_ORELSE` | `proof_strategy_seq_orelse()` |
| `try t` | 尝试：执行 t，失败则无效果 | `STRATEGY_FLAG_TRY` + `ORELSE` | `proof_strategy_try()` |
| `repeat t` | 重复：重复执行 t 直到失败 | `STRATEGY_FLAG_REPEAT` | `proof_strategy_repeat()` |
| `match goal with ... end` | 模式匹配目标 | `proof_goal_pattern_match()` | 匹配约束图模式 |
| `apply lemma` | 应用已知引理 | `proof_block_apply_lemma()` | 引理实例化 |
| `assert (H : P)` | 声明子命题 | `proof_subgoal_create()` | 创建子证明 |
| `Ltac name := ...` | 定义命名策略 | `proof_register_strategy()` | 注册自定义策略 |
| `progress t` | 要求策略必须推动证明进展 | `STRATEGY_FLAG_PROGRESS` | 进展检查 |
| `solve [t1 \| t2 \| ...]` | 解决目标：尝试每个 ti 直到一个完全解决 | `proof_strategy_solve()` | 解决组合器 |

#### 模式匹配目标：`match goal with ... end`

Ltac 最强大的特性之一是**目标模式匹配**——根据当前证明目标的形状选择不同的策略分支：

```
(* Ltac 示例 *)
Ltac my_tactic :=
  match goal with
  | |- ?A = ?A => reflexivity
  | |- ?X + 0 = ?X => rewrite add_0_r
  | [ H: is_triangle ?T |- area ?T > 0 ] => apply triangle_area_pos with H
  | _ => idtac "cannot solve"
  end.
```

在 Lv-00 中，目标模式匹配被映射为约束图模式匹配：

```c
/**
 * @brief 证明目标模式 —— Ltac 的 "goal pattern"
 *
 * 借鉴 Ltac 的 match goal with 语法，在约束图上进行模式匹配。
 * 一个 goal_pattern 描述约束图的"形状"——特定节点和约束边构成的子图。
 */
typedef struct {
    char        *pattern_name;      /**< 模式名称（如 "triangle_area"） */
    int         *node_types;        /**< 节点类型列表（TYPE_KIND_* 数组） */
    int          node_count;        /**< 节点数量 */
    int         *constraint_types;  /**< 约束类型列表 */
    int          constraint_count;  /**< 约束数量 */
    char        *bindings;          /**< 模式绑定（?A, ?T 等变量的名称列表） */
    StrategyFn   branch_fn;         /**< 匹配成功时执行的策略分支 */
} GoalPattern;

/**
 * @brief 目标模式匹配 —— 借鉴 Ltac 的 match goal with
 *
 * 在给定的证明目标上尝试所有注册的模式。
 * 返回第一个成功匹配的模式（按注册顺序），或 NULL 表示无匹配。
 *
 * @param ctx        精化上下文
 * @param goal       当前证明目标
 * @param patterns   模式列表
 * @param count      模式数量
 * @param out_binding 输出：匹配时的变量绑定（?A → 具体节点ID）
 * @return 匹配的模式，或 NULL
 *
 * 示例用法：
 *   GoalPattern patterns[] = {
 *       {"identity", ..., reflexivity_fn},
 *       {"triangle_area_positive", ..., triangle_area_fn},
 *   };
 *   GoalPattern *matched = proof_goal_pattern_match(ctx, goal, patterns, 2, &binding);
 *   if (matched) matched->branch_fn(ctx, goal, graph);
 */
GoalPattern *proof_goal_pattern_match(
    GeomElabContext *ctx,
    ProofGoal *goal,
    GoalPattern *patterns,
    int count,
    GoalBinding *out_binding
);
```

### 2.3 SSReflect 反射证明 → Lv-00 几何计算自动化

SSReflect 的核心思想是 **小规模反射**（Small Scale Reflection）：将证明项作为可计算的值，通过反射将逻辑命题转化为计算任务。在几何证明中，这一思想尤为适用：

- 坐标代入计算：将符号坐标代入几何约束，通过代数化简判定真伪
- 边长/面积计算：用面积公式自动计算三角形面积，比较等式两边
- 角度归一化：将角度表达式自动化简为规范形式（0-2π 范围内）

SSReflect 的核心策略及其 Lv-00 映射：

| SSReflect 策略 | 功能 | Lv-00 映射 | 几何应用 |
|:---|:---|:---|:---|
| `move=>` | 引入假设 | `proof_intro_hypothesis()` | 引入"AB = CD"等假设 |
| `apply:` | 应用引理 | `proof_apply_lemma()` | 应用三角形全等引理 |
| `rewrite` | 等量代换 | `proof_rewrite_by_equality()` | 用已知等式代换 |
| `case:` | 分类讨论 | `proof_case_analysis()` | 点在线上/线外分类 |
| `by` | 简单证明 | `proof_trivial_verify()` | 恒等式验证 |
| `//` | 化简解决 | `proof_simplify_and_solve()` | 代数化简 + 判定 |
| `congr` | 同余推理 | `proof_congruence_chain()` | 全等链 |

#### 反射计算：几何公式的自动求值

```c
/**
 * @brief 反射计算器 —— 借鉴 SSReflect 的 small-scale reflection
 *
 * SSReflect 的 "proof by computation" 思想在 Lv-00 中体现为：
 * 对于可计算的几何约束（坐标代入、距离计算、角度求和），
 * 直接通过计算验证而非通过演绎推理。
 *
 * 几何反射计算器支持的运算：
 *  - 坐标代入求值：已知 A(0,0), B(3,0), C(0,4)，计算 AB 长度 = 3
 *  - 距离公式计算：dist(A, B) = sqrt((x_B - x_A)^2 + (y_B - y_A)^2)
 *  - 面积公式计算：S(ABC) = 0.5 * |det(B-A, C-A)|
 *  - 角度归一化：将角度值映射到 [0, 2π) 或 [-π, π)
 *  - 代数恒等式验证：直接展开等号两边并比较
 */
typedef struct GeomReflectionCalculator GeomReflectionCalculator;

/**
 * @brief 创建几何反射计算器
 *
 * @param ts 类型系统（提供重写规则）
 * @return 反射计算器实例
 */
GeomReflectionCalculator *geom_reflection_calc_create(TypeSystem *ts);

/**
 * @brief 反射计算：验证两个几何表达式是否相等
 *
 * 这是 SSReflect 中 `//` 和 `by` 策略的几何化版本。
 *
 * 工作流程：
 *  1. 提取 lhs 和 rhs 中所有符号坐标的具体值
 *  2. 代入距离/面积/角度公式进行计算
 *  3. 比较计算结果（在数值容差范围内）
 *  4. 返回 PASS（相等）或 FAIL（不等）或 UNKNOWN（无法计算）
 *
 * @param calc         反射计算器
 * @param lhs          左表达式（约束图节点）
 * @param rhs          右表达式（约束图节点）
 * @param graph        约束图（提供坐标绑定）
 * @param tolerance    数值容差（如 1e-9）
 *
 * @return REFLECT_PASS / REFLECT_FAIL / REFLECT_UNKNOWN
 */
ReflectResult geom_reflection_calc_verify_equality(
    GeomReflectionCalculator *calc,
    ConstraintNode *lhs,
    ConstraintNode *rhs,
    ConstraintGraph *graph,
    double tolerance
);

/**
 * @brief 反射计算：化简几何表达式
 *
 * 借鉴 SSReflect 的 `simpl` 策略。
 * 对几何表达式进行代数化简：
 *   sin(0) → 0, cos(0) → 1
 *   dist(A, A) → 0
 *   angle_sum + 0 → angle_sum
 *   2 * (a + b) / 2 → a + b
 *
 * @param calc    反射计算器
 * @param expr    要化简的表达式节点
 * @param graph   约束图
 * @param out_simplified 输出：化简后的表达式
 * @return 是否真正发生了化简（progress）
 */
bool geom_reflection_calc_simplify(
    GeomReflectionCalculator *calc,
    ConstraintNode *expr,
    ConstraintGraph *graph,
    ConstraintNode **out_simplified
);
```

---

## 3. 几何判定过程的 Ltac 策略化

### 3.1 面积法（Area Method）→ Lv-00 策略

面积法是几何定理机器证明的经典方法（由张景中、Chou、Gao 等发展），其核心思想是将几何命题转化为关于三角形面积的有理等式，通过面积坐标消元来判定命题真伪。

面积法作为 Ltac 风格策略的完整实现：

```c
/**
 * @brief 面积法策略 —— 几何定理证明的经典判定过程
 *
 * 面积法的核心思想：
 *  1. 选取一组非共线点作为"自由点"（面积坐标系的基）
 *  2. 将所有几何约束（共线、平行、中点、比值等）编码为面积等式
 *  3. 将目标命题也编码为面积等式
 *  4. 通过面积消元简化等式系统
 *  5. 检查目标等式是否被约束等式蕴含
 *
 * 面积法在 Lv-00 中的策略化实现：
 *  - strategy_fn = area_method_execute
 *  - 使用模式匹配判断当前目标是否适用于面积法
 *  - 如果适用，构造面积坐标系统并进行消元
 *  - 如果消元后恒等式成立 → 证明完成
 *  - 如果消元后非恒等式 → 无法用面积法证明（交给下一个策略）
 */

/**
 * @brief 面积法策略的执行函数
 *
 * 策略步骤：
 *  1. 分析约束图，提取所有"自由点"（未被其他点约束确定的点）
 *  2. 选取 3 个非共线自由点作为面积基 (O, U, V)
 *  3. 对所有其他点 P，定义其面积坐标：
 *       x_P = S(P, V, O) / S(U, V, O)
 *       y_P = S(U, P, O) / S(U, V, O)
 *  4. 将每个几何约束翻译为关于 S(., ., .) 的面积等式
 *  5. 将目标命题也翻译为面积等式
 *  6. 通过 Groebner 基或逐次消元简化等式系统
 *  7. 检查目标是否被等式系统蕴含
 *
 * 面积法的适用范围（在几何题中的覆盖率约 60-70%）：
 *  - 共线点约束 (collinear)
 *  - 平行线约束 (parallel)
 *  - 中点约束 (midpoint)
 *  - 线段比例相等
 *  - 面积比例相等
 *  - 不需要角度和距离的纯仿射几何命题
 *
 * @return STRATEGY_RESULT_DONE — 面积法证毕
 *         STRATEGY_RESULT_FAILED — 面积法不适用（交给下个策略）
 *         STRATEGY_RESULT_SUBGOALS — 部分化简，有剩余子目标
 */
StrategyResult area_method_execute(
    GeomElabContext *ctx,
    ProofGoal *goal,
    ConstraintGraph *graph
);

/**
 * @brief 面积坐标构造器
 *
 * 选择面积坐标系的原点 O 和基向量 OU, OV。
 * 对于约束图中的每个点 P，计算其面积坐标 (x_P, y_P)。
 */
typedef struct {
    int  origin_id;          /**< 原点 O 的节点 ID */
    int  u_id;               /**< U 点的节点 ID */
    int  v_id;               /**< V 点的节点 ID */
    int *point_ids;          /**< 所有点的节点 ID 列表 */
    double *x_coords;        /**< 面积 x 坐标数组 */
    double *y_coords;        /**< 面积 y 坐标数组 */
    int   point_count;       /**< 点数量 */
    bool  basis_valid;       /**< 基是否合法（O,U,V 非共线） */
} AreaCoordinateSystem;

/**
 * @brief 构造面积坐标系
 *
 * @param graph       约束图
 * @param origin_id   原点 O
 * @param u_id        U 点
 * @param v_id        V 点
 * @return 面积坐标系，失败返回 NULL
 */
AreaCoordinateSystem *area_coordinate_system_build(
    ConstraintGraph *graph,
    int origin_id,
    int u_id,
    int v_id
);

/**
 * @brief 将几何约束编码为面积等式
 *
 * 编码规则示例：
 *   collinear(A, B, C) → S(A, B, C) = 0
 *   parallel(AB, CD)   → S(A, B, C) = S(A, B, D)
 *   midpoint(M, A, B)  → S(A, M, O) = S(B, M, O) 对所有 O
 *
 * @param coord_sys   面积坐标系
 * @param constraint  几何约束
 * @param out_eqs     输出：面积等式列表
 * @return 成功编码的等式数量
 */
int area_method_encode_constraint(
    AreaCoordinateSystem *coord_sys,
    ConstraintNode *constraint,
    AreaEquationList *out_eqs
);
```

### 3.2 吴方法（Wu's Method）→ Lv-00 策略

吴方法是几何定理机器证明的另一个经典方法（由吴文俊提出），核心是将几何命题转化为多项式方程组的零点问题，通过特征列（Characteristic Set）消元来判定命题。

```c
/**
 * @brief 吴方法策略 —— 几何定理的多项式消元证明
 *
 * 吴方法的核心思想：
 *  1. 将几何约束条件编码为多项式方程（按一定顺序排列）
 *  2. 计算该多项式集合的"特征列"（Characteristic Set / 吴-Ritt 特征列）
 *  3. 将目标命题也编码为多项式
 *  4. 对目标多项式关于特征列做伪除法（pseudo-division）
 *  5. 如果伪除法的余式恒为零 → 命题被特征列蕴含 → 定理成立
 *
 * 吴方法的优势：
 *  - 适用于涉及多项式方程的大多数几何命题
 *  - 计算过程完全代数化，无需启发式
 *  - 理论完备（对一类广泛的几何定理）
 *
 * 吴方法的局限：
 *  - 需要处理非退化条件（Non-Degeneracy Conditions, NDCs）
 *  - 多项式膨胀可能导致大型中间表达式
 *  - 效率取决于特征列计算中的变量排序
 */
StrategyResult wu_method_execute(
    GeomElabContext *ctx,
    ProofGoal *goal,
    ConstraintGraph *graph
);

/**
 * @brief 特征列计算器配置
 */
typedef struct {
    int  *variable_order;       /**< 变量排序（关键：影响消元效率） */
    int   var_count;            /**< 变量数量 */
    bool  auto_order;           /**< 自动计算最优变量排序 */
    int   max_poly_degree;      /**< 多项式最大次数限制 */
    int   timeout_ms;           /**< 超时限制（毫秒） */
} CharSetConfig;

/**
 * @brief 计算多项式集合的特征列
 *
 * @param polys      输入多项式集合
 * @param config     特征列计算配置
 * @param out_charset 输出：特征列
 * @param out_ndcs   输出：非退化条件列表
 * @return 0 成功，-1 失败
 */
int wu_method_compute_characteristic_set(
    PolynomialList *polys,
    CharSetConfig *config,
    PolynomialList *out_charset,
    PolynomialList *out_ndcs
);
```

### 3.3 策略注册与自动调度

```c
/**
 * @brief 注册所有内置几何证明策略
 *
 * 策略按优先级排序（数字越小优先级越高）：
 *  1. trivial_reflection    (优先: 10) — 反射计算（恒等式直接求值）
 *  2. area_method           (优先: 20) — 面积法
 *  3. angle_chase           (优先: 30) — 角度追踪法
 *  4. wu_method             (优先: 40) — 吴方法
 *  5. congruence            (优先: 50) — 全等/相似判定
 *  6. coordinate_bash       (优先: 60) — 坐标暴力计算
 *  7. smt_oracle            (优先: 90) — SMT 外部求解器（最后手段）
 */
void proof_register_builtin_strategies(ProofStrategyRegistry *registry);

/**
 * @brief 自动策略 —— 尝试所有注册策略直到一个成功
 *
 * 借鉴 Coq 的 `auto` 策略：按优先级顺序尝试所有注册的策略，
 * 每个策略失败时自动回退并尝试下一个。
 *
 * @param ctx    精化上下文
 * @param goal   证明目标
 * @param graph  约束图
 * @param registry 策略注册表
 * @return 第一个成功的策略的结果
 */
StrategyResult proof_strategy_auto(
    GeomElabContext *ctx,
    ProofGoal *goal,
    ConstraintGraph *graph,
    ProofStrategyRegistry *registry
);
```

---

## 4. 实现方案

### 4.1 第一阶段：TCB 微内核审计与加固（P1-1）

- [ ] 审计 `proof.h` 中所有现有函数的信任级别，标注 TCB 边界
- [ ] 将 TCB 函数压缩到 ~600 行 C 代码以内
- [ ] 为每个 TCB 函数编写详细的前置条件 / 后置条件注释
- [ ] 实现 `proof_tcb_verify()` —— TCB 统一验证入口
- [ ] 实现 TCB 验证报告的格式化输出
- [ ] 编写 TCB 的回归测试套件（覆盖正常路径和所有异常路径）

### 4.2 第二阶段：策略引擎核心（P1-2）

- [ ] 实现 `ProofGoal` 结构体 —— 证明目标的定义
- [ ] 实现 `GoalPattern` 和 `proof_goal_pattern_match()` —— 目标模式匹配
- [ ] 实现 `proof_strategy_seq_then()` —— 策略 THEN 组合器
- [ ] 实现 `proof_strategy_seq_orelse()` —— 策略 ORELSE 组合器（带回退）
- [ ] 实现 `proof_strategy_try()` / `proof_strategy_repeat()` / `proof_strategy_solve()`
- [ ] 实现 `proof_strategy_auto()` —— 自动策略调度器

### 4.3 第三阶段：几何判定过程实现（P1-3）

- [ ] 实现 `geom_reflection_calc_create()` / `geom_reflection_calc_verify_equality()` / `geom_reflection_calc_simplify()`
- [ ] 实现 `AreaCoordinateSystem` 和面积坐标构造
- [ ] 实现 `area_method_encode_constraint()` —— 几何约束到面积等式的编码
- [ ] 实现 `area_method_execute()` 策略函数
- [ ] 实现 `wu_method_compute_characteristic_set()` —— 特征列计算
- [ ] 实现 `wu_method_execute()` 策略函数

### 4.4 第四阶段：集成与测试（P1-4）

- [ ] 注册所有内置策略到 `ProofStrategyRegistry`
- [ ] 实现 `proof_strategy_auto()` 的端到端集成
- [ ] 设计标准几何测试用例库（至少 50 道经典几何题）
- [ ] 编写策略引擎的性能基准测试
- [ ] 编写策略组合器和回溯机制的单元测试

---

## 5. 设计决策与权衡

### 5.1 TCB 的规模目标

Coq 内核约 2000 行 OCaml，但 OCaml 的模式匹配和代数数据类型使代码比等价 C 代码更紧凑。Lv-00 目标 600 行 C 代码的 TCB 是考虑到 C 语言的冗长特性后调整的目标。关键的 TCB 函数列表：

- `proof_unify()` — 合一检查（~80 行）
- `proof_normalize()` — 范式计算（~60 行）
- `type_rewrite_find_path()` — 重写路径探索（~100 行）
- `type_universe_check()` — 宇宙层级检查（~40 行）
- `type_region_match()` — 类型区域匹配（~70 行）
- 辅助函数 — 约 150 行
- TCB 入口封装 — 约 100 行

### 5.2 面积法 vs 吴方法的选择策略

面积法的优势在于高效（多项式次数低、消元简单）、适用于仿射几何的大部分命题。吴方法的优势在于理论完备性更高、可处理涉及圆和角度的非线性几何。Lv-00 采用"面积法优先、吴方法兜底"的策略调度：

```
auto 策略调度：
  1. reflection_calc (PASS) → 完成
  2. area_method (PASS) → 完成
  3. angle_chase (PASS) → 完成
  4. wu_method (PASS) → 完成
  5. congruence  (PASS) → 完成
  6. coordinate_bash + SMT → 最终手段
```

### 5.3 策略回溯的性能代价

ORELSE 组合器的回退需要保存/恢复约束图状态。Coq 中通过维护"证明树"（proof tree）的原生支持高效回退。在 Lv-00 中，使用约束图浅拷贝 + copy-on-write 方案来平衡性能和内存。每个策略执行前创建一个轻量级"快照"，如果该策略失败，丢弃快照；如果成功，快照的修改合并回原图。

---

## 6. 参考资源

- Rocq/Coq 官方仓库：https://github.com/rocq-prover/rocq
- Coq Reference Manual — Chapter 9: Ltac: https://coq.inria.fr/doc/master/refman/proof-engine/ltac.html
- SSReflect 证明语言：https://coq.inria.fr/doc/master/refman/proof-engine/ssreflect-proof-language.html
- "Mathematical Components" (MathComp) — SSReflect tutorial: https://math-comp.github.io/mcb/
- Coq's Architecture — The Kernel (coq/coq#kernel): 约 2000 行 OCaml 的 TCB
- "The Area Method" — Chou, Gao, Zhang (1994) — Journal of Automated Reasoning
- "Wu's Method for Automated Geometry Theorem Proving" — Wen-Tsun Wu (1978)
- "A Survey of Automated Geometry Theorem Proving" — Timothy Stokes, John Harrison
- Lv-00 已有借鉴文档：`fstar_refinement_smt.md`（混合验证）、`hol_light_microkernel.md`（微内核架构）
- Lv-00 TCB 定义：`proof.h` 中的核心 API（`proof_unify`, `proof_minimal_verify` 等）

---

## 7. 总结

Rocq/Coq 为 Lv-00 提供了三个核心架构模式：（1）**微内核 TCB 最小化**——将 `proof.h` 设计为约 600 行 C 代码的最小信任计算基，所有外围策略的输出必须通过独立的内核验证；（2）**Ltac 策略语言**——将几何证明策略设计为可组合、可回溯、支持模式匹配的一等实体，通过 THEN/ORELSE/REPEAT/SOLVE 组合器实现策略复用；（3）**几何判定过程的策略化**——面积法（Area Method）和吴方法（Wu's Method）作为 Ltac 风格策略实现，由自动策略调度器按优先级顺序调用，实现"面积法优先、吴方法兜底"的混合几何证明架构。
