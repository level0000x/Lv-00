/-
Lv00Formal 主入口

本文件作为 formal/Lv00Formal 模块的主入口，导入并导出所有子模块。
包括 R1-R3 新模块：Lv00Lang, IR, Compiler, CompilerCorrectness,
Cv00Lang, Cv00Memory, BootstrapCorrectness。
-/

import Lv00Formal.Theory.Lv00Lang
import Lv00Formal.Theory.IR
import Lv00Formal.Theory.Compiler
import Lv00Formal.Theory.CompilerCorrectness
import Lv00Formal.Theory.Cv00Lang
import Lv00Formal.Theory.Cv00Memory
import Lv00Formal.Theory.BootstrapCorrectness

-- Round 5-6: Theory modules (rebuilt)
import Lv00Formal.Theory.ProofEngineSoundness
import Lv00Formal.Theory.SolverCorrectness
import Lv00Formal.Theory.RewriteProperties
import Lv00Formal.Theory.KernelInvariants
import Lv00Formal.Theory.ConstraintSoundness
import Lv00Formal.Theory.ROSECognition
import Lv00Formal.Theory.GroebnerTheory
import Lv00Formal.Theory.MathPresetSoundness
import Lv00Formal.Theory.DSLWrappersSoundness
import Lv00Formal.Theory.InteractiveGeoSoundness
import Lv00Formal.Theory.GeomPresetSoundness
import Lv00Formal.Theory.NormalizationProperties
import Lv00Formal.Theory.StreamInvariants
import Lv00Formal.Theory.EngineInvariants
import Lv00Formal.Theory.NDimGeometry
import Lv00Formal.Theory.DifferentialGeometry
import Lv00Formal.Theory.GeometryPresets
import Lv00Formal.Theory.GeometricAlgebra
import Lv00Formal.Theory.Numeric
import Lv00Formal.Theory.ODESolver
import Lv00Formal.Theory.PresetGeometry
-- v1.1 R3+: Definition modules (dependencies)
import Lv00Formal.Theory.GeometricAlgebraDefs
import Lv00Formal.Theory.GeometryPresetDefs
import Lv00Formal.Theory.ODESolverDefs
import Lv00Formal.Theory.NumericDefs
import Lv00Formal.Theory.PresetGeometryDefs
import Lv00Formal.Theory.BootstrapDefs
-- v1.1 R4+: Coverage modules
import Lv00Formal.Theory.ConstraintPropagation
import Lv00Formal.Theory.InteropSoundness
import Lv00Formal.Theory.OrchestrationSoundness
import Lv00Formal.Theory.AxiomDiscoveryTheory
import Lv00Formal.Theory.FormulaSemantics
import Lv00Formal.Theory.VisualLayerSoundness
import Lv00Formal.Theory.MetaVerificationTheory
import Lv00Formal.Theory.StreamingTheory
-- v1.1 R4: Codegen + CodegenCorrectness
import Lv00Formal.Theory.Codegen
import Lv00Formal.Theory.CodegenCorrectness
-- v1.1 R5: UndefinedBehavior + Evidence
import Lv00Formal.Theory.UndefinedBehavior
import Lv00Formal.Theory.Evidence
-- v1.1 Hilbert framework
import Lv00.Basic
import Lv00.Incidence
import Lv00.Betweenness
import Lv00.Congruence
import Lv00.Parallel
import Lv00.Continuity
import Lv00.Order
import Lv00.HilbertAxioms
import Lv00.EuclideanPlane
import Lv00.Lv00Meta

namespace Lv00Formal

-- Lv00Lang exports
export Theory.Lv00Lang (
  VarName Coord
  Lv00Point Lv00ConstraintKind Lv00Constraint Lv00Stmt Lv00Program
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
  Cv00Type sizeof sizeof_positive
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
export Theory.ROSECognition (rose_cognition_sound rose_cognition_complete)
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

end Lv00Formal
