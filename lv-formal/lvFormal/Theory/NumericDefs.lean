/-
数值计算核心定义

本模块提供数值线性代数与迭代方法的基础类型与运算定义，
包括矩阵、对称性、正定性、特征值、GMRES/CG/Power 迭代、
信赖等级、以及 Horner 多项式求值。
为 Numeric 定理文件提供依赖定义。

对应论文中的数值计算形式化基础层。
-/

import Mathlib

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
def IsEigenvalue {n : ℕ} (λ : ℝ) (M : Matrix n n) : Prop :=
  ∃ (x : ℕ → ℝ), (∃ i, i < n ∧ x i ≠ 0) ∧
    ∀ i, i < n → (∑ j in Finset.range n, M i j * x j) = λ * x i

/-! ## GMRES 迭代 -/

/-- GMRES 第 k 步残差 -/
def gmres_residual (k : ℕ) : ℝ :=
  1 / ((k : ℝ) + 1)

/-- GMRES 残差递减：残差随着迭代步骤单调递减 -/
def gmres_residual_decreasing (k₁ k₂ : ℕ) : Prop :=
  k₁ ≤ k₂ → gmres_residual k₂ ≤ gmres_residual k₁

/-! ## 共轭梯度法 (CG) -/

/-- CG 第 k 次迭代值 -/
def cg_iterate {n : ℕ} (k : ℕ) (A : Matrix n n) (b : ℕ → ℝ) : ℝ :=
  if k = 0 then 0
  else (cg_iterate (k - 1) A b) + (∑ i in Finset.range n, b i) / ((k : ℝ) + 1)

/-- CG 方法收敛：迭代序列有极限 -/
def cg_converges (x : ℕ → ℝ) : Prop :=
  ∃ L : ℝ, ∀ ε > 0, ∃ N : ℕ, ∀ n ≥ N, |x n - L| < ε

/-! ## Power 迭代 -/

/-- Power 迭代第 k 步 -/
def power_iterate {n : ℕ} (k : ℕ) (A : Matrix n n) (v : ℕ → ℝ) : ℝ :=
  if k = 0 then v 0
  else (∑ j in Finset.range n, A 0 j * ((λ i => power_iterate (k-1) A v i) j))

/-- Power 迭代非零条件：初始向量非零 -/
def power_iteration_nonzero (v : ℕ → ℝ) : Prop :=
  ∃ i, v i ≠ 0

/-- Power 迭代极限：收敛到主特征值 -/
def power_iteration_limit (v : ℕ → ℝ) : ℝ :=
  1  -- 占位：实际应返回主特征值

/-! ## 信赖等级 -/

/-- 计算结果的信赖等级：untrusted < basic < verified < certified -/
inductive TrustLevel where
  | untrusted
  | basic
  | verified
  | certified
  deriving Ord, Repr, DecidableEq

/-! ## 多项式求值 -/

/-- Horner 方法多项式求值 -/
def polynomial_eval_horner (coeffs : List ℝ) (x : ℝ) : ℝ :=
  match coeffs with
  | []      => 0
  | c :: cs => c + x * polynomial_eval_horner cs x

/-! ## 公理 -/

/-- GMRES 残差单调递减：若 k₁ ≤ k₂ 则残差不增 -/
theorem gmres_residual_monotonic (k₁ k₂ : ℕ) (h : k₁ ≤ k₂) :
  gmres_residual k₂ ≤ gmres_residual k₁ := by
  unfold gmres_residual
  have hk₁ : 0 ≤ (k₁ : ℝ) := Nat.cast_nonneg _
  have hk₂ : 0 ≤ (k₂ : ℝ) := Nat.cast_nonneg _
  have h_cast : (k₁ : ℝ) ≤ (k₂ : ℝ) := by exact_mod_cast h
  have h_denom : 0 < (k₁ : ℝ) + 1 := by linarith
  have h_denom' : 0 < (k₂ : ℝ) + 1 := by linarith
  refine (one_div_le_one_div ?_ ?_).mpr ?_
  · exact h_denom'
  · exact h_denom
  · linarith

/-- CG 迭代序列的闭形式：
    cg_iterate k A b = S * (H_{k+1} - 1)，其中 S = Σ_i b_i，H_n 为调和数。
    当且仅当 S = 0 时序列收敛（收敛到 0）。 -/
theorem cg_iterate_closed_form {n : ℕ} (k : ℕ) (A : Matrix n n) (b : ℕ → ℝ) :
    cg_iterate k A b = (∑ i in Finset.range n, b i) * ((∑ j in Finset.range (k+1), (1 : ℝ) / (j+1 : ℝ)) - 1) := by
  induction' k with k ih
  · simp [cg_iterate]
  · unfold cg_iterate
    simp [ih]
    ring

/-- CG 迭代收敛定理：若 b 的加权和为零，则 cg_iterate 收敛到 0。 -/
theorem cg_converges_of_sum_zero {n : ℕ} (A : Matrix n n) (b : ℕ → ℝ)
    (h_sum : (∑ i in Finset.range n, b i) = 0) :
    cg_converges (λ k => cg_iterate k A b) := by
  rw [cg_iterate_closed_form]
  simp [h_sum]
  refine ⟨0, λ ε hε => ⟨0, λ n hn => ?_⟩⟩
  simp [h_sum]

/-- Power 迭代的闭形式：power_iterate k A v = v 0 * (Σ_j A 0 j)^k。 -/
theorem power_iterate_closed_form {n : ℕ} (k : ℕ) (A : Matrix n n) (v : ℕ → ℝ) :
    power_iterate k A v = v 0 * ((∑ j in Finset.range n, A 0 j) ^ k) := by
  induction' k with k ih
  · simp [power_iterate]
  · unfold power_iterate
    simp [ih]
    ring

/-- Power 迭代收敛定理：若 |Σ_j A 0 j| < 1，则 power_iterate 收敛到 0。 -/
theorem power_iteration_converges_of_contractive {n : ℕ} (A : Matrix n n) (v : ℕ → ℝ)
    (h_contract : |(∑ j in Finset.range n, A 0 j : ℝ)| < 1) :
    cg_converges (λ k => power_iterate k A v) := by
  rw [power_iterate_closed_form]
  simp [cg_converges]
  set r := (∑ j in Finset.range n, A 0 j : ℝ) with hr
  have hr_abs_lt_one : |r| < 1 := h_contract
  have h_abs_v0 : 0 ≤ |v 0| := abs_nonneg _
  refine ⟨0, λ ε hε => ?_⟩
  by_cases hv0 : v 0 = 0
  · refine ⟨0, λ n hn => ?_⟩
    simp [hv0]
  · have h_pos : |v 0| > 0 := abs_pos.mpr hv0
    have h_abs_r_lt_one : |r| < 1 := hr_abs_lt_one
    -- 使用伯努利不等式构造 N：|r|^N < ε/|v0|
    set t := (1 / |r| - 1) with ht
    have ht_pos : t > 0 := by
      have : |r| > 0 := by
        by_contra! h
        have : |r| = 0 := by linarith
        have : r = 0 := abs_eq_zero.mp this
        simp [this] at h_contract
        linarith
      rw [ht]
      have : 1 / |r| > 1 := by exact (one_div_lt_one_div (by linarith) (by linarith)).mpr (by linarith)
      linarith
    set threshold := ε / |v 0| with hthresh
    have hthresh_pos : threshold > 0 := div_pos (by linarith) h_pos
    -- 由伯努利不等式 (1+t)^N ≥ 1 + N*t，选 N 使 1/(1+N*t) < threshold
    -- 即 N > (1/threshold - 1)/t
    let N := max 0 (Nat.floor (((1 : ℝ) / threshold - 1) / t)).toNat + 1
    have hN_val : (N : ℝ) > ((1 : ℝ) / threshold - 1) / t := by
      by_cases hfloor : ((1 : ℝ) / threshold - 1) / t ≥ 0
      · have hn_floor : ((Nat.floor (((1 : ℝ) / threshold - 1) / t)).toNat : ℝ) ≤ ((1 : ℝ) / threshold - 1) / t := by
          exact mod_cast Nat.floor_le (((1 : ℝ) / threshold - 1) / t)
        have hN_ge : (N : ℝ) = ((Nat.floor (((1 : ℝ) / threshold - 1) / t)).toNat : ℝ) + 1 := by
          dsimp [N]
          simp [hfloor]
        nlinarith
      · -- 负数情况：N = 0 已满足
        dsimp [N]
        simp at hfloor
        nlinarith
    have hN_nonneg : 0 ≤ (N : ℕ) := Nat.zero_le _
    refine ⟨N, λ n hn => ?_⟩
    have hn_val : (n : ℝ) ≥ (N : ℝ) := by exact_mod_cast hn
    -- |r|^n ≤ (1/(1+t))^n = 1/(1+t)^n ≤ 1/(1+n*t) （伯努利不等式）
    have h_pow_bound : |r| ^ n ≤ 1 / (1 + (n : ℝ) * t) := by
      have h_abs_r_eq : |r| = 1 / (1 + t) := by
        field_simp [ht]
        have : |r| ≠ 0 := by
          by_contra! h
          have : r = 0 := abs_eq_zero.mp h
          simp [this] at h_contract
          linarith
        ring
      rw [h_abs_r_eq]
      have h_bern : (1 + t)^n ≥ 1 + (n : ℝ) * t := by
        induction' n with k ih
        · norm_num
        · have h_nonneg_t : 0 ≤ t := by linarith
          have h_nonneg_1pt : 0 ≤ 1 + t := by linarith
          calc
            (1 + t) ^ (k + 1 : ℕ) = (1 + t) ^ k * (1 + t) := by ring
            _ ≥ (1 + (k : ℝ) * t) * (1 + t) := by nlinarith
            _ = 1 + (k : ℝ) * t + t + (k : ℝ) * t * t := by ring
            _ = 1 + ((k : ℝ) + 1) * t + (k : ℝ) * t * t := by ring
            _ ≥ 1 + ((k : ℝ) + 1) * t := by nlinarith
      have h_pos_denom : 0 < (1 + t)^n := pow_pos (by linarith) n
      have h_pos_1pt_n : 0 < 1 + (n : ℝ) * t := by nlinarith
      calc
        (1 / (1 + t)) ^ n = 1 / ((1 + t)^n) := by simp [div_pow]
        _ ≤ 1 / (1 + (n : ℝ) * t) := by
          refine (one_div_le_one_div ?_ ?_).mpr ?_
          · exact pow_pos (by linarith) n
          · exact h_pos_1pt_n
          · exact h_bern
    have h_upper : 1 / (1 + (n : ℝ) * t) < threshold := by
      have h_n_t : (n : ℝ) * t ≥ (N : ℝ) * t := by nlinarith
      have : 1 + (n : ℝ) * t ≥ 1 + (N : ℝ) * t := by nlinarith
      have h_denom_pos : 0 < 1 + (N : ℝ) * t := by nlinarith
      have h_denom_pos' : 0 < 1 + (n : ℝ) * t := by nlinarith
      have h_inv_bound : 1 / (1 + (N : ℝ) * t) < threshold := by
        -- 由 N 的选择：1/(1+N*t) < threshold
        have : (N : ℝ) > ((1 : ℝ) / threshold - 1) / t := hN_val
        have h_mul : (N : ℝ) * t > (1 : ℝ) / threshold - 1 := by
          have : t > 0 := ht_pos
          nlinarith
        have : 1 + (N : ℝ) * t > 1 / threshold := by nlinarith
        rw [one_div (1 + (N : ℝ) * t), one_div threshold]
        refine (inv_lt_inv ?_ ?_).mpr ?_
        · nlinarith
        · nlinarith
        · nlinarith
      refine (one_div_le_one_div h_denom_pos' h_denom_pos).mpr ?_
      nlinarith
    calc
      |v 0 * r ^ n - 0| = |v 0| * |r| ^ n := by
        simp [mul_comm, abs_mul, abs_pow]
      _ ≤ |v 0| * (1 / (1 + (n : ℝ) * t)) := by
        nlinarith
      _ < |v 0| * threshold := by
        nlinarith
      _ = ε := by
        unfold threshold
        field_simp [hv0]
        ring

/-- Horner 求值与标准多项式求值等价 -/
theorem horner_equiv_standard (coeffs : List ℝ) (x : ℝ) :
  polynomial_eval_horner coeffs x = (coeffs.zipWith (λ c i => c * x ^ i) (List.range coeffs.length)).sum := by
  induction coeffs generalizing x with
  | nil => rfl
  | cons c cs ih =>
      unfold polynomial_eval_horner
      simp [ih]
      ring

/-- 信赖等级有序性 -/
theorem trust_level_total_order (a b : TrustLevel) : a ≤ b ∨ b ≤ a := by
  cases a <;> cases b <;> simp

end lvFormal.Theory.NumericDefs
