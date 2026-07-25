/-
Lv-00 formal: ProofStrategy — 证明策略与自动化 (v1.2 R1)
=========================================================

本文件定义 Lv-00 形式化验证体系的证明策略层，
将证据系统、约束求解、公理发现等机制统一为可组合的证明策略。

核心内容：
  1. Goal — 证明目标（要证明的约束或公式）
  2. Tactic — 证明策略原语（等价于 Evidence.ProofStep 的语义版本）
  3. Strategy — 策略组合子（顺序、选择、重复、条件）
  4. ForwardChain — 前向链策略
  5. BackwardChain — 后向链策略
  6. StrategyCorrectness — 策略正确性定理
  7. Connection to AxiomDiscovery — 连接公理发现系统

本层填补了"核心约束求解/验证"与"策略自动化"之间的架构缺口。
-/

import Mathlib
import lvFormal.Theory.Evidence
import lvFormal.Theory.IR
import lvFormal.Theory.AxiomDiscoveryTheory

namespace lvFormal.Theory.ProofStrategy

open lvFormal.Theory.Evidence
open lvFormal.Theory.IR
open lvFormal.Theory.AxiomDiscoveryTheory

/-! ===============================================================
   第一部分：证明目标（Goal）
   
   一个证明目标是要证明一个约束图（或子图）在给定已验证
   约束集下是可满足的。
   =============================================================== -/

/-- 证明目标：在当前已证明的约束集合下，需要证明目标约束可满足。 -/
structure Goal where
  /-- 已证明的约束集（当前的"已知事实"） -/
  proved : ConstraintGraph
  /-- 待证明的目标约束集 -/
  target : ConstraintGraph
  /-- 原始约束图（用于最终 qed 检查） -/
  original : ConstraintGraph
  deriving Repr

/-- 从约束图创建初始目标（无任何先验知识）。 -/
def goalFromGraph (g : ConstraintGraph) : Goal :=
  { proved := []
  , target := g
  , original := g
  }

/-- 目标已解决：当目标约束集为空（所有目标都已证明） -/
def goal_solved (g : Goal) : Bool :=
  g.target.isEmpty

/-- 目标的部分求解：将已证明部分从 target 移动到 proved。 -/
def goal_advance (g : Goal) (proved_prefix : ConstraintGraph) : Goal :=
  { proved := g.proved ++ proved_prefix
  , target := g.target.drop proved_prefix.length
  , original := g.original
  }

/-! ===============================================================
   第二部分：策略原语（Tactic）
   
   每个 Tactic 是一个从 Goal 到新 Goal 的转换函数。
   如果转换成功，返回 some (new_goal, proof_steps)。
   如果失败，返回 none。
   =============================================================== -/

/-- 策略执行结果：新目标 + 生成的证明步骤列表。 -/
structure TacticResult where
  newGoal : Goal
  steps   : ProofTrace
  deriving Repr

/-- 策略类型：Goal → Option TacticResult
    
    每个策略接收一个目标，尝试推进证明。
    成功时返回新目标和产生的证明步骤；失败时返回 none。 -/
abbrev Tactic := Goal → Option TacticResult

/-- 策略库：命名的策略函数集合。 -/
structure TacticLibrary where
  /-- 假设策略：将 target 中的第一个约束视为假设（直接标记为已