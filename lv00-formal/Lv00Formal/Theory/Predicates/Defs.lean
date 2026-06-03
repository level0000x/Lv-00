/-
Lv-00 自有理论核心：六条本原谓词

本文件定义 Lv-00 内部关系接口。它们是进入约束图、推理规则和归一化内核的
稳定谓词，不从 Hilbert 或传统欧氏公理继承。
-/

import Lv00Formal.Theory.Ontology.Defs

namespace Lv00Formal
namespace Theory
namespace Predicates

open Ontology

/-- 六类本原谓词。论文初稿明确提到关联、之间、相交、包含、连接等关系；
    第六类保留为等价/同一关系，用于归一化与代表元合并。 -/
inductive PrimPredKind where
  | incidence
  | between
  | intersection
  | containment
  | connection
  | equivalence
  deriving DecidableEq, Repr

/-- 本原谓词实例：谓词类别加参数对象列表。 -/
structure PrimPred where
  kind : PrimPredKind
  args : List LvObj
  deriving Repr

/-- 谓词参数个数约束。 -/
def arity : PrimPredKind → Nat
  | .incidence => 2
  | .between => 3
  | .intersection => 3
  | .containment => 2
  | .connection => 2
  | .equivalence => 2

/-- 本原谓词良构性：参数数量正确，且所有参数都是良构对象。 -/
def WellFormedPred (p : PrimPred) : Prop :=
  p.args.length = arity p.kind ∧ ∀ o ∈ p.args, WellFormedObj o

/-- 语义封闭性：Lv-00 基础关系只能通过六类本原谓词进入核心。 -/
theorem predicate_closed (p : PrimPred) :
    p.kind = PrimPredKind.incidence ∨
    p.kind = PrimPredKind.between ∨
    p.kind = PrimPredKind.intersection ∨
    p.kind = PrimPredKind.containment ∨
    p.kind = PrimPredKind.connection ∨
    p.kind = PrimPredKind.equivalence := by
  cases p.kind <;> simp

end Predicates
end Theory
end Lv00Formal
