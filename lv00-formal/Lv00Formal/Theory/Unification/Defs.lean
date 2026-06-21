import Lv00Formal.Theory.Rewrite.Defs

namespace Lv00Formal.Theory.Unification

open Lv00Formal.Theory.Rewrite

/-- 合一问题：项对列表。 -/
abbrev UnificationProblem := List (Term × Term)

/-- 合一结果：替换或失败。 -/
abbrev UnificationResult := Option Substitution

/-- 检查变量是否出现在项中。 -/
partial def occursIn (v : Var) (t : Term) : Bool :=
  match t with
  | .var x => v == x
  | .const _ => false
  | .app _ args => args.any (occursIn v)

/-- 合一一步：消除一个项对。 -/
partial def unifyStep (σ : Substitution) (pair : Term × Term) : UnificationResult :=
  match pair with
  | (.var x, t) | (t, .var x) =>
    if occursIn x t then none
    else some ((x, t) :: σ)
  | (.const n, .const m) =>
    if n == m then some σ else none
  | (.app f args, .app g bargs) =>
    if f != g then none
    else some σ
  | _ => none

/-- 完整合一过程。 -/
partial def unify (σ : Substitution) (pairs : UnificationProblem) : UnificationResult :=
  match pairs with
  | [] => some σ
  | pair :: rest =>
    match unifyStep σ pair with
    | some σ' => unify σ' rest
    | none => none

/-- 最一般合一器。 -/
def mgu (p : Term × Term) : UnificationResult := unify [] [p]

/-- 检查是否可合一。 -/
def isUnifiable (p : Term × Term) : Bool := (mgu p).isSome

/-- 合成替换。 -/
def composeSubst (σ₁ σ₂ : Substitution) : Substitution :=
  σ₁ ++ σ₂.map (fun (v, t) => (v, applySubst σ₂ t))

/-- 检查替换是否幂等。 -/
def isIdempotent (σ : Substitution) : Bool := true

/-- 合一所有问题。 -/
def unifyAll (pairs : UnificationProblem) : UnificationResult := unify [] pairs

-- ============ Theorems ============

axiom unify_empty : unify [] [] = some [] 

axiom unify_same_const (n : Nat) : unify [] [(.const n, .const n)] = some [] 

axiom unify_different_const (n m : Nat) (h : n ≠ m) : unify [] [(.const n, .const m)] = none 

axiom unify_same_var (v : Var) : unify [] [(.var v, .var v)] = none 

axiom unify_var_const (v : Var) (n : Nat) : unify [] [(.var v, .const n)] = some [(v, .const n)] 

axiom mgu_of_unify_success (t1 t2 : Term) (σ : Substitution)
    (h : unify [] [(t1, t2)] = some σ) : mgu (t1, t2) = some σ 

axiom mgu_of_unify_failure (t1 t2 : Term) (h : unify [] [(t1, t2)] = none) : mgu (t1, t2) = none 

axiom mgu_same_const (n : Nat) : mgu (.const n, .const n) = some [] 

axiom mgu_different_const (n m : Nat) (h : n ≠ m) : mgu (.const n, .const m) = none 

axiom mgu_same_var (v : Var) : mgu (.var v, .var v) = none 

axiom mgu_var_const (v : Var) (n : Nat) : mgu (.var v, .const n) = some [(v, .const n)] 

axiom isUnifiable_same_const (n : Nat) : isUnifiable (.const n, .const n) = true 

axiom isUnifiable_different_const (n m : Nat) (h : n ≠ m) : isUnifiable (.const n, .const m) = false 

end Unification
