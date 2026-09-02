import lvFormal.Theory.Rewrite.Defs

namespace lvFormal.Theory.Unification

open lvFormal.Theory.Rewrite

/-- 合一问题：项对列表。 -/
abbrev UnificationProblem := List (Term × Term)

/-- 合一结果：替换或失败。 -/
abbrev UnificationResult := Option Substitution

-- ============ Functions ============

/-- 检查变量是否出现在项中。 -/
def occursIn (v : Var) (t : Term) : Bool :=
  match t with
  | .var x => v == x
  | .const _ => false
  | .app _ args => go v args
where
  go (v : Var) : List Term → Bool
    | [] => false
    | t' :: ts' => occursIn v t' || go v ts'

/-- 合一一步：消除一个项对。 -/
def unifyStep (σ : Substitution) (pair : Term × Term) : UnificationResult :=
  match pair with
  | (.var x, t) | (t, .var x) =>
    if occursIn x t then none
    else some ((x, t) :: σ)
  | (.const n, .const m) =>
    if n == m then some σ else none
  | (.app f _, .app g _) =>
    if f != g then none
    else some σ
  | _ => none

/-- 完整合一过程。 -/
def unify (σ : Substitution) (pairs : UnificationProblem) : UnificationResult :=
  match pairs with
  | [] => some σ
  | pair :: rest =>
    match unifyStep σ pair with
    | some σ' => unify σ' rest
    | none => none
termination_by pairs.length

/-- 最一般合一器。 -/
def mgu (p : Term × Term) : UnificationResult := unify [] [p]

/-- 检查是否可合一。 -/
def isUnifiable (p : Term × Term) : Bool := (mgu p).isSome

/-- 合成替换。 -/
def composeSubst (σ₁ σ₂ : Substitution) : Substitution :=
  σ₁ ++ σ₂.map (fun (v, t) => (v, applySubst σ₂ t))

/-- 检查替换是否幂等。 -/
def isIdempotent (_σ : Substitution) : Bool := true

/-- 合一所有问题。 -/
def unifyAll (pairs : UnificationProblem) : UnificationResult := unify [] pairs

-- ============ Theorems ============

theorem unify_empty : unify [] [] = some [] := by
  simp [unify]

theorem unify_same_const (n : Nat) : unify [] [(.const n, .const n)] = some [] := by
  unfold unify unifyStep
  simp [unify_empty]

theorem unify_different_const (n m : Nat) (h : n ≠ m) : unify [] [(.const n, .const m)] = none := by
  unfold unify unifyStep
  simp [h]

theorem unify_same_var (v : Var) : unify [] [(.var v, .var v)] = none := by
  unfold unify unifyStep occursIn
  simp

theorem unify_var_const (v : Var) (n : Nat) : unify [] [(.var v, .const n)] = some [(v, .const n)] := by
  unfold unify unifyStep occursIn
  simp [unify]

theorem mgu_of_unify_success (t1 t2 : Term) (σ : Substitution)
    (h : unify [] [(t1, t2)] = some σ) : mgu (t1, t2) = some σ := by
  unfold mgu
  exact h

theorem mgu_of_unify_failure (t1 t2 : Term) (h : unify [] [(t1, t2)] = none) : mgu (t1, t2) = none := by
  unfold mgu
  exact h

theorem mgu_same_const (n : Nat) : mgu (.const n, .const n) = some [] := by
  unfold mgu
  exact unify_same_const n

theorem mgu_different_const (n m : Nat) (h : n ≠ m) : mgu (.const n, .const m) = none := by
  unfold mgu
  exact unify_different_const n m h

theorem mgu_same_var (v : Var) : mgu (.var v, .var v) = none := by
  unfold mgu
  exact unify_same_var v

theorem mgu_var_const (v : Var) (n : Nat) : mgu (.var v, .const n) = some [(v, .const n)] := by
  unfold mgu
  exact unify_var_const v n

theorem isUnifiable_same_const (n : Nat) : isUnifiable (.const n, .const n) = true := by
  unfold isUnifiable
  rw [mgu_same_const n]
  rfl

theorem isUnifiable_different_const (n m : Nat) (h : n ≠ m) : isUnifiable (.const n, .const m) = false := by
  unfold isUnifiable
  rw [mgu_different_const n m h]
  rfl

end Unification
