import Mathlib

#check @Nat.Prime.dvd_of_dvd_pow
#check @Int.natAbs_dvd
#check @Int.dvd_natAbs
#check @pow_succ'
#check @Int.natAbs_dvd_natAbs
#check @Int.natAbs_pow

-- Test: can we convert between ℤ and ℕ divisibility?
example (p a : ℤ) (hp : Nat.Prime p.natAbs) (h : p ∣ a ^ 2) : p ∣ a := by
  have h1 : p.natAbs ∣ a.natAbs ^ 2 := by
    have h' : p.natAbs ∣ (a ^ 2).natAbs := Int.natAbs_dvd_natAbs.mpr h
    simpa [Int.natAbs_pow] using h'
  have h2 : p.natAbs ∣ a.natAbs := Nat.Prime.dvd_of_dvd_pow hp h1
  exact Int.natAbs_dvd_natAbs.mp h2
