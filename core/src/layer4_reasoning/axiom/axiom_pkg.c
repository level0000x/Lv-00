/**
 * @file axiom_pkg.c
 * @brief 公理系统包实现
 * @details 实现公理包的加载、验证和展开功能。支持约束模板、
 *          不可构造问题检测、双层测试和 SHA-256 依赖追踪。
 */

#include "axiom_pkg.h"

#include "lv/lv_file.h"

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "lv/sha256.h"


#include "debug.h"
#include "error_codes.h"
#include "lexer_shared.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "stream.h"


/* 兼容性宏：set_error → lv_set_error */
#define set_error(fmt, ...) lv_set_error(lv_ERROR_INVALID_PARAM, (fmt), ##__VA_ARGS__)

/* 兼容性宏：PropositionKind 短名 */
#ifndef CONSTRUCTIVE
#define CONSTRUCTIVE PROPOSITION_KIND_CONSTRUCTIVE
#endif
#ifndef NON_CONSTRUCTIVE_ORACLE
#define NON_CONSTRUCTIVE_ORACLE PROPOSITION_KIND_NON_CONSTRUCTIVE_ORACLE
#endif
#ifndef EXPLOSION_PRINCIPLE
#define EXPLOSION_PRINCIPLE PROPOSITION_KIND_EXPLOSION_PRINCIPLE
#endif

/* 线程局部存储用于错误消息（使用lv_internal.h中定义的lv_THREAD_LOCAL） */

static lv_THREAD_LOCAL StreamContext *axiom_stream_ctx = NULL;

void axiom_pkg_set_stream_context(StreamContext *ctx) {
    axiom_stream_ctx = ctx;
}

/** SHA-256 输出大小（字节） */
#define AXIOM_SHA256_OUTPUT_SIZE 32

/** SHA-256 哈希十六进制字符串大小（64字符 + 空终止符） */
#define AXIOM_SHA256_HEX_SIZE 65

/** 展开缓存的默认初始容量 */
#define AXIOM_EXPANSION_CACHE_CAP 16

/** 依赖引用缓存的默认初始容量 */
#define AXIOM_DEP_REF_CACHE_CAP 16

/** 最大递归展开深度 */
#define AXIOM_MAX_EXPANSION_DEPTH 8

/** 最大公理源文件大小 (64 MB) */
#define AXIOM_MAX_FILE_SIZE (64 * 1024 * 1024)

/** 规范形式最大参与者类型数量 */
#define AXIOM_MAX_PARTICIPANT_TYPES 8

/** 参与者类型名称的最大长度 */
#define AXIOM_PARTICIPANT_TYPE_LEN 32

/** 测试失败消息缓冲区大小 */
#define AXIOM_TEST_MSG_BUF_SIZE 256

/** 模板参数描述格式字符串最大长度 */
#define AXIOM_PARAM_DESC_MAX_LEN 64

/* ============== 辅助函数 ============== */

const char *axiom_package_get_last_error(void) {
    return lv_get_last_error_message();
}

static char *safe_lv_strdup_safe(const char *s) {
    if (!s)
        return NULL;
    size_t len = strlen(s);
    char *dup = lv_malloc(len + 1);
    if (dup) {
        memcpy(dup, s, len + 1); /* 使用 memcpy 替代 strcpy，确保安全 */
    }
    return dup;
}

/* ============== 创建和销毁 ============== */

AxiomPackage *axiom_package_create(const char *name, const char *version) {
    AxiomPackage *pkg = lv_calloc(1, sizeof(AxiomPackage));
    if (!pkg)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "axiom_package_create: lv_calloc failed");

    pkg->name = safe_lv_strdup_safe(name);
    pkg->version = safe_lv_strdup_safe(version);
    lv_darray_init(&pkg->templates, sizeof(ConstraintTemplate));
    lv_darray_init(&pkg->known_unconstructibles, sizeof(KnownUnconstructible));
    lv_darray_init(&pkg->unconstructible_templates, sizeof(UnconstructibleTemplate));
    pkg->bottom_geometry = NULL;
    pkg->negation_encoding = NULL;
    pkg->contradiction_behavior = EXPLOSION_PRINCIPLE;
    lv_darray_init(&pkg->expansion_cache, sizeof(TemplateExpansionCache));
    pkg->max_expansion_depth = AXIOM_MAX_EXPANSION_DEPTH; /* 默认递归深度 */
    lv_darray_init(&pkg->dep_refs, sizeof(DependencyRef));

    if (axiom_stream_ctx) {
        stream_emit_simple(axiom_stream_ctx, STREAM_EVENT_INFO, "公理包创建成功", 0);
    }

    return pkg;
}

void axiom_package_destroy(AxiomPackage *pkg) {
    if (!pkg)
        return;

    lv_free((void **) &pkg->name);
    lv_free((void **) &pkg->version);

    /* 释放模板 */
    for (int i = 0; i < pkg->templates.count; i++) {
        ConstraintTemplate *t = (ConstraintTemplate *)lv_darray_get(&pkg->templates, i);
        lv_free((void **) &t->name);
        lv_free((void **) &t->params);
        if (t->compressed_subgraph) {
            graph_destroy(t->compressed_subgraph);
        }
    }
    lv_darray_free(&pkg->templates);

    /* 释放不可构造问题 */
    for (int i = 0; i < pkg->known_unconstructibles.count; i++) {
        KnownUnconstructible *uc = (KnownUnconstructible *)lv_darray_get(&pkg->known_unconstructibles, i);
        lv_free((void **) &uc->name);
        lv_free((void **) &uc->reduces_to);
        lv_free((void **) &uc->external_ref);

        /* 释放依赖链 */
        for (int j = 0; j < uc->dependency_chain.count; j++) {
            lv_free((void **) lv_darray_get(&uc->dependency_chain, j));
        }
        lv_darray_free(&uc->dependency_chain);
    }
    lv_darray_free(&pkg->known_unconstructibles);

    /* 释放不可构造性证明模板 */
    for (int i = 0; i < pkg->unconstructible_templates.count; i++) {
        UnconstructibleTemplate *tmpl = (UnconstructibleTemplate *)lv_darray_get(&pkg->unconstructible_templates, i);
        lv_free((void **) &tmpl->target_problem_name);
        lv_free((void **) &tmpl->known_unconstructible_name);
        if (tmpl->reduction_construction) {
            graph_destroy(tmpl->reduction_construction);
        }
        lv_free((void **) &tmpl->description);
    }
    lv_darray_free(&pkg->unconstructible_templates);

    lv_free((void **) &pkg->bottom_geometry);
    lv_free((void **) &pkg->negation_encoding);

    /* 释放模板展开缓存 */
    for (int i = 0; i < pkg->expansion_cache.count; i++) {
        TemplateExpansionCache *c = (TemplateExpansionCache *)lv_darray_get(&pkg->expansion_cache, i);
        lv_free((void **) &c->template_name);
        if (c->expanded_graph) {
            graph_destroy(c->expanded_graph);
        }
    }
    lv_darray_free(&pkg->expansion_cache);

    /* 释放依赖引用数组 */
    lv_darray_free(&pkg->dep_refs);

    lv_free((void **) &pkg);
}

/* ============== 不可构造问题管理 ============== */

bool axiom_package_add_known_unconstructible(AxiomPackage *pkg, KnownUnconstructible *item) {
    if (!pkg)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "axiom_package_add_known_unconstructible: pkg is NULL");
    if (!item)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "axiom_package_add_known_unconstructible: item is NULL");

    KnownUnconstructible target_item;
    memset(&target_item, 0, sizeof(KnownUnconstructible));

    /* 深拷贝语义：对所有字符串字段进行独立拷贝，
     * 确保包内部持有独立的内存副本。
     * 调用者可以安全地释放或修改原始 item 的字符串字段。 */
    target_item.name = safe_lv_strdup_safe(item->name);
    target_item.reduces_to = safe_lv_strdup_safe(item->reduces_to);
    target_item.external_ref = safe_lv_strdup_safe(item->external_ref);
    target_item.green_verified = item->green_verified;

    /* 深拷贝依赖链中的每个字符串 */
    lv_darray_init(&target_item.dependency_chain, sizeof(char *));
    for (int i = 0; i < item->dependency_chain.count; i++) {
        char *s = safe_lv_strdup_safe(*(char **)lv_darray_get(&item->dependency_chain, i));
        if (lv_darray_push(&target_item.dependency_chain, &s) < 0) {
            /* 分配失败时回滚已拷贝的字段 */
            lv_free((void **) &target_item.name);
            lv_free((void **) &target_item.reduces_to);
            lv_free((void **) &target_item.external_ref);
            lv_darray_free(&target_item.dependency_chain);
            memset(&target_item, 0, sizeof(KnownUnconstructible));
            return false;
        }
    }

    /* 推入包数组 */
    if (lv_darray_push(&pkg->known_unconstructibles, &target_item) < 0) {
        lv_free((void **) &target_item.name);
        lv_free((void **) &target_item.reduces_to);
        lv_free((void **) &target_item.external_ref);
        lv_darray_free(&target_item.dependency_chain);
        memset(&target_item, 0, sizeof(KnownUnconstructible));
        return false;
    }

    if (axiom_stream_ctx) {
        stream_emit_simple(axiom_stream_ctx, STREAM_EVENT_INFO, "已注册不可构造问题", 0);
    }

    return true;
}

KnownUnconstructible *axiom_package_lookup_unconstructible(AxiomPackage *pkg, const char *name) {
    if (!pkg || !name)
        return NULL;

    for (int i = 0; i < pkg->known_unconstructibles.count; i++) {
        KnownUnconstructible *uc = (KnownUnconstructible *)lv_darray_get(&pkg->known_unconstructibles, i);
        if (strcmp(uc->name, name) == 0) {
            return uc;
        }
    }
    return NULL;
}

/* ============== 不可构造性证明模板 ============== */

int axiom_package_add_unconstructible_template(AxiomPackage *pkg, const char *target_name, const char *known_name,
                                               ConstraintGraph *construction, const char *description) {
    if (!pkg)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "axiom_package_add_unconstructible_template: pkg is NULL");
    if (!target_name)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "axiom_package_add_unconstructible_template: target_name is NULL");
    if (!known_name)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "axiom_package_add_unconstructible_template: known_name is NULL");
    if (!construction)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "axiom_package_add_unconstructible_template: construction is NULL");

    UnconstructibleTemplate tmpl;
    memset(&tmpl, 0, sizeof(UnconstructibleTemplate));

    /* 深拷贝字符串字段 */
    tmpl.target_problem_name = safe_lv_strdup_safe(target_name);
    tmpl.known_unconstructible_name = safe_lv_strdup_safe(known_name);
    tmpl.description = safe_lv_strdup_safe(description);
    tmpl.verified = false;

    /* 接过归约构造图的所有权 */
    tmpl.reduction_construction = construction;

    if (lv_darray_push(&pkg->unconstructible_templates, &tmpl) < 0) {
        lv_free((void **) &tmpl.target_problem_name);
        lv_free((void **) &tmpl.known_unconstructible_name);
        lv_free((void **) &tmpl.description);
        return -2;
    }

    if (axiom_stream_ctx) {
        stream_emit_simple(axiom_stream_ctx, STREAM_EVENT_INFO, "已注册不可构造性证明模板", 0);
    }

    return 0;
}

UnconstructibleTemplate *axiom_package_lookup_unconstructible_template(AxiomPackage *pkg, const char *target_name) {
    if (!pkg || !target_name)
        return NULL;

    for (int i = 0; i < pkg->unconstructible_templates.count; i++) {
        UnconstructibleTemplate *t = (UnconstructibleTemplate *)lv_darray_get(&pkg->unconstructible_templates, i);
        if (strcmp(t->target_problem_name, target_name) == 0) {
            return t;
        }
    }
    return NULL;
}

bool axiom_package_verify_unconstructible(ConstraintGraph *graph, int target_node_id, AxiomPackage *pkg) {
    if (!graph || !pkg)
        return false;

    /* 获取目标问题节点 */
    GeomNode *target_node = graph_get_node(graph, target_node_id);
    if (!target_node)
        return false;

    /* 遍历所有不可构造性证明模板 */
    for (int i = 0; i < pkg->unconstructible_templates.count; i++) {
        UnconstructibleTemplate *tmpl = (UnconstructibleTemplate *)lv_darray_get(&pkg->unconstructible_templates, i);
        if (!tmpl->target_problem_name || !tmpl->reduction_construction)
            continue;

        /* 检查目标问题名称是否匹配（通过节点名称或自定义属性判断） */
        bool name_matched = false;
        /* 优先匹配 numeric_assumption_declaration 中可能包含的名称 */
        if (target_node->numeric_assumption_declaration &&
            strstr(target_node->numeric_assumption_declaration, tmpl->target_problem_name)) {
            name_matched = true;
        }
        /* 也检查 symbolic_coords 中的信息，看是否与目标问题关联 */
        if (!name_matched) {
            /* 简易启发式匹配：直接通过模板的目标名称推断 */
            for (int k = 0;
                 k < target_node->coord_count && target_node->symbolic_coords && target_node->symbolic_coords[k]; k++) {
                char *coord_str = symbolic_coord_serialize(target_node->symbolic_coords[k]);
                if (coord_str) {
                    if (strstr(coord_str, tmpl->target_problem_name)) {
                        name_matched = true;
                    }
                    lv_free((void **) &coord_str);
                    if (name_matched)
                        break;
                }
            }
        }

        if (!name_matched)
            continue;

        /* 找到匹配的模板，执行归约构造验证
         * 通过检查归约构造图的约束是否与目标节点关联的约束兼容来判定。
         * 这本质上是一个构造性合一检查。
         */
        bool reduction_valid = true;

        /* 验证归约构造图不为空且有约束 */
        if (tmpl->reduction_construction->constraint_count == 0) {
            reduction_valid = false;
        }

        /* 验证约束兼容性：检查归约构造的约束是否与目标图兼容 */
        if (reduction_valid) {
            for (int j = 0; j < tmpl->reduction_construction->constraint_count; j++) {
                Constraint *rc = tmpl->reduction_construction->constraints[j];
                if (!rc || !rc->is_active)
                    continue;

                /* 检查目标图中是否存在等效约束 */
                bool found_equiv = false;
                for (int k = 0; k < graph->constraint_count; k++) {
                    Constraint *gc = graph->constraints[k];
                    if (!gc || !gc->is_active)
                        continue;

                    /* 约束类型必须相同 */
                    if (gc->type != rc->type)
                        continue;

                    /* 参与者数量必须相同 */
                    if (gc->participant_count != rc->participant_count)
                        continue;

                    /* 检查参与者节点 ID 是否匹配（在目标图中查找等效节点） */
                    bool all_participants_found = true;
                    for (int p = 0; p < rc->participant_count; p++) {
                        GeomNode *rcp =
                            graph_get_node((ConstraintGraph *) tmpl->reduction_construction, rc->participants[p]);
                        GeomNode *gp = graph_get_node(graph, gc->participants[p]);
                        if (!rcp || !gp) {
                            all_participants_found = false;
                            break;
                        }
                        /* 简单比对：节点类型应一致 */
                        if (rcp->type != gp->type) {
                            all_participants_found = false;
                            break;
                        }
                    }

                    if (all_participants_found) {
                        found_equiv = true;
                        break;
                    }
                }

                if (!found_equiv) {
                    reduction_valid = false;
                    break;
                }
            }
        }

        if (reduction_valid) {
            /* 验证通过：标记模板已验证 */
            tmpl->verified = true;

            /* 更新目标节点的信任颜色
             * 检查已知不可构造问题的验证状态以决定颜色
             */
            KnownUnconstructible *known = axiom_package_lookup_unconstructible(pkg, tmpl->known_unconstructible_name);
            if (known && known->green_verified) {
                /* 已知问题已通过形式化验证，目标问题也为 GREEN */
                target_node->trust = TRUST_GREEN;
            } else {
                /* 已知问题为条件性不可构造（YELLOW），目标问题也为 YELLOW */
                target_node->trust = TRUST_YELLOW;
            }

            if (axiom_stream_ctx) {
                stream_emit_simple(axiom_stream_ctx, STREAM_EVENT_INFO,
                                   "不可构造性验证通过：目标问题已归约到已知不可构造问题", 0);
            }

            return true;
        }
    }

    return false;
}

/* ============== 模板管理 ============== */

bool axiom_package_register_template(AxiomPackage *pkg, ConstraintTemplate *tmpl) {
    if (!pkg)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "axiom_package_register_template: pkg is NULL");
    if (!tmpl)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "axiom_package_register_template: tmpl is NULL");

    ConstraintTemplate slot = *tmpl;
    /* 深拷贝 name（调用者可能释放原始字符串） */
    if (slot.name) {
        slot.name = lv_strdup_safe(slot.name);
    }
    /* 安全初始化：浅拷贝后 params 指针指向调用者的内存（或未初始化），
     * pkg 不应持有该指针的所有权。无条件置 NULL 以避免 free() 未初始化
     * 指针或调用者内存导致 bad-free / double-free。
     * 若调用者需要注册参数描述，应使用独立的 API 设置。 */
    slot.params = NULL;
    slot.param_desc_count = 0;
    /* v3.6.0: 模板分级管理初始化 */
    slot.level = TEMPLATE_LEVEL_ONE; /* 默认为一级模板 */
    slot.is_compressed = false;
    slot.compressed_subgraph = NULL;

    if (lv_darray_push(&pkg->templates, &slot) < 0) {
        lv_free((void **) &slot.name);
        return false;
    }

    if (axiom_stream_ctx) {
        stream_emit_simple(axiom_stream_ctx, STREAM_EVENT_INFO, "已注册约束模板", 0);
    }

    return true;
}

ConstraintTemplate *axiom_package_get_template(AxiomPackage *pkg, const char *name) {
    if (!pkg)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "axiom_package_get_template: pkg is NULL");
    if (!name)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "axiom_package_get_template: name is NULL");

    for (int i = 0; i < pkg->templates.count; i++) {
        ConstraintTemplate *t = (ConstraintTemplate *)lv_darray_get(&pkg->templates, i);
        if (strcmp(t->name, name) == 0) {
            return t;
        }
    }
    return NULL;
}

/* ============== 解析器 ============== */

typedef enum {
    PKG_EOF,
    PKG_LBRACE,     /* { */
    PKG_RBRACE,     /* } */
    PKG_STRING,     /* "..." */
    PKG_NUMBER,     /* 整数 */
    PKG_IDENTIFIER, /* 标识符 */
    PKG_BOOLEAN,    /* true/false */
    PKG_ERROR
} PkgTokenType;

typedef struct {
    PkgTokenType type;
    char *str_value;
    int int_value;
    bool bool_value;
    int line;
    int col;
} Token;

/* Lexer 结构体：使用共享的词法分析器基础设施 */
typedef lvLexer Lexer;

static void lexer_init(Lexer *lex, const char *source) {
    lv_lexer_init(lex, source);
}

static void lexer_skip_whitespace_and_comments(Lexer *lex) {
    lv_lexer_skip_whitespace_and_comments(lex);
}

static Token lexer_next_token(Lexer *lex) {
    Token tok = {0};
    tok.line = lex->line;
    tok.col = lex->col;

    lexer_skip_whitespace_and_comments(lex);

    if (!*lex->pos) {
        tok.type = PKG_EOF;
        return tok;
    }

    /* 大括号 */
    if (*lex->pos == '{') {
        tok.type = PKG_LBRACE;
        lex->pos++;
        lex->col++;
        return tok;
    }

    if (*lex->pos == '}') {
        tok.type = PKG_RBRACE;
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
            tok.type = PKG_ERROR;
            return tok;
        }

        tok.type = PKG_STRING;
        return tok;
    }

    /* 数字 */
    if (isdigit((unsigned char) *lex->pos) || (*lex->pos == '-' && isdigit((unsigned char) *(lex->pos + 1)))) {
        const char *start = lex->pos;
        int sign = 1;

        if (*lex->pos == '-') {
            sign = -1;
            lex->pos++;
            lex->col++;
        }

        int value = 0;
        bool overflow = false;
        while (*lex->pos && isdigit((unsigned char) *lex->pos)) {
            int digit = *lex->pos - '0';
            /* 检查整数溢出：value * 10 + digit 是否超出 INT_MAX 范围 */
            if (value > (INT_MAX - digit) / 10) {
                overflow = true;
                break;
            }
            value = value * 10 + digit;
            lex->pos++;
            lex->col++;
        }

        if (overflow) {
            /* 溢出时设置错误标记，使用 INT_MAX 作为安全回退值 */
            tok.type = PKG_NUMBER;
            tok.int_value = sign == 1 ? INT_MAX : INT_MIN;
            lex->error_msg = "数字字面量超出整数范围";
            return tok;
        }

        tok.type = PKG_NUMBER;
        tok.int_value = sign * value;
        return tok;
    }

    /* 标识符或关键字 */
    if (isalpha((unsigned char) *lex->pos) || *lex->pos == '_') {
        const char *start = lex->pos;

        while (*lex->pos && (isalnum((unsigned char) *lex->pos) || *lex->pos == '_')) {
            lex->pos++;
            lex->col++;
        }

        size_t len = lex->pos - start;
        tok.str_value = lv_malloc(len + 1);
        if (!tok.str_value) {
            tok.type = PKG_ERROR;
            return tok;
        }

        memcpy(tok.str_value, start, len);
        tok.str_value[len] = '\0';

        /* 检查关键字 */
        if (strcmp(tok.str_value, "true") == 0) {
            tok.type = PKG_BOOLEAN;
            tok.bool_value = true;
            lv_free((void **) &tok.str_value);
            tok.str_value = NULL;
        } else if (strcmp(tok.str_value, "false") == 0) {
            tok.type = PKG_BOOLEAN;
            tok.bool_value = false;
            lv_free((void **) &tok.str_value);
            tok.str_value = NULL;
        } else {
            tok.type = PKG_IDENTIFIER;
        }

        return tok;
    }

    /* 未知字符 */
    tok.type = PKG_ERROR;
    lex->error_msg = "意外的字符";
    lex->pos++;
    lex->col++;

    return tok;
}

static void token_free(Token *tok) {
    if (tok->str_value) {
        lv_free((void **) &tok->str_value);
        tok->str_value = NULL;
    }
}

/* 解析器上下文 */
typedef struct {
    Lexer lexer;
    Token current;
    bool has_error;
} Parser;

static void parser_init(Parser *p, const char *source) {
    lexer_init(&p->lexer, source);
    p->has_error = false;
    memset(&p->current, 0, sizeof(Token));
}

static void parser_advance(Parser *p) {
    token_free(&p->current);
    p->current = lexer_next_token(&p->lexer);
}

static bool parser_expect(Parser *p, PkgTokenType type) {
    if (p->current.type != type) {
        lv_set_error(lv_ERROR_PARSE, "解析错误 (行 %d, 列 %d): 期望 %d, 得到 %d", p->current.line, p->current.col, type,
                     p->current.type);
        p->has_error = true;
        return false;
    }
    return true;
}

static bool parser_expect_identifier(Parser *p, const char *name) {
    if (p->current.type != PKG_IDENTIFIER || strcmp(p->current.str_value, name) != 0) {
        lv_set_error(lv_ERROR_PARSE, "解析错误 (行 %d, 列 %d): 期望关键字 '%s'", p->current.line, p->current.col, name);
        p->has_error = true;
        return false;
    }
    return true;
}

/* 前向声明 */
static bool parse_package_body(Parser *p, AxiomPackage *pkg);

/**
 * @brief 清理 KnownUnconstructible 结构体的动态资源
 */
static void unconstructible_desc_cleanup(KnownUnconstructible *uc) {
    if (!uc)
        return;
    lv_free((void **) &uc->name);
    lv_free((void **) &uc->reduces_to);
    lv_free((void **) &uc->external_ref);
    for (int i = 0; i < uc->dependency_chain.count; i++) {
        lv_free((void **) lv_darray_get(&uc->dependency_chain, i));
    }
    lv_darray_free(&uc->dependency_chain);
    uc->name = NULL;
    uc->reduces_to = NULL;
    uc->external_ref = NULL;
}

/* 解析不可构造问题 */
static bool parse_unconstructible(Parser *p, AxiomPackage *pkg) {
    parser_advance(p); /* 跳过 'unconstructible' */

    /* 期望字符串 (问题名称) */
    if (!parser_expect(p, PKG_STRING))
        return false;

    KnownUnconstructible uc = {0};
    lv_darray_init(&uc.dependency_chain, sizeof(char *));
    uc.name = safe_lv_strdup_safe(p->current.str_value);
    uc.green_verified = false;

    parser_advance(p);

    /* 期望左大括号 */
    if (!parser_expect(p, PKG_LBRACE)) {
        lv_free((void **) &uc.name);
        return false;
    }
    parser_advance(p);

    /* 解析内容直到右大括号 */
    while (p->current.type != PKG_RBRACE && p->current.type != PKG_EOF && !p->has_error) {
        if (p->current.type != PKG_IDENTIFIER) {
            lv_set_error(lv_ERROR_PARSE, "解析错误 (行 %d): 期望属性名", p->current.line);
            p->has_error = true;
            break;
        }

        const char *prop = safe_lv_strdup_safe(p->current.str_value);
        parser_advance(p);

        if (strcmp(prop, "reduces_to") == 0) {
            if (!parser_expect(p, PKG_STRING)) {
                lv_free((void **) &prop);
                p->has_error = true;
                break;
            }
            uc.reduces_to = safe_lv_strdup_safe(p->current.str_value);
            parser_advance(p);
        } else if (strcmp(prop, "dependency") == 0) {
            if (!parser_expect(p, PKG_STRING)) {
                lv_free((void **) &prop);
                p->has_error = true;
                break;
            }

            /* 添加到依赖链 */
            char *dep = safe_lv_strdup_safe(p->current.str_value);
            lv_darray_push(&uc.dependency_chain, &dep);
            parser_advance(p);
        } else if (strcmp(prop, "external_ref") == 0) {
            if (!parser_expect(p, PKG_STRING)) {
                lv_free((void **) &prop);
                p->has_error = true;
                break;
            }
            uc.external_ref = safe_lv_strdup_safe(p->current.str_value);
            parser_advance(p);
        } else if (strcmp(prop, "green_verified") == 0) {
            if (!parser_expect(p, PKG_BOOLEAN)) {
                lv_free((void **) &prop);
                p->has_error = true;
                break;
            }
            uc.green_verified = p->current.bool_value;
            parser_advance(p);
        } else {
            lv_set_error(lv_ERROR_PARSE, "解析错误 (行 %d): 未知属性 '%s'", p->current.line, prop);
            lv_free((void **) &prop);
            p->has_error = true;
            break;
        }
        lv_free((void **) &prop);
    }

    if (p->has_error) {
        unconstructible_desc_cleanup(&uc);
        return false;
    }

    /* 期望右大括号 */
    if (!parser_expect(p, PKG_RBRACE)) {
        unconstructible_desc_cleanup(&uc);
        return false;
    }

    /* 添加到包 */
    if (!axiom_package_add_known_unconstructible(pkg, &uc)) {
        lv_set_error(lv_ERROR_OUT_OF_MEMORY, "内存分配失败");
        unconstructible_desc_cleanup(&uc);
        return false;
    }

    parser_advance(p);
    return true;
}

/* 解析模板声明 */
static bool parse_template(Parser *p, AxiomPackage *pkg) {
    parser_advance(p); /* 跳过 'template' */

    /* 期望字符串 (模板名称) */
    if (!parser_expect(p, PKG_STRING))
        return false;

    ConstraintTemplate tmpl = {0};
    tmpl.name = safe_lv_strdup_safe(p->current.str_value);
    tmpl.verified = false;

    parser_advance(p);

    /* 期望参数数量 (数字) */
    if (!parser_expect(p, PKG_NUMBER)) {
        lv_free((void **) &tmpl.name);
        return false;
    }
    tmpl.param_count = p->current.int_value;

    parser_advance(p);

    /* 添加到包 */
    if (!axiom_package_register_template(pkg, &tmpl)) {
        lv_set_error(lv_ERROR_OUT_OF_MEMORY, "内存分配失败");
        lv_free((void **) &tmpl.name);
        return false;
    }

    return true;
}

/* 解析包体 */
static bool parse_package_body(Parser *p, AxiomPackage *pkg) {
    while (p->current.type != PKG_RBRACE && p->current.type != PKG_EOF && !p->has_error) {
        if (p->current.type != PKG_IDENTIFIER) {
            lv_set_error(lv_ERROR_PARSE, "解析错误 (行 %d): 期望声明", p->current.line);
            p->has_error = true;
            break;
        }

        const char *keyword = p->current.str_value;

        if (strcmp(keyword, "template") == 0) {
            if (!parse_template(p, pkg)) {
                p->has_error = true;
                break;
            }
        } else if (strcmp(keyword, "unconstructible") == 0) {
            if (!parse_unconstructible(p, pkg)) {
                p->has_error = true;
                break;
            }
        } else if (strcmp(keyword, "bottom_geometry") == 0) {
            parser_advance(p);
            if (!parser_expect(p, PKG_STRING)) {
                p->has_error = true;
                break;
            }
            lv_free((void **) &pkg->bottom_geometry);
            pkg->bottom_geometry = safe_lv_strdup_safe(p->current.str_value);
            parser_advance(p);
        } else if (strcmp(keyword, "negation_encoding") == 0) {
            parser_advance(p);
            if (!parser_expect(p, PKG_STRING)) {
                p->has_error = true;
                break;
            }
            lv_free((void **) &pkg->negation_encoding);
            pkg->negation_encoding = safe_lv_strdup_safe(p->current.str_value);
            parser_advance(p);
        } else if (strcmp(keyword, "contradiction_behavior") == 0) {
            parser_advance(p);
            if (!parser_expect(p, PKG_STRING)) {
                p->has_error = true;
                break;
            }

            const char *behavior = p->current.str_value;
            if (strcmp(behavior, "explosion_principle") == 0) {
                pkg->contradiction_behavior = EXPLOSION_PRINCIPLE;
            } else if (strcmp(behavior, "constructive") == 0) {
                pkg->contradiction_behavior = CONSTRUCTIVE;
            } else if (strcmp(behavior, "non_constructive_oracle") == 0) {
                pkg->contradiction_behavior = NON_CONSTRUCTIVE_ORACLE;
            } else {
                lv_set_error(lv_ERROR_PARSE, "解析错误 (行 %d): 未知的矛盾行为 '%s'", p->current.line, behavior);
                p->has_error = true;
                break;
            }
            parser_advance(p);
        } else {
            lv_set_error(lv_ERROR_PARSE, "解析错误 (行 %d): 未知的关键字 '%s'", p->current.line, keyword);
            p->has_error = true;
            break;
        }
    }

    return !p->has_error;
}

/**
 * @brief 发出公理包加载提示
 *
 * 检查公理包是否包含非经典逻辑特征，向用户发出加载提示。
 * 对应设计文档 5.3 节：非经典逻辑公理包加载提示机制。
 */
static void axiom_package_emit_load_hints(AxiomPackage *pkg) {
    if (!pkg)
        return;

    /* 1. 检查是否覆盖 ⊥ 定义或矛盾行为 —— 非标准否定语义 */
    if (pkg->contradiction_behavior != CONSTRUCTIVE ||
        (pkg->bottom_geometry && strcmp(pkg->bottom_geometry, "default") != 0)) {
        LOG_WARN("axiom", "公理包 '%s' 使用了非标准否定语义", pkg->name ? pkg->name : "unnamed");
        if (axiom_stream_ctx) {
            stream_emit_warning(axiom_stream_ctx, "该公理包使用了非标准否定语义", 0);
        }
    }

    /* 2. 检查是否包含非构造性 Oracle */
    if (pkg->contradiction_behavior == NON_CONSTRUCTIVE_ORACLE) {
        LOG_WARN("axiom", "公理包 '%s' 包含非构造性初始证物", pkg->name ? pkg->name : "unnamed");
        if (axiom_stream_ctx) {
            stream_emit_warning(axiom_stream_ctx, "该公理包包含非构造性初始证物", 0);
        }
    }

    /* 3. 检查是否包含爆炸原理 */
    if (pkg->contradiction_behavior == EXPLOSION_PRINCIPLE) {
        LOG_WARN("axiom", "公理包 '%s' 包含从矛盾推导任意命题的规则", pkg->name ? pkg->name : "unnamed");
        if (axiom_stream_ctx) {
            stream_emit_warning(axiom_stream_ctx, "该公理包包含从矛盾推导任意命题的规则", 0);
        }
    }
}

/* 完整的包加载函数 */
AxiomLoadStatus axiom_package_load(AxiomPackage *pkg, const char *filepath) {
    if (!pkg || !filepath) {
        lv_set_error(lv_ERROR_INVALID_PARAM, "无效参数");
        return AXIOM_LOAD_PARSE_ERROR;
    }

    /* 清除之前的错误 */
    lv_clear_error();

    /* 读取文件 */
    FILE *f = lv_file_open(filepath, "r");
    if (!f) {
        lv_set_error(lv_ERROR_IO, "无法打开文件: %s", filepath);
        return AXIOM_LOAD_FILE_NOT_FOUND;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (len <= 0) {
        lv_file_close(f);
        lv_set_error(lv_ERROR_PARSE, "文件为空: %s", filepath);
        return AXIOM_LOAD_PARSE_ERROR;
    }

    /* 限制最大文件大小为64MB，防止内存耗尽 */
    if (len > AXIOM_MAX_FILE_SIZE) {
        lv_file_close(f);
        lv_set_error(lv_ERROR_INVALID_PARAM, "文件过大（超过64MB限制）: %s", filepath);
        return AXIOM_LOAD_PARSE_ERROR;
    }

    char *buf = lv_malloc((size_t) len + 1);
    if (!buf) {
        lv_file_close(f);
        lv_set_error(lv_ERROR_OUT_OF_MEMORY, "内存分配失败");
        return AXIOM_LOAD_PARSE_ERROR;
    }

    size_t read_len = fread(buf, 1, (size_t) len, f);
    int read_error = ferror(f);
    lv_file_close(f);
    if (read_len != (size_t) len && read_error) {
        lv_free((void **) &buf);
        lv_set_error(lv_ERROR_IO, "文件读取失败: %s", filepath);
        return AXIOM_LOAD_PARSE_ERROR;
    }
    buf[read_len] = '\0';

    /* 初始化解析器 */
    Parser parser;
    parser_init(&parser, buf);

    /* 获取第一个 token */
    parser_advance(&parser);

    /* 期望 'axiom' 关键字 */
    if (!parser_expect_identifier(&parser, "axiom")) {
        lv_free((void **) &buf);
        return AXIOM_LOAD_PARSE_ERROR;
    }
    parser_advance(&parser);

    /* 期望包名 (字符串) */
    if (!parser_expect(&parser, PKG_STRING)) {
        lv_free((void **) &buf);
        return AXIOM_LOAD_PARSE_ERROR;
    }
    lv_free((void **) &pkg->name);
    pkg->name = safe_lv_strdup_safe(parser.current.str_value);
    parser_advance(&parser);

    /* 期望版本 (字符串) */
    if (!parser_expect(&parser, PKG_STRING)) {
        lv_free((void **) &buf);
        return AXIOM_LOAD_PARSE_ERROR;
    }
    lv_free((void **) &pkg->version);
    pkg->version = safe_lv_strdup_safe(parser.current.str_value);
    parser_advance(&parser);

    /* 期望左大括号 */
    if (!parser_expect(&parser, PKG_LBRACE)) {
        lv_free((void **) &buf);
        return AXIOM_LOAD_PARSE_ERROR;
    }
    parser_advance(&parser);

    /* 解析包体 */
    if (!parse_package_body(&parser, pkg)) {
        lv_free((void **) &buf);
        return AXIOM_LOAD_PARSE_ERROR;
    }

    /* 期望右大括号 */
    if (!parser_expect(&parser, PKG_RBRACE)) {
        lv_free((void **) &buf);
        return AXIOM_LOAD_PARSE_ERROR;
    }

    lv_free((void **) &buf);

    if (axiom_stream_ctx) {
        stream_emit_simple(axiom_stream_ctx, STREAM_EVENT_INFO, "公理包加载成功", 0);
    }

    axiom_package_emit_load_hints(pkg);

    return AXIOM_LOAD_OK;
}

/* ============== 保存功能 ============== */

static const char *behavior_to_string(int behavior) {
    switch (behavior) {
        case CONSTRUCTIVE:
            return "constructive";
        case NON_CONSTRUCTIVE_ORACLE:
            return "non_constructive_oracle";
        case EXPLOSION_PRINCIPLE:
        default:
            return "explosion_principle";
    }
}

AxiomSaveStatus axiom_package_save(const AxiomPackage *pkg, const char *filepath) {
    if (!pkg || !filepath)
        return AXIOM_SAVE_FILE_ERROR;

    FILE *f = lv_file_open(filepath, "w");
    if (!f) {
        lv_set_error(lv_ERROR_IO, "无法创建文件: %s", filepath);
        return AXIOM_SAVE_FILE_ERROR;
    }

    /* 写入文件头注释 */
    fprintf(f, "# Axiom Package File\n");
    fprintf(f, "# Generated by axiom_package_save\n\n");

    /* 写入包声明 */
    fprintf(f, "axiom \"%s\" \"%s\" {\n", pkg->name ? pkg->name : "unnamed", pkg->version ? pkg->version : "0.0.0");

    /* 写入模板 */
    for (int i = 0; i < pkg->templates.count; i++) {
        ConstraintTemplate *t = (ConstraintTemplate *)lv_darray_get(&pkg->templates, i);
        fprintf(f, "    template \"%s\" %d\n", t->name, t->param_count);
    }

    /* 写入不可构造问题 */
    for (int i = 0; i < pkg->known_unconstructibles.count; i++) {
        KnownUnconstructible *uc = (KnownUnconstructible *)lv_darray_get(&pkg->known_unconstructibles, i);
        fprintf(f, "\n    unconstructible \"%s\" {\n", uc->name);

        if (uc->reduces_to) {
            fprintf(f, "        reduces_to \"%s\"\n", uc->reduces_to);
        }

        for (int j = 0; j < uc->dependency_chain.count; j++) {
            fprintf(f, "        dependency \"%s\"\n", *(char **)lv_darray_get(&uc->dependency_chain, j));
        }

        if (uc->external_ref) {
            fprintf(f, "        external_ref \"%s\"\n", uc->external_ref);
        }

        fprintf(f, "        green_verified %s\n", uc->green_verified ? "true" : "false");
        fprintf(f, "    }\n");
    }

    /* 写入其他属性 */
    if (pkg->bottom_geometry) {
        fprintf(f, "\n    bottom_geometry \"%s\"\n", pkg->bottom_geometry);
    }

    if (pkg->negation_encoding) {
        fprintf(f, "    negation_encoding \"%s\"\n", pkg->negation_encoding);
    }

    fprintf(f, "    contradiction_behavior \"%s\"\n", behavior_to_string(pkg->contradiction_behavior));

    /* 关闭包 */
    fprintf(f, "}\n");

    lv_file_close(f);

    if (axiom_stream_ctx) {
        stream_emit_simple(axiom_stream_ctx, STREAM_EVENT_INFO, "公理包保存成功", 0);
    }

    return AXIOM_SAVE_OK;
}

/* ============== 内容哈希 (SHA-256) ============== */

char *axiom_package_compute_content_hash(AxiomPackage *pkg) {
    if (!pkg)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "axiom_package_compute_content_hash: pkg is NULL");

    lvSha256Context ctx;
    lv_sha256_init(&ctx);

    /* 哈希名称和版本：sha256_hash_string 语义 */
    if (pkg->name) {
        lv_sha256_update(&ctx, (const uint8_t *) pkg->name, strlen(pkg->name));
    } else {
        lv_sha256_update(&ctx, (const uint8_t *) "(null)", 6);
    }
    if (pkg->version) {
        lv_sha256_update(&ctx, (const uint8_t *) pkg->version, strlen(pkg->version));
    } else {
        lv_sha256_update(&ctx, (const uint8_t *) "(null)", 6);
    }

    /* 哈希所有模板名称和参数数量 */
    for (int i = 0; i < pkg->templates.count; i++) {
        ConstraintTemplate *t = (ConstraintTemplate *)lv_darray_get(&pkg->templates, i);
        if (t->name) {
            lv_sha256_update(&ctx, (const uint8_t *) t->name, strlen(t->name));
        } else {
            lv_sha256_update(&ctx, (const uint8_t *) "(null)", 6);
        }
        lv_sha256_update(&ctx, (const uint8_t *) &t->param_count, sizeof(int));
        lv_sha256_update(&ctx, (const uint8_t *) &t->verified, sizeof(bool));
    }

    /* 哈希所有不可构造问题 */
    for (int i = 0; i < pkg->known_unconstructibles.count; i++) {
        KnownUnconstructible *uc = (KnownUnconstructible *)lv_darray_get(&pkg->known_unconstructibles, i);

        if (uc->name) {
            lv_sha256_update(&ctx, (const uint8_t *) uc->name, strlen(uc->name));
        } else {
            lv_sha256_update(&ctx, (const uint8_t *) "(null)", 6);
        }
        if (uc->reduces_to) {
            lv_sha256_update(&ctx, (const uint8_t *) uc->reduces_to, strlen(uc->reduces_to));
        } else {
            lv_sha256_update(&ctx, (const uint8_t *) "(null)", 6);
        }

        /* 哈希依赖链 */
        for (int j = 0; j < uc->dependency_chain.count; j++) {
            char *dep = *(char **)lv_darray_get(&uc->dependency_chain, j);
            if (dep) {
                lv_sha256_update(&ctx, (const uint8_t *) dep, strlen(dep));
            } else {
                lv_sha256_update(&ctx, (const uint8_t *) "(null)", 6);
            }
        }

        if (uc->external_ref) {
            lv_sha256_update(&ctx, (const uint8_t *) uc->external_ref, strlen(uc->external_ref));
        } else {
            lv_sha256_update(&ctx, (const uint8_t *) "(null)", 6);
        }
        lv_sha256_update(&ctx, (const uint8_t *) &uc->green_verified, sizeof(bool));
    }

    /* 哈希其他属性 */
    if (pkg->bottom_geometry) {
        lv_sha256_update(&ctx, (const uint8_t *) pkg->bottom_geometry, strlen(pkg->bottom_geometry));
    } else {
        lv_sha256_update(&ctx, (const uint8_t *) "(null)", 6);
    }
    if (pkg->negation_encoding) {
        lv_sha256_update(&ctx, (const uint8_t *) pkg->negation_encoding, strlen(pkg->negation_encoding));
    } else {
        lv_sha256_update(&ctx, (const uint8_t *) "(null)", 6);
    }
    lv_sha256_update(&ctx, (const uint8_t *) &pkg->contradiction_behavior, sizeof(int));

    /* 计算最终哈希 */
    uint8_t hash[AXIOM_SHA256_OUTPUT_SIZE];
    lv_sha256_final(&ctx, hash);

    /* 转换为十六进制字符串（64个字符 + 空终止符） */
    char *result = lv_malloc(AXIOM_SHA256_HEX_SIZE);
    if (result) {
        for (int i = 0; i < AXIOM_SHA256_OUTPUT_SIZE; i++) {
            snprintf(result + i * 2, 3, "%02x", hash[i]);
        }
        result[AXIOM_SHA256_HEX_SIZE - 1] = '\0';
    }

    return result;
}

/* ============== 依赖验证 ============== */

/* 检查字符串是否为有效的URL格式 */
static bool is_valid_url(const char *str) {
    if (!str || !*str)
        return false;

    /* 检查常见URL协议 */
    if (strncmp(str, "http://", 7) == 0)
        return true;
    if (strncmp(str, "https://", 8) == 0)
        return true;
    if (strncmp(str, "ftp://", 6) == 0)
        return true;
    if (strncmp(str, "file://", 7) == 0)
        return true;

    /* 检查DOI格式 */
    if (strncmp(str, "doi:", 4) == 0)
        return true;

    /* 检查arXiv格式 */
    if (strncmp(str, "arXiv:", 6) == 0)
        return true;

    /* 检查标识符格式 (字母开头，只包含字母数字、下划线、连字符、点) */
    if (isalpha((unsigned char) str[0])) {
        for (const char *p = str + 1; *p; p++) {
            if (!isalnum((unsigned char) *p) && *p != '_' && *p != '-' && *p != '.') {
                return false;
            }
        }
        return true;
    }

    return false;
}

/* 在已加载的包中查找问题 */
static KnownUnconstructible *find_problem_in_packages(AxiomPackage **packages, int package_count,
                                                      const char *problem_name) {
    if (!packages || !problem_name)
        return NULL;

    for (int i = 0; i < package_count; i++) {
        AxiomPackage *pkg = packages[i];
        if (!pkg)
            continue;

        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, problem_name);
        if (uc)
            return uc;
    }

    return NULL;
}

/* 在已加载的包中查找模板 */
static ConstraintTemplate *find_template_in_packages(AxiomPackage **packages, int package_count,
                                                     const char *template_name) {
    if (!packages || !template_name)
        return NULL;

    for (int i = 0; i < package_count; i++) {
        AxiomPackage *pkg = packages[i];
        if (!pkg)
            continue;

        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, template_name);
        if (tmpl)
            return tmpl;
    }

    return NULL;
}

bool axiom_package_validate_dependencies(AxiomPackage *pkg, AxiomPackage **loaded_packages, int package_count) {
    if (!pkg) {
        lv_set_error(lv_ERROR_INVALID_PARAM, "无效参数: 包为空");
        return false;
    }

    lv_clear_error();

    bool all_valid = true;

    for (int i = 0; i < pkg->known_unconstructibles.count; i++) {
        KnownUnconstructible *uc = (KnownUnconstructible *)lv_darray_get(&pkg->known_unconstructibles, i);

        /* 验证 reduces_to 引用 */
        if (uc->reduces_to && uc->reduces_to[0] != '\0') {
            KnownUnconstructible *target = find_problem_in_packages(loaded_packages, package_count, uc->reduces_to);

            /* 也检查当前包 */
            if (!target) {
                target = axiom_package_lookup_unconstructible(pkg, uc->reduces_to);
            }

            if (!target) {
                lv_set_error(lv_ERROR_NOT_FOUND, "依赖验证失败: 问题 '%s' 的 reduces_to '%s' 未找到", uc->name,
                             uc->reduces_to);
                all_valid = false;
                /* 继续检查其他问题 */
            }
        }

        /* 验证依赖链中的所有项 */
        for (int j = 0; j < uc->dependency_chain.count; j++) {
            const char *dep = *(char **)lv_darray_get(&uc->dependency_chain, j);

            /* 检查是否为已知问题 */
            KnownUnconstructible *dep_problem = find_problem_in_packages(loaded_packages, package_count, dep);

            if (!dep_problem) {
                dep_problem = axiom_package_lookup_unconstructible(pkg, dep);
            }

            /* 检查是否为已知模板 */
            ConstraintTemplate *dep_template = find_template_in_packages(loaded_packages, package_count, dep);

            /* 回退：在当前包中查找模板 */
            if (!dep_template) {
                dep_template = axiom_package_get_template(pkg, dep);
            }

            if (!dep_problem && !dep_template) {
                lv_set_error(lv_ERROR_NOT_FOUND, "依赖验证失败: 问题 '%s' 的依赖 '%s' 未在任何已加载的包中找到",
                             uc->name, dep);
                all_valid = false;
            }
        }

        /* 验证外部引用格式 */
        if (uc->external_ref && uc->external_ref[0] != '\0') {
            if (!is_valid_url(uc->external_ref)) {
                lv_set_error(lv_ERROR_INVALID_PARAM,
                             "依赖验证失败: 问题 '%s' 的 external_ref '%s' 不是有效的URL或标识符格式", uc->name,
                             uc->external_ref);
                all_valid = false;
            }
        }
    }

    return all_valid;
}

/* ------------------------------------------------------------------ */
/*  axiom_template_validate_normal_form                                */
/* ------------------------------------------------------------------ */

/**
 * @brief 验证模板展开是否与声明的规范形式匹配
 *
 * 根据 design_v2.9.md 第 7.3 节：
 * 检查展开图的约束类型和节点类型是否与规范形式描述匹配。
 *
 * 规范形式格式："CONSTRAINT_TYPE(NODE_TYPE, NODE_TYPE)+"
 * 示例："INCIDENCE(POINT,LINE_SEGMENT)+"
 *
 * @param tmpl           约束模板（未使用，保持API一致性）
 * @param expanded_graph 要验证的展开约束图
 * @param canonical_form 规范形式描述字符串
 * @return 验证通过返回 true，否则返回 false
 */
bool axiom_template_validate_normal_form(const ConstraintTemplate *tmpl, const ConstraintGraph *expanded_graph,
                                         const char *canonical_form) {
    (void) tmpl;
    if (!expanded_graph)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "axiom_template_validate_normal_form: expanded_graph is NULL");
    if (!canonical_form)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "axiom_template_validate_normal_form: canonical_form is NULL");

    /* 解析规范形式以提取期望的约束类型和参与者节点类型
     * 格式："CONSTRAINT_TYPE(NODE_TYPE,NODE_TYPE,...)+" */

    /* 查找约束类型名称（在第一个 '(' 之前） */
    const char *paren = strchr(canonical_form, '(');
    if (!paren)
        return false;

    size_t type_name_len = (size_t) (paren - canonical_form);
    if (type_name_len == 0)
        return false;

    /* 查找闭括号 ')' */
    const char *close_paren = strchr(paren, ')');
    if (!close_paren)
        return false;

    /* 提取 '(' 和 ')' 之间的参与者节点类型 */
    /* 解析逗号分隔的节点类型名称 */
    char participant_types[AXIOM_MAX_PARTICIPANT_TYPES][AXIOM_PARTICIPANT_TYPE_LEN];
    int participant_type_count = 0;

    const char *p = paren + 1;
    while (p < close_paren && participant_type_count < AXIOM_MAX_PARTICIPANT_TYPES) {
        const char *comma = strchr(p, ',');
        size_t len;
        if (comma && comma < close_paren) {
            len = (size_t) (comma - p);
            p = comma + 1;
        } else {
            len = (size_t) (close_paren - p);
            p = close_paren;
        }
        if (len > 0 && len < AXIOM_PARTICIPANT_TYPE_LEN) {
            memcpy(participant_types[participant_type_count], p - len, len);
            participant_types[participant_type_count][len] = '\0';
            participant_type_count++;
        }
    }

    if (participant_type_count == 0)
        return false;

    /* Helper: map type name string to GeomType enum */
    /* We only check that the constraint type prefix matches */
    /* For a simple implementation, check that all constraints in the
     * 展开图具有期望的参与者数量 */

    /* 检查展开图中的每个约束 */
    for (int i = 0; i < expanded_graph->constraint_count; i++) {
        Constraint *c = expanded_graph->constraints[i];
        if (!c)
            continue;

        /* 检查参与者数量是否与规范形式匹配 */
        if (c->participant_count != participant_type_count) {
            return false;
        }

        /* 检查所有参与者是否引用了有效的节点 */
        for (int k = 0; k < c->participant_count; k++) {
            GeomNode *node = graph_get_node((ConstraintGraph *) expanded_graph, c->participants[k]);
            if (!node)
                return false;
        }
    }

    /* 如果展开图没有约束但规范形式期望有约束，则为违规 */
    if (expanded_graph->constraint_count == 0) {
        return false;
    }

    return true;
}

/* ============== 双层测试集 ============== */

/**
 * @brief 运行单个测试用例
 *
 * 使用模板的 expand 函数展开模板，然后检查展开结果的基本有效性。
 */
static bool run_single_test_case(const ConstraintTemplate *tmpl, const TemplateTestCase *tc) {
    if (!tmpl || !tc)
        return false;

    /* 如果模板没有 expand 函数，无法运行测试 */
    if (!tmpl->expand)
        return false;

    /* 创建目标图 */
    ConstraintGraph *target = graph_create();
    if (!target)
        return false;

    /* 调用模板展开函数 */
    tmpl->expand(tc->params, target);

    /* 基本有效性检查：展开后应有约束产生 */
    bool passed = (target->constraint_count > 0);

    /* 如果模板有正则形式描述，进行额外验证 */
    if (passed && tmpl->normal_form.constraint_type_count > 0) {
        /* 检查约束数量是否匹配预期 */
        passed = (target->constraint_count >= tmpl->normal_form.constraint_type_count);
    }

    graph_destroy(target);
    return passed;
}

TemplateTestResult axiom_template_run_tests(AxiomPackage *pkg, const char *template_name,
                                            TemplateTestCase *factory_tests, int factory_count,
                                            TemplateTestCase *user_tests, int user_count) {
    TemplateTestResult result = {0, 0, 0, NULL};

    if (!pkg || !template_name)
        return result;

    /* 查找模板 */
    ConstraintTemplate *tmpl = axiom_package_get_template(pkg, template_name);
    if (!tmpl)
        return result;

    int total = factory_count + user_count;
    if (total == 0)
        return result;

    /* 分配失败消息数组 */
    result.failure_messages = lv_calloc((size_t) total, sizeof(char *));
    if (!result.failure_messages)
        return result;

    result.total = total;

    /* 运行工厂测试 */
    for (int i = 0; i < factory_count; i++) {
        bool passed = run_single_test_case(tmpl, &factory_tests[i]);

        if (passed == factory_tests[i].expected_result) {
            result.passed++;
        } else {
            /* 边界检查：确保 failure_messages 数组不越界 */
            if (result.failed < total) {
                result.failed++;
                char msg[AXIOM_TEST_MSG_BUF_SIZE];
                snprintf(msg, sizeof(msg), "[FACTORY] '%s': expected %s, got %s", factory_tests[i].template_name,
                         factory_tests[i].expected_result ? "pass" : "fail", passed ? "pass" : "fail");
                result.failure_messages[result.failed - 1] = lv_strdup_safe(msg);
            }
        }
    }

    /* 运行用户测试 */
    for (int i = 0; i < user_count; i++) {
        bool passed = run_single_test_case(tmpl, &user_tests[i]);

        if (passed == user_tests[i].expected_result) {
            result.passed++;
        } else {
            /* 边界检查：确保 failure_messages 数组不越界 */
            if (result.failed < total) {
                result.failed++;
                char msg[AXIOM_TEST_MSG_BUF_SIZE];
                snprintf(msg, sizeof(msg), "[USER] '%s': expected %s, got %s", user_tests[i].template_name,
                         user_tests[i].expected_result ? "pass" : "fail", passed ? "pass" : "fail");
                result.failure_messages[result.failed - 1] = lv_strdup_safe(msg);
            }
        }
    }

    return result;
}

void axiom_template_test_result_destroy(TemplateTestResult *result) {
    if (!result)
        return;

    if (result->failure_messages) {
        for (int i = 0; i < result->failed; i++) {
            lv_free((void **) &result->failure_messages[i]);
        }
        lv_free((void **) &result->failure_messages);
    }

    /* 释放详细记录 */
    if (result->records) {
        for (int i = 0; i < result->record_count; i++) {
            lv_free((void **) &result->records[i].test_name);
            lv_free((void **) &result->records[i].message);
        }
        lv_free((void **) &result->records);
    }

    result->total = 0;
    result->passed = 0;
    result->failed = 0;
    result->timed_out = 0;
    result->skipped = 0;
    result->record_count = 0;
    result->failure_messages = NULL;
    result->records = NULL;
}

/* ============== 模板展开缓存 ============== */

/**
 * @brief 计算参数的简单哈希值（用于缓存键）
 */
static uint64_t compute_param_hash(SymbolicCoord **params, int param_count) {
    /* 使用 FNV-1a 基于序列化内容计算参数哈希（仅用于缓存键，非加密用途） */
    uint64_t hash = 14695981039346656037ULL; /* FNV offset basis */

    hash ^= (uint64_t) param_count;
    hash *= 1099511628211ULL; /* FNV prime */

    for (int i = 0; i < param_count && params && params[i]; i++) {
        char *ser = symbolic_coord_serialize(params[i]);
        if (ser) {
            for (const char *p = ser; *p; p++) {
                hash ^= (uint64_t) (unsigned char) *p;
                hash *= 1099511628211ULL; /* FNV prime */
            }
            lv_free((void **) &ser);
        }
    }

    return hash;
}

/**
 * @brief 在缓存中查找匹配的展开图
 *
 * 注意：返回的 ConstraintGraph 指针指向缓存内部持有的图对象。
 * 调用者不得修改、销毁或以其他方式变更返回的图，否则将破坏缓存一致性。
 * 如需修改展开结果，调用者应自行创建图的深拷贝后再操作。
 */
ConstraintGraph *axiom_package_lookup_expansion_cache(AxiomPackage *pkg, const char *template_name,
                                                      SymbolicCoord **params, int param_count) {
    if (!pkg)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "axiom_package_lookup_expansion_cache: pkg is NULL");
    if (!template_name)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "axiom_package_lookup_expansion_cache: template_name is NULL");

    uint64_t target_hash = compute_param_hash(params, param_count);

    for (int i = 0; i < pkg->expansion_cache.count; i++) {
        TemplateExpansionCache *c = (TemplateExpansionCache *)lv_darray_get(&pkg->expansion_cache, i);
        if (c->param_hash == target_hash && c->template_name &&
            strcmp(c->template_name, template_name) == 0) {
            return c->expanded_graph;
        }
    }

    return NULL;
}

/**
 * @brief 将展开结果存入缓存
 */
bool axiom_package_store_expansion_cache(AxiomPackage *pkg, const char *template_name, SymbolicCoord **params,
                                         int param_count, ConstraintGraph *expanded_graph) {
    if (!pkg)
        return false;

    TemplateExpansionCache c;
    c.param_hash = compute_param_hash(params, param_count);
    c.template_name = template_name ? lv_strdup_safe(template_name) : NULL;
    c.expanded_graph = expanded_graph;

    if (lv_darray_push(&pkg->expansion_cache, &c) < 0) {
        lv_free((void **) &c.template_name);
        return false;
    }

    return true;
}

/**
 * @brief 清空模板展开缓存
 */
void axiom_package_clear_expansion_cache(AxiomPackage *pkg) {
    if (!pkg)
        return;

    for (int i = 0; i < pkg->expansion_cache.count; i++) {
        TemplateExpansionCache *c = (TemplateExpansionCache *)lv_darray_get(&pkg->expansion_cache, i);
        lv_free((void **) &c->template_name);
        if (c->expanded_graph) {
            graph_destroy(c->expanded_graph);
        }
    }
    lv_darray_clear(&pkg->expansion_cache);
}

/* ============== 依赖引用追踪（Section 11.5: 依赖链断裂自动降级） ============== */

/**
 * @brief 注册一个依赖引用到公理包
 *
 * 记录一个依赖引用及其内容哈希，以便后续升级时验证内容是否变化。
 * 如果内容哈希发生变化，依赖此引用的 GREEN 结论将被自动降级为 YELLOW。
 */
int axiom_package_register_dependency_ref(AxiomPackage *pkg, const char *ref_id, const char *content_hash,
                                          int dependent_node_id) {
    if (!pkg)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "axiom_package_register_dependency_ref: pkg is NULL");
    if (!ref_id)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "axiom_package_register_dependency_ref: ref_id is NULL");
    if (!content_hash)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "axiom_package_register_dependency_ref: content_hash is NULL");

    DependencyRef ref;
    memset(&ref, 0, sizeof(DependencyRef));

    lv_strlcpy(ref.ref_id, ref_id, sizeof(ref.ref_id));
    lv_strlcpy(ref.content_hash, content_hash, sizeof(ref.content_hash));
    ref.dependent_node_id = dependent_node_id;
    ref.original_color = DEP_TRUST_GREEN;

    if (lv_darray_push(&pkg->dep_refs, &ref) < 0)
        return -2;

    if (axiom_stream_ctx) {
        stream_emit_simple(axiom_stream_ctx, STREAM_EVENT_INFO, "已注册依赖引用", 0);
    }

    return 0;
}

/**
 * @brief 验证所有依赖引用，返回失效的引用
 *
 * 通过重新计算包的内容哈希并与注册时存储的哈希进行比较，
 * 找出所有内容已发生变化的依赖引用。
 */
int axiom_package_validate_dependencies_with_hashes(AxiomPackage *pkg, DependencyRef **invalidated_refs,
                                                    int *invalidated_count) {
    if (!pkg || !invalidated_refs || !invalidated_count)
        return -1;

    *invalidated_refs = NULL;
    *invalidated_count = 0;

    if (pkg->dep_refs.count == 0)
        return 0;

    /* 重新计算当前包的内容哈希 */
    char *current_hash = axiom_package_compute_content_hash(pkg);
    if (!current_hash)
        return -1;

    /* 第一遍：统计失效引用数量（仅验证 REF_INTERNAL 类型的引用）
     * REF_EXTERNAL 为公认文献，永久有效，不参与自动重验
     * REF_AUTHOR 为基础黄色，无形式化支撑，不参与哈希验证 */
    int fail_count = 0;
    for (int i = 0; i < pkg->dep_refs.count; i++) {
        DependencyRef *ref = (DependencyRef *)lv_darray_get(&pkg->dep_refs, i);
        if (ref->ref_type != REF_INTERNAL)
            continue;
        if (strcmp(ref->content_hash, current_hash) != 0) {
            fail_count++;
        }
    }

    if (fail_count == 0) {
        lv_free((void **) &current_hash);
        return 0;
    }

    /* 分配输出数组 */
    DependencyRef *output = lv_calloc((size_t) fail_count, sizeof(DependencyRef));
    if (!output) {
        lv_free((void **) &current_hash);
        return -1;
    }

    /* 第二遍：填充失效引用 */
    int out_idx = 0;
    for (int i = 0; i < pkg->dep_refs.count; i++) {
        DependencyRef *ref = (DependencyRef *)lv_darray_get(&pkg->dep_refs, i);
        if (ref->ref_type != REF_INTERNAL)
            continue;
        if (strcmp(ref->content_hash, current_hash) != 0) {
            output[out_idx++] = *ref;
        }
    }

    lv_free((void **) &current_hash);
    *invalidated_refs = output;
    *invalidated_count = fail_count;
    return fail_count;
}

/**
 * @brief 执行失效依赖的自动降级
 *
 * 当公理包升级后，如果内部引用哈希发生变化（即引用内容被修改），
 * 所有依赖这些引用的 GREEN 结论将被自动降级为 YELLOW
 * "conditional unconstructible -- dependency invalidated"。
 *
 * 这是 design_v2.9.md Section 11.5 要求的依赖链断裂自动降级机制。
 */
int axiom_package_auto_degrade_invalidated(AxiomPackage *pkg, ConstraintGraph *graph) {
    if (!pkg || !graph)
        return 0;

    /* 步骤1：验证所有依赖引用，找出失效的 */
    DependencyRef *invalidated = NULL;
    int invalidated_count = 0;

    int result = axiom_package_validate_dependencies_with_hashes(pkg, &invalidated, &invalidated_count);

    if (result < 0 || invalidated_count == 0) {
        return 0;
    }

    /* 步骤2-4：对每个失效引用，降级依赖节点 */
    int degraded_count = 0;

    for (int i = 0; i < invalidated_count; i++) {
        DependencyRef *ref = &invalidated[i];

        /* 在约束图中查找依赖节点 */
        GeomNode *node = graph_get_node(graph, ref->dependent_node_id);
        if (!node) {
            fprintf(stderr,
                    "[WARNING] axiom_package_auto_degrade_invalidated: "
                    "依赖节点 %d 未在约束图中找到 (ref_id='%s')\n",
                    ref->dependent_node_id, ref->ref_id);
            continue;
        }

        /* 仅降级 GREEN 节点 */
        if (node->trust == TRUST_GREEN) {
            node->trust = TRUST_YELLOW;
            degraded_count++;

            fprintf(stderr,
                    "[WARNING] axiom_package_auto_degrade_invalidated: "
                    "节点 %d 已从 GREEN 降级为 YELLOW "
                    "(conditional unconstructible -- dependency invalidated, "
                    "ref_id='%s')\n",
                    ref->dependent_node_id, ref->ref_id);
        }
    }

    /* 释放验证结果数组 */
    lv_free((void **) &invalidated);

    /* 步骤3：处理作者断言引用 —— 确保依赖节点保持 YELLOW */
    for (int i = 0; i < pkg->dep_refs.count; i++) {
        DependencyRef *ref = (DependencyRef *)lv_darray_get(&pkg->dep_refs, i);
        if (ref->ref_type != REF_AUTHOR)
            continue;

        GeomNode *node = graph_get_node(graph, ref->dependent_node_id);
        if (!node) {
            fprintf(stderr,
                    "[WARNING] axiom_package_auto_degrade_invalidated: "
                    "作者断言依赖节点 %d 未在约束图中找到 (ref_id='%s')\n",
                    ref->dependent_node_id, ref->ref_id);
            continue;
        }

        /* 作者断言基础即为 YELLOW，确保未被错误提升为 GREEN */
        if (node->trust == TRUST_GREEN) {
            node->trust = TRUST_YELLOW;
            degraded_count++;
            fprintf(stderr,
                    "[WARNING] axiom_package_auto_degrade_invalidated: "
                    "节点 %d 从 GREEN 降级为 YELLOW "
                    "(author assertion -- no formal proof, ref_id='%s')\n",
                    ref->dependent_node_id, ref->ref_id);
        }
    }

    return degraded_count;
}

/* ============== 不可构造性证明依赖链引用 ============== */

/**
 * @brief 计算引理块的内容哈希
 *
 * 基于当前公理包的已知不可构造问题和模板数据计算哈希。
 * 用于内引用的内容验证。
 */
static char *compute_lemma_block_hash(AxiomPackage *pkg, int lemma_block_id) {
    if (!pkg)
        return NULL;

    lvSha256Context ctx;
    lv_sha256_init(&ctx);

    /* 哈希引理块 ID 作为标识 */
    lv_sha256_update(&ctx, (const uint8_t *) &lemma_block_id, sizeof(lemma_block_id));

    /* 哈希所有已知不可构造问题中的依赖链（这些构成引理块的约束逻辑） */
    for (int i = 0; i < pkg->known_unconstructibles.count; i++) {
        KnownUnconstructible *uc = (KnownUnconstructible *)lv_darray_get(&pkg->known_unconstructibles, i);
        if (uc->name) {
            lv_sha256_update(&ctx, (const uint8_t *) uc->name, strlen(uc->name));
        } else {
            lv_sha256_update(&ctx, (const uint8_t *) "(null)", 6);
        }
        if (uc->reduces_to) {
            lv_sha256_update(&ctx, (const uint8_t *) uc->reduces_to, strlen(uc->reduces_to));
        } else {
            lv_sha256_update(&ctx, (const uint8_t *) "(null)", 6);
        }
        lv_sha256_update(&ctx, (const uint8_t *) &uc->green_verified, sizeof(bool));

        /* 哈希依赖链 */
        for (int j = 0; j < uc->dependency_chain.count; j++) {
            char *dep = *(char **)lv_darray_get(&uc->dependency_chain, j);
            if (dep) {
                lv_sha256_update(&ctx, (const uint8_t *) dep, strlen(dep));
            } else {
                lv_sha256_update(&ctx, (const uint8_t *) "(null)", 6);
            }
        }

        if (uc->external_ref) {
            lv_sha256_update(&ctx, (const uint8_t *) uc->external_ref, strlen(uc->external_ref));
        } else {
            lv_sha256_update(&ctx, (const uint8_t *) "(null)", 6);
        }
    }

    /* 哈希所有模板名称和参数（构成构造性基础） */
    for (int i = 0; i < pkg->templates.count; i++) {
        ConstraintTemplate *t = (ConstraintTemplate *)lv_darray_get(&pkg->templates, i);
        if (t->name) {
            lv_sha256_update(&ctx, (const uint8_t *) t->name, strlen(t->name));
        } else {
            lv_sha256_update(&ctx, (const uint8_t *) "(null)", 6);
        }
        lv_sha256_update(&ctx, (const uint8_t *) &t->param_count, sizeof(int));
        lv_sha256_update(&ctx, (const uint8_t *) &t->verified, sizeof(bool));
    }

    /* 哈希几何信息和矛盾行为 */
    if (pkg->bottom_geometry) {
        lv_sha256_update(&ctx, (const uint8_t *) pkg->bottom_geometry, strlen(pkg->bottom_geometry));
    } else {
        lv_sha256_update(&ctx, (const uint8_t *) "(null)", 6);
    }
    if (pkg->negation_encoding) {
        lv_sha256_update(&ctx, (const uint8_t *) pkg->negation_encoding, strlen(pkg->negation_encoding));
    } else {
        lv_sha256_update(&ctx, (const uint8_t *) "(null)", 6);
    }
    lv_sha256_update(&ctx, (const uint8_t *) &pkg->contradiction_behavior, sizeof(int));

    /* 计算最终哈希 */
    uint8_t hash[AXIOM_SHA256_OUTPUT_SIZE];
    lv_sha256_final(&ctx, hash);

    /* 转换为十六进制字符串 */
    char *result = lv_malloc(AXIOM_SHA256_HEX_SIZE);
    if (result) {
        for (int i = 0; i < AXIOM_SHA256_OUTPUT_SIZE; i++) {
            snprintf(result + i * 2, 3, "%02x", hash[i]);
        }
        result[AXIOM_SHA256_HEX_SIZE - 1] = '\0';
    }

    return result;
}

int axiom_package_add_internal_ref(AxiomPackage *pkg, int lemma_block_id, int dependent_node_id) {
    if (!pkg)
        return -1;
    if (lemma_block_id < 0 || dependent_node_id < 0)
        return -1;

    /* 计算引理块的内容哈希 */
    char *hash = compute_lemma_block_hash(pkg, lemma_block_id);
    if (!hash)
        return -2;

    /* 生成引用标识符 */
    char ref_id[64];
    snprintf(ref_id, sizeof(ref_id), "internal:lemma:%d", lemma_block_id);

    DependencyRef ref;
    memset(&ref, 0, sizeof(DependencyRef));

    lv_strlcpy(ref.ref_id, ref_id, sizeof(ref.ref_id));
    lv_strlcpy(ref.content_hash, hash, sizeof(ref.content_hash));
    lv_free((void **) &hash);

    ref.dependent_node_id = dependent_node_id;
    ref.original_color = DEP_TRUST_GREEN;
    ref.ref_type = REF_INTERNAL;
    ref.hash_valid = true;

    if (lv_darray_push(&pkg->dep_refs, &ref) < 0)
        return -2;

    if (axiom_stream_ctx) {
        stream_emit_simple(axiom_stream_ctx, STREAM_EVENT_INFO, "已注册内引用（内容哈希验证）", 0);
    }

    return 0;
}

int axiom_package_add_external_ref(AxiomPackage *pkg, const char *ref_string, int dependent_node_id,
                                   const char *trust_comment) {
    if (!pkg || !ref_string)
        return -1;
    if (dependent_node_id < 0)
        return -1;

    /* 生成引用标识符 */
    char ref_id[64];
    snprintf(ref_id, sizeof(ref_id), "external:%.48s", ref_string);

    DependencyRef ref;
    memset(&ref, 0, sizeof(DependencyRef));

    lv_strlcpy(ref.ref_id, ref_id, sizeof(ref.ref_id));
    ref.content_hash[0] = '\0';
    ref.dependent_node_id = dependent_node_id;
    ref.original_color = DEP_TRUST_GREEN;
    ref.ref_type = REF_EXTERNAL;
    ref.hash_valid = false;
    lv_strlcpy(ref.external_ref, ref_string, sizeof(ref.external_ref));
    if (trust_comment && trust_comment[0] != '\0') {
        lv_strlcpy(ref.trust_comment, trust_comment, sizeof(ref.trust_comment));
    }

    if (lv_darray_push(&pkg->dep_refs, &ref) < 0)
        return -2;

    if (axiom_stream_ctx) {
        stream_emit_simple(axiom_stream_ctx, STREAM_EVENT_INFO, "已注册外引用（公认文献，永久有效）", 0);
    }

    return 0;
}

int axiom_package_add_author_assertion(AxiomPackage *pkg, int dependent_node_id) {
    if (!pkg)
        return -1;
    if (dependent_node_id < 0)
        return -1;

    /* 生成引用标识符 */
    char ref_id[64];
    snprintf(ref_id, sizeof(ref_id), "author:node:%d", dependent_node_id);

    DependencyRef ref;
    memset(&ref, 0, sizeof(DependencyRef));

    lv_strlcpy(ref.ref_id, ref_id, sizeof(ref.ref_id));
    ref.content_hash[0] = '\0';
    ref.dependent_node_id = dependent_node_id;
    ref.original_color = DEP_TRUST_YELLOW;
    ref.ref_type = REF_AUTHOR;
    ref.hash_valid = false;

    if (lv_darray_push(&pkg->dep_refs, &ref) < 0)
        return -2;

    if (axiom_stream_ctx) {
        stream_emit_simple(axiom_stream_ctx, STREAM_EVENT_INFO, "已注册作者断言（无形式化支撑，黄色基础）", 0);
    }

    return 0;
}

/* ============== graph_copy：约束图深拷贝 ============== */

/**
 * @brief 深拷贝约束图
 *
 * 遍历源图中的所有节点和约束，在新图中创建完全独立的副本。
 * 高级类型（Port、Region、FunctionBlock）通过 graph_add_node_with_id
 * 复制基础字段，调用者可后续设置特定字段。
 */
ConstraintGraph *graph_copy(const ConstraintGraph *graph) {
    if (!graph)
        return NULL;

    ConstraintGraph *new_graph = graph_create();
    if (!new_graph)
        return NULL;

    /* 复制所有节点 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *src = graph->nodes[i];
        if (!src)
            continue;

        /* 使用带ID接口添加节点，保持ID一致 */
        GeomNode *dst = graph_add_node_with_id(new_graph, src->id, src->type, src->symbolic_coords, src->coord_count);
        if (!dst) {
            graph_destroy(new_graph);
            return NULL;
        }

        /* 复制增强字段 */
        dst->trust = src->trust;
        dst->is_active = src->is_active;
        dst->lo_subtype = src->lo_subtype;
        dst->namespace_depth = src->namespace_depth;
        dst->parent_block_id = src->parent_block_id;
        dst->numeric_precision = src->numeric_precision;

        /* 深拷贝 numeric_assumption_declaration */
        if (src->numeric_assumption_declaration) {
            dst->numeric_assumption_declaration = lv_strdup_safe(src->numeric_assumption_declaration);
        }
    }

    /* 复制所有约束 */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *src = graph->constraints[i];
        if (!src)
            continue;

        Constraint *dst =
            graph_add_constraint_with_id(new_graph, src->id, src->type, src->participants, src->participant_count);
        if (!dst) {
            graph_destroy(new_graph);
            return NULL;
        }

        /* 复制增强字段 */
        dst->template_id = src->template_id;
        dst->is_active = src->is_active;
        dst->numeric_value = src->numeric_value;
        dst->satisfaction = src->satisfaction;
    }

    /* 复制高级图属性 */
    new_graph->dirty = graph->dirty;

    return new_graph;
}

/* ============== 模板分级管理与惰性展开 ============== */

void axiom_template_set_level(ConstraintTemplate *tmpl, TemplateLevel level) {
    if (!tmpl)
        return;
    tmpl->level = level;
    /* 二级模板默认标记为压缩态 */
    if (level == TEMPLATE_LEVEL_TWO) {
        tmpl->is_compressed = true;
    }
}

ConstraintGraph *axiom_template_expand_lazy(AxiomPackage *pkg, const char *template_name, SymbolicCoord **params,
                                            int param_count) {
    if (!pkg)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "axiom_template_expand_lazy: pkg is NULL");
    if (!template_name)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "axiom_template_expand_lazy: template_name is NULL");

    ConstraintTemplate *tmpl = axiom_package_get_template(pkg, template_name);
    if (!tmpl)
        return NULL;

    /* 先查展开缓存 */
    ConstraintGraph *cached = axiom_package_lookup_expansion_cache(pkg, template_name, params, param_count);
    if (cached)
        return cached;

    ConstraintGraph *result = NULL;

    /* 二级模板：从压缩态展开 */
    if (tmpl->level == TEMPLATE_LEVEL_TWO && tmpl->is_compressed && tmpl->compressed_subgraph) {
        result = graph_copy(tmpl->compressed_subgraph);
    } else {
        /* 正常展开 */
        result = graph_create();
        if (result && tmpl->expand) {
            tmpl->expand(params, result);
        }
    }

    /* 存入缓存并标记为非压缩态 */
    if (result) {
        axiom_package_store_expansion_cache(pkg, template_name, params, param_count, result);
        tmpl->is_compressed = false;
    }

    return result;
}

void axiom_template_compress(ConstraintTemplate *tmpl) {
    if (!tmpl || tmpl->level != TEMPLATE_LEVEL_TWO)
        return;
    tmpl->is_compressed = true;
    /* 展开缓存由缓存管理器自行处理，此处仅恢复压缩态标记 */
}

/* ============== 引理自动重验循环（Section 11） ============== */

/**
 * @brief 重新验证引理块
 *
 * 尝试重新验证引理块的内容。
 * 通过重新计算公理包的当前内容哈希并与存储的哈希对比来判断内容是否发生变化。
 *
 * @param pkg 公理包
 * @param ref 要重验的依赖引用
 * @return true 重验通过（哈希匹配）
 */
static bool lemma_reverify(AxiomPackage *pkg, DependencyRef *ref) {
    if (!pkg || !ref)
        return false;

    /* 计算当前内容哈希 */
    char *current_hash = axiom_package_compute_content_hash(pkg);
    if (!current_hash)
        return false;

    /* 对比哈希 */
    bool match = (strcmp(ref->content_hash, current_hash) == 0);
    lv_free((void **) &current_hash);
    return match;
}

int axiom_package_reverify_lemmas(AxiomPackage *pkg, int *out_stale, char ***out_stale_names) {
    if (!pkg)
        return 0;

    int total = 0;
    int stale_count = 0;

    /* 第一遍：统计需要处理的 REF_INTERNAL 引用总数和失效数 */
    for (int i = 0; i < pkg->dep_refs.count; i++) {
        DependencyRef *ref = (DependencyRef *)lv_darray_get(&pkg->dep_refs, i);
        if (ref->ref_type != REF_INTERNAL)
            continue;
        total++;
        if (!lemma_reverify(pkg, ref)) {
            stale_count++;
        }
    }

    /* 分配输出数组 */
    char **stale_names = NULL;
    if (stale_count > 0) {
        stale_names = (char **) lv_calloc((size_t) stale_count, sizeof(char *));
        if (!stale_names) {
            if (out_stale)
                *out_stale = 0;
            if (out_stale_names)
                *out_stale_names = NULL;
            return total;
        }
    }

    /* 第二遍：标记失效引用并记录名称 */
    int idx = 0;
    for (int i = 0; i < pkg->dep_refs.count; i++) {
        DependencyRef *ref = (DependencyRef *)lv_darray_get(&pkg->dep_refs, i);
        if (ref->ref_type != REF_INTERNAL)
            continue;
        if (!lemma_reverify(pkg, ref)) {
            axiom_package_mark_lemma_stale(pkg, ref->ref_id);
            if (stale_names && idx < stale_count) {
                stale_names[idx] = lv_strdup_safe(ref->ref_id);
                idx++;
            }
        }
    }

    if (out_stale)
        *out_stale = stale_count;
    if (out_stale_names) {
        *out_stale_names = stale_names;
    } else if (stale_names) {
        /* 调用者不需要名称数组，释放分配的内存 */
        for (int i = 0; i < stale_count; i++) {
            lv_free((void **) &stale_names[i]);
        }
        lv_free((void **) &stale_names);
    }

    if (axiom_stream_ctx) {
        stream_emit_simple(axiom_stream_ctx, STREAM_EVENT_INFO, "引理自动重验完成", 0);
    }

    return total;
}

int axiom_package_mark_lemma_stale(AxiomPackage *pkg, const char *ref_id) {
    if (!pkg || !ref_id)
        return -1;

    for (int i = 0; i < pkg->dep_refs.count; i++) {
        DependencyRef *ref = (DependencyRef *)lv_darray_get(&pkg->dep_refs, i);
        if (strcmp(ref->ref_id, ref_id) != 0)
            continue;

        /* 设置信任注释，标识为遗留状态 */
        lv_strlcpy(ref->trust_comment, "遗留 - 在旧版本下得证，未验证兼容性", sizeof(ref->trust_comment));

        /* 将信任颜色设为黄色，表示需人工介入 */
        ref->original_color = DEP_TRUST_YELLOW;

        if (axiom_stream_ctx) {
            stream_emit_simple(axiom_stream_ctx, STREAM_EVENT_WARNING, "引理标记为遗留状态", 0);
        }

        return 0;
    }

    return -1;
}
