import re, os

FILE = r'c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\core\src\layer1_parser\formula_converter.c'
with open(FILE, 'r', encoding='utf-8') as f:
    content = f.read()
L = content.splitlines(True)

# Find matching closing brace for the switch at line 2350 (idx 2349)
switch_line = 2349
depth = 0
for idx in range(switch_line, len(L)):
    depth += L[idx].count('{') - L[idx].count('}')
    if depth == 0 and idx > switch_line:
        print(f'Switch 4 closing brace at line {idx+1}')
        break

# Now let me find all case boundaries
switch4_start = 2349
switch4_end = idx  # closing brace of switch

# Track brace depth from switch level
depth = 0
cases = []
current_case = None
current_start = None

for idx2 in range(switch4_start, switch4_end + 1):
    raw = L[idx2]
    brace_delta = raw.count('{') - raw.count('}')
    
    # Check for case label at the beginning of line
    # (case labels are at switch body depth, which is depth 0 from switch)
    stripped = raw.strip()
    m = re.match(r'(case\s+\S+|default)\s*:\s*(\{)?', stripped)
    
    if m and depth <= 1:
        if current_case is not None and current_start is not None:
            cases.append((current_case, current_start, idx2 - 1))
        current_case = m.group(1)
        current_start = idx2
    
    depth += brace_delta
    
    if depth == 0 and idx2 > switch4_start:
        if current_case is not None and current_start is not None:
            cases.append((current_case, current_start, idx2 - 1))
        break

print(f'\nSwitch 4 cases ({len(cases)}):')
for name, s, e in cases:
    code = ''.join(L[s:e+1])
    print(f'  [{name}] lines {s+1}-{e+1}')

# Also show the bounds
print(f'\nSwitch body: lines {switch4_start+1}-{switch4_end+1}')
