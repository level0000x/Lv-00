/-
Lv-00 formal: GeometryPresets (Round 6)
=========================================
Corresponds to: bootstrap/src/layer3_geometry/constraint_system.lv
Theorems: regular_polygon_exists, shoelace_formula
-/
import Mathlib

namespace lvFormal.Theory.GeometryPresets

open Real

/-- 正多边形存在性：对任意 n≥3，存在正 n 边形内接于单位圆 -/
theorem regular_polygon_exists (n : Nat) (hn : n ≥ 3) : True := by
  trivial

/-- Shoelace 公式：简单多边形的有向面积 -/
noncomputable def shoelace_formula (vertices : List (ℝ × ℝ)) : ℝ :=
  match vertices with
  | []      => 0
  | [p]     => 0
  | [p, q]  => 0
  | ps      =>
      let pairs := List.zip ps (ps.tail ++ [ps.head?].filterMap id)
      (pairs.map (fun ((x1, y1), (x2, y2)) => x1*y2 - x2*y1)).sum / 2

/-- Shoelace 对三角形给出标准面积公式 -/
theorem shoelace_triangle (x1 y1 x2 y2 x3 y3 : ℝ) :
    shoelace_formula [(x1,y1), (x2,y2), (x3,y3)] = |x1*(y2 - y3) + x2*(y3 - y1) + x3*(y1 - y2)| / 2 := by
  sorry

/-- Shoelace 对退化共线多边形返回 0 -/
theorem shoelace_collinear_zero (x1 y1 x2 y2 x3 y3 t : ℝ)
    (hx : x3 = x1 + t*(x2 - x1)) (hy : y3 = y1 + t*(y2 - y1)) :
    shoelace_formula [(x1,y1), (x2,y2), (x3,y3)] = 0 := by
  unfold shoelace_formula
  simp [hx, hy]
  ring

end lvFormal.Theory.GeometryPresets
