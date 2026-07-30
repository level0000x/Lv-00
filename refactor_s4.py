import sys
fp = r"c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\core\src\layer1_parser\formula_converter.c"
with open(fp, "r", encoding="utf-8") as f:
    t = f.read()

# Insert function pointer table before function
ins = "typedef bool (*ProcessStmtFunc)"
if ins not in t:
    marker = "static bool formula_to_graph_process_statement"
    idx = t.find(marker)
    assert idx != -1, "marker not found"
    funcs = """
/* 函数指针类型：process_statement 分派 */
typedef bool (*ProcessStmtFunc)(const FormulaNode *stmt, ConstraintGraph *graph, FormulaToGraphResult *result);

static bool pstmt_p(const FormulaNode *s, ConstraintGraph *g, FormulaToGraphResult *r) {
    int nid = -1;
    if (formula_convert_point(s, g, &nid)) { if (r->created_node_count < 256) r->created_node_ids[r->created_node_count++] = nid; }
    return true; }
static bool pstmt_s(const FormulaNode *s, ConstraintGraph *g, FormulaToGraphResult *r) {
    int nid = -1;
    if (formula_convert_segment(s, g, &nid)) { if (r->created_node_count < 256) r->created_node_ids[r->created_node_count++] = nid; }
    return true; }
static bool pstmt_c(const FormulaNode *s, ConstraintGraph *g, FormulaToGraphResult *r) {
    int nid = -1;
    if (formula_convert_circle(s, g, &nid)) { if (r->created_node_count < 256) r->created_node_ids[r->created_node_count++] = nid; }
    return true; }
static bool pstmt_perp(const FormulaNode *s, ConstraintGraph *g, FormulaToGraphResult *r) {
    int cid = -1;
    if (formula_convert_perpendicular(s, g, &cid)) { if (r->created_constraint_count < 64) r->created_constraint_ids[r->created_constraint_count++] = cid; }
    return true; }
static bool pstmt_par(const FormulaNode *s, ConstraintGraph *g, FormulaToGraphResult *r) {
    int cid = -1;
    if (formula_convert_parallel(s, g, &cid)) { if (r->created_constraint_count < 64) r->created_constraint_ids[r->created_constraint_count++] = cid; }
    return true; }
static bool pstmt_mid(const FormulaNode *s, ConstraintGraph *g, FormulaToGraphResult *r) {
    int nid = -1;
    if (formula_convert_midpoint(s, g, &nid)) { if (r->created_node_count < 256) r->created_node_ids[r->created_node_count++] = nid; }
    return true; }
static bool pstmt_ang(const FormulaNode *s, ConstraintGraph *g, FormulaToGraphResult *r) {
    int cid = -1;
    if (formula_convert_angle(s, g, &cid)) { if (r->created_constraint_count < 64) r->created_constraint_ids[r->created_constraint_count++] = cid; }
    return true; }
static bool pstmt_eq(const FormulaNode *s, ConstraintGraph *g, FormulaToGraphResult *r) {
    int nid = -1;
    if (formula_convert_equation(s, g, &nid)) { if (r->created_node_count < 256) r->created_node_ids[r->created_node_count++] = nid; }
    return true; }
static bool pstmt_poly(const FormulaNode *s, ConstraintGraph *g, FormulaToGraphResult *r) {
    int ids[64]; int cnt = 0;
    if (formula_convert_polygon(s, g, ids, &cnt)) {
        if (cnt > 64) cnt = 64;
        for (int j = 0; j < cnt && r->created_node_count < 256; j++) r->created_node_ids[r->created_node_count++] = ids[j];
    }
    return true; }
static bool pstmt_reg(const FormulaNode *s, ConstraintGraph *g, FormulaToGraphResult *r) {
    int nid = -1;
    if (formula_convert_region(s, g, &nid)) { if (r->created_node_count < 256) r->created_node_ids[r->created_node_count++] = nid; }
    return true; }
static bool pstmt_arc(const FormulaNode *s, ConstraintGraph *g, FormulaToGraphResult *r) {
    int ids[10]; int cnt = 0;
    if (formula_convert_arc(s, g, ids, &cnt)) {
        if (cnt > 10) cnt = 10;
        for (int j = 0; j < cnt && r->created_node_count < 256; j++) r->created_node_ids[r->created_node_count++] = ids[j];
    }
    return true; }

static const ProcessStmtFunc s_stmt_funcs[] = {
    [NODE_GEOM_POINT] = pstmt_p,
    [NODE_GEOM_SEGMENT] = pstmt_s,
    [NODE_GEOM_CIRCLE] = pstmt_c,
    [NODE_CONSTRAINT_PERPENDICULAR] = pstmt_perp,
    [NODE_CONSTRAINT_PARALLEL] = pstmt_par,
    [NODE_CONSTRAINT_MIDPOINT] = pstmt_mid,
    [NODE_CONSTRAINT_ANGLE] = pstmt_ang,
    [NODE_EQUATION] = pstmt_eq,
    [NODE_GEOM_POLYGON] = pstmt_poly,
    [NODE_GEOM_REGION] = pstmt_reg,
    [NODE_GEOM_ARC] = pstmt_arc,
};
"""
    t = t[:idx] + funcs + t[idx:]
    print("Added funcs")

# Remove node_id/constraint_id vars
old_v = "static bool formula_to_graph_process_statement(const FormulaNode *stmt, ConstraintGraph *graph,\n                                               FormulaToGraphResult *result) {\n    int node_id = -1;\n    int constraint_id = -1;\n"
new_v = "static bool formula_to_graph_process_statement(const FormulaNode *stmt, ConstraintGraph *graph,\n                                               FormulaToGraphResult *result) {\n"
if old_v in t:
    t = t.replace(old_v, new_v, 1)
    print("Removed vars")

# Replace switch
old_sw = "    switch (stmt->type) {\n        case NODE_GEOM_POINT:\n            if (formula_convert_point(stmt, graph, &node_id)) {"
if old_sw in t:
    idx_sw = t.find(old_sw)
    sc = t[idx_sw:].find("        default:\n            return false;\n    }")
    assert sc != -1, "switch end not found"
    switch_block_len = sc + len("        default:\n            return false;\n    }")
    old_full_switch = t[idx_sw:idx_sw+switch_block_len]
    new_sw = "    if ((unsigned)stmt->type < 36 && s_stmt_funcs[stmt->type]) {\n        return s_stmt_funcs[stmt->type](stmt, graph, result);\n    }\n    return false;"
    t = t[:idx_sw] + new_sw + t[idx_sw+switch_block_len:]
    print("Replaced switch")
else:
    print("Switch already replaced or not found")

with open(fp, "w", encoding="utf-8") as f:
    f.write(t)
print("DONE")
