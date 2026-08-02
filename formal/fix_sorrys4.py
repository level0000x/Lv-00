import re

with open('lvFormal/Theory/LvDSL.lean', 'r') as f:
    content = f.read()

# Fix type_infer_check_consistent: replace all `simp [lv_type_infer]` with `unfold lv_type_infer` 
# and `simp [lv_type_check` with `unfold lv_type_check; simp`

# First, let me find the exact text of type_infer_check_consistent
idx = content.find('type_infer_check_consistent (e : LvExpr)')
if idx < 0:
    print('Could not find type_infer_check_consistent')
else:
    # Find the end of the proof (next lemma/example/theorem)
    rest = content[idx:]
    # Find the next lemma/example/theorem or end of section
    next_marker = len(rest)
    for marker in ['\nlemma ', '\ntheorem ', '\nexample ', '\n/-! ']:
        pos = rest.find(marker, 1)
        if pos > 0 and pos < next_marker:
            next_marker = pos
    proof_text = rest[:next_marker]
    print(f'Proof text length: {len(proof_text)}')
    print(f'First 200 chars: {proof_text[:200]}')
    print(f'Last 200 chars: {proof_text[-200:]}')
    
    # Count occurrences of simp [lv_type_infer] and simp [lv_type_check
    print(f'  simp [lv_type_infer] count: {proof_text.count("simp [lv_type_infer]")}')
    print(f'  simp [lv_type_check] count: {proof_text.count("simp [lv_type_check]")}')
    print(f'  simp [lv_type_check, count: {proof_text.count("simp [lv_type_check,")}')
    print(f'  simp [h_infer, lv_type_check] count: {proof_text.count("simp [h_infer, lv_type_check]")}')
    print(f'  simp [lv_type_check, hc1, hc2] count: {proof_text.count("simp [lv_type_check, hc1, hc2]")}')

# Now let me also check the arithmetic_preservation proof
idx2 = content.find('theorem arithmetic_preservation')
if idx2 >= 0:
    # Find the end
    rest2 = content[idx2:]
    next_marker = len(rest2)
    for marker in ['\ntheorem ', '\nlemma ', '\n/-! ']:
        if marker == '\ntheorem ':
            # Skip the first theorem (arithmetic_preservation itself)
            pos = rest2.find(marker, 1)
        else:
            pos = rest2.find(marker, 1)
        if pos > 0 and pos < next_marker:
            next_marker = pos
    ap_text = rest2[:next_marker]
    print(f'\narithmetic_preservation length: {len(ap_text)}')
    print(f'  simp [lv_type_check] count: {ap_text.count("simp [lv_type_check]")}')

with open('lvFormal/Theory/LvDSL.lean', 'w') as f:
    f.write(content)
print('\nDone checking')
