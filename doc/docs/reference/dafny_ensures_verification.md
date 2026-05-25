# Dafny 程序即规约一体化验证借鉴设计

> **版本**: 1.0.0
> **日期**: 2026-05-24
> **参考**: [Dafny](https://github.com/dafny-lang/dafny) —— 验证感知编程语言，"程序 + 规约统一体"设计范式
> **目标**: 借鉴 Dafny 的 `ensures` 规约内嵌、auto-active 自动验证和 `calc` 链式计算语句，将 Lv-00 的几何构造建模为"构造体（方法）+ 命题（ensures 子句）"的统一体，实现构造完成即自动验证的工作流

---

## 目录

1. [项目概述](#1-项目概述)
2. [核心借鉴要点](#2-核心借鉴要点)
3. [Lv-00 映射方案](#3-lv-00-映射方案)
4. [实现路线图](#4-实现路线图)

---

## 1. 项目概述

### 1.1 Dafny 是什么

Dafny 是由微软研究院（Rustan Leino 团队）开发的验证感知编程语言。其核心理念是"程序 + 规约统一体"——程序员在编写可执行代码的同时，在同一源文件中嵌入形式化规约（前置条件 `requires`、后置条件 `ensures`、循环不变式 `invariant`），编译器在编译时自动验证代码是否满足规约。Dafny 的三个核心特性对 Lv-00 的构造-验证一体化设计有直接借鉴价值：

1. **"程序 + 规约统一体"设计**：Dafny 的每个方法（method）都可以携带 `requires`（前置条件）和 `ensures`（后置条件）子句。例如 `method Sort(a: array<int>) ensures sorted(a) ensures multiset(a[..]) == multiset(old(a[..]))`——方法体是可执行的排序代码，`ensures` 子句声明"排序后数组有序"和"元素多重集不变"。这种"代码 + 规约同体"的设计是 Lv-00 将几何构造与命题声明统一的直接灵感来源。

2. **Auto-active 自动验证**：Dafny 的验证器在编译时自动运行——不需要用户显式调用验证命令，编译器自动将方法的 `ensures` 子句转换为验证条件（VC），发送给 Z3 SMT 求解器。用户只需在必要时提供 `assert` 和 `invariant` 作为验证提示。这种"自动运行 + 可选提示"的验证模式非常适合 Lv-00 的交互式几何证明——构造完成时自动触发验证，用户可选择性提供辅助引理。

3. **`calc` 链式计算语句**：Dafny 提供 `calc` 语句用于以可读的链式格式展示逐步等式推导。例如 `calc { a + b; == { lemma1; } b + a; == { lemma2; } a + c; }` 展示了从 `a + b` 到 `a + c` 的推导链，每步都标注了依赖的引理。这种格式自然地对齐几何证明中"从已知构造逐步推导目标命题"的链式推理风格。

### 1.2 为什么借鉴 Dafny

Lv-00 当前的几何构造与命题验证是分离的——用户先完成构造，然后切换到"命题模式"声明要证明的命题，最后手动触发验证。这种分离造成了三个问题：(1) 用户需要在构造界面和验证界面之间切换；(2) 构造完成后容易忘记声明命题；(3) 构造步骤与证明步骤之间没有直接关联。借鉴 Dafny 的 ensurses 一体设计意味着：

1. 每个几何构造块自动携带一个 `ensures` 命题声明——"构造 = 方法，命题 = ensures"
2. 构造完成时自动触发验证——"方法体完成 → 自动验证所有 ensures 子句"
3. 链式推导以可读的步骤化格式展示——"构造链 = calc 语句"

---

## 2. 核心借鉴要点

### 2.1 "程序 + 规约统一体"设计

Dafny 的核心语法结构：

```
method ConstructMidpoint(a: Point, b: Point) returns (m: Point)
    requires a != b                     // 前置条件：a 和 b 是不同的点
    ensures Distance(m, a) == Distance(m, b)  // 后置条件：m 在 a 和 b 的垂直平分线上
    ensures Collinear(a, m, b)                // 后置条件：a, m, b 共线（m 在线段 ab 上）
{
    // 方法体：执行中点构造
    m := Point((a.x + b.x) / 2.0, (a.y + b.y) / 2.0);
}
```

关键结构：
- `method` 声明 + `returns` 参数 → 几何构造的输入/输出
- `requires` 子句 → 构造的前置条件（例如"两个点不能重合"）
- `ensures` 子句 → 构造完成后必须满足的命题（即"构造的性质"）
- 方法体 → 实际的几何构造步骤

### 2.2 Auto-active 自动验证

Dafny 的验证器是自动运行的——不需要手动触发：

```
程序源文件 (.dfy)
       ↓
[Dafny 编译器]
  ├─ 解析 method 声明
  ├─ 提取所有 requires / ensures 子句
  ├─ 将方法体转换为 SSA（静态单赋值）形式
  ├─ 生成验证条件（VCs）
  ├─ [Z3 SMT 求解器] 自动验证
  │    ├─ VC satisfied → 验证通过
  │    └─ VC failed → 报告验证失败位置
  └─ 生成可执行代码
```

用户可以在方法体中插入 `assert`（断言）和 `invariant`（循环不变式）作为验证提示，但不需要显式调用验证命令。验证失败时，Dafny 会报告具体的失败位置和反例。

### 2.3 `calc` 链式计算语句

Dafny 的 `calc` 语句将等式推导以可读的链式格式展示：

```
calc {
    Distance(A, B) * 2;
==  { DefinitionOfMidpoint(m, A, B); }
    Distance(A, m) * 4;
==  { SymmetricProperty(A, m); }
    Distance(m, A) * 4;
==  { MidpointDistanceTheorem(A, m, B); }
    Distance(m, B) * 4;
}
```

每行格式：`表达式; == { 依赖的引理; } 下一个表达式;`。这种格式直接对齐几何证明中的链式推导——"从已知构造到目标命题，每一步标注依赖的几何定理"。

### 2.4 核心借鉴点映射表

| Dafny 概念 | Lv-00 对应概念 | 映射说明 |
|:---|:---|:---|
| `method` 声明 | `FUNCTION_BLOCK`（几何构造块） | 可执行的几何构造 |
| `returns` 参数 | 构造块的输出节点 | 构造产生的新几何对象 |
| `requires` 子句 | 前置约束（构造输入的类型/几何约束） | 构造的前置条件 |
| `ensures` 子句 | `PropositionPattern`（命题模式） | 构造完成后应满足的几何命题 |
| 方法体（代码） | `proof_step` 序列（构造步骤） | 实现构造的具体步骤 |
| `assert` 断言 | 构造中的辅助检查步骤 | 用户提供的验证提示 |
| `invariant` 不变式 | 循环构造中的不变量 | 迭代构造中的不变性质 |
| Auto-active 验证 | `construction_auto_verify()` | 构造完成时自动验证 |
| `calc` 语句 | `proof_chain_display()` | 链式推导可视化 |
| VC 生成 | 几何约束 → 代数方程的编码 | 约束编码为验证条件 |
| Z3 验证 | `solver.h` 代数求解 | 约束系统的求解验证 |
| 验证失败报告 | 构造图中的冲突约束高亮 | 失败位置的可视化定位 |

---

## 3. Lv-00 映射方案

### 3.1 几何构造 = method，命题 = ensures

将 Dafny 的"程序 + 规约统一体"映射到 Lv-00 的构造块模型：

```c
/**
 * @brief 带规约的几何构造块 —— 借鉴 Dafny method + ensures 一体设计
 *
 * 在 Lv-00 中，每个 FUNCTION_BLOCK 天然就是一个 "method"。
 * 扩展约束图节点，使每个构造块携带:
 *   - requires:  构造的前置条件（输入节点的类型/几何约束）
 *   - ensures:   构造完成后应满足的命题模式（即"这个构造想证明什么"）
 *   - body:      构造步骤序列（method body）
 *
 * Dafny 对应关系:
 *   Dafny method      → Lv-00 FUNCTION_BLOCK
 *   Dafny requires    → Lv-00 前置约束（input_constraints）
 *   Dafny ensures     → Lv-00 PropositionPattern（目标命题）
 *   Dafny body        → Lv-00 proof_step 序列（构造步骤）
 *   Dafny assert      → Lv-00 proof_minimal_verify()（辅助检查）
 *   Dafny auto-verify → Lv-00 construction_auto_verify()（自动验证）
 */

typedef struct {
    /* 构造块标识 */
    int block_id;                          /**< 构造块 ID */
    char *block_name;                      /**< 构造块名称（如 "ConstructMidpoint"） */

    /* Dafny method 映射 */
    int *input_node_ids;                   /**< 输入节点 ID（对应 method 参数） */
    int input_count;                       /**< 输入数量 */
    int *output_node_ids;                  /**< 输出节点 ID（对应 returns 参数） */
    int output_count;                      /**< 输出数量 */

    /* Dafny requires 映射 */
    int *requires_constraint_ids;          /**< 前置约束 ID 数组 */
    int requires_count;                    /**< 前置约束数量 */

    /* Dafny ensures 映射 */
    int *ensures_proposition_ids;          /**< 后置命题 ID 数组（要证明的命题） */
    int ensures_count;                     /**< 后置命题数量 */

    /* Dafny body 映射 */
    int *proof_step_ids;                   /**< 构造步骤 ID 序列 */
    int proof_step_count;                  /**< 构造步骤数量 */

    /* 验证状态 */
    bool requires_satisfied;               /**< 前置条件是否满足 */
    bool ensures_verified;                 /**< 所有 ensures 命题是否已验证 */
    int failed_ensures_id;                 /**< 失败的 ensures 命题 ID（-1 = 全部通过） */
    char *verification_error;              /**< 验证失败原因（反例描述） */

    /* 自动验证配置 */
    bool auto_verify_enabled;              /**< 是否启用构造完成时自动验证 */
    int64_t auto_verify_timeout_ms;        /**< 自动验证超时 */
} VerifiableConstructionBlock;
```

### 3.2 构造完成即自动验证

```c
/**
 * @brief 构造完成时自动验证 ensures 命题 —— 借鉴 Dafny auto-active 验证
 *
 * 当用户在 Web GUI 中完成一个几何构造（添加了所有必需的步骤）后，
 * 自动触发对所有 ensures 命题的验证。
 *
 * 验证流程：
 *  1. 检查所有 requires 前置条件是否满足
 *     - 输入节点的类型是否符合要求
 *     - 前置几何约束是否在约束图中已成立
 *  2. 如果 requires 不满足 → 报告前置条件失败，中止验证
 *  3. 如果 requires 满足 → 对每个 ensures 命题：
 *     a. 在约束图中建立验证子图（命题涉及的节点和约束）
 *     b. 调用 proof_unify() 执行图级合一检查
 *     c. 如果合一失败 → 调用 solver_incremental_solve() 进行代数验证
 *     d. 记录每个命题的验证结果
 *  4. 汇总结果：
 *     - 所有 ensures 通过 → 构造标记为"已验证"，构造块变为绿色
 *     - 部分通过 → 构造标记为"部分验证"，失败命题高亮
 *     - 全部失败 → 构造标记为"未验证"，报告验证错误
 *
 * @param graph         约束图（含构造数据）
 * @param block_id      要验证的构造块 ID
 * @param solver        求解器引擎（可为 NULL，走纯合一验证）
 * @param out_report    输出：验证报告（调用者需释放）
 * @return 验证结果
 *
 * @see dafny_ensures_verification.md —— Dafny auto-active 参考
 */
typedef enum {
    VERIFY_ALL_PASSED,          /**< 所有 ensures 命题通过验证 */
    VERIFY_PARTIAL,             /**< 部分命题通过 */
    VERIFY_ALL_FAILED,          /**< 所有命题失败 */
    VERIFY_REQUIRES_FAILED,     /**< 前置条件未满足 */
    VERIFY_TIMEOUT,             /**< 验证超时 */
    VERIFY_ERROR                /**< 内部错误 */
} AutoVerifyResult;

typedef struct {
    int proposition_id;              /**< 命题 ID */
    char *proposition_description;   /**< 命题描述 */
    bool passed;                     /**< 是否通过 */
    char *failure_detail;            /**< 失败细节（反例/冲突约束） */
    int64_t verify_time_ms;          /**< 本命题验证耗时 */
} EnsuresVerifyEntry;

typedef struct {
    AutoVerifyResult overall_result;      /**< 总结果 */
    EnsuresVerifyEntry *entries;          /**< 各命题验证结果 */
    int entry_count;                      /**< 命题数量 */
    int passed_count;                     /**< 通过数量 */
    int failed_count;                     /**< 失败数量 */
    char *formatted_report;               /**< 格式化的可读报告 */
    int64_t total_time_ms;                /**< 总耗时 */
} AutoVerifyReport;

AutoVerifyResult construction_auto_verify(
    ConstraintGraph *graph,
    int block_id,
    ConstraintSolver *solver,
    AutoVerifyReport **out_report
);
```

### 3.3 `calc` 链式推导的可视化映射

```c
/**
 * @brief 证明链步骤 —— 借鉴 Dafny calc 语句的可读链式推导展示
 *
 * 将几何证明的逐步推导以链式格式编码:
 *   Step 1:  已知状态 (KnownState)
 *   Step 2:  == { Lemma / Construction applied } →
 *           推导后状态 (DerivedState)
 *   ...
 *   最终到达目标命题
 *
 * 在 Web GUI 中以可折叠的展开式列表展示，每一步可点击查看几何可视化。
 */
typedef struct {
    int step_index;                     /**< 步骤序号 */
    char *state_before;                 /**< 推导前状态描述 */
    char *state_after;                  /**< 推导后状态描述 */
    char *justification;                /**< 推导依据（引理/公理/构造名称） */
    int justification_theorem_id;       /**< 依赖定理 ID（-1 = 基本构造） */
    bool is_verified;                   /**< 此步推导是否已通过验证 */
    int *highlighted_node_ids;          /**< 此步涉及的需要高亮的节点 ID */
    int highlighted_count;              /**< 高亮节点数量 */
} ProofChainStep;

/**
 * @brief 构建并展示证明链（对应 Dafny calc 语句）
 *
 * 给定一条从根到目标命题的证明路径，生成可读的链式演示。
 *
 * @param nav           证明导航器
 * @param target_node   证明树的目标节点
 * @param out_chain     输出：证明链步骤数组（调用者需释放）
 * @param out_count     输出：步骤数量
 * @return 成功返回 0，失败返回 -1
 */
int proof_chain_build(
    ProofNavigator *nav,
    int target_node,
    ProofChainStep **out_chain,
    int *out_count
);
```

**Web GUI 中的 calc 式展示示例**：

```
证明链：三角形 ABC 的垂直平分线交于一点

已知:
  |AB| = 线段 AB 的长度（由构造给出）
  |AC| = 线段 AC 的长度（由构造给出）

Step 1: 构造 AB 的垂直平分线 L1
  状态:        A, B 已知 → 构造完成，L1 为 AB 的垂直平分线
  依据:        [中点 + 垂线构造法]
  状态: ✓ 已验证

Step 2: 构造 AC 的垂直平分线 L2
  状态:        A, C 已知 → 构造完成，L2 为 AC 的垂直平分线
  依据:        [中点 + 垂线构造法]
  状态: ✓ 已验证

Step 3: L1 ∩ L2 = {O}
  状态:        两条不平行直线相交于唯一点 O
  依据:        [两直线相交定理]
  状态: ✓ 已验证

Step 4: 证明 |OA| = |OB| = |OC|
  状态:        O 在 L1 上 ⇒ |OA| = |OB|
               O 在 L2 上 ⇒ |OA| = |OC|
               ∴ |OA| = |OB| = |OC|
  依据:        [垂直平分线性质定理]
  状态: ✓ 已验证

结论:     三角形 ABC 的垂直平分线交于外心 O，且 O 到三个顶点的距离相等
```

### 3.4 构造-验证一体化的工作流

将 Dafny 的"写代码即验证"工作流映射到 Lv-00 的 Web GUI 交互模式：

```
Dafny 工作流                          Lv-00 Web GUI 工作流
─────────────────────────────────────────────────────────────
1. 写 method 声明                    1. 用户选择"新建几何构造块"
   (名称 + 参数 + requires/ensures)     └─ 填写构造名称、输入/输出节点、目标命题

2. 写方法体（代码）                   2. 用户在画布上执行构造步骤
   (逐步实现算法)                       └─ 每个步骤实时添加到构造块

3. 编译器自动验证                    3. 构造完成 → 自动触发验证
   (后台运行 Z3)                        └─ 绿色 ✓ 出现在每个已验证的 ensures

4. 验证失败 → 修改代码                4. 验证失败 → 约束冲突高亮
   (根据反例调整实现)                   └─ 失败的命题红色标记，显示反例

5. 添加 assert 辅助验证              5. 添加辅助构造/引理
   (给验证器更多提示)                   └─ 选择性添加辅助线或中间引理
```

### 3.5 约束图中的 requires/ensures 集成

在 `constraint_graph.h` 中，每个 `GeomNode` 可以携带其类型约束（requires），而 `Constraint` 可以标记为 ensures 类型：

```c
/**
 * @brief Constraint 的类型扩展 —— 区分 requires 和 ensures
 *
 * 借鉴 Dafny 的 requires/ensures 区分：
 *  - CONSTRAINT_REQUIRES: 构造的前置条件（输入节点必须满足的约束）
 *  - CONSTRAINT_ENSURES:  构造的后置条件（构造完成后要验证的命题）
 *  - CONSTRAINT_INTERNAL: 构造过程中的中间约束（类似于 assert）
 */
typedef enum {
    CONSTRAINT_REQUIRES,         /**< 前置条件约束（Dafny requires） */
    CONSTRAINT_ENSURES,          /**< 后置条件约束（Dafny ensures） */
    CONSTRAINT_INTERNAL,         /**< 中间约束（Dafny assert/invariant） */
    CONSTRAINT_DEDUCED           /**< 推导出的约束（由求解器自动添加） */
} ConstraintRole;

/**
 * @brief 约束结构体扩展 —— 增加约束角色字段
 */
typedef struct {
    // ... 现有字段 ...
    ConstraintRole role;              /**< 约束角色（requires/ensures/internal/deduced） */
    int owner_block_id;               /**< 所属构造块 ID（-1 = 无归属） */
    bool is_verified;                 /**< 约束是否已通过验证 */
    int verified_by_theorem_id;       /**< 验证该约束的定理 ID（-1 = 直接构造） */
} ConstraintV2;
```

---

## 4. 实现路线图

### 4.1 第一阶段：构造块规约基础设施（P1）

- [ ] 在 `constraint_graph.h` 中增加 `ConstraintRole` 枚举（requires/ensures/internal/deduced）
- [ ] 扩展 `Constraint` 结构体增加 `role`、`owner_block_id`、`is_verified` 字段
- [ ] 实现 `construction_block_create()` —— 创建带 requires/ensures 的构造块
- [ ] 实现 `construction_block_add_ensures()` —— 为构造块添加 ensures 命题
- [ ] 在 Web GUI 中增加"构造块属性面板"—— 显示/编辑 requires 和 ensures
- [ ] 编写单元测试

### 4.2 第二阶段：自动验证引擎（P1-P2）

- [ ] 实现 `construction_auto_verify()` 核心逻辑
- [ ] 实现前置条件检查（requires 验证）
- [ ] 实现后置条件检查（ensures 验证 —— 先合一、再求解器）
- [ ] 实现 `AutoVerifyReport` 报告生成
- [ ] 在 Web GUI 中实现验证状态的可视化
  - 通过：绿色 ✓ 图标
  - 失败：红色 ✗ 图标 + 反例信息
  - 进行中：橙色旋转图标
- [ ] 实现验证结果的实时推送（通过 `lv00_protocol.h`）

### 4.3 第三阶段：calc 链式推导展示（P2-P3）

- [ ] 实现 `ProofChainStep` 数据结构
- [ ] 实现 `proof_chain_build()` 链式推导构建
- [ ] 在 Web GUI 中实现 calc 风格的可视化展示
  - 可折叠的步骤列表
  - 每步点击可高亮几何对象
  - 每步显示依赖的定理/引理
- [ ] 实现证明链的导出（JSON/Markdown）

### 4.4 第四阶段：一体化工作流（P3）

- [ ] 构造完成事件监听 → 自动触发 `construction_auto_verify()`
- [ ] 验证失败时的交互式引导（提示可能缺少的引理/辅助线）
- [ ] 构造块模板库（常见几何构造的预置 requires/ensures）
- [ ] 性能优化：仅验证受影响构造块的 ensures（而非全局重验证）

---

## 5. 设计决策与权衡

### 5.1 隐式验证 vs 显式验证

Dafny 的验证是完全隐式的——编译器自动运行验证，用户不需要写"验证"命令。Lv-00 的几何构造是否也应该完全自动？

| 方面 | 完全自动（Dafny 风格） | 手动触发 |
|:---|:---|:---|
| 用户体验 | 低认知负担，构造完成即知结果 | 用户控制验证时机 |
| 性能 | 每次修改都触发验证（可能频繁） | 按需验证，节省计算 |
| 适用场景 | 小型构造（<20 步骤） | 大型构造（50+ 步骤） |

**决策**：采用混合策略——默认启用自动验证（小构造即时反馈），当构造步骤 > 20 时自动切换为手动触发模式（用户点击"验证"按钮）。这一阈值可在设置中调整。

### 5.2 requires 的严格程度

Dafny 的 `requires` 是强制的——如果不满足，方法不能被调用。Lv-00 中 requires 应该如何表现？

**决策**：采用"宽松前置条件"策略：
- **严格类型 requires**：输入节点的几何类型不匹配 → 阻止构造执行（如"将点作为线段端点"时点不存在则报错）
- **宽松几何 requires**：几何约束不满足 → 警告但允许继续（如"两点重合"时仍然构造中点，但标记为"退化构造"）
- 所有 requires 违规都会在 Web GUI 中以黄色警告图标显示

### 5.3 calc 链 vs 构造树展示

Dafny 的 `calc` 语句是线性的链式推导，而 Lv-00 的证明树是树形结构。如何统一？

**决策**：提供两种视图：
- **树视图**（默认）：展示完整的证明树结构，适合探索多分支
- **calc 视图**（切换）：提取从根到当前目标的一条路径，以链式格式展示，适合逐步理解

两种视图通过 Web GUI 工具栏切换。

---

## 6. 总结

Dafny 的"程序 + 规约统一体"设计为 Lv-00 提供了"构造 = method + ensures"的核心模型——每个几何构造块不仅包含可执行的构造步骤（method body），还声明了构造完成后应满足的几何命题（ensures 子句）。这种一体设计消除了构造与验证之间的分离：用户在画布上完成构造的同时，自动触发的 `construction_auto_verify()` 会立即验证所有 ensures 命题，使得"构造完成 = 验证完成"成为 Lv-00 的默认工作流。`calc` 链式推导语句为几何证明的可视化提供了可读的逐步展示格式——每步推导都带有所依赖的定理标注，在 Web GUI 中以可折叠的链式列表呈现。

| Dafny 核心概念 | Lv-00 映射组件 | 实现文件 |
|:---|:---|:---|
| method + requires + ensures | `VerifiableConstructionBlock` 结构体 | `construction.h` |
| Auto-active 自动验证 | `construction_auto_verify()` | `construction_verify.c`（新文件） |
| `calc` 链式推导 | `ProofChainStep` + `proof_chain_build()` | `proof_chain.c`（新文件） |
| `assert` 辅助断言 | `proof_minimal_verify()` 中间检查 | `proof.c` |
| `invariant` 不变式 | 迭代构造的不变量标记 | `construction.c` |
| requires/ensures 约束区分 | `ConstraintRole` 枚举 | `constraint_graph.h` |
| 验证报告 | `AutoVerifyReport` + `EnsuresVerifyEntry` | `construction_verify.c` |

---

> **文档结束**
> 本文档详述了 Dafny 的程序即规约一体化设计如何映射到 Lv-00 的几何构造验证系统。核心结论：通过将几何构造块建模为"method + requires + ensures + body"的统一体，Lv-00 实现了构造完成即自动验证的工作流——用户只需关注构造过程，命题验证自动在后台执行。`calc` 链式推导为证明过程提供了可读的逐步展示，每步标注依赖定理，使几何证明的推理链对用户透明。
