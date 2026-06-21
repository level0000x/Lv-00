/-  Lv-00 经典几何公理化框架：基础类型定义
   定义 Point、Line、Plane、MetricSpace、Congruence 等数学对象的类 -/

-- [QA] Parallel Hilbert formalization. lv00-formal/Classical/Hilbert/ provides
--      the full axiomatic treatment with proofs; this file provides the typeclass
--      abstraction layer. Do NOT merge; they serve different architectural roles.
-- [QA] Point/Line etc defined here as typeclasses. The structural versions
--      live in lv00-formal/Basic/Defs.lean. Both are needed: typeclasses for
--      abstract reasoning, structures for concrete computation.

import Mathlib

namespace Lv00.Basic

/-- 点 --/
class Point (α : Type) where

/-- 线 --/
class Line (α : Type) where

/-- 面 --/
class Plane (α : Type) where

/-- 度量空间：含距离公理 --/
class MetricSpace (α : Type) where
  dist : α → α → ℝ
  dist_nonneg : ∀ x y, dist x y ≥ 0
  dist_self : ∀ x, dist x x = 0
  dist_comm : ∀ x y, dist x y = dist y x
  dist_triangle : ∀ x y z, dist x z ≤ dist x y + dist y z
  eq_of_dist_eq_zero : ∀ {x y}, dist x y = 0 → x = y

/-- 全等关系：线段全等与角全等的抽象类型 --/
class Congruence (α : Type) where
  seg_congruent : α → α → α → α → Prop
  angle_congruent : α → α → α → α → α → α → Prop

end Lv00.Basic
