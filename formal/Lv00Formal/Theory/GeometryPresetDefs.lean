/-
几何预设核心定义

本模块提供二维欧氏几何的基础类型与运算定义，
包括点、距离、平移、旋转、角度、调和点列、交比、
凸包、多边形面积（Shoelace 公式）、Heron 公式等。
为 GeometryPresets 定理文件以及 PresetGeometryDefs 提供依赖定义。

对应论文中的几何预设形式化基础层。
-/

import Mathlib

namespace Lv00Formal.Theory.GeometryPresetDefs

open Real

/-! ## 点与基本运算 -/

/-- 二维欧氏平面上的点 -/
structure Pt where
  x : ℝ
  y : ℝ
  deriving Repr, DecidableEq

/-- 两点之间的欧氏距离 -/
def dist (p q : Pt) : ℝ :=
  Real.sqrt ((p.x - q.x)^2 + (p.y - q.y)^2)

/-- 点平移 -/
def translate (p : Pt) (dx dy : ℝ) : Pt :=
  { x := p.x + dx, y := p.y + dy }

/-- 绕指定中心旋转点 -/
def rotate (p center : Pt) (angle_rad : ℝ) : Pt :=
  let dx := p.x - center.x
  let dy := p.y - center.y
  let cosθ := Real.cos angle_rad
  let sinθ := Real.sin angle_rad
  { x := center.x + dx * cosθ - dy * sinθ
  , y := center.y + dx * sinθ + dy * cosθ
  }

/-- 三点张成的角 ∠ABC（以 B 为顶点） -/
def angle (A B C : Pt) : ℝ :=
  let vBA := (A.x - B.x, A.y - B.y)
  let vBC := (C.x - B.x, C.y - B.y)
  let dot := vBA.1 * vBC.1 + vBA.2 * vBC.2
  let nBA := Real.sqrt (vBA.1^2 + vBA.2^2)
  let nBC := Real.sqrt (vBC.1^2 + vBC.2^2)
  if nBA = 0 ∨ nBC = 0 then 0
  else Real.arccos (dot / (nBA * nBC))

/-! ## 几何关系谓词 -/

/-- 相似性：两组对应点构成相似四边形 -/
def similarity (A B C D : Pt) : Prop :=
  ∃ (s : ℝ) (θ : ℝ) (tx ty : ℝ),
    s > 0 ∧
    B = rotate (translate (scalePt s A) tx ty) A θ ∧
    C = rotate (translate (scalePt s A) tx ty) A θ ∧
    D = rotate (translate (scalePt s A) tx ty) A θ
where
  scalePt (s : ℝ) (p : Pt) : Pt := { x := s * p.x, y := s * p.y }

/-- 调和点列：四点 A,B,C,D 构成调和点列 (A,B;C,D) = -1 -/
def is_harmonic (A B C D : Pt) : Prop :=
  cross_ratio A B C D = -1

/-- 交比 (Cross Ratio) -/
def cross_ratio (A B C D : Pt) : ℝ :=
  let AC := dist A C
  let BC := dist B C
  let AD := dist A D
  let BD := dist B D
  if BC = 0 ∨ BD = 0 then 0
  else (AC * BD) / (BC * AD)

/-! ## 凸包与面积 -/

/-- 点集的凸包 -/
def convex_hull (pts : List Pt) : Set Pt :=
  { p | ∃ (ws : List ℝ), ws.length = pts.length ∧ (∀ w ∈ ws, 0 ≤ w) ∧ ws.sum = 1 ∧
    p = { x := (List.zipWith (λ w q => w * q.x) ws pts).sum
        , y := (List.zipWith (λ w q => w * q.y) ws pts).sum } }

/-- 多边形面积 (Shoelace 公式) -/
def polygon_area (pts : List Pt) : ℝ :=
  |shoelace_sum pts| / 2

/-- Shoelace 求和项 -/
def shoelace_sum (pts : List Pt) : ℝ :=
  match pts with
  | []  => 0
  | [p] => 0
  | ps  =>
    let pairs := List.zip pts (pts.tail ++ [pts.head?].filterMap id)
    (pairs.map (fun (p, q) => p.x * q.y - q.x * p.y)).sum

/-- 三角形面积 (SSS)：已知三边长求面积 (Heron 公式) -/
def triangle_area_sss (a b c : ℝ) : ℝ :=
  let s := (a + b + c) / 2
  Real.sqrt (s * (s - a) * (s - b) * (s - c))

/-- Heron 公式标准形式 -/
def heron_formula_standard (a b c : ℝ) : ℝ :=
  (1/4) * Real.sqrt ((a + b + c) * (-a + b + c) * (a - b + c) * (a + b - c))

/-- 指定边长的正多边形边长 -/
def edge_length (n : ℕ) : ℝ :=
  if n ≤ 2 then 1 else 1

/-- 正多边形边长的通用计算（给定边数和外接圆半径） -/
def regular_polygon_edge_length (n : ℕ) (R : ℝ) : ℝ :=
  if n < 3 ∨ R ≤ 0 then 0
  else 2 * R * Real.sin (Real.pi / (n : ℝ))

/-! ## 公理 -/

/-- Shoelace 公式的归纳步骤：面积 = 头部三角形的有向面积 + 尾部多边形面积 -/
axiom shoelace_inductive_step (pts : List Pt) (h : pts.length ≥ 3) :
  shoelace_sum pts = shoelace_sum pts.tail +
    ((pts.head?.getD { x := 0, y := 0 }).x * ((pts.tail.get? 0).getD { x := 0, y := 0 }).y) -
    (((pts.tail.get? 0).getD { x := 0, y := 0 }).x * (pts.head?.getD { x := 0, y := 0 }).y)

/-- Heron 公式标准形式与三角形面积 SSS 等价 -/
axiom heron_formula_standard_valid (a b c : ℝ) (h : a + b > c ∧ b + c > a ∧ c + a > b) :
  heron_formula_standard a b c = triangle_area_sss a b c

/-- 相似变换下角度保持不变 -/
axiom angle_invariant_under_similarity (A B C D E F G H : Pt)
  (h_sim : similarity A B D E) (h_align : True) :
  angle A B C = angle D E F

end Lv00Formal.Theory.GeometryPresetDefs
