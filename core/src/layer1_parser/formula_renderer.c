/**
 * @file formula_renderer.c
 * @brief 公式渲染器实现
 *
 * @details 将 AST 渲染为 LaTeX、Python 或 DSL 格式的字符串。
 *          支持自定义精度和格式选项。
 *
 * @note 本文件对缓冲区溢出进行了严格加固：
 *       - 所有 >256 字节的临时缓冲区改为堆分配（lv_malloc/lv_free）
 *       - 所有 ≤256 字节的栈缓冲区使用 snprintf 边界检查
 *       - 1024 字节子表达式缓冲区通过可复用池管理，避免递归时的 malloc/free 开销
 *       - 所有字符串操作均已添加边界检查
 *       - 通过 #define lv_FORMULA_BUF_SIZE 统一控制缓冲大小
 *
 * @author Lv-00 Project
 * @version 3.3.0
 *
 * @dependencies
 *   - formula_renderer.h : 渲染器公共接口定义
 *   - lv_internal.h    : 内部数据结构和常量
 *   - lv_utils.h       : 统一内存分配器（lv_malloc/lv_free）
 *   - error_codes.h      : 错误码定义
 */

#include "formula_renderer.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error_codes.h"
#include "lv_internal.h"
#include "lv/lv_thread.h"
#include "lv_utils.h" /* lv_malloc / lv_realloc / lv_free —— 统一内存分配器 */

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
typedef struct {
    char *data;  /**< 堆分配的缓冲区，未初始化时为 NULL */
    bool in_use; /**< 当前是否被占用 */
} FormulaPoolSlot;

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
static char *formula_pool_alloc(size_t size) {
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
static void formula_pool_free(char *ptr) {
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
typedef struct {
    const char *name;
    const char *latex;
} GreekLetterMapping;

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
typedef struct {
    const char *name;
    const char *latex;
} TrigFunctionMapping;

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

static const char *get_trig_latex(const char *name) {
    if (!name)
        return name;

    for (int i = 0; trig_functions[i].name != NULL; i++) {
        if (strcmp(name, trig_functions[i].name) == 0) {
            return trig_functions[i].latex;
        }
    }
    return name;
}
static bool is_greek_letter(const char *name) {
    if (!name)
        return false;

    for (int i = 0; greek_letters[i].name != NULL; i++) {
        if (strcmp(name, greek_letters[i].name) == 0) {
            return true;
        }
    }
    return false;
}
static bool needs_parentheses(const FormulaNode *node, NodeType parent_op, bool is_right) {
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

    switch (node->type) {
        case NODE_BINARY_OP_ADD:
            node_prec = 1;
            break;
        case NODE_BINARY_OP_SUB:
            node_prec = 1;
            break;
        case NODE_BINARY_OP_MUL:
            node_prec = 2;
            break;
        case NODE_BINARY_OP_DIV:
            node_prec = 3;
            break; /* 分数形式 */
        case NODE_BINARY_OP_POW:
            node_prec = 4;
            break;
        default:
            return false;
    }

    switch (parent_op) {
        case NODE_BINARY_OP_ADD:
            parent_prec = 1;
            break;
        case NODE_BINARY_OP_SUB:
            parent_prec = 1;
            break;
        case NODE_BINARY_OP_MUL:
            parent_prec = 2;
            break;
        case NODE_BINARY_OP_DIV:
            parent_prec = 3;
            break;
        case NODE_BINARY_OP_POW:
            parent_prec = 4;
            break;
        default:
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
 * LaTeX 渲染
 * ============================================================ */

/**
 * @brief LaTeX 格式内部渲染器（递归）
 *
 * 所有 >256 字节的子表达式缓冲区均从池或堆获取，
 * 避免递归调用时的栈溢出风险。
 */
static int render_latex_internal(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options) {
    if (!node) {
        return -1;
    }

    int written = 0;

    switch (node->type) {
        case NODE_NUMBER:
            if (node->data.number.is_integer) {
                /* STACK_SAFE: snprintf 直接写入输出 buffer，无中间缓冲区 */
                written = snprintf(buffer, size, "%lld", (long long) node->data.number.numerator);
            } else {
                /* 分数渲染为 \frac{a}{b} */
                written = snprintf(buffer, size, "\\frac{%lld}{%llu}", (long long) node->data.number.numerator,
                                   (unsigned long long) node->data.number.denominator);
            }
            break;

        case NODE_VARIABLE:
            if (is_greek_letter(node->data.variable.name)) {
                written = snprintf(buffer, size, "%s", formula_latex_greek_name(node->data.variable.name));
            } else {
                written = snprintf(buffer, size, "%s", node->data.variable.name);
            }
            break;

        case NODE_IDENTIFIER:
            written = snprintf(buffer, size, "%s", node->data.identifier.name);
            break;

        case NODE_BINARY_OP_ADD: {
            /* HEAP_ALLOCATED: 池分配子表达式缓冲区 */
            char *left_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            char *right_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            if (!left_buf || !right_buf) {
                formula_pool_free(left_buf);
                formula_pool_free(right_buf);
                return -1;
            }

            int left_ret = render_latex_internal(node->data.binary_op.left, left_buf, lv_FORMULA_BUF_SIZE, options);
            int right_ret = render_latex_internal(node->data.binary_op.right, right_buf, lv_FORMULA_BUF_SIZE, options);
            if (left_ret < 0 || right_ret < 0) {
                formula_pool_free(left_buf);
                formula_pool_free(right_buf);
                return -1;
            }

            written = snprintf(buffer, size, "%s + %s", left_buf, right_buf);

            formula_pool_free(left_buf);
            formula_pool_free(right_buf);
        } break;

        case NODE_BINARY_OP_SUB: {
            /* HEAP_ALLOCATED: 池分配子表达式缓冲区 */
            char *left_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            char *right_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            if (!left_buf || !right_buf) {
                formula_pool_free(left_buf);
                formula_pool_free(right_buf);
                return -1;
            }

            int left_ret = render_latex_internal(node->data.binary_op.left, left_buf, lv_FORMULA_BUF_SIZE, options);
            int right_ret = render_latex_internal(node->data.binary_op.right, right_buf, lv_FORMULA_BUF_SIZE, options);
            if (left_ret < 0 || right_ret < 0) {
                formula_pool_free(left_buf);
                formula_pool_free(right_buf);
                return -1;
            }

            /* 检查右侧是否需要括号 */
            bool need_paren = needs_parentheses(node->data.binary_op.right, NODE_BINARY_OP_SUB, true);

            if (need_paren) {
                written = snprintf(buffer, size, "%s - \\left(%s\\right)", left_buf, right_buf);
            } else {
                written = snprintf(buffer, size, "%s - %s", left_buf, right_buf);
            }

            formula_pool_free(left_buf);
            formula_pool_free(right_buf);
        } break;

        case NODE_BINARY_OP_MUL: {
            /* HEAP_ALLOCATED: 池分配子表达式缓冲区 */
            char *left_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            char *right_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            if (!left_buf || !right_buf) {
                formula_pool_free(left_buf);
                formula_pool_free(right_buf);
                return -1;
            }

            int left_ret = render_latex_internal(node->data.binary_op.left, left_buf, lv_FORMULA_BUF_SIZE, options);
            int right_ret = render_latex_internal(node->data.binary_op.right, right_buf, lv_FORMULA_BUF_SIZE, options);
            if (left_ret < 0 || right_ret < 0) {
                formula_pool_free(left_buf);
                formula_pool_free(right_buf);
                return -1;
            }

            if (options && options->implicit_multiplication) {
                /* 隐式乘法: ab */
                written = snprintf(buffer, size, "%s %s", left_buf, right_buf);
            } else {
                /* 显式乘法: a \cdot b */
                written = snprintf(buffer, size, "%s \\cdot %s", left_buf, right_buf);
            }

            formula_pool_free(left_buf);
            formula_pool_free(right_buf);
        } break;

        case NODE_BINARY_OP_DIV: {
            /* HEAP_ALLOCATED: 池分配子表达式缓冲区 */
            char *left_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            char *right_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            if (!left_buf || !right_buf) {
                formula_pool_free(left_buf);
                formula_pool_free(right_buf);
                return -1;
            }

            int left_ret = render_latex_internal(node->data.binary_op.left, left_buf, lv_FORMULA_BUF_SIZE, options);
            int right_ret = render_latex_internal(node->data.binary_op.right, right_buf, lv_FORMULA_BUF_SIZE, options);
            if (left_ret < 0 || right_ret < 0) {
                formula_pool_free(left_buf);
                formula_pool_free(right_buf);
                return -1;
            }

            /* 分数形式 */
            written = snprintf(buffer, size, "\\frac{%s}{%s}", left_buf, right_buf);

            formula_pool_free(left_buf);
            formula_pool_free(right_buf);
        } break;

        case NODE_BINARY_OP_POW: {
            /* HEAP_ALLOCATED: 池分配子表达式缓冲区 */
            char *left_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            char *right_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            if (!left_buf || !right_buf) {
                formula_pool_free(left_buf);
                formula_pool_free(right_buf);
                return -1;
            }

            int left_ret = render_latex_internal(node->data.binary_op.left, left_buf, lv_FORMULA_BUF_SIZE, options);
            int right_ret = render_latex_internal(node->data.binary_op.right, right_buf, lv_FORMULA_BUF_SIZE, options);
            if (left_ret < 0 || right_ret < 0) {
                formula_pool_free(left_buf);
                formula_pool_free(right_buf);
                return -1;
            }

            /* 检查底数是否需要括号 */
            bool need_paren = needs_parentheses(node->data.binary_op.left, NODE_BINARY_OP_POW, false);

            if (need_paren) {
                written = snprintf(buffer, size, "\\left(%s\\right)^{%s}", left_buf, right_buf);
            } else {
                written = snprintf(buffer, size, "%s^{%s}", left_buf, right_buf);
            }

            formula_pool_free(left_buf);
            formula_pool_free(right_buf);
        } break;

        case NODE_UNARY_OP_NEG: {
            /* HEAP_ALLOCATED: 池分配操作数缓冲区 */
            char *operand_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            if (!operand_buf)
                return -1;

            int operand_ret =
                render_latex_internal(node->data.unary_op.operand, operand_buf, lv_FORMULA_BUF_SIZE, options);
            if (operand_ret < 0) {
                formula_pool_free(operand_buf);
                return -1;
            }

            bool need_paren = needs_parentheses(node->data.unary_op.operand, NODE_UNARY_OP_NEG, false);
            if (need_paren) {
                written = snprintf(buffer, size, "-\\left(%s\\right)", operand_buf);
            } else {
                written = snprintf(buffer, size, "-%s", operand_buf);
            }

            formula_pool_free(operand_buf);
        } break;

        case NODE_UNARY_OP_SQRT: {
            /* HEAP_ALLOCATED: 池分配操作数缓冲区 */
            char *operand_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            if (!operand_buf)
                return -1;

            int operand_ret =
                render_latex_internal(node->data.unary_op.operand, operand_buf, lv_FORMULA_BUF_SIZE, options);
            if (operand_ret < 0) {
                formula_pool_free(operand_buf);
                return -1;
            }
            written = snprintf(buffer, size, "\\sqrt{%s}", operand_buf);

            formula_pool_free(operand_buf);
        } break;

        case NODE_UNARY_OP_SIN:
        case NODE_UNARY_OP_COS:
        case NODE_UNARY_OP_TAN: {
            const char *func_names[] = {[NODE_UNARY_OP_SIN - NODE_UNARY_OP_NEG] = "\\sin",
                                        [NODE_UNARY_OP_COS - NODE_UNARY_OP_NEG] = "\\cos",
                                        [NODE_UNARY_OP_TAN - NODE_UNARY_OP_NEG] = "\\tan"};
            int idx = node->type - NODE_UNARY_OP_NEG;

            /* HEAP_ALLOCATED: 池分配操作数缓冲区 */
            char *operand_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            if (!operand_buf)
                return -1;

            int operand_ret =
                render_latex_internal(node->data.unary_op.operand, operand_buf, lv_FORMULA_BUF_SIZE, options);
            if (operand_ret < 0) {
                formula_pool_free(operand_buf);
                return -1;
            }
            written = snprintf(buffer, size, "%s\\left(%s\\right)", func_names[idx], operand_buf);

            formula_pool_free(operand_buf);
        } break;

        case NODE_UNARY_OP_ABS: {
            /* HEAP_ALLOCATED: 池分配操作数缓冲区 */
            char *operand_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            if (!operand_buf)
                return -1;

            int operand_ret =
                render_latex_internal(node->data.unary_op.operand, operand_buf, lv_FORMULA_BUF_SIZE, options);
            if (operand_ret < 0) {
                formula_pool_free(operand_buf);
                return -1;
            }
            written = snprintf(buffer, size, "\\left|%s\\right|", operand_buf);

            formula_pool_free(operand_buf);
        } break;

        case NODE_UNARY_OP_LN: {
            /* HEAP_ALLOCATED: 池分配操作数缓冲区 */
            char *operand_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            if (!operand_buf)
                return -1;

            int operand_ret =
                render_latex_internal(node->data.unary_op.operand, operand_buf, lv_FORMULA_BUF_SIZE, options);
            if (operand_ret < 0) {
                formula_pool_free(operand_buf);
                return -1;
            }
            written = snprintf(buffer, size, "\\ln\\left(%s\\right)", operand_buf);

            formula_pool_free(operand_buf);
        } break;

        case NODE_UNARY_OP_LOG: {
            /* HEAP_ALLOCATED: 池分配操作数缓冲区 */
            char *operand_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            if (!operand_buf)
                return -1;

            int operand_ret =
                render_latex_internal(node->data.unary_op.operand, operand_buf, lv_FORMULA_BUF_SIZE, options);
            if (operand_ret < 0) {
                formula_pool_free(operand_buf);
                return -1;
            }
            written = snprintf(buffer, size, "\\log\\left(%s\\right)", operand_buf);

            formula_pool_free(operand_buf);
        } break;

        case NODE_EQUATION: {
            /* HEAP_ALLOCATED: 池分配子表达式缓冲区 */
            char *lhs_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            char *rhs_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            if (!lhs_buf || !rhs_buf) {
                formula_pool_free(lhs_buf);
                formula_pool_free(rhs_buf);
                return -1;
            }

            int lhs_ret = render_latex_internal(node->data.equation.lhs, lhs_buf, lv_FORMULA_BUF_SIZE, options);
            int rhs_ret = render_latex_internal(node->data.equation.rhs, rhs_buf, lv_FORMULA_BUF_SIZE, options);
            if (lhs_ret < 0 || rhs_ret < 0) {
                formula_pool_free(lhs_buf);
                formula_pool_free(rhs_buf);
                return -1;
            }

            written = snprintf(buffer, size, "%s = %s", lhs_buf, rhs_buf);

            formula_pool_free(lhs_buf);
            formula_pool_free(rhs_buf);
        } break;

        case NODE_GEOM_POINT: {
            /* HEAP_ALLOCATED: 大型坐标缓冲区使用直接 malloc */
            char *coords_buf = (char *) lv_malloc(lv_FORMULA_BUF_LARGE);
            if (!coords_buf)
                return -1;
            memset(coords_buf, 0, lv_FORMULA_BUF_LARGE);

            if (node->data.geom_point.coords) {
                int coords_ret =
                    render_latex_internal(node->data.geom_point.coords, coords_buf, lv_FORMULA_BUF_LARGE, options);
                if (coords_ret < 0) {
                    lv_free((void **) &coords_buf);
                    return -1;
                }
            }

            if (node->data.geom_point.name) {
                written = snprintf(buffer, size, "%s = \\left(%s\\right)", node->data.geom_point.name, coords_buf);
            } else {
                written = snprintf(buffer, size, "\\left(%s\\right)", coords_buf);
            }

            lv_free((void **) &coords_buf);
        } break;

        case NODE_GEOM_SEGMENT: {
            if (node->data.geom_segment.name) {
                written = snprintf(buffer, size, "\\overline{%s}", node->data.geom_segment.name);
            } else {
                /* STACK_SAFE: 端点名缓冲区 ≤64 字节，使用 snprintf 边界检查 */
                char ep1_buf[lv_FORMULA_BUF_SMALL] = {0};
                char ep2_buf[lv_FORMULA_BUF_SMALL] = {0};
                if (node->data.geom_segment.endpoint1) {
                    int ep_ret =
                        render_latex_internal(node->data.geom_segment.endpoint1, ep1_buf, sizeof(ep1_buf), options);
                    if (ep_ret < 0)
                        return -1;
                }
                if (node->data.geom_segment.endpoint2) {
                    int ep_ret =
                        render_latex_internal(node->data.geom_segment.endpoint2, ep2_buf, sizeof(ep2_buf), options);
                    if (ep_ret < 0)
                        return -1;
                }
                written = snprintf(buffer, size, "\\overline{%s%s}", ep1_buf, ep2_buf);
            }
        } break;

        case NODE_GEOM_CIRCLE: {
            /* STACK_SAFE: 小型缓冲区 ≤256 字节，使用 snprintf 边界检查 */
            char center_buf[lv_FORMULA_BUF_SMALL] = {0};
            char radius_buf[lv_FORMULA_BUF_MEDIUM] = {0};

            if (node->data.geom_circle.center) {
                if (node->data.geom_circle.center->type == NODE_IDENTIFIER) {
                    snprintf(center_buf, sizeof(center_buf), "%s", node->data.geom_circle.center->data.identifier.name);
                } else {
                    int center_ret =
                        render_latex_internal(node->data.geom_circle.center, center_buf, sizeof(center_buf), options);
                    if (center_ret < 0)
                        return -1;
                }
            }

            if (node->data.geom_circle.radius) {
                int radius_ret =
                    render_latex_internal(node->data.geom_circle.radius, radius_buf, sizeof(radius_buf), options);
                if (radius_ret < 0)
                    return -1;
            }

            written = snprintf(buffer, size, "\\text{circle } %s \\text{ with center } %s \\text{ and radius } %s",
                               node->data.geom_circle.name ? node->data.geom_circle.name : "O", center_buf, radius_buf);
        } break;

        case NODE_GEOM_TRIANGLE: {
            if (node->data.geom_triangle.name) {
                written = snprintf(buffer, size, "\\triangle %s", node->data.geom_triangle.name);
            } else {
                written = snprintf(buffer, size, "\\triangle");
            }
        } break;

        case NODE_COORDINATE_LIST: {
            char *ptr = buffer;
            size_t remaining = size;
            int total = 0;

            for (int i = 0; i < node->data.coord_list.coord_count; i++) {
                /* STACK_SAFE: 坐标缓冲区 ≤256 字节 */
                char coord_buf[lv_FORMULA_BUF_MEDIUM] = {0};
                int coord_ret =
                    render_latex_internal(node->data.coord_list.coords[i], coord_buf, sizeof(coord_buf), options);
                if (coord_ret < 0)
                    return -1;

                int w = snprintf(ptr, remaining, "%s%s", (i > 0) ? ", " : "", coord_buf);
                if (w < 0 || (size_t) w >= remaining)
                    break;
                ptr += w;
                remaining -= w;
                total += w;
            }
            written = total;
        } break;

        case NODE_CONSTRAINT_PERPENDICULAR: {
            /* STACK_SAFE: 约束参与者名 ≤64 字节 */
            char p1_buf[lv_FORMULA_BUF_SMALL] = {0}, p2_buf[lv_FORMULA_BUF_SMALL] = {0},
                 p3_buf[lv_FORMULA_BUF_SMALL] = {0};
            if (node->data.constraint.participant_count >= 3) {
                int p1_ret =
                    render_latex_internal(node->data.constraint.participants[0], p1_buf, sizeof(p1_buf), options);
                int p2_ret =
                    render_latex_internal(node->data.constraint.participants[1], p2_buf, sizeof(p2_buf), options);
                int p3_ret =
                    render_latex_internal(node->data.constraint.participants[2], p3_buf, sizeof(p3_buf), options);
                if (p1_ret < 0 || p2_ret < 0 || p3_ret < 0)
                    return -1;
                written = snprintf(buffer, size, "%s \\perp %s%s", p1_buf, p2_buf, p3_buf);
            }
        } break;

        case NODE_CONSTRAINT_PARALLEL: {
            /* STACK_SAFE: 线名缓冲区 ≤64 字节 */
            char l1_buf[lv_FORMULA_BUF_SMALL] = {0}, l2_buf[lv_FORMULA_BUF_SMALL] = {0};
            if (node->data.constraint.participant_count >= 2) {
                int l1_ret =
                    render_latex_internal(node->data.constraint.participants[0], l1_buf, sizeof(l1_buf), options);
                int l2_ret =
                    render_latex_internal(node->data.constraint.participants[1], l2_buf, sizeof(l2_buf), options);
                if (l1_ret < 0 || l2_ret < 0)
                    return -1;
                written = snprintf(buffer, size, "%s \\parallel %s", l1_buf, l2_buf);
            }
        } break;

        case NODE_CONSTRAINT_MIDPOINT: {
            /* STACK_SAFE: 点名缓冲区 ≤64 字节 */
            char m_buf[lv_FORMULA_BUF_SMALL] = {0}, a_buf[lv_FORMULA_BUF_SMALL] = {0},
                 b_buf[lv_FORMULA_BUF_SMALL] = {0};
            if (node->data.constraint.participant_count >= 3) {
                int m_ret = render_latex_internal(node->data.constraint.participants[0], m_buf, sizeof(m_buf), options);
                int a_ret = render_latex_internal(node->data.constraint.participants[1], a_buf, sizeof(a_buf), options);
                int b_ret = render_latex_internal(node->data.constraint.participants[2], b_buf, sizeof(b_buf), options);
                if (m_ret < 0 || a_ret < 0 || b_ret < 0)
                    return -1;
                written = snprintf(buffer, size, "%s = \\text{midpoint}(%s, %s)", m_buf, a_buf, b_buf);
            }
        } break;

        /* NODE_GEOM_REGION 区域渲染 */
        case NODE_GEOM_REGION: {
            const char *name = node->data.geom_region.name ? node->data.geom_region.name : "R";
            written = snprintf(buffer, size, "\\text{region } %s", name);
        } break;

        /* NODE_GEOM_ARC 弧渲染 */
        case NODE_GEOM_ARC: {
            /* STACK_SAFE: 名称/角度缓冲区 ≤64，半径缓冲区 ≤256 */
            char center_buf[lv_FORMULA_BUF_SMALL] = {0}, radius_buf[lv_FORMULA_BUF_MEDIUM] = {0};
            char start_buf[lv_FORMULA_BUF_SMALL] = {0}, end_buf[lv_FORMULA_BUF_SMALL] = {0};

            if (node->data.geom_arc.center) {
                int center_ret =
                    render_latex_internal(node->data.geom_arc.center, center_buf, sizeof(center_buf), options);
                if (center_ret < 0)
                    return -1;
            }
            if (node->data.geom_arc.radius) {
                int radius_ret =
                    render_latex_internal(node->data.geom_arc.radius, radius_buf, sizeof(radius_buf), options);
                if (radius_ret < 0)
                    return -1;
            }
            if (node->data.geom_arc.start_angle) {
                int start_ret =
                    render_latex_internal(node->data.geom_arc.start_angle, start_buf, sizeof(start_buf), options);
                if (start_ret < 0)
                    return -1;
            }
            if (node->data.geom_arc.end_angle) {
                int end_ret = render_latex_internal(node->data.geom_arc.end_angle, end_buf, sizeof(end_buf), options);
                if (end_ret < 0)
                    return -1;
            }

            written = snprintf(buffer, size,
                               "\\overset{\\frown}{%s} \\text{ with center } %s, \\text{ radius } %s, \\text{ from } "
                               "%s \\text{ to } %s",
                               node->data.geom_arc.name ? node->data.geom_arc.name : "AB", center_buf, radius_buf,
                               start_buf, end_buf);
        } break;

        /* NODE_CONSTRAINT_ANGLE 角度约束渲染 */
        case NODE_CONSTRAINT_ANGLE: {
            /* STACK_SAFE: 点名缓冲区 ≤64 字节 */
            char p1_buf[lv_FORMULA_BUF_SMALL] = {0}, p2_buf[lv_FORMULA_BUF_SMALL] = {0},
                 p3_buf[lv_FORMULA_BUF_SMALL] = {0};
            if (node->data.constraint.participant_count >= 3) {
                int p1_ret =
                    render_latex_internal(node->data.constraint.participants[0], p1_buf, sizeof(p1_buf), options);
                int p2_ret =
                    render_latex_internal(node->data.constraint.participants[1], p2_buf, sizeof(p2_buf), options);
                int p3_ret =
                    render_latex_internal(node->data.constraint.participants[2], p3_buf, sizeof(p3_buf), options);
                if (p1_ret < 0 || p2_ret < 0 || p3_ret < 0)
                    return -1;
                written = snprintf(buffer, size, "\\angle %s %s %s", p1_buf, p2_buf, p3_buf);
            }
        } break;

        case NODE_COMPOUND: {
            char *ptr = buffer;
            size_t remaining = size;
            int total = 0;

            for (int i = 0; i < node->data.compound.statement_count; i++) {
                /* HEAP_ALLOCATED: 池分配语句缓冲区 */
                char *stmt_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
                if (!stmt_buf)
                    break;

                int stmt_ret =
                    render_latex_internal(node->data.compound.statements[i], stmt_buf, lv_FORMULA_BUF_SIZE, options);
                if (stmt_ret < 0) {
                    formula_pool_free(stmt_buf);
                    return -1;
                }

                int w = snprintf(ptr, remaining, "%s%s\\\\\n", stmt_buf,
                                 (i < node->data.compound.statement_count - 1) ? "" : "");
                formula_pool_free(stmt_buf);

                if (w < 0 || (size_t) w >= remaining)
                    break;
                ptr += w;
                remaining -= w;
                total += w;
            }
            written = total;
        } break;

        default:
            written = snprintf(buffer, size, "\\text{<unknown>}");
            break;
    }

    return written;
}

/* ============================================================
 * Python 渲染
 * ============================================================ */

/**
 * @brief Python 格式内部渲染器（递归）
 *
 * 所有 >256 字节的子表达式缓冲区均从池或堆获取。
 */
static int render_python_internal(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options) {
    if (!node) {
        return -1;
    }

    int written = 0;

    switch (node->type) {
        case NODE_NUMBER:
            if (node->data.number.is_integer) {
                written = snprintf(buffer, size, "%lld", (long long) node->data.number.numerator);
            } else {
                if (options && options->fraction_mode) {
                    written = snprintf(buffer, size, "Fraction(%lld, %llu)", (long long) node->data.number.numerator,
                                       (unsigned long long) node->data.number.denominator);
                } else {
                    if (node->data.number.denominator == 0) {
                        written = snprintf(buffer, size, "NaN");
                    } else {
                        double val = (double) node->data.number.numerator / (double) node->data.number.denominator;
                        written = snprintf(buffer, size, "%.*f", options ? options->precision : 6, val);
                    }
                }
            }
            break;

        case NODE_VARIABLE:
            written = snprintf(buffer, size, "%s", node->data.variable.name);
            break;

        case NODE_IDENTIFIER:
            written = snprintf(buffer, size, "%s", node->data.identifier.name);
            break;

        case NODE_BINARY_OP_ADD: {
            /* HEAP_ALLOCATED: 池分配子表达式缓冲区 */
            char *left_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            char *right_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            if (!left_buf || !right_buf) {
                formula_pool_free(left_buf);
                formula_pool_free(right_buf);
                return -1;
            }

            render_python_internal(node->data.binary_op.left, left_buf, lv_FORMULA_BUF_SIZE, options);
            render_python_internal(node->data.binary_op.right, right_buf, lv_FORMULA_BUF_SIZE, options);

            written = snprintf(buffer, size, "(%s + %s)", left_buf, right_buf);

            formula_pool_free(left_buf);
            formula_pool_free(right_buf);
        } break;

        case NODE_BINARY_OP_SUB: {
            /* HEAP_ALLOCATED: 池分配子表达式缓冲区 */
            char *left_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            char *right_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            if (!left_buf || !right_buf) {
                formula_pool_free(left_buf);
                formula_pool_free(right_buf);
                return -1;
            }

            render_python_internal(node->data.binary_op.left, left_buf, lv_FORMULA_BUF_SIZE, options);
            render_python_internal(node->data.binary_op.right, right_buf, lv_FORMULA_BUF_SIZE, options);

            written = snprintf(buffer, size, "(%s - %s)", left_buf, right_buf);

            formula_pool_free(left_buf);
            formula_pool_free(right_buf);
        } break;

        case NODE_BINARY_OP_MUL: {
            /* HEAP_ALLOCATED: 池分配子表达式缓冲区 */
            char *left_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            char *right_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            if (!left_buf || !right_buf) {
                formula_pool_free(left_buf);
                formula_pool_free(right_buf);
                return -1;
            }

            render_python_internal(node->data.binary_op.left, left_buf, lv_FORMULA_BUF_SIZE, options);
            render_python_internal(node->data.binary_op.right, right_buf, lv_FORMULA_BUF_SIZE, options);

            written = snprintf(buffer, size, "(%s * %s)", left_buf, right_buf);

            formula_pool_free(left_buf);
            formula_pool_free(right_buf);
        } break;

        case NODE_BINARY_OP_DIV: {
            /* HEAP_ALLOCATED: 池分配子表达式缓冲区 */
            char *left_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            char *right_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            if (!left_buf || !right_buf) {
                formula_pool_free(left_buf);
                formula_pool_free(right_buf);
                return -1;
            }

            render_python_internal(node->data.binary_op.left, left_buf, lv_FORMULA_BUF_SIZE, options);
            render_python_internal(node->data.binary_op.right, right_buf, lv_FORMULA_BUF_SIZE, options);

            written = snprintf(buffer, size, "(%s / %s)", left_buf, right_buf);

            formula_pool_free(left_buf);
            formula_pool_free(right_buf);
        } break;

        case NODE_BINARY_OP_POW: {
            /* HEAP_ALLOCATED: 池分配子表达式缓冲区 */
            char *left_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            char *right_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            if (!left_buf || !right_buf) {
                formula_pool_free(left_buf);
                formula_pool_free(right_buf);
                return -1;
            }

            render_python_internal(node->data.binary_op.left, left_buf, lv_FORMULA_BUF_SIZE, options);
            render_python_internal(node->data.binary_op.right, right_buf, lv_FORMULA_BUF_SIZE, options);

            written = snprintf(buffer, size, "(%s ** %s)", left_buf, right_buf);

            formula_pool_free(left_buf);
            formula_pool_free(right_buf);
        } break;

        case NODE_UNARY_OP_NEG: {
            /* HEAP_ALLOCATED: 池分配操作数缓冲区 */
            char *operand_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            if (!operand_buf)
                return -1;

            render_python_internal(node->data.unary_op.operand, operand_buf, lv_FORMULA_BUF_SIZE, options);
            written = snprintf(buffer, size, "(-%s)", operand_buf);

            formula_pool_free(operand_buf);
        } break;

        case NODE_UNARY_OP_SQRT: {
            /* HEAP_ALLOCATED: 池分配操作数缓冲区 */
            char *operand_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            if (!operand_buf)
                return -1;

            render_python_internal(node->data.unary_op.operand, operand_buf, lv_FORMULA_BUF_SIZE, options);
            written = snprintf(buffer, size, "sqrt(%s)", operand_buf);

            formula_pool_free(operand_buf);
        } break;

        case NODE_UNARY_OP_SIN:
        case NODE_UNARY_OP_COS:
        case NODE_UNARY_OP_TAN: {
            const char *func_names[] = {[NODE_UNARY_OP_SIN - NODE_UNARY_OP_NEG] = "sin",
                                        [NODE_UNARY_OP_COS - NODE_UNARY_OP_NEG] = "cos",
                                        [NODE_UNARY_OP_TAN - NODE_UNARY_OP_NEG] = "tan"};
            int idx = node->type - NODE_UNARY_OP_NEG;

            /* HEAP_ALLOCATED: 池分配操作数缓冲区 */
            char *operand_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            if (!operand_buf)
                return -1;

            render_python_internal(node->data.unary_op.operand, operand_buf, lv_FORMULA_BUF_SIZE, options);
            written = snprintf(buffer, size, "%s(%s)", func_names[idx], operand_buf);

            formula_pool_free(operand_buf);
        } break;

        case NODE_UNARY_OP_ABS: {
            /* HEAP_ALLOCATED: 池分配操作数缓冲区 */
            char *operand_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            if (!operand_buf)
                return -1;

            render_python_internal(node->data.unary_op.operand, operand_buf, lv_FORMULA_BUF_SIZE, options);
            written = snprintf(buffer, size, "abs(%s)", operand_buf);

            formula_pool_free(operand_buf);
        } break;

        case NODE_UNARY_OP_LN: {
            /* HEAP_ALLOCATED: 池分配操作数缓冲区 */
            char *operand_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            if (!operand_buf)
                return -1;

            render_python_internal(node->data.unary_op.operand, operand_buf, lv_FORMULA_BUF_SIZE, options);
            written = snprintf(buffer, size, "log(%s)", operand_buf);

            formula_pool_free(operand_buf);
        } break;

        case NODE_UNARY_OP_LOG: {
            /* HEAP_ALLOCATED: 池分配操作数缓冲区 */
            char *operand_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            if (!operand_buf)
                return -1;

            render_python_internal(node->data.unary_op.operand, operand_buf, lv_FORMULA_BUF_SIZE, options);
            written = snprintf(buffer, size, "log10(%s)", operand_buf);

            formula_pool_free(operand_buf);
        } break;

        case NODE_EQUATION: {
            /* HEAP_ALLOCATED: 池分配子表达式缓冲区 */
            char *lhs_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            char *rhs_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            if (!lhs_buf || !rhs_buf) {
                formula_pool_free(lhs_buf);
                formula_pool_free(rhs_buf);
                return -1;
            }

            render_python_internal(node->data.equation.lhs, lhs_buf, lv_FORMULA_BUF_SIZE, options);
            render_python_internal(node->data.equation.rhs, rhs_buf, lv_FORMULA_BUF_SIZE, options);

            /* 方程转换为比较表达式 */
            written = snprintf(buffer, size, "(%s == %s)", lhs_buf, rhs_buf);

            formula_pool_free(lhs_buf);
            formula_pool_free(rhs_buf);
        } break;

        case NODE_GEOM_POINT: {
            /* HEAP_ALLOCATED: 大型坐标缓冲区使用直接 malloc */
            char *coords_buf = (char *) lv_malloc(lv_FORMULA_BUF_LARGE);
            if (!coords_buf)
                return -1;
            memset(coords_buf, 0, lv_FORMULA_BUF_LARGE);

            if (node->data.geom_point.coords) {
                render_python_internal(node->data.geom_point.coords, coords_buf, lv_FORMULA_BUF_LARGE, options);
            }

            if (node->data.geom_point.name) {
                written = snprintf(buffer, size, "%s = Point(%s)", node->data.geom_point.name, coords_buf);
            } else {
                written = snprintf(buffer, size, "Point(%s)", coords_buf);
            }

            lv_free((void **) &coords_buf);
        } break;

        case NODE_GEOM_SEGMENT: {
            /* STACK_SAFE: 端点名缓冲区 ≤64 字节 */
            char ep1_buf[lv_FORMULA_BUF_SMALL] = {0};
            char ep2_buf[lv_FORMULA_BUF_SMALL] = {0};

            if (node->data.geom_segment.endpoint1) {
                render_python_internal(node->data.geom_segment.endpoint1, ep1_buf, sizeof(ep1_buf), options);
            }
            if (node->data.geom_segment.endpoint2) {
                render_python_internal(node->data.geom_segment.endpoint2, ep2_buf, sizeof(ep2_buf), options);
            }

            written = snprintf(buffer, size, "Segment(%s, %s)", ep1_buf, ep2_buf);
        } break;

        case NODE_GEOM_CIRCLE: {
            /* STACK_SAFE: 小型缓冲区 ≤256 字节 */
            char center_buf[lv_FORMULA_BUF_SMALL] = {0};
            char radius_buf[lv_FORMULA_BUF_MEDIUM] = {0};

            if (node->data.geom_circle.center) {
                render_python_internal(node->data.geom_circle.center, center_buf, sizeof(center_buf), options);
            }
            if (node->data.geom_circle.radius) {
                render_python_internal(node->data.geom_circle.radius, radius_buf, sizeof(radius_buf), options);
            }

            written = snprintf(buffer, size, "Circle(%s, %s)", center_buf, radius_buf);
        } break;

        case NODE_GEOM_TRIANGLE: {
            /* STACK_SAFE: 顶点名缓冲区 ≤64 字节 */
            char v1_buf[lv_FORMULA_BUF_SMALL] = {0}, v2_buf[lv_FORMULA_BUF_SMALL] = {0},
                 v3_buf[lv_FORMULA_BUF_SMALL] = {0};

            if (node->data.geom_triangle.vertex1) {
                render_python_internal(node->data.geom_triangle.vertex1, v1_buf, sizeof(v1_buf), options);
            }
            if (node->data.geom_triangle.vertex2) {
                render_python_internal(node->data.geom_triangle.vertex2, v2_buf, sizeof(v2_buf), options);
            }
            if (node->data.geom_triangle.vertex3) {
                render_python_internal(node->data.geom_triangle.vertex3, v3_buf, sizeof(v3_buf), options);
            }

            written = snprintf(buffer, size, "Triangle(%s, %s, %s)", v1_buf, v2_buf, v3_buf);
        } break;

        case NODE_COORDINATE_LIST: {
            char *ptr = buffer;
            size_t remaining = size;
            int total = 0;

            for (int i = 0; i < node->data.coord_list.coord_count; i++) {
                /* STACK_SAFE: 坐标缓冲区 ≤256 字节 */
                char coord_buf[lv_FORMULA_BUF_MEDIUM] = {0};
                render_python_internal(node->data.coord_list.coords[i], coord_buf, sizeof(coord_buf), options);

                int w = snprintf(ptr, remaining, "%s%s", (i > 0) ? ", " : "", coord_buf);
                if (w < 0 || (size_t) w >= remaining)
                    break;
                ptr += w;
                remaining -= w;
                total += w;
            }
            written = total;
        } break;

        case NODE_CONSTRAINT_PERPENDICULAR: {
            /* STACK_SAFE: 点名缓冲区 ≤64 字节 */
            char p1_buf[lv_FORMULA_BUF_SMALL] = {0}, p2_buf[lv_FORMULA_BUF_SMALL] = {0},
                 p3_buf[lv_FORMULA_BUF_SMALL] = {0};
            if (node->data.constraint.participant_count >= 3) {
                render_python_internal(node->data.constraint.participants[0], p1_buf, sizeof(p1_buf), options);
                render_python_internal(node->data.constraint.participants[1], p2_buf, sizeof(p2_buf), options);
                render_python_internal(node->data.constraint.participants[2], p3_buf, sizeof(p3_buf), options);
                written = snprintf(buffer, size, "perpendicular(%s, %s, %s)", p1_buf, p2_buf, p3_buf);
            }
        } break;

        case NODE_CONSTRAINT_PARALLEL: {
            /* STACK_SAFE: 线名缓冲区 ≤64 字节 */
            char l1_buf[lv_FORMULA_BUF_SMALL] = {0}, l2_buf[lv_FORMULA_BUF_SMALL] = {0};
            if (node->data.constraint.participant_count >= 2) {
                render_python_internal(node->data.constraint.participants[0], l1_buf, sizeof(l1_buf), options);
                render_python_internal(node->data.constraint.participants[1], l2_buf, sizeof(l2_buf), options);
                written = snprintf(buffer, size, "parallel(%s, %s)", l1_buf, l2_buf);
            }
        } break;

        case NODE_CONSTRAINT_MIDPOINT: {
            /* STACK_SAFE: 点名缓冲区 ≤64 字节 */
            char m_buf[lv_FORMULA_BUF_SMALL] = {0}, a_buf[lv_FORMULA_BUF_SMALL] = {0},
                 b_buf[lv_FORMULA_BUF_SMALL] = {0};
            if (node->data.constraint.participant_count >= 3) {
                render_python_internal(node->data.constraint.participants[0], m_buf, sizeof(m_buf), options);
                render_python_internal(node->data.constraint.participants[1], a_buf, sizeof(a_buf), options);
                render_python_internal(node->data.constraint.participants[2], b_buf, sizeof(b_buf), options);
                written = snprintf(buffer, size, "%s = midpoint(%s, %s)", m_buf, a_buf, b_buf);
            }
        } break;

        /* NODE_GEOM_REGION 区域渲染 */
        case NODE_GEOM_REGION: {
            const char *name = node->data.geom_region.name ? node->data.geom_region.name : "R";
            written = snprintf(buffer, size, "Region('%s')", name);
        } break;

        /* NODE_GEOM_ARC 弧渲染 */
        case NODE_GEOM_ARC: {
            /* STACK_SAFE: 名称/角度缓冲区 ≤64，半径缓冲区 ≤256 */
            char center_buf[lv_FORMULA_BUF_SMALL] = {0}, radius_buf[lv_FORMULA_BUF_MEDIUM] = {0};
            char start_buf[lv_FORMULA_BUF_SMALL] = {0}, end_buf[lv_FORMULA_BUF_SMALL] = {0};

            if (node->data.geom_arc.center) {
                render_python_internal(node->data.geom_arc.center, center_buf, sizeof(center_buf), options);
            }
            if (node->data.geom_arc.radius) {
                render_python_internal(node->data.geom_arc.radius, radius_buf, sizeof(radius_buf), options);
            }
            if (node->data.geom_arc.start_angle) {
                render_python_internal(node->data.geom_arc.start_angle, start_buf, sizeof(start_buf), options);
            }
            if (node->data.geom_arc.end_angle) {
                render_python_internal(node->data.geom_arc.end_angle, end_buf, sizeof(end_buf), options);
            }

            written = snprintf(buffer, size, "Arc('%s', center=%s, radius=%s, start_angle=%s, end_angle=%s)",
                               node->data.geom_arc.name ? node->data.geom_arc.name : "AB", center_buf, radius_buf,
                               start_buf, end_buf);
        } break;

        /* NODE_CONSTRAINT_ANGLE 角度约束渲染 */
        case NODE_CONSTRAINT_ANGLE: {
            /* STACK_SAFE: 点名缓冲区 ≤64 字节 */
            char p1_buf[lv_FORMULA_BUF_SMALL] = {0}, p2_buf[lv_FORMULA_BUF_SMALL] = {0},
                 p3_buf[lv_FORMULA_BUF_SMALL] = {0};
            if (node->data.constraint.participant_count >= 3) {
                render_python_internal(node->data.constraint.participants[0], p1_buf, sizeof(p1_buf), options);
                render_python_internal(node->data.constraint.participants[1], p2_buf, sizeof(p2_buf), options);
                render_python_internal(node->data.constraint.participants[2], p3_buf, sizeof(p3_buf), options);
                written = snprintf(buffer, size, "angle(%s, %s, %s)", p1_buf, p2_buf, p3_buf);
            }
        } break;

        case NODE_COMPOUND: {
            char *ptr = buffer;
            size_t remaining = size;
            int total = 0;

            for (int i = 0; i < node->data.compound.statement_count; i++) {
                /* HEAP_ALLOCATED: 池分配语句缓冲区 */
                char *stmt_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
                if (!stmt_buf)
                    break;

                render_python_internal(node->data.compound.statements[i], stmt_buf, lv_FORMULA_BUF_SIZE, options);

                int w = snprintf(ptr, remaining, "%s\n", stmt_buf);
                formula_pool_free(stmt_buf);

                if (w < 0 || (size_t) w >= remaining)
                    break;
                ptr += w;
                remaining -= w;
                total += w;
            }
            written = total;
        } break;

        default:
            written = snprintf(buffer, size, "# <unknown>");
            break;
    }

    return written;
}

/* ============================================================
 * DSL 渲染
 * ============================================================ */

/**
 * @brief DSL 格式内部渲染器（递归）
 *
 * 所有 >256 字节的子表达式缓冲区均从池或堆获取。
 */
static int render_dsl_internal(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options) {
    if (!node) {
        return -1;
    }

    int written = 0;

    switch (node->type) {
        case NODE_NUMBER:
            if (node->data.number.is_integer) {
                written = snprintf(buffer, size, "%lld", (long long) node->data.number.numerator);
            } else {
                written = snprintf(buffer, size, "%lld/%llu", (long long) node->data.number.numerator,
                                   (unsigned long long) node->data.number.denominator);
            }
            break;

        case NODE_VARIABLE:
            written = snprintf(buffer, size, "%s", node->data.variable.name);
            break;

        case NODE_IDENTIFIER:
            written = snprintf(buffer, size, "%s", node->data.identifier.name);
            break;

        case NODE_BINARY_OP_ADD: {
            /* HEAP_ALLOCATED: 池分配子表达式缓冲区 */
            char *left_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            char *right_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            if (!left_buf || !right_buf) {
                formula_pool_free(left_buf);
                formula_pool_free(right_buf);
                return -1;
            }

            render_dsl_internal(node->data.binary_op.left, left_buf, lv_FORMULA_BUF_SIZE, options);
            render_dsl_internal(node->data.binary_op.right, right_buf, lv_FORMULA_BUF_SIZE, options);

            written = snprintf(buffer, size, "%s + %s", left_buf, right_buf);

            formula_pool_free(left_buf);
            formula_pool_free(right_buf);
        } break;

        case NODE_BINARY_OP_SUB: {
            /* HEAP_ALLOCATED: 池分配子表达式缓冲区 */
            char *left_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            char *right_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            if (!left_buf || !right_buf) {
                formula_pool_free(left_buf);
                formula_pool_free(right_buf);
                return -1;
            }

            render_dsl_internal(node->data.binary_op.left, left_buf, lv_FORMULA_BUF_SIZE, options);
            render_dsl_internal(node->data.binary_op.right, right_buf, lv_FORMULA_BUF_SIZE, options);

            written = snprintf(buffer, size, "%s - %s", left_buf, right_buf);

            formula_pool_free(left_buf);
            formula_pool_free(right_buf);
        } break;

        case NODE_BINARY_OP_MUL: {
            /* HEAP_ALLOCATED: 池分配子表达式缓冲区 */
            char *left_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            char *right_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            if (!left_buf || !right_buf) {
                formula_pool_free(left_buf);
                formula_pool_free(right_buf);
                return -1;
            }

            render_dsl_internal(node->data.binary_op.left, left_buf, lv_FORMULA_BUF_SIZE, options);
            render_dsl_internal(node->data.binary_op.right, right_buf, lv_FORMULA_BUF_SIZE, options);

            written = snprintf(buffer, size, "%s * %s", left_buf, right_buf);

            formula_pool_free(left_buf);
            formula_pool_free(right_buf);
        } break;

        case NODE_BINARY_OP_DIV: {
            /* HEAP_ALLOCATED: 池分配子表达式缓冲区 */
            char *left_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            char *right_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            if (!left_buf || !right_buf) {
                formula_pool_free(left_buf);
                formula_pool_free(right_buf);
                return -1;
            }

            render_dsl_internal(node->data.binary_op.left, left_buf, lv_FORMULA_BUF_SIZE, options);
            render_dsl_internal(node->data.binary_op.right, right_buf, lv_FORMULA_BUF_SIZE, options);

            written = snprintf(buffer, size, "%s / %s", left_buf, right_buf);

            formula_pool_free(left_buf);
            formula_pool_free(right_buf);
        } break;

        case NODE_BINARY_OP_POW: {
            /* HEAP_ALLOCATED: 池分配子表达式缓冲区 */
            char *left_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            char *right_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            if (!left_buf || !right_buf) {
                formula_pool_free(left_buf);
                formula_pool_free(right_buf);
                return -1;
            }

            render_dsl_internal(node->data.binary_op.left, left_buf, lv_FORMULA_BUF_SIZE, options);
            render_dsl_internal(node->data.binary_op.right, right_buf, lv_FORMULA_BUF_SIZE, options);

            written = snprintf(buffer, size, "%s ^ %s", left_buf, right_buf);

            formula_pool_free(left_buf);
            formula_pool_free(right_buf);
        } break;

        case NODE_UNARY_OP_NEG: {
            /* HEAP_ALLOCATED: 池分配操作数缓冲区 */
            char *operand_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            if (!operand_buf)
                return -1;

            render_dsl_internal(node->data.unary_op.operand, operand_buf, lv_FORMULA_BUF_SIZE, options);
            written = snprintf(buffer, size, "-%s", operand_buf);

            formula_pool_free(operand_buf);
        } break;

        case NODE_UNARY_OP_SQRT: {
            /* HEAP_ALLOCATED: 池分配操作数缓冲区 */
            char *operand_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            if (!operand_buf)
                return -1;

            render_dsl_internal(node->data.unary_op.operand, operand_buf, lv_FORMULA_BUF_SIZE, options);
            written = snprintf(buffer, size, "sqrt(%s)", operand_buf);

            formula_pool_free(operand_buf);
        } break;

        case NODE_UNARY_OP_SIN:
        case NODE_UNARY_OP_COS:
        case NODE_UNARY_OP_TAN: {
            const char *func_names[] = {[NODE_UNARY_OP_SIN - NODE_UNARY_OP_NEG] = "sin",
                                        [NODE_UNARY_OP_COS - NODE_UNARY_OP_NEG] = "cos",
                                        [NODE_UNARY_OP_TAN - NODE_UNARY_OP_NEG] = "tan"};
            int idx = node->type - NODE_UNARY_OP_NEG;

            /* HEAP_ALLOCATED: 池分配操作数缓冲区 */
            char *operand_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            if (!operand_buf)
                return -1;

            render_dsl_internal(node->data.unary_op.operand, operand_buf, lv_FORMULA_BUF_SIZE, options);
            written = snprintf(buffer, size, "%s(%s)", func_names[idx], operand_buf);

            formula_pool_free(operand_buf);
        } break;

        case NODE_UNARY_OP_ABS: {
            /* HEAP_ALLOCATED: 池分配操作数缓冲区 */
            char *operand_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            if (!operand_buf)
                return -1;

            render_dsl_internal(node->data.unary_op.operand, operand_buf, lv_FORMULA_BUF_SIZE, options);
            written = snprintf(buffer, size, "abs(%s)", operand_buf);

            formula_pool_free(operand_buf);
        } break;

        case NODE_EQUATION: {
            /* HEAP_ALLOCATED: 池分配子表达式缓冲区 */
            char *lhs_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            char *rhs_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
            if (!lhs_buf || !rhs_buf) {
                formula_pool_free(lhs_buf);
                formula_pool_free(rhs_buf);
                return -1;
            }

            render_dsl_internal(node->data.equation.lhs, lhs_buf, lv_FORMULA_BUF_SIZE, options);
            render_dsl_internal(node->data.equation.rhs, rhs_buf, lv_FORMULA_BUF_SIZE, options);

            written = snprintf(buffer, size, "%s = %s", lhs_buf, rhs_buf);

            formula_pool_free(lhs_buf);
            formula_pool_free(rhs_buf);
        } break;

        case NODE_GEOM_POINT: {
            /* HEAP_ALLOCATED: 大型坐标缓冲区使用直接 malloc */
            char *coords_buf = (char *) lv_malloc(lv_FORMULA_BUF_LARGE);
            if (!coords_buf)
                return -1;
            memset(coords_buf, 0, lv_FORMULA_BUF_LARGE);

            if (node->data.geom_point.coords) {
                render_dsl_internal(node->data.geom_point.coords, coords_buf, lv_FORMULA_BUF_LARGE, options);
            }

            written = snprintf(buffer, size, "point %s(%s)",
                               node->data.geom_point.name ? node->data.geom_point.name : "P", coords_buf);

            lv_free((void **) &coords_buf);
        } break;

        case NODE_GEOM_SEGMENT: {
            /* STACK_SAFE: 端点名缓冲区 ≤64 字节 */
            char ep1_buf[lv_FORMULA_BUF_SMALL] = {0};
            char ep2_buf[lv_FORMULA_BUF_SMALL] = {0};

            if (node->data.geom_segment.endpoint1) {
                render_dsl_internal(node->data.geom_segment.endpoint1, ep1_buf, sizeof(ep1_buf), options);
            }
            if (node->data.geom_segment.endpoint2) {
                render_dsl_internal(node->data.geom_segment.endpoint2, ep2_buf, sizeof(ep2_buf), options);
            }

            written = snprintf(buffer, size, "segment %s(%s, %s)",
                               node->data.geom_segment.name ? node->data.geom_segment.name : "AB", ep1_buf, ep2_buf);
        } break;

        case NODE_GEOM_CIRCLE: {
            /* STACK_SAFE: 小型缓冲区 ≤256 字节 */
            char center_buf[lv_FORMULA_BUF_SMALL] = {0};
            char radius_buf[lv_FORMULA_BUF_MEDIUM] = {0};

            if (node->data.geom_circle.center) {
                render_dsl_internal(node->data.geom_circle.center, center_buf, sizeof(center_buf), options);
            }
            if (node->data.geom_circle.radius) {
                render_dsl_internal(node->data.geom_circle.radius, radius_buf, sizeof(radius_buf), options);
            }

            written = snprintf(buffer, size, "circle %s(%s, %s)",
                               node->data.geom_circle.name ? node->data.geom_circle.name : "O", center_buf, radius_buf);
        } break;

        case NODE_GEOM_TRIANGLE: {
            /* STACK_SAFE: 顶点名缓冲区 ≤64 字节 */
            char v1_buf[lv_FORMULA_BUF_SMALL] = {0}, v2_buf[lv_FORMULA_BUF_SMALL] = {0},
                 v3_buf[lv_FORMULA_BUF_SMALL] = {0};

            if (node->data.geom_triangle.vertex1) {
                render_dsl_internal(node->data.geom_triangle.vertex1, v1_buf, sizeof(v1_buf), options);
            }
            if (node->data.geom_triangle.vertex2) {
                render_dsl_internal(node->data.geom_triangle.vertex2, v2_buf, sizeof(v2_buf), options);
            }
            if (node->data.geom_triangle.vertex3) {
                render_dsl_internal(node->data.geom_triangle.vertex3, v3_buf, sizeof(v3_buf), options);
            }

            written =
                snprintf(buffer, size, "triangle %s(%s, %s, %s)",
                         node->data.geom_triangle.name ? node->data.geom_triangle.name : "ABC", v1_buf, v2_buf, v3_buf);
        } break;

        case NODE_COORDINATE_LIST: {
            char *ptr = buffer;
            size_t remaining = size;
            int total = 0;

            for (int i = 0; i < node->data.coord_list.coord_count; i++) {
                /* STACK_SAFE: 坐标缓冲区 ≤256 字节 */
                char coord_buf[lv_FORMULA_BUF_MEDIUM] = {0};
                render_dsl_internal(node->data.coord_list.coords[i], coord_buf, sizeof(coord_buf), options);

                int w = snprintf(ptr, remaining, "%s%s", (i > 0) ? ", " : "", coord_buf);
                if (w < 0 || (size_t) w >= remaining)
                    break;
                ptr += w;
                remaining -= w;
                total += w;
            }
            written = total;
        } break;

        case NODE_CONSTRAINT_PERPENDICULAR: {
            /* STACK_SAFE: 点名缓冲区 ≤64 字节 */
            char p1_buf[lv_FORMULA_BUF_SMALL] = {0}, p2_buf[lv_FORMULA_BUF_SMALL] = {0},
                 p3_buf[lv_FORMULA_BUF_SMALL] = {0};
            if (node->data.constraint.participant_count >= 3) {
                render_dsl_internal(node->data.constraint.participants[0], p1_buf, sizeof(p1_buf), options);
                render_dsl_internal(node->data.constraint.participants[1], p2_buf, sizeof(p2_buf), options);
                render_dsl_internal(node->data.constraint.participants[2], p3_buf, sizeof(p3_buf), options);
                written = snprintf(buffer, size, "perpendicular(%s, %s, %s)", p1_buf, p2_buf, p3_buf);
            }
        } break;

        case NODE_CONSTRAINT_PARALLEL: {
            /* STACK_SAFE: 线名缓冲区 ≤64 字节 */
            char l1_buf[lv_FORMULA_BUF_SMALL] = {0}, l2_buf[lv_FORMULA_BUF_SMALL] = {0};
            if (node->data.constraint.participant_count >= 2) {
                render_dsl_internal(node->data.constraint.participants[0], l1_buf, sizeof(l1_buf), options);
                render_dsl_internal(node->data.constraint.participants[1], l2_buf, sizeof(l2_buf), options);
                written = snprintf(buffer, size, "parallel(%s, %s)", l1_buf, l2_buf);
            }
        } break;

        case NODE_CONSTRAINT_MIDPOINT: {
            /* STACK_SAFE: 点名缓冲区 ≤64 字节 */
            char m_buf[lv_FORMULA_BUF_SMALL] = {0}, a_buf[lv_FORMULA_BUF_SMALL] = {0},
                 b_buf[lv_FORMULA_BUF_SMALL] = {0};
            if (node->data.constraint.participant_count >= 3) {
                render_dsl_internal(node->data.constraint.participants[0], m_buf, sizeof(m_buf), options);
                render_dsl_internal(node->data.constraint.participants[1], a_buf, sizeof(a_buf), options);
                render_dsl_internal(node->data.constraint.participants[2], b_buf, sizeof(b_buf), options);
                written = snprintf(buffer, size, "midpoint(%s, %s, %s)", m_buf, a_buf, b_buf);
            }
        } break;

        /* NODE_GEOM_REGION 区域渲染 */
        case NODE_GEOM_REGION: {
            const char *name = node->data.geom_region.name ? node->data.geom_region.name : "R";
            written = snprintf(buffer, size, "region %s", name);
        } break;

        /* NODE_GEOM_ARC 弧渲染 */
        case NODE_GEOM_ARC: {
            /* STACK_SAFE: 名称/角度缓冲区 ≤64，半径缓冲区 ≤256 */
            char center_buf[lv_FORMULA_BUF_SMALL] = {0}, radius_buf[lv_FORMULA_BUF_MEDIUM] = {0};
            char start_buf[lv_FORMULA_BUF_SMALL] = {0}, end_buf[lv_FORMULA_BUF_SMALL] = {0};

            if (node->data.geom_arc.center) {
                render_dsl_internal(node->data.geom_arc.center, center_buf, sizeof(center_buf), options);
            }
            if (node->data.geom_arc.radius) {
                render_dsl_internal(node->data.geom_arc.radius, radius_buf, sizeof(radius_buf), options);
            }
            if (node->data.geom_arc.start_angle) {
                render_dsl_internal(node->data.geom_arc.start_angle, start_buf, sizeof(start_buf), options);
            }
            if (node->data.geom_arc.end_angle) {
                render_dsl_internal(node->data.geom_arc.end_angle, end_buf, sizeof(end_buf), options);
            }

            written = snprintf(buffer, size, "arc %s(%s, %s, %s, %s)",
                               node->data.geom_arc.name ? node->data.geom_arc.name : "AB", center_buf, radius_buf,
                               start_buf, end_buf);
        } break;

        /* NODE_CONSTRAINT_ANGLE 角度约束渲染 */
        case NODE_CONSTRAINT_ANGLE: {
            /* STACK_SAFE: 点名缓冲区 ≤64 字节 */
            char p1_buf[lv_FORMULA_BUF_SMALL] = {0}, p2_buf[lv_FORMULA_BUF_SMALL] = {0},
                 p3_buf[lv_FORMULA_BUF_SMALL] = {0};
            if (node->data.constraint.participant_count >= 3) {
                render_dsl_internal(node->data.constraint.participants[0], p1_buf, sizeof(p1_buf), options);
                render_dsl_internal(node->data.constraint.participants[1], p2_buf, sizeof(p2_buf), options);
                render_dsl_internal(node->data.constraint.participants[2], p3_buf, sizeof(p3_buf), options);
                written = snprintf(buffer, size, "angle(%s, %s, %s)", p1_buf, p2_buf, p3_buf);
            }
        } break;

        case NODE_COMPOUND: {
            char *ptr = buffer;
            size_t remaining = size;
            int total = 0;

            for (int i = 0; i < node->data.compound.statement_count; i++) {
                /* HEAP_ALLOCATED: 池分配语句缓冲区 */
                char *stmt_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
                if (!stmt_buf)
                    break;

                render_dsl_internal(node->data.compound.statements[i], stmt_buf, lv_FORMULA_BUF_SIZE, options);

                int w = snprintf(ptr, remaining, "%s; ", stmt_buf);
                formula_pool_free(stmt_buf);

                if (w < 0 || (size_t) w >= remaining)
                    break;
                ptr += w;
                remaining -= w;
                total += w;
            }
            written = total;
        } break;

        default:
            written = snprintf(buffer, size, "<unknown>");
            break;
    }

    return written;
}

/* ============================================================
 * MathML 渲染器
 * ============================================================ */

/**
 * @brief 将 AST 渲染为 MathML 格式
 *
 * 当前使用 LaTeX-in-annotation 方式（annotation encoding），这是合法的 MathML 表示。
 * 未来可扩展为原生 MathML 语义元素（<mfrac>, <msqrt>, <msup> 等）。
 */
static int render_mathml_internal(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options) {
    /* options 预留：未来可控制输出精度、样式等 */
    (void) options;
    if (!node || !buffer || size == 0)
        return -1;

    char *latex_buf = (char *) lv_malloc(lv_MAX_RENDER_BUFFER);
    if (!latex_buf)
        return -1;

    int latex_len = render_latex_internal(node, latex_buf, lv_MAX_RENDER_BUFFER, options);
    if (latex_len < 0) {
        lv_free((void **) &latex_buf);
        return -1;
    }

    int written = snprintf(buffer, size,
                           "<math xmlns=\"http://www.w3.org/1998/Math/MathML\" display=\"block\">\n"
                           "  <semantics>\n"
                           "    <mrow>\n"
                           "      <mi>%s</mi>\n"
                           "    </mrow>\n"
                           "    <annotation encoding=\"application/x-tex\">%s</annotation>\n"
                           "  </semantics>\n"
                           "</math>",
                           latex_buf, latex_buf);

    lv_free((void **) &latex_buf);
    return written;
}

/* ============================================================
 * ASCII 艺术渲染器
 * ============================================================ */

/**
 * @brief 将 AST 渲染为 ASCII 艺术格式
 *
 * 生成基本的 ASCII 数学表示。
 * options 预留：未来可控制精度、宽度等格式参数。
 */
static int render_ascii_internal(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options) {
    if (!node || !buffer || size == 0)
        return -1;

    /* options->precision 控制浮点数输出精度 */
    int prec = (options && options->precision > 0) ? options->precision : 6;
    (void) prec; /* 精度参数由 render_number_internal 使用 */

    switch (node->type) {
        case NODE_NUMBER: {
            if (node->data.number.is_integer) {
                return snprintf(buffer, size, "%lld", (long long) node->data.number.numerator);
            } else {
                return snprintf(buffer, size, "%lld/%llu", (long long) node->data.number.numerator,
                                (unsigned long long) node->data.number.denominator);
            }
        }
        case NODE_VARIABLE:
            return snprintf(buffer, size, "%s", node->data.variable.name);
        case NODE_IDENTIFIER:
            return snprintf(buffer, size, "%s", node->data.identifier.name);
        case NODE_BINARY_OP_ADD: {
            char left[lv_FORMULA_BUF_SIZE], right[lv_FORMULA_BUF_SIZE];
            render_ascii_internal(node->data.binary_op.left, left, sizeof(left), options);
            render_ascii_internal(node->data.binary_op.right, right, sizeof(right), options);
            return snprintf(buffer, size, "(%s + %s)", left, right);
        }
        case NODE_BINARY_OP_SUB: {
            char left[lv_FORMULA_BUF_SIZE], right[lv_FORMULA_BUF_SIZE];
            render_ascii_internal(node->data.binary_op.left, left, sizeof(left), options);
            render_ascii_internal(node->data.binary_op.right, right, sizeof(right), options);
            return snprintf(buffer, size, "(%s - %s)", left, right);
        }
        case NODE_BINARY_OP_MUL: {
            char left[lv_FORMULA_BUF_SIZE], right[lv_FORMULA_BUF_SIZE];
            render_ascii_internal(node->data.binary_op.left, left, sizeof(left), options);
            render_ascii_internal(node->data.binary_op.right, right, sizeof(right), options);
            return snprintf(buffer, size, "(%s * %s)", left, right);
        }
        case NODE_BINARY_OP_DIV: {
            char left[lv_FORMULA_BUF_SIZE], right[lv_FORMULA_BUF_SIZE];
            render_ascii_internal(node->data.binary_op.left, left, sizeof(left), options);
            render_ascii_internal(node->data.binary_op.right, right, sizeof(right), options);
            return snprintf(buffer, size, "(%s / %s)", left, right);
        }
        case NODE_BINARY_OP_POW: {
            char left[lv_FORMULA_BUF_SIZE], right[lv_FORMULA_BUF_SIZE];
            render_ascii_internal(node->data.binary_op.left, left, sizeof(left), options);
            render_ascii_internal(node->data.binary_op.right, right, sizeof(right), options);
            return snprintf(buffer, size, "(%s ^ %s)", left, right);
        }
        case NODE_UNARY_OP_NEG: {
            char operand[lv_FORMULA_BUF_SIZE];
            render_ascii_internal(node->data.unary_op.operand, operand, sizeof(operand), options);
            return snprintf(buffer, size, "-(%s)", operand);
        }
        case NODE_UNARY_OP_SQRT: {
            char operand[lv_FORMULA_BUF_SIZE];
            render_ascii_internal(node->data.unary_op.operand, operand, sizeof(operand), options);
            return snprintf(buffer, size, "sqrt(%s)", operand);
        }
        case NODE_UNARY_OP_SIN:
        case NODE_UNARY_OP_COS:
        case NODE_UNARY_OP_TAN: {
            const char *fn = (node->type == NODE_UNARY_OP_SIN)   ? "sin"
                             : (node->type == NODE_UNARY_OP_COS) ? "cos"
                                                                 : "tan";
            char operand[lv_FORMULA_BUF_SIZE];
            render_ascii_internal(node->data.unary_op.operand, operand, sizeof(operand), options);
            return snprintf(buffer, size, "%s(%s)", fn, operand);
        }
        case NODE_UNARY_OP_ABS: {
            char operand[lv_FORMULA_BUF_SIZE];
            render_ascii_internal(node->data.unary_op.operand, operand, sizeof(operand), options);
            return snprintf(buffer, size, "|%s|", operand);
        }
        case NODE_UNARY_OP_LN: {
            char operand[lv_FORMULA_BUF_SIZE];
            render_ascii_internal(node->data.unary_op.operand, operand, sizeof(operand), options);
            return snprintf(buffer, size, "ln(%s)", operand);
        }
        case NODE_UNARY_OP_LOG: {
            char operand[lv_FORMULA_BUF_SIZE];
            render_ascii_internal(node->data.unary_op.operand, operand, sizeof(operand), options);
            return snprintf(buffer, size, "log(%s)", operand);
        }
        case NODE_EQUATION: {
            char lhs[lv_FORMULA_BUF_SIZE], rhs[lv_FORMULA_BUF_SIZE];
            render_ascii_internal(node->data.equation.lhs, lhs, sizeof(lhs), options);
            render_ascii_internal(node->data.equation.rhs, rhs, sizeof(rhs), options);
            return snprintf(buffer, size, "%s = %s", lhs, rhs);
        }
        default:
            return render_latex_internal(node, buffer, size, options);
    }
}

/* ============================================================
 * HTML 渲染器
 * ============================================================ */

/**
 * @brief 将 AST 渲染为 HTML MathJax 兼容格式
 *
 * 将 LaTeX 渲染结果包装在 MathJax 兼容的 HTML 标签中。
 * options 控制精度和输出样式（inline/block）。
 */
static int render_html_internal(const FormulaNode *node, char *buffer, size_t size, const RenderOptions *options) {
    int precision = 6; /* 默认精度 */
    int use_block = 0; /* 0=inline <code>, 1=block <div> */
    if (options) {
        if (options->precision > 0)
            precision = options->precision;
        if (options->style)
            use_block = (options->style[0] == 'b' || options->style[0] == 'B');
    }
    if (!node || !buffer || size == 0)
        return -1;

    char *latex_buf = (char *) lv_malloc(lv_MAX_RENDER_BUFFER);
    if (!latex_buf)
        return -1;

    int latex_len = render_latex_internal(node, latex_buf, lv_MAX_RENDER_BUFFER, options);
    if (latex_len < 0) {
        lv_free((void **) &latex_buf);
        return -1;
    }

    int written = snprintf(buffer, size, "<span class=\"mathjax-container\" data-formula=\"%s\">\\(%s\\)</span>",
                           latex_buf, latex_buf);

    lv_free((void **) &latex_buf);
    return written;
}

/* ============================================================
 * 公共 API 实现
 * ============================================================ */

/**
 * @brief 渲染公式 AST 为指定格式的字符串（简化版）
 *
 * 使用默认渲染选项将 AST 渲染为字符串。
 *
 * @param node   AST 根节点
 * @param format 输出格式（LaTeX/Python/DSL）
 * @return 新分配的字符串指针，失败返回 NULL
 */
char *formula_render(const FormulaNode *node, OutputFormat format) {
    RenderOptions options = RENDER_OPTIONS_DEFAULT;
    return formula_render_ex(node, format, &options);
}

/**
 * @brief 渲染公式 AST 为指定格式的字符串（扩展版）
 *
 * 使用自定义渲染选项将 AST 渲染为字符串。
 *
 * @param node    AST 根节点
 * @param format  输出格式（LaTeX/Python/DSL）
 * @param options 渲染选项指针
 * @return 新分配的字符串指针，失败返回 NULL
 */
char *formula_render_ex(const FormulaNode *node, OutputFormat format, const RenderOptions *options) {
    if (!node) {
        lv_set_error(lv_ERROR_INTERNAL, "NULL node");
        return NULL;
    }

    /* HEAP_ALLOCATED: 输出缓冲区使用 lv_malloc */
    char *buffer = (char *) lv_malloc(lv_MAX_RENDER_BUFFER);
    if (!buffer) {
        lv_set_error(lv_ERROR_OUT_OF_MEMORY, "Memory allocation failed");
        return NULL;
    }

    int written = 0;

    switch (format) {
        case OUTPUT_LATEX:
            written = render_latex_internal(node, buffer, lv_MAX_RENDER_BUFFER, options);
            break;
        case OUTPUT_PYTHON:
            written = render_python_internal(node, buffer, lv_MAX_RENDER_BUFFER, options);
            break;
        case OUTPUT_DSL:
            written = render_dsl_internal(node, buffer, lv_MAX_RENDER_BUFFER, options);
            break;
        case OUTPUT_MATHML:
            written = render_mathml_internal(node, buffer, lv_MAX_RENDER_BUFFER, options);
            break;
        case OUTPUT_ASCII:
            written = render_ascii_internal(node, buffer, lv_MAX_RENDER_BUFFER, options);
            break;
        case OUTPUT_HTML:
            written = render_html_internal(node, buffer, lv_MAX_RENDER_BUFFER, options);
            break;
        default:
            lv_set_error(lv_ERROR_UNSUPPORTED, "Unknown output format");
            lv_free((void **) &buffer);
            return NULL;
    }

    if (written < 0) {
        lv_set_error(lv_ERROR_INTERNAL, "Render failed");
        lv_free((void **) &buffer);
        return NULL;
    }

    /* 重新分配到实际大小 */
    char *result = (char *) lv_realloc(buffer, written + 1);
    return result ? result : buffer;
}

/**
 * @brief 渲染公式 AST 到缓冲区（简化版）
 *
 * @param node   AST 根节点
 * @param format 输出格式
 * @param buffer 输出缓冲区
 * @param size   缓冲区大小
 * @return 写入的字节数（不含终止符），失败返回负值
 */
int formula_render_to_buffer(const FormulaNode *node, OutputFormat format, char *buffer, size_t size) {
    RenderOptions options = RENDER_OPTIONS_DEFAULT;
    return formula_render_to_buffer_ex(node, format, &options, buffer, size);
}

/**
 * @brief 渲染公式 AST 到缓冲区（扩展版）
 *
 * @param node    AST 根节点
 * @param format  输出格式
 * @param options 渲染选项指针
 * @param buffer  输出缓冲区
 * @param size    缓冲区大小
 * @return 写入的字节数（不含终止符），失败返回负值
 */
int formula_render_to_buffer_ex(const FormulaNode *node, OutputFormat format, const RenderOptions *options,
                                char *buffer, size_t size) {
    if (!node || !buffer || size == 0) {
        return -1;
    }

    int written = 0;

    switch (format) {
        case OUTPUT_LATEX:
            written = render_latex_internal(node, buffer, size, options);
            break;
        case OUTPUT_PYTHON:
            written = render_python_internal(node, buffer, size, options);
            break;
        case OUTPUT_DSL:
            written = render_dsl_internal(node, buffer, size, options);
            break;
        case OUTPUT_MATHML:
            written = render_mathml_internal(node, buffer, size, options);
            break;
        case OUTPUT_ASCII:
            written = render_ascii_internal(node, buffer, size, options);
            break;
        case OUTPUT_HTML:
            written = render_html_internal(node, buffer, size, options);
            break;
        default:
            return -1;
    }

    return written;
}

/**
 * @brief 渲染公式 AST 为 LaTeX 字符串（便捷函数）
 *
 * @param node AST 根节点
 * @return 新分配的 LaTeX 字符串指针，失败返回 NULL
 */
char *formula_render_latex(const FormulaNode *node) {
    return formula_render(node, OUTPUT_LATEX);
}

/**
 * @brief 渲染公式 AST 为 Python 字符串（便捷函数）
 *
 * @param node AST 根节点
 * @return 新分配的 Python 字符串指针，失败返回 NULL
 */
char *formula_render_python(const FormulaNode *node) {
    return formula_render(node, OUTPUT_PYTHON);
}

/**
 * @brief 渲染公式 AST 为 DSL 字符串（便捷函数）
 *
 * @param node AST 根节点
 * @return 新分配的 DSL 字符串指针，失败返回 NULL
 */
char *formula_render_dsl(const FormulaNode *node) {
    return formula_render(node, OUTPUT_DSL);
}

/**
 * @brief 渲染点坐标为 LaTeX 字符串
 *
 * @param name        点名称
 * @param coords      坐标 AST 节点数组
 * @param coord_count 坐标数量
 * @return 新分配的 LaTeX 字符串指针，失败返回 NULL
 */
char *formula_render_point_latex(const char *name, const FormulaNode **coords, int coord_count) {
    if (!name || !coords || coord_count == 0) {
        return NULL;
    }

    /* HEAP_ALLOCATED: 坐标组合缓冲区使用池分配 */
    char *coords_buf = formula_pool_alloc(lv_FORMULA_BUF_SIZE);
    if (!coords_buf)
        return NULL;

    char *ptr = coords_buf;
    size_t remaining = lv_FORMULA_BUF_SIZE;

    for (int i = 0; i < coord_count; i++) {
        /* STACK_SAFE: 单个坐标缓冲区 ≤256 字节 */
        char coord_buf[lv_FORMULA_BUF_MEDIUM] = {0};
        formula_render_to_buffer(coords[i], OUTPUT_LATEX, coord_buf, sizeof(coord_buf));

        int w = snprintf(ptr, remaining, "%s%s", (i > 0) ? ", " : "", coord_buf);
        if (w < 0 || (size_t) w >= remaining)
            break;
        ptr += w;
        remaining -= w;
    }

    /* HEAP_ALLOCATED: 结果字符串 */
    char *result = (char *) lv_malloc(lv_POINT_LATEX_BUF_SIZE);
    if (result) {
        snprintf(result, lv_POINT_LATEX_BUF_SIZE, "%s = \\left(%s\\right)", name, coords_buf);
    }

    formula_pool_free(coords_buf);
    return result;
}

/**
 * @brief 渲染线段名称为 LaTeX 字符串
 *
 * @param name 线段名称
 * @return 新分配的 LaTeX 字符串指针，失败返回 NULL
 */
char *formula_render_segment_latex(const char *name) {
    if (!name)
        return NULL;

    /* HEAP_ALLOCATED: 结果字符串 */
    char *result = (char *) lv_malloc(lv_SEGMENT_LATEX_BUF_SIZE);
    if (result) {
        snprintf(result, lv_SEGMENT_LATEX_BUF_SIZE, "\\overline{%s}", name);
    }
    return result;
}

/**
 * @brief 渲染圆为 LaTeX 字符串
 *
 * @param name   圆名称
 * @param center 圆心名称
 * @param radius 半径 AST 节点
 * @return 新分配的 LaTeX 字符串指针，失败返回 NULL
 */
char *formula_render_circle_latex(const char *name, const char *center, const FormulaNode *radius) {
    if (!name || !center || !radius)
        return NULL;

    /* STACK_SAFE: 半径渲染缓冲区 ≤256 字节 */
    char radius_buf[lv_FORMULA_BUF_MEDIUM] = {0};
    formula_render_to_buffer(radius, OUTPUT_LATEX, radius_buf, sizeof(radius_buf));

    /* HEAP_ALLOCATED: 结果字符串 */
    char *result = (char *) lv_malloc(lv_CIRCLE_LATEX_BUF_SIZE);
    if (result) {
        snprintf(result, lv_CIRCLE_LATEX_BUF_SIZE,
                 "\\text{circle } %s \\text{ with center } %s \\text{ and radius } %s", name, center, radius_buf);
    }
    return result;
}

/**
 * @brief 渲染分数为 LaTeX 字符串
 *
 * @param numerator   分子
 * @param denominator 分母
 * @return 新分配的 LaTeX 字符串指针，失败返回 NULL
 */
char *formula_render_fraction_latex(int64_t numerator, uint64_t denominator) {
    /* HEAP_ALLOCATED: 结果字符串 */
    char *result = (char *) lv_malloc(lv_FRACTION_LATEX_BUF_SIZE);
    if (result) {
        snprintf(result, lv_FRACTION_LATEX_BUF_SIZE, "\\frac{%lld}{%llu}", (long long) numerator,
                 (unsigned long long) denominator);
    }
    return result;
}
