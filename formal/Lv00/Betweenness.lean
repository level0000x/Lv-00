/-  Hilbert 序公理 B1-B3
   B1: 若 B 介于 A 和 C 之间，则 B 也介于 C 和 A 之间
   B2: 对任意两点 A ≠ C，在线段 AC 的延长线上存在点 B 使得 C 介于 A 和 B 之间
   B3: 共线的三点中至多有一点在另两点之间 -/

import Mathlib

namespace Lv00.Betweenness

/-- Hilbert 序公理 --/
class Betweenness (Point : Type) where
  between : Point → Point → Point → Prop
  B1 : ∀ (A B C : Point), between A B C → between C B A
  B2 : ∀ (A B : Point), A ≠ B → ∃ (C : Point), between A B C
  B3 : ∀ (A B C : Point), between A B C → ¬ (between B A C) ∧ ¬ (between A C B)

end Lv00.Betweenness
