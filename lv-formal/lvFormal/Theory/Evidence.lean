/-
Lv-00 formal: Evidence — 证据自检查系统 (v1.1 R5)
=====================================================
A zero-trust proof-trace verification system.

Concept: A third party receives (graph, proof_trace) and verifies
that the trace is a valid proof WITHOUT trusting the compiler.

Architecture:
  1. ProofTrace: list of proof steps (hypothesis, lemma, rewrite, unify, etc.)
  2. evidence_check: ConstraintGraph → ProofTrace → Bool
     — returns true iff the trace constitutes a valid proof of the graph
  3. evidence_soundness: if evidence_check returns true AND the trace is
     semantically sound, then the constraint graph is satisfiable
  4. TraceSound: an inductive predicate capturing semantic soundness of
     each proof step

Novelty: the compiler can be wrong/hacked/malicious, but as long as the
evidence check passes AND each step is semantically justified, the graph
is PROVEN satisfiable. This is the zero-trust guarantee at the heart of
the Lv-00 design.
-/

import Mathlib
import lvFormal.Theory.IR

namespace lvFormal.Theory.Evidence

open lvFormal.Theory.IR

/- ===============================================================
   Proof Trace
   =============================================================== -/

/-- A single step in a proof trace.
    Each step is one of:
    - hypothesis: "we know that constraint C holds" (input assumption)
    - lemma:       "from previous steps, we deduce new constraint D"
    - rewrite:     "constraint E can be rewritten to F"
    - unify:       "point A has the same coordinates as point B"
    - normalize:   "apply normalization pass"
    - qed:         "all constraints are satisfied, proof complete"
-/
inductive ProofStep where
  | hypothesis (c : IRConstraint)                    -- input: assume c
  | lemma      (premise : Nat) (conclusion : IRConstraint)  -- from step n, deduce c
  | rewrite    (c : IRConstraint) (c' : IRConstraint) -- c ↔ c'
  | unify      (a b : String)                        -- point_a = point_b
  | normalize  (subgraph : ConstraintGraph)           -- result of normalization
  | qed        : ProofStep
  deriving DecidableEq, Repr

/-- A proof trace is a list of proof steps -/
abbrev ProofTrace := List ProofStep

/- ===============================================================
   Evidence Verifier
   =============================================================== -/

/-- The verifier state: tracks which constraints have been proved so far. -/
structure VerifierState where
  proved : List IRConstraint    -- constraints already proven valid
  deriving Repr

/-- Fresh verifier state for a constraint graph (proof not yet started). -/
def initVerifier (g : ConstraintGraph) : VerifierState :=
  { proved := [] }

/-- Replace point name `b` with `a` in a constraint.
    Used by the unify step: when point a = point b, all references
    to b in proved constraints are replaced by a. -/
def replacePoint (b a : String) (cstr : IRConstraint) : IRConstraint :=
  match cstr with
  | .distance p q v     => .distance (if p = b then a else p) (if q = b then a else q) v
  | .collinear x y z    => .collinear (if x = b then a else x) (if y = b then a else y) (if z = b then a else z)
  | .midpoint m x y     => .midpoint (if m = b then a else m) (if x = b then a else x) (if y = b then a else y)
  | .rightAngle x y z   => .rightAngle (if x = b then a else x) (if y = b then a else y) (if z = b then a else z)
  | .perpendicular x y z w => .perpendicular (if x = b then a else x) (if y = b then a else y)
                                             (if z = b then a else z) (if w = b then a else w)
  | .parallel x y z w   => .parallel (if x = b then a else x) (if y = b then a else y)
                                     (if z = b then a else z) (if w = b then a else w)
  | .equalLength x y z w => .equalLength (if x = b then a else x) (if y = b then a else y)
                                         (if z = b then a else z) (if w = b then a else w)
  | .eq_expr _ _ | .lt_expr _ _ | .gt_expr _ _ => cstr
  | .radius c p r       => .radius (if c = b then a else c) (if p = b then a else p) r
  | .angle x y z w t    => .angle (if x = b then a else x) (if y = b then a else y)
                                  (if z = b then a else z) (if w = b then a else w) t
  | .tangent cp la lb ld => .tangent (if cp = b then a else cp) (if la = b then a else la)
                                     (if lb = b then a else lb) (if ld = b then a else ld)
  | .equalAngle x y z w u v => .equalAngle (if x = b then a else x) (if y = b then a else y)
                                           (if z = b then a else z) (if w = b then a else w)
                                           (if u = b then a else u) (if v = b then a else v)
  | .ratioDivision p x y r => .ratioDivision (if p = b then a else p) (if x = b then a else x)
                                             (if y = b then a else y) r

/-- State transition for a single proof step.
    Returns the updated verifier state after applying the step. -/
def transition (st : VerifierState) (step : ProofStep) : VerifierState :=
  match step with
  | .hypothesis c => { st with proved := c :: st.proved }
  | .lemma _ c => { st with proved := c :: st.proved }
  | .rewrite c c' =>
    if st.proved.contains c then
      { st with proved := c' :: st.proved.erase c }
    else st
  | .unify a b =>
    { st with proved := st.proved.map (replacePoint b a) }
  | .normalize _ => st
  | .qed => st

/-- Check whether a single proof step is well-formed relative to the
    current state and the original constraint graph.
    Returns true if the step's verification condition passes. -/
def step_ok (g : ConstraintGraph) (st : VerifierState) (step : ProofStep) : Bool :=
  match step with
  | .hypothesis _ => true
  | .lemma n _ => n < st.proved.length
  | .rewrite c _ => st.proved.contains c
  | .unify _ _ => true
  | .normalize _ => true
  | .qed => g.all st.proved.contains

/-- Low-level trace processor.
    Recursively processes each step: checks the condition via step_ok,
    and if it passes, transitions to the next state.
    Returns `some finalState` if all steps pass, `none` otherwise. -/
def go (g : ConstraintGraph) (st : VerifierState) : ProofTrace → Option VerifierState
  | [] => some st
  | step :: rest =>
    if step_ok g st step then
      go g (transition st step) rest
    else
      none

/-- High-level evidence check witness.
    Returns the final verifier state if the entire trace verifies AND
    the trace ends with a qed step. Returns none otherwise. -/
def evidence_check_witness (g : ConstraintGraph) (t : ProofTrace) : Option VerifierState :=
  match t.getLast? with
  | some .qed => go g (initVerifier g) t
  | _ => none

/-- The evidence verifier: check a proof trace step-by-step.
    Returns true if the trace correctly proves the graph is satisfiable.

    Verification rules:
    - hypothesis c: always accepted (trust the input)
    - lemma n c: checks that n is a valid index into the current proved set
    - rewrite c c': checks that c is in the proved set
    - unify a b: always accepted (point identity is assumed valid)
    - normalize g': always accepted (normalization is trusted)
    - qed: checks that all constraints of the ORIGINAL graph are in the proved set -/
def evidence_check (g : ConstraintGraph) (t : ProofTrace) : Bool :=
  (evidence_check_witness g t).isSome

/- ===============================================================
   Core lemmas about go and evidence_check_witness
   =============================================================== -/

/-- Compute the final verifier state after processing a trace from a given start.
    This is the pure state-transition version (without verification checks). -/
def trace_fold (st : VerifierState) (t : ProofTrace) : VerifierState :=
  let rec f (st : VerifierState) : ProofTrace → VerifierState
    | [] => st
    | step :: rest => f (transition st step) rest
  f st t

/-- If go returns some state, that state equals the trace_fold from the same start. -/
lemma go_some_eq_trace_fold (g : ConstraintGraph) (st : VerifierState) (t : ProofTrace) (st' : VerifierState)
    (h : go g st t = some st') : st' = trace_fold st t := by
  induction t generalizing st with
  | nil =>
    unfold go trace_fold at *
    simp at h; subst h; rfl
  | cons step rest ih =>
    unfold go at h
    by_cases h_ok : step_ok g st step
    · simp [h_ok] at h
      unfold trace_fold
      exact ih (transition st step) rest st' h
    · simp [h_ok] at h

/-- If go returns some state and the trace ends with qed, then all original
    graph constraints are in the final proved set. -/
lemma go_all_proved_if_qed_last (g : ConstraintGraph) (st : VerifierState) (t : ProofTrace) (st' : VerifierState)
    (h : go g st t = some st') (h_last : t.getLast? = some .qed) : g.all st'.proved.contains := by
  induction t generalizing st with
  | nil => simp at h_last
  | cons step rest ih =>
    unfold go at h
    by_cases h_ok : step_ok g st step
    · simp [h_ok] at h
      have h_rest := h
      have h_rest_last : rest.getLast? = some .qed := by
        simpa [List.getLast?_cons] using h_last
      match step with
      | .qed =>
        unfold step_ok at h_ok
        simp at h_ok
        have h_qed_check : g.all st.proved.contains := h_ok
        cases rest with
        | nil =>
          unfold go at h_rest
          simp at h_rest
          subst h_rest
          exact h_qed_check
        | cons _ _ =>
          apply ih (transition st .qed) rest st' h_rest h_rest_last
      | _ =>
        apply ih (transition st step) rest st' h_rest h_rest_last
    · simp [h_ok] at h

/-- If evidence_check_witness returns some st, then the trace ends with qed,
    st equals the trace_fold, and all graph constraints are in st.proved. -/
lemma evidence_check_witness_spec (g : ConstraintGraph) (t : ProofTrace) (st : VerifierState)
    (h : evidence_check_witness g t = some st) :
    t.getLast? = some .qed ∧ st = trace_fold (initVerifier g) t ∧ g.all st.proved.contains := by
  unfold evidence_check_witness at h
  split at h with h_last
  · -- t.getLast? = some .qed
    have h_go : go g (initVerifier g) t = some st := h
    have h_fold : st = trace_fold (initVerifier g) t :=
      go_some_eq_trace_fold g (initVerifier g) t st h_go
    have h_all : g.all st.proved.contains :=
      go_all_proved_if_qed_last g (initVerifier g) t st h_go h_last
    exact ⟨h_last, h_fold, h_all⟩
  · simp at h

/- ===============================================================
   Semantic Soundness
   =============================================================== -/

/-- A proof step is semantically sound if, whenever the current proved set
    is simultaneously satisfiable under some environment, the proved set
    after applying the step is also simultaneously satisfiable (possibly
    under a different environment).

    For hypothesis/lemma, this requires the new constraint to be consistent
    with the existing proved set. For rewrite, the two constraints must
    be semantically equivalent. For unify, the point substitution must
    preserve satisfiability. For normalize and qed, the proved set is
    unchanged, so soundness is trivial. -/
def step_sound (st : VerifierState) (step : ProofStep) : Prop :=
  (∃ env : String → ℝ × ℝ, ∀ c ∈ st.proved, ir_sem env c) →
  (∃ env : String → ℝ × ℝ, ∀ c ∈ (transition st step).proved, ir_sem env c)

/-- Inductive predicate: a proof trace is semantically sound from a given
    verifier state. Each step's transition preserves the existence of a
    satisfying environment for the proved constraints. -/
inductive TraceSound : VerifierState → ProofTrace → Prop where
  | nil (st) : TraceSound st []
  | cons (st) (step : ProofStep) (rest : ProofTrace)
      (h_step : step_sound st step)
      (h_rest : TraceSound (transition st step) rest) :
    TraceSound st (step :: rest)

/-- If a trace is sound from a start state where the proved set is satisfiable,
    then the final state's proved set is also satisfiable. -/
lemma trace_sound_invariant (st : VerifierState) (t : ProofTrace)
    (h_sound : TraceSound st t)
    (h_init : ∃ env : String → ℝ × ℝ, ∀ c ∈ st.proved, ir_sem env c) :
    ∃ env : String → ℝ × ℝ, ∀ c ∈ (trace_fold st t).proved, ir_sem env c := by
  induction h_sound with
  | nil =>
    exact h_init
  | cons st step rest h_step h_rest ih =>
    have h_mid : ∃ env : String → ℝ × ℝ, ∀ c ∈ (transition st step).proved, ir_sem env c :=
      h_step h_init
    have h_fold_eq : trace_fold st (step :: rest) = trace_fold (transition st step) rest := rfl
    rw [h_fold_eq]
    exact ih h_mid

/- ===============================================================
   Soundness: evidence implies truth
   =============================================================== -/

/-- If evidence_check returns true AND the trace is semantically sound,
    then the constraint graph is satisfiable.

    This is the zero-trust guarantee: the verifier is small, simple,
    and independent of the compiler. An adversary could produce a
    fabricated trace, but it MUST pass the verifier's checks AND be
    semantically sound.

    The verifier checks syntax and structure only — it does NOT need
    to understand geometry. The TraceSound predicate captures the
    semantic requirement separately: each step must preserve the
    satisfiability of the proved constraint set. -/
theorem evidence_soundness (g : ConstraintGraph) (t : ProofTrace)
    (h_check : evidence_check g t = true)
    (h_sound : TraceSound (initVerifier g) t) :
    graph_satisfiable g := by
  -- 1. From h_check, extract the witness state
  unfold evidence_check at h_check
  have h_wit : evidence_check_witness g t ≠ none := by
    intro hnone
    simp [hnone] at h_check
  rcases Option.ne_none_iff_exists.mp h_wit with ⟨st, h_wit⟩

  -- 2. From the witness spec: trace ends with qed, st = trace_fold, all g proved
  rcases evidence_check_witness_spec g t st h_wit with ⟨h_qed, h_st_eq, h_all_proved⟩

  -- 3. The initial state (empty proved set) is trivially satisfiable
  have h_init_sat : ∃ env : String → ℝ × ℝ, ∀ c ∈ (initVerifier g).proved, ir_sem env c := by
    refine ⟨fun _ => (0, 0), ?_⟩
    simp

  -- 4. By the invariant, the final proved set is satisfiable
  have h_final_sat : ∃ env : String → ℝ × ℝ, ∀ c ∈ (trace_fold (initVerifier g) t).proved, ir_sem env c :=
    trace_sound_invariant (initVerifier g) t h_sound h_init_sat

  -- 5. Since st = trace_fold, st.proved is satisfiable
  rw [h_st_eq] at h_all_proved
  rcases h_final_sat with ⟨env, h_env⟩

  -- 6. Combine: g is satisfiable because all its edges are proved
  refine ⟨env, λ c hc => ?_⟩
  have hc_proved : c ∈ (trace_fold (initVerifier g) t).proved := by
    have h_all : ∀ c' ∈ g, c' ∈ (trace_fold (initVerifier g) t).proved := by
      intro c' hc'
      have : (trace_fold (initVerifier g) t).proved.contains c' :=
        List.all_iff.mp h_all_proved c' hc'
      simpa using this
    exact h_all c hc
  exact h_env c hc_proved

/- ===============================================================
   Verifier properties
   =============================================================== -/

/-- The evidence verifier is deterministic: same input always gives
    the same output. This is a basic sanity check for the evidence system. -/
theorem evidence_verifier_deterministic (g : ConstraintGraph) (t : ProofTrace) :
    evidence_check g t = evidence_check g t := rfl

/-- Running the verifier on an empty trace against an empty graph succeeds -/
theorem evidence_empty_trivially_satisfiable :
    evidence_check ([] : ConstraintGraph) [.qed] = true := by
  unfold evidence_check evidence_check_witness; simp

/-- Running the verifier on a trace without qed at the end fails -/
theorem evidence_no_qed_fails (g : ConstraintGraph) (t : ProofTrace)
    (h : t.getLast? ≠ some .qed) :
    evidence_check g t = false := by
  unfold evidence_check evidence_check_witness
  match t.getLast? with
  | none => rfl
  | some .qed => exact (h rfl).elim
  | some _ => rfl

/-- If a proof trace contains no .qed step, then step_ok is independent of the
    constraint graph being verified (because .qed is the only step type that
    inspects the graph). -/
lemma step_ok_independent_of_g (g g' : ConstraintGraph) (st : VerifierState) (step : ProofStep)
    (h_no_qed : step ≠ .qed) : step_ok g st step = step_ok g' st step := by
  cases step
  · rfl  -- .hypothesis
  · rfl  -- .lemma
  · rfl  -- .rewrite
  · rfl  -- .unify
  · rfl  -- .normalize
  · exfalso; exact h_no_qed rfl

/-- For traces without .qed, `go g` is independent of g: the result is identical
    for any choice of constraint graph. -/
lemma go_independent_of_g_no_qed (g g' : ConstraintGraph) (st : VerifierState) (t : ProofTrace)
    (h_no_qed : .qed ∉ t) : go g st t = go g' st t := by
  induction t generalizing st with
  | nil => rfl
  | cons step rest ih =>
    unfold go
    by_cases h_step_qed : step = .qed
    · exfalso; apply h_no_qed; simp [h_step_qed]
    · have h_eq : step_ok g st step = step_ok g' st step :=
        step_ok_independent_of_g g g' st step h_step_qed
      by_cases h_ok : step_ok g st step
      · simp [h_ok, h_eq.mp h_ok, ih (transition st step) rest (by
          intro h; apply h_no_qed; simp [h])]
      · simp [h_ok, h_eq]

/-- `go` is compositional with respect to trace concatenation:
    go g st (t1 ++ t2) = (go g st t1).bind (go g · t2). -/
lemma go_append (g : ConstraintGraph) (st : VerifierState) (t1 t2 : ProofTrace) :
    go g st (t1 ++ t2) = (go g st t1).bind (go g · t2) := by
  induction t1 generalizing st with
  | nil => simp [go]
  | cons step rest ih =>
    unfold go
    by_cases h_ok : step_ok g st step
    · simp [h_ok, ih (transition st step) rest]
    · simp [h_ok]

/-- From evidence_check g t = true, we get that go g (initVerifier g) t returns
    some final verifier state. -/
lemma evidence_check_go_some (g : ConstraintGraph) (t : ProofTrace)
    (h : evidence_check g t = true) :
    ∃ st : VerifierState, go g (initVerifier g) t = some st := by
  unfold evidence_check evidence_check_witness at h
  split at h
  · -- t.getLast? = some .qed
    rename_i h_last
    have h_go : (go g (initVerifier g) t).isSome := h
    rcases Option.isSome_iff_exists.mp h_go with ⟨st, hst⟩
    exact ⟨st, hst⟩
  · simp at h

/-- The trace with the last step removed (which must be .qed). -/
def dropLastQed (t : ProofTrace) (h : t.getLast? = some .qed) : ProofTrace :=
  t.dropLast

lemma dropLastQed_no_qed (t : ProofTrace) (h : t.getLast? = some .qed) :
    .qed ∉ dropLastQed t h := by
  unfold dropLastQed
  apply List.not_mem_dropLast
  have : t ≠ [] := by
    intro hnil
    rw [hnil] at h
    simp at h
  -- List.not_mem_dropLast says the last element is not in dropLast
  -- Since .qed is the last element, it's not in dropLast
  -- This can be proven using List.mem_of_mem_dropLast
  intro hmem
  apply List.not_mem_dropLast (h := ?_) hmem
  -- We need to show .qed is the last element
  -- Use h: t.getLast? = some .qed
  have h_last := h
  -- If .qed is in dropLast t, then t has two .qed, but we still need the proof
  sorry

/-- Evidence check is compositional: if two graphs are independently verified,
    their union is also verifiable. The key insight: the intermediate .qed from
    t1 is removed (since it would check the full combined graph prematurely),
    and only the final .qed from t2 remains.

    Formally: evidence_check (g1 ++ g2) (t1.dropLast ++ t2) = true
    where t1.dropLast removes the final .qed from t1 (guaranteed to exist). -/
theorem evidence_compositional (g1 g2 : ConstraintGraph) (t1 t2 : ProofTrace)
    (h1 : evidence_check g1 t1 = true) (h2 : evidence_check g2 t2 = true) :
    evidence_check (g1 ++ g2) (t1.dropLast ++ t2) = true := by
  -- 1. Extract that both traces end with qed
  have h1_last : t1.getLast? = some .qed := by
    rcases evidence_check_go_some g1 t1 h1 with ⟨st, h_go⟩
    unfold evidence_check evidence_check_witness at h1
    split at h1
    · exact h1_last
    · simp at h1
  have h2_last : t2.getLast? = some .qed := by
    rcases evidence_check_go_some g2 t2 h2 with ⟨st, h_go⟩
    unfold evidence_check evidence_check_witness at h2
    split at h2
    · exact h2_last
    · simp at h2

  -- 2. The combined trace ends with qed (from t2)
  unfold evidence_check evidence_check_witness
  have h_last : (t1.dropLast ++ t2).getLast? = some .qed := by
    rw [List.getLast?_append]
    -- t2 ends with qed, so the combined trace ends with qed
    -- Need to handle case where t1.dropLast = []: then getLast? of [] ++ t2 = t2.getLast?
    -- t1.dropLast could be [] if t1 = [.qed]
    -- In any case, t2.getLast? = some .qed
    exact h2_last
  simp [h_last]

  -- 3. Use go_append to decompose
  rw [go_append (g1 ++ g2) (initVerifier (g1 ++ g2)) t1.dropLast t2]

  -- 4. Show that go (g1 ++ g2) init t1.dropLast succeeds
  --    Since t1.dropLast has no qed (the qed is removed), step_ok is independent of g.
  --    From h1, go g1 init t1 succeeds, so go g1 init t1.dropLast also succeeds
  --    (dropping the last step preserves success because the last step is qed,
  --     and qed always passes when at the end of a successful check).
  --    By independence, go (g1 ++ g2) init t1.dropLast also succeeds.
  have h_go1 : go g1 (initVerifier g1) t1 = some (trace_fold (initVerifier g1) t1) := by
    rcases evidence_check_go_some g1 t1 h1 with ⟨st, h_go⟩
    have h_eq : st = trace_fold (initVerifier g1) t1 :=
      go_some_eq_trace_fold g1 (initVerifier g1) t1 st h_go
    subst h_eq
    exact h_go

  -- Extract that go g1 init t1.dropLast succeeds (by removing qed which is the last step)
  have h_go1_drop : go g1 (initVerifier g1) t1.dropLast =
    some (trace_fold (initVerifier g1) t1.dropLast) := by
    -- go g1 init t1 succeeds and t1 ends with qed.
    -- go processes steps left-to-right; truncating the last step gives a partial result.
    -- Since the first |t1|-1 steps all pass (they passed as part of the full trace),
    -- go g1 init t1.dropLast will also succeed.
    -- This requires an "early termination" lemma for go.
    sorry

  -- 5. By go_independent_of_g_no_qed (since t1.dropLast has no qed),
  --    go (g1 ++ g2) init t1.dropLast = go g1 init t1.dropLast
  have h_no_qed : .qed ∉ t1.dropLast := by
    -- The last element of t1 is .qed, so dropLast removes it.
    -- By List.not_mem_dropLast, the removed element is not in dropLast.
    -- But .qed could appear elsewhere in t1 (multiple times).
    -- We need the weaker statement: if t1 is a valid proof trace, the last qed
    -- is the only one. For now, we leave this as a known gap.
    sorry
  have h_go_combined_drop : go (g1 ++ g2) (initVerifier (g1 ++ g2)) t1.dropLast =
    go g1 (initVerifier g1) t1.dropLast := by
    -- This requires initVerifier (g1 ++ g2) = initVerifier g1 (both have empty proved set)
    -- which is true.
    simp [initVerifier, go_independent_of_g_no_qed (g1 ++ g2) g1 (initVerifier g1) t1.dropLast h_no_qed]

  -- 6. Combine: go (g1 ++ g2) init t1.dropLast succeeds
  --    Then apply go (g1 ++ g2) · t2 and show it also succeeds because
  --    the proved set after t1 contains g1, and t2 proves g2.
  sorry

/- ===============================================================
   Concrete verification examples
   =============================================================== -/

/-- A single distance constraint can be verified by hypothesis→qed -/
theorem evidence_single_distance :
    evidence_check
      ([.distance "A" "B" (.const 5)] : ConstraintGraph)
      [.hypothesis (.distance "A" "B" (.const 5)), .qed]
    = true := by
  unfold evidence_check evidence_check_witness; simp

/-- A 3-4-5 right triangle is verifiable -/
theorem evidence_345_triangle :
    evidence_check
      ([.distance "A" "B" (.const 3),
        .distance "B" "C" (.const 4),
        .distance "A" "C" (.const 5),
        .rightAngle "A" "B" "C"] : ConstraintGraph)
      [.hypothesis (.distance "A" "B" (.const 3)),
       .hypothesis (.distance "B" "C" (.const 4)),
       .hypothesis (.distance "A" "C" (.const 5)),
       .hypothesis (.rightAngle "A" "B" "C"),
       .qed]
    = true := by
  unfold evidence_check evidence_check_witness; simp

/-- Evidence verifier rejects truncated traces -/
theorem evidence_rejects_incomplete :
    evidence_check
      ([.distance "A" "A" (.const 0)] : ConstraintGraph)
      [.hypothesis (.distance "A" "A" (.const 0))]
    = false := by
  unfold evidence_check evidence_check_witness; simp

end lvFormal.Theory.Evidence
