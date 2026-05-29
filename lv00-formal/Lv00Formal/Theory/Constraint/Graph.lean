/-
Lv-00 自有理论核心：约束图与四态相容性

约束图是 Lv-00 将对象、谓词和推理结果组织为可归一化结构的核心载体。
归一化函数的具体实现在 Normalization.lean 中提供。
-/

import Lv00Formal.Theory.Predicates.Defs

namespace Lv00Formal
namespace Theory
namespace Constraint

open Ontology
open Predicates

/-- 约束状态：相容、矛盾、欠约束、过约束。 -/
inductive ConstraintStatus where
  | consistent
  | contradictory
  | underconstrained
  | overconstrained
  deriving DecidableEq, Repr

/-- Lv-00 约束图：节点为三元本体对象，边/关系为六类本原谓词。 -/
structure ConstraintGraph where
  nodes : List LvObj
  constraints : List PrimPred
  deriving Repr

/-- 约束图良构性。 -/
def WellFormedGraph (g : ConstraintGraph) : Prop :=
  (∀ o ∈ g.nodes, WellFormedObj o) ∧
  (∀ c ∈ g.constraints, WellFormedPred c)

/-- 归一化幂等性是 Lv-00 自有体系的关键元理论目标。
    具体实现和证明在 Normalization.lean 中提供。 -/
def NormalizationIdempotent (normalize : ConstraintGraph → ConstraintGraph) : Prop :=
  ∀ g : ConstraintGraph, normalize (normalize g) = normalize g

/-- 归一化保持良构性。
    具体实现和证明在 Normalization.lean 中提供。 -/
def NormalizationPreservesWellFormedness (normalize : ConstraintGraph → ConstraintGraph) : Prop :=
  ∀ g : ConstraintGraph, WellFormedGraph g → WellFormedGraph (normalize g)

/-- 约束图状态判定接口。 -/
axiom checkStatus : ConstraintGraph → ConstraintStatus

end Constraint
end Theory
end Lv00Formal
