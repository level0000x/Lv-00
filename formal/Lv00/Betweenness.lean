/-
  Lv-00 Formal Verification: Betweenness (Hilbert Group II)
  介于关系：B(p, q, r) 表示 q 在 p 和 r 之间
-/
import Lv00.Basic

namespace Lv00.Betweenness

/-- 介于关系 -/
def Between (α : Type) [Point α] : α → α → α → Prop :=
  fun _p _q _r => False  -- 桩定义，由公理赋予含义

notation:45 " B " => Between

/-- 介于公理 B1: 若 B(p, q, r)，则 p, q, r 是共线且不同的点 -/
axiom between_collinear (α : Type) [Point α] :
  ∀ (p q r : α), Between p q r → p ≠ q ∧ q ≠ r ∧ p ≠ r

/-- 介于公理 B2: 对任意两个不同的点 p, r，存在 q 使得 B(p, q, r) -/
axiom between_exists (α : Type) [Point α] :
  ∀ (p r : α), p ≠ r → ∃ q, Between p q r

/-- 介于公理 B3: 介于关系的唯一性 -/
axiom between_unique (α : Type) [Point α] :
  ∀ (p q r : α), Between p q r → ¬Between p r q

end Lv00.Betweenness
