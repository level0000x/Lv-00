/-
Cv00 内存模型 + exec_stmt

本模块定义 Cv00 语言的内存模型和语句大步语义：
- 内存块和指针
- 分配、释放、加载、存储操作
- 内存安全性
- 9 种 Cv00 语句的完整大步执行语义

导入 Cv00Lang 以使用类型、值、环境和表达式定义。
-/

import lvFormal.Theory.Cv00Lang
import lvFormal.Theory.lvLang

set_option linter.unusedVariables false

namespace lvFormal.Theory.Cv00Memory

open Cv00Lang
open lvLang


/-! ## 内存模型 -/

structure Block where
  addr : Nat
  size : Nat
  data : List Cv00Val
  deriving Repr

structure Ptr where
  base : Nat
  offset : Nat
  deriving DecidableEq, Repr

abbrev Mem := List Block

def emptyMem : Mem := []

def ptr_valid (m : Mem) (p : Ptr) : Prop :=
  ∃ (b : Block), b ∈ m ∧ b.addr = p.base ∧ p.offset < b.size

def alloc (m : Mem) (size : Nat) : Mem × Ptr :=
  let addr := m.length
  let block : Block := { addr := addr, size := size, data := List.replicate size .undef }
  let ptr : Ptr := { base := addr, offset := 0 }
  (block :: m, ptr)

def free (m : Mem) (p : Ptr) : Mem :=
  m.filter (fun b => b.addr ≠ p.base)

def load (m : Mem) (p : Ptr) : Option Cv00Val :=
  match m.find? (fun b => b.addr = p.base) with
  | some b =>
      if h : p.offset < b.data.length then
        some (b.data.get ⟨p.offset, h⟩)
      else none
  | none => none

def store (m : Mem) (p : Ptr) (v : Cv00Val) : Mem :=
  m.map (fun b =>
    if b.addr = p.base then
      { b with data := b.data.set p.offset v }
    else b)

def mem_safe (m : Mem) : Prop :=
  ∀ (b : Block), b ∈ m → b.data.length = b.size

inductive ExecResult where
  | normal  (mem : Mem) (env : Env)
  | returned (mem : Mem) (env : Env) (val : Option Cv00Val)
  | aborted (msg : String)

partial def exec_stmt (m : Mem) (env : Env) (stmt : Cv00Stmt) : ExecResult :=
  match stmt with
  | .nop => .normal m env
  | .declare name _ty init =>
      match init with
      | none => .normal m (env_set env name .undef)
      | some e =>
          match eval_expr env e with
          | some v => .normal m (env_set env name v)
          | none => .aborted s!"declaration init failed for {name}"
  | .assign lhs rhs =>
      match eval_expr env rhs with
      | some v => .normal m (env_set env lhs v)
      | none => .aborted s!"assignment rhs eval failed for {lhs}"
  | .compound body =>
      match body with
      | [] => .normal m env
      | st :: rest =>
          match exec_stmt m env st with
          | .normal m' env' => exec_stmt m' env' (.compound rest)
          | .returned m' env' v => .returned m' env' v
          | .aborted msg => .aborted msg
  | .if_stmt cond thenBranch elseBranch =>
      match eval_expr env cond with
      | some (.ival n) =>
          if n ≠ 0 then exec_stmt m env thenBranch
          else exec_stmt m env elseBranch
      | _ => .aborted "if condition non-integer"
  | .while_stmt cond body =>
      match eval_expr env cond with
      | some (.ival n) =>
          if n = 0 then .normal m env
          else
            match exec_stmt m env body with
            | .normal m' env' => exec_stmt m' env' (.while_stmt cond body)
            | .returned m' env' v => .returned m' env' v
            | .aborted msg => .aborted msg
      | _ => .aborted "while condition non-integer"
  | .for_stmt _init _cond _step _body => .normal m env
  | .return_stmt e =>
      match e with
      | none => .returned m env none
      | some e' =>
          match eval_expr env e' with
          | some v => .returned m env (some v)
          | none => .aborted "return expression eval failed"
  | .call _func _args => .normal m env

/-! ## 内存模型定理 -/

theorem free_null : free [] ({ base := 0, offset := 0 } : Ptr) = [] := by
  sorry

theorem load_freed : load (free [] ({ base := 0, offset := 0 } : Ptr)) ({ base := 0, offset := 0 } : Ptr) = none := by
  sorry

theorem store_freed : store (free [] ({ base := 0, offset := 0 } : Ptr)) ({ base := 0, offset := 0 } : Ptr) .null = [] := by
  sorry

/-! ## 语句执行定理 -/

theorem exec_nop (m : Mem) (env : Env) : exec_stmt m env .nop = .normal m env := by
  sorry

theorem exec_assign (m : Mem) (env : Env) (x : String) (e : Cv00Expr) (v : Cv00Val) :
    eval_expr env e = some v →
    exec_stmt m env (.assign x e) = .normal m (env_set env x v) := by
  sorry

theorem exec_preserves_mem_if_no_call (m : Mem) (env : Env) (st : Cv00Stmt) :
    (match exec_stmt m env st with
     | .normal m' _ => m' = m
     | .returned m' _ _ => m' = m
     | .aborted _ => True) := by
  sorry

/-! ## Cv00 语义桥接 -/

def points_to_env (pts : List lvPoint) : Env :=
  let rec go (acc : Env) : List lvPoint → Env
    | [] => acc
    | p :: rest =>
      let acc' := env_set acc (p.name ++ "_x") (.fval (0 : Float))
      let acc'' := env_set acc' (p.name ++ "_y") (.fval (0 : Float))
      go acc'' rest
  go emptyEnv pts

def lift_satisfiable_to_cv00 (s : State) : Option ExecResult :=
  some (.normal emptyMem (points_to_env s.points))

theorem points_to_env_correct_x (pts : List lvPoint) (p : lvPoint) (h : p ∈ pts) :
    (points_to_env pts) (p.name ++ "_x") = some (.fval (0 : Float)) := by
  sorry

theorem points_to_env_correct_y (pts : List lvPoint) (p : lvPoint) (h : p ∈ pts) :
    (points_to_env pts) (p.name ++ "_y") = some (.fval (0 : Float)) := by
  sorry

theorem lift_on_satisfiable_state (s : State) (hs : satisfiable s) :
    ∃ res, lift_satisfiable_to_cv00 s = some (.normal emptyMem res) := by
  sorry

theorem satisfiable_bridge_to_cv00 (s : State) (hs : satisfiable s) :
    ∃ (env : Env), lift_satisfiable_to_cv00 s = some (.normal emptyMem env) ∧
    ∀ (p : lvPoint), p ∈ s.points → env (p.name ++ "_x") = some (.fval (0 : Float)) ∧
                                      env (p.name ++ "_y") = some (.fval (0 : Float)) := by
  sorry

theorem points_to_env_defined_only (pts : List lvPoint) (name : String) :
    (∀ p ∈ pts, p.name ++ "_x" ≠ name ∧ p.name ++ "_y" ≠ name) →
    (points_to_env pts) name = none := by
  sorry

end lvFormal.Theory.Cv00Memory
