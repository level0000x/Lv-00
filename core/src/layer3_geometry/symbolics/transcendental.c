/**
 * @file transcendental.c
 * @brief Transcendental 超越数类型（π, e, 及其有理倍）
 *
 * @details 支持超越常数 π 和 e 及其有理倍数形式 (k*T)/m。
 *          核心操作：
 *          - transcendental_create("pi"/"e"/"pi/2"/"pi/4" 等): 按名称创建
 *          - transcendental_evaluate: 获取 double 近似值
 *          - transcendental_compare: 通过有理倍数比较（相同类型时精确）
 *          - transcendental_serialize: 序列化为人类可读名称
 *
 *          支持的表达式格式：
 *          - 基础常量：pi, e
 *          - 有理倍数：pi/2, pi/3, pi/4, pi/6, 3*pi/4, 5*pi/6, 2*pi/3
 *          - 负数形式：-pi/2, -pi/4 等
 *
 *          注意：不同超越常数的加减法不可合并（如 π + e 无法进一步简化）。
 *          此类操作返回的结果标记为 TRANSCENDENTAL 类型，但内部表达式仅
 *          包含系数信息，不存储真实的复合表达式树。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/lv_platform.h"
#include <errno.h>
#include <float.h>
#include <math.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/symbolic_coord.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "mpz_poly.h"

#define SYM_COORD_DYNAMIC_ARRAY_INIT_CAP 16
#define SYM_COORD_SIGFIGS_MIN_SAFE 6
#define SYM_COORD_SIGFIGS_APPROX 4
#define SYM_COORD_MAX_REFINE 15
#define SYM_COORD_AMB_MIN_SIGFIGS 3
#define COORD_SEVEN_OVER_FIVE_N 32
/* ── Transcendental type ── */

Transcendental *transcendental_create(const char *name) {
    if (!name)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "transcendental_create: name is NULL");

    /* 支持基础常量 "pi" 和 "e"，以及复合表达式如 "pi/2", "pi/3",
     * "pi/4", "pi/6", "3*pi/4", "5*pi/6", "2*pi/3" 及其负数形式 */
    const char *base = NULL;
    int64_t coeff_num = 1; /* 系数分子（默认为 1） */
    int64_t coeff_den = 1; /* 系数分母（默认为 1） */
    bool is_mul = false;   /* true = coeff*base, false = base/coeff */

    if (strcmp(name, "pi") == 0 || strcmp(name, "e") == 0) {
        /* 裸常量 */
        base = name;
    } else if (lv_str_startswith(name, "-pi") && (name[3] == '\0' || name[3] == '/')) {
        /* 负 pi 变体: -pi, -pi/2, -pi/3, ... */
        base = "pi";
        coeff_num = -1;
        if (name[3] == '/') {
            is_mul = false;
            {
                char *e = NULL;
                errno = 0;
                long v = strtol(name + 4, &e, 10);
                if (errno == 0 && e != name + 4)
                    coeff_den = (int64_t) v;
            }
            if (coeff_den <= 0)
                lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "transcendental_create: invalid den");
        }
    } else if (lv_str_startswith(name, "pi/")) {
        /* pi/N 形式 */
        base = "pi";
        is_mul = false;
        {
            char *e = NULL;
            errno = 0;
            long v = strtol(name + 3, &e, 10);
            if (errno == 0 && e != name + 3)
                coeff_den = (int64_t) v;
        }
        if (coeff_den <= 0)
            lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "transcendental_create: invalid den");
    } else if (lv_str_startswith(name, "-pi/")) {
        /* 已在上面处理 */
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "transcendental_create: unsupported format");
    } else {
        /* 尝试解析 N*pi/M 或 N*pi 形式 */
        char *star_pos = strstr(name, "*pi");
        if (star_pos && star_pos == name + 1 && name[0] != '-') {
            /* N*pi 或 N*pi/M */
            base = "pi";
            is_mul = true;
            {
                char *e = NULL;
                errno = 0;
                long v = strtol(name, &e, 10);
                if (errno == 0 && e != name)
                    coeff_num = (int64_t) v;
            }
            if (coeff_num <= 0)
                lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "transcendental_create: invalid num");
            const char *after = star_pos + 3; /* skip "*pi" */
            if (*after == '/') {
                {
                    char *e = NULL;
                    errno = 0;
                    long v = strtol(after + 1, &e, 10);
                    if (errno == 0 && e != after + 1)
                        coeff_den = (int64_t) v;
                }
                if (coeff_den <= 0)
                    lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "transcendental_create: invalid den");
            }
        } else if (star_pos && star_pos == name + 2 && name[0] == '-') {
            /* -N*pi 或 -N*pi/M */
            base = "pi";
            is_mul = true;
            {
                char *e = NULL;
                errno = 0;
                long v = strtol(name, &e, 10);
                if (errno == 0 && e != name)
                    coeff_num = (int64_t) v;
            }
            if (coeff_num >= 0)
                lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "transcendental_create: invalid num");
            const char *after = star_pos + 3;
            if (*after == '/') {
                {
                    char *e = NULL;
                    errno = 0;
                    long v = strtol(after + 1, &e, 10);
                    if (errno == 0 && e != after + 1)
                        coeff_den = (int64_t) v;
                }
                if (coeff_den <= 0)
                    lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "transcendental_create: invalid den");
            }
        } else {
            /* 任意超越函数名（如 "sinθ_mrad"、"cosθ" 等）：作为裸常量处理，
             * 以便支持符号三角函数表达式的创建 */
            base = name;
        }
    }

    if (!base)
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_STATE, "transcendental_create: base is NULL");

    Transcendental *t = lv_calloc(1, sizeof(Transcendental));
    if (!t)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "transcendental_create: calloc failed");
    /* 安全字符串复制：确保以 null 终止 */
    {
        size_t name_len = strlen(name);
        if (name_len >= sizeof(t->name))
            name_len = sizeof(t->name) - 1;
        memcpy(t->name, name, name_len);
        t->name[name_len] = '\0';
    }

    /* 如果是裸常量（pi 或 e），expr 保持 NULL */
    if (strcmp(name, "pi") == 0 || strcmp(name, "e") == 0) {
        t->expr = NULL;
    } else {
        /* 构造表达式树 */
        TranscendentalExpr *expr = lv_calloc(1, sizeof(TranscendentalExpr));
        if (!expr) {
            lv_free((void **) &t);
            lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "transcendental_create: expr calloc failed");
        }
        /* 安全字符串复制：确保以 null 终止 */
        {
            size_t base_len = strlen(base);
            if (base_len >= sizeof(expr->base_name))
                base_len = sizeof(expr->base_name) - 1;
            memcpy(expr->base_name, base, base_len);
            expr->base_name[base_len] = '\0';
        }
        expr->out_of_scope = false;

        if (is_mul) {
            expr->expr_type = TRANS_EXPR_MUL_RATIONAL;
            expr->rational_operand = rational_create(coeff_num, (uint64_t) coeff_den);
        } else {
            /* base/coeff 等价于 base * (1/coeff) */
            expr->expr_type = TRANS_EXPR_MUL_RATIONAL;
            expr->rational_operand = rational_create(coeff_num, (uint64_t) coeff_den);
        }

        if (!expr->rational_operand) {
            lv_free((void **) &expr);
            lv_free((void **) &t);
            lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "transcendental_create: rational_operand failed");
        }

        t->expr = expr;
    }

    return t;
}

void transcendental_destroy(Transcendental *t) {
    if (!t)
        return;
    if (t->expr) {
        if (t->expr->rational_operand) {
            rational_destroy(t->expr->rational_operand);
        }
        lv_free((void **) &t->expr);
    }
    lv_free((void **) &t);
}

/**
 * 比较两个超越数的大小。
 *
 * @param a 第一个超越数（不能为 NULL）
 * @param b 第二个超越数（不能为 NULL）
 * @return -1 表示 a < b，0 表示 a == b，1 表示 a > b
 */
int transcendental_compare(const Transcendental *a, const Transcendental *b) {
    /* Compare base names first */
    int name_cmp = strcmp(a->name, b->name);
    if (name_cmp != 0)
        return name_cmp;

    /* Both NULL expr means bare constants with same name -> equal */
    if (!a->expr && !b->expr)
        return 0;
    /* NULL expr vs non-NULL expr: bare constant vs expression */
    if (!a->expr)
        return -1;
    if (!b->expr)
        return 1;

    /* Both have expr: compare expression trees */
    if (a->expr->expr_type != b->expr->expr_type) {
        return (a->expr->expr_type < b->expr->expr_type) ? -1 : 1;
    }

    /* Same expr_type: compare rational operands if present */
    if (a->expr->rational_operand && b->expr->rational_operand) {
        return rational_compare(a->expr->rational_operand, b->expr->rational_operand);
    }
    if (a->expr->rational_operand)
        return 1;
    if (b->expr->rational_operand)
        return -1;

    return 0;
}

/* ================================================================
 * 表达式类型 -> 运算符/语义 静态查找表（数据表化，替代 switch）
 * ================================================================ */

/** @brief 表达式类型 -> 序列化运算符字符串 */
static const char *const s_trans_expr_op_str[] = {
    [TRANS_EXPR_ADD_RATIONAL] = "+",
    [TRANS_EXPR_MUL_RATIONAL] = "*",
    [TRANS_EXPR_ADD_ALGEBRAIC] = "+",
    [TRANS_EXPR_MUL_ALGEBRAIC] = "*",
};

/** @brief 表达式类型 -> 是否为乘法（true=系数*基础常数，false=基础常数+系数） */
static const bool s_trans_expr_is_mul[] = {
    [TRANS_EXPR_ADD_RATIONAL] = false,
    [TRANS_EXPR_MUL_RATIONAL] = true,
    [TRANS_EXPR_ADD_ALGEBRAIC] = false,
    [TRANS_EXPR_MUL_ALGEBRAIC] = true,
};

char *transcendental_serialize(const Transcendental *t) {
    if (!t->expr) {
        /* 裸常量：使用 lv_strdup 分配内存 */
        return lv_strdup(t->name);
    }

    /* Serialize expression tree */
    const char *op_str = NULL;
    /* 查表获取运算符字符串；未知表达式类型回退到裸常量名（原 default 分支） */
    TransExprType et = t->expr->expr_type;
    if ((unsigned) et < lv_ARRAY_SIZE(s_trans_expr_op_str) && s_trans_expr_op_str[et]) {
        op_str = s_trans_expr_op_str[et];
    } else {
        /* 未知表达式类型：使用 lv_strdup 分配内存 */
        return lv_strdup(t->name);
    }

    if (t->expr->out_of_scope) {
        /* Out-of-scope expression: mark clearly */
        size_t len = strlen(t->name) + strlen(op_str) + 32;
        char *buf = lv_malloc(len);
        if (!buf)
            lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "transcendental_serialize: malloc failed");
        snprintf(buf, len, "[%s %s <out-of-scope>]", t->name, op_str);
        return buf;
    }

    if (t->expr->rational_operand) {
        char *rat_str = rational_serialize(t->expr->rational_operand);
        if (!rat_str)
            return lv_strdup(t->name); /* rational_serialize 失败时使用 lv_strdup */
        size_t len = strlen(t->name) + strlen(op_str) + strlen(rat_str) + 8;
        char *buf = lv_malloc(len);
        if (!buf) {
            lv_free((void **) &rat_str); /* lv_malloc分配 */
            return lv_strdup(t->name);   /* 内存不足时使用 lv_strdup */
        }
        snprintf(buf, len, "(%s %s %s)", t->name, op_str, rat_str);
        lv_free((void **) &rat_str); /* lv_malloc分配 */
        return buf;
    }

    /* 无理数操作数：使用 lv_strdup 分配内存 */
    return lv_strdup(t->name);
}

/**
 * 获取超越数的数值近似。
 *
 * 支持的超越数包括：
 * - 基本常数：pi, e
 * - 有理数运算：k*pi, pi/k, k*e, e/k（k 为有理数）
 * - 超出作用域的表达式（如 pi + sqrt(2)）：返回基常数的近似值
 *
 * 对于无法解析的表达式，默认返回 0.0。
 *
 * @param t 超越数对象（不能为 NULL）
 * @return double 近似值；如果无法解析则返回 0.0
 */
double transcendental_to_double(const Transcendental *t) {
    /* Get base constant value */
    double base_val = 0.0;
    if (strcmp(t->name, "pi") == 0) {
        base_val = M_PI;
    } else if (strcmp(t->name, "e") == 0) {
        base_val = M_E;
    } else {
        return 0.0;
    }

    /* 如果没有表达式修饰，直接返回基础常数 */
    if (!t->expr)
        return base_val;

    /* 超出作用域的表达式返回基础常数近似值 */
    if (t->expr->out_of_scope)
        return base_val;

    /* 应用有理数运算 */
    if (t->expr->rational_operand) {
        /* 将有理数转换为 double */
        mpq_t rat_val;
        mpq_init(rat_val);
        /* Rational 结构体内部是 mpq_t value */
        double k = 0.0;
        /* 从 t->name 推断基础常数 */
        k = mpq_get_d(t->expr->rational_operand->value);
        mpq_clear(rat_val);

        /* 查表判断运算语义：乘法 k*base / 加法 base+k；未知类型返回基础常数（原 default 分支） */
        TransExprType et = t->expr->expr_type;
        if ((unsigned) et < lv_ARRAY_SIZE(s_trans_expr_is_mul)) {
            if (s_trans_expr_is_mul[et])
                return k * base_val;
            return base_val + k;
        }
        return base_val;
    }

    return base_val;
}
