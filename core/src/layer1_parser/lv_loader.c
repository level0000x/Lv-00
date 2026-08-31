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
 *          通过名称映射表跟踪 AST 中声明的实体名称到
 *          引擎节点 ID 的映射关系。
 *
 * @author Lv-00 Project
 */

#include "lv/lv_platform.h"
#include "lv/lv_loader.h"
#include "lv/lv_file.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_ast.h"      /* lv_ast_max_depth（K28/F54 AST 深度闸门） */
#include "lv/parser_safety.h" /* lv_check_ast_depth（读 lvConfig 配置） */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv.h"
#include "lv/lv_lexer.h"
#include "lv/lv_xmacro.h"

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"

#include "lv/lv_registry.h"
#include "lv/lv_thread.h"

/* F24/I5：层依赖声明——Layer 1（解析）依赖 Layer 2（资源）及以下；
 * lv.h 伞头归 L0 便利层（协调各层），经此放行 */
#include "lv/engine.h" /* lv_ALLOW_LAYER（宏定义处，引用即包含） */
lv_ALLOW_LAYER(lv_LAYER_RESOURCE);


/* ── 名称映射表：跟踪 AST 名称到引擎节点 ID 的映射 ── */

/**
 * @brief 名称映射表（通用注册表设施：key = 实体名称，value = boxed int 引擎节点 ID）。
 *
 * 将 AST 中声明的实体名称与引擎内部的节点 ID 关联，
 * 用于在后续约束和证明语句中引用已声明的实体。
 * 文件级单例（lv_once 惰性初始化，线程安全）；strcmp 查重、内部 name 拷贝、
 * 动态扩容与清空均由 lv_registry 承担。
 *
 * 语义保持：重复添加同名实体时首次映射生效（原线性查找"首个匹配优先"），
 * 容量上限 LV_MAX_NAMED_ENTITIES 与原实现一致（满时静默忽略）。
 */
lv_REGISTRY_STATIC(loader_names, 32);

/** @brief 名称映射表最大容量 */
#define LV_MAX_NAMED_ENTITIES 256

/** @brief 装箱引擎节点 ID（注册表 value） */
static void *loader_box_id(int node_id) {
    int *p = (int *) lv_malloc(sizeof(int));
    if (p) {
        *p = node_id;
    }
    return p;
}

/** @brief boxed 节点 ID 的注册表 destroy 回调适配器（void(*)(void*) 形态） */
static void loader_box_destroy(void *value) {
    lv_free((void **) &value);
}

/**
 * @brief 清空名称映射表
 *
 * 释放所有条目（destroy 回调 + 内部 name），保留注册表结构可继续使用。
 */
static void loader_names_clear(void) {
    loader_names_ensure();
    lv_registry_clear(&g_loader_names);
}

/**
 * @brief 重置加载器名称映射表（测试进程内隔离用）
 *
 * 清空 static 全局名称映射表 g_loader_names（名称 → 引擎节点 ID）。
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
    loader_names_ensure();
    if (lv_registry_count(&g_loader_names) >= LV_MAX_NAMED_ENTITIES)
        return;

    void *boxed = loader_box_id(node_id);
    if (!boxed)
        return;
    if (!lv_registry_put_ex(&g_loader_names, name, boxed, loader_box_destroy)) {
        /* 名称重复：保留首次映射（与原线性查找"首个匹配优先"语义一致） */
        lv_free((void **) &boxed);
    }
}

/**
 * @brief 在名称映射表中查找名称
 *
 * 委托注册表按名称查找（strcmp 由 lv_registry 承担）。
 *
 * @param name 实体名称（允许为 NULL，返回 -1）
 * @return 引擎节点 ID，未找到或 name 为 NULL 返回 -1
 */
static int loader_names_lookup(const char *name) {
    if (!name)
        return -1;
    loader_names_ensure();
    void *boxed = lv_registry_get(&g_loader_names, name);
    return boxed ? *(int *) boxed : -1;
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

/* ── 声明值解析辅助（坐标/构造值）── */

/**
 * @brief 从声明值 AST 中提取整数字面量
 * @param node AST 节点（可能为 NULL）
 * @param out  输出值
 * @return 找到整数返回 true
 */
static bool loader_expr_int(const LvAstNode *node, long long *out) {
    if (node && node->type == LV_AST_INTEGER_LITERAL) {
        *out = node->data.literal.integer_value;
        return true;
    }
    return false;
}

/**
 * @brief 提取结构化坐标：Point A := {x: N, y: M}
 *
 * STRUCT_LITERAL → child 链表为 FIELD 节点，每个 FIELD 有 name 和 value。
 * 支持字段名 "x"/"y"（及别名 "X"/"Y"）。
 *
 * @param value AST 声明值节点
 * @param out_x 输出 x（仅当找到时有效）
 * @param out_y 输出 y
 * @param has_x / has_y 输出是否找到对应字段
 */
static void loader_extract_struct_coords(const LvAstNode *value,
                                         long long *out_x, long long *out_y,
                                         bool *has_x, bool *has_y) {
    *has_x = *has_y = false;
    if (!value || value->type != LV_AST_STRUCT_LITERAL)
        return;
    for (const LvAstNode *f = value->child; f; f = f->next) {
        if (f->type != LV_AST_STRUCT_FIELD || !f->data.field.name)
            continue;
        const char *fn = f->data.field.name;
        if ((fn[0] == 'x' || fn[0] == 'X') && !*has_x)
            *has_x = loader_expr_int(f->data.field.value, out_x);
        else if ((fn[0] == 'y' || fn[0] == 'Y') && !*has_y)
            *has_y = loader_expr_int(f->data.field.value, out_y);
    }
}

/**
 * @brief 提取几何表达式坐标：Point A := point(x, y)
 *
 * GEOMETRY_EXPR 且 func_name == "point"，参数链表为整数列表。
 * 取前两个整数作为坐标。
 *
 * @param value AST 声明值节点
 * @param out_x / out_y 输出坐标
 * @return 成功提取两个坐标返回 true
 */
static bool loader_extract_point_expr(const LvAstNode *value,
                                      long long *out_x, long long *out_y) {
    if (!value || value->type != LV_AST_GEOMETRY_EXPR)
        return false;
    if (!value->data.call.func_name || !lv_str_eq(value->data.call.func_name, "point"))
        return false;
    const LvAstNode *args = value->data.call.args;
    long long x, y;
    if (!args || !loader_expr_int(args, &x))
        return false;
    args = args->next;
    if (!args || !loader_expr_int(args, &y))
        return false;
    *out_x = x;
    *out_y = y;
    return true;
}

/** 注册为自由点（默认坐标 (0,1,0,1) 即 (0,0)；带声明值时使用真实坐标） */
static void decl_register_point(lvEngine *engine, const char *name) {
    int id = lv_add_point(engine, 0, 1, 0, 1);
    if (id >= 0)
        loader_names_add(name, id);
}

/**
 * @brief 注册带坐标的点（声明值版本）
 *
 * 支持两种声明值形态：
 *   - Point A := {x: 3, y: 4};   （结构字面量）
 *   - Point A := point(3, 4);    （几何表达式）
 * 坐标用 lv_add_point 的精确有理数接口写入。
 *
 * @param engine 引擎
 * @param name   实体名
 * @param value  声明值 AST（可为 NULL，NULL 时按默认坐标注册）
 */
static void decl_register_point_with_value(lvEngine *engine, const char *name,
                                           const LvAstNode *value) {
    long long x = 0, y = 0;
    bool has_x = false, has_y = false;
    bool ok = false;

    if (value) {
        /* 形态 1: 结构字面量 {x, y} */
        loader_extract_struct_coords(value, &x, &y, &has_x, &has_y);
        if (has_x && has_y) {
            ok = true;
        } else {
            /* 形态 2: point(x, y) 几何表达式 */
            long long px = 0, py = 0;
            if (loader_extract_point_expr(value, &px, &py)) {
                x = px; y = py; ok = true;
            }
        }
    }

    int id;
    if (ok) {
        id = lv_add_point(engine, (int64_t) x, 1, (int64_t) y, 1);
    } else {
        id = lv_add_point(engine, 0, 1, 0, 1);
    }
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
 * 带声明值（":= Expr"）时：
 *   - Point A := {x: N, y: M}; / Point A := point(N, M);  → 真实坐标
 *   - Line L := line(a, b);    → 创建线段（第二遍端点已知后）
 *   - 其他类型 → 暂存名称（与无值一致）
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
    lv_strlcpy(buf, names, sizeof(buf));

    /* VTable 调度（越界/未登记类型走暂存，对应原 default 分支） */
    EntityDeclHandler handler = decl_stash;
    if ((unsigned)etype < (unsigned)lv_ARRAY_SIZE(kEntityDeclHandlers))
        handler = kEntityDeclHandlers[etype];

    /* 拆分逗号分隔的名称列表 */
    char *save;
    char *tok = lv_strtok_r(buf, ",", &save);
    while (tok) {
        /* 带声明值且为 Point 时走坐标注册；声明值仅为单个名称服务 */
        if (node->data.decl.value && etype == LV_ENTITY_POINT) {
            decl_register_point_with_value(engine, tok, node->data.decl.value);
        } else {
            handler(engine, tok);
        }
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
        lv_strlcpy(result.errors[0].message, "filepath is NULL", sizeof(result.errors[0].message));
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

    /* F16/G1：解析安全闸门 —— 输入长度/非法字符校验（上限读
     * lvConfig.parser.parser_max_input_length，默认 1MB 可配置不硬编码）；
     * 校验失败直接拒绝，不进入 Lex/Parse。 */
    lvErrorCode vrc = lv_input_validate(source, len);
    if (vrc != lv_OK) {
        result.error_count = 1;
        const char *emsg = lv_get_last_error_message();
        lv_strlcpy(result.errors[0].message, emsg ? emsg : "input validation failed", sizeof(result.errors[0].message));
        lv_free((void **) &source);
        return result;
    }

    /* Lex → Parse */
    LvLexer *lexer = lv_lexer_create(source, len);
    if (!lexer) {
        lv_free((void **) &source);
        result.error_count = 1;
        lv_strlcpy(result.errors[0].message, "failed to create lexer", sizeof(result.errors[0].message));
        return result;
    }

    LvParser *parser = lv_parser_create(lexer);
    if (!parser) {
        lv_lexer_destroy(lexer);
        lv_free((void **) &source);
        result.error_count = 1;
        lv_strlcpy(result.errors[0].message, "failed to create parser", sizeof(result.errors[0].message));
        return result;
    }

    result = lv_parser_parse_program(parser);

    /* K28/F54：AST 深度闸门 —— 解析完成后遍历 AST 测深度，
     * 超限即报错拒绝（上限读 lvConfig.parser.parser_max_ast_depth，
     * 默认 256 可经 lv.config.json 调整，不硬编码）。
     * 正常 .lv 文件深度远小于上限；深嵌套/恶意输入在此被拦。 */
    if (result.error_count == 0 && result.ast) {
        int ast_depth = lv_ast_max_depth(result.ast);
        if (ast_depth > 0 && lv_check_ast_depth(ast_depth) != lv_OK) {
            if (result.error_count < 64) {
                result.errors[result.error_count].loc.line = 0;
                lv_snprintf(result.errors[result.error_count].message,
                            sizeof(result.errors[result.error_count].message),
                            "AST 深度 %d 超过上限（见 lvConfig.parser.parser_max_ast_depth）", ast_depth);
                result.error_count++;
            }
        }
        /* F16/G1：AST 节点数闸门（上限 lvConfig.parser.parser_max_ast_nodes，
         * 默认 500000 可配置不硬编码） */
        if (result.error_count == 0) {
            int ast_nodes = lv_ast_node_count(result.ast);
            if (ast_nodes > 0 && lv_check_ast_node_count(ast_nodes) != lv_OK) {
                if (result.error_count < 64) {
                    result.errors[result.error_count].loc.line = 0;
                    lv_snprintf(result.errors[result.error_count].message,
                                sizeof(result.errors[result.error_count].message),
                                "AST 节点数 %d 超过上限（见 lvConfig.parser.parser_max_ast_nodes）", ast_nodes);
                    result.error_count++;
                }
            }
        }
    }

    lv_parser_destroy(parser);
    lv_lexer_destroy(lexer);
    lv_free((void **) &source);

    return result;
}

/**
 * @brief 将解析结果应用到引擎
 *
 * 三遍处理策略：
 * 1. 第一遍：处理声明，向引擎添加几何对象（点等）
 * 2. 第二遍：处理需要端点已知的实体（线段/直线：Line L := line(a, b);
 *    / Segment S := segment(a, b);，端点在第一遍注册后此处创建线段）
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
        if (stmt->type != LV_AST_DECLARATION)
            continue;
        LvEntityType etype = (LvEntityType) stmt->data.decl.entity_type;
        if (etype != LV_ENTITY_SEGMENT && etype != LV_ENTITY_LINE)
            continue;
        if (!stmt->data.decl.value)
            continue; /* 无构造值，无法创建 */

        /* 支持 Line L := line(a, b); / Segment S := line(a, b); / segment(a, b);
         * 构造值形态：GEOMETRY_EXPR，func_name ∈ {line, segment}，参数为两个标识符 */
        const LvAstNode *v = stmt->data.decl.value;
        if (v->type != LV_AST_GEOMETRY_EXPR)
            continue;
        const char *fn = v->data.call.func_name;
        if (!fn || !(lv_str_eq(fn, "line") || lv_str_eq(fn, "segment")))
            continue;
        const LvAstNode *a0 = v->data.call.args;
        const LvAstNode *a1 = a0 ? a0->next : NULL;
        if (!a0 || !a1 || a0->type != LV_AST_IDENTIFIER_EXPR || a1->type != LV_AST_IDENTIFIER_EXPR)
            continue;

        int p1 = loader_names_lookup(a0->data.ident.name);
        int p2 = loader_names_lookup(a1->data.ident.name);
        if (p1 < 0 || p2 < 0)
            continue; /* 端点未注册，跳过 */

        int seg_id = lv_add_line_segment(engine, p1, p2);
        if (seg_id >= 0) {
            /* 名称列表拆分（声明值只作用于首个名称，此处为单名声明场景） */
            char buf[1024];
            lv_strlcpy(buf, stmt->data.decl.names, sizeof(buf));
            char *save;
            char *tok = lv_strtok_r(buf, ",", &save);
            if (tok)
                loader_names_add(tok, seg_id);
        }
    }

    /* 第三遍：处理 Constraint 和 Prove 语句 */
    for (LvAstNode *stmt = ast->child; stmt; stmt = stmt->next) {
        if (stmt->type == LV_AST_CONSTRAINT_STMT) {
            LvAstNode *expr = stmt->data.stmt.expr;
            if (!expr)
                continue;

            /* ── 形态 1: dist(a, b) = N; —— 距离约束
             *   AST: COMPARE(op==, left={MEASURE|FUNCTION_CALL}{dist/distance, args=[a,b]}, right=INT N)
             *   dist 作为普通标识符解析为 FUNCTION_CALL；distance 关键字解析为 MEASURE。 */
            if (expr->type == LV_AST_COMPARE && expr->data.compare.left &&
                (expr->data.compare.left->type == LV_AST_MEASURE ||
                 expr->data.compare.left->type == LV_AST_FUNCTION_CALL)) {
                const LvAstNode *call = expr->data.compare.left;
                const char *fn = call->data.call.func_name;
                if (fn && (lv_str_eq(fn, "dist") || lv_str_eq(fn, "distance"))) {
                    const LvAstNode *a0 = call->data.call.args;
                    const LvAstNode *a1 = a0 ? a0->next : NULL;
                    if (a0 && a1 && a0->type == LV_AST_IDENTIFIER_EXPR &&
                        a1->type == LV_AST_IDENTIFIER_EXPR) {
                        int id0 = loader_names_lookup(a0->data.ident.name);
                        int id1 = loader_names_lookup(a1->data.ident.name);
                        long long dist_val = 0;
                        loader_expr_int(expr->data.compare.right, &dist_val);
                        if (id0 >= 0 && id1 >= 0 && engine_get_main_graph(engine)) {
                            /* 用 INCIDENCE 承载距离约束对，numeric_value 存距离值 */
                            int participants[2] = {id0, id1};
                            int cid = engine_get_main_graph(engine)->next_constraint_id;
                            Constraint *c = graph_add_constraint_with_id(
                                engine_get_main_graph(engine), cid, INCIDENCE, participants, 2);
                            if (c) {
                                c->numeric_value = (double) dist_val;
                                c->satisfaction = 1.0;
                            }
                        }
                    }
                }
                continue;
            }

            /* ── 形态 2: 关系调用 collinear/perpendicular/parallel/... —— 关系约束
             *   AST: RELATION{func_name, args=[a,b,c,...]} */
            if (expr->type == LV_AST_RELATION) {
                const char *fname = expr->data.call.func_name;
                int arg_ids[16];
                int arg_count = 0;
                for (const LvAstNode *a = expr->data.call.args; a && arg_count < 16; a = a->next) {
                    if (a->type == LV_AST_IDENTIFIER_EXPR) {
                        int id = loader_names_lookup(a->data.ident.name);
                        if (id >= 0)
                            arg_ids[arg_count++] = id;
                    }
                }

                if (fname && engine_get_main_graph(engine)) {
                    if (lv_str_eq(fname, "collinear") && arg_count >= 3) {
                        /* 共线：依次用 betweenness 连接首点与其余各点 */
                        for (int i = 1; i + 1 < arg_count; i++) {
                            int p[3] = {arg_ids[0], arg_ids[i], arg_ids[i + 1]};
                            int cid = engine_get_main_graph(engine)->next_constraint_id;
                            graph_add_constraint_with_id(engine_get_main_graph(engine), cid,
                                                         BETWEENNESS, p, 3);
                        }
                    } else if (lv_str_eq(fname, "perpendicular") && arg_count >= 2) {
                        int p[2] = {arg_ids[0], arg_ids[1]};
                        int cid = engine_get_main_graph(engine)->next_constraint_id;
                        graph_add_constraint_with_id(engine_get_main_graph(engine), cid,
                                                     ANGLE, p, 2);
                    } else if (lv_str_eq(fname, "containment") && arg_count >= 2) {
                        int p[2] = {arg_ids[0], arg_ids[1]};
                        int cid = engine_get_main_graph(engine)->next_constraint_id;
                        graph_add_constraint_with_id(engine_get_main_graph(engine), cid,
                                                     CONTAINMENT, p, 2);
                    } else if (lv_str_eq(fname, "connection") && arg_count >= 2) {
                        int p[2] = {arg_ids[0], arg_ids[1]};
                        int cid = engine_get_main_graph(engine)->next_constraint_id;
                        graph_add_constraint_with_id(engine_get_main_graph(engine), cid,
                                                     CONNECTION, p, 2);
                    }
                }
                continue;
            }

            /* ── 形态 3: 其他已命名约束（保留原样，语义由引擎后续处理） */
        } else if (stmt->type == LV_AST_PROVE_STMT) {
            /* Prove 语句：加载阶段不改变引擎状态（证明目标不影响图结构）。
             * 验证语义由 lv_verify_proofs / lv_load_file_verified 完成
             * （见下方「微自举 B —— 证明验证器」）。 */
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
 *   5. 命题逻辑验证（首次自举）：对纯命题公式（and/or/not/->/iff +
 *      原子命题）穷举全真值表（2^n）验证恒真，反例判 FAIL；
 *   其余（量词、未知函数、除零等）→ SKIP，不误报。
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

/* ── 表达式折叠：闭合表达式 → LvVal（VTable 化：按节点类型查表分发，替代 switch）──
 *
 * 说明：LvAstVTable 类型与 kAstVTable 定义于 lv_ast.c 内部（lv_ast.h 为公共头，
 * 被 lv_parser.h/lv_sema.h 等多文件消费，VTable 类型按约定不对外暴露），且本文件
 * 求值 handler 依赖本文件私有的 LvVal/Church 表，故 AST 求值分发表定义于本文件；
 * 槽位语义与 lv_ast.c 中 LvAstVTable 新增的求值槽位一一对应。 */

static bool fold_expr(LvAstNode *node, LvVal *out, int depth);

/** @brief 折叠 handler 类型：闭合表达式 → LvVal */
typedef bool (*AstFoldFn)(LvAstNode *node, LvVal *out, int depth);

static bool fold_integer_literal(LvAstNode *node, LvVal *out, int depth) {
    (void) depth;
    out->kind = LV_VAL_NUM;
    out->num = node->data.literal.integer_value;
    return true;
}

static bool fold_bool_literal(LvAstNode *node, LvVal *out, int depth) {
    (void) depth;
    out->kind = LV_VAL_BOOL;
    out->boolean = node->data.literal.bool_value ? 1 : 0;
    return true;
}

static bool fold_binary_op(LvAstNode *node, LvVal *out, int depth) {
    LvVal l, r;
    if (!fold_expr(node->data.binary.left, &l, depth + 1))
        return false;
    if (!fold_expr(node->data.binary.right, &r, depth + 1))
        return false;
    if (l.kind != LV_VAL_NUM || r.kind != LV_VAL_NUM)
        return false;
    const char *op = node->data.binary.op;
    if (lv_str_eq(op, "+")) {
        out->kind = LV_VAL_NUM; out->num = l.num + r.num; return true;
    }
    if (lv_str_eq(op, "-")) {
        out->kind = LV_VAL_NUM; out->num = l.num - r.num; return true;
    }
    if (lv_str_eq(op, "*")) {
        out->kind = LV_VAL_NUM; out->num = l.num * r.num; return true;
    }
    if (lv_str_eq(op, "/")) {
        if (r.num == 0)
            return false;
        out->kind = LV_VAL_NUM; out->num = l.num / r.num; return true;
    }
    if (lv_str_eq(op, "^")) {
        if (r.num < 0)
            return false;
        long long v = 1;
        for (long long i = 0; i < r.num && v <= 0x3fffffffffffffffLL; i++)
            v *= l.num;
        out->kind = LV_VAL_NUM; out->num = v; return true;
    }
    return false;
}

static bool fold_unary_op(LvAstNode *node, LvVal *out, int depth) {
    LvVal v;
    if (!fold_expr(node->data.unary.operand, &v, depth + 1))
        return false;
    if (v.kind != LV_VAL_NUM)
        return false;
    if (lv_str_eq(node->data.unary.op, "-")) {
        out->kind = LV_VAL_NUM; out->num = -v.num; return true;
    }
    if (lv_str_eq(node->data.unary.op, "+")) {
        *out = v; return true;
    }
    return false;
}

static bool fold_function_call(LvAstNode *node, LvVal *out, int depth) {
    const char *fname = node->data.call.func_name;
    if (!fname)
        return false;
    const ChurchFnEntry *entry = NULL;
    for (size_t i = 0; i < lv_ARRAY_SIZE(kChurchFnTable); i++) {
        if (lv_str_eq(kChurchFnTable[i].name, fname)) {
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

/** @brief 折叠分发表（按节点类型索引；未登记类型为 NULL，对应原 default 分支返回 false） */
static const AstFoldFn kFoldDispatch[LV_AST_COUNT] = {
    [LV_AST_INTEGER_LITERAL] = fold_integer_literal,
    [LV_AST_BOOL_LITERAL]    = fold_bool_literal,
    [LV_AST_BINARY_OP]       = fold_binary_op,
    [LV_AST_UNARY_OP]        = fold_unary_op,
    [LV_AST_FUNCTION_CALL]   = fold_function_call,
};

static bool fold_expr(LvAstNode *node, LvVal *out, int depth) {
    if (!node || !out)
        return false;
    if (depth > LV_PROVE_MAX_DEPTH)
        return false;
    return LV_DISPATCH(kFoldDispatch, node->type, false, node, out, depth);
}

/* ── 命题求值：返回 -1 无法判定，0 假，1 真（VTable 化：按节点类型查表分发，替代 switch）── */

static int eval_proposition(LvAstNode *node, int depth);

static int eval_bool_literal(const LvAstNode *node, int depth) {
    (void) depth;
    return node->data.literal.bool_value ? 1 : 0;
}

static int eval_compare(const LvAstNode *node, int depth) {
    const char *op = node->data.compare.op;
    LvVal l, r;
    if (!fold_expr(node->data.compare.left, &l, depth + 1))
        return -1;
    if (!fold_expr(node->data.compare.right, &r, depth + 1))
        return -1;
    if (l.kind == LV_VAL_NUM && r.kind == LV_VAL_NUM) {
        if (lv_str_eq(op, "==")) return l.num == r.num ? 1 : 0;
        if (lv_str_eq(op, "!=")) return l.num != r.num ? 1 : 0;
        if (lv_str_eq(op, "<"))  return l.num < r.num ? 1 : 0;
        if (lv_str_eq(op, "<=")) return l.num <= r.num ? 1 : 0;
        if (lv_str_eq(op, ">"))  return l.num > r.num ? 1 : 0;
        if (lv_str_eq(op, ">=")) return l.num >= r.num ? 1 : 0;
        return -1;
    }
    if (l.kind == LV_VAL_BOOL && r.kind == LV_VAL_BOOL) {
        if (lv_str_eq(op, "==")) return l.boolean == r.boolean ? 1 : 0;
        if (lv_str_eq(op, "!=")) return l.boolean != r.boolean ? 1 : 0;
        return -1;
    }
    return -1;
}

static int eval_logic_and(const LvAstNode *node, int depth) {
    int l = eval_proposition(node->data.binary.left, depth + 1);
    int r = eval_proposition(node->data.binary.right, depth + 1);
    if (l < 0 || r < 0)
        return -1;
    return (l && r) ? 1 : 0;
}

static int eval_logic_or(const LvAstNode *node, int depth) {
    int l = eval_proposition(node->data.binary.left, depth + 1);
    int r = eval_proposition(node->data.binary.right, depth + 1);
    if (l < 0 || r < 0)
        return -1;
    return (l || r) ? 1 : 0;
}

static int eval_logic_not(const LvAstNode *node, int depth) {
    int v = eval_proposition(node->data.unary.operand, depth + 1);
    return v < 0 ? -1 : (v ? 0 : 1);
}

static int eval_logic_implies(const LvAstNode *node, int depth) {
    int l = eval_proposition(node->data.binary.left, depth + 1);
    int r = eval_proposition(node->data.binary.right, depth + 1);
    if (l < 0 || r < 0)
        return -1;
    return (l == 1 && r == 0) ? 0 : 1;
}

static int eval_logic_iff(const LvAstNode *node, int depth) {
    int l = eval_proposition(node->data.binary.left, depth + 1);
    int r = eval_proposition(node->data.binary.right, depth + 1);
    if (l < 0 || r < 0)
        return -1;
    return (l == r) ? 1 : 0;
}

static int eval_relation(const LvAstNode *node, int depth) {
    (void) depth;
    /* 反射律：所有参数为同一标识符的关系调用恒真（如 collinear(A,A,A)） */
    const char *first_name = NULL;
    for (LvAstNode *a = node->data.call.args; a; a = a->next) {
        if (a->type != LV_AST_IDENTIFIER_EXPR || !a->data.ident.name)
            return -1;
        if (!first_name)
            first_name = a->data.ident.name;
        else if (lv_str_ne(first_name, a->data.ident.name))
            return -1;
    }
    return first_name ? 1 : -1;
}

/** @brief 命题求值 handler 类型 */
typedef int (*AstEvalPropFn)(const LvAstNode *node, int depth);

/** @brief 命题求值分发表（按节点类型索引；未登记类型走原 default 分支） */
static const AstEvalPropFn kEvalPropDispatch[LV_AST_COUNT] = {
    [LV_AST_BOOL_LITERAL]  = eval_bool_literal,
    [LV_AST_COMPARE]       = eval_compare,
    [LV_AST_LOGIC_AND]     = eval_logic_and,
    [LV_AST_LOGIC_OR]      = eval_logic_or,
    [LV_AST_LOGIC_NOT]     = eval_logic_not,
    [LV_AST_LOGIC_IMPLIES] = eval_logic_implies,
    [LV_AST_LOGIC_IFF]     = eval_logic_iff,
    [LV_AST_RELATION]      = eval_relation,
};

static int eval_proposition(LvAstNode *node, int depth) {
    if (!node)
        return -1;
    if (depth > LV_PROVE_MAX_DEPTH)
        return -1;
    if ((unsigned) node->type < (unsigned) LV_AST_COUNT && kEvalPropDispatch[node->type])
        return kEvalPropDispatch[node->type](node, depth);
    /* 纯布尔目标：如 Prove eq(2, 2); / Prove iszero(0);（原 default 分支） */
    {
        LvVal v;
        if (fold_expr(node, &v, depth + 1) && v.kind == LV_VAL_BOOL)
            return v.boolean ? 1 : 0;
    }
    return -1;
}

/* ── 命题逻辑验证：全真值表枚举（首次自举，路线图步骤 6）──
 *
 * 对"纯命题骨架"表达式（仅由布尔字面量、逻辑运算符 and/or/not/->/iff
 * 与裸标识符（原子命题）构成）穷举全部命题变量赋值（2^n 行真值表）：
 *   - 所有赋值下公式为真 → 恒真（tautology）→ 返回 1；
 *   - 存在反例赋值 → 返回 0；
 *   - 变量数超限 / 非纯命题骨架 / 求值失败 → 返回 -1（回退既有语义）。
 * 既有验证语义（Church 归约、算术、反射律、量词/未知函数 → SKIP）
 * 完全不受影响：本函数仅在纯命题骨架时接管，其余情况一律返回 -1 回退。 */

#define LV_PROP_MAX_VARS 8       /**< 真值表枚举的命题变量数上限（2^8 行） */
#define LV_PROP_VAR_NAME_MAX 32  /**< 命题变量名长度上限 */

/** @brief 是否为纯命题骨架：仅布尔字面量 + 逻辑运算 + 裸标识符（查表分发，替代 switch） */
static bool is_pure_propositional(LvAstNode *node, int depth);

static bool is_pure_leaf(const LvAstNode *node, int depth) {
    (void) node;
    (void) depth;
    return true; /* BOOL_LITERAL / IDENTIFIER_EXPR */
}

static bool is_pure_logic_binary(const LvAstNode *node, int depth) {
    return is_pure_propositional(node->data.binary.left, depth + 1) &&
           is_pure_propositional(node->data.binary.right, depth + 1);
}

static bool is_pure_logic_not(const LvAstNode *node, int depth) {
    return is_pure_propositional(node->data.unary.operand, depth + 1);
}

/** @brief 纯命题骨架判定 handler 类型 */
typedef bool (*AstIsPureFn)(const LvAstNode *node, int depth);

/** @brief 纯命题骨架判定分发表（未登记类型走原 default 分支返回 false） */
static const AstIsPureFn kIsPureDispatch[LV_AST_COUNT] = {
    [LV_AST_BOOL_LITERAL]    = is_pure_leaf,
    [LV_AST_IDENTIFIER_EXPR] = is_pure_leaf,
    [LV_AST_LOGIC_AND]       = is_pure_logic_binary,
    [LV_AST_LOGIC_OR]        = is_pure_logic_binary,
    [LV_AST_LOGIC_IMPLIES]   = is_pure_logic_binary,
    [LV_AST_LOGIC_IFF]       = is_pure_logic_binary,
    [LV_AST_LOGIC_NOT]       = is_pure_logic_not,
};

static bool is_pure_propositional(LvAstNode *node, int depth) {
    if (!node || depth > LV_PROVE_MAX_DEPTH)
        return false;
    return LV_DISPATCH(kIsPureDispatch, node->type, false, node, depth);
}

/** @brief 收集表达式中的原子命题名（去重，按首次出现顺序）；变量数超限置 overflow（查表分发，替代 switch） */
static void collect_prop_vars(LvAstNode *node,
                              char vars[][LV_PROP_VAR_NAME_MAX],
                              int *count,
                              bool *overflow,
                              int depth);

static void collect_ident(LvAstNode *node,
                          char vars[][LV_PROP_VAR_NAME_MAX],
                          int *count,
                          bool *overflow,
                          int depth) {
    (void) depth;
    const char *name = node->data.ident.name;
    if (!name) {
        *overflow = true;
        return;
    }
    for (int i = 0; i < *count; i++) {
        if (lv_str_eq(vars[i], name))
            return;
    }
    if (*count >= LV_PROP_MAX_VARS) {
        *overflow = true;
        return;
    }
    lv_strlcpy(vars[*count], name, LV_PROP_VAR_NAME_MAX);
    (*count)++;
}

static void collect_logic_binary(LvAstNode *node,
                                 char vars[][LV_PROP_VAR_NAME_MAX],
                                 int *count,
                                 bool *overflow,
                                 int depth) {
    collect_prop_vars(node->data.binary.left, vars, count, overflow, depth + 1);
    collect_prop_vars(node->data.binary.right, vars, count, overflow, depth + 1);
}

static void collect_logic_not(LvAstNode *node,
                              char vars[][LV_PROP_VAR_NAME_MAX],
                              int *count,
                              bool *overflow,
                              int depth) {
    collect_prop_vars(node->data.unary.operand, vars, count, overflow, depth + 1);
}

/** @brief 原子命题名收集 handler 类型 */
typedef void (*AstCollectVarsFn)(LvAstNode *node, char vars[][LV_PROP_VAR_NAME_MAX], int *count, bool *overflow,
                                 int depth);

/** @brief 原子命题名收集分发表（未登记类型走原 default 分支，无操作） */
static const AstCollectVarsFn kCollectVarsDispatch[LV_AST_COUNT] = {
    [LV_AST_IDENTIFIER_EXPR] = collect_ident,
    [LV_AST_LOGIC_AND]       = collect_logic_binary,
    [LV_AST_LOGIC_OR]        = collect_logic_binary,
    [LV_AST_LOGIC_IMPLIES]   = collect_logic_binary,
    [LV_AST_LOGIC_IFF]       = collect_logic_binary,
    [LV_AST_LOGIC_NOT]       = collect_logic_not,
};

static void collect_prop_vars(LvAstNode *node,
                              char vars[][LV_PROP_VAR_NAME_MAX],
                              int *count,
                              bool *overflow,
                              int depth) {
    if (!node || depth > LV_PROVE_MAX_DEPTH || *overflow)
        return;
    LV_DISPATCH_VOID(kCollectVarsDispatch, node->type, node, vars, count, overflow, depth);
}

/** @brief 在给定赋值下求值纯命题骨架：返回 -1 无法判定，0 假，1 真（查表分发，替代 switch） */
static int eval_prop_skeleton(LvAstNode *node,
                              const char vars[][LV_PROP_VAR_NAME_MAX],
                              const int *vals,
                              int nvars,
                              int depth);

static int skeleton_bool_literal(const LvAstNode *node,
                                 const char vars[][LV_PROP_VAR_NAME_MAX],
                                 const int *vals, int nvars, int depth) {
    (void) vars; (void) vals; (void) nvars; (void) depth;
    return node->data.literal.bool_value ? 1 : 0;
}

static int skeleton_ident(const LvAstNode *node,
                          const char vars[][LV_PROP_VAR_NAME_MAX],
                          const int *vals, int nvars, int depth) {
    (void) depth;
    const char *name = node->data.ident.name;
    if (!name)
        return -1;
    for (int i = 0; i < nvars; i++) {
        if (lv_str_eq(vars[i], name))
            return vals[i] ? 1 : 0;
    }
    return -1;
}

static int skeleton_logic_and(const LvAstNode *node,
                              const char vars[][LV_PROP_VAR_NAME_MAX],
                              const int *vals, int nvars, int depth) {
    int l = eval_prop_skeleton(node->data.binary.left, vars, vals, nvars, depth + 1);
    int r = eval_prop_skeleton(node->data.binary.right, vars, vals, nvars, depth + 1);
    if (l < 0 || r < 0)
        return -1;
    return (l && r) ? 1 : 0;
}

static int skeleton_logic_or(const LvAstNode *node,
                             const char vars[][LV_PROP_VAR_NAME_MAX],
                             const int *vals, int nvars, int depth) {
    int l = eval_prop_skeleton(node->data.binary.left, vars, vals, nvars, depth + 1);
    int r = eval_prop_skeleton(node->data.binary.right, vars, vals, nvars, depth + 1);
    if (l < 0 || r < 0)
        return -1;
    return (l || r) ? 1 : 0;
}

static int skeleton_logic_not(const LvAstNode *node,
                              const char vars[][LV_PROP_VAR_NAME_MAX],
                              const int *vals, int nvars, int depth) {
    int v = eval_prop_skeleton(node->data.unary.operand, vars, vals, nvars, depth + 1);
    return v < 0 ? -1 : (v ? 0 : 1);
}

static int skeleton_logic_implies(const LvAstNode *node,
                                  const char vars[][LV_PROP_VAR_NAME_MAX],
                                  const int *vals, int nvars, int depth) {
    int l = eval_prop_skeleton(node->data.binary.left, vars, vals, nvars, depth + 1);
    int r = eval_prop_skeleton(node->data.binary.right, vars, vals, nvars, depth + 1);
    if (l < 0 || r < 0)
        return -1;
    return (l == 1 && r == 0) ? 0 : 1;
}

static int skeleton_logic_iff(const LvAstNode *node,
                              const char vars[][LV_PROP_VAR_NAME_MAX],
                              const int *vals, int nvars, int depth) {
    int l = eval_prop_skeleton(node->data.binary.left, vars, vals, nvars, depth + 1);
    int r = eval_prop_skeleton(node->data.binary.right, vars, vals, nvars, depth + 1);
    if (l < 0 || r < 0)
        return -1;
    return (l == r) ? 1 : 0;
}

/** @brief 真值表骨架求值 handler 类型 */
typedef int (*AstEvalSkeletonFn)(const LvAstNode *node, const char vars[][LV_PROP_VAR_NAME_MAX],
                                 const int *vals, int nvars, int depth);

/** @brief 真值表骨架求值分发表（未登记类型走原 default 分支返回 -1） */
static const AstEvalSkeletonFn kEvalSkeletonDispatch[LV_AST_COUNT] = {
    [LV_AST_BOOL_LITERAL]    = skeleton_bool_literal,
    [LV_AST_IDENTIFIER_EXPR] = skeleton_ident,
    [LV_AST_LOGIC_AND]       = skeleton_logic_and,
    [LV_AST_LOGIC_OR]        = skeleton_logic_or,
    [LV_AST_LOGIC_NOT]       = skeleton_logic_not,
    [LV_AST_LOGIC_IMPLIES]   = skeleton_logic_implies,
    [LV_AST_LOGIC_IFF]       = skeleton_logic_iff,
};

static int eval_prop_skeleton(LvAstNode *node,
                              const char vars[][LV_PROP_VAR_NAME_MAX],
                              const int *vals,
                              int nvars,
                              int depth) {
    if (!node || depth > LV_PROVE_MAX_DEPTH)
        return -1;
    return LV_DISPATCH(kEvalSkeletonDispatch, node->type, -1, node, vars, vals, nvars, depth);
}

/** @brief 命题逻辑全真值表验证：恒真 → 1，有反例 → 0，无法判定 → -1 */
static int eval_propositional_truth_table(LvAstNode *node) {
    if (!node || !is_pure_propositional(node, 0))
        return -1;

    char vars[LV_PROP_MAX_VARS][LV_PROP_VAR_NAME_MAX];
    int nvars = 0;
    bool overflow = false;
    collect_prop_vars(node, vars, &nvars, &overflow, 0);
    if (overflow)
        return -1; /* 变量数超限：保守回退既有语义（SKIP） */

    /* 无命题变量：直接求值（与既有布尔逻辑语义一致） */
    if (nvars == 0)
        return eval_prop_skeleton(node, vars, NULL, 0, 0);

    int total = 1;
    for (int i = 0; i < nvars; i++)
        total *= 2;

    for (int mask = 0; mask < total; mask++) {
        int vals[LV_PROP_MAX_VARS];
        for (int i = 0; i < nvars; i++)
            vals[i] = (mask >> i) & 1;
        int r = eval_prop_skeleton(node, vars, vals, nvars, 0);
        if (r < 0)
            return -1; /* 骨架求值失败：回退既有语义 */
        if (r == 0)
            return 0;  /* 存在反例：非恒真 */
    }
    return 1; /* 所有赋值均为真：恒真 */
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

    /* 首次自举：先尝试命题逻辑全真值表验证（纯命题骨架）；
       非骨架表达式返回 -1 回退既有语义（Church/算术/反射律/SKIP） */
    int r = eval_propositional_truth_table(expr);
    if (r < 0)
        r = eval_proposition(expr, 0);
    if (r < 0) {
        rep->verdict = LV_PROVE_SKIP;
        lv_snprintf(rep->detail, sizeof(rep->detail),
                    "not mechanically decidable (quantifier / unknown function / open term)");
        summary->skip_count++;
    } else if (r == 1) {
        rep->verdict = LV_PROVE_PASS;
        lv_snprintf(rep->detail, sizeof(rep->detail),
                    "verified: conclusion holds (truth-table / church-eval / arith / logic)");
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