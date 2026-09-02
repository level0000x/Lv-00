/-
Lv-00 自有理论核心：重写项与替换

该文件为合一算法提供最小项语言与替换操作。
-/

namespace lvFormal
namespace Theory
namespace Rewrite

/-- 项变量。 -/
abbrev Var := String

/-- 简单项：变量、常数或函数应用。 -/
inductive Term where
  | var (name : Var)
  | const (value : Nat)
  | app (fn : String) (args : List Term)
  deriving Repr, BEq

/-- 替换：变量到项的映射。 -/
abbrev Substitution := List (Var × Term)

/-- 空替换。 -/
def emptySubst : Substitution := []

/-- 查找变量在替换中的值。 -/
def lookupSubst (σ : Substitution) (v : Var) : Option Term :=
  σ.findSome? (fun (x, t) => if x = v then some t else none)

/-- 应用替换到项。 -/
def applySubst (σ : Substitution) : Term → Term
  | .var v =>
    match lookupSubst σ v with
    | some t => t
    | none => .var v
  | .const n => .const n
  | .app f args => .app f (args.map (applySubst σ))

/-- 空替换保持项不变。 -/
theorem apply_emptySubst (t : Term) : applySubst emptySubst t = t := by
  induction t with
  | var v =>
      unfold applySubst lookupSubst emptySubst
      simp
  | const n =>
      unfold applySubst
      rfl
  | app f args ih =>
      unfold applySubst
      simp [ih]

end Rewrite
end Theory
end lvFormal
