#include "lv/lv_platform.h"

#include "lv/lv_sema.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_xmacro.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "lv/lv_hashtable.h"
#include "lv_internal.h"
#include "lv_utils.h"

/* ── 符号表 ──
 *
 * 【lv_hashtable 收敛评估结论（已迁移，2026-08-08）】
 * 原实现为固定数组 LvSymbol symbols[LV_SEMA_MAX_SYMBOLS]（上限 256）+ 线性
 * strcmp 扫描（find_symbol/add_symbol），满时报 "symbol table full"。
 * 符号表语义为 name → (type, loc) 注册表，键是符号名字符串，与
 * lv_hashtable_str 形态完全匹配，故迁移：
 *   - 键 = 符号名（表内部持有副本，不再受 64 字节截断与 256 上限约束）；
 *   - 值 = LvSymbol*（堆分配，类型/位置信息），所有权归本模块，
 *     destroy / analyze 重置时释放；
 *   - 查重从 O(n) strcmp 线性扫描降为 O(1) 哈希查找。
 * 外部行为不变：重复声明仍报 "duplicate declaration"，仅内存分配失败时
 * add_symbol 返回 false（不再有 "symbol table full" 错误路径）。
 */
#define LV_SEMA_MAX_ERRORS 64

typedef struct {
    char name[64]; /* 冗余保留：表键已存完整名字，此字段仅保持既有结构形态 */
    LvSemanticType type;
    LvSourceLoc loc;
} LvSymbol;

struct LvSemaContext {
    lvHashtable *symbols; /* str 形态：符号名 → LvSymbol* */
    char errors[LV_SEMA_MAX_ERRORS][256];
    int error_count;
};

/* ── 内部辅助 ── */

static void sema_error(LvSemaContext *ctx, LvSourceLoc loc, const char *fmt, ...) {
    if (ctx->error_count >= LV_SEMA_MAX_ERRORS)
        return;
    char *buf = ctx->errors[ctx->error_count];
    int n = lv_snprintf(buf, 256, "(%d,%d) ", loc.line, loc.column);
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf + n, (size_t) (256 - n), fmt, args);
    va_end(args);
    ctx->error_count++;
}

/** LvEntityType → LvSemanticType 查找表 */
static const LvSemanticType kEntityToSemanticType[] = {
    [LV_ENTITY_POINT]       = LV_TYPE_POINT,
    [LV_ENTITY_LINE]        = LV_TYPE_LINE,
    [LV_ENTITY_CIRCLE]      = LV_TYPE_CIRCLE,
    [LV_ENTITY_SEGMENT]     = LV_TYPE_SEGMENT,
    [LV_ENTITY_RAY]         = LV_TYPE_RAY,
    [LV_ENTITY_ANGLE]       = LV_TYPE_ANGLE,
    [LV_ENTITY_TRIANGLE]    = LV_TYPE_TRIANGLE,
    [LV_ENTITY_POLYGON]     = LV_TYPE_POLYGON,
    [LV_ENTITY_SCALAR]      = LV_TYPE_SCALAR,
    [LV_ENTITY_BOOL]        = LV_TYPE_BOOL,
    [LV_ENTITY_PROPOSITION] = LV_TYPE_PROPOSITION,
    [LV_ENTITY_PROOF]       = LV_TYPE_PROOF,
};

/** 将 LvEntityType 映射为 LvSemanticType */
static LvSemanticType entity_to_semantic_type(LvEntityType etype) {
    if (etype >= 0 && etype < (int)(sizeof(kEntityToSemanticType) / sizeof(kEntityToSemanticType[0])))
        return kEntityToSemanticType[etype];
    return LV_TYPE_UNKNOWN;
}

/* ── 字符串↔枚举 X-macro 列表 ── */

#define LV_SEMANTIC_TYPE_X(x) \
    x(LV_TYPE_POINT, "Point") \
    x(LV_TYPE_LINE, "Line") \
    x(LV_TYPE_CIRCLE, "Circle") \
    x(LV_TYPE_SEGMENT, "Segment") \
    x(LV_TYPE_RAY, "Ray") \
    x(LV_TYPE_ANGLE, "Angle") \
    x(LV_TYPE_TRIANGLE, "Triangle") \
    x(LV_TYPE_POLYGON, "Polygon") \
    x(LV_TYPE_SCALAR, "Scalar") \
    x(LV_TYPE_BOOL, "Bool") \
    x(LV_TYPE_PROPOSITION, "Proposition") \
    x(LV_TYPE_PROOF, "Proof")

static const lvStrToEnumEntry sema_type_map[] = {
    lv_XMACRO_TO_ENUM_TABLE(LV_SEMANTIC_TYPE_X)
};

/** 根据字符串名称查找语义类型 */
static LvSemanticType type_name_to_type(const char *name) {
    if (!name)
        return LV_TYPE_UNKNOWN;
    return (LvSemanticType)lv_str_to_enum(sema_type_map, 12, name, LV_TYPE_UNKNOWN);
}

/** 释放单个符号值（lv_hashtable_str foreach 回调） */
static void free_symbol_value(const char *key, void *value, void *ctx) {
    (void) key;
    (void) ctx;
    lv_free((void **) &value);
}

/** 在符号表中查找标识符（O(1) 哈希查找） */
static LvSymbol *find_symbol(LvSemaContext *ctx, const char *name) {
    if (!ctx->symbols)
        return NULL;
    return (LvSymbol *) lv_hashtable_str_get(ctx->symbols, name);
}

/** 向符号表添加标识符 */
static bool add_symbol(LvSemaContext *ctx, const char *name, LvSemanticType type, LvSourceLoc loc) {
    if (!ctx->symbols)
        return false; /* 符号表不可用（分配失败） */
    if (find_symbol(ctx, name)) {
        sema_error(ctx, loc, "duplicate declaration: '%s'", name);
        return false;
    }
    LvSymbol *sym = (LvSymbol *) lv_calloc(1, sizeof(LvSymbol));
    if (!sym)
        return false;
    lv_strlcpy(sym->name, name, sizeof(sym->name));
    sym->type = type;
    sym->loc = loc;
    /* 表内部复制键副本（完整名字，不受 name[64] 截断限制）；
     * insert 失败仅可能是分配失败或并发重复（find 已查，理论不可达） */
    if (!lv_hashtable_str_insert(ctx->symbols, name, sym)) {
        lv_free((void **) &sym);
        return false;
    }
    return true;
}

/* ── 类型检查函数 ── */

/** 前向声明 */
static LvSemanticType check_expr(LvSemaContext *ctx, LvAstNode *node);
static void check_stmt(LvSemaContext *ctx, LvAstNode *node);

/** 处理 Declaration 节点：将声明的标识符加入符号表 */
static void check_declaration(LvSemaContext *ctx, LvAstNode *node) {
    LvSemanticType stype = entity_to_semantic_type((LvEntityType) node->data.decl.entity_type);
    const char *names = node->data.decl.names;
    if (!names)
        return;

    /* 复制 names 用于拆分，避免修改原始字符串 */
    char buf[1024];
    lv_strlcpy(buf, names, sizeof(buf));

    char *save;
    char *tok = lv_strtok_r(buf, ",", &save);
    while (tok) {
        add_symbol(ctx, tok, stype, node->loc);
        tok = lv_strtok_r(NULL, ",", &save);
    }
}

/** 处理 Let 节点 */
static void check_let(LvSemaContext *ctx, LvAstNode *node) {
    LvSemanticType decl_type = type_name_to_type(node->data.let_def.type_name);
    LvSemanticType val_type = LV_TYPE_UNKNOWN;

    if (node->data.let_def.value) {
        val_type = check_expr(ctx, node->data.let_def.value);
    }

    /* 检查值类型是否匹配声明类型 */
    if (decl_type != LV_TYPE_UNKNOWN && val_type != LV_TYPE_UNKNOWN && val_type != LV_TYPE_ERROR &&
        decl_type != val_type) {
        sema_error(ctx, node->loc, "type mismatch in Let: declared '%s', value has type '%s'",
                   node->data.let_def.type_name, "?");
    }

    if (node->data.let_def.name) {
        add_symbol(ctx, node->data.let_def.name, decl_type != LV_TYPE_UNKNOWN ? decl_type : val_type, node->loc);
    }
}

/* ── check_call 关键字表（替代手写 strcmp 链） ── */

/** @brief 在 NULL 结尾关键字表中精确匹配（strcmp 语义，避免 strstr 子串误匹配） */
static bool lv_sema_match_name(const char *fname, const char *const *names) {
    for (int i = 0; names[i] != NULL; i++) {
        if (strcmp(fname, names[i]) == 0)
            return true;
    }
    return false;
}

/** @brief 关系/度量调用函数名表
 *
 * 词表收敛：由 lv_lexer.h 共享表 lv_geometry_relation_keywords /
 * lv_measurement_keywords（NULL 结尾）提供，与 lv_parser.c 共用单一事实源，
 * 下方 check_call 直接引用。 */

typedef LvSemanticType (*GeomCallCheckFn)(LvSemaContext *ctx, LvAstNode *node);

/* 几何构造检查（查找表 handler） */

static LvSemanticType check_geom_point(LvSemaContext *ctx, LvAstNode *node) {
    for (LvAstNode *a = node->data.call.args; a; a = a->next) {
        check_expr(ctx, a);
    }
    return LV_TYPE_POINT;
}

static LvSemanticType check_geom_line(LvSemaContext *ctx, LvAstNode *node) {
    for (LvAstNode *a = node->data.call.args; a; a = a->next) {
        LvSemanticType at = check_expr(ctx, a);
        if (at != LV_TYPE_POINT && at != LV_TYPE_ERROR) {
            sema_error(ctx, a->loc, "line() expects Point arguments");
        }
    }
    return LV_TYPE_LINE;
}

static LvSemanticType check_geom_segment(LvSemaContext *ctx, LvAstNode *node) {
    for (LvAstNode *a = node->data.call.args; a; a = a->next) {
        LvSemanticType at = check_expr(ctx, a);
        if (at != LV_TYPE_POINT && at != LV_TYPE_ERROR) {
            sema_error(ctx, a->loc, "segment() expects Point arguments");
        }
    }
    return LV_TYPE_SEGMENT;
}

static LvSemanticType check_geom_circle(LvSemaContext *ctx, LvAstNode *node) {
    LvAstNode *args = node->data.call.args;
    if (args) {
        check_expr(ctx, args);
        if (args->next)
            check_expr(ctx, args->next);
    }
    return LV_TYPE_CIRCLE;
}

static LvSemanticType check_geom_ray(LvSemaContext *ctx, LvAstNode *node) {
    for (LvAstNode *a = node->data.call.args; a; a = a->next) {
        LvSemanticType at = check_expr(ctx, a);
        if (at != LV_TYPE_POINT && at != LV_TYPE_ERROR) {
            sema_error(ctx, a->loc, "ray() expects Point arguments");
        }
    }
    return LV_TYPE_RAY;
}

static LvSemanticType check_geom_triangle(LvSemaContext *ctx, LvAstNode *node) {
    for (LvAstNode *a = node->data.call.args; a; a = a->next) {
        LvSemanticType at = check_expr(ctx, a);
        if (at != LV_TYPE_POINT && at != LV_TYPE_ERROR) {
            sema_error(ctx, a->loc, "triangle() expects Point arguments");
        }
    }
    return LV_TYPE_TRIANGLE;
}

/** @brief 几何构造函数名→检查函数 查找表（替代 6 段 if 链） */
static const struct {
    const char *name;
    GeomCallCheckFn handler;
} kGeomConstructorTable[] = {
    {"point", check_geom_point},
    {"line", check_geom_line},
    {"segment", check_geom_segment},
    {"circle", check_geom_circle},
    {"ray", check_geom_ray},
    {"triangle", check_geom_triangle},
};

/** 检查函数/关系/度量/几何调用 */
static LvSemanticType check_call(LvSemaContext *ctx, LvAstNode *node) {
    const char *fname = node->data.call.func_name;
    if (!fname)
        return LV_TYPE_ERROR;

    /* 关系调用：collinear, parallel, perpendicular, congruent, tangent */
    if (lv_sema_match_name(fname, lv_geometry_relation_keywords)) {
        /* 参数必须是 Point 类型 */
        for (LvAstNode *a = node->data.call.args; a; a = a->next) {
            LvSemanticType at = check_expr(ctx, a);
            if (at != LV_TYPE_POINT && at != LV_TYPE_ERROR) {
                sema_error(ctx, a->loc, "relation '%s' expects Point arguments", fname);
            }
        }
        return LV_TYPE_PROPOSITION;
    }

    /* 度量调用：length, distance, angle, measure, area, radius */
    if (lv_sema_match_name(fname, lv_measurement_keywords)) {
        for (LvAstNode *a = node->data.call.args; a; a = a->next) {
            LvSemanticType at = check_expr(ctx, a);
            if (at != LV_TYPE_POINT && at != LV_TYPE_ERROR) {
                sema_error(ctx, a->loc, "measure '%s' expects Point arguments", fname);
            }
        }
        return LV_TYPE_SCALAR;
    }

    /* 几何构造：point, line, circle, segment, ray, triangle（查找表） */
    for (size_t i = 0; i < lv_ARRAY_SIZE(kGeomConstructorTable); i++) {
        if (strcmp(fname, kGeomConstructorTable[i].name) == 0) {
            return kGeomConstructorTable[i].handler(ctx, node);
        }
    }

    /* 普通函数调用：未知类型 */
    return LV_TYPE_UNKNOWN;
}

/* ── VTable for check_expr ── */

typedef LvSemanticType (*CheckExprFn)(LvSemaContext *, LvAstNode *);

static LvSemanticType check_expr_ident(LvSemaContext *ctx, LvAstNode *node) {
    const char *name = node->data.ident.name;
    if (!name)
        return LV_TYPE_ERROR;
    LvSymbol *sym = find_symbol(ctx, name);
    if (!sym) {
        sema_error(ctx, node->loc, "undeclared identifier: '%s'", name);
        return LV_TYPE_ERROR;
    }
    return sym->type;
}

static LvSemanticType check_expr_literal_scalar(LvSemaContext *ctx, LvAstNode *node) {
    (void)ctx;
    (void)node;
    return LV_TYPE_SCALAR;
}

static LvSemanticType check_expr_literal_string(LvSemaContext *ctx, LvAstNode *node) {
    (void)ctx;
    (void)node;
    return LV_TYPE_UNKNOWN;
}

static LvSemanticType check_expr_literal_bool(LvSemaContext *ctx, LvAstNode *node) {
    (void)ctx;
    (void)node;
    return LV_TYPE_BOOL;
}

static LvSemanticType check_expr_binary_op(LvSemaContext *ctx, LvAstNode *node) {
    LvSemanticType lt = check_expr(ctx, node->data.binary.left);
    LvSemanticType rt = check_expr(ctx, node->data.binary.right);
    if (lt != LV_TYPE_SCALAR && lt != LV_TYPE_ERROR)
        sema_error(ctx, node->data.binary.left->loc, "binary op expects Scalar operands");
    if (rt != LV_TYPE_SCALAR && rt != LV_TYPE_ERROR)
        sema_error(ctx, node->data.binary.right->loc, "binary op expects Scalar operands");
    return LV_TYPE_SCALAR;
}

static LvSemanticType check_expr_unary_op(LvSemaContext *ctx, LvAstNode *node) {
    return check_expr(ctx, node->data.unary.operand);
}

static LvSemanticType check_expr_compare(LvSemaContext *ctx, LvAstNode *node) {
    LvSemanticType lt = check_expr(ctx, node->data.compare.left);
    LvSemanticType rt = check_expr(ctx, node->data.compare.right);
    (void)lt;
    (void)rt;
    return LV_TYPE_PROPOSITION;
}

static LvSemanticType check_expr_call(LvSemaContext *ctx, LvAstNode *node) {
    return check_call(ctx, node);
}

static LvSemanticType check_expr_logic_and_or(LvSemaContext *ctx, LvAstNode *node) {
    LvSemanticType lt = check_expr(ctx, node->data.binary.left);
    LvSemanticType rt = check_expr(ctx, node->data.binary.right);
    if (lt != LV_TYPE_PROPOSITION && lt != LV_TYPE_ERROR)
        sema_error(ctx, node->data.binary.left->loc, "logic op expects Proposition operands");
    if (rt != LV_TYPE_PROPOSITION && rt != LV_TYPE_ERROR)
        sema_error(ctx, node->data.binary.right->loc, "logic op expects Proposition operands");
    return LV_TYPE_PROPOSITION;
}

static LvSemanticType check_expr_logic_not(LvSemaContext *ctx, LvAstNode *node) {
    LvSemanticType ot = check_expr(ctx, node->data.unary.operand);
    if (ot != LV_TYPE_PROPOSITION && ot != LV_TYPE_ERROR)
        sema_error(ctx, node->data.unary.operand->loc, "not expects Proposition operand");
    return LV_TYPE_PROPOSITION;
}

static LvSemanticType check_expr_logic_implies_iff(LvSemaContext *ctx, LvAstNode *node) {
    LvSemanticType lt = check_expr(ctx, node->data.binary.left);
    LvSemanticType rt = check_expr(ctx, node->data.binary.right);
    if (lt != LV_TYPE_PROPOSITION && lt != LV_TYPE_ERROR)
        sema_error(ctx, node->data.binary.left->loc, "implies/iff expects Proposition operands");
    if (rt != LV_TYPE_PROPOSITION && rt != LV_TYPE_ERROR)
        sema_error(ctx, node->data.binary.right->loc, "implies/iff expects Proposition operands");
    return LV_TYPE_PROPOSITION;
}

static LvSemanticType check_expr_quantifier(LvSemaContext *ctx, LvAstNode *node) {
    const char *vname = node->data.quantifier.var_name;
    LvSemanticType vtype = type_name_to_type(node->data.quantifier.var_type);
    if (vtype == LV_TYPE_UNKNOWN) {
        sema_error(ctx, node->loc, "unknown type '%s' in quantifier",
                   node->data.quantifier.var_type ? node->data.quantifier.var_type : "?");
    }
    if (vname) {
        LvSymbol *existing = find_symbol(ctx, vname);
        if (!existing) {
            add_symbol(ctx, vname, vtype, node->loc);
        }
    }
    LvSemanticType bt = LV_TYPE_ERROR;
    if (node->data.quantifier.body) {
        bt = check_expr(ctx, node->data.quantifier.body);
    }
    if (bt != LV_TYPE_PROPOSITION && bt != LV_TYPE_ERROR)
        sema_error(ctx, node->loc, "quantifier body must be Proposition");
    return LV_TYPE_PROPOSITION;
}

static LvSemanticType check_expr_struct_literal(LvSemaContext *ctx, LvAstNode *node) {
    for (LvAstNode *f = node->child; f; f = f->next) {
        if (f->data.field.value)
            check_expr(ctx, f->data.field.value);
    }
    return LV_TYPE_UNKNOWN;
}

static LvSemanticType check_expr_union(LvSemaContext *ctx, LvAstNode *node) {
    if (node->data.binary.left)
        check_expr(ctx, node->data.binary.left);
    if (node->data.binary.right)
        check_expr(ctx, node->data.binary.right);
    return LV_TYPE_UNKNOWN;
}

static LvSemanticType check_expr_predicate_app(LvSemaContext *ctx, LvAstNode *node) {
    if (node->data.call.args)
        check_expr(ctx, node->data.call.args);
    return LV_TYPE_PROPOSITION;
}

/** 命名参数（LV_AST_NAMED_ARG）：检查其值表达式，返回值的类型 */
static LvSemanticType check_expr_named_arg(LvSemaContext *ctx, LvAstNode *node) {
    if (node->data.field.value)
        return check_expr(ctx, node->data.field.value);
    return LV_TYPE_UNKNOWN;
}

static LvSemanticType check_expr_default(LvSemaContext *ctx, LvAstNode *node) {
    (void)ctx;
    (void)node;
    return LV_TYPE_UNKNOWN;
}

static const CheckExprFn check_expr_table[LV_AST_COUNT] = {
    [LV_AST_IDENTIFIER_EXPR] = check_expr_ident,
    [LV_AST_INTEGER_LITERAL] = check_expr_literal_scalar,
    [LV_AST_RATIONAL_LITERAL] = check_expr_literal_scalar,
    [LV_AST_DECIMAL_LITERAL] = check_expr_literal_scalar,
    [LV_AST_STRING_LITERAL] = check_expr_literal_string,
    [LV_AST_BOOL_LITERAL] = check_expr_literal_bool,
    [LV_AST_BINARY_OP] = check_expr_binary_op,
    [LV_AST_UNARY_OP] = check_expr_unary_op,
    [LV_AST_COMPARE] = check_expr_compare,
    [LV_AST_FUNCTION_CALL] = check_expr_call,
    [LV_AST_RELATION] = check_expr_call,
    [LV_AST_MEASURE] = check_expr_call,
    [LV_AST_GEOMETRY_EXPR] = check_expr_call,
    [LV_AST_LOGIC_AND] = check_expr_logic_and_or,
    [LV_AST_LOGIC_OR] = check_expr_logic_and_or,
    [LV_AST_LOGIC_NOT] = check_expr_logic_not,
    [LV_AST_LOGIC_IMPLIES] = check_expr_logic_implies_iff,
    [LV_AST_LOGIC_IFF] = check_expr_logic_implies_iff,
    [LV_AST_LOGIC_FORALL] = check_expr_quantifier,
    [LV_AST_LOGIC_EXISTS] = check_expr_quantifier,
    [LV_AST_STRUCT_LITERAL] = check_expr_struct_literal,
    [LV_AST_STRUCT_FIELD] = check_expr_struct_literal,
    [LV_AST_NAMED_ARG] = check_expr_named_arg,
    [LV_AST_UNION] = check_expr_union,
    [LV_AST_PREDICATE_APP] = check_expr_predicate_app,
};

/** 递归检查表达式，返回其语义类型 */
static LvSemanticType check_expr(LvSemaContext *ctx, LvAstNode *node) {
    if (!node)
        return LV_TYPE_ERROR;

    LvAstNodeType t = node->type;
    if (t >= 0 && t < LV_AST_COUNT && check_expr_table[t])
        return check_expr_table[t](ctx, node);
    return LV_TYPE_UNKNOWN;
}

/* ── VTable for check_stmt ── */

typedef void (*CheckStmtFn)(LvSemaContext *, LvAstNode *);

static void check_stmt_declaration(LvSemaContext *ctx, LvAstNode *node) {
    check_declaration(ctx, node);
}

static void check_stmt_let(LvSemaContext *ctx, LvAstNode *node) {
    check_let(ctx, node);
}

static void check_stmt_constraint_like(LvSemaContext *ctx, LvAstNode *node) {
    LvSemanticType etype = LV_TYPE_UNKNOWN;
    if (node->data.stmt.expr) {
        etype = check_expr(ctx, node->data.stmt.expr);
    }
    /* Constraint/Prove/Assume/Assert 需要 Proposition 类型 */
    if (node->type != LV_AST_COMPUTE_STMT && etype != LV_TYPE_ERROR && etype != LV_TYPE_PROPOSITION &&
        etype != LV_TYPE_UNKNOWN) {
        sema_error(ctx, node->loc, "expected Proposition expression in statement");
    }
}

static void check_stmt_theorem(LvSemaContext *ctx, LvAstNode *node) {
    if (node->data.theorem.proposition) {
        LvSemanticType pt = check_expr(ctx, node->data.theorem.proposition);
        if (pt != LV_TYPE_PROPOSITION && pt != LV_TYPE_ERROR) {
            sema_error(ctx, node->loc, "theorem proposition must be Proposition type");
        }
    }
    if (node->data.theorem.proof_block) {
        for (LvAstNode *s = node->data.theorem.proof_block->child; s; s = s->next) {
            check_stmt(ctx, s);
        }
    }
}

static void check_stmt_noop(LvSemaContext *ctx, LvAstNode *node) {
    (void)ctx;
    (void)node;
}

static const CheckStmtFn check_stmt_table[LV_AST_COUNT] = {
    [LV_AST_DECLARATION] = check_stmt_declaration,
    [LV_AST_LET] = check_stmt_let,
    [LV_AST_CONSTRAINT_STMT] = check_stmt_constraint_like,
    [LV_AST_ASSUME_STMT] = check_stmt_constraint_like,
    [LV_AST_ASSERT_STMT] = check_stmt_constraint_like,
    [LV_AST_PROVE_STMT] = check_stmt_constraint_like,
    [LV_AST_AXIOM_STMT] = check_stmt_constraint_like,
    [LV_AST_COMPUTE_STMT] = check_stmt_constraint_like,
    [LV_AST_THEOREM_STMT] = check_stmt_theorem,
    [LV_AST_EXPORT_STMT] = check_stmt_noop,
    [LV_AST_NORMALIZE_STMT] = check_stmt_noop,
    [LV_AST_MODULE_DECL] = check_stmt_noop,
    [LV_AST_IMPORT_DECL] = check_stmt_noop,
    [LV_AST_PROOF_BLOCK] = check_stmt_noop,
};

/** 检查语句节点 */
static void check_stmt(LvSemaContext *ctx, LvAstNode *node) {
    if (!node)
        return;

    LvAstNodeType t = node->type;
    LV_DISPATCH_VOID(check_stmt_table, t, ctx, node);
}

/* ── 公共 API ── */

LvSemaContext *lv_sema_create(void) {
    LvSemaContext *ctx = (LvSemaContext *) lv_calloc(1, sizeof(LvSemaContext));
    if (!ctx)
        return NULL;
    ctx->symbols = lv_hashtable_str_create(0);
    if (!ctx->symbols) {
        lv_free((void **) &ctx);
        return NULL;
    }
    return ctx;
}

void lv_sema_destroy(LvSemaContext *ctx) {
    if (!ctx)
        return;
    if (ctx->symbols) {
        lv_hashtable_str_foreach(ctx->symbols, free_symbol_value, NULL);
        lv_hashtable_str_destroy(ctx->symbols);
    }
    lv_free((void **) &ctx);
}

bool lv_sema_analyze(LvSemaContext *ctx, LvAstNode *ast) {
    if (!ctx || !ast)
        return false;

    ctx->error_count = 0;

    /* 重置符号表：释放旧符号并重建空表（str 形态 foreach 中不可删除节点，
     * 故先 foreach 释放值，再销毁重建） */
    lv_hashtable_str_foreach(ctx->symbols, free_symbol_value, NULL);
    lv_hashtable_str_destroy(ctx->symbols);
    ctx->symbols = lv_hashtable_str_create(0);

    if (ast->type == LV_AST_PROGRAM) {
        for (LvAstNode *s = ast->child; s; s = s->next) {
            check_stmt(ctx, s);
        }
    } else {
        check_stmt(ctx, ast);
    }

    return ctx->error_count == 0;
}

int lv_sema_error_count(const LvSemaContext *ctx) {
    if (!ctx)
        return 0;
    return ctx->error_count;
}

const char *lv_sema_error_msg(const LvSemaContext *ctx, int index) {
    if (!ctx || index < 0 || index >= ctx->error_count)
        return NULL;
    return ctx->errors[index];
}
