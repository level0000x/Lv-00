import Mathlib

#check @Nat.Prime.dvd_of_dvd_pow
#check @Int.natAbs_dvd
#check @Int.dvd_natAbs
#check @pow_succ'
#check @Int.Prime.dvd_of_dvd_pow

-- Test: can we convert between ℤ and ℕ divisibility?
example (p a : ℤ) (hp : Nat.Prime p.natAbs) (h : p ∣ a ^ 2) : p ∣ a := by
  have h1 : p.natAbs ∣ a.natAbs ^ 2 := by
    sorry -- this is the key conversion
  have h2 : p.natAbs ∣ a.natAbs := Nat.Prime.dvd_of_dvd_pow hp (by omega) h1
  sorry -- lift back to ℤ

-- Check Int.Prime
#check @Int.Prime
#check @Int.prime_def_nat
