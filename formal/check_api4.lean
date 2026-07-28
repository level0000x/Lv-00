import Mathlib

-- Check Nat.prime_def
#check @Nat.prime_def
#check @Nat.Prime_iff

-- Test: can we convert between IsPrime and Nat.Prime?
def IsPrime (p : ℕ) : Prop :=
  2 ≤ p ∧ ∀ d : ℕ, d ∣ p → d = 1 ∨ d = p

example (p : ℕ) : Nat.Prime p → IsPrime p := by
  intro hp
  constructor
  · exact hp.two_le
  · intro d hd
    rcases hp.eq_two_or_odd with (rfl | hodd)
    · -- p = 2
      have hd' : d ≤ 2 := Nat.le_of_dvd (by omega) hd
      have hpos : 1 ≤ d := Nat.pos_of_dvd_of_pos hd (by omega)
      interval_cases d <;> [left; rfl; right; rfl]
    · -- p is odd
      sorry

-- Check if there's a simpler way
example (p : ℕ) : Nat.Prime p → IsPrime p := by
  intro hp
  refine ⟨hp.two_le, ?_⟩
  intro d hd
  exact (hp.eq_one_or_self_of_dvd hd).symm
