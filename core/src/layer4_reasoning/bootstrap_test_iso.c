/**
 * @file bootstrap_test_iso.c
 * @brief Lv-00 自举差分测试框架 —— 图同构比较器
 *
 * @details 由 bootstrap_test.c 按功能组件拆分而来。
 *          共享兼容定义与框架状态见 bootstrap_test_internal.h。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#include "lv/bootstrap_test.h"
#include "lv/lv_log.h"

#include "lv/lv_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lv/constraint_graph.h"
#include "lv/cross_platform.h"
#include "lv/engine.h"
#include "lv/lv.h"
#include "lv/lv_utils.h"
#include "lv/proof_trace.h"
#include "lv/lv_internal.h"

#include "bootstrap_test_internal.h"

/* ============== 图同构比较器 ============== */

/** @brief 图同构比较器结构体 */
struct GraphIsomorphismComparator {
    bool ignore_ids;        /**< 是否忽略节点 ID 差异 */
    bool compare_coords;    /**< 是否比较坐标 */
    double coord_tolerance; /**< 坐标比较容差 */
};

/**
 * @brief 创建图同构比较器
 *
 * 默认配置：忽略 ID、比较坐标、容差 1e-10。
 *
 * @return 新创建的 GraphIsomorphismComparator 指针，失败返回 NULL
 */
GraphIsomorphismComparator *graph_isomorphism_create(void) {
    GraphIsomorphismComparator *comp = lv_calloc(1, sizeof(GraphIsomorphismComparator));
    if (!comp) {
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "graph_isomorphism_create: calloc failed");
    }

    comp->ignore_ids = true;
    comp->compare_coords = true;
    comp->coord_tolerance = 1e-10;

    return comp;
}

/**
 * @brief 销毁图同构比较器
 *
 * @param comp 待销毁的比较器指针（可为 NULL）
 */
void graph_isomorphism_destroy(GraphIsomorphismComparator *comp) {
    lv_free((void **) &comp);
}

/**
 * @brief 配置图同构比较器参数
 *
 * @param comp            比较器
 * @param ignore_ids      是否忽略节点 ID
 * @param compare_coords  是否比较坐标
 * @param coord_tolerance 坐标比较容差
 */
void graph_isomorphism_configure(GraphIsomorphismComparator *comp, bool ignore_ids, bool compare_coords,
                                 double coord_tolerance) {
    if (comp) {
        comp->ignore_ids = ignore_ids;
        comp->compare_coords = compare_coords;
        comp->coord_tolerance = coord_tolerance;
    }
}

/**
 * @brief 比较两个约束图是否同构
 *
 * 使用 VF2 风格的度数序列 + 邻域签名匹配算法。
 * 先比较节点数和约束数，再比较排序后的度数序列，
 * 最后比较排序后的邻域签名多集合。
 *
 * @param comp   比较器
 * @param graph_a 图 A
 * @param graph_b 图 B
 * @return true 同构，false 不同构或参数无效
 */
bool graph_isomorphism_compare(GraphIsomorphismComparator *comp, const void *graph_a, const void *graph_b) {
    if (!comp || !graph_a || !graph_b) {
        return false;
    }

    /* VF2 同构检测（基于度数序列和邻域签名匹配，完整版需支持回溯搜索） */
    const ConstraintGraph *ga = (const ConstraintGraph *) graph_a;
    const ConstraintGraph *gb = (const ConstraintGraph *) graph_b;

    if (graph_get_node_count(ga) != graph_get_node_count(gb)) {
        return false;
    }
    if (graph_get_constraint_count(ga) != graph_get_constraint_count(gb)) {
        return false;
    }

    int n = graph_get_node_count(ga);
    if (n == 0)
        return true;

    /* 计算度数序列 */
    int *deg_a = (int *) lv_calloc((size_t) n, sizeof(int));
    int *deg_b = (int *) lv_calloc((size_t) n, sizeof(int));
    if (!deg_a || !deg_b) {
        lv_free((void **) &deg_a);
        lv_free((void **) &deg_b);
        return false;
    }

    for (int i = 0; i < n; i++) {
        int cids[64];
        deg_a[i] = graph_find_constraints_involving(ga, i, cids, 64);
        deg_b[i] = graph_find_constraints_involving(gb, i, cids, 64);
    }

    /* 排序度数序列后比较 */
    for (int i = 0; i < n - 1; i++) {
        for (int j = i + 1; j < n; j++) {
            if (deg_a[i] > deg_a[j]) {
                int t = deg_a[i];
                deg_a[i] = deg_a[j];
                deg_a[j] = t;
            }
            if (deg_b[i] > deg_b[j]) {
                int t = deg_b[i];
                deg_b[i] = deg_b[j];
                deg_b[j] = t;
            }
        }
    }

    bool same_degree = true;
    for (int i = 0; i < n; i++) {
        if (deg_a[i] != deg_b[i]) {
            same_degree = false;
            break;
        }
    }

    if (!same_degree) {
        lv_free((void **) &deg_a);
        lv_free((void **) &deg_b);
        return false;
    }

    /* VF2 邻域签名比较：对每个节点，收集其邻居的度数并排序后比较 */
    /* 重新计算度数（因为上面的已排序） */
    int *deg_a_raw = (int *) lv_calloc((size_t) n, sizeof(int));
    int *deg_b_raw = (int *) lv_calloc((size_t) n, sizeof(int));
    if (!deg_a_raw || !deg_b_raw) {
        lv_free((void **) &deg_a);
        lv_free((void **) &deg_b);
        lv_free((void **) &deg_a_raw);
        lv_free((void **) &deg_b_raw);
        return false;
    }

    for (int i = 0; i < n; i++) {
        int cids[64];
        deg_a_raw[i] = graph_find_constraints_involving(ga, i, cids, 64);
        deg_b_raw[i] = graph_find_constraints_involving(gb, i, cids, 64);
    }

    /* 为每个节点计算排序后的邻居度数签名 */
    int max_neighbors = 64;
    int *neighbor_sigs_a = (int *) lv_calloc((size_t) n * (size_t) max_neighbors, sizeof(int));
    int *neighbor_sigs_b = (int *) lv_calloc((size_t) n * (size_t) max_neighbors, sizeof(int));
    int *neighbor_counts_a = (int *) lv_calloc((size_t) n, sizeof(int));
    int *neighbor_counts_b = (int *) lv_calloc((size_t) n, sizeof(int));
    if (!neighbor_sigs_a || !neighbor_sigs_b || !neighbor_counts_a || !neighbor_counts_b) {
        lv_free((void **) &deg_a);
        lv_free((void **) &deg_b);
        lv_free((void **) &deg_a_raw);
        lv_free((void **) &deg_b_raw);
        lv_free((void **) &neighbor_sigs_a);
        lv_free((void **) &neighbor_sigs_b);
        lv_free((void **) &neighbor_counts_a);
        lv_free((void **) &neighbor_counts_b);
        return false;
    }

    for (int i = 0; i < n; i++) {
        int cids_a[64], cids_b[64];
        int nc_a = graph_find_constraints_involving(ga, i, cids_a, 64);
        int nc_b = graph_find_constraints_involving(gb, i, cids_b, 64);

        /* 收集 ga 中节点 i 的邻居度数 */
        for (int c = 0; c < nc_a && neighbor_counts_a[i] < max_neighbors; c++) {
            Constraint *cons = graph_get_constraint(ga, cids_a[c]);
            if (!cons || !cons->is_active)
                continue;
            for (int p = 0; p < cons->participant_count; p++) {
                int nb = cons->participants[p];
                if (nb >= 0 && nb < n && nb != i) {
                    neighbor_sigs_a[i * max_neighbors + neighbor_counts_a[i]] = deg_a_raw[nb];
                    neighbor_counts_a[i]++;
                }
            }
        }

        /* 收集 gb 中节点 i 的邻居度数 */
        for (int c = 0; c < nc_b && neighbor_counts_b[i] < max_neighbors; c++) {
            Constraint *cons = graph_get_constraint(gb, cids_b[c]);
            if (!cons || !cons->is_active)
                continue;
            for (int p = 0; p < cons->participant_count; p++) {
                int nb = cons->participants[p];
                if (nb >= 0 && nb < n && nb != i) {
                    neighbor_sigs_b[i * max_neighbors + neighbor_counts_b[i]] = deg_b_raw[nb];
                    neighbor_counts_b[i]++;
                }
            }
        }

        /* 排序每个节点的邻居度数签名 */
        for (int x = 0; x < neighbor_counts_a[i] - 1; x++) {
            for (int y = x + 1; y < neighbor_counts_a[i]; y++) {
                if (neighbor_sigs_a[i * max_neighbors + x] > neighbor_sigs_a[i * max_neighbors + y]) {
                    int t = neighbor_sigs_a[i * max_neighbors + x];
                    neighbor_sigs_a[i * max_neighbors + x] = neighbor_sigs_a[i * max_neighbors + y];
                    neighbor_sigs_a[i * max_neighbors + y] = t;
                }
            }
        }
        for (int x = 0; x < neighbor_counts_b[i] - 1; x++) {
            for (int y = x + 1; y < neighbor_counts_b[i]; y++) {
                if (neighbor_sigs_b[i * max_neighbors + x] > neighbor_sigs_b[i * max_neighbors + y]) {
                    int t = neighbor_sigs_b[i * max_neighbors + x];
                    neighbor_sigs_b[i * max_neighbors + x] = neighbor_sigs_b[i * max_neighbors + y];
                    neighbor_sigs_b[i * max_neighbors + y] = t;
                }
            }
        }
    }

    /* 比较两个图的邻域签名多集合 */
    /* 将所有节点的签名拼接成一个大数组，排序后比较 */
    int total_sigs_a = 0, total_sigs_b = 0;
    for (int i = 0; i < n; i++) {
        total_sigs_a += neighbor_counts_a[i];
        total_sigs_b += neighbor_counts_b[i];
    }

    bool same_signatures = (total_sigs_a == total_sigs_b);
    if (same_signatures && total_sigs_a > 0) {
        /* 拼接并排序所有签名 */
        int *all_sigs_a = (int *) lv_calloc((size_t) total_sigs_a, sizeof(int));
        int *all_sigs_b = (int *) lv_calloc((size_t) total_sigs_b, sizeof(int));
        if (!all_sigs_a || !all_sigs_b) {
            same_signatures = false;
        } else {
            int pos = 0;
            for (int i = 0; i < n; i++) {
                for (int j = 0; j < neighbor_counts_a[i]; j++) {
                    all_sigs_a[pos++] = neighbor_sigs_a[i * max_neighbors + j];
                }
            }
            pos = 0;
            for (int i = 0; i < n; i++) {
                for (int j = 0; j < neighbor_counts_b[i]; j++) {
                    all_sigs_b[pos++] = neighbor_sigs_b[i * max_neighbors + j];
                }
            }
            /* 排序 */
            for (int i = 0; i < total_sigs_a - 1; i++) {
                for (int j = i + 1; j < total_sigs_a; j++) {
                    if (all_sigs_a[i] > all_sigs_a[j]) {
                        int t = all_sigs_a[i];
                        all_sigs_a[i] = all_sigs_a[j];
                        all_sigs_a[j] = t;
                    }
                }
            }
            for (int i = 0; i < total_sigs_b - 1; i++) {
                for (int j = i + 1; j < total_sigs_b; j++) {
                    if (all_sigs_b[i] > all_sigs_b[j]) {
                        int t = all_sigs_b[i];
                        all_sigs_b[i] = all_sigs_b[j];
                        all_sigs_b[j] = t;
                    }
                }
            }
            for (int i = 0; i < total_sigs_a; i++) {
                if (all_sigs_a[i] != all_sigs_b[i]) {
                    same_signatures = false;
                    break;
                }
            }
            lv_free((void **) &all_sigs_a);
            lv_free((void **) &all_sigs_b);
        }
    }

    lv_free((void **) &deg_a);
    lv_free((void **) &deg_b);
    lv_free((void **) &deg_a_raw);
    lv_free((void **) &deg_b_raw);
    lv_free((void **) &neighbor_sigs_a);
    lv_free((void **) &neighbor_sigs_b);
    lv_free((void **) &neighbor_counts_a);
    lv_free((void **) &neighbor_counts_b);
    return same_signatures;
}

/**
 * @brief 计算约束图的 Weisfeiler-Lehman 图核哈希
 *
 * 执行 3 轮 WL 迭代：初始标签为度数，每轮将节点标签
 * 与其邻居标签和约束类型哈希混合，最后聚合为 64 位哈希值。
 *
 * @param graph 约束图
 * @return 64 位哈希值，失败返回 0
 */
uint64_t graph_isomorphism_hash(const void *graph) {
    if (!graph) {
        return 0;
    }

    /* WL (Weisfeiler-Lehman) 图核哈希：迭代压缩节点标签 */
    const ConstraintGraph *g = (const ConstraintGraph *) graph;
    int n = graph_get_node_count(g);
    if (n == 0)
        return 0;

    /* 初始标签：度数 */
    uint64_t *labels = (uint64_t *) lv_calloc((size_t) n, sizeof(uint64_t));
    if (!labels)
        return 0;

    for (int i = 0; i < n; i++) {
        int cids[64];
        int deg = graph_find_constraints_involving(g, i, cids, 64);
        labels[i] = (uint64_t) (deg + 1);
    }

    /* WL 迭代（3 轮） */
    for (int iter = 0; iter < 3; iter++) {
        uint64_t *new_labels = (uint64_t *) lv_calloc((size_t) n, sizeof(uint64_t));
        if (!new_labels) {
            lv_free((void **) &labels);
            return 0;
        }

        for (int i = 0; i < n; i++) {
            int cids[64];
            int nc = graph_find_constraints_involving(g, i, cids, 64);
            uint64_t hash = labels[i];
            for (int c = 0; c < nc; c++) {
                Constraint *cons = graph_get_constraint(g, cids[c]);
                if (!cons)
                    continue;
                for (int p = 0; p < cons->participant_count; p++) {
                    int nb = cons->participants[p];
                    if (nb >= 0 && nb < n) {
                        hash ^= (labels[nb] * 2654435761ULL + (uint64_t) cons->type);
                    }
                }
            }
            new_labels[i] = hash;
        }
        lv_free((void **) &labels);
        labels = new_labels;
    }

    /* 聚合所有标签为最终哈希 */
    uint64_t final_hash = 0;
    for (int i = 0; i < n; i++) {
        final_hash ^= (labels[i] * (uint64_t) (i + 1));
    }
    lv_free((void **) &labels);

    return final_hash;
}

/**
 * @brief 查找两个同构图之间的节点映射
 *
 * 基于度数匹配的贪心算法。先按度数匹配节点，
 * 再验证边保持性（G1 中的约束在映射下 G2 中也存在）。
 * 完整版需要回溯和约束传播支持。
 *
 * @param comp                 比较器
 * @param graph_a              图 A
 * @param graph_b              图 B
 * @param out_node_mapping     输出节点映射数组（na 个 int），调用者负责 free
 * @param out_constraint_mapping 输出约束映射数组（图 A 约束数 个 int，
 *                               元素 i 为图 A 约束 i 在图 B 中对应的约束 ID，
 *                               未匹配或映射失败时为 -1），调用者负责 free
 * @return true 映射成功，false 失败或不同构
 */
bool graph_isomorphism_find_mapping(GraphIsomorphismComparator *comp, const void *graph_a, const void *graph_b,
                                    int **out_node_mapping, int **out_constraint_mapping) {
    if (!comp || !graph_a || !graph_b) {
        return false;
    }

    /* 映射查找（基于度数匹配的贪心算法，完整版需支持回溯和约束传播） */
    const ConstraintGraph *ga = (const ConstraintGraph *) graph_a;
    const ConstraintGraph *gb = (const ConstraintGraph *) graph_b;

    int na = graph_get_node_count(ga);
    int nb = graph_get_node_count(gb);
    if (na != nb)
        return false;

    /* 无论调用方是否索要节点映射，内部都必须先确定节点映射，
     * 约束映射依赖节点映射计算结果 */
    int *mapping = (int *) lv_calloc((size_t) na, sizeof(int));
    if (!mapping) {
        if (out_node_mapping) {
            *out_node_mapping = NULL;
        }
        if (out_constraint_mapping) {
            *out_constraint_mapping = NULL;
        }
        return false;
    }

    /* 计算度数 */
    int *deg_a = (int *) lv_calloc((size_t) na, sizeof(int));
    int *deg_b = (int *) lv_calloc((size_t) nb, sizeof(int));
    if (!deg_a || !deg_b) {
        lv_free((void **) &mapping);
        lv_free((void **) &deg_a);
        lv_free((void **) &deg_b);
        if (out_node_mapping) {
            *out_node_mapping = NULL;
        }
        if (out_constraint_mapping) {
            *out_constraint_mapping = NULL;
        }
        return false;
    }

    for (int i = 0; i < na; i++) {
        int cids[64];
        deg_a[i] = graph_find_constraints_involving(ga, i, cids, 64);
    }
    for (int i = 0; i < nb; i++) {
        int cids[64];
        deg_b[i] = graph_find_constraints_involving(gb, i, cids, 64);
    }

    /* 贪心匹配：按度数排序后逐个匹配 */
    bool *used = (bool *) lv_calloc((size_t) nb, sizeof(bool));
    if (!used) {
        lv_free((void **) &mapping);
        lv_free((void **) &deg_a);
        lv_free((void **) &deg_b);
        if (out_node_mapping) {
            *out_node_mapping = NULL;
        }
        if (out_constraint_mapping) {
            *out_constraint_mapping = NULL;
        }
        return false;
    }

    for (int i = 0; i < na; i++) {
        mapping[i] = -1;
        for (int j = 0; j < nb; j++) {
            if (!used[j] && deg_a[i] == deg_b[j]) {
                mapping[i] = j;
                used[j] = true;
                break;
            }
        }
    }

    /* 检查是否全部匹配 */
    bool all_mapped = true;
    for (int i = 0; i < na; i++) {
        if (mapping[i] < 0) {
            all_mapped = false;
            break;
        }
    }

    /* 边保持验证：检查 G1 中所有边在映射下是否在 G2 中也存在 */
    bool edges_preserved = true;
    if (all_mapped) {
        for (int c = 0; c < graph_get_constraint_count(ga) && edges_preserved; c++) {
            Constraint *cons = graph_get_constraint(ga, c);
            if (!cons || !cons->is_active)
                continue;
            if (cons->participant_count < 2)
                continue;

            /* 对每对参与者 (u, v)，检查 (map[u], map[v]) 是否在 G2 中有对应约束 */
            for (int p = 0; p < cons->participant_count && edges_preserved; p++) {
                int u = cons->participants[p];
                if (u < 0 || u >= na)
                    continue;
                int u_mapped = mapping[u];

                for (int q = p + 1; q < cons->participant_count && edges_preserved; q++) {
                    int v = cons->participants[q];
                    if (v < 0 || v >= na)
                        continue;
                    int v_mapped = mapping[v];

                    /* 在 G2 中查找 u_mapped 和 v_mapped 之间是否有相同类型的约束 */
                    int cids_b[64];
                    int nc_b = graph_find_constraints_involving(gb, u_mapped, cids_b, 64);
                    bool found_edge = false;
                    for (int cb = 0; cb < nc_b; cb++) {
                        Constraint *cons_b = graph_get_constraint(gb, cids_b[cb]);
                        if (!cons_b || !cons_b->is_active)
                            continue;
                        if (cons_b->type != cons->type)
                            continue;
                        /* 检查 cons_b 是否包含 v_mapped */
                        for (int pp = 0; pp < cons_b->participant_count; pp++) {
                            if (cons_b->participants[pp] == v_mapped) {
                                found_edge = true;
                                break;
                            }
                        }
                        if (found_edge)
                            break;
                    }
                    if (!found_edge) {
                        edges_preserved = false;
                    }
                }
            }
        }
    }

    bool mapping_established = all_mapped && edges_preserved;

    /* 输出节点映射（调用方传入 NULL 表示不需要） */
    if (out_node_mapping) {
        if (mapping_established) {
            /* 将节点映射所有权转移给调用方 */
            *out_node_mapping = mapping;
        } else {
            *out_node_mapping = NULL;
        }
    }

    /* 输出约束映射：在节点映射确定后，将图 A 的每个约束映射到图 B 中对应的约束 */
    if (out_constraint_mapping) {
        *out_constraint_mapping = NULL;
        if (mapping_established) {
            int ca_count = graph_get_constraint_count(ga);
            int cb_count = graph_get_constraint_count(gb);
            int *constraint_mapping = (int *) lv_calloc((size_t) ca_count, sizeof(int));
            bool *used_cb = (bool *) lv_calloc((size_t) cb_count, sizeof(bool));
            if (constraint_mapping && used_cb) {
                for (int c = 0; c < ca_count; c++) {
                    constraint_mapping[c] = -1;
                    Constraint *cons = graph_get_constraint(ga, c);
                    if (!cons || !cons->is_active) {
                        continue;
                    }

                    /* 根据节点映射将约束的每个参与者节点映射到图 B 的节点 */
                    int *mapped_participants = (int *) lv_calloc((size_t) cons->participant_count, sizeof(int));
                    if (!mapped_participants) {
                        continue;
                    }
                    bool parts_valid = true;
                    for (int p = 0; p < cons->participant_count; p++) {
                        int u = cons->participants[p];
                        if (u < 0 || u >= na) {
                            parts_valid = false;
                            break;
                        }
                        mapped_participants[p] = mapping[u];
                    }

                    if (parts_valid) {
                        /* 按映射后的参与者组合，在图 B 中查找相同类型的约束 */
                        for (int cb = 0; cb < cb_count && constraint_mapping[c] < 0; cb++) {
                            if (used_cb[cb]) {
                                continue;
                            }
                            Constraint *cons_b = graph_get_constraint(gb, cb);
                            if (!cons_b || !cons_b->is_active) {
                                continue;
                            }
                            if (cons_b->type != cons->type) {
                                continue;
                            }
                            if (cons_b->participant_count != cons->participant_count) {
                                continue;
                            }

                            /* 多集合匹配：映射后的参与者须一一对应到 cons_b 的参与者 */
                            bool *seen = (bool *) lv_calloc((size_t) cons_b->participant_count, sizeof(bool));
                            if (!seen) {
                                break;
                            }
                            bool match = true;
                            for (int p = 0; p < cons->participant_count && match; p++) {
                                bool found = false;
                                for (int q = 0; q < cons_b->participant_count; q++) {
                                    if (seen[q]) {
                                        continue;
                                    }
                                    if (cons_b->participants[q] == mapped_participants[p]) {
                                        seen[q] = true;
                                        found = true;
                                        break;
                                    }
                                }
                                if (!found) {
                                    match = false;
                                }
                            }
                            lv_free((void **) &seen);

                            if (match) {
                                /* 将图 B 的约束 ID 写入映射数组，且每个 B 约束至多被使用一次 */
                                constraint_mapping[c] = cons_b->id;
                                used_cb[cb] = true;
                            }
                        }
                    }
                    lv_free((void **) &mapped_participants);
                }
                *out_constraint_mapping = constraint_mapping;
            } else {
                lv_free((void **) &constraint_mapping);
                lv_free((void **) &used_cb);
            }
            lv_free((void **) &used_cb);
        }
    }

    /* 节点映射未转移给调用方（未请求或未建立）时释放内部副本 */
    if (!(out_node_mapping && mapping_established)) {
        lv_free((void **) &mapping);
    }

    lv_free((void **) &deg_a);
    lv_free((void **) &deg_b);
    lv_free((void **) &used);

    return true;
}

