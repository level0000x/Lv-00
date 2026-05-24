/**
 * @file lv00_web_bindings.c
 * @brief WASM 绑定 (v1) [已废弃]
 *
 * 【已废弃】此文件为 WASM 绑定的第一版实现，已被 lv00_web_bindings_v2.c 替代。
 * v2 版本提供了更完整的功能（区域管理、候选合并、冲突分组等）。
 * 保留此文件仅供参考，新代码请使用 v2 版本。
 * @deprecated 请使用 lv00_web_bindings_v2.c
 */

#include "lv00/lv00.h"
#include <emscripten.h>
#include <stdlib.h>
#include <string.h>

/* W4 修复：移除未使用的 js_string_to_c 函数，避免编译器警告 */

/* ==================== Graph Operations ==================== */

EMSCRIPTEN_KEEPALIVE
void* web_graph_create(void) {
    return graph_create();
}

EMSCRIPTEN_KEEPALIVE
void web_graph_destroy(void* graph) {
    if (graph) {
        graph_destroy((ConstraintGraph*)graph);
    }
}

EMSCRIPTEN_KEEPALIVE
int web_graph_add_point(void* graph, int64_t x_num, uint64_t x_den, int64_t y_num, uint64_t y_den) {
    if (!graph) return -1;

    SymbolicCoord* cx = symbolic_coord_create_rational(x_num, x_den);
    SymbolicCoord* cy = symbolic_coord_create_rational(y_num, y_den);

    if (!cx || !cy) {
        if (cx) symbolic_coord_destroy(cx);
        if (cy) symbolic_coord_destroy(cy);
        return -1;
    }

    SymbolicCoord* coords[2] = {cx, cy};
    /* W1 所有权语义说明：graph_add_point 内部通过 symbolic_coord_copy 深拷贝坐标，
     * 不接管调用方传入的 coords 所有权，因此调用方需手动释放原始坐标 */
    AddNodeResult result = graph_add_point((ConstraintGraph*)graph, coords, 2);

    symbolic_coord_destroy(cx);
    symbolic_coord_destroy(cy);

    if (result == ADD_NODE_OK) {
        return ((ConstraintGraph*)graph)->next_node_id - 1;
    }
    return -1;
}

EMSCRIPTEN_KEEPALIVE
int web_graph_add_line_segment(void* graph, int p1, int p2) {
    if (!graph) return -1;
    AddNodeResult result = graph_add_line_segment((ConstraintGraph*)graph, p1, p2);
    if (result == ADD_NODE_OK) {
        return ((ConstraintGraph*)graph)->next_node_id - 1;
    }
    return -1;
}

EMSCRIPTEN_KEEPALIVE
int web_graph_add_betweenness(void* graph, int p1, int p2, int p3) {
    if (!graph) return -1;
    AddConstraintResult result = graph_add_betweenness((ConstraintGraph*)graph, p1, p2, p3);
    if (result == ADD_CONSTRAINT_OK) return 0;
    return -1;
}

EMSCRIPTEN_KEEPALIVE
int web_graph_normalize(void* graph) {
    if (!graph) return -1;

    NormalizationResult* result = graph_normalize((ConstraintGraph*)graph, false);
    if (result) {
        int merged = result->merged_count;
        normalization_result_destroy(result);
        return merged;
    }
    return -1;
}

/* ==================== Coordinate Operations ==================== */

EMSCRIPTEN_KEEPALIVE
void* web_coord_create_rational(int64_t num, uint64_t den) {
    if (den == 0) return NULL;
    return (void*)symbolic_coord_create_rational(num, den);
}

EMSCRIPTEN_KEEPALIVE
void web_coord_destroy(void* coord) {
    if (coord) {
        symbolic_coord_destroy((SymbolicCoord*)coord);
    }
}

EMSCRIPTEN_KEEPALIVE
char* web_coord_serialize(void* coord) {
    if (!coord) return NULL;
    return symbolic_coord_serialize((SymbolicCoord*)coord);
}

EMSCRIPTEN_KEEPALIVE
void web_free_string(char* str) {
    if (str) {
        free(str);
    }
}

/* ==================== Graph Query ==================== */

EMSCRIPTEN_KEEPALIVE
int web_graph_get_node_count(void* graph) {
    if (!graph) return 0;
    return ((ConstraintGraph*)graph)->node_count;
}

EMSCRIPTEN_KEEPALIVE
int web_graph_get_constraint_count(void* graph) {
    if (!graph) return 0;
    return ((ConstraintGraph*)graph)->constraint_count;
}

/* ==================== Point Query ==================== */

typedef struct {
    int id;
    int type;
    double x;
    double y;
} WebPointInfo;

EMSCRIPTEN_KEEPALIVE
int web_get_points(void* graph, WebPointInfo* out_points, int max_points) {
    if (!graph || !out_points || max_points <= 0) return 0;

    ConstraintGraph* g = (ConstraintGraph*)graph;
    int count = 0;

    for (int i = 0; i < g->node_count && count < max_points; i++) {
        GeomNode* node = g->nodes[i];
        if (!node) continue;

        if (node->type == GEOM_POINT || node->type == GEOM_PORT || node->type == GEOM_FUNCTION_BLOCK) {
            out_points[count].id = node->id;
            out_points[count].type = (int)node->type;

            /* Convert symbolic coordinates to double for rendering */
            if (node->symbolic_coords && node->coord_count >= 2) {
                out_points[count].x = symbolic_coord_to_double(node->symbolic_coords[0]);
                out_points[count].y = symbolic_coord_to_double(node->symbolic_coords[1]);
            } else {
                out_points[count].x = 0.0;
                out_points[count].y = 0.0;
            }

            count++;
        }
    }

    return count;
}

/* ==================== Utility ==================== */

EMSCRIPTEN_KEEPALIVE
const char* web_get_version(void) {
    /* W5 修复：使用 LV00_VERSION_STRING 宏，避免硬编码版本号 */
    return LV00_VERSION_STRING;
}

EMSCRIPTEN_KEEPALIVE
int web_memory_usage(void) {
    /* Return approximate memory usage */
    return 0; /* Placeholder */
}
/* ==================== Formula Operations ==================== */

EMSCRIPTEN_KEEPALIVE
int web_formula_detect_syntax(const char* input) {
    if (!input) return 0; /* SYNTAX_AUTO */
    return (int)formula_detect_syntax(input);
}

EMSCRIPTEN_KEEPALIVE
void* web_formula_parse(const char* input, int syntax) {
    if (!input) return NULL;
    return (void*)formula_parse(input, (SyntaxType)syntax);
}

EMSCRIPTEN_KEEPALIVE
void web_parse_result_destroy(void* result) {
    if (result) {
        parse_result_destroy((ParseResult*)result);
    }
}

EMSCRIPTEN_KEEPALIVE
void* web_parse_result_get_ast(void* result) {
    if (!result) return NULL;
    return (void*)((ParseResult*)result)->ast;
}

EMSCRIPTEN_KEEPALIVE
int web_parse_result_success(void* result) {
    if (!result) return 0;
    return ((ParseResult*)result)->success ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
const char* web_parse_result_get_error(void* result) {
    if (!result) return NULL;
    return ((ParseResult*)result)->error_message;
}

EMSCRIPTEN_KEEPALIVE
void web_formula_node_destroy(void* node) {
    if (node) {
        formula_node_destroy((FormulaNode*)node);
    }
}

EMSCRIPTEN_KEEPALIVE
char* web_formula_render(void* ast, int format) {
    if (!ast) return NULL;
    return formula_render((FormulaNode*)ast, (OutputFormat)format);
}

EMSCRIPTEN_KEEPALIVE
void* web_formula_to_graph(void* ast, void* graph) {
    if (!ast || !graph) return NULL;
    return (void*)formula_to_graph((FormulaNode*)ast, (ConstraintGraph*)graph);
}

EMSCRIPTEN_KEEPALIVE
void web_formula_to_graph_result_destroy(void* result) {
    if (result) {
        formula_to_graph_result_destroy((FormulaToGraphResult*)result);
    }
}

EMSCRIPTEN_KEEPALIVE
int web_formula_to_graph_success(void* result) {
    if (!result) return 0;
    return ((FormulaToGraphResult*)result)->success ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int web_formula_to_graph_node_count(void* result) {
    if (!result) return 0;
    return ((FormulaToGraphResult*)result)->created_node_count;
}

EMSCRIPTEN_KEEPALIVE
void* web_graph_to_formula(void* graph) {
    if (!graph) return NULL;
    return (void*)graph_to_formula((ConstraintGraph*)graph);
}

EMSCRIPTEN_KEEPALIVE
void web_graph_to_formula_result_destroy(void* result) {
    if (result) {
        graph_to_formula_result_destroy((GraphToFormulaResult*)result);
    }
}

EMSCRIPTEN_KEEPALIVE
const char* web_graph_to_formula_latex(void* result) {
    if (!result) return NULL;
    return ((GraphToFormulaResult*)result)->latex_output;
}

EMSCRIPTEN_KEEPALIVE
const char* web_graph_to_formula_python(void* result) {
    if (!result) return NULL;
    return ((GraphToFormulaResult*)result)->python_output;
}

EMSCRIPTEN_KEEPALIVE
const char* web_graph_to_formula_dsl(void* result) {
    if (!result) return NULL;
    return ((GraphToFormulaResult*)result)->dsl_output;
}