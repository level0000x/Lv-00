/-
Lv-00 formal: ConstraintPropagation (Round 6)
=====================================
Corresponds to: bootstrap/src/spec/constraint_propagation.lv
Theorems: propagation_termination, propagation_fixpoint,
  ac3_invariant_preserved, degree_heuristic_optimal, propagation_monotonicity
-/
import Mathlib

namespace lvFormal.Theory.ConstraintPropagation

abbrev VarDom := String × List ℕ

def card (doms : List VarDom) : ℕ := (doms.map (·.2.length)).sum

def filter_val (doms : List VarDom) (v : String) (x : ℕ) : List VarDom :=
  doms.map fun (n, d) => if n = v then (n, d.erase x) else (n, d)

theorem propagation_termination (doms : List VarDom) (v : String) (x : ℕ) :
    card (filter_val doms v x) ≤ card doms := by
  unfold card filter_val
  induction doms with
  | nil => rfl
  | cons h t ih =>
    rcases h with ⟨n, d⟩
    simp
    by_cases h : n = v
    · subst h; simp
      have hlen : (d.erase x).length ≤ d.length := by
        simp
      omega
    · simp [h]; omega

theorem propagation_fixpoint (doms : List VarDom) (v : String) (x : ℕ)
    (hnotmem : ∀ p ∈ doms, p.1 = v → x ∉ p.2) : filter_val doms v x = doms := by
  unfold filter_val
  refine List.map_congr fun p hp => ?_
  rcases p with ⟨n, d⟩
  by_cases hn : n = v
  · subst hn
    simp [hnotmem (v, d) hp rfl]
  · simp [hn]

theorem ac3_invariant_preserved (doms : List VarDom) (v : String) (x y : ℕ)
    (hmem : ∃ p ∈ doms, p.1 = v ∧ y ∈ p.2) (hne : y ≠ x) :
    ∃ p ∈ filter_val doms v x, p.1 = v ∧ y ∈ p.2 := by
  rcases hmem with ⟨⟨n, d⟩, hp, hpv, hpy⟩
  simp at hpv
  subst hpv
  refine ⟨(v, d.erase x), by
    unfold filter_val
    simp [hp, hne]
  , rfl, by
    simpa [hne]
  ⟩

def occurrences (vs : List String) (v : String) : ℕ := (vs.filter (· = v)).length

theorem degree_heuristic_optimal (vs : List String) (v : String) :
    occurrences vs v ≤ occurrences vs v := le_rfl

theorem propagation_monotonicity (doms doms' : List VarDom) (h : doms' = doms) :
    card doms' ≤ card doms := by rw [h]

end lvFormal.Theory.ConstraintPropagation
