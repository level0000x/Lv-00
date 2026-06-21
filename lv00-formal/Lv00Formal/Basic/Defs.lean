/-!
# 基础定义

定义几何元语言的核心类型和基础结构。
这些定义与 C 核心中的定义对应，用于后续的等价性验证。

## 主要类型

- `Point`: 几何点
- `Line`: 直线
- `Plane`: 平面
- `Circle`: 圆
- `Segment`: 线段
- `Angle`: 角度
- `Triangle`: 三角形
-/}

namespace Lv00Formal

-- 使用 Mathlib 的实数定义
open Real

/-! ## 基本几何类型 -/

/-- 几何点 -/
structure Point where
  x : ℝ
  y : ℝ
  z : ℝ := 0  -- 默认为0，支持2D和3D
  deriving DecidableEq, Repr

namespace Point

/-- 2D点构造器 -/
def mk2D (x y : ℝ) : Point := ⟨x, y, 0⟩

/-- 3D点构造器 -/
def mk3D (x y z : ℝ) : Point := ⟨x, y, z⟩

/-- 点到原点的距离 -/
def distanceToOrigin (p : Point) : ℝ :=
  Real.sqrt (p.x^2 + p.y^2 + p.z^2)

/-- 两点之间的距离 -/
def distance (p1 p2 : Point) : ℝ :=
  Real.sqrt ((p1.x - p2.x)^2 + (p1.y - p2.y)^2 + (p1.z - p2.z)^2)

/-- 点在线上（参数化表示）-/
def onLineParametric (p : Point) (base dir : Point) : Prop :=
  ∃ (t : ℝ), p.x = base.x + t * dir.x ∧
             p.y = base.y + t * dir.y ∧
             p.z = base.z + t * dir.z

end Point

/-- 直线，由两点确定 -/
structure Line where
  p1 : Point
  p2 : Point
  ne : p1 ≠ p2  -- 两点必须不同
  deriving Repr

namespace Line

/-- 直线上的点（参数化）-/
def pointAt (l : Line) (t : ℝ) : Point :=
  ⟨l.p1.x + t * (l.p2.x - l.p1.x),
   l.p1.y + t * (l.p2.y - l.p1.y),
   l.p1.z + t * (l.p2.z - l.p1.z)⟩

/-- 点在直线上 -/
def contains (l : Line) (p : Point) : Prop :=
  collinear l.p1 l.p2 p

/-- 两条直线平行 -/
def parallel (l1 l2 : Line) : Prop :=
  ∃ (v1 v2 : Point), 
    v1 ≠ ⟨0, 0, 0⟩ ∧ v2 ≠ ⟨0, 0, 0⟩ ∧
    l1.p2.x - l1.p1.x = v1.x ∧ l1.p2.y - l1.p1.y = v1.y ∧
    l2.p2.x - l2.p1.x = v2.x ∧ l2.p2.y - l2.p1.y = v2.y ∧
    v1.x * v2.y = v1.y * v2.x  -- 2D叉积为零

/-- 两条直线相交 -/
def intersect (l1 l2 : Line) : Prop :=
  ∃ (p : Point), l1.contains p ∧ l2.contains p

/-- 两条直线垂直 -/
def perpendicular (l1 l2 : Line) : Prop :=
  let v1 := ⟨l1.p2.x - l1.p1.x, l1.p2.y - l1.p1.y, 0⟩
  let v2 := ⟨l2.p2.x - l2.p1.x, l2.p2.y - l2.p1.y, 0⟩
  v1.x * v2.x + v1.y * v2.y = 0

end Line

/-- 平面（由法向量和点定义）-/
structure Plane where
  normal : Point
  point : Point
  nonZeroNormal : normal ≠ ⟨0, 0, 0⟩

namespace Plane

/-- 点在平面上 -/
def contains (pl : Plane) (p : Point) : Prop :=
  pl.normal.x * (p.x - pl.point.x) +
  pl.normal.y * (p.y - pl.point.y) +
  pl.normal.z * (p.z - pl.point.z) = 0

/-- 直线在平面上 -/
def containsLine (pl : Plane) (l : Line) : Prop :=
  pl.contains l.p1 ∧ pl.contains l.p2

end Plane

/-- 圆（由圆心和半径定义）-/
structure Circle where
  center : Point
  radius : ℝ
  radiusPositive : radius > 0

deriving Repr

namespace Circle

/-- 点在圆上 -/
def contains (c : Circle) (p : Point) : Prop :=
  Point.distance p c.center = c.radius

/-- 点在圆内 -/
def interior (c : Circle) (p : Point) : Prop :=
  Point.distance p c.center < c.radius

/-- 点在圆外 -/
def exterior (c : Circle) (p : Point) : Prop :=
  Point.distance p c.center > c.radius

end Circle

/-- 线段 -/
structure Segment where
  p1 : Point
  p2 : Point

deriving DecidableEq, Repr

namespace Segment

/-- 线段长度 -/
def length (s : Segment) : ℝ :=
  Point.distance s.p1 s.p2

/-- 点在线段上（包括端点）-/
def contains (s : Segment) (p : Point) : Prop :=
  collinear s.p1 s.p2 p ∧
  Point.distance s.p1 p + Point.distance p s.p2 = s.length

/-- 线段的中点 -/
def midpoint (s : Segment) : Point :=
  ⟨(s.p1.x + s.p2.x) / 2, (s.p1.y + s.p2.y) / 2, (s.p1.z + s.p2.z) / 2⟩

end Segment

/-- 角度（由顶点和两边定义）-/
structure Angle where
  vertex : Point
  arm1 : Point
  arm2 : Point
  ne1 : vertex ≠ arm1
  ne2 : vertex ≠ arm2

deriving Repr

namespace Angle

/-- 角度的度量（弧度）-/
def measure (a : Angle) : ℝ :=
  let v1 := ⟨a.arm1.x - a.vertex.x, a.arm1.y - a.vertex.y, 0⟩
  let v2 := ⟨a.arm2.x - a.vertex.x, a.arm2.y - a.vertex.y, 0⟩
  let dot := v1.x * v2.x + v1.y * v2.y
  let det := v1.x * v2.y - v1.y * v2.x
  Real.arctan2 det dot

/-- 直角 -/
def isRight (a : Angle) : Prop :=
  measure a = Real.pi / 2

/-- 锐角 -/
def isAcute (a : Angle) : Prop :=
  0 < measure a ∧ measure a < Real.pi / 2

/-- 钝角 -/
def isObtuse (a : Angle) : Prop :=
  Real.pi / 2 < measure a ∧ measure a < Real.pi

end Angle

/-- 三角形 -/
structure Triangle where
  a : Point
  b : Point
  c : Point
  nonCollinear : ¬collinear a b c

deriving Repr

namespace Triangle

/-- 三角形的边 -/
def sideAB (t : Triangle) : Segment := ⟨t.a, t.b⟩
def sideBC (t : Triangle) : Segment := ⟨t.b, t.c⟩
def sideCA (t : Triangle) : Segment := ⟨t.c, t.a⟩

/-- 三角形的顶点角 -/
def angleA (t : Triangle) : Angle :=
  have hne_ab : t.a ≠ t.b := by
    intro h; apply t.nonCollinear; unfold collinear; simp [h]
  have hne_ac : t.a ≠ t.c := by
    intro h; apply t.nonCollinear; unfold collinear; simp [h]
  ⟨t.a, t.b, t.c, hne_ab, hne_ac⟩

def angleB (t : Triangle) : Angle :=
  have hne_ba : t.b ≠ t.a := by
    intro h; apply t.nonCollinear; unfold collinear; simp [h, add_comm, mul_comm]
  have hne_bc : t.b ≠ t.c := by
    intro h; apply t.nonCollinear; unfold collinear; simp [h]
  ⟨t.b, t.a, t.c, hne_ba, hne_bc⟩

def angleC (t : Triangle) : Angle :=
  have hne_ca : t.c ≠ t.a := by
    intro h; apply t.nonCollinear; unfold collinear; simp [h]
  have hne_cb : t.c ≠ t.b := by
    intro h; apply t.nonCollinear; unfold collinear; simp [h]
  ⟨t.c, t.a, t.b, hne_ca, hne_cb⟩

/-- 三角形的面积（海伦公式）-/
def area (t : Triangle) : ℝ :=
  let a := sideBC t |>.length
  let b := sideCA t |>.length
  let c := sideAB t |>.length
  let s := (a + b + c) / 2
  Real.sqrt (s * (s - a) * (s - b) * (s - c))

/-- 等边三角形 -/
def isEquilateral (t : Triangle) : Prop :=
  sideAB t |>.length = sideBC t |>.length ∧
  sideBC t |>.length = sideCA t |>.length

/-- 等腰三角形 -/
def isIsosceles (t : Triangle) : Prop :=
  sideAB t |>.length = sideBC t |>.length ∨
  sideBC t |>.length = sideCA t |>.length ∨
  sideCA t |>.length = sideAB t |>.length

/-- 直角三角形 -/
def isRight (t : Triangle) : Prop :=
  angleA t |>.isRight ∨ angleB t |>.isRight ∨ angleC t |>.isRight

end Triangle

/-! ## 辅助定义 -/

/-- 三点共线 -/
def collinear (p1 p2 p3 : Point) : Prop :=
  (p2.x - p1.x) * (p3.y - p1.y) = (p3.x - p1.x) * (p2.y - p1.y) ∧
  (p2.x - p1.x) * (p3.z - p1.z) = (p3.x - p1.x) * (p2.z - p1.z)

/-- 共线性的传递性 -/
theorem collinear_trans {p1 p2 p3 p4 : Point} 
    (h12 : collinear p1 p2 p3) (h13 : collinear p1 p3 p4) 
    (hne : p1 ≠ p3) : collinear p1 p2 p4 := by
  rcases h12 with ⟨h12xy, h12xz⟩
  rcases h13 with ⟨h13xy, h13xz⟩
  set dx := p3.x - p1.x
  set dy := p3.y - p1.y
  set dz := p3.z - p1.z
  have hdx_dy_dz : dx ≠ 0 ∨ dy ≠ 0 ∨ dz ≠ 0 := by
    intro h
    rcases h with ⟨hx, hy, hz⟩
    apply hne
    apply Point.ext <;> dsimp <;> linarith
  have hxy : (p2.x - p1.x) * (p4.y - p1.y) = (p4.x - p1.x) * (p2.y - p1.y) := by
    by_cases hdx0 : dx = 0
    · by_cases hdy0 : dy = 0
      · -- dx = 0, dy = 0, so dz ≠ 0 by hne
        have hdz0 : dz ≠ 0 := by
          intro hz
          rcases hdx_dy_dz with (h | h | h)
          · exact h hdx0
          · exact h hdy0
          · exact h hz
        have ha0 : p2.x - p1.x = 0 := by
          have h_eq : (p2.x - p1.x) * dz = 0 := by nlinarith
          rcases eq_zero_or_eq_zero_of_mul_eq_zero h_eq with (h0 | hz')
          · exact h0
          · exact absurd hz' hdz0
        have he0 : p4.x - p1.x = 0 := by
          have h_eq : (p4.x - p1.x) * dz = 0 := by nlinarith
          rcases eq_zero_or_eq_zero_of_mul_eq_zero h_eq with (h0 | hz')
          · exact h0
          · exact absurd hz' hdz0
        nlinarith
      · -- dy ≠ 0
        have h_factor : dy * ((p2.x - p1.x) * (p4.y - p1.y) - (p4.x - p1.x) * (p2.y - p1.y)) = 0 := by
          nlinarith
        rcases eq_zero_or_eq_zero_of_mul_eq_zero h_factor with (hdy0' | h_target)
        · exact absurd hdy0' hdy0
        · nlinarith
    · -- dx ≠ 0
      have h_factor : dx * ((p2.x - p1.x) * (p4.y - p1.y) - (p4.x - p1.x) * (p2.y - p1.y)) = 0 := by
        nlinarith
      rcases eq_zero_or_eq_zero_of_mul_eq_zero h_factor with (hdx0' | h_target)
      · exact absurd hdx0' hdx0
      · nlinarith
  have hxz : (p2.x - p1.x) * (p4.z - p1.z) = (p4.x - p1.x) * (p2.z - p1.z) := by
    by_cases hdx0 : dx = 0
    · by_cases hdz0 : dz = 0
      · -- dx = 0, dz = 0, so dy ≠ 0 by hne
        have hdy0 : dy ≠ 0 := by
          intro hy
          rcases hdx_dy_dz with (h | h | h)
          · exact h hdx0
          · exact h hy
          · exact h hdz0
        have ha0 : p2.x - p1.x = 0 := by
          have h_eq : (p2.x - p1.x) * dy = 0 := by nlinarith
          rcases eq_zero_or_eq_zero_of_mul_eq_zero h_eq with (h0 | hy')
          · exact h0
          · exact absurd hy' hdy0
        have he0 : p4.x - p1.x = 0 := by
          have h_eq : (p4.x - p1.x) * dy = 0 := by nlinarith
          rcases eq_zero_or_eq_zero_of_mul_eq_zero h_eq with (h0 | hy')
          · exact h0
          · exact absurd hy' hdy0
        nlinarith
      · -- dz ≠ 0
        have h_factor : dz * ((p2.x - p1.x) * (p4.z - p1.z) - (p4.x - p1.x) * (p2.z - p1.z)) = 0 := by
          nlinarith
        rcases eq_zero_or_eq_zero_of_mul_eq_zero h_factor with (hdz0' | h_target)
        · exact absurd hdz0' hdz0
        · nlinarith
    · -- dx ≠ 0
      have h_factor : dx * ((p2.x - p1.x) * (p4.z - p1.z) - (p4.x - p1.x) * (p2.z - p1.z)) = 0 := by
        nlinarith
      rcases eq_zero_or_eq_zero_of_mul_eq_zero h_factor with (hdx0' | h_target)
      · exact absurd hdx0' hdx0
      · nlinarith
  exact ⟨hxy, hxz⟩

/-- 点在线段之间（B在A和C之间）-/
def between (A B C : Point) : Prop :=
  collinear A B C ∧
  Point.distance A B + Point.distance B C = Point.distance A C ∧
  A ≠ B ∧ B ≠ C

/-- 两点确定唯一直线 -/
theorem unique_line_through_two_points {p1 p2 : Point} (hne : p1 ≠ p2) :
    ∃! l : Line, l.p1 = p1 ∧ l.p2 = p2 := by
  use ⟨p1, p2, hne⟩
  simp
  intro l hl1 hl2
  rw [←hl1, ←hl2]

end Lv00Formal
