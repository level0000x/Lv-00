/-
lvFormal 主入口（统一版 v1.1）

本文件作为 lvFormal 模块的主入口，导入并导出所有子模块。
合并自 formal/ 和 lv-formal/ 两个项目的历史分叉。

包含：
- Theory/*（平铺的 47 个证明模块）
- Theory/Axioms/（Instances_* + PackageValidation_* 系列）
- Theory/{Constraint,Groebner,Ontology,Predicates,Proof,Rewrite,Unification,Reasoning}/
- Classical/Hilbert/（Hilbert 几何公理类型类 + 无矛盾性证明）
- Basic/Defs.lean（基础几何结构定义）
- Interop/（互操作层）
-/

-- Theory: 平铺模块
import lvFormal.Theory.IR
import lvFormal.Theory.lvLang
import lvFormal.Theory.LvDSL
import lvFormal.Theory.LvSemantics
import lvFormal.Theory.LogicalFramework
import lvFormal.Theory.Compiler
import lvFormal.Theory.CompilerCorrectness
import lvFormal.Theory.Codegen
import lvFormal.Theory.CodegenCorrectness
import lvFormal.Theory.Cv00Lang
import lvFormal.Theory.Cv00Memory
import lvFormal.Theory.BootstrapCorrectness
import lvFormal.Theory.BootstrapDefs
import lvFormal.Theory.ProofEngineSoundness
import lvFormal.Theory.SolverCorrectness
import lvFormal.Theory.RewriteProperties
import lvFormal.Theory.KernelInvariants
import lvFormal.Theory.ConstraintSoundness
import lvFormal.Theory.RemovedModule
import lvFormal.Theory.GroebnerTheory
import lvFormal.Theory.MathPresetSoundness
import lvFormal.Theory.DSLWrappersSoundness
import lvFormal.Theory.InteractiveGeoSoundness
import lvFormal.Theory.GeomPresetSoundness
import lvFormal.Theory.NormalizationProperties
import lvFormal.Theory.StreamInvariants
import lvFormal.Theory.EngineInvariants
import lvFormal.Theory.NDimGeometry
import lvFormal.Theory.DifferentialGeometry
import lvFormal.Theory.GeometryPresets
import lvFormal.Theory.GeometricAlgebra
import lvFormal.Theory.Numeric
import lvFormal.Theory.ODESolver
import lvFormal.Theory.PresetGeometry
import lvFormal.Theory.GeometricAlgebraDefs
import lvFormal.Theory.GeometryPresetDefs
import lvFormal.Theory.ODESolverDefs
import lvFormal.Theory.NumericDefs
import lvFormal.Theory.PresetGeometryDefs
import lvFormal.Theory.ConstraintPropagation
import lvFormal.Theory.InteropSoundness
import lvFormal.Theory.OrchestrationSoundness
import lvFormal.Theory.AxiomDiscoveryTheory
import lvFormal.Theory.FormulaSemantics
import lvFormal.Theory.VisualLayerSoundness
import lvFormal.Theory.MetaVerificationTheory
import lvFormal.Theory.StreamingTheory
import lvFormal.Theory.UndefinedBehavior
import lvFormal.Theory.Evidence
import lvFormal.Theory.InteropCorrectness

-- Theory: 子目录模块
import lvFormal.Theory.Ontology.Defs
import lvFormal.Theory.Predicates.Defs
import lvFormal.Theory.Axioms.Primitive
import lvFormal.Theory.Axioms.RuleTemplate
import lvFormal.Theory.Axioms.Instances
import lvFormal.Theory.Axioms.PackageValidation
import lvFormal.Theory.Constraint.Graph
import lvFormal.Theory.Constraint.Normalization
import lvFormal.Theory.Rewrite.Defs
import lvFormal.Theory.Unification.Defs
import lvFormal.Theory.Groebner.Defs
import lvFormal.Theory.Reasoning.Soundness
import lvFormal.Theory.Proof.Trace

-- Classical Hilbert 框架
import lvFormal.Basic.Defs
import lvFormal.Classical.Hilbert.Basic
import lvFormal.Classical.Hilbert.Incidence
import lvFormal.Classical.Hilbert.Betweenness
import lvFormal.Classical.Hilbert.Congruence
import lvFormal.Classical.Hilbert.Parallel
import lvFormal.Classical.Hilbert.Order
import lvFormal.Classical.Hilbert.Continuity
import lvFormal.Classical.Hilbert.HilbertAxioms
import lvFormal.Classical.Hilbert.EuclideanPlane
import lvFormal.Classical.Hilbert.Consistency
import lvFormal.Classical.Hilbert.lvMeta

-- Interop
import lvFormal.Interop.Equivalence
import lvFormal.Interop.FFI

namespace lvFormal

-- lvLang exports
export Theory.lvLang (
  VarName Coord
  lvPoint lvConstraintKind lvConstraint lvStmt lvProgram
  State initialState
  addPoint addConstraint setProve setNormalize
  eval_stmt eval_program
  satisfiable no_errors
  eval_program_empty
  empty_state_no_errors empty_satisfiable
  eval_point_defines_var eval_point_preserves_other
  eval_constraint_adds_one
  eval_point_preserves_constraints eval_constraint_preserves_points
  prove_idempotent normalize_idempotent
)

-- IR exports
export Theory.IR (
  ptX ptY dist dot cross
  IRExpr eval_expr
  IRConstraint ir_sem
  ConstraintGraph graph_satisfied graph_satisfiable
  empty_graph_satisfiable
  dist_symm dist_self collinear_symm
)

-- Compiler exports
export Theory.Compiler (
  compile_point compile_points
  compile_constraint compile_stmt compile_program
  compile_empty compile_prove_empty compile_point_single
  compile_constraint_single compile_program_append
)

-- CompilerCorrectness exports
export Theory.CompilerCorrectness (
  compile_preserves_satisfiability
  compile_never_unsatisfiable
  correctness_with_sat_hypothesis
)

-- Cv00Lang exports
export Theory.Cv00Lang (
  Cv00Type sizeof Nondegenerate sizeof_positive
  Cv00Val
  Env emptyEnv env_set env_alloc
  Cv00Expr eval_expr
  Cv00Stmt
  eval_lit eval_var_defined eval_var_undefined
  eval_add_ival eval_sizeof
)

-- Cv00Memory exports
export Theory.Cv00Memory (
  Block Ptr Mem emptyMem
  ptr_valid alloc free load store
  mem_safe
  ExecResult exec_stmt
)

-- BootstrapCorrectness exports
export Theory.BootstrapCorrectness (
  bootstrap_pipeline
  bootstrap_empty_program
  compiler_semantics_preservation
  bootstrap_empty_correct
  bootstrap_preserves_satisfiability
  bootstrap_never_unsatisfiable_empty
)

-- Round 5-6 exports
-- (reserved for future re-exports)

end lvFormal
