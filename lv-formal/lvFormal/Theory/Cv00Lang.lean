/-
C11 子集操作语义 (Cv00Lang)

本模块定义 Lv-00 自举编译目标语言 Cv00 的语法和表达式语义：
- 7 种 C 类型
- 6 种 C 值
- 环境（变量到值的映射）
- 21 种 C 表达式及其大步求值
- 9 种 C 语句

语句的大步语义 (exec_stmt) 定义在 Cv00Memory.lean 中，
因为它依赖内存模型。
-/

import Mathlib

open Classical

namespace lvFormal.Theory.Cv00Lang

/-! ## C 类型 -/

/-- Cv00 的 7 种类型 -/
inductive Cv00Type where
  | void
  | int32
  | int64
  | float64
  | pointer (base : Cv00Type)
  | array (elem : Cv00Type) (len : Nat)
  | struct (fields : List Cv00Type)
  deriving Repr

/-- sizeof 计算（简化）-/
partial def sizeof : Cv00Type → Nat
  | .void          => 0
  | .int32         => 4
  | .int64         => 8
  | .float64       => 8
  | .pointer _     => 8
  | .array t n     => sizeof t * n
  | .struct fs     => (fs.map sizeof).sum

/-- 非退化类型归纳谓词：sizeof 严格为正 -/
inductive Nondegenerate : Cv00Type → Prop
  | int32     : Nondegenerate .int32
  | int64     : Nondegenerate .int64
  | float64   : Nondegenerate .float64
  | pointer   (t : Cv00Type) : Nondegenerate (.pointer t)
  | array     (elem : Cv00Type) (len : Nat) : Nondegenerate elem → len > 0 → Nondegenerate (.array elem len)
  | struct_cons (f : Cv00Type) (fs : List Cv00Type) : Nondegenerate f → Nondegenerate (.struct (f :: fs))

/-- 非退化类型的 sizeof 严格为正（替代原公理，杜绝零长数组 / 空结构体反例） -/
theorem sizeof_positive (t : Cv00Type) (h : Nondegenerate t) : sizeof t > 0 := by
  sorry

/-! ## C 值 -/

/-- Cv00 的 6 种值 -/
inductive Cv00Val where
  | ival (n : Int)
  | fval (x : Float)
  | ptr (addr : Nat)
  | structVal (fields : List Cv00Val)
  | null
  | undef
  deriving Repr

/-! ## 环境 -/

/-- 变量环境：变量名到值的映射 -/
def Env := String → Option Cv00Val

/-- 空环境 -/
def emptyEnv : Env := fun _ => none

/-- 环境写入 -/
def env_set (env : Env) (name : String) (val : Cv00Val) : Env :=
  fun n => if n = name then some val else env n

/-- 环境分配（无初值）-/
def env_alloc (env : Env) (name : String) : Env :=
  env_set env name .undef

/-! ## C 表达式 -/

/-- Cv00 表达式：21 种 -/
inductive Cv00Expr where
  | lit_int  (n : Int)
  | lit_float (x : Float)
  | lit_null
  | var (name : String)
  | add (e1 e2 : Cv00Expr)
  | sub (e1 e2 : Cv00Expr)
  | mul (e1 e2 : Cv00Expr)
  | div (e1 e2 : Cv00Expr)
  | mod (e1 e2 : Cv00Expr)
  | neg (e : Cv00Expr)
  | eq (e1 e2 : Cv00Expr)
  | ne (e1 e2 : Cv00Expr)
  | lt (e1 e2 : Cv00Expr)
  | le (e1 e2 : Cv00Expr)
  | gt (e1 e2 : Cv00Expr)
  | ge (e1 e2 : Cv00Expr)
  | addr_of (name : String)
  | deref (e : Cv00Expr)
  | sizeof_expr (t : Cv00Type)
  | cast (t : Cv00Type) (e : Cv00Expr)
  | field_access (e : Cv00Expr) (field : Nat)
  | call (name : String) (args : List Cv00Expr)
  | cmp_eq (e1 e2 : Cv00Expr)
  | cmp_ne (e1 e2 : Cv00Expr)
  | cmp_ge (e1 e2 : Cv00Expr)
  | cmp_le (e1 e2 : Cv00Expr)
  | or_op (e1 e2 : Cv00Expr)
  deriving Repr

/-! ## 表达式求值 -/

/-- Cv00 表达式大步求值 -/
noncomputable def eval_expr (env : Env) : Cv00Expr → Option Cv00Val
  | .lit_int n       => some (.ival n)
  | .lit_float x     => some (.fval x)
  | .lit_null        => some .null
  | .var name        => env name
  | .add e1 e2       =>
      match eval_expr env e1, eval_expr env e2 with
      | some (.ival n1), some (.ival n2) => some (.ival (n1 + n2))
      | some (.fval x1), some (.fval x2) => some (.fval (x1 + x2))
      | _, _ => none
  | .sub e1 e2       =>
      match eval_expr env e1, eval_expr env e2 with
      | some (.ival n1), some (.ival n2) => some (.ival (n1 - n2))
      | some (.fval x1), some (.fval x2) => some (.fval (x1 - x2))
      | _, _ => none
  | .mul e1 e2       =>
      match eval_expr env e1, eval_expr env e2 with
      | some (.ival n1), some (.ival n2) => some (.ival (n1 * n2))
      | some (.fval x1), some (.fval x2) => some (.fval (x1 * x2))
      | _, _ => none
  | .div e1 e2       =>
      match eval_expr env e1, eval_expr env e2 with
      | some (.ival n1), some (.ival n2) =>
          if n2 = 0 then none else some (.ival (n1 / n2))
      | some (.fval x1), some (.fval x2) =>
          if x2 = 0 then none else some (.fval (x1 / x2))
      | _, _ => none
  | .mod e1 e2       =>
      match eval_expr env e1, eval_expr env e2 with
      | some (.ival n1), some (.ival n2) =>
          if n2 = 0 then none else some (.ival (n1 % n2))
      | _, _ => none
  | .neg e           =>
      match eval_expr env e with
      | some (.ival n) => some (.ival (-n))
      | some (.fval x) => some (.fval (-x))
      | _ => none
  | .eq e1 e2        =>
      match eval_expr env e1, eval_expr env e2 with
      | some v1, some v2 => some (.ival (if v1 = v2 then 1 else 0))
      | _, _ => none
  | .ne e1 e2        =>
      match eval_expr env e1, eval_expr env e2 with
      | some v1, some v2 => some (.ival (if v1 = v2 then 0 else 1))
      | _, _ => none
  | .lt e1 e2        =>
      match eval_expr env e1, eval_expr env e2 with
      | some (.ival n1), some (.ival n2) => some (.ival (if n1 < n2 then 1 else 0))
      | some (.fval x1), some (.fval x2) => some (.ival (if x1 < x2 then 1 else 0))
      | _, _ => none
  | .le e1 e2        =>
      match eval_expr env e1, eval_expr env e2 with
      | some (.ival n1), some (.ival n2) => some (.ival (if n1 ≤ n2 then 1 else 0))
      | some (.fval x1), some (.fval x2) => some (.ival (if x1 ≤ x2 then 1 else 0))
      | _, _ => none
  | .gt e1 e2        =>
      match eval_expr env e1, eval_expr env e2 with
      | some (.ival n1), some (.ival n2) => some (.ival (if n1 > n2 then 1 else 0))
      | some (.fval x1), some (.fval x2) => some (.ival (if x1 > x2 then 1 else 0))
      | _, _ => none
  | .ge e1 e2        =>
      match eval_expr env e1, eval_expr env e2 with
      | some (.ival n1), some (.ival n2) => some (.ival (if n1 ≥ n2 then 1 else 0))
      | some (.fval x1), some (.fval x2) => some (.ival (if x1 ≥ x2 then 1 else 0))
      | _, _ => none
  | .addr_of _       => none
  | .deref _         => none
  | .sizeof_expr t   => some (.ival (sizeof t))
  | .cast t e        =>
      match eval_expr env e with
      | some v => some v
      | none => none
  | .call name args => none
  | .cmp_eq e1 e2 =>
      match eval_expr env e1, eval_expr env e2 with
      | some v1, some v2 => some (.ival (if v1 = v2 then 1 else 0))
      | _, _ => none
  | .cmp_ne e1 e2 =>
      match eval_expr env e1, eval_expr env e2 with
      | some v1, some v2 => some (.ival (if v1 ≠ v2 then 1 else 0))
      | _, _ => none
  | .cmp_ge e1 e2 =>
      match eval_expr env e1, eval_expr env e2 with
      | some (.ival n1), some (.ival n2) => some (.ival (if n1 ≥ n2 then 1 else 0))
      | some (.fval x1), some (.fval x2) => some (.ival (if x1 ≥ x2 then 1 else 0))
      | _, _ => none
  | .cmp_le e1 e2 =>
      match eval_expr env e1, eval_expr env e2 with
      | some (.ival n1), some (.ival n2) => some (.ival (if n1 ≤ n2 then 1 else 0))
      | some (.fval x1), some (.fval x2) => some (.ival (if x1 ≤ x2 then 1 else 0))
      | _, _ => none
  | .or_op e1 e2 =>
      match eval_expr env e1, eval_expr env e2 with
      | some (.ival n1), some (.ival n2) => some (.ival (if n1 ≠ 0 ∨ n2 ≠ 0 then 1 else 0))
      | _, _ => none
  | .field_access _ _ => none

/-! ## C 语句 -/

/-- Cv00 语句：9 种 -/
inductive Cv00Stmt where
  | assign   (lhs : String) (rhs : Cv00Expr)
  | declare  (name : String) (ty : Cv00Type) (init : Option Cv00Expr)
  | compound (body : List Cv00Stmt)
  | if_stmt  (cond : Cv00Expr) (thenBranch elseBranch : Cv00Stmt)
  | while_stmt (cond : Cv00Expr) (body : Cv00Stmt)
  | for_stmt (init cond step body : Cv00Stmt)
  | return_stmt (e : Option Cv00Expr)
  | call     (func : String) (args : List Cv00Expr)
  | nop
  deriving Repr

/-
  注：exec_stmt 的完整大步语义定义在 Cv00Memory.lean 中，
  因为它需要内存模型（Block, Ptr, Mem, alloc, free, load, store）。
-/

/-! ## 元理论性质 -/

/-- 字面量求值总是成功 -/
theorem eval_lit (n : Int) : eval_expr emptyEnv (.lit_int n) = some (.ival n) := by
  simp [eval_expr]

/-- 已定义变量的求值 -/
theorem eval_var_defined (env : Env) (x : String) (v : Cv00Val) :
    env x = some v → eval_expr env (.var x) = some v := by
  intro h
  simp [eval_expr, h]

/-- 未定义变量的求值 -/
theorem eval_var_undefined (x : String) : eval_expr emptyEnv (.var x) = none := by
  simp [eval_expr, emptyEnv]

/-- 整数加法的求值 -/
theorem eval_add_ival (n1 n2 : Int) :
    eval_expr emptyEnv (.add (.lit_int n1) (.lit_int n2)) = some (.ival (n1 + n2)) := by
  simp [eval_expr]

/-- sizeof_expr 求值 -/
theorem eval_sizeof (t : Cv00Type) :
    eval_expr emptyEnv (.sizeof_expr t) = some (.ival (sizeof t)) := by
  simp [eval_expr]

end lvFormal.Theory.Cv00Lang
