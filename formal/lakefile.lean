import Lake
open Lake DSL

package Lv00 where

lean_options := #[`autoImplicit, false]

@[default_target]
lean_lib Lv00 where

require mathlib from git
  "https://github.com/leanprover-community/mathlib4.git" @ "v4.14.0"

@[default_target]
lean_exe tests where
  root := `tests
