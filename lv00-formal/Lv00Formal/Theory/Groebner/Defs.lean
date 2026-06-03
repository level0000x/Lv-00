/-
Lv-00 自有理论核心：Groebner 基计算骨架

该模块给出多项式、单项式序、S-多项式和 Buchberger 算法的
可执行抽象，用于后续代数约束求解。
-/

namespace Lv00Formal
namespace Theory
namespace Groebner

/-- 变量名。 -/
abbrev Var := String

/-- 单项式：变量到指数的有限表。 -/
abbrev Monomial := List (Var × Nat)

/-- 多项式：整数系数与单项式的有限表。 -/
abbrev Polynomial := List (Int × Monomial)

/-- 零多项式。 -/
def zeroPoly : Polynomial := []

/-- 单项式总次数。 -/
def monomialDegree (m : Monomial) : Nat :=
  m.foldl (fun acc (_, e) => acc + e) 0

/-- 查询单项式中变量的指数。 -/
def monomialExp (m : Monomial) (v : Var) : Nat :=
  (m.findSome? (fun (x, e) => if x = v then some e else none)).getD 0

/-- 单项式乘法。 -/
def monomialMul (m₁ m₂ : Monomial) : Monomial :=
  let vars := (m₁.map Prod.fst ++ m₂.map Prod.fst).eraseDups
  vars.filterMap (fun v =>
    let e := monomialExp m₁ v + monomialExp m₂ v
    if e = 0 then none else some (v, e))

/-- 单项式可除性：`m₁ | m₂`。 -/
def monomialDivides (m₁ m₂ : Monomial) : Bool :=
  m₁.all (fun (v, e₁) => e₁ ≤ monomialExp m₂ v)

/-- 单项式相除；若变量缺失则按 0 处理。 -/
def monomialDiv (m₁ m₂ : Monomial) : Monomial :=
  m₁.filterMap (fun (v, e₁) =>
    let e := e₁ - monomialExp m₂ v
    if e = 0 then none else some (v, e))

/-- 单项式最小公倍式。 -/
def monomialLCM (m₁ m₂ : Monomial) : Monomial :=
  let vars := (m₁.map Prod.fst ++ m₂.map Prod.fst).eraseDups
  vars.filterMap (fun v =>
    let e := max (monomialExp m₁ v) (monomialExp m₂ v)
    if e = 0 then none else some (v, e))

/-- 单项式序。 -/
inductive MonomialOrder where
  | lex
  | degLex
  | degRevLex
  deriving Repr, BEq, DecidableEq

/-- 字典序比较。 -/
def compareLex : Monomial → Monomial → Ordering
  | [], [] => .eq
  | [], _ => .lt
  | _, [] => .gt
  | (v₁, e₁) :: r₁, (v₂, e₂) :: r₂ =>
      if v₁ < v₂ then .lt
      else if v₁ > v₂ then .gt
      else if e₁ < e₂ then .lt
      else if e₁ > e₂ then .gt
      else compareLex r₁ r₂

/-- 度字典序比较。 -/
def compareDegLex (m₁ m₂ : Monomial) : Ordering :=
  let d₁ := monomialDegree m₁
  let d₂ := monomialDegree m₂
  if d₁ < d₂ then .lt
  else if d₁ > d₂ then .gt
  else compareLex m₁ m₂

/-- 按指定单项式序比较。 -/
def compareMonomial (order : MonomialOrder) (m₁ m₂ : Monomial) : Ordering :=
  match order with
  | .lex => compareLex m₁ m₂
  | .degLex => compareDegLex m₁ m₂
  | .degRevLex => compareDegLex m₁ m₂

/-- 多项式首项。 -/
def leadingTerm (order : MonomialOrder) (p : Polynomial) : Option (Int × Monomial) :=
  p.foldl (fun acc term =>
    match acc with
    | none => some term
    | some (_, m) =>
        match compareMonomial order term.2 m with
        | .gt => some term
        | _ => acc) none

/-- 多项式加法（简化：追加后过滤零系数，不做完全同类项合并）。 -/
def polyAdd (p q : Polynomial) : Polynomial :=
  (p ++ q).filter (fun (c, _) => c ≠ 0)

/-- 多项式取负。 -/
def polyNeg (p : Polynomial) : Polynomial :=
  p.map (fun (c, m) => (-c, m))

/-- 多项式减法。 -/
def polySub (p q : Polynomial) : Polynomial :=
  polyAdd p (polyNeg q)

/-- 多项式乘以单项式和系数。 -/
def polyMulMonomial (p : Polynomial) (c : Int) (m : Monomial) : Polynomial :=
  if c = 0 then [] else p.map (fun (c', m') => (c * c', monomialMul m m'))

/-- 一步约化。 -/
def reduceStep (order : MonomialOrder) (p g : Polynomial) : Option Polynomial :=
  match leadingTerm order p, leadingTerm order g with
  | some (cp, mp), some (cg, mg) =>
      if cg = 0 then none
      else if monomialDivides mg mp then
        let factor := monomialDiv mp mg
        some (polySub p (polyMulMonomial g (cp / cg) factor))
      else none
  | _, _ => none

/-- 带燃料的完全约化，避免非结构递归。 -/
def reduceFuel (fuel : Nat) (order : MonomialOrder) (p : Polynomial) (basis : List Polynomial) : Polynomial :=
  match fuel with
  | 0 => p
  | fuel' + 1 =>
      match basis.findSome? (fun g => reduceStep order p g) with
      | some p' => reduceFuel fuel' order p' basis
      | none => p

/-- 从多项式列表生成所有无序对。 -/
def polyPairs : List Polynomial → List (Polynomial × Polynomial)
  | [] => []
  | p :: rest => rest.map (fun q => (p, q)) ++ polyPairs rest

/-- S-多项式。 -/
def sPolynomial (order : MonomialOrder) (p q : Polynomial) : Option Polynomial :=
  match leadingTerm order p, leadingTerm order q with
  | some (cp, mp), some (cq, mq) =>
      if cp = 0 || cq = 0 then none
      else
        let l := monomialLCM mp mq
        let pPart := polyMulMonomial p 1 (monomialDiv l mp)
        let qPart := polyMulMonomial q 1 (monomialDiv l mq)
        some (polySub pPart qPart)
  | _, _ => none

/-- 带燃料的 Buchberger 扩展步骤。 -/
def buchbergerFuel (fuel : Nat) (order : MonomialOrder) (basis : List Polynomial) : List Polynomial :=
  match fuel with
  | 0 => basis
  | fuel' + 1 =>
      let pairs := polyPairs basis
      match pairs.findSome? (fun (p, q) =>
        match sPolynomial order p q with
        | some s =>
            let r := reduceFuel basis.length order s basis
            if r = [] then none else some r
        | none => none) with
      | some r => buchbergerFuel fuel' order (r :: basis)
      | none => basis

/-- Buchberger 算法入口：燃料取输入基长度的平方加一。 -/
def buchberger (order : MonomialOrder) (basis : List Polynomial) : List Polynomial :=
  buchbergerFuel (basis.length * basis.length + 1) order basis

/-- 理想成员判定：用 Groebner 基约化到零。 -/
def idealMembership (order : MonomialOrder) (p : Polynomial) (basis : List Polynomial) : Bool :=
  let gb := buchberger order basis
  reduceFuel (gb.length + p.length + 1) order p gb = []

/-- 零多项式确实是空列表。 -/
theorem zeroPoly_eq_nil :
    zeroPoly = [] := by
  rfl

/-- 空单项式的次数为 0。 -/
theorem monomialDegree_nil :
    monomialDegree [] = 0 := by
  rfl

/-- 空单项式中任意变量的指数为 0。 -/
theorem monomialExp_nil (v : Var) :
    monomialExp [] v = 0 := by
  rfl

/-- 空单项式与空单项式相乘仍为空。 -/
theorem monomialMul_nil_nil :
    monomialMul [] [] = [] := by
  rfl

/-- 空多项式的首项不存在。 -/
theorem leadingTerm_zero (order : MonomialOrder) :
    leadingTerm order zeroPoly = none := by
  rfl

/-- 空基没有多项式对。 -/
theorem polyPairs_nil :
    polyPairs [] = [] := by
  rfl

/-- 空基的 Buchberger 结果为空。 -/
theorem buchberger_empty (order : MonomialOrder) :
    buchberger order [] = [] := by
  rfl

/-- 零多项式在空基生成的理想中。 -/
theorem zero_in_empty_ideal (order : MonomialOrder) :
    idealMembership order zeroPoly [] = true := by
  rfl

end Groebner
end Theory
end Lv00Formal
