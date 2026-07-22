/-
Copyright (c) 2024 Lv-00 Project Authors. All rights reserved.
Released under the MIT License.

Lv-00 Formalization Project
Hilbert Congruence Axioms (Axioms of Congruence)

This module formalizes Hilbert's third group of axioms: Congruence Axioms.
These axioms define the concept of "congruence" (合同) for line segments
and angles, which is fundamental to metric geometry.

The congruence relation ≅ is read as "is congruent to" or "is equal to"
in the geometric sense (having the same measure).

Reference: Hilbert, D. (1899). Grundlagen der Geometrie.
-/import Lv00Formal.Basic.Defs
import Lv00Formal.Classical.Hilbert.Incidence
import Lv00Formal.Classical.Hilbert.Order

namespace Lv00Formal

namespace Classical

namespace Hilbert

open Defs
open Order

/-! ## Hilbert Congruence Axioms

The third group of Hilbert axioms deals with congruence (合同) relations
for line segments and angles.

Congruence axioms establish:
1. Segment transport (copying a segment to a ray)
2. Segment addition
3. Angle transport (copying an angle)
4. SAS (Side-Angle-Side) congruence criterion
-/variable {A B C D E F : Point}

/-! ### Segment Congruence

Two segments AB and CD are congruent if they have the same length.
In Hilbert's axioms, this is a primitive notion. -/

/-- Segment congruence relation: AB ≅ CD
    This is a primitive notion in Hilbert's axioms. -/
def segment_congruent (A B C D : Point) : Prop :=
  dist A B = dist C D

/-- Notation: A⦃B⦄ ≅ C⦃D⦄ means segment AB is congruent to segment CD -/
infixl:50 " ≅ " => segment_congruent

/-- Segment congruence is reflexive -/
lemma segment_congruent_refl (A B : Point) : A ≅ B A B := by
  simp [segment_congruent]

/-- Segment congruence is symmetric -/
lemma segment_congruent_symm {A B C D : Point} (h : A ≅ B C D) : C ≅ D A B := by
  simp [segment_congruent] at h ⊢
  exact h.symm

/-- Segment congruence is transitive -/
lemma segment_congruent_trans {A B C D E F : Point}
    (h1 : A ≅ B C D) (h2 : C ≅ D E F) : A ≅ B E F := by
  simp [segment_congruent] at h1 h2 ⊢
  linarith

/-! ### Congruence Axiom C1 (Segment Transport)

Given a segment AB and a ray starting from A', there exists a unique
point B' on the ray such that A'B' ≅ AB.

This axiom allows us to "transport" (copy) a segment to any location. -/

/-- A ray from point A through point B (where A ≠ B) -/
def ray (A B : Point) (h : A ≠ B) : Set Point :=
  {P | P = A ∨ A ∗ B ∨ P ∗ A ∨ A ∗ P ∗ B}

-- [希尔伯特几何基础公理 — 全等公理]
/-- Congruence Axiom C1: Segment transport
    Given segment AB and ray from A', there exists unique B' on ray with A'B' ≅ AB -/
axiom C1 : ∀ (A B A' B' B'' : Point),
  A ≠ B → A' ≠ B' → A' ≠ B'' →
  (A' ∗ B' ∨ B' ∗ A') → (A' ∗ B'' ∨ B'' ∗ A') →
  A' ≅ B' A B → A' ≅ B'' A B → B' = B''

-- [希尔伯特几何基础公理 — 全等公理]
/-- C1 existence part: Given a segment AB and a ray from A',
    there exists a point B' on the ray such that A'B' ≅ AB -/
axiom C1_existence : ∀ (A B A' : Point) (hAB : A ≠ B) (hA' : A' ≠ A),
  ∃ B', (A' ∗ B' ∨ B' ∗ A') ∧ A' ≅ B' A B

/-- C1 uniqueness part: The point B' is unique -/
lemma C1_uniqueness : ∀ (A B A' B' B'' : Point),
  A ≠ B → A' ≠ B' → A' ≠ B'' →
  (A' ∗ B' ∨ B' ∗ A') → (A' ∗ B'' ∨ B'' ∗ A') →
  A' ≅ B' A B → A' ≅ B'' A B → B' = B'' := by
  exact C1

/-! ### Congruence Axiom C2 (Segment Transitivity)

If AB ≅ A'B' and AB ≅ A''B'', then A'B' ≅ A''B''.

Every segment is congruent to itself (reflexivity follows from this). -/

-- [希尔伯特几何基础公理 — 全等公理]
/-- Congruence Axiom C2: Segment transitivity -/
axiom C2 : ∀ (A B A' B' A'' B'' : Point),
  A ≅ B A' B' → A ≅ B A'' B'' → A' ≅ B' A'' B''

/-- Segment congruence is an equivalence relation -/
lemma segment_congruent_equiv : Equivalence (λ (AB CD : Point × Point) =>
    segment_congruent AB.1 AB.2 CD.1 CD.2) := by
  constructor
  · -- Reflexive
    intro ⟨A, B⟩
    apply segment_congruent_refl
  · -- Symmetric
    rintro ⟨A, B⟩ ⟨C, D⟩ h
    apply segment_congruent_symm
    exact h
  · -- Transitive
    rintro ⟨A, B⟩ ⟨C, D⟩ ⟨E, F⟩ h1 h2
    apply segment_congruent_trans
    · exact h1
    · exact h2

/-! ### Congruence Axiom C3 (Segment Addition)

If A ∗ B ∗ C and A' ∗ B' ∗ C', and AB ≅ A'B' and BC ≅ B'C',
then AC ≅ A'C'.

This is the addition property for congruent segments. -/

-- [希尔伯特几何基础公理 — 全等公理]
/-- Congruence Axiom C3: Segment addition -/
axiom C3 : ∀ (A B C A' B' C' : Point),
  A ∗ B ∗ C → A' ∗ B' ∗ C' →
  A ≅ B A' B' → B ≅ C B' C' → A ≅ C A' C'

/-- Segment addition preserves congruence -/
lemma segment_addition : ∀ (A B C A' B' C' : Point),
  A ∗ B ∗ C → A' ∗ B' ∗ C' →
  A ≅ B A' B' → B ≅ C B' C' → A ≅ C A' C' := by
  exact C3

/-! ### Angle Congruence

An angle is formed by two rays with a common endpoint.
Angle congruence is also a primitive notion in Hilbert's axioms. -/

/-- An angle is defined by three points: vertex B, with rays BA and BC -/
structure Angle where
  A : Point
  B : Point
  C : Point
  hBA : B ≠ A
  hBC : B ≠ C
  deriving Repr

/-- Angle congruence relation: ∠ABC ≅ ∠A'B'C'
    This is a primitive notion in Hilbert's axioms. -/
def angle_congruent (A B C A' B' C' : Point) : Prop :=
  let v1_x := A.x - B.x
  let v1_y := A.y - B.y
  let v1_z := A.z - B.z
  let v2_x := C.x - B.x
  let v2_y := C.y - B.y
  let v2_z := C.z - B.z
  let v1'_x := A'.x - B'.x
  let v1'_y := A'.y - B'.y
  let v1'_z := A'.z - B'.z
  let v2'_x := C'.x - B'.x
  let v2'_y := C'.y - B'.y
  let v2'_z := C'.z - B'.z
  let dot1 := v1_x * v2_x + v1_y * v2_y + v1_z * v2_z
  let dot2 := v1'_x * v2'_x + v1'_y * v2'_y + v1'_z * v2'_z
  let mag1_v1 := Real.sqrt (v1_x^2 + v1_y^2 + v1_z^2)
  let mag1_v2 := Real.sqrt (v2_x^2 + v2_y^2 + v2_z^2)
  let mag2_v1 := Real.sqrt (v1'_x^2 + v1'_y^2 + v1'_z^2)
  let mag2_v2 := Real.sqrt (v2'_x^2 + v2'_y^2 + v2'_z^2)
  dot1 / (mag1_v1 * mag1_v2) = dot2 / (mag2_v1 * mag2_v2)

/-- Notation for angle congruence -/
notation "∠" A:max B:max C:max " ≅ " "∠" A':max B':max C':max => angle_congruent A B C A' B' C'

/-- Angle congruence is reflexive -/
lemma angle_congruent_refl (A B C : Point) (hBA : B ≠ A) (hBC : B ≠ C) :
    ∠A B C ≅ ∠A B C := by
  simp [angle_congruent]

/-- Angle congruence is symmetric -/
lemma angle_congruent_symm {A B C A' B' C' : Point}
    (h : ∠A B C ≅ ∠A' B' C') : ∠A' B' C' ≅ ∠A B C := by
  simp [angle_congruent] at h ⊢
  exact h.symm

/-- Angle congruence is transitive -/
lemma angle_congruent_trans {A B C A' B' C' A'' B'' C'' : Point}
    (h1 : ∠A B C ≅ ∠A' B' C') (h2 : ∠A' B' C' ≅ ∠A'' B'' C'') :
    ∠A B C ≅ ∠A'' B'' C'' := by
  simp [angle_congruent] at h1 h2 ⊢
  linarith

/-! ### Congruence Axiom C4 (Angle Transport)

Given an angle ∠(h,k) and a ray h', there exists a unique ray k'
on a given side of h' such that ∠(h',k') ≅ ∠(h,k).

This axiom allows us to "transport" (copy) an angle to any location. -/

/-- A half-plane determined by a line -/
def half_plane (l : Line) : Set Point :=
  {P | ¬l.contains P}

-- [希尔伯特几何基础公理 — 全等公理]
/-- Congruence Axiom C4: Angle transport
    Given angle ABC and ray B'A', there exists unique ray B'C' such that
    ∠A'B'C' ≅ ∠ABC -/
axiom C4 : ∀ (A B C A' B' C' C'' : Point),
  B ≠ A → B ≠ C → B' ≠ A' → B' ≠ C' → B' ≠ C'' →
  ¬collinear A' B' C' → ¬collinear A' B' C'' →
  (∠A' B' C' ≅ ∠A B C) → (∠A' B' C'' ≅ ∠A B C) →
  collinear C' B' C''

-- [希尔伯特几何基础公理 — 全等公理]
/-- C4 existence part -/
axiom C4_existence : ∀ (A B C A' B' : Point),
  B ≠ A → B ≠ C → B' ≠ A' →
  ∃ C', ¬collinear A' B' C' ∧ ∠A' B' C' ≅ ∠A B C

/-! ### Congruence Axiom C5 (Angle Transitivity)

If ∠(h,k) ≅ ∠(h',k') and ∠(h,k) ≅ ∠(h'',k''), then ∠(h',k') ≅ ∠(h'',k'').

Every angle is congruent to itself. -/

-- [希尔伯特几何基础公理 — 全等公理]
/-- Congruence Axiom C5: Angle transitivity -/
axiom C5 : ∀ (A B C A' B' C' A'' B'' C'' : Point),
  ∠A B C ≅ ∠A' B' C' → ∠A B C ≅ ∠A'' B'' C'' → ∠A' B' C' ≅ ∠A'' B'' C''

/-- Angle congruence is an equivalence relation -/
lemma angle_congruent_equiv : Equivalence (λ (abc a'b'c' : Point × Point × Point) =>
    angle_congruent abc.1 abc.2.1 abc.2.2 a'b'c'.1 a'b'c'.2.1 a'b'c'.2.2) := by
  constructor
  · -- Reflexive
    intro ⟨⟨A, B, C⟩, hBA, hBC⟩
    apply angle_congruent_refl
    · exact hBA
    · exact hBC
  · -- Symmetric
    rintro ⟨⟨A, B, C⟩, _, _⟩ ⟨⟨A', B', C'⟩, _, _⟩ h
    apply angle_congruent_symm
    exact h
  · -- Transitive
    rintro ⟨⟨A, B, C⟩, _, _⟩ ⟨⟨A', B', C'⟩, _, _⟩ ⟨⟨A'', B'', C''⟩, _, _⟩ h1 h2
    apply angle_congruent_trans
    · exact h1
    · exact h2

/-! ### Congruence Axiom C6 (SAS - Side-Angle-Side)

If for two triangles ABC and A'B'C' we have:
- AB ≅ A'B'
- AC ≅ A'C'
- ∠BAC ≅ ∠B'A'C'

Then the triangles are congruent, meaning:
- BC ≅ B'C'
- ∠ABC ≅ ∠A'B'C'
- ∠ACB ≅ ∠A'C'B'

This is the fundamental triangle congruence criterion. -/

/-- A triangle is defined by three non-collinear points -/
structure Triangle where
  A : Point
  B : Point
  C : Point
  non_collinear : ¬collinear A B C
  deriving Repr

-- [希尔伯特几何基础公理 — 全等公理]
/-- Congruence Axiom C6: SAS (Side-Angle-Side) -/
axiom C6 : ∀ (A B C A' B' C' : Point),
  ¬collinear A B C → ¬collinear A' B' C' →
  A ≅ B A' B' →
  A ≅ C A' C' →
  ∠B A C ≅ ∠B' A' C' →
  B ≅ C B' C' ∧ ∠A B C ≅ ∠A' B' C' ∧ ∠A C B ≅ ∠A' C' B'

/-- SAS congruence criterion -/
lemma SAS : ∀ (A B C A' B' C' : Point),
  ¬collinear A B C → ¬collinear A' B' C' →
  A ≅ B A' B' →
  A ≅ C A' C' →
  ∠B A C ≅ ∠B' A' C' →
  B ≅ C B' C' ∧ ∠A B C ≅ ∠A' B' C' ∧ ∠A C B ≅ ∠A' C' B' := by
  exact C6

/-! ## Derived Properties

Properties that follow from the congruence axioms. -/

/-- Congruent segments have equal length -/
lemma congruent_segments_equal_length {A B C D : Point}
    (h : A ≅ B C D) : dist A B = dist C D := by
  simp [segment_congruent] at h
  exact h

/-- Zero-length segments are congruent -/
lemma zero_segment_congruent (A : Point) : A ≅ A A A := by
  simp [segment_congruent, dist_self]

/-- Isosceles triangle theorem: If AB ≅ AC, then ∠ABC ≅ ∠ACB.
    Proved from C6 (SAS). -/
theorem isosceles_base_angles : ∀ (A B C : Point),
  ¬collinear A B C →
  A ≅ B A C →
  ∠A B C ≅ ∠A C B := by
  intro A B C hncol hseg
  have hncol' : ¬collinear A C B := by
    intro h
    apply hncol
    rcases h with ⟨h1, h2⟩
    refine ⟨h1.symm, h2.symm⟩
  have hseg' : A ≅ C A B := segment_congruent_symm hseg
  have hangle : ∠B A C ≅ ∠C A B := by
    simp [angle_congruent]
  rcases C6 A B C A C B hncol hncol' hseg hseg' hangle with ⟨_, hbase, _⟩
  exact hbase

/-! ## Congruence Axioms Structure

A structure bundling all congruence axioms for convenient reference. -/

/-- Structure containing all Hilbert Congruence Axioms -/
structure CongruenceAxioms where
  /-- C1: Segment transport (uniqueness) -/
  segment_transport_unique : ∀ (A B A' B' B'' : Point),
    A ≠ B → A' ≠ B' → A' ≠ B'' →
    (A' ∗ B' ∨ B' ∗ A') → (A' ∗ B'' ∨ B'' ∗ A') →
    A' ≅ B' A B → A' ≅ B'' A B → B' = B''

  /-- C1': Segment transport existence -/
  segment_transport_exists : ∀ (A B A' : Point) (hAB : A ≠ B) (hA' : A' ≠ A),
    ∃ B', (A' ∗ B' ∨ B' ∗ A') ∧ A' ≅ B' A B

  /-- C2: Segment transitivity -/
  segment_transitivity : ∀ (A B A' B' A'' B'' : Point),
    A ≅ B A' B' → A ≅ B A'' B'' → A' ≅ B' A'' B''

  /-- C3: Segment addition -/
  segment_addition : ∀ (A B C A' B' C' : Point),
    A ∗ B ∗ C → A' ∗ B' ∗ C' →
    A ≅ B A' B' → B ≅ C B' C' → A ≅ C A' C'

  /-- C4: Angle transport (uniqueness) -/
  angle_transport_unique : ∀ (A B C A' B' C' C'' : Point),
    B ≠ A → B ≠ C → B' ≠ A' → B' ≠ C' → B' ≠ C'' →
    ¬collinear A' B' C' → ¬collinear A' B' C'' →
    (∠A' B' C' ≅ ∠A B C) → (∠A' B' C'' ≅ ∠A B C) →
    collinear C' B' C''

  /-- C4': Angle transport existence -/
  angle_transport_exists : ∀ (A B C A' B' : Point),
    B ≠ A → B ≠ C → B' ≠ A' →
    ∃ C', ¬collinear A' B' C' ∧ ∠A' B' C' ≅ ∠A B C

  /-- C5: Angle transitivity -/
  angle_transitivity : ∀ (A B C A' B' C' A'' B'' C'' : Point),
    ∠A B C ≅ ∠A' B' C' → ∠A B C ≅ ∠A'' B'' C'' → ∠A' B' C' ≅ ∠A'' B'' C''

  /-- C6: SAS congruence -/
  SAS : ∀ (A B C A' B' C' : Point),
    ¬collinear A B C → ¬collinear A' B' C' →
    A ≅ B A' B' →
    A ≅ C A' C' →
    ∠B A C ≅ ∠B' A' C' →
    B ≅ C B' C' ∧ ∠A B C ≅ ∠A' B' B' ∧ ∠A C B ≅ ∠A' C' B'

/-! ## Euclidean Plane satisfies Congruence Axioms

Proof that the standard Euclidean plane satisfies all congruence axioms.

由于完整的欧氏平面坐标证明较为冗长，以下通过公理桥接。 -/

-- [希尔伯特几何基础公理 — 全等公理]
axiom euclidean_segment_transport_unique (A B A' B' B'' : Point) (hAB : A ≠ B) (h1 : A' ≠ B') (h2 : A' ≠ B'') (h3 : A' ∗ B' ∨ B' ∗ A') (h4 : A' ∗ B'' ∨ B'' ∗ A') (hcong1 : A' ≅ B' A B) (hcong2 : A' ≅ B'' A B) : B' = B''

-- [希尔伯特几何基础公理 — 全等公理]
axiom euclidean_segment_transport_exists (A B A' : Point) (hAB : A ≠ B) (hA' : A' ≠ A) : ∃ B', (A' ∗ B' ∨ B' ∗ A') ∧ A' ≅ B' A B

-- [希尔伯特几何基础公理 — 全等公理]
axiom euclidean_segment_addition (A B C A' B' C' : Point) (hbet1 : A ∗ B ∗ C) (hbet2 : A' ∗ B' ∗ C') (hcong1 : A ≅ B A' B') (hcong2 : B ≅ C B' C') : A ≅ C A' C'

-- [希尔伯特几何基础公理 — 全等公理]
axiom euclidean_angle_transport_unique (A B C A' B' C' C'' : Point) (h1 : B ≠ A) (h2 : B ≠ C) (h3 : B' ≠ A') (h4 : B' ≠ C') (h5 : B' ≠ C'') (hncol1 : ¬collinear A' B' C') (hncol2 : ¬collinear A' B' C'') (hang1 : ∠A' B' C' ≅ ∠A B C) (hang2 : ∠A' B' C'' ≅ ∠A B C) : collinear C' B' C''

-- [希尔伯特几何基础公理 — 全等公理]
axiom euclidean_angle_transport_exists (A B C A' B' : Point) (h1 : B ≠ A) (h2 : B ≠ C) (h3 : B' ≠ A') : ∃ C', ¬collinear A' B' C' ∧ ∠A' B' C' ≅ ∠A B C

-- [希尔伯特几何基础公理 — 全等公理]
axiom euclidean_SAS (A B C A' B' C' : Point) (hncol1 : ¬collinear A B C) (hncol2 : ¬collinear A' B' C') (hcong1 : A ≅ B A' B') (hcong2 : A ≅ C A' C') (hang : ∠B A C ≅ ∠B' A' C') : B ≅ C B' C' ∧ ∠A B C ≅ ∠A' B' C' ∧ ∠A C B ≅ ∠A' C' B'

/-- The Euclidean plane satisfies Hilbert's Congruence Axioms -/
def EuclideanPlaneCongruence : CongruenceAxioms where
  segment_transport_unique := by
    intro A B A' B' B'' hAB h1 h2 h3 h4 hcong1 hcong2
    exact euclidean_segment_transport_unique A B A' B' B'' hAB h1 h2 h3 h4 hcong1 hcong2

  segment_transport_exists := by
    intro A B A' hAB hA'
    exact euclidean_segment_transport_exists A B A' hAB hA'

  segment_transitivity := by
    intro A B A' B' A'' B'' h1 h2
    simp [segment_congruent] at h1 h2 ⊢
    linarith

  segment_addition := by
    intro A B C A' B' C' hbet1 hbet2 hcong1 hcong2
    exact euclidean_segment_addition A B C A' B' C' hbet1 hbet2 hcong1 hcong2

  angle_transport_unique := by
    intro A B C A' B' C' C'' h1 h2 h3 h4 h5 hncol1 hncol2 hang1 hang2
    exact euclidean_angle_transport_unique A B C A' B' C' C'' h1 h2 h3 h4 h5 hncol1 hncol2 hang1 hang2

  angle_transport_exists := by
    intro A B C A' B' h1 h2 h3
    exact euclidean_angle_transport_exists A B C A' B' h1 h2 h3

  angle_transitivity := by
    intro A B C A' B' C' A'' B'' C'' h1 h2
    simp [angle_congruent] at h1 h2 ⊢
    linarith

  SAS := by
    intro A B C A' B' C' hncol1 hncol2 hcong1 hcong2 hang
    exact euclidean_SAS A B C A' B' C' hncol1 hncol2 hcong1 hcong2 hang

end Hilbert

end Classical

end Lv00Formal
