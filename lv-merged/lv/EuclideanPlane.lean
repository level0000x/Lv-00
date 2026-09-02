/-  EuclideanPlane: 在 Hilbert 平面上的度量定理
   三角形不等式、距离零当且仅当两点重合、距离对称性 -/

import lv.HilbertAxioms

namespace lv.EuclideanPlane

variable {P L : Type} [hp : lv.HilbertAxioms.HilbertPlane P L]

/-- 三角形不等式：两边之和不小于第三边 --/
theorem triangle_inequality (a b c : P) : hp.toMetricSpace.dist a b + hp.toMetricSpace.dist b c ≥ hp.toMetricSpace.dist a c := by
  have h := hp.toMetricSpace.dist_triangle a b c
  linarith

/-- 两点距离为零当且仅当两点重合 --/
theorem dist_zero_iff_eq (a b : P) : hp.toMetricSpace.dist a b = 0 ↔ a = b := by
  constructor
  · intro h
    exact hp.toMetricSpace.eq_of_dist_eq_zero h
  · intro h
    rw [h]
    exact hp.toMetricSpace.dist_self b

/-- 距离的对称性 --/
theorem dist_comm (a b : P) : hp.toMetricSpace.dist a b = hp.toMetricSpace.dist b a :=
  hp.toMetricSpace.dist_comm a b

end lv.EuclideanPlane
