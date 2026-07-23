#include "lv/lv_ast.h"
#include "lv_utils.h"
#include <stdio.h>
#include <string.h>

/* ── 创建基础节点 ── */
LvAstNode *lv_ast_create(LvAstNodeType type, LvSourceLoc loc) {
    LvAstNode *node = (LvAstNode *)lv_calloc(1, sizeof(LvAstNode));
    if (!node) return NULL;
    node->type = type;
    node->loc  = loc;
    return node;
}

LvAstNode *lv_ast_create_ident(LvSourceLoc loc, const char *name) {
    LvAstNode *node = lv_ast_create(LV_AST_IDENTIFIER_EXPR, loc);
    if (!node) return NULL;
    node->data.ident.name = lv_strdup(name);
    return node;
}

LvAstNode *lv_ast_create_int(LvSourceLoc loc, long long value) {
    LvAstNode *node = lv_ast_create(LV_AST_INTEGER_LITERAL, loc);
    if (!node) return NULL;
    node->data.literal.integer_value = value;
    return node;
}

LvAstNode *lv_ast_create_rational(LvSourceLoc loc, long long num, long long den) {
    LvAstNode *node = lv_ast_create(LV_AST_RATIONAL_LITERAL, loc);
    if (!node) return NULL;
    node->data.literal.rational_value.num = num;
    node->data.literal.rational_value.den = den;
    return node;
}

LvAstNode *lv_ast_create_decimal(LvSourceLoc loc, double value) {
    LvAstNode *node = lv_ast_create(LV_AST_DECIMAL_LITERAL, loc);
    if (!node) return NULL;
    node->data.literal.decimal_value = value;
    return node;
}

LvAstNode *lv_ast_create_string(LvSourceLoc loc, const char *value) {
    LvAstNode *node = lv_ast_create(LV_AST_STRING_LITERAL, loc);
    if (!node) return NULL;
    node->data.literal.string_value = lv_strdup(value);
    return node;
}

LvAstNode *lv_ast_create_bool(LvSourceLoc loc, int value) {
    LvAstNode *node = lv_ast_create(LV_AST_BOOL_LITERAL, loc);
    if (!node) return NULL;
    node->data.literal.bool_value = value;
    return node;
}

LvAstNode *lv_ast_create_call(LvSourceLoc loc, const char *func_name, LvAstNode *args) {
    LvAstNode *node = lv_ast_create(LV_AST_FUNCTION_CALL, loc);
    if (!node) return NULL;
    node->data.call.func_name = lv_strdup(func_name);
    node->data.call.args = args;
    if (args) {
        node->child = args;
        /* 计数子节点 */
        int count = 0;
        for (LvAstNode *c = args; c; c = c->next) count++;
        node->child_count = count;
    }
    return node;
}

LvAstNode *lv_ast_create_binary(LvSourceLoc loc, const char *op, LvAstNode *left, LvAstNode *right) {
    LvAstNode *node = lv_ast_create(LV_AST_BINARY_OP, loc);
    if (!node) return NULL;
    lv_strncpy(node->data.binary.op, op, sizeof(node->data.binary.op));
    node->data.binary.left  = left;
    node->data.binary.right = right;
    return node;
}

LvAstNode *lv_ast_create_unary(LvSourceLoc loc, const char *op, LvAstNode *operand) {
    LvAstNode *node = lv_ast_create(LV_AST_UNARY_OP, loc);
    if (!node) return NULL;
    lv_strncpy(node->data.unary.op, op, sizeof(node->data.unary.op));
    node->data.unary.operand = operand;
    return node;
}

LvAstNode *lv_ast_create_compare(LvSourceLoc loc, const char *op, LvAstNode *left, LvAstNode *right) {
    LvAstNode *node = lv_ast_create(LV_AST_COMPARE, loc);
    if (!node) return NULL;
    lv_strncpy(node->data.compare.op, op, sizeof(node->data.compare.op));
    node->data.compare.left  = left;
    node->data.compare.right = right;
    return node;
}

/* ── 追加子节点 ── */
void lv_ast_append_child(LvAstNode *parent, LvAstNode *child) {
    if (!parent || !child) return;
    if (!parent->child) {
        parent->child = child;
    } else {
        LvAstNode *last = parent->child;
        while (last->next) last = last->next;
        last->next = child;
    }
    parent->child_count++;
}

/* ── 销毁 AST 树 ── */
void lv_ast_destroy(LvAstNode *node) {
    if (!node) return;

    /* 先递归销毁兄弟和子节点 */
    if (node->next) {
        lv_ast_destroy(node->next);
        node->next = NULL;
    }
    if (node->child) {
        lv_ast_destroy(node->child);
        node->child = NULL;
    }

    /* 释放 union 中动态分配的内存 */
    switch (node->type) {
    case LV_AST_DECLARATION:
        lv_free((void **)&node->data.decl.names);
        break;
    case LV_AST_LET:
        lv_free((void **)&node->data.let_def.name);
        lv_free((void **)&node->data.let_def.type_name);
        break;
    case LV_AST_IDENTIFIER_EXPR:
        lv_free((void **)&node->data.ident.name);
        break;
    case LV_AST_STRING_LITERAL:
        lv_free((void **)&node->data.literal.string_value);
        break;
    case LV_AST_LOGIC_FORALL:
    case LV_AST_LOGIC_EXISTS:
        lv_free((void **)&node->data.quantifier.var_name);
        lv_free((void **)&node->data.quantifier.var_type);
        break;
    case LV_AST_FUNCTION_CALL:
    case LV_AST_RELATION:
    case LV_AST_MEASURE:
    case LV_AST_GEOMETRY_EXPR:
        lv_free((void **)&node->data.call.func_name);
        break;
    case LV_AST_EXPORT_STMT:
        lv_free((void **)&node->data.export_stmt.target);
        lv_free((void **)&node->data.export_stmt.format);
        lv_free((void **)&node->data.export_stmt.path);
        break;
    case LV_AST_MODULE_DECL:
    case LV_AST_IMPORT_DECL:
        lv_free((void **)&node->data.module_import.qualified_name);
        break;
    case LV_AST_THEOREM_STMT:
        lv_free((void **)&node->data.theorem.name);
        break;
    case LV_AST_NORMALIZE_STMT:
        lv_free((void **)&node->data.normalize.target);
        break;
    default:
        break;
    }

    lv_free((void **)&node);
}

/* ── 打印 AST 树 ── */
static void print_indent(int indent) {
    for (int i = 0; i < indent; i++) fputs("  ", stdout);
}

static const char *ast_type_name(LvAstNodeType type) {
    static const char *names[] = {
        "PROGRAM",
        "DECLARATION", "LET",
        "CONSTRAINT_STMT", "ASSUME_STMT", "ASSERT_STMT", "PROVE_STMT",
        "COMPUTE_STMT", "NORMALIZE_STMT", "EXPORT_STMT", "AXIOM_STMT",
        "THEOREM_STMT",
        "IDENTIFIER_EXPR", "INTEGER_LITERAL", "RATIONAL_LITERAL",
        "DECIMAL_LITERAL", "STRING_LITERAL", "BOOL_LITERAL",
        "LOGIC_AND", "LOGIC_OR", "LOGIC_NOT", "LOGIC_IMPLIES", "LOGIC_IFF",
        "LOGIC_FORALL", "LOGIC_EXISTS",
        "BINARY_OP", "UNARY_OP", "FUNCTION_CALL",
        "RELATION", "MEASURE", "GEOMETRY_EXPR", "COMPARE",
        "MODULE_DECL", "IMPORT_DECL", "PROOF_BLOCK"
    };
    if (type >= 0 && type < LV_AST_COUNT)
        return names[type];
    return "UNKNOWN";
}

void lv_ast_print(const LvAstNode *node, int indent) {
    if (!node) return;
    print_indent(indent);
    printf("%s", ast_type_name(node->type));

    switch (node->type) {
    case LV_AST_DECLARATION:
        printf(" [entity=%s, names=%s]",
               lv_entity_type_name((LvEntityType)node->data.decl.entity_type),
               node->data.decl.names ? node->data.decl.names : "");
        break;
    case LV_AST_LET:
        printf(" [name=%s, type=%s]",
               node->data.let_def.name ? node->data.let_def.name : "",
               node->data.let_def.type_name ? node->data.let_def.type_name : "");
        break;
    case LV_AST_IDENTIFIER_EXPR:
        printf(" [%s]", node->data.ident.name ? node->data.ident.name : "");
        break;
    case LV_AST_INTEGER_LITERAL:
        printf(" [%lld]", node->data.literal.integer_value);
        break;
    case LV_AST_RATIONAL_LITERAL:
        printf(" [%lld/%lld]",
               node->data.literal.rational_value.num,
               node->data.literal.rational_value.den);
        break;
    case LV_AST_DECIMAL_LITERAL:
        printf(" [%g]", node->data.literal.decimal_value);
        break;
    case LV_AST_STRING_LITERAL:
        printf(" [\"%s\"]", node->data.literal.string_value ? node->data.literal.string_value : "");
        break;
    case LV_AST_BOOL_LITERAL:
        printf(" [%s]", node->data.literal.bool_value ? "true" : "false");
        break;
    case LV_AST_BINARY_OP:
        printf(" [%s]", node->data.binary.op);
        break;
    case LV_AST_UNARY_OP:
        printf(" [%s]", node->data.unary.op);
        break;
    case LV_AST_COMPARE:
        printf(" [%s]", node->data.compare.op);
        break;
    case LV_AST_FUNCTION_CALL:
    case LV_AST_RELATION:
    case LV_AST_MEASURE:
    case LV_AST_GEOMETRY_EXPR:
        printf(" [%s]", node->data.call.func_name ? node->data.call.func_name : "");
        break;
    case LV_AST_LOGIC_FORALL:
    case LV_AST_LOGIC_EXISTS:
        printf(" [%s: %s]",
               node->data.quantifier.var_name ? node->data.quantifier.var_name : "",
               node->data.quantifier.var_type ? node->data.quantifier.var_type : "");
        break;
    case LV_AST_EXPORT_STMT:
        printf(" [target=%s, format=%s]",
               node->data.export_stmt.target ? node->data.export_stmt.target : "",
               node->data.export_stmt.format ? node->data.export_stmt.format : "");
        break;
    case LV_AST_MODULE_DECL:
    case LV_AST_IMPORT_DECL:
        printf(" [%s]", node->data.module_import.qualified_name ? node->data.module_import.qualified_name : "");
        break;
    case LV_AST_THEOREM_STMT:
        printf(" [%s]", node->data.theorem.name ? node->data.theorem.name : "");
        break;
    case LV_AST_NORMALIZE_STMT:
        printf(" [%s]", node->data.normalize.target ? node->data.normalize.target : "");
        break;
    default:
        break;
    }

    putchar('\n');

    /* 递归打印子节点 */
    if (node->child) {
        lv_ast_print(node->child, indent + 1);
    }

    /* 根据节点类型打印特殊的子节点 */
    switch (node->type) {
    case LV_AST_BINARY_OP:
        if (node->data.binary.left)  lv_ast_print(node->data.binary.left,  indent + 1);
        if (node->data.binary.right) lv_ast_print(node->data.binary.right, indent + 1);
        break;
    case LV_AST_UNARY_OP:
        if (node->data.unary.operand) lv_ast_print(node->data.unary.operand, indent + 1);
        break;
    case LV_AST_COMPARE:
        if (node->data.compare.left)  lv_ast_print(node->data.compare.left,  indent + 1);
        if (node->data.compare.right) lv_ast_print(node->data.compare.right, indent + 1);
        break;
    case LV_AST_LET:
        if (node->data.let_def.value) lv_ast_print(node->data.let_def.value, indent + 1);
        break;
    case LV_AST_CONSTRAINT_STMT:
    case LV_AST_ASSUME_STMT:
    case LV_AST_ASSERT_STMT:
    case LV_AST_PROVE_STMT:
    case LV_AST_COMPUTE_STMT:
    case LV_AST_AXIOM_STMT:
        if (node->data.stmt.expr) lv_ast_print(node->data.stmt.expr, indent + 1);
        break;
    case LV_AST_THEOREM_STMT:
        if (node->data.theorem.proposition) lv_ast_print(node->data.theorem.proposition, indent + 1);
        if (node->data.theorem.proof_block) lv_ast_print(node->data.theorem.proof_block, indent + 1);
        break;
    case LV_AST_FUNCTION_CALL:
    case LV_AST_RELATION:
    case LV_AST_MEASURE:
    case LV_AST_GEOMETRY_EXPR:
        if (node->data.call.args) lv_ast_print(node->data.call.args, indent + 1);
        break;
    case LV_AST_LOGIC_FORALL:
    case LV_AST_LOGIC_EXISTS:
        if (node->data.quantifier.body) lv_ast_print(node->data.quantifier.body, indent + 1);
        break;
    default:
        break;
    }

    /* 打印兄弟节点 */
    if (node->next) {
        lv_ast_print(node->next, indent);
    }
}

/* ── 实体类型名称 ── */
const char *lv_entity_type_name(LvEntityType type) {
    static const char *names[] = {
        "Point", "Line", "Circle", "Segment", "Ray", "Angle",
        "Triangle", "Polygon", "Scalar", "Bool", "Proposition", "Proof"
    };
    if (type >= 0 && type < LV_ENTITY_COUNT)
        return names[type];
    return "Unknown";
}

/* ── 根据关键字 token 获取实体类型 ── */
LvEntityType lv_entity_type_from_token(LvTokenType tok) {
    switch (tok) {
    case LV_TOKEN_KW_POINT:        return LV_ENTITY_POINT;
    case LV_TOKEN_KW_LINE:         return LV_ENTITY_LINE;
    case LV_TOKEN_KW_CIRCLE:       return LV_ENTITY_CIRCLE;
    case LV_TOKEN_KW_SEGMENT:      return LV_ENTITY_SEGMENT;
    case LV_TOKEN_KW_RAY:          return LV_ENTITY_RAY;
    case LV_TOKEN_KW_ANGLE:        return LV_ENTITY_ANGLE;
    case LV_TOKEN_KW_TRIANGLE:     return LV_ENTITY_TRIANGLE;
    case LV_TOKEN_KW_POLYGON:      return LV_ENTITY_POLYGON;
    case LV_TOKEN_KW_SCALAR:       return LV_ENTITY_SCALAR;
    case LV_TOKEN_KW_BOOL:         return LV_ENTITY_BOOL;
    case LV_TOKEN_KW_PROPOSITION:  return LV_ENTITY_PROPOSITION;
    case LV_TOKEN_KW_PROOF:        return LV_ENTITY_PROOF;
    default:                       return LV_ENTITY_COUNT; /* 非法值 */
    }
}
