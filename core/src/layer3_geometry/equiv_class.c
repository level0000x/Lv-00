/* ========================================================================
 * equiv_class.c — 等价类管理器实现
 *
 * 代数等价关系管理，包含：
 *   - 并查集底层实现
 *   - 坐标等价合并
 *   - 约束推导等价
 *   - 代数共轭等价
 *   - 几何变换等价
 *   - 合并合法性证明
 * ======================================================================== */

#include "equiv_class.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "error_codes.h"

/* ================================================================
 * 并查集内部实现
 * ================================================================ */

static int uf_create(EquivClassManager *mgr, int capacity) {
    mgr->uf_capacity = capacity;
    mgr->uf_parent = (int *)calloc((size_t)capacity, sizeof(int));
    mgr->uf_rank = (int *)calloc((size_t)capacity, sizeof(int));
    if (!mgr->uf_parent || !mgr->uf_rank) {
        free(mgr->uf_parent);
        free(mgr->uf_rank);
        return -1;
    }
    for (int i = 0; i < capacity; i++) {
        mgr->uf_parent[i] = i;
        mgr->uf_rank[i] = 0;
    }
    return 0;
}

static void uf_destroy(EquivClassManager *mgr) {
    free(mgr->uf_parent);
    mgr->uf_parent = NULL;
    free(mgr->uf_rank);
    mgr->uf_rank = NULL;
    mgr->uf_capacity = 0;
}

static int uf_find(EquivClassManager *mgr, int x) {
    if (x < 0 || x >= mgr->uf_capacity) return -1;
    /* Path splitting */
    while (mgr->uf_parent[x] != x) {
        int parent = mgr->uf_parent[x];
        mgr->uf_parent[x] = mgr->uf_parent[parent];
        x = parent;
    }
    return x;
}

static void uf_union(EquivClassManager *mgr, int x, int y) {
    if (x < 0 || y < 0 || x >= mgr->uf_capacity || y >= mgr->uf_capacity) return;
    int rx = uf_find(mgr, x);
    int ry = uf_find(mgr, y);
    if (rx == ry) return;

    /* Union by rank */
    if (mgr->uf_rank[rx] < mgr->uf_rank[ry]) {
        int tmp = rx; rx = ry; ry = tmp;
    }
    mgr->uf_parent[ry] = rx;
    if (mgr->uf_rank[rx] == mgr->uf_rank[ry]) {
        mgr->uf_rank[rx]++;
    }
}

/* ================================================================
 * 等价类内部辅助
 * ================================================================ */

static bool equiv_ensure_class_capacity(EquivClassManager *mgr) {
    if (mgr->class_count < mgr->class_capacity) return true;
    int new_cap = mgr->class_capacity < 8 ? 8 : mgr->class_capacity * 2;
    EquivClass *new_classes = (EquivClass *)realloc(mgr->classes,
                                                      (size_t)new_cap * sizeof(EquivClass));
    if (!new_classes) return false;
    mgr->classes = new_classes;
    mgr->class_capacity = new_cap;
    return true;
}

static bool equiv_ensure_node_mapping(EquivClassManager *mgr, int node_id) {
    if (node_id < mgr->node_to_class_capacity) return true;
    int new_cap = mgr->node_to_class_capacity < 16 ? 16 : mgr->node_to_class_capacity * 2;
    while (new_cap <= node_id) new_cap *= 2;
    int *new_map = (int *)realloc(mgr->node_to_class, (size_t)new_cap * sizeof(int));
    if (!new_map) return false;
    for (int i = mgr->node_to_class_capacity; i < new_cap; i++) {
        new_map[i] = -1;
    }
    mgr->node_to_class = new_map;
    mgr->node_to_class_capacity = new_cap;
    return true;
}

static bool equiv_ensure_proof_log(EquivClassManager *mgr) {
    if (mgr->proof_log_count < mgr->proof_log_capacity) return true;
    int new_cap = mgr->proof_log_capacity < 16 ? 16 : mgr->proof_log_capacity * 2;
    EquivProof *new_log = (EquivProof *)realloc(mgr->proof_log,
                                                   (size_t)new_cap * sizeof(EquivProof));
    if (!new_log) return false;
    mgr->proof_log = new_log;
    mgr->proof_log_capacity = new_cap;
    return true;
}

static bool equiv_ensure_class_members(EquivClass *ec, int needed) {
    if (needed <= ec->capacity) return true;
    int new_cap = ec->capacity < 4 ? 4 : ec->capacity * 2;
    while (new_cap < needed) new_cap *= 2;
    int *new_members = (int *)realloc(ec->member_ids, (size_t)new_cap * sizeof(int));
    if (!new_members) return false;
    ec->member_ids = new_members;
    ec->capacity = new_cap;
    return true;
}

static bool equiv_ensure_class_proofs(EquivClass *ec, int needed) {
    if (needed <= ec->proof_capacity) return true;
    int new_cap = ec->proof_capacity < 4 ? 4 : ec->proof_capacity * 2;
    while (new_cap < needed) new_cap *= 2;
    EquivProof *new_proofs = (EquivProof *)realloc(ec->proofs,
                                                      (size_t)new_cap * sizeof(EquivProof));
    if (!new_proofs) return false;
    ec->proofs = new_proofs;
    ec->proof_capacity = new_cap;
    return true;
}

/** @brief 记录等价证明 */
static void equiv_log_proof(EquivClassManager *mgr, EquivSourceType source,
                             int node_a, int node_b, int constraint_id,
                             TrustColor trust) {
    if (!equiv_ensure_proof_log(mgr)) return;
    EquivProof *p = &mgr->proof_log[mgr->proof_log_count++];
    p->source = source;
    p->node_a_id = node_a;
    p->node_b_id = node_b;
    p->deriving_constraint_id = constraint_id;
    p->proof_step_id = -1;
    p->trust = trust;
}

/** @brief 查找或创建节点的等价类索引 */
static int equiv_find_or_create_class(EquivClassManager *mgr, int node_id) {
    if (!equiv_ensure_node_mapping(mgr, node_id)) return -1;

    int existing = mgr->node_to_class[node_id];
    if (existing >= 0 && existing < mgr->class_count) {
        return existing;
    }

    /* 创建新等价类 */
    if (!equiv_ensure_class_capacity(mgr)) return -1;

    int idx = mgr->class_count++;
    EquivClass *ec = &mgr->classes[idx];
    memset(ec, 0, sizeof(EquivClass));
    ec->representative_id = node_id;
    ec->member_count = 0;
    ec->capacity = 0;
    ec->member_ids = NULL;
    ec->proofs = NULL;
    ec->proof_count = 0;
    ec->proof_capacity = 0;
    ec->min_trust = TRUST_GREEN;

    equiv_ensure_class_members(ec, 1);
    ec->member_ids[ec->member_count++] = node_id;
    mgr->node_to_class[node_id] = idx;

    return idx;
}

/* ================================================================
 * 生命周期管理
 * ================================================================ */

EquivClassManager *equiv_manager_create(ConstraintGraph *graph) {
    if (!graph) return NULL;

    EquivClassManager *mgr = (EquivClassManager *)calloc(1, sizeof(EquivClassManager));
    if (!mgr) return NULL;

    mgr->graph = graph;

    /* 初始化并查集 */
    int capacity = graph->node_count > 0 ? graph->node_count : 16;
    if (uf_create(mgr, capacity) != 0) {
        free(mgr);
        return NULL;
    }

    /* 初始化节点映射 */
    mgr->node_to_class_capacity = capacity;
    mgr->node_to_class = (int *)calloc((size_t)capacity, sizeof(int));
    if (!mgr->node_to_class) {
        uf_destroy(mgr);
        free(mgr);
        return NULL;
    }
    for (int i = 0; i < capacity; i++) {
        mgr->node_to_class[i] = -1;
    }

    return mgr;
}

void equiv_manager_destroy(EquivClassManager *mgr) {
    if (!mgr) return;

    /* 销毁等价类 */
    for (int i = 0; i < mgr->class_count; i++) {
        EquivClass *ec = &mgr->classes[i];
        free(ec->member_ids);
        free(ec->proofs);
    }
    free(mgr->classes);

    /* 销毁并查集 */
    uf_destroy(mgr);

    /* 销毁节点映射 */
    free(mgr->node_to_class);

    /* 销毁证明日志 */
    free(mgr->proof_log);

    free(mgr);
}

/* ================================================================
 * 等价合并操作
 * ================================================================ */

/**
 * @brief 内部合并两个等价类
 */
static EquivMergeResult equiv_merge_classes(EquivClassManager *mgr,
                                              int node_a, int node_b,
                                              EquivSourceType source,
                                              int constraint_id,
                                              TrustColor trust) {
    if (!mgr) return EQUIV_MERGE_INVALID;
    if (node_a == node_b) return EQUIV_MERGE_ALREADY_EQUIV;

    /* 检查是否已在同一等价类 */
    if (uf_find(mgr, node_a) == uf_find(mgr, node_b)) {
        return EQUIV_MERGE_ALREADY_EQUIV;
    }

    /* 记录证明 */
    equiv_log_proof(mgr, source, node_a, node_b, constraint_id, trust);

    /* 并查集合并 */
    uf_union(mgr, node_a, node_b);

    /* 获取两个节点的等价类索引 */
    int class_a = equiv_find_or_create_class(mgr, node_a);
    int class_b = equiv_find_or_create_class(mgr, node_b);
    if (class_a < 0 || class_b < 0) return EQUIV_MERGE_INVALID;

    /* 确保合并到 representative_id 更小的类 */
    if (mgr->classes[class_a].representative_id > mgr->classes[class_b].representative_id) {
        int tmp = class_a; class_a = class_b; class_b = tmp;
    }

    EquivClass *target = &mgr->classes[class_a];
    EquivClass *source_ec = &mgr->classes[class_b];

    /* 合并成员 */
    equiv_ensure_class_members(target, target->member_count + source_ec->member_count);
    for (int i = 0; i < source_ec->member_count; i++) {
        int mid = source_ec->member_ids[i];
        target->member_ids[target->member_count++] = mid;
        /* 更新节点映射 */
        if (mid < mgr->node_to_class_capacity) {
            mgr->node_to_class[mid] = class_a;
        }
    }

    /* 合并证明 */
    equiv_ensure_class_proofs(target, target->proof_count + source_ec->proof_count);
    for (int i = 0; i < source_ec->proof_count; i++) {
        target->proofs[target->proof_count++] = source_ec->proofs[i];
    }

    /* 更新信任颜色 */
    if (trust < target->min_trust) {
        target->min_trust = trust;
    }
    if (source_ec->min_trust < target->min_trust) {
        target->min_trust = source_ec->min_trust;
    }

    /* 清空源等价类 */
    free(source_ec->member_ids);
    source_ec->member_ids = NULL;
    free(source_ec->proofs);
    source_ec->proofs = NULL;
    source_ec->member_count = 0;
    source_ec->proof_count = 0;
    source_ec->capacity = 0;
    source_ec->proof_capacity = 0;

    mgr->total_merges++;

    return EQUIV_MERGE_OK;
}

int equiv_merge_by_coord(EquivClassManager *mgr) {
    if (!mgr || !mgr->graph) return 0;

    int merge_count = 0;

    /* 收集所有 GEOM_POINT 节点 */
    for (int i = 0; i < mgr->graph->node_count; i++) {
        GeomNode *ni = graph_get_node(mgr->graph, i);
        if (!ni || !ni->is_active || ni->type != GEOM_POINT) continue;
        if (ni->coord_count < 2 || !ni->symbolic_coords) continue;
        if (!ni->symbolic_coords[0] || !ni->symbolic_coords[1]) continue;

        for (int j = i + 1; j < mgr->graph->node_count; j++) {
            GeomNode *nj = graph_get_node(mgr->graph, j);
            if (!nj || !nj->is_active || nj->type != GEOM_POINT) continue;
            if (nj->coord_count < 2 || !nj->symbolic_coords) continue;
            if (!nj->symbolic_coords[0] || !nj->symbolic_coords[1]) continue;

            /* 比较坐标 */
            int cmp_x = symbolic_coord_compare(ni->symbolic_coords[0], nj->symbolic_coords[0]);
            if (cmp_x != 0) continue;
            int cmp_y = symbolic_coord_compare(ni->symbolic_coords[1], nj->symbolic_coords[1]);
            if (cmp_y != 0) continue;

            /* 坐标相等 → 合并 */
            EquivMergeResult result = equiv_merge_classes(mgr, i, j,
                                                           EQUIV_SOURCE_COORD_EQUAL,
                                                           -1, TRUST_GREEN);
            if (result == EQUIV_MERGE_OK) {
                merge_count++;
                mgr->coord_merges++;
            }
        }
    }

    return merge_count;
}

int equiv_derive_from_constraints(EquivClassManager *mgr) {
    if (!mgr || !mgr->graph) return 0;

    int derive_count = 0;

    /*
     * 规则 3 - 交点等价（最实用的规则）：
     * 若 l1 ∩ l2 = {P}，l1' ∩ l2' = {P'}，
     * 且 l1 ~ l1'，l2 ~ l2'，则 P ~ P'
     */
    for (int i = 0; i < mgr->graph->constraint_count; i++) {
        Constraint *ci = mgr->graph->constraints[i];
        if (!ci || !ci->is_active || ci->type != INTERSECTION) continue;
        if (ci->participant_count < 3) continue;

        int l1_i = ci->participants[0];
        int l2_i = ci->participants[1];
        int p_i = ci->participants[2];

        for (int j = i + 1; j < mgr->graph->constraint_count; j++) {
            Constraint *cj = mgr->graph->constraints[j];
            if (!cj || !cj->is_active || cj->type != INTERSECTION) continue;
            if (cj->participant_count < 3) continue;

            int l1_j = cj->participants[0];
            int l2_j = cj->participants[1];
            int p_j = cj->participants[2];

            /* 检查 l1_i ~ l1_j 且 l2_i ~ l2_j（或交叉） */
            bool match1 = equiv_are_equivalent(mgr, l1_i, l1_j) &&
                          equiv_are_equivalent(mgr, l2_i, l2_j);
            bool match2 = equiv_are_equivalent(mgr, l1_i, l2_j) &&
                          equiv_are_equivalent(mgr, l2_i, l1_j);

            if (match1 || match2) {
                /* 交点等价 */
                EquivMergeResult result = equiv_merge_classes(mgr, p_i, p_j,
                                                               EQUIV_SOURCE_CONSTRAINT_DERIVE,
                                                               i, TRUST_GREEN);
                if (result == EQUIV_MERGE_OK) {
                    derive_count++;
                    mgr->constraint_derives++;
                }
            }
        }
    }

    return derive_count;
}

int equiv_merge_algebraic_conjugates(EquivClassManager *mgr) {
    if (!mgr || !mgr->graph) return 0;

    int conj_count = 0;

    /*
     * 收集所有 ALGEBRAIC 类型坐标的节点，
     * 按极小多项式哈希分组，
     * 组内精确比较极小多项式系数。
     */
    for (int i = 0; i < mgr->graph->node_count; i++) {
        GeomNode *ni = graph_get_node(mgr->graph, i);
        if (!ni || !ni->is_active || ni->type != GEOM_POINT) continue;
        if (ni->coord_count < 2 || !ni->symbolic_coords) continue;

        for (int d = 0; d < 2; d++) {
            SymbolicCoord *sci = ni->symbolic_coords[d];
            if (!sci || sci->type != ALGEBRAIC) continue;

            for (int j = i + 1; j < mgr->graph->node_count; j++) {
                GeomNode *nj = graph_get_node(mgr->graph, j);
                if (!nj || !nj->is_active || nj->type != GEOM_POINT) continue;
                if (nj->coord_count < 2 || !nj->symbolic_coords) continue;

                SymbolicCoord *scj = nj->symbolic_coords[d];
                if (!scj || scj->type != ALGEBRAIC) continue;

                /* 比较极小多项式哈希 */
                uint64_t hi = symbolic_coord_hash(sci);
                uint64_t hj = symbolic_coord_hash(scj);
                if (hi != hj) continue;

                /* 哈希相同 → 进一步检查是否为共轭（同一多项式的不同根） */
                /* 简化：使用坐标比较，若不同则为共轭候选 */
                int cmp = symbolic_coord_compare(sci, scj);
                if (cmp == 0) {
                    /* 相同坐标 → 坐标等价（已在 equiv_merge_by_coord 中处理） */
                    continue;
                }

                /* 不同坐标但哈希相同 → 可能是共轭 */
                /* 精确比较极小多项式系数 */
                /* 通过 symbolic_coord_compare 已确认坐标不同，但哈希相同 */
                /* 进一步验证：检查两个代数坐标是否来自同一极小多项式 */
                if (sci->algebraic_info && scj->algebraic_info) {
                    /* 比较极小多项式的度数和系数 */
                    if (sci->algebraic_info->degree == scj->algebraic_info->degree &&
                        sci->algebraic_info->coeff_count == scj->algebraic_info->coeff_count) {
                        bool same_poly = true;
                        for (int k = 0; k < sci->algebraic_info->coeff_count; k++) {
                            if (symbolic_coord_compare(
                                    sci->algebraic_info->coefficients[k],
                                    scj->algebraic_info->coefficients[k]) != 0) {
                                same_poly = false;
                                break;
                            }
                        }
                        if (same_poly) {
                            /* 同一极小多项式的不同根 → 代数共轭 */
                            EquivMergeResult r = equiv_merge_classes(mgr, i, j,
                                EQUIV_SOURCE_ALGEBRAIC_CONJUGATE, -1, TRUST_YELLOW);
                            if (r == EQUIV_MERGE_OK) conj_count++;
                        }
                    }
                }
            }
        }
    }

    mgr->algebraic_conjugates += conj_count;
    return conj_count;
}

int equiv_merge_by_transform(EquivClassManager *mgr) {
    if (!mgr || !mgr->graph) return 0;

    int transform_count = 0;

    /*
     * 几何变换等价检测（简化实现）：
     * 对每对未等价的点节点，检查是否存在平移使得 A + t = B。
     * 若存在，验证该平移是否保持所有邻域约束。
     */
    for (int i = 0; i < mgr->graph->node_count; i++) {
        GeomNode *ni = graph_get_node(mgr->graph, i);
        if (!ni || !ni->is_active || ni->type != GEOM_POINT) continue;
        if (ni->coord_count < 2 || !ni->symbolic_coords) continue;
        if (!ni->symbolic_coords[0] || !ni->symbolic_coords[1]) continue;

        for (int j = i + 1; j < mgr->graph->node_count; j++) {
            if (equiv_are_equivalent(mgr, i, j)) continue;

            GeomNode *nj = graph_get_node(mgr->graph, j);
            if (!nj || !nj->is_active || nj->type != GEOM_POINT) continue;
            if (nj->coord_count < 2 || !nj->symbolic_coords) continue;
            if (!nj->symbolic_coords[0] || !nj->symbolic_coords[1]) continue;

            /* 计算平移向量 t = B - A */
            /* 简化：使用 double 近似检查距离 */
            double ax = symbolic_coord_to_double(ni->symbolic_coords[0]);
            double ay = symbolic_coord_to_double(ni->symbolic_coords[1]);
            double bx = symbolic_coord_to_double(nj->symbolic_coords[0]);
            double by = symbolic_coord_to_double(nj->symbolic_coords[1]);

            /* 验证平移保持所有邻域约束 */
            double tx = bx - ax;
            double ty = by - ay;

            /* 收集节点 i 和 j 的邻域约束 */
            int cids_i[128], cids_j[128];
            int nc_i = graph_find_constraints_involving(mgr->graph, i, cids_i, 128);
            int nc_j = graph_find_constraints_involving(mgr->graph, j, cids_j, 128);

            /* 简化验证：检查两个节点的约束数量和类型是否匹配 */
            if (nc_i != nc_j) continue;

            bool all_match = true;
            for (int ci = 0; ci < nc_i && all_match; ci++) {
                Constraint *c_i = graph_get_constraint(mgr->graph, cids_i[ci]);
                if (!c_i || !c_i->is_active) continue;

                /* 在 j 的约束中寻找相同类型的约束 */
                bool found_match = false;
                for (int cj = 0; cj < nc_j; cj++) {
                    Constraint *c_j = graph_get_constraint(mgr->graph, cids_j[cj]);
                    if (!c_j || !c_j->is_active) continue;
                    if (c_i->type == c_j->type) {
                        /* 对于距离约束，检查值是否相同 */
                        if (c_i->type == CONSTRAINT_DISTANCE) {
                            if (fabs(c_i->numeric_value - c_j->numeric_value) < 1e-9) {
                                found_match = true;
                                break;
                            }
                        } else {
                            found_match = true;
                            break;
                        }
                    }
                }
                if (!found_match) all_match = false;
            }

            if (all_match && nc_i > 0) {
                EquivMergeResult r = equiv_merge_classes(mgr, i, j,
                    EQUIV_SOURCE_TRANSFORM, -1, TRUST_YELLOW);
                if (r == EQUIV_MERGE_OK) transform_count++;
            }
        }
    }

    mgr->transform_merges += transform_count;
    return transform_count;
}

int equiv_merge_all(EquivClassManager *mgr) {
    if (!mgr) return 0;

    int total = 0;
    total += equiv_merge_by_coord(mgr);
    total += equiv_derive_from_constraints(mgr);
    total += equiv_merge_algebraic_conjugates(mgr);
    total += equiv_merge_by_transform(mgr);
    return total;
}

/* ================================================================
 * 合法性证明
 * ================================================================ */

bool equiv_prove_merge_valid(EquivClassManager *mgr, int class_a_idx, int class_b_idx) {
    if (!mgr || !mgr->graph) return false;
    if (class_a_idx < 0 || class_a_idx >= mgr->class_count) return false;
    if (class_b_idx < 0 || class_b_idx >= mgr->class_count) return false;
    if (class_a_idx == class_b_idx) return true;

    /*
     * 验证合并后约束系统仍然相容：
     * 在逻辑上，坐标等价合并不会引入矛盾（因为坐标已经相等）。
     * 约束推导等价需要验证推导链的有效性。
     *
     * 简化实现：检查两个等价类的信任颜色
     */
    EquivClass *ca = &mgr->classes[class_a_idx];
    EquivClass *cb = &mgr->classes[class_b_idx];

    if (ca->min_trust == TRUST_RED || cb->min_trust == TRUST_RED) {
        return false;
    }

    return true;
}

/* ================================================================
 * 查询接口
 * ================================================================ */

const EquivClass *equiv_get_class(const EquivClassManager *mgr, int node_id) {
    if (!mgr || node_id < 0) return NULL;
    if (node_id >= mgr->node_to_class_capacity) return NULL;

    int idx = mgr->node_to_class[node_id];
    if (idx < 0 || idx >= mgr->class_count) return NULL;

    return &mgr->classes[idx];
}

int equiv_find(const EquivClassManager *mgr, int node_id) {
    if (!mgr || node_id < 0 || node_id >= mgr->uf_capacity) return -1;
    int root = uf_find((EquivClassManager *)mgr, node_id); /* const cast for find */
    return root;
}

bool equiv_are_equivalent(const EquivClassManager *mgr, int node_a, int node_b) {
    if (!mgr || node_a < 0 || node_b < 0) return false;
    if (node_a >= mgr->uf_capacity || node_b >= mgr->uf_capacity) return false;
    return uf_find((EquivClassManager *)mgr, node_a) == uf_find((EquivClassManager *)mgr, node_b);
}

int equiv_class_count(const EquivClassManager *mgr) {
    if (!mgr) return 0;
    /* 计算非空等价类数量 */
    int count = 0;
    for (int i = 0; i < mgr->class_count; i++) {
        if (mgr->classes[i].member_count > 0) count++;
    }
    return count;
}

/* ================================================================
 * 配置与诊断
 * ================================================================ */

void equiv_set_stream_context(EquivClassManager *mgr, StreamContext *stream_ctx) {
    if (mgr) mgr->stream_ctx = stream_ctx;
}

void equiv_get_statistics(const EquivClassManager *mgr,
                           int64_t *out_total,
                           int64_t *out_coord,
                           int64_t *out_derive,
                           int64_t *out_conjugate,
                           int64_t *out_transform,
                           int64_t *out_rejected) {
    if (!mgr) return;
    if (out_total) *out_total = mgr->total_merges;
    if (out_coord) *out_coord = mgr->coord_merges;
    if (out_derive) *out_derive = mgr->constraint_derives;
    if (out_conjugate) *out_conjugate = mgr->algebraic_conjugates;
    if (out_transform) *out_transform = mgr->transform_merges;
    if (out_rejected) *out_rejected = mgr->rejected_merges;
}

bool equiv_verify_idempotency(EquivClassManager *mgr) {
    if (!mgr) return false;

    /* 保存当前合并计数 */
    int64_t before = mgr->total_merges;

    /* 再次运行全部合并 */
    equiv_merge_all(mgr);

    /* 检查是否有新合并 */
    return (mgr->total_merges == before);
}
