# Isabelle/HOL Isar 结构化证明与 Sledgehammer 集成借鉴设计

> **借鉴项目**：Isabelle/HOL（isabelle.in.tum.de）
> **核心借鉴点**：Isar 结构化证明语言、Sledgehammer 自动证明调度、Locales 模块化理论架构
> **分类**：P1 高优先级 / 证明输出格式与自动证明调度
> **日期**：2026-05-24

---

## 1. 概述

Isabelle/HOL 是由 Lawrence Paulson 和 Tobias Nipkow 等开发的通用证明助手，基于高阶逻辑（HOL）。它不属于依赖类型体系，而是构建在简单类型论之上的经典逻辑系统。Isabelle 的三个核心特性对 Lv-00 有着关键的架构借鉴价值：

1. **Isar 结构化证明语言**：Isar（Intelligible Semi-Automated Reasoning）提供类自然语言的证明书写风格，按 `proof - ... qed` 嵌套块组织，每一步明确标注推理规则。这种可读性至上的输出格式，正是 Lv-00 的 `proof_export_natural_language` 和 `proof_export_latex` 所追求的目标。

2. **Sledgehammer 自动证明调度**：Sledgehammer 将当前证明目标发送给多个外部 ATP（自动定理证明器），包括 E、Vampire、Z3、CVC4、SPASS 等，收集它们的证明并重建为 Isar 结构化证明。这种"多求解器并行 + 结果重建"的架构直接对应 Lv-00 的 `ProofMultiStrategy` 框架。

3. **Locales 模块化理论**：Locales 机制允许定义可参数化的局部理论上下文，支持继承、合并、解释。这与 Lv-00 的公理包（Axiom Package）设计形成天然对应——公理包本质上是带参数的约束集，而 Locales 提供了其形式化基础。

---

## 2. Isar 结构化证明语言映射到 proof.c 输出格式

### 2.1 Isar 的核心证明结构

一个典型的 Isar 证明结构：

```
lemma example:
  assumes "A" and "B"
  shows "C"
proof -
  from `A` have "D" by (rule rule1)
  moreover from `B` and `D` have "E" by (rule rule2)
  ultimately show "C" by (rule rule3)
qed
```

Isar 的结构要素：
- `lemma ... assumes ... shows`：引理声明
- `proof - ... qed`：证明块（可嵌套）
- `from ... have ... by`：推理步骤
- `moreover ... ultimately`：累积推理
- `show`：子目标展示

### 2.2 Lv-00 proof.c 中的 Isar 映射

在 Lv-00 的 `proof.c` 中，证明步骤的链式结构已经存在（`ProofNavigator` 的 `steps` 数组）。Isar 格式本质上是将这些步骤组织为可读的块状嵌套文本：

| Isar 结构 | Lv-00 proof.c 映射 | 输出格式 |
|:---|:---|:---|
| `lemma ... assumes ... shows` | `Proposition.input_port_ids` + `Proposition.output_port_ids` | 引理声明头 |
| `proof -` | `ProofNavigator.strategy_note` | 策略概览 |
| `from ... have ... by (rule ...)` | 单个 `ProofStep` 转换为自然语言 | 推理步骤描述 |
| `moreover ... ultimately` | 连续的多个步骤（无分支） | 累积推理段落 |
| `show` | 步骤标记为 `is_completed` + 对应输出端口 | 子目标达成标记 |
| `qed` | `ProofNavigator.is_complete == true` | 证明结束标记 |
| 嵌套 `proof ... qed` | 子 `ProofNavigator`（引理内引用） | 嵌套证明块 |

### 2.3 proof.c 中增强的 Isar 导出函数

```c
/**
 * @brief 将证明导出为 Isar 风格的结构化文本
 *
 * 借鉴 Isabelle 的 Isar 语言，将约束图证明步骤转换为
 * 嵌套块状结构化证明。输出包含：
 *  - lemma 声明头（assumes + shows）
 *  - 策略注释（借鉴 LeanGeo 风格）
 *  - 推理步骤链（每一步标注推理规则/几何构造/约束）
 *  - 嵌套子证明块（lemma 内引用）
 *  - 辅助构造解释（为什么添加这条辅助线）
 *
 * @param nav       证明导航器
 * @param filepath   输出文件路径
 * @param lang       输出语言
 * @param with_sledgehammer_annotations 是否附加 Sledgehammer 策略标注
 * @return 是否成功
 *
 * @note 输出格式示例（英文）：
 *   lemma triangle_congruence:
 *     assumes "AB = DE" and "AC = DF" and "∠BAC = ∠EDF"
 *     shows   "△ABC ≅ △DEF"
 *   proof -
 *     -- strategy: apply SAS congruence criterion
 *     from `AB = DE` and `AC = DF` and `∠BAC = ∠EDF`
 *     have "△ABC ≅ △DEF" by (rule SAS_congruence)
 *   qed
 *
 * @see isabelle_sledgehammer_integration.md
 */
bool proof_export_isar(
    ProofNavigator *nav,
    const char *filepath,
    ProofNaturalLanguage lang,
    bool with_sledgehammer_annotations
);
```

### 2.4 Isar 结构化级别

```c
/**
 * @brief Isar 导出结构化级别
 */
typedef enum {
    ISAR_STRUCTURE_FLAT,       /**< 扁平结构：所有步骤线性排列（与现有 natural_language 类似） */
    ISAR_STRUCTURE_NESTED,     /**< 嵌套结构：子证明块嵌套显示（与 Isabelle 一致） */
    ISAR_STRUCTURE_STRATEGIC   /**< 策略结构：先展示策略再展开细节（LeanneGeo 风格） */
} IsarStructureLevel;
```

---

## 3. Sledgehammer 自动证明调度集成

### 3.1 Sledgehammer 的调度架构

Sledgehammer 的工作流程：

```
1. 用户声明目标:    lemma P
2. Sledgehammer:
   a. 将目标 + 上下文转换为 TPTP（标准格式）
   b. 并行发送给 E、Vampire、Z3、CVC4、SPASS
   c. 各求解器尝试证明（时间限制 30s）
   d. 收集成功的证明（通常为 resolution/superposition 推导）
   e. 将 ATP 证明重建为 Isar 结构化证明
   f. 将重建的证明插入用户源文件
```

### 3.2 Lv-00 的对应：proof_multi_strategy 框架

Lv-00 已有的 `ProofMultiStrategy` 框架（`proof.h` 第 875-1099 行）已经实现了多策略引擎的基础设施。Sledgehammer 的调度模式可以精确映射：

| Sledgehammer 概念 | Lv-00 ProofMultiStrategy 映射 | 说明 |
|:---|:---|:---|
| ATP 求解器 | `ProofStrategyDescriptor` （如 PROOF_STRATEGY_GROEBNER_BASIS） | 每种策略是一个独立求解器 |
| TPTP 转换 | 约束图 → 策略特定格式的序列化 | 将几何约束转换为代数方程/面积关系等 |
| 并行求解 | `proof_multi_strategy_try_all()` 的竞争模式 | 多策略并行/串行尝试 |
| 时间限制 | 策略的 `timing_ms` 统计 | 每种策略有独立的时间预算 |
| 证明重建 | 策略输出 → 约束图步骤 | 将外部求解器结果翻译回 Lv-00 步骤 |
| 结果嵌入 | `proof_navigator_add_step()` | 插入证明导航器 |

### 3.3 proof_sledgehammer_dispatch() API

```c
/**
 * @brief Sledgehammer 风格多求解器自动证明调度
 *
 * 借鉴 Isabelle Sledgehammer 的多求解器并行调度架构，
 * 将当前证明目标同时（或按优先级）发送给多个证明策略，
 * 收集各策略的尝试结果，选择最优解并重建为 Lv-00 证明步骤。
 *
 * 调度流程（对应 Sledgehammer 的核心循环）：
 *  1. 提取当前目标（从 ProofNavigator 的 target_prop）
 *  2. 将目标转换为各策略的内部格式
 *  3. 并行启动所有可用策略（受时间预算限制）
 *  4. 收集结果：成功/失败/超时
 *  5. 对成功结果按质量排序（简洁性 + 信任颜色）
 *  6. 将最优结果重建为 Lv-00 证明步骤
 *  7. 将重建的步骤插入导航器
 *
 * 调度模式：
 *  - SLEDGEHAMMER_RACE：并行启动所有策略，取最先成功的（速度优先）
 *  - SLEDGEHAMMER_QUALITY：并行启动所有策略，等全部完成取最优（质量优先）
 *  - SLEDGEHAMMER_PIPELINE：按指定顺序串行执行（可控性优先）
 *
 * @param nav              证明导航器（包含目标和上下文）
 * @param mse              多策略引擎（包含已注册的策略）
 * @param mode              调度模式
 * @param time_budget_ms   总时间预算（毫秒），0 = 不限制
 * @param out_report       输出：调度报告（调用者需用 lv00_free 释放）
 * @return 成功的策略类型，失败返回 PROOF_STRATEGY_COUNT
 *
 * @note 借鉴 Sledgehammer 的经验法则：
 *        - 简单目标（30s 预算足够）优先用 RACE 模式
 *        - 复杂目标用 QUALITY 模式，让所有求解器充分尝试
 *        - 已知某策略不适用时用 PIPELINE 跳过
 *
 * @see proof_multi_strategy_try_all() —— 底层竞争模式实现
 * @see proof_multi_strategy_pipeline() —— 底层流水线模式实现
 */
ProofStrategyType proof_sledgehammer_dispatch(
    ProofNavigator *nav,
    ProofMultiStrategy *mse,
    SledgehammerMode mode,
    int64_t time_budget_ms,
    char **out_report
);
```

### 3.4 Sledgehammer 调度模式与调度报告

```c
/**
 * @brief Sledgehammer 调度模式
 */
typedef enum {
    SLEDGEHAMMER_RACE,      /**< 竞速模式：并行启动，取最先成功的 */
    SLEDGEHAMMER_QUALITY,   /**< 质量模式：等全部完成取最优 */
    SLEDGEHAMMER_PIPELINE   /**< 流水线模式：按优先级顺序串行 */
} SledgehammerMode;

/**
 * @brief 单策略调度结果
 */
typedef struct {
    ProofStrategyType strategy;  /**< 策略类型 */
    bool success;                /**< 是否成功 */
    int64_t elapsed_ms;          /**< 耗时（毫秒） */
    int steps_generated;         /**< 生成的步骤数 */
    ProofColor best_color;       /**< 最佳信任颜色 */
    char *error_message;         /**< 错误信息（失败时） */
} SledgehammerStrategyResult;

/**
 * @brief Sledgehammer 调度完整报告
 *
 * 包含每个策略的详细调度结果，格式化的可读报告，
 * 以及推荐的最优解。
 */
typedef struct {
    SledgehammerStrategyResult *strategy_results;  /**< 各策略结果数组 */
    int strategy_count;                             /**< 策略数量 */
    ProofStrategyType winner;                       /**< 获胜策略（PROOF_STRATEGY_COUNT = 无胜者） */
    char *formatted_report;                         /**< 格式化的可读报告 */
    int total_attempts;                             /**< 总尝试次数 */
    int64_t total_elapsed_ms;                       /**< 总耗时 */
} SledgehammerReport;

/**
 * @brief 释放 Sledgehammer 调度报告
 */
void sledgehammer_report_destroy(SledgehammerReport *report);
```

---

## 4. Locales 模块化理论映射到公理包设计

### 4.1 Isabelle Locales 的核心机制

Locale 是一种带参数的命名理论上下文：

```
locale semigroup =
  fixes f :: "'a ⇒ 'a ⇒ 'a"   -- 固定参数
  assumes assoc: "f (f x y) z = f x (f y z)"  -- 公理

locale monoid = semigroup +
  fixes neutral :: "'a"        -- 额外参数
  assumes left_neutral: "f neutral x = x"
      and right_neutral: "f x neutral = x"
```

Locale 的操作：
- **定义（definition）**：创建新的 locale
- **继承（inheritance）**：`locale B = A + ...`（B 继承 A 的所有参数和公理）
- **合并（merge）**：`locale C = A + B`（C 同时拥有 A 和 B 的参数和公理）
- **解释（interpretation）**：将 locale 应用于具体实例

### 4.2 Locale → Lv-00 公理包映射

| Locale 概念 | Lv-00 公理包映射 | 说明 |
|:---|:---|:---|
| `locale` 声明 | `axiom_package` 定义（名称 + 描述） | 命名理论上下文 |
| `fixes` 参数 | 公理包的参数端口 | 可实例化的参数 |
| `assumes` 公理 | 参数节点上的约束集 | locale 的不变式 |
| `+` 继承 | 公理包的 `extends` 关系 | 子公理包继承父公理包的所有约束 |
| 合并 | 公理包的 `merge` 操作 | 多个公理包的约束联合 |
| `interpretation` | 公理包应用到具体几何配置 | 实例化参数的约束绑定 |
| 子 locale | 子公理包 | 特化的几何理论（如"欧几里得平面几何" ⊆ "仿射几何"） |

### 4.3 Lv-00 公理包 Locale 风格的 DSL

参照 Isabelle Locale 语法，Lv-00 公理包的声明可以采用如下 DSL：

```
axiom_package "欧几里得平面几何" {
    fixes:
        point A, B, C          -- 参数：三个自由点
    assumes:
        CONSTRAINT(∠ABC, LESS_THAN, π)   -- ABC 在欧氏平面内
        CONSTRAINT(A, NOT_ON, LINE(B, C)) -- 三点不共线
}

axiom_package "尺规构造" extends "欧几里得平面几何" {
    fixes:
        construction_methods: [COMPASS, STRAIGHTEDGE]
    assumes:
        CONSTRAINT(all_constructed_points, CONSTRUCTIBLE_BY, {compass, straightedge})
}

axiom_package "仿射几何" merge "欧几里得平面几何" {
    -- 继承欧氏平面几何的所有公理
    -- 额外添加仿射变换公理
    assumes:
        CONSTRAINT(parallel_lines, PRESERVED_BY, affine_transformation)
}
```

### 4.4 Locale 解释与公理包实例化

```
interpretation 欧几里得平面几何 for A, B, C
    where A = O, B = P, C = Q
    -- 将抽象点 A, B, C 绑定到具体几何节点 O, P, Q
    -- 系统自动检查：O.P.Q 是否满足 Locale 的所有 assumes
```

在 Lv-00 中对应：

```c
/**
 * @brief 实例化公理包（对应 Locale interpretation）
 *
 * 将公理包的抽象参数绑定到具体约束图中的节点，
 * 并在绑定后验证所有 assumes 条件。
 *
 * @param nav           证明导航器（含引擎中的公理包引用）
 * @param package_name  公理包名称
 * @param param_bindings 参数绑定数组 [abstract_param_id, concrete_node_id, ...]
 * @param binding_count  绑定对数量
 * @return 是否实例化成功（所有 assumes 通过验证）
 */
bool axiom_package_instantiate(
    ProofNavigator *nav,
    const char *package_name,
    const int *param_bindings,
    int binding_count
);
```

---

## 5. 实现路线图

### 5.1 第一阶段：Isar 导出增强（P1-1）

- [ ] 实现 `proof_export_isar()` 核心格式化逻辑
- [ ] 实现嵌套证明块的检测与渲染
- [ ] 实现策略注释的自动生成与嵌入
- [ ] 实现 Isar 结构级别的选择（FLAT/NESTED/STRATEGIC）
- [ ] 编写 Isar 导出与现有 natural_language 导出的对比测试

### 5.2 第二阶段：Sledgehammer 调度核心（P1-2）

- [ ] 实现 `proof_sledgehammer_dispatch()` 核心调度逻辑
- [ ] 实现 RACE/QUALITY/PIPELINE 三种调度模式
- [ ] 实现目标到各策略内部格式的转换
- [ ] 实现结果重建逻辑（ATP 步骤 → 约束图步骤）
- [ ] 实现调度报告的格式化生成
- [ ] 实现时间预算管理和超时处理

### 5.3 第三阶段：Locale 风格公理包系统（P1-3）

- [ ] 设计公理包 DSL 的 BNF 语法
- [ ] 实现公理包解析器
- [ ] 实现公理包继承（extends）机制
- [ ] 实现公理包合并（merge）机制
- [ ] 实现公理包解释（interpretation/实例化）机制
- [ ] 实现公理包间的约束一致性检查

### 5.4 第四阶段：集成与优化（P1-4）

- [ ] 将 Sledgehammer 调度集成到 ProofPanel 的"自动证明"按钮
- [ ] 实现调度进度可视化（各策略的实时状态）
- [ ] 实现 Isar 证明的 Web 端预览
- [ ] 性能优化：策略结果的增量更新
- [ ] 编写端到端集成测试

---

## 6. 设计决策与权衡

### 6.1 Isar 导出 vs Coq 导出

Lv-00 同时支持多种证明导出格式。Isar 和 Coq 的区别：

| 方面 | Isar 导出 | Coq 导出 |
|:---|:---|:---|
| 目标读者 | 人类（数学论文） | 机器（Coq 类型检查器） |
| 可读性 | 极高（类自然语言） | 中等（需了解 Coq 语法） |
| 嵌套结构 | 原生支持 | 通过 `{ }` 块支持 |
| 推理规则标注 | 显式 `by (rule ...)` | 隐式（tactic 调用） |
| 策略注释 | 自然段落 | 注释形式 |

### 6.2 Sledgehammer 的时间预算策略

与 Isabelle Sledgehammer 类似，Lv-00 的调度器采用自适应时间预算：
- 第一轮：每个策略分配 5s，RACE 模式
- 若未成功：增加预算到 15s，QUALITY 模式
- 若仍未成功：报告所有策略的部分结果和失败原因

### 6.3 Locale vs 直接公理包

Locale 机制引入额外的抽象层，好处是：
- 公理包间的关系显式化（继承、合并）
- 参数化复用（同一 locale 可多次解释到不同配置）
- 一致性检查（interpretation 时自动验证）

代价是增加实现复杂度。建议从简单的继承/合并开始，逐步增加完整的 Locale 能力。

---

## 7. 总结

Isabelle/HOL 的三大机制——Isar 结构化证明语言、Sledgehammer 自动证明调度、Locales 模块化理论——分别为 Lv-00 提供了证明输出格式、自动证明调度策略和公理包模块化设计的明确参照。Isar 使 Lv-00 的证明输出从简单的步骤列表升级为人类可读的结构化证明；Sledgehammer 的并行调度模式完美适配已有的 ProofMultiStrategy 框架；Locales 为公理包系统提供了参数化、继承、合并、解释的完整概念模型。这三个特性的集成将使 Lv-00 的证明工作流从"手动逐步构造"跃升为"多策略自动搜索 + 结构化可读输出"的成熟范式。
