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

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "module_helpers.h"

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
        lv_set_error(lv_ERROR_PARSE, "解析错误 (行 %d, 列 %d): 期望 token 类型 %d, 得到 %d", p->current.line,
                     p->current.col, type, p->current.type);
        p->has_error = true;
        return false;
    }
    return true;
}

bool lvz_parser_expect_identifier(LvzParser *p, const char *name) {
    if (p->current.type != TOK_IDENTIFIER || strcmp(p->current.str_value, name) != 0) {
        lv_set_error(lv_ERROR_PARSE, "解析错误 (行 %d, 列 %d): 期望关键字 '%s'", p->current.line, p->current.col, name);
        p->has_error = true;
        return false;
    }
    return true;
}

bool lvz_parser_expect_number(LvzParser *p, int *value) {
    if (p->current.type != TOK_NUMBER) {
        lv_set_error(lv_ERROR_PARSE, "解析错误 (行 %d, 列 %d): 期望数字", p->current.line, p->current.col);
        p->has_error = true;
        return false;
    }
    if (value)
        *value = (int) p->current.num_value;
    return true;
}

bool lvz_parser_expect_string(LvzParser *p, char **out) {
    if (p->current.type != TOK_STRING) {
        lv_set_error(lv_ERROR_PARSE, "解析错误 (行 %d, 列 %d): 期望字符串", p->current.line, p->current.col);
        p->has_error = true;
        return false;
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
            lv_set_error(lv_ERROR_PARSE, "解析错误 (行 %d): 期望 'dep' 关键字", p->current.line);
            return false;
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
            lv_set_error(lv_ERROR_PARSE, "解析错误 (行 %d): 期望 'func_block' 关键字", p->current.line);
            return false;
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
            lv_set_error(lv_ERROR_PARSE, "解析错误 (行 %d): 期望 'type_region' 关键字", p->current.line);
            return false;
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
            lv_set_error(lv_ERROR_PARSE, "解析错误 (行 %d): 期望 'axiom' 关键字", p->current.line);
            return false;
        }
        lvz_parser_advance(p);

        /* 期望公理包名 (字符串) */
        if (!lvz_parser_expect(p, TOK_STRING))
            return false;

        /* 创建公理包并加载 */
        AxiomPackage *pkg = axiom_package_create(p->current.str_value, "0.0.0");
        if (pkg) {
            module_add_axiom_package(mod, pkg);
        }
        lvz_parser_advance(p);
    }

    return true;
}

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
            lv_set_error(lv_ERROR_OUT_OF_MEMORY, "无法创建约束图");
            return false;
        }
    }

    /* 解析每个节点 */
    for (int i = 0; i < count && !p->has_error; i++) {
        if (p->current.type != TOK_IDENTIFIER) {
            lv_set_error(lv_ERROR_PARSE, "解析错误 (行 %d): 期望节点类型", p->current.line);
            return false;
        }

        const char *node_type = p->current.str_value;
        lvz_parser_advance(p);

        /* 期望节点 ID */
        int node_id = 0;
        if (!lvz_parser_expect_number(p, &node_id))
            return false;
        lvz_parser_advance(p);

        if (strcmp(node_type, "point") == 0) {
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
            SymbolicCoord *sx = symbolic_coord_create_rational((int64_t) round(x * 10000), 10000);
            SymbolicCoord *sy = symbolic_coord_create_rational((int64_t) round(y * 10000), 10000);
            SymbolicCoord *coords[2] = {sx, sy};
            if (sx && sy) {
                graph_add_point(mod->graph, coords, 2);
                symbolic_coord_destroy(sx);
                symbolic_coord_destroy(sy);
            }
        } else if (strcmp(node_type, "line") == 0) {
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
        } else if (strcmp(node_type, "circle") == 0) {
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
            /* 圆节点暂不支持，跳过 */
            (void) center;
            (void) radius;
        } else {
            /* 未知节点类型，跳过参数 */
            while (p->current.type == TOK_NUMBER) {
                lvz_parser_advance(p);
            }
        }
    }

    return true;
}

/* 解析约束部分: constraints N { constraint_type ... } */
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
            lv_set_error(lv_ERROR_OUT_OF_MEMORY, "无法创建约束图");
            return false;
        }
    }

    /* 解析每个约束 */
    for (int i = 0; i < count && !p->has_error; i++) {
        if (p->current.type != TOK_IDENTIFIER) {
            lv_set_error(lv_ERROR_PARSE, "解析错误 (行 %d): 期望约束类型", p->current.line);
            return false;
        }

        const char *constraint_type = p->current.str_value;
        lvz_parser_advance(p);

        /* 解析约束参数 (简化实现，读取所有数字参数) */
        int params[8];
        int param_count = 0;

        while (p->current.type == TOK_NUMBER && param_count < 8) {
            params[param_count++] = (int) p->current.num_value;
            lvz_parser_advance(p);
        }

        /* 创建约束 (简化实现) */
        if (strcmp(constraint_type, "incidence") == 0 && param_count >= 2) {
            /* incidence point_id line_id */
            if (params[0] >= 0 && params[1] >= 0) {
                graph_add_incidence(mod->graph, params[0], params[1]);
            }
        } else if (strcmp(constraint_type, "betweenness") == 0 && param_count >= 3) {
            /* betweenness p1 p2 p3 */
            if (params[0] >= 0 && params[1] >= 0 && params[2] >= 0) {
                graph_add_betweenness(mod->graph, params[0], params[1], params[2]);
            }
        } else if (strcmp(constraint_type, "intersection") == 0 && param_count >= 3) {
            /* intersection line1 line2 result_point */
            if (params[0] >= 0 && params[1] >= 0 && params[2] >= 0) {
                graph_add_intersection(mod->graph, params[0], params[1], params[2]);
            }
        } else if (strcmp(constraint_type, "containment") == 0 && param_count >= 2) {
            /* containment inner outer */
            if (params[0] >= 0 && params[1] >= 0) {
                graph_add_containment(mod->graph, params[0], params[1]);
            }
        } else if (strcmp(constraint_type, "connection") == 0 && param_count >= 2) {
            /* connection src_port dst_port */
            if (params[0] >= 0 && params[1] >= 0) {
                graph_add_connection(mod->graph, params[0], params[1]);
            }
        }
        /* 其他约束类型 (distance, angle, parallel, perpendicular, tangent) 
           是公理包定义的高级约束，需要通过模板展开，这里暂不支持 */
    }

    return true;
}

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
            lv_set_error(lv_ERROR_PARSE, "解析错误 (行 %d): 期望 'func_block' 关键字", p->current.line);
            return false;
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

        /* 解析 inputs/outputs/internal */
        int inputs = 0, outputs = 0, internal = 0;

        while (p->current.type == TOK_IDENTIFIER && !p->has_error) {
            if (strcmp(p->current.str_value, "inputs") == 0) {
                lvz_parser_advance(p);
                if (!lvz_parser_expect_number(p, &inputs))
                    break;
                lvz_parser_advance(p);
            } else if (strcmp(p->current.str_value, "outputs") == 0) {
                lvz_parser_advance(p);
                if (!lvz_parser_expect_number(p, &outputs))
                    break;
                lvz_parser_advance(p);
            } else if (strcmp(p->current.str_value, "internal") == 0) {
                lvz_parser_advance(p);
                if (!lvz_parser_expect_number(p, &internal))
                    break;
                lvz_parser_advance(p);
            } else {
                break;
            }
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

/* 主解析函数 */
bool lvz_parse(LvzParser *p, Module *mod) {
    /* 获取第一个 token */
    lvz_parser_advance(p);

    /* 期望 'lvz' 关键字 */
    if (!lvz_parser_expect_identifier(p, "lvz")) {
        lv_set_error(lv_ERROR_PARSE, "无效的 LVZ 文件: 缺少 'lvz' 头");
        return false;
    }
    lvz_parser_advance(p);

    /* 期望版本号 */
    if (!lvz_parser_expect(p, TOK_NUMBER)) {
        lv_set_error(lv_ERROR_PARSE, "无效的 LVZ 文件: 缺少版本号");
        return false;
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
        lv_set_error(lv_ERROR_UNSUPPORTED, "不支持的 LVZ 版本: %d.%d (最高支持 %d.%d)", major, minor, LVZ_VERSION_MAJOR,
                     LVZ_VERSION_MINOR);
        return false;
    }

    /* 解析各个部分 */
    while (p->current.type != TOK_EOF && !p->has_error) {
        if (p->current.type != TOK_IDENTIFIER) {
            lv_set_error(lv_ERROR_PARSE, "解析错误 (行 %d): 期望节名称", p->current.line);
            return false;
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
        } else if (strcmp(section, "end") == 0) {
            lvz_parser_advance(p);
            break;
        } else {
            lv_set_error(lv_ERROR_PARSE, "解析错误 (行 %d): 未知的节 '%s'", p->current.line, section);
            return false;
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
