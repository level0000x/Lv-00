/-
  Lv-00 Formal Verification: Order Axioms (Extended)
  序公理扩展：Pasch 公理、直线上的稠密性
-/
import Lv00.Basic
import Lv00.Betweenness
import Lv00.Incidence

namespace Lv00.Order

/-- Pasch 公理: 若直线 l 与三角形 ABC 的一边相交，
   则 l 必与三角形的另一边或第三边相交 -/
axiom pasch (α : Type) [Point α] [Line α] :
  ∀ (A B C : α) (l : α),
    A ≠ B → B ≠ C → A ≠ C →
    ¬(∃ m, Line.lies_on A m ∧ Line.lies_on B m ∧ Line.lies_on C m) →
    (∃ P, Line.lies_on P l ∧ Betweenness.Between A P B) →
    (∃ Q, Line.lies_on Q l ∧
      (Betweenness.Between B Q C ∨ Betweenness.Between A Q C))

/-- 稠密性: 线上任意两点之间存在另一点 -/
axiom line_density (α : Type) [Point α] [Line α] :
  ∀ (l : α) (P Q : α),
    Line.lies_on P l → Line.lies_on Q l → P ≠ Q →
    ∃ R, Line.lies_on R l ∧ R ≠ P ∧ R ≠ Q ∧
      (Betweenness.Between P R Q ∨ Betweenness.Between Q R P)

/-- 线段中点存在性 -/
axiom midpoint_exists (α : Type) [Point α] [MetricSpace α] :
  ∀ (A B : α), A ≠ B →
    ∃ M, Betweenness.Between A M B ∧
    MetricSpace.dist A M = MetricSpace.dist M B

/-- 线段延长公理: 对任意线段 AB 和射线 AC，存在点 D 使得 B 在 A 和 D 之间且 AD = AC -/
axiom segment_extension (α : Type) [Point α] [MetricSpace α] :
  ∀ (A B C : α),
    A ≠ B → A ≠ C →
    ∃ D, Betweenness.Between A B D ∧
    MetricSpace.dist A D = MetricSpace.dist A C

end Lv00.Order
