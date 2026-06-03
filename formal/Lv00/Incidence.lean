/-
  Lv-00 Formal Verification: Incidence Axioms (Hilbert Group I)
  关联公理：点-线-面的关联关系
-/
import Lv00.Basic

namespace Lv00.Incidence

/-- 关联公理 I1: 任意两个不同的点确定唯一一条直线 -/
axiom unique_line_through (α : Type) [Point α] [Line α] :
  ∀ (p q : α), p ≠ q →
    ∃! l : α, Line.lies_on p l ∧ Line.lies_on q l

/-- 关联公理 I2: 直线上至少存在两个不同的点 -/
axiom line_has_two_points (α : Type) [Line α] :
  ∀ (l : α), ∃ (p q : α), p ≠ q ∧ Line.lies_on p l ∧ Line.lies_on q l

/-- 关联公理 I3: 存在不共线的三个点 -/
axiom exists_noncollinear_points (α : Type) [Point α] [Line α] :
  ∃ (p q r : α),
    p ≠ q ∧ q ≠ r ∧ p ≠ r ∧
    ∀ (l : α), ¬(Line.lies_on p l ∧ Line.lies_on q l ∧ Line.lies_on r l)

end Lv00.Incidence
