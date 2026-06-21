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

end Lv00Formal
