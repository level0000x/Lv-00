#!/usr/bin/env python
import os, re

def generate_message(args_str):
    """Generate a descriptive message from the condition."""
    s = args_str.strip()
    
    # Exact match checks FIRST (before stripping negation)
    if s == '!ok':
        return 'NULL input should return failure'
    elif s == 'ok':
        return 'operation should succeed'
    elif s == 'resp.success':
        return 'response should indicate success'
    elif s == '!resp.success':
        return 'response should indicate failure'
    elif s == 'cfg.include_metadata':
        return 'default config should include metadata'
    elif s == '!cfg.verbose':
        return 'default config should not be verbose'
    
    if s.startswith('!'):
        negated = True
        cond = s[1:].strip()
    else:
        negated = False
        cond = s
    
    func_match = re.match(r'^(\w+)\s*\(', cond)
    
    # domain_is_active
    if cond == 'domain_is_active(d)' and negated:
        return 'domain should not be active'
    elif cond == 'domain_is_active(d)':
        return 'domain should be active'
    elif cond == 'domain_is_active(NULL)' and negated:
        return 'NULL domain should not be active'
    elif cond == 'domain_is_active(dom)' and negated:
        return 'domain should not be active'
    elif cond.find('domain_is_active(') >= 0:
        if negated:
            return 'domain should not be active'
        return 'domain should be active'
    
    # Function-based checks (for specific functions that return bool)
    if func_match:
        fn_name = func_match.group(1)
        if negated:
            return f'{fn_name} should fail for invalid input'
        else:
            return f'{fn_name} should succeed'
    
    # Comparison patterns
    if '>=' in s or '>' in s:
        parts = re.split(r'>=?', s)
        var = parts[0].strip()
        var_short = var.rsplit('.', 1)[-1] if '.' in var else var
        if '<' in s or '<=' in s:
            return f'{var_short} should be in valid range'
        return f'{var_short} should be valid'
    
    if '==' in s:
        var = s.split('==')[0].strip()
        var_short = var.rsplit('.', 1)[-1] if '.' in var else var
        val = s.split('==')[1].strip()
        return f'{var_short} should equal {val}'
    
    if '!=' in s:
        return 'values should differ'
    
    # strstr pattern
    if 'strstr' in s:
        return 'output should contain expected content'
    
    # mpz_cmp patterns
    if 'mpz_cmp' in s:
        val_match = re.search(r'mpz_cmp_si\([^,]+,\s*(-?\d+)\)', s)
        if val_match:
            return f'result should be {val_match.group(1)}'
        return 'integer comparison should match'
    
    # fabs pattern
    if 'fabs' in s:
        return 'value should be close to expected'
    
    # is_out_of_scope
    if 'is_out_of_scope' in cond:
        if negated:
            return 'polynomial should be in scope'
        return 'polynomial should be out of scope'
    
    # Variable comparisons like "st == SOLVER_STATUS_OK"
    if cond.startswith('st '):
        val = cond.split()[-1]
        val_short = val.replace('SOLVER_STATUS_', '')
        return f'status should be {val_short}'
    
    # Generic boolean checks as last resort
    if negated:
        return f'condition should be false: {cond[:60]}'
    else:
        return f'condition should be true: {cond[:60]}'


def fix_asserts_in_text(content):
    """Fix all 1-arg TEST_ASSERT calls in the content string."""
    pattern = re.compile(r'TEST_ASSERT\(')
    changes = []
    
    for m in pattern.finditer(content):
        start_pos = m.start()
        # Find matching closing paren
        depth = 1
        i = m.end()
        while i < len(content) and depth > 0:
            if content[i] == '(':
                depth += 1
            elif content[i] == ')':
                depth -= 1
            i += 1
        end_pos = i - 1  # position of ')'
        
        args_str = content[m.end():end_pos]
        
        # Check for top-level comma
        depth = 0
        has_comma = False
        for c in args_str:
            if c == '(':
                depth += 1
            elif c == ')':
                depth -= 1
            elif c == ',' and depth == 0:
                has_comma = True
                break
        
        if not has_comma:
            line_num = content[:start_pos].count('\n') + 1
            msg = generate_message(args_str)
            changes.append((line_num, start_pos, end_pos, args_str.strip(), msg))
    
    # Apply changes from end to start to preserve positions
    changes.sort(key=lambda x: -x[1])
    
    for line_num, start_pos, end_pos, args_str, msg in changes:
        insertion = ', "' + msg + '"'
        content = content[:end_pos] + insertion + content[end_pos:]
    
    return content, changes


# Fix all files
test_dir = r'c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\test\c'
files_to_fix = ['test_output_export.c', 'test_bdd_sat_atp.c', 'test_layer5_core.c', 
                'test_layer5_output.c', 'test_proof_infra.c', 'test_solver_submodules.c']

total = 0
for fname in files_to_fix:
    fpath = os.path.join(test_dir, fname)
    if not os.path.exists(fpath):
        print(f'SKIP {fname}: file not found')
        continue
    
    with open(fpath, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read()
    
    new_content, changes = fix_asserts_in_text(content)
    
    if changes:
        with open(fpath, 'w', encoding='utf-8') as f:
            f.write(new_content)
        print(f'=== {fname}: {len(changes)} fixes ===')
        for ln, sp, ep, args, msg in changes:
            print(f'  L{ln}: TEST_ASSERT({args}) -> added "{msg}"')
        total += len(changes)
    else:
        print(f'=== {fname}: no changes ===')
    print()

print(f'Total: {total} fixes')
