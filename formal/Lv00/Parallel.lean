/-
  Lv-00 Formal Verification: Parallel Axiom (Hilbert Group IV)
  平行公理：通过一点恰有一条直线与给定直线不相交
-/
import Lv00.Basic
import Lv00.Incidence

namespace Lv00.Parallel

/-- 平行关系 -/
def IsParallel (α : Type) [Line α] : α → α → Prop :=
  fun l₁ l₂ => l₁ ≠ l₂ ∧ ∀ (p : α), ¬(Line.lies_on p l₁ ∧ Line.lies_on p l₂)

notation:50 " ∥ " => IsParallel

/-- 平行公理 (Playfair 形式): 通过直线外一点，恰有一条平行线 -/
axiom playfair (α : Type) [Point α] [Line α] :
  ∀ (p : α) (l : α),
    ¬Line.lies_on p l →
    ∃! m, ¬Line.lies_on p m ∧ m ∥ l

end Lv00.Parallel
