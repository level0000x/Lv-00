# miniF2F + lean-gym 神经符号定理证明核心借鉴设计

> **借鉴项目**：miniF2F + lean-gym（github.com/facebookresearch/miniF2F + github.com/openai/lean-gym）
> **核心借鉴点**：跨形式系统统一基准（Lean/Isabelle/Metamath/HOL Light 四系统编码同一问题）、REPL 式证明状态协议、GPT-f + HyperTree Proof Search 神经符号架构、证明验证严格性
> **分类**：P2 高优先级 / 神经符号定理证明基础设施
> **日期**：2026-05-24

---

## 1. 概述

miniF2F（Mini Formal-to-Formal）是由 OpenAI 和 Meta（Facebook AI Research）联合推出的跨形式系统数学定理基准测试集。其核心创新在于将 **488 个数学竞赛问题（来自 IMO、AMC、AIME 等）同时在四个不同形式系统中编码**——Lean 3、Isabelle/HOL、Metamath 和 HOL Light。同一问题的四种形式化版本使研究者能够在均匀的测试条件下比较不同证明助手的自动化定理证明（ATP）能力。对 Lv-00 而言，miniF2F 的跨系统编码策略直接启发了几何基准测试的多后端输出设计：同一几何问题可同时输出为 Lv-00 内部格式、Lean 4 代码、Coq 代码和 GeoGebra 构造，最大化复用性与可比较性。

lean-gym 是 OpenAI 为 Lean 3 证明助手开发的 REPL（Read-Eval-Print Loop）式交互协议。它将 Lean 的证明状态暴露为结构化 JSON 消息，使外部 AI 代理可以读取当前证明目标、提交策略（tactic）并接收更新后的证明状态。这种"证明助手作为服务器，AI 作为客户端"的架构是神经符号定理证明的核心基础设施。Lv-00 可以借鉴 lean-gym 的协议设计，将其证明引擎暴露为结构化的 REPL API，使 LLM 能够以 tactic-by-tactic 的方式与 Lv-00 的约束图证明引擎交互。

GPT-f 是 OpenAI 在 Lean 中实现的基于 Transformer 的证明策略生成器。其工作流程为：给定当前证明状态（tactic_state），GPT-f 生成候选策略，Lean 执行策略并返回新状态，成功则继续，失败则回溯尝试下一个候选。配合 **HyperTree Proof Search（HTPS）**——一种在证明树中维护多个搜索分支的在线搜索算法，根据模型置信度和搜索启发式动态分配计算资源——构成了"LLM 生成策略→树搜索探索→证明助手验证"的神经符号闭环。这对 Lv-00 引入 AI 辅助几何证明具有直接启发意义。

"草稿→草图→证明"（Draft, Sketch, and Prove, DSP）流水线是神经符号定理证明的最新范式：LLM 首先生成非形式化的"草稿"（自然语言证明思路），再将草稿细化为包含关键引理和证明步骤的"草图"，最后由形式化证明助手将草图填充为完整的形式化证明。DSP 与几何定理证明高度契合——几何证明天然具有"直觉→构造→形式化"的层次性。

---

## 2. 核心借鉴 → Lv-00 映射

### 2.1 跨形式系统统一基准

miniF2F 的核心设计是同一数学问题被手动翻译为四种形式系统的代码。以 IMO 1959 问题 1 为例：

```
Lean 3:    theorem imo_1959_p1 : ∀ n : ℕ, ¬ (Nat.gcd (21*n+4) (14*n+3) = 1) := ...
Isabelle:  theorem imo_1959_p1: fixes n :: nat shows "gcd (21*n+4) (14*n+3) = 1" ...
Metamath:  ${ ... $e |- gcd ( (21*N)+4 ) ( (14*N)+3 ) = 1 $. $}
HOL Light: let IMO_1959_P1 = prove (`!n. gcd(21*n+4,14*n+3) = 1`, ...)
```

四种编码共享统一的"语义标签"（`IMO_1959_p1`），使评估可在统一框架下进行。

**Lv-00 借鉴**：几何基准的多后端编码策略。

| miniF2F 特性 | Lv-00 几何基准映射 | 说明 |
|:---|:---|:---|
| 多形式系统编码同一问题 | 几何命题的 Lv-00/Lean/Coq/GeoGebra 四输出 | 一次定义，多后端验证 |
| 统一的语义标签 | 几何命题的 UUID + 语义标签（如 `IMO_2024_G4`） | 跨后端追踪 |
| 自动评估管道 | 对每个后端运行 ATP + 人工验证结果提取 | 评估矩阵生成 |
| 难度分级 | 几何命题的难度标签（AMC→AIME→IMO） | 渐进式基准 |

### 2.2 lean-gym REPL 协议

lean-gym 定义了一套基于 JSON 的 REPL 协议，将证明交互分解为核心消息循环：

```
客户端 → 服务端：
  init_search { decl_name, search_id }  // 初始化证明搜索
  run_tac { search_id, tactic, tactic_state_id }  // 执行策略

服务端 → 客户端：
  tactic_state { search_id, tactic_state_id, tactic_state, goals[] }  // 状态更新
  proof_finished { search_id, proof, total_steps }  // 证明完成
```

**Lv-00 借鉴**：将证明引擎暴露为 lean-gym 风格的 REPL API。

| lean-gym 协议概念 | Lv-00 REPL 映射 | 说明 |
|:---|:---|:---|
| `search_id` | `search_id`（UUID） | 证明搜索会话标识 |
| `tactic_state` | `goal_state`（当前证明目标 + 假设列表） | 几何约束图的目标投影 |
| `tactic_state_id` | `goal_state_id`（状态哈希） | 增量状态追踪 |
| `run_tac` | `apply_step(proof_step)` | 在约束图上施加一个证明步骤 |
| `goals` | `pending_goals`（未解决的几何子目标） | 多目标管理 |
| `proof_finished` | `proof_complete`（完整证明轨线 + 证书） | 证明序列化 |

### 2.3 GPT-f + HyperTree Proof Search 神经符号架构

GPT-f + HTPS 是神经符号定理证明的经典架构。其完整工作流程为：

```
[初始证明状态 (goal)]
  ↓
[GPT-f 模型]
  ├─ 输入：当前目标 + 上下文 + 历史步骤
  ├─ 输出：N 个候选策略（tactic），每个附置信度
  └─ 策略排名：P(tactic | goal) * beam_score
  ↓
[HTPS 搜索树]
  ├─ 为每个候选策略创建子节点
  ├─ 调用 Lean 验证每个策略 → 成功：新节点继续搜索；失败：关闭并回溯
  └─ 按 UCB/置信度分配搜索预算
  ↓
[Lean 验证] → 无目标残留且类型检查通过 → 证明完成
```

| HTPS 组件 | 功能 | Lv-00 映射 |
|:---|:---|:---|
| 策略生成器（GPT-f） | LLM 生成候选策略 | `lv00_llm_tactic_generator()` |
| 搜索树（Proof Tree） | 维护多分支搜索状态 | `lv00_proof_search_tree` |
| 节点评分 | 模型置信度 + 搜索深度 + 目标复杂度 | 构造步数 + 约束数复合评分 |
| 搜索预算分配 | UCB 或 PUCT 算法 | MCTS 风格预算分配 |
| 验证器（Lean） | 执行策略并返回新状态 | `proof_unify` + `solver.h` |

### 2.4 证明验证严格性

miniF2F 和 lean-gym 中，证明验证的严格性由四个条件保证：

1. **无目标残留（no goals）**：`tactic_state.goals = []` ——所有子目标都被证明
2. **类型匹配**：证明项的类型与声明的定理类型精确匹配
3. **无 `sorry`**：证明中不得出现占位符
4. **无 `undefined`**：所有引用的定义和定理必须已定义且可访问

对 Lv-00 而言，四项严格性条件转化为证明检查器的四个断言：

```c
typedef enum {
    PROOF_CHECK_NO_PENDING_GOALS     = 1 << 0,  /**< 无未解决的子目标 */
    PROOF_CHECK_TYPE_MATCH           = 1 << 1,  /**< 结论类型与声明匹配 */
    PROOF_CHECK_NO_SORRY             = 1 << 2,  /**< 无占位符 */
    PROOF_CHECK_NO_UNDEFINED         = 1 << 3,  /**< 所有依赖已解析 */
} ProofCheckStrictness;

#define PROOF_CHECK_STRICT_ALL \
    (PROOF_CHECK_NO_PENDING_GOALS | PROOF_CHECK_TYPE_MATCH | \
     PROOF_CHECK_NO_SORRY | PROOF_CHECK_NO_UNDEFINED)
```

### 2.5 对照表：miniF2F 基准格式 → Lv-00 几何测试用例格式

| miniF2F 格式元素 | Lv-00 几何测试用例映射 | 示例 |
|:---|:---|:---|
| `decl_name` | 几何定理标识符 | `IMO_2024_G4` |
| `formal_statement` | 几何命题的形式化声明 | `triangle(A,B,C) → area(ABC) > 0` |
| `informal_statement` | 自然语言命题描述 | "证明三角形的面积为正" |
| `source` | 题目来源 | `IMO 2024 Problem 4` |
| `split`（train/valid/test） | 训练/验证/测试集划分 | 对应 Lv-00 难度分级 |
| `proof`（正式证明） | 约束图证明轨线 | 证明步骤序列 |
| `auto_level` | 自动化难度评级 | 约束数 + 搜索深度的复合评级 |

### 2.6 Lv-00 REPL 协议核心 API

将 Lv-00 的约束图证明引擎暴露为结构化 JSON REPL 接口，使 LLM 可以 tactic-by-tactic 方式与 Lv-00 交互进行几何定理证明。

```c
/**
 * @brief Lv-00 证明 REPL 协议 —— 借鉴 OpenAI lean-gym 设计
 */

/* === 消息类型 === */
typedef enum {
    REPL_MSG_INIT_SEARCH,       /**< 初始化搜索：theorem_name + formal_statement */
    REPL_MSG_RUN_TAC,           /**< 执行证明步骤：search_id + tactic + tactic_state_id */
    REPL_MSG_TACTIC_STATE,      /**< 证明状态更新：goals[] + 假设列表 */
    REPL_MSG_PROOF_FINISHED,    /**< 证明完成：proof_script + certificate */
    REPL_MSG_ERROR,             /**< 错误：错误码 + 消息 */
} REPLMessageType;

/* === 核心 API === */

/** 创建 REPL 服务端 */
void *lv00_repl_server_create(TypeSystem *ts, ConstraintSolver *solver, int port);

/** 启动 REPL 主循环（阻塞，建议在独立线程中运行） */
int lv00_repl_server_run(void *server);

/** 处理 init_search 请求：解析声明→构建约束图→创建搜索树→返回初始状态 */
int lv00_repl_handle_init_search(void *server, const REPLInitSearch *req,
                                  REPLTacticState *out_state);

/** 处理 run_tac 请求：验证状态→解析tactic→施加步骤→精化检查→返回新状态 */
int lv00_repl_handle_run_tac(void *server, const REPLRunTactic *req,
                              REPLTacticState *out_state);

/** 将证明状态编码为 JSON（供 LLM 客户端解析） */
char *lv00_repl_encode_tactic_state_json(const REPLTacticState *state);
```

**LLM 与 Lv-00 REPL 的交互流程示例**：

```
[客户端] init_search { theorem: "IMO_2024_G4",
  statement: "triangle(A,B,C) -> on_circle(I, incircle(A,B,C))" }

[服务端] tactic_state {
  goals: ["点 I 在内心内切圆 incircle(A,B,C) 上"],
  hyps: ["triangle(A,B,C)", "incenter(I,A,B,C)"] }

[客户端] run_tac { tactic: "construct_perp(I, AB) -> D, similarly E, F" }

[服务端] tactic_state {
  goals: ["ID ⟂ AB", "IE ⟂ BC", "IF ⟂ CA", "|ID| = |IE| = |IF|"],
  hyps: [D,E,F tangent] }

... (LLM 逐步生成策略，Lv-00 逐步验证) ...

[服务端] proof_finished { total_steps: 12, proof_time: 2.45s,
  certificate: "<proof_certificate>" }
```

### 2.7 "草稿→草图→证明"（DSP）流水线在几何定理中的应用

DSP 流水线连接 LLM 非形式化推理与形式化验证。在几何定理证明中，三阶段具有天然对应：

```
[Draft] ─ LLM 生成自然语言证明思路
  输入：几何命题的自然语言描述
  输出："要证明内心在内切圆上，考虑从内心向三边做垂线。
         由内心性质，这三条垂线等长......"

[Sketch] ─ LLM 将草稿细化为结构化步骤
  输入：自然语言草稿 + 形式化声明
  输出：Step 1: construct_perp(I, AB, BC, CA) → D, E, F
        Step 2: prove |ID| = |IE| = |IF| [by incenter_property]
        Step 3: construct incircle from (I, r=|ID|) [by definition]

[Prove] ─ Lv-00 将草图填充为完整形式证明
  输入：结构化草图
  输出：约束图证明轨线 + 证明证书
  过程：每个 Sketch 步骤通过 lv00_proof_step_apply() 在约束图上展开，
        子目标自动分解，失败时反馈给 LLM 请求修正
```

**DSP 在 Lv-00 中的核心数据结构**：

```c
typedef struct {
    char *draft_text;                            /**< LLM 生成的草稿文本 */
    struct {
        char *step_description;
        GeoConstructKind construct;              /**< 对应几何构造类型 */
        int *source_ids; int source_count;
        char *expected_lemma;                    /**< 期望依赖的引理 */
    } *sketch_steps;
    int sketch_step_count;
    int *resolved_goal_ids; int resolved_count;  /**< 已解决的子目标 */
    bool proof_complete;
    char *certificate;
} GeometryDSPPipeline;
```

---

## 3. 实现方案

### 3.1 第一阶段：几何基准测试与多后端编码（P2-1）

- [ ] 整理 50+ 个经典几何定理作为基准测试集
  - 按难度分级：AMC 级（30 题）→ AIME 级（15 题）→ IMO 级（5 题）
  - 覆盖构造类别：三角形、圆、四边形、圆锥曲线、变换
- [ ] 为每个问题设计统一的元数据格式（`theorem_name`/`informal`/`formal`/`difficulty`/`approach`）
- [ ] 实现 Lv-00 → Lean 4 代码的导出器
- [ ] 实现 Lv-00 → Coq 代码的导出器
- [ ] 实现 Lv-00 → GeoGebra 构造的导出器
- [ ] 构建自动化评估管道：对每个后端运行 ATP，收集结果矩阵

### 3.2 第二阶段：REPL 证明协议实现（P2-2）

- [ ] 设计 Lv-00 REPL 协议的完整 JSON Schema
- [ ] 实现 `lv00_repl_server_create()` 和 `lv00_repl_server_run()`
- [ ] 实现 `lv00_repl_handle_init_search()`：解析声明→构建约束图→创建搜索树根节点→返回 `tactic_state`
- [ ] 实现 `lv00_repl_handle_run_tac()`：验证状态→解析 tactic→在约束图上施加步骤→更新搜索树→返回新状态
- [ ] 实现增量状态追踪（`tactic_state_id` 哈希生成与校验）
- [ ] 实现证明完成检测和 `proof_finished` 消息生成
- [ ] 编写 10+ 个 REPL 交互的场景测试

### 3.3 第三阶段：LLM 策略生成器集成（P2-3）

- [ ] 实现 `lv00_llm_tactic_generator()` 接口（输入 `REPLTacticState`，输出候选策略列表）
- [ ] 支持多个 LLM 后端（OpenAI API、本地模型、HuggingFace 推理端点）
- [ ] 实现 prompt 工程模板：几何背景知识注入 + 状态格式化 + Few-shot 示例选择
- [ ] 实现策略解析器（LLM 输出 → 结构化 GeoProofStep）和策略评分（置信度 + 启发式 + 历史成功率）

### 3.4 第四阶段：HyperTree Proof Search（P2-4）

- [ ] 实现 `lv00_proof_search_tree`：节点（状态 + 父 + 已尝试策略 + 子列表），支持序列化
- [ ] 实现 HTPS 核心：节点扩展（LLM 生成 N 个候选）→ 评估（Lv-00 验证每个）→ 预算分配（UCB/PUCT）→ 回溯
- [ ] 实现搜索启发式：约束数减少度量 + 目标复杂度估算 + 策略成功率历史
- [ ] 实现搜索可视化（证明树的可视化探索）

### 3.5 第五阶段：DSP 流水线实现（P2-5）

- [ ] 实现 Draft 阶段：LLM 生成自然语言证明草稿
- [ ] 实现 Sketch 阶段：LLM 将草稿细化为结构化步骤（映射到 GeoConstruct 枚举 + 引理标注）
- [ ] 实现 Prove 阶段：Lv-00 在每个 Sketch 步骤上展开，子目标自动分解，失败时反馈修正
- [ ] 实现 DSP 循环：Prove 失败 → 反馈 LLM → 修正 Sketch → 重新 Prove

---

## 4. 设计决策与权衡

### 4.1 REPL 协议：同步 vs 异步

lean-gym 采用同步协议（请求→响应→再请求）。在 Lv-00 中，由于代数求解可能耗时，建议渐进式引入异步模式：

| 模式 | 优点 | 缺点 | 采用阶段 |
|:---|:---|:---|:---|
| 同步 | 简单，易实现，易调试 | 长求解阻塞客户端 | P2-2 先采用 |
| 异步 | 客户端可并行探索多分支 | 状态管理复杂 | P2-4 升级 |

### 4.2 LLM 策略生成：通用模型 vs 几何特化模型

通用 LLM（GPT-4、Claude）在几何定理证明上受训练数据限制。几何特化模型（在 Lv-00 轨线上微调）可显著提升质量。

建议路线：P2-3 使用通用 LLM + 提示工程；后续收集 Lv-00 证明轨线数据微调特化模型。评估指标：Top-1 准确率、Top-5 命中率、平均步骤数。

### 4.3 证明验证严格性分级

miniF2F 的全严格验证在交互探索中过于严格。Lv-00 的严格性分级：

```
级别 0 — 宽松：允许未解决的子目标（探索模式）
级别 1 — 标准：无 sorry，允许多目标未完全解决
级别 2 — 严格：miniF2F 级全验证（最终发布模式）
级别 3 — 证书：额外生成可独立检查的证明证书
```

### 4.4 多后端编码：翻译 vs 互操作

将 Lv-00 翻译为 Lean/Coq 存在语义保真度问题（如交点分支选择无直接对应）。

缓解策略：优先 Lean 4（与 Lv-00 语义最接近）；对其他后端"尽力而为"翻译；差异性评估表记录语义差异；未来实现双向互操作。

---

## 5. 参考资源

- miniF2F 官方仓库：github.com/facebookresearch/miniF2F
- lean-gym 官方仓库：github.com/openai/lean-gym
- GPT-f 论文：Polu & Sutskever, "Generative Language Modeling for Automated Theorem Proving" (2020), arXiv:2009.03393
- HTPS 论文：Lample et al., "HyperTree Proof Search for Neural Theorem Proving" (2022), arXiv:2205.11491
- DSP 论文：Jiang et al., "Draft, Sketch, and Prove: Guiding Formal Theorem Provers with Informal Proofs" (2023), ICLR 2023
- Lean 4 官方文档：lean-lang.org
- mathlib4 数学库：github.com/leanprover-community/mathlib4
- PISA（Portal-to-ISAbelle）：github.com/albertqjiang/Portal-to-ISAbelle —— Isabelle REPL 协议参考
- ProverBot9001：github.com/zhangir-azerbayev/lean-chat —— Lean + LLM 聊天式定理证明

---

## 6. 总结

miniF2F 和 lean-gym 共同构成了神经符号定理证明的核心基础设施，为 Lv-00 在三个关键方向上提供了借鉴。在基准测试层面，miniF2F 的跨系统编码策略启发了几何基准的多后端设计——同一几何问题输出为 Lv-00/Lean/Coq/GeoGebra 四种形式，最大化复用性和可比性。在协议层面，lean-gym 的 REPL 证明状态协议为 Lv-00 暴露证明引擎给外部 AI 提供了经过验证的设计模式——`init_search → run_tac → tactic_state → proof_finished` 的消息循环精确对应几何证明的声明→构造→验证→确认流程。在架构层面，GPT-f + HyperTree Proof Search 的神经符号架构展示了 LLM 策略生成与形式化验证的深度集成方式——LLM 负责"直觉跳跃"，搜索树负责"系统探索"，证明助手负责"逻辑仲裁"。DSP（草稿→草图→证明）流水线进一步连接了 LLM 的非形式化推理与 Lv-00 的形式化验证，使几何定理的证明可以从自然语言描述自动化地推进到可检查的形式证明。这些借鉴共同为 Lv-00 引入 AI 辅助几何定理证明提供了完整的工程路线图。
