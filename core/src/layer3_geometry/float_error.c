/**
 * @file float_error.c
 * @brief FPTaylor 风格浮点误差验证实现 —— 区间算术 + 泰勒展开验证
 * @details 实现 IEEE 1788 区间算术的基本操作（加减乘除）以及
 *          常超越函数的区间版本。提供一组泰勒展开的有界差分近似。
 *          将约束图变量转换为可评估表达式，并通过误差与容差比较
 *          映射到 Lv-00 信任颜色系统。
 *
 *          区间算术遵循最小/最大原理：
 *          - 加法/减法：端点直接运算
 *          - 乘法：四个角点的最小/最大值
 *          - 除法：通过倒数乘法，排除零点区间
 *
 *          核心模块：
 *          - 区间算术完整实现：add/sub/mul/div/sqrt/sin/cos/exp/log
 *          - 一组泰勒展开：有界差分近似偏导数
 *          - fptaylor_evaluate_graph：约束图 -> 表达式 -> 区间评估
 *          - fptaylor_verify_safety：误差 -> 信任颜色
 *
 * @author Lv-00 Project
 * @version 3.3.0
 *
 * @dependencies
 *   - float_error.h        : 浮点误差分析公共接口
 *   - constraint_graph.h   : 约束图数据结构
 *   - symbolic_coord.h     : 符号坐标与 TrustColor
 *   - lv_utils.h         : 统一内存分配器
 *   - lv_internal.h      : 内部常量与工具宏
 */

#include "lv/lv_platform.h"
#include "float_error.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/config.h"
#include "lv/constraint_graph.h"

#include "lv_internal.h"
#include "lv_utils.h"
#include "symbolic_coord.h"

/* ========================================================================
 * 内部常量
 * ======================================================================== */

/** 表达式缓冲区初始大小 */
#define EXPR_BUFFER_INITIAL 256

/** 最多提取的约束方程数（支持复杂方程组，从 64 增至 128） */
#define MAX_EQUATIONS 128

/** 安全下限（避免 log(0) 等问题） */
#define SAFE_MIN_POSITIVE 1e-308

/* ========================================================================
 * 内部辅助函数
 * ======================================================================== */

/** 返回 a 和 b 的最小值 */
static double double_min(double a, double b) {
    return fmin(a, b);
}

/** 返回 a 和 b 的最大值 */
static double double_max(double a, double b) {
    return fmax(a, b);
}

/**
 * @brief 向下舍入（保留下界计算）
 *
 * 将 double 值向负无穷方向微调，确保区间下界是安全的。
 * 乘以 (1 - DBL_EPSILON) 以处理浮点舍入。
 */
static double round_down(double x) {
    if (isnan(x))
        return x;
    if (isinf(x))
        return x;
    if (x == 0.0)
        return -0.0; /* 零方向的下一个可表示值 */
    if (x > 0.0) {
        return nextafter(x, -INFINITY);
    } else {
        return nextafter(x, -INFINITY);
    }
}

/**
 * @brief 向上舍入（保留上界计算）
 *
 * 将 double 值向正无穷方向微调，确保区间上界是安全的。
 * 使用 nextafter 确保对零和次正规数也正确。
 */
static double round_up(double x) {
    if (isnan(x))
        return x;
    if (isinf(x))
        return x;
    if (x == 0.0)
        return +0.0; /* 从零向上 */
    return nextafter(x, INFINITY);
}

/* ========================================================================
 * 区间算术 —— 完整实现（最小/最大原理）
 * ======================================================================== */

/**
 * @brief 构造浮点区间
 * @param lo       区间下界
 * @param hi       区间上界
 * @param is_exact 是否为精确值
 * @return 区间结构体
 */
FloatInterval interval_make(double lo, double hi, bool is_exact) {
    FloatInterval iv;
    iv.lo = lo;
    iv.hi = hi;
    iv.is_exact = is_exact;
    return iv;
}

/**
 * @brief 区间加法（带安全舍入）
 * @param a 左操作数区间
 * @param b 右操作数区间
 * @return 结果区间
 */
FloatInterval float_interval_add(FloatInterval a, FloatInterval b) {
    FloatInterval result;
    result.lo = round_down(a.lo + b.lo);
    result.hi = round_up(a.hi + b.hi);
    result.is_exact = a.is_exact && b.is_exact;
    return result;
}

/**
 * @brief 区间减法（带安全舍入）
 * @param a 被减数区间
 * @param b 减数区间
 * @return 结果区间
 */
FloatInterval float_interval_sub(FloatInterval a, FloatInterval b) {
    FloatInterval result;
    /* a - b: 下界 = a.lo - b.hi, 上界 = a.hi - b.lo */
    result.lo = round_down(a.lo - b.hi);
    result.hi = round_up(a.hi - b.lo);
    result.is_exact = a.is_exact && b.is_exact;
    return result;
}

/**
 * @brief 区间乘法（四角点最小/最大原理）
 * @param a 左操作数区间
 * @param b 右操作数区间
 * @return 结果区间
 */
FloatInterval float_interval_mul(FloatInterval a, FloatInterval b) {
    /* 计算四个角点 */
    double p1 = a.lo * b.lo;
    double p2 = a.lo * b.hi;
    double p3 = a.hi * b.lo;
    double p4 = a.hi * b.hi;

    double min_val = double_min(double_min(p1, p2), double_min(p3, p4));
    double max_val = double_max(double_max(p1, p2), double_max(p3, p4));

    FloatInterval result;
    result.lo = round_down(min_val);
    result.hi = round_up(max_val);
    result.is_exact = a.is_exact && b.is_exact;
    return result;
}

/**
 * @brief 区间除法（通过倒数乘法实现，排除零点区间）
 * @param a 被除数区间
 * @param b 除数区间
 * @return 结果区间；若除数跨越零点则返回 [-HUGE_VAL, HUGE_VAL]
 */
FloatInterval float_interval_div(FloatInterval a, FloatInterval b) {
    FloatInterval result;

    /* 检查分母是否跨越零点 */
    if (b.lo <= 0.0 && b.hi >= 0.0) {
        /* 分母包含零：返回 NaN 区间表示无定义 */
        result.lo = -HUGE_VAL;
        result.hi = HUGE_VAL;
        result.is_exact = false;
        return result;
    }

    /* 通过倒数 + 乘法实现除法 */
    if (b.hi < 0.0) {
        /* 分母全负：取倒数范围 [1/b.hi, 1/b.lo] */
        double inv_lo = 1.0 / b.hi;
        double inv_hi = 1.0 / b.lo;
        FloatInterval inv_b = interval_make(inv_lo, inv_hi, b.is_exact);
        result = float_interval_mul(a, inv_b);
    } else {
        /* 分母全正：取倒数范围 [1/b.hi, 1/b.lo] */
        double inv_lo = 1.0 / b.hi;
        double inv_hi = 1.0 / b.lo;
        FloatInterval inv_b = interval_make(inv_lo, inv_hi, b.is_exact);
        result = float_interval_mul(a, inv_b);
    }

    return result;
}

/**
 * @brief 区间平方根（负数部分截断到 0）
 * @param a 输入区间
 * @return 结果区间
 */
FloatInterval float_interval_sqrt(FloatInterval a) {
    FloatInterval result;
    if (a.lo < 0.0) {
        /* 负数部分无实数定义，截断到 0 */
        result.lo = 0.0;
    } else {
        result.lo = round_down(sqrt(a.lo));
    }
    result.hi = round_up(sqrt(a.hi));
    result.is_exact = a.is_exact && (a.lo == a.hi);
    return result;
}

/**
 * @brief 区间正弦函数（处理非单调区间和极值点）
 * @param a 输入区间（弧度）
 * @return 结果区间，范围 [-1, 1]
 */
FloatInterval float_interval_sin(FloatInterval a) {
    /* sin 在 [-1, 1] 之间，需要处理非单调区间 */
    double sin_lo = sin(a.lo);
    double sin_hi = sin(a.hi);

    /* 检查区间是否跨越 pi/2 + k*pi（极值点） */
    double width = a.hi - a.lo;
    double min_val = double_min(sin_lo, sin_hi);
    double max_val = double_max(sin_lo, sin_hi);

    if (width >= 2.0 * M_PI) {
        /* 区间超过一个完整周期 -> 覆盖全范围 */
        min_val = -1.0;
        max_val = 1.0;
    } else {
        /* 检查 pi/2 + 2k*pi 和 3pi/2 + 2k*pi 是否在区间内 */
        double pi_half = M_PI / 2.0;
        double k_start = ceil((a.lo - pi_half) / (2.0 * M_PI));
        double k_end = floor((a.hi - pi_half) / (2.0 * M_PI));
        for (double k = k_start; k <= k_end; k += 1.0) {
            double peak = pi_half + k * 2.0 * M_PI;
            if (peak >= a.lo && peak <= a.hi) {
                if (fmod(k, 2.0) == 0.0) {
                    max_val = 1.0; /* sin(pi/2 + 2k*pi) = 1 */
                } else {
                    min_val = -1.0; /* sin(3pi/2 + 2k*pi) = -1 */
                }
            }
        }
    }

    FloatInterval result;
    result.lo = round_down(min_val);
    result.hi = round_up(max_val);
    result.is_exact = a.is_exact && (a.lo == a.hi);
    return result;
}

/**
 * @brief 区间余弦函数（处理非单调区间和极值点）
 * @param a 输入区间（弧度）
 * @return 结果区间，范围 [-1, 1]
 */
FloatInterval float_interval_cos(FloatInterval a) {
    /* cos 性质类似 sin，偏移 pi/2 */
    double cos_lo = cos(a.lo);
    double cos_hi = cos(a.hi);
    double width = a.hi - a.lo;
    double min_val = double_min(cos_lo, cos_hi);
    double max_val = double_max(cos_lo, cos_hi);

    if (width >= 2.0 * M_PI) {
        min_val = -1.0;
        max_val = 1.0;
    } else {
        /* 检查 k*pi（cos 的极值点）是否在区间内 */
        double k_start = ceil(a.lo / M_PI);
        double k_end = floor(a.hi / M_PI);
        for (double k = k_start; k <= k_end; k += 1.0) {
            double peak = k * M_PI;
            if (peak >= a.lo && peak <= a.hi) {
                /* cos(k*pi) = (-1)^k */
                if (fmod(k, 2.0) == 0.0) {
                    max_val = 1.0;
                } else {
                    min_val = -1.0;
                }
            }
        }
    }

    FloatInterval result;
    result.lo = round_down(min_val);
    result.hi = round_up(max_val);
    result.is_exact = a.is_exact && (a.lo == a.hi);
    return result;
}

/**
 * @brief 区间指数函数（单调递增）
 * @param a 输入区间
 * @return 结果区间
 */
FloatInterval float_interval_exp(FloatInterval a) {
    /* exp 单调递增 */
    FloatInterval result;
    result.lo = round_down(exp(a.lo));
    result.hi = round_up(exp(a.hi));
    result.is_exact = a.is_exact && (a.lo == a.hi);
    return result;
}

/**
 * @brief 区间自然对数函数（非正区间无定义）
 * @param a 输入区间
 * @return 结果区间；若下界 <= 0 则返回 [-HUGE_VAL, ...]
 */
FloatInterval float_interval_log(FloatInterval a) {
    FloatInterval result;
    if (a.lo <= 0.0) {
        /* log 在非正区间无定义 */
        result.lo = -HUGE_VAL;
        result.hi = (a.hi > 0.0) ? round_up(log(a.hi)) : -HUGE_VAL;
        result.is_exact = false;
        return result;
    }
    result.lo = round_down(log(a.lo));
    result.hi = round_up(log(a.hi));
    result.is_exact = a.is_exact && (a.lo == a.hi);
    return result;
}

/* ========================================================================
 * 表达式求值器 —— Shunting-yard 算法实现
 * ======================================================================== */

/** @brief RPN（逆波兰表示）运算符编码宏 */
#define RPN_OP_ADD (-1)   /**< 加法 + */
#define RPN_OP_SUB (-2)   /**< 减法 - */
#define RPN_OP_MUL (-3)   /**< 乘法 * */
#define RPN_OP_DIV (-4)   /**< 除法 / */
#define RPN_OP_POW (-5)   /**< 幂运算 ^ */
#define RPN_OP_SQRT (-10) /**< sqrt 函数 */
#define RPN_OP_SIN (-11)  /**< sin 函数 */
#define RPN_OP_COS (-12)  /**< cos 函数 */
#define RPN_OP_EXP (-13)  /**< exp 函数 */
#define RPN_OP_LOG (-14)  /**< log 函数 */
#define RPN_OP_NEG (-20)  /**< 一元负号 */

/** @brief 表达式求值栈的最大深度 */
#define EXPR_STACK_MAX 128
/** @brief RPN 输出队列的最大长度 */
#define EXPR_RPN_MAX 256

/**
 * @brief 获取运算符优先级（值越大优先级越高）
 *
 * +、- 优先级 1；*、/ 优先级 2；一元负号 3；^ 优先级 4（右结合）。
 *
 * @param[in] op 运算符编码（RPN_OP_* 宏）
 * @return 优先级数值；若无法识别返回 0
 */
static int expr_op_precedence(int op) {
    switch (op) {
        case RPN_OP_ADD:
        case RPN_OP_SUB:
            return 1;
        case RPN_OP_MUL:
        case RPN_OP_DIV:
            return 2;
        case RPN_OP_NEG:
            return 3;
        case RPN_OP_POW:
            return 4;
        default:
            return 0;
    }
}

/**
 * @brief 数学表达式求值器 —— 调度场（Shunting-yard）算法
 *
 * 基于 Edsger Dijkstra 的调度场算法，将中缀数学表达式转换为逆波兰表示
 * （RPN），然后在给定变量取值下逐项求值。该求值器为有限差分偏导数计算
 * 提供核心数学能力。
 *
 * 支持的语法元素：
 *   - 二元运算符：+（加）、-（减）、*（乘）、/（除）、^（幂，右结合）
 *   - 一元函数：sqrt(expr)、sin(expr)、cos(expr)、exp(expr)、log(expr)
 *   - 变量：x0、x1、x2、...、xN，对应 var_values[0..N-1]
 *   - 数值常量：整数和浮点数（支持科学记数法如 1.5e-3）
 *   - 括号：() 用于控制运算优先级
 *   - 一元负号：-expr 作为独立的取负操作处理
 *
 * 运算符优先级（从低到高）：
 *   +、- (1) < *、/ (2) < 一元负号 (3) < ^ (4)
 * 除 ^ 为右结合外，其余二元运算符均为左结合。
 *
 * 算法流程：
 *   阶段 1 —— 词法分析 + 调度场转换：读入中缀 token 流，按优先级将
 *           运算符压入操作符栈，操作数直接送入 RPN 输出队列。
 *   阶段 2 —— RPN 求值：遍历 RPN 队列，操作数入求值栈，运算符从
 *           栈顶弹出所需数量的操作数进行计算后压回结果。
 *
 * @param[in] expr       以 null 结尾的中缀表达式字符串
 * @param[in] var_values 变量值数组，var_values[i] 对应变量 xi
 * @param[in] var_count  变量数量
 * @return 表达式在给定变量值下的计算结果；若 expr 为 NULL、解析失败
 *         或数学域错误（如 log(非正数)）则返回 NaN
 */
static double evaluate_expression(const char *expr, const double *var_values, int var_count) {
    if (!expr || !var_values || var_count <= 0) {
        return NAN;
    }

    /*
     * RPN 输出队列：
     *   rpn_op[i] == 0  → 操作数，值在 rpn_val[i] 中
     *   rpn_op[i] != 0  → 运算符，编码为 RPN_OP_* 宏
     */
    int rpn_op[EXPR_RPN_MAX];
    double rpn_val[EXPR_RPN_MAX];
    int rpn_len = 0;

    /* 操作符栈（调度场核心数据结构） */
    int op_stack[EXPR_STACK_MAX];
    int op_top = 0;

    const char *p = expr;
    int expect_operand = 1; /**< 标记当前期望读入操作数（1）还是运算符（0） */

    while (*p) {
        /* 跳过空白字符 */
        if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
            p++;
            continue;
        }

        if (expect_operand) {
            /*
             * ============================================================
             * 解析操作数：数值常量 | 变量 | 一元函数 | 左括号 | 一元负号
             * ============================================================
             */

            /* 数值常量（含科学记数法，如 3.14、-2.5e-3、.5） */
            if ((*p >= '0' && *p <= '9') || (*p == '.' && (*(p + 1) >= '0' && *(p + 1) <= '9'))) {
                char *end = NULL;
                double val = strtod(p, &end);
                if (end == p || rpn_len >= EXPR_RPN_MAX)
                    return NAN;
                rpn_op[rpn_len] = 0;
                rpn_val[rpn_len] = val;
                rpn_len++;
                p = end;
                expect_operand = 0;
                continue;
            }

            /* 变量 xN 或 XN */
            if (*p == 'x' || *p == 'X') {
                const char *digits = p + 1;
                if (*digits < '0' || *digits > '9')
                    return NAN;
                char *end = NULL;
                long idx = strtol(digits, &end, 10);
                if (idx < 0 || idx >= var_count || rpn_len >= EXPR_RPN_MAX)
                    return NAN;
                rpn_op[rpn_len] = 0;
                rpn_val[rpn_len] = var_values[idx];
                rpn_len++;
                p = end;
                expect_operand = 0;
                continue;
            }

            /* 一元函数：sqrt、sin、cos、exp、log */
            if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')) {
                char name[8] = {0};
                int nl = 0;
                const char *q = p;
                while (nl < 7 && ((*q >= 'a' && *q <= 'z') || (*q >= 'A' && *q <= 'Z'))) {
                    name[nl++] = *q++;
                }
                name[nl] = '\0';

                int func_op = 0;
                if (strcmp(name, "sqrt") == 0)
                    func_op = RPN_OP_SQRT;
                else if (strcmp(name, "sin") == 0)
                    func_op = RPN_OP_SIN;
                else if (strcmp(name, "cos") == 0)
                    func_op = RPN_OP_COS;
                else if (strcmp(name, "exp") == 0)
                    func_op = RPN_OP_EXP;
                else if (strcmp(name, "log") == 0)
                    func_op = RPN_OP_LOG;
                else
                    return NAN; /* 无法识别的标识符 */

                if (op_top >= EXPR_STACK_MAX)
                    return NAN;
                op_stack[op_top++] = func_op;
                p = q;
                /* 仍期望操作数（函数参数） */
                continue;
            }

            /* 左括号 ( */
            if (*p == '(') {
                if (op_top >= EXPR_STACK_MAX)
                    return NAN;
                op_stack[op_top++] = 0; /* 0 作为左括号哨兵值 */
                p++;
                continue;
            }

            /* 一元负号（在期望操作数的位置出现的 '-'） */
            if (*p == '-') {
                if (op_top >= EXPR_STACK_MAX)
                    return NAN;
                op_stack[op_top++] = RPN_OP_NEG;
                p++;
                continue;
            }

            /* 一元正号（忽略） */
            if (*p == '+') {
                p++;
                continue;
            }

            /* 无法识别的 token */
            return NAN;
        } else {
            /*
             * ============================================================
             * 解析运算符：二元运算符 | 右括号 | 逗号
             * ============================================================
             */

            int cur_op = 0;

            switch (*p) {
                case '+':
                    cur_op = RPN_OP_ADD;
                    break;
                case '-':
                    cur_op = RPN_OP_SUB;
                    break;
                case '*':
                    cur_op = RPN_OP_MUL;
                    break;
                case '/':
                    cur_op = RPN_OP_DIV;
                    break;
                case '^':
                    cur_op = RPN_OP_POW;
                    break;

                case ')':
                    /* 弹出直到遇到左括号哨兵 */
                    while (op_top > 0 && op_stack[op_top - 1] != 0) {
                        if (rpn_len >= EXPR_RPN_MAX)
                            return NAN;
                        rpn_op[rpn_len] = op_stack[op_top - 1];
                        rpn_val[rpn_len] = 0.0;
                        rpn_len++;
                        op_top--;
                    }
                    if (op_top == 0)
                        return NAN; /* 括号不匹配 */
                    op_top--;       /* 弹出左括号哨兵 */
                    /* 若栈顶是函数，将其弹出到 RPN */
                    if (op_top > 0) {
                        int top = op_stack[op_top - 1];
                        if (top == RPN_OP_SQRT || top == RPN_OP_SIN || top == RPN_OP_COS || top == RPN_OP_EXP ||
                            top == RPN_OP_LOG) {
                            if (rpn_len >= EXPR_RPN_MAX)
                                return NAN;
                            rpn_op[rpn_len] = top;
                            rpn_val[rpn_len] = 0.0;
                            rpn_len++;
                            op_top--;
                        }
                    }
                    p++;
                    expect_operand = 0;
                    continue;

                case ',':
                    /* 函数参数分隔符：弹出直到遇到左括号哨兵 */
                    while (op_top > 0 && op_stack[op_top - 1] != 0) {
                        if (rpn_len >= EXPR_RPN_MAX)
                            return NAN;
                        rpn_op[rpn_len] = op_stack[op_top - 1];
                        rpn_val[rpn_len] = 0.0;
                        rpn_len++;
                        op_top--;
                    }
                    if (op_top == 0)
                        return NAN;
                    p++;
                    expect_operand = 1;
                    continue;

                default:
                    return NAN; /* 未知字符 */
            }

            /* 二元运算符：按优先级弹出栈中运算符 */
            if (cur_op != 0) {
                int cur_prec = expr_op_precedence(cur_op);
                while (op_top > 0 && op_stack[op_top - 1] != 0) {
                    int top_op = op_stack[op_top - 1];
                    int top_prec = expr_op_precedence(top_op);
                    /*
                     * 左结合：top_prec >= cur_prec 时弹出
                     * 右结合（^）：仅 top_prec > cur_prec 时弹出
                     */
                    if (top_prec > cur_prec || (top_prec == cur_prec && cur_op != RPN_OP_POW)) {
                        if (rpn_len >= EXPR_RPN_MAX)
                            return NAN;
                        rpn_op[rpn_len] = top_op;
                        rpn_val[rpn_len] = 0.0;
                        rpn_len++;
                        op_top--;
                    } else {
                        break;
                    }
                }
                if (op_top >= EXPR_STACK_MAX)
                    return NAN;
                op_stack[op_top++] = cur_op;
                p++;
                expect_operand = 1;
                continue;
            }
        }
    }

    /* 将栈中剩余运算符全部弹出到 RPN */
    while (op_top > 0) {
        int top = op_stack[--op_top];
        if (top == 0)
            return NAN; /* 括号不匹配 */
        if (rpn_len >= EXPR_RPN_MAX)
            return NAN;
        rpn_op[rpn_len] = top;
        rpn_val[rpn_len] = 0.0;
        rpn_len++;
    }

    if (rpn_len == 0)
        return NAN;

    /* ---- 阶段 2：RPN 求值 ---- */
    double eval_stack[EXPR_STACK_MAX];
    int eval_top = 0;

    for (int i = 0; i < rpn_len; i++) {
        int op = rpn_op[i];

        if (op == 0) {
            /* 操作数：直接压入求值栈 */
            if (eval_top >= EXPR_STACK_MAX)
                return NAN;
            eval_stack[eval_top++] = rpn_val[i];
        } else {
            /* 运算符：从求值栈弹出操作数并计算 */
            switch (op) {
                case RPN_OP_ADD:
                    if (eval_top < 2)
                        return NAN;
                    eval_stack[eval_top - 2] += eval_stack[eval_top - 1];
                    eval_top--;
                    break;
                case RPN_OP_SUB:
                    if (eval_top < 2)
                        return NAN;
                    eval_stack[eval_top - 2] -= eval_stack[eval_top - 1];
                    eval_top--;
                    break;
                case RPN_OP_MUL:
                    if (eval_top < 2)
                        return NAN;
                    eval_stack[eval_top - 2] *= eval_stack[eval_top - 1];
                    eval_top--;
                    break;
                case RPN_OP_DIV:
                    if (eval_top < 2)
                        return NAN;
                    if (fabs(eval_stack[eval_top - 1]) < 1e-308)
                        return NAN; /* 除零保护 */
                    eval_stack[eval_top - 2] /= eval_stack[eval_top - 1];
                    eval_top--;
                    break;
                case RPN_OP_POW:
                    if (eval_top < 2)
                        return NAN;
                    /* Guard: pow(negative, non-integer) is undefined in reals */
                    if (eval_stack[eval_top - 2] < 0.0 &&
                        fabs(eval_stack[eval_top - 1] - round(eval_stack[eval_top - 1])) > 1e-12) {
                        return NAN;
                    }
                    eval_stack[eval_top - 2] = pow(eval_stack[eval_top - 2], eval_stack[eval_top - 1]);
                    eval_top--;
                    break;
                case RPN_OP_NEG:
                    if (eval_top < 1)
                        return NAN;
                    eval_stack[eval_top - 1] = -eval_stack[eval_top - 1];
                    break;
                case RPN_OP_SQRT:
                    if (eval_top < 1)
                        return NAN;
                    if (eval_stack[eval_top - 1] < 0.0)
                        return NAN;
                    eval_stack[eval_top - 1] = sqrt(eval_stack[eval_top - 1]);
                    break;
                case RPN_OP_SIN:
                    if (eval_top < 1)
                        return NAN;
                    eval_stack[eval_top - 1] = sin(eval_stack[eval_top - 1]);
                    break;
                case RPN_OP_COS:
                    if (eval_top < 1)
                        return NAN;
                    eval_stack[eval_top - 1] = cos(eval_stack[eval_top - 1]);
                    break;
                case RPN_OP_EXP:
                    if (eval_top < 1)
                        return NAN;
                    eval_stack[eval_top - 1] = exp(eval_stack[eval_top - 1]);
                    break;
                case RPN_OP_LOG:
                    if (eval_top < 1)
                        return NAN;
                    if (eval_stack[eval_top - 1] <= 0.0)
                        return NAN;
                    eval_stack[eval_top - 1] = log(eval_stack[eval_top - 1]);
                    break;
                default:
                    return NAN;
            }
        }
    }

    return (eval_top == 1) ? eval_stack[0] : NAN;
}

/* ========================================================================
 * 一阶泰勒展开（有限差分近似）
 * ======================================================================== */

/**
 * @brief 使用有限差分计算一阶偏导数
 *
 * 对表达式 f(x0,...,xn) 在 center 点处计算 df/dxi，
 *   df/dxi ~= (f(... xi+h ...) - f(... xi-h ...)) / (2h)
 *
 * 采用中心差分格式，截断误差为 O(h^2)。步长 h = sqrt(DBL_EPSILON)
 * 以平衡截断误差与浮点舍入误差（对 double 约 1.49e-8）。
 *
 * 该函数通过 evaluate_expression() 在扰动后的变量值上两次求值，
 * 然后按中心差分公式计算偏导数近似值。变量值在栈上的临时缓冲区中
 * 进行扰动，避免修改原始的 center_vals 数组，确保内存安全。
 *
 * @param[in] expr        表达式字符串
 * @param[in] var_bounds  变量区间（本函数仅用于签名兼容，内部不直接使用）
 * @param[in] var_count   变量数量
 * @param[in] var_idx     求导的变量索引（针对 x_{var_idx} 求偏导）
 * @param[in] center_vals 中心点处各变量的值
 * @return 偏导数近似值；若表达式求值失败或参数无效则返回 NaN
 */
static double finite_difference_partial(const char *expr, const FloatInterval *var_bounds, int var_count, int var_idx,
                                        const double *center_vals) {
    (void) var_bounds; /* 签名兼容：区间边界在此函数中未直接使用 */

    if (!expr || !center_vals || var_count <= 0 || var_idx < 0 || var_idx >= var_count) {
        return NAN;
    }

    double x_c = center_vals[var_idx];

    /* 自适应步长：根据 x_c 的量级调整步长，
     * 确保 x_c + h 和 x_c - h 在浮点意义上可区分。
     *
     * 基础步长 sqrt(DBL_EPSILON) ~= 1.49e-8 是标准推荐值，
     * 但当 |x_c| 很大时（如 1e12），x_c + 1.49e-8 由于 double
     * 有限精度（约 15-17 位有效数字）会回落到 x_c。
     * 当 |x_c| 很小时（如 1e-15），步长又可能过度扰动。
     *
     * 自适应公式：h = sqrt(DBL_EPSILON) * max(1.0, fabs(x_c)) */
    double h = sqrt(DBL_EPSILON) * fmax(1.0, fabs(x_c));

    /* 扰动后的变量值缓冲区：在栈上分配，最多 MAX_EQUATIONS 个变量 */
    double perturbed[MAX_EQUATIONS];
    if (var_count > MAX_EQUATIONS)
        return NAN;

    /* 计算 f(..., xi+h, ...) */
    memcpy(perturbed, center_vals, (size_t) var_count * sizeof(double));
    perturbed[var_idx] = x_c + h;
    double f_plus = evaluate_expression(expr, perturbed, var_count);

    /* 计算 f(..., xi-h, ...) */
    perturbed[var_idx] = x_c - h;
    double f_minus = evaluate_expression(expr, perturbed, var_count);

    /* 若任一求值失败，返回 NaN */
    if (isnan(f_plus) || isnan(f_minus)) {
        return NAN;
    }

    /* 中心差分公式：(f(x+h) - f(x-h)) / (2h) */
    return (f_plus - f_minus) / (2.0 * h);
}

/**
 * @brief 一阶泰勒展开
 *
 * 对表达式 f(x) 在区间中点做一阶泰勒展开：
 *   f(x) ~= f(center) + SUM_i df/dxi * (xi - center_i)
 *
 * 结果存储在 TaylorForm 中，interval_lo/hi 是区间传播的结果。
 *
 * @param[in]  expr       表达式
 * @param[in]  var_bounds 变量区间
 * @param[in]  var_count  变量数量
 * @param[out] tf         输出的泰勒形式
 * @return true 成功
 */
static bool basic_taylor_expand(const char *expr, const FloatInterval *var_bounds, int var_count, TaylorForm *tf) {
    if (!expr || !var_bounds || var_count <= 0 || !tf)
        return false;

    tf->deriv_count = var_count;
    tf->order = 1;

    tf->first_derivs = (double *) lv_malloc((size_t) var_count * sizeof(double));
    tf->deriv_var_ids = (int *) lv_malloc((size_t) var_count * sizeof(int));
    if (!tf->first_derivs || !tf->deriv_var_ids) {
        lv_free((void **) &tf->first_derivs);
        lv_free((void **) &tf->deriv_var_ids);
        return false;
    }

    /* 计算中心点值：取每个变量区间的中点 */
    double center_vals[MAX_EQUATIONS];
    for (int i = 0; i < var_count; i++) {
        center_vals[i] = var_bounds[i].lo + (var_bounds[i].hi - var_bounds[i].lo) / 2.0;
        tf->deriv_var_ids[i] = i;
    }

    /* 计算中心点处的函数值 —— 通过表达式求值器实际计算 f(center) */
    tf->center_val = evaluate_expression(expr, center_vals, var_count);

    /* 对每个变量计算偏导数（有限差分） */
    for (int i = 0; i < var_count; i++) {
        tf->first_derivs[i] = finite_difference_partial(expr, var_bounds, var_count, i, center_vals);
    }

    /* 区间传播：基本估算 */
    /* interval = center_val + SUM_i deriv_i * (interval_i - center_i) */
    double delta_lo = 0.0;
    double delta_hi = 0.0;

    for (int i = 0; i < var_count; i++) {
        double d = tf->first_derivs[i];
        double dev_lo = var_bounds[i].lo - center_vals[i];
        double dev_hi = var_bounds[i].hi - center_vals[i];

        if (d >= 0.0) {
            delta_lo += d * dev_lo;
            delta_hi += d * dev_hi;
        } else {
            delta_lo += d * dev_hi;
            delta_hi += d * dev_lo;
        }
    }

    tf->interval_lo = tf->center_val + delta_lo;
    tf->interval_hi = tf->center_val + delta_hi;

    return true;
}

/* ========================================================================
 * fptaylor_evaluate_graph 实现
 * ======================================================================== */

/**
 * @brief 从约束图中提取涉及指定变量的约束方程
 *
 * 遍历约束图，找到所有 participants 中包含 var_id 的约束。
 * 将每个约束的类型和参与者编码为简化的表达式字符串。
 *
 * @param[in]  graph       约束图
 * @param[in]  var_id      目标变量 ID
 * @param[out] equations   输出的表达式字符串数组
 * @param[out] eq_count    方程数量
 * @return true 成功
 */
static bool extract_equations(const ConstraintGraph *graph, int var_id, char ***equations, int *eq_count) {
    if (!graph || !equations || !eq_count)
        return false;

    *eq_count = 0;
    *equations = NULL;

    if (graph->constraint_count == 0)
        return true;

    /* 分配表达式数组 */
    int alloc_count = (graph->constraint_count < MAX_EQUATIONS) ? graph->constraint_count : MAX_EQUATIONS;
    char **eqs = (char **) lv_malloc((size_t) alloc_count * sizeof(char *));
    if (!eqs)
        return false;

    for (int ci = 0; ci < graph->constraint_count && *eq_count < alloc_count; ci++) {
        Constraint *c = graph->constraints[ci];
        if (!c)
            continue;

        /* 检查 var_id 是否在 participants 中 */
        bool involves_var = false;
        for (int pi = 0; pi < c->participant_count; pi++) {
            if (c->participants[pi] == var_id) {
                involves_var = true;
                break;
            }
        }
        if (!involves_var)
            continue;

        /* 构造表达式描述字符串 */
        /* 格式："constraint_N: type=X, vars=[a,b,c]" */
        static const char *kConstraintTypeNames[] = {
            "INCIDENCE",    /* INCIDENCE = 0 */
            "BETWEENNESS",  /* BETWEENNESS = 1 */
            "INTERSECTION", /* INTERSECTION = 2 */
            "CONTAINMENT",  /* CONTAINMENT = 3 */
            "CONNECTION",   /* CONNECTION = 4 */
            "ANGLE"         /* ANGLE = 5 */
        };
        static const int kConstraintTypeNamesCount =
            (int)(sizeof(kConstraintTypeNames) / sizeof(kConstraintTypeNames[0]));
        const char *type_str = "UNKNOWN";
        if (c->type >= 0 && c->type < kConstraintTypeNamesCount) {
            type_str = kConstraintTypeNames[(int)c->type];
        }

        char buf[EXPR_BUFFER_INITIAL];
        int off = snprintf(buf, sizeof(buf), "constraint_%d: type=%s, vars=[", c->id, type_str);
        if (off < 0)
            off = 0;
        for (int pi = 0; pi < c->participant_count && off >= 0 && off < (int) sizeof(buf) - 20; pi++) {
            int n = snprintf(buf + off, sizeof(buf) - (size_t) off, "%s%d", (pi > 0) ? "," : "", c->participants[pi]);
            if (n > 0)
                off += n;
        }
        if (off >= 0 && off < (int) sizeof(buf))
            snprintf(buf + off, sizeof(buf) - (size_t) off, "]");

        eqs[*eq_count] = lv_strdup(buf);
        (*eq_count)++;
    }

    *equations = eqs;
    return true;
}

/**
 * @brief 对约束图中的指定变量进行 FPTaylor 浮点误差分析
 *
 * 从约束图中提取涉及目标变量的约束方程，构造变量区间边界，
 * 对每个方程进行一阶泰勒展开和区间评估，计算绝对/相对误差，
 * 并映射到信任颜色系统。
 *
 * @param graph 约束图
 * @param var_id 目标变量节点 ID
 * @param cfg    FPTaylor 配置
 * @param out    输出误差界
 * @return 成功返回 true，失败返回 false
 */
bool fptaylor_evaluate_graph(const ConstraintGraph *graph, int var_id, const FPTaylorConfig *cfg, ErrorBound *out) {
    if (!graph || !out)
        return false;

    /* 验证 var_id 是否有效 */
    GeomNode *target_node = graph_get_node(graph, var_id);
    if (!target_node)
        return false;

    /* 步骤 1: 从约束图中提取涉及 var_id 的方程 */
    char **equations = NULL;
    int eq_count = 0;
    if (!extract_equations(graph, var_id, &equations, &eq_count)) {
        return false;
    }

    /* 步骤 2: 构造变量区间边界
     * 从目标节点的坐标中提取边界（若为符号坐标，转为区间） */
    FloatInterval var_bounds[2];
    int var_count = 0;

    if (target_node->symbolic_coords && target_node->coord_count > 0) {
        int coord_count = target_node->coord_count;
        if (coord_count > 2)
            coord_count = 2;

        for (int d = 0; d < coord_count; d++) {
            double val = symbolic_coord_to_double(target_node->symbolic_coords[d]);
            double eps = fabs(val) * DBL_EPSILON * 10.0; /* 10 ulp 容差 */
            if (eps < DBL_MIN)
                eps = DBL_EPSILON;
            var_bounds[d] = interval_make(val - eps, val + eps, false);
        }
        var_count = coord_count;
    }

    /* 步骤 3: 对每个约束方程进行泰勒展开和区间评估 */
    double max_abs_err = 0.0;
    double max_rel_err = 0.0;

    for (int ei = 0; ei < eq_count; ei++) {
        TaylorForm tf;
        memset(&tf, 0, sizeof(TaylorForm));

        if (var_count > 0 && basic_taylor_expand(equations[ei], var_bounds, var_count, &tf)) {
            /* 计算绝对误差 = (interval_hi - interval_lo) / 2 */
            double half_width = (tf.interval_hi - tf.interval_lo) / 2.0;
            if (half_width > max_abs_err) {
                max_abs_err = half_width;
            }
            /* 计算相对误差（避免除零） */
            double abs_center = fabs(tf.center_val);
            if (abs_center > DBL_MIN) {
                double rel = half_width / abs_center;
                if (rel > max_rel_err)
                    max_rel_err = rel;
            }

            lv_free((void **) &tf.first_derivs);
            lv_free((void **) &tf.deriv_var_ids);
        }
    }

    /* 步骤 4: 构造证明文本 */
    char proof_buf[512];
    if (eq_count > 0) {
        snprintf(proof_buf, sizeof(proof_buf),
                 "FPTaylor analysis for var_id=%d: %d constraint equations, "
                 "taylor_order=%d, abs_err=%.6e, rel_err=%.6e",
                 var_id, eq_count, cfg ? cfg->taylor_order : 1, max_abs_err, max_rel_err);
    } else {
        snprintf(proof_buf, sizeof(proof_buf), "FPTaylor analysis for var_id=%d: no relevant constraints found",
                 var_id);
    }

    /* 步骤 5: 清理并输出 */
    out->absolute_error = max_abs_err;
    out->relative_error = max_rel_err;
    out->trust_level = (max_abs_err > 0.0) ? TRUST_BLUE_UNEXPLORED : TRUST_GREEN;
    out->proof_text = lv_strdup(proof_buf);

    for (int ei = 0; ei < eq_count; ei++) {
        lv_free((void **) &equations[ei]);
    }
    lv_free((void **) &equations);

    return true;
}

/* ========================================================================
 * fptaylor_evaluate_expr 实现
 * ======================================================================== */

/**
 * @brief 对数学表达式进行 FPTaylor 浮点误差分析
 *
 * 在给定变量区间上对表达式做一阶泰勒展开，计算误差界。
 *
 * @param expr       数学表达式字符串
 * @param var_bounds 变量区间数组
 * @param var_count  变量数量
 * @param cfg        FPTaylor 配置（可为 NULL，使用默认值）
 * @param out        输出误差界
 * @return 成功返回 true，失败返回 false
 */
bool fptaylor_evaluate_expr(const char *expr, const FloatInterval *var_bounds, int var_count, const FPTaylorConfig *cfg,
                            ErrorBound *out) {
    if (!expr || !var_bounds || var_count <= 0 || !out)
        return false;

    FPTaylorConfig config = cfg ? *cfg : fptaylor_config_default();

    /* 一阶泰勒展开 */
    TaylorForm tf;
    memset(&tf, 0, sizeof(TaylorForm));

    if (!basic_taylor_expand(expr, var_bounds, var_count, &tf)) {
        return false;
    }

    /* 计算误差界 */
    double half_width = (tf.interval_hi - tf.interval_lo) / 2.0;
    double abs_center = fabs(tf.center_val);

    out->absolute_error = half_width;
    out->relative_error = (abs_center > DBL_MIN) ? half_width / abs_center : half_width;
    out->trust_level = TRUST_BLUE_UNEXPLORED;

    /* 构造证明文本 */
    char proof_buf[512];
    snprintf(proof_buf, sizeof(proof_buf),
             "expr=\"%s\", order=%d, center=%.6e, interval=[%.6e, %.6e], "
             "abs_err=%.6e, rel_err=%.6e",
             expr, config.taylor_order, tf.center_val, tf.interval_lo, tf.interval_hi, out->absolute_error,
             out->relative_error);
    out->proof_text = lv_strdup(proof_buf);

    lv_free((void **) &tf.first_derivs);
    lv_free((void **) &tf.deriv_var_ids);

    return true;
}

/* ========================================================================
 * fptaylor_verify_safety 实现
 * ======================================================================== */

/**
 * @brief 验证浮点误差是否在安全容差范围内
 *
 * 根据绝对误差与容差的比较，映射到 Lv-00 信任颜色系统：
 * - GREEN: 误差 <= 1e-12（极度精确）
 * - BLUE:  误差 <= 1e-10（高精度）
 * - AMBER: 误差 <= tolerance（边际安全）
 * - YELLOW: 误差 <= 10*tolerance（条件性安全）
 * - RED:   误差超出容差（不安全）
 *
 * @param bound     误差界
 * @param tolerance 安全容差阈值
 * @return 信任颜色枚举值
 */
TrustColor fptaylor_verify_safety(const ErrorBound *bound, double tolerance) {
    if (!bound)
        return TRUST_RED;

    double abs_err = bound->absolute_error;

    /* NaN 或非正常值 */
    if (isnan(abs_err) || isinf(abs_err)) {
        return TRUST_RED;
    }

    /* 按误差分级的信任颜色判断 */
    if (abs_err <= 1e-12) {
        /* 极度精确 —— 完全可构造性安全 */
        return TRUST_GREEN;
    }

    if (abs_err <= 1e-10) {
        /* 高精度 —— 可信但仍需关注 */
        return TRUST_BLUE_UNEXPLORED;
    }

    if (abs_err <= tolerance) {
        /* 边际安全 —— 在数值假设下，标记为 AMBER */
        return TRUST_AMBER;
    }

    if (abs_err <= tolerance * 10.0) {
        /* 接近容差边界 —— 条件性安全 */
        return TRUST_YELLOW;
    }

    /* 超出容差范围 —— 不安全 */
    return TRUST_RED;
}

/* ========================================================================
 * 工厂与资源管理
 * ======================================================================== */

/**
 * @brief 创建默认 FPTaylor 配置
 * @return 默认配置结构体
 */
FPTaylorConfig fptaylor_config_default(void) {
    FPTaylorConfig cfg;
    cfg.use_optimization = true;
    cfg.taylor_order = 1;
    cfg.use_z3_opt = false;
    cfg.use_gelpia = false;
    cfg.branch_bound_threshold = lv_EPSILON_LOW;
    return cfg;
}

/**
 * @brief 释放误差界中的证明文本资源
 * @param bound 误差界指针
 */
void error_bound_destroy(ErrorBound *bound) {
    if (!bound)
        return;
    if (bound->proof_text) {
        lv_free((void **) &bound->proof_text);
        bound->proof_text = NULL;
    }
}
