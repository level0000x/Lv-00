import sys
fp = r"c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\core\src\layer1_parser\formula_converter.c"
with open(fp, "r", encoding="utf-8") as f:
    t = f.read()
marker = "static bool formula_to_graph_process_statement"
idx = t.find(marker)
assert idx != -1
print("Marker at", idx)
print("Has typedef:", "ProcessStmtFunc" in t)
print("Has funcs:", "pstmt_p" in t)
