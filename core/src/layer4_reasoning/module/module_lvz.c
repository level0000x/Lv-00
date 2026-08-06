/**
 * @file module_lvz.c
 * @brief .lvz 词法/语法解析器
 *
 * @details 拆分子模块（Lv-00 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/module.h"
#include "lv/module_internal.h"
#include "lv/lv_file.h"
#include "lv/lv_path.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "module_helpers.h"

#include "lv/preset_blocks.h" /* 用于 preset 注册 */

/* graph_index.c 实现：按约束类型分发到 typed graph_add_*（收敛三处平行分发） */
AddConstraintResult graph_add_constraint_dispatch(ConstraintGraph *graph, ConstraintType type,
                                                  const int *participants, int count, double numeric_value);

/* LVZ 格式版本（与 module.c 保持一致） */
#ifndef LVZ_VERSION_MAJOR
#define LVZ_VERSION_MAJOR 1
#endif
#ifndef LVZ_VERSION_MINOR
#define LVZ_VERSION_MINOR 0
#endif

void lvz_lexer_init(LvzLexer *lex, const char *source) {
    lv_lexer_init(lex, source);
}

void lvz_lexer_skip_whitespace_and_comments(LvzLexer *lex) {
    lv_lexer_skip_whitespace_and_comments(lex);
}

LvzToken lvz_lexer_next_token(LvzLexer *lex) {
    LvzToken tok = {0};
    tok.line = lex->line;
    tok.col = lex->col;

    lvz_lexer_skip_whitespace_and_comments(lex);

    if (!*lex->pos) {
        tok.type = TOK_EOF;
        return tok;
    }

    /* 大括号 */
    if (*lex->pos == '{') {
        tok.type = TOK_LBRACE;
        lex->pos++;
        lex->col++;
        return tok;
    }

    if (*lex->pos == '}') {
        tok.type = TOK_RBRACE;
        lex->pos++;
        lex->col++;
        return tok;
    }

    /* 字符串字面量 */
    if (*lex->pos == '"') {
        lex->pos++; /* 跳过开引号 */
        lex->col++;

        tok.str_value = lv_lexer_extract_string(lex);
        if (!tok.str_value) {
            tok.type = TOK_ERROR;
            lex->error_msg = "字符串字面量解析失败";
            return tok;
        }

        tok.type = TOK_STRING;
        return tok;
    }

    /* 数字 (整数或浮点数) */
    if (isdigit((unsigned char) *lex->pos) || (*lex->pos == '-' && isdigit((unsigned char) *(lex->pos + 1)))) {
        const char *start = lex->pos;
        int sign = 1;

        if (*lex->pos == '-') {
            sign = -1;
            lex->pos++;
            lex->col++;
        }

        double value = 0;
        while (*lex->pos && isdigit((unsigned char) *lex->pos)) {
            value = value * 10 + (*lex->pos - '0');
            lex->pos++;
            lex->col++;
        }

        /* 处理小数部分 */
        if (*lex->pos == '.') {
            lex->pos++;
            lex->col++;
            double frac = 0.1;
            while (*lex->pos && isdigit((unsigned char) *lex->pos)) {
                value += (*lex->pos - '0') * frac;
                frac *= 0.1;
                lex->pos++;
                lex->col++;
            }
        }

        tok.type = TOK_NUMBER;
        tok.num_value = sign * value;
        return tok;
    }

    /* 标识符 */
    if (isalpha((unsigned char) *lex->pos) || *lex->pos == '_') {
        const char *start = lex->pos;

        while (*lex->pos &&
               (isalnum((unsigned char) *lex->pos) || *lex->pos == '_' || *lex->pos == '-' || *lex->pos == '.')) {
            lex->pos++;
            lex->col++;
        }

        size_t len = lex->pos - start;
        tok.str_value = lv_malloc(len + 1);
        if (!tok.str_value) {
            tok.type = TOK_ERROR;
            return tok;
        }

        memcpy(tok.str_value, start, len);
        tok.str_value[len] = '\0';
        tok.type = TOK_IDENTIFIER;

        return tok;
    }

    /* 未知字符 */
    tok.type = TOK_ERROR;
    lex->error_msg = "意外的字符";
    lex->pos++;
    lex->col++;

    return tok;
}

void lvz_token_free(LvzToken *tok) {
    if (tok->str_value) {
        lv_free((void **) &tok->str_value);
        tok->str_value = NULL;
    }
}

/* ============== 递归下降解析器 ============== */

/* LvzParser 类型定义已提取至 module_helpers.h */

void lvz_parser_init(LvzParser *p, const char *source) {
    lvz_lexer_init(&p->lexer, source);
    p->has_error = false;
    p->module_dir = NULL;
    memset(&p->current, 0, sizeof(LvzToken));
}

void lvz_parser_cleanup(LvzParser *p) {
    lvz_token_free(&p->current);
    if (p->module_dir) {
        lv_free((void **) &p->module_dir);
        p->module_dir = NULL;
    }
}

void lvz_parser_advance(LvzParser *p) {
    lvz_token_free(&p->current);
    p->current = lvz_lexer_next_token(&p->lexer);
}

bool lvz_parser_expect(LvzParser *p, LvzTokenType type) {
    if (p->current.type != type) {
        p->has_error = true;
        lv_RETURN_ERROR_BOOL(lv_ERROR_PARSE, "解析错误 (行 %d, 列 %d): 期望 token 类型 %d, 得到 %d",
                             p->current.line, p->current.col, type, p->current.type);
    }
    return true;
}

bool lvz_parser_expect_identifier(LvzParser *p, const char *name) {
    if (p->current.type != TOK_IDENTIFIER || strcmp(p->current.str_value, name) != 0) {
        p->has_error = true;
        lv_RETURN_ERROR_BOOL(lv_ERROR_PARSE, "解析错误 (行 %d, 列 %d): 期望关键字 '%s'", p->current.line,
                             p->current.col, name);
    }
    return true;
}

bool lvz_parser_expect_number(LvzParser *p, int *value) {
    if (p->current.type != TOK_NUMBER) {
        p->has_error = true;
        lv_RETURN_ERROR_BOOL(lv_ERROR_PARSE, "解析错误 (行 %d, 列 %d): 期望数字", p->current.line, p->current.col);
    }
    if (value)
        *value = (int) p->current.num_value;
    return true;
}

bool lvz_parser_expect_string(LvzParser *p, char **out) {
    if (p->current.type != TOK_STRING) {
        p->has_error = true;
        lv_RETURN_ERROR_BOOL(lv_ERROR_PARSE, "解析错误 (行 %d, 列 %d): 期望字符串", p->current.line, p->current.col);
    }
    if (out)
        *out = lv_strdup_safe(p->current.str_value);
    return true;
}

/* 前向声明 */
static bool lvz_parse_module_section(LvzParser *p, Module *mod);
static bool lvz_parse_deps_section(LvzParser *p, Module *mod);
static bool lvz_parse_exports_section(LvzParser *p, Module *mod);
static bool lvz_parse_axioms_section(LvzParser *p, Module *mod);
static bool lvz_parse_nodes_section(LvzParser *p, Module *mod);
static bool lvz_parse_constraints_section(LvzParser *p, Module *mod);
static bool lvz_parse_func_blocks_section(LvzParser *p, Module *mod);
static bool lvz_parse_presets_section(LvzParser *p, Module *mod);

/* 解析模块声明: module "name" "version" */
static bool lvz_parse_module_decl(LvzParser *p, Module *mod) {
    lvz_parser_advance(p); /* 跳过 'module' */

    /* 期望模块名 (字符串) */
    if (!lvz_parser_expect(p, TOK_STRING))
        return false;
    lv_free((void **) &mod->name);
    mod->name = lv_strdup_safe(p->current.str_value);
    lvz_parser_advance(p);

    /* 期望版本 (字符串) */
    if (!lvz_parser_expect(p, TOK_STRING))
        return false;
    lv_free((void **) &mod->version);
    mod->version = lv_strdup_safe(p->current.str_value);
    lvz_parser_advance(p);

    return true;
}

/* 解析依赖声明: dep "name" "version_constraint" */
static bool lvz_parse_dep(LvzParser *p, Module *mod) {
    lvz_parser_advance(p); /* 跳过 'dep' */

    /* 期望依赖名 (字符串) */
    if (!lvz_parser_expect(p, TOK_STRING))
        return false;
    char *dep_name = lv_strdup_safe(p->current.str_value);
    lvz_parser_advance(p);

    /* 期望版本约束 (字符串) */
    if (!lvz_parser_expect(p, TOK_STRING)) {
        lv_free((void **) &dep_name);
        return false;
    }
    char *version_constraint = lv_strdup_safe(p->current.str_value);
    lvz_parser_advance(p);

    /* 添加依赖 */
    bool result = module_add_dependency(mod, dep_name, version_constraint);

    lv_free((void **) &dep_name);
    lv_free((void **) &version_constraint);
    return result;
}

/* 解析依赖部分: deps N { dep ... } */
static bool lvz_parse_deps_section(LvzParser *p, Module *mod) {
    lvz_parser_advance(p); /* 跳过 'deps' */

    /* 期望依赖数量 */
    int count = 0;
    if (!lvz_parser_expect_number(p, &count))
        return false;
    lvz_parser_advance(p);

    /* 解析每个依赖 */
    for (int i = 0; i < count && !p->has_error; i++) {
        if (!lvz_parser_expect_identifier(p, "dep")) {
            lv_RETURN_ERROR_BOOL(lv_ERROR_PARSE, "解析错误 (行 %d): 期望 'dep' 关键字", p->current.line);
        }
        if (!lvz_parse_dep(p, mod))
            return false;
    }

    return true;
}

/* 解析导出部分: exports func_count type_count { func_block ... type_region ... } */
static bool lvz_parse_exports_section(LvzParser *p, Module *mod) {
    lvz_parser_advance(p); /* 跳过 'exports' */

    /* 期望函数块数量 */
    int func_count = 0;
    if (!lvz_parser_expect_number(p, &func_count))
        return false;
    lvz_parser_advance(p);

    /* 期望类型区域数量 */
    int type_count = 0;
    if (!lvz_parser_expect_number(p, &type_count))
        return false;
    lvz_parser_advance(p);

    /* 解析函数块导出 */
    for (int i = 0; i < func_count && !p->has_error; i++) {
        if (!lvz_parser_expect_identifier(p, "func_block")) {
            lv_RETURN_ERROR_BOOL(lv_ERROR_PARSE, "解析错误 (行 %d): 期望 'func_block' 关键字", p->current.line);
        }
        lvz_parser_advance(p);

        int id = 0;
        if (!lvz_parser_expect_number(p, &id))
            return false;
        module_export_function_block(mod, id);
        lvz_parser_advance(p);
    }

    /* 解析类型区域导出 */
    for (int i = 0; i < type_count && !p->has_error; i++) {
        if (!lvz_parser_expect_identifier(p, "type_region")) {
            lv_RETURN_ERROR_BOOL(lv_ERROR_PARSE, "解析错误 (行 %d): 期望 'type_region' 关键字", p->current.line);
        }
        lvz_parser_advance(p);

        int id = 0;
        if (!lvz_parser_expect_number(p, &id))
            return false;
        module_export_type_region(mod, id);
        lvz_parser_advance(p);
    }

    return true;
}

/* 解析公理部分: axioms N { axiom ... } */
static bool lvz_parse_axioms_section(LvzParser *p, Module *mod) {
    lvz_parser_advance(p); /* 跳过 'axioms' */

    /* 期望公理包数量 */
    int count = 0;
    if (!lvz_parser_expect_number(p, &count))
        return false;
    lvz_parser_advance(p);

    /* 解析每个公理包引用 */
    for (int i = 0; i < count && !p->has_error; i++) {
        if (!lvz_parser_expect_identifier(p, "axiom")) {
            lv_RETURN_ERROR_BOOL(lv_ERROR_PARSE, "解析错误 (行 %d): 期望 'axiom' 关键字", p->current.line);
        }
        lvz_parser_advance(p);

        /* 期望公理包名 (字符串) */
        if (!lvz_parser_expect(p, TOK_STRING))
            return false;

        /* 创建公理包并加载 */
        AxiomPackage *pkg = lv_axiom_package_create(p->current.str_value, "0.0.0");
        if (pkg) {
            module_add_axiom_package(mod, pkg);
        }
        lvz_parser_advance(p);
    }

    return true;
}

/* ── 节点类型分发（查找表，替代 3 分支 strcmp 链） ── */

typedef bool (*LvzNodeHandler)(LvzParser *p, Module *mod, int node_id);

static bool lvz_node_point(LvzParser *p, Module *mod, int node_id) {
    (void) node_id;
    /* 点节点: point id x y */
    double x = 0, y = 0;
    if (p->current.type == TOK_NUMBER) {
        x = p->current.num_value;
        lvz_parser_advance(p);
    }
    if (p->current.type == TOK_NUMBER) {
        y = p->current.num_value;
        lvz_parser_advance(p);
    }
    /* 创建点节点 (简化实现 - 使用有理数坐标) */
    SymbolicCoord *sx = symbolic_coord_from_double_rounded(x, 10000);
    SymbolicCoord *sy = symbolic_coord_from_double_rounded(y, 10000);
    SymbolicCoord *coords[2] = {sx, sy};
    if (sx && sy) {
        graph_add_point(mod->graph, coords, 2);
        symbolic_coord_destroy(sx);
        symbolic_coord_destroy(sy);
    }
    return true;
}

static bool lvz_node_line(LvzParser *p, Module *mod, int node_id) {
    (void) node_id;
    /* 线节点: line id p1 p2 */
    int p1 = 0, p2 = 0;
    if (p->current.type == TOK_NUMBER) {
        p1 = (int) p->current.num_value;
        lvz_parser_advance(p);
    }
    if (p->current.type == TOK_NUMBER) {
        p2 = (int) p->current.num_value;
        lvz_parser_advance(p);
    }
    /* 创建线节点 (简化实现 - 使用端点ID) */
    if (p1 >= 0 && p2 >= 0) {
        graph_add_line_segment(mod->graph, p1, p2);
    }
    return true;
}

static bool lvz_node_circle(LvzParser *p, Module *mod, int node_id) {
    /* 圆节点: circle id center radius */
    int center = 0;
    double radius = 0;
    if (p->current.type == TOK_NUMBER) {
        center = (int) p->current.num_value;
        lvz_parser_advance(p);
    }
    if (p->current.type == TOK_NUMBER) {
        radius = p->current.num_value;
        lvz_parser_advance(p);
    }

    /* 圆心节点引用必须有效 */
    if (center < 0) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_PARSE, "解析错误 (行 %d): 圆节点 #%d 的圆心节点 ID 无效 (%d)",
                             p->current.line, node_id, center);
    }
    GeomNode *center_node = graph_get_node(mod->graph, center);
    if (!center_node) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_PARSE, "解析错误 (行 %d): 圆节点 #%d 引用的圆心节点 #%d 不存在",
                             p->current.line, node_id, center);
    }

    /* 取圆心节点坐标，用于构造半径端点点 */
    double cx = 0.0, cy = 0.0;
    if (center_node->symbolic_coords && center_node->coord_count >= 1) {
        cx = symbolic_coord_to_double(center_node->symbolic_coords[0]);
    }
    if (center_node->symbolic_coords && center_node->coord_count >= 2) {
        cy = symbolic_coord_to_double(center_node->symbolic_coords[1]);
    }

    /* 创建半径端点点节点 (cx + radius, cy)，使圆心到此点的距离恰为半径 */
    SymbolicCoord *srx = symbolic_coord_from_double_rounded(cx + radius, 10000);
    SymbolicCoord *sry = symbolic_coord_from_double_rounded(cy, 10000);
    if (!srx || !sry) {
        symbolic_coord_destroy(srx);
        symbolic_coord_destroy(sry);
        lv_RETURN_ERROR_BOOL(lv_ERROR_OUT_OF_MEMORY, "解析错误 (行 %d): 无法为圆节点 #%d 构造半径端点点坐标",
                             p->current.line, node_id);
    }
    SymbolicCoord *radius_coords[2] = {srx, sry};
    AddNodeResult add_result = graph_add_point(mod->graph, radius_coords, 2);
    symbolic_coord_destroy(srx);
    symbolic_coord_destroy(sry);
    if (add_result != ADD_NODE_OK) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_PARSE, "解析错误 (行 %d): 无法创建圆节点 #%d 的半径端点点节点",
                             p->current.line, node_id);
    }

    /* 创建圆节点并关联圆心与半径端点（沿用自动分配 ID，与点/线节点一致） */
    int radius_point_id = graph_get_last_added_node_id(mod->graph);
    int circle_id = radius_point_id + 1;
    GeomNode *circle_node = graph_add_node_with_id(mod->graph, circle_id, GEOM_CIRCLE, NULL, 0);
    if (!circle_node) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_PARSE, "解析错误 (行 %d): 无法创建圆节点 #%d (节点 ID 冲突?)",
                             p->current.line, circle_id);
    }
    circle_node->data.circle.center_node_id = center;
    circle_node->data.circle.radius_node_id = radius_point_id;
    return true;
}

/** @brief 节点类型名→处理函数 查找表（替代 3 分支 strcmp 链） */
static const struct {
    const char *name;
    LvzNodeHandler handler;
} kNodeTypeHandlers[] = {
    {"point", lvz_node_point},
    {"line", lvz_node_line},
    {"circle", lvz_node_circle},
};

/* 解析节点部分: nodes N { point/line/... } */
static bool lvz_parse_nodes_section(LvzParser *p, Module *mod) {
    lvz_parser_advance(p); /* 跳过 'nodes' */

    /* 期望节点数量 */
    int count = 0;
    if (!lvz_parser_expect_number(p, &count))
        return false;
    lvz_parser_advance(p);

    /* 确保图存在 */
    if (!mod->graph) {
        mod->graph = graph_create();
        if (!mod->graph) {
            lv_RETURN_ERROR_BOOL(lv_ERROR_OUT_OF_MEMORY, "无法创建约束图");
        }
    }

    /* 解析每个节点 */
    for (int i = 0; i < count && !p->has_error; i++) {
        if (p->current.type != TOK_IDENTIFIER) {
            lv_RETURN_ERROR_BOOL(lv_ERROR_PARSE, "解析错误 (行 %d): 期望节点类型", p->current.line);
        }

        const char *node_type = p->current.str_value;
        lvz_parser_advance(p);

        /* 期望节点 ID */
        int node_id = 0;
        if (!lvz_parser_expect_number(p, &node_id))
            return false;
        lvz_parser_advance(p);

        /* 节点类型→处理函数 查表（替代 3 分支 strcmp 链） */
        bool handled = false;
        for (size_t i = 0; i < lv_ARRAY_SIZE(kNodeTypeHandlers); i++) {
            if (strcmp(node_type, kNodeTypeHandlers[i].name) == 0) {
                handled = true;
                if (!kNodeTypeHandlers[i].handler(p, mod, node_id))
                    return false;
                break;
            }
        }
        if (!handled) {
            /* 未知节点类型，跳过参数 */
            while (p->current.type == TOK_NUMBER) {
                lvz_parser_advance(p);
            }
        }
    }

    return true;
}

/* 判断约束类型是否为原生约束（原生约束直接构造，不走模板展开） */
static bool lvz_is_native_constraint_type(const char *type) {
    if (!type)
        return false;
    return strcmp(type, "incidence") == 0 || strcmp(type, "betweenness") == 0 ||
           strcmp(type, "intersection") == 0 || strcmp(type, "containment") == 0 ||
           strcmp(type, "connection") == 0 || strcmp(type, "angle") == 0;
}

/**
 * @brief 将模板展开结果图的节点与约束合并进模块主图
 *
 * 节点按 ID 复制到主图；若主图已存在同 ID 节点则分配新 ID，
 * 并通过 id_map 记录 ID 重映射关系。约束参与者 ID 经 id_map 重映射后复制。
 * 圆节点的圆心/半径端点节点引用在全部节点复制完成后统一重映射。
 *
 * @param mod      模块（主图）
 * @param expanded 模板展开结果图（归公理包展开缓存所有，只读使用，勿销毁）
 * @return true 成功；false 失败
 */
static bool lvz_merge_template_expansion(Module *mod, const ConstraintGraph *expanded) {
    if (!mod || !mod->graph || !expanded)
        return false;

    /* 统计展开图的节点 ID 范围，用于构建重映射表 */
    int max_node_id = -1;
    for (int i = 0; i < expanded->node_count; i++) {
        GeomNode *src = expanded->nodes[i];
        if (src && src->id > max_node_id)
            max_node_id = src->id;
    }

    /* id_map[src_id] = dst_id；-1 表示展开图中不存在该 ID */
    int *id_map = NULL;
    if (max_node_id >= 0) {
        id_map = lv_calloc((size_t) max_node_id + 1, sizeof(int));
        if (!id_map)
            return false;
        for (int i = 0; i <= max_node_id; i++)
            id_map[i] = -1;
    }

    /* 第一轮：复制全部节点，记录 ID 重映射 */
    for (int i = 0; i < expanded->node_count; i++) {
        GeomNode *src = expanded->nodes[i];
        if (!src)
            continue;

        /* 若主图已有同 ID 节点，寻找主图中第一个空闲节点 ID */
        int dst_id = src->id;
        if (dst_id < 0 || graph_get_node(mod->graph, dst_id)) {
            dst_id = 0;
            while (graph_get_node(mod->graph, dst_id))
                dst_id++;
        }

        GeomNode *dst = graph_add_node_with_id(mod->graph, dst_id, src->type, src->symbolic_coords, src->coord_count);
        if (!dst) {
            lv_free((void **) &id_map);
            return false;
        }

        /* 复制节点增强字段 */
        dst->trust = src->trust;
        dst->is_active = src->is_active;
        dst->lo_subtype = src->lo_subtype;
        dst->namespace_depth = src->namespace_depth;
        dst->parent_block_id = src->parent_block_id;
        dst->numeric_precision = src->numeric_precision;
        if (src->numeric_assumption_declaration) {
            dst->numeric_assumption_declaration = lv_strdup_safe(src->numeric_assumption_declaration);
        }

        /* 圆节点的圆心/半径端点引用先保持源 ID，第二轮统一重映射 */
        if (src->type == GEOM_CIRCLE) {
            dst->data.circle.center_node_id = src->data.circle.center_node_id;
            dst->data.circle.radius_node_id = src->data.circle.radius_node_id;
        }

        if (src->id >= 0 && id_map)
            id_map[src->id] = dst_id;
    }

    /* 第二轮：重映射圆节点引用的圆心/半径端点节点 ID */
    for (int i = 0; i < expanded->node_count; i++) {
        GeomNode *src = expanded->nodes[i];
        if (!src || src->type != GEOM_CIRCLE || src->id < 0)
            continue;
        int dst_id = (id_map && src->id <= max_node_id) ? id_map[src->id] : src->id;
        GeomNode *dst = dst_id >= 0 ? graph_get_node(mod->graph, dst_id) : NULL;
        if (!dst)
            continue;

        int center_id = src->data.circle.center_node_id;
        int radius_id = src->data.circle.radius_node_id;
        if (center_id >= 0 && id_map && center_id <= max_node_id && id_map[center_id] >= 0)
            dst->data.circle.center_node_id = id_map[center_id];
        else
            dst->data.circle.center_node_id = center_id;
        if (radius_id >= 0 && id_map && radius_id <= max_node_id && id_map[radius_id] >= 0)
            dst->data.circle.radius_node_id = id_map[radius_id];
        else
            dst->data.circle.radius_node_id = radius_id;
    }

    /* 第三轮：复制全部约束，参与者 ID 经 id_map 重映射 */
    for (int i = 0; i < expanded->constraint_count; i++) {
        Constraint *src = expanded->constraints[i];
        if (!src)
            continue;

        int mapped_participants[8];
        int mapped_count = src->participant_count < 8 ? src->participant_count : 8;
        for (int j = 0; j < mapped_count; j++) {
            int pid = src->participants[j];
            if (pid >= 0 && id_map && pid <= max_node_id && id_map[pid] >= 0)
                mapped_participants[j] = id_map[pid];
            else
                mapped_participants[j] = pid;
        }

        /* 若主图已有同 ID 约束，寻找第一个空闲约束 ID */
        int dst_id = src->id;
        if (dst_id < 0 || graph_get_constraint(mod->graph, dst_id)) {
            dst_id = 0;
            while (graph_get_constraint(mod->graph, dst_id))
                dst_id++;
        }

        Constraint *dst = graph_add_constraint_with_id(mod->graph, dst_id, src->type, mapped_participants, mapped_count);
        if (!dst) {
            lv_free((void **) &id_map);
            return false;
        }

        /* 复制约束增强字段 */
        dst->template_id = src->template_id;
        dst->is_active = src->is_active;
        dst->numeric_value = src->numeric_value;
        dst->satisfaction = src->satisfaction;
    }

    lv_free((void **) &id_map);
    return true;
}

/**
 * @brief 通过公理包模板展开高级约束 (distance/parallel/perpendicular/tangent 等)
 *
 * 流程：
 *   1. 在模块声明的公理包中查找匹配约束类型的模板
 *   2. 将约束参数（参与者节点坐标或数值）收集为 SymbolicCoord** 参数数组
 *   3. 调用 axiom_template_expand_lazy 惰性展开为独立约束图
 *   4. 将展开图的节点与约束合并进模块主图（处理 ID 冲突）
 *
 * @param p              解析器
 * @param mod            模块
 * @param constraint_type 约束类型名（即模板名）
 * @param params         约束参数（节点 ID / 数值）
 * @param param_count    参数数量
 * @return true 成功；false 失败（模板缺失/参数无效/展开为空/合并失败）
 */
static bool lvz_expand_constraint_template(LvzParser *p, Module *mod, const char *constraint_type, const int *params,
                                           int param_count) {
    if (!p || !mod || !constraint_type)
        return false;

    /* 原生约束类型走到此处说明参数不足，明确报错而非静默跳过 */
    if (lvz_is_native_constraint_type(constraint_type)) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_PARSE, "解析错误 (行 %d): 原生约束 '%s' 参数不足 (实际 %d 个)",
                             p->current.line, constraint_type, param_count);
    }

    /* 在模块声明的公理包中查找匹配约束类型的模板 */
    AxiomPackage *found_pkg = NULL;
    for (int i = 0; i < mod->axiom_packages.count; i++) {
        AxiomPackage **slot = (AxiomPackage **) lv_darray_get(&mod->axiom_packages, i);
        if (slot && *slot && axiom_package_get_template(*slot, constraint_type)) {
            found_pkg = *slot;
            break;
        }
    }
    if (!found_pkg) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_PARSE, "解析错误 (行 %d): 高级约束 '%s' 未在任何已声明公理包中找到对应模板",
                             p->current.line, constraint_type);
    }

    /* 收集模板参数：参与者节点展开为其全部符号坐标，数值参数转换为有理数坐标 */
    int coord_total = 0;
    for (int i = 0; i < param_count; i++) {
        GeomNode *node = graph_get_node(mod->graph, params[i]);
        if (node)
            coord_total += node->coord_count;
        else
            coord_total += 1; /* 非节点 ID（如距离数值），作为单个有理数坐标 */
    }

    SymbolicCoord **tmpl_params = NULL;
    if (coord_total > 0) {
        tmpl_params = lv_calloc((size_t) coord_total, sizeof(SymbolicCoord *));
        if (!tmpl_params) {
            lv_RETURN_ERROR_BOOL(lv_ERROR_OUT_OF_MEMORY, "解析错误 (行 %d): 无法分配模板参数数组", p->current.line);
        }
        int idx = 0;
        for (int i = 0; i < param_count; i++) {
            GeomNode *node = graph_get_node(mod->graph, params[i]);
            if (node) {
                for (int j = 0; j < node->coord_count; j++) {
                    tmpl_params[idx] = symbolic_coord_copy(node->symbolic_coords[j]);
                    idx++;
                }
            } else {
                tmpl_params[idx] = symbolic_coord_create_rational(params[i], 1);
                idx++;
            }
        }
    }

    /* 惰性展开模板为独立约束图（返回图归公理包展开缓存所有，只读使用） */
    ConstraintGraph *expanded = axiom_template_expand_lazy(found_pkg, constraint_type, tmpl_params, coord_total);

    /* 释放模板参数（深拷贝） */
    if (tmpl_params) {
        for (int i = 0; i < coord_total; i++) {
            if (tmpl_params[i])
                symbolic_coord_destroy(tmpl_params[i]);
        }
        lv_free((void **) &tmpl_params);
    }

    if (!expanded) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_PARSE, "解析错误 (行 %d): 模板 '%s' 展开失败", p->current.line, constraint_type);
    }

    /* 展开结果为空图：模板缺少可执行的展开体，属于明确错误而非静默跳过 */
    if (expanded->node_count == 0 && expanded->constraint_count == 0) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_PARSE, "解析错误 (行 %d): 模板 '%s' 展开结果为空（模板未提供可执行的展开体）",
                             p->current.line, constraint_type);
    }

    /* 将展开图的节点与约束合并进模块主图（处理 ID 冲突） */
    if (!lvz_merge_template_expansion(mod, expanded)) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_PARSE, "解析错误 (行 %d): 模板 '%s' 展开结果合并进主图失败",
                             p->current.line, constraint_type);
    }

    return true;
}

/* 解析约束部分: constraints N { constraint_type ... } */
/* ── 原生约束类型分发（查找表，替代 5 分支 strcmp 链；未命中走模板展开） ── */

typedef void (*LvzNativeConstraintFn)(Module *mod, const int *params, int param_count);

static void lvz_constraint_incidence(Module *mod, const int *params, int param_count) {
    (void) param_count;
    /* incidence point_id line_id */
    if (params[0] >= 0 && params[1] >= 0) {
        graph_add_constraint_dispatch(mod->graph, INCIDENCE, params, 2, 0.0);
    }
}

static void lvz_constraint_betweenness(Module *mod, const int *params, int param_count) {
    (void) param_count;
    /* betweenness p1 p2 p3 */
    if (params[0] >= 0 && params[1] >= 0 && params[2] >= 0) {
        graph_add_constraint_dispatch(mod->graph, BETWEENNESS, params, 3, 0.0);
    }
}

static void lvz_constraint_intersection(Module *mod, const int *params, int param_count) {
    (void) param_count;
    /* intersection line1 line2 result_point */
    if (params[0] >= 0 && params[1] >= 0 && params[2] >= 0) {
        graph_add_constraint_dispatch(mod->graph, INTERSECTION, params, 3, 0.0);
    }
}

static void lvz_constraint_containment(Module *mod, const int *params, int param_count) {
    (void) param_count;
    /* containment inner outer */
    if (params[0] >= 0 && params[1] >= 0) {
        graph_add_constraint_dispatch(mod->graph, CONTAINMENT, params, 2, 0.0);
    }
}

static void lvz_constraint_connection(Module *mod, const int *params, int param_count) {
    (void) param_count;
    /* connection src_port dst_port */
    if (params[0] >= 0 && params[1] >= 0) {
        graph_add_constraint_dispatch(mod->graph, CONNECTION, params, 2, 0.0);
    }
}

/** @brief 原生约束类型表：名称 + 最少参数数 + 构造函数（替代 5 分支 strcmp 链） */
static const struct {
    const char *name;
    int min_params;
    LvzNativeConstraintFn fn;
} kNativeConstraintTable[] = {
    {"incidence", 2, lvz_constraint_incidence},
    {"betweenness", 3, lvz_constraint_betweenness},
    {"intersection", 3, lvz_constraint_intersection},
    {"containment", 2, lvz_constraint_containment},
    {"connection", 2, lvz_constraint_connection},
};

static bool lvz_parse_constraints_section(LvzParser *p, Module *mod) {
    lvz_parser_advance(p); /* 跳过 'constraints' */

    /* 期望约束数量 */
    int count = 0;
    if (!lvz_parser_expect_number(p, &count))
        return false;
    lvz_parser_advance(p);

    /* 确保图存在 */
    if (!mod->graph) {
        mod->graph = graph_create();
        if (!mod->graph) {
            lv_RETURN_ERROR_BOOL(lv_ERROR_OUT_OF_MEMORY, "无法创建约束图");
        }
    }

    /* 解析每个约束 */
    for (int i = 0; i < count && !p->has_error; i++) {
        if (p->current.type != TOK_IDENTIFIER) {
            lv_RETURN_ERROR_BOOL(lv_ERROR_PARSE, "解析错误 (行 %d): 期望约束类型", p->current.line);
        }

        /* 复制约束类型名：lvz_parser_advance 会释放 current.str_value，
           若不复制，参数解析后的 strcmp/模板查找将读取悬垂内存 */
        char constraint_type_buf[64];
        size_t ct_len = strlen(p->current.str_value);
        if (ct_len >= sizeof(constraint_type_buf)) {
            lv_RETURN_ERROR_BOOL(lv_ERROR_PARSE, "解析错误 (行 %d): 约束类型名过长", p->current.line);
        }
        memcpy(constraint_type_buf, p->current.str_value, ct_len + 1);
        const char *constraint_type = constraint_type_buf;
        lvz_parser_advance(p);

        /* 解析约束参数 (简化实现，读取所有数字参数) */
        int params[8];
        int param_count = 0;

        while (p->current.type == TOK_NUMBER && param_count < 8) {
            params[param_count++] = (int) p->current.num_value;
            lvz_parser_advance(p);
        }

        /* 创建约束：原生约束查表（替代 5 分支 strcmp 链；未命中走模板展开） */
        bool native = false;
        for (size_t i = 0; i < lv_ARRAY_SIZE(kNativeConstraintTable); i++) {
            if (strcmp(constraint_type, kNativeConstraintTable[i].name) == 0 &&
                param_count >= kNativeConstraintTable[i].min_params) {
                kNativeConstraintTable[i].fn(mod, params, param_count);
                native = true;
                break;
            }
        }
        if (!native) {
            /* 其他约束类型 (distance, angle, parallel, perpendicular, tangent)
               是公理包定义的高级约束，通过公理包模板展开实现 */
            if (!lvz_expand_constraint_template(p, mod, constraint_type, params, param_count))
                return false;
        }
    }

    return true;
}

/* ── func_blocks 块字段分发（查找表，替代 3 分支 strcmp 链） ── */

typedef bool (*LvzIntFieldFn)(LvzParser *p, int *dst);

static bool lvz_field_int(LvzParser *p, int *dst) {
    lvz_parser_advance(p);
    if (!lvz_parser_expect_number(p, dst))
        return false;
    lvz_parser_advance(p);
    return true;
}

/** @brief func_blocks 块字段表（inputs/outputs/internal，顺序与调用方目标数组一致） */
static const struct {
    const char *name;
    LvzIntFieldFn fn;
} kFuncBlockIntFields[] = {
    {"inputs", lvz_field_int},
    {"outputs", lvz_field_int},
    {"internal", lvz_field_int},
};

/* 解析函数块部分: func_blocks N { func_block ... } */
static bool lvz_parse_func_blocks_section(LvzParser *p, Module *mod) {
    lvz_parser_advance(p); /* 跳过 'func_blocks' */

    /* 期望函数块数量 */
    int count = 0;
    if (!lvz_parser_expect_number(p, &count))
        return false;
    lvz_parser_advance(p);

    /* 解析每个函数块 */
    for (int i = 0; i < count && !p->has_error; i++) {
        if (!lvz_parser_expect_identifier(p, "func_block")) {
            lv_RETURN_ERROR_BOOL(lv_ERROR_PARSE, "解析错误 (行 %d): 期望 'func_block' 关键字", p->current.line);
        }
        lvz_parser_advance(p);

        /* 期望函数块 ID */
        int block_id = 0;
        if (!lvz_parser_expect_number(p, &block_id))
            return false;
        lvz_parser_advance(p);

        /* 期望函数名 (字符串) */
        char *func_name = NULL;
        if (!lvz_parser_expect_string(p, &func_name))
            return false;
        lvz_parser_advance(p);

        /* 解析 inputs/outputs/internal（字段查表，替代 3 分支 strcmp 链） */
        int inputs = 0, outputs = 0, internal = 0;
        int *field_dst[] = {&inputs, &outputs, &internal}; /* 顺序与 kFuncBlockIntFields 一致 */

        while (p->current.type == TOK_IDENTIFIER && !p->has_error) {
            bool matched = false;
            for (size_t fi = 0; fi < lv_ARRAY_SIZE(kFuncBlockIntFields); fi++) {
                if (strcmp(p->current.str_value, kFuncBlockIntFields[fi].name) == 0) {
                    if (!kFuncBlockIntFields[fi].fn(p, field_dst[fi]))
                        break; /* 解析失败：跳出循环（与手写链的 break 行为一致） */
                    matched = true;
                    break;
                }
            }
            if (!matched)
                break;
        }

        /* 期望 'end' */
        if (lvz_parser_expect_identifier(p, "end")) {
            lvz_parser_advance(p);
        }

        lv_free((void **) &func_name);
        (void) block_id;
        (void) inputs;
        (void) outputs;
        (void) internal;
    }

    return true;
}

/* ==================== 预设解析辅助函数 ==================== */

/**
 * @brief 将字符串类别名映射为 PresetCategory 枚举值
 */

/** @brief lvz_category_from_string 查找表（字符串 -> PresetCategory 枚举映射） */
static const struct {
    const char *name;
    PresetCategory category;
} kCategoryMap[] = {
    {"CONSTRUCTION", PRESET_CATEGORY_CONSTRUCTION},
    {"MEASUREMENT", PRESET_CATEGORY_MEASUREMENT},
    {"TRANSFORMATION", PRESET_CATEGORY_TRANSFORMATION},
    {"ALGEBRAIC", PRESET_CATEGORY_ALGEBRAIC},
    {"LOGIC", PRESET_CATEGORY_LOGIC},
    {"ANALYSIS", PRESET_CATEGORY_ANALYSIS},
    {"NUMBER_THEORY", PRESET_CATEGORY_NUMBER_THEORY},
    {"GROUP_THEORY", PRESET_CATEGORY_GROUP_THEORY},
    {"RING_THEORY", PRESET_CATEGORY_RING_THEORY},
    {"FIELD_THEORY", PRESET_CATEGORY_FIELD_THEORY},
    {"TOPOLOGY", PRESET_CATEGORY_TOPOLOGY},
    {"LINEAR_ALGEBRA", PRESET_CATEGORY_LINEAR_ALGEBRA},
    {"COMBINATORICS", PRESET_CATEGORY_COMBINATORICS},
    {"COMPLEX_ANALYSIS", PRESET_CATEGORY_COMPLEX_ANALYSIS},
    {"PROBABILITY", PRESET_CATEGORY_PROBABILITY},
    {"GEOMETRY", PRESET_CATEGORY_GEOMETRY},
    {"ALGEBRA", PRESET_CATEGORY_ALGEBRA},
    {"CATEGORY_THEORY", PRESET_CATEGORY_CATEGORY_THEORY},
    {"SET_THEORY", PRESET_CATEGORY_SET_THEORY},
    {"GRAPH_THEORY", PRESET_CATEGORY_GRAPH_THEORY},
    {"DIFFERENTIAL_GEOMETRY", PRESET_CATEGORY_DIFFERENTIAL_GEOMETRY},
    {"NUMERICAL", PRESET_CATEGORY_NUMERICAL},
    {"OPTIMIZATION", PRESET_CATEGORY_OPTIMIZATION},
    {"MATH_LOGIC", PRESET_CATEGORY_MATH_LOGIC},
};

static PresetCategory lvz_category_from_string(const char *name) {
    if (!name) return PRESET_CATEGORY_CUSTOM;
    for (size_t i = 0; i < lv_ARRAY_SIZE(kCategoryMap); i++) {
        if (strcmp(name, kCategoryMap[i].name) == 0)
            return kCategoryMap[i].category;
    }
    return PRESET_CATEGORY_CUSTOM;
}

/**
 * @brief 将字符串类型名映射为 PresetType 枚举值
 */

/** @brief lvz_type_from_string 查找表（字符串 -> PresetType 枚举映射） */
static const struct {
    const char *name;
    PresetType type;
} kTypeMap[] = {
    {"POINT", PRESET_TYPE_POINT},
    {"LINE", PRESET_TYPE_LINE},
    {"LINE_SEGMENT", PRESET_TYPE_LINE_SEGMENT},
    {"RAY", PRESET_TYPE_RAY},
    {"CIRCLE", PRESET_TYPE_CIRCLE},
    {"POLYGON", PRESET_TYPE_POLYGON},
    {"ANGLE", PRESET_TYPE_ANGLE},
    {"SCALAR", PRESET_TYPE_SCALAR},
    {"VECTOR", PRESET_TYPE_VECTOR},
    {"MATRIX", PRESET_TYPE_MATRIX},
    {"BOOLEAN", PRESET_TYPE_BOOLEAN},
    {"INTEGER", PRESET_TYPE_INTEGER},
    {"SET", PRESET_TYPE_SET},
    {"FUNCTION", PRESET_TYPE_FUNCTION},
    {"TUPLE", PRESET_TYPE_TUPLE},
    {"LIST", PRESET_TYPE_LIST},
    {"SEQUENCE", PRESET_TYPE_SEQUENCE},
    {"REGION", PRESET_TYPE_REGION},
    {"PATH", PRESET_TYPE_PATH},
    {"SURFACE", PRESET_TYPE_SURFACE},
    {"SPACE", PRESET_TYPE_SPACE},
    {"GROUP", PRESET_TYPE_GROUP},
    {"GROUP_ELEMENT", PRESET_TYPE_GROUP_ELEMENT},
    {"SUBGROUP", PRESET_TYPE_SUBGROUP},
    {"HOMOMORPHISM", PRESET_TYPE_HOMOMORPHISM},
    {"PRIME", PRESET_TYPE_PRIME},
    {"EQUATION", PRESET_TYPE_EQUATION},
    {"LIMIT", PRESET_TYPE_LIMIT},
    {"DERIVATIVE", PRESET_TYPE_DERIVATIVE},
    {"POLYNOMIAL", PRESET_TYPE_POLYNOMIAL},
    {"LIMIT_EXPRESSION", PRESET_TYPE_LIMIT_EXPRESSION},
    {"RING", PRESET_TYPE_RING},
    {"IDEAL", PRESET_TYPE_IDEAL},
    {"FIELD", PRESET_TYPE_FIELD},
    {"MODULE", PRESET_TYPE_MODULE},
    {"ALGEBRA", PRESET_TYPE_ALGEBRA},
    {"TOPOLOGY", PRESET_TYPE_TOPOLOGY},
    {"MANIFOLD", PRESET_TYPE_MANIFOLD},
    {"DISTRIBUTION", PRESET_TYPE_DISTRIBUTION},
    {"PROBABILITY", PRESET_TYPE_PROBABILITY},
    {"GRAPH", PRESET_TYPE_GRAPH},
    {"TREE", PRESET_TYPE_TREE},
    {"INTEGRAL", PRESET_TYPE_INTEGRAL},
    {"SERIES", PRESET_TYPE_SERIES},
    {"COMPLEX", PRESET_TYPE_COMPLEX},
    {"PERMUTATION", PRESET_TYPE_PERMUTATION},
    {"COSET", PRESET_TYPE_COSET},
    {"EXTENSION", PRESET_TYPE_EXTENSION},
    {"AUTOMORPHISM", PRESET_TYPE_AUTOMORPHISM},
    {"DISTANCE", PRESET_TYPE_DISTANCE},
    {"AREA", PRESET_TYPE_AREA},
    {"LENGTH", PRESET_TYPE_LENGTH},
    {"CURVATURE", PRESET_TYPE_CURVATURE},
    {"OPEN_SET", PRESET_TYPE_OPEN_SET},
    {"CLOSED_SET", PRESET_TYPE_CLOSED_SET},
    {"RESIDUE", PRESET_TYPE_RESIDUE},
    {"FORMULA", PRESET_TYPE_FORMULA},
    {"EXPRESSION", PRESET_TYPE_EXPRESSION},
    {"STRUCTURE", PRESET_TYPE_STRUCTURE},
    {"STRING", PRESET_TYPE_STRING},
    {"ANY", PRESET_TYPE_ANY},
};

static PresetType lvz_type_from_string(const char *name) {
    if (!name) return PRESET_TYPE_ANY;
    for (size_t i = 0; i < lv_ARRAY_SIZE(kTypeMap); i++) {
        if (strcmp(name, kTypeMap[i].name) == 0)
            return kTypeMap[i].type;
    }
    return PRESET_TYPE_ANY;
}

/* ==================== 预设节解析 ==================== */

/* ── preset 块字段分发（查找表，替代 8 分支 strcmp 链） ── */

/** @brief 预设体解析上下文：聚合全部输出字段指针 */
typedef struct {
    char **out_desc;
    PresetCategory *out_category;
    PresetType **out_types;
    int *out_type_count;
    PresetType *out_output;
    char **out_math;
    char **out_complexity;
    bool *out_constructive;
    bool *out_reversible;
} LvzPresetCtx;

typedef bool (*LvzPresetFieldFn)(LvzParser *p, LvzPresetCtx *ctx);

static bool preset_field_description(LvzParser *p, LvzPresetCtx *ctx) {
    lvz_parser_advance(p);
    if (!lvz_parser_expect_string(p, ctx->out_desc))
        return false;
    lvz_parser_advance(p);
    return true;
}

static bool preset_field_category(LvzParser *p, LvzPresetCtx *ctx) {
    lvz_parser_advance(p);
    if (!lvz_parser_expect(p, TOK_STRING))
        return false;
    *ctx->out_category = lvz_category_from_string(p->current.str_value);
    lvz_parser_advance(p);
    return true;
}

static bool preset_field_inputs(LvzParser *p, LvzPresetCtx *ctx) {
    lvz_parser_advance(p);
    /* 期望输入类型数量 */
    if (!lvz_parser_expect(p, TOK_NUMBER))
        return false;
    int count = (int) p->current.num_value;
    lvz_parser_advance(p);

    if (count > 0) {
        *ctx->out_types = (PresetType *) lv_malloc((size_t) count * sizeof(PresetType));
        if (!*ctx->out_types) {
            p->has_error = true;
            lv_RETURN_ERROR_BOOL(lv_ERROR_OUT_OF_MEMORY, "解析错误 (行 %d): 无法分配输入类型数组", p->current.line);
        }
        for (int i = 0; i < count; i++) {
            if (!lvz_parser_expect(p, TOK_STRING)) {
                lv_free((void **) ctx->out_types);
                return false;
            }
            (*ctx->out_types)[i] = lvz_type_from_string(p->current.str_value);
            lvz_parser_advance(p);
        }
    }
    *ctx->out_type_count = count;
    return true;
}

static bool preset_field_output(LvzParser *p, LvzPresetCtx *ctx) {
    lvz_parser_advance(p);
    if (!lvz_parser_expect(p, TOK_STRING))
        return false;
    *ctx->out_output = lvz_type_from_string(p->current.str_value);
    lvz_parser_advance(p);
    return true;
}

static bool preset_field_math_def(LvzParser *p, LvzPresetCtx *ctx) {
    lvz_parser_advance(p);
    if (!lvz_parser_expect_string(p, ctx->out_math))
        return false;
    lvz_parser_advance(p);
    return true;
}

static bool preset_field_complexity(LvzParser *p, LvzPresetCtx *ctx) {
    lvz_parser_advance(p);
    if (!lvz_parser_expect_string(p, ctx->out_complexity))
        return false;
    lvz_parser_advance(p);
    return true;
}

static bool preset_field_constructive(LvzParser *p, LvzPresetCtx *ctx) {
    lvz_parser_advance(p);
    if (p->current.type == TOK_IDENTIFIER) {
        *ctx->out_constructive = (strcmp(p->current.str_value, "true") == 0);
        lvz_parser_advance(p);
    }
    return true;
}

static bool preset_field_reversible(LvzParser *p, LvzPresetCtx *ctx) {
    lvz_parser_advance(p);
    if (p->current.type == TOK_IDENTIFIER) {
        *ctx->out_reversible = (strcmp(p->current.str_value, "true") == 0);
        lvz_parser_advance(p);
    }
    return true;
}

/** @brief preset 块字段名→处理函数 查找表（替代 8 分支 strcmp 链） */
static const struct {
    const char *name;
    LvzPresetFieldFn fn;
} kPresetFieldTable[] = {
    {"description", preset_field_description},
    {"category", preset_field_category},
    {"inputs", preset_field_inputs},
    {"output", preset_field_output},
    {"math_def", preset_field_math_def},
    {"complexity", preset_field_complexity},
    {"constructive", preset_field_constructive},
    {"reversible", preset_field_reversible},
};

/**
 * @brief 解析 preset 块体内的单个字段
 *
 * 支持的字段: description, category, inputs, output, math_def, complexity, constructive, reversible
 *
 * @param p 解析器
 * @param name 预设名称（用于错误消息）
 * @param out_desc 输出描述字符串（调用者负责释放）
 * @param out_category 输出类别
 * @param out_types 输出输入类型数组（调用者负责释放）
 * @param out_type_count 输出输入类型数量
 * @param out_output 输出类型
 * @param out_math 输出数学定义字符串（调用者负责释放）
 * @param out_complexity 输出复杂度字符串（调用者负责释放）
 * @param out_constructive 输出是否构造性
 * @param out_reversible 输出是否可逆
 * @return true 解析成功，false 解析失败
 */
static bool lvz_parse_preset_body(LvzParser *p, const char *name,
                                   char **out_desc, PresetCategory *out_category,
                                   PresetType **out_types, int *out_type_count,
                                   PresetType *out_output,
                                   char **out_math, char **out_complexity,
                                   bool *out_constructive, bool *out_reversible) {
    /* 初始化字段默认值 */
    *out_desc = NULL;
    *out_category = PRESET_CATEGORY_CUSTOM;
    *out_types = NULL;
    *out_type_count = 0;
    *out_output = PRESET_TYPE_ANY;
    *out_math = NULL;
    *out_complexity = NULL;
    *out_constructive = true;
    *out_reversible = false;

    /* 构建字段解析上下文 */
    LvzPresetCtx ctx;
    ctx.out_desc = out_desc;
    ctx.out_category = out_category;
    ctx.out_types = out_types;
    ctx.out_type_count = out_type_count;
    ctx.out_output = out_output;
    ctx.out_math = out_math;
    ctx.out_complexity = out_complexity;
    ctx.out_constructive = out_constructive;
    ctx.out_reversible = out_reversible;

    /* 跳过 { */
    if (p->current.type == TOK_LBRACE) {
        lvz_parser_advance(p);
    }

    /* 解析字段，直到遇到 } 或 EOF */
    while (p->current.type != TOK_RBRACE && p->current.type != TOK_EOF && !p->has_error) {
        if (p->current.type != TOK_IDENTIFIER) {
            p->has_error = true;
            lv_RETURN_ERROR_BOOL(lv_ERROR_PARSE, "解析错误 (行 %d, 预设 '%s'): 期望字段名", p->current.line, name);
        }

        const char *field = p->current.str_value;

        /* 字段查表（替代 8 分支 strcmp 链） */
        bool matched = false;
        for (size_t i = 0; i < lv_ARRAY_SIZE(kPresetFieldTable); i++) {
            if (strcmp(field, kPresetFieldTable[i].name) == 0) {
                if (!kPresetFieldTable[i].fn(p, &ctx))
                    return false;
                matched = true;
                break;
            }
        }
        if (!matched) {
            p->has_error = true;
            lv_RETURN_ERROR_BOOL(lv_ERROR_PARSE, "解析错误 (行 %d, 预设 '%s'): 未知字段 '%s'", p->current.line, name,
                                 field);
        }
    }

    /* 期望 } */
    if (p->current.type == TOK_RBRACE) {
        lvz_parser_advance(p);
    }

    return !p->has_error;
}

/**
 * @brief 解析 preset 节: presets { preset "name" { ... } ... }
 *
 * @param p 解析器
 * @param mod 模块（可选，预设注册不依赖模块，此处保留参数以兼容主解析器接口）
 * @return true 解析成功，false 解析失败
 */
static bool lvz_parse_presets_section(LvzParser *p, Module *mod) {
    (void) mod; /* 预设注册不依赖模块 */
    lvz_parser_advance(p); /* 跳过 'presets' */

    /* 期望 { */
    if (p->current.type != TOK_LBRACE) {
        p->has_error = true;
        lv_RETURN_ERROR_BOOL(lv_ERROR_PARSE, "解析错误 (行 %d): 期望 '{' 开始预设节", p->current.line);
    }
    lvz_parser_advance(p);

    /* 解析每个 preset 块 */
    while (p->current.type == TOK_IDENTIFIER && strcmp(p->current.str_value, "preset") == 0 && !p->has_error) {
        lvz_parser_advance(p); /* 跳过 'preset' */

        /* 期望预设名称 (字符串) */
        if (!lvz_parser_expect(p, TOK_STRING)) {
            p->has_error = true;
            lv_RETURN_ERROR_BOOL(lv_ERROR_PARSE, "解析错误 (行 %d): 期望预设名称字符串", p->current.line);
        }
        char *preset_name = lv_strdup_safe(p->current.str_value);
        lvz_parser_advance(p);

        /* 解析预设体 */
        char *desc = NULL;
        PresetCategory category = PRESET_CATEGORY_CUSTOM;
        PresetType *input_types = NULL;
        int input_count = 0;
        PresetType output_type = PRESET_TYPE_ANY;
        char *math_def = NULL;
        char *complexity = NULL;
        bool constructive = true;
        bool reversible = false;

        bool ok = lvz_parse_preset_body(p, preset_name, &desc, &category,
                                         &input_types, &input_count,
                                         &output_type, &math_def, &complexity,
                                         &constructive, &reversible);
        if (ok) {
            /* 注册预设 */
            ok = preset_blocks_register_simple(preset_name, desc ? desc : "",
                                                category,
                                                input_types, input_count,
                                                output_type,
                                                math_def, complexity,
                                                constructive, reversible);
            if (!ok) {
                lv_LOG_WARNING("预设 '%s' 注册失败（在 .lvz 文件中）", preset_name);
            }
        }

        /* 释放临时资源 */
        lv_free((void **) &desc);
        lv_free((void **) &math_def);
        lv_free((void **) &complexity);
        lv_free((void **) &input_types);
        lv_free((void **) &preset_name);

        if (!ok) {
            return false;
        }
    }

    /* 期望 } 结束 presets 节 */
    if (p->current.type != TOK_RBRACE) {
        p->has_error = true;
        lv_RETURN_ERROR_BOOL(lv_ERROR_PARSE, "解析错误 (行 %d): 预设节缺少结束 '}'", p->current.line);
    }
    lvz_parser_advance(p);

    return !p->has_error;
}

/* 主解析函数 */
bool lvz_parse(LvzParser *p, Module *mod) {
    /* 获取第一个 token */
    lvz_parser_advance(p);

    /* 期望 'lvz' 关键字 */
    if (!lvz_parser_expect_identifier(p, "lvz")) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_PARSE, "无效的 LVZ 文件: 缺少 'lvz' 头");
    }
    lvz_parser_advance(p);

    /* 期望版本号 */
    if (!lvz_parser_expect(p, TOK_NUMBER)) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_PARSE, "无效的 LVZ 文件: 缺少版本号");
    }
    int major = (int) p->current.num_value;
    lvz_parser_advance(p);

    /* 可选的次版本号 */
    int minor = 0;
    if (p->current.type == TOK_NUMBER) {
        minor = (int) p->current.num_value;
        lvz_parser_advance(p);
    } else if (p->current.type == TOK_IDENTIFIER) {
        /* 可能是 "1.0" 格式，标识符包含点 */
        /* 已经作为标识符读取，跳过 */
        lvz_parser_advance(p);
    }

    /* 检查版本兼容性 */
    if (major > LVZ_VERSION_MAJOR) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_UNSUPPORTED, "不支持的 LVZ 版本: %d.%d (最高支持 %d.%d)", major, minor,
                             LVZ_VERSION_MAJOR, LVZ_VERSION_MINOR);
    }

    /* 解析各个部分 */
    while (p->current.type != TOK_EOF && !p->has_error) {
        if (p->current.type != TOK_IDENTIFIER) {
            lv_RETURN_ERROR_BOOL(lv_ERROR_PARSE, "解析错误 (行 %d): 期望节名称", p->current.line);
        }

        const char *section = p->current.str_value;

        if (strcmp(section, "module") == 0) {
            if (!lvz_parse_module_decl(p, mod))
                return false;
        } else if (strcmp(section, "deps") == 0) {
            if (!lvz_parse_deps_section(p, mod))
                return false;
        } else if (strcmp(section, "exports") == 0) {
            if (!lvz_parse_exports_section(p, mod))
                return false;
        } else if (strcmp(section, "axioms") == 0) {
            if (!lvz_parse_axioms_section(p, mod))
                return false;
        } else if (strcmp(section, "nodes") == 0) {
            if (!lvz_parse_nodes_section(p, mod))
                return false;
        } else if (strcmp(section, "constraints") == 0) {
            if (!lvz_parse_constraints_section(p, mod))
                return false;
        } else if (strcmp(section, "func_blocks") == 0) {
            if (!lvz_parse_func_blocks_section(p, mod))
                return false;
        } else if (strcmp(section, "presets") == 0) {
            if (!lvz_parse_presets_section(p, mod))
                return false;
        } else if (strcmp(section, "end") == 0) {
            lvz_parser_advance(p);
            break;
        } else {
            lv_RETURN_ERROR_BOOL(lv_ERROR_PARSE, "解析错误 (行 %d): 未知的节 '%s'", p->current.line, section);
        }
    }

    return !p->has_error;
}

bool dependency_exists(Module **visited, int count, Module *mod) {
    for (int i = 0; i < count; i++) {
        if (visited[i] == mod)
            return true;
    }
    return false;
}

/**
 * @brief 从 .lvz 文件加载预设定义并注册
 *
 * 读取指定 .lvz 文件，解析其中的 presets 节，自动注册所有预设。
 * 文件的其余部分（module/deps/axioms/nodes/constraints 等）会被忽略。
 *
 * @param filepath .lvz 文件的完整路径
 * @return true 加载成功，false 加载失败（文件不存在、解析错误等）
 */
bool lvz_load_presets_file(const char *filepath) {
    if (!filepath) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_PARSE, "lvz_load_presets_file: 文件路径为空");
    }

    /* 读取文件内容（lv_file_read_all：失败/空文件返回 NULL，成功时缓冲以 NUL 结尾） */
    size_t file_size = 0;
    char *source = (char *) lv_file_read_all(filepath, &file_size);
    if (!source) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_IO, "lvz_load_presets_file: 无法读取文件 '%s'（不存在、为空或读取失败）", filepath);
    }

    /* 解析 .lvz 内容（presets 节会自动注册预设） */
    LvzParser parser;
    lvz_parser_init(&parser, source);
    /* 记录模块目录路径，以便在解析错误时提供上下文
     * （lv_path_dirname 同时识别 '/' 与 '\\'，无分隔符时返回 NULL 不分配） */
    {
        size_t dir_len = 0;
        const char *dir_start = lv_path_dirname(filepath, &dir_len);
        if (dir_start) {
            parser.module_dir = (char *) lv_malloc(dir_len + 1);
            if (parser.module_dir) {
                memcpy(parser.module_dir, dir_start, dir_len);
                parser.module_dir[dir_len] = '\0';
            }
        }
    }

    /* 解析（preset 注册在解析过程中自动完成） */
    bool ok = lvz_parse(&parser, NULL);

    /* 清理 */
    lvz_parser_cleanup(&parser);
    lv_free((void **) &source);

    if (!ok) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_PARSE, "lvz_load_presets_file: 解析 '%s' 失败", filepath);
    }

    return true;
}
