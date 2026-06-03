/-
  Lv-00 Formal Verification: Basic Definitions
  基础几何定义：点、线、平面、集合论基础
-/
namespace Lv00

/-- 几何点（抽象类型，由公理定义行为） -/
class Point (α : Type) where
  /-- 任意两个不同的点确定一条直线 -/
  line_through : α → α → Prop
  line_through_ne : ∀ (p q : α), p ≠ q → ∃ l, line_through p l ∧ line_through q l

/-- 直线 -/
class Line (α : Type) where
  /-- 直线上的点 -/
  lies_on : α → α → Prop
  /-- 直线包含至少两个不同的点 -/
  exists_two_points : ∃ (p q : α), p ≠ q ∧ lies_on p α ∧ lies_on q α

/-- 平面 -/
class Plane (α : Type) where
  /-- 平面上的点 -/
  point_in_plane : α → Prop
  /-- 平面包含至少三个不共线的点 -/
  exists_noncollinear : ∃ (p q r : α),
    p ≠ q ∧ q ≠ r ∧ p ≠ r ∧
    ¬∃ l, Line.line_through p l ∧ Line.line_through q l ∧ Line.line_through r l

/-- 距离度量 -/
class MetricSpace (α : Type) where
  dist : α → α → ℝ
  dist_nonneg : ∀ (p q : α), 0 ≤ dist p q
  dist_self : ∀ (p : α), dist p p = 0
  dist_comm : ∀ (p q : α), dist p q = dist q p
  dist_triangle : ∀ (p q r : α), dist p r ≤ dist p q + dist q r

/-- 全等关系 -/
class Congruence (α : Type) where
  congr : α → α → α → α → Prop
  congr_refl : ∀ (p q : α), congr p q p q
  congr_sym : ∀ (p q r s : α), congr p q r s → congr r s p q
  congr_trans : ∀ (a b c d e f : α), congr a b c d → congr c d e f → congr a b e f

end Lv00
