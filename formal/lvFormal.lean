/-
lvFormal 主入口

本文件作为 formal/lvFormal 模块的主入口，导入并导出所有子模块。
包括 R1-R3 新模块：lvLang, IR, Compiler, CompilerCorrectness,
Cv00Lang, Cv00Memory, BootstrapCorrectness。
-/

import lvFormal.Theory.lvLang
import lvFormal.Theory.IR
import lvFormal.Theory.Compiler
import lvFormal.Theory.CompilerCorrectness
import lvFormal.Theory.Cv00Lang
import lvFormal.Theory.Cv00Memory
import lvFormal.Theory.BootstrapCorrectness

-- Round 5-6: Theory modules (rebuilt)
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
-- v1.1 R3+: Definition modules (dependencies)
import lvFormal.Theory.GeometricAlgebraDefs
import lvFormal.Theory.GeometryPresetDefs
import lvFormal.Theory.ODESolverDefs
import lvFormal.Theory.NumericDefs
import lvFormal.Theory.PresetGeometryDefs
import lvFormal.Theory.BootstrapDefs
-- v1.1 R4+: Coverage modules
import lvFormal.Theory.ConstraintPropagation
import lvFormal.Theory.InteropSoundness
import lvFormal.Theory.OrchestrationSoundness
import lvFormal.Theory.AxiomDiscoveryTheory
import lvFormal.Theory.FormulaSemantics
import lvFormal.Theory.VisualLayerSoundness
import lvFormal.Theory.MetaVerificationTheory
import lvFormal.Theory.StreamingTheory
-- v1.1 R4: Codegen + CodegenCorrectness
import lvFormal.Theory.Codegen
import lvFormal.Theory.CodegenCorrectness
-- v1.1 R5: UndefinedBehavior + Evidence
import lvFormal.Theory.UndefinedBehavior
import lvFormal.Theory.Evidence
-- v1.1 R6: InteropCorrectness
import lvFormal.Theory.InteropCorrectness
-- v1.1 Hilbert framework
import lv.Basic
import lv.Incidence
import lv.Betweenness
import lv.Congruence
import lv.Parallel
import lv.Continuity
import lv.Order
import lv.HilbertAxioms
import lv.EuclideanPlane
import lv.lvMeta

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
  zero_pt zero_sc
  stmt_compiled_edge_correct_point
  stmt_compiled_edge_correct_constraint
  stmt_compiled_edge_correct_prove
  stmt_compiled_edge_correct_normalize
  compile_preserves_satisfiability
  compiler_semantics_consistent compiler_idempotent
  compile_append_satisfiable
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
  free_null load_freed store_freed
  exec_nop exec_assign exec_preserves_mem_if_no_call
  Cv00Semantics
)

-- BootstrapCorrectness exports
export Theory.BootstrapCorrectness (
  bootstrap_pipeline
  bootstrap_empty_program
  compiler_semantics_preservation
  bootstrap_empty_correct
  compiler_semantics_fidelity
  bootstrap_pipeline_idempotent
  bootstrap_preserves_satisfiability
  bootstrap_never_unsatisfiable_empty
)

-- Round 5-6 exports
export Theory.ProofEngineSoundness (soundness_of_proof multi_strategy_completeness)
export Theory.SolverCorrectness (solver_soundness solver_termination)
export Theory.RewriteProperties (rewrite_soundness rewrite_confluence)
export Theory.KernelInvariants (kernel_preserves_invariants kernel_state_safe)
export Theory.ConstraintSoundness (constraint_soundness constraint_preserved)
export Theory.RemovedModule (rose_cognition_sound rose_cognition_complete)
export Theory.GroebnerTheory (groebner_soundness groebner_completeness)
export Theory.MathPresetSoundness (math_preset_soundness math_preset_preserves)
export Theory.DSLWrappersSoundness (dsl_wrapper_sound dsl_wrapper_correct)
export Theory.InteractiveGeoSoundness (interactive_geo_sound interactive_geo_deterministic)
export Theory.GeomPresetSoundness (geom_preset_soundness geom_preset_valid)
export Theory.NormalizationProperties (normalization_sound normalization_confluent)
export Theory.StreamInvariants (stream_invariant_preserved stream_state_safe)
export Theory.EngineInvariants (engine_invariant_preserved engine_state_consistent)
export Theory.NDimGeometry (ndim_soundness ndim_completeness)
export Theory.DifferentialGeometry (diff_geo_soundness diff_geo_consistent)
export Theory.GeometryPresets (geometry_preset_sound geometry_preset_complete)
export Theory.GeometricAlgebra (geometric_algebra_sound geometric_algebra_consistent)
export Theory.Numeric (numeric_stability numeric_precision)
export Theory.ODESolver (ode_solver_soundness ode_solver_convergence)
export Theory.PresetGeometry (preset_geometry_sound preset_geometry_valid)

end lvFormal
