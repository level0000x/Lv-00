/**
 * @file unify_equivalence.c
 * @brief proposition equivalence
 * @details Split from unify.c
 */

#include "lv/unify.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/geometric_primitives.h"
#include "lv/proof.h"

#include "lv/debug.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/normalization.h"
#include "lv/stream.h"
#include "lv/stream_context_util.h"
#include "lv/type_system.h"
#include "lv/lv_strbuf.h"
#include "unify_internal.h"

/* ---------------------------------------------------------------------------
 * 命题的等价变换
 * ------------------------------------------------------------------------- */

/* 等价声明存储（lvTlsVector：TLS 指针 + 计数 + 容量，惰性分配，动态扩容消除 MAX_EQUIVALENCES 上限） */
static lv_THREAD_LOCAL lvTlsVector g_equivalences = {0};

bool unify_declare_proposition_equivalence(int prop_a_id, int prop_b_id, ConstraintGraph *transformation_rule) {
    PropositionEquivalence *arr = (PropositionEquivalence *) g_equivalences.ptr;
    /* 检查是否已存在相同的等价声明 */
    for (int i = 0; i < g_equivalences.count; i++) {
        if ((arr[i].prop_a_id == prop_a_id && arr[i].prop_b_id == prop_b_id) ||
            (arr[i].prop_a_id == prop_b_id && arr[i].prop_b_id == prop_a_id)) {
            /* 已存在，更新变换规则 */
            if (arr[i].transformation) {
                graph_destroy(arr[i].transformation);
            }
            arr[i].transformation = transformation_rule;
            return true;
        }
    }

    /* 动态扩容（倍增），替代原 MAX_EQUIVALENCES 满表失败 */
    if (!lv_tls_vector_ensure(&g_equivalences, g_equivalences.count + 1, sizeof(PropositionEquivalence))) {
        LOG_WARN("unify", "Proposition equivalence table growth failed (OOM), cannot add more");
        return false;
    }
    /* 扩容可能 realloc，重新取指针 */
    arr = (PropositionEquivalence *) g_equivalences.ptr;

    arr[g_equivalences.count].prop_a_id = prop_a_id;
    arr[g_equivalences.count].prop_b_id = prop_b_id;
    arr[g_equivalences.count].transformation = transformation_rule;
    g_equivalences.count++;
    return true;
}

int unify_find_equivalent_proposition(int prop_id, int *equivalent_ids, int max_count) {
    if (!equivalent_ids || max_count <= 0)
        return 0;

    PropositionEquivalence *arr = (PropositionEquivalence *) g_equivalences.ptr;
    int found = 0;
    for (int i = 0; i < g_equivalences.count && found < max_count; i++) {
        if (arr[i].prop_a_id == prop_id) {
            equivalent_ids[found++] = arr[i].prop_b_id;
        } else if (arr[i].prop_b_id == prop_id) {
            equivalent_ids[found++] = arr[i].prop_a_id;
        }
    }
    return found;
}

void unify_clear_equivalences(void) {
    PropositionEquivalence *arr = (PropositionEquivalence *) g_equivalences.ptr;
    for (int i = 0; i < g_equivalences.count; i++) {
        if (arr[i].transformation) {
            graph_destroy(arr[i].transformation);
            arr[i].transformation = NULL;
        }
    }
    lv_tls_vector_clear(&g_equivalences);
}

void lv_unify_equivalence_storage_cleanup(void) {
    unify_clear_equivalences();
    lv_tls_vector_cleanup(&g_equivalences);
}
