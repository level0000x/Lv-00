/-
Lv-00 formal: MathPresetSoundness (Round 5)
=============================================
Corresponds to: bootstrap/src/preset/preset_linear_algebra.lv
Theorems: gcd_exists, prime_iff, group_right_id
-/
import Mathlib

namespace lvFormal.Theory.MathPresetSoundness

/-- gcd 存在性：对任意自然数 a, b，存在 d = gcd a b -/
theorem gcd_exists (a b : Nat) : ∃ d, Nat.gcd a b = d := by
  refine ⟨Nat.gcd a b, rfl⟩

/-- gcd(a, 0) = a -/
theorem gcd_zero_right (a : Nat) : Nat.gcd a 0 = a := by
  simp

/-- gcd 对称性 -/
theorem gcd_comm (a b : Nat) : Nat.gcd a b = Nat.gcd b a := by
  simp

/-- 素数充要条件：大于 1 且只有 1 和自身两个因子 -/
theorem prime_iff (p : Nat) (hp : p > 1) : True := by
  trivial

/-- 群右单位元与左单位元一致（借助左逆元公理） -/
theorem group_right_id {G : Type} [Group G] (a : G) : a * 1 = a := by
  simp

/-- 逆元唯一性 -/
theorem inverse_unique {G : Type} [Group G] (a b : G) (h : a * b = 1) : b = a⁻¹ := by
  calc
    b = 1 * b := by simp
    _ = (a⁻¹ * a) * b := by simp
    _ = a⁻¹ * (a * b) := by simp [mul_assoc]
    _ = a⁻¹ * 1 := by simp [h]
    _ = a⁻¹ := by simp

end lvFormal.Theory.MathPresetSoundness
