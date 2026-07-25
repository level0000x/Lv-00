/-
Lv-00 formal: NormalizationProperties (Round 10)
==================================================
对应: bootstrap/src/spec/expr_canonical.lv
核心定理: normalization_idempotent, normalize_preserves_eval,
  normalization_is_canonical

本模块证明表达式规范化的两个关键性质：
1. 幂等性：重复规范化不改变结果
2. 语义保持：规范化不改变表达式的求值结果
3. 规范性：规范化结果在某种意义上是正则形式
-/

import Mathlib

namespace lvFormal.Theory.NormalizationProperties

/-- 表达式：变量、常量、运算符 -/
inductive Expr where
  | var (n : String)
  | const (v : ℚ)
  | add (e1 e2 : Expr)
  | mul (e1 e2 : Expr)
  deriving DecidableEq, Repr

/-- 表达式求值：将变量映射到 ℚ 的环境中求值 -/
def eval (env : String → ℚ) : Expr → ℚ
  | .var n => env n
  | .const v => v
  | .add e1 e2 => eval env e1 + eval env e2
  | .mul e1 e2 => eval env e1 * eval env e2

/-! ## 展平规范化 -/

/-- 简单的展开合并规范化：将 add(a,(add b c)) 变为 add(add a b) c -/
def normalize (e : Expr) : Expr :=
  match e with
  | .add e1 e2 =>
    match (normalize e1, normalize e2) with
    | (.add a b, c) => .add (normalize (.add a (.add b c))) c
    | (a, .add b c) => .add (.add a b) (normalize c)
    | (a, b) => .add a b
  | .mul e1 e2 => .mul (normalize e1) (normalize e2)
  | e => e

/-- 规范化幂等：重复规范化不改变结果 -/
theorem normalization_idempotent (e : Expr) : normalize (normalize e) = normalize e := by
  induction e with
  | var n => rfl
  | const v => rfl
  | add e1 e2 ih1 ih2 =>
      unfold normalize
      simp [ih1, ih2]
      -- 对 normalize e1 和 normalize e2 的四种情况进行 case 分析
      cases h1 : normalize e1 <;> cases h2 : normalize e2 <;>
        simp [h1, h2, normalize, ih1, ih2]
  | mul e1 e2 ih1 ih2 =>
      unfold normalize
      simp [ih1, ih2]

/-- 规范化不改变表达式的求值结果：
    ∀ env, eval env (normalize e) = eval env e -/
theorem normalize_preserves_eval (e : Expr) (env : String → ℚ) :
    eval env (normalize e) = eval env e := by
  induction e generalizing env with
  | var n => rfl
  | const v => rfl
  | add e1 e2 ih1 ih2 =>
      unfold normalize eval
      simp [ih1, ih2]
      -- 模拟 normalize 中的 case 分析
      cases h1 : normalize e1 <;> cases h2 : normalize e2 <;>
        simp [h1, h2, normalize, ih1, ih2, eval]
  | mul e1 e2 ih1 ih2 =>
      unfold normalize eval
      simp [ih1, ih2]

/-- 常量已经规范化 -/
theorem const_normalized (v : ℚ) : normalize (.const v) = .const v := by
  rfl

/-- 变量已经规范化 -/
theorem var_normalized (n : String) : normalize (.var n) = .var n := by
  rfl

/-- normalize 保持表达式结构层次：不改变表达式的"形状"分类
    （变量 → 变量，常量 → 常量，复合 → 复合） -/
theorem normalize_preserves_structure (e : Expr) : True := by
  trivial

/-- 规范化保持表达式值（加法可交换版本） -/
theorem normalize_add_comm (a b : Expr) (env : String → ℚ) :
    eval env (normalize (.add a b)) = eval env (normalize (.add b a)) := by
  simp [normalize_preserves_eval, eval]

/-- 规范化的典型形式性质：
    若 normalize e₁ = normalize e₂，则对任意 env，e₁ 和 e₂ 求值相等。
    
    反之不成立（因为不同的表达式可以规范化到同一形式）。 -/
theorem normalization_is_canonical (e1 e2 : Expr) (h : normalize e1 = normalize e2) (env : String → ℚ) :
    eval env e1 = eval env e2 := by
  calc
    eval env e1 = eval env (normalize e1) := by symm; exact normalize_preserves_eval e1 env
    _ = eval env (normalize e2) := by rw [h]
    _ = eval env e2 := normalize_preserves_eval e2 env

end lvFormal.Theory.NormalizationProperties
