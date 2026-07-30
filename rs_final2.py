import re

FILE = "C:/Users/xingg/Desktop/\u77e5\u8bc6\u4f53\u7cfb\u5316Wiki/Lv-00/core/src/layer1_parser/formula_renderer.c"

with open(FILE, "r", encoding="utf-8") as f:
    lines = f.read().split(chr(10))

def find_matching_brace(lines, start_line, start_col):
    bc = 0
    for i in range(start_col, len(lines[start_line])):
        if lines[start_line][i] == "{": bc += 1
        elif lines[start_line][i] == "}":
            bc -= 1
            if bc == 0: return start_line
    for li in range(start_line + 1, len(lines)):
        for ch in lines[li]:
            if ch == "{": bc += 1
            elif ch == "}":
                bc -= 1
                if bc == 0: return li
    return None

def find_func_end(lines, func_start):
    for i in range(func_start, min(func_start + 10, len(lines))):
        if "{" in lines[i]:
            ci = lines[i].index("{")
            return find_matching_brace(lines, i, ci)
    return None

def find_switch(lines, fs, fe):
    for i in range(fs, fe):
        if "switch (node->type)" in lines[i]:
            ci = lines[i].index("{")
            se = find_matching_brace(lines, i, ci)
            return i, se
    return None, None

def strip_outer_braces(lines_list):
    """Remove the outermost {} wrapper and trailing break; from case body lines."""
    if not lines_list:
        return [], False
    cl = list(lines_list)
    # Remove trailing empty lines
    while cl and cl[-1].strip() == "":
        cl.pop()
    # Remove or handle trailing break;
    if cl:
        last = cl[-1].strip()
        if last == "break;":
            cl.pop()
        elif last.endswith("break;"):
            # Handle "} break;" pattern
            cl[-1] = cl[-1].rstrip().rstrip("break;").rstrip()
        elif last == "}":
            # The outer closing brace - keep it for processing
            pass
    
    # Remove leading empty lines
    while cl and cl[0].strip() == "":
        cl.pop(0)
    
    if not cl:
        return [], False
    
    has_outer = cl[0].strip().startswith("{")
    if not has_outer:
        result = []
        for line in cl:
            s = line.strip()
            if s.startswith("break;"):
                continue
            result.append(s)
        return result, False
    
    # Handle outer braces
    # The first line has { somewhere (possibly only {)
    first = cl[0]
    # Find the matching closing brace
    # But first we need to handle the case where the line has "} break;" at the end
    bi = first.index("{")
    after_brace = first[bi+1:].strip()
    cr = find_matching_brace(cl, 0, bi)
    
    result = []
    if after_brace:
        result.append(after_brace)
    for li in range(1, cr):
        result.append(cl[li].strip())
    last = cl[cr]
    ei = last.rindex("}")
    rest = last[ei+1:].strip()
    if rest and not rest.startswith("break;"):
        result.append(rest)
    
    return result, True

FUNC_CFG = [
    ("render_latex_internal", 390, 950, '\\\\text{<unknown>}'),
    ("render_python_internal", 961, 1385, '# <unknown>'),
    ("render_dsl_internal", 1396, 1787, '<unknown>'),
    ("render_ascii_internal", 1840, 1935, None),
]

for fname, fs, fe, def_val in reversed(FUNC_CFG):
    ss, se = find_switch(lines, fs, fe)
    print("\n" + fname + " switch=" + str(ss) + "-" + str(se))
    
    sbody = lines[ss+1:se]
    case_starts = []
    for i, line in enumerate(sbody):
        m = re.match(r"\s*(case\s+(\w+)|default)\s*:", line)
        if m:
            case_starts.append((i, m.group(2) if m.group(2) else "__DEFAULT__"))
    
    groups = []
    i = 0
    while i < len(case_starts):
        off, name = case_starts[i]
        names = [name]
        j = off + 1
        while j < len(sbody) and re.match(r"\s*case\s+\w+\s*:", sbody[j]):
            m = re.match(r"\s*case\s+(\w+)\s*:", sbody[j])
            if m:
                names.append(m.group(1))
                j += 1
            else:
                break
        end_off = len(sbody)
        for k in range(i + 1, len(case_starts)):
            end_off = case_starts[k][0]
            break
        groups.append((names, j, end_off))
        i += 1
        while i < len(case_starts) and case_starts[i][0] < end_off:
            i += 1
    
    helpers = []
    entries = []
    
    for names, bs, be in groups:
        if names == ["__DEFAULT__"]:
            continue
        
        first = names[0].lower()
        if first.startswith("node_"):
            first = first[5:]
        hname = fname + "_" + first
        
        body, _ = strip_outer_braces(sbody[bs:be])
        while body and body[-1].strip() == "":
            body.pop()
        
        # Fix the broken indent by computing base indent
        # The body lines come from case body within a switch(12 spaces) then function body(4 spaces)
        # We add proper indentation based on nesting level
        
        code = []
        code.append("static int " + hname + "(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options)")
        code.append("{")
        
        has_w = any("written =" in line for line in body)
        has_wd = any("int written" in line for line in body)
        
        if has_w and not has_wd:
            code.append("    int written = 0;")
        
        for line in body:
            code.append("    " + line)
        
        bt = " ".join(body)
        if has_w and "written =" in bt:
            lw = bt.rfind("written =")
            lr = bt.rfind("return ")
            if lr <= lw:
                code.append("    return written;")
        
        code.append("}")
        code.append("")
        
        helpers.extend(code)
        for nm in names:
            entries.append((nm, hname))
    
    tbl = ["static const RenderNodeFunc s_" + fname + "_funcs[] = {"]
    for nm, hn in entries:
        tbl.append("    [" + nm + "] = " + hn + ",")
    tbl.append("};")
    
    nf = []
    for i in range(fs, ss):
        nf.append(lines[i])
    nf.append("    if ((unsigned)node->type < lv_ARRAY_SIZE(s_" + fname + "_funcs) && s_" + fname + "_funcs[node->type]) {")
    nf.append("        return s_" + fname + "_funcs[node->type](node, buffer, size, options);")
    nf.append("    }")
    if def_val is None:
        nf.append("    return render_latex_internal(node, buffer, size, options);")
    else:
        nf.append('    return snprintf(buffer, size, "' + def_val + '");')
    nf.append("}")
    
    before = lines[:fs]
    after = lines[fe+1:]
    
    res = list(before)
    if fname == "render_latex_internal":
        res.append("")
        res.append("/* Function pointer type for node renderers */")
        res.append("typedef int (*RenderNodeFunc)(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options);")
        res.append("")
    
    res.append("")
    res.append("/* --- " + fname + " helper functions --- */")
    res.append("")
    res.extend(helpers)
    res.extend(tbl)
    res.append("")
    res.extend(nf)
    res.extend(after)
    
    lines = res

print("Writing " + str(len(lines)) + " lines...")
with open(FILE, "w", encoding="utf-8") as f:
    f.write(chr(10).join(lines))
print("Done!")
