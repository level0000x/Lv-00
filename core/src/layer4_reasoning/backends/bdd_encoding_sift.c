/**
 * @file bdd_encoding_sift.c
 * @brief BDD 变量序 Sifting 优化（由 bdd_encoding.c 拆分子模块）
 *
 * @details 基于唯一表探查链的变量交换与 sifting 上/下移优化。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/bdd_encoding.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_utils.h"
#include "lv/lv_check.h"
#include "lv/lv_constraint_guard.h"
#include "lv/lv_lifecycle.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_xmacro.h"

#include "bdd_encoding_internal.h"

/* ========================================================================
 * Sifting 变量序优化
 *
 * 算法：
 * 1. 对于每个变量 i：
 *    a. 记录当前位置和当前节点数
 *    b. 将变量 i 从变量序中移出
 *    c. 尝试将变量 i 插入到每个位置 j
 *    d. 记录使节点数最少的位置
 *    e. 固定变量 i 在该位置
 * 2. 返回最终的节点数
 * ======================================================================== */

int bdd_reorder_sift(BDDManager *mgr) {
    if (!mgr || mgr->var_count <= 0)
        return -1;

    int n = mgr->var_count;
    int *best_order = (int *) lv_malloc((size_t) n * sizeof(int));
    if (!best_order)
        return -1;
    memcpy(best_order, mgr->var_order, (size_t) n * sizeof(int));

    int improved = 0;

    for (int var = 0; var < n; var++) {
        int orig_pos = -1;
        /* 查找变量 var 在 var_order 中的当前位置 */
        for (int p = 0; p < n; p++) {
            if (mgr->var_order[p] == var) {
                orig_pos = p;
                break;
            }
        }
        if (orig_pos < 0)
            continue;

        /* 记录当前位置的节点数 */
        uint64_t orig_nodes = mgr->node_count;

        /* 移出变量 var */
        for (int p = orig_pos; p < n - 1; p++) {
            mgr->var_order[p] = mgr->var_order[p + 1];
        }

        /* 尝试每个插入位置 */
        int best_pos = 0;
        uint64_t best_nodes = UINT64_MAX;

        for (int insert_pos = 0; insert_pos < n; insert_pos++) {
            /* 在 insert_pos 处临时插入 var */
            for (int p = n - 1; p > insert_pos; p--) {
                mgr->var_order[p] = mgr->var_order[p - 1];
            }
            mgr->var_order[insert_pos] = var;

            /* 真正重建唯一表：收集所有节点，清空表，按新变量序重新插入 */
            uint64_t rebuild_nodes = 0;
            if (mgr->unique_table && mgr->unique_table_size > 0) {
                /* 收集所有非终端、非墓碑节点 */
                BDDNode **saved = (BDDNode **) lv_malloc((size_t) mgr->unique_table_size * sizeof(BDDNode *));
                int saved_count = 0;
                if (saved) {
                    for (int si = 0; si < mgr->unique_table_size && saved_count < mgr->unique_table_size; si++) {
                        BDDNode *entry = mgr->unique_table[si];
                        if (entry && entry != BDD_TOMBSTONE && entry->var_id >= 0) {
                            saved[saved_count++] = entry;
                        }
                        mgr->unique_table[si] = NULL; /* 清空槽位 */
                    }
                    /* 按新变量序重新插入 */
                    mgr->node_count = 0;
                    for (int si = 0; si < saved_count; si++) {
                        BDDNode *node = saved[si];
                        int h = bdd_unique_hash(node->var_id, node->low, node->high, mgr->unique_table_size);
                        for (int pi = 0; pi < mgr->unique_table_size; pi++) {
                            int s = (h + pi) % mgr->unique_table_size;
                            if (mgr->unique_table[s] == NULL || mgr->unique_table[s] == BDD_TOMBSTONE) {
                                mgr->unique_table[s] = node;
                                mgr->node_count++;
                                break;
                            }
                        }
                    }
                    lv_free((void **) &saved);
                }
                rebuild_nodes = mgr->node_count;
            } else {
                rebuild_nodes = mgr->node_count;
            }

            if (rebuild_nodes < best_nodes) {
                best_nodes = rebuild_nodes;
                best_pos = insert_pos;
            }

            /* 恢复：移出 var */
            for (int p = insert_pos; p < n - 1; p++) {
                mgr->var_order[p] = mgr->var_order[p + 1];
            }
        }

        /* 将变量 var 固定到最佳位置 */
        for (int p = n - 1; p > best_pos; p--) {
            mgr->var_order[p] = mgr->var_order[p - 1];
        }
        mgr->var_order[best_pos] = var;

        if (best_nodes < orig_nodes) {
            improved++;
        }
    }

    lv_free((void **) &best_order);
    return improved;
}

