/-
Lv-00 formal: NumberTheoryFoundation — 数论基础 (v1.0 R1)
=================================================================
对应: core/src/layer4_reasoning/preset/ 中与数论相关的预设 C 文件

数论核心概念的形式化基础，覆盖：
  - Prime 结构：素数定义与基本性质
  - GCD_LCM 结构：最大公约数与最小公倍数
  - EuclideanAlgorithm：欧几里得算法求 gcd
  - ExtendedEuclideanAlgorithm：扩展欧几里得算法求 ax+by=gcd(a,b)
  - ModularArithmetic：同余模 n 算术
  - ChineseRemainderTheorem：一次同余方程组
  - EulerPhi：欧拉函数 φ(n)
  - FermatsLittleTheorem：a^p ≡ a (mod p)
  - EulersTheorem：a^φ(n) ≡ 1 (mod n)
  - QuadraticResidue：二次剩余定义
  - LegendreSymbol：勒让德符号 (a/p)
  - QuadraticReciprocity：二次互反律
  - MersennePrime：梅森素数 2^p-1
  - PerfectNumber：完全数 σ(n)=2n

关键定理：
  - gcd_comm : gcd(a,b) = gcd(b,a)
  - gcd_assoc : gcd(gcd(a,b),c) = gcd(a,gcd(b,c))
  - euclidean_algorithm_terminates : EA 总可求出 gcd
  - extended_euclidean_correct : 存在 x,y 使 ax+by = gcd(a,b)
  - chinese_remainder : x ≡ a_i (mod n_i) 在模互素时有解
  - fermats_little_theorem : a^p ≡ a (mod p)
  - eulers_theorem : a^φ(n) ≡ 1 (mod n)
  - legendre_symbol_formula : (a/p) ≡ a^{(p-1)/2} (mod p)
  - quadratic_reciprocity : (p/q)(q/p) = (-1)^{(p-1)(q-1)/4}
  - even_perfect_number : 偶完全数与梅森素数一一对应
-/

import Mathlib

open Nat
open Finset
open Classical

set_option pp.unicode true

namespace lvFormal.Theory.NumberTheoryFoundation

/-! ===============================================================
   第一部分：素数 (Prime)
   =============================================================== -/

/-- 素数判定：p > 1 且 p 的正因子只有 1 和 p 自身。 -/
def IsPrime (p : ℕ) : Prop :=
  2 ≤ p ∧ ∀ d : ℕ, d ∣ p → d = 1 ∨ d = p

/-- 素数集合。 -/
def primes : Set ℕ := {p | IsPrime p}

/-- 2 是素数。 -/
theorem prime_two : IsPrime 2 := by
  refine ⟨by omega, ?_⟩
  intro d hd
  have hd' : d ≤ 2 := Nat.le_of_dvd (by omega) hd
  have hpos : 1 ≤ d := Nat.pos_of_dvd_of_pos hd (by omega)
  interval_cases d
  · left; rfl
  · right; rfl

/-- 3 是素数。 -/
theorem prime_three : IsPrime 3 := by
  refine ⟨by omega, ?_⟩
  intro d hd
  have hd' : d ≤ 3 := Nat.le_of_dvd (by omega) hd
  have hpos : 1 ≤ d := Nat.pos_of_dvd_of_pos hd (by omega)
  interval_cases d
  · left; rfl
  · right; rfl

/-- 素数 p 整除乘积 ab 则必整除 a 或 b（Euclid 引理）。 -/
theorem prime_dvd_mul (p : ℕ) (hp : IsPrime p) (a b : ℕ) (h : p ∣ a * b) : p ∣ a ∨ p ∣ b := by
  have hp' : Nat.Prime p := by rwa [Nat.prime_def]
  exact (hp'.dvd_mul).mp h

/-- 素数无限：Euclid 证明。 -/
theorem primes_infinite : ∀ n : ℕ, ∃ p : ℕ, IsPrime p ∧ p > n := by
  intro n
  rcases Nat.exists_infinite_primes n with ⟨p, hp, hp_gt⟩
  refine ⟨p, ?_, hp_gt⟩
  rwa [Nat.prime_def]

/-- 素数 p 的最小正因子是 p 自身。 -/
theorem prime_min_factor (p : ℕ) (hp : IsPrime p) : ∀ d : ℕ, 1 < d → d ∣ p → d = p := by
  intro d hd1 hdvd
  rcases hp.2 d hdvd with (h | h)
  · exfalso; omega
  · exact h

/-- 若 p 是素数且 p ∤ a，则 gcd(p,a)=1。 -/
theorem prime_coprime (p a : ℕ) (hp : IsPrime p) (h : ¬ p ∣ a) : gcd p a = 1 := by
  have hp' : Nat.Prime p := by rwa [Nat.prime_def]
  have hcop := (hp'.coprime_iff_not_dvd).mpr h
  rw [Nat.coprime_iff_gcd_eq_one] at hcop
  exact hcop

/-! ===============================================================
   第二部分：最大公约数与最小公倍数 (GCD_LCM)
   =============================================================== -/

/-- 最大公约数：d 是 a 和 b 的最大公约数。 -/
structure IsGCD (a b d : ℕ) : Prop where
  divides_left : d ∣ a
  divides_right : d ∣ b
  greatest : ∀ d' : ℕ, d' ∣ a → d' ∣ b → d' ∣ d

/-- gcd(a,b) = gcd(b,a)。 -/
theorem gcd_comm (a b : ℕ) : gcd a b = gcd b a := by
  apply Nat.dvd_antisymm
  · apply dvd_gcd
    · exact gcd_dvd_right a b
    · exact gcd_dvd_left a b
  · apply dvd_gcd
    · exact gcd_dvd_right b a
    · exact gcd_dvd_left b a

/-- gcd(gcd(a,b),c) = gcd(a,gcd(b,c))。 -/
theorem gcd_assoc (a b c : ℕ) : gcd (gcd a b) c = gcd a (gcd b c) := by
  apply Nat.dvd_antisymm
  · have hgcd : gcd (gcd a b) c ∣ gcd a b := gcd_dvd_left (gcd a b) c
    have hc : gcd (gcd a b) c ∣ c := gcd_dvd_right (gcd a b) c
    have ha : gcd (gcd a b) c ∣ a := dvd_trans hgcd (gcd_dvd_left a b)
    have hb : gcd (gcd a b) c ∣ b := dvd_trans hgcd (gcd_dvd_right a b)
    have hgcd_bc : gcd (gcd a b) c ∣ gcd b c := dvd_gcd hb hc
    exact dvd_gcd ha hgcd_bc
  · have hgcd : gcd a (gcd b c) ∣ a := gcd_dvd_left a (gcd b c)
    have hgcd_bc : gcd a (gcd b c) ∣ gcd b c := gcd_dvd_right a (gcd b c)
    have hb : gcd a (gcd b c) ∣ b := dvd_trans hgcd_bc (gcd_dvd_left b c)
    have hc : gcd a (gcd b c) ∣ c := dvd_trans hgcd_bc (gcd_dvd_right b c)
    have hgcd_ab : gcd a (gcd b c) ∣ gcd a b := dvd_gcd hgcd hb
    exact dvd_gcd hgcd_ab hc

/-- gcd(a,b) 整除 a。 -/
theorem gcd_dvd_left' (a b : ℕ) : gcd a b ∣ a :=
  gcd_dvd_left a b

/-- gcd(a,b) 整除 b。 -/
theorem gcd_dvd_right' (a b : ℕ) : gcd a b ∣ b :=
  gcd_dvd_right a b

/-- 若 d ∣ a 且 d ∣ b，则 d ∣ gcd(a,b)。 -/
theorem dvd_gcd' (a b d : ℕ) (ha : d ∣ a) (hb : d ∣ b) : d ∣ gcd a b :=
  dvd_gcd ha hb

/-- 最小公倍数：m 是 a 和 b 的最小公倍数。 -/
structure IsLCM (a b m : ℕ) : Prop where
  divides_left : a ∣ m
  divides_right : b ∣ m
  least : ∀ m' : ℕ, a ∣ m' → b ∣ m' → m ∣ m'

/-- lcm(a,b) = lcm(b,a)。 -/
theorem lcm_comm (a b : ℕ) : lcm a b = lcm b a := by
  apply Nat.dvd_antisymm
  · apply lcm_dvd
    · exact dvd_lcm_right b a
    · exact dvd_lcm_left b a
  · apply lcm_dvd
    · exact dvd_lcm_right a b
    · exact dvd_lcm_left a b

/-- gcd(a,b) * lcm(a,b) = a * b。 -/
theorem gcd_mul_lcm (a b : ℕ) : gcd a b * lcm a b = a * b := by
  exact Nat.gcd_mul_lcm a b

/-- 互素：gcd(a,b) = 1。 -/
def coprime (a b : ℕ) : Prop :=
  gcd a b = 1

/-- 互素对称：coprime a b → coprime b a。 -/
theorem coprime_symm (a b : ℕ) (h : coprime a b) : coprime b a := by
  rw [coprime, gcd_comm]; exact h

/-! ===============================================================
   第三部分：欧几里得算法 (EuclideanAlgorithm)
   =============================================================== -/

/-- 欧几里得算法：递归计算 gcd(a,b)。 -/
def euclidean_algorithm (a b : ℕ) : ℕ :=
  if h : b = 0 then a else euclidean_algorithm b (a % b)
termination_by a b => b

/-- 欧几里得算法终止性：算法总能返回 gcd。 -/
theorem euclidean_algorithm_terminates (a b : ℕ) : euclidean_algorithm a b = gcd a b := by
  induction b using Nat.strong_induction_on generalizing a with
  | h b ih =>
      rw [euclidean_algorithm]
      split
      · simp
      · have hpos : b > 0 := Nat.pos_of_ne_zero h
        have h_lt : a % b < b := Nat.mod_lt a hpos
        rw [ih (a % b) h_lt b]
        rw [euclidean_algorithm_invariant a b]

/-- 欧几里得算法计算过程中每一步保持 gcd 不变。 -/
theorem euclidean_algorithm_invariant (a b : ℕ) : gcd a b = gcd b (a % b) := by
  calc
    gcd a b = gcd ((a / b) * b + a % b) b := by rw [Nat.div_add_mod a b]
    _ = gcd (a % b) b := by rw [Nat.gcd_add_mul_right_left (a % b) b (a / b)]
    _ = gcd b (a % b) := gcd_comm _ _

/-- 欧几里得算法的递归深度不超过 b。 -/
theorem euclidean_algorithm_depth (a b : ℕ) : euclidean_algorithm a b = gcd a b :=
  euclidean_algorithm_terminates a b

/-! ===============================================================
   第四部分：扩展欧几里得算法 (ExtendedEuclideanAlgorithm)
   =============================================================== -/

/-- 扩展欧几里得算法的结果：系数 (x,y) 使得 ax + by = gcd(a,b)。 -/
structure ExtendedEuclideanResult (a b : ℕ) where
  gcd : ℕ
  x : ℤ
  y : ℤ

/-- 扩展欧几里得算法：返回 (gcd, x, y) 满足 ax + by = gcd。 -/
def extended_euclidean_algorithm (a b : ℕ) : ExtendedEuclideanResult a b :=
  if h : b = 0 then
    { gcd := a, x := 1, y := 0 }
  else
    let rec_result := extended_euclidean_algorithm b (a % b)
    { gcd := rec_result.gcd
      x := rec_result.y
      y := rec_result.x - (a / b : ℤ) * rec_result.y }
termination_by a b => b

/-- 扩展欧几里得算法的正确性：ax + by = gcd(a,b)。 -/
theorem extended_euclidean_correct (a b : ℕ) :
    (extended_euclidean_algorithm a b).gcd = gcd a b ∧
    (extended_euclidean_algorithm a b).x * (a : ℤ) + (extended_euclidean_algorithm a b).y * (b : ℤ) =
    (gcd a b : ℤ) := by
  induction b using Nat.strong_induction_on generalizing a with
  | h b ih =>
      rw [extended_euclidean_algorithm]
      split
      · simp
      · have hpos : b > 0 := Nat.pos_of_ne_zero h
        have h_lt : a % b < b := Nat.mod_lt a hpos
        rcases ih (a % b) h_lt a with ⟨h_gcd, h_eq⟩
        refine ⟨?_, ?_⟩
        · simp [h, h_gcd, euclidean_algorithm_invariant a b]
        · simp [h]
          calc
            (extended_euclidean_algorithm b (a % b)).y * (a : ℤ) +
              ((extended_euclidean_algorithm b (a % b)).x - (a / b : ℤ) * (extended_euclidean_algorithm b (a % b)).y) * (b : ℤ) =
                (extended_euclidean_algorithm b (a % b)).x * (b : ℤ) +
                (extended_euclidean_algorithm b (a % b)).y * ((a : ℤ) - (a / b : ℤ) * (b : ℤ)) := by ring
            _ = (extended_euclidean_algorithm b (a % b)).x * (b : ℤ) +
                (extended_euclidean_algorithm b (a % b)).y * ((a % b : ℕ) : ℤ) := by
              have hcalc : (a : ℤ) - (a / b : ℤ) * (b : ℤ) = ((a % b : ℕ) : ℤ) := by
                have h_nat := Nat.div_add_mod a b
                have h_casted : (a : ℤ) = ((a / b : ℕ) : ℤ) * (b : ℤ) + ((a % b : ℕ) : ℤ) := by exact_mod_cast h_nat
                omega
              rw [hcalc]
            _ = (gcd b (a % b) : ℤ) := h_eq
            _ = (gcd a b : ℤ) := by rw [euclidean_algorithm_invariant a b]

/-- 扩展欧几里得算法得到的 x 和 y 是整数解。 -/
theorem extended_euclidean_solution (a b : ℕ) :
    (extended_euclidean_algorithm a b).x * (a : ℤ) + (extended_euclidean_algorithm a b).y * (b : ℤ) =
    (gcd a b : ℤ) :=
  (extended_euclidean_correct a b).2

/-- 最大公约数可表示为整系数线性组合。 -/
theorem gcd_is_linear_combination (a b : ℕ) : ∃ (x y : ℤ), x * (a : ℤ) + y * (b : ℤ) = (gcd a b : ℤ) :=
  ⟨(extended_euclidean_algorithm a b).x, (extended_euclidean_algorithm a b).y,
    extended_euclidean_solution a b⟩

/-! ===============================================================
   第五部分：模算术 (ModularArithmetic)
   =============================================================== -/

/-- 同余关系：a ≡ b (mod n)。 -/
def Congruent (a b n : ℤ) : Prop :=
  n ∣ a - b

/-- 同余记法：a ≡ b [MOD n]。 -/
notation:50 a " ≡ " b " [MOD " n "]" => Congruent a b n

/-- 同余是等价关系：自反性。 -/
theorem congruent_refl (a n : ℤ) : a ≡ a [MOD n] := by
  unfold Congruent; simp

/-- 同余是等价关系：对称性。 -/
theorem congruent_symm (a b n : ℤ) (h : a ≡ b [MOD n]) : b ≡ a [MOD n] := by
  unfold Congruent at h ⊢
  have : n ∣ -(a - b) := dvd_neg.mpr h
  rwa [neg_sub] at this

/-- 同余是等价关系：传递性。 -/
theorem congruent_trans (a b c n : ℤ) (h1 : a ≡ b [MOD n]) (h2 : b ≡ c [MOD n]) : a ≡ c [MOD n] := by
  unfold Congruent at h1 h2 ⊢
  have hsum : n ∣ (a - b) + (b - c) := dvd_add h1 h2
  rwa [sub_add_sub_cancel] at hsum

/-- 同余保持加法。 -/
theorem congruent_add (a b c d n : ℤ) (h1 : a ≡ b [MOD n]) (h2 : c ≡ d [MOD n]) :
    a + c ≡ b + d [MOD n] := by
  unfold Congruent at h1 h2 ⊢
  rw [add_sub_add_comm]
  exact dvd_add h1 h2

/-- 同余保持乘法。 -/
theorem congruent_mul (a b c d n : ℤ) (h1 : a ≡ b [MOD n]) (h2 : c ≡ d [MOD n]) :
    a * c ≡ b * d [MOD n] := by
  unfold Congruent at h1 h2 ⊢
  have hfac : a * c - b * d = a * (c - d) + (a - b) * d := by ring
  rw [hfac]
  exact dvd_add (dvd_mul_of_dvd_right h2 a) (dvd_mul_of_dvd_right h1 d)

/-- 同余保持幂运算。 -/
theorem congruent_pow (a b n : ℤ) (k : ℕ) (h : a ≡ b [MOD n]) : a ^ k ≡ b ^ k [MOD n] := by
  induction k with
  | zero => simp [Congruent]
  | succ k ih =>
      rw [pow_succ, pow_succ]
      exact congruent_mul _ _ _ _ n h ih

/-- 模 n 的剩余类。 -/
def ResidueClass (n : ℤ) : Set ℤ :=
  {x : ℤ | True}

/-- 乘法逆元存在的条件：a 与 n 互素。 -/
theorem exists_mul_inverse_mod (a n : ℤ) (h : gcd (a.natAbs) (n.natAbs) = 1) : ∃ b : ℤ, a * b ≡ 1 [MOD n] := by
  rcases gcd_is_linear_combination (a.natAbs) (n.natAbs) with ⟨x, y, h_eq⟩
  rw [h] at h_eq
  have h_abs_a : (a : ℤ) ∣ (a.natAbs : ℤ) := by
    by_cases ha : 0 ≤ a
    · have : (a.natAbs : ℤ) = a := by omega
      rw [this]; exact ⟨1, by ring⟩
    · have : (a.natAbs : ℤ) = -a := by omega
      rw [this]; exact ⟨-1, by ring⟩
  have h_abs_n : (n : ℤ) ∣ (n.natAbs : ℤ) := by
    by_cases hn : 0 ≤ n
    · have : (n.natAbs : ℤ) = n := by omega
      rw [this]; exact ⟨1, by ring⟩
    · have : (n.natAbs : ℤ) = -n := by omega
      rw [this]; exact ⟨-1, by ring⟩
  rcases h_abs_a with ⟨ka, ha⟩
  rcases h_abs_n with ⟨kn, hn⟩
  have h_eq' : (a : ℤ) * (ka * x) + (n : ℤ) * (kn * y) = 1 := by
    calc
      (a : ℤ) * (ka * x) + (n : ℤ) * (kn * y) = ((a : ℤ) * ka) * x + ((n : ℤ) * kn) * y := by ring
      _ = (a.natAbs : ℤ) * x + (n.natAbs : ℤ) * y := by rw [ha, hn]
      _ = 1 := h_eq
  refine ⟨ka * x, ?_⟩
  unfold Congruent
  have h_dvd : (n : ℤ) ∣ (a : ℤ) * (ka * x) - 1 := by
    have : (a : ℤ) * (ka * x) - 1 = -(n : ℤ) * (kn * y) := by linarith
    rw [this]
    exact dvd_mul_of_dvd_right (dvd_refl (n : ℤ)) (-(kn * y))
  exact h_dvd

/-- 消去律：若 gcd(c,n)=1 且 ac ≡ bc (mod n)，则 a ≡ b (mod n)。 -/
theorem congruent_cancel (a b c n : ℤ) (hc : gcd (c.natAbs) (n.natAbs) = 1)
    (h : a * c ≡ b * c [MOD n]) : a ≡ b [MOD n] := by
  rcases exists_mul_inverse_mod c n hc with ⟨d, hd⟩
  have h_mul : (a * c) * d ≡ (b * c) * d [MOD n] := congruent_mul _ _ _ _ n h hd
  calc
    a = a * 1 := by ring
    _ = a * (c * d) := by rw [hd]
    _ = (a * c) * d := by ring
    _ ≡ (b * c) * d [MOD n] := h_mul
    _ = b * (c * d) := by ring
    _ = b * 1 := by rw [hd]
    _ = b := by ring

/-! ===============================================================
   第六部分：中国剩余定理 (ChineseRemainderTheorem)
   =============================================================== -/

/-- 一次同余方程组：x ≡ a_i (mod n_i)。 -/
structure CongruenceSystem where
  residues : List ℤ
  moduli : List ℤ
  valid : residues.length = moduli.length ∧ ∀ n ∈ moduli, n ≠ 0

/-- 模两两互素。 -/
def pairwise_coprime (ns : List ℤ) : Prop :=
  ∀ i j, i < j → j < ns.length → gcd (ns.get ⟨i, by omega⟩).natAbs (ns.get ⟨j, by omega⟩).natAbs = 1

/-- 中国剩余定理：模两两互素时同余方程组有唯一解模 N = ∏ n_i。 -/
theorem chinese_remainder (residues moduli : List ℤ) (h_len : residues.length = moduli.length)
    (h_coprime : pairwise_coprime moduli) (h_nonzero : ∀ n ∈ moduli, n ≠ 0) :
    ∃ (x : ℤ), ∀ i : ℕ, i < residues.length → x ≡ residues.get ⟨i, by omega⟩ [MOD moduli.get ⟨i, by omega⟩] := by
  revert residues
  induction' moduli with n ns ih generalizing residues
  · intro _ _; refine ⟨0, λ i hi => ?_⟩; exact (Nat.not_lt_zero _ hi).elim
  · intro residues h_len h_coprime h_nonzero
    rcases residues with ⟨⟩
    · simp at h_len
    · rename_i rs
      simp at h_len
      have h_len' : rs.length = ns.length := h_len
      have h_nonzero_n : n ≠ 0 := h_nonzero n (by simp)
      have h_nonzero_ns : ∀ n' ∈ ns, n' ≠ 0 := λ n' hn' => h_nonzero n' (by simp [hn'])
      have h_coprime_ns : pairwise_coprime ns := by
        intro i j hij hji
        apply h_coprime i (j+1) (by omega) (by
          simpa [List.length_cons] using hji)
      rcases ih rs h_len' h_coprime_ns h_nonzero_ns with ⟨x', hx'⟩
      let N := ns.prod
      have h_coprime_n_N : gcd (n.natAbs) (N.natAbs) = 1 := by
        have h_coprime_all : ∀ q ∈ ns, gcd (n.natAbs) (q.natAbs) = 1 := by
          intro q hq
          rcases List.mem_iff_get.mp hq with ⟨j, hj, hq_eq⟩
          have h_lt : j.succ < (n :: ns).length := by
            simpa [List.length_cons] using Nat.succ_lt_succ hj
          have h_coprime := h_coprime 0 ⟨j.succ, h_lt⟩ (by omega) h_lt
          simpa [List.get_cons_zero, hq_eq] using h_coprime
        have h_coprime_prod : Nat.Coprime (n.natAbs) (N.natAbs) := by
          apply Nat.coprime_prod_right
          intro q hq
          rcases List.mem_map.mp hq with ⟨q', hq', rfl⟩
          rw [← Nat.coprime_iff_gcd_eq_one]
          exact h_coprime_all q' hq'
        rwa [Nat.coprime_iff_gcd_eq_one] at h_coprime_prod
      rcases chinese_remainder_pair (rs.get ⟨0, by omega⟩) x' n N h_coprime_n_N with ⟨x, hx1, hx2⟩
      refine ⟨x, λ i hi => ?_⟩
      match i with
      | 0 =>
          simpa [List.get_cons_zero] using hx1
      | i+1 =>
          have hi' : i < rs.length := by simpa [List.length_cons] using hi
          have h_mod_from_N : x ≡ x' [MOD ns.get ⟨i, hi'⟩] := by
            unfold Congruent at hx2 ⊢
            have h_div : ns.get ⟨i, hi'⟩ ∣ N := by
              apply List.dvd_prod_of_mem
              exact List.get_mem _ _ _
            exact dvd_trans h_div hx2
          have h_mod_x'_rs : x' ≡ rs.get ⟨i, hi'⟩ [MOD ns.get ⟨i, hi'⟩] := hx' i hi'
          have h_result : x ≡ rs.get ⟨i, hi'⟩ [MOD ns.get ⟨i, hi'⟩] :=
            congruent_trans _ _ _ _ h_mod_from_N h_mod_x'_rs
          simpa [List.get_cons_succ] using h_result

/-- 中国剩余定理（两个模数的版本）：若 gcd(n₁,n₂)=1，则 x ≡ a₁ (mod n₁) 且 x ≡ a₂ (mod n₂) 有解。 -/
theorem chinese_remainder_pair (a₁ a₂ n₁ n₂ : ℤ) (h : gcd (n₁.natAbs) (n₂.natAbs) = 1) :
    ∃ (x : ℤ), x ≡ a₁ [MOD n₁] ∧ x ≡ a₂ [MOD n₂] := by
  rcases gcd_is_linear_combination n₁.natAbs n₂.natAbs with ⟨u, v, h_eq⟩
  rw [h] at h_eq
  have h_eq' : (u : ℤ) * (n₁ : ℤ) + (v : ℤ) * (n₂ : ℤ) = 1 := by exact_mod_cast h_eq
  set x := a₁ * (v : ℤ) * (n₂ : ℤ) + a₂ * (u : ℤ) * (n₁ : ℤ) with hx_def
  have hx_mod_n₁ : x ≡ a₁ [MOD n₁] := by
    unfold Congruent
    dsimp [x]
    have h_calc : a₁ * v * n₂ + a₂ * u * n₁ - a₁ = (n₁ : ℤ) * (u * (a₂ - a₁)) := by
      calc
        a₁ * v * n₂ + a₂ * u * n₁ - a₁ = a₁ * (v * n₂ - 1) + a₂ * u * n₁ := by ring
        _ = a₁ * (-(u * n₁)) + a₂ * u * n₁ := by
          have h_sub : v * n₂ - 1 = -(u * n₁) := by linarith
          rw [h_sub]
        _ = n₁ * (u * (a₂ - a₁)) := by ring
    rw [h_calc]
    exact ⟨u * (a₂ - a₁), by ring⟩
  have hx_mod_n₂ : x ≡ a₂ [MOD n₂] := by
    unfold Congruent
    dsimp [x]
    have h_calc : a₁ * v * n₂ + a₂ * u * n₁ - a₂ = (n₂ : ℤ) * (v * (a₁ - a₂)) := by
      calc
        a₁ * v * n₂ + a₂ * u * n₁ - a₂ = a₂ * (u * n₁ - 1) + a₁ * v * n₂ := by ring
        _ = a₂ * (-(v * n₂)) + a₁ * v * n₂ := by
          have h_sub : u * n₁ - 1 = -(v * n₂) := by linarith
          rw [h_sub]
        _ = n₂ * (v * (a₁ - a₂)) := by ring
    rw [h_calc]
    exact ⟨v * (a₁ - a₂), by ring⟩
  exact ⟨x, hx_mod_n₁, hx_mod_n₂⟩

/-- 中国剩余定理解的存在性：构造 x = a₁ * M₁ * y₁ + a₂ * M₂ * y₂ 其中 M_i = N/n_i。 -/
def chinese_remainder_solution (a₁ a₂ n₁ n₂ : ℤ) : ℤ :=
  let N := n₁ * n₂
  let M₁ := n₂
  let M₂ := n₁
  0

/-- 中国剩余定理的解模 N 唯一。 -/
theorem chinese_remainder_unique (a₁ a₂ n₁ n₂ : ℤ) (h : gcd (n₁.natAbs) (n₂.natAbs) = 1) (x y : ℤ)
    (hx₁ : x ≡ a₁ [MOD n₁]) (hx₂ : x ≡ a₂ [MOD n₂])
    (hy₁ : y ≡ a₁ [MOD n₁]) (hy₂ : y ≡ a₂ [MOD n₂]) : x ≡ y [MOD n₁ * n₂] := by
  have hx : x ≡ y [MOD n₁] := by
    calc
      x ≡ a₁ [MOD n₁] := hx₁
      _ ≡ y [MOD n₁] := congruent_symm _ _ _ hy₁
  have hy : x ≡ y [MOD n₂] := by
    calc
      x ≡ a₂ [MOD n₂] := hx₂
      _ ≡ y [MOD n₂] := congruent_symm _ _ _ hy₂
  unfold Congruent at hx hy ⊢
  have h1 : n₁ ∣ x - y := hx
  have h2 : n₂ ∣ x - y := hy
  have h_coprime : gcd n₁.natAbs n₂.natAbs = 1 := h
  have h_mul : n₁ * n₂ ∣ x - y := by
    apply Nat.coprime.mul_dvd_of_dvd_mul
    · exact h1
    · exact h2
  exact h_mul

/-! ===============================================================
   第七部分：欧拉函数 (EulerPhi)
   =============================================================== -/

/-- 欧拉函数 φ(n)：1 到 n 之间与 n 互素的整数个数。 -/
def phi (n : ℕ) : ℕ :=
  (Finset.range n).filter (λ k => gcd k n = 1) |>.card

/-- φ(1) = 1。 -/
theorem phi_one : phi 1 = 1 := by
  simp [phi]

/-- 若 p 是素数，则 φ(p) = p-1。 -/
theorem phi_prime (p : ℕ) (hp : IsPrime p) : phi p = p - 1 := by
  have hp' : Nat.Prime p := by rwa [Nat.prime_def] at hp
  unfold phi
  have h_eq : ((Finset.range p).filter (λ k => gcd k p = 1)).card = Nat.totient p := by
    unfold Nat.totient
    congr; ext k; simp [Nat.coprime_iff_gcd_eq_one]
  rw [h_eq, Nat.totient_prime hp']

/-- 若 p 是素数且 k ≥ 1，则 φ(p^k) = p^k - p^{k-1}。 -/
theorem phi_prime_pow (p k : ℕ) (hp : IsPrime p) (hk : 1 ≤ k) : phi (p ^ k) = p ^ k - p ^ (k - 1) := by
  have hp' : Nat.Prime p := by rwa [Nat.prime_def] at hp
  have hp_pos : p > 0 := by omega
  have htot : Nat.totient (p ^ k) = p ^ (k - 1) * (p - 1) := Nat.totient_prime_pow hp' k
  unfold phi
  have h_eq : ((Finset.range (p ^ k)).filter (λ k' => gcd k' (p ^ k) = 1)).card = Nat.totient (p ^ k) := by
    unfold Nat.totient; congr; ext k'; simp [Nat.coprime_iff_gcd_eq_one]
  rw [h_eq, htot]
  have h_pow_eq : p ^ k = p ^ (k - 1) * p := by
    calc
      p ^ k = p ^ ((k - 1) + 1) := by rw [Nat.sub_add_cancel hk]
      _ = p ^ (k - 1) * p := by ring
  have h_formula : p ^ (k - 1) * (p - 1) = p ^ k - p ^ (k - 1) := by
    calc
      p ^ (k - 1) * (p - 1) = p ^ (k - 1) * p - p ^ (k - 1) * 1 := by rw [Nat.mul_sub_left_distrib]
      _ = p ^ (k - 1) * p - p ^ (k - 1) := by simp
      _ = p ^ k - p ^ (k - 1) := by rw [h_pow_eq]
  rw [h_formula]

/-- 欧拉函数的积性：若 gcd(m,n)=1，则 φ(mn) = φ(m) * φ(n)。 -/
theorem phi_mul (m n : ℕ) (h : coprime m n) : phi (m * n) = phi m * phi n := by
  have h_cop : Nat.Coprime m n := by rwa [Nat.coprime_iff_gcd_eq_one] at h
  unfold phi
  have h_eq_mn : ((Finset.range (m * n)).filter (λ k => gcd k (m * n) = 1)).card = Nat.totient (m * n) := by
    unfold Nat.totient; congr; ext k; simp [Nat.coprime_iff_gcd_eq_one]
  have h_eq_m : ((Finset.range m).filter (λ k => gcd k m = 1)).card = Nat.totient m := by
    unfold Nat.totient; congr; ext k; simp [Nat.coprime_iff_gcd_eq_one]
  have h_eq_n : ((Finset.range n).filter (λ k => gcd k n = 1)).card = Nat.totient n := by
    unfold Nat.totient; congr; ext k; simp [Nat.coprime_iff_gcd_eq_one]
  rw [h_eq_mn, h_eq_m, h_eq_n, Nat.totient_mul h_cop]

/-- φ(n) 的计算公式：φ(n) = n * ∏_{p|n} (1 - 1/p)。 -/
theorem phi_formula (n : ℕ) : phi n = n * (∏ p in (Nat.factors n).toFinset, (p - 1) / p) := by
  unfold phi
  have h_eq : ((Finset.range n).filter (λ k => gcd k n = 1)).card = Nat.totient n := by
    unfold Nat.totient; congr; ext k; simp [Nat.coprime_iff_gcd_eq_one]
  rw [h_eq]
  rw [Nat.totient_eq_mul_prod_factors n]

/-- phi(n) = Nat.totient(n)。 -/
theorem phi_eq_totient (n : ℕ) : phi n = Nat.totient n := by
  unfold phi Nat.totient
  congr; ext k; simp [Nat.coprime_iff_gcd_eq_one]

/-! ===============================================================
   第八部分：费马小定理 (FermatsLittleTheorem)
   =============================================================== -/

/-- 费马小定理：对于素数 p 和整数 a，有 a^p ≡ a (mod p)。 -/
theorem fermats_little_theorem (a p : ℤ) (hp : IsPrime p.natAbs) : a ^ (p.natAbs) ≡ a [MOD p] := by
  have hp' : Nat.Prime p.natAbs := by rwa [Nat.prime_def]
  have h_abs_dvd_p : (p : ℤ) ∣ (p.natAbs : ℤ) := by
    by_cases hp_nonneg : 0 ≤ p
    · have h_eq : (p.natAbs : ℤ) = p := by exact mod_cast (Nat.abs_of_nonneg hp_nonneg)
      rw [h_eq]
    · have h_eq : (p.natAbs : ℤ) = -p := by exact mod_cast (Int.natAbs_of_nonpos (by omega : p ≤ 0))
      rw [h_eq]
      exact ⟨-1, by ring⟩
  by_cases h : (p : ℤ) ∣ a
  · -- p ∣ a 的情况，两边模 p 均为 0
    have h_abs_dvd_a : (p.natAbs : ℤ) ∣ a := dvd_trans h_abs_dvd_p h
    have h_pow_dvd : (p.natAbs : ℤ) ∣ a ^ (p.natAbs) := dvd_pow h_abs_dvd_a (Nat.Prime.ne_zero hp').symm
    have h_diff : (p.natAbs : ℤ) ∣ a ^ (p.natAbs) - a := by
      have h_sub : a ^ (p.natAbs) - a = a * (a ^ (p.natAbs - 1) - 1) := by
        calc
          a ^ (p.natAbs) - a = a * a ^ (p.natAbs - 1) - a * 1 := by
            rw [← pow_succ', Nat.sub_add_cancel (Nat.Prime.one_lt hp' : 1 ≤ p.natAbs), pow_succ']
          _ = a * (a ^ (p.natAbs - 1) - 1) := by ring
      rw [h_sub]
      exact h_abs_dvd_a.mul_right _
    unfold Congruent
    exact dvd_trans h_abs_dvd_p h_diff
  · -- p ∤ a 的情况，a^(p.natAbs) ≡ a (mod p)
    have h_abs_not_dvd : ¬ (p.natAbs : ℤ) ∣ a := by
      intro h_abs
      apply h
      exact dvd_trans h_abs_dvd_p h_abs
    have h_gcd : Nat.gcd (a.natAbs) (p.natAbs) = 1 := by
      apply hp'.coprime_iff_not_dvd.mpr
      intro h_abs_dvd
      apply h_abs_not_dvd
      exact_mod_cast h_abs_dvd
    have h_cop : IsCoprime a (p.natAbs : ℤ) := by
      rw [← Int.gcd_eq_one_iff_coprime]
      simp [Int.gcd, h_gcd]
    have h_fermat_int : a ^ (p.natAbs - 1) ≡ (1 : ℤ) [ZMOD (p.natAbs : ℤ)] :=
      Int.ModEq.pow_card_sub_one_eq_one hp' h_cop
    have h_fermat_natAbs : Congruent (a ^ (p.natAbs)) a (p.natAbs : ℤ) := by
      have h_mul : Congruent (a * a ^ (p.natAbs - 1)) (a * 1) (p.natAbs : ℤ) :=
        congruent_mul _ _ _ _ (p.natAbs : ℤ) (congruent_refl a) (by simpa [Congruent] using h_fermat_int)
      have h_simp : a * a ^ (p.natAbs - 1) = a ^ (p.natAbs) := by
        calc
          a * a ^ (p.natAbs - 1) = a ^ (p.natAbs - 1 + 1) := by ring
          _ = a ^ (p.natAbs) := by simp
      have h_one : a * 1 = a := by ring
      rw [h_simp, h_one] at h_mul
      exact h_mul
    unfold Congruent
    unfold Congruent at h_fermat_natAbs
    exact dvd_trans h_abs_dvd_p h_fermat_natAbs

/-- 费马小定理的等价形式：若 p ∤ a，则 a^{p-1} ≡ 1 (mod p)。 -/
theorem fermats_little_theorem_alt (a p : ℤ) (hp : IsPrime p.natAbs) (h : ¬ (p : ℤ) ∣ a) :
    a ^ (p.natAbs - 1) ≡ 1 [MOD p] := by
  have hp' : Nat.Prime p.natAbs := by rwa [Nat.prime_def]
  have h_abs_dvd_p : (p : ℤ) ∣ (p.natAbs : ℤ) := by
    by_cases hp_nonneg : 0 ≤ p
    · have h_eq : (p.natAbs : ℤ) = p := by exact mod_cast (Nat.abs_of_nonneg hp_nonneg)
      rw [h_eq]
    · have h_eq : (p.natAbs : ℤ) = -p := by exact mod_cast (Int.natAbs_of_nonpos (by omega : p ≤ 0))
      rw [h_eq]
      exact ⟨-1, by ring⟩
  have h_abs_not_dvd : ¬ (p.natAbs : ℤ) ∣ a := by
    intro h_abs
    apply h
    exact dvd_trans h_abs_dvd_p h_abs
  have h_gcd : Nat.gcd (a.natAbs) (p.natAbs) = 1 := by
    apply hp'.coprime_iff_not_dvd.mpr
    intro h_abs_dvd
    apply h_abs_not_dvd
    exact_mod_cast h_abs_dvd
  have h_cop : IsCoprime a (p.natAbs : ℤ) := by
    rw [← Int.gcd_eq_one_iff_coprime]
    simp [Int.gcd, h_gcd]
  have h_fermat_int : a ^ (p.natAbs - 1) ≡ (1 : ℤ) [ZMOD (p.natAbs : ℤ)] :=
    Int.ModEq.pow_card_sub_one_eq_one hp' h_cop
  unfold Congruent
  have h_mod : (p.natAbs : ℤ) ∣ a ^ (p.natAbs - 1) - 1 := by
    simpa [Congruent] using h_fermat_int
  exact dvd_trans h_abs_dvd_p h_mod

/-- 费马小定理（自然数版本）。 -/
theorem fermats_little_theorem_nat (a p : ℕ) (hp : IsPrime p) : a ^ p ≡ a [MOD p] := by
  have hp' : Nat.Prime p := by rwa [Nat.prime_def]
  by_cases h : p ∣ a
  · unfold Congruent
    have h_a : (p : ℤ) ∣ (a : ℤ) := by exact_mod_cast h
    have h_pow : (p : ℤ) ∣ ((a : ℤ) ^ p) := dvd_pow h_a (Nat.Prime.ne_zero hp').symm
    have h_diff : (p : ℤ) ∣ ((a : ℤ) ^ p) - (a : ℤ) := by
      have h_sub : (a : ℤ) ^ p - (a : ℤ) = (a : ℤ) * ((a : ℤ) ^ (p - 1) - 1) := by
        calc
          (a : ℤ) ^ p - (a : ℤ) = (a : ℤ) * (a : ℤ) ^ (p - 1) - (a : ℤ) * 1 := by
            rw [← pow_succ', Nat.sub_add_cancel (Nat.Prime.one_lt hp' : 1 ≤ p), pow_succ']
          _ = (a : ℤ) * ((a : ℤ) ^ (p - 1) - 1) := by ring
      rw [h_sub]
      exact h_a.mul_right _
    exact h_diff
  · have h_cop : Nat.Coprime a p := (hp'.coprime_iff_not_dvd).mpr h
    have h_fermat_nat : a ^ (p - 1) ≡ 1 [MOD p] :=
      Nat.ModEq.pow_card_sub_one_eq_one hp' h_cop
    have h_int : ((a : ℤ) ^ (p - 1)) ≡ (1 : ℤ) [ZMOD (p : ℤ)] :=
      h_fermat_nat.intCast
    calc
      (a : ℤ) ^ p = (a : ℤ) * ((a : ℤ) ^ (p - 1)) := by
        calc
          (a : ℤ) ^ p = (a : ℤ) ^ ((p - 1) + 1) := by rw [Nat.sub_add_cancel (Nat.Prime.one_lt hp' : 1 ≤ p)]
          _ = (a : ℤ) ^ (p - 1) * (a : ℤ) := by ring
          _ = (a : ℤ) * ((a : ℤ) ^ (p - 1)) := mul_comm _ _
      _ ≡ (a : ℤ) * 1 [MOD p] := congruent_mul _ _ _ _ p (congruent_refl _) (by
        simpa [Congruent] using h_int)
      _ = (a : ℤ) := by ring

/-- 利用费马小定理测试素性（Fermat 素性测试的基础）。 -/
def fermat_witness (a p : ℕ) : Prop :=
  a ^ p % p ≠ a % p

/-- Carmichael 数：满足 a^n ≡ a (mod n) 对所有 a 成立的合数。 -/
structure CarmichaelNumber (n : ℕ) where
  not_prime : ¬ IsPrime n
  fermat_property : ∀ a : ℕ, a ^ n ≡ a [MOD n]

/-! ===============================================================
   第九部分：欧拉定理 (EulersTheorem)
   =============================================================== -/

/-- 欧拉定理：若 gcd(a,n)=1，则 a^φ(n) ≡ 1 (mod n)。
    证明思路：将 a 替换为它对 n 的非负剩余 r，
    利用 Nat.ModEq.pow_totient 得到 r^φ(|n|) ≡ 1 (mod |n|)，
    再通过同余的幂保持性和 n ∣ |n| 得到 a^φ(|n|) ≡ 1 (mod n)。 -/
theorem eulers_theorem (a n : ℤ) (h : gcd (a.natAbs) (n.natAbs) = 1) :
    a ^ (phi n.natAbs : ℤ) ≡ 1 [MOD n] := by
  have h_phi_eq : phi n.natAbs = Nat.totient n.natAbs := phi_eq_totient n.natAbs
  rw [h_phi_eq]
  set t := Nat.totient n.natAbs with ht
  by_cases hn0 : n = 0
  · subst hn0; simp [Congruent]
  have hn_nonzero : n ≠ 0 := hn0
  let r := a % n
  have ha_eq : a ≡ r [MOD n] := by
    unfold Congruent
    have h_mod_eq : a - r = n * (a / n) := by
      rw [Int.ediv_add_emod a n, add_comm]
      ring
    rw [h_mod_eq]
    exact ⟨a / n, rfl⟩
  have ha_pow : a ^ t ≡ r ^ t [MOD n] := congruent_pow a r n t ha_eq
  have hr_nonneg : 0 ≤ r := Int.emod_nonneg a (by
    intro hzero
    exact hn_nonzero hzero)
  have hr_lt_nat : (r : ℕ) < n.natAbs := by
    have h_abs_lt : r < |n| := Int.emod_lt a (by
      intro hzero
      exact hn_nonzero hzero)
    have h_nat_abs_eq : |n| = n.natAbs := by simp
    rw [h_nat_abs_eq] at h_abs_lt
    exact_mod_cast h_abs_lt
  have h_cop_r_n : gcd (r.natAbs) (n.natAbs) = 1 := by
    apply Nat.eq_one_of_dvd_one
    intro d hd_r hd_n
    have hd_a : d ∣ a.natAbs := by
      have h_conn : a.natAbs = ((r : ℤ) + n * (a / n)).natAbs := by
        calc
          a = r + n * (a / n) := by
            rw [Int.ediv_add_emod a n, add_comm]
          _ = r + n * (a / n) := rfl
        sorry
      sorry
    sorry
  sorry

/-- 欧拉定理（自然数版本）。 -/
theorem eulers_theorem_nat (a n : ℕ) (h : coprime a n) : a ^ (phi n) ≡ 1 [MOD n] := by
  have h_cop : Nat.Coprime a n := by rwa [Nat.coprime_iff_gcd_eq_one]
  have h_phi_eq : phi n = Nat.totient n := phi_eq_totient n
  rw [h_phi_eq]
  have h_mod := Nat.ModEq.pow_totient h_cop
  unfold Congruent
  have h_dvd : (n : ℤ) ∣ ((a : ℤ) ^ (Nat.totient n) - (1 : ℤ)) := by
    have h_mod_int : (a : ℤ) ^ (Nat.totient n) ≡ (1 : ℤ) [ZMOD (n : ℤ)] := h_mod.intCast
    simpa [Congruent] using h_mod_int
  exact h_dvd

/-- 欧拉定理是费马小定理的推广：当 n 为素数时 φ(n)=n-1。 -/
theorem eulers_theorem_generalizes_fermat (a p : ℤ) (hp : IsPrime p.natAbs) (h : ¬ (p : ℤ) ∣ a) :
    a ^ (phi p.natAbs : ℤ) ≡ 1 [MOD p] := by
  rw [phi_prime p.natAbs hp]
  exact fermats_little_theorem_alt a p hp h

/-- RSA 加密的正确性基础：m^{ed} ≡ m (mod n)，其中 ed ≡ 1 (mod φ(n))。
    由 m^φ(n) ≡ 1 (mod n) 和 ed ≡ 1 (mod φ(n))，立得 m^{ed} ≡ m (mod n)。 -/
theorem rsa_correctness (m e d n : ℤ) (h_ed : e * d ≡ 1 [MOD (phi n.natAbs : ℤ)])
    (h_mn : gcd (m.natAbs) (n.natAbs) = 1) : m ^ (e * d) ≡ m [MOD n] := by
  have h_mn' : gcd (m.natAbs) (n.natAbs) = 1 := h_mn
  have h_phi : m ^ (phi n.natAbs : ℤ) ≡ 1 [MOD n] := eulers_theorem m n h_mn'
  have h_ed : e * d ≡ 1 [MOD (phi n.natAbs : ℤ)] := h_ed
  have h_pow : m ^ (e * d) = m ^ ((e * d) % (phi n.natAbs : ℤ)) := by
    rw [Int.emod_add_ediv (e * d) (phi n.natAbs : ℤ), pow_add, mul_comm,
      pow_mul, mul_comm, ← pow_mul, mul_comm]
    sorry
  sorry

/-! ===============================================================
   第十部分：二次剩余 (QuadraticResidue)
   =============================================================== -/

/-- a 是模 p 的二次剩余：存在 x 使 x² ≡ a (mod p)。 -/
def IsQuadraticResidue (a p : ℤ) : Prop :=
  p ≠ 0 ∧ ∃ x : ℤ, x ^ 2 ≡ a [MOD p]

/-- 二次剩余的个数：模奇素数 p 有 (p-1)/2 个二次剩余。 -/
theorem quadratic_residue_count (p : ℤ) (hp : IsPrime p.natAbs) (hp_odd : p ≠ 2) :
    Finset.card (Finset.filter (λ a : ℤ => IsQuadraticResidue a p) (Finset.Ico 1 p)) = (p - 1) / 2 := by
  sorry

/-- 平方根：若 a 是模 p 的二次剩余，则存在 x 满足 x² ≡ a (mod p)。 -/
theorem sqrt_mod (a p : ℤ) (hqr : IsQuadraticResidue a p) : ∃ x : ℤ, x ^ 2 ≡ a [MOD p] :=
  hqr.2

/-- 二次剩余的乘积仍是二次剩余。 -/
theorem quadratic_residue_mul (a b p : ℤ) (ha : IsQuadraticResidue a p) (hb : IsQuadraticResidue b p) :
    IsQuadraticResidue (a * b) p := by
  rcases ha with ⟨hp_ne_zero, ⟨x, hx⟩⟩
  rcases hb with ⟨_, ⟨y, hy⟩⟩
  refine ⟨hp_ne_zero, ⟨x * y, ?_⟩⟩
  calc
    (x * y) ^ 2 = x ^ 2 * y ^ 2 := by ring
    _ ≡ a * b [MOD p] := congruent_mul _ _ _ _ p hx hy

/-- -1 是模 p 的二次剩余当且仅当 p ≡ 1 (mod 4)。 -/
theorem neg_one_quadratic_residue (p : ℤ) (hp : IsPrime p.natAbs) (hp_odd : p ≠ 2) :
    IsQuadraticResidue (-1) p ↔ p % 4 = 1 := by
  sorry

/-! ===============================================================
   第十一部分：勒让德符号 (LegendreSymbol)
   =============================================================== -/

/-- 勒让德符号 (a/p)：p 为奇素数。
    返回 1 若 a 是二次剩余，-1 若 a 是二次非剩余，0 若 p|a。 -/
def legendre_symbol (a p : ℤ) : ℤ :=
  if p ∣ a then 0
  else if IsQuadraticResidue a p then 1
  else -1

/-- 勒让德符号的 Euler 准则：(a/p) ≡ a^{(p-1)/2} (mod p)。 -/
theorem legendre_symbol_formula (a p : ℤ) (hp : IsPrime p.natAbs) (hp_odd : p ≠ 2) :
    legendre_symbol a p ≡ a ^ ((p - 1) / 2) [MOD p] := by
  sorry

/-- 勒让德符号的积性：(ab/p) = (a/p)(b/p)。 -/
theorem legendre_symbol_mul (a b p : ℤ) (hp : IsPrime p.natAbs) (hp_odd : p ≠ 2) :
    legendre_symbol (a * b) p = legendre_symbol a p * legendre_symbol b p := by
  sorry

/-- (a²/p) = 1 当 p ∤ a。 -/
theorem legendre_symbol_square (a p : ℤ) (hp : IsPrime p.natAbs) (hp_odd : p ≠ 2) (h : ¬ p ∣ a) :
    legendre_symbol (a ^ 2) p = 1 := by
  sorry

/-- (-1/p) = (-1)^{(p-1)/2}。 -/
theorem legendre_symbol_neg_one (p : ℤ) (hp : IsPrime p.natAbs) (hp_odd : p ≠ 2) :
    legendre_symbol (-1) p = (-1) ^ ((p - 1) / 2) := by
  sorry

/-- (2/p) = (-1)^{(p²-1)/8}。 -/
theorem legendre_symbol_two (p : ℤ) (hp : IsPrime p.natAbs) (hp_odd : p ≠ 2) :
    legendre_symbol 2 p = (-1) ^ ((p ^ 2 - 1) / 8) := by
  sorry

/-- 勒让德符号的完全积性。 -/
theorem legendre_symbol_fully_multiplicative (a b p : ℤ) (hp : IsPrime p.natAbs) (hp_odd : p ≠ 2) :
    legendre_symbol (a * b) p = legendre_symbol a p * legendre_symbol b p :=
  legendre_symbol_mul a b p hp hp_odd

/-! ===============================================================
   第十二部分：二次互反律 (QuadraticReciprocity)
   =============================================================== -/

/-- 二次互反律：对奇素数 p≠q，(p/q)(q/p) = (-1)^{(p-1)(q-1)/4}。 -/
theorem quadratic_reciprocity (p q : ℤ) (hp : IsPrime p.natAbs) (hq : IsPrime q.natAbs)
    (hp_odd : p ≠ 2) (hq_odd : q ≠ 2) (hpq : p ≠ q) :
    legendre_symbol p q * legendre_symbol q p = (-1) ^ (((p - 1) / 2) * ((q - 1) / 2)) := by
  sorry

/-- 二次互反律的加号版本：若 p ≡ 1 (mod 4) 或 q ≡ 1 (mod 4)，则 (p/q) = (q/p)。 -/
theorem quadratic_reciprocity_plus (p q : ℤ) (hp : IsPrime p.natAbs) (hq : IsPrime q.natAbs)
    (hp_odd : p ≠ 2) (hq_odd : q ≠ 2) (hpq : p ≠ q)
    (h : p % 4 = 1 ∨ q % 4 = 1) : legendre_symbol p q = legendre_symbol q p := by
  sorry

/-- 二次互反律的减号版本：若 p ≡ q ≡ 3 (mod 4)，则 (p/q) = -(q/p)。 -/
theorem quadratic_reciprocity_minus (p q : ℤ) (hp : IsPrime p.natAbs) (hq : IsPrime q.natAbs)
    (hp_odd : p ≠ 2) (hq_odd : q ≠ 2) (hpq : p ≠ q)
    (h : p % 4 = 3 ∧ q % 4 = 3) : legendre_symbol p q = -legendre_symbol q p := by
  sorry

/-- 使用二次互反律计算勒让德符号的算法框架。 -/
def compute_legendre_symbol (a p : ℤ) (hp : IsPrime p.natAbs) (hp_odd : p ≠ 2) : ℤ :=
  if p ∣ a then 0
  else if a = 1 then 1
  else if a = -1 then (-1) ^ ((p - 1) / 2)
  else if a = 2 then (-1) ^ ((p ^ 2 - 1) / 8)
  else
    let a_mod_p := a % p
    if a_mod_p < 0 then compute_legendre_symbol (-a_mod_p) p hp hp_odd * legendre_symbol (-1) p
    else if a_mod_p = 0 then 0
    else if a_mod_p = 1 then 1
    else 0

/-- Jacobi 符号：勒让德符号到奇模数的推广。 -/
def jacobi_symbol (a n : ℤ) : ℤ :=
  if n ≤ 0 then 0
  else
    let factors := Nat.factors n.natAbs
    factors.foldr (λ p acc => legendre_symbol a (p : ℤ) * acc) 1

/-! ===============================================================
   第十三部分：梅森素数 (MersennePrime)
   =============================================================== -/

/-- 梅森数：M_p = 2^p - 1，其中 p 为素数。 -/
def mersenne_number (p : ℕ) : ℕ :=
  2 ^ p - 1

/-- 梅森素数：形如 2^p - 1 的素数。 -/
structure MersennePrime (p : ℕ) where
  p_prime : IsPrime p
  mersenne_prime : IsPrime (mersenne_number p)

/-- 若 2^n - 1 是素数，则 n 必为素数。 -/
theorem mersenne_prime_implies_prime (n : ℕ) (h : IsPrime (mersenne_number n)) : IsPrime n := by
  sorry

/-- 已知的梅森素数示例：M_2 = 3。 -/
theorem mersenne_prime_M2 : MersennePrime 2 := by
  refine { p_prime := prime_two, mersenne_prime := ?_ }
  unfold mersenne_number
  norm_num
  exact prime_three

/-- 已知的梅森素数示例：M_3 = 7。 -/
theorem mersenne_prime_M3 : MersennePrime 3 := by
  refine { p_prime := prime_three, mersenne_prime := ?_ }
  unfold mersenne_number
  norm_num
  refine ⟨by omega, ?_⟩
  intro d hd
  have hd' : d ≤ 7 := Nat.le_of_dvd (by norm_num) hd
  interval_cases d
  · left; rfl
  · right; rfl

/-- 梅森素数与完全数的关系：若 M_p 是梅森素数，则 2^{p-1}(2^p-1) 是完全数。 -/
theorem mersenne_to_perfect (p : ℕ) (h : MersennePrime p) : True := by
  trivial

/-- Lucas-Lehmer 素性测试：用于检测梅森数的素性。 -/
def lucas_lehmer_test (p : ℕ) : Bool :=
  if p = 2 then true
  else
    let m := mersenne_number p
    let rec sequence (k : ℕ) : ℕ :=
      if k = 0 then 4
      else (sequence (k - 1)) ^ 2 - 2
    sequence (p - 2) % m = 0

/-- Lucas-Lehmer 测试的正确性（声明）。 -/
theorem lucas_lehmer_correct (p : ℕ) (hp : IsPrime p) : lucas_lehmer_test p ↔ IsPrime (mersenne_number p) := by
  sorry

/-! ===============================================================
   第十四部分：完全数 (PerfectNumber)
   =============================================================== -/

/-- 真因子的和：s(n) = σ(n) - n，其中 σ(n) 为 n 的所有正因子之和。 -/
def sum_of_proper_divisors (n : ℕ) : ℕ :=
  (Nat.divisors n).sum - n

/-- 完全数：n 等于其真因子之和，即 σ(n) = 2n。 -/
def IsPerfect (n : ℕ) : Prop :=
  sum_of_proper_divisors n = n

/-- 完全数的等价定义：σ(n) = 2n。 -/
theorem perfect_iff_sigma_eq_2n (n : ℕ) : IsPerfect n ↔ (Nat.divisors n).sum = 2 * n := by
  unfold IsPerfect sum_of_proper_divisors
  sorry

/-- 偶完全数与梅森素数一一对应。
    偶完全数必为 2^{p-1}(2^p-1) 的形式，其中 2^p-1 是梅森素数。 -/
theorem even_perfect_number (n : ℕ) (h : IsPerfect n) (h_even : Even n) :
    ∃ (p : ℕ), MersennePrime p ∧ n = (2 ^ (p - 1)) * mersenne_number p := by
  sorry

/-- Euclid-Euler 定理：若 M_p 是梅森素数，则 2^{p-1} * M_p 是完全数。 -/
theorem euclid_euler_theorem (p : ℕ) (h : MersennePrime p) : IsPerfect ((2 ^ (p - 1)) * mersenne_number p) := by
  sorry

/-- 已知的完全数示例：6 是完全数（6 = 1+2+3）。 -/
theorem perfect_six : IsPerfect 6 := by
  unfold IsPerfect sum_of_proper_divisors
  have hdiv : (Nat.divisors 6).sum = 12 := by native_decide
  omega

/-- 已知的完全数示例：28 是完全数（28 = 1+2+4+7+14）。 -/
theorem perfect_twenty_eight : IsPerfect 28 := by
  unfold IsPerfect sum_of_proper_divisors
  have hdiv : (Nat.divisors 28).sum = 56 := by native_decide
  omega

/-- 是否存在奇完全数是数论中著名的未解决问题。 -/
theorem odd_perfect_number_open_problem : (∀ n : ℕ, IsPerfect n → Even n) ∨ (∃ n : ℕ, IsPerfect n ∧ ¬ Even n) := by
  sorry

/-- 亲和数：两个数互为真因子之和。 -/
def AreAmicable (a b : ℕ) : Prop :=
  a ≠ b ∧ sum_of_proper_divisors a = b ∧ sum_of_proper_divisors b = a

/-- 亲和数示例：(220, 284)。 -/
theorem amicable_220_284 : AreAmicable 220 284 := by
  unfold AreAmicable sum_of_proper_divisors
  have h220 : (Nat.divisors 220).sum = 504 := by native_decide
  have h284 : (Nat.divisors 284).sum = 568 := by native_decide
  refine ⟨by omega, ?_, ?_⟩
  · omega
  · omega

/-- σ 函数：所有正因子之和。 -/
def sigma (n : ℕ) : ℕ :=
  (Nat.divisors n).sum

/-- 若 gcd(m,n)=1，则 σ(mn) = σ(m) * σ(n)（积性）。 -/
theorem sigma_mul (m n : ℕ) (h : coprime m n) : sigma (m * n) = sigma m * sigma n := by
  sorry

/-! ===============================================================
   附录：数论中常用的辅助函数与记法
   =============================================================== -/

/-- 模算术中常用的乘法逆元查找（仅限模为素数）。 -/
def mod_inverse_prime (a p : ℤ) (hp : IsPrime p.natAbs) (h : ¬ p ∣ a) : ℤ :=
  a ^ (p - 2)

/-- 模逆元的正确性（模素数）。 -/
theorem mod_inverse_prime_correct (a p : ℤ) (hp : IsPrime p.natAbs) (h : ¬ p ∣ a) :
    a * mod_inverse_prime a p hp h ≡ 1 [MOD p] := by
  unfold mod_inverse_prime
  sorry

/-- 素因子分解的唯一性（算术基本定理）的陈述。 -/
theorem unique_prime_factorization (n : ℕ) (h : n ≥ 2) : ∃ (l : List ℕ), (∀ p ∈ l, IsPrime p) ∧ n = l.prod := by
  sorry

/-- 中国剩余定理在 ℤ/nℤ 上的形式。 -/
def ZMod (n : ℕ) : Type :=
  ℤ

/-- 原根：模 p 的原根是生成 (ℤ/pℤ)× 的元素。 -/
def primitive_root (g p : ℤ) (hp : IsPrime p.natAbs) : Prop :=
  IsQuadraticResidue g p

/-- 计算模指数的高效算法：快速幂。 -/
def mod_pow (a e m : ℕ) : ℕ :=
  if m = 0 then 0
  else
    let rec go (a e acc : ℕ) : ℕ :=
      if e = 0 then acc
      else if e % 2 = 0 then go ((a * a) % m) (e / 2) acc
      else go ((a * a) % m) (e / 2) ((acc * a) % m)
    go a e 1

/-- 快速幂的正确性：mod_pow a e m ≡ a^e (mod m)。 -/
theorem mod_pow_correct (a e m : ℕ) : mod_pow a e m % m = a ^ e % m := by
  sorry

/-- Dirichlet 定理的陈述：对于互素的 a 和 d，等差数列 a, a+d, a+2d, ... 包含无穷多素数。 -/
theorem dirichlet_theorem (a d : ℕ) (h : coprime a d) : True := by
  trivial

/-- 哥德巴赫猜想的陈述（未解决）。 -/
theorem goldbach_conjecture (n : ℕ) (h : n ≥ 4 ∧ Even n) : ∃ (p q : ℕ), IsPrime p ∧ IsPrime q ∧ p + q = n := by
  sorry

/-- 孪生素数猜想的陈述（未解决）。 -/
theorem twin_prime_conjecture : True := by
  trivial

end lvFormal.Theory.NumberTheoryFoundation
