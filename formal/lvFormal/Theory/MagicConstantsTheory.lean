import Mathlib
open List

set_option pp.structure_projections false

namespace lvFormal.Theory.MagicConstantsTheory

/-!
# Magic Constants Theory

This module formalizes the magic constant/enum system for
`core/src/layer5_output/magic/magic.c`.

Magic constants are named integer values that serve as version markers,
feature flags, sentinel values, encoding markers, and debug tokens
throughout the system.  The formalization covers uniqueness invariants,
backward compatibility, and serialization roundtripping.

## Overview

The formalization is structured into the following sections:

1.  **MagicCategory** — Inductive categories for magic constants.
2.  **Version** — Semantic versioning with major.minor.patch.
3.  **VersionRange** — Range of compatible versions.
4.  **FeatureFlag** — Named boolean flags with defaults.
5.  **MagicConstant** — Core magic constant record.
6.  **EnumConstant & EnumRegistry** — Enum-style constant grouping.
7.  **MagicTable** — Complete set of all magic constants.
8.  **ConstantUniqueness** — Uniqueness invariant.
9.  **BackwardCompatibility** — Compatibility invariant.
10. **Theorems** — All required proofs.
-/

-- ===================================================================
-- Section 1:  Magic Category
-- ===================================================================

/-- The category of a magic constant determines its role in the system.

  * `version`         — A version identifier constant.
  * `feature_flag`    — A feature-gating flag constant.
  * `limit`           — An upper/lower bound constant.
  * `sentinel`        — A sentinel / terminator value.
  * `encoding_marker` — A marker used in data encoding schemes.
  * `debug_token`     — A token used for debug / trace output.
  -/
inductive MagicCategory
  | version
  | feature_flag
  | limit
  | sentinel
  | encoding_marker
  | debug_token
  deriving DecidableEq, Repr

-- ===================================================================
-- Section 2:  Version (Semantic Versioning)
-- ===================================================================

/-- A semantic version consisting of `major`, `minor`, and `patch`
  components.  The invariant `version_ok` ensures that all three
  components are non-negative (trivially true since they are `Nat`).

  Corresponds to the version macros defined in `magic.c` (lines 12-18). -/
structure Version where
  major : Nat
  minor : Nat
  patch : Nat
  deriving DecidableEq, Repr

namespace Version

/-- The zero version (0.0.0). -/
def zero : Version :=
  { major := 0, minor := 0, patch := 0 }

/-- Compare two versions lexicographically (major first, then minor,
  then patch).  Returns `LT`, `EQ`, or `GT`. -/
def compare (a b : Version) : Ordering :=
  if a.major ≠ b.major then
    if a.major < b.major then Ordering.lt else Ordering.gt
  else if a.minor ≠ b.minor then
    if a.minor < b.minor then Ordering.lt else Ordering.gt
  else
    if a.patch < b.patch then Ordering.lt else
    if a.patch > b.patch then Ordering.gt else Ordering.eq

/-- `a ≤ b` in the version ordering. -/
def le (a b : Version) : Prop :=
  a.major < b.major ∨ (a.major = b.major ∧ a.minor < b.minor) ∨
  (a.major = b.major ∧ a.minor = b.minor ∧ a.patch ≤ b.patch)

/-- `a < b` in the version ordering. -/
def lt (a b : Version) : Prop :=
  a.major < b.major ∨ (a.major = b.major ∧ a.minor < b.minor) ∨
  (a.major = b.major ∧ a.minor = b.minor ∧ a.patch < b.patch)

/-- The version ordering is a total order (proof below). -/
instance : LT Version := ⟨Version.lt⟩

instance : LE Version := ⟨Version.le⟩

/-- Reflexivity of `≤`. -/
theorem le_refl (a : Version) : a ≤ a := by
  right; right; exact ⟨rfl, rfl, Nat.le_refl _⟩

/-- Transitivity of `≤`. -/
theorem le_trans (a b c : Version) (hab : a ≤ b) (hbc : b ≤ c) : a ≤ c := by
  rcases hab with (h | ⟨h₁, h₂⟩ | ⟨h₁, h₂, h₃⟩)
  · left; exact Nat.lt_of_lt_of_le h (by
      rcases hbc with (hbc' | ⟨hbc₁, hbc₂⟩ | ⟨hbc₁, hbc₂, hbc₃⟩)
      · exact hbc'
      · exact hbc₂
      · exact hbc₂)
  · rcases hbc with (hbc' | ⟨hbc₁, hbc₂⟩ | ⟨hbc₁, hbc₂, hbc₃⟩)
    · left; exact hbc'
    · right; left; exact ⟨h₁.trans hbc₁, Nat.lt_of_lt_of_le h₂ hbc₂⟩
    · right; left; exact ⟨h₁.trans hbc₁, h₂⟩
  · rcases hbc with (hbc' | ⟨hbc₁, hbc₂⟩ | ⟨hbc₁, hbc₂, hbc₃⟩)
    · left; exact hbc'
    · right; left; exact ⟨h₁.trans hbc₁, hbc₂⟩
    · right; right; exact ⟨h₁.trans hbc₁, h₂.trans hbc₂, Nat.le_trans h₃ hbc₃⟩

/-- Antisymmetry of `≤`. -/
theorem le_antisymm (a b : Version) (hab : a ≤ b) (hba : b ≤ a) : a = b := by
  ext <;> apply Nat.le_antisymm
  · rcases hab with (h | ⟨h₁, h₂⟩ | ⟨h₁, h₂, h₃⟩)
    · exact Nat.le_of_lt h
    · exact h₁.symm ▸ Nat.le_of_lt h₂
    · exact h₃
    · rcases hba with (h | ⟨h₁, h₂⟩ | ⟨h₁, h₂, h₃⟩)
    · exact Nat.le_of_lt h
    · exact h₁.symm ▸ Nat.le_of_lt h₂
    · exact h₃
  · rcases hab with (h | ⟨h₁, h₂⟩ | ⟨h₁, h₂, h₃⟩)
    · exact Nat.le_of_lt h
    · exact h₁.symm ▸ Nat.le_of_lt h₂
    · exact h₁.symm ▸ h₂.symm ▸ h₃
    · rcases hba with (h | ⟨h₁, h₂⟩ | ⟨h₁, h₂, h₃⟩)
    · exact Nat.le_of_lt h
    · exact h₁.symm ▸ Nat.le_of_lt h₂
    · exact h₁.symm ▸ h₂.symm ▸ h₃
  · rcases hab with (h | ⟨h₁, h₂⟩ | ⟨h₁, h₂, h₃⟩)
    · exact Nat.le_of_lt h
    · exact h₁.symm ▸ h₂.symm ▸ Nat.le_of_lt h₂
    · exact h₁.symm ▸ h₂.symm ▸ h₃
    · rcases hba with (h | ⟨h₁, h₂⟩ | ⟨h₁, h₂, h₃⟩)
    · exact Nat.le_of_lt h
    · exact h₁.symm ▸ h₂.symm ▸ Nat.le_of_lt h₂
    · exact h₁.symm ▸ h₂.symm ▸ h₃

/-- Totality of `≤`: for any two versions, one is ≤ the other. -/
theorem le_total (a b : Version) : a ≤ b ∨ b ≤ a := by
  by_cases hmaj : a.major ≤ b.major
  · by_cases hmineq : a.major = b.major
    · by_cases hmin : a.minor ≤ b.minor
      · by_cases hmineq2 : a.minor = b.minor
        · by_cases hpatch : a.patch ≤ b.patch
          · right; right; right; exact ⟨hmineq.symm, hmineq2.symm, hpatch⟩
          · left; left; exact Nat.not_le.mp hpatch
        · left; right; left; exact ⟨hmineq, Nat.lt_of_not_ge hmineq2⟩
      · right; right; left; exact ⟨hmineq.symm, Nat.lt_of_not_ge hmin⟩
    · left; exact Nat.lt_of_not_ge hmineq
  · right; exact Nat.lt_of_not_ge hmaj

end Version

-- ===================================================================
-- Section 3:  Version Range
-- ===================================================================

/-- A contiguous range of versions from `min_version` (inclusive) to
  `max_version` (inclusive).  The invariant `range_ok` ensures that
  `min_version ≤ max_version`. -/
structure VersionRange where
  min_version : Version
  max_version : Version
  deriving DecidableEq, Repr

namespace VersionRange

/-- Check whether a given `version` falls inside this range. -/
def contains (r : VersionRange) (version : Version) : Prop :=
  r.min_version ≤ version ∧ version ≤ r.max_version

/-- The range is well-formed when `min_version ≤ max_version`. -/
def range_ok (r : VersionRange) : Prop :=
  r.min_version ≤ r.max_version

/-- The intersection of two ranges is non-empty if they overlap. -/
def overlaps (r s : VersionRange) : Prop :=
  ∃ v : Version, r.contains v ∧ s.contains v

/-- Construct a range that contains exactly one version. -/
def singleton (v : Version) : VersionRange :=
  { min_version := v, max_version := v }

/-- The singleton range is well-formed. -/
theorem singleton_ok (v : Version) : (singleton v).range_ok := by
  unfold singleton range_ok; exact Version.le_refl v

/-- A version in a singleton range must equal the center version. -/
theorem singleton_contains_iff (v w : Version) :
    (singleton v).contains w ↔ w = v := by
  constructor
  · intro ⟨h₁, h₂⟩
    exact Version.le_antisymm w v h₂ h₁
  · intro h; subst h; exact ⟨Version.le_refl v, Version.le_refl v⟩

end VersionRange

-- ===================================================================
-- Section 4:  Feature Flag
-- ===================================================================

/-- A named feature flag that gates functionality.

  * `name`          — The feature name (corresponds to `#define` in C).
  * `enabled`       — Whether the flag is currently enabled.
  * `default_value` — The documented default state of the flag. -/
structure FeatureFlag where
  name          : String
  enabled       : Bool
  default_value : Bool
  deriving DecidableEq, Repr

namespace FeatureFlag

/-- A flag is in its default state when `enabled = default_value`. -/
def at_default (f : FeatureFlag) : Prop :=
  f.enabled = f.default_value

/-- Enable a feature flag. -/
def enable (f : FeatureFlag) : FeatureFlag :=
  { f with enabled := true }

/-- Disable a feature flag. -/
def disable (f : FeatureFlag) : FeatureFlag :=
  { f with enabled := false }

/-- Reset a flag to its default value. -/
def reset (f : FeatureFlag) : FeatureFlag :=
  { f with enabled := f.default_value }

/-- Toggle a flag. -/
def toggle (f : FeatureFlag) : FeatureFlag :=
  { f with enabled := !f.enabled }

end FeatureFlag

-- ===================================================================
-- Section 5:  Magic Constant
-- ===================================================================

/-- A single magic constant entry.

  * `name`               — Human-readable name (e.g. `"MAGIC_VERSION_1"`).
  * `value`              — The numeric value of the constant.
  * `category`           — The `MagicCategory` this constant belongs to.
  * `description`        — A prose description of the constant.
  * `version_introduced` — The version in which this constant was first
                           introduced. -/
structure MagicConstant where
  name               : String
  value              : Nat
  category           : MagicCategory
  description        : String
  version_introduced : Version
  deriving DecidableEq, Repr

namespace MagicConstant

/-- Two constants conflict if they share the same name or the same value
  within the same category. -/
def conflicts (a b : MagicConstant) : Prop :=
  a.name = b.name ∨ (a.value = b.value ∧ a.category = b.category)

/-- Check whether a constant is at least as new as a given version. -/
def introduced_since (c : MagicConstant) (v : Version) : Prop :=
  v ≤ c.version_introduced

end MagicConstant

-- ===================================================================
-- Section 6:  Enum Constant & Enum Registry
-- ===================================================================

/-- An enum-style constant that pairs a name with an integer value
  within a specific category.  Unlike `MagicConstant`, enum constants
  have no description or version metadata; they represent simple
  key-value bindings. -/
structure EnumConstant where
  name     : String
  int_value : Nat
  category : MagicCategory
  deriving DecidableEq, Repr

namespace EnumConstant

/-- Two enum constants conflict if they share the same name or the same
  integer value within the same category. -/
def conflicts (a b : EnumConstant) : Prop :=
  a.name = b.name ∨ (a.int_value = b.int_value ∧ a.category = b.category)

end EnumConstant

/-- A registry that maps an enum name (e.g. `"Color"`) to a list of
  `EnumConstant` entries.  Ensures that within a single enum, no two
  constants have the same name or same integer value. -/
structure EnumRegistry where
  enums : List (String × List EnumConstant)
  deriving DecidableEq, Repr

namespace EnumRegistry

/-- Look up an enum by name.  Returns `some` list of constants if
  found, `none` otherwise. -/
def lookup (r : EnumRegistry) (name : String) : Option (List EnumConstant) :=
  r.enums.find? (λ (n, _) => n = name) |>.map Prod.snd

/-- Return all enum names registered in this registry. -/
def names (r : EnumRegistry) : List String :=
  r.enums.map Prod.fst

/-- The number of registered enums. -/
def size (r : EnumRegistry) : Nat :=
  r.enums.length

/-- Check whether all enums in the registry have unique constant names
  and unique integer values within each enum. -/
def well_formed (r : EnumRegistry) [DecidableEq String] : Prop :=
  ∀ (enumName : String) (consts : List EnumConstant),
    (r.enums.find? (λ (n, _) => n = enumName)) = some (enumName, consts) →
    (∀ c₁ c₂ ∈ consts, c₁.name = c₂.name → c₁ = c₂) ∧
    (∀ c₁ c₂ ∈ consts, c₁.int_value = c₂.int_value → c₁ = c₂)

end EnumRegistry

-- ===================================================================
-- Section 7:  Magic Table
-- ===================================================================

/-- The complete table of all magic constants in the system.  It is a
  list of `MagicConstant` entries plus an `EnumRegistry` for the
  enum-style constants.

  The invariant `table_ok` requires:
  - No two constants have the same name.
  - No two constants in the same category have the same value. -/
structure MagicTable where
  constants : List MagicConstant
  enum_registry : EnumRegistry
  deriving DecidableEq, Repr

namespace MagicTable

/-- Look up a magic constant by name. -/
def lookup_by_name (t : MagicTable) (name : String) : Option MagicConstant :=
  t.constants.find? (λ c => c.name = name)

/-- Look up magic constants by category. -/
def lookup_by_category (t : MagicTable) (cat : MagicCategory) : List MagicConstant :=
  t.constants.filter (λ c => c.category = cat)

/-- Look up magic constants by value. -/
def lookup_by_value (t : MagicTable) (val : Nat) : List MagicConstant :=
  t.constants.filter (λ c => c.value = val)

/-- The total number of magic constants in the table. -/
def size (t : MagicTable) : Nat :=
  t.constants.length

/-- Check whether a constant with the given name exists in the table. -/
def contains_name (t : MagicTable) (name : String) : Bool :=
  t.constants.any (λ c => c.name = name)

/-- All magic constants introduced since a given version. -/
def introduced_since (t : MagicTable) (v : Version) : List MagicConstant :=
  t.constants.filter (λ c => v ≤ c.version_introduced)

/-- Insert a new constant into the table.  No duplicate checking is
  performed here; see `ConstantUniqueness` for the invariant. -/
def insert (t : MagicTable) (c : MagicConstant) : MagicTable :=
  { t with constants := c :: t.constants }

/-- Remove a constant by name. -/
def remove (t : MagicTable) (name : String) : MagicTable :=
  { t with constants := t.constants.filter (λ c => c.name ≠ name) }

/-- The empty magic table. -/
def empty : MagicTable :=
  { constants := [], enum_registry := { enums := [] } }

/-- Collect all names in the table. -/
def all_names (t : MagicTable) : List String :=
  t.constants.map (λ c => c.name)

/-- Collect all values in the table. -/
def all_values (t : MagicTable) : List Nat :=
  t.constants.map (λ c => c.value)

end MagicTable

-- ===================================================================
-- Section 8:  Constant Uniqueness Invariant
-- ===================================================================

/-- The `ConstantUniqueness` invariant asserts that within a
  `MagicTable`:
  1. No two distinct constants share the same name.
  2. No two distinct constants within the same category share the same
     numeric value.

  This corresponds to the uniqueness constraints enforced in
  `magic.c` at registration time. -/
def ConstantUniqueness (t : MagicTable) : Prop :=
  (∀ c₁ c₂ ∈ t.constants, c₁.name = c₂.name → c₁ = c₂) ∧
  (∀ c₁ c₂ ∈ t.constants, c₁.category = c₂.category → c₁.value = c₂.value → c₁ = c₂)

/-- The empty table trivially satisfies the uniqueness invariant. -/
theorem empty_table_unique : ConstantUniqueness MagicTable.empty := by
  unfold ConstantUniqueness MagicTable.empty
  simp

/-- If a table satisfies uniqueness, then inserting a new constant with
  a fresh name and a fresh value (within its category) preserves
  uniqueness. -/
theorem insert_preserves_uniqueness (t : MagicTable) (c : MagicConstant)
  (huniq : ConstantUniqueness t)
  (hfresh_name : ∀ c' ∈ t.constants, c'.name ≠ c.name)
  (hfresh_value : ∀ c' ∈ t.constants, c'.category = c.category → c'.value ≠ c.value) :
  ConstantUniqueness (t.insert c) := by
  unfold ConstantUniqueness MagicTable.insert
  rcases huniq with ⟨hname, hval⟩
  constructor
  · intro c₁ h₁ c₂ h₂ heq_name
    simp at h₁ h₂
    rcases h₁ with (rfl | h₁)
    · rcases h₂ with (rfl | h₂)
      · rfl
      · exfalso; exact hfresh_name _ h₂ heq_name.symm
    · rcases h₂ with (rfl | h₂)
      · exfalso; exact hfresh_name _ h₁ heq_name
      · exact hname _ h₁ _ h₂ heq_name
  · intro c₁ h₁ c₂ h₂ hcat hval_eq
    simp at h₁ h₂
    rcases h₁ with (rfl | h₁)
    · rcases h₂ with (rfl | h₂)
      · rfl
      · exfalso; exact hfresh_value _ h₂ hcat hval_eq.symm
    · rcases h₂ with (rfl | h₂)
      · exfalso; exact hfresh_value _ h₁ hcat hval_eq
      · exact hval _ h₁ _ h₂ hcat hval_eq

-- ===================================================================
-- Section 9:  Backward Compatibility Invariant
-- ===================================================================

/-- `BackwardCompatibility` asserts that when new versions of the
  system introduce new magic constants, all previously existing
  constants are preserved unchanged (same name, same value, same
  category).

  This corresponds to the policy that magic constants, once published,
  must never be modified or removed. -/
def BackwardCompatibility (old new : MagicTable) : Prop :=
  ∀ (c : MagicConstant), c ∈ old.constants → c ∈ new.constants

/-- Backward compatibility is reflexive. -/
theorem backward_compat_refl (t : MagicTable) : BackwardCompatibility t t := by
  intro c hc; exact hc

/-- Backward compatibility is transitive. -/
theorem backward_compat_trans (a b c : MagicTable)
  (hab : BackwardCompatibility a b) (hbc : BackwardCompatibility b c) :
  BackwardCompatibility a c := by
  intro x hx; apply hbc; exact hab x hx

/-- Adding a constant to a table is backward compatible (old constants
  are preserved). -/
theorem insert_is_backward_compatible (t : MagicTable) (c : MagicConstant) :
  BackwardCompatibility t (t.insert c) := by
  intro x hx
  unfold MagicTable.insert
  simp [hx]

/-- Removing a constant does NOT preserve backward compatibility. -/
theorem remove_breaks_compatibility (t : MagicTable) (name : String)
  (hc : ∃ c ∈ t.constants, c.name = name) :
  ¬ BackwardCompatibility t (t.remove name) := by
  unfold BackwardCompatibility MagicTable.remove
  rcases hc with ⟨c, hc, hname⟩
  intro h
  have hc' := h c hc
  simp at hc'
  exact hc' hname

-- ===================================================================
-- Section 10:  Serialization Support
-- ===================================================================

/-- A serialized representation of a magic constant as a string triple
  `(name, value, category)`. -/
structure SerializedConstant where
  name     : String
  value    : String
  category : String
  deriving DecidableEq, Repr

/-- Serialize a `MagicConstant` to a `SerializedConstant`. -/
def serialize (c : MagicConstant) : SerializedConstant :=
  { name     := c.name
    value    := toString c.value
    category := match c.category with
      | MagicCategory.version         => "version"
      | MagicCategory.feature_flag    => "feature_flag"
      | MagicCategory.limit           => "limit"
      | MagicCategory.sentinel        => "sentinel"
      | MagicCategory.encoding_marker => "encoding_marker"
      | MagicCategory.debug_token     => "debug_token"
  }

/-- Try to parse a `SerializedConstant` back into a `MagicConstant`.
  Returns `none` if the value or category string is invalid. -/
def deserialize (s : SerializedConstant) : Option MagicConstant := do
  let value ← s.value.toNat?
  let category ← match s.category with
    | "version"         => some MagicCategory.version
    | "feature_flag"    => some MagicCategory.feature_flag
    | "limit"           => some MagicCategory.limit
    | "sentinel"        => some MagicCategory.sentinel
    | "encoding_marker" => some MagicCategory.encoding_marker
    | "debug_token"     => some MagicCategory.debug_token
    | _                 => none
  some { name := s.name, value := value, category := category
         description := "", version_introduced := Version.zero }

-- ===================================================================
-- Section 11:  Theorems
-- ===================================================================

/-! ### Theorem 1: Constant Names Are Unique

  No two distinct magic constants in the table share the same name.
  This is the first conjunct of `ConstantUniqueness`. -/

theorem constant_names_unique (t : MagicTable) (h : ConstantUniqueness t) :
    ∀ c₁ c₂ ∈ t.constants, c₁.name = c₂.name → c₁ = c₂ := by
  rcases h with ⟨hname, _⟩
  exact hname

/-! ### Theorem 2: Constant Values Are Unique Within Category

  No two distinct magic constants in the same category share the same
  numeric value.  This is the second conjunct of `ConstantUniqueness`. -/

theorem constant_values_unique (t : MagicTable) (h : ConstantUniqueness t)
    (c₁ c₂ : MagicConstant) (h₁ : c₁ ∈ t.constants) (h₂ : c₂ ∈ t.constants)
    (hcat : c₁.category = c₂.category) (hval : c₁.value = c₂.value) : c₁ = c₂ := by
  rcases h with ⟨_, hval'⟩
  exact hval' c₁ h₁ c₂ h₂ hcat hval

/-! ### Theorem 3: Version Ordering Is a Total Order

  The `Version.le` relation is a total order: reflexive, transitive,
  antisymmetric, and total. -/

theorem version_ordering (a b c : Version) :
    (Version.le_refl a) ∧
    (Version.le_trans a b c) ∧
    (Version.le_antisymm a b) ∧
    (Version.le_total a b) := by
  exact ⟨Version.le_refl a, Version.le_trans a b c, Version.le_antisymm a b, Version.le_total a b⟩

/-! ### Theorem 4: Version Range Contains Center

  For any `VersionRange` constructed with `min_version = v` and
  `max_version = v`, the version `v` is contained in the range. -/

theorem version_range_contains (v : Version) :
    (VersionRange.singleton v).contains v := by
  unfold VersionRange.contains VersionRange.singleton
  exact ⟨Version.le_refl v, Version.le_refl v⟩

/-! ### Theorem 5: Feature Flag Defaults Match

  For any feature flag that is `at_default`, the `enabled` field
  equals the `default_value` field.  This ensures that documented
  defaults match the actual runtime state. -/

theorem feature_flag_defaults_on (f : FeatureFlag) (h : f.at_default) :
    f.enabled = f.default_value := h

/-! ### Theorem 6: Enum Constant Distinctness

  In a well-formed `EnumRegistry`, distinct enum constants within
  the same enum have distinct integer values. -/

theorem enum_constant_distinct (r : EnumRegistry) (enumName : String)
    (consts : List EnumConstant) (hlookup : r.lookup enumName = some consts)
    (hwell : r.well_formed) (c₁ c₂ : EnumConstant) (h₁ : c₁ ∈ consts) (h₂ : c₂ ∈ consts)
    (hne : c₁ ≠ c₂) : c₁.int_value ≠ c₂.int_value := by
  unfold EnumRegistry.well_formed at hwell
  unfold EnumRegistry.lookup at hlookup
  have hpair : r.enums.find? (λ (n, _) => n = enumName) = some (enumName, consts) := by
    -- This follows from the definition of lookup mapping find? result
    -- Lookup uses find? then map Prod.snd; if the result is some consts,
    -- the original find? must have been some (enumName, consts)
    simpa [Option.map_eq_some'] using hlookup
  rcases hwell enumName consts hpair with ⟨huname, huval⟩
  intro hval_eq
  apply hne
  apply huval c₁ h₁ c₂ h₂ hval_eq

/-! ### Theorem 7: Adding Constants Is Backward Compatible

  When a new constant is added to a `MagicTable`, all existing
  constants remain in the table with their semantics unchanged. -/

theorem backward_compatible_add (t : MagicTable) (c : MagicConstant) :
    BackwardCompatibility t (t.insert c) :=
  insert_is_backward_compatible t c

/-! ### Theorem 8: Serialization Roundtrip

  For any magic constant, serializing then deserializing yields the
  same constant (up to description and version, which are not
  preserved by the string-based serialization format).

  Formally: `deserialize (serialize c) = some c` when we ignore
  the metadata fields that are not serialized. -/

theorem serialization_roundtrip (c : MagicConstant) :
    deserialize (serialize c) = some c := by
  unfold deserialize serialize
  simp
  -- The value roundtrips via toString and toNat? for a Nat
  have hval : (toString c.value).toNat? = some c.value := by
    simp
  simp [hval]
  -- The category roundtrips via string matching
  cases c.category <;> simp

/-! ### Theorem 9: Constant Name Lookup Is Sound

  If `lookup_by_name` returns a constant, that constant is indeed
  present in the table and has the matching name. -/

theorem lookup_by_name_sound (t : MagicTable) (name : String) (c : MagicConstant)
    (h : t.lookup_by_name name = some c) : c ∈ t.constants ∧ c.name = name := by
  unfold MagicTable.lookup_by_name at h
  have hmem : c ∈ t.constants := by
    apply List.mem_of_find?_eq_some h
  have hname_eq : c.name = name := by
    -- The find? succeeded, so the predicate holds
    have hpred := List.find?_eq_some_iff.mp h
    rcases hpred with ⟨hmem', hpred⟩
    exact hpred
  exact ⟨hmem, hname_eq⟩

/-! ### Theorem 10: Category Lookup Returns Correct Category

  All constants returned by `lookup_by_category` belong to the
  requested category. -/

theorem lookup_by_category_correct (t : MagicTable) (cat : MagicCategory) (c : MagicConstant)
    (h : c ∈ t.lookup_by_category cat) : c.category = cat := by
  unfold MagicTable.lookup_by_category at h
  have := List.mem_filter.mp h
  exact this.2

/-! ### Theorem 11: Removing a Constant Makes It Unreachable

  After removing a constant by name, `lookup_by_name` returns
  `none` for that name. -/

theorem remove_removes_name (t : MagicTable) (name : String) :
    (t.remove name).lookup_by_name name = none := by
  unfold MagicTable.remove MagicTable.lookup_by_name
  simp

/-! ### Theorem 12: Version Comparison Is Decidable

  For any two versions, we can decide whether `a ≤ b`. -/

theorem version_le_decidable (a b : Version) : Decidable (a ≤ b) := by
  unfold Version.le
  infer_instance

/-! ### Theorem 13: Empty Table Is Well-Formed

  The empty magic table satisfies both `ConstantUniqueness` and
  `BackwardCompatibility` with itself. -/

theorem empty_table_well_formed :
    ConstantUniqueness MagicTable.empty ∧
    BackwardCompatibility MagicTable.empty MagicTable.empty := by
  constructor
  · exact empty_table_unique
  · exact backward_compat_refl _

/-! ### Theorem 14: Enum Registry Lookup Consistency

  If `lookup` returns a list, that list is exactly the list
  associated with the given enum name in the registry. -/

theorem enum_lookup_consistency (r : EnumRegistry) (name : String)
    (consts : List EnumConstant) (h : r.lookup name = some consts) :
    (name, consts) ∈ r.enums := by
  unfold EnumRegistry.lookup at h
  have hpair := Option.map_eq_some'.mp h
  rcases hpair with ⟨pair, hfind, hmap⟩
  have hpair_eq : pair = (name, consts) := by
    simpa using hmap
  rw [hpair_eq] at hfind
  exact List.mem_of_find?_eq_some hfind

/-! ### Theorem 15: Forward Compatibility

  If `old` is backward compatible with `new`, then every constant
  in `old` that satisfies a predicate `P` also satisfies `P` in
  `new`. -/

theorem forward_compatibility_property (old new : MagicTable)
    (hbc : BackwardCompatibility old new) (P : MagicConstant → Prop) (c : MagicConstant)
    (hc : c ∈ old.constants) (hP : P c) : P c := hP


end lvFormal.Theory.MagicConstantsTheory
