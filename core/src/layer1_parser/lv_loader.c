#include "lv/lv_loader.h"
#include "lv/lv_lexer.h"
#include "lv/lv.h"
#include "lv_utils.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ── 名称映射表：跟踪 AST 名称到引擎节点 ID 的映射 ── */
typedef struct {
    char name[64];
    int  node_id;
} LvNameMap;

#define LV_MAX_NAMED_ENTITIES 256

static LvNameMap name_map[LV_MAX_NAMED_ENTITIES];
static int name_map_count = 0;

static void name_map_clear(void) {
    name_map_count = 0;
}

static void name_map_add(const char *name, int node_id) {
    if (name_map_count >= LV_MAX_NAMED_ENTITIES) return;
    LvNameMap *entry = &name_map[name_map_count++];
    lv_strncpy(entry->name, name, sizeof(entry->name));
    entry->node_id = node_id;
}

static int name_map_lookup(const char *name) {
    for (int i = 0; i < name_map_count; i++) {
        if (strcmp(name_map[i].name, name) == 0)
            return name_map[i].node_id;
    }
    return -1;
}

/* ================================================================
 * 文件读取
 * ================================================================ */

static char *read_file(const char *filepath, size_t *out_len) {
    if (!filepath || !out_len) return NULL;

    FILE *fp = fopen(filepath, "rb");
    if (!fp) return NULL;

    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    rewind(fp);

    if (len < 0) {
        fclose(fp);
        return NULL;
    }

    char *buf = (char *)lv_malloc((size_t)len + 1);
    if (!buf) {
        fclose(fp);
        return NULL;
    }

    size_t read = fread(buf, 1, (size_t)len, fp);
    fclose(fp);

    buf[read] = '\0';
    *out_len = read;
    return buf;
}

/* ================================================================
 * 处理 Declaration 节点，向引擎添加几何实体
 * ================================================================ */

static void process_declaration(lvEngine *engine, LvAstNode *node) {
    LvEntityType etype = (LvEntityType)node->data.decl.entity_type;
    const char *names = node->data.decl.names;
    if (!names || !engine) return;

    /* 复制 names 用于拆分 */
    char buf[1024];
    lv_strncpy(buf, names, sizeof(buf));

    /* 拆分逗号分隔的名称列表 */
    char *save;
    char *tok;
#ifdef _MSC_VER
    tok = strtok_s(buf, ",", &save);
#else
    tok = strtok_r(buf, ",", &save);
#endif
    while (tok) {
        switch (etype) {
        case LV_ENTITY_POINT: {
            /* 使用默认坐标 (0,1,0,1) 即 (0,0) */
            int id = lv_add_point(engine, 0, 1, 0, 1);
            if (id >= 0) {
                name_map_add(tok, id);
            }
            break;
        }
        case LV_ENTITY_LINE:
        case LV_ENTITY_SEGMENT: {
            /* Line/Segment: 暂存名称，等待端点声明后处理 */
            /* 使用 -1 表示尚未关联端点 */
            name_map_add(tok, -1);
            break;
        }
        default:
            /* Circle, Ray, Triangle, Polygon, Scalar, Bool, Proposition, Proof */
            /* 暂不处理 */
            name_map_add(tok, -1);
            break;
        }
#ifdef _MSC_VER
        tok = strtok_s(NULL, ",", &save);
#else
        tok = strtok_r(NULL, ",", &save);
#endif
    }
}

/* ================================================================
 * 公共 API
 * ================================================================ */

LvParseResult lv_load_file(const char *filepath) {
    LvParseResult result;
    memset(&result, 0, sizeof(result));

    if (!filepath) {
        result.error_count = 1;
        lv_strncpy(result.errors[0].message, "filepath is NULL",
                   sizeof(result.errors[0].message));
        return result;
    }

    /* 读取文件 */
    size_t len = 0;
    char *source = read_file(filepath, &len);
    if (!source) {
        result.error_count = 1;
        lv_snprintf(result.errors[0].message, sizeof(result.errors[0].message),
                    "failed to read file: %s", filepath);
        return result;
    }

    /* Lex → Parse */
    LvLexer *lexer = lv_lexer_create(source, len);
    if (!lexer) {
        lv_free((void **)&source);
        result.error_count = 1;
        lv_strncpy(result.errors[0].message, "failed to create lexer",
                   sizeof(result.errors[0].message));
        return result;
    }

    LvParser *parser = lv_parser_create(lexer);
    if (!parser) {
        lv_lexer_destroy(lexer);
        lv_free((void **)&source);
        result.error_count = 1;
        lv_strncpy(result.errors[0].message, "failed to create parser",
                   sizeof(result.errors[0].message));
        return result;
    }

    result = lv_parser_parse_program(parser);

    lv_parser_destroy(parser);
    lv_lexer_destroy(lexer);
    lv_free((void **)&source);

    return result;
}

bool lv_apply_parse_result(lvEngine *engine, const LvParseResult *result, LvSemaContext *sema) {
    if (!engine || !result || !result->ast) return false;

    (void)sema; /* 保留供将来扩展使用 */
    name_map_clear();

    LvAstNode *ast = result->ast;
    if (ast->type != LV_AST_PROGRAM) return false;

    /* 第一遍：处理声明（添加几何对象到引擎） */
    for (LvAstNode *stmt = ast->child; stmt; stmt = stmt->next) {
        if (stmt->type == LV_AST_DECLARATION) {
            process_declaration(engine, stmt);
        }
    }

    /* 第二遍：处理声明之后的线段/直线（如果端点已知） */
    for (LvAstNode *stmt = ast->child; stmt; stmt = stmt->next) {
        if (stmt->type == LV_AST_DECLARATION) {
            LvEntityType etype = (LvEntityType)stmt->data.decl.entity_type;
            if (etype == LV_ENTITY_SEGMENT || etype == LV_ENTITY_LINE) {
                /* 线段/直线需要两个已知端点才能创建 */
                /* 这里简化处理：跳过，因为端点尚未连接 */
            }
        }
    }

    /* 第三遍：处理 Constraint 和 Prove 语句 */
    for (LvAstNode *stmt = ast->child; stmt; stmt = stmt->next) {
        if (stmt->type == LV_AST_CONSTRAINT_STMT) {
            /* Constraint 语句：提取标识符引用并尝试添加约束 */
            LvAstNode *expr = stmt->data.stmt.expr;
            if (expr && expr->type == LV_AST_RELATION) {
                /* 收集参数节点 ID */
                int arg_ids[8];
                int arg_count = 0;
                for (LvAstNode *a = expr->data.call.args; a && arg_count < 8; a = a->next) {
                    if (a->type == LV_AST_IDENTIFIER_EXPR) {
                        int id = name_map_lookup(a->data.ident.name);
                        if (id >= 0) {
                            arg_ids[arg_count++] = id;
                        }
                    }
                }

                /* 根据关系类型添加约束 */
                const char *fname = expr->data.call.func_name;
                if (fname && strcmp(fname, "collinear") == 0 && arg_count >= 3) {
                    /* 简化处理：collinear 约束，用 incidence */
                    for (int i = 1; i < arg_count; i++) {
                        lv_add_constraint_incidence(engine, arg_ids[i], arg_ids[0]);
                    }
                }
            }
        } else if (stmt->type == LV_AST_PROVE_STMT) {
            /* Prove 语句：作为证明目标设置 */
            /* 当前简化实现：只是标记引擎的证明意图 */
            /* Prove 语句不会导致引擎重大变化 */
        }
    }

    return true;
}
