import Mathlib
import lvFormal.Theory.LvDSL

open lvFormal.Theory.LvDSL

set_option pp.all true

namespace ScratchD

lemma s_intLit (v : ℤ) : lv_type_check (.intLit v) .int = true := by
  simp [lv_type_check]

lemma s_lambda (p : String) (t codom : LvType) (b : LvExpr)
    (h_body : lv_type_check b codom = true) :
    lv_type_check (.lambda p t b) (.arrow t codom) = true := by
  simp [lv_type_check, h_body]

lemma s_none (t : LvType) : lv_type_check (.none t) (.option t) = true := by
  simp [lv_type_check]

lemma s_some (e : LvExpr) (t : LvType) (h : lv_type_check e t = true) :
    lv_type_check (.some e) (.option t) = true := by
  simp [lv_type_check, h]

end ScratchD
