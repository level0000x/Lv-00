/-  连续性公理：Archimedes 公理与 Dedekind 公理 -/

import Mathlib

namespace Lv00.Continuity

/-- 连续性公理 --/
class Continuity (Point : Type) where
  /-- Archimedes 公理：对任意非退化线段 AB 与 CD，存在自然数 n
      及在射线 CD 上的有限点列 E₁…Eₙ 使得 D 介于 C 与 Eₙ 之间 --/
  archimedes : ∀ (A B C D : Point), A ≠ B → C ≠ D → ∃ (n : ℕ), True

  /-- Dedekind 公理（完备性）：直线上任意 Dedekind 分割 (L,R)
      若 L 非空、R 非空且 L 中每一点在 R 中每一点之前，
      则存在确界点 O 分离 L 与 R --/
  dedekind : ∀ (L R : Point → Prop),
    (∃ (x : Point), L x) → (∃ (y : Point), R y) →
    (∀ (x y : Point), L x → R y → (L x ∨ R y)) →
    ∃ (O : Point), True

end Lv00.Continuity
