/-
自举编译器补充定义

本模块为 formal/Lv00/ 自举编译器提供基础类型与语义桩定义，
包括 Lv00 源码表示、编译函数和语义函数。
为 BootstrapCorrectness 定理文件提供必要的类型依赖。

对应论文中的自举编译器形式化补充定义层。
-/

import Lv00Formal.Theory.Lv00Lang

namespace Lv00Formal.Theory.BootstrapDefs

open Lv00Lang

/-! ## 源码与编译 -/

/-- Lv00 源码：Lv00 程序的字符串表示 -/
def Lv00Source := String

/-- Lv00 源码编译为 Lv00 语句列表（占位） -/
def compile_lv00 (src : Lv00Source) : List Lv00Stmt :=
  []

/-- Lv00 源码的语义：执行后的状态 -/
def semantics_lv00 (src : Lv00Source) : State :=
  eval_program initialState (compile_lv00 src)

/-! ## 空状态 -/

/-- 空状态引用（委托给 Lv00Lang.emptyState 的语义等价） -/
def emptyState : State :=
  initialState

end Lv00Formal.Theory.BootstrapDefs
