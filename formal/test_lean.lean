import Mathlib
noncomputable section
namespace lvFormal.Theory.LvDSL

inductive LvType where
  | point | constraint | real | int | bool | string | name | nat
  | arrow (dom codom : LvType) | list (elem : LvType) | set (elem : LvType)
  | option (elem : LvType) | pair (first second : LvType)
  deriving DecidableEq

inductive LvExpr where
  | var (name : String) | intLit (val : ℤ) | floatLit (val : ℝ) | strLit (val : String)
  | boolLit (val : Bool) | app (fn : LvExpr) (arg : LvExpr)
  | add (e1 e2 : LvExpr) | sub (e1 e2 : LvExpr) | mul (e1 e2 : LvExpr) | div (e1 e2 : LvExpr)
  | lambda (param : String) (paramType : LvType) (body : LvExpr)
  | forall_ (x : String) (ty : LvType) (body : LvExpr)
  | exists (x : String) (ty : LvType) (body : LvExpr)
  | listLit (elems : List LvExpr) | setLit (elems : List LvExpr)
  | some (e : LvExpr) | none (ty : LvType) | pair (e1 e2 : LvExpr)

def lv_type_infer : LvExpr → Option LvType
  | .var _ => none
  | .intLit _ => some .int
  | .floatLit _ => some .real
  | .strLit _ => some .string
  | .boolLit _ => some .bool
  | .add e1 e2 => match lv_type_infer e1, lv_type_infer e2 with | some .int, some .int => some .int | some .real, some .real => some .real | _, _ => none
  | .sub e1 e2 => match lv_type_infer e1, lv_type_infer e2 with | some .int, some .int => some .int | some .real, some .real => some .real | _, _ => none
  | .mul e1 e2 => match lv_type_infer e1, lv_type_infer e2 with | some .int, some .int => some .int | some .real, some .real => some .real | _, _ => none
  | .div e1 e2 => match lv_type_infer e1, lv_type_infer e2 with | some .int, some .int => some .int | some .real, some .real => some .real | _, _ => none
  | .lambda _p t b => some (LvType.arrow t ((lv_type_infer b).getD LvType.real))
  | .forall_ _ _ _ => some .bool
  | .exists _ _ _ => some .bool
  | .listLit es => match es with | [] => none | e :: _ => (lv_type_infer e).map .list
  | .setLit es => match es with | [] => none | e :: _ => (lv_type_infer e).map .set
  | .some e => (lv_type_infer e).map .option
  | .none t => some (.option t)
  | .pair e1 e2 => match lv_type_infer e1, lv_type_infer e2 with | some t1, some t2 => some (.pair t1 t2) | _, _ => none
  | .app f _a => match lv_type_infer f with | some (.arrow _ codom) => some codom | _ => none
termination_by e => e

def lv_type_check : LvExpr → LvType → Bool
  | .var _, _ => true
  | .intLit _, .int => true
  | .intLit _, .real => true
  | .floatLit _, .real => true
  | .strLit _, .string => true
  | .boolLit _, .bool => true
  | .add e1 e2, .real => lv_type_check e1 .real ∧ lv_type_check e2 .real
  | .add e1 e2, .int  => lv_type_check e1 .int  ∧ lv_type_check e2 .int
  | .sub e1 e2, .real => lv_type_check e1 .real ∧ lv_type_check e2 .real
  | .sub e1 e2, .int  => lv_type_check e1 .int  ∧ lv_type_check e2 .int
  | .mul e1 e2, .real => lv_type_check e1 .real ∧ lv_type_check e2 .real
  | .mul e1 e2, .int  => lv_type_check e1 .int  ∧ lv_type_check e2 .int
  | .div e1 e2, .real => lv_type_check e1 .real ∧ lv_type_check e2 .real
  | .div e1 e2, .int  => lv_type_check e1 .int  ∧ lv_type_check e2 .int
  | .lambda _p t b, .arrow dom codom => t = dom ∧ lv_type_check b codom
  | .lambda _ _ _, _ => false
  | .forall_ _ _ _, .bool => true
  | .exists _ _ _, .bool => true
  | .listLit es, .list t =>
    match es with | [] => true | e :: es' => lv_type_check e t ∧ lv_type_check (.listLit es') (.list t)
  | .setLit es, .set t =>
    match es with | [] => true | e :: es' => lv_type_check e t ∧ lv_type_check (.setLit es') (.set t)
  | .some e, .option t => lv_type_check e t
  | .none t', .option t => t' = t
  | .pair e1 e2, .pair t1 t2 => lv_type_check e1 t1 ∧ lv_type_check e2 t2
  | .app f a, t => match lv_type_infer f with | some (.arrow dom codom) => lv_type_check a dom ∧ codom = t | _ => false
  | _, _ => false
termination_by e _ => e

-- Test: simpa [lv_type_check] for app case
example (f a : LvExpr) (t : LvType) (h_type : lv_type_check (.app f a) t) :
    (match lv_type_infer f with
    | some (.arrow dom codom) => lv_type_check a dom ∧ codom = t
    | _ => false) = true := by
  simpa [lv_type_check] using h_type

-- Test: using simp [lv_type_check] at h_type
example (f a : LvExpr) (t : LvType) (h_type : lv_type_check (.app f a) t) : True := by
  simp [lv_type_check] at h_type
  -- h_type should be: (match lv_type_infer f with ...) = true
  trivial

-- Test: h_false pattern for add_intLit with t not int/real
example (v1 v2 : ℤ) (t : LvType) (h_int : t ≠ .int) (h_real : t ≠ .real) :
    lv_type_check (.add (.intLit v1) (.intLit v2)) t = false := by
  unfold lv_type_check
  cases t <;> try rfl
  · exfalso; exact h_int rfl
  · exfalso; exact h_real rfl

-- Test: h_false for div with variable e1, e2
example (e1 e2 : LvExpr) (t : LvType) (h_int : t ≠ .int) (h_real : t ≠ .real) :
    lv_type_check (.div e1 e2) t = false := by
  unfold lv_type_check
  cases t <;> try rfl
  · exfalso; exact h_int rfl
  · exfalso; exact h_real rfl

-- Test: using cases on lv_type_infer for app case
example (f a : LvExpr) (t : LvType) (h_type : lv_type_check (.app f a) t) : True := by
  simp [lv_type_check] at h_type
  -- h_type: (match lv_type_infer f with ...) = true
  cases h_f : lv_type_infer f
  · simp [h_f] at h_type
  · rename_i ty
    cases ty
    · simp [h_f] at h_type
    · simp [h_f] at h_type
    · simp [h_f] at h_type
    · simp [h_f] at h_type
    · simp [h_f] at h_type
    · simp [h_f] at h_type
    · simp [h_f] at h_type
    · simp [h_f] at h_type
    · -- arrow dom codom
      rename_i dom codom
      simp [h_f] at h_type
      rcases h_type with ⟨h_a, h_eq⟩
      trivial
    · simp [h_f] at h_type
    · simp [h_f] at h_type
    · simp [h_f] at h_type
    · simp [h_f] at h_type

end lvFormal.Theory.LvDSL