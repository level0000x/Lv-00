/-
  Lv-00 Formal Verification: Order Axioms
  序公理：线上的点的顺序关系
-/
import Lv00.Basic
import Lv00.Betweenness

namespace Lv00.Order

/-- 线上的点可以线性排序 -/
axiom line_order (α : Type) [Point α] [Line α] :
  ∀ (l : α) (p q r : α),
    Line.lies_on p l → Line.lies_on q l → Line.lies_on r l →
    Betweenness.Between p q r ∨ Betweenness.Between q r p ∨ Betweenness.Between r p q

end Lv00.Order
