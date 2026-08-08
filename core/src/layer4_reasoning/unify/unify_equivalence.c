/**
 * @file unify_equivalence.c
 * @brief proposition equivalence
 * @details Split from unify.c
 */

#include "unify.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/geometric_primitives.h"
#include "lv/proof.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "normalization.h"
#include "stream.h"
#include "stream_context_util.h"
#include "type_system.h"
#include "lv/lv_strbuf.h"
#include "unify_internal.h"

/* ---------------------------------------------------------------------------
 * 命题的等价变换
 * ------------------------------------------------------------------------- */

/* 等价声明存储（TLS 指针 + 惰性分配，动态扩容消除 MAX_EQUIVALENCES 上限） */
static lv_THREAD_LOCAL PropositionEquivalence *g_equivalences = NULL;
static lv_THREAD_LOCAL int g_equivalence_count = 0;
static lv_THREAD_LOCAL int g_equivalence_capacity = 0;

bool unify_declare_proposition_equivalence(int prop_a_id, int prop_b_id, ConstraintGraph *transformation_rule) {
    /* 检查是否已存在相同的等价声明 */
    for (int i = 0; i < g_equivalence_count; i++) {
        if ((g_equivalences[i].prop_a_id == prop_a_id && g_equivalences[i].prop_b_id == prop_b_id) ||
            (g_equivalences[i].prop_a_id == prop_b_id && g_equivalences[i].prop_b_id == prop_a_id)) {
            /* 已存在，更新变换规则 */
            if (g_equivalences[i].transformation) {
                graph_destroy(g_equivalences[i].transformation);
            }
            g_equivalences[i].transformation = transformation_rule;
            return true;
        }
    }

    /* 动态扩容（倍增），替代原 MAX_EQUIVALENCES 满表失败 */
    if (g_equivalence_count >= g_equivalence_capacity) {
        if (!lv_ensure_capacity((void **) &g_equivalences, g_equivalence_count,
                                &g_equivalence_capacity, sizeof(PropositionEquivalence), 0)) {
            LOG_WARN("unify", "Proposition equivalence table growth failed (OOM), cannot add more");
            return false;
        }
    }

    g_equivalences[g_equivalence_count].prop_a_id = prop_a_id;
    g_equivalences[g_equivalence_count].prop_b_id = prop_b_id;
    g_equivalences[g_equivalence_count].transformation = transformation_rule;
    g_equivalence_count++;
    return true;
}

int unify_find_equivalent_proposition(int prop_id, int *equivalent_ids, int max_count) {
    if (!equivalent_ids || max_count <= 0)
        return 0;

    int found = 0;
    for (int i = 0; i < g_equivalence_count && found < max_count; i++) {
        if (g_equivalences[i].prop_a_id == prop_id) {
            equivalent_ids[found++] = g_equivalences[i].prop_b_id;
        } else if (g_equivalences[i].prop_b_id == prop_id) {
            equivalent_ids[found++] = g_equivalences[i].prop_a_id;
        }
    }
    return found;
}

void unify_clear_equivalences(void) {
    for (int i = 0; i < g_equivalence_count; i++) {
        if (g_equivalences[i].transformation) {
            graph_destroy(g_equivalences[i].transformation);
            g_equivalences[i].transformation = NULL;
        }
    }
    g_equivalence_count = 0;
}

void lv_unify_equivalence_storage_cleanup(void) {
    unify_clear_equivalences();
    lv_free((void **) &g_equivalences);
    g_equivalence_capacity = 0;
}
