/-
Lv-00 自有理论核心：重写项与替换

该文件为合一算法提供最小项语言与替换操作。
-/

namespace Lv00Formal
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
theorem apply_emptySubst (t : Term) :
    applySubst emptySubst t = t := by
  cases t with
  | var v => simp [applySubst, emptySubst, lookupSubst]
  | const n => simp [applySubst, emptySubst]
  | app f args =>
      simp [applySubst, emptySubst]
      -- 使用列表归纳证明 map 保持
      have h : ∀ (l : List Term), l.map (applySubst emptySubst) = l := by
        intro l
        induction l with
        | nil => simp
        | cons a rest ih =>
            simp [ih]
            rw [apply_emptySubst a]
      subst h
      simp [apply_emptySubst]

end Rewrite
end Theory
end Lv00Formal
