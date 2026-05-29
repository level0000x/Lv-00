# Proof General 通用证明界面借鉴设计

> **借鉴项目**：Proof General（github.com/ProofGeneral/PG）
> **核心借鉴点**：通用证明脚本模式（Generic Proof Script Mode）、锁定区域（Locked Region）机制、证明器实例化（Prover Instantiation）适配层
> **分类**：P3 中优先级 / 证明交互UX架构
> **日期**：2026-05-24

---

## 1. 概述

Proof General 是 Emacs 生态中历史最悠久的通用证明助手前端，由 David Aspinall 在爱丁堡大学 LFCS 实验室开发并持续维护超过二十年。其核心理念是通过一个**通用证明脚本模式**（Generic Proof Script Mode）提供与底层证明器无关的交互界面——用户只需学会一套 UI 操作范式，即可驱动 Coq、Isabelle、LEGO、HOL 等多种证明系统。这种"一次学习、多证明器驱动"的设计哲学对 Lv-00 的多求解器前端统一接口具有直接的借鉴价值。

Proof General 的架构围绕三个核心抽象展开：**锁定区域（Locked Region）** 将证明脚本分为已由证明器处理的"已锁定"区和尚未提交的"编辑"区，保证了证明状态的可预测性和增量性；**证明器实例化（Prover Instantiation）** 通过定义语法规则、锁定策略和目标提取正则表达式，使同一 Emacs 界面适配完全不同的证明器；**Electric Terminator** 自动插入证明步骤分隔符，消除了手动管理证明脚本结构的认知负担。

对 Lv-00 而言，Proof General 的"通用前端 + 证明器抽象层"架构精确对应着 Lv-00 Web GUI 证明面板需要同时驱动几何求解器（Groebner基/面积法）、SMT 求解器（Z3/CVC5）和数值验证器（浮点逼近）的多引擎需求。锁定区域机制为 Lv-00 证明面板提供了保证"已提交步骤不可逆"的 UI 状态管理模型，而证明器实例化的插件化思想启发了 Lv-00 求解器后端的统一 `Lv00SolverBackend` 抽象接口。

---

## 2. 核心借鉴 → Lv-00 映射

### 2.1 通用证明脚本模式（Generic Proof Script Mode）

Proof General 的核心抽象是"证明脚本"（Proof Script）——一组按顺序提交给证明器的命令序列。证明脚本被分为两个区域：

```
┌──────────────────────────────────────────┐
│  [已锁定区 Locked]    │  [编辑区 Editing]  │
│  lemma foo: ...      │  rewrite H.        │
│  intros x H.         │  apply lemma_bar.  │ ← 光标在此
│  induction x.        │                    │
│  (已被证明器处理)     │  (尚未提交)         │
└──────────────────────────────────────────┘
        ↑ 锁定指示器（Lock Indicator）
```

**关键设计原则**：
- 已锁定区域是只读的——用户不允许编辑已被证明器接受的命令
- 编辑区可以自由修改——用户在提交前可以任意编辑
- 提交操作（`proof-assert-next-command-interactive`）将编辑区的下一条命令发送给证明器，成功后将其移入锁定区
- 回退操作（`proof-undo-last-successful-command`）将锁定区的最后一条命令移回编辑区

### 2.2 Lv-00 证明面板 → 锁定区域映射

Lv-00 Web GUI 证明面板可以精确映射 Proof General 的锁定区域概念：

| Proof General 概念 | Lv-00 Web GUI 映射 | 说明 |
|:---|:---|:---|
| 锁定区域（Locked） | `proof.pending_steps` = COMMITTED 的步骤 | 已由求解器验证的证明步骤，UI 上灰显不可编辑 |
| 编辑区（Editing） | `proof.pending_steps` = DRAFT 的步骤 | 用户正在编辑的未提交步骤 |
| 锁定指示器（Lock Indicator） | GUI 中的 LockIndicator 组件（进度条上的标记） | 视觉分隔已锁定与编辑区域 |
| 提交（assert） | `proof_commit_step()` 操作 | 将当前步骤提交给后端求解器 |
| 回退（undo） | `proof_undo_step()` 操作 | 撤销上一个已提交步骤，移回编辑区 |
| 目标显示（Goals） | `display_panel.goals_window` | 显示当前证明位置的剩余目标 |
| 证明器进程（Prover Process） | `Lv00SolverBackend` 状态机 | 管理后端求解器的连接、查询和响应 |

### 2.3 证明器实例化（Prover Instanciation）适配层

Proof General 通过"证明器实例化"（Prover Instantiation）机制适配不同的后端。每个证明器实例需要定义：

```
Proof General Prover Instance 配置：
  ├─ prog-name:           求解器可执行路径
  ├─ prog-args:           启动参数
  ├─ syntax-table-entries: 语法规则（注释、字符串、关键字）
  ├─ script-command-regexp: 命令边界正则（如何识别一条命令的结束）
  ├─ goal-regexp:           目标提取正则（如何从输出中解析剩余目标）
  ├─ save-command-regexp:  保存命令正则
  ├─ locked-end:            锁定区域的视觉标记
  └─ proof-terminal-string: 命令终止符（如 Coq 的 ". " 或 Isabelle 的 ";" ）
```

**Lv-00 求解器后端抽象接口**：借鉴证明器实例化，Lv-00 定义统一的 `Lv00SolverBackend` 抽象接口，每种求解器只需实现自己的"实例化配置"：

```c
/**
 * @brief Lv-00 求解器后端抽象接口 —— 借鉴 Proof General Prover Instanciation
 *
 * 每个求解器后端（Groebner基、SMT、数值验证）实现此接口，
 * 证明面板通过统一接口驱动所有后端，无需关心底层求解器差异。
 */
typedef struct Lv00SolverBackend {
    /** 后端标识 */
    const char *backend_name;

    /** 后端能力描述 */
    const char *backend_description;

    /** 求解器启动/连接 */
    int (*connect)(void *ctx);

    /** 提交证明步骤（类型检查/约束验证） */
    Lv00SolverResult (*commit_step)(void *ctx,
                                     ProofStep *step,
                                     TypeSystem *ts);

    /** 查询当前目标状态 */
    char *(*get_goals)(void *ctx, ProofNavigator *nav);

    /** 撤销上一个已提交步骤 */
    Lv00SolverResult (*undo_step)(void *ctx, ProofNavigator *nav);

    /** 提取错误位置（对应 PG 的 error-regexp） */
    char *(*extract_error_location)(void *ctx, const char *raw_output);

    /** 求解器断开/清理 */
    void (*disconnect)(void *ctx);

    /** 后端私有数据（如子进程句柄、SMT-LIB 编码器状态等） */
    void *private_data;

    /** 后端能力标志位 */
    uint32_t capabilities;
    /* 能力标志位定义：
     *   BIT(0): 支持增量求解
     *   BIT(1): 支持反例生成
     *   BIT(2): 支持回退（undo）
     *   BIT(3): 支持中断（interrupt/timeout）
     */
} Lv00SolverBackend;

/** 求解器结果类型 */
typedef enum {
    SOLVER_RESULT_ACCEPTED,       /**< 步骤被接受，移入锁定区 */
    SOLVER_RESULT_REJECTED,       /**< 步骤被拒绝，留在编辑区 */
    SOLVER_RESULT_PENDING,        /**< 求解器仍在计算中 */
    SOLVER_RESULT_TIMEOUT,        /**< 求解超时 */
    SOLVER_RESULT_ERROR,          /**< 求解器内部错误 */
    SOLVER_RESULT_DISCONNECTED    /**< 求解器连接断开 */
} Lv00SolverResult;
```

### 2.4 后端的实例化配置示例

```c
/**
 * @brief Groebner 基求解器后端实例化
 *
 * 借鉴 PG 的 prover-instance 配置模式，
 * 每组配置封装了如何与特定后端通信的全部信息。
 */
Lv00SolverBackend groebner_backend = {
    .backend_name        = "GroebnerBasis",
    .backend_description = "基于Groebner基的符号代数求解器",
    .connect             = groebner_connect_pipe,
    .commit_step         = groebner_commit_step,
    .get_goals           = groebner_get_goals,
    .undo_step           = groebner_undo_step,
    .extract_error_location = groebner_extract_error,
    .disconnect          = groebner_disconnect,
    .capabilities        = BIT(0) | BIT(1) | BIT(2) | BIT(3),
    /* 增量求解 + 反例 + 回退 + 中断 */
};

/**
 * @brief SMT 求解器后端实例化（Z3/CVC5）
 */
Lv00SolverBackend smt_backend = {
    .backend_name        = "SMT_Z3",
    .backend_description = "基于Z3/CVC5的SMT求解器后端",
    .connect             = smt_connect_subprocess,
    .commit_step         = smt_commit_step,
    .get_goals           = smt_get_goals,
    .undo_step           = smt_undo_step,
    .extract_error_location = smt_extract_counterexample,
    .disconnect          = smt_disconnect,
    .capabilities        = BIT(0) | BIT(1) | BIT(2),
    /* 不支持中断（SMT 求解中途难以取消） */
};
```

### 2.5 锁定区域的状态机模型

```c
/**
 * @brief 证明步骤锁定状态 —— 借鉴 PG Locked Region
 *
 * 每个证明步骤在 UI 中处于以下状态之一：
 */
typedef enum {
    STEP_STATE_DRAFT,          /**< 用户编辑中（对应 PG 编辑区） */
    STEP_STATE_QUEUED,         /**< 已排队等待提交 */
    STEP_STATE_PROCESSING,     /**< 后端求解器处理中 */
    STEP_STATE_COMMITTED,      /**< 已接受并锁定（对应 PG 锁定区） */
    STEP_STATE_REJECTED,       /**< 被求解器拒绝 */
    STEP_STATE_GHOST_COMMITTED /**< Ghost 步骤已接受（需标记为 Ghost 效果） */
} ProofStepState;

/**
 * @brief 证明步骤锁定管理器
 *
 * 管理证明脚本的锁定/编辑区域边界。
 * 借鉴 PG 的 locked-end 和 proof-locked-span 概念。
 */
typedef struct {
    ProofStep *steps;             /**< 步骤数组 */
    int step_count;               /**< 总步骤数 */
    int locked_boundary;          /**< 锁定边界索引：
                                   *   steps[0..locked_boundary-1] = 锁定区
                                   *   steps[locked_boundary..]  = 编辑区 */
    int active_step_index;        /**< 当前活跃步骤（光标位置） */
    bool read_only_locked_region; /**< 锁定区是否只读 */
} ProofLockManager;
```

### 2.6 Electric Terminator 自动分隔符

Proof General 的 Electric Terminator 功能自动识别命令结束位置并插入分隔符（如 Coq 的 `.`）。在 Lv-00 中，此概念映射为**证明步骤自动分界**：

```c
/**
 * @brief 证明步骤自动分界 —— 借鉴 PG Electric Terminator
 *
 * 在用户输入几何构造/命题声明时，自动检测步骤边界并
 * 插入步骤分隔标记，无需用户手动管理步骤结构。
 *
 * 自动检测规则：
 *  1. 遇到完整的几何构造语句（POINT/SEGMENT/CIRCLE 等声明）→ 步骤边界
 *  2. 遇到 by 关键字后的完整论证 → 步骤边界
 *  3. 遇到 qed 或 done 关键字 → 步骤终止
 *  4. 遇到新定理声明 → 前一步骤的强制边界
 */
typedef enum {
    STEP_BOUNDARY_STATEMENT,     /**< 完整语句边界 */
    STEP_BOUNDARY_TACTIC,        /**< 策略调用边界（by 后） */
    STEP_BOUNDARY_TERMINAL,      /**< 终止符（qed/done） */
    STEP_BOUNDARY_NEW_THEOREM,   /**< 新定理声明强制边界 */
    STEP_BOUNDARY_MANUAL         /**< 用户手动分隔 */
} StepBoundaryType;

/**
 * @brief 分析当前光标位置的步骤边界
 *
 * @param input_buffer  当前输入缓冲区的完整文本
 * @param cursor_pos    光标在缓冲区中的位置
 * @param out_boundary  输出：检测到的边界类型
 * @param out_split_pos 输出：建议的分隔位置（字节偏移）
 * @return true 如果检测到步骤边界，false 如果没有
 */
bool step_boundary_detect(
    const char *input_buffer,
    int cursor_pos,
    StepBoundaryType *out_boundary,
    int *out_split_pos
);
```

---

## 3. 实现方案

### 3.1 第一阶段：证明面板基础设施（P3-1）

- [ ] 在 Web GUI 中设计 `ProofPanel` 组件结构
  - `LockedRegionView`（锁定区只读渲染）
  - `EditingRegionView`（编辑区可交互渲染）
  - `LockIndicator`（锁定边界视觉分隔器）
  - `GoalWindow`（当前目标显示面板）
- [ ] 实现 `ProofLockManager` 数据结构及其状态机
- [ ] 实现 `proof_commit_step()` 和 `proof_undo_step()` 操作
- [ ] 实现锁定区只读保护的 UI 逻辑
- [ ] 编写证明面板的单元测试

### 3.2 第二阶段：SolverBackend 抽象接口（P3-2）

- [ ] 定义 `Lv00SolverBackend` 抽象接口结构体
- [ ] 实现 `Lv00SolverBackend` 的生命周期管理（connect/disconnect）
- [ ] 实现后端注册表和发现机制
- [ ] 实现 Groebner 基求解器的 `Lv00SolverBackend` 实例化
- [ ] 实现 SMT 求解器的 `Lv00SolverBackend` 实例化（Z3 + CVC5）
- [ ] 实现数值验证器的 `Lv00SolverBackend` 实例化
- [ ] 编写后端注册表的单元测试

### 3.3 第三阶段：证明步骤自动分界（P3-3）

- [ ] 定义 Lv-00 几何元语言的"命令边界"正则/语法规则
- [ ] 实现 `step_boundary_detect()` 函数
- [ ] 实现 Electric Terminator 风格的自动分隔符插入
- [ ] 实现多步提交（批量提交编辑区所有步骤）
- [ ] 实现步骤的语法高亮（锁定区 vs 编辑区 不同颜色）
- [ ] 编写自动分界的单元测试

### 3.4 第四阶段：目标提取与显示（P3-4）

- [ ] 定义每个后端的目标输出格式
- [ ] 实现 `get_goals()` 的统一输出解析
- [ ] 实现 GoalWindow 组件的 Goals 树形渲染
- [ ] 实现目标 → 约束图节点的反向映射（点击目标高亮对应约束）
- [ ] 实现已锁定目标的折叠显示
- [ ] 编写目标解析的单元测试

---

## 4. 设计决策与权衡

### 4.1 锁定与并发

Proof General 假设单证明器单用户串行工作流。Lv-00 需要考虑 Web 环境下的多用户协作和并发证明。设计策略：

- **串行模式（默认）**：`ProofLockManager` 保证每次只有一个步骤处于 PROCESSING 状态——与 PG 行为一致
- **并行模式（实验性）**：允许多个独立子目标的步骤并行提交给不同后端——利用 `capabilities` 标志位检测后端是否支持
- **协作模式**：多个用户共享同一锁定区域视图，通过 CRDT（Conflict-free Replicated Data Type）合并编辑区内容

### 4.2 Web 环境下的证明器生命周期

Proof General 中证明器进程与 Emacs 进程同生命周期。Lv-00 的 Web 环境要求求解器后端支持无状态或会话恢复：

- Groebner 基求解器：天然无状态（每次提交是独立的方程组求解）
- SMT 求解器：通过 `(push)` 和 `(pop)` 管理增量求解栈
- 数值验证器：通过保存检查点支持回退

### 4.3 锁定区域粒度

Proof General 以"命令"为锁定粒度。Lv-00 中证明步骤的粒度对应：

- **粗粒度**（默认）：每个完整的几何构造 + 论证 = 一个锁定单元
- **细粒度**（可选）：每个单独的构造语句 = 一个锁定单元（相当于 Coq 的 `.` 粒度）
- **子目标粒度**：每个子目标的完整推理链 = 一个锁定单元

粒度选择通过 `ProofLockManager.lock_granularity` 配置。

---

## 5. 补充：PG 的 Span 管理与 Lv-00 的代码位置追踪

### 5.1 Span 管理（PG 的 proof-locked-span）

Proof General 使用 Emacs 的 overlay/span 机制标记锁定区域，支持：
- 锁定区的只读保护（`read-only` text property）
- 锁定区的视觉样式（不同的 face/颜色）
- 锁定/解锁事件的钩子（`proof-activate-scripting-hook`）

Lv-00 Web GUI 中的对应实现：

```c
/**
 * @brief 锁定区域的 DOM Span 管理
 *
 * 借鉴 PG 的 proof-locked-span 机制，
 * 在 Web 前端用 CSS class 标记锁定区域的 DOM 节点。
 */
typedef struct {
    /** 锁定区域的 DOM 选择器 */
    const char *dom_selector;

    /** 锁定区域的 CSS 类名 */
    const char *locked_css_class;       /* "proof-locked" — 灰显 + 不可编辑 */
    const char *processing_css_class;   /* "proof-processing" — 旋转加载指示 */
    const char *rejected_css_class;     /* "proof-rejected" — 红色下划线 */
    const char *ghost_css_class;        /* "proof-ghost" — 半透明 + 虚线边框 */

    /** 锁定事件回调 */
    void (*on_lock)(int step_index);
    void (*on_unlock)(int step_index);
    void (*on_reject)(int step_index, const char *error_message);

    /** 只读保护的 contentEditable 控制 */
    bool content_editable_locked;       /**< 锁定区 contentEditable = false */
} LockedSpanManager;
```

### 5.2 目标提取正则（Goal Regexp）的 Lv-00 适配

Proof General 使用 `proof-shell-goal-regexp` 从证明器输出中提取当前目标。Lv-00 中每个后端需要定义自己的目标解析器：

```c
/**
 * @brief 目标提取器（Goal Extractor）—— 借鉴 PG goal-regexp
 *
 * 每种求解器后端定义自己的目标输出格式和解析规则。
 */

/** SMT 后端：从 SMT-LIB 输出中提取未满足的约束 */
typedef struct {
    /** 约束编号的正则模式 */
    const char *constraint_id_pattern;     /* "constraint_(\d+)" */
    /** 约束描述的正则模式 */
    const char *constraint_desc_pattern;   /* "goal:\s*(.+)" */
    /** 子目标层级缩进检测 */
    int indent_level_multiplier;           /* 2 spaces per level */
} GoalExtractorSMT;

/** Groebner 后端：从多项式方程组中提取未消解的变量 */
typedef struct {
    /** 剩余变量的声明格式 */
    const char *remaining_var_pattern;     /* "unresolved:\s*([a-z]+)" */
    /** 生成多项式的格式 */
    const char *groebner_poly_pattern;     /* "^\[\d+\]:\s*(.+)" */
} GoalExtractorGroebner;
```

---

## 6. 参考资源

- Proof General 官方仓库：https://github.com/ProofGeneral/PG
- Proof General 用户手册（Generic Proof Script Mode）：https://proofgeneral.github.io/doc/master/userman/
- David Aspinall, "Proof General: A Generic Tool for Proof Development", TACAS 2000
- PG 架构文档中关于 Prover Instanciation 的章节：`generic/proof-site.el` 中的 `proof-assistant-table` 定义
- 与 Lv-00 相关的已有借鉴文档：
  - `isabelle_sledgehammer_integration.md` —— 证明面板与外部求解器集成参考
  - `why3_multi_prover_dispatch.md` —— 多求解器调度策略参考
  - `agda_hole_driven_proof.md` —— 交互式证明编辑 UX 参考
  - `coq_ltac_proof_engine.md` —— 证明策略语言的语法边界检测参考
