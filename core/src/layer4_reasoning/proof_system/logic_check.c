/**
 * @file logic_check.c
 * @brief 逻辑检查模块（子目录版本）
 *
 * 提供命题逻辑的基本检查功能：重言式检测、矛盾式检测和等价性检测。
 * 使用真值表枚举法对输入公式进行穷举验证。
 *
 * 支持的逻辑连接词：AND(&), OR(|), NOT(!), IMPLIES(->), EQUIV(<->)
 */

#include "lv/logic_check.h"

#include <ctype.h>
#include <string.h>

/* ================================================================
 *  内部常量
 * ================================================================ */

#define LOGIC_MAX_VARS      8      /**< 最大变量数（真值表枚举 2^8 = 256 行） */
#define LOGIC_MAX_FORMULA  256     /**< 公式最大长度 */
#define LOGIC_MAX_STACK    64      /**< 求值栈最大深度 */

/* ================================================================
 *  内部数据结构
 * ================================================================ */

/**
 * @brief 变量表条目
 */
typedef struct {
    char name;          /**< 变量名（单字符 A-Z） */
    int  index;         /**< 变量索引 */
} LogicVarEntry;

/**
 * @brief 逻辑检查上下文
 */
typedef struct {
    LogicVarEntry vars[LOGIC_MAX_VARS]; /**< 变量表 */
    int           var_count;            /**< 变量数量 */
    char          formula[LOGIC_MAX_FORMULA]; /**< 公式副本 */
} LogicCheckCtx;

/* ================================================================
 *  内部辅助函数
 * ================================================================ */

/**
 * @brief 从公式中提取所有唯一变量
 *
 * 扫描公式字符串，提取所有大写字母作为命题变量。
 */
static int logic_extract_vars(const char *formula, LogicVarEntry *vars, int max_vars)
{
    int count = 0;
    const char *p = formula;
    int seen[26] = {0};

    while (*p) {
        if (isalpha((unsigned char)*p) && isupper((unsigned char)*p)) {
            int idx = *p - 'A';
            if (!seen[idx] && count < max_vars) {
                vars[count].name = *p;
                vars[count].index = count;
                seen[idx] = 1;
                count++;
            }
        }
        p++;
    }
    return count;
}

/**
 * @brief 简单命题公式求值器
 *
 * 使用递归下降解析器求值命题公式。
 * 支持: 变量, !var, (expr), expr & expr, expr | expr, expr -> expr
 *
 * @param formula  公式字符串
 * @param vars     变量表
 * @param values   变量赋值数组（values[i] = 0 或 1）
 * @param pos      当前解析位置（输入/输出）
 * @return 求值结果 (0 或 1)，-1 表示错误
 */
static int logic_eval_expr(const char *formula, const LogicVarEntry *vars,
                            const int *values, int var_count, int *pos);

/**
 * @brief 解析原子（变量、否定、括号表达式）
 */
static int logic_parse_atom(const char *formula, const LogicVarEntry *vars,
                             const int *values, int var_count, int *pos)
{
    int result;

    /* 跳过空白 */
    while (formula[*pos] == ' ') (*pos)++;

    /* 否定 */
    if (formula[*pos] == '!') {
        (*pos)++;
        result = logic_parse_atom(formula, vars, values, var_count, pos);
        return result < 0 ? result : !result;
    }

    /* 括号表达式 */
    if (formula[*pos] == '(') {
        (*pos)++;
        result = logic_eval_expr(formula, vars, values, var_count, pos);
        if (formula[*pos] == ')') (*pos)++;
        return result;
    }

    /* 变量 */
    if (isalpha((unsigned char)formula[*pos]) && isupper((unsigned char)formula[*pos])) {
        char c = formula[*pos];
        int i;
        (*pos)++;
        for (i = 0; i < var_count; i++) {
            if (vars[i].name == c) return values[i];
        }
        return -1;  /* 未知变量 */
    }

    return -1;  /* 语法错误 */
}

/**
 * @brief 解析蕴涵 (最低优先级)
 */
static int logic_eval_expr(const char *formula, const LogicVarEntry *vars,
                            const int *values, int var_count, int *pos)
{
    int left, right;

    /* 先解析左侧（或表达式） */
    left = logic_parse_atom(formula, vars, values, var_count, pos);
    if (left < 0) return left;

    while (formula[*pos] == ' ') (*pos)++;

    /* 检查二元运算符 */
    if (formula[*pos] == '&' && formula[*pos + 1] != '>') {
        (*pos)++;
        right = logic_parse_atom(formula, vars, values, var_count, pos);
        return (left && right);
    }
    if (formula[*pos] == '|') {
        (*pos)++;
        right = logic_parse_atom(formula, vars, values, var_count, pos);
        return (left || right);
    }
    if (formula[*pos] == '-' && formula[*pos + 1] == '>') {
        (*pos) += 2;
        right = logic_eval_expr(formula, vars, values, var_count, pos);
        return (!left || right);
    }

    return left;
}

/**
 * @brief 对所有变量赋值组合求值公式
 *
 * @return 1 所有组合为真（重言式），0 存在假值，-1 错误
 */
static int logic_check_all_combinations(const char *formula)
{
    LogicCheckCtx ctx;
    int combinations, mask;
    int values[LOGIC_MAX_VARS];
    int all_true = 1;
    int all_false = 1;
    int i, pos;

    memset(&ctx, 0, sizeof(ctx));
    ctx.var_count = logic_extract_vars(formula, ctx.vars, LOGIC_MAX_VARS);
    if (ctx.var_count == 0) return -1;

    combinations = 1 << ctx.var_count;
    mask = combinations - 1;

    for (i = 0; i < combinations; i++) {
        int j, result;
        for (j = 0; j < ctx.var_count; j++) {
            values[j] = (i >> j) & 1;
        }
        pos = 0;
        result = logic_eval_expr(formula, ctx.vars, values, ctx.var_count, &pos);
        if (result < 0) return -1;
        if (!result) all_true = 0;
        if (result) all_false = 0;
    }

    if (all_true) return 1;   /* 重言式 */
    if (all_false) return -1;  /* 矛盾式 */
    return 0;                  /* 偶然式 */
}

/* ================================================================
 *  公共 API 实现
 * ================================================================ */

/**
 * @brief 检查公式是否为重言式
 *
 * @param formula 命题公式字符串
 * @return 1 重言式，0 非重言式，-1 错误
 */
int lv_logic_check_tautology(const char *formula)
{
    if (!formula || !*formula) return -1;
    return logic_check_all_combinations(formula) == 1 ? 1 : 0;
}

/**
 * @brief 检查公式是否为矛盾式
 *
 * @param formula 命题公式字符串
 * @return 1 矛盾式，0 非矛盾式，-1 错误
 */
int lv_logic_check_contradiction(const char *formula)
{
    if (!formula || !*formula) return -1;
    return logic_check_all_combinations(formula) == -1 ? 1 : 0;
}

/**
 * @brief 检查两个公式是否等价
 *
 * @param a 第一个公式
 * @param b 第二个公式
 * @return 1 等价，0 不等价，-1 错误
 */
int lv_logic_check_equivalence(const char *a, const char *b)
{
    char combined[LOGIC_MAX_FORMULA * 2 + 16];
    LogicVarEntry vars_a[LOGIC_MAX_VARS], vars_b[LOGIC_MAX_VARS];
    int count_a, count_b;
    int combinations;
    int values[LOGIC_MAX_VARS];
    int i, pos_a, pos_b;

    if (!a || !b) return -1;

    count_a = logic_extract_vars(a, vars_a, LOGIC_MAX_VARS);
    count_b = logic_extract_vars(b, vars_b, LOGIC_MAX_VARS);
    if (count_a < 0 || count_b < 0) return -1;

    /* 使用更大的变量集 */
    {
        int total = count_a;
        int j;
        for (j = 0; j < count_b; j++) {
            int found = 0, k;
            for (k = 0; k < total; k++) {
                if (vars_a[k].name == vars_b[j].name) { found = 1; break; }
            }
            if (!found && total < LOGIC_MAX_VARS) {
                vars_a[total].name = vars_b[j].name;
                vars_a[total].index = total;
                total++;
            }
        }
        count_a = total;
    }

    combinations = 1 << count_a;
    for (i = 0; i < combinations; i++) {
        int j, ra, rb;
        for (j = 0; j < count_a; j++) {
            values[j] = (i >> j) & 1;
        }
        pos_a = 0;
        pos_b = 0;
        ra = logic_eval_expr(a, vars_a, values, count_a, &pos_a);
        rb = logic_eval_expr(b, vars_a, values, count_a, &pos_b);
        if (ra < 0 || rb < 0) return -1;
        if (ra != rb) return 0;  /* 不等价 */
    }

    return 1;  /* 等价 */
}
