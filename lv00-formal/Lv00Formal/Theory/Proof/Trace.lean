/-
Lv-00 自有理论核心：证明对象与可追溯性

证明不是一次性结论，而是由 ProofStep、依赖关系和状态快照组成的可复核对象。
-/

import Lv00Formal.Theory.Reasoning.Soundness

namespace Lv00Formal
namespace Theory
namespace Proof

open Reasoning
open Constraint

/-- 证明步骤编号。 -/
abbrev StepId := Nat

/-- 证明步骤。 -/
structure ProofStep where
  id : StepId
  inference : InferenceStep
  deps : List StepId
  beforeStatus : ConstraintStatus
  afterStatus : ConstraintStatus
  deriving Repr

/-- Lv-00 证明对象。 -/
structure ProofObject where
  steps : List ProofStep
  finalStatus : ConstraintStatus
  deriving Repr

/-- 可追溯性接口：每个依赖编号都应指向已有步骤。 -/
def Traceable (p : ProofObject) : Prop :=
  ∀ s ∈ p.steps, ∀ d ∈ s.deps, ∃ t ∈ p.steps, t.id = d

/-- 证明可靠性接口：每一步推理都良构。 -/
def ProofWellFormed (p : ProofObject) : Prop :=
  ∀ s ∈ p.steps, WellFormedStep s.inference

end Proof
end Theory
end Lv00Formal
