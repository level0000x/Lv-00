import Mathlib
open Real
example (x₁ x₂ : ℝ) (hx₁ : 0 ≤ x₁) (hx₂ : x₂ ≤ 1) (hlt : x₁ ≤ x₂) : x₁ ^ 2 * (3 - 2 * x₁) ≤ x₂ ^ 2 * (3 - 2 * x₂) := by
  nlinarith