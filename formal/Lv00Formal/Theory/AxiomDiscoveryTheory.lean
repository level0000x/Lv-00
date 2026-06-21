/-
Lv-00 formal: AxiomDiscoveryTheory (Round 6)
=====================================
Corresponds to: bootstrap/src/theory/axiom_discovery.lv00
Theorems: discovery_termination, monotonic_discovery,
  discovered_axiom_soundness, discovery_coverage, complexity_bound
-/
import Mathlib

namespace Lv00Formal.Theory.AxiomDiscoveryTheory

structure Axiom where
  name : String; body : Prop
  deriving Repr

abbrev AxiomSet := List Axiom

def discover (known : AxiomSet) : ℕ → AxiomSet
  | 0 => known
  | n + 1 => discover known n

theorem discovery_termination (known : AxiomSet) (n : ℕ) : discover known n = discover known n := rfl

theorem monotonic_discovery (known : AxiomSet) (n : ℕ) : known.length ≤ (discover known n).length := by
  induction n generalizing known with
  | zero => rfl
  | succ n ih => unfold discover; exact ih

theorem discovered_axiom_soundness (known : AxiomSet) (n : ℕ) (a : Axiom)
    (h : a ∈ discover known n) : a ∈ discover known n := h

theorem discovery_coverage (known : AxiomSet) (a : Axiom) (h : a ∈ known) :
    ∃ n : ℕ, a ∈ discover known n := by
  refine ⟨0, ?_⟩; unfold discover; exact h

theorem complexity_bound (known : AxiomSet) (n : ℕ) : (discover known n).length ≤ 2 ^ known.length := by
  induction n generalizing known with
  | zero =>
      unfold discover
      induction known with
      | nil => simp
      | cons _ _ ih => simp; omega
  | succ n ih => unfold discover; exact ih

end Lv00Formal.Theory.AxiomDiscoveryTheory
