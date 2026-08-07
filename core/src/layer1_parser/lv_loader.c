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

#include "lv/lv_platform.h"
#include "lv/lv_loader.h"
#include "lv/lv_file.h"
#include "lv/lv_str_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv.h"
#include "lv/lv_lexer.h"

#include "lv_internal.h"
#include "lv_utils.h"


/* ── 名称映射表：跟踪 AST 名称到引擎节点 ID 的映射 ── */

/**
 * @brief 名称映射条目
 *
 * 将 AST 中声明的实体名称与引擎内部的节点 ID 关联，
 * 用于在后续约束和证明语句中引用已声明的实体。
 */
typedef struct {
    char name[64]; /**< 实体名称（最多 63 字符） */
    int node_id;   /**< 引擎节点 ID，-1 表示尚未关联 */
} LvNameMap;

/** @brief 名称映射表最大容量 */
#define LV_MAX_NAMED_ENTITIES 256

/** @brief 名称映射表单例状态 */
typedef struct {
    LvNameMap entries[LV_MAX_NAMED_ENTITIES]; /**< 名称映射表 */
    int count;                                /**< 当前已注册的名称数量 */
} LoaderNameState;

/** @brief 名称映射表全局单例 */
static LoaderNameState s_loader_names = {0};

/**
 * @brief 清空名称映射表
 *
 * 重置映射表计数器，清除所有已注册的名称映射。
 */
static void loader_names_clear(void) {
    s_loader_names.count = 0;
}

/**
 * @brief 重置加载器名称映射表（测试进程内隔离用）
 *
 * 清空 static 全局名称映射表 s_loader_names（名称 → 引擎节点 ID）。
 * lv_loader.c 无独立 ID 计数器（引擎节点 ID 由引擎生成，loader 仅记录），
 * 本函数为名称表这一 static 全局状态提供显式重置能力，供测试进程内
 * 隔离使用。正常加载路径（lv_apply_parse_result 每次调用已内部清空）
 * 行为完全不变。
 */
void lv_loader_reset(void) {
    loader_names_clear();
}

/**
 * @brief 向名称映射表添加条目
 *
 * @param name    实体名称
 * @param node_id 引擎节点 ID
 */
static void loader_names_add(const char *name, int node_id) {
    if (s_loader_names.count >= LV_MAX_NAMED_ENTITIES)
        return;
    LvNameMap *entry = &s_loader_names.entries[s_loader_names.count++];
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
static int loader_names_lookup(const char *name) {
    if (!name)
        return -1;
    for (int i = 0; i < s_loader_names.count; i++) {
        if (strcmp(s_loader_names.entries[i].name, name) == 0)
            return s_loader_names.entries[i].node_id;
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
    if (!filepath || !out_len)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "filepath or out_len is NULL");

    return (char *) lv_file_read_all(filepath, out_len);
}

/* ================================================================
 * 处理 Declaration 节点，向引擎添加几何实体
 * ================================================================ */

/** @brief 实体声明处理器函数指针类型 */
typedef void (*EntityDeclHandler)(lvEngine *engine, const char *name);

/** 注册为自由点（默认坐标 (0,1,0,1) 即 (0,0)） */
static void decl_register_point(lvEngine *engine, const char *name) {
    int id = lv_add_point(engine, 0, 1, 0, 1);
    if (id >= 0)
        loader_names_add(name, id);
}

/** 暂存名称（-1 表示尚未关联端点；未支持类型同样走暂存而非报错，保留原 default 折叠语义） */
static void decl_stash(lvEngine *engine, const char *name) {
    (void) engine;
    loader_names_add(name, -1);
}

/** @brief 实体类型 → 声明处理器 VTable（按枚举索引；显式列出全部类型，未支持类型走暂存） */
static const EntityDeclHandler kEntityDeclHandlers[] = {
    [LV_ENTITY_POINT]       = decl_register_point,
    [LV_ENTITY_LINE]        = decl_stash,
    [LV_ENTITY_CIRCLE]      = decl_stash,
    [LV_ENTITY_SEGMENT]     = decl_stash,
    [LV_ENTITY_RAY]         = decl_stash,
    [LV_ENTITY_ANGLE]       = decl_stash,
    [LV_ENTITY_TRIANGLE]    = decl_stash,
    [LV_ENTITY_POLYGON]     = decl_stash,
    [LV_ENTITY_SCALAR]      = decl_stash,
    [LV_ENTITY_BOOL]        = decl_stash,
    [LV_ENTITY_PROPOSITION] = decl_stash,
    [LV_ENTITY_PROOF]       = decl_stash,
};

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
    LvEntityType etype = (LvEntityType) node->data.decl.entity_type;
    const char *names = node->data.decl.names;
    if (!names || !engine)
        return;

    /* 复制 names 用于拆分 */
    char buf[1024];
    lv_strncpy(buf, names, sizeof(buf));

    /* VTable 调度（越界/未登记类型走暂存，对应原 default 分支） */
    EntityDeclHandler handler = decl_stash;
    if ((unsigned)etype < (unsigned)lv_ARRAY_SIZE(kEntityDeclHandlers))
        handler = kEntityDeclHandlers[etype];

    /* 拆分逗号分隔的名称列表 */
    char *save;
    char *tok = lv_strtok_r(buf, ",", &save);
    while (tok) {
        handler(engine, tok);
        tok = lv_strtok_r(NULL, ",", &save);
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
        lv_strncpy(result.errors[0].message, "filepath is NULL", sizeof(result.errors[0].message));
        return result;
    }

    /* 读取文件 */
    size_t len = 0;
    char *source = read_file(filepath, &len);
    if (!source) {
        result.error_count = 1;
        lv_snprintf(result.errors[0].message, sizeof(result.errors[0].message), "failed to read file: %s", filepath);
        return result;
    }

    /* Lex → Parse */
    LvLexer *lexer = lv_lexer_create(source, len);
    if (!lexer) {
        lv_free((void **) &source);
        result.error_count = 1;
        lv_strncpy(result.errors[0].message, "failed to create lexer", sizeof(result.errors[0].message));
        return result;
    }

    LvParser *parser = lv_parser_create(lexer);
    if (!parser) {
        lv_lexer_destroy(lexer);
        lv_free((void **) &source);
        result.error_count = 1;
        lv_strncpy(result.errors[0].message, "failed to create parser", sizeof(result.errors[0].message));
        return result;
    }

    result = lv_parser_parse_program(parser);

    lv_parser_destroy(parser);
    lv_lexer_destroy(lexer);
    lv_free((void **) &source);

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
    if (!engine || !result || !result->ast)
        return false;

    loader_names_clear();

    /* 使用 sema 进行语义验证 */
    if (sema) {
        if (lv_sema_error_count(sema) > 0) {
            return false;
        }
    }

    LvAstNode *ast = result->ast;
    if (ast->type != LV_AST_PROGRAM)
        return false;

    /* 第一遍：处理声明（添加几何对象到引擎） */
    for (LvAstNode *stmt = ast->child; stmt; stmt = stmt->next) {
        if (stmt->type == LV_AST_DECLARATION) {
            process_declaration(engine, stmt);
        }
    }

    /* 第二遍：处理声明之后的线段/直线（如果端点已知） */
    for (LvAstNode *stmt = ast->child; stmt; stmt = stmt->next) {
        if (stmt->type == LV_AST_DECLARATION) {
            LvEntityType etype = (LvEntityType) stmt->data.decl.entity_type;
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
                        int id = loader_names_lookup(a->data.ident.name);
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

/* ================================================================
 * 微自举 B —— 证明验证器（Prove 语句验证语义，C 内实现 verify）
 *
 * 语义与规格：bootstrap/src/proofs/proof_verifier.lv 用 lv 语言描述
 * 验证器（Verdict/ProofTerm/verify），本实现提供其 verify 语义：
 * 加载 .lv 时对 Prove 断言执行真实验证 ——
 *   1. λ-演算验证：Church 编码运算（add/sub/mul/pow/succ/pred/eq/leq/
 *      gt/iszero/and/or/xor/not/if/true/false）按 Church 运算定理对
 *      闭合表达式求值（与 λ-项 β-归约在这些闭合项上的结果等价，
 *      参照 test_lambda_eval 的 Church 归约验证能力）；
 *   2. 算术验证：整数表达式（+ - * / ^ 与括号）求值比较；
 *   3. 布尔验证：布尔字面量、逻辑运算（and/or/not/->/iff）、纯布尔目标；
 *   4. 反射律：全同名参数的关系调用（collinear(A,A,A)）恒真；
 *   其余（量词、未知函数、除零、开放变量等）→ SKIP，不误报。
 * ================================================================ */

#define LV_PROVE_MAX_REPORTS 64
#define LV_PROVE_MAX_DEPTH 256
#define LV_PROVE_MAX_ARGS 8

/* ── 折叠值（验证语义下的归一化值）── */

typedef enum {
    LV_VAL_NUM,  /**< 整数值 */
    LV_VAL_BOOL  /**< 布尔值 */
} LvValKind;

typedef struct {
    LvValKind kind;
    long long num;
    int boolean;
} LvVal;

/* ── Church 运算语义表（函数名 → 运算）── */

typedef bool (*ChurchOpFn)(const LvVal *args, int argc, LvVal *out);

typedef struct {
    const char *name;
    ChurchOpFn fn;
    int min_args;
} ChurchFnEntry;

static bool op_succ(const LvVal *a, int n, LvVal *o) {
    if (n != 1 || a[0].kind != LV_VAL_NUM) return false;
    o->kind = LV_VAL_NUM; o->num = a[0].num + 1; return true;
}
static bool op_pred(const LvVal *a, int n, LvVal *o) {
    if (n != 1 || a[0].kind != LV_VAL_NUM) return false;
    o->kind = LV_VAL_NUM; o->num = a[0].num > 0 ? a[0].num - 1 : 0; return true;
}
static bool op_add(const LvVal *a, int n, LvVal *o) {
    if (n != 2 || a[0].kind != LV_VAL_NUM || a[1].kind != LV_VAL_NUM) return false;
    o->kind = LV_VAL_NUM; o->num = a[0].num + a[1].num; return true;
}
static bool op_sub(const LvVal *a, int n, LvVal *o) {
    if (n != 2 || a[0].kind != LV_VAL_NUM || a[1].kind != LV_VAL_NUM) return false;
    /* Church 减法：m < n 时结果为 0 */
    o->kind = LV_VAL_NUM; o->num = a[0].num > a[1].num ? a[0].num - a[1].num : 0; return true;
}
static bool op_mul(const LvVal *a, int n, LvVal *o) {
    if (n != 2 || a[0].kind != LV_VAL_NUM || a[1].kind != LV_VAL_NUM) return false;
    o->kind = LV_VAL_NUM; o->num = a[0].num * a[1].num; return true;
}
static bool op_pow(const LvVal *a, int n, LvVal *o) {
    if (n != 2 || a[0].kind != LV_VAL_NUM || a[1].kind != LV_VAL_NUM || a[1].num < 0) return false;
    long long v = 1;
    for (long long i = 0; i < a[1].num && v <= 0x3fffffffffffffffLL; i++)
        v *= a[0].num;
    o->kind = LV_VAL_NUM; o->num = v; return true;
}
static bool op_true(const LvVal *a, int n, LvVal *o) {
    (void)a; if (n != 0) return false;
    o->kind = LV_VAL_BOOL; o->boolean = 1; return true;
}
static bool op_false(const LvVal *a, int n, LvVal *o) {
    (void)a; if (n != 0) return false;
    o->kind = LV_VAL_BOOL; o->boolean = 0; return true;
}
static bool op_iszero(const LvVal *a, int n, LvVal *o) {
    if (n != 1 || a[0].kind != LV_VAL_NUM) return false;
    o->kind = LV_VAL_BOOL; o->boolean = (a[0].num == 0) ? 1 : 0; return true;
}
static bool op_eq(const LvVal *a, int n, LvVal *o) {
    if (n != 2) return false;
    if (a[0].kind != a[1].kind) return false;
    if (a[0].kind == LV_VAL_NUM) o->boolean = (a[0].num == a[1].num) ? 1 : 0;
    else o->boolean = (a[0].boolean == a[1].boolean) ? 1 : 0;
    o->kind = LV_VAL_BOOL; return true;
}
static bool op_leq(const LvVal *a, int n, LvVal *o) {
    if (n != 2 || a[0].kind != LV_VAL_NUM || a[1].kind != LV_VAL_NUM) return false;
    o->kind = LV_VAL_BOOL; o->boolean = (a[0].num <= a[1].num) ? 1 : 0; return true;
}
static bool op_gt(const LvVal *a, int n, LvVal *o) {
    if (n != 2 || a[0].kind != LV_VAL_NUM || a[1].kind != LV_VAL_NUM) return false;
    o->kind = LV_VAL_BOOL; o->boolean = (a[0].num > a[1].num) ? 1 : 0; return true;
}
static bool op_not(const LvVal *a, int n, LvVal *o) {
    if (n != 1 || a[0].kind != LV_VAL_BOOL) return false;
    o->kind = LV_VAL_BOOL; o->boolean = a[0].boolean ? 0 : 1; return true;
}
static bool op_and(const LvVal *a, int n, LvVal *o) {
    if (n != 2 || a[0].kind != LV_VAL_BOOL || a[1].kind != LV_VAL_BOOL) return false;
    o->kind = LV_VAL_BOOL; o->boolean = (a[0].boolean && a[1].boolean) ? 1 : 0; return true;
}
static bool op_or(const LvVal *a, int n, LvVal *o) {
    if (n != 2 || a[0].kind != LV_VAL_BOOL || a[1].kind != LV_VAL_BOOL) return false;
    o->kind = LV_VAL_BOOL; o->boolean = (a[0].boolean || a[1].boolean) ? 1 : 0; return true;
}
static bool op_xor(const LvVal *a, int n, LvVal *o) {
    if (n != 2 || a[0].kind != LV_VAL_BOOL || a[1].kind != LV_VAL_BOOL) return false;
    o->kind = LV_VAL_BOOL; o->boolean = (a[0].boolean != a[1].boolean) ? 1 : 0; return true;
}
static bool op_if(const LvVal *a, int n, LvVal *o) {
    if (n != 3 || a[0].kind != LV_VAL_BOOL) return false;
    *o = a[0].boolean ? a[1] : a[2];
    return true;
}

/** @brief Church 运算函数名 → 运算语义（不含 Y 组合子递归，保证终止） */
static const ChurchFnEntry kChurchFnTable[] = {
    {"succ", op_succ, 1}, {"pred", op_pred, 1},
    {"add", op_add, 2},   {"sub", op_sub, 2},
    {"mul", op_mul, 2},   {"pow", op_pow, 2},
    {"true", op_true, 0}, {"false", op_false, 0},
    {"if", op_if, 3},     {"iszero", op_iszero, 1},
    {"not", op_not, 1},   {"and", op_and, 2},
    {"or", op_or, 2},     {"xor", op_xor, 2},
    {"eq", op_eq, 2},     {"leq", op_leq, 2}, {"gt", op_gt, 2},
};

/* ── 表达式折叠：闭合表达式 → LvVal ── */

static bool fold_expr(LvAstNode *node, LvVal *out, int depth) {
    if (!node || !out)
        return false;
    if (depth > LV_PROVE_MAX_DEPTH)
        return false;
    switch (node->type) {
    case LV_AST_INTEGER_LITERAL:
        out->kind = LV_VAL_NUM;
        out->num = node->data.literal.integer_value;
        return true;
    case LV_AST_BOOL_LITERAL:
        out->kind = LV_VAL_BOOL;
        out->boolean = node->data.literal.bool_value ? 1 : 0;
        return true;
    case LV_AST_BINARY_OP: {
        LvVal l, r;
        if (!fold_expr(node->data.binary.left, &l, depth + 1))
            return false;
        if (!fold_expr(node->data.binary.right, &r, depth + 1))
            return false;
        if (l.kind != LV_VAL_NUM || r.kind != LV_VAL_NUM)
            return false;
        const char *op = node->data.binary.op;
        if (strcmp(op, "+") == 0) {
            out->kind = LV_VAL_NUM; out->num = l.num + r.num; return true;
        }
        if (strcmp(op, "-") == 0) {
            out->kind = LV_VAL_NUM; out->num = l.num - r.num; return true;
        }
        if (strcmp(op, "*") == 0) {
            out->kind = LV_VAL_NUM; out->num = l.num * r.num; return true;
        }
        if (strcmp(op, "/") == 0) {
            if (r.num == 0)
                return false;
            out->kind = LV_VAL_NUM; out->num = l.num / r.num; return true;
        }
        if (strcmp(op, "^") == 0) {
            if (r.num < 0)
                return false;
            long long v = 1;
            for (long long i = 0; i < r.num && v <= 0x3fffffffffffffffLL; i++)
                v *= l.num;
            out->kind = LV_VAL_NUM; out->num = v; return true;
        }
        return false;
    }
    case LV_AST_UNARY_OP: {
        LvVal v;
        if (!fold_expr(node->data.unary.operand, &v, depth + 1))
            return false;
        if (v.kind != LV_VAL_NUM)
            return false;
        if (strcmp(node->data.unary.op, "-") == 0) {
            out->kind = LV_VAL_NUM; out->num = -v.num; return true;
        }
        if (strcmp(node->data.unary.op, "+") == 0) {
            *out = v; return true;
        }
        return false;
    }
    case LV_AST_FUNCTION_CALL: {
        const char *fname = node->data.call.func_name;
        if (!fname)
            return false;
        const ChurchFnEntry *entry = NULL;
        for (size_t i = 0; i < lv_ARRAY_SIZE(kChurchFnTable); i++) {
            if (strcmp(kChurchFnTable[i].name, fname) == 0) {
                entry = &kChurchFnTable[i];
                break;
            }
        }
        if (!entry)
            return false;
        LvVal args[LV_PROVE_MAX_ARGS];
        int argc = 0;
        for (LvAstNode *a = node->data.call.args; a; a = a->next) {
            if (argc >= LV_PROVE_MAX_ARGS)
                return false;
            if (!fold_expr(a, &args[argc], depth + 1))
                return false;
            argc++;
        }
        if (argc != entry->min_args)
            return false;
        return entry->fn(args, argc, out);
    }
    default:
        return false;
    }
}

/* ── 命题求值：返回 -1 无法判定，0 假，1 真 ── */

static int eval_proposition(LvAstNode *node, int depth) {
    if (!node)
        return -1;
    if (depth > LV_PROVE_MAX_DEPTH)
        return -1;
    switch (node->type) {
    case LV_AST_BOOL_LITERAL:
        return node->data.literal.bool_value ? 1 : 0;
    case LV_AST_COMPARE: {
        const char *op = node->data.compare.op;
        LvVal l, r;
        if (!fold_expr(node->data.compare.left, &l, depth + 1))
            return -1;
        if (!fold_expr(node->data.compare.right, &r, depth + 1))
            return -1;
        if (l.kind == LV_VAL_NUM && r.kind == LV_VAL_NUM) {
            if (strcmp(op, "==") == 0) return l.num == r.num ? 1 : 0;
            if (strcmp(op, "!=") == 0) return l.num != r.num ? 1 : 0;
            if (strcmp(op, "<") == 0)  return l.num < r.num ? 1 : 0;
            if (strcmp(op, "<=") == 0) return l.num <= r.num ? 1 : 0;
            if (strcmp(op, ">") == 0)  return l.num > r.num ? 1 : 0;
            if (strcmp(op, ">=") == 0) return l.num >= r.num ? 1 : 0;
            return -1;
        }
        if (l.kind == LV_VAL_BOOL && r.kind == LV_VAL_BOOL) {
            if (strcmp(op, "==") == 0) return l.boolean == r.boolean ? 1 : 0;
            if (strcmp(op, "!=") == 0) return l.boolean != r.boolean ? 1 : 0;
            return -1;
        }
        return -1;
    }
    case LV_AST_LOGIC_AND: {
        int l = eval_proposition(node->data.binary.left, depth + 1);
        int r = eval_proposition(node->data.binary.right, depth + 1);
        if (l < 0 || r < 0)
            return -1;
        return (l && r) ? 1 : 0;
    }
    case LV_AST_LOGIC_OR: {
        int l = eval_proposition(node->data.binary.left, depth + 1);
        int r = eval_proposition(node->data.binary.right, depth + 1);
        if (l < 0 || r < 0)
            return -1;
        return (l || r) ? 1 : 0;
    }
    case LV_AST_LOGIC_NOT: {
        int v = eval_proposition(node->data.unary.operand, depth + 1);
        return v < 0 ? -1 : (v ? 0 : 1);
    }
    case LV_AST_LOGIC_IMPLIES: {
        int l = eval_proposition(node->data.binary.left, depth + 1);
        int r = eval_proposition(node->data.binary.right, depth + 1);
        if (l < 0 || r < 0)
            return -1;
        return (l == 1 && r == 0) ? 0 : 1;
    }
    case LV_AST_LOGIC_IFF: {
        int l = eval_proposition(node->data.binary.left, depth + 1);
        int r = eval_proposition(node->data.binary.right, depth + 1);
        if (l < 0 || r < 0)
            return -1;
        return (l == r) ? 1 : 0;
    }
    case LV_AST_RELATION: {
        /* 反射律：所有参数为同一标识符的关系调用恒真（如 collinear(A,A,A)） */
        const char *first_name = NULL;
        for (LvAstNode *a = node->data.call.args; a; a = a->next) {
            if (a->type != LV_AST_IDENTIFIER_EXPR || !a->data.ident.name)
                return -1;
            if (!first_name)
                first_name = a->data.ident.name;
            else if (strcmp(first_name, a->data.ident.name) != 0)
                return -1;
        }
        return first_name ? 1 : -1;
    }
    default:
        /* 纯布尔目标：如 Prove eq(2, 2); / Prove iszero(0); */
        {
            LvVal v;
            if (fold_expr(node, &v, depth + 1) && v.kind == LV_VAL_BOOL)
                return v.boolean ? 1 : 0;
        }
        return -1;
    }
}

/* ── 逐条验证 Prove 语句 ── */

static void verify_prove_node(LvAstNode *prove, int index, LvProveSummary *summary) {
    if (!prove || !summary)
        return;
    if (summary->prove_count >= LV_PROVE_MAX_REPORTS)
        return;

    LvProveReport *rep = &summary->reports[summary->prove_count];
    memset(rep, 0, sizeof(*rep));
    summary->prove_count++;

    rep->line = prove->loc.line;
    rep->column = prove->loc.column;
    lv_snprintf(rep->name, sizeof(rep->name), "prove#%d", index);

    LvAstNode *expr = prove->data.stmt.expr;
    if (!expr) {
        rep->verdict = LV_PROVE_SKIP;
        lv_snprintf(rep->detail, sizeof(rep->detail), "empty Prove statement");
        summary->skip_count++;
        return;
    }

    int r = eval_proposition(expr, 0);
    if (r < 0) {
        rep->verdict = LV_PROVE_SKIP;
        lv_snprintf(rep->detail, sizeof(rep->detail),
                    "not mechanically decidable (quantifier / unknown function / open term)");
        summary->skip_count++;
    } else if (r == 1) {
        rep->verdict = LV_PROVE_PASS;
        lv_snprintf(rep->detail, sizeof(rep->detail),
                    "verified: conclusion holds (church-eval / arith / logic)");
        summary->pass_count++;
    } else {
        rep->verdict = LV_PROVE_FAIL;
        lv_snprintf(rep->detail, sizeof(rep->detail),
                    "verification failed: conclusion evaluates to false");
        summary->fail_count++;
    }
}

/* ── 微自举 B 公共 API ── */

bool lv_verify_proofs(const LvParseResult *result, LvProveSummary *summary) {
    if (!result || !summary)
        return false;
    memset(summary, 0, sizeof(*summary));
    if (!result->ast || result->ast->type != LV_AST_PROGRAM)
        return false;

    int index = 0;
    for (LvAstNode *stmt = result->ast->child; stmt; stmt = stmt->next) {
        if (stmt->type == LV_AST_PROVE_STMT)
            verify_prove_node(stmt, index++, summary);
    }
    return true;
}

bool lv_load_file_verified(const char *filepath, LvProveSummary *summary) {
    LvParseResult res = lv_load_file(filepath);
    if (!res.ast) {
        if (summary)
            memset(summary, 0, sizeof(*summary));
        return false;
    }
    bool ok = true;
    if (summary)
        ok = lv_verify_proofs(&res, summary);
    lv_ast_destroy(res.ast);
    return ok;
}