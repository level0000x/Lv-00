import re

FILE = "C:/Users/xingg/Desktop/\u77e5\u8bc6\u4f53\u7cfb\u5316Wiki/Lv-00/core/src/layer1_parser/formula_renderer.c"
with open(FILE, "r", encoding="utf-8") as f:
    content = f.read()

# Fix 1: Remove "} break;" lines inside function bodies
# These appear as standalone lines with just "} break;"
content = re.sub(r"\n\s*\}\s*break;", "", content)

# Fix 2: Remove lines with just "/* NODE_... 注释 */" that are remnants of original switch comments
# Actually these comments should stay if they are within functions

# Fix 3: Fix table entries for SIN/COS/TAN to point to the tan helper
# In each renderer table, the entries for SIN, COS should point to the same helper as TAN
for renderer in ["latex", "python", "dsl", "ascii"]:
    # Replace: [NODE_UNARY_OP_SIN] = render_XXX_internal_unary_op_sin,
    # With:    [NODE_UNARY_OP_SIN] = render_XXX_internal_unary_op_tan,
    content = re.sub(
        r"(\[" + renderer + r"_internal_unary_op_sin\])",
        r"\1" + " /* will fix below */",
        content
    )

# Actually, let me just fix all three entries at once more carefully
# Find and fix the tables
lines = content.split(chr(10))
for i, l in enumerate(lines):
    # Fix table entries for all renderers
    m = re.match(r"\s+\[NODE_UNARY_OP_SIN\]\s*=\s*(\w+),", l)
    if m:
        fn = m.group(1)
        # Determine the correct helper name (the TAN helper)
        tan_name = fn.replace("_sin", "_tan")
        lines[i] = "    [NODE_UNARY_OP_SIN] = " + tan_name + ","
    m = re.match(r"\s+\[NODE_UNARY_OP_COS\]\s*=\s*(\w+),", l)
    if m:
        fn = m.group(1)
        tan_name = fn.replace("_cos", "_tan")
        lines[i] = "    [NODE_UNARY_OP_COS] = " + tan_name + ","

# Fix 4: Remove any empty helper functions that might remain
# Find functions that have empty body (just { })
i = 0
while i < len(lines):
    l = lines[i].strip()
    if l.startswith("static int ") and l.endswith("("):
        # Function declaration - check if next two lines are { and }
        if i + 2 < len(lines) and lines[i+1].strip() == "{" and lines[i+2].strip() == "}":
            # Empty function - remove it
            del lines[i:i+3]
            continue
        # Function declaration - check if next line is just { and line after that is }
        if i + 2 < len(lines):
            combined = lines[i+1].strip() + " " + lines[i+2].strip()
            if lines[i+1].strip() == "{" and lines[i+2].strip() == "}":
                del lines[i:i+3]
                continue
    i += 1

# Fix 5: For DSL table, the SIN/COS entries may now point to unary_op_tan
# Check that these exist
func_names = set()
for l in lines:
    m = re.match(r"static int (\w+)\(const FormulaNode", l)
    if m:
        func_names.add(m.group(1))

# Fix 6: Remove orphaned comment lines that are between functions
# A comment line like "/* NODE_... 注释 */" right before "}" should be removed
# Actually, these should be fine - comments don't cause compilation errors

print("Writing", len(lines), "lines...")
with open(FILE, "w", encoding="utf-8") as f:
    f.write(chr(10).join(lines))
print("Done!")
