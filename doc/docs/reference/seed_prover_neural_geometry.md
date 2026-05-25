# Seed-Prover / Seed-Geometry 神经几何证明参考文档

> **借鉴项目**：Seed-Prover 1.5 / Seed-Geometry（字节跳动 Seed 团队）
> **核心借鉴点**：Agentic RL 训练的形式化数学推理专用模型、神经推理与符号推理的混合架构、
>   自动辅助构造搜索、IMO 级别几何定理证明、形式化证明输出
> **分类**：P1 最高优先级 / 神经符号几何定理证明
> **日期**：2026-05-25

---

## 1. 项目概述

### 1.1 项目简介

Seed-Prover 是字节跳动 Seed 团队开发的形式化数学推理专用大语言模型系列，最新版本为
Seed-Prover 1.5（2025 年 12 月 24 日发布）。该模型通过大规模 Agentic Reinforcement
Learning（智能体强化学习）训练，在形式化数学证明生成方面取得了突破性成果。其姊妹项目
Seed-Geometry 专注于几何定理的神经符号证明，在 IMO-AG-50 基准上解决了 43 道几何题
（2000-2024 年），全面超越 Google DeepMind 的 AlphaGeometry 2。

Seed-Prover 的核心定位是"形式化数学推理专用模型"，而非通用对话模型。它直接输出可编译
验证的 Lean 4 证明代码，实现了从自然语言数学问题到机器可验证证明的端到端生成。这一设计
理念与 Lv-00 的"几何证明形式化验证"目标高度契合。

### 1.2 技术栈

| 维度 | 技术详情 |
|:---|:---|
| 基础模型架构 | 大规模 LLM（Transformer），支持长思维链推理 |
| 训练范式 | Agentic RL（智能体强化学习），深度与广度推理结合 |
| 形式化后端 | Lean 4（可编译验证的形式化证明输出） |
| 几何推理 | 神经推理 + 符号推理混合方法（Seed-Geometry） |
| 辅助构造 | 基于强化学习的自动搜索策略 |
| 推理策略 | 多策略并行搜索、草稿-草图-证明流水线 |
| 输出格式 | 完整可编译的 Lean 4 证明代码 |
| 发布形式 | 技术报告已公开，API 计划开放 |

### 1.3 核心性能指标

| 基准测试 | Seed-Prover 1.5 成绩 | 对比基准 |
|:---|:---|:---|
| IMO 2025（前 5 题） | 35/42（金牌线），16.5 小时 | AlphaGeometry 2 未达金牌线 |
| IMO-AG-50（2000-2024 几何题） | 43/50 解决 | AlphaGeometry 2：42/50 |
| MiniF2F 数据集 | 100% 正确率 | 此前 SOTA 低于 100% |
| Putnam 2025（12 题中 11 题） | 11/12，9 小时 | 历史评估集 88% |
| Fate-H（硕士级） | 80% 解决率 | 刷新 SOTA |
| Fate-X（博士级） | 33% 解决率 | 刷新 SOTA |

### 1.4 社区活跃度与许可证

- **开发团队**：字节跳动 Seed 团队（ByteDance Seed Team）
- **技术报告**：已公开发布
- **开源状态**：技术报告公开，API 计划开放；核心模型权重尚未完全开源
- **社区影响**：在形式化数学推理领域引发广泛关注，被认为是 2025 年度最重要的
  数学 AI 突破之一
- **许可证**：具体许可证条款待 API 正式发布后确认

---

## 2. 核心借鉴点

### 2.1 Agentic RL 训练范式

Seed-Prover 1.5 的核心技术突破在于 Agentic Reinforcement Learning 训练范式。与传统
的监督微调（SFT）不同，Agentic RL 将模型视为一个在形式化证明环境中自主决策的智能体：

- **环境**：Lean 4 形式化证明系统，提供编译验证反馈
- **动作空间**：生成 Lean 4 策略（tactic）序列
- **奖励信号**：证明编译通过（正奖励）、编译失败（负奖励）、证明长度惩罚
- **深度推理**：长思维链（Long Chain-of-Thought），支持数百步推理
- **广度推理**：多分支并行探索，在证明树中维护多个候选路径

**对 Lv-00 的启发**：Lv-00 的证明引擎（第 4 层）目前采用确定性的符号推理策略。
Agentic RL 的训练范式提示我们可以在第 7 层 LLM 助手中引入"证明搜索智能体"，
通过与环境（Lv-00 证明引擎）的交互反馈来优化证明策略选择。

具体而言，Lv-00 可以构建类似的"环境-智能体"循环：
- **环境**：Lv-00 证明引擎（Lv00ProofEngine），提供合一检查、约束求解等验证反馈
- **动作空间**：选择证明策略（Lv00StrategyType）、添加辅助构造、切换推理方向
- **奖励信号**：证明完成（正奖励）、步骤数增加（轻微惩罚）、回溯（负奖励）
- **深度推理**：利用 LLM 的长上下文窗口维护完整的证明步骤链
- **广度推理**：利用 Lv00ProofEngine 的并行证明能力（enable_parallel）探索多分支

### 2.2 神经推理与符号推理的混合架构

Seed-Geometry 采用"神经推理 + 符号推理"的双轨架构：

```
[几何问题]
    |
    +-- [神经推理轨道] --> LLM 生成辅助构造、直觉推理
    +-- [符号推理轨道] --> 约束求解、代数推理、Groebner 基
    +-- [混合控制器]   --> 动态调度两条轨道，合并结果
                            |
                        [形式化证明]
```

这一架构与 Lv-00 的多策略引擎（JGEX 风格的 8 种证明方法共存）高度互补。Lv-00 已有
符号推理能力（约束求解、Groebner 基、面积法等），缺少的是"神经推理轨道"——即 LLM
提供直觉性推理建议的能力。

### 2.3 自动辅助构造搜索

几何定理证明中最困难的环节之一是"辅助构造"（auxiliary construction）——即在证明
过程中添加原本不在题目中的点、线、圆等几何元素。Seed-Geometry 通过强化学习训练模型
自动搜索有效的辅助构造，这是其超越 AlphaGeometry 2 的关键因素。

**对 Lv-00 的启发**：Lv-00 的证明导航器（ProofNavigator）支持步骤添加和回溯，
但辅助构造的自动搜索尚未实现。可以借鉴 Seed-Geometry 的方法，在第 7 层 LLM 助手中
实现"辅助构造建议"功能。具体实现路径包括：

1. **规则库驱动**：构建常见辅助构造模式的启发式规则库（中点、外接圆、高线、
   角平分线、切线、平行线等），根据约束图特征匹配触发
2. **LLM 增强**：将约束图状态序列化为结构化描述，由 LLM 生成候选辅助构造
3. **符号验证**：通过 Lv-00 的合一引擎（unify.h）验证辅助构造的合法性，
   确保不引入矛盾
4. **搜索剪枝**：利用 Lv00ProofEngine 的超时机制（timeout_ms）和最大深度
   限制（max_depth）控制搜索空间

### 2.4 形式化证明输出

Seed-Prover 直接输出可编译验证的 Lean 4 证明代码，确保每一步推理的严格正确性。这与
Lv-00 的证明导出系统（第 6 层：HTML/LaTeX/Coq/自然语言）目标一致。Lv-00 可以
进一步增加 Lean 4 导出后端，实现与 Seed-Prover 生态的互操作。

Lv-00 当前的多后端导出能力与 Lean 4 的对比如下：

| 导出格式 | 当前状态 | 与 Lean 4 的关系 |
|:---|:---|:---|
| HTML | 已实现 | Lean 4 可通过 Aesop 渲染为 HTML |
| LaTeX | 已实现 | Lean 4 证明可嵌入 LaTeX 文档 |
| Coq | 已实现 | Coq 与 Lean 4 同属归纳构造族，可参照映射 |
| 自然语言 | 已实现 | Lean 4 的 Aesop 策略可生成自然语言证明 |
| Lean 4 | **待实现** | Seed-Prover 的原生输出格式 |

增加 Lean 4 导出后端后，Lv-00 的证明结果可以直接提交给 Lean 4 编译器验证，
也可以与 Seed-Prover 的输出进行交叉验证，进一步提升证明的可信度。

### 2.5 Seed-Prover 特性与 Lv-00 证明引擎对照表

| Seed-Prover 特性 | Lv-00 对应模块 | 当前状态 | 借鉴方向 |
|:---|:---|:---|:---|
| Agentic RL 训练 | 第 7 层 LLM 助手（ai_engine.py） | 已有基础 LLM 集成 | 引入证明搜索智能体，增加 RL 反馈循环 |
| 长思维链推理 | 第 4 层 ProofNavigator | 已有步骤链 | 增加神经推理建议层 |
| 神经+符号混合推理 | 第 4 层多策略引擎 | 已有 8 种符号策略 | 增加第 9 种"神经建议"策略 |
| 自动辅助构造搜索 | 第 4 层 ProofStep（ADD_NODE） | 手动添加 | LLM 自动生成辅助构造建议 |
| 形式化 Lean 4 输出 | 第 6 层证明导出 | 已有 Coq 导出 | 增加 Lean 4 导出后端 |
| 多分支并行搜索 | 第 3 层引擎调度器 | 已有并行支持 | 增加神经引导的搜索优先级 |
| 草稿-草图-证明流水线 | 第 4 层证明引擎 | 已有直接/反证法等 | 增加三级流水线：直觉->构造->形式化 |
| IMO-AG-50 基准 | axiom_packages/ | 已有公理包系统 | 增加 IMO 几何题基准测试集 |
| MiniF2F 100% 正确率 | benchmark_results/ | 已有基准框架 | 增加 MiniF2F 风格评估 |
| 证明编译验证 | 第 4 层合一引擎 | 已有合一检查 | 增加外部 Lean 4 编译验证接口 |

---

## 3. Lv-00 映射方案

### 3.1 总体集成架构

在 Lv-00 的 7 层架构中，Seed-Prover 的借鉴主要体现在第 4 层（证明引擎）和
第 7 层（LLM 编码助手）的增强。整体集成方案如下：

```
Lv-00 第 7 层（应用框架）
    +-- LLM 编码助手（ai_engine.py）
    |       +-- [新增] 神经证明搜索模块（neural_proof_search.py）
    |       |       +-- 辅助构造建议器
    |       |       +-- 证明策略选择器
    |       |       +-- 草稿-草图-证明流水线
    |       +-- [新增] Lean 4 导出适配器
    |
Lv-00 第 4 层（证明引擎）
    +-- Lv00ProofEngine（proof_engine_enhanced.h）
    |       +-- [新增] 策略类型：STRATEGY_NEURAL_SUGGEST
    |       +-- [增强] ProofNavigator：增加 LLM 建议步骤注入
    |
Lv-00 第 6 层（数据交换）
    +-- [新增] Lean 4 导出后端
```

### 3.2 第 4 层：证明引擎增强

#### 3.2.1 新增神经建议策略类型

在 `proof_engine_enhanced.h` 的 `Lv00StrategyType` 枚举中增加神经推理策略：

```c
typedef enum {
    STRATEGY_DIRECT,            /* 直接证明 */
    STRATEGY_CONTRADICTION,     /* 反证法 */
    STRATEGY_CONTRAPOSITIVE,    /* 逆否证明 */
    STRATEGY_INDUCTION,         /* 数学归纳法 */
    STRATEGY_CASES,             /* 分情况讨论 */
    STRATEGY_CONSTRUCTION,      /* 构造性证明 */
    STRATEGY_UNFOLDING,         /* 定义展开 */
    STRATEGY_BACKWARD,          /* 逆向推理 */
    STRATEGY_FORWARD,           /* 正向推理 */
    STRATEGY_HYBRID,            /* 混合策略 */
    /* --- 新增：神经推理策略（借鉴 Seed-Prover） --- */
    STRATEGY_NEURAL_SUGGEST,    /* 神经建议：LLM 生成推理方向 */
    STRATEGY_AUX_CONSTRUCT,     /* 辅助构造：LLM 建议辅助点/线/圆 */
    STRATEGY_DRAFT_SKETCH_PROVE /* 草稿-草图-证明流水线 */
} Lv00StrategyTypeEx;
```

#### 3.2.2 神经建议步骤注入接口

在 `proof.h` 的 `ProofStepType` 枚举中增加 LLM 建议步骤类型：

```c
typedef enum {
    PROOF_STEP_ADD_NODE,         /* 添加节点 */
    PROOF_STEP_ADD_CONSTRAINT,   /* 添加约束 */
    PROOF_STEP_REWRITE,          /* 重写步骤 */
    PROOF_STEP_FUNCTION_APP,     /* 函数应用 */
    PROOF_STEP_PACK_FUNCTION,    /* 打包函数块 */
    PROOF_STEP_NORMALIZATION,    /* 自动规范化 */
    PROOF_STEP_UNIFY,            /* 合一检查 */
    PROOF_STEP_EX_FALSO,         /* 爆炸原理步骤 */
    PROOF_STEP_ORACLE,           /* Oracle 依赖 */
    /* --- 新增：神经推理步骤（借鉴 Seed-Prover） --- */
    PROOF_STEP_NEURAL_SUGGESTION,   /* LLM 推理建议（非绑定） */
    PROOF_STEP_AUX_CONSTRUCTION,    /* LLM 辅助构造建议 */
    PROOF_STEP_STRATEGY_SWITCH      /* 策略切换（由 LLM 触发） */
} ProofStepTypeEx;
```

#### 3.2.3 证明引擎 REPL 接口

借鉴 Seed-Prover 的"模型-环境交互"范式，为 Lv-00 证明引擎设计 JSON REPL 接口，
使第 7 层的 LLM 助手能够以结构化方式与证明引擎交互：

```c
/* 证明搜索会话：使 LLM 以 step-by-step 方式参与证明搜索 */
typedef struct {
    char session_id[64];          /* 会话唯一标识 */
    Proposition *target;          /* 目标命题 */
    ConstraintGraph *graph;       /* 当前约束图快照 */
    ProofNavigator *navigator;    /* 证明导航器 */
    int step_count;               /* 已执行步骤数 */
    double llm_confidence;        /* LLM 建议置信度 */
    bool is_complete;             /* 证明是否完成 */
} Lv00ProofSession;

/* 创建/销毁证明搜索会话 */
Lv00ProofSession *lv00_proof_session_create(
    Lv00ProofEngine *engine, const Proposition *prop);
void lv00_proof_session_destroy(Lv00ProofSession *session);

/* 获取当前证明状态 JSON（含 pending_goals, available_rules 等） */
char *lv00_proof_session_get_state_json(const Lv00ProofSession *session);

/* 提交 LLM 建议（辅助构造/策略切换/推理方向），返回执行结果 JSON */
int lv00_proof_session_submit_suggestion(
    Lv00ProofSession *session, const char *suggestion, char **out_result);
```

### 3.3 第 7 层：LLM 助手中的神经证明搜索模块

在 `llm_coding_assistant/core/` 下新增 `neural_proof_search.py`，实现
Seed-Prover 风格的神经证明搜索。核心类包括：

**AuxiliaryConstructionAdvisor（辅助构造建议器）**：借鉴 Seed-Geometry 的自动
辅助构造搜索策略，通过分析约束图结构识别需要添加的辅助几何元素。内置启发式规则库
（中点、外接圆、高线、角平分线、切线、平行线等 6 种常见模式），按置信度排序输出建议。

**DraftSketchProvePipeline（草稿-草图-证明流水线）**：借鉴 Seed-Prover 的 DSP
范式，将证明生成分三个递进阶段：
1. 草稿阶段：LLM 生成直觉性推理思路和关键思路列表
2. 草图阶段：将草稿细化为结构化步骤和所需引理
3. 证明阶段：通过 Lv-00 REPL 接口逐步提交并验证

**NeuralProofAgent（神经证明搜索智能体）**：借鉴 Agentic RL 范式，实现广度优先
+ 神经引导的搜索策略。在每次迭代中执行完整的 DSP 流水线，失败时尝试添加辅助构造
或切换证明策略，最多迭代 10 次。

```python
"""neural_proof_search.py -- 核心数据结构与辅助构造建议器"""

from dataclasses import dataclass
from typing import Optional, List, Dict, Any

@dataclass
class AuxiliaryConstruction:
    """辅助构造建议"""
    element_type: str          # "point" | "line" | "circle" | "segment"
    description: str           # 自然语言描述
    formal_expr: str           # Lv-00 形式化表达式
    confidence: float          # 置信度 [0, 1]
    rationale: str             # 推理依据

class AuxiliaryConstructionAdvisor:
    """辅助构造建议器 -- 借鉴 Seed-Geometry 的自动辅助构造搜索"""

    CONSTRUCTION_PATTERNS = {
        "midpoint":       {"trigger": "三角形中位线", "type": "point",
                           "desc": "取边的中点", "why": "中位线平行于第三边"},
        "circumcircle":   {"trigger": "共圆条件",   "type": "circle",
                           "desc": "作外接圆",   "why": "圆周角定理建立角度关系"},
        "altitude":       {"trigger": "垂直关系",   "type": "line",
                           "desc": "作高线",     "why": "提供直角三角形和面积关系"},
        "angle_bisector": {"trigger": "角度等分",   "type": "line",
                           "desc": "作角平分线", "why": "角平分线分对边成比例"},
        "tangent":        {"trigger": "切线条件",   "type": "line",
                           "desc": "作切线",     "why": "切线垂直于半径"},
        "parallel":       {"trigger": "平行关系",   "type": "line",
                           "desc": "作平行线",   "why": "平行线截割定理建立比例"},
    }

    def suggest(self, context: Dict[str, Any]) -> List[AuxiliaryConstruction]:
        """根据问题上下文生成辅助构造建议，按置信度排序"""
        suggestions = []
        for name, pat in self.CONSTRUCTION_PATTERNS.items():
            relevance = self._analyze_relevance(pat["trigger"], context)
            if relevance > 0.3:
                suggestions.append(AuxiliaryConstruction(
                    element_type=pat["type"], description=pat["desc"],
                    formal_expr="", confidence=relevance, rationale=pat["why"]))
        return sorted(suggestions, key=lambda x: x.confidence, reverse=True)

    def _analyze_relevance(self, trigger: str, context: Dict) -> float:
        """分析构造模式与当前问题的相关性（简化版）"""
        text = str(context.get("graph_summary", "")) + " ".join(context.get("goals", []))
        return 0.6 if any(kw in text for kw in trigger.split("/")) else 0.0


class DraftSketchProvePipeline:
    """草稿-草图-证明三级流水线 -- 借鉴 Seed-Prover 的 DSP 范式"""

    def __init__(self, ai_engine, advisor: AuxiliaryConstructionAdvisor):
        self.ai_engine = ai_engine
        self.advisor = advisor

    async def generate_proof(self, problem: str) -> "NeuralProofResult":
        """执行三级流水线：草稿 -> 草图 -> 形式化证明"""
        result = NeuralProofResult()
        result.draft = await self._generate_draft(problem)
        if result.draft is None:
            return result
        result.sketch = await self._generate_sketch(result.draft)
        if result.sketch is None:
            return result
        # 阶段三：通过 Lv-00 REPL 接口逐步提交并验证
        result.formal_proof = await self._formalize_proof(result.sketch)
        return result

    async def _generate_draft(self, problem: str) -> "ProofDraft":
        """阶段一：LLM 生成直觉性推理思路"""
        prompt = (f"作为几何证明专家，分析以下几何问题并给出直觉性推理思路：\n"
                  f"问题：{problem}\n请以 JSON 格式返回直觉描述和关键思路。")
        response = await self.ai_engine.chat(prompt)
        # 解析 JSON 并构造 ProofDraft（省略异常处理）
        ...

    async def _generate_sketch(self, draft) -> "ProofSketch":
        """阶段二：将草稿细化为结构化步骤和所需引理"""
        ...

    async def _formalize_proof(self, sketch) -> Optional[str]:
        """阶段三：通过 ctypes 调用 lv00_proof_session_* 系列函数验证"""
        ...


class NeuralProofAgent:
    """神经证明搜索智能体 -- 借鉴 Agentic RL 范式"""

    def __init__(self, ai_engine):
        self.ai_engine = ai_engine
        self.advisor = AuxiliaryConstructionAdvisor()
        self.pipeline = DraftSketchProvePipeline(ai_engine, self.advisor)
        self.max_iterations = 10

    async def search_proof(self, problem: str, timeout: int = 300) -> "NeuralProofResult":
        """广度优先 + 神经引导的搜索，失败时尝试辅助构造或策略切换"""
        best = NeuralProofResult()
        for i in range(self.max_iterations):
            result = await self.pipeline.generate_proof(problem)
            if result.formal_proof is not None:
                return result  # 证明成功
            if result.confidence > best.confidence:
                best = result
        return best
```

在 `ai_engine.py` 中注册神经证明搜索方法：

```python
async def search_geometry_proof(self, problem: str,
                                 context: Optional[Dict] = None) -> Dict:
    """调用神经证明搜索模块进行几何证明搜索（借鉴 Seed-Prover Agentic RL）"""
    from .neural_proof_search import NeuralProofAgent
    agent = NeuralProofAgent(self)
    result = await agent.search_proof(problem)
    return {
        "draft": {"intuition": result.draft.intuition if result.draft else None,
                  "key_ideas": result.draft.key_ideas if result.draft else []},
        "sketch": {"steps": result.sketch.steps if result.sketch else [],
                   "strategy": result.sketch.strategy if result.sketch else None},
        "formal_proof": result.formal_proof,
        "confidence": result.confidence, "total_steps": result.total_steps,
    }
```

### 3.4 第 6 层：Lean 4 导出后端

借鉴 Seed-Prover 的 Lean 4 输出能力，在 Lv-00 的证明导出系统中增加 Lean 4 后端：

```c
typedef struct {
    char theorem_name[256];       /* 定理名称 */
    char namespace[128];          /* Lean 命名空间 */
    bool import_mathlib;          /* 是否导入 Mathlib */
    bool include_comments;        /* 是否包含注释 */
} Lv00Lean4ExportConfig;

/* 将证明导出为 Lean 4 代码 */
char *lv00_export_lean4(const ProofNavigator *navigator,
                        const Lv00Lean4ExportConfig *config);
```

---

## 4. 实现路线图

### 4.1 短期（1-2 个月）

| 序号 | 任务 | 涉及层级 | 优先级 | 预期成果 |
|:---|:---|:---|:---|:---|
| S-1 | 新增 `STRATEGY_NEURAL_SUGGEST` 策略类型 | 第 4 层 | P0 | proof_engine_enhanced.h 扩展 |
| S-2 | 新增 `PROOF_STEP_NEURAL_SUGGESTION` 步骤类型 | 第 4 层 | P0 | proof.h 扩展 |
| S-3 | 实现辅助构造建议器（启发式规则库） | 第 7 层 | P0 | neural_proof_search.py |
| S-4 | 实现草稿-草图-证明流水线框架 | 第 7 层 | P1 | DraftSketchProvePipeline 类 |
| S-5 | 定义证明引擎 REPL 接口（C 头文件） | 第 4 层 | P1 | proof_session API |
| S-6 | 集成到现有 AI 引擎 | 第 7 层 | P1 | ai_engine.py 扩展 |

### 4.2 中期（3-6 个月）

| 序号 | 任务 | 涉及层级 | 优先级 | 预期成果 |
|:---|:---|:---|:---|:---|
| M-1 | 实现证明引擎 REPL 接口（C 实现） | 第 4 层 | P0 | proof_session.c 完整实现 |
| M-2 | 实现 Python-FFI 桥接（ctypes/cffi） | 第 7 层 | P0 | lv00_proof_session Python 绑定 |
| M-3 | 增加 Lean 4 导出后端 | 第 6 层 | P1 | lv00_export_lean4 函数 |
| M-4 | 构建 IMO 几何题基准测试集 | 全局 | P1 | benchmark IMO-AG-50 兼容 |
| M-5 | 实现神经引导的搜索优先级调度 | 第 3+4 层 | P1 | 搜索效率提升 |
| M-6 | 实现证明策略自动选择器 | 第 7 层 | P2 | 基于问题特征的策略推荐 |
| M-7 | 增加辅助构造的符号验证 | 第 4 层 | P1 | 确保辅助构造的合法性 |

### 4.3 长期（6-12 个月）

| 序号 | 任务 | 涉及层级 | 优先级 | 预期成果 |
|:---|:---|:---|:---|:---|
| L-1 | 实现 Agentic RL 训练反馈循环 | 第 7 层 | P1 | 证明搜索策略自动优化 |
| L-2 | 构建神经推理模型微调管道 | 第 7 层 | P2 | 几何推理专用小模型 |
| L-3 | 实现多模型协作证明（集成 Seed-Prover API） | 第 7 层 | P1 | 外部神经推理 + 内部符号验证 |
| L-4 | 支持 Lean 4 证明的导入与验证 | 第 6 层 | P2 | 双向 Lean 4 互操作 |
| L-5 | 构建 MiniF2F 风格评估框架 | 全局 | P2 | 跨系统可比较评估 |
| L-6 | 实现证明搜索的分布式并行 | 第 3 层 | P2 | 大规模并行证明搜索 |

---

## 5. 附录

### 5.1 关键 API 列表

#### Lv-00 新增 API

| API | 头文件 | 功能说明 |
|:---|:---|:---|
| `lv00_proof_session_create()` | proof.h | 创建证明搜索会话 |
| `lv00_proof_session_get_state_json()` | proof.h | 获取当前证明状态 JSON |
| `lv00_proof_session_submit_suggestion()` | proof.h | 提交 LLM 建议步骤 |
| `lv00_proof_session_destroy()` | proof.h | 销毁证明搜索会话 |
| `lv00_export_lean4()` | proof.h | 导出 Lean 4 代码 |

#### Lv-00 现有 API（与集成相关）

| API | 头文件 | 功能说明 |
|:---|:---|:---|
| `lv00_trace_tree_create()` | proof_engine_enhanced.h | 创建溯源树 |
| `lv00_trace_node_add_child()` | proof_engine_enhanced.h | 添加溯源节点 |
| `lv00_trace_tree_export_dot()` | proof_engine_enhanced.h | 导出 DOT 格式 |
| `lv00_navigator_add_step()` | proof.h | 添加证明步骤 |
| `lv00_unify_check()` | unify.h | 合一检查 |

#### Seed-Prover 相关 API（计划集成）

| API | 来源 | 功能说明 |
|:---|:---|:---|
| Seed-Prover REST API | 字节跳动（计划开放） | 提交数学问题，获取 Lean 4 证明 |
| Lean 4 编译器 | leanprover/lean4 | 验证 Lean 4 证明正确性 |
| Mathlib | leanprover-community/mathlib | Lean 4 数学标准库 |

### 5.2 参考文献

1. ByteDance Seed Team. *Seed Prover 1.5: Formal Mathematical Reasoning via Agentic
   Reinforcement Learning*. 技术报告, 2025 年 12 月.
2. ByteDance Seed Team. *Seed-Geometry: Neural-Symbolic Geometry Theorem Proving*.
   技术报告, 2025. IMO-AG-50 基准上 43/50 的解决率.
3. Trieu H. Trinh, Yuhuai Wu, Quoc V. Le, et al. *Solving olympiad geometry without
   human demonstrations*. Nature, 2024. (AlphaGeometry 2)
4. Stanislas Polu, Jesse Michael Han, et al. *Formal Mathematics Statement Curriculum
   Learning*. arXiv:2202.01344, 2022. (miniF2F)
5. Jason Rute, Yuhuai Wu, Edward Ayers, et al. *The Draft, Sketch, and Prove Pipeline
   for Formal Mathematics*. arXiv:2210.12283, 2022.
6. Leanprover Community. *Lean 4 Programming Language*.
   https://leanprover.github.io/lean4/
7. Leanprover Community. *Mathlib: Lean mathematical library*.
   https://github.com/leanprover-community/mathlib
8. Lv-00 Project. *Lv-00 证明引擎设计文档*.
   `docs/09_proof.md`, `docs/reference/minif2f_neural_theorem_proving.md`

### 5.3 术语对照表

| 英文术语 | 中文术语 | 说明 |
|:---|:---|:---|
| Agentic RL | 智能体强化学习 | 将 LLM 视为自主决策智能体的训练范式 |
| Auxiliary Construction | 辅助构造 | 证明中添加的额外几何元素 |
| Chain-of-Thought (CoT) | 思维链 | 模型逐步推理的过程 |
| Draft-Sketch-Prove | 草稿-草图-证明 | 三级递进式证明生成范式 |
| Formal Verification | 形式化验证 | 通过数学方法证明系统正确性 |
| IMO-AG-50 | IMO 几何基准 | 2000-2024 年 50 道 IMO 几何题 |
| MiniF2F | 小型形式化基准 | 488 个跨系统编码的数学竞赛问题 |
| Neural-Symbolic | 神经符号 | 神经网络与符号推理的混合方法 |
| Proof Search | 证明搜索 | 自动寻找证明的过程 |
| Tactic | 策略 | 形式化证明中的推理步骤 |
| Trace Tree | 溯源树 | 证明依赖关系的树形表示 |
