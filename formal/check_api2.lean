import Mathlib

-- Check Nat.factors related APIs
#check @Nat.factors
#check @List.mem_iff_get
-- Try different names
#check @Nat.prime_of_mem_factors
#check @Nat.mem_factors_iff
#check @List.prod_eq_foldr
#check @Nat.prod_factors'
#check @Nat.factors_prod
#check @Nat.prod_factors
#check @Nat.prime_iff
-- Check what Nat.factors returns and its properties
example (n : ℕ) (h : n ≠ 0) : 1 < n → ∀ p ∈ n.factors, Nat.Prime p := by
  intro h1n p hp
  exact Nat.prime_of_mem_factors h hp
