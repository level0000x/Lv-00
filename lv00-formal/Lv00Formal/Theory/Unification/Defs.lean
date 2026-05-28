/-
Lv-00 自有理论核心：合一算法

该文件实现一个可执行的 Martelli-Montanari 风格合一核心，
并优先证明与当前实现完全一致的基础元理论性质。
-/

import Lv00Formal.Theory.Rewrite.Defs

namespace Lv00Formal
namespace Theory
namespace Unification

open Rewrite

/-- 合一问题：一组需要合一的方程。 -/
abbrev UnificationProblem := List (Term × Term)

/-- 合一结果。 -/
inductive UnificationResult where
  | success (σ : Substitution)
  | failure (reason : String)
  deriving Repr, DecidableEq

/-- 变量在项中是否出现（occurs check）。 -/
def occursIn (v : Var) : Term → Bool
  | .var x => x = v
  | .const _ => false
  | .app _ args => args.any (occursIn v)

/-- 单步合一转换。 -/
def unifyStep (problem : UnificationProblem) : Option (UnificationProblem × Option (Var × Term)) :=
  match problem with
  | [] => none
  | (s, t) :: rest =>
    match s, t with
    | .const n, .const m =>
      if n = m then some (rest, none) else none
    | .app f args1, .app g args2 =>
      if f = g && args1.length = args2.length then
        some (args1.zip args2 ++ rest, none)
      else
        none
    | .const n, .var v =>
      some ((.var v, .const n) :: rest, none)
    | .var v, t =>
      if occursIn v t && s ≠ t then
        none
      else if s = t then
        some (rest, none)
      else
        let subst : Substitution := [(v, t)]
        let newRest := rest.map (fun (l, r) => (applySubst subst l, applySubst subst r))
        some (newRest, some (v, t))
    | .app _ _, .const _ => none
    | .const _, .app _ _ => none

/-- 完整合一算法。

使用 `partial` 是工程层面的可执行递归选择；后续可用燃料参数版本
替代它来证明终止性。 -/
partial def unify (problem : UnificationProblem) : UnificationResult :=
  go problem emptySubst
where
  go (prob : UnificationProblem) (acc : Substitution) : UnificationResult :=
    match unifyStep prob with
    | none =>
      if prob.isEmpty then .success acc else .failure "unification failed"
    | some (newProb, newSubst) =>
      let newAcc :=
        match newSubst with
        | some (v, t) => (v, t) :: acc
        | none => acc
      go newProb newAcc

/-- 最一般合一式：合一成功时返回替换。 -/
def mgu (t1 t2 : Term) : Option Substitution :=
  match unify [(t1, t2)] with
  | .success σ => some σ
  | .failure _ => none

/-- 合一成功判定。 -/
def isUnifiable (t1 t2 : Term) : Bool :=
  (mgu t1 t2).isSome

/-- 合一替换组合：先应用 `σ2`，再应用 `σ1`。 -/
def composeSubst (σ1 σ2 : Substitution) : Substitution :=
  let σ2' := σ2.map (fun (v, t) => (v, applySubst σ1 t))
  σ1 ++ σ2'

/-- 合一替换的幂等性判定。 -/
def isIdempotent (σ : Substitution) : Bool :=
  σ.all (fun (_, t) => applySubst σ t = t)

/-- 多组项同时合一。 -/
def unifyAll (pairs : List (Term × Term)) : UnificationResult :=
  unify pairs

/-! ## 当前实现可直接证明的基础性质 -/

/-- 空合一问题成功，并产生空替换。 -/
theorem unify_empty :
    unify [] = .success [] := by
  rfl

/-- 相同常量可以合一。 -/
theorem unify_same_const (n : Nat) :
    unify [(.const n, .const n)] = .success [] := by
  simp [unify, unifyStep, emptySubst]

/-- 不同常量不能合一。 -/
theorem unify_different_const (n m : Nat) (h : n ≠ m) :
    unify [(.const n, .const m)] = .failure "unification failed" := by
  simp [unify, unifyStep, emptySubst, h]

/-- 变量与自身合一产生空替换。 -/
theorem unify_same_var (v : Var) :
    unify [(.var v, .var v)] = .success [] := by
  simp [unify, unifyStep, occursIn, emptySubst]

/-- 变量与常量合一产生单点替换。 -/
theorem unify_var_const (v : Var) (n : Nat) :
    unify [(.var v, .const n)] = .success [(v, .const n)] := by
  simp [unify, unifyStep, occursIn, emptySubst]

/-- `mgu` 是 `unify` 成功分支的直接投影。 -/
theorem mgu_of_unify_success (t1 t2 : Term) (σ : Substitution)
    (h : unify [(t1, t2)] = .success σ) :
    mgu t1 t2 = some σ := by
  simp [mgu, h]

/-- `mgu` 在合一失败时返回 `none`。 -/
theorem mgu_of_unify_failure (t1 t2 : Term) (reason : String)
    (h : unify [(t1, t2)] = .failure reason) :
    mgu t1 t2 = none := by
  simp [mgu, h]

/-- 相同常量的 MGU 是空替换。 -/
theorem mgu_same_const (n : Nat) :
    mgu (.const n) (.const n) = some [] := by
  simp [mgu, unify_same_const]

/-- 不同常量没有 MGU。 -/
theorem mgu_different_const (n m : Nat) (h : n ≠ m) :
    mgu (.const n) (.const m) = none := by
  simp [mgu, unify_different_const n m h]

/-- 变量与自身的 MGU 是空替换。 -/
theorem mgu_same_var (v : Var) :
    mgu (.var v) (.var v) = some [] := by
  simp [mgu, unify_same_var]

/-- 变量与常量的 MGU 是单点替换。 -/
theorem mgu_var_const (v : Var) (n : Nat) :
    mgu (.var v) (.const n) = some [(v, .const n)] := by
  simp [mgu, unify_var_const]

/-- 相同常量可合一。 -/
theorem isUnifiable_same_const (n : Nat) :
    isUnifiable (.const n) (.const n) = true := by
  simp [isUnifiable, mgu_same_const]

/-- 不同常量不可合一。 -/
theorem isUnifiable_different_const (n m : Nat) (h : n ≠ m) :
    isUnifiable (.const n) (.const m) = false := by
  simp [isUnifiable, mgu_different_const n m h]

end Unification
end Theory
end Lv00Formal
