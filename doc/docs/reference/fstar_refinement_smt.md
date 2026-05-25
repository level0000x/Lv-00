# F* 精化类型 + SMT 混合验证借鉴设计

> **借鉴项目**：F*（github.com/FStarLang/FStar）
> **核心借鉴点**：精化类型（Refinement Types）+ SMT 混合验证、Ghost 效果分离证明/计算、双引擎架构（类型检查器 + SMT 求解器）
> **分类**：P1 高优先级 / 混合验证引擎与精化类型
> **日期**：2026-05-24

---

## 1. 概述

F*（FStar）是由 Microsoft Research 和 INRIA 联合开发的面向程序验证的函数式编程语言。其核心创新在于将**精化类型**（Refinement Types）与 **SMT（Satisfiability Modulo Theories）自动求解**深度集成，同时通过 **Ghost 效果**严格分离证明与计算。F* 的三个核心特性对 Lv-00 的验证引擎架构有关键借鉴价值：

1. **精化类型 + SMT 混合验证**：F* 的类型系统在简单类型基础上附加逻辑谓词约束（如 `x:nat{x > 0}` 表示"大于 0 的自然数"），类型检查时将验证条件（VC）发送给 Z3 SMT 求解器。这种"类型检查器 + SMT 求解器"的双引擎架构精确对应 Lv-00 的 `type_system.h` + `solver.h` 双引擎设计。

2. **Ghost 效果**：F* 通过效果系统将计算分为 `Tot`（全计算）、`Ghost`（仅用于证明）、`ST`（状态化计算）等。`Ghost` 效果标记的代码在程序提取（extraction）时被完全消除，只保留 `Tot`/`ST` 代码。这与 Lv-00 的"证明仅编译期"概念完美对应。

3. **SMT 编码与反编码**：F* 将类型化程序编码为 SMT-LIB 查询，Z3 返回结果后反编码为 F* 错误位置。Lv-00 可借鉴此模式将几何约束编码为方程系统并发送给代数求解器。

---

## 2. 精化类型 + SMT 映射到 Lv-00 type_system.h + solver.h

### 2.1 F* 的精化类型定义

```
type nat = x:int{x >= 0}                          // 精化：整数 x，满足 x >= 0
type pos = x:int{x > 0}                           // 精化：正整数
type vec a n = l:list a{length l = n}             // 精化：长度为 n 的列表
type sorted_list a = l:list a{is_sorted l}        // 精化：排序列表
```

精化类型的结构：`base_type{logical_predicate}` —— 基础类型 + 逻辑谓词约束。

### 2.2 Lv-00 中的精化类型映射

在 Lv-00 中，精化类型（Refinement Type）可以自然地映射为**附带约束的类型区域**：

| F* 精化类型 | Lv-00 type_system.h 映射 | 说明 |
|:---|:---|:---|
| `x:int{x >= 0}` | `TYPE_KIND_REGION` + `constraint_ids` = [POSITIVE] | 附带宽约束的区域类型 |
| `x:int{x > 0}` | `TYPE_KIND_REGION` + `constraint_ids` = [STRICTLY_POSITIVE] | 严格正约束 |
| `vec a n` | `TYPE_KIND_DEPENDENT` + `body_type` = 长度约束 | 依赖类型 + 长度谓词 |
| `sorted_list` | `TYPE_KIND_PRODUCT` + 有序性约束 | 乘积类型 + 排序谓词 |

**关键设计**：Lv-00 已有的 `TypeRegion.constraint_ids` 字段天然就是精化类型中的"逻辑谓词"——将类型别名扩展为"基础类型 + 约束集"的精化形式。

### 2.3 SMT 双引擎架构

F* 的类型检查流程：

```
F* 程序
  ↓
[类型检查器]
  ├─ 语法检查
  ├─ 类型推导
  └─ 生成验证条件 (VCs)
       ↓
    [Z3 SMT 求解器]
       ├─ 编码 VC 为 SMT-LIB
       ├─ 求解
       └─ 返回 SAT/UNSAT
       ↓
    [反编码]
       ├─ UNSAT → 类型检查通过
       └─ SAT   → 报告反例位置
```

Lv-00 的双引擎架构映射：

```
几何构造 + 命题模式
  ↓
[type_system.h — 类型引擎]
  ├─ 类型区域管理
  ├─ 宇宙层级检查
  ├─ 类型等价（重写路径探索）
  └─ 生成精化约束
       ↓
    [solver.h — 求解引擎]
       ├─ 符号坐标消解
       ├─ 约束方程求解
       ├─ Groebner 基 / 面积法
       └─ 数值逼近验证
       ↓
    [合一检查 proof_unify]
       ├─ 类型满足 → 通过
       └─ 类型冲突 → 报告不匹配
```

### 2.4 Lv-00 的 Refinement Type 类型种类扩展

```c
/**
 * @brief 精化类型——Lv-00 对 F* Refinement Types 的几何化编码
 *
 * 精化类型 = base_type + constraint_set
 * 已有的 TypeRegion.constraint_ids 字段恰好承担 constraint_set 的角色。
 *
 * 增加 TYPE_KIND_REFINEMENT 使精化类型成为一等类型，支持：
 *  - 类型组合：精化类型的精化（精化链）
 *  - 精化类型的子类型关系（精化蕴含检查）
 *  - 精化类型的 SMT 友好编码
 */
typedef enum {
    // ... 已有的种类 ...
    TYPE_KIND_REFINEMENT   /**< 精化类型：base_type{logical_predicate} */
} TypeKindRefinement;
```

**精化类型的构造 API**：

```c
/**
 * @brief 创建精化类型
 *
 * 对应 F* 的 refinement type: x:base_type{predicate}
 * 在 Lv-00 中编码为 base TypeRegion + constraint 数组
 *
 * @param ts           类型系统
 * @param base_type    基础类型（如 TYPE_KIND_LINE_SEGMENT）
 * @param constraints  精化约束数组（如 {LENGTH_EQ, ANGLE_EQ}）
 * @param count        约束数量
 * @return 新创建的精化类型，失败返回 NULL
 *
 * @note 精化约束在几何元语言中的语义：
 *       LINE_SEGMENT{LENGTH_EQ(3)}   → 长度为 3 的线段
 *       POINT{ON_LINE(L), DIST_FROM(O) < r} → 在圆内的点
 */
TypeRegion *type_create_refinement(
    TypeSystem *ts,
    TypeRegion *base_type,
    int *constraints,
    int count
);
```

---

## 3. Ghost 效果实现证明/计算分离

### 3.1 F* 的 Ghost 效果系统

```
let lemma_something (x:nat) : Lemma (x + 1 > x) = ()
  // ^^^^^ Lemma = Ghost 效果，编译后消除

let compute_something (x:nat) : Tot nat = x + 1
  // ^^^ Tot = 全计算，运行时可执行
```

关键区别：
- `Lemma`（Ghost）：类型检查需要但运行时消除
- `Tot`：纯计算，运行时保留

### 3.2 Lv-00 的 Ghost 效果映射

Lv-00 的 Ghost 概念已经在 Idris 2 借鉴文档中建立（`proof_mark_ghost()`）。F* 进一步提供了 **Ghost 效果系统** 的完整概念模型：

| F* 效果 | Lv-00 映射 | 含义 |
|:---|:---|:---|
| `Tot`（全计算） | 普通 `FUNCTION_BLOCK`（无 ghost 标记） | 几何构造，运行时可执行 |
| `Ghost`（鬼影） | `FUNCTION_BLOCK` + `QUANTIFIER_ZERO` | 仅用于证明的辅助构造 |
| `Lemma`（引理） | `ProofStep` + `is_ghost = true` | 编译后完全消除 |
| `ST`（状态） | `FUNCTION_BLOCK` + 约束图外部修改 | 带副作用的几何变换 |

### 3.3 Ghost 效果的约束图生命周期

F* 的 Ghost 提取（extraction）概念对应 Lv-00 的 Ghost 消除：

```
原始约束图（含 Ghost 节点）
         ↓
    [Ghost 消除遍]
     ├─ 识别所有 QUANTIFIER_ZERO 标记的节点/步骤
     ├─ 检查：Ghost 节点不影响非 Ghost 节点的拓扑结构
     ├─ 剥离：移除 Ghost 节点和纯 Ghost 约束边
     └─ 重连：保留 Ghost 节点的非 Ghost 下游
         ↓
    精简约束图（仅含可执行几何构造）
```

---

## 4. proof_refinement_check() API 设计

### 4.1 函数声明（追加到 proof.h）

```c
/**
 * @brief 精化类型检查 —— 借鉴 F* Refinement Types + SMT 混合验证
 *
 * 对几何构造执行精化类型检查：验证构造图中的节点是否满足
 * 其声明类型上的所有精化约束（refinement predicates）。
 *
 * 工作流程（借鉴 F* 的 typechecker + Z3 双引擎）：
 *  1. 提取节点的精化约束（从 TypeRegion.constraint_ids）
 *  2. 将约束编码为代数/几何方程系统
 *  3. 启动双引擎验证：
 *     a. [type_system.h 引擎]：通过重写路径探索进行符号验证
 *     b. [solver.h 引擎]：通过代数求解（Groebner 基/面积法）进行等式验证
 *  4. 两个引擎的结果合并：
 *     - 任一引擎返回 SATISFIED → 通过
 *     - 两个引擎都返回 UNKNOWN → 需交互式证明
 *     - 任一引擎返回 VIOLATED → 报告反例/冲突约束
 *  5. 返回精化检查报告
 *
 * 精化类型在 Lv-00 中的几何语义示例：
 *  - POINT{ON_SEGMENT(AB)}     → 点必须在 AB 线段上
 *  - SEGMENT{LENGTH_EQ(5)}     → 线段长度必须为 5
 *  - CIRCLE{RADIUS_GT(0)}      → 圆的半径必须大于 0
 *  - TRIANGLE{IS_RIGHT_ANGLE}  → 三角形必须是直角三角形
 *
 * @param nav              证明导航器
 * @param node_id          要检查的节点 ID（-1 = 检查所有节点）
 * @param ts               类型系统（提供类型和重写规则）
 * @param solver           求解器引擎（提供代数求解能力，可为 NULL 表示仅符号验证）
 * @param out_report       输出：精化检查报告（调用者需用 lv00_free 释放）
 * @return 检查结果
 *
 * @note 与 proof_unify 的区别：
 *       proof_unify 检查构造图与命题模式是否一致（图级合一）
 *       proof_refinement_check 检查节点的精化约束是否满足（类SMT验证）
 *
 * @see fstar_refinement_smt.md —— F* 混合验证参考
 * @see proof_minimal_verify() —— 类似的最小化信任验证
 */
RefinementCheckResult proof_refinement_check(
    ProofNavigator *nav,
    int node_id,
    TypeSystem *ts,
    ConstraintSolver *solver,
    char **out_report
);
```

### 4.2 精化检查结果类型

```c
/**
 * @brief 精化检查结果
 */
typedef enum {
    REFINEMENT_SATISFIED,        /**< 所有精化约束满足 */
    REFINEMENT_VIOLATED,         /**< 存在违反的精化约束（有反例） */
    REFINEMENT_PARTIALLY,        /**< 部分满足（某些约束无法判定） */
    REFINEMENT_UNKNOWN,          /**< 无法判定（需交互式证明） */
    REFINEMENT_TIMEOUT,          /**< 检查超时 */
    REFINEMENT_ERROR             /**< 检查内部错误 */
} RefinementCheckResult;

/**
 * @brief 单条精化约束的检查结果
 */
typedef struct {
    int constraint_id;           /**< 约束 ID */
    char *constraint_description; /**< 约束的自然语言描述 */
    bool satisfied;              /**< 是否满足 */
    char *counterexample;        /**< 反例描述（VIOLATED 时，可为 NULL） */
    char *engine_used;           /**< 使用的验证引擎（"type_system" / "solver" / "both"） */
} RefinementCheckEntry;

/**
 * @brief 精化检查完整报告
 */
typedef struct {
    RefinementCheckResult result;        /**< 总结果 */
    RefinementCheckEntry *entries;       /**< 各约束检查结果 */
    int entry_count;                     /**< 约束数量 */
    int satisfied_count;                 /**< 满足的数量 */
    int violated_count;                  /**< 违反的数量 */
    int unknown_count;                   /**< 未知的数量 */
    char *formatted_report;              /**< 格式化的可读报告 */
    int64_t type_engine_time_ms;         /**< 类型引擎耗时 */
    int64_t solver_engine_time_ms;       /**< 求解器引擎耗时 */
} RefinementCheckReport;

/**
 * @brief 释放精化检查报告
 */
void refinement_check_report_destroy(RefinementCheckReport *report);
```

### 4.3 ConstraintSolver 求解器概念类型

```c
/**
 * @brief 约束求解器（solver.h 的前向概念声明）
 *
 * 对应 F* 中 Z3 的角色。在 Lv-00 中，solver 负责将
 * 几何约束转换为代数方程系统并求解。
 *
 * 求解器能力：
 *  - 符号坐标消解
 *  - 等式系统求解（Groebner 基）
 *  - 不等式检查（面积/长度非负等）
 *  - 数值逼近验证
 */
typedef struct ConstraintSolver ConstraintSolver;
```

---

## 5. 实现路线图

### 5.1 第一阶段：精化类型基础设施（P1-1）

- [ ] 在 `type_system.h` 中增加 `TYPE_KIND_REFINEMENT` 枚举值
- [ ] 实现 `type_create_refinement()` 构造器
- [ ] 实现精化类型的规范化（展开精化链）
- [ ] 实现精化类型的子类型关系检查（精化蕴含）
- [ ] 编写精化类型的单元测试

### 5.2 第二阶段：SMT 编码引擎（P1-2）

- [ ] 设计几何约束 → SMT-LIB 编码规则
  - 点约束 → 实数变量声明
  - 线段长度约束 → 距离公式等式
  - 角度约束 → 余弦/正弦等式
  - 共线/平行约束 → 行列式等式
- [ ] 实现 SMT-LIB 编码器
- [ ] 集成外部 SMT 求解器（Z3/CVC5 命令行调用）
- [ ] 实现 SMT 结果反编码（UNSAT → 通过，SAT → 反例坐标）
- [ ] 实现 SMT 结果到几何约束的映射

### 5.3 第三阶段：双引擎协同（P1-3）

- [ ] 实现 `proof_refinement_check()` 核心逻辑
- [ ] 实现类型引擎验证路径（基于重写路径探索）
- [ ] 实现求解器引擎验证路径（基于代数求解）
- [ ] 实现双引擎结果合并策略
- [ ] 实现超时管理和引擎选择启发式

### 5.4 第四阶段：Ghost 消除遍（P1-4）

- [ ] 实现 Ghost 节点的依赖分析（哪些非 Ghost 节点依赖 Ghost 节点）
- [ ] 实现 Ghost 约束的剥离算法
- [ ] 实现 Ghost 消除后的约束图一致性验证
- [ ] 在代码生成管道中插入 Ghost 消除遍
- [ ] 编写 Ghost 消除的单元测试和集成测试

---

## 6. 设计决策与权衡

### 6.1 双引擎 vs 单引擎

F* 的双引擎（类型检查器 + Z3）比 Lv-00 的单引擎（合一检查）更强大但更复杂。Lv-00 选择双引擎的原因：

- **类型引擎**（type_system.h）：适合处理结构化约束（类型等价、宇宙层级、重写规则），输出为确定性的"是/否"
- **求解器引擎**（solver.h）：适合处理数值/代数约束（距离计算、面积关系、角等式），输出可为反例

两个引擎互补覆盖不同的约束类型，合并结果时采用"任一满足即通过"的乐观策略。

### 6.2 SMT 编码的边界

几何约束的 SMT 编码存在理论上的限制：
- 涉及超越数（如 π）的约束难以精确编码为 SMT
- 辐射角的三角约束需要非线性实数运算（NRA），Z3 对此支持有限
- 复杂的几何构造可能导致编码后的公式规模爆炸

缓解策略：
- 优先使用符号型引擎（面积法/Groebner 基）处理简单几何
- SMT 仅作为"最后一招"处理复杂非线性约束
- 对 SMT 结果设置严格的超时限制（默认 10s）

### 6.3 Ghost 消除 vs Idris 2 量词 0

F* 的 Ghost 效果系统和 Idris 2 的 QTT 量词 0 都实现"证明仅编译期"：

| 方面 | F* Ghost | Idris 2 QTT 0 | Lv-00 Ghost |
|:---|:---|:---|:---|
| 理论基础 | 效果系统（Effect System） | 量词类型论（QTT） | 步骤元数据标记 |
| 标记位置 | 函数/引理级别 | 函数参数级别 | 证明步骤级别 |
| 消除机制 | 程序提取（extraction） | 编译期擦除（erasure） | Ghost 消除遍 |
| 粒度 | 整个函数体 | 单个参数 | 单个证明步骤 |

Lv-00 选择步骤级别的 Ghost 标记，提供最细粒度的控制，适用于几何构造中辅助线与核心构造的精确分离。

---

## 7. 补充：F* 的 SMT 编码策略与 Lv-00 适配细节

### 7.1 F* 的验证条件生成（VCG）

F* 的类型检查器在遇到精化类型时，生成 SMT 验证条件（Verification Conditions）：

```
(* 检查 x:nat{x > 0} 的精化 *)
let vc = "x >= 0 ∧ not (x > 0)"  (* 否定精化条件以寻找反例 *)

(* 发送给 Z3 *)
Z3.check_sat vc → UNSAT → 没有满足 x >= 0 且 ¬(x > 0) 的 x → 精化成立
```

在 Lv-00 中，对应流程为：
```
1. 提取 TypeRegion.constraint_ids 中的约束列表
2. 将每条约束转换为代数方程/不等式
3. 否定目标约束（寻找反例）
4. 调用 solver.h 求解
5. 若反例不可满足 → 精化成立
```

### 7.2 F* 效果系统的完整层级

F* 的效果系统包含一个完整的层级：

```
效果层级（从弱到强）：
  Pure     — 纯计算，无副作用
  Ghost    — 鬼影计算，仅用于证明
  Div      — 可能不终止的计算
  ST       — 状态化计算（堆操作）
  Exn      — 可能抛出异常
  ALL      — 任意效果
```

在 Lv-00 中，对应的效果层级简化为：
```
GEOMETRIC       — 纯几何构造（无副作用）
GHOST           — 鬼影构造（仅证明，编译期消除）
NUMERIC         — 数值逼近构造（依赖浮点精度）
ORACLE          — 外部求解器构造（依赖非构造性推理）
```

### 7.3 F* 程序提取（Extraction）与 Lv-00 Ghost 消除的对比

F* 的程序提取流程：
1. 类型检查通过（含所有 Ghost 引理验证）
2. `fstar --extract` 提取阶段
3. 剥离所有 Ghost 效果的函数和引理
4. 仅保留 Tot/ST 代码
5. 输出为目标语言（OCaml/F#/C）

Lv-00 的 Ghost 消除流程（对应）：
1. 合一检查通过（含所有 Ghost 步骤的验证）
2. Ghost 消除遍
3. 剥离所有 QUANTIFIER_ZERO 标记的步骤和节点
4. 重连被 Ghost 隔断的约束图拓扑
5. 输出精简约束图

关键区别：F* 的提取保留类型结构（类型在运行时仍有检查意义），Lv-00 的 Ghost 消除直接简化约束图（约束在运行时被完全移除）。

---

## 8. 总结

F* 的精化类型 + SMT 混合验证架构为 Lv-00 提供了从简单合一检查升级到双引擎协同验证的升级路径。精化类型（`base_type{logical_predicate}`）与 Lv-00 已有的 `TypeRegion.constraint_ids` 字段天然对应，无需引入新的类型理论。SMT 编码/反编码循环为几何约束的自动求解提供了工程上成熟的方案（借鉴 F*→Z3→反编码）——Lv-00 将其适配为几何约束→代数方程→求解器→结果翻译。F* 的效果系统层级启发了 Lv-00 将构造步骤分层为纯几何/Ghost/数值/Oracle 四类，每种对应不同的信任颜色和运行时行为。Ghost 效果系统进一步强化了 Lv-00 从 Idris 2 借鉴的"证明仅编译期"概念，使辅助构造与核心构造在生命周期的每个阶段都被明确区分。
