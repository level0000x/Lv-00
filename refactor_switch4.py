import re

filepath = r'c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\core\src\layer1_parser\formula_converter.c'
with open(filepath, 'r', encoding='utf-8') as f:
    content = f.read()

# ============================================================
# Switch 4: formula_to_graph_process_statement
# ============================================================

switch4_new_funcs = '''
/* 函数指针类型：处理单个 AST 语句 */
typedef bool (*ProcessStmtFunc)(const FormulaNode *stmt, ConstraintGraph *graph, FormulaToGraphResult *result);

static bool process_stmt_geom_point(const FormulaNode *stmt, ConstraintGraph *graph, FormulaToGraphResult *result) {
    int node_id = -1;
    if (formula_convert_point(stmt, graph, &node_id)) {
        if (result->created_node_count < MAX_CREATED_NODES)
            result->created_node_ids[result->created_node_count++] = node_id;
    }
    return true;
}

static bool process_stmt_geom_segment(const FormulaNode *stmt, ConstraintGraph *graph, FormulaToGraphResult *result) {
    int node_id = -1;
    if (formula_convert_segment(stmt, graph, &node_id)) {
        if (result->created_node_count < MAX_CREATED_NODES)
            result->created_node_ids[result->created_node_count++] = node_id;
    }
    return true;
}

static bool process_stmt_geom_circle(const FormulaNode *stmt, ConstraintGraph *graph, FormulaToGraphResult *result) {
    int node_id = -1;
    if (formula_convert_circle(stmt, graph, &node_id)) {
        if (result->created_node_count < MAX_CREATED_NODES)
            result->created_node_ids[result->created_node_count++] = node_id;
    }
    return true;
}

static bool process_stmt_constraint_perpendicular(const FormulaNode *stmt, ConstraintGraph *graph, FormulaToGraphResult *result) {
    int constraint_id = -1;
    if (formula_convert_perpendicular(stmt, graph, &constraint_id)) {
        if (result->created_constraint_count < MAX_CREATED_CONSTRAINTS)
            result->created_constraint_ids[result->created_constraint_count++] = constraint_id;
    }
    return true;
}

static bool process_stmt_constraint_parallel(const FormulaNode *stmt, ConstraintGraph *graph, FormulaToGraphResult *result) {
    int constraint_id = -1;
    if (formula_convert_parallel(stmt, graph, &constraint_id)) {
        if (result->created_constraint_count < MAX_CREATED_CONSTRAINTS)
            result->created_constraint_ids[result->created_constraint_count++] = constraint_id;
    }
    return true;
}

static bool process_stmt_constraint_midpoint(const FormulaNode *stmt, ConstraintGraph *graph, FormulaToGraphResult *result) {
    int node_id = -1;
    if (formula_convert_midpoint(stmt, graph, &node_id)) {
        if (result->created_node_count < MAX_CREATED_NODES)
            result->created_node_ids[result->created_node_count++] = node_id;
    }
    return true;
}

static bool process_stmt_constraint_angle(const FormulaNode *stmt, ConstraintGraph *graph, FormulaToGraphResult *result) {
    int constraint_id = -1;
    if (formula_convert_angle(stmt, graph, &constraint_id)) {
        if (result->created_constraint_count < MAX_CREATED_CONSTRAINTS)
            result->created_constraint_ids[result->created_constraint_count++] = constraint_id;
    }
    return true;
}

static bool process_stmt_equation(const FormulaNode *stmt, ConstraintGraph *graph, FormulaToGraphResult *result) {
    int node_id = -1;
    /* 代数方程：转换为约束图中的隐式曲线 */
    if (formula_convert_equation(stmt, graph, &node_id)) {
        if (result->created_node_count < MAX_CREATED_NODES)
            result->created_node_ids[result->created_node_count++] = node_id;
    }
    return true;
}

static bool process_stmt_geom_polygon(const FormulaNode *stmt, ConstraintGraph *graph, FormulaToGraphResult *result) {
    int node_ids[FORMULA_NODE_IDS_SIZE];
    int count = 0;
    if (formula_convert_polygon(stmt, graph, node_ids, &count)) {
        if (count > FORMULA_NODE_IDS_SIZE)
            count = FORMULA_NODE_IDS_SIZE;
        for (int j = 0; j < count && result->created_node_count < MAX_CREATED_NODES; j++) {
            result->created_node_ids[result->created_node_count++] = node_ids[j];
        }
    }
    return true;
}

static bool process_stmt_geom_region(const FormulaNode *stmt, ConstraintGraph *graph, FormulaToGraphResult *result) {
    int node_id = -1;
    if (formula_convert_region(stmt, graph, &node_id)) {
        if (result->created_node_count < MAX_CREATED_NODES)
            result->created_node_ids[result->created_node_count++] = node_id;
    }
    return true;
}

static bool process_stmt_geom_arc(const FormulaNode *stmt, ConstraintGraph *graph, FormulaToGraphResult *result) {
    int node_ids[10];
    int count = 0;
    if (formula_convert_arc(stmt, graph, node_ids, &count)) {
        if (count > 10)
            count = 10;
        for (int j = 0; j < count && result->created_node_count < MAX_CREATED_NODES; j++) {
            result->created_node_ids[result->created_node_count++] = node_ids[j];
        }
    }
    return true;
}

static const ProcessStmtFunc s_stmt_funcs[] = {
    [NODE_GEOM_POINT] = process_stmt_geom_point,
    [NODE_GEOM_SEGMENT] = process_stmt_geom_segment,
    [NODE_GEOM_CIRCLE] = process_stmt_geom_circle,
    [NODE_CONSTRAINT_PERPENDICULAR] = process_stmt_constraint_perpendicular,
    [NODE_CONSTRAINT_PARALLEL] = process_stmt_constraint_parallel,
    [NODE_CONSTRAINT_MIDPOINT] = process_stmt_constraint_midpoint,
    [NODE_CONSTRAINT_ANGLE] = process_stmt_constraint_angle,
    [NODE_EQUATION] = process_stmt_equation,
    [NODE_GEOM_POLYGON] = process_stmt_geom_polygon,
    [NODE_GEOM_REGION] = process_stmt_geom_region,
    [NODE_GEOM_ARC] = process_stmt_geom_arc,
};
#define STMT_FUNC_SIZE (sizeof(s_stmt_funcs) / sizeof(s_stmt_funcs[0]))
'''

switch4_insert_point = '/**\n * @brief 处理单个 AST 语句节点，将其转换为图节点/约束并记录到结果中\n *\n * 从 formula_to_graph 中提取的公共函数，避免 NODE_COMPOUND 循环和\n * 单语句 else 分支之间的 switch 逻辑重复。\n *\n * @param stmt        当前语句的 AST 节点\n * @param graph       目标约束图\n * @param result      转换结果（用于记录创建的节点/约束 ID）\n * @return true 表示成功处理了该语句类型，false 表示未处理（未知类型）\n */\n\n/* 创建节点/约束的最大数量限制 */\n#define MAX_CREATED_NODES 256\n#define MAX_CREATED_CONSTRAINTS 64'

assert switch4_insert_point in content, "Switch 4 insert point not found!"
content = content.replace(switch4_insert_point, switch4_new_funcs + switch4_insert_point, 1)

# Now remove the unused variables and replace the switch
# The function now has node_id and constraint_id vars that are unused with the table approach
old_func_start = 'static bool formula_to_graph_process_statement(const FormulaNode *stmt, ConstraintGraph *graph,\n                                               FormulaToGraphResult *result) {\n    int node_id = -1;\n    int constraint_id = -1;\n'
new_func_start = 'static bool formula_to_graph_process_statement(const FormulaNode *stmt, ConstraintGraph *graph,\n                                               FormulaToGraphResult *result) {\n'

assert old_func_start in content, "Switch 4 function start not found!"
content = content.replace(old_func_start, new_func_start, 1)

# Replace the switch body
switch4_old_switch = '''    switch (stmt->type) {
        case NODE_GEOM_POINT:
            if (formula_convert_point(stmt, graph, &node_id)) {
                if (result->created_node_count < MAX_CREATED_NODES)
                    result->created_node_ids[result->created_node_count++] = node_id;
            }
            return true;

        case NODE_GEOM_SEGMENT:
            if (formula_convert_segment(stmt, graph, &node_id)) {
                if (result->created_node_count < MAX_CREATED_NODES)
                    result->created_node_ids[result->created_node_count++] = node_id;
            }
            return true;

        case NODE_GEOM_CIRCLE:
            if (formula_convert_circle(stmt, graph, &node_id)) {
                if (result->created_node_count < MAX_CREATED_NODES)
                    result->created_node_ids[result->created_node_count++] = node_id;
            }
            return true;

        case NODE_CONSTRAINT_PERPENDICULAR:
            if (formula_convert_perpendicular(stmt, graph, &constraint_id)) {
                if (result->created_constraint_count < MAX_CREATED_CONSTRAINTS)
                    result->created_constraint_ids[result->created_constraint_count++] = constraint_id;
            }
            return true;

        case NODE_CONSTRAINT_PARALLEL:
            if (formula_convert_parallel(stmt, graph, &constraint_id)) {
                if (result->created_constraint_count < MAX_CREATED_CONSTRAINTS)
                    result->created_constraint_ids[result->created_constraint_count++] = constraint_id;
            }
            return true;

        case NODE_CONSTRAINT_MIDPOINT:
            if (formula_convert_midpoint(stmt, graph, &node_id)) {
                if (result->created_node_count < MAX_CREATED_NODES)
                    result->created_node_ids[result->created_node_count++] = node_id;
            }
            return true;

        case NODE_CONSTRAINT_ANGLE:
            if (formula_convert_angle(stmt, graph, &constraint_id)) {
                if (result->created_constraint_count < MAX_CREATED_CONSTRAINTS)
                    result->created_constraint_ids[result->created_constraint_count++] = constraint_id;
            }
            return true;

        case NODE_EQUATION:
            /* 代数方程：转换为约束图中的隐式曲线 */
            if (formula_convert_equation(stmt, graph, &node_id)) {
                if (result->created_node_count < MAX_CREATED_NODES)
                    result->created_node_ids[result->created_node_count++] = node_id;
            }
            return true;

        case NODE_GEOM_POLYGON: {
            int node_ids[FORMULA_NODE_IDS_SIZE];
            int count = 0;
            if (formula_convert_polygon(stmt, graph, node_ids, &count)) {
                if (count > FORMULA_NODE_IDS_SIZE)
                    count = FORMULA_NODE_IDS_SIZE; /* bounds check */
                for (int j = 0; j < count && result->created_node_count < MAX_CREATED_NODES; j++) {
                    result->created_node_ids[result->created_node_count++] = node_ids[j];
                }
            }
        }
            return true;

        case NODE_GEOM_REGION:
            if (formula_convert_region(stmt, graph, &node_id)) {
                if (result->created_node_count < MAX_CREATED_NODES)
                    result->created_node_ids[result->created_node_count++] = node_id;
            }
            return true;

        case NODE_GEOM_ARC: {
            int node_ids[10];
            int count = 0;
            if (formula_convert_arc(stmt, graph, node_ids, &count)) {
                if (count > 10)
                    count = 10; /* bounds check */
                for (int j = 0; j < count && result->created_node_count < MAX_CREATED_NODES; j++) {
                    result->created_node_ids[result->created_node_count++] = node_ids[j];
                }
            }
        }
            return true;

        default:
            return false;
    }'''

switch4_new_switch = '''    if ((unsigned)stmt->type < STMT_FUNC_SIZE && s_stmt_funcs[stmt->type]) {
        return s_stmt_funcs[stmt->type](stmt, graph, result);
    }
    return false;'''

assert switch4_old_switch in content, "Switch 4 old switch not found!"
content = content.replace(switch4_old_switch, switch4_new_switch, 1)

with open(filepath, 'w', encoding='utf-8') as f:
    f.write(content)
print('Switch 4 done successfully')
