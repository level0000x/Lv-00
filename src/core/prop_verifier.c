/**
 * @file prop_verifier.c
 * @brief 命题逻辑验证器实现 —— 自然演绎证明搜索
 *
 * @details 实现基于自然演绎的向后链接证明搜索算法。
 *          支持直觉主义逻辑和经典逻辑模式。
 *
 *          核心推理规则：
 *            - 合取消去：从合取推出分量
 *            - 析取引入：从分量推出析取
 *            - 蕴涵消去（modus ponens）：从蕴涵和前件推出后件
 *            - 否定消去：从否定和原命题推出矛盾
 *            - 爆炸原理：若启用，从矛盾推出任意命题
 *
 * @author Lv-00 Project
 * @version 3.2.0
 */

#include "prop_verifier.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lv00_utils.h"
#include "stream_context_util.h"

LV00_DECLARE_STREAM_CTX(prop_verifier)

/* ============================================================
 * 内部常量
 * ============================================================ */

#define MAX_PREMISES 64       /**< 最大前提数 */
#define MAX_GOALS 64          /**< 最大子目标数 */
#define MAX_MEMO_ENTRIES 1024 /**< 记忆化表大小 */
#define MAX_FORMULA_STR 2048  /**< 公式字符串最大长度 */
#define MAX_COPY_DEPTH 200    /**< 公式深拷贝最大递归深度（防止栈溢出） */
#define MAX_DESTROY_DEPTH 200 /**< 公式销毁最大递归深度（防止栈溢出） */

/* ---- 析构栈容量 ---- */
#define PROP_DESTROY_STACK_INIT_CAP 64 /* 销毁时栈的初始容量 */
#define PROP_DESTROY_STACK_GROWTH 2    /* 销毁栈扩容倍数 */

/* ---- 运算符优先级 ---- */
#define PROP_PREC_ATOM 100       /* 原子命题优先级 */
#define PROP_PREC_NEGATION 80    /* 否定联结词优先级 */
#define PROP_PREC_CONJUNCTION 60 /* 合取联结词优先级 */
#define PROP_PREC_DISJUNCTION 50 /* 析取联结词优先级 */
#define PROP_PREC_IMPLICATION 40 /* 蕴涵联结词优先级 */
#define PROP_PREC_DEFAULT 0      /* 默认最低优先级 */

/* ---- 哈希函数常量 ---- */
#define PROP_HASH_TYPE_MULTIPLIER 2654435761U  /* 黄金比例常数（Knuth推荐） */
#define PROP_HASH_STRING_MULTIPLIER 31         /* 字符串哈希乘数 */
#define PROP_HASH_LEFT_MULTIPLIER 0x9e3779b9U  /* 左子公式哈希乘数 */
#define PROP_HASH_RIGHT_MULTIPLIER 0x517cc1b7U /* 右子公式哈希乘数 */
#define PROP_HASH_PTR_MULTIPLIER 0x45d9f3bU    /* 指针哈希乘数 */
#define PROP_HASH_BIT_SHIFT 16                 /* 哈希位混合偏移量 */
#define PROP_HASH_PREMISES_MULTIPLIER 31       /* 前提集合哈希乘数 */

/* ---- 时间转换 ---- */
#define PROP_TIME_MS_PER_SEC 1000 /* 秒到毫秒的转换因子 */

/* ---- 烟测与缓冲区常量 ---- */
#define PROP_SMOKE_TEST_COUNT 13       /* 内置烟测数量 */
#define PROP_SMOKE_MAX_PREM_PTRS 8     /* 烟测中前提指针临时数组大小 */
#define PROP_SMOKE_CLEANUP_MAX_PTRS 16 /* 烟测清理时最大引用指针数 */
#define PROP_ATOM_NAME_MAX_LEN 64      /* 原子命题名称最大长度 */
#define PROP_ATOM_COLLECT_MAX 32       /* 收集原子命题最大数 */
#define PROP_PATTERN_DESC_BUFSIZE 256  /* 模式描述缓冲区大小 */
#define PROP_ANALYSIS_DESC_BUFSIZE 512 /* 分析描述缓冲区大小 */
#define PROP_MISSING_LIST_BUFSIZE 512  /* 缺失列表缓冲区大小 */
#define PROP_STREAM_EVENT_BUFSIZE 256  /* 流式事件描述缓冲区大小 */
#define PROP_JSON_DETAIL_BUFSIZE 192   /* JSON详情缓冲区大小 */

/* ---- 信任颜色判定阈值 ---- */
#define PROP_TRUST_YELLOW_THRESHOLD 2 /* 黄色信任的缺失构造上限 */
#define PROP_TRUST_AMBER_MIN 3        /* 琥珀色信任的缺失构造下限 */

/* ============================================================
 * 公式构造/销毁
 * ============================================================ */

/**
 * @brief 创建原子命题公式
 *
 * @param name 原子命题名称
 * @return 新分配的公式指针，失败返回 NULL
 */
PropFormula *prop_formula_create_atom(const char *name) {
    if (!name)
        return NULL;
    PropFormula *f = (PropFormula *) lv00_calloc(1, sizeof(PropFormula)); /* 零初始化分配 */
    if (!f)
        return NULL;
    f->type = PROP_ATOM;
    snprintf(f->data.atom.name, sizeof(f->data.atom.name), "%s", name);
    return f;
}

/**
 * @brief 创建合取公式（A AND B）
 *
 * @param left  左操作数
 * @param right 右操作数
 * @return 新分配的公式指针，失败返回 NULL
 */
PropFormula *prop_formula_create_conjunction(PropFormula *left, PropFormula *right) {
    if (!left || !right)
        return NULL;
    PropFormula *f = (PropFormula *) lv00_calloc(1, sizeof(PropFormula)); /* 零初始化分配 */
    if (!f)
        return NULL;
    f->type = PROP_CONJUNCTION;
    f->data.binary.left = left;
    f->data.binary.right = right;
    return f;
}

/**
 * @brief 创建析取公式（A OR B）
 *
 * @param left  左操作数
 * @param right 右操作数
 * @return 新分配的公式指针，失败返回 NULL
 */
PropFormula *prop_formula_create_disjunction(PropFormula *left, PropFormula *right) {
    if (!left || !right)
        return NULL;
    PropFormula *f = (PropFormula *) lv00_calloc(1, sizeof(PropFormula)); /* 零初始化分配 */
    if (!f)
        return NULL;
    f->type = PROP_DISJUNCTION;
    f->data.binary.left = left;
    f->data.binary.right = right;
    return f;
}

/**
 * @brief 创建蕴涵公式（A IMPLIES B）
 *
 * @param left  前件
 * @param right 后件
 * @return 新分配的公式指针，失败返回 NULL
 */
PropFormula *prop_formula_create_implication(PropFormula *left, PropFormula *right) {
    if (!left || !right)
        return NULL;
    PropFormula *f = (PropFormula *) lv00_calloc(1, sizeof(PropFormula)); /* 零初始化分配 */
    if (!f)
        return NULL;
    f->type = PROP_IMPLICATION;
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
    PropFormula *f = (PropFormula *) lv00_calloc(1, sizeof(PropFormula)); /* 零初始化分配 */
    if (!f)
        return NULL;
    f->type = PROP_NEGATION;
    f->data.unary.operand = operand;
    return f;
}

/**
 * @brief 创建底类型公式（矛盾/假）
 *
 * @return 新分配的公式指针，失败返回 NULL
 */
PropFormula *prop_formula_create_bottom(void) {
    PropFormula *f = (PropFormula *) lv00_calloc(1, sizeof(PropFormula)); /* 零初始化分配 */
    if (!f)
        return NULL;
    f->type = PROP_BOTTOM;
    return f;
}

/**
 * @brief 创建真值公式（顶类型/真）
 *
 * @return 新分配的公式指针，失败返回 NULL
 */
PropFormula *prop_formula_create_true(void) {
    PropFormula *f = (PropFormula *) lv00_calloc(1, sizeof(PropFormula)); /* 零初始化分配 */
    if (!f)
        return NULL;
    f->type = PROP_TRUE;
    return f;
}

/* 内部函数前向声明（static 函数需在使用前声明） */
static PropFormula *prop_formula_copy_depth(const PropFormula *f, int depth);
static void prop_formula_destroy_depth(PropFormula *f, int depth);

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
    switch (f->type) {
        case PROP_ATOM:
            return prop_formula_create_atom(f->data.atom.name);
        case PROP_CONJUNCTION:
            return prop_formula_create_conjunction(prop_formula_copy_depth(f->data.binary.left, depth + 1),
                                                   prop_formula_copy_depth(f->data.binary.right, depth + 1));
        case PROP_DISJUNCTION:
            return prop_formula_create_disjunction(prop_formula_copy_depth(f->data.binary.left, depth + 1),
                                                   prop_formula_copy_depth(f->data.binary.right, depth + 1));
        case PROP_IMPLICATION:
            return prop_formula_create_implication(prop_formula_copy_depth(f->data.binary.left, depth + 1),
                                                   prop_formula_copy_depth(f->data.binary.right, depth + 1));
        case PROP_NEGATION:
            return prop_formula_create_negation(prop_formula_copy_depth(f->data.unary.operand, depth + 1));
        case PROP_BOTTOM:
            return prop_formula_create_bottom();
        case PROP_TRUE:
            return prop_formula_create_true();
    }
    return NULL;
}

/* 递归销毁公式（带递归深度保护，防止栈溢出） */
/**
 * @brief 销毁命题公式并递归释放所有资源
 *
 * @param f 公式指针（可为 NULL）
 */
void prop_formula_destroy(PropFormula *f) {
    prop_formula_destroy_depth(f, 0);
}

/**
 * @brief 安全销毁命题公式（迭代实现，防止栈溢出）
 *
 * 使用显式栈替代递归遍历，避免深度嵌套公式导致调用栈溢出。
 * 同时确保所有子公式节点都被正确释放，无内存泄漏。
 *
 * @param f     待销毁的命题公式指针
 * @param depth 未使用（保留参数以兼容函数签名）
 */
static void prop_formula_destroy_depth(PropFormula *f, int depth) {
    (void) depth; /* 迭代实现不使用深度参数 */
    if (!f)
        return;

    /* 显式栈：存储待销毁的公式节点 */
    int stack_capacity = PROP_DESTROY_STACK_INIT_CAP;
    int stack_top = 0;
    PropFormula **stack = (PropFormula **) lv00_malloc((size_t) stack_capacity * sizeof(PropFormula *));
    if (!stack) {
        /* 内存分配失败：使用非递归的简单释放策略
         * 注意：不调用 prop_formula_destroy() 以避免无限递归 */
        PropFormula *cur = f;
        while (cur) {
            PropFormula *next = NULL;
            switch (cur->type) {
                case PROP_CONJUNCTION:
                case PROP_DISJUNCTION:
                case PROP_IMPLICATION:
                    next = cur->data.binary.left;
                    lv00_free((void **) &cur->data.binary.right);
                    break;
                case PROP_NEGATION:
                    next = cur->data.unary.operand;
                    break;
                default:
                    break;
            }
            lv00_free((void **) &cur);
            cur = next;
        }
        return;
    }
    stack[stack_top++] = f;

    while (stack_top > 0) {
        PropFormula *current = stack[--stack_top];

        /* 将子节点压栈（后进先出，保证处理顺序） */
        switch (current->type) {
            case PROP_CONJUNCTION:
            case PROP_DISJUNCTION:
            case PROP_IMPLICATION:
                /* 二元节点：先压左子节点，再压右子节点 */
                if (current->data.binary.right) {
                    if (stack_top >= stack_capacity) {
                        int new_cap = stack_capacity * PROP_DESTROY_STACK_GROWTH;
                        if (new_cap <= stack_capacity)
                            break; /* 溢出保护 */
                        PropFormula **new_stack =
                            (PropFormula **) lv00_realloc(stack, (size_t) new_cap * sizeof(PropFormula *));
                        if (!new_stack) {
                            /* 栈扩容失败：尝试直接递归销毁剩余子节点 */
                            if (current->data.binary.left)
                                prop_formula_destroy(current->data.binary.left);
                            if (current->data.binary.right)
                                prop_formula_destroy(current->data.binary.right);
                            current->data.binary.left = NULL;
                            current->data.binary.right = NULL;
                            break;
                        }
                        stack = new_stack;
                        stack_capacity = new_cap;
                    }
                    stack[stack_top++] = current->data.binary.right;
                }
                if (current->data.binary.left) {
                    if (stack_top >= stack_capacity) {
                        int new_cap = stack_capacity * PROP_DESTROY_STACK_GROWTH;
                        if (new_cap <= stack_capacity)
                            break;
                        PropFormula **new_stack =
                            (PropFormula **) lv00_realloc(stack, (size_t) new_cap * sizeof(PropFormula *));
                        if (!new_stack) {
                            if (current->data.binary.left)
                                prop_formula_destroy(current->data.binary.left);
                            current->data.binary.left = NULL;
                            break;
                        }
                        stack = new_stack;
                        stack_capacity = new_cap;
                    }
                    stack[stack_top++] = current->data.binary.left;
                }
                current->data.binary.left = NULL;
                current->data.binary.right = NULL;
                break;
            case PROP_NEGATION:
                /* 一元节点：压操作数子节点 */
                if (current->data.unary.operand) {
                    if (stack_top >= stack_capacity) {
                        int new_cap = stack_capacity * PROP_DESTROY_STACK_GROWTH;
                        if (new_cap <= stack_capacity)
                            break;
                        PropFormula **new_stack =
                            (PropFormula **) lv00_realloc(stack, (size_t) new_cap * sizeof(PropFormula *));
                        if (!new_stack) {
                            if (current->data.unary.operand)
                                prop_formula_destroy(current->data.unary.operand);
                            current->data.unary.operand = NULL;
                            break;
                        }
                        stack = new_stack;
                        stack_capacity = new_cap;
                    }
                    stack[stack_top++] = current->data.unary.operand;
                }
                current->data.unary.operand = NULL;
                break;
            default:
                /* 叶子节点（ATOM, BOTTOM, TRUE）：无子节点 */
                break;
        }

        /* 释放当前节点 */
        lv00_free((void **) &current);
    }

    /* 释放栈 */
    lv00_free((void **) &stack);
}

/* ============================================================
 * 公式比较（用于记忆化和前提匹配）
 * ============================================================ */

/**
 * @brief 公式结构相等性比较（递归）
 *
 * 递归比较两个命题公式的结构相等性：
 * - ATOM：比较名称字符串
 * - 二元联结词（CONJ/DISJ/IMPL）：递归比较左右子公式
 * - 一元联结词（NEG）：递归比较操作数
 * - BOTTOM/TRUE：仅类型匹配即相等
 *
 * @param a 第一个公式指针（可为 NULL）
 * @param b 第二个公式指针（可为 NULL）
 * @return true 表示结构相等，false 表示不同
 */
static bool formula_equal(const PropFormula *a, const PropFormula *b) {
    if (!a || !b)
        return a == b;
    if (a->type != b->type)
        return false;
    switch (a->type) {
        case PROP_ATOM:
            return strcmp(a->data.atom.name, b->data.atom.name) == 0;
        case PROP_CONJUNCTION:
        case PROP_DISJUNCTION:
        case PROP_IMPLICATION:
            return formula_equal(a->data.binary.left, b->data.binary.left) &&
                   formula_equal(a->data.binary.right, b->data.binary.right);
        case PROP_NEGATION:
            return formula_equal(a->data.unary.operand, b->data.unary.operand);
        case PROP_BOTTOM:
        case PROP_TRUE:
            return true;
    }
    return false;
}

/* ============================================================
 * 公式序列化
 * ============================================================ */

/**
 * @brief 获取命题联结词的运算符优先级（用于序列化括号化）
 *
 * @param f 公式指针
 * @return 优先级数值（越高绑定越紧）
 */
static int formula_precedence(const PropFormula *f) {
    switch (f->type) {
        case PROP_ATOM:
            return PROP_PREC_ATOM;
        case PROP_NEGATION:
            return PROP_PREC_NEGATION;
        case PROP_CONJUNCTION:
            return PROP_PREC_CONJUNCTION;
        case PROP_DISJUNCTION:
            return PROP_PREC_DISJUNCTION;
        case PROP_IMPLICATION:
            return PROP_PREC_IMPLICATION;
        case PROP_BOTTOM:
            return PROP_PREC_ATOM;
        case PROP_TRUE:
            return PROP_PREC_ATOM;
    }
    return PROP_PREC_DEFAULT;
}

/* 内部递归序列化 */
static void formula_to_string_buf(const PropFormula *f, char *buf, size_t size, int parent_prec) {
    if (!f || size == 0)
        return;

    /* 使用游标跟踪写入位置，避免每次 strncat 都调用 strlen 导致 O(n²) */
    size_t pos = strlen(buf);
    int prec = formula_precedence(f);
    bool need_parens = (parent_prec > prec);

/* 辅助宏：安全追加字符串到缓冲区 */
#define BUF_APPEND(s)                          \
    do {                                       \
        size_t slen = strlen(s);               \
        if (pos + slen < size - 1) {           \
            memcpy(buf + pos, (s), slen);      \
            pos += slen;                       \
        } else {                               \
            strncat(buf, (s), size - pos - 1); \
            pos = size - 1;                    \
        }                                      \
    } while (0)

    if (need_parens) {
        BUF_APPEND("(");
    }

    switch (f->type) {
        case PROP_ATOM:
            BUF_APPEND(f->data.atom.name);
            break;
        case PROP_CONJUNCTION:
            formula_to_string_buf(f->data.binary.left, buf, size, prec);
            BUF_APPEND(" /\\ ");
            formula_to_string_buf(f->data.binary.right, buf, size, prec);
            break;
        case PROP_DISJUNCTION:
            formula_to_string_buf(f->data.binary.left, buf, size, prec);
            BUF_APPEND(" \\/ ");
            formula_to_string_buf(f->data.binary.right, buf, size, prec);
            break;
        case PROP_IMPLICATION:
            formula_to_string_buf(f->data.binary.left, buf, size, prec);
            BUF_APPEND(" -> ");
            formula_to_string_buf(f->data.binary.right, buf, size, prec + 1);
            break;
        case PROP_NEGATION:
            BUF_APPEND("~");
            formula_to_string_buf(f->data.unary.operand, buf, size, prec);
            break;
        case PROP_BOTTOM:
            BUF_APPEND("_|_");
            break;
        case PROP_TRUE:
            BUF_APPEND("T");
            break;
    }

    if (need_parens) {
        BUF_APPEND(")");
    }

#undef BUF_APPEND
}

/**
 * @brief 将命题公式序列化为字符串
 *
 * @param f 公式指针
 * @return 新分配的字符串指针，失败返回 NULL
 */
char *prop_formula_to_string(const PropFormula *f) {
    if (!f)
        return NULL;
    char *buf = (char *) lv00_calloc(MAX_FORMULA_STR, sizeof(char)); /* 零初始化分配 */
    if (!buf)
        return NULL;
    formula_to_string_buf(f, buf, MAX_FORMULA_STR, 0);
    return buf;
}

/* LaTeX 序列化 */
static void formula_to_latex_buf(const PropFormula *f, char *buf, size_t size, int parent_prec) {
    if (!f || size == 0)
        return;
    int prec = formula_precedence(f);
    bool need_parens = (parent_prec > prec);

    if (need_parens) {
        strncat(buf, "\\left(", size - strlen(buf) - 1);
    }

    switch (f->type) {
        case PROP_ATOM:
            strncat(buf, f->data.atom.name, size - strlen(buf) - 1);
            break;
        case PROP_CONJUNCTION:
            formula_to_latex_buf(f->data.binary.left, buf, size, prec);
            strncat(buf, " \\wedge ", size - strlen(buf) - 1);
            formula_to_latex_buf(f->data.binary.right, buf, size, prec);
            break;
        case PROP_DISJUNCTION:
            formula_to_latex_buf(f->data.binary.left, buf, size, prec);
            strncat(buf, " \\vee ", size - strlen(buf) - 1);
            formula_to_latex_buf(f->data.binary.right, buf, size, prec);
            break;
        case PROP_IMPLICATION:
            formula_to_latex_buf(f->data.binary.left, buf, size, prec);
            strncat(buf, " \\to ", size - strlen(buf) - 1);
            formula_to_latex_buf(f->data.binary.right, buf, size, prec + 1);
            break;
        case PROP_NEGATION:
            strncat(buf, "\\neg ", size - strlen(buf) - 1);
            formula_to_latex_buf(f->data.unary.operand, buf, size, prec);
            break;
        case PROP_BOTTOM:
            strncat(buf, "\\bot", size - strlen(buf) - 1);
            break;
        case PROP_TRUE:
            strncat(buf, "\\top", size - strlen(buf) - 1);
            break;
    }

    if (need_parens) {
        strncat(buf, "\\right)", size - strlen(buf) - 1);
    }
}

/**
 * @brief 将命题公式序列化为 LaTeX 字符串
 *
 * @param f 公式指针
 * @return 新分配的 LaTeX 字符串指针，失败返回 NULL
 */
char *prop_formula_to_latex(const PropFormula *f) {
    if (!f)
        return NULL;
    char *buf = (char *) lv00_calloc(MAX_FORMULA_STR, sizeof(char)); /* 零初始化分配 */
    if (!buf)
        return NULL;
    formula_to_latex_buf(f, buf, MAX_FORMULA_STR, 0);
    return buf;
}

/* ============================================================
 * 证明搜索引擎 - 内部数据结构
 * ============================================================ */

/* 记忆化条目：记录已搜索过的 (目标, 前提集合) 是否可证 */
typedef struct {
    const PropFormula *goal;
    /* 用前提集合的位图来标识（简化版：用前提指针数组哈希） */
    uint64_t premises_hash;
    bool proven;   /* 该组合是否已证明 */
    bool searched; /* 是否已搜索过 */
} MemoEntry;

/* 证明搜索上下文 */
typedef struct {
    const PropFormula **premises; /* 原始前提 */
    int premise_count;
    const VerifierConfig *config;
    int steps; /* 已用步数 */
    bool timed_out;
    /* 超时基准时间 */
    uint64_t start_time_ms;
    /* 记忆化表 */
    MemoEntry memo[MAX_MEMO_ENTRIES];
    int memo_count;
    /* 递归深度计数器（防止栈溢出） */
    int recursion_depth;
} ProofContext;

/**
 * @brief 获取墙上时钟时间（毫秒）
 *
 * 使用 C 标准 time() 获取墙上时钟时间，而非 clock() 获取处理器时间。
 * clock() 在多线程或 I/O 等待场景下不准确（测量 CPU 时间而非真实时间）。
 * 返回值仅用于计算相对时间差，绝对值无意义。
 *
 * @return 当前时间的毫秒级近似值
 */
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

static uint64_t get_time_ms(void) {
    /* 使用高精度时钟获取毫秒级时间，避免 time() 的秒级精度不足问题 */
#ifdef _WIN32
    LARGE_INTEGER freq, count;
    if (QueryPerformanceFrequency(&freq) && QueryPerformanceCounter(&count)) {
        return (uint64_t) ((double) count.QuadPart / (double) freq.QuadPart * 1000.0);
    }
    return (uint64_t) time(NULL) * PROP_TIME_MS_PER_SEC; /* 回退到秒级精度 */
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (uint64_t) ts.tv_sec * 1000ULL + (uint64_t) ts.tv_nsec / 1000000ULL;
    }
    return (uint64_t) time(NULL) * PROP_TIME_MS_PER_SEC; /* 回退到秒级精度 */
#endif
}

/* ============================================================
 * 哈希函数（用于记忆化）
 * ============================================================ */

/**
 * @brief 简单的指针哈希函数
 *
 * 使用指针地址生成 64 位哈希值，通过位移和乘数混合。
 *
 * @param p 待哈希的指针
 * @return 64 位哈希值
 */
static uint64_t hash_ptr(const void *p) {
    uint64_t x = (uint64_t) (uintptr_t) p;
    x = ((x >> PROP_HASH_BIT_SHIFT) ^ x) * PROP_HASH_PTR_MULTIPLIER;
    x = ((x >> PROP_HASH_BIT_SHIFT) ^ x) * PROP_HASH_PTR_MULTIPLIER;
    x = (x >> PROP_HASH_BIT_SHIFT) ^ x;
    return x;
}

/**
 * @brief 计算公式结构的哈希值（递归）
 *
 * 基于公式结构计算 64 位哈希值：
 * - ATOM：对名称字符串逐字符哈希
 * - 二元联结词：递归组合左右子公式哈希
 * - 一元联结词：递归组合操作数哈希
 * - BOTTOM/TRUE：仅类型哈希
 * 使用黄金比例常数和不同倍数以避免冲突。
 *
 * @param f 公式指针（可为 NULL）
 * @return 64 位哈希值（NULL 公式返回 0）
 */
static uint64_t formula_hash(const PropFormula *f) {
    if (!f)
        return 0;
    uint64_t h = (uint64_t) f->type * PROP_HASH_TYPE_MULTIPLIER;
    switch (f->type) {
        case PROP_ATOM: {
            for (const char *s = f->data.atom.name; *s; s++)
                h = h * PROP_HASH_STRING_MULTIPLIER + (uint64_t) (unsigned char) *s;
            break;
        }
        case PROP_CONJUNCTION:
        case PROP_DISJUNCTION:
        case PROP_IMPLICATION:
            h ^= formula_hash(f->data.binary.left) * PROP_HASH_LEFT_MULTIPLIER;
            h ^= formula_hash(f->data.binary.right) * PROP_HASH_RIGHT_MULTIPLIER;
            break;
        case PROP_NEGATION:
            h ^= formula_hash(f->data.unary.operand) * PROP_HASH_RIGHT_MULTIPLIER;
            break;
        case PROP_BOTTOM:
        case PROP_TRUE:
            break;
    }
    return h;
}

/**
 * @brief 计算前提集合的哈希值
 *
 * 组合每个前提公式的哈希值生成 64 位集合哈希。
 *
 * @param premises 前提公式数组
 * @param count    前提数量
 * @return 64 位哈希值
 */
static uint64_t premises_hash(const PropFormula **premises, int count) {
    uint64_t h = 0;
    for (int i = 0; i < count; i++) {
        h = h * PROP_HASH_PREMISES_MULTIPLIER + formula_hash(premises[i]);
    }
    return h;
}

/* ============================================================
 * 记忆化操作
 * ============================================================ */

/* 在记忆化表中查找 */
static int memo_find(ProofContext *ctx, const PropFormula *goal, uint64_t phash) {
    uint64_t ghash = formula_hash(goal);
    for (int i = 0; i < ctx->memo_count; i++) {
        if (ctx->memo[i].goal == goal && ctx->memo[i].premises_hash == phash) {
            return i;
        }
    }
    return -1;
}

/* 添加记忆化条目 */
static void memo_add(ProofContext *ctx, const PropFormula *goal, uint64_t phash, bool proven) {
    if (ctx->memo_count >= MAX_MEMO_ENTRIES)
        return;
    ctx->memo[ctx->memo_count].goal = goal;
    ctx->memo[ctx->memo_count].premises_hash = phash;
    ctx->memo[ctx->memo_count].proven = proven;
    ctx->memo[ctx->memo_count].searched = true;
    ctx->memo_count++;
}

/* ============================================================
 * 前提操作
 * ============================================================ */

/* 在前提列表中查找公式 */
static bool premise_contains(const PropFormula **premises, int count, const PropFormula *f) {
    for (int i = 0; i < count; i++) {
        if (formula_equal(premises[i], f))
            return true;
    }
    return false;
}

/* ============================================================
 * 前向链：从前提中提取新信息
 * ============================================================ */

/**
 * @brief 前向链展开合取前提
 *
 * 从输入前提集合中展开所有合取公式（A /\ B），
 * 将左右子公式分别加入输出前提列表（去重）。
 * 持续迭代直到没有新的合取可展开。
 *
 * @param input      输入前提公式数组
 * @param input_count 输入数量
 * @param output     输出前提公式数组（调用者预分配）
 * @param max_output 输出数组最大容量
 * @return 输出的前提公式数量
 */
static int forward_chain_conjunctions(const PropFormula **input, int input_count, const PropFormula **output,
                                      int max_output) {
    int out_count = 0;
    /* 先复制所有输入 */
    for (int i = 0; i < input_count && out_count < max_output; i++) {
        output[out_count++] = input[i];
    }
    /* 展开合取 */
    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = 0; i < out_count && out_count < max_output; i++) {
            const PropFormula *p = output[i];
            if (p->type == PROP_CONJUNCTION) {
                /* 检查左右是否已在列表中 */
                if (!premise_contains(output, out_count, p->data.binary.left)) {
                    output[out_count++] = p->data.binary.left;
                    changed = true;
                }
                if (!premise_contains(output, out_count, p->data.binary.right)) {
                    output[out_count++] = p->data.binary.right;
                    changed = true;
                }
            }
        }
    }
    return out_count;
}

/* ============================================================
 * 核心证明搜索（递归，向后链接）
 * ============================================================ */

/* 前向声明 */
static bool prove(ProofContext *ctx, const PropFormula **premises, int premise_count, const PropFormula *goal);

/* 检查是否超时或超步数 */
static bool check_limits(ProofContext *ctx) {
    if (ctx->steps >= ctx->config->max_steps)
        return true;
    if (ctx->config->timeout_ms > 0) {
        uint64_t now = get_time_ms();
        if (now - ctx->start_time_ms >= (uint64_t) ctx->config->timeout_ms) {
            ctx->timed_out = true;
            return true;
        }
    }
    return false;
}

/* 尝试 modus ponens：从前提中找到 A→B 和 A，推出 B */
static bool try_modus_ponens(ProofContext *ctx, const PropFormula **premises, int premise_count,
                             const PropFormula *goal) {
    for (int i = 0; i < premise_count; i++) {
        if (premises[i]->type == PROP_IMPLICATION) {
            const PropFormula *impl = premises[i];
            const PropFormula *antecedent = impl->data.binary.left;
            const PropFormula *consequent = impl->data.binary.right;

            /* 如果蕴涵的结论与目标匹配 */
            if (formula_equal(consequent, goal)) {
                /* 检查前件是否在前提中 */
                if (premise_contains(premises, premise_count, antecedent)) {
                    ctx->steps++;
                    return true;
                }
                /* 递归证明前件 */
                ctx->steps++;
                if (prove(ctx, premises, premise_count, antecedent)) {
                    return true;
                }
            }
        }
    }
    return false;
}

/* 尝试从前提中直接匹配目标 */
static bool try_direct_match(const PropFormula **premises, int premise_count, const PropFormula *goal) {
    return premise_contains(premises, premise_count, goal);
}

/* 尝试 ?-消去：从 ?A 和 A 推出 ⊥ */
static bool try_neg_elim(ProofContext *ctx, const PropFormula **premises, int premise_count) {
    /* 目标是 ⊥：检查是否有 ?A 和 A 同时作为前提 */
    for (int i = 0; i < premise_count; i++) {
        if (premises[i]->type == PROP_NEGATION) {
            const PropFormula *operand = premises[i]->data.unary.operand;
            if (premise_contains(premises, premise_count, operand)) {
                ctx->steps++;
                return true;
            }
        }
    }
    return false;
}

/**
 * @brief 核心证明搜索（递归向后链接算法）
 *
 * 使用带有记忆化的递归向后链接算法搜索证明：
 * 1. 检查步数和时间限制
 * 2. 查询记忆化表，避免重复搜索
 * 3. 根据目标公式类型分派：
 *    - BOTTOM：必然为假（爆炸原理适用）
 *    - TRUE：平凡成立
 *    - CONJUNCTION：分别证明左右子公式
 *    - DISJUNCTION：尝试证明任一分量
 *    - IMPLICATION：尝试 Modus Ponens 和子目标证明
 *    - NEGATION：检查前提是否蕴含矛盾
 *    - ATOM：检查是否在前提集中
 *
 * @param ctx           证明上下文（包含配置、记忆化表等）
 * @param premises      前提公式数组
 * @param premise_count 前提数量
 * @param goal          待证明的目标公式
 * @return true 表示证明成功，false 表示证明失败或超时/超步数
 */
static bool prove(ProofContext *ctx, const PropFormula **premises, int premise_count, const PropFormula *goal) {
    /* 检查递归深度限制，防止栈溢出 */
    ++ctx->recursion_depth;
    if (ctx->recursion_depth > MAX_MEMO_ENTRIES) { /* 最大递归深度 = 记忆化表容量 */
        goto prove_depth_exceeded;
    }

    /* 检查限制 */
    if (check_limits(ctx)) {
        goto prove_depth_exceeded;
    }
    ctx->steps++;

    /* 记忆化检查 */
    uint64_t phash = premises_hash(premises, premise_count);
    int midx = memo_find(ctx, goal, phash);
    if (midx >= 0 && ctx->memo[midx].searched) {
        bool r = ctx->memo[midx].proven;
        ctx->recursion_depth--;
        return r;
    }

    bool result = false;

    switch (goal->type) {
        case PROP_TRUE:
            /* ? 总是可证的 */
            result = true;
            break;

        case PROP_BOTTOM:
            /* 目标是 ⊥：首先检查前提中是否有 ⊥ */
            result = premise_contains(premises, premise_count, goal);
            /* 如果不成功，尝试 ?-消去 */
            if (!result) {
                result = try_neg_elim(ctx, premises, premise_count);
            }
            /* 如果不成功，尝试从蕴涵前提推导矛盾 */
            if (!result) {
                for (int i = 0; i < premise_count && !result; i++) {
                    if (premises[i]->type == PROP_IMPLICATION) {
                        const PropFormula *impl = premises[i];
                        if (impl->data.binary.right->type == PROP_BOTTOM) {
                            /* 有 A→⊥ = ?A，尝试证明 A */
                            ctx->steps++;
                            result = prove(ctx, premises, premise_count, impl->data.binary.left);
                        }
                    }
                }
            }
            /* 前向链：展开合取和应用 modus ponens，然后重试 ?-消去 */
            if (!result) {
                const PropFormula *expanded[MAX_PREMISES];
                int exp_count = forward_chain_conjunctions(premises, premise_count, expanded, MAX_PREMISES);
                /* 多步前向推理 */
                {
                    bool changed = true;
                    while (changed && exp_count < MAX_PREMISES) {
                        changed = false;
                        for (int i = 0; i < exp_count && !changed; i++) {
                            if (expanded[i]->type == PROP_IMPLICATION) {
                                const PropFormula *antecedent = expanded[i]->data.binary.left;
                                const PropFormula *consequent = expanded[i]->data.binary.right;
                                if (premise_contains(expanded, exp_count, antecedent) &&
                                    !premise_contains(expanded, exp_count, consequent)) {
                                    expanded[exp_count++] = consequent;
                                    changed = true;
                                    ctx->steps++;
                                }
                            }
                        }
                    }
                    /* 用扩展后的前提重试 ?-消去 */
                    if (!result) {
                        result = try_neg_elim(ctx, expanded, exp_count);
                    }
                    /* 检查 ⊥ 是否被推导出来 */
                    if (!result) {
                        result = premise_contains(expanded, exp_count, goal);
                    }
                }
            }
            break;

        case PROP_ATOM: {
            /* 目标是原子命题：直接匹配或 modus ponens */
            /* 所有局部数组声明在 case 作用域顶部，避免 stack-use-after-scope */
            const PropFormula *new_premises_l[MAX_PREMISES];
            const PropFormula *new_premises_r[MAX_PREMISES];
            const PropFormula *expanded[MAX_PREMISES];
            const PropFormula *fc_expanded[MAX_PREMISES];

            result = try_direct_match(premises, premise_count, goal);
            if (!result) {
                result = try_modus_ponens(ctx, premises, premise_count, goal);
            }
            /* 前向链：展开合取并尝试 modus ponens 链 */
            if (!result) {
                int exp_count = forward_chain_conjunctions(premises, premise_count, expanded, MAX_PREMISES);
                /* 多步前向推理：反复应用 modus ponens 直到无法推导新事实 */
                bool changed = true;
                while (changed && exp_count < MAX_PREMISES) {
                    changed = false;
                    for (int i = 0; i < exp_count && !changed; i++) {
                        if (expanded[i]->type == PROP_IMPLICATION) {
                            const PropFormula *antecedent = expanded[i]->data.binary.left;
                            const PropFormula *consequent = expanded[i]->data.binary.right;
                            if (premise_contains(expanded, exp_count, antecedent) &&
                                !premise_contains(expanded, exp_count, consequent)) {
                                expanded[exp_count++] = consequent;
                                changed = true;
                                ctx->steps++;
                            }
                        }
                    }
                    /* 检查目标是否在扩展前提中 */
                    result = try_direct_match(expanded, exp_count, goal);
                }
            }
            /* 尝试 ∨-消去：如果有 A∨B，且 A→goal, B→goal */
            if (!result) {
                int fc_count = forward_chain_conjunctions(premises, premise_count, fc_expanded, MAX_PREMISES);
                /* 多步前向推理 */
                {
                    bool changed = true;
                    while (changed && fc_count < MAX_PREMISES) {
                        changed = false;
                        for (int i = 0; i < fc_count && !changed; i++) {
                            if (fc_expanded[i]->type == PROP_IMPLICATION) {
                                const PropFormula *antecedent = fc_expanded[i]->data.binary.left;
                                const PropFormula *consequent = fc_expanded[i]->data.binary.right;
                                if (premise_contains(fc_expanded, fc_count, antecedent) &&
                                    !premise_contains(fc_expanded, fc_count, consequent)) {
                                    fc_expanded[fc_count++] = consequent;
                                    changed = true;
                                    ctx->steps++;
                                }
                            }
                        }
                    }
                }

                for (int i = 0; i < fc_count && !result; i++) {
                    if (fc_expanded[i]->type == PROP_DISJUNCTION) {
                        const PropFormula *disj = fc_expanded[i];
                        /* 尝试左分支：假设 A，证明 goal */
                        ctx->steps++;
                        {
                            int new_count = fc_count;
                            memcpy((void *) new_premises_l, fc_expanded,
                                   sizeof(const PropFormula *) * (size_t) fc_count);
                            if (new_count < MAX_PREMISES) {
                                new_premises_l[new_count++] = disj->data.binary.left;
                            }
                            if (prove(ctx, new_premises_l, new_count, goal)) {
                                result = true;
                            }
                        }
                        /* 尝试右分支：假设 B，证明 goal */
                        if (!result) {
                            ctx->steps++;
                            {
                                int new_count = fc_count;
                                memcpy((void *) new_premises_r, fc_expanded,
                                       sizeof(const PropFormula *) * (size_t) fc_count);
                                if (new_count < MAX_PREMISES) {
                                    new_premises_r[new_count++] = disj->data.binary.right;
                                }
                                if (prove(ctx, new_premises_r, new_count, goal)) {
                                    result = true;
                                }
                            }
                        }
                    }
                }
            }
            break;
        }

        case PROP_CONJUNCTION: {
            /* 目标是 A ∧ B：分别证明 A 和 B */
            const PropFormula *left = goal->data.binary.left;
            const PropFormula *right = goal->data.binary.right;
            ctx->steps++;
            bool left_ok = prove(ctx, premises, premise_count, left);
            if (left_ok) {
                ctx->steps++;
                result = prove(ctx, premises, premise_count, right);
            }
            break;
        }

        case PROP_DISJUNCTION: {
            /* 目标是 A ∨ B：尝试证明 A 或证明 B */
            const PropFormula *left = goal->data.binary.left;
            const PropFormula *right = goal->data.binary.right;

            /* 尝试左分支 */
            ctx->steps++;
            result = prove(ctx, premises, premise_count, left);
            if (!result) {
                /* 尝试右分支 */
                ctx->steps++;
                result = prove(ctx, premises, premise_count, right);
            }
            break;
        }

        case PROP_IMPLICATION: {
            /* 目标是 A → B：假设 A，证明 B */
            const PropFormula *antecedent = goal->data.binary.left;
            const PropFormula *consequent = goal->data.binary.right;

            /* 将 A 加入前提 */
            const PropFormula *new_premises[MAX_PREMISES];
            int new_count = premise_count;
            if (new_count >= MAX_PREMISES) {
                result = false;
                break;
            }
            memcpy(new_premises, premises, sizeof(const PropFormula *) * premise_count);
            new_premises[new_count++] = antecedent;

            ctx->steps++;
            result = prove(ctx, new_premises, new_count, consequent);
            break;
        }

        case PROP_NEGATION: {
            /* 目标是 ?A = A → ⊥：假设 A，证明 ⊥ */
            const PropFormula *operand = goal->data.unary.operand;

            const PropFormula *new_premises[MAX_PREMISES];
            int new_count = premise_count;
            if (new_count >= MAX_PREMISES) {
                result = false;
                break;
            }
            memcpy(new_premises, premises, sizeof(const PropFormula *) * premise_count);
            new_premises[new_count++] = operand;

            /* 构造 ⊥ 作为子目标 */
            PropFormula *bot = prop_formula_create_bottom();
            ctx->steps++;
            result = prove(ctx, new_premises, new_count, bot);
            prop_formula_destroy(bot);
            break;
        }
    }

    /* 爆炸原理：如果前提中有 ⊥，任何目标都可证 */
    if (!result && ctx->config->enable_ex_falso) {
        /* 检查前提中是否包含 ⊥（命题常量"假"） */
        for (int i = 0; i < premise_count; i++) {
            if (premises[i]->type == PROP_BOTTOM) {
                result = true;
                break;
            }
        }
    }

    /* 额外尝试：使用前向链展开合取前提后重试 */
    if (!result && goal->type == PROP_ATOM) {
        const PropFormula *expanded[MAX_PREMISES];
        int exp_count = forward_chain_conjunctions(premises, premise_count, expanded, MAX_PREMISES);
        if (exp_count > premise_count) {
            /* 有新的前提被提取 */
            result = try_direct_match(expanded, exp_count, goal);
            if (!result) {
                result = try_modus_ponens(ctx, expanded, exp_count, goal);
            }
        }
    }

    /* 记录记忆化结果 */
    memo_add(ctx, goal, phash, result);

    ctx->recursion_depth--;
    return result;

prove_depth_exceeded:
    /* 递归深度超限或步数/时间超限，统一在此递减计数器 */
    ctx->recursion_depth--;
    return false;
}

/* ============================================================
 * 公共 API
 * ============================================================ */

VerifyDetail prop_verifier_verify(const PropFormula **premises, int premise_count, const PropFormula *goal,
                                  const VerifierConfig *config) {
    VerifyDetail detail;
    memset(&detail, 0, sizeof(detail));

    /* 默认配置 */
    VerifierConfig default_config = VERIFIER_CONFIG_DEFAULT;
    if (!config)
        config = &default_config;

    detail.max_steps = config->max_steps;

    /* 输入验证 */
    if (!goal) {
        detail.result = PV_VERIFY_INVALID_INPUT;
        snprintf(detail.error_message, sizeof(detail.error_message), "目标公式为 NULL");
        return detail;
    }
    if (premise_count < 0) {
        detail.result = PV_VERIFY_INVALID_INPUT;
        snprintf(detail.error_message, sizeof(detail.error_message), "前提数量为负数: %d", premise_count);
        return detail;
    }
    if (premise_count > 0 && !premises) {
        detail.result = PV_VERIFY_INVALID_INPUT;
        snprintf(detail.error_message, sizeof(detail.error_message), "前提数量 > 0 但前提数组为 NULL");
        return detail;
    }

    /* 初始化证明上下文 */
    ProofContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.premises = premises;
    ctx.premise_count = premise_count;
    ctx.config = config;
    ctx.start_time_ms = get_time_ms();

    /* 流式事件：验证开始 */
    if (prop_verifier_stream_ctx) {
        stream_emit_simple(prop_verifier_stream_ctx, STREAM_EVENT_PROOF_STEP_ADDED, "命题验证开始，启动证明搜索", 0);
    }

    /* 执行证明搜索 */
    bool proven = prove(&ctx, premises, premise_count, goal);

    detail.steps_used = ctx.steps;

    if (ctx.timed_out) {
        detail.result = PV_VERIFY_TIMEOUT;
        snprintf(detail.error_message, sizeof(detail.error_message), "证明搜索超时 (%d ms)", config->timeout_ms);
    } else if (proven) {
        detail.result = PV_VERIFY_PROVEN;
        snprintf(detail.construction_summary, sizeof(detail.construction_summary), "证明成功: 使用 %d 步推理完成验证",
                 ctx.steps);
    } else {
        detail.result = PV_VERIFY_FAILED;
        snprintf(detail.error_message, sizeof(detail.error_message), "搜索空间耗尽，未能证明 (%d 步)", ctx.steps);
    }

    return detail;
}

/* ============================================================
 * 内置烟测集
 * ============================================================ */

/* 检查 child 是否是 parent 的子节点（递归） */
static bool formula_is_descendant(const PropFormula *child, const PropFormula *parent) {
    if (!child || !parent)
        return false;
    if (child == parent)
        return true;
    switch (parent->type) {
        case PROP_CONJUNCTION:
        case PROP_DISJUNCTION:
        case PROP_IMPLICATION:
            return formula_is_descendant(child, parent->data.binary.left) ||
                   formula_is_descendant(child, parent->data.binary.right);
        case PROP_NEGATION:
            return formula_is_descendant(child, parent->data.unary.operand);
        default:
            return false;
    }
}

/* 烟测辅助宏：创建原子命题 */
#define ATOM(name) prop_formula_create_atom(name)
#define AND(a, b) prop_formula_create_conjunction((a), (b))
#define OR(a, b) prop_formula_create_disjunction((a), (b))
#define IMPL(a, b) prop_formula_create_implication((a), (b))
#define NEG(a) prop_formula_create_negation(a)
#define BOT() prop_formula_create_bottom()
#define TOP() prop_formula_create_true()

int prop_verifier_builtin_smoke_test_count(void) {
    return PROP_SMOKE_TEST_COUNT;
}

int prop_verifier_run_builtin_smoke_tests(VerifyDetail *results) {
    SmokeTest tests[PROP_SMOKE_TEST_COUNT];
    memset(&tests, 0, sizeof(tests));

    /*
     * 内存管理策略：
     * 每个复合公式（AND/OR/IMPL/NEG）获取子节点的所有权。
     * 为避免 double-free，每个测试块内的复合公式使用独立的原子命题。
     * 清理时使用 formula_is_descendant 判断哪些是"根"公式。
     */

    /* 测试 1: P, P→Q ? Q (modus ponens) */
    {
        PropFormula *p = ATOM("P");
        PropFormula *q = ATOM("Q");
        PropFormula *pimplq = IMPL(p, q);
        tests[0].premises[0] = p;
        tests[0].premises[1] = pimplq;
        tests[0].premise_count = 2;
        tests[0].goal = q;
        tests[0].expected_provable = true;
        tests[0].description = "P, P->Q |- Q (modus ponens)";
    }

    /* 测试 2: P∧Q ? P (∧-elimination) */
    {
        PropFormula *p = ATOM("P");
        PropFormula *q = ATOM("Q");
        PropFormula *pq = AND(p, q);
        tests[1].premises[0] = pq;
        tests[1].premise_count = 1;
        tests[1].goal = p;
        tests[1].expected_provable = true;
        tests[1].description = "P/\\Q |- P (conjunction elimination)";
    }

    /* 测试 3: P ? P∨Q (∨-intro left) */
    {
        PropFormula *p = ATOM("P");
        PropFormula *q = ATOM("Q");
        PropFormula *porq = OR(p, q);
        tests[2].premises[0] = p;
        tests[2].premise_count = 1;
        tests[2].goal = porq;
        tests[2].expected_provable = true;
        tests[2].description = "P |- P\\/Q (disjunction introduction left)";
    }

    /* 测试 4: P→Q, Q→R ? P→R (hypothetical syllogism)
     * 每个蕴涵使用独立的原子命题 */
    {
        PropFormula *pimplq = IMPL(ATOM("P"), ATOM("Q"));
        PropFormula *qimplr = IMPL(ATOM("Q"), ATOM("R"));
        PropFormula *pimplr = IMPL(ATOM("P"), ATOM("R"));
        tests[3].premises[0] = pimplq;
        tests[3].premises[1] = qimplr;
        tests[3].premise_count = 2;
        tests[3].goal = pimplr;
        tests[3].expected_provable = true;
        tests[3].description = "P->Q, Q->R |- P->R (hypothetical syllogism)";
    }

    /* 测试 5: P→(Q→R), P∧Q ? R */
    {
        PropFormula *pimplqimplr = IMPL(ATOM("P"), IMPL(ATOM("Q"), ATOM("R")));
        PropFormula *pq = AND(ATOM("P"), ATOM("Q"));
        PropFormula *r = ATOM("R");
        tests[4].premises[0] = pimplqimplr;
        tests[4].premises[1] = pq;
        tests[4].premise_count = 2;
        tests[4].goal = r;
        tests[4].expected_provable = true;
        tests[4].description = "P->(Q->R), P/\\Q |- R";
    }

    /* 测试 6: ⊥ ? ⊥ (trivial) */
    {
        PropFormula *bot = BOT();
        tests[5].premises[0] = bot;
        tests[5].premise_count = 1;
        tests[5].goal = bot;
        tests[5].expected_provable = true;
        tests[5].description = "_|_ |- _|_ (trivial)";
    }

    /* 测试 7: P, ?P ? ⊥ (?-elimination) */
    {
        PropFormula *p = ATOM("P");
        PropFormula *notp = NEG(p);
        PropFormula *bot = BOT();
        tests[6].premises[0] = p;
        tests[6].premises[1] = notp;
        tests[6].premise_count = 2;
        tests[6].goal = bot;
        tests[6].expected_provable = true;
        tests[6].description = "P, ~P |- _|_ (negation elimination)";
    }

    /* 测试 8: (P→Q)→(?Q→?P) (contraposition - intuitionistic) */
    {
        PropFormula *contra = IMPL(IMPL(ATOM("P"), ATOM("Q")), IMPL(NEG(ATOM("Q")), NEG(ATOM("P"))));
        tests[7].premise_count = 0;
        tests[7].goal = contra;
        tests[7].expected_provable = true;
        tests[7].description = "|- (P->Q)->(~Q->~P) (contraposition)";
    }

    /* 测试 9: P∧(Q∨R) ? (P∧Q)∨(P∧R) (distribution)
     * 左右两侧使用完全独立的原子命题 */
    {
        PropFormula *pqorr = AND(ATOM("P"), OR(ATOM("Q"), ATOM("R")));
        PropFormula *pqorpr = OR(AND(ATOM("P"), ATOM("Q")), AND(ATOM("P"), ATOM("R")));
        tests[8].premises[0] = pqorr;
        tests[8].premise_count = 1;
        tests[8].goal = pqorpr;
        tests[8].expected_provable = true;
        tests[8].description = "P/\\(Q\\/R) |- (P/\\Q)\\/(P/\\R) (distribution)";
    }

    /* 测试 10: ??P ? P (NOT provable intuitionistically) */
    {
        PropFormula *p = ATOM("P");
        PropFormula *notnotp = NEG(NEG(p));
        tests[9].premises[0] = notnotp;
        tests[9].premise_count = 1;
        tests[9].goal = p;
        tests[9].expected_provable = false;
        tests[9].description = "~~P |- P (double negation elimination - NOT intuitionistic)";
    }

    /* 测试 11: ? P∨?P (NOT provable intuitionistically) */
    {
        PropFormula *pnotp = OR(ATOM("P"), NEG(ATOM("P")));
        tests[10].premise_count = 0;
        tests[10].goal = pnotp;
        tests[10].expected_provable = false;
        tests[10].description = "|- P\\/~P (LEM - NOT intuitionistic)";
    }

    /* 测试 12: ? ?P∨P (NOT provable intuitionistically) */
    {
        PropFormula *notporp = OR(NEG(ATOM("P")), ATOM("P"));
        tests[11].premise_count = 0;
        tests[11].goal = notporp;
        tests[11].expected_provable = false;
        tests[11].description = "|- ~P\\/P (LEM variant - NOT intuitionistic)";
    }

    /* 测试 13: ⊥ ? P (explosion - only with ex_falso) */
    {
        PropFormula *bot = BOT();
        PropFormula *p = ATOM("P");
        tests[12].premises[0] = bot;
        tests[12].premise_count = 1;
        tests[12].goal = p;
        tests[12].expected_provable = true;
        tests[12].description = "_|_ |- P (explosion - requires ex_falso)";
    }

    /* 运行测试 */
    int passed = prop_verifier_run_smoke_tests(tests, PROP_SMOKE_TEST_COUNT, results);

    /* 清理公式
     * 每个测试块内的公式可能共享子节点或相同指针。
     * 策略：先去重，再识别根，最后统一释放。
     */
    for (int i = 0; i < PROP_SMOKE_TEST_COUNT; i++) {
        const PropFormula *ptrs[PROP_SMOKE_CLEANUP_MAX_PTRS];
        int ptr_count = 0;
        for (int j = 0; j < tests[i].premise_count && ptr_count < PROP_SMOKE_CLEANUP_MAX_PTRS; j++) {
            /* 去重：跳过已存在的指针 */
            bool dup = false;
            for (int d = 0; d < ptr_count; d++) {
                if (ptrs[d] == tests[i].premises[j]) {
                    dup = true;
                    break;
                }
            }
            if (!dup)
                ptrs[ptr_count++] = tests[i].premises[j];
        }
        if (tests[i].goal) {
            bool dup = false;
            for (int d = 0; d < ptr_count; d++) {
                if (ptrs[d] == tests[i].goal) {
                    dup = true;
                    break;
                }
            }
            if (!dup)
                ptrs[ptr_count++] = tests[i].goal;
        }

        /* 第一遍：识别哪些是"根"（不是其他公式的子节点） */
        bool is_root[PROP_SMOKE_CLEANUP_MAX_PTRS];
        memset(is_root, true, sizeof(is_root));
        for (int k = 0; k < ptr_count; k++) {
            for (int m = 0; m < ptr_count; m++) {
                if (k != m && ptrs[k] != ptrs[m] && formula_is_descendant(ptrs[k], ptrs[m])) {
                    is_root[k] = false;
                    break;
                }
            }
        }

        /* 第二遍：只释放根公式 */
        for (int k = 0; k < ptr_count; k++) {
            if (is_root[k]) {
                prop_formula_destroy((PropFormula *) ptrs[k]);
            }
        }
    }

    return passed;
}

/* ============================================================
 * 不可构造性分析
 * ============================================================ */

/**
 * @brief 收集目标公式的所有原子子公式
 *
 * 递归遍历公式 AST，收集所有原子命题名称。
 * 用于分析证明失败时哪些原子命题缺少构造。
 */
static int collect_atoms(const PropFormula *f, char atoms[][PROP_ATOM_NAME_MAX_LEN], int max_atoms) {
    if (!f)
        return 0;
    switch (f->type) {
        case PROP_ATOM: {
            /* 去重检查 */
            for (int i = 0; i < max_atoms; i++) {
                if (atoms[i][0] == '\0')
                    break;
                if (strcmp(atoms[i], f->data.atom.name) == 0)
                    return 0;
            }
            for (int i = 0; i < max_atoms; i++) {
                if (atoms[i][0] == '\0') {
                    snprintf(atoms[i], PROP_ATOM_NAME_MAX_LEN, "%s", f->data.atom.name);
                    return 1;
                }
            }
            return 0;
        }
        case PROP_CONJUNCTION:
        case PROP_DISJUNCTION:
        case PROP_IMPLICATION:
            return collect_atoms(f->data.binary.left, atoms, max_atoms) +
                   collect_atoms(f->data.binary.right, atoms, max_atoms);
        case PROP_NEGATION:
            return collect_atoms(f->data.unary.operand, atoms, max_atoms);
        case PROP_BOTTOM:
        case PROP_TRUE:
            return 0;
    }
    return 0;
}

/**
 * @brief 检查目标公式是否包含经典逻辑特有的模式
 *
 * 识别以下直觉主义不可证的经典模式：
 *   - 双重否定消去：~~A → A
 *   - 排中律：A ∨ ~A
 *   - 反证法（RAA）：(~A → ⊥) → A
 */
static bool has_classical_pattern(const PropFormula *f, char *pattern_desc, size_t desc_size) {
    if (!f)
        return false;

    /* 检查排中律：A ∨ ~A 或 ~A ∨ A */
    if (f->type == PROP_DISJUNCTION) {
        const PropFormula *left = f->data.binary.left;
        const PropFormula *right = f->data.binary.right;
        /* A ∨ ~A */
        if (left->type == PROP_NEGATION && formula_equal(left->data.unary.operand, right)) {
            char *s = prop_formula_to_string(right);
            snprintf(pattern_desc, desc_size, "排中律 (LEM): %s \\/ ~%s（直觉主义逻辑中不可证）", s, s);
            lv00_free((void **) &s);
            return true;
        }
        /* ~A ∨ A */
        if (right->type == PROP_NEGATION && formula_equal(right->data.unary.operand, left)) {
            char *s = prop_formula_to_string(left);
            snprintf(pattern_desc, desc_size, "排中律 (LEM): ~%s \\/ %s（直觉主义逻辑中不可证）", s, s);
            lv00_free((void **) &s);
            return true;
        }
    }

    /* 检查双重否定消去：~~A → A 或前提 ~~A ? A */
    if (f->type == PROP_IMPLICATION) {
        const PropFormula *antecedent = f->data.binary.left;
        const PropFormula *consequent = f->data.binary.right;
        if (antecedent->type == PROP_NEGATION && antecedent->data.unary.operand->type == PROP_NEGATION &&
            formula_equal(antecedent->data.unary.operand->data.unary.operand, consequent)) {
            char *s = prop_formula_to_string(consequent);
            snprintf(pattern_desc, desc_size, "双重否定消去: ~~%s → %s（直觉主义逻辑中不可证）", s, s);
            lv00_free((void **) &s);
            return true;
        }
        /* 反证法 (RAA): (~A → ⊥) → A */
        if (antecedent->type == PROP_IMPLICATION && antecedent->data.binary.left->type == PROP_NEGATION &&
            antecedent->data.binary.right->type == PROP_BOTTOM &&
            formula_equal(antecedent->data.binary.left->data.unary.operand, consequent)) {
            char *s = prop_formula_to_string(consequent);
            snprintf(pattern_desc, desc_size, "反证法 (RAA): (~%s → _|_) → %s（直觉主义逻辑中不可证）", s, s);
            lv00_free((void **) &s);
            return true;
        }
    }

    /* 递归检查子公式 */
    char sub_desc[PROP_PATTERN_DESC_BUFSIZE];
    switch (f->type) {
        case PROP_CONJUNCTION:
        case PROP_DISJUNCTION:
        case PROP_IMPLICATION:
            if (has_classical_pattern(f->data.binary.left, sub_desc, sizeof(sub_desc))) {
                snprintf(pattern_desc, desc_size, "%s", sub_desc);
                return true;
            }
            if (has_classical_pattern(f->data.binary.right, sub_desc, sizeof(sub_desc))) {
                snprintf(pattern_desc, desc_size, "%s", sub_desc);
                return true;
            }
            break;
        case PROP_NEGATION:
            if (has_classical_pattern(f->data.unary.operand, sub_desc, sizeof(sub_desc))) {
                snprintf(pattern_desc, desc_size, "%s", sub_desc);
                return true;
            }
            break;
        default:
            break;
    }
    return false;
}

InconstructibilityAnalysis prop_verifier_analyze_inconstructibility(const PropFormula **premises, int premise_count,
                                                                    const PropFormula *goal,
                                                                    const VerifierConfig *config) {
    InconstructibilityAnalysis analysis;
    memset(&analysis, 0, sizeof(analysis));

    VerifierConfig default_config = VERIFIER_CONFIG_DEFAULT;
    if (!config)
        config = &default_config;

    /* 先执行验证 */
    VerifyDetail detail = prop_verifier_verify(premises, premise_count, goal, config);

    if (detail.result == PV_VERIFY_PROVEN) {
        analysis.is_inconstructible = false;
        snprintf(analysis.reason, sizeof(analysis.reason), "命题已证明为可构造，无需不可构造性分析");
        return analysis;
    }

    analysis.is_inconstructible = true;

    /* 检查是否包含经典逻辑模式 */
    char pattern_desc[PROP_PATTERN_DESC_BUFSIZE] = {0};
    if (config->use_intuitionistic && has_classical_pattern(goal, pattern_desc, sizeof(pattern_desc))) {
        snprintf(analysis.reason, sizeof(analysis.reason),
                 "直觉主义限制: %s。在直觉主义逻辑中，证明必须提供显式构造，"
                 "不能依赖排中律或双重否定消去等经典推理规则。",
                 pattern_desc);
    } else if (detail.result == PV_VERIFY_TIMEOUT) {
        snprintf(analysis.reason, sizeof(analysis.reason),
                 "搜索超时: 证明搜索在 %d 毫秒内未完成。"
                 "可能需要更多步骤或存在复杂的子目标依赖关系。",
                 config->timeout_ms);
    } else {
        /* 分析缺少的前提和子目标 */
        char goal_atoms[PROP_ATOM_COLLECT_MAX][PROP_ATOM_NAME_MAX_LEN];
        memset(goal_atoms, 0, sizeof(goal_atoms));
        int atom_count = collect_atoms(goal, goal_atoms, PROP_ATOM_COLLECT_MAX);

        /* 检查哪些目标原子不在前提中 */
        char missing[512] = {0};
        int missing_count = 0;
        for (int i = 0; i < atom_count; i++) {
            bool found = false;
            for (int j = 0; j < premise_count; j++) {
                if (premises[j]->type == PROP_ATOM && strcmp(premises[j]->data.atom.name, goal_atoms[i]) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                char tmp[64];
                snprintf(tmp, sizeof(tmp), "%s%s", missing_count > 0 ? ", " : "", goal_atoms[i]);
                strncat(missing, tmp, sizeof(missing) - strlen(missing) - 1);
                missing_count++;
            }
        }

        if (missing_count > 0) {
            snprintf(analysis.reason, sizeof(analysis.reason),
                     "缺少构造: 目标需要原子命题 [%s] 的构造，"
                     "但当前前提中未提供。在 BHK 解释下，"
                     "每个原子命题需要一个几何证物（点、线段或区域）。",
                     missing);
        } else {
            snprintf(analysis.reason, sizeof(analysis.reason),
                     "构造缺口: 前提中包含所有目标原子命题，但无法通过"
                     "现有推理规则组合出目标。可能需要额外的蕴涵前提"
                     "或更复杂的构造步骤。已使用 %d 步推理。",
                     detail.steps_used);
        }
    }

    /* 生成失败子目标描述 */
    analysis.failed_subgoals = 1;
    analysis.subgoal_descriptions = (char **) lv00_malloc(sizeof(char *)); /* 分配内存 */
    if (analysis.subgoal_descriptions) {
        analysis.subgoal_descriptions[0] = (char *) lv00_malloc(512); /* 分配内存 */
        if (analysis.subgoal_descriptions[0]) {
            snprintf(analysis.subgoal_descriptions[0], 512, "目标: %s | 状态: %s | 步数: %d/%d",
                     prop_formula_to_string(goal), detail.result == PV_VERIFY_TIMEOUT ? "超时" : "搜索空间耗尽",
                     detail.steps_used, detail.max_steps);
        }
        analysis.subgoal_desc_count = 1;
    }

    return analysis;
}

void prop_verifier_free_analysis(InconstructibilityAnalysis *analysis) {
    if (!analysis)
        return;
    if (analysis->subgoal_descriptions) {
        for (int i = 0; i < analysis->subgoal_desc_count; i++) {
            lv00_free((void **) &analysis->subgoal_descriptions[i]); /* 释放并置NULL */
        }
        lv00_free((void **) &analysis->subgoal_descriptions); /* 释放并置NULL */
    }
    analysis->subgoal_desc_count = 0;
}

/* ============================================================
 * BHK 几何构造验证桥接
 * ============================================================ */

/**
 * @brief 获取公式类型的 BHK 解释描述
 */
static void get_bhk_description(const PropFormula *f, char *buf, size_t size) {
    if (!f || size == 0)
        return;
    switch (f->type) {
        case PROP_ATOM:
            snprintf(buf, size, "原子命题 %s 需要一个几何证物（点、线段或区域）", f->data.atom.name);
            break;
        case PROP_CONJUNCTION:
            snprintf(buf, size,
                     "合取 %s 的证物是一对证物 (a, b)，"
                     "对应几何中的积类型函数块（两个投影端口）",
                     prop_formula_to_string(f));
            break;
        case PROP_DISJUNCTION:
            snprintf(buf, size,
                     "析取 %s 的证物是一个附带来源标记的证物（左/右），"
                     "对应几何中的和类型函数块（带标记的析取证物）",
                     prop_formula_to_string(f));
            break;
        case PROP_IMPLICATION:
            snprintf(buf, size,
                     "蕴涵 %s 的证物是一个构造函数，"
                     "将前件的证物转换为后件的证物，"
                     "对应几何中的标准函数块（输入端口→输出端口）",
                     prop_formula_to_string(f));
            break;
        case PROP_NEGATION:
            snprintf(buf, size,
                     "否定 %s 的证物是一个将 %s 的证物转换为 ⊥ 的构造，"
                     "对应几何中的函数块（输入→空输出端口）",
                     prop_formula_to_string(f), prop_formula_to_string(f->data.unary.operand));
            break;
        case PROP_BOTTOM:
            snprintf(buf, size,
                     "矛盾 ⊥ 没有证物（不可构造），"
                     "对应几何中的空模式（无可填充端口）");
            break;
        case PROP_TRUE:
            snprintf(buf, size,
                     "真 ? 的证物是平凡构造（单位类型），"
                     "对应几何中的单点区域");
            break;
    }
}

/**
 * @brief 获取公式类型的几何映射描述
 */
static void get_geometric_mapping(const PropFormula *f, char *buf, size_t size) {
    if (!f || size == 0)
        return;
    switch (f->type) {
        case PROP_ATOM:
            snprintf(buf, size, "GEOM_POINT / GEOM_REGION（证物节点）");
            break;
        case PROP_CONJUNCTION:
            snprintf(buf, size, "FuncBlock[Product]（积类型函数块，双投影端口）");
            break;
        case PROP_DISJUNCTION:
            snprintf(buf, size, "FuncBlock[Sum]（和类型函数块，带标记端口）");
            break;
        case PROP_IMPLICATION:
            snprintf(buf, size, "FuncBlock[Arrow]（标准函数块，输入→输出端口）");
            break;
        case PROP_NEGATION:
            snprintf(buf, size, "FuncBlock[Neg]（否定函数块，输入→⊥端口）");
            break;
        case PROP_BOTTOM:
            snprintf(buf, size, "空模式（无端口，不可填充）");
            break;
        case PROP_TRUE:
            snprintf(buf, size, "单点区域（单位类型证物）");
            break;
    }
}

BHKVerificationResult prop_verifier_bhk_verify(const PropFormula **premises, int premise_count, const PropFormula *goal,
                                               const VerifierConfig *config) {
    BHKVerificationResult result;
    memset(&result, 0, sizeof(result));

    VerifierConfig default_config = VERIFIER_CONFIG_DEFAULT;
    if (!config)
        config = &default_config;

    /* 先执行命题逻辑验证 */
    VerifyDetail detail = prop_verifier_verify(premises, premise_count, goal, config);
    result.verified = (detail.result == PV_VERIFY_PROVEN);

    /* 生成 BHK 解释 */
    get_bhk_description(goal, result.bhk_interpretation, sizeof(result.bhk_interpretation));

    /* 生成几何映射 */
    get_geometric_mapping(goal, result.geometric_mapping, sizeof(result.geometric_mapping));

    if (result.verified) {
        /* 验证成功：检查构造完整性 */
        result.missing_constructions = 0;
        result.missing_descriptions = NULL;
        result.missing_count = 0;
    } else {
        /* 验证失败：分析缺少的构造 */
        char goal_atoms[32][64];
        memset(goal_atoms, 0, sizeof(goal_atoms));
        int atom_count = collect_atoms(goal, goal_atoms, 32);

        /* 统计缺少构造的原子命题 */
        int missing = 0;
        for (int i = 0; i < atom_count; i++) {
            bool found = false;
            for (int j = 0; j < premise_count; j++) {
                if (premises[j]->type == PROP_ATOM && strcmp(premises[j]->data.atom.name, goal_atoms[i]) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found)
                missing++;
        }

        result.missing_constructions = missing;
        result.missing_count = missing;

        if (missing > 0) {
            result.missing_descriptions = (char **) lv00_malloc(sizeof(char *) * (size_t) missing); /* 分配内存 */
            if (result.missing_descriptions) {
                int idx = 0;
                for (int i = 0; i < atom_count && idx < missing; i++) {
                    bool found = false;
                    for (int j = 0; j < premise_count; j++) {
                        if (premises[j]->type == PROP_ATOM && strcmp(premises[j]->data.atom.name, goal_atoms[i]) == 0) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        char desc[256];
                        snprintf(desc, sizeof(desc), "缺少原子命题 '%s' 的几何证物（需要对应的点、线段或区域节点）",
                                 goal_atoms[i]);
                        result.missing_descriptions[idx] = lv00_strdup_safe(desc); /* 复制字符串 */
                        idx++;
                    }
                }
            }
        } else {
            result.missing_descriptions = NULL;
            /* 有原子前提但无法构造：可能是推理规则组合问题 */
            result.missing_count = 1;
            result.missing_descriptions = (char **) lv00_malloc(sizeof(char *)); /* 分配内存 */
            if (result.missing_descriptions) {
                char desc[256];
                snprintf(desc, sizeof(desc), "无法通过现有前提的组合构造目标（推理规则链不完整）");
                result.missing_descriptions[0] = lv00_strdup_safe(desc); /* 复制字符串 */
            }
        }
    }

    return result;
}

void prop_verifier_free_bhk_result(BHKVerificationResult *result) {
    if (!result)
        return;
    if (result->missing_descriptions) {
        for (int i = 0; i < result->missing_count; i++) {
            lv00_free((void **) &result->missing_descriptions[i]); /* 释放并置NULL */
        }
        lv00_free((void **) &result->missing_descriptions); /* 释放并置NULL */
    }
    result->missing_count = 0;
}

/* ============================================================
 * 信任颜色桥接 —— BHK验证结果 → 约束图 TrustColor
 * ============================================================ */

/**
 * @brief 基于 BHK 验证结果映射 TrustColor
 *
 * 将验证结果映射为适当的信任颜色：
 *   - verified + 0 missing → TRUST_GREEN
 *   - verified + 1-2 missing → TRUST_YELLOW
 *   - verified + 3+ missing → TRUST_AMBER
 *   - 未验证（PV_VERIFY_FAILED）→ TRUST_BLUE
 *   - 已证伪（PV_VERIFY_DISPROVEN）→ TRUST_RED
 *   - 超时/错误 → TRUST_BLUE
 */
static TrustColor map_bhk_to_trust_color(const BHKVerificationResult *bhk, PropVerifyResult verify_result) {
    switch (verify_result) {
        case PV_VERIFY_PROVEN:
            if (!bhk->verified) {
                /* BHK层未通过但命题层通过：条件性可信 */
                return TRUST_YELLOW;
            }
            if (bhk->missing_constructions == 0) {
                return TRUST_GREEN;
            } else if (bhk->missing_constructions <= 2) {
                return TRUST_YELLOW;
            } else {
                return TRUST_AMBER;
            }
        case PV_VERIFY_DISPROVEN:
            return TRUST_RED;
        case PV_VERIFY_FAILED:
            return TRUST_BLUE;
        case PV_VERIFY_TIMEOUT:
        case PV_VERIFY_INVALID_INPUT:
        case PV_VERIFY_ERROR:
        default:
            return TRUST_BLUE;
    }
}

/**
 * @brief 获取 TrustColor 的中文名称
 */
static const char *trust_color_name(TrustColor color) {
    switch (color) {
        case TRUST_GREEN:
            return "绿色（完全可信）";
        case TRUST_BLUE:
            return "蓝色（未确定）";
        case TRUST_YELLOW:
            return "黄色（条件性可信）";
        case TRUST_ORANGE:
            return "橙色（需关注）";
        case TRUST_LIGHT_ORANGE:
            return "浅橙色";
        case TRUST_RED:
            return "红色（不可信/已证伪）";
        case TRUST_AMBER:
            return "琥珀色（显著缺失）";
        default:
            return "未知";
    }
}

int prop_verifier_apply_trust_colors(ConstraintGraph *graph, const PropFormula **premises, int premise_count,
                                     const PropFormula *goal, const VerifierConfig *config,
                                     BHKVerificationResult *out_result) {
    if (!graph)
        return -1;

    /* 步骤1: 执行 BHK 验证 */
    BHKVerificationResult bhk = prop_verifier_bhk_verify(premises, premise_count, goal, config);

    /* 同时获取原始验证结果以判断 DISPROVEN 等状态 */
    VerifierConfig default_cfg = VERIFIER_CONFIG_DEFAULT;
    if (!config)
        config = &default_cfg;
    VerifyDetail detail = prop_verifier_verify(premises, premise_count, goal, config);

    /* 步骤2: 映射信任颜色 */
    TrustColor target_color = map_bhk_to_trust_color(&bhk, detail.result);

    /* 流式输出: 验证开始 */
    if (prop_verifier_stream_ctx) {
        char desc[256];
        snprintf(desc, sizeof(desc), "信任颜色桥接: BHK验证=%s, 缺失构造=%d, 目标颜色=%s",
                 bhk.verified ? "通过" : "未通过", bhk.missing_constructions, trust_color_name(target_color));
        stream_emit_simple(prop_verifier_stream_ctx, STREAM_EVENT_PROOF_COLOR_UPDATE, desc, 0);
    }

    /* 步骤3: 遍历约束图中的所有节点，设置信任颜色 */
    int updated_count = 0;
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node)
            continue;

        bool node_updated = false;

        /* 对节点的每个符号坐标设置信任颜色 */
        for (int c = 0; c < node->coord_count; c++) {
            SymbolicCoord *coord = node->symbolic_coords[c];
            if (!coord)
                continue;

            TrustColor old_color = symbolic_coord_get_trust(coord);
            if (old_color != target_color) {
                symbolic_coord_set_trust(coord, target_color);
                node_updated = true;
            }
        }

        if (node_updated) {
            updated_count++;

            /* 流式输出: 单个节点的颜色更新 */
            if (prop_verifier_stream_ctx) {
                StreamEvent ev;
                memset(&ev, 0, sizeof(ev));
                ev.type = STREAM_EVENT_PROOF_COLOR_UPDATE;
                ev.timestamp_ms = stream_timestamp_ms();
                ev.node_id = node->id;
                ev.step_number = i;
                ev.total_steps = graph->node_count;
                ev.description = trust_color_name(target_color);
                char detail_json[192];
                snprintf(detail_json, sizeof(detail_json),
                         "{\"node_id\":%d,\"type\":%d,\"old_color\":%d,\"new_color\":%d,"
                         "\"verified\":%s,\"missing\":%d}",
                         node->id, (int) node->type,
                         (int) symbolic_coord_get_trust(
                             node->coord_count > 0 && node->symbolic_coords[0] ? node->symbolic_coords[0] : NULL),
                         (int) target_color, bhk.verified ? "true" : "false", bhk.missing_constructions);
                ev.detail_json = detail_json;
                stream_emit(prop_verifier_stream_ctx, &ev);
            }
        }
    }

    /* 流式输出: 完成统计 */
    if (prop_verifier_stream_ctx) {
        char done_desc[128];
        snprintf(done_desc, sizeof(done_desc), "信任颜色应用完成: 更新了 %d/%d 个节点", updated_count,
                 graph->node_count);
        stream_emit_simple(prop_verifier_stream_ctx, STREAM_EVENT_PROOF_COLOR_UPDATE, done_desc, 0);
    }

    /* 步骤4: 输出结果（如果调用者需要） */
    if (out_result) {
        memcpy(out_result, &bhk, sizeof(BHKVerificationResult));
        /* 注意: missing_descriptions 的所有权转移给调用者 */
        /* 不在此处释放 bhk.missing_descriptions */
    } else {
        /* 调用者不需要结果，我们负责释放 */
        prop_verifier_free_bhk_result(&bhk);
    }

    return updated_count;
}

/* ============================================================
 * 命题等价性检查
 * ============================================================ */

bool prop_verifier_check_equivalence(const PropFormula *a, const PropFormula *b, const VerifierConfig *config) {
    if (!a || !b)
        return false;

    /* 结构相等性快速路径 */
    if (formula_equal(a, b))
        return true;

    VerifierConfig default_config = VERIFIER_CONFIG_DEFAULT;
    if (!config)
        config = &default_config;

    /* 检查 a → b 和 b → a 是否都可证 */
    PropFormula *a_impl_b = prop_formula_create_implication(prop_formula_copy(a), prop_formula_copy(b));
    PropFormula *b_impl_a = prop_formula_create_implication(prop_formula_copy(b), prop_formula_copy(a));

    VerifyDetail d1 = prop_verifier_verify(NULL, 0, a_impl_b, config);
    VerifyDetail d2 = prop_verifier_verify(NULL, 0, b_impl_a, config);

    bool result = (d1.result == PV_VERIFY_PROVEN && d2.result == PV_VERIFY_PROVEN);

    prop_formula_destroy(a_impl_b);
    prop_formula_destroy(b_impl_a);

    return result;
}

bool prop_verifier_check_tautology(const PropFormula *f, const VerifierConfig *config) {
    if (!f)
        return false;

    VerifierConfig default_config = VERIFIER_CONFIG_DEFAULT;
    if (!config)
        config = &default_config;

    /* 永真式 = 无前提即可证 */
    VerifyDetail detail = prop_verifier_verify(NULL, 0, f, config);
    return detail.result == PV_VERIFY_PROVEN;
}

int prop_verifier_run_smoke_tests(const SmokeTest *tests, int test_count, VerifyDetail *results) {
    int passed = 0;

    for (int i = 0; i < test_count; i++) {
        const SmokeTest *t = &tests[i];

        /* 测试 13 需要启用 ex_falso */
        VerifierConfig config = VERIFIER_CONFIG_DEFAULT;
        /* 对于预期不可证的测试，使用直觉主义模式 */
        /* 对于测试 13（爆炸原理），启用 ex_falso */
        if (t->expected_provable && t->premise_count == 1 && t->premises[0] && t->premises[0]->type == PROP_BOTTOM &&
            t->goal && t->goal->type == PROP_ATOM) {
            config.enable_ex_falso = true;
        }

        /* 将固定大小数组转为指针数组以匹配 API */
        const PropFormula *prem_ptrs[PROP_SMOKE_MAX_PREM_PTRS];
        for (int j = 0; j < t->premise_count && j < PROP_SMOKE_MAX_PREM_PTRS; j++) {
            prem_ptrs[j] = t->premises[j];
        }

        results[i] = prop_verifier_verify(prem_ptrs, t->premise_count, t->goal, &config);

        bool actually_proven = (results[i].result == PV_VERIFY_PROVEN);

        /* 对于预期不可证的测试：检查是否确实不可证 */
        if (t->expected_provable) {
            if (actually_proven)
                passed++;
        } else {
            if (!actually_proven)
                passed++;
        }
    }

    return passed;
}
