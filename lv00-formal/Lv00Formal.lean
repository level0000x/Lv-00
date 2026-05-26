/-!
# Lv-00 几何元语言形式化验证项目

本项目现在以 Lv-00 自有几何元语言理论为主线，而不是把 Hilbert 公理体系
作为主体直接复刻。Hilbert 相关文件仅保留在 `Classical/Hilbert` 中，作为
经典几何对照层与后续解释目标。

主线结构来自论文初稿：
1. 三元本体：点、线、域
2. 六条本原谓词：关联、之间、相交、包含、连接、等价
3. 八条基础公理/规则接口
4. 约束图与四态相容性
5. 归一化与多策略推理可靠性
6. 可追溯证明对象
-/

-- Lv-00 自有理论主线
import Lv00Formal.Theory.Ontology.Defs
import Lv00Formal.Theory.Predicates.Defs
import Lv00Formal.Theory.Axioms.Primitive
import Lv00Formal.Theory.Constraint.Graph
import Lv00Formal.Theory.Reasoning.Soundness
import Lv00Formal.Theory.Proof.Trace

-- 基础几何类型：仅作为工程互操作和外部解释的底层辅助
import Lv00Formal.Basic.Defs
import Lv00Formal.Basic.Notation

-- 经典对照层：不是主理论
import Lv00Formal.Classical.Hilbert.Incidence
import Lv00Formal.Classical.Hilbert.Consistency

-- 互操作验证接口
import Lv00Formal.Interop.FFI
import Lv00Formal.Interop.Equivalence

namespace Lv00Formal

-- 自有理论核心导出
export Theory.Ontology (ObjId OntKind LvPoint LvLine LvDomain LvObj WellFormedObj ontology_closed)
export Theory.Predicates (PrimPredKind PrimPred arity WellFormedPred predicate_closed)
export Theory.Axioms (BaseAxiomKind BaseAxiomRule WellFormedRule LvAxiomSystem rule_conclusion_closed)
export Theory.Constraint (ConstraintStatus ConstraintGraph WellFormedGraph normalize NormalizationIdempotent NormalizationPreservesWellFormedness checkStatus)
export Theory.Reasoning (InferenceStep WellFormedStep ReasoningSoundness reasoning_soundness_skeleton)
export Theory.Proof (StepId ProofStep ProofObject Traceable ProofWellFormed)

-- 经典对照层导出
export Classical.Hilbert (IncidenceAxioms HilbertAxioms consistency_theorem)

end Lv00Formal
