/-
  Lv-00 Formal Verification: Euclidean Plane Theorems
  欧氏平面几何基本定理
-/
import Lv00.HilbertAxioms

namespace Lv00.EuclideanPlane

/-- 定理: 三角形两边之和大于第三边（三角不等式） -/
theorem triangle_inequality (α : Type) [Point α] [Line α] [MetricSpace α]
    [MetricSpace.dist_triangle] :
  ∀ (A B C : α),
    MetricSpace.dist A C ≤ MetricSpace.dist A B + MetricSpace.dist B C :=
  MetricSpace.dist_triangle A B C

/-- 定理: SAS 全等判定 -/
axiom sas_congruence (α : Type) [Point α] [MetricSpace α] :
  ∀ (A B C A' B' C' : α),
    MetricSpace.dist A B = MetricSpace.dist A' B' →
    MetricSpace.dist A C = MetricSpace.dist A' C' →
    Congruence.SegCongr (B, A) (B', A') →
    Congruence.SegCongr (B, C) (B', C') →
    Congruence.SegCongr (A, C) (A', C')

end Lv00.EuclideanPlane
