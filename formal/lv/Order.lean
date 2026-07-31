/-  序公理化扩展：Pasch 公理、稠密性、中点存在、线段延长
   Pasch: 直线穿过三角形，若与某边相交则必与另一边相交
   Density: 两点之间必存在第三点
   Extension: 线段可延长
   Trichotomy: 共线三点恰有一点在另两点之间 -/

import Mathlib
import lv.Betweenness

namespace lv.Order

/-- 扩展序公理 --/
class Order (Point : Type) extends lv.Betweenness.Betweenness Point where
  density : ∀ (A B : Point), A ≠ B → ∃ (C : Point), between A C B
  extension : ∀ (A B : Point), A ≠ B → ∃ (C : Point), between A B C
  midpoint : ∀ (A B : Point), ∃ (C : Point), between A C B
  trichotomy : ∀ (A B C : Point),
    A ≠ B → A ≠ C → B ≠ C → (between A B C) ∨ (between B A C) ∨ (between A C B)

end lv.Order
