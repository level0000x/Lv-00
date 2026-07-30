import re

FILE = "C:/Users/xingg/Desktop/\u77e5\u8bc6\u4f53\u7cfb\u5316Wiki/Lv-00/core/src/layer1_parser/formula_renderer.c"
with open(FILE, "r", encoding="utf-8") as f:
    content = f.read()
lines = content.split(chr(10))

# Fix 1: Remove "} break;" lines - they are remnants of case block endings
# Pattern: line with just "    } break;" inside a function body
new_lines = []
skip_next = False
for i, l in enumerate(lines):
    if skip_next:
        skip_next = False
        continue
    # Remove "} break;" as standalone line
    if l.strip() == "} break;":
        continue
    # Remove lines "    } break;" 
    if l.strip().endswith("} break;") and l.strip() != "} break;":
        # This has content before } break; - keep everything before }
        idx = l.rindex("}")
        before = l[:idx].rstrip()
        if before:
            new_lines.append(before)
        continue
    new_lines.append(l)
lines = new_lines

# Fix 2: Add forward declarations for all 4 internal render functions
# Find the first helper function for each renderer and add forward decl before it
for fn in ["render_latex_internal", "render_python_internal", "render_dsl_internal", "render_ascii_internal"]:
    for i, l in enumerate(lines):
        # Find the first helper function (which calls the main function)
        if l.strip().startswith("static int " + fn + "_") and "(" in l:
            # Check if forward declaration already exists
            has_fwd = False
            for j in range(max(0,i-20), i):
                if fn + "(const FormulaNode" in lines[j]:
                    has_fwd = True
                    break
            if not has_fwd:
                insert = "static int " + fn + "(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options);"
                lines.insert(i, insert)
                print("Added forward decl for " + fn + " at line " + str(i))
            break

# Fix 3: Fix compound case - the "if (!stmt_buf) break;" was broken
# Look for "if (!stmt_buf)" followed by nothing (break was removed)
for i, l in enumerate(lines):
    if "if (!stmt_buf)" in l and i+1 < len(lines):
        nxt = lines[i+1].strip()
        if nxt == "" or nxt.startswith("//") or nxt.startswith("/*"):
            continue
        # Check if the if body is empty or invalid
        if nxt.startswith("int ") or nxt.startswith("lv_") or nxt.startswith("formula_"):
            # The break was stripped, need to add it back
            lines.insert(i+1, "        break;")
            print("Fixed compound break at line " + str(i+1))

# Fix 4: Fix table entries for SIN/COS/TAN - all should point to the TAN helper
for i, l in enumerate(lines):
    m = re.match(r"(\s+)\[NODE_UNARY_OP_SIN\]\s*=\s*(\w+),", l)
    if m:
        indent, func = m.group(1), m.group(2)
        tan_func = func.replace("_sin", "_tan")
        lines[i] = indent + "[NODE_UNARY_OP_SIN] = " + tan_func + ","
    m = re.match(r"(\s+)\[NODE_UNARY_OP_COS\]\s*=\s*(\w+),", l)
    if m:
        indent, func = m.group(1), m.group(2)
        tan_func = func.replace("_cos", "_tan")
        lines[i] = indent + "[NODE_UNARY_OP_COS] = " + tan_func + ","

print("Writing " + str(len(lines)) + " lines...")
with open(FILE, "w", encoding="utf-8") as f:
    f.write(chr(10).join(lines))
print("Done!")
