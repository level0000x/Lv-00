import Mathlib

#check @Nat.divisors
#check @Nat.divisors_val
example (n : ℕ) : ℕ → Finset ℕ := Nat.divisors
-- Check if Nat.divisors n is a Finset
example (n : ℕ) : (Nat.divisors n).sum id = n := by sorry
-- Check the type
#check (Nat.divisors 6)
#check (Nat.divisors 6).sum
