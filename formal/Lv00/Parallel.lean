/-  Playfair 平行公理
   过直线外一点至多有一条直线与该直线平行
   两条直线平行当且仅当它们不相交 -/

-- [QA] Parallel Hilbert formalization. lv00-formal/Classical/Hilbert/ provides
--      the full axiomatic treatment with proofs; this file provides the typeclass
--      abstraction layer. Do NOT merge; they serve different architectural roles.

import Mathlib
import Lv00.Incidence

namespace Lv00.Parallel

/-- 平行关系定义 --/
def parallel (Point Line : Type) [Incidence Point Line] (l m : Line) : Prop :=
  ¬ ∃ (P : Point), Incidence.lies_on P l ∧ Incidence.lies_on P m

/-- Playfair 公理：过直线外一点至多有一条平行线 --/
class Parallel (Point Line : Type) extends Incidence Point Line where
  playfair : ∀ (P : Point) (l : Line),
    ¬ lies_on P l → ∀ (m n : Line),
    parallel Point Line l m → parallel Point Line l n →
    lies_on P m → lies_on P n → m = n

end Lv00.Parallel
