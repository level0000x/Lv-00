/-  Hilbert 关联公理 I1-I3
   I1: 过任意两点存在一条直线
   I2: 过任意两点至多存在一条直线
   I3: 每条直线上至少有两个点；存在至少三个不共线的点 -/

-- [QA] Parallel Hilbert formalization. lv00-formal/Classical/Hilbert/ provides
--      the full axiomatic treatment with proofs; this file provides the typeclass
--      abstraction layer. Do NOT merge; they serve different architectural roles.

import Mathlib

namespace Lv00.Incidence

/-- Hilbert 关联公理 --/
class Incidence (Point Line : Type) where
  lies_on : Point → Line → Prop
  I1 : ∀ (A B : Point), A ≠ B → ∃ (l : Line), lies_on A l ∧ lies_on B l
  I2 : ∀ (A B : Point) (l m : Line), A ≠ B →
    lies_on A l → lies_on B l → lies_on A m → lies_on B m → l = m
  I3 : ∀ (l : Line), ∃ (A B C : Point),
    lies_on A l ∧ lies_on B l ∧ A ≠ B ∧ ¬ lies_on C l

end Lv00.Incidence
