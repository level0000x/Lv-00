/**
 * @file lv_loader.c
 * @brief .lv 文件加载与引擎集成实现
 *
 * @details 实现 .lv 源文件的读取、解析和加载到 lvEngine 的全流程。
 *          包含三个处理阶段：
 *          1. 文件读取
 *          2. 词法分析 → 语法分析 → 语义分析管线
 *          3. 将解析结果应用到引擎（实体声明、约束添加、证明目标设置）
 *
 *          通过名称映射表（LvNameMap）跟踪 AST 中声明的实体名称到
 *          引擎节点 ID 的映射关系。
 *
 * @author Lv-00 Project
 */

#include "lv/lv_loader.h"
#include "lv/lv_lexer.h"
#include "lv/lv.h"
#include "lv_utils.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ── 名称映射表：跟踪 AST 名称到引擎节点 ID 的映射 ── */

/**
 * @brief 名称映射条目
 *
 * 将 AST 中声明的实体名称与引擎内部的节点 ID 关联，
 * 用于在后续约束和证明语句中引用已声明的实体。
 */
typedef struct {
    char name[64];  /**< 实体名称（最多 63 字符） */
    int  node_id;   /**< 引擎节点 ID，-1 表示尚未关联 */
} LvNameMap;

/** @brief 名称映射表最大容量 */
#define LV_MAX_NAMED_ENTITIES 256

/** @brief 全局名称映射表 */
static LvNameMap name_map[LV_MAX_NAMED_ENTITIES];
/** @brief 当前已注册的名称数量 */
static int name_map_count = 0;

/**
 * @brief 清空名称映射表
 *
 * 重置映射表计数器，清除所有已注册的名称映射。
 */
static void name_map_clear(void) {
    name_map_count = 0;
}

/**
 * @brief 向名称映射表添加条目
 *
 * @param name    实体名称
 * @param node_id 引擎节点 ID
 */
static void name_map_add(const char *name, int node_id) {
    if (name_map_count >= LV_MAX_NAMED_ENTITIES) return;
    LvNameMap *entry = &name_map[name_map_count++];
    lv_strncpy(entry->name, name, sizeof(entry->name));
    entry->node_id = node_id;
}

/**
 * @brief 在名称映射表中查找名称
 *
 * 遍历名称映射表，查找与给定名称匹配的条目。
 *
 * @param name 实体名称（允许为 NULL，返回 -1）
 * @return 引擎节点 ID，未找到或 name 为 NULL 返回 -1
 */
static int name_map_lookup(const char *name) {
    if (!name) return -1;
    for (int i = 0; i < name_map_count; i++) {
        if (strcmp(name_map[i].name, name) == 0)
            return name_map[i].node_id;
    }
    return -1;
}

/* ================================================================
 * 文件读取
 * ================================================================ */

/**
 * @brief 读取文件内容到内存
 *
 * 以二进制模式打开文件，读取全部内容并分配缓冲区。
 * 调用者需通过 lv_free 释放返回的缓冲区。
 *
 * @param filepath 文件路径
 * @param out_len  输出：文件长度（字节数）
 * @return 文件内容的堆分配缓冲区，失败返回 NULL
 */
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

/**
 * @brief 处理 AST 声明节点，向引擎添加几何实体
 *
 * 解析 Declaration 节点的实体类型和名称列表，根据实体类型
 * 执行对应的引擎添加操作（如添加点、预留线段/直线名称等）。
 * 名称列表为逗号分隔的字符串。
 *
 * @param engine 引擎指针
 * @param node   AST 声明节点
 */
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

/**
 * @brief 加载并解析一个 .lv 文件
 *
 * 完整的文件加载管线：读取文件 → 创建词法分析器 → 创建解析器 →
 * 解析为 AST → 释放临时资源 → 返回解析结果。
 *
 * @param filepath .lv 文件路径
 * @return 解析结果结构体，包含 AST 和可能的错误信息
 */
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

/**
 * @brief 将解析结果应用到引擎
 *
 * 三遍处理策略：
 * 1. 第一遍：处理声明，向引擎添加几何对象（点、线等）
 * 2. 第二遍：处理需要端点已知的实体（线段/直线，当前简化跳过）
 * 3. 第三遍：处理约束和证明语句
 *
 * @param engine 引擎指针
 * @param result 解析结果
 * @param sema   语义分析上下文（当前保留供将来扩展）
 * @return 应用成功返回 true，失败返回 false
 */
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
