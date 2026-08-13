/**
 * @file lv_ast.c
 * @brief AST 节点创建、销毁与打印实现
 *
 * @details 实现 Lv-00 抽象语法树（AST）的节点工厂函数、递归销毁和可视化打印。
 *          支持 28 种节点类型，覆盖声明、表达式、字面量、逻辑运算符、
 *          量化表达式、语句和模块声明等。
 *
 *          节点使用 lv_malloc/lv_calloc 统一分配，销毁时自动释放所有
 *          动态分配的内部数据（字符串、子节点等）。
 *
 * @author Lv-00 Project
 */

#include "lv/lv_ast.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "lv/lv_log.h"
#include "lv/lv_strbuf.h"
#include "lv_internal.h"
#include "lv_utils.h"

/* ── 创建基础节点 ── */

/**
 * @brief 创建 AST 基础节点
 *
 * 分配并初始化一个 AST 节点，设置类型和源代码位置信息。
 * 使用 lv_calloc 确保所有字段初始为零。
 *
 * @param type 节点类型
 * @param loc  源代码位置
 * @return 节点指针，分配失败返回 NULL
 */
LvAstNode *lv_ast_create(LvAstNodeType type, LvSourceLoc loc) {
    LvAstNode *node = (LvAstNode *) lv_calloc(1, sizeof(LvAstNode));
    if (!node)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "failed to create AST node");
    node->type = type;
    node->loc = loc;
    return node;
}

static LvAstNode *ast_alloc(LvAstNodeType type, LvSourceLoc loc, const char *err_msg) {
    LvAstNode *node = lv_ast_create(type, loc);
    if (!node)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, err_msg);
    return node;
}

/**
 * @brief 创建标识符表达式节点
 *
 * @param loc  源代码位置
 * @param name 标识符名称（会被复制到节点内部存储）
 * @return 节点指针，失败返回 NULL
 */
LvAstNode *lv_ast_create_ident(LvSourceLoc loc, const char *name) {
    LvAstNode *node = ast_alloc(LV_AST_IDENTIFIER_EXPR, loc, "failed to create ident node");
    node->data.ident.name = lv_strdup(name);
    return node;
}

/**
 * @brief 创建整数字面量节点
 *
 * @param loc   源代码位置
 * @param value 整数值
 * @return 节点指针，失败返回 NULL
 */
LvAstNode *lv_ast_create_int(LvSourceLoc loc, long long value) {
    LvAstNode *node = ast_alloc(LV_AST_INTEGER_LITERAL, loc, "failed to create int node");
    node->data.literal.integer_value = value;
    return node;
}

/**
 * @brief 创建有理数字面量节点
 *
 * @param loc 源代码位置
 * @param num 分子
 * @param den 分母
 * @return 节点指针，失败返回 NULL
 */
LvAstNode *lv_ast_create_rational(LvSourceLoc loc, long long num, long long den) {
    LvAstNode *node = ast_alloc(LV_AST_RATIONAL_LITERAL, loc, "failed to create rational node");
    node->data.literal.rational_value.num = num;
    node->data.literal.rational_value.den = den;
    return node;
}

/**
 * @brief 创建小数字面量节点
 *
 * @param loc   源代码位置
 * @param value 浮点数值
 * @return 节点指针，失败返回 NULL
 */
LvAstNode *lv_ast_create_decimal(LvSourceLoc loc, double value) {
    LvAstNode *node = ast_alloc(LV_AST_DECIMAL_LITERAL, loc, "failed to create decimal node");
    node->data.literal.decimal_value = value;
    return node;
}

/**
 * @brief 创建字符串字面量节点
 *
 * @param loc   源代码位置
 * @param value 字符串值（会被复制到节点内部存储）
 * @return 节点指针，失败返回 NULL
 */
LvAstNode *lv_ast_create_string(LvSourceLoc loc, const char *value) {
    LvAstNode *node = ast_alloc(LV_AST_STRING_LITERAL, loc, "failed to create string node");
    node->data.literal.string_value = lv_strdup(value);
    return node;
}

/**
 * @brief 创建布尔字面量节点
 *
 * @param loc   源代码位置
 * @param value 布尔值（0=假, 非0=真）
 * @return 节点指针，失败返回 NULL
 */
LvAstNode *lv_ast_create_bool(LvSourceLoc loc, int value) {
    LvAstNode *node = ast_alloc(LV_AST_BOOL_LITERAL, loc, "failed to create bool node");
    node->data.literal.bool_value = value;
    return node;
}

/**
 * @brief 创建函数调用表达式节点
 *
 * @param loc       源代码位置
 * @param func_name 函数名称（会被复制到节点内部存储）
 * @param args      参数链表头节点
 * @return 节点指针，失败返回 NULL
 */
LvAstNode *lv_ast_create_call(LvSourceLoc loc, const char *func_name, LvAstNode *args) {
    LvAstNode *node = ast_alloc(LV_AST_FUNCTION_CALL, loc, "failed to create call node");
    node->data.call.func_name = lv_strdup(func_name);
    node->data.call.args = args;
    if (args) {
        node->child = args;
        /* 计数子节点 */
        int count = 0;
        for (LvAstNode *c = args; c; c = c->next)
            count++;
        node->child_count = count;
    }
    return node;
}

/**
 * @brief 创建带节点类型的函数/关系/度量/几何调用节点
 *
 * @param type      节点类型（LV_AST_FUNCTION_CALL / LV_AST_RELATION /
 *                  LV_AST_MEASURE / LV_AST_GEOMETRY_EXPR）
 * @param loc       源代码位置
 * @param func_name 函数名称（会被复制到节点内部存储）
 * @param args      参数指针数组（数组内节点会按顺序链接为参数链表）
 * @param arg_count 参数个数
 * @return 节点指针，失败返回 NULL
 */
LvAstNode *lv_ast_create_call_typed(LvAstNodeType type, LvSourceLoc loc, const char *func_name,
                                    LvAstNode *const *args, int arg_count) {
    LvAstNode *node = ast_alloc(type, loc, "failed to create call node");
    if (!node)
        return NULL;
    node->data.call.func_name = lv_strdup(func_name);
    LvAstNode *head = NULL;
    LvAstNode *tail = NULL;
    for (int i = 0; i < arg_count; i++) {
        if (head)
            tail->next = args[i];
        else
            head = args[i];
        tail = args[i];
    }
    if (tail)
        tail->next = NULL;
    node->data.call.args = head;
    node->child = head;
    node->child_count = arg_count;
    return node;
}

/**
 * @brief 创建二元运算表达式节点
 *
 * @param loc   源代码位置
 * @param op    运算符字符串（如 "+", "-", "*", "/"）
 * @param left  左操作数
 * @param right 右操作数
 * @return 节点指针，失败返回 NULL
 */
LvAstNode *lv_ast_create_binary(LvSourceLoc loc, const char *op, LvAstNode *left, LvAstNode *right) {
    LvAstNode *node = ast_alloc(LV_AST_BINARY_OP, loc, "failed to create binary node");
    lv_strlcpy(node->data.binary.op, op, sizeof(node->data.binary.op));
    node->data.binary.left = left;
    node->data.binary.right = right;
    return node;
}

/**
 * @brief 创建一元运算表达式节点
 *
 * @param loc     源代码位置
 * @param op      运算符字符串
 * @param operand 操作数
 * @return 节点指针，失败返回 NULL
 */
LvAstNode *lv_ast_create_unary(LvSourceLoc loc, const char *op, LvAstNode *operand) {
    LvAstNode *node = ast_alloc(LV_AST_UNARY_OP, loc, "failed to create unary node");
    lv_strlcpy(node->data.unary.op, op, sizeof(node->data.unary.op));
    node->data.unary.operand = operand;
    return node;
}

/**
 * @brief 创建比较表达式节点
 *
 * @param loc   源代码位置
 * @param op    比较运算符（如 "==", "!=", "<", ">"）
 * @param left  左操作数
 * @param right 右操作数
 * @return 节点指针，失败返回 NULL
 */
LvAstNode *lv_ast_create_compare(LvSourceLoc loc, const char *op, LvAstNode *left, LvAstNode *right) {
    LvAstNode *node = ast_alloc(LV_AST_COMPARE, loc, "failed to create compare node");
    lv_strlcpy(node->data.compare.op, op, sizeof(node->data.compare.op));
    node->data.compare.left = left;
    node->data.compare.right = right;
    return node;
}

/**
 * @brief 创建逻辑二元运算节点
 *
 * @param type  节点类型（LV_AST_LOGIC_AND / LV_AST_LOGIC_OR /
 *              LV_AST_LOGIC_IMPLIES / LV_AST_LOGIC_IFF）
 * @param loc   源代码位置
 * @param op    运算符字符串（如 "and", "or", "->", "iff"）
 * @param left  左操作数
 * @param right 右操作数
 * @return 节点指针，失败返回 NULL
 */
LvAstNode *lv_ast_create_logic_binary(LvAstNodeType type, LvSourceLoc loc, const char *op,
                                      LvAstNode *left, LvAstNode *right) {
    LvAstNode *node = ast_alloc(type, loc, "failed to create logic binary node");
    if (!node)
        return NULL;
    lv_strlcpy(node->data.binary.op, op, sizeof(node->data.binary.op));
    node->data.binary.left = left;
    node->data.binary.right = right;
    return node;
}

/* ── 追加子节点 ── */

/**
 * @brief 向父节点追加一个子节点
 *
 * 将 child 追加到 parent 的子节点链表末尾。同时更新 child_count。
 *
 * @param parent 父节点
 * @param child  子节点
 */
void lv_ast_append_child(LvAstNode *parent, LvAstNode *child) {
    if (!parent || !child)
        return;
    if (!parent->child) {
        parent->child = child;
    } else {
        LvAstNode *last = parent->child;
        while (last->next)
            last = last->next;
        last->next = child;
    }
    parent->child_count++;
}

/* ── VTable 定义 ── */

/** 销毁 handler 函数指针类型 */
typedef void (*LvAstDestroyFunc)(LvAstNode *node);

/** Debug 打印 handler 函数指针类型，向 lvStrBuf 追加调试信息 */
typedef int (*LvAstDebugPrintFunc)(const LvAstNode *node, lvStrBuf *sb);

/** 子节点打印 handler 函数指针类型 */
typedef void (*LvAstPrintFunc)(const LvAstNode *node, int indent);

/* ── 求值/折叠/比较 handler 类型（VTable 分发契约；签名与 lv_loader.c 求值函数对应）── */

/** @brief 命题变量名长度上限（与 lv_loader.c 的 LV_PROP_VAR_NAME_MAX 一致） */
#define LV_AST_PROP_VAR_NAME_MAX 32

/** @brief AST 折叠值类型（与 lv_loader.c 的 LvVal 布局一致；仅用于槽位签名） */
typedef enum {
    LV_AST_VAL_NUM,
    LV_AST_VAL_BOOL
} LvAstValKind;

typedef struct {
    LvAstValKind kind;
    long long num;
    int boolean;
} LvAstVal;

/** 折叠 handler：闭合表达式 → LvAstVal；返回 false 表示无法折叠 */
typedef bool (*LvAstFoldFunc)(LvAstNode *node, LvAstVal *out, int depth);
/** 命题求值 handler：返回 -1 无法判定，0 假，1 真 */
typedef int (*LvAstEvalPropFunc)(const LvAstNode *node, int depth);
/** 纯命题骨架判定 handler */
typedef bool (*LvAstIsPurePropFunc)(const LvAstNode *node, int depth);
/** 原子命题名收集 handler */
typedef void (*LvAstCollectVarsFunc)(const LvAstNode *node, char vars[][LV_AST_PROP_VAR_NAME_MAX], int *count,
                                     bool *overflow, int depth);
/** 真值表骨架求值 handler */
typedef int (*LvAstEvalSkeletonFunc)(const LvAstNode *node, const char vars[][LV_AST_PROP_VAR_NAME_MAX],
                                     const int *vals, int nvars, int depth);

/**
 * @brief AST VTable 结构体
 *
 * 每个 AST 节点类型通过 VTable 分发三种操作：
 * - destroy:    释放节点 union data 中的动态分配内存
 * - debug_print:向缓冲区追加节点类型相关的调试信息，返回新偏移量
 * - print:      递归打印节点特有的子节点
 *
 * 求值/折叠/比较槽位（fold / eval_proposition / is_pure_propositional /
 * collect_prop_vars / eval_prop_skeleton）为 AST 求值分发契约：实际求值逻辑
 * 定义于 lv_loader.c 的 AST 求值分发表（handler 依赖 lv_loader.c 私有的
 * LvVal/Church 表），本表对应槽位保持 NULL。
 */
typedef struct {
    LvAstDestroyFunc    destroy;
    LvAstDebugPrintFunc debug_print;
    LvAstPrintFunc      print;
    LvAstFoldFunc         fold;              /**< 表达式折叠（闭合 → 值） */
    LvAstEvalPropFunc     eval_proposition;  /**< 命题求值（-1/0/1） */
    LvAstIsPurePropFunc   is_pure_propositional; /**< 纯命题骨架判定 */
    LvAstCollectVarsFunc  collect_prop_vars; /**< 原子命题名收集 */
    LvAstEvalSkeletonFunc eval_prop_skeleton;/**< 真值表骨架求值 */
} LvAstVTable;

/* ── Destroy handlers ── */

static void ast_destroy_declaration(LvAstNode *node) {
    lv_free((void **) &node->data.decl.names);
    lv_free((void **) &node->data.decl.return_type);
    if (node->data.decl.value) {
        lv_ast_destroy(node->data.decl.value);
        node->data.decl.value = NULL;
    }
}

static void ast_destroy_let(LvAstNode *node) {
    lv_free((void **) &node->data.let_def.name);
    lv_free((void **) &node->data.let_def.type_name);
}

static void ast_destroy_ident(LvAstNode *node) {
    lv_free((void **) &node->data.ident.name);
}

static void ast_destroy_string(LvAstNode *node) {
    lv_free((void **) &node->data.literal.string_value);
}

static void ast_destroy_quantifier(LvAstNode *node) {
    lv_free((void **) &node->data.quantifier.var_name);
    lv_free((void **) &node->data.quantifier.var_type);
}

static void ast_destroy_call(LvAstNode *node) {
    lv_free((void **) &node->data.call.func_name);
}

static void ast_destroy_export(LvAstNode *node) {
    lv_free((void **) &node->data.export_stmt.target);
    lv_free((void **) &node->data.export_stmt.format);
    lv_free((void **) &node->data.export_stmt.path);
}

static void ast_destroy_module_import(LvAstNode *node) {
    lv_free((void **) &node->data.module_import.qualified_name);
}

static void ast_destroy_theorem(LvAstNode *node) {
    lv_free((void **) &node->data.theorem.name);
}

static void ast_destroy_normalize(LvAstNode *node) {
    lv_free((void **) &node->data.normalize.target);
}

static void ast_destroy_stmt_name(LvAstNode *node) {
    lv_free((void **) &node->data.stmt.name);
}

static void ast_destroy_struct_field(LvAstNode *node) {
    lv_free((void **) &node->data.field.name);
    if (node->data.field.value) {
        lv_ast_destroy(node->data.field.value);
        node->data.field.value = NULL;
    }
}

static void ast_destroy_union(LvAstNode *node) {
    if (node->data.binary.left) {
        lv_ast_destroy(node->data.binary.left);
        node->data.binary.left = NULL;
    }
    if (node->data.binary.right) {
        lv_ast_destroy(node->data.binary.right);
        node->data.binary.right = NULL;
    }
}

static void ast_destroy_predicate_app(LvAstNode *node) {
    lv_free((void **) &node->data.call.func_name);
    if (node->data.call.args) {
        lv_ast_destroy(node->data.call.args);
        node->data.call.args = NULL;
    }
}

static void ast_destroy_nop(LvAstNode *node) {
    (void)node;
}

/* ── Debug print handlers ── */

static int ast_debug_declaration(const LvAstNode *node, lvStrBuf *sb) {
    lv_strbuf_printf(sb, " [entity=%s, names=%s]",
                     lv_entity_type_name((LvEntityType) node->data.decl.entity_type),
                     node->data.decl.names ? node->data.decl.names : "");
    if (node->data.decl.return_type)
        lv_strbuf_printf(sb, " [return_type=%s]", node->data.decl.return_type);
    return 0;
}

static int ast_debug_let(const LvAstNode *node, lvStrBuf *sb) {
    lv_strbuf_printf(sb, " [name=%s, type=%s]",
                     node->data.let_def.name ? node->data.let_def.name : "",
                     node->data.let_def.type_name ? node->data.let_def.type_name : "");
    return 0;
}

static int ast_debug_ident(const LvAstNode *node, lvStrBuf *sb) {
    lv_strbuf_printf(sb, " [%s]",
                     node->data.ident.name ? node->data.ident.name : "");
    return 0;
}

static int ast_debug_integer(const LvAstNode *node, lvStrBuf *sb) {
    lv_strbuf_printf(sb, " [%lld]",
                     node->data.literal.integer_value);
    return 0;
}

static int ast_debug_rational(const LvAstNode *node, lvStrBuf *sb) {
    lv_strbuf_printf(sb, " [%lld/%lld]",
                     node->data.literal.rational_value.num,
                     node->data.literal.rational_value.den);
    return 0;
}

static int ast_debug_decimal(const LvAstNode *node, lvStrBuf *sb) {
    lv_strbuf_printf(sb, " [%g]",
                     node->data.literal.decimal_value);
    return 0;
}

static int ast_debug_string(const LvAstNode *node, lvStrBuf *sb) {
    lv_strbuf_printf(sb, " [\"%s\"]",
                     node->data.literal.string_value ? node->data.literal.string_value : "");
    return 0;
}

static int ast_debug_bool(const LvAstNode *node, lvStrBuf *sb) {
    lv_strbuf_printf(sb, " [%s]",
                     node->data.literal.bool_value ? "true" : "false");
    return 0;
}

static int ast_debug_binary_op(const LvAstNode *node, lvStrBuf *sb) {
    lv_strbuf_printf(sb, " [%s]", node->data.binary.op);
    return 0;
}

static int ast_debug_unary_op(const LvAstNode *node, lvStrBuf *sb) {
    lv_strbuf_printf(sb, " [%s]", node->data.unary.op);
    return 0;
}

static int ast_debug_compare(const LvAstNode *node, lvStrBuf *sb) {
    lv_strbuf_printf(sb, " [%s]", node->data.compare.op);
    return 0;
}

static int ast_debug_call(const LvAstNode *node, lvStrBuf *sb) {
    lv_strbuf_printf(sb, " [%s]",
                     node->data.call.func_name ? node->data.call.func_name : "");
    return 0;
}

static int ast_debug_quantifier(const LvAstNode *node, lvStrBuf *sb) {
    lv_strbuf_printf(sb, " [%s: %s]",
                     node->data.quantifier.var_name ? node->data.quantifier.var_name : "",
                     node->data.quantifier.var_type ? node->data.quantifier.var_type : "");
    return 0;
}

static int ast_debug_export(const LvAstNode *node, lvStrBuf *sb) {
    lv_strbuf_printf(sb, " [target=%s, format=%s]",
                     node->data.export_stmt.target ? node->data.export_stmt.target : "",
                     node->data.export_stmt.format ? node->data.export_stmt.format : "");
    return 0;
}

static int ast_debug_module_import(const LvAstNode *node, lvStrBuf *sb) {
    lv_strbuf_printf(sb, " [%s]",
                     node->data.module_import.qualified_name ? node->data.module_import.qualified_name : "");
    return 0;
}

static int ast_debug_theorem(const LvAstNode *node, lvStrBuf *sb) {
    lv_strbuf_printf(sb, " [%s]",
                     node->data.theorem.name ? node->data.theorem.name : "");
    return 0;
}

static int ast_debug_normalize(const LvAstNode *node, lvStrBuf *sb) {
    lv_strbuf_printf(sb, " [%s]",
                     node->data.normalize.target ? node->data.normalize.target : "");
    return 0;
}

static int ast_debug_stmt_name(const LvAstNode *node, lvStrBuf *sb) {
    lv_strbuf_printf(sb, " [name=%s]",
                     node->data.stmt.name ? node->data.stmt.name : "");
    return 0;
}

static int ast_debug_struct_literal(const LvAstNode *node, lvStrBuf *sb) {
    lv_strbuf_printf(sb, " [fields=%d]", node->child_count);
    return 0;
}

static int ast_debug_struct_field(const LvAstNode *node, lvStrBuf *sb) {
    lv_strbuf_printf(sb, " [%s]",
                     node->data.field.name ? node->data.field.name : "");
    return 0;
}

static int ast_debug_nop(const LvAstNode *node, lvStrBuf *sb) {
    (void)node;
    (void)sb;
    return 0;
}

/* ── Print children handlers ── */

/* Forward declaration needed by VTable print handlers */
void lv_ast_print(const LvAstNode *node, int indent);

static void ast_print_binary_op(const LvAstNode *node, int indent) {
    if (node->data.binary.left)
        lv_ast_print(node->data.binary.left, indent + 1);
    if (node->data.binary.right)
        lv_ast_print(node->data.binary.right, indent + 1);
}

static void ast_print_unary_op(const LvAstNode *node, int indent) {
    if (node->data.unary.operand)
        lv_ast_print(node->data.unary.operand, indent + 1);
}

static void ast_print_compare(const LvAstNode *node, int indent) {
    if (node->data.compare.left)
        lv_ast_print(node->data.compare.left, indent + 1);
    if (node->data.compare.right)
        lv_ast_print(node->data.compare.right, indent + 1);
}

static void ast_print_let(const LvAstNode *node, int indent) {
    if (node->data.let_def.value)
        lv_ast_print(node->data.let_def.value, indent + 1);
}

static void ast_print_stmt(const LvAstNode *node, int indent) {
    if (node->data.stmt.expr)
        lv_ast_print(node->data.stmt.expr, indent + 1);
}

static void ast_print_theorem(const LvAstNode *node, int indent) {
    if (node->data.theorem.proposition)
        lv_ast_print(node->data.theorem.proposition, indent + 1);
    if (node->data.theorem.proof_block)
        lv_ast_print(node->data.theorem.proof_block, indent + 1);
}

static void ast_print_call(const LvAstNode *node, int indent) {
    if (node->data.call.args)
        lv_ast_print(node->data.call.args, indent + 1);
}

static void ast_print_declaration(const LvAstNode *node, int indent) {
    if (node->data.decl.value)
        lv_ast_print(node->data.decl.value, indent + 1);
}

static void ast_print_struct_field(const LvAstNode *node, int indent) {
    if (node->data.field.value)
        lv_ast_print(node->data.field.value, indent + 1);
}

static void ast_print_quantifier(const LvAstNode *node, int indent) {
    if (node->data.quantifier.body)
        lv_ast_print(node->data.quantifier.body, indent + 1);
}

static void ast_print_nop(const LvAstNode *node, int indent) {
    (void)node;
    (void)indent;
}

/* ── VTable 查找表 ── */

#define LV_AST_VTABLE_COUNT LV_AST_COUNT

static const LvAstVTable kAstVTable[LV_AST_VTABLE_COUNT] = {
    [LV_AST_PROGRAM]         = { ast_destroy_nop,        ast_debug_nop,        ast_print_nop },
    [LV_AST_DECLARATION]     = { ast_destroy_declaration, ast_debug_declaration, ast_print_declaration },
    [LV_AST_LET]             = { ast_destroy_let,         ast_debug_let,         ast_print_let },
    [LV_AST_CONSTRAINT_STMT] = { ast_destroy_stmt_name,   ast_debug_stmt_name,   ast_print_stmt },
    [LV_AST_ASSUME_STMT]     = { ast_destroy_nop,        ast_debug_nop,         ast_print_stmt },
    [LV_AST_ASSERT_STMT]     = { ast_destroy_nop,        ast_debug_nop,         ast_print_stmt },
    [LV_AST_PROVE_STMT]      = { ast_destroy_nop,        ast_debug_nop,         ast_print_stmt },
    [LV_AST_COMPUTE_STMT]    = { ast_destroy_nop,        ast_debug_nop,         ast_print_stmt },
    [LV_AST_NORMALIZE_STMT]  = { ast_destroy_normalize,  ast_debug_normalize,   ast_print_nop },
    [LV_AST_EXPORT_STMT]     = { ast_destroy_export,     ast_debug_export,      ast_print_nop },
    [LV_AST_AXIOM_STMT]      = { ast_destroy_nop,        ast_debug_nop,         ast_print_stmt },
    [LV_AST_THEOREM_STMT]    = { ast_destroy_theorem,    ast_debug_theorem,     ast_print_theorem },
    [LV_AST_IDENTIFIER_EXPR] = { ast_destroy_ident,      ast_debug_ident,       ast_print_nop },
    [LV_AST_INTEGER_LITERAL] = { ast_destroy_nop,        ast_debug_integer,     ast_print_nop },
    [LV_AST_RATIONAL_LITERAL]= { ast_destroy_nop,        ast_debug_rational,    ast_print_nop },
    [LV_AST_DECIMAL_LITERAL] = { ast_destroy_nop,        ast_debug_decimal,     ast_print_nop },
    [LV_AST_STRING_LITERAL]  = { ast_destroy_string,     ast_debug_string,      ast_print_nop },
    [LV_AST_BOOL_LITERAL]    = { ast_destroy_nop,        ast_debug_bool,        ast_print_nop },
    [LV_AST_LOGIC_AND]       = { ast_destroy_nop,        ast_debug_binary_op,   ast_print_nop },
    [LV_AST_LOGIC_OR]        = { ast_destroy_nop,        ast_debug_binary_op,   ast_print_nop },
    [LV_AST_LOGIC_NOT]       = { ast_destroy_nop,        ast_debug_nop,         ast_print_nop },
    [LV_AST_LOGIC_IMPLIES]   = { ast_destroy_nop,        ast_debug_binary_op,   ast_print_nop },
    [LV_AST_LOGIC_IFF]       = { ast_destroy_nop,        ast_debug_binary_op,   ast_print_nop },
    [LV_AST_LOGIC_FORALL]    = { ast_destroy_quantifier, ast_debug_quantifier,  ast_print_quantifier },
    [LV_AST_LOGIC_EXISTS]    = { ast_destroy_quantifier, ast_debug_quantifier,  ast_print_quantifier },
    [LV_AST_BINARY_OP]       = { ast_destroy_nop,        ast_debug_binary_op,   ast_print_binary_op },
    [LV_AST_UNARY_OP]        = { ast_destroy_nop,        ast_debug_unary_op,    ast_print_unary_op },
    [LV_AST_FUNCTION_CALL]   = { ast_destroy_call,       ast_debug_call,        ast_print_call },
    [LV_AST_RELATION]        = { ast_destroy_call,       ast_debug_call,        ast_print_call },
    [LV_AST_MEASURE]         = { ast_destroy_call,       ast_debug_call,        ast_print_call },
    [LV_AST_GEOMETRY_EXPR]   = { ast_destroy_call,       ast_debug_call,        ast_print_call },
    [LV_AST_COMPARE]         = { ast_destroy_nop,        ast_debug_compare,     ast_print_compare },
    [LV_AST_STRUCT_LITERAL]  = { ast_destroy_nop,        ast_debug_struct_literal, ast_print_nop },
    [LV_AST_STRUCT_FIELD]    = { ast_destroy_struct_field, ast_debug_struct_field, ast_print_struct_field },
    [LV_AST_NAMED_ARG]       = { ast_destroy_struct_field, ast_debug_struct_field, ast_print_struct_field },
    [LV_AST_UNION]           = { ast_destroy_union,      ast_debug_binary_op,   ast_print_binary_op },
    [LV_AST_PREDICATE_APP]   = { ast_destroy_predicate_app, ast_debug_call,      ast_print_call },
    [LV_AST_MODULE_DECL]     = { ast_destroy_module_import, ast_debug_module_import, ast_print_nop },
    [LV_AST_IMPORT_DECL]     = { ast_destroy_module_import, ast_debug_module_import, ast_print_nop },
    [LV_AST_PROOF_BLOCK]     = { ast_destroy_nop,        ast_debug_nop,         ast_print_nop },
};

/* ── 销毁 AST 树 ── */

/**
 * @brief 递归销毁 AST 树
 *
 * 先递归销毁兄弟节点（next）和子节点（child），然后根据节点类型
 * 释放 union data 中的动态分配数据，最后释放节点本身。
 *
 * @param node 要销毁的 AST 子树根节点（允许为 NULL）
 */
void lv_ast_destroy(LvAstNode *node) {
    if (!node)
        return;

    /* 先递归销毁兄弟和子节点 */
    if (node->next) {
        lv_ast_destroy(node->next);
        node->next = NULL;
    }
    if (node->child) {
        lv_ast_destroy(node->child);
        node->child = NULL;
    }

    /* 通过 VTable 释放 union 中动态分配的内存 */
    if (node->type >= 0 && node->type < LV_AST_VTABLE_COUNT && kAstVTable[node->type].destroy)
        kAstVTable[node->type].destroy(node);

    lv_free((void **) &node);
}

/* ── 打印 AST 树 ── */

/**
 * @brief 获取 AST 节点类型的字符串名称
 *
 * @param type 节点类型枚举值
 * @return 类型名称字符串（静态存储，无需释放）
 */
/* LV_AST_TYPE_X：AST 节点类型 → 规范名 单一事实来源（ast_type_name 由宏生成） */
#define LV_AST_TYPE_X(x) \
    x(LV_AST_PROGRAM, "PROGRAM") \
    x(LV_AST_DECLARATION, "DECLARATION") \
    x(LV_AST_LET, "LET") \
    x(LV_AST_CONSTRAINT_STMT, "CONSTRAINT_STMT") \
    x(LV_AST_ASSUME_STMT, "ASSUME_STMT") \
    x(LV_AST_ASSERT_STMT, "ASSERT_STMT") \
    x(LV_AST_PROVE_STMT, "PROVE_STMT") \
    x(LV_AST_COMPUTE_STMT, "COMPUTE_STMT") \
    x(LV_AST_NORMALIZE_STMT, "NORMALIZE_STMT") \
    x(LV_AST_EXPORT_STMT, "EXPORT_STMT") \
    x(LV_AST_AXIOM_STMT, "AXIOM_STMT") \
    x(LV_AST_THEOREM_STMT, "THEOREM_STMT") \
    x(LV_AST_IDENTIFIER_EXPR, "IDENTIFIER_EXPR") \
    x(LV_AST_INTEGER_LITERAL, "INTEGER_LITERAL") \
    x(LV_AST_RATIONAL_LITERAL, "RATIONAL_LITERAL") \
    x(LV_AST_DECIMAL_LITERAL, "DECIMAL_LITERAL") \
    x(LV_AST_STRING_LITERAL, "STRING_LITERAL") \
    x(LV_AST_BOOL_LITERAL, "BOOL_LITERAL") \
    x(LV_AST_LOGIC_AND, "LOGIC_AND") \
    x(LV_AST_LOGIC_OR, "LOGIC_OR") \
    x(LV_AST_LOGIC_NOT, "LOGIC_NOT") \
    x(LV_AST_LOGIC_IMPLIES, "LOGIC_IMPLIES") \
    x(LV_AST_LOGIC_IFF, "LOGIC_IFF") \
    x(LV_AST_LOGIC_FORALL, "LOGIC_FORALL") \
    x(LV_AST_LOGIC_EXISTS, "LOGIC_EXISTS") \
    x(LV_AST_BINARY_OP, "BINARY_OP") \
    x(LV_AST_UNARY_OP, "UNARY_OP") \
    x(LV_AST_FUNCTION_CALL, "FUNCTION_CALL") \
    x(LV_AST_RELATION, "RELATION") \
    x(LV_AST_MEASURE, "MEASURE") \
    x(LV_AST_GEOMETRY_EXPR, "GEOMETRY_EXPR") \
    x(LV_AST_COMPARE, "COMPARE") \
    x(LV_AST_STRUCT_LITERAL, "STRUCT_LITERAL") \
    x(LV_AST_STRUCT_FIELD, "STRUCT_FIELD") \
    x(LV_AST_NAMED_ARG, "NAMED_ARG") \
    x(LV_AST_UNION, "UNION") \
    x(LV_AST_PREDICATE_APP, "PREDICATE_APP") \
    x(LV_AST_MODULE_DECL, "MODULE_DECL") \
    x(LV_AST_IMPORT_DECL, "IMPORT_DECL") \
    x(LV_AST_PROOF_BLOCK, "PROOF_BLOCK")

static const char *ast_type_name(LvAstNodeType type) {
    static const char *const names[LV_AST_COUNT] = {
#define LV_AST_NAME_ENTRY(tag, str) [tag] = str,
        LV_AST_TYPE_X(LV_AST_NAME_ENTRY)
#undef LV_AST_NAME_ENTRY
    };
    if (type >= 0 && type < LV_AST_COUNT)
        return names[type];
    return "UNKNOWN";
}

/**
 * @brief 递归打印 AST 树
 *
 * 以缩进格式输出 AST 树的结构，显示每个节点的类型和附加信息
 * （如标识符名称、字面量值、运算符等）。递归遍历兄弟节点、子节点
 * 以及特殊子节点（二元/一元运算的操作数等）。
 *
 * @param node   要打印的节点（允许为 NULL）
 * @param indent 当前缩进层级
 */
void lv_ast_print(const LvAstNode *node, int indent) {
    if (!node)
        return;

    /* 使用 lvStrBuf 构建行，替代固定栈缓冲区 + off 游标 */
    lvStrBuf sb = {0};

    /* 缩进 */
    for (int i = 0; i < indent; i++) {
        lv_strbuf_printf(&sb, "  ");
    }

    lv_strbuf_printf(&sb, "%s", ast_type_name(node->type));

    /* 通过 VTable 打印节点类型相关的调试信息 */
    if (node->type >= 0 && node->type < LV_AST_VTABLE_COUNT && kAstVTable[node->type].debug_print)
        kAstVTable[node->type].debug_print(node, &sb);

    if (sb.len > 0)
        lv_INFO("%s", lv_strbuf_cstr(&sb));

    lv_strbuf_destroy(&sb);

    /* 递归打印子节点 */
    if (node->child) {
        lv_ast_print(node->child, indent + 1);
    }

    /* 通过 VTable 打印节点特有的子节点 */
    if (node->type >= 0 && node->type < LV_AST_VTABLE_COUNT && kAstVTable[node->type].print)
        kAstVTable[node->type].print(node, indent);

    /* 打印兄弟节点 */
    if (node->next) {
        lv_ast_print(node->next, indent);
    }
}

/* ── 实体类型名称 ── */

/**
 * @brief 获取实体类型的字符串名称
 *
 * @param type 实体类型枚举值
 * @return 类型名称字符串（静态存储，无需释放）
 */
const char *lv_entity_type_name(LvEntityType type) {
    static const char *const names[] = {
        lv_XMACRO_TO_NAME_ARRAY(LV_ENTITY_TYPE_X)
    };
    if (type >= 0 && type < LV_ENTITY_COUNT)
        return names[type];
    return "Unknown";
}

/* ── 根据关键字 token 获取实体类型 ── */

/**
 * @brief 根据关键字 Token 类型获取对应的实体类型
 *
 * 将解析器识别的关键字 Token（如 LV_TOKEN_KW_POINT）转换为
 * 实体类型枚举值（如 LV_ENTITY_POINT）。
 *
 * @param tok Token 类型
 * @return 对应的实体类型；若无法映射则返回 LV_ENTITY_COUNT（非法值）
 */
/** LvTokenType → LvEntityType 查找表 */
static const LvEntityType kTokenToEntityType[LV_TOKEN_COUNT] = {
    [LV_TOKEN_KW_POINT]       = LV_ENTITY_POINT,
    [LV_TOKEN_KW_LINE]        = LV_ENTITY_LINE,
    [LV_TOKEN_KW_CIRCLE]      = LV_ENTITY_CIRCLE,
    [LV_TOKEN_KW_SEGMENT]     = LV_ENTITY_SEGMENT,
    [LV_TOKEN_KW_RAY]         = LV_ENTITY_RAY,
    [LV_TOKEN_KW_ANGLE]       = LV_ENTITY_ANGLE,
    [LV_TOKEN_KW_TRIANGLE]    = LV_ENTITY_TRIANGLE,
    [LV_TOKEN_KW_POLYGON]     = LV_ENTITY_POLYGON,
    [LV_TOKEN_KW_SCALAR]      = LV_ENTITY_SCALAR,
    [LV_TOKEN_KW_BOOL]        = LV_ENTITY_BOOL,
    [LV_TOKEN_KW_PROPOSITION] = LV_ENTITY_PROPOSITION,
    [LV_TOKEN_KW_PROOF]       = LV_ENTITY_PROOF,
};

LvEntityType lv_entity_type_from_token(LvTokenType tok) {
    if ((unsigned)tok < LV_TOKEN_COUNT) {
        LvEntityType result = kTokenToEntityType[tok];
        /* 未显式初始化的条目为 0 (LV_ENTITY_POINT)，需排除非 LV_TOKEN_KW_POINT 的误匹配 */
        if (result != LV_ENTITY_POINT || tok == LV_TOKEN_KW_POINT)
            return result;
    }
    return LV_ENTITY_COUNT; /* 非法值 */
}
