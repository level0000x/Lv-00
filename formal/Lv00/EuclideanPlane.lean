/-  EuclideanPlane: 在 Hilbert 平面上的度量定理
   三角形不等式、距离零当且仅当两点重合、距离对称性 -/

import Lv00.HilbertAxioms

namespace Lv00.EuclideanPlane

open HilbertAxioms
open Lv00.Basic

variable {P L : Type} [hp : HilbertPlane P L]

/-- 将 `HilbertPlane` 投影为 `MetricSpace` 实例 --/
instance : MetricSpace P := hp.toMetricSpace

/-- 三角形不等式：两边之和不小于第三边 --/
theorem triangle_inequality (a b c : P) : dist a b + dist b c ≥ dist a c := by
  have h := dist_triangle a b c
  linarith

/-- 两点距离为零当且仅当两点重合 --/
theorem dist_zero_iff_eq (a b : P) : dist a b = 0 ↔ a = b := by
  constructor
  · intro h
    exact eq_of_dist_eq_zero h
  · intro h
    rw [h]
    exact dist_self a

/-- 距离的对称性 --/
theorem dist_comm (a b : P) : dist a b = dist b a :=
  MetricSpace.dist_comm a b

end Lv00.EuclideanPlane
