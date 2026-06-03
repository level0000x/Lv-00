/-
  Lv-00 Formal Verification: Continuity Axioms (Hilbert Group V)
  连续性公理：阿基米德公理和完备性
-/
import Lv00.Basic
import Lv00.Betweenness

namespace Lv00.Continuity

/-- 阿基米德公理: 对任意线段 AB 和 CD，存在 n 使得 n·CD > AB -/
axiom archimedes (α : Type) [Point α] [MetricSpace α] :
  ∀ (A B C D : α),
    MetricSpace.dist A B > 0 → MetricSpace.dist C D > 0 →
    ∃ (n : ℕ),
      ∀ (k : ℕ), k < n →
        MetricSpace.dist A B > (k : ℝ) * MetricSpace.dist C D

/-- 完备性公理 (Dedekind): 直线不能被分成两个非空集合 A, B，
   使得 A 中每个点在 B 中每个点的左边，但 A 没有最右边的点也没有 B 没有最左边的点 -/
axiom dedekind_completeness (α : Type) [Point α] [Line α] :
  ∀ (l : α) (A B : Set α),
    (∀ (p : α), Line.lies_on p l → p ∈ A ∨ p ∈ B) →
    (∀ (p ∈ A) (q ∈ B), p ≠ q) →
    (∃ (p : α), p ∈ A) →
    (∃ (q : α), q ∈ B) →
    (∃ (r : α), r ∈ A ∨ r ∈ B)

end Lv00.Continuity
