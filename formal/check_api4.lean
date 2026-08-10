import Mathlib

-- Check Nat.prime_def
#check @Nat.prime_def

-- Test: can we convert between IsPrime and Nat.Prime?
def IsPrime (p : ℕ) : Prop :=
  2 ≤ p ∧ ∀ d : ℕ, d ∣ p → d = 1 ∨ d = p

example (p : ℕ) : Nat.Prime p → IsPrime p := by
  intro hp
  constructor
  · exact hp.two_le
  · intro d hd
    exact hp.eq_one_or_self_of_dvd d hd

-- Check if there's a simpler way
example (p : ℕ) : Nat.Prime p → IsPrime p := by
  intro hp
  refine ⟨hp.two_le, ?_⟩
  intro d hd
  exact hp.eq_one_or_self_of_dvd d hd
