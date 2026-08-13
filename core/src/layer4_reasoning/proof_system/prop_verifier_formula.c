/*
 * @file prop_verifier_formula.c
 * @brief Proposition verifier module - formula create/destroy
 * @details Split from prop_verifier.c
 */

#include "lv/prop_verifier.h"
#include "prop_verifier_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"
#include "lv/stream_context_util.h"
#include "lv/lv_xmacro.h" /* LV_DISPATCH / LV_DISPATCH_VOID */

/* ============================================================
 * 公式创建/销毁
 * ============================================================ */

/**
 * @brief 创建原子命题公式
 *
 * @param name 原子命题名称
 * @return 新分配的公式指针，失败返回 NULL
 */
static PropFormula *formula_alloc(PropFormulaType type) {
    PropFormula *f = (PropFormula *) lv_calloc(1, sizeof(PropFormula)); /* 零初始化分配 */
    if (!f)
        return NULL;
    f->type = type;
    return f;
}

PropFormula *prop_formula_create_atom(const char *name) {
    if (!name)
        return NULL;
    PropFormula *f = formula_alloc(PROP_ATOM);
    if (!f)
        return NULL;
    lv_strlcpy(f->data.atom.name, name, sizeof(f->data.atom.name));
    return f;
}

/**
 * @brief 创建合取公式（A AND B）
 *
 * @param left  左侧操作数
 * @param right 右侧操作数
 * @return 新分配的公式指针，失败返回 NULL
 */
PropFormula *prop_formula_create_conjunction(PropFormula *left, PropFormula *right) {
    if (!left || !right)
        return NULL;
    PropFormula *f = formula_alloc(PROP_CONJUNCTION);
    if (!f)
        return NULL;
    f->data.binary.left = left;
    f->data.binary.right = right;
    return f;
}

/**
 * @brief 创建析取公式（A OR B）
 *
 * @param left  左侧操作数
 * @param right 右侧操作数
 * @return 新分配的公式指针，失败返回 NULL
 */
PropFormula *prop_formula_create_disjunction(PropFormula *left, PropFormula *right) {
    if (!left || !right)
        return NULL;
    PropFormula *f = formula_alloc(PROP_DISJUNCTION);
    if (!f)
        return NULL;
    f->data.binary.left = left;
    f->data.binary.right = right;
    return f;
}

/**
 * @brief 创建蕴含公式（A IMPLIES B）
 *
 * @param left  前件
 * @param right 后件
 * @return 新分配的公式指针，失败返回 NULL
 */
PropFormula *prop_formula_create_implication(PropFormula *left, PropFormula *right) {
    if (!left || !right)
        return NULL;
    PropFormula *f = formula_alloc(PROP_IMPLICATION);
    if (!f)
        return NULL;
    f->data.binary.left = left;
    f->data.binary.right = right;
    return f;
}

/**
 * @brief 创建否定公式（NOT A）
 *
 * @param operand 操作数
 * @return 新分配的公式指针，失败返回 NULL
 */
PropFormula *prop_formula_create_negation(PropFormula *operand) {
    if (!operand)
        return NULL;
    PropFormula *f = formula_alloc(PROP_NEGATION);
    if (!f)
        return NULL;
    f->data.unary.operand = operand;
    return f;
}

/**
 * @brief 创建底类型公式（矛盾/假）
 *
 * @return 新分配的公式指针，失败返回 NULL
 */
PropFormula *prop_formula_create_bottom(void) {
    return formula_alloc(PROP_BOTTOM);
}

/**
 * @brief 创建真值公式（永真/真）
 *
 * @return 新分配的公式指针，失败返回 NULL
 */
PropFormula *prop_formula_create_true(void) {
    return formula_alloc(PROP_TRUE);
}

/* 内部前置声明（static 函数使用前置声明） */
static PropFormula *prop_formula_copy_depth(const PropFormula *f, int depth);
static void prop_formula_destroy_depth(PropFormula *f, int depth);

/* ---- Copy VTable ---- */

/** 拷贝处理函数类型 */
typedef PropFormula *(*CopyHandler)(const PropFormula *f, int depth);

static PropFormula *copy_atom(const PropFormula *f, int depth) {
    (void)depth;
    return prop_formula_create_atom(f->data.atom.name);
}

static PropFormula *copy_conjunction(const PropFormula *f, int depth) {
    return prop_formula_create_conjunction(prop_formula_copy_depth(f->data.binary.left, depth + 1),
                                           prop_formula_copy_depth(f->data.binary.right, depth + 1));
}

static PropFormula *copy_disjunction(const PropFormula *f, int depth) {
    return prop_formula_create_disjunction(prop_formula_copy_depth(f->data.binary.left, depth + 1),
                                           prop_formula_copy_depth(f->data.binary.right, depth + 1));
}

static PropFormula *copy_implication(const PropFormula *f, int depth) {
    return prop_formula_create_implication(prop_formula_copy_depth(f->data.binary.left, depth + 1),
                                           prop_formula_copy_depth(f->data.binary.right, depth + 1));
}

static PropFormula *copy_negation(const PropFormula *f, int depth) {
    return prop_formula_create_negation(prop_formula_copy_depth(f->data.unary.operand, depth + 1));
}

static PropFormula *copy_bottom(const PropFormula *f, int depth) {
    (void)f; (void)depth;
    return prop_formula_create_bottom();
}

static PropFormula *copy_true(const PropFormula *f, int depth) {
    (void)f; (void)depth;
    return prop_formula_create_true();
}

/** 拷贝 VTable — 按 PropFormulaType 枚举值索引 */
static const CopyHandler copy_handlers[] = {
    [PROP_ATOM]         = copy_atom,
    [PROP_CONJUNCTION]  = copy_conjunction,
    [PROP_DISJUNCTION]  = copy_disjunction,
    [PROP_IMPLICATION]  = copy_implication,
    [PROP_NEGATION]     = copy_negation,
    [PROP_BOTTOM]       = copy_bottom,
    [PROP_TRUE]         = copy_true,
};

/* ---- Destroy VTable ---- */

/** 销毁栈推送辅助：将子节点压入销毁栈，必要时自动扩容 */
static bool push_to_stack(PropFormula *child, PropFormula ***stack, int *stack_top, int *stack_capacity) {
    if (!child)
        return true; /* NULL 子节点，无需操作 */
    if (*stack_top >= *stack_capacity) {
        if (!lv_ensure_capacity((void **) stack, *stack_top, stack_capacity, sizeof(PropFormula *), 0)) {
            /* 栈扩容失败，直接递归销毁该子节点 */
            prop_formula_destroy(child);
            return false;
        }
    }
    (*stack)[(*stack_top)++] = child;
    return true;
}

/** 销毁处理函数类型 */
typedef void (*DestroyHandler)(PropFormula *f, PropFormula ***stack, int *stack_top, int *stack_capacity);

static void destroy_binary(PropFormula *f, PropFormula ***stack, int *stack_top, int *stack_capacity) {
    /* 二元节点：先压右子节点，再压左子节点（后进先出） */
    push_to_stack(f->data.binary.right, stack, stack_top, stack_capacity);
    push_to_stack(f->data.binary.left, stack, stack_top, stack_capacity);
    f->data.binary.left = NULL;
    f->data.binary.right = NULL;
}

static void destroy_unary(PropFormula *f, PropFormula ***stack, int *stack_top, int *stack_capacity) {
    /* 一元节点：压入其子节点 */
    push_to_stack(f->data.unary.operand, stack, stack_top, stack_capacity);
    f->data.unary.operand = NULL;
}

static void destroy_leaf(PropFormula *f, PropFormula ***stack, int *stack_top, int *stack_capacity) {
    /* 叶子节点（ATOM, BOTTOM, TRUE）：无子节点 */
    (void)f; (void)stack; (void)stack_top; (void)stack_capacity;
}

/** 销毁 VTable — 按 PropFormulaType 枚举值索引 */
static const DestroyHandler destroy_handlers[] = {
    [PROP_ATOM]         = destroy_leaf,
    [PROP_CONJUNCTION]  = destroy_binary,
    [PROP_DISJUNCTION]  = destroy_binary,
    [PROP_IMPLICATION]  = destroy_binary,
    [PROP_NEGATION]     = destroy_unary,
    [PROP_BOTTOM]       = destroy_leaf,
    [PROP_TRUE]         = destroy_leaf,
};

/* 深拷贝公式（带递归深度保护，防止栈溢出） */
/**
 * @brief 深拷贝命题公式
 *
 * @param f 源公式指针
 * @return 副本公式指针，失败返回 NULL
 */
PropFormula *prop_formula_copy(const PropFormula *f) {
    return prop_formula_copy_depth(f, 0);
}

/**
 * @brief 深拷贝公式（内部实现，带递归深度保护）
 *
 * @param f     源公式
 * @param depth 当前递归深度
 * @return 副本公式指针，超深度返回 NULL
 */
static PropFormula *prop_formula_copy_depth(const PropFormula *f, int depth) {
    if (!f)
        return NULL;
    if (depth > MAX_COPY_DEPTH) {
        /* 递归深度超限，防止栈溢出 */
        return NULL;
    }
    /* 统一调度表分发（LV_DISPATCH：越界/NULL 槽返回 fallback NULL） */
    return LV_DISPATCH(copy_handlers, f->type, NULL, f, depth);
}

/* 递归销毁公式（带递归深度保护，防止栈溢出） */
/**
 * @brief 销毁命题公式（递归释放所有资源）
 *
 * @param f 公式指针（可为 NULL）
 */
void prop_formula_destroy(PropFormula *f) {
    prop_formula_destroy_depth(f, 0);
}

/**
 * @brief 安全销毁命题公式（迭代实现，防止栈溢出）
 *
 * 使用显式栈代替递归，避免深层嵌套公式导致的调用栈溢出。
 * 同时确保所有子公式节点都被正确释放，无内存泄漏。
 *
 * @param f     待销毁的命题公式指针
 * @param depth 未使用，保留以兼容递归函数签名
 */
static void prop_formula_destroy_depth(PropFormula *f, int depth) {
    (void) depth; /* 迭代实现不使用深度参数 */
    if (!f)
        return;

    /* 显式栈存储待销毁的公式节点 */
    int stack_capacity = PROP_DESTROY_STACK_INIT_CAP;
    int stack_top = 0;
    PropFormula **stack = (PropFormula **) lv_malloc((size_t) stack_capacity * sizeof(PropFormula *));
    if (!stack) {
        /* 内存分配失败，退化为简单递归（浅层公式仍可正确销毁） */
        prop_formula_destroy(f);
        return;
    }
    stack[stack_top++] = f;

    while (stack_top > 0) {
        PropFormula *current = stack[--stack_top];

        /* 统一调度表分发（LV_DISPATCH_VOID：越界/NULL 槽自动跳过） */
        LV_DISPATCH_VOID(destroy_handlers, current->type, current, &stack, &stack_top, &stack_capacity);

        /* 释放当前节点 */
        lv_free((void **) &current);
    }

    /* 释放栈 */
    lv_free((void **) &stack);
}