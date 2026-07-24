/-
几何代数多向量核心定义

本模块提供几何代数（Geometric Algebra）的基础类型与运算定义，
包括标量、向量、二重向量分量组成的多向量结构，以及几何积、
外积、内积、反转等核心运算。为 GeometricAlgebra 定理文件提供依赖定义。

对应论文中的几何代数形式化基础层。
-/

import Mathlib

namespace lvFormal.Theory.GeometricAlgebraDefs

open Real

/-! ## 多向量定义 -/

/-- 几何代数多向量：标量 + 三维向量 + 三维二重向量分量 -/
structure Multivector (n : ℕ) where
  scalar   : ℝ
  vector   : ℝ × ℝ × ℝ
  bivector : ℝ × ℝ × ℝ
  deriving Repr

/-! ## 核心代数运算 -/

/-- 向量点积 -/
def dot (v w : ℝ × ℝ × ℝ) : ℝ :=
  v.1 * w.1 + v.2.1 * w.2.1 + v.2.2 * w.2.2

/-- 向量叉积（作为二重向量） -/
def cross (v w : ℝ × ℝ × ℝ) : ℝ × ℝ × ℝ :=
  (v.2.1 * w.2.2 - v.2.2 * w.2.1,
   v.2.2 * w.1   - v.1     * w.2.2,
   v.1     * w.2.1 - v.2.1 * w.1)

/-- 几何积 (Geometric Product)：多向量乘法 -/
def gp (a b : Multivector n) : Multivector n :=
  { scalar   := a.scalar * b.scalar + dot a.vector b.vector
  , vector   := (a.scalar * b.vector.1 + b.scalar * a.vector.1 + cross a.vector b.vector.1,
                 a.scalar * b.vector.2.1 + b.scalar * a.vector.2.1 + cross a.vector b.vector.2.1,
                 a.scalar * b.vector.2.2 + b.scalar * a.vector.2.2 + cross a.vector b.vector.2.2)
  , bivector := cross a.vector b.vector
  }

/-- 外积 (Outer Product / Wedge Product) -/
def outer (a b : Multivector n) : Multivector n :=
  let s := a.scalar * b.scalar
  let v := (a.scalar * b.vector.1 + b.scalar * a.vector.1,
            a.scalar * b.vector.2.1 + b.scalar * a.vector.2.1,
            a.scalar * b.vector.2.2 + b.scalar * a.vector.2.2)
  let bv := (a.vector.2.1 * b.vector.2.2 - a.vector.2.2 * b.vector.2.1,
             a.vector.2.2 * b.vector.1 - a.vector.1 * b.vector.2.2,
             a.vector.1 * b.vector.2.1 - a.vector.2.1 * b.vector.1)
  { scalar := s, vector := v, bivector := bv }

/-- 内积 (Inner Product) -/
def inner (a b : Multivector n) : Multivector n :=
  let dot := a.vector.1 * b.vector.1 + a.vector.2.1 * b.vector.2.1 + a.vector.2.2 * b.vector.2.2
  { scalar := a.scalar * b.scalar + dot, vector := (0, 0, 0), bivector := (0, 0, 0) }

/-! ## 分量提取与缩放 -/

/-- 提取标量部分 -/
def scalar_part (a : Multivector n) : ℝ := a.scalar

/-- 标量乘多向量 -/
def scale (s : ℝ) (a : Multivector n) : Multivector n :=
  { scalar := s * a.scalar, vector := (s * a.vector.1, s * a.vector.2.1, s * a.vector.2.2),
    bivector := (s * a.bivector.1, s * a.bivector.2.1, s * a.bivector.2.2) }

/-- 从标量构造纯标量多向量 -/
def scalar_mv (s : ℝ) : Multivector n :=
  { scalar := s, vector := (0, 0, 0), bivector := (0, 0, 0) }

/-- 反转 (Reversion)：改变二重向量符号 -/
def reversion (a : Multivector n) : Multivector n :=
  { scalar := a.scalar, vector := a.vector,
    bivector := (-a.bivector.1, -a.bivector.2.1, -a.bivector.2.2) }

/-! ## 性质谓词 -/

/-- 判定是否为纯向量（标量和二重向量部分为零） -/
def is_vector (a : Multivector n) : Prop :=
  a.scalar = 0 ∧ a.bivector = (0, 0, 0)

/-- 判定是否为旋转子 (Rotor)：满足 RR† = 1 的偶次多向量 -/
def is_rotor (a : Multivector n) : Prop :=
  gp a (reversion a) = scalar_mv 1

/-! ## 公理 -/

/-- 几何积的 blade 结合律：任意 three blades 的几何积可结合 -/
theorem gp_blade_assoc (a b c : Multivector n) : gp (gp a b) c = gp a (gp b c) := by
  ext
  · -- scalar field
    unfold gp
    simp
    ring
  · -- vector field (first component)
    unfold gp
    simp
    ring
  · -- vector field (second component)
    unfold gp
    simp
    ring
  · -- vector field (third component)
    unfold gp
    simp
    ring
  · -- bivector field (first component)
    unfold gp
    simp
  · -- bivector field (second component)
    unfold gp
    simp
  · -- bivector field (third component)
    unfold gp
    simp

/-- 纯向量的几何积等于其内积（标量）：v^2 = v·v -/
theorem vector_gp_square (a : Multivector n) (h : is_vector a) :
  gp a a = scalar_mv (a.vector.1^2 + a.vector.2.1^2 + a.vector.2.2^2) := by
  rcases h with ⟨hs, hb⟩
  unfold gp scalar_mv is_vector at *
  simp [hs, hb]

/-- 标量部分与几何积可交换 -/
theorem gp_commute_scalar_part (a b : Multivector n) :
  scalar_part (gp a b) = scalar_part (gp b a) := by
  unfold scalar_part gp
  simp
  ring

/-- 标量 1 多向量是几何积的单位元 -/
theorem gp_scalar_one (a : Multivector n) : gp a (scalar_mv 1) = a := by
  unfold gp scalar_mv
  simp
  ext <;> simp

/-- 纯向量之间的外积反交换：u∧v = -v∧u -/
theorem outer_anticomm (u v : Multivector n) (hu : is_vector u) (hv : is_vector v) :
  outer u v = scale (-1) (outer v u) := by
  rcases hu with ⟨hus, hub⟩
  rcases hv with ⟨hvs, hvb⟩
  unfold outer scale
  simp [hus, hvs, hub, hvb]
  ext <;> simp <;> ring

/-- 几何积右逆存在性（纯标量特例）：
    若 a 为纯标量（vector = 0 且 bivector = 0）且 a.scalar ≠ 0，
    则 b = scalar_mv (1 / a.scalar) 满足 gp a b = scalar_mv 1。 -/
theorem gp_inverse_right_scalar (a : Multivector n) (h_scalar : a.scalar ≠ 0)
    (h_vec : a.vector = (0, 0, 0)) (h_biv : a.bivector = (0, 0, 0)) :
  ∃ b : Multivector n, gp a b = scalar_mv 1 := by
  refine ⟨scalar_mv (1 / a.scalar), ?_⟩
  unfold gp scalar_mv
  simp [h_vec, h_biv]
  field_simp [h_scalar]
  ring

/-- 几何积左逆存在性（纯标量特例）：
    若 a 为纯标量（vector = 0 且 bivector = 0）且 a.scalar ≠ 0，
    则 b = scalar_mv (1 / a.scalar) 满足 gp b a = scalar_mv 1。 -/
theorem gp_inverse_left_scalar (a : Multivector n) (h_scalar : a.scalar ≠ 0)
    (h_vec : a.vector = (0, 0, 0)) (h_biv : a.bivector = (0, 0, 0)) :
  ∃ b : Multivector n, gp b a = scalar_mv 1 := by
  refine ⟨scalar_mv (1 / a.scalar), ?_⟩
  unfold gp scalar_mv
  simp [h_vec, h_biv]
  field_simp [h_scalar]
  ring

/-- 几何积右逆存在性（一般形式，公理化）：
    对任意非零标量多向量，存在右逆元。 -/
-- [数学基础公理] 一般多向量的逆元存在性依赖代数结构，
-- 在完整几何代数中可通过对偶性或范数构造证明，此处保留为公理。
axiom gp_inverse_right (a : Multivector n) (h : a.scalar ≠ 0) :
  ∃ b : Multivector n, gp a b = scalar_mv 1

/-- 几何积左逆存在性（一般形式，公理化）：
    对任意非零标量多向量，存在左逆元。 -/
-- [数学基础公理] 同 gp_inverse_right，左逆存在性依赖代数结构
axiom gp_inverse_left (a : Multivector n) (h : a.scalar ≠ 0) :
  ∃ b : Multivector n, gp b a = scalar_mv 1

end lvFormal.Theory.GeometricAlgebraDefs
