# LeanDojo 证明树 + RAG 检索增强证明借鉴设计

> **版本**: 1.0.0
> **日期**: 2026-05-24
> **参考**: [LeanDojo](https://github.com/lean-dojo/LeanDojo) —— Lean 4 的机器学习辅助证明平台，证明树数据模型 + 检索增强生成（RAG）
> **目标**: 借鉴 LeanDojo 的证明树数据模型（四元组节点结构）增强 Lv-00 `proof.h` 的 `ProofNavigator` 数据模型，借鉴 Pantograph 的 LSP 风格 JSON 协议设计 `engine ↔ Web GUI` 通信协议，借鉴 RAG 检索机制增强证明步骤的自动补全能力

---

## 目录

1. [项目概述](#1-项目概述)
2. [核心借鉴要点](#2-核心借鉴要点)
3. [Lv-00 映射方案](#3-lv-00-映射方案)
4. [实现路线图](#4-实现路线图)

---

## 1. 项目概述

### 1.1 LeanDojo 是什么

LeanDojo 是由加州理工学院团队开发的 Lean 4 机器学习辅助证明平台。它提供了一套完整的工具链，使 AI 模型能够与 Lean 4 证明助手交互。LeanDojo 的核心贡献包括三个层次：

1. **证明树数据模型**：将 Lean 4 的证明状态建模为一棵以四元组为节点的树。每个节点包含 `(goal, hypotheses, tactic, children[])` 四个字段——当前证明目标、假设环境、应用的策略（tactic）、以及子目标列表。这种数据结构自然地支持证明搜索（proof search）中的回溯和分支。

2. **检索增强生成（RAG）**：对于当前证明目标，LeanDojo 能够从 mathlib4（Lean 的标准数学库，包含超过 12 万个定理和定义）中检索与当前目标语义最相关的引理。RAG 使用预训练的语言模型将目标和数学陈述编码为向量，然后在向量数据库中执行近似最近邻（ANN）检索。

3. **证明回放（Proof Replay）**：LeanDojo 能够在新版本的 Lean 4 或 mathlib4 环境中重放（replay）之前成功的证明，验证证明的可复现性。这对于持续集成和依赖升级至关重要。

4. **Pantograph 交互协议**：LeanDojo 通过一个名为 Pantograph 的独立进程提供与 Lean 4 的交互。Pantograph 使用 LSP（Language Server Protocol）风格的 JSON 消息格式，支持 `goal`（获取当前目标）、`tactic`（执行策略）、`search`（检索引理）等操作。这使得外部程序（如 Python 脚本）可以轻松地与 Lean 4 核心通信。

### 1.2 为什么借鉴 LeanDojo

Lv-00 的证明系统当前通过 `proof.h` 中的 `ProofNavigator` 管理证明步骤，其数据结构是一个线性步骤序列。这种设计缺少对证明搜索中**分支回溯**的原生支持——当用户尝试一条证明路径后发现走不通，需要手动回到分叉点。借鉴 LeanDojo 的证明树模型意味着：

1. 将 `proof_step` 从线性序列升级为树形结构的 `ProofTreeNode`，支持多分支证明路径
2. 借鉴 Pantograph 的 LSP 风格 JSON 协议，设计 `engine ↔ Web GUI` 的结构化通信协议
3. 借鉴 RAG 检索机制，为几何证明的自动补全提供语义检索能力

---

## 2. 核心借鉴要点

### 2.1 证明树数据模型 —— LeanDojo 四元组

LeanDojo 的证明树节点是一个四元组：

```
ProofTreeNode = {
    goal: Expr,           // 当前证明目标（一个 Lean 表达式）
    hypotheses: List[Expr], // 当前可用的所有假设
    tactic: String,       // 应用的策略名称（如 "rw", "apply", "have"）
    children: List[ProofTreeNode]  // 子目标节点列表
}
```

关键特征：
- **goal + hypotheses 构成完整的证明状态**（proof state），可以序列化为可读的数学陈述
- **tactic 是状态转换函数**：将当前证明状态映射为一组子目标状态
- **children 形成森林**：一个 tactic 可能生成 0 个（证明完成）、1 个（线性递进）或多个（分叉）子目标

### 2.2 RAG 检索增强生成

LeanDojo 的 RAG 流程：

```
当前证明目标 goal
       ↓
[编码器] 将 goal 编码为语义向量 v_goal
       ↓
[向量数据库] ANN 检索 → 前 k 个最相似引理
       ↓
[Reranker] 对 k 个候选引理精排
       ↓
[策略生成器] 使用检索到的引理生成 tactic
       ↓
tactic 应用于当前目标 → 新的证明状态
```

在 Lv-00 的上下文中，"引理"对应的是**已证明的几何定理**和**已知的构造模式**。

### 2.3 Pantograph —— LSP 风格 JSON 协议

Pantograph 的协议基于 JSON 结构化消息，每条消息包含 `command` 和 `params` 两个字段。响应消息包含 `status`、`result` 或 `error`。这种风格与 Language Server Protocol 非常相似——简洁、结构化、易于在跨进程通信中使用。

典型的交互流程：

```
客户端 → Pantograph:  {"cmd": "run_tactic", "tactic": "intro x", "state": state_id}
Pantograph → 客户端:  {"status": "ok", "goals": [...]}

客户端 → Pantograph:  {"cmd": "search", "query": "a + b = b + a", "n": 5}
Pantograph → 客户端:  {"statements": [...]}
```

### 2.4 核心借鉴点映射表

| LeanDojo 概念 | Lv-00 对应概念 | 映射说明 |
|:---|:---|:---|
| `ProofTreeNode`（四元组） | `ProofStepNode`（扩展了 children[] 字段） | 线性步骤 → 树形节点 |
| `goal`（证明目标） | `PropositionPattern`（命题模式） | 几何命题 = 待证明的构造关系 |
| `hypotheses`（假设列表） | `ConstraintGraph` 中的已满足约束 | 当前已建立的几何关系 |
| `tactic`（策略） | `ProofAction`（几何构造动作） | 证明步骤执行的构造操作 |
| `children[]`（子目标） | `branch_ids[]`（分支 ID 数组） | 多解/多路径的证明分支 |
| RAG 检索 | `proof_retrieve_similar()` 语义检索 | 检索相似几何构造模式 |
| 向量数据库 | 几何定理库 + 构造模式索引 | 已证明定理的嵌入向量库 |
| Pantograph 协议 | `lv00_protocol.h`（新文件） | engine ↔ Web GUI 通信协议 |
| `{"cmd": "run_tactic"}` | `{"action": "proof_step_execute"}` | 执行证明步骤 |
| `{"cmd": "search"}` | `{"action": "proof_suggest"}` | 智能补全建议 |
| `{"cmd": "goal"}` | `{"action": "proof_state_get"}` | 获取当前证明状态 |
| 证明回放（replay） | `proof_replay()` 重放验证 | 在新版本环境中重放证明 |

---

## 3. Lv-00 映射方案

### 3.1 证明树数据模型增强 —— 从线性步骤到树形节点

当前的 `proof.h` 将证明步骤建模为线性序列。在 LeanDojo 启发下，为证明步骤增加 `children` 分支支持，使其转变为树形结构：

```c
/**
 * @brief 证明树节点 —— 借鉴 LeanDojo ProofTreeNode 四元组
 *
 * 将 proof_step 从线性序列扩展为树形结构。
 * 节点表示一个证明状态（goal + hypotheses），
 * action 是状态转换函数，children 是子目标分支。
 *
 * 对应 LeanDojo 四元组:
 *   goal       → proposition_id（待证明的几何命题）
 *   hypotheses → hypothesis_ids[]（当前可用的假设/约束集）
 *   tactic     → action（几何构造动作）
 *   children[] → branch_ids[]（子目标/分支节点）
 */
typedef struct {
    int step_id;                        /**< 步骤 ID（全局唯一） */
    int parent_id;                      /**< 父步骤 ID（根节点为 -1） */

    /* LeanDojo 四元组映射 */
    int proposition_id;                 /**< goal: 当前待证明的命题 ID */
    int *hypothesis_ids;                /**< hypotheses: 可用假设的约束 ID 数组 */
    int hypothesis_count;               /**< 假设数量 */

    /* 动作（对应 tactic） */
    ProofAction action;                 /**< 本次执行的构造动作 */
    int action_confidence;              /**< 动作置信度（0-100，AI 建议时使用） */

    /* 子目标（对应 children[]） */
    int *branch_ids;                    /**< 子目标步骤 ID 数组 */
    int branch_count;                   /**< 子目标数量 */

    /* 状态 */
    bool is_completed;                  /**< 此节点的证明是否完成 */
    bool is_leaf;                       /**< 是否为叶节点（无子目标） */
    bool is_backtracked;                /**< 是否被回溯标记（死路径） */

    /* 元数据 */
    int depth;                          /**< 在证明树中的深度 */
    char *description;                  /**< 步骤的人类可读描述 */
    int64_t timestamp_ms;               /**< 创建时间戳 */
} ProofStepNode;
```

### 3.2 证明导航器的树操作 API

```c
/**
 * @brief 在证明树的指定节点下添加子步骤
 *
 * 对应 LeanDojo 中 "在某个 goal 上应用 tactic 产生 children"
 *
 * @param nav          证明导航器
 * @param parent_id    父节点 ID
 * @param action       要执行的构造动作
 * @return 新创建的子步骤 ID，失败返回 -1
 */
int proof_tree_add_child(ProofNavigator *nav, int parent_id, ProofAction action);

/**
 * @brief 回溯到指定节点（废弃当前分支）
 *
 * 将当前节点及其子树标记为 is_backtracked = true，
 * 导航器回退到 target_id 节点
 *
 * @param nav       证明导航器
 * @param target_id 回溯目标节点 ID
 * @return 回溯成功返回 0，失败返回 -1
 */
int proof_tree_backtrack(ProofNavigator *nav, int target_id);

/**
 * @brief 获取从根到指定节点的证明路径
 *
 * 遍历 parent_id 链，返回完整的证明步骤序列
 *
 * @param nav       证明导航器
 * @param node_id   目标节点 ID
 * @param out_path  输出：证明路径（调用者需用 lv00_free 释放）
 * @param out_len   输出：路径长度
 * @return 成功返回 0，失败返回 -1
 */
int proof_tree_get_path(ProofNavigator *nav, int node_id,
                        ProofStepNode **out_path, int *out_len);
```

### 3.3 RAG 检索 —— 几何构造模式语义检索

```c
/**
 * @brief 几何构造模式的语义检索（借鉴 LeanDojo RAG）
 *
 * 给定当前证明目标（一个 PropositionPattern），在 Lv-00 的
 * 几何定理库中检索语义最相似的已证明定理和构造模式。
 *
 * 工作流程：
 *  1. 将 proposition 编码为几何语义向量
 *     - 提取命题中的几何关系类型（共线、共点、垂直、平行...）
 *     - 提取涉及的几何对象类型（点、线、圆、三角形...）
 *     - 编码为稀疏特征向量
 *  2. 在几何定理库的向量索引中执行 ANN 检索
 *  3. 对前 top_k 个候选进行重排序
 *     - 考虑命题的精确匹配度
 *     - 考虑构造步骤的复杂度（优先简单构造）
 *     - 考虑历史使用频率（常用定理加权）
 *  4. 返回排序后的检索结果
 *
 * @param nav           证明导航器（提供当前上下文）
 * @param proposition   当前要证明的命题
 * @param top_k         返回前 k 个最相关结果
 * @param out_results   输出：检索结果数组（调用者需释放）
 * @param out_count     输出：实际结果数量
 * @return 成功返回 0，失败返回 -1
 *
 * @see leandojo_proof_tree.md —— LeanDojo RAG 参考
 */
typedef struct {
    int theorem_id;              /**< 定理/模式 ID */
    char *theorem_name;          /**< 定理名称 */
    char *theorem_statement;     /**< 定理陈述 */
    float similarity_score;      /**< 语义相似度 (0.0 ~ 1.0) */
    int usage_count;             /**< 历史使用次数 */
    char *applicable_reason;     /**< 可应用理由（自然语言） */
} ProofRetrievalResult;

int proof_retrieve_similar(
    ProofNavigator *nav,
    const PropositionPattern *proposition,
    int top_k,
    ProofRetrievalResult **out_results,
    int *out_count
);
```

### 3.4 Pantograph 风格协议 —— engine ↔ Web GUI 通信

借鉴 Pantograph 的 LSP 风格 JSON 协议，设计 Lv-00 的进程间通信协议：

```
协议设计原则（借鉴 Pantograph）：
  1. 请求-响应模型：Web GUI 发送 JSON 请求，engine 返回 JSON 响应
  2. 状态化：每个证明会话有唯一 session_id
  3. 可扩展：新增 action 类型不破坏现有协议
  4. 错误标准化：统一的错误码体系
```

**核心消息类型**：

| 消息方向 | action 名称 | 说明 | LeanDojo 对应 |
|:---|:---|:---|:---|
| GUI → Engine | `proof_step_execute` | 执行一个证明步骤 | `run_tactic` |
| GUI → Engine | `proof_state_get` | 获取当前证明状态 | `goal` |
| GUI → Engine | `proof_suggest` | 获取自动补全建议 | `search` |
| GUI → Engine | `proof_tree_get` | 获取完整证明树 | (无直接对应) |
| GUI → Engine | `proof_replay` | 在新环境中重放证明 | (proof replay 功能) |
| Engine → GUI | `proof_state_update` | 推送证明状态变更 | (goal 更新通知) |
| Engine → GUI | `proof_error` | 证明错误通知 | (tactic 错误) |
| Engine → GUI | `proof_completed` | 证明完成通知 | (goal 关闭) |

**协议消息格式**：

```json
// GUI → Engine: 执行证明步骤
{
    "session_id": "abc123",
    "action": "proof_step_execute",
    "params": {
        "parent_step_id": 5,
        "construction_type": "GEOM_CONSTRUCT_CIRCLE_CENTER_RADIUS",
        "parameters": {
            "center_id": 12,
            "radius_expr": "AB_distance"
        }
    }
}

// Engine → GUI: 步骤执行成功
{
    "session_id": "abc123",
    "action": "proof_step_execute",
    "status": "ok",
    "result": {
        "new_step_id": 7,
        "new_branches": [8],
        "constraints_added": 2,
        "proposition_status": "partial"
    }
}

// GUI → Engine: 获取自动补全建议
{
    "session_id": "abc123",
    "action": "proof_suggest",
    "params": {
        "current_step_id": 7,
        "top_k": 5
    }
}

// Engine → GUI: 返回建议
{
    "session_id": "abc123",
    "action": "proof_suggest",
    "status": "ok",
    "result": {
        "suggestions": [
            {
                "rank": 1,
                "construction_type": "GEOM_CONSTRUCT_MIDPOINT",
                "description": "取线段AB的中点M",
                "confidence": 0.92,
                "rationale": "中点可用于构造对称辅助线"
            },
            ...
        ]
    }
}
```

### 3.5 协议头文件设计

```c
/**
 * @file lv00_protocol.h
 * @brief Lv-00 engine ↔ Web GUI 通信协议（借鉴 LeanDojo Pantograph）
 *
 * 每条消息由固定的四部分组成:
 *   session_id:  会话标识符（UUID）
 *   action:      操作类型（字符串，见 ProtocolAction 枚举）
 *   params:      操作参数（JSON 对象，action 相关）
 *   status:      响应状态（"ok" / "error" / "pending"）
 *   result:      操作结果（JSON 对象，成功时；action 相关）
 *   error:       错误信息（JSON 对象，失败时）
 */

/**
 * @brief 协议操作类型枚举
 */
typedef enum {
    /* 证明步骤操作 */
    PROTO_ACTION_PROOF_STEP_EXECUTE,     /**< 执行证明步骤 */
    PROTO_ACTION_PROOF_STEP_UNDO,        /**< 撤销最近步骤 */
    PROTO_ACTION_PROOF_STEP_REDO,        /**< 重做已撤销步骤 */

    /* 证明状态查询 */
    PROTO_ACTION_PROOF_STATE_GET,        /**< 获取当前证明状态 */
    PROTO_ACTION_PROOF_TREE_GET,         /**< 获取完整证明树 */

    /* 智能建议 */
    PROTO_ACTION_PROOF_SUGGEST,          /**< 获取自动补全建议 */
    PROTO_ACTION_PROOF_RETRIEVE,         /**< RAG 检索相关定理 */

    /* 证明回放 */
    PROTO_ACTION_PROOF_REPLAY,           /**< 证明回放 */

    /* 会话管理 */
    PROTO_ACTION_SESSION_CREATE,         /**< 创建会话 */
    PROTO_ACTION_SESSION_CLOSE,          /**< 关闭会话 */

    /* 引擎通知（Engine → GUI） */
    PROTO_ACTION_PROOF_STATE_UPDATE,     /**< 证明状态变更推送 */
    PROTO_ACTION_PROOF_ERROR,            /**< 证明错误通知 */
    PROTO_ACTION_PROOF_COMPLETED,        /**< 证明完成通知 */
} ProtocolAction;
```

### 3.6 协议与现有系统的集成点

| Lv-00 现有组件 | 协议集成方式 |
|:---|:---|
| `proof.h` / `ProofNavigator` | `proof_state_get` 序列化当前 `ProofStepNode` 树 |
| `constraint_graph.h` | 约束变更通过 `proof_state_update` 推送到 Web GUI |
| `solver.h` | 求解器状态通过专用 `solver_status` 字段报告 |
| `type_system.h` | 类型检查结果通过 `proof_error` 通知 |
| Web GUI 拖拽交互 | GUI 中的交互操作通过协议封装为 `proof_step_execute` 请求 |
| 已有 JSON 序列化 | 复用 `json_serialize_*()` 函数进行协议消息的序列化/反序列化 |

### 3.7 证明回放机制

```c
/**
 * @brief 证明回放 —— 借鉴 LeanDojo 的 proof replay
 *
 * 在新的证明环境（约束图状态、求解器版本、定理库版本）中
 * 重新执行已有的证明树，验证证明的可复现性。
 *
 * 回放流程：
 *  1. 加载已存储的证明树（JSON 格式）
 *  2. 从根节点开始，逐步重建每个中间步骤
 *  3. 在每个步骤执行后，验证目标命题状态是否与原始一致
 *  4. 如果任一步骤失败，返回失败节点 ID 和原因
 *  5. 如果全部成功，输出回放报告
 *
 * @param nav           证明导航器（新环境）
 * @param proof_json    已存储的证明树 JSON
 * @param out_report    输出：回放报告（调用者需释放）
 * @return 回放成功返回 0，失败返回失败节点 ID 的负值
 *
 * @note 证明回放对以下场景至关重要:
 *       - 约束图结构升级后的向后兼容性验证
 *       - 求解器算法改进后的正确性回归测试
 *       - 定理库扩充后已有证明的再验证
 */
typedef struct {
    bool replay_success;             /**< 回放是否完全成功 */
    int failed_step_id;              /**< 失败步骤 ID（成功时为 -1） */
    char *failure_reason;            /**< 失败原因（成功时为 NULL） */
    int total_steps;                 /**< 总步骤数 */
    int replayed_steps;              /**< 成功回放的步骤数 */
    int64_t replay_time_ms;          /**< 回放耗时 */
    char *diff_report;               /**< 差异报告（证明树结构变化） */
} ProofReplayReport;

int proof_replay(ProofNavigator *nav, const char *proof_json,
                 ProofReplayReport **out_report);
```

---

## 4. 实现路线图

### 4.1 第一阶段：证明树数据模型（P1）

- [ ] 在 `proof.h` 中定义 `ProofStepNode` 结构体（包含四元组字段）
- [ ] 实现 `proof_tree_add_child()` 添加子步骤
- [ ] 实现 `proof_tree_backtrack()` 回溯操作
- [ ] 实现 `proof_tree_get_path()` 获取证明路径
- [ ] 实现证明树的 JSON 序列化/反序列化
- [ ] 将现有的 `proof_step` 线性序列迁移到树结构
- [ ] 编写证明树操作单元测试

### 4.2 第二阶段：通信协议（P1-P2）

- [ ] 创建 `include/lv00/lv00_protocol.h` 协议头文件
- [ ] 定义所有 `ProtocolAction` 枚举值和消息格式
- [ ] 实现 JSON 消息的打包/解包函数
- [ ] 实现 `proof_step_execute` 处理逻辑
- [ ] 实现 `proof_state_get` 和 `proof_state_update`
- [ ] 实现统一错误码体系和错误处理
- [ ] 编写协议消息的单元测试
- [ ] Web GUI 协议适配层（WebSocket handler）

### 4.3 第三阶段：RAG 检索（P2-P3）

- [ ] 设计几何命题的特征向量编码方案
  - 几何关系类型 one-hot 编码
  - 几何对象类型的层次编码
  - 组合为稀疏语义向量
- [ ] 实现 `proof_retrieve_similar()` 检索函数
- [ ] 构建几何定理库的向量索引
- [ ] 实现 `proof_suggest` 协议的完整处理链
- [ ] 实现重排序（reranking）逻辑
- [ ] 编写检索质量的评估测试

### 4.4 第四阶段：证明回放（P3）

- [ ] 实现 `proof_replay()` 回放核心逻辑
- [ ] 实现回放差异检测和报告生成
- [ ] 实现回放断点续跑（从失败步骤继续）
- [ ] 集成到 CI/CD 流水线
- [ ] 编写回归证明套件

---

## 5. 设计决策与权衡

### 5.1 树结构 vs 线性结构

当前 Lv-00 的证明步骤是线性序列，升级到树结构的代价和收益：

| 方面 | 线性结构（当前） | 树结构（LeanDojo 启发） |
|:---|:---|:---|
| 实现复杂度 | 简单 | 中等（需管理回溯状态） |
| 空间开销 | 低（O(n) n=步骤数） | 中等（O(n) + 分支开销） |
| 多分支支持 | 不支持 | 原生支持 |
| 回溯操作 | 需手动管理 | `proof_tree_backtrack()` 一次调用 |
| Web GUI 展示 | 简单线性列表 | 需树形可视化组件 |
| 证明搜索（AI） | 受限 | 完全支持 best-first search / beam search |

**决策**：升级到树结构。虽然增加了一定复杂度，但为 AI 辅助证明搜索和多路径探索提供了必要的基础设施。

### 5.2 RAG 检索的冷启动问题

RAG 检索需要预先构建几何定理库的向量索引。在 Lv-00 项目的早期阶段，定理库可能很小。缓解策略：

- **规则优先**：在定理库不足时，默认使用硬编码的几何规则（如"两点确定一条直线"、"三角形内角和 180 度"）
- **增量索引**：每次用户完成一个证明，自动将新定理加入向量索引
- **渐进式 RAG**：当定理库 < 50 条时使用精确匹配，50-200 条时使用简单余弦相似度，>200 条时启用 ANN 近似检索

### 5.3 Pantograph 协议 vs REST/gRPC

对 engine ↔ Web GUI 通信的三种协议方案比较：

| 方面 | Pantograph 风格 JSON | REST API | gRPC |
|:---|:---|:---|:---|
| 复杂度 | 低 | 低 | 中 |
| 实时推送 | 天然支持 | 需 WebSocket/SSE | 需 stream API |
| 浏览器兼容 | 完全（JSON + WebSocket） | 完全 | 需 grpc-web 代理 |
| 类型安全 | 手动（JSON Schema） | 手动 | 自动（protobuf） |
| LSP 对齐性 | 高度对齐 | 部分对齐 | 不对齐 |

**决策**：采用 Pantograph 风格 JSON + WebSocket。Lv-00 的通信需求相对简单（证明步骤、状态查询、建议），JSON 协议足够表达。WebSocket 支持服务端主动推送（状态变更通知），且所有现代浏览器原生支持。

---

## 6. 总结

LeanDojo 的证明树数据模型为 Lv-00 的证明系统提供了从线性步骤到树形分支的关键升级路径。证明树四元组 `(goal, hypotheses, tactic, children[])` 精确映射到 Lv-00 的 `(proposition_id, hypothesis_ids[], action, branch_ids[])`，使得多分支证明搜索和回溯操作成为一等公民。Pantograph 的 LSP 风格 JSON 协议直接启发了 `lv00_protocol.h` 的设计——通过 `session_id + action + params + result` 的标准化消息格式，实现 engine 与 Web GUI 之间的松耦合通信。RAG 检索机制为几何证明的智能补全提供了理论基础，当定理库逐渐丰富后，`proof_retrieve_similar()` 能够在亿级构造模式中秒级返回最相关的候选构造。

| LeanDojo 核心概念 | Lv-00 映射组件 | 实现文件 |
|:---|:---|:---|
| ProofTreeNode 四元组 | `ProofStepNode` 结构体 | `proof.h` |
| 证明树遍历/回溯 | `proof_tree_add_child/backtrack/get_path` | `proof.c` |
| RAG 检索 | `proof_retrieve_similar()` + 向量索引 | `proof_retrieve.c`（新文件） |
| Pantograph LSP 协议 | `lv00_protocol.h` + JSON 消息格式 | `lv00_protocol.h`（新文件） |
| 证明回放 | `proof_replay()` + `ProofReplayReport` | `proof_replay.c`（新文件） |
| `run_tactic` → `proof_step_execute` | ProtocolAction 枚举 + handler | `lv00_protocol.c`（新文件） |
| `search` → `proof_suggest` | 智能补全协议处理 | `lv00_protocol.c`（新文件） |
| `goal` → `proof_state_get` | 证明状态序列化 | `lv00_protocol.c`（新文件） |

---

> **文档结束**
> 本文档详述了 LeanDojo 的证明树数据模型、RAG 检索增强生成、证明回放机制和 Pantograph LSP 风格通信协议如何映射到 Lv-00 的证明系统和通信架构。核心结论：通过将证明步骤从线性序列升级为四元组证明树，Lv-00 获得了多分支证明搜索和回溯的原生支持；通过借鉴 Pantograph 的 JSON 协议，engine 与 Web GUI 的通信变得标准化和可扩展；通过 RAG 的语义检索，几何证明的自动补全获得了理论基础——当定理库扩充后，AI 能够在亿级构造模式中检索最相关的候选构造。
