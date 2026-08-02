/**
 * @file formula_renderer_internal.c
 * @brief 公式渲染器基础设施（缓冲区池/希腊字母与三角函数映射/通用渲染工具）
 *
 * @details 从 formula_renderer.c 拆分的子模块（Lv-00 项目 v3.3.0+）。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "formula_renderer.h"
#include "formula_renderer_internal.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error_codes.h"
#include "lv_internal.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_thread.h"
#include "lv_utils.h"

/* ============================================================
 * 内部常量和宏
 * ============================================================ */

#define lv_MAX_RENDER_BUFFER 16384     /**< 渲染输出缓冲区大小 */
#define lv_MAX_ERROR_MESSAGE 256       /**< 错误消息缓冲区大小 */
#define lv_POINT_LATEX_BUF_SIZE 256    /**< 点坐标 LaTeX 渲染缓冲区大小 */
#define lv_SEGMENT_LATEX_BUF_SIZE 128  /**< 线段 LaTeX 渲染缓冲区大小 */
#define lv_CIRCLE_LATEX_BUF_SIZE 512   /**< 圆 LaTeX 渲染缓冲区大小 */
#define lv_FRACTION_LATEX_BUF_SIZE 128 /**< 分数 LaTeX 渲染缓冲区大小 */

/* ---------- 缓冲区大小控制宏（编译时可调整） ---------- */

/** 子表达式渲染默认缓冲区大小（统一控制所有 1024 字节缓冲） */
#ifndef lv_FORMULA_BUF_SIZE
#define lv_FORMULA_BUF_SIZE 1024
#endif

/** 小型缓冲区 —— 几何元素短标签（点名 "P1"、端点标识等） */
#define lv_FORMULA_BUF_SMALL 64

/** 中型缓冲区 —— 坐标字符串、半径表达式等 */
#define lv_FORMULA_BUF_MEDIUM 256

/** 大型缓冲区 —— 几何坐标组合渲染 */
#define lv_FORMULA_BUF_LARGE 2048

/* ---------- 缓冲区池（减少递归渲染时的 malloc/free 开销） ---------- */

/** 池中可用的缓冲区槽位数量 */
#define lv_FORMULA_POOL_SLOTS 8

/**
 * @brief 缓冲区池槽位
 *
 * 每个槽位持有一个堆分配的缓冲区指针。
 * 缓冲区仅在首次使用时懒初始化，之后被复用。
 */
/* ---------- 缓冲区池线程安全保护 ----------
 *
 * 缓冲区池是全局共享的静态数组，多线程并发渲染时存在竞态条件。
 * 使用 lv/lv_thread.h 提供的跨平台互斥锁保护 formula_pool_alloc 和
 * formula_pool_free 中的 in_use 标志读写。
 *
 * 线程安全的惰性初始化通过 lv_once() 实现，消除手动
 * InterlockedCompareExchange / PTHREAD_MUTEX_INITIALIZER 的平台差异。
 */

static lv_mutex_t g_formula_pool_mutex;
static lv_once_t g_formula_pool_once = lv_ONCE_INIT;

static void formula_pool_mutex_init(void) {
    lv_mutex_init(&g_formula_pool_mutex);
}

static void formula_pool_ensure_mutex_init(void) {
    lv_once(&g_formula_pool_once, formula_pool_mutex_init);
}

#define lv_FORMULA_POOL_LOCK()                       \
    do {                                             \
        formula_pool_ensure_mutex_init();            \
        lv_mutex_lock(&g_formula_pool_mutex);        \
    } while (0)
#define lv_FORMULA_POOL_UNLOCK() lv_mutex_unlock(&g_formula_pool_mutex)

/** 文件级缓冲区池，所有内部渲染函数共用（由 g_formula_pool_mutex 保护） */
static FormulaPoolSlot g_formula_buf_pool[lv_FORMULA_POOL_SLOTS] = {{NULL, false}};

/**
 * @brief 从池中获取一个已分配大小的缓冲区
 *
 * 优先从池中寻找空闲的同尺寸缓冲区复用；若没有空闲缓冲区且槽位未满，
 * 则分配新缓冲区；若池已满则回退到 lv_malloc 直接分配。
 *
 * 线程安全：整个函数在 lv_FORMULA_POOL_LOCK/UNLOCK 保护内执行，
 * 确保 in_use 标志的读写是原子的。
 *
 * @param size 需要的缓冲区大小（应 ≤ lv_FORMULA_BUF_SIZE）
 * @return 堆分配的缓冲区指针，失败返回 NULL
 */
char *formula_pool_alloc(size_t size) {
    char *result = NULL;

    lv_FORMULA_POOL_LOCK();

    /* 优先查找同尺寸的已分配空闲槽位 */
    for (int i = 0; i < lv_FORMULA_POOL_SLOTS; i++) {
        if (g_formula_buf_pool[i].data && !g_formula_buf_pool[i].in_use) {
            g_formula_buf_pool[i].in_use = true;
            /* STACK_SAFE: 零初始化后复用 */
            memset(g_formula_buf_pool[i].data, 0, lv_FORMULA_BUF_SIZE);
            result = g_formula_buf_pool[i].data;
            goto done;
        }
    }

    /* 查找空槽位以分配新缓冲区 */
    for (int i = 0; i < lv_FORMULA_POOL_SLOTS; i++) {
        if (!g_formula_buf_pool[i].data) {
            g_formula_buf_pool[i].data = (char *) lv_malloc(lv_FORMULA_BUF_SIZE);
            if (!g_formula_buf_pool[i].data) {
                result = NULL; /* OOM */
                goto done;
            }
            g_formula_buf_pool[i].in_use = true;
            memset(g_formula_buf_pool[i].data, 0, lv_FORMULA_BUF_SIZE);
            result = g_formula_buf_pool[i].data;
            goto done;
        }
    }

    /* 池已满，回退到直接分配（rare case，渲染输出仍正确） */
    result = (char *) lv_malloc(size);
    if (result) {
        memset(result, 0, size);
    }

done:
    lv_FORMULA_POOL_UNLOCK();
    return result;
}

/**
 * @brief 将池分配或回退分配的缓冲区归还
 *
 * 若指针属于池中的某个槽位，则标记为空闲以供复用；
 * 否则直接释放（回退分配的情况）。
 *
 * 线程安全：整个函数在 lv_FORMULA_POOL_LOCK/UNLOCK 保护内执行，
 * 确保 in_use 标志的读写是原子的。
 *
 * @param ptr 待归还的缓冲区指针
 */
void formula_pool_free(char *ptr) {
    if (!ptr)
        return;

    lv_FORMULA_POOL_LOCK();

    /* 检查是否属于池 */
    for (int i = 0; i < lv_FORMULA_POOL_SLOTS; i++) {
        if (g_formula_buf_pool[i].data == ptr) {
            g_formula_buf_pool[i].in_use = false;
            lv_FORMULA_POOL_UNLOCK();
            return;
        }
    }

    lv_FORMULA_POOL_UNLOCK();

    /* 不属于池，直接释放（回退分配） */
    lv_free((void **) &ptr);
}

/* 希腊字母映射表 */
static const GreekLetterMapping greek_letters[] = {{"alpha", "\\alpha"},
                                                   {"beta", "\\beta"},
                                                   {"gamma", "\\gamma"},
                                                   {"delta", "\\delta"},
                                                   {"epsilon", "\\epsilon"},
                                                   {"zeta", "\\zeta"},
                                                   {"eta", "\\eta"},
                                                   {"theta", "\\theta"},
                                                   {"iota", "\\iota"},
                                                   {"kappa", "\\kappa"},
                                                   {"lambda", "\\lambda"},
                                                   {"mu", "\\mu"},
                                                   {"nu", "\\nu"},
                                                   {"xi", "\\xi"},
                                                   {"omicron", "\\omicron"},
                                                   {"pi", "\\pi"},
                                                   {"rho", "\\rho"},
                                                   {"sigma", "\\sigma"},
                                                   {"tau", "\\tau"},
                                                   {"upsilon", "\\upsilon"},
                                                   {"phi", "\\phi"},
                                                   {"chi", "\\chi"},
                                                   {"psi", "\\psi"},
                                                   {"omega", "\\omega"},
                                                   {"Alpha", "A"},
                                                   {"Beta", "B"},
                                                   {"Gamma", "\\Gamma"},
                                                   {"Delta", "\\Delta"},
                                                   {"Epsilon", "E"},
                                                   {"Zeta", "Z"},
                                                   {"Eta", "E"},
                                                   {"Theta", "\\Theta"},
                                                   {"Iota", "I"},
                                                   {"Kappa", "K"},
                                                   {"Lambda", "\\Lambda"},
                                                   {"Mu", "M"},
                                                   {"Nu", "N"},
                                                   {"Xi", "\\Xi"},
                                                   {"Omicron", "O"},
                                                   {"Pi", "\\Pi"},
                                                   {"Rho", "P"},
                                                   {"Sigma", "\\Sigma"},
                                                   {"Tau", "T"},
                                                   {"Upsilon", "\\Upsilon"},
                                                   {"Phi", "\\Phi"},
                                                   {"Chi", "X"},
                                                   {"Psi", "\\Psi"},
                                                   {"Omega", "\\Omega"},
                                                   {NULL, NULL}};

/* 三角函数名映射表 */
static const TrigFunctionMapping trig_functions[] = {
    {"sin", "\\sin"},       {"cos", "\\cos"},   {"tan", "\\tan"},       {"cot", "\\cot"},
    {"sec", "\\sec"},       {"csc", "\\csc"},   {"arcsin", "\\arcsin"}, {"arccos", "\\arccos"},
    {"arctan", "\\arctan"}, {"sinh", "\\sinh"}, {"cosh", "\\cosh"},     {"tanh", "\\tanh"},
    {"ln", "\\ln"},         {"log", "\\log"},   {"exp", "\\exp"},       {NULL, NULL}};

/* ============================================================
 * 错误处理
 * ============================================================ */

/**
 * @brief 获取渲染器最近一次错误信息
 *
 * @return 错误信息字符串指针（内部缓冲区，无需释放），无错误时返回 NULL
 */
const char *formula_render_get_last_error(void) {
    return lv_get_last_error_message();
}

/* ============================================================
 * 辅助函数
 * ============================================================ */

const char *formula_latex_greek_name(const char *name) {
    if (!name)
        return name;

    for (int i = 0; greek_letters[i].name != NULL; i++) {
        if (strcmp(name, greek_letters[i].name) == 0) {
            return greek_letters[i].latex;
        }
    }
    return name;
}

const char *get_trig_latex(const char *name) {
    if (!name)
        return name;

    for (int i = 0; trig_functions[i].name != NULL; i++) {
        if (strcmp(name, trig_functions[i].name) == 0) {
            return trig_functions[i].latex;
        }
    }
    return name;
}
bool is_greek_letter(const char *name) {
    if (!name)
        return false;

    for (int i = 0; greek_letters[i].name != NULL; i++) {
        if (strcmp(name, greek_letters[i].name) == 0) {
            return true;
        }
    }
    return false;
}
bool needs_parentheses(const FormulaNode *node, NodeType parent_op, bool is_right) {
    if (!node)
        return false;

    /* 数字和变量不需要括号 */
    if (node->type == NODE_NUMBER || node->type == NODE_VARIABLE || node->type == NODE_IDENTIFIER) {
        return false;
    }

    /* 一元运算不需要括号（除了负号在某些情况下） */
    if (node->type >= NODE_UNARY_OP_NEG && node->type <= NODE_UNARY_OP_LOG) {
        if (parent_op == NODE_BINARY_OP_MUL || parent_op == NODE_BINARY_OP_DIV || parent_op == NODE_BINARY_OP_POW) {
            return true;
        }
        return false;
    }

    /* 几何对象不需要括号 */
    if (node->type >= NODE_GEOM_POINT && node->type <= NODE_GEOM_VECTOR) {
        return false;
    }

    /* 方程需要括号 */
    if (node->type == NODE_EQUATION) {
        return true;
    }

    /* 二元运算符优先级比较 */
    int node_prec = 0;
    int parent_prec = 0;

    /* 二元运算符优先级静态表 */
    static const int s_binop_prec[] = {
        [NODE_BINARY_OP_ADD] = 1,
        [NODE_BINARY_OP_SUB] = 1,
        [NODE_BINARY_OP_MUL] = 2,
        [NODE_BINARY_OP_DIV] = 3,
        [NODE_BINARY_OP_POW] = 4,
    };
#define BINOP_PREC_COUNT (sizeof(s_binop_prec) / sizeof(s_binop_prec[0]))
    if ((unsigned)node->type < BINOP_PREC_COUNT && s_binop_prec[node->type] > 0) {
        node_prec = s_binop_prec[node->type];
    } else {
        return false;
    }
    if ((unsigned)parent_op < BINOP_PREC_COUNT && s_binop_prec[parent_op] > 0) {
        parent_prec = s_binop_prec[parent_op];
    } else {
        return false;
    }

    /* 子节点优先级更低时需要括号 */
    if (node_prec < parent_prec) {
        return true;
    }

    /* 同优先级时，右侧减法需要括号 */
    if (node_prec == parent_prec && is_right && (parent_op == NODE_BINARY_OP_SUB || parent_op == NODE_BINARY_OP_DIV)) {
        return node->type == NODE_BINARY_OP_ADD || node->type == NODE_BINARY_OP_SUB;
    }

    return false;
}

/* ============================================================
 * 共享遍历器（binary / unary / dispatch 骨架去重）
 * ============================================================ */

int render_binary_via(const FormulaNode *node, const char *fmt, unsigned flags, char *buffer, size_t size,
                      const RenderOptions *options, RenderNodeFunc dispatch) {
    char *left_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
    char *right_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
    if (!left_buf || !right_buf) {
        formula_pool_free(left_buf);
        formula_pool_free(right_buf);
        if (flags & RENDER_VIA_ERROR_CTX)
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "failed to allocate sub-expression buffers");
        return -1;
    }

    int left_ret = dispatch(node->data.binary_op.left, left_buf, lv_FORMULA_BUF_SIZE, options);
    int right_ret = dispatch(node->data.binary_op.right, right_buf, lv_FORMULA_BUF_SIZE, options);
    if (flags & RENDER_VIA_CHECK_RET) {
        if (left_ret < 0 || right_ret < 0) {
            formula_pool_free(left_buf);
            formula_pool_free(right_buf);
            if (flags & RENDER_VIA_ERROR_CTX)
                lv_RETURN_ERROR(lv_ERROR_INTERNAL, "sub-expression render failed");
            return -1;
        }
    }

    int written = snprintf(buffer, size, fmt, left_buf, right_buf);

    formula_pool_free(left_buf);
    formula_pool_free(right_buf);
    return written;
}

int render_unary_via(const FormulaNode *node, const char *prefix, const char *suffix, unsigned flags,
                     char *buffer, size_t size, const RenderOptions *options, RenderNodeFunc dispatch) {
    char *operand_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
    if (!operand_buf) {
        if (flags & RENDER_VIA_ERROR_CTX)
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "failed to allocate operand buffer");
        return -1;
    }

    int operand_ret = dispatch(node->data.unary_op.operand, operand_buf, lv_FORMULA_BUF_SIZE, options);
    if (flags & RENDER_VIA_CHECK_RET) {
        if (operand_ret < 0) {
            formula_pool_free(operand_buf);
            if (flags & RENDER_VIA_ERROR_CTX)
                lv_RETURN_ERROR(lv_ERROR_INTERNAL, "operand sub-render failed");
            return -1;
        }
    }

    int written = snprintf(buffer, size, "%s%s%s", prefix, operand_buf, suffix);

    formula_pool_free(operand_buf);
    return written;
}

int dispatch_via(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options,
                 const RenderNodeFunc *table, size_t table_count, RenderNodeFunc fallback) {
    if (!node) {
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "node is NULL");
    }

    if ((unsigned)node->type < table_count && table[node->type]) {
        return table[node->type](node, buffer, size, options);
    }

    if (fallback) {
        return fallback(node, buffer, size, options);
    }
    return snprintf(buffer, size, "<unknown>");
}

const char *formula_render_trig_name(const FormulaNode *node, const char *const *names, size_t count) {
    int idx = node->type - NODE_UNARY_OP_NEG;
    if (idx < 0 || (size_t) idx >= count)
        return NULL;
    return names[idx];
}

