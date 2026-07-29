/-
自举编译器补充定义

本模块为 formal/lv/ 自举编译器提供基础类型与语义桩定义，
包括 lv 源码表示、编译函数和语义函数。
为 BootstrapCorrectness 定理文件提供必要的类型依赖。

对应论文中的自举编译器形式化补充定义层。
-/

import lvFormal.Theory.lvLang

namespace lvFormal.Theory.BootstrapDefs

open lvLang

/-! ## 源码与编译 -/

/-- lv 源码：lv 程序的字符串表示 -/
def lvSource := String

/-- lv 源码编译为 lv 语句列表（占位） -/
def compile_lv (src : lvSource) : List lvStmt :=
  []

/-- lv 源码的语义：执行后的状态 -/
def semantics_lv (src : lvSource) : State :=
  eval_program initialState (compile_lv src)

/-! ## 空状态 -/

/-- 空状态引用（委托给 lvLang.emptyState 的语义等价） -/
def emptyState : State :=
  initialState

end lvFormal.Theory.BootstrapDefs
