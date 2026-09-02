/-  Hilbert 合同公理 C1-C4 以及 SAS 全等定理
   C1: 线段 AB 全等于自身 BA
   C2: 全等关系的传递性
   C3: 线段可加性（存在全等线段）
   C4: 角的全等关系
   C5_SAS: 边-角-边三角形全等准则 -/

-- [QA] Parallel Hilbert formalization. lv-formal/Classical/Hilbert/ provides
--      the full axiomatic treatment with proofs; this file provides the typeclass
--      abstraction layer. Do NOT merge; they serve different architectural roles.

import Mathlib
import lv.Basic

namespace lv.Congruence

/-- Hilbert 合同公理 --/
class HCongruence (Point : Type) where
  segCong : Point → Point → Point → Point → Prop
  angCong : Point → Point → Point → Point → Point → Point → Prop
  C1 : ∀ (A B : Point), segCong A B B A
  C2 : ∀ (A B C D E F : Point),
    segCong A B C D → segCong A B E F → segCong C D E F
  C3 : ∀ (A B C A' B' C' : Point),
    segCong A B A' B' → segCong B C B' C' → segCong A C A' C'
  C4_reflexive : ∀ (A O B : Point), angCong A O B A O B
  C4_symmetric : ∀ (A O B A' O' B' : Point),
    angCong A O B A' O' B' → angCong A' O' B' A O B
  C5_SAS : ∀ (A B C A' B' C' : Point),
    segCong A B A' B' → segCong B C B' C' →
    angCong A B C A' B' C' → segCong A C A' C'

end lv.Congruence
