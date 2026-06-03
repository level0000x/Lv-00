/-
  Lv-00 Formal Verification: Hilbert Axiom System
  希尔伯特公理体系总览，整合所有 5 组公理
-/
import Lv00.Incidence
import Lv00.Betweenness
import Lv00.Congruence
import Lv00.Parallel
import Lv00.Continuity
import Lv00.Order

namespace Lv00.HilbertAxioms

/-- 希尔伯特公理体系包含 5 组公理 -/
structure HilbertSystem (α : Type) [Point α] [Line α] [MetricSpace α] where
  incidence : Incidence.unique_line_through α
  between : Betweenness.between_exists α
  congruence : Congruence.seg_congr_refl α
  parallel : Parallel.playfair α
  continuity : Continuity.archimedes α

end Lv00.HilbertAxioms
