/**
 * @file bdd_encoding_cnf.c
 * @brief BDD→CNF 转换（Tseitin 变换）（由 bdd_encoding.c 拆分子模块）
 *
 * @details BDD 节点收集遍历与基于 Tseitin 的 CNF 输出。
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
 * 内部：BDD 节点遍历辅助
 * ======================================================================== */

/** BDD 遍历访问标记（用于 bdd_to_cnf 的拓扑排序） */
typedef struct BDDVisitEntry {
    BDDNode *node;
    int aux_var; /**< Tseitin 辅助变量编号 */
    bool visited;
} BDDVisitEntry;

/** 最大 BDD 节点数（用于遍历数组） */
#define BDD_TRAVERSE_MAX 65536

/* 【lv_bfs_run / lv_cycle_detect 收敛评估结论（不收敛，保留本实现）】
 *   lv_bfs_run / lv_cycle_detect 要求"整数 id 空间 0..node_count-1 + 出边
 *   邻居回调"。本函数是 BDD DAG 的"收集"语义，两者形态不匹配：
 *     1. 节点无整数 id：BDDNode 是指针 DAG（high/low 边），无 id 字段、
 *        var_id 为变量编号且终端节点为负，无法映射到连续的 int id 空间。
 *     2. 收集语义：调用方 bdd_to_cnf 需要 entries 数组按 DFS 顺序直接索引
 *        （随后为每个条目分配 aux_var 并回查 root），是"填充调用方预分配
 *        数组"而非"遍历回调"；max_entries（BDD_TRAVERSE_MAX）硬上限截断
 *        语义也无法用 lv_bfs_run 表达。
 *     3. 指针去重：本函数按 BDDNode* 指针线性去重；lv_bfs_run 的 visited
 *        是 bool 数组，按 id 索引，无法承载指针键。
 *   结论：语义确实不同（指针 DAG 收集 vs 整数 id 图回调遍历），保持本实现。 */
/** 收集 BDD 中所有非终端节点（拓扑排序） */
static int bdd_collect_nodes(BDDNode *root, BDDVisitEntry *entries, int max_entries) {
    if (!root || max_entries <= 0)
        return 0;

    /* 简单 DFS 收集 */
    int count = 0;
    BDDNode *stack[BDD_TRAVERSE_MAX];
    int stack_top = 0;

    stack[stack_top++] = root;

    while (stack_top > 0 && count < max_entries) {
        BDDNode *node = stack[--stack_top];

        /* 终端节点跳过 */
        if (!node || node->var_id < 0)
            continue;

        /* 检查是否已收集 */
        bool found = false;
        for (int i = 0; i < count; i++) {
            if (entries[i].node == node) {
                found = true;
                break;
            }
        }
        if (found)
            continue;

        entries[count].node = node;
        entries[count].aux_var = 0;
        entries[count].visited = false;
        count++;

        if (stack_top < BDD_TRAVERSE_MAX) {
            if (node->high)
                stack[stack_top++] = node->high;
            if (node->low)
                stack[stack_top++] = node->low;
        }
    }

    return count;
}

/* ========================================================================
 * bdd_to_cnf —— BDD -> DIMACS CNF
 *
 * 使用 Tseitin 变换：对 BDD 中每个非终端节点引入辅助变量。
 * 节点 v = ITE(var, low, high) 的 Tseitin 编码：
 *   (~v | ~var | high) & (~v | var | low) & (v | ~var | ~high) & (v | var | ~low)
 *
 * 变量编号：
 *   1..n = 原始 BDD 变量
 *   n+1.. = Tseitin 辅助变量
 * 根节点的辅助变量必须为 true（单位子句）。
 * ======================================================================== */

bool bdd_to_cnf(BDDNode *bdd, char **out_cnf) {
    if (!bdd || !out_cnf)
        return false;

    /* 终端节点特例 */
    if (bdd->var_id < 0) {
        lvStrBuf sb;
        lv_strbuf_init(&sb);
        if (bdd->complemented) {
            /* False 节点 -> 空 CNF（不可满足） */
            lv_strbuf_printf(&sb, "c BDD is FALSE\np cnf 1 1\n1 0\n-1 0\n");
        } else {
            /* True 节点 -> 空 CNF（可满足） */
            lv_strbuf_printf(&sb, "c BDD is TRUE\np cnf 1 1\n1 0\n");
        }
        *out_cnf = lv_strbuf_to_string(&sb);
        return *out_cnf != NULL;
    }

    /* 收集所有非终端节点 */
    BDDVisitEntry *entries = (BDDVisitEntry *) lv_malloc((size_t) BDD_TRAVERSE_MAX * sizeof(BDDVisitEntry));
    if (!entries)
        return false;

    int node_count = bdd_collect_nodes(bdd, entries, BDD_TRAVERSE_MAX);

    /* 确定最大原始变量 ID */
    int max_var_id = 0;
    for (int i = 0; i < node_count; i++) {
        if (entries[i].node->var_id > max_var_id)
            max_var_id = entries[i].node->var_id;
    }

    /* 分配辅助变量：从 max_var_id + 1 开始 */
    int aux_base = max_var_id + 1;
    for (int i = 0; i < node_count; i++) {
        entries[i].aux_var = aux_base + i;
    }

    /* 根节点的辅助变量映射 */
    int root_aux = -1;
    for (int i = 0; i < node_count; i++) {
        if (entries[i].node == bdd) {
            root_aux = entries[i].aux_var;
            break;
        }
    }

    /* 子句体先构建到临时 lvStrBuf（自动扩容，消除原固定估算 4096+node_count*256 的静默截断），
     * 最后与 DIMACS 头部合并输出 */
    lvStrBuf body;
    lv_strbuf_init(&body);
    int clause_count = 0;

    /* 辅助变量查找函数 */
    /* 对于终端节点，返回其布尔值对应的变量 */
    /* 这里我们用一个简单的线性查找 */

    /* 生成子句 */
    for (int i = 0; i < node_count; i++) {
        BDDNode *node = entries[i].node;
        int v = entries[i].aux_var; /* 辅助变量 */
        int x = node->var_id;       /* 决策变量 */

        /* 确定 low 和 high 的变量编号 */
        int low_lit, high_lit;
        if (!node->low || node->low->var_id < 0) {
            /* low 是终端节点 */
            low_lit = 0; /* 表示 false */
        } else {
            /* 查找 low 的辅助变量 */
            low_lit = 0;
            for (int j = 0; j < node_count; j++) {
                if (entries[j].node == node->low) {
                    low_lit = entries[j].aux_var;
                    break;
                }
            }
        }

        if (!node->high || node->high->var_id < 0) {
            /* high 是终端节点 */
            high_lit = 0;
        } else {
            high_lit = 0;
            for (int j = 0; j < node_count; j++) {
                if (entries[j].node == node->high) {
                    high_lit = entries[j].aux_var;
                    break;
                }
            }
        }

        /* Tseitin 编码：node = ITE(x, high, low)
         * 等价于四个子句：
         *   (~v | ~x | high_lit)   -- 如果 v=1 且 x=1 则 high=1
         *   (~v | x | low_lit)     -- 如果 v=1 且 x=0 则 low=1
         *   (v | ~x | ~high_lit)   -- 如果 v=0 且 x=1 则 high=0
         *   (v | x | ~low_lit)     -- 如果 v=0 且 x=0 则 low=0
         *
         * 对于终端节点（lit=0），子句简化
         */

        /* 子句 1: ~v | ~x | high */
        if (high_lit > 0) {
            lv_strbuf_printf(&body, "%d %d %d 0\n", -v, -x, high_lit);
        } else {
            /* high 是 false (0)，子句变为 ~v | ~x（省略 0） */
            lv_strbuf_printf(&body, "%d %d 0\n", -v, -x);
        }
        clause_count++;

        /* 子句 2: ~v | x | low */
        if (low_lit > 0) {
            lv_strbuf_printf(&body, "%d %d %d 0\n", -v, x, low_lit);
        } else {
            lv_strbuf_printf(&body, "%d %d 0\n", -v, x);
        }
        clause_count++;

        /* 子句 3: v | ~x | ~high */
        if (high_lit > 0) {
            lv_strbuf_printf(&body, "%d %d %d 0\n", v, -x, -high_lit);
            clause_count++;
        }

        /* 子句 4: v | x | ~low */
        if (low_lit > 0) {
            lv_strbuf_printf(&body, "%d %d %d 0\n", v, x, -low_lit);
            clause_count++;
        }
    }

    /* 根节点单位子句：root_aux 必须为 true */
    if (root_aux > 0) {
        lv_strbuf_printf(&body, "%d 0\n", root_aux);
        clause_count++;
    }

    /* 组装最终输出：注释行 + DIMACS p 行 + 子句体（顺序与原回填实现字节级一致） */
    int total_vars = aux_base + node_count - 1;
    lvStrBuf out;
    lv_strbuf_init(&out);
    lv_strbuf_printf(&out, "c BDD-to-CNF conversion (Tseitin)\n");
    lv_strbuf_printf(&out, "p cnf %d %d\n", total_vars, clause_count);
    lv_strbuf_append_str(&out, lv_strbuf_cstr(&body));
    lv_strbuf_destroy(&body);

    lv_free((void **) &entries);

    *out_cnf = lv_strbuf_to_string(&out);
    return *out_cnf != NULL;
}

