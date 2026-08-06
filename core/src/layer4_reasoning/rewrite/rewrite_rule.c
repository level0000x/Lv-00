/**
 * @file rewrite_rule.c
 * @brief 重写规则：规则创建与销毁
 *
 * 从 rewrite_match.c 拆分的模块之一（拆分清单见 rewrite_binding.c）。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/rewrite.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "mpz_poly.h"
/* ---------------------------------------------------------------------------
 * 公共 API
 * ------------------------------------------------------------------------- */

/**
 * @brief 销毁重写匹配，释放 node_bindings / constraint_bindings 与结构体本身
 *
 * NULL 安全：match 或任一内部字段为 NULL 时对应释放直接跳过
 * （lv_free 对 NULL 安全），集中收敛散落各文件的
 * lv_free(node_bindings) → lv_free(constraint_bindings) → lv_free(match)
 * 三连释放样板。先例：axiom_rule_engine.c 的 lv_rule_match_destroy。
 */
void rewrite_match_destroy(RewriteMatch *match) {
    if (!match)
        return;
    lv_free((void **) &match->node_bindings);
    lv_free((void **) &match->constraint_bindings);
    lv_free((void **) &match);
}

RewriteRule *rewrite_rule_create(const char *name, RewritePattern *pattern, RewriteReplacement *replacement,
                                 int measure) {
    RewriteRule *rule = lv_calloc(1, sizeof(RewriteRule));
    if (!rule)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "rewrite_rule_create: lv_calloc(%zu) failed", sizeof(RewriteRule));
    /* 【内存管理策略】strdup 为 rule->name 分配独立副本。
     * 若分配失败，需回滚已分配的 rule 结构体。
     * 注意：pattern 和 replacement 的所有权不属于 rule，
     * 由调用者管理，无需在此处释放。 */
    rule->name = lv_strdup_safe(name);
    if (!rule->name) {
        lv_free((void **) &rule);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "rewrite_rule_create: lv_strdup_safe failed for name");
    }
    rule->pattern = pattern;
    rule->replacement = replacement;
    rule->reduction_measure = measure;
    rule->condition_func = NULL;
    rule->condition_data = NULL;
    return rule;
}

/* 销毁重写规则，释放其持有的资源。
 *
 * 【所有权说明】
 * 此函数仅释放 rule 本身及其 name 字符串。
 * rule->pattern 和 rule->replacement 的所有权不属于 rule 对象，
 * 它们由规则文件解析器（parse_lvz_file）统一管理，在解析器销毁时
 * 通过 parsed_rule_destroy 释放。因此此处不释放 pattern 和 replacement，
 * 避免双重释放。
 * 如果需要在其他场景下独立销毁 rule 及其子对象，应先调用相应的
 * pattern/replacement 销毁函数，再调用此函数。
 */
void rewrite_rule_destroy(RewriteRule *rule) {
    if (rule) {
        lv_free((void **) &rule->name);
        /* 注意：不释放 rule->pattern 和 rule->replacement，所有权不属于此对象 */
        lv_free((void **) &rule);
    }
}
