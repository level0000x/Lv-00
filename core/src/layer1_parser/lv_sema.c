#include "lv/lv_sema.h"
#include "lv_utils.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* ── 符号表 ── */
#define LV_SEMA_MAX_SYMBOLS 256
#define LV_SEMA_MAX_ERRORS 64

typedef struct {
    char          name[64];
    LvSemanticType type;
    LvSourceLoc    loc;
} LvSymbol;

struct LvSemaContext {
    LvSymbol symbols[LV_SEMA_MAX_SYMBOLS];
    int      symbol_count;
    char     errors[LV_SEMA_MAX_ERRORS][256];
    int      error_count;
};

/* ── 内部辅助 ── */

static void sema_error(LvSemaContext *ctx, LvSourceLoc loc, const char *fmt, ...) {
    if (ctx->error_count >= LV_SEMA_MAX_ERRORS) return;
    char *buf = ctx->errors[ctx->error_count];
    int n = lv_snprintf(buf, 256, "(%d,%d) ", loc.line, loc.column);
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf + n, (size_t)(256 - n), fmt, args);
    va_end(args);
    ctx->error_count++;
}

/* 跨平台 strtok（避免 MSVC 缺少 strtok_r）*/
static char *sema_strtok(char *str, const char *delim, char **save) {
#ifdef _MSC_VER
    return strtok_s(str, delim, save);
#else
    return strtok_r(str, delim, save);
#endif
}

/** 将 LvEntityType 映射为 LvSemanticType */
static LvSemanticType entity_to_semantic_type(LvEntityType etype) {
    switch (etype) {
    case LV_ENTITY_POINT:       return LV_TYPE_POINT;
    case LV_ENTITY_LINE:        return LV_TYPE_LINE;
    case LV_ENTITY_CIRCLE:      return LV_TYPE_CIRCLE;
    case LV_ENTITY_SEGMENT:     return LV_TYPE_SEGMENT;
    case LV_ENTITY_RAY:         return LV_TYPE_RAY;
    case LV_ENTITY_ANGLE:       return LV_TYPE_ANGLE;
    case LV_ENTITY_TRIANGLE:    return LV_TYPE_TRIANGLE;
    case LV_ENTITY_POLYGON:     return LV_TYPE_POLYGON;
    case LV_ENTITY_SCALAR:      return LV_TYPE_SCALAR;
    case LV_ENTITY_BOOL:        return LV_TYPE_BOOL;
    case LV_ENTITY_PROPOSITION: return LV_TYPE_PROPOSITION;
    case LV_ENTITY_PROOF:       return LV_TYPE_PROOF;
    default:                    return LV_TYPE_UNKNOWN;
    }
}

/** 根据字符串名称查找语义类型 */
static LvSemanticType type_name_to_type(const char *name) {
    if (!name) return LV_TYPE_UNKNOWN;
    if (strcmp(name, "Point") == 0)       return LV_TYPE_POINT;
    if (strcmp(name, "Line") == 0)        return LV_TYPE_LINE;
    if (strcmp(name, "Circle") == 0)      return LV_TYPE_CIRCLE;
    if (strcmp(name, "Segment") == 0)     return LV_TYPE_SEGMENT;
    if (strcmp(name, "Ray") == 0)         return LV_TYPE_RAY;
    if (strcmp(name, "Angle") == 0)       return LV_TYPE_ANGLE;
    if (strcmp(name, "Triangle") == 0)    return LV_TYPE_TRIANGLE;
    if (strcmp(name, "Polygon") == 0)     return LV_TYPE_POLYGON;
    if (strcmp(name, "Scalar") == 0)      return LV_TYPE_SCALAR;
    if (strcmp(name, "Bool") == 0)        return LV_TYPE_BOOL;
    if (strcmp(name, "Proposition") == 0) return LV_TYPE_PROPOSITION;
    if (strcmp(name, "Proof") == 0)       return LV_TYPE_PROOF;
    return LV_TYPE_UNKNOWN;
}

/** 在符号表中查找标识符 */
static LvSymbol *find_symbol(LvSemaContext *ctx, const char *name) {
    for (int i = 0; i < ctx->symbol_count; i++) {
        if (strcmp(ctx->symbols[i].name, name) == 0)
            return &ctx->symbols[i];
    }
    return NULL;
}

/** 向符号表添加标识符 */
static bool add_symbol(LvSemaContext *ctx, const char *name, LvSemanticType type, LvSourceLoc loc) {
    if (ctx->symbol_count >= LV_SEMA_MAX_SYMBOLS) {
        sema_error(ctx, loc, "symbol table full");
        return false;
    }
    if (find_symbol(ctx, name)) {
        sema_error(ctx, loc, "duplicate declaration: '%s'", name);
        return false;
    }
    LvSymbol *sym = &ctx->symbols[ctx->symbol_count++];
    lv_strncpy(sym->name, name, sizeof(sym->name));
    sym->type = type;
    sym->loc  = loc;
    return true;
}

/* ── 类型检查函数 ── */

/** 前向声明 */
static LvSemanticType check_expr(LvSemaContext *ctx, LvAstNode *node);

/** 处理 Declaration 节点：将声明的标识符加入符号表 */
static void check_declaration(LvSemaContext *ctx, LvAstNode *node) {
    LvSemanticType stype = entity_to_semantic_type((LvEntityType)node->data.decl.entity_type);
    const char *names = node->data.decl.names;
    if (!names) return;

    /* 复制 names 用于拆分，避免修改原始字符串 */
    char buf[1024];
    lv_strncpy(buf, names, sizeof(buf));

    char *save;
    char *tok = sema_strtok(buf, ",", &save);
    while (tok) {
        add_symbol(ctx, tok, stype, node->loc);
        tok = sema_strtok(NULL, ",", &save);
    }
}

/** 处理 Let 节点 */
static void check_let(LvSemaContext *ctx, LvAstNode *node) {
    LvSemanticType decl_type = type_name_to_type(node->data.let_def.type_name);
    LvSemanticType val_type  = LV_TYPE_UNKNOWN;

    if (node->data.let_def.value) {
        val_type = check_expr(ctx, node->data.let_def.value);
    }

    /* 检查值类型是否匹配声明类型 */
    if (decl_type != LV_TYPE_UNKNOWN && val_type != LV_TYPE_UNKNOWN &&
        val_type != LV_TYPE_ERROR && decl_type != val_type) {
        sema_error(ctx, node->loc, "type mismatch in Let: declared '%s', value has type '%s'",
                   node->data.let_def.type_name, "?");
    }

    if (node->data.let_def.name) {
        add_symbol(ctx, node->data.let_def.name,
                   decl_type != LV_TYPE_UNKNOWN ? decl_type : val_type,
                   node->loc);
    }
}

/** 检查函数/关系/度量/几何调用 */
static LvSemanticType check_call(LvSemaContext *ctx, LvAstNode *node) {
    const char *fname = node->data.call.func_name;
    if (!fname) return LV_TYPE_ERROR;

    /* 关系调用：collinear, parallel, perpendicular, congruent, tangent */
    if (strcmp(fname, "collinear") == 0 ||
        strcmp(fname, "parallel") == 0 ||
        strcmp(fname, "perpendicular") == 0 ||
        strcmp(fname, "congruent") == 0 ||
        strcmp(fname, "tangent") == 0) {
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
    if (strcmp(fname, "length") == 0 ||
        strcmp(fname, "distance") == 0 ||
        strcmp(fname, "angle") == 0 ||
        strcmp(fname, "measure") == 0 ||
        strcmp(fname, "area") == 0 ||
        strcmp(fname, "radius") == 0) {
        for (LvAstNode *a = node->data.call.args; a; a = a->next) {
            LvSemanticType at = check_expr(ctx, a);
            if (at != LV_TYPE_POINT && at != LV_TYPE_ERROR) {
                sema_error(ctx, a->loc, "measure '%s' expects Point arguments", fname);
            }
        }
        return LV_TYPE_SCALAR;
    }

    /* 几何构造：point, line, circle, segment, ray, triangle */
    if (strcmp(fname, "point") == 0) {
        for (LvAstNode *a = node->data.call.args; a; a = a->next) {
            check_expr(ctx, a);
        }
        return LV_TYPE_POINT;
    }
    if (strcmp(fname, "line") == 0) {
        for (LvAstNode *a = node->data.call.args; a; a = a->next) {
            LvSemanticType at = check_expr(ctx, a);
            if (at != LV_TYPE_POINT && at != LV_TYPE_ERROR) {
                sema_error(ctx, a->loc, "line() expects Point arguments");
            }
        }
        return LV_TYPE_LINE;
    }
    if (strcmp(fname, "segment") == 0) {
        for (LvAstNode *a = node->data.call.args; a; a = a->next) {
            LvSemanticType at = check_expr(ctx, a);
            if (at != LV_TYPE_POINT && at != LV_TYPE_ERROR) {
                sema_error(ctx, a->loc, "segment() expects Point arguments");
            }
        }
        return LV_TYPE_SEGMENT;
    }
    if (strcmp(fname, "circle") == 0) {
        LvAstNode *args = node->data.call.args;
        if (args) {
            check_expr(ctx, args);
            if (args->next) check_expr(ctx, args->next);
        }
        return LV_TYPE_CIRCLE;
    }
    if (strcmp(fname, "ray") == 0) {
        for (LvAstNode *a = node->data.call.args; a; a = a->next) {
            LvSemanticType at = check_expr(ctx, a);
            if (at != LV_TYPE_POINT && at != LV_TYPE_ERROR) {
                sema_error(ctx, a->loc, "ray() expects Point arguments");
            }
        }
        return LV_TYPE_RAY;
    }
    if (strcmp(fname, "triangle") == 0) {
        for (LvAstNode *a = node->data.call.args; a; a = a->next) {
            LvSemanticType at = check_expr(ctx, a);
            if (at != LV_TYPE_POINT && at != LV_TYPE_ERROR) {
                sema_error(ctx, a->loc, "triangle() expects Point arguments");
            }
        }
        return LV_TYPE_TRIANGLE;
    }

    /* 普通函数调用：未知类型 */
    return LV_TYPE_UNKNOWN;
}

/** 递归检查表达式，返回其语义类型 */
static LvSemanticType check_expr(LvSemaContext *ctx, LvAstNode *node) {
    if (!node) return LV_TYPE_ERROR;

    switch (node->type) {
    case LV_AST_IDENTIFIER_EXPR: {
        const char *name = node->data.ident.name;
        if (!name) return LV_TYPE_ERROR;
        LvSymbol *sym = find_symbol(ctx, name);
        if (!sym) {
            sema_error(ctx, node->loc, "undeclared identifier: '%s'", name);
            return LV_TYPE_ERROR;
        }
        return sym->type;
    }

    case LV_AST_INTEGER_LITERAL:
    case LV_AST_RATIONAL_LITERAL:
    case LV_AST_DECIMAL_LITERAL:
        return LV_TYPE_SCALAR;

    case LV_AST_STRING_LITERAL:
        return LV_TYPE_UNKNOWN;

    case LV_AST_BOOL_LITERAL:
        return LV_TYPE_BOOL;

    case LV_AST_BINARY_OP: {
        LvSemanticType lt = check_expr(ctx, node->data.binary.left);
        LvSemanticType rt = check_expr(ctx, node->data.binary.right);
        if (lt != LV_TYPE_SCALAR && lt != LV_TYPE_ERROR)
            sema_error(ctx, node->data.binary.left->loc, "binary op expects Scalar operands");
        if (rt != LV_TYPE_SCALAR && rt != LV_TYPE_ERROR)
            sema_error(ctx, node->data.binary.right->loc, "binary op expects Scalar operands");
        return LV_TYPE_SCALAR;
    }

    case LV_AST_UNARY_OP:
        return check_expr(ctx, node->data.unary.operand);

    case LV_AST_COMPARE: {
        LvSemanticType lt = check_expr(ctx, node->data.compare.left);
        LvSemanticType rt = check_expr(ctx, node->data.compare.right);
        (void)lt; (void)rt;
        /* 比较返回 Proposition */
        return LV_TYPE_PROPOSITION;
    }

    case LV_AST_FUNCTION_CALL:
    case LV_AST_RELATION:
    case LV_AST_MEASURE:
    case LV_AST_GEOMETRY_EXPR:
        return check_call(ctx, node);

    case LV_AST_LOGIC_AND:
    case LV_AST_LOGIC_OR: {
        LvSemanticType lt = check_expr(ctx, node->data.binary.left);
        LvSemanticType rt = check_expr(ctx, node->data.binary.right);
        if (lt != LV_TYPE_PROPOSITION && lt != LV_TYPE_ERROR)
            sema_error(ctx, node->data.binary.left->loc, "logic op expects Proposition operands");
        if (rt != LV_TYPE_PROPOSITION && rt != LV_TYPE_ERROR)
            sema_error(ctx, node->data.binary.right->loc, "logic op expects Proposition operands");
        return LV_TYPE_PROPOSITION;
    }

    case LV_AST_LOGIC_NOT: {
        LvSemanticType ot = check_expr(ctx, node->data.unary.operand);
        if (ot != LV_TYPE_PROPOSITION && ot != LV_TYPE_ERROR)
            sema_error(ctx, node->data.unary.operand->loc, "not expects Proposition operand");
        return LV_TYPE_PROPOSITION;
    }

    case LV_AST_LOGIC_IMPLIES:
    case LV_AST_LOGIC_IFF: {
        LvSemanticType lt = check_expr(ctx, node->data.binary.left);
        LvSemanticType rt = check_expr(ctx, node->data.binary.right);
        if (lt != LV_TYPE_PROPOSITION && lt != LV_TYPE_ERROR)
            sema_error(ctx, node->data.binary.left->loc, "implies/iff expects Proposition operands");
        if (rt != LV_TYPE_PROPOSITION && rt != LV_TYPE_ERROR)
            sema_error(ctx, node->data.binary.right->loc, "implies/iff expects Proposition operands");
        return LV_TYPE_PROPOSITION;
    }

    case LV_AST_LOGIC_FORALL:
    case LV_AST_LOGIC_EXISTS: {
        /* 将绑定变量加入符号表 */
        const char *vname = node->data.quantifier.var_name;
        LvSemanticType vtype = type_name_to_type(node->data.quantifier.var_type);
        if (vtype == LV_TYPE_UNKNOWN) {
            sema_error(ctx, node->loc, "unknown type '%s' in quantifier",
                       node->data.quantifier.var_type ? node->data.quantifier.var_type : "?");
        }
        /* 暂时先添加变量 */
        if (vname) {
            /* 检查是否已存在，先不报错因为可能是同名不同作用域 */
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

    default:
        return LV_TYPE_UNKNOWN;
    }
}

/** 检查语句节点 */
static void check_stmt(LvSemaContext *ctx, LvAstNode *node) {
    if (!node) return;

    switch (node->type) {
    case LV_AST_DECLARATION:
        check_declaration(ctx, node);
        break;

    case LV_AST_LET:
        check_let(ctx, node);
        break;

    case LV_AST_CONSTRAINT_STMT:
    case LV_AST_ASSUME_STMT:
    case LV_AST_ASSERT_STMT:
    case LV_AST_PROVE_STMT:
    case LV_AST_AXIOM_STMT:
    case LV_AST_COMPUTE_STMT: {
        LvSemanticType etype = LV_TYPE_UNKNOWN;
        if (node->data.stmt.expr) {
            etype = check_expr(ctx, node->data.stmt.expr);
        }
        /* Constraint/Prove/Assume/Assert 需要 Proposition 类型 */
        if (node->type != LV_AST_COMPUTE_STMT && etype != LV_TYPE_ERROR && etype != LV_TYPE_PROPOSITION && etype != LV_TYPE_UNKNOWN) {
            sema_error(ctx, node->loc, "expected Proposition expression in statement");
        }
        break;
    }

    case LV_AST_THEOREM_STMT: {
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
        break;
    }

    case LV_AST_EXPORT_STMT:
    case LV_AST_NORMALIZE_STMT:
    case LV_AST_MODULE_DECL:
    case LV_AST_IMPORT_DECL:
    case LV_AST_PROOF_BLOCK:
        /* 这些语句不需要类型检查 */
        break;

    default:
        break;
    }
}

/* ── 公共 API ── */

LvSemaContext *lv_sema_create(void) {
    LvSemaContext *ctx = (LvSemaContext *)lv_calloc(1, sizeof(LvSemaContext));
    return ctx;
}

void lv_sema_destroy(LvSemaContext *ctx) {
    if (!ctx) return;
    lv_free((void **)&ctx);
}

bool lv_sema_analyze(LvSemaContext *ctx, LvAstNode *ast) {
    if (!ctx || !ast) return false;

    ctx->error_count = 0;
    ctx->symbol_count = 0;

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
    if (!ctx) return 0;
    return ctx->error_count;
}

const char *lv_sema_error_msg(const LvSemaContext *ctx, int index) {
    if (!ctx || index < 0 || index >= ctx->error_count) return NULL;
    return ctx->errors[index];
}
