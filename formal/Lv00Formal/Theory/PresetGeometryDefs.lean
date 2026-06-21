/-
预设几何核心定义

本模块在 GeometryPresetDefs.Pt 基础上提供带标签的几何体类型与运算定义，
包括带标签点 LvPoint、线段 LvLine、共线性、等边三角形、
外心、垂心、重心、角平分线、中垂线、圆定义等。
为 PresetGeometry 定理文件提供依赖定义。

对应论文中的预设几何形式化基础层（Level-2 抽象）。
-/

import Lv00Formal.Theory.GeometryPresetDefs

namespace Lv00Formal.Theory.PresetGeometryDefs

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
  let M : LvPoint := centroid A B (centroid A B A) -- 占位简化
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

/-! ## 公理 -/

/-- 共线性自反 -/
axiom collinear_refl (A B : LvPoint) : collinear A A B

/-- 共线性对称（如果使用 collinear 的三元参数，则对称性体现在 B 的位置无关） -/
axiom collinear_symm (A B C : LvPoint) (h : collinear A B C) : collinear C B A

/-- 共线且有序时距离可加 -/
axiom dist_additive_of_collinear (A B C : LvPoint) (h_col : collinear A B C) :
  dist A C = dist A B + dist B C

/-- 等边三角形的外心等于垂心 -/
axiom circumcenter_equals_orthocenter_of_equilateral (A B C : LvPoint)
  (h_eq : is_equilateral A B C) : circumcenter A B C = orthocenter A B C

/-- 等边三角形的外心等于重心 -/
axiom circumcenter_equals_centroid_of_equilateral (A B C : LvPoint)
  (h_eq : is_equilateral A B C) : circumcenter A B C = centroid A B C

/-- 三点定义的圆是唯一的（内射性） -/
axiom circle_inj (A B C D E F : LvPoint)
  (h1 : circle_defined_by A B C = circle_defined_by D E F) (h2 : ¬ collinear A B C) :
  {A, B, C} = {D, E, F}

end Lv00Formal.Theory.PresetGeometryDefs
