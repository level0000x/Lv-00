/-
数值计算核心定义

本模块提供数值线性代数与迭代方法的基础类型与运算定义，
包括矩阵、对称性、正定性、特征值、GMRES/CG/Power 迭代、
信赖等级、以及 Horner 多项式求值。
为 Numeric 定理文件提供依赖定义。

对应论文中的数值计算形式化基础层。
-/

import Mathlib

noncomputable section

namespace lvFormal.Theory.NumericDefs

open Real

/-! ## 矩阵类型与性质 -/

/-- 矩阵缩写：维度 n×m 的实数矩阵，通过函数索引访问 -/
abbrev Matrix (n m : ℕ) := ℕ → ℕ → ℝ

/-- 正定性：对任意非零向量 x，xᵀ M x > 0 -/
def PosDef {n : ℕ} (M : Matrix n n) : Prop :=
  ∀ (x : ℕ → ℝ), (∃ i, i < n ∧ x i ≠ 0) →
    (∑ i in Finset.range n, ∑ j in Finset.range n, x i * M i j * x j) > 0

/-- 对称性：M i j = M j i 对所有 i,j 成立 -/
def Symmetric {n : ℕ} (M : Matrix n n) : Prop :=
  ∀ i j, M i j = M j i

/-- 特征值：存在非零向量 x 满足 M x = λ x -/
def IsEigenvalue {n : ℕ} (l : ℝ) (M : Matrix n n) : Prop :=
  ∃ (x : ℕ → ℝ), (∃ i, i < n ∧ x i ≠ 0) ∧
    ∀ i, i < n → (∑ j in Finset.range n, M i j * x j) = l * x i

/-! ## GMR -/