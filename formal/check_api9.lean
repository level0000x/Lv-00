import Mathlib

-- Test: can omega handle toNat?
example (p : ℤ) (hp : 2 ≤ p.natAbs) : (1 + (p - 2).toNat : ℕ) = (p - 1).toNat := by
  omega
