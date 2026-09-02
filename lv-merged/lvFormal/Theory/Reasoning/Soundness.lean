/-
Lv-00 自有理论核心：推理可靠性接口

多策略推理层应证明：若从良构约束图和良构规则出发，则每一步推理产生的结论
仍是 Lv-00 内部合法谓词，并可被约束图吸收。
-/

import lvFormal.Theory.Axioms.Primitive
import lvFormal.Theory.Constraint.Graph

namespace lvFormal
namespace Theory
namespace Reasoning

open Axioms
open Constraint
open Predicates

/-- 单步推理。 -/
structure InferenceStep where
  rule : BaseAxiomRule
  used : List PrimPred
  derived : PrimPred
  deriving Repr

/-- 推理步良构性。 -/
def WellFormedStep (s : InferenceStep) : Prop :=
  WellFormedRule s.rule ∧
  (∀ p ∈ s.used, WellFormedPred p) ∧
  WellFormedPred s.derived

/-- 规则应用的可靠性接口：良构规则只能推出良构结论。 -/
theorem rule_application_sound {r : BaseAxiomRule} (h : WellFormedRule r) :
    WellFormedPred r.conclusion := by
  exact h.2

/-- 多策略推理可靠性的目标命题。 -/
def ReasoningSoundness : Prop :=
  ∀ s : InferenceStep, WellFormedStep s → WellFormedPred s.derived

/-- 目前的可靠性骨架：由推理步良构性直接得到结论良构。 -/
theorem reasoning_soundness_skeleton : ReasoningSoundness := by
  intro s hs
  exact hs.2.2

end Reasoning
end Theory
end lvFormal
