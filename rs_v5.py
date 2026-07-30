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

# Find all function boundaries
funcs_info = []
for fn in ["render_latex_internal","render_python_internal","render_dsl_internal","render_ascii_internal"]:
    for i,l in enumerate(lines):
        if l.strip().startswith("static int "+fn+"("):
            fe = find_func_end(lines,i)
            ss,se = find_switch(lines,i,fe)
            funcs_info.append((fn,i,fe,ss,se))
            print(fn+": func="+str(i)+"-"+str(fe)+" switch="+str(ss)+"-"+str(se))
            break

# Process bottom to top
for fname,fs,fe,ss,se in reversed(funcs_info):
    print("\n=== "+fname+" ===")
    
    # Extract switch body
    sbody = lines[ss+1:se]
    
    # Find case boundaries using brace-depth tracking
    # For each case, track: names, body_start_idx, body_end_idx (in sbody)
    i = 0
    case_groups = []
    while i < len(sbody):
        m = re.match(r"\s*(case\s+(\w+)|default)\s*:", sbody[i])
        if not m:
            i += 1
            continue
        
        names = [m.group(2) if m.group(2) else "__DEFAULT__"]
        is_def = m.group(1) == "default"
        
        # Skip fall-through cases
        body_start = i + 1
        while body_start < len(sbody):
            m2 = re.match(r"\s*case\s+\w+\s*:", sbody[body_start])
            if m2:
                names.append(m2.group(1))
                body_start += 1
            else:
                break
        
        # Now find where this case ends (next case label or default)
        # Count brace depth to find the actual end
        body_end = body_start
        depth = 0
        # Check if body starts with {
        has_outer_brace = body_start < len(sbody) and "{" in sbody[body_start]
        if has_outer_brace:
            # Find the opening brace position
            line = sbody[body_start]
            brace_pos = line.index("{")
            # Track depth from this position
            # Start depth counting from the {
            for c in range(brace_pos, len(line)):
                if line[c] == "{": depth += 1
                elif line[c] == "}": depth -= 1
            body_end = body_start + 1
            while body_end < len(sbody) and depth > 0:
                for ch in sbody[body_end]:
                    if ch == "{": depth += 1
                    elif ch == "}": depth -= 1
                body_end += 1
            # body_end now points to the line AFTER the closing brace
        else:
            # No outer brace - body ends at next case/break/default pattern
            body_end = body_start
            while body_end < len(sbody):
                stripped = sbody[body_end].strip()
                if re.match(r"(case\s+\w+|default)\s*:", stripped):
                    break
                if stripped == "break;" or stripped == "break":
                    body_end += 1
                    break
                body_end += 1
        
        # Also ensure we don't go past the switch end
        if body_end > len(sbody):
            body_end = len(sbody)
        
        case_groups.append({
            "names": names,
            "start": body_start,
            "end": body_end,
            "has_outer_brace": has_outer_brace,
            "is_def": is_def
        })
        
        i = body_end
    
    print("  "+str(len(case_groups))+" case groups")
    for cg in case_groups:
        print("    "+str(cg["names"])+" lines "+str(cg["start"])+"-"+str(cg["end"])+" brace="+str(cg["has_outer_brace"]))

