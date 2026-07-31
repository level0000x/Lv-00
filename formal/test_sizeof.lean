import Mathlib

inductive T where
  | void
  | int32
  | float64
  | pointer (base : T)
  | array (elem : T) (len : Nat)
  | struct (fields : List T)

def sizeof : T → Nat
  | .void => 0
  | .int32 => 4
  | .float64 => 8
  | .pointer _ => 8
  | .array t n => sizeof t * n
  | .struct fs => sizeof_fields fs
where
  sizeof_fields : List T → Nat
    | [] => 0
    | f :: fs => sizeof f + sizeof_fields fs

-- 变量情形：change 的关键
example {elem : T} (len : Nat) : sizeof (.array elem len) = sizeof elem * len := by
  rfl

example {f : T} {fs : List T} : sizeof (.struct (f :: fs)) = sizeof f + sizeof.sizeof_fields fs := by
  rfl

example {f : T} {fs : List T} : sizeof (.struct (f :: fs)) = sizeof f + sizeof (.struct fs) := by
  rfl

-- 定理证明演练
theorem sizeof_positive (t : T) (h : Nondegenerate t) : sizeof t > 0 := by
  induction h with
  | int32 => decide
  | int64 => decide
  | float64 => decide
  | pointer _ => decide
  | array elem len _hlen hlen ih =>
      change 0 < sizeof elem * len
      exact Nat.mul_pos ih hlen
  | struct_cons f fs _hf ih =>
      change 0 < sizeof f + sizeof.sizeof_fields fs
      exact Nat.add_pos_left ih

inductive Nondegenerate : T → Prop
  | int32     : Nondegenerate .int32
  | int64     : Nondegenerate .int64
  | float64   : Nondegenerate .float64
  | pointer   (t : T) : Nondegenerate (.pointer t)
  | array     (elem : T) (len : Nat) : Nondegenerate elem → len > 0 → Nondegenerate (.array elem len)
  | struct_cons (f : T) (fs : List T) : Nondegenerate f → Nondegenerate (.struct (f :: fs))
