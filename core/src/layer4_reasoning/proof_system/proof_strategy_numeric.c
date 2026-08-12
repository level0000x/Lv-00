/**
 * @file proof_strategy_numeric.c
 * @brief 数值验证策略 —— 区间算术求值 + FPTaylor 误差界分级验证浮点数值命题
 *
 * 将上一轮铺垫的两项能力接入策略注册表：
 *   1. 区间算术公共库（lv/interval_arith.h, lv_interval_* 全套，float_error 语义为基准）
 *   2. FPTaylor 误差验证（lv/float_error.h, fptaylor_verify_safety 输出 TrustColor）
 *
 * 策略逻辑：
 *   - 适用性判定（numeric_verification_applicability_check）：
 *     命题文本（name/label/description）包含比较谓词（= != < > <= >=），
 *     且谓词两侧为「仅含实数常量」的表达式（无变量）→ 适用；否则 false。
 *   - 命题→数值表达式转换（numeric_verify_extract_claim）：
 *     从命题文本中截取 lhs <op> rhs，两侧必须通过字符级常量表达式预检。
 *   - 执行（execute_numeric_verification）：
 *     1) 用 lv_interval_* 对 lhs、rhs 做保守区间求值（递归下降解析常量表达式），
 *        差值区间 diff = lhs - rhs 作为判定依据；
 *     2) 区间半宽作为绝对误差界构造 ErrorBound，调用 fptaylor_verify_safety
 *        得到 TrustColor 信任颜色；
 *     3) 区间成立 + 信任颜色为 GREEN/BLUE/AMBER → 证明成功
 *        （YELLOW/RED → 失败，经 navigator 步骤 note 记录原因）。
 *   - 搜索算法：DFS（数值验证是单步计算，无需搜索），见 default_strategy_table。
 *
 * 最小可行版限制（2026-08-06）：
 *   - Proposition 没有结构化数值表达式字段（只有 pattern 几何约束图 + 文本元数据），
 *     因此仅支持文本形式、且两侧为常量表达式的数值命题；含变量（如 "x1 > 3.14"）
 *     的命题在缺少变量区间约束时无法求值，applicability_check 返回 false。
 *   - 扩展方向（后续迭代）：从约束图中 ANGLE 等约束的 numeric_value 提取数值命题，
 *     或对含变量命题绑定变量区间后调用 fptaylor_evaluate_expr（var_count>0）。
 *
 * @version v3.7.0
 */

#include "proof_multi_strategy_internal.h"

#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/config.h" /* NUMERIC_VERIFY_TOLERANCE 语义别名 = lv_GEO_DISTANCE_EPSILON */
#include "lv/float_error.h"
#include "lv/interval_arith.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_utils.h"
#include "lv/proof.h"
#include "lv/trust_color.h"

/* ============== 常量 ============== */

#define NUMERIC_CLAIM_MAX_LEN 128    /**< 命题两侧表达式最大长度 */
#define NUMERIC_COMPARATOR_MAX_LEN 4 /**< 比较谓词字符串容量（含 NUL） */

/**
 * @brief fptaylor_verify_safety 的安全容差阈值
 *
 * 对应 fptaylor_verify_safety 的分级：GREEN<=1e-12、BLUE<=1e-10、
 * AMBER<=tolerance、YELLOW<=10*tolerance、RED 其余。
 * 数值验证的证明门槛：误差界（区间半宽）不超过本容差时视为可接受。
 */
#define NUMERIC_VERIFY_TOLERANCE lv_GEO_DISTANCE_EPSILON /* 语义别名 = config.h lv_GEO_DISTANCE_EPSILON（1e-8） */

/* ============== 数值命题提取（命题 -> lhs <op> rhs） ============== */

typedef struct {
    char lhs[NUMERIC_CLAIM_MAX_LEN];   /**< 比较谓词左侧常量表达式 */
    char rhs[NUMERIC_CLAIM_MAX_LEN];   /**< 比较谓词右侧常量表达式 */
    char comparator[NUMERIC_COMPARATOR_MAX_LEN]; /**< 比较谓词：= != < <= > >= */
} NumericClaim;

/**
 * @brief 判断字符串是否为已支持的函数名
 */
static bool numeric_verify_is_function_name(const char *s, size_t len) {
    static const char *const kFuncs[] = {"sqrt", "sin", "cos", "exp", "log", "abs",
                                         "tan",  "atan", "asin", "acos", "floor", "ceil"};
    for (size_t k = 0; k < sizeof(kFuncs) / sizeof(kFuncs[0]); k++) {
        if (strlen(kFuncs[k]) == len && strncmp(s, kFuncs[k], len) == 0)
            return true;
    }
    return false;
}

/**
 * @brief 字符级常量表达式预检（applicability 用，快速过滤）
 *
 * 规则：
 *   - 只允许 数字、小数点、+ - * / ^、括号、空白、科学计数法 e/E；
 *   - 字母只能构成已支持的函数名且必须后跟 '('（"e"/"E" 仅允许作为指数符号）；
 *   - 括号必须配对；至少含一个数字。
 * 完整语法校验由 numeric_verify_eval_const 在 execute 阶段完成。
 */
static bool numeric_verify_is_const_expr(const char *s) {
    size_t len = strlen(s);
    if (len == 0)
        return false;

    bool has_digit = false;
    int depth = 0;
    size_t i = 0;

    while (i < len) {
        char c = s[i];
        if (isdigit((unsigned char) c)) {
            has_digit = true;
            i++;
        } else if (c == '(') {
            depth++;
            i++;
        } else if (c == ')') {
            depth--;
            if (depth < 0)
                return false;
            i++;
        } else if (c == '.' || c == '+' || c == '-' || c == '*' || c == '/' || c == '^') {
            i++;
        } else if (isspace((unsigned char) c)) {
            i++;
        } else if (isalpha((unsigned char) c)) {
            size_t start = i;
            while (i < len && isalpha((unsigned char) s[i]))
                i++;
            size_t run_len = i - start;

            /* 科学计数法指数：e/E 必须前有数字/小数点、后有符号或数字 */
            if (run_len == 1 && (s[start] == 'e' || s[start] == 'E')) {
                bool prev_ok = (start > 0 && (isdigit((unsigned char) s[start - 1]) || s[start - 1] == '.'));
                bool next_ok = (i < len && (isdigit((unsigned char) s[i]) || s[i] == '+' || s[i] == '-'));
                if (!prev_ok || !next_ok)
                    return false;
                if (i < len && (s[i] == '+' || s[i] == '-'))
                    i++;
                continue;
            }

            /* 函数名：必须后跟 '(' */
            if (!numeric_verify_is_function_name(s + start, run_len))
                return false;
            size_t j = i;
            while (j < len && isspace((unsigned char) s[j]))
                j++;
            if (j >= len || s[j] != '(')
                return false;
            i = j;
        } else {
            return false;
        }
    }

    return has_digit && depth == 0;
}

/**
 * @brief 从命题文本提取数值命题 lhs <op> rhs（命题 -> 数值表达式转换）
 *
 * 输入来源：prop->name / prop->label / prop->description 拼接文本。
 * 规则（保守，最小可行版）：
 *   - 找到首个比较谓词（双字符 >= <= != == 优先于单字符 = < >，"==" 规范化为 "="）；
 *   - lhs = 谓词前的文本（去尾空白），必须完整是一个常量表达式；
 *   - rhs = 谓词后的首个空白前片段（去首空白），必须是一个常量表达式；
 *   - 任一侧含变量/非法字符则判定无法提取，返回 false。
 *
 * @return true 提取成功，false 命题不含可数值验证的常量比较
 */
static bool numeric_verify_extract_claim(const Proposition *prop, NumericClaim *out) {
    if (!prop || !out)
        return false;

    /* 拼接 name/label/description（截断到 1KB） */
    char text[1024];
    size_t used = 0;
    const char *parts[3] = {prop->name, prop->label, prop->description};
    for (int i = 0; i < 3; i++) {
        if (!parts[i])
            continue;
        size_t n = strlen(parts[i]);
        if (used + n + 2 >= sizeof(text))
            n = sizeof(text) - used - 3;
        if (n == 0)
            continue;
        memcpy(text + used, parts[i], n);
        used += n;
        text[used++] = ' ';
    }
    text[used] = '\0';
    if (used == 0)
        return false;

    /* 查找首个比较谓词（双字符运算符优先匹配） */
    static const char *const kOps[] = {">=", "<=", "!=", "==", "=", "<", ">"};
    const char *found = NULL;
    int found_len = 0;
    for (size_t k = 0; k < sizeof(kOps) / sizeof(kOps[0]); k++) {
        const char *p = strstr(text, kOps[k]);
        if (p && (!found || p < found)) {
            found = p;
            found_len = (int) strlen(kOps[k]);
        }
    }
    if (!found)
        return false;

    /* lhs：谓词之前（去尾空白） */
    size_t lhs_len = (size_t) (found - text);
    while (lhs_len > 0 && isspace((unsigned char) text[lhs_len - 1]))
        lhs_len--;
    if (lhs_len == 0 || lhs_len >= NUMERIC_CLAIM_MAX_LEN)
        return false;
    lv_strlcpy_n(out->lhs, NUMERIC_CLAIM_MAX_LEN, text, lhs_len);

    /* rhs：谓词之后到首个空白（去首空白，统一 lv_str_ltrim） */
    const char *rhs_start = lv_str_ltrim(found + found_len);
    const char *rhs_end = rhs_start;
    while (*rhs_end && !isspace((unsigned char) *rhs_end))
        rhs_end++;
    size_t rhs_len = (size_t) (rhs_end - rhs_start);
    if (rhs_len == 0 || rhs_len >= NUMERIC_CLAIM_MAX_LEN)
        return false;
    lv_strlcpy_n(out->rhs, NUMERIC_CLAIM_MAX_LEN, rhs_start, rhs_len);

    /* 比较谓词规范化："==" -> "=" */
    if (found_len == 2 && found[0] == '=' && found[1] == '=') {
        out->comparator[0] = '=';
        out->comparator[1] = '\0';
    } else {
        lv_strlcpy_n(out->comparator, NUMERIC_COMPARATOR_MAX_LEN, found, (size_t) found_len);
    }

    /* 两侧必须是仅含实数常量的表达式（含变量/散文文本 -> 不适用） */
    if (!numeric_verify_is_const_expr(out->lhs) || !numeric_verify_is_const_expr(out->rhs))
        return false;

    return true;
}

/* ============== 常量表达式区间求值（基于 lv_interval_*） ============== */

typedef struct {
    const char *s;
    size_t pos;
    bool error;
} ConstExprParser;

static void numeric_ce_skip_ws(ConstExprParser *p) {
    while (p->s[p->pos] && isspace((unsigned char) p->s[p->pos]))
        p->pos++;
}

static lvInterval numeric_ce_parse_expr(ConstExprParser *p);
static lvInterval numeric_ce_parse_term(ConstExprParser *p);
static lvInterval numeric_ce_parse_factor(ConstExprParser *p);
static lvInterval numeric_ce_parse_primary(ConstExprParser *p);

static lvInterval numeric_ce_apply_function(const char *name, size_t len, lvInterval arg, ConstExprParser *p) {
    if (len == 4 && strncmp(name, "sqrt", 4) == 0)
        return lv_interval_sqrt(arg);
    if (len == 3 && strncmp(name, "sin", 3) == 0)
        return lv_interval_sin(arg);
    if (len == 3 && strncmp(name, "cos", 3) == 0)
        return lv_interval_cos(arg);
    if (len == 3 && strncmp(name, "exp", 3) == 0)
        return lv_interval_exp(arg);
    if (len == 3 && strncmp(name, "log", 3) == 0)
        return lv_interval_log(arg);
    if (len == 3 && strncmp(name, "abs", 3) == 0)
        return lv_interval_abs(arg);
    if (len == 3 && strncmp(name, "tan", 3) == 0)
        return lv_interval_tan(arg);
    if (len == 4 && strncmp(name, "atan", 4) == 0)
        return lv_interval_atan(arg);
    if (len == 4 && strncmp(name, "asin", 4) == 0)
        return lv_interval_asin(arg);
    if (len == 4 && strncmp(name, "acos", 4) == 0)
        return lv_interval_acos(arg);
    if (len == 5 && strncmp(name, "floor", 5) == 0)
        return lv_interval_floor(arg);
    if (len == 4 && strncmp(name, "ceil", 4) == 0)
        return lv_interval_ceil(arg);
    p->error = true;
    return lv_interval_make(0.0, 0.0, 0);
}

/**
 * @brief 解析数字常量（strtod 支持 ".5"、"1e-5"、"3." 等写法）
 */
static lvInterval numeric_ce_parse_number(ConstExprParser *p) {
    char *end = NULL;
    double val = strtod(p->s + p->pos, &end);
    if (end == p->s + p->pos) {
        p->error = true;
        return lv_interval_make(0.0, 0.0, 0);
    }
    p->pos = (size_t) (end - p->s);
    return lv_interval_make(val, val, 1);
}

static lvInterval numeric_ce_parse_primary(ConstExprParser *p) {
    numeric_ce_skip_ws(p);
    if (p->error)
        return lv_interval_make(0.0, 0.0, 0);

    char c = p->s[p->pos];
    if (c == '(') {
        p->pos++;
        lvInterval v = numeric_ce_parse_expr(p);
        numeric_ce_skip_ws(p);
        if (p->s[p->pos] != ')') {
            p->error = true;
        } else {
            p->pos++;
        }
        return v;
    }
    if (isdigit((unsigned char) c) || c == '.') {
        return numeric_ce_parse_number(p);
    }
    if (isalpha((unsigned char) c)) {
        /* 函数调用：name '(' expr ')' */
        size_t start = p->pos;
        while (p->s[p->pos] && isalpha((unsigned char) p->s[p->pos]))
            p->pos++;
        size_t run_len = p->pos - start;
        numeric_ce_skip_ws(p);
        if (p->s[p->pos] != '(') {
            p->error = true;
            return lv_interval_make(0.0, 0.0, 0);
        }
        p->pos++;
        lvInterval arg = numeric_ce_parse_expr(p);
        numeric_ce_skip_ws(p);
        if (p->s[p->pos] != ')') {
            p->error = true;
        } else {
            p->pos++;
        }
        return numeric_ce_apply_function(p->s + start, run_len, arg, p);
    }

    p->error = true;
    return lv_interval_make(0.0, 0.0, 0);
}

/**
 * @brief factor := ('+' | '-') factor | primary ('^' factor)？—— 幂右结合
 */
static lvInterval numeric_ce_parse_factor(ConstExprParser *p) {
    numeric_ce_skip_ws(p);
    char c = p->s[p->pos];
    if (c == '+' || c == '-') {
        p->pos++;
        lvInterval v = numeric_ce_parse_factor(p);
        return (c == '-') ? lv_interval_neg(v) : v;
    }
    lvInterval base = numeric_ce_parse_primary(p);
    numeric_ce_skip_ws(p);
    if (p->s[p->pos] == '^') {
        p->pos++;
        lvInterval exp_iv = numeric_ce_parse_factor(p);
        return lv_interval_pow(base, exp_iv);
    }
    return base;
}

/**
 * @brief term := factor (('*' | '/') factor)*
 */
static lvInterval numeric_ce_parse_term(ConstExprParser *p) {
    lvInterval left = numeric_ce_parse_factor(p);
    for (;;) {
        numeric_ce_skip_ws(p);
        char c = p->s[p->pos];
        if (c == '*' || c == '/') {
            p->pos++;
            lvInterval right = numeric_ce_parse_factor(p);
            left = (c == '*') ? lv_interval_mul(left, right) : lv_interval_div(left, right);
        } else {
            break;
        }
    }
    return left;
}

/**
 * @brief expr := term (('+' | '-') term)*
 */
static lvInterval numeric_ce_parse_expr(ConstExprParser *p) {
    lvInterval left = numeric_ce_parse_term(p);
    for (;;) {
        numeric_ce_skip_ws(p);
        char c = p->s[p->pos];
        if (c == '+' || c == '-') {
            p->pos++;
            lvInterval right = numeric_ce_parse_term(p);
            left = (c == '+') ? lv_interval_add(left, right) : lv_interval_sub(left, right);
        } else {
            break;
        }
    }
    return left;
}

/**
 * @brief 对常量表达式做保守区间求值（基于 lv_interval_*，端点向外取整）
 *
 * @return true 解析成功，false 语法不支持（调用方按失败处理）
 */
static bool numeric_verify_eval_const(const char *expr, lvInterval *out) {
    ConstExprParser p;
    p.s = expr;
    p.pos = 0;
    p.error = false;

    lvInterval v = numeric_ce_parse_expr(&p);
    numeric_ce_skip_ws(&p);
    if (p.error || p.s[p.pos] != '\0')
        return false;

    if (out)
        *out = v;
    return true;
}

/* ============== 策略入口 ============== */

/**
 * @brief 数值验证适用性检查
 *
 * 保守判定：命题文本（name/label/description）包含比较谓词，且谓词两侧为
 * 仅含实数常量的表达式时可数值验证；含变量/纯几何命题/无法提取 -> false。
 */
bool numeric_verification_applicability_check(const ProofMultiStrategy *mse, const ConstraintGraph *graph,
                                              const Proposition *prop) {
    (void) mse;
    (void) graph;
    if (!prop)
        return false;
    NumericClaim claim;
    return numeric_verify_extract_claim(prop, &claim);
}

/**
 * @brief 数值验证策略执行
 *
 * 流程：
 *   1. 从 nav->target_prop 提取数值命题 lhs <op> rhs（无法提取 -> 不适用，返回 false）；
 *   2. 用 lv_interval_* 对两侧做保守区间求值，得差值区间 diff = lhs - rhs；
 *   3. 区间判定成立性 + 区间半宽作为绝对误差界构造 ErrorBound；
 *   4. fptaylor_verify_safety 输出 TrustColor；
 *   5. 区间成立且信任颜色为 GREEN/BLUE/AMBER -> 成功，否则失败（note 记录原因）。
 */
bool execute_numeric_verification(ProofMultiStrategy *mse, ProofNavigator *nav) {
    (void) mse;
    if (!nav || !nav->target_prop)
        return false;

    NumericClaim claim;
    if (!numeric_verify_extract_claim(nav->target_prop, &claim))
        return false; /* 非数值命题（或无法提取）：策略不适用，直接失败 */

    lvInterval lhs_iv, rhs_iv;
    if (!numeric_verify_eval_const(claim.lhs, &lhs_iv) || !numeric_verify_eval_const(claim.rhs, &rhs_iv)) {
        /* 字符级预检通过但语法解析失败（理论上罕见），记录失败步骤 */
        ProofStep *step = proof_step_create(PROOF_STEP_REWRITE);
        if (step) {
            step->color = PROOF_COLOR_YELLOW;
            char buf[256];
            snprintf(buf, sizeof(buf), "[数值验证] 表达式语法不支持，无法求值: %s %s %s", claim.lhs, claim.comparator,
                     claim.rhs);
            step->note = lv_strdup_safe(buf);
            proof_navigator_add_step(nav, step);
        }
        return false;
    }

    /* 差值区间：判定依据 */
    lvInterval diff = lv_interval_sub(lhs_iv, rhs_iv);

    /* 区间判定：diff = lhs - rhs，按比较谓词判成立 */
    bool holds = false;
    const char *rel = NULL;
    if (strcmp(claim.comparator, "<") == 0) {
        holds = diff.hi < 0.0;
        rel = "<";
    } else if (strcmp(claim.comparator, "<=") == 0) {
        holds = diff.hi <= 0.0;
        rel = "<=";
    } else if (strcmp(claim.comparator, ">") == 0) {
        holds = diff.lo > 0.0;
        rel = ">";
    } else if (strcmp(claim.comparator, ">=") == 0) {
        holds = diff.lo >= 0.0;
        rel = ">=";
    } else if (strcmp(claim.comparator, "!=") == 0) {
        holds = (diff.lo > 0.0 || diff.hi < 0.0);
        rel = "!=";
    } else { /* "="：零在容差扩展区间内即视为容差内成立（同 interval_verify_solution 语义） */
        holds = (diff.lo <= NUMERIC_VERIFY_TOLERANCE && diff.hi >= -NUMERIC_VERIFY_TOLERANCE);
        rel = "=";
    }

    /* 误差界：区间半宽（与 float_error.c fptaylor_evaluate_expr 的 half_width 同款语义） */
    double half_width = (diff.hi - diff.lo) / 2.0;
    double abs_center = fabs(diff.lo + diff.hi) * 0.5;

    ErrorBound bound;
    memset(&bound, 0, sizeof(bound));
    bound.absolute_error = half_width;
    bound.relative_error = (abs_center > DBL_MIN) ? half_width / abs_center : half_width;
    char proof_buf[512];
    snprintf(proof_buf, sizeof(proof_buf),
             "numeric verification: %s %s %s; lhs=[%.6e, %.6e], rhs=[%.6e, %.6e], diff=[%.6e, %.6e], "
             "abs_err=%.6e, rel_err=%.6e",
             claim.lhs, rel, claim.rhs, lhs_iv.lo, lhs_iv.hi, rhs_iv.lo, rhs_iv.hi, diff.lo, diff.hi,
             bound.absolute_error, bound.relative_error);
    bound.proof_text = lv_strdup_safe(proof_buf);

    /* FPTaylor 信任颜色分级：GREEN<=1e-12, BLUE<=1e-10, AMBER<=tol, YELLOW<=10*tol, RED 其余 */
    TrustColor tc = fptaylor_verify_safety(&bound, NUMERIC_VERIFY_TOLERANCE);

    /* 成功 = 区间判定成立 + 误差界达到 GREEN/BLUE/AMBER（YELLOW/RED 视为无法确认/不安全） */
    bool success = holds && (tc == TRUST_GREEN || tc == TRUST_BLUE_UNEXPLORED || tc == TRUST_AMBER);
    ProofColor step_color =
        success ? (tc == TRUST_GREEN ? PROOF_COLOR_GREEN_COMPLETE : trust_color_to_proof(tc))
                : (holds ? PROOF_COLOR_YELLOW : PROOF_COLOR_RED_CONFLICT);

    /* 记录证明步骤（note 含区间与信任颜色详情；失败时记录原因） */
    ProofStep *step = proof_step_create(PROOF_STEP_REWRITE);
    if (step) {
        step->color = step_color;
        char buf[384];
        snprintf(buf, sizeof(buf),
                 "[数值验证] %s %s %s => %s（区间 [%.6e, %.6e]，TrustColor=%s，abs_err=%.3e，容差=%.0e）", claim.lhs, rel,
                 claim.rhs, success ? "成立" : (holds ? "区间成立但误差界超容差，无法确认" : "不成立"), diff.lo, diff.hi,
                 trust_color_name(tc), half_width, NUMERIC_VERIFY_TOLERANCE);
        step->note = lv_strdup_safe(buf);
        proof_navigator_add_step(nav, step);
    }

    if (success) {
        proof_navigator_set_strategy_note(nav, "数值验证：区间算术求值 + FPTaylor 误差界分级验证浮点数值命题");
    }

    error_bound_destroy(&bound);
    return success;
}
