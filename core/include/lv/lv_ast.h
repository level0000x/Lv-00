#ifndef LV_AST_H
#define LV_AST_H

#include <stddef.h>

#include "lv/lv_lexer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── AST 节点类型 ── */
typedef enum {
    LV_AST_PROGRAM,

    /* 声明 */
    LV_AST_DECLARATION,  // Point A, B, C;
    LV_AST_LET,          // Let x : Type = expr;

    /* 语句 */
    LV_AST_CONSTRAINT_STMT,  // Constraint ...;
    LV_AST_ASSUME_STMT,
    LV_AST_ASSERT_STMT,
    LV_AST_PROVE_STMT,
    LV_AST_COMPUTE_STMT,
    LV_AST_NORMALIZE_STMT,
    LV_AST_EXPORT_STMT,
    LV_AST_AXIOM_STMT,
    LV_AST_THEOREM_STMT,

    /* 表达式 */
    LV_AST_IDENTIFIER_EXPR,
    LV_AST_INTEGER_LITERAL,
    LV_AST_RATIONAL_LITERAL,
    LV_AST_DECIMAL_LITERAL,
    LV_AST_STRING_LITERAL,
    LV_AST_BOOL_LITERAL,

    LV_AST_LOGIC_AND,
    LV_AST_LOGIC_OR,
    LV_AST_LOGIC_NOT,
    LV_AST_LOGIC_IMPLIES,
    LV_AST_LOGIC_IFF,
    LV_AST_LOGIC_FORALL,
    LV_AST_LOGIC_EXISTS,

    LV_AST_BINARY_OP,  // +, -, *, /
    LV_AST_UNARY_OP,   // +, -
    LV_AST_FUNCTION_CALL,

    LV_AST_RELATION,       // collinear(A,B,C)
    LV_AST_MEASURE,        // length(A,B)
    LV_AST_GEOMETRY_EXPR,  // point(1,2), line(A,B)
    LV_AST_COMPARE,        // a == b, a != b, a < b, etc.

    LV_AST_MODULE_DECL,
    LV_AST_IMPORT_DECL,
    LV_AST_PROOF_BLOCK,

    LV_AST_COUNT
} LvAstNodeType;

/* 实体类型（对应 BNF 中 EntityType） */
typedef enum {
    LV_ENTITY_POINT = 0,
    LV_ENTITY_LINE,
    LV_ENTITY_CIRCLE,
    LV_ENTITY_SEGMENT,
    LV_ENTITY_RAY,
    LV_ENTITY_ANGLE,
    LV_ENTITY_TRIANGLE,
    LV_ENTITY_POLYGON,
    LV_ENTITY_SCALAR,
    LV_ENTITY_BOOL,
    LV_ENTITY_PROPOSITION,
    LV_ENTITY_PROOF,
    LV_ENTITY_COUNT
} LvEntityType;

/* ── 前向声明 ── */
typedef struct LvAstNode LvAstNode;

/* ── AST 节点（tagged union） ── */
struct LvAstNode {
    LvAstNodeType type;
    LvSourceLoc loc;
    LvAstNode *next;  /* 兄弟节点链表 */
    LvAstNode *child; /* 第一个子节点 */
    int child_count;

    union {
        /* LV_AST_DECLARATION */
        struct {
            int entity_type; /* LvEntityType */
            char *names;     /* "A,B,C" 逗号分隔 */
        } decl;

        /* LV_AST_LET */
        struct {
            char *name;
            char *type_name;
            LvAstNode *value;
        } let_def;

        /* LV_AST_IDENTIFIER_EXPR */
        struct {
            char *name;
        } ident;

        /* 字面量 */
        struct {
            long long integer_value;
            struct {
                long long num;
                long long den;
            } rational_value;
            double decimal_value;
            char *string_value;
            int bool_value;
        } literal;

        /* 逻辑量词 */
        struct {
            char *var_name;
            char *var_type;
            LvAstNode *body;
        } quantifier;

        /* 函数/关系/度量/几何调用 */
        struct {
            char *func_name;
            LvAstNode *args;
        } call;

        /* 二元运算 */
        struct {
            char op[4]; /* "+", "-", "*", "/", "^" */
            LvAstNode *left;
            LvAstNode *right;
        } binary;

        /* 一元运算 */
        struct {
            char op[4];
            LvAstNode *operand;
        } unary;

        /* 比较运算: ==, !=, <, <=, >, >= */
        struct {
            char op[4];
            LvAstNode *left;
            LvAstNode *right;
        } compare;

        /* 约束/假设/断言/证明（单表达式语句） */
        struct {
            LvAstNode *expr;
        } stmt;

        /* ExportStmt */
        struct {
            char *target;
            char *format;
            char *path;
        } export_stmt;

        /* Module/Import */
        struct {
            char *qualified_name;
        } module_import;

        /* Theorem */
        struct {
            char *name;
            LvAstNode *proposition;
            LvAstNode *proof_block;
        } theorem;

        /* Normalize */
        struct {
            char *target;
        } normalize;
    } data;
};

/* ── API ── */

/** 创建基础 AST 节点 */
LvAstNode *lv_ast_create(LvAstNodeType type, LvSourceLoc loc);

/** 创建标识符表达式节点 */
LvAstNode *lv_ast_create_ident(LvSourceLoc loc, const char *name);

/** 创建整数字面量节点 */
LvAstNode *lv_ast_create_int(LvSourceLoc loc, long long value);

/** 创建有理数字面量节点 */
LvAstNode *lv_ast_create_rational(LvSourceLoc loc, long long num, long long den);

/** 创建小数字面量节点 */
LvAstNode *lv_ast_create_decimal(LvSourceLoc loc, double value);

/** 创建字符串字面量节点 */
LvAstNode *lv_ast_create_string(LvSourceLoc loc, const char *value);

/** 创建布尔字面量节点 */
LvAstNode *lv_ast_create_bool(LvSourceLoc loc, int value);

/** 创建函数/关系调用节点 */
LvAstNode *lv_ast_create_call(LvSourceLoc loc, const char *func_name, LvAstNode *args);

/** 创建带节点类型的函数/关系/度量/几何调用节点 */
LvAstNode *lv_ast_create_call_typed(LvAstNodeType type, LvSourceLoc loc, const char *func_name,
                                    LvAstNode *const *args, int arg_count);

/** 创建二元运算节点 */
LvAstNode *lv_ast_create_binary(LvSourceLoc loc, const char *op, LvAstNode *left, LvAstNode *right);

/** 创建一元运算节点 */
LvAstNode *lv_ast_create_unary(LvSourceLoc loc, const char *op, LvAstNode *operand);

/** 创建比较运算节点 */
LvAstNode *lv_ast_create_compare(LvSourceLoc loc, const char *op, LvAstNode *left, LvAstNode *right);

/** 创建逻辑二元运算节点（iff/implies/or/and） */
LvAstNode *lv_ast_create_logic_binary(LvAstNodeType type, LvSourceLoc loc, const char *op,
                                      LvAstNode *left, LvAstNode *right);

/** 追加子节点到链表末尾 */
void lv_ast_append_child(LvAstNode *parent, LvAstNode *child);

/** 销毁整个 AST 树 */
void lv_ast_destroy(LvAstNode *node);

/** 打印 AST 树（调试用） */
void lv_ast_print(const LvAstNode *node, int indent);

/** 获取实体类型名称 */
const char *lv_entity_type_name(LvEntityType type);

/** 根据关键字 token 类型获取实体类型 */
LvEntityType lv_entity_type_from_token(LvTokenType tok);

#ifdef __cplusplus
}
#endif

#endif /* LV_AST_H */
