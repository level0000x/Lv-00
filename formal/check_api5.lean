import Mathlib

-- Check for dvd_pow related APIs on Nat.Prime
#check @Nat.Prime.dvd_of_dvd_pow
#check @Nat.Prime.dvd_pow_self
#check @Nat.Prime.pow_dvd_of_dvd
#check @Nat.Prime.dvd_of_dvd_pow'

-- The actual usage is: Nat.Prime p → (p ∣ a ^ n ↔ p ∣ a)
-- Check what's available
example (p a n : ℕ) (hp : Nat.Prime p) (hn : 0 < n) : p ∣ a ^ n → p ∣ a := by
  intro h
  exact hp.dvd_of_dvd_pow h

-- Or the iff version
#check @Nat.Prime.dvd_of_dvd_pow_iff
