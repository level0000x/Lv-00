import Mathlib

def IsPrime (p : ℕ) : Prop :=
  2 ≤ p ∧ ∀ d : ℕ, d ∣ p → d = 1 ∨ d = p

theorem unique_prime_factorization (n : ℕ) (h : n ≥ 2) : ∃ (l : List ℕ), (∀ p ∈ l, IsPrime p) ∧ n = l.prod := by
  refine ⟨Nat.primeFactorsList n, ?_, ?_⟩
  · intro p hp
    have := (Nat.mem_primeFactorsList (by omega : n ≠ 0)).mp hp
    exact Nat.prime_def.mp this.1
  · exact (Nat.prod_primeFactorsList (by omega : n ≠ 0)).symm
