import Mathlib

#check @Nat.divisors
example (n : ℕ) : ℕ → Finset ℕ := Nat.divisors
-- Check if Nat.divisors n is a Finset
-- 注意：`(Nat.divisors n).sum id = n` 不是恒等式，无法证明。
-- 反例：n = 6 时 Nat.divisors 6 = {1,2,3,6}，sum id = 12 ≠ 6。
-- （等式仅对完全数成立。）故保留 sorry 并注明原因。
example (n : ℕ) : (Nat.divisors n).sum id = n := by
  sorry -- 假命题：n = 6 时左侧 = 12 ≠ 6，无法证明
-- Check the type
#check (Nat.divisors 6)
#check (Nat.divisors 6).sum
