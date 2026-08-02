import re

with open('lvFormal/Theory/LvDSL.lean', 'r') as f:
    content = f.read()

# Fix 1: Replace all `simp [lv_type_check]` with `native_decide` for simple closed lemmas
# These are the simple type_check lemmas that don't have hypotheses
replacements = [
    # type_check_intLit
    ('lemma type_check_intLit (v : ℤ) : lv_type_check (.intLit v) .int := by\n  simp [lv_type_check]',
     'lemma type_check_intLit (v : ℤ) : lv_type_check (.intLit v) .int := by\n  native_decide'),
    # type_check_floatLit
    ('lemma type_check_floatLit (v : ℝ) : lv_type_check (.floatLit v) .real := by\n  simp [lv_type_check]',
     'lemma type_check_floatLit (v : ℝ) : lv_type_check (.floatLit v) .real := by\n  native_decide'),
    # type_check_boolLit
    ('lemma type_check_boolLit (v : Bool) : lv_type_check (.boolLit v) .bool := by\n  simp [lv_type_check]',
     'lemma type_check_boolLit (v : Bool) : lv_type_check (.boolLit v) .bool := by\n  native_decide'),
    # type_check_none
    ('lemma type_check_none (t : LvType) : lv_type_check (.none t) (.option t) := by\n  simp [lv_type_check]',
     'lemma type_check_none (t : LvType) : lv_type_check (.none t) (.option t) := by\n  native_decide'),
    # Example intLit
    ('example : lv_type_check (.intLit 42) .int := by\n  simp [lv_type_check]',
     'example : lv_type_check (.intLit 42) .int := by\n  native_decide'),
    # Example lambda  
    ('example : lv_type_check (.lambda "x" .int (.add (.var "x") (.intLit 1))) (.arrow .int .int) := by\n  simp [lv_type_check]',
     'example : lv_type_check (.lambda "x" .int (.add (.var "x") (.intLit 1))) (.arrow .int .int) := by\n  native_decide'),
    # Example listLit
    ('example : lv_type_check (.listLit [.intLit 1, .intLit 2, .intLit 3]) (.list .int) := by\n  simp [lv_type_check]',
     'example : lv_type_check (.listLit [.intLit 1, .intLit 2, .intLit 3]) (.list .int) := by\n  native_decide'),
    # Example pair
    ('example : lv_type_check (.pair (.intLit 1) (.floatLit 2.0)) (.pair .int .real) := by\n  simp [lv_type_check]',
     'example : lv_type_check (.pair (.intLit 1) (.floatLit 2.0)) (.pair .int .real) := by\n  native_decide'),
    # type_check_add_int and type_check_add_real need to handle hypotheses - use unfold approach
    # type_check_lambda, type_check_app, type_check_some - same
]

for old, new in replacements:
    if old in content:
        content = content.replace(old, new)
        print(f'OK: {old[:60]}')
    else:
        print(f'MISS: {old[:60]}')

# For proofs with hypotheses, we need to use unfold + rfl approach
# type_check_add_int, type_check_add_real, type_check_lambda, type_check_app, type_check_some
# These use `simp [lv_type_check, h1, h2]` etc. which won't work with partial.
# Let me use a different approach: `simpa [h1, h2] using (by native_decide : lv_type_check (.add e1 e2) .int)`
# But that's wrong because e1, e2 are variables.
# Better approach: use `unfold lv_type_check` or `dsimp` with `h1`, `h2`.

# Actually, let me try using `simpa [lv_type_check]` with `h1` and `h2`  
# The issue is that `simp` can't unfold `lv_type_check`. Let me try using `unfold lv_type_check` first.

# Replace type_check_add_int
old = 'lemma type_check_add_int (e1 e2 : LvExpr) (h1 : lv_type_check e1 .int) (h2 : lv_type_check e2 .int) :\n    lv_type_check (.add e1 e2) .int := by\n  simp [lv_type_check, h1, h2]'
new = 'lemma type_check_add_int (e1 e2 : LvExpr) (h1 : lv_type_check e1 .int) (h2 : lv_type_check e2 .int) :\n    lv_type_check (.add e1 e2) .int := by\n  unfold lv_type_check\n  simp [h1, h2]'
if old in content:
    content = content.replace(old, new)
    print(f'OK: add_int')
else:
    print(f'MISS: add_int')

# Replace type_check_add_real
old = 'lemma type_check_add_real (e1 e2 : LvExpr) (h1 : lv_type_check e1 .real) (h2 : lv_type_check e2 .real) :\n    lv_type_check (.add e1 e2) .real := by\n  simp [lv_type_check, h1, h2]'
new = 'lemma type_check_add_real (e1 e2 : LvExpr) (h1 : lv_type_check e1 .real) (h2 : lv_type_check e2 .real) :\n    lv_type_check (.add e1 e2) .real := by\n  unfold lv_type_check\n  simp [h1, h2]'
if old in content:
    content = content.replace(old, new)
    print(f'OK: add_real')
else:
    print(f'MISS: add_real')

# Replace type_check_lambda
old = 'lemma type_check_lambda (p : String) (t codom : LvType) (b : LvExpr)\n    (h_body : lv_type_check b codom) : lv_type_check (.lambda p t b) (.arrow t codom) := by\n  simp [lv_type_check, h_body]'
new = 'lemma type_check_lambda (p : String) (t codom : LvType) (b : LvExpr)\n    (h_body : lv_type_check b codom) : lv_type_check (.lambda p t b) (.arrow t codom) := by\n  unfold lv_type_check\n  simp [h_body]'
if old in content:
    content = content.replace(old, new)
    print(f'OK: lambda')
else:
    print(f'MISS: lambda')

# Replace type_check_app
old = 'lemma type_check_app (f a : LvExpr) (dom codom : LvType)\n    (h_f : lv_type_infer f = some (.arrow dom codom))\n    (h_a : lv_type_check a dom) : lv_type_check (.app f a) codom := by\n  simp [lv_type_check, h_f, h_a]'
new = 'lemma type_check_app (f a : LvExpr) (dom codom : LvType)\n    (h_f : lv_type_infer f = some (.arrow dom codom))\n    (h_a : lv_type_check a dom) : lv_type_check (.app f a) codom := by\n  unfold lv_type_check\n  simp [h_f, h_a]'
if old in content:
    content = content.replace(old, new)
    print(f'OK: app')
else:
    print(f'MISS: app')

# Replace type_check_some
old = 'lemma type_check_some (e : LvExpr) (t : LvType) (h : lv_type_check e t) :\n    lv_type_check (.some e) (.option t) := by\n  simp [lv_type_check, h]'
new = 'lemma type_check_some (e : LvExpr) (t : LvType) (h : lv_type_check e t) :\n    lv_type_check (.some e) (.option t) := by\n  unfold lv_type_check\n  simp [h]'
if old in content:
    content = content.replace(old, new)
    print(f'OK: some')
else:
    print(f'MISS: some')

# Fix type_infer example - use native_decide
old = "example : lv_type_infer (.app (.lambda \"x\" .int (.add (.var \"x\") (.intLit 1))) (.intLit 5)) = some .int := by\n  native_decide"
# Let's try to find this in the content
import re as re2
pattern = r"example : lv_type_infer \(\.app \(\.lambda \"x\" \.int \(\.add \(\.var \"x\"\) \(\.intLit 1\)\)\) \(\.intLit 5\)\) = some \.int := by\n  native_decide"
if re2.search(pattern, content):
    print(f'OK: type_infer example found')
else:
    # Try to find the actual text
    idx = content.find('lv_type_infer (.app (.lambda')
    if idx >= 0:
        print(f'Found at {idx}: {content[idx:idx+120]}')
    else:
        print('type_infer example not found')

# Fix type_infer_check_consistent - replace all `simp [lv_type_infer]` with `unfold lv_type_infer` for the induction cases
# Actually, let me check if `unfold` works on partial functions
# For now, let me try `dsimp` instead of `simp` for the partial functions

# Fix the `forall` keyword issue in the induction
# Replace `| forall x' _ b ih =>` with `| forallS x' _ b ih =>`
content = content.replace('\n    | forall x\' _ b ih =>', '\n    | forallS x\' _ b ih =>')
print('Fixed forall keyword (if found)')

# Fix preservation_multi - use `cases` instead of `induction`
old_pm = 'theorem preservation_multi (e e\' : LvExpr) (t : LvType) (h_type : lv_type_check e t)\n    (h_steps : Steps e e\') : lv_type_check e\' t := by\n  induction h_steps with\n    | refl => exact h_type\n    | tail h_first h_rest ih =>\n      have h_mid : lv_type_check _ t := arithmetic_preservation e _ t h_type h_first\n      exact ih h_mid'
new_pm = 'theorem preservation_multi (e e\' : LvExpr) (t : LvType) (h_type : lv_type_check e t)\n    (h_steps : Steps e e\') : lv_type_check e\' t := by\n  cases h_steps with\n    | refl => exact h_type\n    | tail h_first h_rest =>\n      have h_mid : lv_type_check _ t := arithmetic_preservation e _ t h_type h_first\n      exact preservation_multi _ _ t h_mid h_rest'
if old_pm in content:
    content = content.replace(old_pm, new_pm)
    print('Fixed preservation_multi')
else:
    print('MISS: preservation_multi')
    # Debug: find the text
    idx = content.find('preservation_multi')
    if idx >= 0:
        print(f'  Found at {idx}: {content[idx:idx+200]}')

# Fix type_check_add_int_implies and type_check_add_real_implies
old = 'lemma type_check_add_int_implies (e1 e2 : LvExpr) (h : lv_type_check (.add e1 e2) .int) :\n    lv_type_check e1 .int ∧ lv_type_check e2 .int := by\n  simp [lv_type_check] at h\n  exact h'
new = 'lemma type_check_add_int_implies (e1 e2 : LvExpr) (h : lv_type_check (.add e1 e2) .int) :\n    lv_type_check e1 .int ∧ lv_type_check e2 .int := by\n  unfold lv_type_check at h\n  exact h'
if old in content:
    content = content.replace(old, new)
    print(f'OK: add_int_implies')
else:
    print(f'MISS: add_int_implies')

old = 'lemma type_check_add_real_implies (e1 e2 : LvExpr) (h : lv_type_check (.add e1 e2) .real) :\n    lv_type_check e1 .real ∧ lv_type_check e2 .real := by\n  simp [lv_type_check] at h\n  exact h'
new = 'lemma type_check_add_real_implies (e1 e2 : LvExpr) (h : lv_type_check (.add e1 e2) .real) :\n    lv_type_check e1 .real ∧ lv_type_check e2 .real := by\n  unfold lv_type_check at h\n  exact h'
if old in content:
    content = content.replace(old, new)
    print(f'OK: add_real_implies')
else:
    print(f'MISS: add_real_implies')

# Now fix the type_infer_check_consistent proof - replace all `simp` with `unfold` for lv_type_infer and lv_type_check
# The issue is that `simp [lv_type_infer]` and `simp [lv_type_check]` don't work on partial functions

# Let me check what the current content looks like around type_infer_check_consistent
idx = content.find('type_infer_check_consistent')
if idx >= 0:
    print(f'\ntype_infer_check_consistent found at {idx}')
    print(content[idx:idx+500])

with open('lvFormal/Theory/LvDSL.lean', 'w') as f:
    f.write(content)
print('\nDone')
