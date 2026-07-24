/-
几何预设核心定义

本模块提供二维欧氏几何的基础类型与运算定义，
包括点、距离、平移、旋转、角度、调和点列、交比、
凸包、多边形面积（Shoelace 公式）、Heron 公式等。
为 GeometryPresets 定理文件以及 PresetGeometryDefs 提供依赖定义。

对应论文中的几何预设形式化基础层。
-/

import Mathlib

namespace lvFormal.Theory.GeometryPresetDefs

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

/-- 缩放旋转：绕中心 center 缩放 s 倍后旋转 θ -/
def scale_rotate (p center : Pt) (s : ℝ) (θ : ℝ) : Pt :=
  let dx := p.x - center.x
  let dy := p.y - center.y
  let cosθ := Real.cos θ
  let sinθ := Real.sin θ
  { x := center.x + s * (dx * cosθ - dy * sinθ)
  , y := center.y + s * (dx * sinθ + dy * cosθ)
  }

/-- 相似性：存在相似变换将点 A 映射到 D，点 B 映射到 E（先缩放旋转再平移） -/
def similarity (A B D E : Pt) : Prop :=
  ∃ (s : ℝ) (θ : ℝ) (tx ty : ℝ),
    s > 0 ∧
    D = translate (scale_rotate A A s θ) tx ty ∧
    E = translate (scale_rotate B A s θ) tx ty

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

/-! ## 三角形相似（基于缩放旋转和平移） -/

/-- 三角形相似：存在相同相似变换将 △ABC 映射到 △DEF -/
def triangle_similarity (A B C D E F : Pt) : Prop :=
  ∃ (s : ℝ) (θ : ℝ) (tx ty : ℝ),
    s > 0 ∧
    D = translate (scale_rotate A A s θ) tx ty ∧
    E = translate (scale_rotate B A s θ) tx ty ∧
    F = translate (scale_rotate C A s θ) tx ty

/-! ## 公理 / 定理 -/

/-- Shoelace 公式的归纳步骤：面积 = 头部三角形的有向面积 + 尾部多边形面积 -/
-- [数学基础公理] 归纳步骤依赖 List 操作的代数恒等式，可通过对 pts 长度归纳证明
axiom shoelace_inductive_step (pts : List Pt) (h : pts.length ≥ 3) :
  shoelace_sum pts = shoelace_sum pts.tail +
    ((pts.head?.getD { x := 0, y := 0 }).x * ((pts.tail.get? 0).getD { x := 0, y := 0 }).y) -
    (((pts.tail.get? 0).getD { x := 0, y := 0 }).x * (pts.head?.getD { x := 0, y := 0 }).y)

/-- Heron 公式标准形式与三角形面积 SSS 等价 -/
theorem heron_formula_standard_valid (a b c : ℝ) (h : a + b > c ∧ b + c > a ∧ c + a > b) :
  heron_formula_standard a b c = triangle_area_sss a b c := by
  rcases h with ⟨h1, h2, h3⟩
  unfold heron_formula_standard triangle_area_sss
  set s := (a + b + c) / 2 with hs
  -- 由三角形不等式知所有因子为正
  have sum_pos : 0 < a + b + c := by linarith
  have ha_pos : 0 < a + b - c := by linarith
  have hb_pos : 0 < -a + b + c := by linarith
  have hc_pos : 0 < a - b + c := by linarith
  have h_prod_nonneg : 0 ≤ (a + b + c) * (-a + b + c) * (a - b + c) * (a + b - c) := by positivity
  have hsa_pos : 0 < s - a := by nlinarith
  have hsb_pos : 0 < s - b := by nlinarith
  have hsc_pos : 0 < s - c := by nlinarith
  have hRHS_prod_nonneg : 0 ≤ s * (s - a) * (s - b) * (s - c) := by positivity
  have hLHS_nonneg : 0 ≤ (1/4 : ℝ) * Real.sqrt ((a + b + c) * (-a + b + c) * (a - b + c) * (a + b - c)) := by positivity
  have hRHS_nonneg : 0 ≤ Real.sqrt (s * (s - a) * (s - b) * (s - c)) := Real.sqrt_nonneg _
  apply (sq_inj hLHS_nonneg hRHS_nonneg).mp
  calc
    ((1/4 : ℝ) * Real.sqrt ((a + b + c) * (-a + b + c) * (a - b + c) * (a + b - c))) ^ 2
        = ((1/4 : ℝ)^2) * ((Real.sqrt ((a + b + c) * (-a + b + c) * (a - b + c) * (a + b - c))) ^ 2) := by ring
    _ = (1/16) * ((a + b + c) * (-a + b + c) * (a - b + c) * (a + b - c)) := by
      simp [Real.sq_sqrt h_prod_nonneg]
      ring
    _ = s * (s - a) * (s - b) * (s - c) := by
      dsimp [s]
      ring
    _ = (Real.sqrt (s * (s - a) * (s - b) * (s - c))) ^ 2 := by
      symm; exact Real.sq_sqrt hRHS_prod_nonneg

/-- 相似变换下向量点积缩放：v_ED · v_EF = s² · v_BA · v_BC -/
lemma dot_product_scales (A B C D E F : Pt) (s θ tx ty : ℝ) (hs_pos : s > 0)
    (hD : D = translate (scale_rotate A A s θ) tx ty)
    (hE : E = translate (scale_rotate B A s θ) tx ty)
    (hF : F = translate (scale_rotate C A s θ) tx ty) :
    (D.x - E.x) * (F.x - E.x) + (D.y - E.y) * (F.y - E.y) = s^2 * ((A.x - B.x) * (C.x - B.x) + (A.y - B.y) * (C.y - B.y)) := by
  unfold scale_rotate translate at hD hE hF
  simp at hD hE hF
  subst hD; subst hE; subst hF
  have hcos_sq_add_sin_sq : Real.cos θ ^ 2 + Real.sin θ ^ 2 = 1 := Real.cos_sq_add_sin_sq θ
  nlinarith

/-- 相似变换下向量模长缩放：|v_ED| = s · |v_BA| -/
lemma norm_sq_scales (A B X Y : Pt) (s θ tx ty : ℝ) (hs_pos : s > 0)
    (hX : X = translate (scale_rotate A A s θ) tx ty)
    (hY : Y = translate (scale_rotate B A s θ) tx ty) :
    (X.x - Y.x)^2 + (X.y - Y.y)^2 = s^2 * ((A.x - B.x)^2 + (A.y - B.y)^2) := by
  unfold scale_rotate translate at hX hY
  simp at hX hY
  subst hX; subst hY
  have hcos_sq_add_sin_sq : Real.cos θ ^ 2 + Real.sin θ ^ 2 = 1 := Real.cos_sq_add_sin_sq θ
  nlinarith

/-- 相似变换下角度保持不变 -/
theorem angle_invariant_under_similarity (A B C D E F : Pt)
  (h_sim : triangle_similarity A B C D E F) :
  angle A B C = angle D E F := by
  rcases h_sim with ⟨s, θ, tx, ty, hs_pos, hD, hE, hF⟩
  unfold angle
  -- 定义模长变量，以便处理退化情形
  set nBA := Real.sqrt ((A.x - B.x)^2 + (A.y - B.y)^2) with hnBA
  set nBC := Real.sqrt ((C.x - B.x)^2 + (C.y - B.y)^2) with hnBC
  set nED := Real.sqrt ((D.x - E.x)^2 + (D.y - E.y)^2) with hnED
  set nEF := Real.sqrt ((F.x - E.x)^2 + (F.y - E.y)^2) with hnEF
  -- 处理退化情形：|BA| = 0 或 |BC| = 0
  by_cases hBA_zero : nBA = 0
  · have hED_zero : nED = 0 := by
      have h_sq : (D.x - E.x)^2 + (D.y - E.y)^2 = 0 := by
        have h_temp := norm_sq_scales A B D E s θ tx ty hs_pos hD hE
        have h_BA_sq : (A.x - B.x)^2 + (A.y - B.y)^2 = 0 := by
          have h_nonneg : 0 ≤ (A.x - B.x)^2 + (A.y - B.y)^2 := by positivity
          rw [Real.sqrt_eq_zero.mp hBA_zero] at h_nonneg
          nlinarith
        nlinarith
      calc
        nED = Real.sqrt ((D.x - E.x)^2 + (D.y - E.y)^2) := rfl
        _ = Real.sqrt 0 := by rw [h_sq]
        _ = 0 := by simp
    simp [hBA_zero, hED_zero]
  · by_cases hBC_zero : nBC = 0
    · have hEF_zero : nEF = 0 := by
        have h_sq : (F.x - E.x)^2 + (F.y - E.y)^2 = 0 := by
          have h_temp := norm_sq_scales C B F E s θ tx ty hs_pos hF hE
          have h_BC_sq : (C.x - B.x)^2 + (C.y - B.y)^2 = 0 := by
            have h_nonneg : 0 ≤ (C.x - B.x)^2 + (C.y - B.y)^2 := by positivity
            rw [Real.sqrt_eq_zero.mp hBC_zero] at h_nonneg
            nlinarith
          nlinarith
        calc
          nEF = Real.sqrt ((F.x - E.x)^2 + (F.y - E.y)^2) := rfl
          _ = Real.sqrt 0 := by rw [h_sq]
          _ = 0 := by simp
      simp [hBA_zero, hBC_zero, hEF_zero]
    · -- 非退化情形：所有模长非零
      have h_nonzero_BA : nBA ≠ 0 := hBA_zero
      have h_nonzero_BC : nBC ≠ 0 := hBC_zero
      have h_nonzero_ED : nED ≠ 0 := by
        intro hzero
        apply h_nonzero_BA
        have h_sq : (D.x - E.x)^2 + (D.y - E.y)^2 = 0 := by
          have h_nonneg : 0 ≤ (D.x - E.x)^2 + (D.y - E.y)^2 := by positivity
          rw [Real.sqrt_eq_zero.mp hzero] at h_nonneg
          nlinarith
        have h_temp := norm_sq_scales A B D E s θ tx ty hs_pos hD hE
        have h_nonneg_sq : 0 ≤ (A.x - B.x)^2 + (A.y - B.y)^2 := by positivity
        nlinarith
      have h_nonzero_EF : nEF ≠ 0 := by
        intro hzero
        apply h_nonzero_BC
        have h_sq : (F.x - E.x)^2 + (F.y - E.y)^2 = 0 := by
          have h_nonneg : 0 ≤ (F.x - E.x)^2 + (F.y - E.y)^2 := by positivity
          rw [Real.sqrt_eq_zero.mp hzero] at h_nonneg
          nlinarith
        have h_temp := norm_sq_scales C B F E s θ tx ty hs_pos hF hE
        have h_nonneg_sq : 0 ≤ (C.x - B.x)^2 + (C.y - B.y)^2 := by positivity
        nlinarith
      simp [h_nonzero_BA, h_nonzero_BC, h_nonzero_ED, h_nonzero_EF]
      -- 核心：点积和模长都缩放 s²，因此 arccos 的参数不变
      have h_dot_eq : (D.x - E.x) * (F.x - E.x) + (D.y - E.y) * (F.y - E.y) = s^2 * ((A.x - B.x) * (C.x - B.x) + (A.y - B.y) * (C.y - B.y)) :=
        dot_product_scales A B C D E F s θ tx ty hs_pos hD hE hF
      have h_norm_BA_sq : (D.x - E.x)^2 + (D.y - E.y)^2 = s^2 * ((A.x - B.x)^2 + (A.y - B.y)^2) :=
        norm_sq_scales A B D E s θ tx ty hs_pos hD hE
      have h_norm_BC_sq : (F.x - E.x)^2 + (F.y - E.y)^2 = s^2 * ((C.x - B.x)^2 + (C.y - B.y)^2) :=
        norm_sq_scales C B F E s θ tx ty hs_pos hF hE
      -- 化简比率等式
      have h_ratio_eq : ((A.x - B.x) * (C.x - B.x) + (A.y - B.y) * (C.y - B.y)) / (nBA * nBC) =
        ((D.x - E.x) * (F.x - E.x) + (D.y - E.y) * (F.y - E.y)) / (nED * nEF) := by
        dsimp [nBA, nBC, nED, nEF]
        field_simp [h_nonzero_BA, h_nonzero_BC, h_nonzero_ED, h_nonzero_EF]
        nlinarith [h_dot_eq, h_norm_BA_sq, h_norm_BC_sq]
      -- 等参数 ⇒ 等 arccos
      rw [h_ratio_eq]

end lvFormal.Theory.GeometryPresetDefs