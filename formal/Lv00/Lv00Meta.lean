/-  元语言桥接：将 Lv00Lang 形式语法的符号映射到 Hilbert 平面的几何语义
   提供从形式语言变量到几何对象的翻译层 -/

import Lv00.HilbertAxioms

namespace Lv00.Lv00Meta

open HilbertAxioms

/--  Lv00Lang 形式语言的定义 --/
namespace Lv00Lang

/--  形式语言中的变量名（用字符串表示） --/
abbrev VarName := String

end Lv00Lang

variable {P L : Type} [HilbertPlane P L]

/--  在给定环境下，将 Lv00Lang 变量名翻译为 Hilbert 平面中的点 --/
def point_of_lv00 (env : Lv00Lang.VarName → P) (v : Lv00Lang.VarName) : P := env v

/--  在给定环境下，将两个变量名翻译为一条线段 --/
def segment_of_lv00 (env : Lv00Lang.VarName → P) (v w : Lv00Lang.VarName) : P × P :=
  (point_of_lv00 env v, point_of_lv00 env w)

end Lv00.Lv00Meta
