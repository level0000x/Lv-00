import re

FILE = r"c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\core\src\layer1_parser\formula_renderer.c"

with open(FILE, 'r', encoding='utf-8') as f:
    lines = f.read().split('\n')

def find_matching_brace(lines, start_line, start_col):
    bc = 0
    for i in range(start_col, len(lines[start_line])):
        if lines[start_line][i] == '{': bc += 1
        elif lines[start_line][i] == '}':
            bc -= 1
            if bc == 0: return (start_line, i)
    for li in range(start_line + 1, len(lines)):
        for ch in lines[li]:
            if ch == '{': bc += 1
            elif ch == '}':
                bc -= 1
                if bc == 0: return (li, lines[li].index('}'))
    return None

def parse_cases(lines, ss, se):
    cases, cur = [], None
    i = ss + 1
    while i < se:
        s = lines[i].strip()
        cm = re.match(r'case\s+(\w+):', s)
        dm = re.match(r'default:', s)
        if cm:
            nm = cm.group(1)
            nxt = ''
            for j in range(i+1, min(i+3, se)):
                t = lines[j].strip()
                if t and not t.startswith('//') and not t.startswith('/*'):
                    nxt = t; break
            if re.match(r'^(case\s+\w+:|default:)', nxt):
                if cur is None: cur = {'cases': [], 'body': [], 'braces': False}
                cur['cases'].append(nm)
            else:
                if cur is not None and cur['cases']: cases.append(cur)
                cur = {'cases': [nm], 'body': [], 'braces': False}
        elif dm:
            if cur is not None and cur['cases']: cases.append(cur)
            cur = {'cases': ['__DEF__'], 'body': [], 'braces': False, 'is_def': True}
        elif cur is not None:
            cur['body'].append((i, lines[i]))
            if len(cur['body']) == 1 and '{' in s: cur['braces'] = True
        i += 1
    if cur and cur['cases']: cases.append(cur)
    return cases

def mk_helper(name, cases, body, braces):
    first = cases[0].lower()
    if first.startswith('node_'): first = first[5:]
    fname = f"{name}_{first}"
    body_lines = []
    for li, line in body:
        s = line.strip()
        if s in ('{', '}'): continue
        if s.startswith('break;'): continue
        body_lines.append(line.rstrip())
    while body_lines and body_lines[-1].strip() == '':
        body_lines.pop()
    code = [f"static int {fname}(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)", "{"]
    for b in body_lines:
        code.append(f"    {b}")
    code.append("}")
    return fname, code

# Function boundaries
FUNCS = [("render_latex_internal", 390, 951), ("render_python_internal", 961, 1386), ("render_dsl_internal", 1396, 1788), ("render_ascii_internal", 1840, 1936)]

DEF_MSGS = {
    'render_latex_internal': 'snprintf(buffer, size, "\\\\text{<unknown>}")',
    'render_python_internal': 'snprintf(buffer, size, "# <unknown>")',
    'render_dsl_internal': 'snprintf(buffer, size, "<unknown>")',
}

def proc(lines, fname, fs, fe):
    ss = se = None
    for i in range(fs, fe):
        if 'switch (node->type)' in lines[i]:
            ss = i
            ci = lines[i].index('{')
            r = find_matching_brace(lines, i, ci)
            se = r[0] if r else None
            break
    if ss is None: return None
    cases = parse_cases(lines, ss, se)
    print(f"{fname}: {len(cases)} cases")
    
    helpers, entries = [], []
    for c in cases:
        if c.get('is_def'): continue
        fn, code = mk_helper(fname, c['cases'], c['body'], c['braces'])
        helpers.extend(code); helpers.append('')
        for cn in c['cases']: entries.append((cn, fn))
    
    tbl = [f"static const RenderNodeFunc s_{fname}_funcs[] = {{"]
    for cn, fn in entries: tbl.append(f"    [{cn}] = {fn},")
    tbl.append("};")
    
    # Build new function body
    new_func = lines[fs:ss]  # function signature + null check
    new_func.append("    if ((unsigned)node->type < lv_ARRAY_SIZE(s_{0}_funcs) && s_{0}_funcs[node->type]) {{".format(fname))
    new_func.append("        return s_{0}_funcs[node->type](node, buffer, size, options);".format(fname))
    new_func.append("    }")
    if fname == "render_ascii_internal":
        new_func.append("    return render_latex_internal(node, buffer, size, options);")
    else:
        new_func.append("    return {};".format(DEF_MSGS[fname]))
    new_func.append("}")
    
    before = lines[:fs]
    after = lines[fe:]
    
    result = list(before)
    if fname == 'render_latex_internal':
        result.append("")
        result.append("/* Function pointer type for node renderers */")
        result.append("typedef int (*RenderNodeFunc)(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options);")
        result.append("")
    result.append("")
    result.append(f"/* --- {fname} helper functions --- */")
    result.append("")
    result.extend(helpers)
    result.extend(tbl)
    result.append("")
    result.extend(new_func)
    result.extend(after)
    return result

for fn, fs, fe in reversed(FUNCS):
    print(f"\nProcessing {fn}...")
    # Find actual boundaries
    act_s = None
    for i, ln in enumerate(lines):
        if re.match(rf'static int {fn}\(', ln): act_s = i; break
    print(f"  Found at line {act_s}")
    result = proc(lines, fn, act_s, None)
    if result: lines = result

with open(FILE, 'w', encoding='utf-8') as f:
    f.write('\n'.join(lines))
print("Done writing!")
