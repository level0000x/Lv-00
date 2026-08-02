$file = "c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\formal\lvFormal\Theory\ConvenienceAPIsTheory.lean"
$content = [System.IO.File]::ReadAllText($file)

# Replace StringOpSpecRefines definition
$old_refines = "def StringOpSpecRefines (spec1 spec2 : StringOpSpec) : Prop :=@n  (∀ s r, spec1.trim_pre s → spec1.trim_post s r → spec2.trim_post s r) ∧@n  (∀ s r, spec1.split_pre s → spec1.split_post s r → spec2.split_post s r) ∧@n  (∀ parts r, spec1.join_pre parts → spec1.join_post parts r → spec2.join_post parts r) ∧@n  (∀ s old new r, spec1.replace_pre s old new → spec1.replace_post s old new r → spec2.replace_post s old new r) ∧@n  (∀ fmt args r, spec1.format_pre fmt args → spec1.format_post fmt args r → spec2.format_post fmt args r)".Replace("@n", "`n")

$new_refines = "def StringOpSpecRefines (spec1 spec2 : StringOpSpec) : Prop :=@n  (∀ s, spec2.trim_pre s → spec1.trim_pre s) ∧@n  (∀ s r, spec1.trim_pre s → spec1.trim_post s r → spec2.trim_post s r) ∧@n  (∀ s, spec2.split_pre s → spec1.split_pre s) ∧@n  (∀ s r, spec1.split_pre s → spec1.split_post s r → spec2.split_post s r) ∧@n  (∀ parts, spec2.join_pre parts → spec1.join_pre parts) ∧@n  (∀ parts r, spec1.join_pre parts → spec1.join_post parts r → spec2.join_post parts r) ∧@n  (∀ s old new, spec2.replace_pre s old new → spec1.replace_pre s old new) ∧@n  (∀ s old new r, spec1.replace_pre s old new → spec1.replace_post s old new r → spec2.replace_post s old new r) ∧@n  (∀ fmt args, spec2.format_pre fmt args → spec1.format_pre fmt args) ∧@n  (∀ fmt args r, spec1.format_pre fmt args → spec1.format_post fmt args r → spec2.format_post fmt args r)".Replace("@n", "`n")

if ($content.Contains($old_refines)) {
    $content = $content.Replace($old_refines, $new_refines)
    Write-Host "Refines definition updated"
} else {
    Write-Host "Refines NOT FOUND"
}

# Replace spec_refinement_string proof
$old_proof = "theorem spec_refinement_string (op : StringOp) (spec1 spec2 : StringOpSpec)@n    (h : StringOpSpecRefines spec1 spec2) (hsat : StringOpSatisfies op spec1) :@n    StringOpSatisfies op spec2 := by@n  sorry".Replace("@n", "`n")

$new_proof = "theorem spec_refinement_string (op : StringOp) (spec1 spec2 : StringOpSpec)@n    (h : StringOpSpecRefines spec1 spec2) (hsat : StringOpSatisfies op spec1) :@n    StringOpSatisfies op spec2 := by@n  rcases h with ⟨h_trim_pre, h_trim_post, h_split_pre, h_split_post, h_join_pre, h_join_post, h_replace_pre, h_replace_post, h_format_pre, h_format_post⟩@n  cases op <;>@n    simp [StringOpSatisfies] at *@n  · intro s hpre@n    have hpre1 : spec1.trim_pre s := h_trim_pre s hpre@n    have hpost1 : spec1.trim_post s (s.trim) := hsat s hpre1@n    exact h_trim_post s (s.trim) hpre1 hpost1@n  · intro s hpre@n    have hpre1 : spec1.split_pre s := h_split_pre s hpre@n    have hpost1 : spec1.split_post s (s.split Char.isWhitespace) := hsat s hpre1@n    exact h_split_post s (s.split Char.isWhitespace) hpre1 hpost1@n  · intro parts hpre@n    have hpre1 : spec1.join_pre parts := h_join_pre parts hpre@n    have hpost1 : spec1.join_post parts (String.intercalate \",\" parts) := hsat parts hpre1@n    exact h_join_post parts (String.intercalate \",\" parts) hpre1 hpost1@n  · intro s old new hpre@n    have hpre1 : spec1.replace_pre s old new := h_replace_pre s old new hpre@n    have hpost1 : spec1.replace_post s old new (s.replace old new) := hsat s old new hpre1@n    exact h_replace_post s old new (s.replace old new) hpre1 hpost1@n  · intro fmt args hpre@n    have hpre1 : spec1.format_pre fmt args := h_format_pre fmt args hpre@n    have hpost1 : spec1.format_post fmt args fmt := hsat fmt args hpre1@n    exact h_format_post fmt args fmt hpre1 hpost1".Replace("@n", "`n")

if ($content.Contains($old_proof)) {
    $content = $content.Replace($old_proof, $new_proof)
    Write-Host "Proof updated"
} else {
    Write-Host "Proof NOT FOUND"
}

[System.IO.File]::WriteAllText($file, $content, [System.Text.Encoding]::UTF8)
Write-Host "Done!"