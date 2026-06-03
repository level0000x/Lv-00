/-
  Lv-00 Formal Verification: Congruence Axioms (Hilbert Group III)
  全等公理：线段和角的全等关系
-/
import Lv00.Basic
import Lv00.Betweenness

namespace Lv00.Congruence

/-- 线段全等 -/
def SegCongr (α : Type) [Point α] [MetricSpace α] : α × α → α × α → Prop :=
  fun (p₁, p₂) (q₁, q₂) => MetricSpace.dist p₁ p₂ = MetricSpace.dist q₁ q₂

notation:50 " ≅ " => SegCongr

/-- 全等公理 C1: 自反性 -/
axiom seg_congr_refl (α : Type) [Point α] [MetricSpace α] :
  ∀ (p q : α), (p, q) ≅ (p, q)

/-- 全等公理 C2: 对称性 -/
axiom seg_congr_sym (α : Type) [Point α] [MetricSpace α] :
  ∀ (p q r s : α), (p, q) ≅ (r, s) → (r, s) ≅ (p, q)

/-- 全等公理 C3: 传递性 -/
axiom seg_congr_trans (α : Type) [Point α] [MetricSpace α] :
  ∀ (p q r s t u : α), (p, q) ≅ (r, s) → (r, s) ≅ (t, u) → (p, q) ≅ (t, u)

/-- 全等公理 C4: 线段加法 -/
axiom seg_congr_add (α : Type) [Point α] [MetricSpace α] :
  ∀ (p q r s t : α),
    Betweenness.Between p q r →
    Betweenness.Between s t u →
    (p, q) ≅ (s, t) → (q, r) ≅ (t, u) → (p, r) ≅ (s, u)

end Lv00.Congruence
