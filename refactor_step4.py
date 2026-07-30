import sys
fp = r"c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\core\src\layer1_parser\formula_converter.c"
with open(fp, "r", encoding="utf-8") as f:
    t = f.read()

# Remove unused variables
old_v = "static bool formula_to_graph_process_statement(const FormulaNode *stmt, ConstraintGraph *graph,\n                                               FormulaToGraphResult *result) {\n    int node_id = -1;\n    int constraint_id = -1;\n"
new_v = "static bool formula_to_graph_process_statement(const FormulaNode *stmt, ConstraintGraph *graph,\n                                               FormulaToGraphResult *result) {\n"
if old_v in t:
    t = t.replace(old_v, new_v, 1)
    print("Removed vars")
else:
    print("Vars already removed or not found")

# Replace switch
old_sw = "    switch (stmt->type) {\n        case NODE_GEOM_POINT:\n            if (formula_convert_point(stmt, graph, &node_id)) {"
if old_sw in t:
    idx_sw = t.find(old_sw)
    sc = t[idx_sw:].find("        default:\n            return false;\n    }")
    assert sc != -1
    switch_block_len = sc + len("        default:\n            return false;\n    }")
    old_full_switch = t[idx_sw:idx_sw+switch_block_len]
    new_sw = "    if ((unsigned)stmt->type < 36 && s_stmt_funcs[stmt->type]) {\n        return s_stmt_funcs[stmt->type](stmt, graph, result);\n    }\n    return false;"
    t = t[:idx_sw] + new_sw + t[idx_sw+switch_block_len:]
    print("Replaced switch")
else:
    print("Switch not found or already replaced")

with open(fp, "w", encoding="utf-8") as f:
    f.write(t)
print("DONE")
