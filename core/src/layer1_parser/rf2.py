import re

FILE = r'C:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\core\src\layer1_parser\formula_renderer.c'
with open(FILE, 'r', encoding='utf-8') as f:
    lines = f.read().split('\n')

def find_matching_brace(lines, start_line, start_col):
    bc = 0
    for i in range(start_col, len(lines[start_line])):
        if lines[start_line][i] == '{': bc += 1
        elif lines[start_line][i] == '}':
            bc -= 1
            if bc == 0: return start_line
    for li in range(start_line+1, len(lines)):
        for ch in lines[li]:
            if ch == '{': bc += 1
            elif ch == '}':
                bc -= 1
                if bc == 0: return li
    return None

def find_func_end(lines, func_start):
    for i in range(func_start, min(func_start+10, len(lines))):
        if '{' in lines[i]:
            ci = lines[i].index('{')
            return find_matching_brace(lines, i, ci)
    return None

print('Finding functions...')
for fname in ['render_latex_internal', 'render_python_internal', 'render_dsl_internal', 'render_ascii_internal']:
    for i, ln in enumerate(lines):
        if re.match(rf'static int {fname}\(', ln):
            fe = find_func_end(lines, i)
            print(f'{fname}: func_start={i}, func_end={fe}')
            break
