/-
Lv-00 自有理论核心：三元本体结构

本文件不以 Hilbert、Tarski 或传统欧氏公理为底层前提，而是根据论文初稿中
“三元本体结构”的描述，定义 Lv-00 内部可讨论对象：点、线、域。
-/

namespace Lv00Formal
namespace Theory
namespace Ontology

/-- Lv-00 内部对象的全局标识。 -/
abbrev ObjId := Nat

/-- Lv-00 三元本体类别：点、线、域。 -/
inductive OntKind where
  | point
  | line
  | domain
  deriving DecidableEq, Repr

/-- 点：最小位置单元，同时也是约束图中可索引、可归一化的实体。 -/
structure LvPoint where
  id : ObjId
  name : String
  deriving DecidableEq, Repr

/-- 线：承载连接、边界、相交、包含等关系的延展对象。 -/
structure LvLine where
  id : ObjId
  name : String
  deriving DecidableEq, Repr

/-- 域：局部几何结构、约束作用范围与推理上下文。 -/
structure LvDomain where
  id : ObjId
  name : String
  deriving DecidableEq, Repr

/-- Lv-00 内部几何对象。 -/
inductive LvObj where
  | pointObj : LvPoint → LvObj
  | lineObj : LvLine → LvObj
  | domainObj : LvDomain → LvObj
  deriving DecidableEq, Repr

/-- 对象所属的三元本体类别。 -/
def LvObj.kind : LvObj → OntKind
  | .pointObj _ => .point
  | .lineObj _ => .line
  | .domainObj _ => .domain

/-- 良构对象：目前作为接口保留，后续可加入命名空间、作用域、信任颜色等约束。 -/
def WellFormedObj (_o : LvObj) : Prop := True

/-- 三元本体封闭性：Lv-00 内部对象只能属于点、线、域三类之一。 -/
theorem ontology_closed (o : LvObj) :
    o.kind = OntKind.point ∨ o.kind = OntKind.line ∨ o.kind = OntKind.domain := by
  cases o <;> simp [LvObj.kind]

end Ontology
end Theory
end Lv00Formal
