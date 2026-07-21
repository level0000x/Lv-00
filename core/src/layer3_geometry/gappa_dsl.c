/**
 * @file gappa_dsl.c
 * @brief Gappa DSL parsing and proof generation (stub implementations)
 */

#include "lv00/gappa_dsl.h"
#include "lv00/lv00_utils.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>

/** @brief 可移植的 strtok_r 实现 */
static char *lv00_strtok_r(char *str, const char *delim, char **saveptr) {
    if (!str) str = *saveptr;
    str += strspn(str, delim);
    if (*str == '\0') { *saveptr = str; return NULL; }
    char *end = str + strcspn(str, delim);
    if (*end != '\0') { *end = '\0'; *saveptr = end + 1; }
    else { *saveptr = end; }
    return str;
}

/* ── Original API ── */

/**
 * @brief 解析 Gappa DSL 输入
 *
 * @param input Gappa DSL 输入字符串
 * @return 成功返回 0，失败返回 -1
 */
int lv00_gappa_parse(const char *input) {
    if (!input) return -1;
    Lv00GappaPredicate *hyp = NULL;
    Lv00GappaProofGoal *goals = NULL;
    int hyp_count = 0, goal_count = 0;
    bool ok = gappa_parse(input, &hyp, &hyp_count, &goals, &goal_count);
    gappa_predicates_free(hyp, hyp_count);
    gappa_goals_free(goals, goal_count);
    return ok ? 0 : -1;
}

/**
 * @brief 在上下文中求值 Gappa 表达式
 *
 * @param expr Gappa 表达式
 * @param lo   输出下界
 * @param hi   输出上界
 * @return 成功返回 0，失败返回 -1
 */
int lv00_gappa_eval(const char *expr, double *lo, double *hi) {
    if (!expr || !lo || !hi) return -1;
    return lv00_gappa_propagate(expr, lo, hi);
}

/**
 * @brief 使用 Gappa 证明目标
 *
 * @param script Gappa 证明脚本
 * @return 证明结果字符串（调用者负责释放），失败返回 NULL
 */
char *lv00_gappa_prove(const char *script) {
    if (!script) return NULL;

    Lv00GappaPredicate *hyp = NULL;
    Lv00GappaProofGoal *goals = NULL;
    int hyp_count = 0, goal_count = 0;

    if (!gappa_parse(script, &hyp, &hyp_count, &goals, &goal_count)) {
        return lv00_strdup("proof result: parse error");
    }

    Lv00GappaProofResult result = gappa_prove(hyp, hyp_count, goals, goal_count, NULL);

    char buf[512];
    if (result.goals_total == 0) {
        snprintf(buf, sizeof(buf), "proof result: no goals, %d hypotheses parsed",
                 hyp_count);
    } else if (result.success) {
        snprintf(buf, sizeof(buf), "proof succeeded: all %d/%d goals proven",
                 result.goals_proven, result.goals_total);
    } else {
        snprintf(buf, sizeof(buf), "proof result: %d/%d goals proven, %d failed",
                 result.goals_proven, result.goals_total, result.goals_failed);
    }

    gappa_result_free(&result);
    gappa_predicates_free(hyp, hyp_count);
    gappa_goals_free(goals, goal_count);

    return lv00_strdup(buf);
}

/* ── Structured API ── */

/**
 * @brief 格式化预定义的 Gappa 模板
 *
 * @param name 格式名称（如 "binary32", "binary64" 等；NULL 使用默认格式）
 * @param out  输出格式描述
 * @return true 表示成功识别并填充格式
 */
int gappa_format_predefined(const char *name, Lv00GappaFormat *out) {
    if (!out) return -1;
    memset(out, 0, sizeof(Lv00GappaFormat));
    if (name) {
        if (strcmp(name, "binary32") == 0) {
            out->format_id = 1;
            out->name = "binary32";
            out->precision_bits = 24;
            out->exponent_bits = 8;
        } else if (strcmp(name, "binary64") == 0) {
            out->format_id = 2;
            out->name = "binary64";
            out->precision_bits = 53;
            out->exponent_bits = 11;
        } else if (strcmp(name, "binary16") == 0) {
            out->format_id = 3;
            out->name = "binary16";
            out->precision_bits = 11;
            out->exponent_bits = 5;
        } else if (strcmp(name, "binary128") == 0) {
            out->format_id = 4;
            out->name = "binary128";
            out->precision_bits = 113;
            out->exponent_bits = 15;
        } else {
            return -1;
        }
    } else {
        out->format_id = 0;
        out->name = "default";
        out->precision_bits = 53;
        out->exponent_bits = 11;
    }
    out->rounding = LV00_ROUND_NE;
    return 0;
}

/**
 * @brief 解析 Gappa 表达式，提取假设与目标
 *
 * @param input      Gappa 表达式输入（以 "->" 分隔假设与目标）
 * @param hyp        输出假设谓词数组（调用者负责释放）
 * @param hyp_count  输出假设数量
 * @param goals      输出证明目标数组（调用者负责释放）
 * @param goal_count 输出目标数量
 * @return true 表示解析成功
 */
int gappa_parse(const char *input, Lv00GappaPredicate **hyp, int *hyp_count,
                 Lv00GappaProofGoal **goals, int *goal_count) {
    if (!input) return -1;
    if (hyp) *hyp = NULL;
    if (hyp_count) *hyp_count = 0;
    if (goals) *goals = NULL;
    if (goal_count) *goal_count = 0;

    /* 分割假设与目标：以 "->" 分隔 */
    const char *arrow = strstr(input, "->");
    char hyp_part[1024] = {0};
    char goal_part[1024] = {0};

    if (arrow) {
        size_t hyp_len = (size_t)(arrow - input);
        if (hyp_len >= sizeof(hyp_part)) hyp_len = sizeof(hyp_part) - 1;
        memcpy(hyp_part, input, hyp_len);
        strncpy(goal_part, arrow + 2, sizeof(goal_part) - 1);
    } else {
        strncpy(hyp_part, input, sizeof(hyp_part) - 1);
    }

    /* 解析假设：按 ";" 分割，每条 "var in [lo, hi]" */
    int h_count = 0;
    Lv00GappaPredicate *h_arr = NULL;
    {
        char buf[1024];
        strncpy(buf, hyp_part, sizeof(buf) - 1);
        char *saveptr = NULL;
        char *token = lv00_strtok_r(buf, ";", &saveptr);
        while (token) {
            /* 跳过空白 */
            while (*token && isspace((unsigned char)*token)) token++;
            if (*token) {
                char varname[256] = {0};
                double lo = 0.0, hi = 0.0;
                if (sscanf(token, "%255[a-zA-Z0-9_] in [%lf , %lf]", varname, &lo, &hi) == 3 ||
                    sscanf(token, "%255[a-zA-Z0-9_] in [%lf,%lf]", varname, &lo, &hi) == 3) {
                    Lv00GappaPredicate *tmp = (Lv00GappaPredicate *)lv00_realloc(h_arr, (size_t)(h_count + 1) * sizeof(Lv00GappaPredicate));
                    if (tmp) {
                        h_arr = tmp;
                        memset(&h_arr[h_count], 0, sizeof(Lv00GappaPredicate));
                        h_arr[h_count].type = LV00_PRED_BND;
                        strncpy(h_arr[h_count].expr_lhs, varname, sizeof(h_arr[h_count].expr_lhs) - 1);
                        h_arr[h_count].bound_lo = lo;
                        h_arr[h_count].bound_hi = hi;
                        h_arr[h_count].is_hypothesis = true;
                        h_count++;
                    }
                }
            }
            token = lv00_strtok_r(NULL, ";", &saveptr);
        }
    }

    /* 解析目标：按 ";" 分割，每条 "|expr| <= bound" */
    int g_count = 0;
    Lv00GappaProofGoal *g_arr = NULL;
    if (goal_part[0]) {
        char buf[1024];
        strncpy(buf, goal_part, sizeof(buf) - 1);
        char *saveptr = NULL;
        char *token = lv00_strtok_r(buf, ";", &saveptr);
        while (token) {
            while (*token && isspace((unsigned char)*token)) token++;
            if (*token) {
                /* 尝试解析 "|...| <= bound" */
                char *abs_start = strchr(token, '|');
                char *abs_end = strrchr(token, '|');
                char *leq = strstr(token, "<=");
                if (abs_start && abs_end && abs_end > abs_start && leq) {
                    double bound = atof(leq + 2);
                    /* 提取 | 内的表达式 */
                    size_t expr_len = (size_t)(abs_end - abs_start - 1);
                    char inner_expr[256] = {0};
                    if (expr_len < sizeof(inner_expr)) {
                        memcpy(inner_expr, abs_start + 1, expr_len);
                    }
                    Lv00GappaProofGoal *tmp = (Lv00GappaProofGoal *)lv00_realloc(g_arr, (size_t)(g_count + 1) * sizeof(Lv00GappaProofGoal));
                    if (tmp) {
                        g_arr = tmp;
                        memset(&g_arr[g_count], 0, sizeof(Lv00GappaProofGoal));
                        g_arr[g_count].predicate.type = LV00_PRED_ABS;
                        strncpy(g_arr[g_count].predicate.expr_lhs, inner_expr, sizeof(g_arr[g_count].predicate.expr_lhs) - 1);
                        g_arr[g_count].predicate.bound_abs = bound;
                        g_arr[g_count].predicate.is_hypothesis = false;
                        g_count++;
                    }
                } else {
                    /* 尝试解析 "var in [lo, hi]" 作为 BND 目标 */
                    char varname[256] = {0};
                    double lo = 0.0, hi = 0.0;
                    if (sscanf(token, "%255[a-zA-Z0-9_] in [%lf , %lf]", varname, &lo, &hi) == 3 ||
                        sscanf(token, "%255[a-zA-Z0-9_] in [%lf,%lf]", varname, &lo, &hi) == 3) {
                        Lv00GappaProofGoal *tmp = (Lv00GappaProofGoal *)lv00_realloc(g_arr, (size_t)(g_count + 1) * sizeof(Lv00GappaProofGoal));
                        if (tmp) {
                            g_arr = tmp;
                            memset(&g_arr[g_count], 0, sizeof(Lv00GappaProofGoal));
                            g_arr[g_count].predicate.type = LV00_PRED_BND;
                            strncpy(g_arr[g_count].predicate.expr_lhs, varname, sizeof(g_arr[g_count].predicate.expr_lhs) - 1);
                            g_arr[g_count].predicate.bound_lo = lo;
                            g_arr[g_count].predicate.bound_hi = hi;
                            g_arr[g_count].predicate.is_hypothesis = false;
                            g_count++;
                        }
                    }
                }
            }
            token = lv00_strtok_r(NULL, ";", &saveptr);
        }
    }

    if (hyp) *hyp = h_arr;
    if (hyp_count) *hyp_count = h_count;
    if (goals) *goals = g_arr;
    if (goal_count) *goal_count = g_count;
    return 0;
}

/**
 * @brief 释放谓词数组
 *
 * @param preds 谓词数组
 * @param count 谓词数量（保留参数，未使用）
 */
void gappa_predicates_free(Lv00GappaPredicate *preds, int count) {
    (void)count;
    lv00_free((void **)&(preds));
}

/**
 * @brief 释放目标数组
 *
 * @param goals 目标数组
 * @param count 目标数量（保留参数，未使用）
 */
void gappa_goals_free(Lv00GappaProofGoal *goals, int count) {
    (void)count;
    lv00_free((void **)&(goals));
}

/**
 * @brief 在给定谓词下证明目标
 *
 * @param hyp       假设谓词数组
 * @param hyp_count 假设数量
 * @param goals     证明目标数组
 * @param goal_count 目标数量
 * @param config    配置参数（预留，可为 NULL）
 * @return 证明结果结构体
 */
Lv00GappaProofResult gappa_prove(const Lv00GappaPredicate *hyp, int hyp_count,
                                  const Lv00GappaProofGoal *goals, int goal_count,
                                  const void *config) {
    (void)config;
    Lv00GappaProofResult result;
    memset(&result, 0, sizeof(result));
    result.goals_total = goal_count;

    if (goal_count > 0) {
        result.goals = (Lv00GappaProofGoal *)calloc((size_t)goal_count, sizeof(Lv00GappaProofGoal));
        if (result.goals) {
            for (int i = 0; i < goal_count; i++) {
                result.goals[i] = goals[i];
                Lv00GappaPredicate gpred = goals[i].predicate;
                bool proven = false;

                /* 查找匹配的假设（按变量名匹配） */
                for (int j = 0; j < hyp_count; j++) {
                    if (strcmp(hyp[j].expr_lhs, gpred.expr_lhs) != 0) continue;

                    if (gpred.type == LV00_PRED_BND) {
                        /* BND 目标：检查假设区间是否包含在目标区间内 */
                        if (hyp[j].bound_lo >= gpred.bound_lo &&
                            hyp[j].bound_hi <= gpred.bound_hi) {
                            proven = true;
                            break;
                        }
                    } else if (gpred.type == LV00_PRED_ABS) {
                        /* ABS 目标：计算最大绝对偏差 */
                        double center = atof(gpred.expr_rhs);
                        double dev_lo = fabs(hyp[j].bound_lo - center);
                        double dev_hi = fabs(hyp[j].bound_hi - center);
                        double max_dev = dev_lo > dev_hi ? dev_lo : dev_hi;
                        if (max_dev <= gpred.bound_abs + 1e-15) {
                            proven = true;
                            break;
                        }
                    }
                }

                result.goals[i].proven = proven;
                if (proven) {
                    result.goals_proven++;
                } else {
                    result.goals_failed++;
                }
            }
        }
    }

    result.success = (result.goals_failed == 0 && goal_count > 0);
    return result;
}

/**
 * @brief 释放证明结果
 *
 * @param result 证明结果（内部 goals 数组将被释放并置 NULL）
 */
void gappa_result_free(Lv00GappaProofResult *result) {
    if (result) {
        lv00_free((void **)&(result->goals));
        result->goals = NULL;
        result->goals_total = 0;
    }
}

/**
 * @brief 注册重写规则
 *
 * @param rules 重写规则数组
 * @param count 规则数量（保留参数，未使用）
 * @return true 表示注册成功
 */
int gappa_register_rewrite_rules(const Lv00GappaRewriteRule *rules, int count) {
    (void)rules; (void)count;
    return 0;
}