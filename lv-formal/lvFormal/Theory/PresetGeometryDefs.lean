/-
预设几何核心定义

本模块在 GeometryPresetDefs.Pt 基础上提供带标签的几何体类型与运算定义，
包括带标签点 LvPoint、线段 LvLine、共线性、等边三角形、
外心、垂心、重心、角平分线、中垂线、圆定义等。
为 PresetGeometry 定理文件提供依赖定义。

对应论文中的预设几何形式化基础层（Level-2 抽象）。
-/

import lvFormal.Theory.GeometryPresetDefs

namespace lvFormal.Theory.PresetGeometryDefs

open GeometryPresetDefs

/-! ## 带标签的几何体 -/

/-- 带标签的点：标签 + 二维坐标 -/
structure LvPoint where
  label : String
  coord : Pt
  deriving Repr, DecidableEq

/-- 线段：由两个端点 LvPoint 定义 -/
structure LvLine where
  a : LvPoint
  b : LvPoint
  deriving Repr

/-! ## 基本谓词与运算 -/

/-- 三点共线 -/
def collinear (A B C : LvPoint) : Prop :=
  (B.coord.x - A.coord.x) * (C.coord.y - A.coord.y) =
  (C.coord.x - A.coord.x) * (B.coord.y - A.coord.y)

/-- 带标签点之间的距离（委托给 Pt.dist） -/
def dist (p q : LvPoint) : ℝ :=
  GeometryPresetDefs.dist p.coord q.coord

/-- 三点构成等边三角形 -/
def is_equilateral (A B C : LvPoint) : Prop :=
  dist A B = dist B C ∧ dist B C = dist C A

/-! ## 三角形特殊点 -/

/-- 外心：三角形三边中垂线的交点 -/
def circumcenter (A B C : LvPoint) : LvPoint :=
  let d := 2 * (A.coord.x * (B.coord.y - C.coord.y) +
                B.coord.x * (C.coord.y - A.coord.y) +
                C.coord.x * (A.coord.y - B.coord.y))
  if d = 0 then
    -- 退化情况返回 A
    A
  else
    let ux := ((A.coord.x^2 + A.coord.y^2) * (B.coord.y - C.coord.y) +
               (B.coord.x^2 + B.coord.y^2) * (C.coord.y - A.coord.y) +
               (C.coord.x^2 + C.coord.y^2) * (A.coord.y - B.coord.y)) / d
    let uy := ((A.coord.x^2 + A.coord.y^2) * (C.coord.x - B.coord.x) +
               (B.coord.x^2 + B.coord.y^2) * (A.coord.x - C.coord.x) +
               (C.coord.x^2 + C.coord.y^2) * (B.coord.x - A.coord.x)) / d
    { label := s!"circum_({A.label},{B.label},{C.label})", coord := { x := ux, y := uy } }

/-- 垂心：三角形三条高的交点 -/
def orthocenter (A B C : LvPoint) : LvPoint :=
  let O := circumcenter A B C
  let G := centroid A B C
  -- 欧拉线：OH = 3·OG 即 H = O + 3(G - O)
  { label := s!"ortho_({A.label},{B.label},{C.label})"
  , coord := { x := O.coord.x + 3 * (G.coord.x - O.coord.x)
             , y := O.coord.y + 3 * (G.coord.y - O.coord.y) }
  }

/-- 重心：三角形三条中线的交点（三顶点坐标均值） -/
def centroid (A B C : LvPoint) : LvPoint :=
  { label := s!"centroid_({A.label},{B.label},{C.label})"
  , coord := { x := (A.coord.x + B.coord.x + C.coord.x) / 3
             , y := (A.coord.y + B.coord.y + C.coord.y) / 3 }
  }

/-! ## 特殊线 -/

/-- 线段 L 是否为 ∠(A,B,C) 的角平分线（以 B 为顶点） -/
def is_angle_bisector (L : LvLine) (A B C : LvPoint) : Prop :=
  on_line B L ∧
  (dist A L.a = dist C L.a) -- 简化：到角平分线上一点距离相等

/-- 线段 L 是否为点 A、B 的中垂线 -/
def is_perpendicular_bisector (L : LvLine) (A B : LvPoint) : Prop :=
  let M : LvPoint :=
    { label := s!"mid_({A.label},{B.label})"
    , coord := { x := (A.coord.x + B.coord.x) / 2, y := (A.coord.y + B.coord.y) / 2 }
    }
  on_line M L ∧
  (L.a.coord.x - L.b.coord.x) * (A.coord.x - B.coord.x) +
  (L.a.coord.y - L.b.coord.y) * (A.coord.y - B.coord.y) = 0

/-- 点是否在线上（共线性检测） -/
def on_line (P : LvPoint) (L : LvLine) : Prop :=
  collinear P L.a L.b

/-! ## 圆 -/

/-- 由三点定义的圆：(圆心, 半径)；若三点共线则返回 none -/
def circle_defined_by (A B C : LvPoint) : Option (LvPoint × ℝ) :=
  if collinear A B C then
    none
  else
    let O := circumcenter A B C
    let r := dist O A
    some (O, r)

/-! ## 介于关系 -/

/-- 点 B 在点 A 和点 C 之间（包含端点），当且仅当存在 t ∈ [0,1] 使得
    B 的坐标是 A 和 C 的凸组合。 -/
def between (A B C : LvPoint) : Prop :=
  ∃ (t : ℝ), 0 ≤ t ∧ t ≤ 1 ∧
    B.coord.x = A.coord.x + t * (C.coord.x - A.coord.x) ∧
    B.coord.y = A.coord.y + t * (C.coord.y - A.coord.y)

/-! ## 公理与定理 -/

/-- 共线性自反 -/
theorem collinear_refl (A B : LvPoint) : collinear A A B := by
  unfold collinear
  simp

/-- 共线性对称（如果使用 collinear 的三元参数，则对称性体现在 B 的位置无关） -/
theorem collinear_symm (A B C : LvPoint) (h : collinear A B C) : collinear C B A := by
  unfold collinear at h ⊢
  linarith

/-- 若 B 在 A 和 C 之间，则 A, B, C 共线 -/
theorem collinear_of_between (A B C : LvPoint) (h_bet : between A B C) : collinear A B C := by
  rcases h_bet with ⟨t, ht0, ht1, hx, hy⟩
  unfold collinear
  calc
    (B.coord.x - A.coord.x) * (C.coord.y - A.coord.y)
        = (t * (C.coord.x - A.coord.x)) * (C.coord.y - A.coord.y) := by
          rw [hx]; ring
    _ = (C.coord.x - A.coord.x) * (t * (C.coord.y - A.coord.y)) := by ring
    _ = (C.coord.x - A.coord.x) * (B.coord.y - A.coord.y) := by
      rw [hy]; ring

/-- 当 B 在 A 和 C 之间时，距离满足可加性：AC = AB + BC -/
theorem dist_additive_of_collinear (A B C : LvPoint) (h_col : collinear A B C)
  (h_bet : between A B C) : dist A C = dist A B + dist B C := by
  rcases h_bet with ⟨t, ht0, ht1, hx, hy⟩
  unfold dist GeometryPresetDefs.dist
  set dx := A.coord.x - C.coord.x with hdx
  set dy := A.coord.y - C.coord.y with hdy
  have hABx : A.coord.x - B.coord.x = t * dx := by rw [hx]; ring
  have hABy : A.coord.y - B.coord.y = t * dy := by rw [hy]; ring
  have hBCx : B.coord.x - C.coord.x = (1 - t) * dx := by rw [hx]; ring
  have hBCy : B.coord.y - C.coord.y = (1 - t) * dy := by rw [hy]; ring
  have h_sq_nonneg : 0 ≤ dx^2 + dy^2 := by positivity
  have ht_nonneg : 0 ≤ t := ht0
  have h1mt_nonneg : 0 ≤ 1 - t := by linarith
  calc
    Real.sqrt (dx^2 + dy^2)
        = (t + (1 - t)) * Real.sqrt (dx^2 + dy^2) := by ring
    _ = t * Real.sqrt (dx^2 + dy^2) + (1 - t) * Real.sqrt (dx^2 + dy^2) := by ring
    _ = Real.sqrt (t^2) * Real.sqrt (dx^2 + dy^2) + Real.sqrt ((1 - t)^2) * Real.sqrt (dx^2 + dy^2) := by
      simp [Real.sqrt_sq ht_nonneg, Real.sqrt_sq h1mt_nonneg]
    _ = Real.sqrt (t^2 * (dx^2 + dy^2)) + Real.sqrt ((1 - t)^2 * (dx^2 + dy^2)) := by
      simp [Real.sqrt_mul (sq_nonneg t), Real.sqrt_mul (sq_nonneg (1 - t))]
    _ = Real.sqrt ((A.coord.x - B.coord.x)^2 + (A.coord.y - B.coord.y)^2) +
        Real.sqrt ((B.coord.x - C.coord.x)^2 + (B.coord.y - C.coord.y)^2) := by
      rw [hABx, hABy, hBCx, hBCy]
      ring

/-- 辅助引理：距离相等推出平方距离相等 -/
lemma sq_dist_eq_of_dist_eq {A B C D : LvPoint} (h : dist A B = dist C D) :
    (A.coord.x - B.coord.x)^2 + (A.coord.y - B.coord.y)^2 =
    (C.coord.x - D.coord.x)^2 + (C.coord.y - D.coord.y)^2 := by
  unfold dist GeometryPresetDefs.dist at h ⊢
  have h_nonneg1 : 0 ≤ (A.coord.x - B.coord.x)^2 + (A.coord.y - B.coord.y)^2 := by nlinarith
  have h_nonneg2 : 0 ≤ (C.coord.x - D.coord.x)^2 + (C.coord.y - D.coord.y)^2 := by nlinarith
  calc
    (A.coord.x - B.coord.x)^2 + (A.coord.y - B.coord.y)^2
        = (Real.sqrt ((A.coord.x - B.coord.x)^2 + (A.coord.y - B.coord.y)^2))^2 := by
      rw [Real.sq_sqrt h_nonneg1]
    _ = (Real.sqrt ((C.coord.x - D.coord.x)^2 + (C.coord.y - D.coord.y)^2))^2 := by rw [h]
    _ = (C.coord.x - D.coord.x)^2 + (C.coord.y - D.coord.y)^2 := by rw [Real.sq_sqrt h_nonneg2]

/-- 等边三角形的外心等于重心（坐标相等）-/
theorem circumcenter_equals_centroid_of_equilateral (A B C : LvPoint)
  (h_eq : is_equilateral A B C) : (circumcenter A B C).coord = (centroid A B C).coord := by
  rcases h_eq with ⟨h_ab_bc, h_bc_ca⟩
  have h_sq_ab_bc : (A.coord.x - B.coord.x)^2 + (A.coord.y - B.coord.y)^2 =
                   (B.coord.x - C.coord.x)^2 + (B.coord.y - C.coord.y)^2 :=
    sq_dist_eq_of_dist_eq h_ab_bc
  have h_sq_bc_ca : (B.coord.x - C.coord.x)^2 + (B.coord.y - C.coord.y)^2 =
                   (C.coord.x - A.coord.x)^2 + (C.coord.y - A.coord.y)^2 :=
    sq_dist_eq_of_dist_eq h_bc_ca
  apply Pt.ext
  · unfold circumcenter centroid
    set d := 2 * (A.coord.x * (B.coord.y - C.coord.y) +
                  B.coord.x * (C.coord.y - A.coord.y) +
                  C.coord.x * (A.coord.y - B.coord.y)) with hd
    by_cases hd0 : d = 0
    · simp [hd, hd0]
      nlinarith
    · simp [hd, hd0]
      field_simp [hd0]
      nlinarith
  · unfold circumcenter centroid
    set d := 2 * (A.coord.x * (B.coord.y - C.coord.y) +
                  B.coord.x * (C.coord.y - A.coord.y) +
                  C.coord.x * (A.coord.y - B.coord.y)) with hd
    by_cases hd0 : d = 0
    · simp [hd, hd0]
      nlinarith
    · simp [hd, hd0]
      field_simp [hd0]
      nlinarith

/-- 等边三角形的外心等于垂心（坐标相等） -/
theorem circumcenter_equals_orthocenter_of_equilateral (A B C : LvPoint)
  (h_eq : is_equilateral A B C) : (circumcenter A B C).coord = (orthocenter A B C).coord := by
  have h_cen : (circumcenter A B C).coord = (centroid A B C).coord :=
    circumcenter_equals_centroid_of_equilateral A B C h_eq
  unfold orthocenter
  -- 垂心定义为 O + 3*(G - O)，由 h_cen 知 O.coord = G.coord，故 O + 3*(G - O) = O
  have h_eq_coord : (circumcenter A B C).coord = (centroid A B C).coord := h_cen
  simp [h_eq_coord]

/-- 若三点不共线，则它们定义的圆存在（不为 none） -/
theorem circle_defined_by_noncollinear (A B C : LvPoint) (h : ¬ collinear A B C) :
    circle_defined_by A B C ≠ none := by
  unfold circle_defined_by
  simp [h]

/-- 若两组三点定义同一个圆，且第一组不共线，则第二组也不共线。

    注：原始的 `circle_inj` 公理声称此时 {A,B,C} = {D,E,F}，但这是错误的——
    同一圆上的不同三点组（如单位圆上的 (1,0),(0,1),(-1,0) 与 (1,0),(0,-1),(-1,0)）
    定义了同一个圆，但点集却不同。故用本定理替代。 -/
theorem circle_inj_implies_noncollinear (A B C D E F : LvPoint)
    (h1 : circle_defined_by A B C = circle_defined_by D E F) (h2 : ¬ collinear A B C) :
    ¬ collinear D E F := by
  have hABC_some : circle_defined_by A B C ≠ none :=
    circle_defined_by_noncollinear A B C h2
  intro hcol_def
  have hDEF_none : circle_defined_by D E F = none := by
    unfold circle_defined_by
    simp [hcol_def]
  rw [hDEF_none] at h1
  exact hABC_some h1

end lvFormal.Theory.PresetGeometryDefs
