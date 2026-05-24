/**
 * @file float_error.c
 * @brief FPTaylor 椋庢牸娴偣璇樊楠岃瘉瀹炵幇 鈥斺€?鍖洪棿绠楁湳 + 娉板嫆灞曞紑妗? *
 * @details 瀹炵幇 IEEE 1788 鍖洪棿绠楁湳鐨勫熀鏈搷浣滐紙鍔犲噺涔橀櫎锛変互鍙? *          甯哥敤瓒呰秺鍑芥暟鐨勫尯闂寸増鏈€傛彁渚涗竴闃舵嘲鍕掑睍寮€鐨勬湁闄愬樊鍒嗚繎浼笺€? *          灏嗙害鏉熷浘鍙橀噺杞崲涓哄彲璇勪及琛ㄨ揪寮忥紝骞堕€氳繃璇樊涓庡宸瘮杈? *          鏄犲皠鍒?Lv-00 淇′换棰滆壊绯荤粺銆? *
 *          鍖洪棿绠楁湳閬靛惊鏈€灏?鏈€澶у師鐞嗭細
 *          - 鍔犳硶/鍑忔硶锛氱鐐圭洿鎺ヨ繍绠? *          - 涔樻硶锛氬洓涓鐐圭殑鏈€灏?鏈€澶у€? *          - 闄ゆ硶锛氶€氳繃鍊掓暟涔樻硶锛屾帓闄ら浂鐐瑰尯闂? *
 *          鏍稿績妯″潡锛? *          - 鍖洪棿绠楁湳瀹屾暣瀹炵幇锛歛dd/sub/mul/div/sqrt/sin/cos/exp/log
 *          - 涓€闃舵嘲鍕掑睍寮€锛氭湁闄愬樊鍒嗚繎浼煎亸瀵兼暟
 *          - fptaylor_evaluate_graph锛氱害鏉熷浘 鈫?琛ㄨ揪寮?鈫?鍖洪棿璇勪及
 *          - fptaylor_verify_safety锛氳宸?鈫?淇′换棰滆壊
 *
 * @author Lv-00 Project
 * @version 3.3.0
 *
 * @dependencies
 *   - float_error.h        : 娴偣璇樊鍒嗘瀽鍏叡鎺ュ彛
 *   - constraint_graph.h   : 绾︽潫鍥炬暟鎹粨鏋? *   - symbolic_coord.h     : 绗﹀彿鍧愭爣涓?TrustColor
 *   - lv00_utils.h         : 缁熶竴鍐呭瓨鍒嗛厤鍣? *   - lv00_internal.h      : 鍐呴儴甯搁噺涓庡伐鍏峰畯
 */

#include "float_error.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constraint_graph.h"
#include "lv00_internal.h"
#include "lv00_utils.h"
#include "symbolic_coord.h"

/* ========================================================================
 * 鍐呴儴甯搁噺
 * ======================================================================== */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/** 琛ㄨ揪寮忕紦鍐插尯鍒濆澶у皬 */
#define EXPR_BUFFER_INITIAL 256

/** 鏈€澶ф彁鍙栫殑绾︽潫鏂圭▼鏁?*/
#define MAX_EQUATIONS 64

/** 瀹夊叏涓嬮檺锛堥伩鍏?log(0) 绛夐棶棰橈級 */
#define SAFE_MIN_POSITIVE 1e-308

/* ========================================================================
 * 鍐呴儴杈呭姪瀹? * ======================================================================== */

/** 杩斿洖 a 鍜?b 鐨勬渶灏忓€?*/
static double double_min(double a, double b) {
    return (a < b) ? a : b;
}

/** 杩斿洖 a 鍜?b 鐨勬渶澶у€?*/
static double double_max(double a, double b) {
    return (a > b) ? a : b;
}

/**
 * @brief 鍚戜笅鑸嶅叆锛堜繚瀹堜笅鐣屼及璁★級
 *
 * 灏?double 鍊煎悜璐熸棤绌锋柟鍚戝井璋冿紝纭繚鍖洪棿涓嬬晫鏄繚瀹堢殑銆? * 涔樹互 (1 - DBL_EPSILON) 浠ュ鐞嗘诞鐐硅垗鍏ャ€? */
static double round_down(double x) {
    if (x > 0.0) {
        return x * (1.0 - DBL_EPSILON);
    } else {
        return x * (1.0 + DBL_EPSILON);
    }
}

/**
 * @brief 鍚戜笂鑸嶅叆锛堜繚瀹堜笂鐣屼及璁★級
 *
 * 灏?double 鍊煎悜姝ｆ棤绌锋柟鍚戝井璋冿紝纭繚鍖洪棿涓婄晫鏄繚瀹堢殑銆? */
static double round_up(double x) {
    if (x > 0.0) {
        return x * (1.0 + DBL_EPSILON);
    } else {
        return x * (1.0 - DBL_EPSILON);
    }
}

/* ========================================================================
 * 鍖洪棿绠楁湳 鈥斺€?瀹屾暣瀹炵幇锛堟渶灏?鏈€澶у師鐞嗭級
 * ======================================================================== */

FloatInterval interval_make(double lo, double hi, bool is_exact) {
    FloatInterval iv;
    iv.lo = lo;
    iv.hi = hi;
    iv.is_exact = is_exact;
    return iv;
}

FloatInterval interval_add(FloatInterval a, FloatInterval b) {
    FloatInterval result;
    result.lo = round_down(a.lo + b.lo);
    result.hi = round_up(a.hi + b.hi);
    result.is_exact = a.is_exact && b.is_exact;
    return result;
}

FloatInterval interval_sub(FloatInterval a, FloatInterval b) {
    FloatInterval result;
    /* a - b: 涓嬬晫 = a.lo - b.hi, 涓婄晫 = a.hi - b.lo */
    result.lo = round_down(a.lo - b.hi);
    result.hi = round_up(a.hi - b.lo);
    result.is_exact = a.is_exact && b.is_exact;
    return result;
}

FloatInterval interval_mul(FloatInterval a, FloatInterval b) {
    /* 璁＄畻鍥涗釜瑙掔偣 */
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

FloatInterval interval_div(FloatInterval a, FloatInterval b) {
    FloatInterval result;

    /* 妫€鏌ュ垎姣嶆槸鍚﹁法瓒婇浂鐐?*/
    if (b.lo <= 0.0 && b.hi >= 0.0) {
        /* 鍒嗘瘝鍖呭惈闆剁偣锛氳繑鍥?NaN 鍖洪棿琛ㄧず鏃犲畾涔?*/
        result.lo = -HUGE_VAL;
        result.hi = HUGE_VAL;
        result.is_exact = false;
        return result;
    }

    /* 閫氳繃鍊掓暟 + 涔樻硶瀹炵幇闄ゆ硶 */
    if (b.hi < 0.0) {
        /* 鍒嗘瘝鍏ㄨ礋锛氬彇鍊掓暟鑼冨洿 [1/b.hi, 1/b.lo] */
        double inv_lo = 1.0 / b.hi;
        double inv_hi = 1.0 / b.lo;
        FloatInterval inv_b = interval_make(inv_lo, inv_hi, b.is_exact);
        result = interval_mul(a, inv_b);
    } else {
        /* 鍒嗘瘝鍏ㄦ锛氬彇鍊掓暟鑼冨洿 [1/b.hi, 1/b.lo] */
        double inv_lo = 1.0 / b.hi;
        double inv_hi = 1.0 / b.lo;
        FloatInterval inv_b = interval_make(inv_lo, inv_hi, b.is_exact);
        result = interval_mul(a, inv_b);
    }

    return result;
}

FloatInterval interval_sqrt(FloatInterval a) {
    FloatInterval result;
    if (a.lo < 0.0) {
        /* 璐熸暟閮ㄥ垎鏃犲疄鏁板畾涔夛紝鎴柇鍒?0 */
        result.lo = 0.0;
    } else {
        result.lo = round_down(sqrt(a.lo));
    }
    result.hi = round_up(sqrt(a.hi));
    result.is_exact = a.is_exact && (a.lo == a.hi);
    return result;
}

FloatInterval interval_sin(FloatInterval a) {
    /* sin 鍦?[-1, 1] 涔嬮棿锛岄渶瑕佸鐞嗛潪鍗曡皟鍖洪棿 */
    double sin_lo = sin(a.lo);
    double sin_hi = sin(a.hi);

    /* 妫€鏌ュ尯闂存槸鍚﹁法瓒?pi/2 + k*pi锛堟瀬澶у€肩偣锛?*/
    double width = a.hi - a.lo;
    double min_val = double_min(sin_lo, sin_hi);
    double max_val = double_max(sin_lo, sin_hi);

    if (width >= 2.0 * M_PI) {
        /* 鍖洪棿瓒呰繃涓€涓畬鏁村懆鏈?鈫?瑕嗙洊鍏ㄨ寖鍥?*/
        min_val = -1.0;
        max_val = 1.0;
    } else {
        /* 妫€鏌?pi/2 + 2k*pi 鍜?3pi/2 + 2k*pi 鏄惁鍦ㄥ尯闂村唴 */
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

FloatInterval interval_cos(FloatInterval a) {
    /* cos 鐗规€х被浼?sin锛屽亸绉?pi/2 */
    double cos_lo = cos(a.lo);
    double cos_hi = cos(a.hi);
    double width = a.hi - a.lo;
    double min_val = double_min(cos_lo, cos_hi);
    double max_val = double_max(cos_lo, cos_hi);

    if (width >= 2.0 * M_PI) {
        min_val = -1.0;
        max_val = 1.0;
    } else {
        /* 妫€鏌?k*pi锛坈os 鐨勬瀬鍊肩偣锛夋槸鍚﹀湪鍖洪棿鍐?*/
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

FloatInterval interval_exp(FloatInterval a) {
    /* exp 鍗曡皟閫掑 */
    FloatInterval result;
    result.lo = round_down(exp(a.lo));
    result.hi = round_up(exp(a.hi));
    result.is_exact = a.is_exact && (a.lo == a.hi);
    return result;
}

FloatInterval interval_log(FloatInterval a) {
    FloatInterval result;
    if (a.lo <= 0.0) {
        /* log 鍦ㄩ潪姝ｅ尯闂存棤瀹氫箟 */
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
#define RPN_OP_ADD   (-1)   /**< 加法 + */
#define RPN_OP_SUB   (-2)   /**< 减法 - */
#define RPN_OP_MUL   (-3)   /**< 乘法 * */
#define RPN_OP_DIV   (-4)   /**< 除法 / */
#define RPN_OP_POW   (-5)   /**< 幂运算 ^ */
#define RPN_OP_SQRT  (-10)  /**< sqrt 函数 */
#define RPN_OP_SIN   (-11)  /**< sin 函数 */
#define RPN_OP_COS   (-12)  /**< cos 函数 */
#define RPN_OP_EXP   (-13)  /**< exp 函数 */
#define RPN_OP_LOG   (-14)  /**< log 函数 */
#define RPN_OP_NEG   (-20)  /**< 一元负号 */

/** @brief 表达式求值栈的最大深度 */
#define EXPR_STACK_MAX  128
/** @brief RPN 输出队列的最大长度 */
#define EXPR_RPN_MAX    256

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
        case RPN_OP_ADD: case RPN_OP_SUB: return 1;
        case RPN_OP_MUL: case RPN_OP_DIV: return 2;
        case RPN_OP_NEG: return 3;
        case RPN_OP_POW: return 4;
        default: return 0;
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
            if ((*p >= '0' && *p <= '9') ||
                (*p == '.' && (*(p + 1) >= '0' && *(p + 1) <= '9'))) {
                char *end = NULL;
                double val = strtod(p, &end);
                if (end == p || rpn_len >= EXPR_RPN_MAX) return NAN;
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
                if (*digits < '0' || *digits > '9') return NAN;
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
                while (nl < 7 &&
                       ((*q >= 'a' && *q <= 'z') || (*q >= 'A' && *q <= 'Z'))) {
                    name[nl++] = *q++;
                }
                name[nl] = '\0';

                int func_op = 0;
                if (strcmp(name, "sqrt") == 0)      func_op = RPN_OP_SQRT;
                else if (strcmp(name, "sin") == 0)  func_op = RPN_OP_SIN;
                else if (strcmp(name, "cos") == 0)  func_op = RPN_OP_COS;
                else if (strcmp(name, "exp") == 0)  func_op = RPN_OP_EXP;
                else if (strcmp(name, "log") == 0)  func_op = RPN_OP_LOG;
                else return NAN; /* 无法识别的标识符 */

                if (op_top >= EXPR_STACK_MAX) return NAN;
                op_stack[op_top++] = func_op;
                p = q;
                /* 仍期望操作数（函数参数） */
                continue;
            }

            /* 左括号 ( */
            if (*p == '(') {
                if (op_top >= EXPR_STACK_MAX) return NAN;
                op_stack[op_top++] = 0; /* 0 作为左括号哨兵值 */
                p++;
                continue;
            }

            /* 一元负号（在期望操作数的位置出现的 '-'） */
            if (*p == '-') {
                if (op_top >= EXPR_STACK_MAX) return NAN;
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
                case '+': cur_op = RPN_OP_ADD; break;
                case '-': cur_op = RPN_OP_SUB; break;
                case '*': cur_op = RPN_OP_MUL; break;
                case '/': cur_op = RPN_OP_DIV; break;
                case '^': cur_op = RPN_OP_POW; break;

                case ')':
                    /* 弹出直到遇到左括号哨兵 */
                    while (op_top > 0 && op_stack[op_top - 1] != 0) {
                        if (rpn_len >= EXPR_RPN_MAX) return NAN;
                        rpn_op[rpn_len] = op_stack[op_top - 1];
                        rpn_val[rpn_len] = 0.0;
                        rpn_len++;
                        op_top--;
                    }
                    if (op_top == 0) return NAN; /* 括号不匹配 */
                    op_top--; /* 弹出左括号哨兵 */
                    /* 若栈顶是函数，将其弹出到 RPN */
                    if (op_top > 0) {
                        int top = op_stack[op_top - 1];
                        if (top == RPN_OP_SQRT || top == RPN_OP_SIN ||
                            top == RPN_OP_COS || top == RPN_OP_EXP ||
                            top == RPN_OP_LOG) {
                            if (rpn_len >= EXPR_RPN_MAX) return NAN;
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
                        if (rpn_len >= EXPR_RPN_MAX) return NAN;
                        rpn_op[rpn_len] = op_stack[op_top - 1];
                        rpn_val[rpn_len] = 0.0;
                        rpn_len++;
                        op_top--;
                    }
                    if (op_top == 0) return NAN;
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
                    if (top_prec > cur_prec ||
                        (top_prec == cur_prec && cur_op != RPN_OP_POW)) {
                        if (rpn_len >= EXPR_RPN_MAX) return NAN;
                        rpn_op[rpn_len] = top_op;
                        rpn_val[rpn_len] = 0.0;
                        rpn_len++;
                        op_top--;
                    } else {
                        break;
                    }
                }
                if (op_top >= EXPR_STACK_MAX) return NAN;
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
        if (top == 0) return NAN; /* 括号不匹配 */
        if (rpn_len >= EXPR_RPN_MAX) return NAN;
        rpn_op[rpn_len] = top;
        rpn_val[rpn_len] = 0.0;
        rpn_len++;
    }

    if (rpn_len == 0) return NAN;

    /* ---- 阶段 2：RPN 求值 ---- */
    double eval_stack[EXPR_STACK_MAX];
    int eval_top = 0;

    for (int i = 0; i < rpn_len; i++) {
        int op = rpn_op[i];

        if (op == 0) {
            /* 操作数：直接压入求值栈 */
            if (eval_top >= EXPR_STACK_MAX) return NAN;
            eval_stack[eval_top++] = rpn_val[i];
        } else {
            /* 运算符：从求值栈弹出操作数并计算 */
            switch (op) {
                case RPN_OP_ADD:
                    if (eval_top < 2) return NAN;
                    eval_stack[eval_top - 2] += eval_stack[eval_top - 1];
                    eval_top--;
                    break;
                case RPN_OP_SUB:
                    if (eval_top < 2) return NAN;
                    eval_stack[eval_top - 2] -= eval_stack[eval_top - 1];
                    eval_top--;
                    break;
                case RPN_OP_MUL:
                    if (eval_top < 2) return NAN;
                    eval_stack[eval_top - 2] *= eval_stack[eval_top - 1];
                    eval_top--;
                    break;
                case RPN_OP_DIV:
                    if (eval_top < 2) return NAN;
                    if (fabs(eval_stack[eval_top - 1]) < 1e-308)
                        return NAN; /* 除零保护 */
                    eval_stack[eval_top - 2] /= eval_stack[eval_top - 1];
                    eval_top--;
                    break;
                case RPN_OP_POW:
                    if (eval_top < 2) return NAN;
                    eval_stack[eval_top - 2] =
                        pow(eval_stack[eval_top - 2], eval_stack[eval_top - 1]);
                    eval_top--;
                    break;
                case RPN_OP_NEG:
                    if (eval_top < 1) return NAN;
                    eval_stack[eval_top - 1] = -eval_stack[eval_top - 1];
                    break;
                case RPN_OP_SQRT:
                    if (eval_top < 1) return NAN;
                    if (eval_stack[eval_top - 1] < 0.0) return NAN;
                    eval_stack[eval_top - 1] =
                        sqrt(eval_stack[eval_top - 1]);
                    break;
                case RPN_OP_SIN:
                    if (eval_top < 1) return NAN;
                    eval_stack[eval_top - 1] =
                        sin(eval_stack[eval_top - 1]);
                    break;
                case RPN_OP_COS:
                    if (eval_top < 1) return NAN;
                    eval_stack[eval_top - 1] =
                        cos(eval_stack[eval_top - 1]);
                    break;
                case RPN_OP_EXP:
                    if (eval_top < 1) return NAN;
                    eval_stack[eval_top - 1] =
                        exp(eval_stack[eval_top - 1]);
                    break;
                case RPN_OP_LOG:
                    if (eval_top < 1) return NAN;
                    if (eval_stack[eval_top - 1] <= 0.0) return NAN;
                    eval_stack[eval_top - 1] =
                        log(eval_stack[eval_top - 1]);
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

    if (!expr || !center_vals || var_count <= 0 ||
        var_idx < 0 || var_idx >= var_count) {
        return NAN;
    }

    /* 步长：约 1.49e-8 for double，平衡截断与舍入误差 */
    double h = sqrt(DBL_EPSILON);
    double x_c = center_vals[var_idx];

    /* 扰动后的变量值缓冲区：在栈上分配，最多 MAX_EQUATIONS 个变量 */
    double perturbed[MAX_EQUATIONS];
    if (var_count > MAX_EQUATIONS) return NAN;

    /* 计算 f(..., xi+h, ...) */
    memcpy(perturbed, center_vals, (size_t)var_count * sizeof(double));
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
 * @brief 涓€闃舵嘲鍕掑睍寮€
 *
 * 瀵硅〃杈惧紡 f(x) 鍦ㄥ尯闂翠腑蹇冪偣鍋氫竴闃舵嘲鍕掑睍寮€锛? *   f(x) ~= f(center) + SUM_i df/dxi * (xi - center_i)
 *
 * 缁撴灉瀛樺偍鍦?TaylorForm 涓紝interval_lo/hi 鏄尯闂翠紶鎾殑缁撴灉銆? *
 * @param[in]  expr       琛ㄨ揪寮? * @param[in]  var_bounds 鍙橀噺鍖洪棿
 * @param[in]  var_count  鍙橀噺鏁伴噺
 * @param[out] tf         杈撳嚭鐨勬嘲鍕掑舰寮? * @return true 鎴愬姛
 */
static bool basic_taylor_expand(const char *expr, const FloatInterval *var_bounds, int var_count, TaylorForm *tf) {
    if (!expr || !var_bounds || var_count <= 0 || !tf)
        return false;

    tf->deriv_count = var_count;
    tf->order = 1;

    tf->first_derivs = (double *) lv00_malloc(var_count * sizeof(double));
    tf->deriv_var_ids = (int *) lv00_malloc(var_count * sizeof(int));
    if (!tf->first_derivs || !tf->deriv_var_ids) {
        free(tf->first_derivs);
        free(tf->deriv_var_ids);
        return false;
    }

    /* 璁＄畻涓績鐐瑰€硷細鍙栨瘡涓彉閲忓尯闂寸殑涓偣 */
    double center_vals[MAX_EQUATIONS];
    for (int i = 0; i < var_count; i++) {
        center_vals[i] = (var_bounds[i].lo + var_bounds[i].hi) / 2.0;
        tf->deriv_var_ids[i] = i;
    }

    /* 计算中心点处的函数值 —— 通过表达式求值器实际计算 f(center) */
    tf->center_val = evaluate_expression(expr, center_vals, var_count);

    /* 瀵规瘡涓彉閲忚绠楀亸瀵兼暟锛堟湁闄愬樊鍒嗭級 */
    for (int i = 0; i < var_count; i++) {
        tf->first_derivs[i] = finite_difference_partial(expr, var_bounds, var_count, i, center_vals);
    }

    /* 鍖洪棿浼犳挱锛氬熀鏈及璁?*/
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
 * fptaylor_evaluate_graph 瀹炵幇
 * ======================================================================== */

/**
 * @brief 浠庣害鏉熷浘涓彁鍙栨秹鍙婃寚瀹氬彉閲忕殑绾︽潫鏂圭▼
 *
 * 閬嶅巻绾︽潫鍥撅紝鎵惧埌鎵€鏈?participants 涓寘鍚?var_id 鐨勭害鏉熴€? * 灏嗘瘡涓害鏉熺殑绫诲瀷鍜屽弬涓庤€呯紪鐮佷负绠€鍖栫殑琛ㄨ揪寮忓瓧绗︿覆銆? *
 * @param[in]  graph       绾︽潫鍥? * @param[in]  var_id      鐩爣鍙橀噺 ID
 * @param[out] equations   杈撳嚭鐨勮〃杈惧紡瀛楃涓叉暟缁? * @param[out] eq_count    鏂圭▼鏁伴噺
 * @return true 鎴愬姛
 */
static bool extract_equations(const ConstraintGraph *graph, int var_id, char ***equations, int *eq_count) {
    if (!graph || !equations || !eq_count)
        return false;

    *eq_count = 0;
    *equations = NULL;

    if (graph->constraint_count == 0)
        return true;

    /* 鍒嗛厤琛ㄨ揪寮忔暟缁?*/
    int alloc_count = (graph->constraint_count < MAX_EQUATIONS) ? graph->constraint_count : MAX_EQUATIONS;
    char **eqs = (char **) lv00_malloc(alloc_count * sizeof(char *));
    if (!eqs)
        return false;

    for (int ci = 0; ci < graph->constraint_count && *eq_count < alloc_count; ci++) {
        Constraint *c = graph->constraints[ci];
        if (!c)
            continue;

        /* 妫€鏌?var_id 鏄惁鍦?participants 涓?*/
        bool involves_var = false;
        for (int pi = 0; pi < c->participant_count; pi++) {
            if (c->participants[pi] == var_id) {
                involves_var = true;
                break;
            }
        }
        if (!involves_var)
            continue;

        /* 鏋勯€犺〃杈惧紡鎻忚堪瀛楃涓?*/
        /* 鏍煎紡锛?constraint_N: type=X, vars=[a,b,c]" */
        const char *type_str = "UNKNOWN";
        switch (c->type) {
            case INCIDENCE:
                type_str = "INCIDENCE";
                break;
            case BETWEENNESS:
                type_str = "BETWEENNESS";
                break;
            case INTERSECTION:
                type_str = "INTERSECTION";
                break;
            case CONTAINMENT:
                type_str = "CONTAINMENT";
                break;
            case CONNECTION:
                type_str = "CONNECTION";
                break;
            default:
                break;
        }

        char buf[EXPR_BUFFER_INITIAL];
        int off = snprintf(buf, sizeof(buf), "constraint_%d: type=%s, vars=[", c->id, type_str);
        for (int pi = 0; pi < c->participant_count && off < (int) sizeof(buf) - 20; pi++) {
            off += snprintf(buf + off, sizeof(buf) - off, "%s%d", (pi > 0) ? "," : "", c->participants[pi]);
        }
        snprintf(buf + off, sizeof(buf) - off, "]");

        eqs[*eq_count] = lv00_strdup(buf);
        (*eq_count)++;
    }

    *equations = eqs;
    return true;
}

bool fptaylor_evaluate_graph(const ConstraintGraph *graph, int var_id, const FPTaylorConfig *cfg, ErrorBound *out) {
    if (!graph || !out)
        return false;

    /* 楠岃瘉 var_id 鏄惁鏈夋晥 */
    GeomNode *target_node = graph_get_node(graph, var_id);
    if (!target_node)
        return false;

    /* 姝ラ 1: 浠庣害鏉熷浘涓彁鍙栨秹鍙?var_id 鐨勬柟绋?*/
    char **equations = NULL;
    int eq_count = 0;
    if (!extract_equations(graph, var_id, &equations, &eq_count)) {
        return false;
    }

    /* 姝ラ 2: 鏋勯€犲彉閲忓尯闂磋竟鐣?     * 浠庣洰鏍囪妭鐐圭殑鍧愭爣涓彁鍙栬竟鐣岋紙鑻ヤ负绗﹀彿鍧愭爣锛岃浆涓哄尯闂达級 */
    FloatInterval var_bounds[2];
    int var_count = 0;

    if (target_node->symbolic_coords && target_node->coord_count > 0) {
        int coord_count = target_node->coord_count;
        if (coord_count > 2)
            coord_count = 2;

        for (int d = 0; d < coord_count; d++) {
            double val = symbolic_coord_to_double(target_node->symbolic_coords[d]);
            double eps = fabs(val) * DBL_EPSILON * 10.0; /* 10 ulp 瀹瑰繊 */
            if (eps < DBL_MIN)
                eps = DBL_EPSILON;
            var_bounds[d] = interval_make(val - eps, val + eps, false);
        }
        var_count = coord_count;
    }

    /* 姝ラ 3: 瀵规瘡涓害鏉熸柟绋嬭繘琛屾嘲鍕掑睍寮€鍜屽尯闂磋瘎浼?*/
    double max_abs_err = 0.0;
    double max_rel_err = 0.0;

    for (int ei = 0; ei < eq_count; ei++) {
        TaylorForm tf;
        memset(&tf, 0, sizeof(TaylorForm));

        if (var_count > 0 && basic_taylor_expand(equations[ei], var_bounds, var_count, &tf)) {
            /* 璁＄畻缁濆璇樊 = (interval_hi - interval_lo) / 2 */
            double half_width = (tf.interval_hi - tf.interval_lo) / 2.0;
            if (half_width > max_abs_err) {
                max_abs_err = half_width;
            }
            /* 璁＄畻鐩稿璇樊锛堥伩鍏嶉櫎闆讹級 */
            double abs_center = fabs(tf.center_val);
            if (abs_center > DBL_MIN) {
                double rel = half_width / abs_center;
                if (rel > max_rel_err)
                    max_rel_err = rel;
            }

            free(tf.first_derivs);
            free(tf.deriv_var_ids);
        }
    }

    /* 姝ラ 4: 鏋勯€犺瘉鏄庢枃鏈?*/
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

    /* 姝ラ 5: 娓呯悊骞惰緭鍑?*/
    out->absolute_error = max_abs_err;
    out->relative_error = max_rel_err;
    out->trust_level = (max_abs_err > 0.0) ? TRUST_BLUE : TRUST_GREEN;
    out->proof_text = lv00_strdup(proof_buf);

    for (int ei = 0; ei < eq_count; ei++) {
        free(equations[ei]);
    }
    free(equations);

    return true;
}

/* ========================================================================
 * fptaylor_evaluate_expr 瀹炵幇
 * ======================================================================== */

bool fptaylor_evaluate_expr(const char *expr, const FloatInterval *var_bounds, int var_count, const FPTaylorConfig *cfg,
                            ErrorBound *out) {
    if (!expr || !var_bounds || var_count <= 0 || !out)
        return false;

    FPTaylorConfig config = cfg ? *cfg : fptaylor_config_default();

    /* 涓€闃舵嘲鍕掑睍寮€ */
    TaylorForm tf;
    memset(&tf, 0, sizeof(TaylorForm));

    if (!basic_taylor_expand(expr, var_bounds, var_count, &tf)) {
        return false;
    }

    /* 璁＄畻璇樊鐣?*/
    double half_width = (tf.interval_hi - tf.interval_lo) / 2.0;
    double abs_center = fabs(tf.center_val);

    out->absolute_error = half_width;
    out->relative_error = (abs_center > DBL_MIN) ? half_width / abs_center : half_width;
    out->trust_level = TRUST_BLUE;

    /* 鏋勯€犺瘉鏄庢枃鏈?*/
    char proof_buf[512];
    snprintf(proof_buf, sizeof(proof_buf),
             "expr=\"%s\", order=%d, center=%.6e, interval=[%.6e, %.6e], "
             "abs_err=%.6e, rel_err=%.6e",
             expr, config.taylor_order, tf.center_val, tf.interval_lo, tf.interval_hi, out->absolute_error,
             out->relative_error);
    out->proof_text = lv00_strdup(proof_buf);

    free(tf.first_derivs);
    free(tf.deriv_var_ids);

    return true;
}

/* ========================================================================
 * fptaylor_verify_safety 瀹炵幇
 * ======================================================================== */

TrustColor fptaylor_verify_safety(const ErrorBound *bound, double tolerance) {
    if (!bound)
        return TRUST_RED;

    double abs_err = bound->absolute_error;

    /* NaN 鎴栭潪姝ｅ父鍊?*/
    if (isnan(abs_err) || isinf(abs_err)) {
        return TRUST_RED;
    }

    /* 鎸夐槇鍊煎垎绾х殑淇′换棰滆壊鍒ゆ柇 */
    if (abs_err <= 1e-12) {
        /* 鏋佸害绮剧‘ 鈥斺€?瀹屽叏鏋勯€犳€у畨鍏?*/
        return TRUST_GREEN;
    }

    if (abs_err <= 1e-10) {
        /* 楂樼簿搴?鈥斺€?鍙俊浣嗕粛闇€鍏虫敞 */
        return TRUST_BLUE;
    }

    if (abs_err <= tolerance) {
        /* 杈圭晫瀹夊叏 鈥斺€?鍚暟鍊煎亣璁撅紝鏍囪涓?AMBER */
        return TRUST_AMBER;
    }

    if (abs_err <= tolerance * 10.0) {
        /* 鎺ヨ繎瀹瑰繊杈圭晫 鈥斺€?鏉′欢鎬у畨鍏?*/
        return TRUST_YELLOW;
    }

    /* 瓒呭嚭瀹瑰繊鑼冨洿 鈥斺€?涓嶅畨鍏?*/
    return TRUST_RED;
}

/* ========================================================================
 * 宸ュ巶涓庤祫婧愮鐞? * ======================================================================== */

FPTaylorConfig fptaylor_config_default(void) {
    FPTaylorConfig cfg;
    cfg.use_optimization = true;
    cfg.taylor_order = 1;
    cfg.use_z3_opt = false;
    cfg.use_gelpia = false;
    cfg.branch_bound_threshold = 1e-6;
    return cfg;
}

void error_bound_free(ErrorBound *bound) {
    if (!bound)
        return;
    if (bound->proof_text) {
        free(bound->proof_text);
        bound->proof_text = NULL;
    }
}
