import sys
fp = r"c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\core\src\layer1_parser\formula_converter.c"
with open(fp, "r", encoding="utf-8") as f:
    lines = f.readlines()

# ========== SWITCH 4: formula_to_graph_process_statement ==========
# Find the function start and switch
fn_start = None
switch_start = None
switch_end = None
for i, line in enumerate(lines):
    if "static bool formula_to_graph_process_statement" in line:
        fn_start = i
    if fn_start is not None and i > fn_start and "switch (stmt->type)" in line:
        switch_start = i
    if switch_start is not None and i > switch_start and "return false;" in line and i > switch_start + 5:
        # Find the closing brace of the switch
        brace_count = 0
        for j in range(switch_start, len(lines)):
            brace_count += lines[j].count("{") - lines[j].count("}")
            if brace_count <= 0 and j > switch_start:
                switch_end = j
                break
        break
        
print(f"Switch 4: fn_start={fn_start}, switch_start={switch_start}, switch_end={switch_end}")
if switch_end:
    for j in range(max(0, switch_end-3), min(len(lines), switch_end+3)):
        print(f"  Line {j}: {lines[j].rstrip()}")

