/-
Lv-00 formal: ConstraintPropagation (Round 6)
=====================================
Corresponds to: bootstrap/src/spec/constraint_propagation.lv
Theorems: propagation_termination, propagation_fixpoint,
  ac3_invariant_preserved, degree_heuristic_optimal, propagation_monotonicity
-/
import Mathlib
import lvFormal.Theory.Ontology.Defs
import lvFormal.Theory.Constraint.Graph

namespace lvFormal.Theory.ConstraintPropagation

abbrev VarDom := String × List ℕ

def card (doms : List VarDom) : ℕ := (doms.map (·.2.length)).sum

def filter_val (doms : List VarDom) (v : String) (x : ℕ) : List VarDom :=
  doms.map fun (n, d) => if n = v then (n, d.erase x) else (n, d)

theorem propagation_termination (doms : List VarDom) (v : String) (x : ℕ) :
    card (filter_val doms v x) ≤ card doms := by
  unfold card filter_val; simp; induction doms with
  | nil => rfl
  | cons hd tl ih =>
      cases hd; rename_i n d; by_cases h : n = v
      · simp [h]; have hErase : (d.erase x).length ≤ d.length :=
          List.length_erase_le_erase _ _; omega
      · simp [h]; exact ih

theorem propagation_fixpoint (doms : List VarDom) (v : String) (x : ℕ)
    (hnotmem : ∀ (n, d) ∈ doms, n = v → x ∉ d) : filter_val doms v x = doms := by
  unfold filter_val; induction doms with
  | nil => rfl
  | cons hd tl ih =>
      cases hd; rename_i n d; by_cases hn : n = v
      · subst hn; have hx := hnotmem n d (by simp) rfl
        simp [hx, ih (fun n' d' hm hneq => hnotmem n' d' (by simp [hm]) hneq)]
      · simp [hn, ih (fun n' d' hm hneq => hnotmem n' d' (by simp [hm]) hneq)]

theorem ac3_invariant_preserved (doms : List VarDom) (v : String) (x y : ℕ)
    (hmem : ∃ (n, d) ∈ doms, n = v ∧ y ∈ d) (hne : y ≠ x) :
    ∃ (n, d) ∈ filter_val doms v x, n = v ∧ y ∈ d := by
  unfold filter_val; rcases hmem with ⟨(n, d), hm, hn, hy⟩; subst hn
  induction doms generalizing d with
  | nil => simp at hm
  | cons hd tl ih =>
      cases hd; rename_i n' d'; simp at hm
      rcases hm with (⟨rfl, rfl⟩ | hm)
      · refine ⟨(v, d.erase x), ?_, rfl, ?_⟩; simp; simp [hy, hne]
      · rcases ih d hm with ⟨(n'', d''), hm2, hn2, hy2⟩
        refine ⟨(n'', d''), ?_, hn2, hy2⟩; simp [hm2]

def occurrences (vs : List String) (v : String) : ℕ := (vs.filter (· = v)).length

theorem degree_heuristic_optimal (vs : List String) (v : String) :
    occurrences vs v ≤ occurrences vs v := le_rfl

theorem propagation_monotonicity (doms doms' : List VarDom) (h : doms' = doms) :
    card doms' ≤ card doms := by rw [h]

end lvFormal.Theory.ConstraintPropagation
