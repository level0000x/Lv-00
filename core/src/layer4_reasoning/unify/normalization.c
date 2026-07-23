/**
 * @file normalization.c
 * @brief 图规范化引擎实现
 * @details 实现约束图的规范化处理，包括并查集合并、哈希预分组 O(n) 优化。
 *          保证规范化操作的幂等性，支持重写历史循环检测。
 *
 * 【重构计划】
 *   以下模块适合提取为独立文件，以降低本文件的复杂度：
 *   1. 并查集（Union-Find）操作
 *      - 来源：disjoint_set_find / disjoint_set_union / disjoint_set_init
 *      - 建议文件：src/norm_union_find.c
 *      - 原因：通用数据结构，被 normalization、rewrite、unify 共用
 *   2. 哈希预分组（Hash Pre-grouping）
 *      - 来源：compute_node_hashes / group_by_hash / hash_sort_compare
 *      - 建议文件：src/norm_hash_group.c
 *      - 原因：独立算法模块，可单独测试和优化
 *   3. 规范化核心流程编排
 *      - 来源：normalize_graph / normalize_constraints
 *      - 建议文件：保留在 normalization.c
 *      - 原因：编排层应与算法实现分离
 *
 *   注意：此计划仅记录结构优化方向，实际拆分需配合头文件重构和
 *   单元测试迁移，避免引入回归。
 *
 * @author Lv-00 Project
 * @version 3.0.1
 *
 * @dependencies
 *   - normalization.h       : 图规范化引擎公共接口定义
 *   - constraint_graph.h    : 约束图接口
 *   - lv_internal.h       : 内部数据结构与常量（FNV 哈希、常量）
 *   - stream.h              : 流式事件输出
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "lv/constraint_graph.h"
#include "lv_internal.h"
#include "normalization.h"
#include "stream.h"
#include "stream_context_util.h"

/* ==================== 命名常量（消除魔术数字） ==================== */

/** 哈希/ID的最大容差值 —— 防止分配过大查找表 */
#define NORM_MAX_ID                 1000000

/** scope_key 使用的质数乘数和异或常量 */
#define NORM_SCOPE_PRIME_B      1000003LL
#define NORM_SCOPE_PRIME_D        7919LL

/** 命名空间深度回退值 —— 当 parent_block_id < 0 时使用 */
#define NORM_FALLBACK_NS_DEPTH 999999999LL

/** 默认初始容量 —— normalization_log、graph_normalize 等函数使用 */
#define NORM_DEFAULT_CAPACITY         16

/** 描述缓冲区大小 —— snprintf 用于流式事件的字符串缓冲区 */
#define NORM_DESC_BUFFER_SIZE        128

/** 约束描述缓冲区大小 —— compute_complete_graph_hash 使用 */
#define NORM_HASH_DESC_BUFFER_SIZE   256

/** 哈希计算描述符大小 —— 约束条目 */
#define NORM_CONSTRAINT_DESC_SIZE     64

/* FNV-1a 别名 —— 引用 lv_internal.h 中的统一定义，本文件内统一缩写 */
#define NORM_FNV1A_OFFSET  lv_FNV64_OFFSET_BASIS
#define NORM_FNV1A_PRIME   lv_FNV64_PRIME

/** 端点哈希混合常量 —— golden ratio */
#define NORM_GOLDEN_RATIO_MIX 0x9e3779b97f4a7c15ULL

/**
 * @brief 哈希-索引对 —— 用于 qsort 预分组排序。
 *
 * 在规范化流程中，对节点按哈希值进行 O(n log n) 排序后，
 * 相同哈希的节点聚在一起，可批量调用并查集合并，将
 * 等价类识别从 O(n^2) 降至 O(n log n)。
 */
typedef struct { uint64_t hash; int idx; } HashIdx;

/**
 * @brief 整数升序比较函数（用于 qsort）
 *
 * 使用分支比较替代 (a>b)-(a<b) 模式，避免 INT_MIN/INT_MAX 时溢出。
 */
static int int_compare_asc(const void *a, const void *b) {
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    if (ia < ib) return -1;
    if (ia > ib) return 1;
    return 0;
}

/** HashIdx 按 hash 升序比较函数（用于 qsort）
 *  使用分支比较替代 (a>b)-(a<b) 避免溢出
 */
static int hash_idx_compare_asc(const void *a, const void *b) {
    const uint64_t ha = ((const HashIdx *)a)->hash;
    const uint64_t hb = ((const HashIdx *)b)->hash;
    if (ha < hb) return -1;
    if (ha > hb) return 1;
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Stream context (线程局部)                                          */
/* ------------------------------------------------------------------ */

lv_DECLARE_STREAM_CTX(normalization);

void normalization_set_stream_context(StreamContext *ctx) {
    normalization_stream_ctx = ctx;
}

/* ------------------------------------------------------------------ */
/*  Global merge confirmation callback (线程局部)                      */
/* ------------------------------------------------------------------ */

static lv_THREAD_LOCAL MergeConfirmCallback g_merge_callback = NULL;
static lv_THREAD_LOCAL void *g_merge_user_data = NULL;

void normalization_set_merge_callback(MergeConfirmCallback cb, void *user_data) {
    g_merge_callback = cb;
    g_merge_user_data = user_data;
}

MergeConfirmCallback normalization_get_merge_callback(void) {
    return g_merge_callback;
}

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                   */
/* ------------------------------------------------------------------ */

static long long scope_key(GeomNode *node) {
    long long pb = (node->parent_block_id >= 0) ? node->parent_block_id : NORM_FALLBACK_NS_DEPTH;
    return (pb * NORM_SCOPE_PRIME_B) ^ ((long long)node->namespace_depth * NORM_SCOPE_PRIME_D);
}

/**
 * @brief 判断两个几何节点的坐标数组是否相等
 *
 * 逐个比较两个节点的符号坐标序列。包含对空指针的防护检查。
 * 如果任一节点的坐标数组为 NULL，则：
 *  - 两者均为 NULL 且坐标数为 0 → 视为相等
 *  - 否则 (一个为 NULL 另一个非 NULL 或坐标数不匹配) → 视为不等
 *
 * @param a 第一个几何节点
 * @param b 第二个几何节点
 * @return true 坐标相等，false 不相等
 */
static bool coords_equal(GeomNode *a, GeomNode *b) {
    /* 防护：检查节点有效性 */
    if (!a || !b) return false;

    if (a->coord_count != b->coord_count) return false;

    /* 防护：若坐标数为 0 或符号坐标数组为 NULL，视为相等 */
    if (a->coord_count == 0) return true;

    /* 防护：任一符号坐标数组为 NULL 时不可能相等（除非 coord_count 为 0，已在上方处理） */
    if (!a->symbolic_coords || !b->symbolic_coords) return false;

    for (int i = 0; i < a->coord_count; i++) {
        /* 防护：检查数组中每个坐标指针的有效性 */
        if (!a->symbolic_coords[i] || !b->symbolic_coords[i]) return false;
        if (symbolic_coord_compare(a->symbolic_coords[i],
                                   b->symbolic_coords[i]) != 0) {
            return false;
        }
    }
    return true;
}

/* 用于 qsort 的线段哈希比较函数（复用 HashIdx 类型） */

static int cmp_seg_hash(const void *a, const void *b) {
    uint64_t ha = ((const HashIdx *)a)->hash;
    uint64_t hb = ((const HashIdx *)b)->hash;
    if (ha < hb) return -1;
    if (ha > hb) return  1;
    return 0;
}

/* Union-Find -------------------------------------------------------- */

/**
 * @brief 创建并查集（含 parent 和 rank 数组）
 *
 * 分配并初始化大小为 n 的并查集结构。每个元素初始为独立集合。
 *
 * @param n     元素个数
 * @param[out] out_rank  输出 rank 数组指针
 * @return parent 数组指针，失败返回 NULL（调用者需用 uf_destroy 释放）
 */
static int *uf_create(int n, int **out_rank) {
    int *parent = lv_calloc((size_t)n , sizeof(int));
    int *rank   = lv_calloc((size_t)n, sizeof(int));
    if (!parent || !rank) {
        lv_free((void**)&parent);
        lv_free((void**)&rank);
        return NULL;
    }
    for (int i = 0; i < n; i++) parent[i] = i;
    *out_rank = rank;
    return parent;
}

/**
 * @brief 销毁并查集
 *
 * 释放由 uf_create 创建的 parent 和 rank 数组。
 *
 * @param parent  parent 数组指针（可为 NULL）
 * @param rank    rank 数组指针（可为 NULL）
 */
static void uf_destroy(int *parent, int *rank) {
    lv_free((void**)&parent);
    lv_free((void**)&rank);
}

static int uf_find(int *parent, int x) {
    while (parent[x] != x) {
        parent[x] = parent[parent[x]];   /* path splitting */
        x = parent[x];
    }
    return x;
}

static void uf_union(int *parent, int *rank, int x, int y) {
    int rx = uf_find(parent, x);
    int ry = uf_find(parent, y);
    if (rx == ry) return;
    if (rank[rx] < rank[ry]) {
        parent[rx] = ry;
    } else if (rank[rx] > rank[ry]) {
        parent[ry] = rx;
    } else {
        parent[ry] = rx;
        rank[rx]++;
    }
}

/**
 * @brief 将并查集每个元素重定向到其集合中最小 ID 的代表节点
 *
 * 遍历所有元素，对每个集合找到 ID 最小的成员作为代表，
 * 然后将该集合中所有元素的 parent 直接指向该代表。
 *
 * @param parent      并查集 parent 数组
 * @param nodes       图节点数组（用于获取节点 ID）
 * @param n           节点数量
 * @return true 成功，false 内存分配失败
 */
static bool uf_resolve_to_min_id(int *parent, GeomNode **nodes, int n) {
    int *set_min_idx = lv_calloc((size_t)n , sizeof(int));
    if (!set_min_idx) return false;
    for (int i = 0; i < n; i++) set_min_idx[i] = -1;
    for (int i = 0; i < n; i++) {
        int ri = uf_find(parent, i);
        if (set_min_idx[ri] == -1 ||
            nodes[i]->id < nodes[set_min_idx[ri]]->id) {
            set_min_idx[ri] = i;
        }
    }
    for (int i = 0; i < n; i++) {
        int ri = uf_find(parent, i);
        parent[i] = set_min_idx[ri];
    }
    lv_free((void**)&set_min_idx);
    return true;
}

/**
 * @brief 更新约束图所有约束的参与者 ID，使其指向并查集代表节点
 *
 * @param graph       约束图
 * @param parent      已 resolve 的并查集 parent 数组
 * @param id_to_idx   ID 到索引的查找表
 * @param max_id      最大节点 ID
 * @param nodes       图节点数组
 */
/* 前向声明：ID查找函数（被并查集辅助函数使用） */
static int idx_from_id(int *id_to_idx, int max_id, int id);

/**
 * @brief 更新约束图中所有约束的参与者ID，指向并查集代表节点
 *
 * @param graph        约束图
 * @param parent       已 resolve 的并查集 parent 数组
 * @param id_to_idx    ID 到索引的查找表
 * @param max_id       最大节点 ID
 * @param nodes        图节点数组
 */
static void uf_update_constraint_participants(ConstraintGraph *graph,
                                               int *parent,
                                               int *id_to_idx, int max_id,
                                               GeomNode **nodes) {
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *con = graph->constraints[i];
        for (int j = 0; j < con->participant_count; j++) {
            int pid = con->participants[j];
            int idx = idx_from_id(id_to_idx, max_id, pid);
            if (idx < 0) continue;
            int rep_idx = parent[idx];
            con->participants[j] = nodes[rep_idx]->id;
        }
    }
}

/**
 * @brief 收集并查集中所有非代表节点（即被合并的节点）的索引
 *
 * @param parent  已 resolve 的并查集 parent 数组
 * @param n       节点数量
 * @param[out] out_count  输出合并节点数量
 * @return 合并节点索引数组（调用者需 free），失败返回 NULL
 */
static int *uf_collect_merged(int *parent, int n, int *out_count) {
    int *merged = lv_calloc((size_t)n , sizeof(int));
    if (!merged) { *out_count = 0; return NULL; }
    int total = 0;
    for (int i = 0; i < n; i++) {
        if (parent[i] != i) {
            merged[total++] = i;
        }
    }
    *out_count = total;
    return merged;
}

/**
 * @brief 构建节点ID到索引的查找表
 *
 * @param graph       约束图
 * @param out_max_id  输出最大节点ID的指针（可为NULL）
 * @return 查找表数组（索引从1开始，0表示不存在），失败返回NULL
 */
static int *build_id_to_idx(ConstraintGraph *graph, int *out_max_id) {
    if (graph->node_count == 0) { if (out_max_id) *out_max_id = -1; return NULL; }
    int max_id = 0;
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i]->id > max_id) max_id = graph->nodes[i]->id;
    }
    if (max_id < 0 || max_id > NORM_MAX_ID) {
        if (out_max_id) *out_max_id = -1;
        return NULL;
    }
    if (out_max_id) *out_max_id = max_id;
    size_t map_size = (size_t)max_id + 1;
    if (map_size > SIZE_MAX / sizeof(int)) {
        return NULL;
    }
    int *map = lv_calloc(map_size, sizeof(int));
    if (!map) return NULL;
    for (int i = 0; i < graph->node_count; i++) {
        int nid = graph->nodes[i]->id;
        if (nid >= 0 && nid <= max_id) {
            map[nid] = i + 1;
        }
    }
    return map;
}

static int idx_from_id(int *id_to_idx, int max_id, int id) {
    if (!id_to_idx || id < 0 || id > max_id) return -1;
    int val = id_to_idx[id];
    return val > 0 ? val - 1 : -1;
}

/* ------------------------------------------------------------------ */
/*  NormalizationLog                                                   */
/* ------------------------------------------------------------------ */

NormalizationLog *normalization_log_create(int initial_capacity) {
    NormalizationLog *log = lv_calloc(1, sizeof(NormalizationLog));
    if (!log) return NULL;
    log->capacity = initial_capacity > 0 ? initial_capacity : NORM_DEFAULT_CAPACITY;
    log->count = 0;
    log->entries = lv_calloc((size_t)log->capacity , sizeof(NormalizationLogEntry));
    if (!log->entries) {
        lv_free((void**)&log);
        return NULL;
    }
    return log;
}

void normalization_log_destroy(NormalizationLog *log) {
    if (log) {
        lv_free((void**)&log->entries);
        lv_free((void**)&log);
    }
}

void normalization_log_record(NormalizationLog *log, int old_id, int new_id,
                               bool auto_merged) {
    if (!log) return;
    if (log->count >= log->capacity) {
        if (log->capacity > INT_MAX / 2) return;
        int new_cap = log->capacity * 2;
        NormalizationLogEntry *new_entries =
            lv_realloc(log->entries, (size_t)new_cap * sizeof(NormalizationLogEntry));
        if (!new_entries) return;
        log->entries = new_entries;
        log->capacity = new_cap;
    }
    log->entries[log->count].old_id = old_id;
    log->entries[log->count].new_id = new_id;
    log->entries[log->count].auto_merged = auto_merged;
    log->count++;
}

/* ------------------------------------------------------------------ */
/*  apply_uf_merges - 应用并查集合并结果到约束图                        */
/* ------------------------------------------------------------------ */

/**
 * @brief 应用并查集合并结果到约束图
 *
 * 将并查集合并结果应用到约束图中：重定向到最小 ID 代表、更新约束参与者、
 * 收集被合并节点、记录日志并发送流式事件、逆序移除已合并节点。
 * 该函数负责清理所有传入资源（id_to_idx、parent、rank），调用者无需重复释放。
 *
 * @param graph        约束图
 * @param parent       已构建的并查集 parent 数组
 * @param rank         并查集 rank 数组
 * @param id_to_idx    ID 到索引的查找表
 * @param max_id       最大节点 ID
 * @param n            节点数量
 * @param log          规范化日志（可为 NULL）
 * @param merge_type   合并类型名称（用于日志和流式事件描述）
 * @param stream_phase 流式事件阶段号（2 = 线段, 3 = 区域）
 * @return int 成功返回合并节点数量，失败返回 -1
 */
static int apply_uf_merges(ConstraintGraph *graph, int *parent, int *rank,
                           int *id_to_idx, int max_id, int n,
                           NormalizationLog *log, const char *merge_type,
                           int stream_phase)
{
    /* 将每个集合重定向到最小 ID 的代表节点 */
    if (!uf_resolve_to_min_id(parent, graph->nodes, n)) {
        lv_free((void**)&id_to_idx);
        uf_destroy(parent, rank);
        return -1;
    }

    /* 更新约束参与者 */
    uf_update_constraint_participants(graph, parent, id_to_idx, max_id, graph->nodes);

    /* 收集被合并的节点索引 */
    int merged_total = 0;
    int *merged_indices = uf_collect_merged(parent, n, &merged_total);
    if (!merged_indices) {
        lv_free((void**)&id_to_idx);
        uf_destroy(parent, rank);
        return -1;
    }

    /* 记录合并到规范化日志 */
    for (int i = 0; i < merged_total; i++) {
        int midx = merged_indices[i];
        int old_id = graph->nodes[midx]->id;
        int rep_idx = parent[midx];
        int new_id = graph->nodes[rep_idx]->id;
        normalization_log_record(log, old_id, new_id, true);

        if (normalization_stream_ctx) {
            char desc[NORM_DESC_BUFFER_SIZE];
            snprintf(desc, sizeof(desc), "%s: 节点 %d → %d", merge_type, old_id, new_id);
            stream_emit_simple(normalization_stream_ctx, STREAM_EVENT_NORMALIZE_MERGE,
                               desc, stream_phase);
        }
    }

    /* 逆序移除已合并的节点 */
    for (int i = merged_total - 1; i >= 0; i--) {
        int midx = merged_indices[i];
        graph_remove_node(graph, graph->nodes[midx]->id);
    }

    lv_free((void**)&merged_indices);
    lv_free((void**)&id_to_idx);
    uf_destroy(parent, rank);
    return merged_total;
}

/* ------------------------------------------------------------------ */
/*  merge_line_segments                                                */
/* ------------------------------------------------------------------ */

/**
 * @brief 合并线段
 *
 * 查找具有相同端点的线段对（按坐标值比较）。
 * 第一阶段点合并后，具有相同端点的线段将通过 coords_equal 返回 true。
 */
int merge_line_segments(ConstraintGraph *graph, NormalizationLog *log) {
    if (graph->node_count < 2) return 0;

    int n = graph->node_count;
    int max_id = -1; int *id_to_idx = build_id_to_idx(graph, &max_id);
    int *rank;
    int *parent = uf_create(n, &rank);
    if (!id_to_idx || !parent) {
        lv_free((void**)&id_to_idx); uf_destroy(parent, rank);
        return 0;
    }

    /* 查找具有相同端点的 LINE_SEGMENT 对（无序比较）*/
    for (int i = 0; i < n; i++) {
        GeomNode *ni = graph->nodes[i];
        if (ni->type != GEOM_LINE_SEGMENT || ni->coord_count < 2) continue;
        for (int j = i + 1; j < n; j++) {
            GeomNode *nj = graph->nodes[j];
            if (nj->type != GEOM_LINE_SEGMENT || nj->coord_count < 2) continue;
            {
                bool fwd_match = (symbolic_coord_compare(ni->symbolic_coords[0], nj->symbolic_coords[0]) == 0 &&
                                  symbolic_coord_compare(ni->symbolic_coords[1], nj->symbolic_coords[1]) == 0);
                bool rev_match = (symbolic_coord_compare(ni->symbolic_coords[0], nj->symbolic_coords[1]) == 0 &&
                                  symbolic_coord_compare(ni->symbolic_coords[1], nj->symbolic_coords[0]) == 0);
                if (!fwd_match && !rev_match) continue;
            }
            if (scope_key(ni) != scope_key(nj)) continue;
            uf_union(parent, rank, i, j);
        }
    }

    /* 应用并查集合并结果 */
    {
        int result = apply_uf_merges(graph, parent, rank, id_to_idx, max_id, n,
                                     log, "线段合并", 2);
        return result >= 0 ? result : 0;
    }
}

/* ------------------------------------------------------------------ */
/*  merge_regions                                                      */
/* ------------------------------------------------------------------ */

/* 辅助函数：比较两个已排序的整数数组是否相等 */
static bool int_arrays_equal(const int *a, const int *b, int count) {
    for (int i = 0; i < count; i++) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

/**
 * @brief 合并区域
 *
 * 查找具有相同边界线段序列的区域对。
 * 线段合并后，如果两个区域的边界线段序列相同，则合并它们。
 */
int merge_regions(ConstraintGraph *graph, NormalizationLog *log) {
    if (graph->node_count < 2) return 0;

    int n = graph->node_count;
    int max_id = -1; int *id_to_idx = build_id_to_idx(graph, &max_id);
    int *rank;
    int *parent = uf_create(n, &rank);
    if (!id_to_idx || !parent) {
        lv_free((void**)&id_to_idx); uf_destroy(parent, rank);
        return 0;
    }

    /* 查找具有相同边界线段序列的 REGION 对 */
    for (int i = 0; i < n; i++) {
        GeomNode *ni = graph->nodes[i];
        if (ni->type != GEOM_REGION) continue;
        for (int j = i + 1; j < n; j++) {
            GeomNode *nj = graph->nodes[j];
            if (nj->type != GEOM_REGION) continue;
            if (ni->data.region.segment_count != nj->data.region.segment_count)
                continue;
            int seg_count = ni->data.region.segment_count;
            if (seg_count == 0) continue;
            if (scope_key(ni) != scope_key(nj)) continue;
            /* 比较边界线段 ID 序列：尝试所有旋转和翻转组合 */
            int *ids_a = lv_calloc((size_t)seg_count , sizeof(int));
            int *ids_b = lv_calloc((size_t)seg_count, sizeof(int));
            int *ids_b_rev = lv_calloc((size_t)seg_count, sizeof(int));
            for (int k = 0; k < seg_count; k++) {
                ids_a[k] = ni->data.region.boundary_segments[k]->id;
                ids_b[k] = nj->data.region.boundary_segments[k]->id;
                ids_b_rev[k] = ids_b[seg_count - 1 - k];
            }

            bool same = false;
            for (int rot = 0; rot < seg_count && !same; rot++) {
                bool fwd = true;
                for (int k = 0; k < seg_count; k++) {
                    if (ids_a[k] != ids_b[(k + rot) % seg_count]) {
                        fwd = false; break;
                    }
                }
                if (fwd) { same = true; break; }

                bool rev = true;
                for (int k = 0; k < seg_count; k++) {
                    if (ids_a[k] != ids_b_rev[(k + rot) % seg_count]) {
                        rev = false; break;
                    }
                }
                if (rev) { same = true; break; }
            }

            lv_free((void**)&ids_a);
            lv_free((void**)&ids_b);
            lv_free((void**)&ids_b_rev);
            if (!same) continue;
            uf_union(parent, rank, i, j);
        }
    }

    /* 应用并查集合并结果 */
    {
        int result = apply_uf_merges(graph, parent, rank, id_to_idx, max_id, n,
                                     log, "区域合并", 3);
        return result >= 0 ? result : 0;
    }
}

/* ------------------------------------------------------------------ */
/*  点合并的哈希预分组辅助函数（O(n)）                                */
/* ------------------------------------------------------------------ */

/**
 * @brief 计算 POINT 节点的坐标哈希值
 *
 * 使用 FNV-1a 算法计算坐标哈希，作为分桶的快速指纹。
 * 精确相等性仍需 coords_equal() 确认。
 */
static uint64_t point_coord_hash(GeomNode *node) {
    if (!node || node->coord_count == 0) return 0;
    uint64_t h = lv_FNV64_OFFSET_BASIS;  /* FNV-1a offset basis */
    for (int i = 0; i < node->coord_count; i++) {
        uint64_t ch = symbolic_coord_hash(node->symbolic_coords[i]);
        h ^= ch;
        h *= lv_FNV64_PRIME;
    }
    return h;
}

/**
 * @brief 计算 LINE_SEGMENT 节点的无序端点哈希
 *
 * 两个具有端点 {A,B} 和 {C,D} 的线段，如果 {A,B}=={C,D} 或 {A,B}=={D,C}，
 * 则具有相同的哈希值（与顺序无关）。
 * 作为分桶的快速指纹，精确相等性仍需 coords_equal() 确认。
 */
static uint64_t segment_endpoint_hash(GeomNode *node) {
    if (!node || node->type != GEOM_LINE_SEGMENT || node->coord_count < 2) return 0;
    uint64_t h1 = symbolic_coord_hash(node->symbolic_coords[0]);
    uint64_t h2 = symbolic_coord_hash(node->symbolic_coords[1]);
    /* 使用交换哈希：h1 ^ h2（与顺序无关） */
    uint64_t h = h1 ^ h2;
    h ^= NORM_GOLDEN_RATIO_MIX;
    h *= lv_FNV64_PRIME;
    return h;
}

/* ------------------------------------------------------------------ */
/*  查找合并候选节点对（使用哈希预分组）                               */
/* ------------------------------------------------------------------ */

/**
 * @brief 查找合并候选节点对
 *
 * 使用哈希预分组优化，将时间复杂度从 O(n^2) 降低到接近 O(n)。
 *
 * 算法：
 * 1. 计算每个节点的坐标哈希值
 * 2. 按哈希值排序
 * 3. 将具有相同哈希的节点分组
 * 4. 仅在每组内进行比较
 */
NodeMergeCandidate *find_merge_candidates(const ConstraintGraph *graph, int *out_count) {
    *out_count = 0;

    /* 最坏情况：所有节点对都是候选，乘以3覆盖三个阶段（点、线段、区域）。
     * 使用 size_t 类型进行计算以防止整数溢出。
     * 如果计算结果超出 int 范围，则限制为 INT_MAX / 2 作为安全上限。 */
    size_t nc = (size_t)graph->node_count;
    size_t max_candidates_sz = nc > 1 ? (nc * (nc - 1) / 2) * 3 : 0;
    int max_candidates;
    if (max_candidates_sz > (size_t)INT_MAX / 2) {
        /* 溢出保护：节点数过多时限制候选数量上限 */
        max_candidates = INT_MAX / 2;
    } else {
        max_candidates = (int)max_candidates_sz;
    }
    if (max_candidates == 0) return NULL;

    NodeMergeCandidate *candidates = lv_calloc((size_t)max_candidates , sizeof(NodeMergeCandidate));
    if (!candidates) return NULL;

    /* 第一阶段：点候选（坐标相等）—— 使用哈希预分组 */
    {
        /* 步骤1：收集所有 POINT 节点索引并计算哈希 */
        int point_count = 0;
        int *point_indices = lv_calloc((size_t)graph->node_count, sizeof(int));
        uint64_t *point_hashes = lv_calloc((size_t)graph->node_count , sizeof(uint64_t));
        if (point_indices && point_hashes) {
            for (int i = 0; i < graph->node_count; i++) {
                GeomNode *ni = graph->nodes[i];
                if (ni->type == GEOM_POINT && ni->coord_count > 0) {
                    point_indices[point_count] = i;
                    point_hashes[point_count] = point_coord_hash(ni);
                    point_count++;
                }
            }
        }

        if (point_count > 1 && point_indices && point_hashes) {
            /* 步骤2：按哈希值排序点索引 */
            HashIdx *pairs = lv_calloc((size_t)point_count , sizeof(HashIdx));
            if (pairs) {
                for (int i = 0; i < point_count; i++) {
                    pairs[i].hash = point_hashes[i];
                    pairs[i].idx  = point_indices[i];
                }
                qsort(pairs, point_count, sizeof(HashIdx), hash_idx_compare_asc);

                /* 步骤3&4：遍历排序后的数组，按哈希分组，在组内使用 coords_equal() 比较 */
                int group_start = 0;
                while (group_start < point_count) {
                    uint64_t group_hash = pairs[group_start].hash;
                    int group_end = group_start + 1;
                    while (group_end < point_count &&
                           pairs[group_end].hash == group_hash) {
                        group_end++;
                    }
                    /* 当前分组为 [group_start, group_end)，仅在该分组内进行逐对比较 */
                    for (int a = group_start; a < group_end; a++) {
                        for (int b = a + 1; b < group_end; b++) {
                            GeomNode *ni = graph->nodes[pairs[a].idx];
                            GeomNode *nj = graph->nodes[pairs[b].idx];
                            if (!coords_equal(ni, nj)) continue;
                            NodeMergeCandidate *c = &candidates[*out_count];
                            c->node_a_id = ni->id;
                            c->node_b_id = nj->id;
                            c->coord_a  = ni->symbolic_coords[0];
                            c->coord_b  = nj->symbolic_coords[0];
                            c->scope_a  = scope_key(ni);
                            c->scope_b  = scope_key(nj);
                            (*out_count)++;
                        }
                    }
                    group_start = group_end;
                }

                lv_free((void**)&pairs);
            }
        }

        lv_free((void**)&point_indices);
        lv_free((void**)&point_hashes);
    }

    /* 第二阶段：线段候选（点合并后端点相同）—— 使用哈希预分组 */
    {
        /* 步骤1：收集所有 LINE_SEGMENT 节点索引和端点哈希 */
        int seg_count = 0;
        int *seg_indices = lv_calloc((size_t)graph->node_count, sizeof(int));
        uint64_t *seg_hashes = lv_calloc((size_t)graph->node_count , sizeof(uint64_t));
        if (seg_indices && seg_hashes) {
            for (int i = 0; i < graph->node_count; i++) {
                GeomNode *ni = graph->nodes[i];
                if (ni->type == GEOM_LINE_SEGMENT && ni->coord_count >= 2) {
                    seg_indices[seg_count] = i;
                    seg_hashes[seg_count] = segment_endpoint_hash(ni);
                    seg_count++;
                }
            }
        }

        if (seg_count > 1 && seg_indices && seg_hashes) {
            /* 使用 qsort 按哈希值排序线段索引 */
            HashIdx *seg_pairs = lv_calloc((size_t)seg_count , sizeof(HashIdx));
            if (seg_pairs) {
                for (int i = 0; i < seg_count; i++) {
                    seg_pairs[i].hash = seg_hashes[i];
                    seg_pairs[i].idx  = seg_indices[i];
                }
                qsort(seg_pairs, (size_t)seg_count, sizeof(HashIdx), cmp_seg_hash);
                for (int i = 0; i < seg_count; i++) {
                    seg_hashes[i]  = seg_pairs[i].hash;
                    seg_indices[i] = seg_pairs[i].idx;
                }
                lv_free((void**)&seg_pairs);
            }

            /* 步骤3：处理具有相同哈希的每个组 */
            int i = 0;
            while (i < seg_count) {
                int group_start = i;
                uint64_t current_hash = seg_hashes[i];
                while (i < seg_count && seg_hashes[i] == current_hash) i++;
                int group_end = i;
                for (int gi = group_start; gi < group_end; gi++) {
                    GeomNode *ni = graph->nodes[seg_indices[gi]];
                    for (int gj = gi + 1; gj < group_end; gj++) {
                        GeomNode *nj = graph->nodes[seg_indices[gj]];
                        if (!coords_equal(ni, nj)) continue;
                        NodeMergeCandidate *c = &candidates[*out_count];
                        c->node_a_id = ni->id;
                        c->node_b_id = nj->id;
                        c->coord_a  = ni->symbolic_coords[0];
                        c->coord_b  = nj->symbolic_coords[0];
                        c->scope_a  = scope_key(ni);
                        c->scope_b  = scope_key(nj);
                        (*out_count)++;
                    }
                }
            }
        }
        lv_free((void**)&seg_indices);
        lv_free((void**)&seg_hashes);
    }

    /* 第三阶段：区域候选（边界线段集合相同） */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *ni = graph->nodes[i];
        if (ni->type != GEOM_REGION) continue;
        for (int j = i + 1; j < graph->node_count; j++) {
            GeomNode *nj = graph->nodes[j];
            if (nj->type != GEOM_REGION) continue;
            if (ni->data.region.segment_count != nj->data.region.segment_count) continue;
            if (ni->data.region.segment_count == 0) continue;
            /* 比较排序后的边界线段 ID 集合 */
            int seg_count = ni->data.region.segment_count;
            int *ids_a = lv_calloc((size_t)seg_count , sizeof(int));
            int *ids_b = lv_calloc((size_t)seg_count, sizeof(int));
            for (int k = 0; k < seg_count; k++) {
                ids_a[k] = ni->data.region.boundary_segments[k]->id;
                ids_b[k] = nj->data.region.boundary_segments[k]->id;
            }
            qsort(ids_a, seg_count, sizeof(int), int_compare_asc);
            qsort(ids_b, seg_count, sizeof(int), int_compare_asc);
            bool same = true;
            for (int k = 0; k < seg_count; k++) {
                if (ids_a[k] != ids_b[k]) { same = false; break; }
            }
            lv_free((void**)&ids_a);
            lv_free((void**)&ids_b);
            if (!same) continue;
            NodeMergeCandidate *c = &candidates[*out_count];
            c->node_a_id = ni->id;
            c->node_b_id = nj->id;
            c->coord_a  = NULL;
            c->coord_b  = NULL;
            c->scope_a  = scope_key(ni);
            c->scope_b  = scope_key(nj);
            (*out_count)++;
        }
    }

    /* 流式事件：候选扫描完成 */
    if (normalization_stream_ctx) {
        char desc[128];
        snprintf(desc, sizeof(desc), "合并候选扫描完成: 发现 %d 对候选", *out_count);
        stream_emit_simple(normalization_stream_ctx, STREAM_EVENT_INFO, desc, 0);
    }

    return candidates;
}

/* ================================================================== */
/*  merge_candidates_destroy - 释放合并候选列表                        */
/* ================================================================== */

/**
 * @brief 释放由 find_merge_candidates() 分配的合并候选数组。
 * @param candidates 候选数组（可为 NULL）
 * @param count 候选数量（当前实现仅用于 API 签名一致性，
 *              释放操作不依赖此参数）
 *
 * @note 此函数仅释放调用者持有的候选数组内存，不修改候选指向的图数据。
 */
void merge_candidates_destroy(NodeMergeCandidate *candidates, int count) {
    lv_UNUSED(count);
    lv_free((void**)&candidates);
}

/* ------------------------------------------------------------------ */
/*  apply_merges                                                       */
/* ------------------------------------------------------------------ */

int apply_merges(ConstraintGraph *graph, NodeMergeCandidate *candidates,
                 int count, bool *user_confirmed) {
    if (count == 0 || graph->node_count == 0) return 0;

    int n = graph->node_count;

    /* 构建节点ID到索引的查找表 */
    int max_id = -1; int *id_to_idx = build_id_to_idx(graph, &max_id);
    int *rank;
    int *parent = uf_create(n, &rank);
    if (!id_to_idx || !parent) {
        lv_free((void**)&id_to_idx); uf_destroy(parent, rank);
        return 0;
    }

    /* 合并相同作用域的候选项；对于跨作用域的候选项，如果注册了合并确认回调函数，
       则调用该回调函数进行判断。
       - 如果回调已设置：调用回调并遵循用户的决定。
       - 如果回调未设置：默认不合并（保守策略）。 */
    for (int i = 0; i < count; i++) {
        NodeMergeCandidate *c = &candidates[i];
        int idx_a = idx_from_id(id_to_idx, max_id, c->node_a_id);
        int idx_b = idx_from_id(id_to_idx, max_id, c->node_b_id);
        if (idx_a < 0 || idx_b < 0) continue;
        if (c->scope_a != c->scope_b) {
            /* 跨作用域候选：需要通过回调函数确认合并 */
            if (g_merge_callback) {
                GeomNode *node_a = graph->nodes[idx_a];
                GeomNode *node_b = graph->nodes[idx_b];
                bool approved = g_merge_callback(
                    c->node_a_id, c->node_b_id,
                    node_a->namespace_depth, node_b->namespace_depth,
                    node_a->parent_block_id, node_b->parent_block_id,
                    g_merge_user_data);
                if (approved) {
                    uf_union(parent, rank, idx_a, idx_b);
                }
                /* 将用户确认结果写入 user_confirmed（如果指针非 NULL） */
                if (user_confirmed) {
                    *user_confirmed = approved;
                }
            } else {
                /* 未注册回调函数：保守策略，不合并跨作用域节点 */
                if (user_confirmed) {
                    *user_confirmed = false;
                }
            }
            continue;
        }
        uf_union(parent, rank, idx_a, idx_b);
    }

    /* 使用公共函数：将每个集合重定向到最小 ID 的代表节点 */
    if (!uf_resolve_to_min_id(parent, graph->nodes, n)) {
        lv_free((void**)&id_to_idx); uf_destroy(parent, rank);
        return 0;
    }

    /* 使用公共函数：更新约束参与者 */
    uf_update_constraint_participants(graph, parent, id_to_idx, max_id, graph->nodes);

    /* 使用公共函数：收集被合并的节点索引 */
    int merged_total = 0;
    int *merged_indices = uf_collect_merged(parent, n, &merged_total);
    if (!merged_indices) {
        lv_free((void**)&id_to_idx); uf_destroy(parent, rank);
        return 0;
    }

    /* 流式事件：批量合并进度 */
    if (normalization_stream_ctx) {
        char desc[128];
        snprintf(desc, sizeof(desc), "批量合并: %d 个节点被合并", merged_total);
        stream_emit_simple(normalization_stream_ctx, STREAM_EVENT_PROGRESS, desc, 0);
    }

    /* 从图中移除已合并的节点（逆序遍历，以确保索引有效） */
    for (int i = merged_total - 1; i >= 0; i--) {
        int midx = merged_indices[i];
        int node_id = graph->nodes[midx]->id;
        graph_remove_node(graph, node_id);
    }

    /* 移除节点后重建 id_to_idx，用于填充结果 */
    lv_free((void**)&id_to_idx);
    max_id = -1; id_to_idx = build_id_to_idx(graph, &max_id);

    int merges = merged_total;

    lv_free((void**)&merged_indices);
    lv_free((void**)&id_to_idx);
    uf_destroy(parent, rank);
    return merges;
}

/* ------------------------------------------------------------------ */
/*  graph_topological_sort_stable                                      */
/* ------------------------------------------------------------------ */

/* 约束规范比较键（用于排序） */
static int constraint_canonical_compare(const void *a, const void *b) {
    Constraint *ca = *(Constraint *const *)a;
    Constraint *cb = *(Constraint *const *)b;

    /* 首先按类型比较 */
    if (ca->type < cb->type) return -1;
    if (ca->type > cb->type) return  1;

    /* 然后按排序后的参与者ID序列比较 */
    int min_pc = ca->participant_count < cb->participant_count
                 ? ca->participant_count : cb->participant_count;
    for (int i = 0; i < min_pc; i++) {
        if (ca->participants[i] < cb->participants[i]) return -1;
        if (ca->participants[i] > cb->participants[i]) return  1;
    }
    /* 较短序列排在前面 */
    if (ca->participant_count < cb->participant_count) return -1;
    if (ca->participant_count > cb->participant_count) return  1;

    /* 以约束ID作为决胜条件，确保完全确定性 */
    if (ca->id < cb->id) return -1;
    if (ca->id > cb->id) return  1;
    return 0;
}

void graph_topological_sort_stable(ConstraintGraph *graph) {
    /* 步骤1：对每个约束的参与者按节点ID升序排序（插入排序，小数组稳定） */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *con = graph->constraints[i];
        for (int j = 1; j < con->participant_count; j++) {
            int key = con->participants[j];
            int k = j - 1;
            while (k >= 0 && con->participants[k] > key) {
                con->participants[k + 1] = con->participants[k];
                k--;
            }
            con->participants[k + 1] = key;
        }
    }

    /* 步骤2：按规范键排序约束数组：类型、排序后的参与者ID、约束ID */
    if (graph->constraint_count > 1) {
        qsort(graph->constraints, (size_t)graph->constraint_count,
              sizeof(Constraint *), constraint_canonical_compare);
    }

    /* 步骤3：按节点ID排序节点数组，确保确定性迭代顺序 */
    if (graph->node_count > 1) {
        for (int i = 1; i < graph->node_count; i++) {
            GeomNode *key = graph->nodes[i];
            int k = i - 1;
            while (k >= 0 && graph->nodes[k]->id > key->id) {
                graph->nodes[k + 1] = graph->nodes[k];
                k--;
            }
            graph->nodes[k + 1] = key;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  图规范化（多阶段流水线）                                           */
/* ------------------------------------------------------------------ */

/* 辅助函数：将合并信息记录到结果中。
 * 在写入数组前检查 merged_count 是否超出预分配容量 merged_capacity，
 * 防止缓冲区越界访问。如果超出容量则跳过此次记录。 */
static void record_merge(NormalizationResult *result, int original_id,
                         int representative_id) {
    if (!result) return;
    /* 边界检查：确保 merged_count 未超出预分配的数组容量 */
    if (result->merged_count >= result->merged_capacity) {
        return;  /* 容量已满，跳过此次记录以防止越界写入 */
    }
    int idx = result->merged_count;
    result->original_ids[idx]      = original_id;
    result->representative_ids[idx] = representative_id;
    result->merged_node_ids[idx]    = representative_id;
    result->merged_count++;
}

/* 执行一个合并阶段：查找候选、应用合并、记录结果。
   返回实际执行的合并次数。 */
static int normalize_phase(ConstraintGraph *graph, NormalizationResult *result,
                           bool *user_confirmed) {
    int candidate_count = 0;
    NodeMergeCandidate *candidates = find_merge_candidates(graph, &candidate_count);
    if (candidate_count == 0) {
        lv_free((void**)&candidates);
        return 0;
    }

    /* 快照合并前的节点ID，用于记录映射关系 */
    int n = graph->node_count;
    int *ids_before = lv_calloc((size_t)n , sizeof(int));
    for (int i = 0; i < n; i++) {
        ids_before[i] = graph->nodes[i]->id;
    }

    int merges = apply_merges(graph, candidates, candidate_count, user_confirmed);

    /* 构建映射：对合并前存在的每个节点，查找其当前映射。
       如果节点不再存在，说明它已被合并。 */
    if (merges > 0) {
        int max_id = -1; int *id_to_idx = build_id_to_idx(graph, &max_id);
        for (int i = 0; i < n; i++) {
            int old_id = ids_before[i];
            int new_idx = idx_from_id(id_to_idx, max_id, old_id);
            if (new_idx < 0) {
                /* 此节点已被移除——它被合并到了某个代表节点。
                   通过在约束中查找引用此旧ID的节点，或通过扫描相同坐标来找到代表节点。 */
                /* 更简单的方法：代表节点是具有相同符号坐标且ID最小的节点。
                   从候选列表中重新推导。 */
                int rep_id = -1;
                for (int c = 0; c < candidate_count; c++) {
                    if (candidates[c].node_a_id == old_id) {
                        rep_id = candidates[c].node_b_id;
                        /* 选择较小的ID */
                        if (rep_id > old_id) rep_id = old_id;
                        break;
                    }
                    if (candidates[c].node_b_id == old_id) {
                        rep_id = candidates[c].node_a_id;
                        if (rep_id > old_id) rep_id = old_id;
                        break;
                    }
                }
                /* 跟踪传递合并链，找到最终代表节点 */
                if (rep_id >= 0) {
                    int safety = 0;
                    while (safety < candidate_count + 1) {
                        bool found_better = false;
                        for (int c = 0; c < candidate_count; c++) {
                            if (candidates[c].node_a_id == rep_id) {
                                int new_rep = candidates[c].node_b_id;
                                if (new_rep < rep_id) {
                                    rep_id = new_rep;
                                    found_better = true;
                                    break;
                                }
                            }
                            if (candidates[c].node_b_id == rep_id) {
                                int new_rep = candidates[c].node_a_id;
                                if (new_rep < rep_id) {
                                    rep_id = new_rep;
                                    found_better = true;
                                    break;
                                }
                            }
                        }
                        if (!found_better) break;
                        safety++;
                    }
                }
                if (rep_id >= 0 && rep_id != old_id) {
                    record_merge(result, old_id, rep_id);

                    /* 流式输出: 点合并事件 (Phase 1) */
                    if (normalization_stream_ctx) {
                        char desc[NORM_DESC_BUFFER_SIZE];
                        snprintf(desc, sizeof(desc), "点合并: 节点 %d → %d", old_id, rep_id);
                        stream_emit_simple(normalization_stream_ctx, STREAM_EVENT_NORMALIZE_MERGE,
                                           desc, 1);  /* phase 1 = point */
                    }
                }
            }
        }
        lv_free((void**)&id_to_idx);
    }

    lv_free((void**)&ids_before);
    lv_free((void**)&candidates);
    return merges;
}

NormalizationResult *graph_normalize(ConstraintGraph *graph, bool scope_aware) {
    NormalizationResult *result = lv_calloc(1, sizeof(NormalizationResult));
    if (!result) return NULL;
    result->user_confirmed = true;

    int total_nodes = graph->node_count;
    /* 预分配结果数组（按最坏情况分配） */
    result->original_ids      = lv_calloc((size_t)total_nodes , sizeof(int));
    result->representative_ids = lv_calloc((size_t)total_nodes, sizeof(int));
    result->merged_node_ids    = lv_calloc((size_t)total_nodes , sizeof(int));
    result->merged_capacity    = total_nodes;  /* 记录预分配容量，供边界检查使用 */
    result->log = normalization_log_create(total_nodes > 0 ? total_nodes : NORM_DEFAULT_CAPACITY);
    if (!result->original_ids || !result->representative_ids ||
        !result->merged_node_ids || !result->log) {
        normalization_result_destroy(result);
        return NULL;
    }

    /* 流式输出: 规范化开始 */
    if (normalization_stream_ctx) {
        char desc[NORM_DESC_BUFFER_SIZE];
        snprintf(desc, sizeof(desc), "规范化开始: %d 个节点, %d 个约束",
                 graph->node_count, graph->constraint_count);
        stream_emit_simple(normalization_stream_ctx, STREAM_EVENT_NORMALIZE_START,
                           desc, 0);
    }

    /* 阶段1：点合并（使用 find_merge_candidates + apply_merges） */
    int phase1 = normalize_phase(graph, result, &result->user_confirmed);

    /* 阶段2：线段合并——阶段1点合并后，检查所有 LINE_SEGMENT 节点。
       如果两条线段的端点现在相同（点合并后节点ID一致），则合并它们。
       保留ID较小的那个，更新所有约束。 */
    int phase2 = merge_line_segments(graph, result->log);
    /* 将阶段2的合并记录到结果数组中 */
    if (phase2 > 0 && result->log) {
        /* 阶段2的日志条目从阶段1条目之后开始 */
        int log_start = result->log->count - phase2;
        /* 防御性检查：确保 log_start 非负，防止因日志计数不一致导致负数索引越界 */
        if (log_start < 0) log_start = 0;
        for (int i = log_start; i < result->log->count; i++) {
            record_merge(result,
                         result->log->entries[i].old_id,
                         result->log->entries[i].new_id);
        }
    }

    /* 阶段3：区域合并——阶段2线段合并后，检查所有 REGION 节点。
       如果两个区域的边界线段序列相同（线段合并后，相同线段ID以相同顺序排列），
       则合并它们。保留ID较小的那个。 */
    int phase3 = merge_regions(graph, result->log);
    /* 将阶段3的合并记录到结果数组中 */
    if (phase3 > 0 && result->log) {
        int log_start = result->log->count - phase3;
        /* 防御性检查：确保 log_start 非负，防止因日志计数不一致导致负数索引越界 */
        if (log_start < 0) log_start = 0;
        for (int i = log_start; i < result->log->count; i++) {
            record_merge(result,
                         result->log->entries[i].old_id,
                         result->log->entries[i].new_id);
        }
    }

    (void)phase1; (void)phase2; (void)phase3;

    if (!scope_aware && !result->user_confirmed) {
        result->user_confirmed = false;
    }

    /* 阶段4：稳定化——确定性拓扑排序 */
    graph_topological_sort_stable(graph);

    /* 计算总合并数（供流式输出使用） */
    int total_merges = (phase1 > 0 ? phase1 : 0) +
                       (phase2 > 0 ? phase2 : 0) +
                       (phase3 > 0 ? phase3 : 0);

    /* 流式输出: 拓扑排序完成 (Phase 4 progress) */
    if (normalization_stream_ctx) {
        char desc[NORM_DESC_BUFFER_SIZE];
        snprintf(desc, sizeof(desc), "拓扑排序完成, 节点: %d", graph->node_count);
        stream_emit_simple(normalization_stream_ctx, STREAM_EVENT_PROGRESS,
                           desc, 4);  /* phase 4 = stabilization */
    }

    /* 流式输出: 规范化完成 */
    if (normalization_stream_ctx) {
        char desc[NORM_DESC_BUFFER_SIZE];
        snprintf(desc, sizeof(desc), "规范化完成: 合并 %d 个节点 (P1:%d P2:%d P3:%d), 剩余 %d 个节点",
                 total_merges, phase1, phase2, phase3, graph->node_count);
        stream_emit_simple(normalization_stream_ctx, STREAM_EVENT_NORMALIZE_DONE,
                           desc, 5);
    }

    return result;
}

void normalization_result_destroy(NormalizationResult *result) {
    if (result) {
        normalization_log_destroy(result->log);
        lv_free((void**)&result->merged_node_ids);
        lv_free((void**)&result->original_ids);
        lv_free((void**)&result->representative_ids);
        lv_free((void**)&result);
    }
}

/* ------------------------------------------------------------------ */
/*  图哈希 / 重写历史记录（功能不变）                                  */
/* ------------------------------------------------------------------ */

static uint64_t fnv1a_hash(const char *s) {
    uint64_t hash = lv_FNV64_OFFSET_BASIS;
    while (*s) {
        hash ^= (uint8_t)(*s++);
        hash *= lv_FNV64_PRIME;
    }
    return hash;
}

GraphHash *compute_complete_graph_hash(const ConstraintGraph *graph) {
    GraphHash *gh = lv_calloc(1, sizeof(GraphHash));
    if (!gh) return NULL;
    gh->node_count = graph->node_count;
    gh->node_hashes = lv_calloc((size_t)graph->node_count , sizeof(uint64_t));
    if (!gh->node_hashes && graph->node_count > 0) {
        lv_free((void**)&gh);
        return NULL;
    }
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        char *desc = lv_malloc(NORM_HASH_DESC_BUFFER_SIZE);
        if (!desc) {
            /* 内存分配失败：使用 lv_malloc 分配最小缓冲区并复制空字符串，
               确保后续 lv_free 能正确释放（strdup 使用标准 malloc，
               与 lv_free 的自定义 AllocHeader 不兼容） */
            desc = lv_malloc(1);
            if (desc) desc[0] = '\0';
        } else {
            snprintf(desc, NORM_HASH_DESC_BUFFER_SIZE, "%d:%d", node->id, (int)node->type);
        }
        if (node->type == GEOM_POINT && node->coord_count > 0) {
            char *coord_str = symbolic_coord_serialize(node->symbolic_coords[0]);
            if (coord_str) {
                /* 检查 strlen 总和是否溢出，防止分配过小的缓冲区 */
                size_t desc_len = strlen(desc);
                size_t coord_len = strlen(coord_str);
                if (desc_len > SIZE_MAX - coord_len - 2) {
                    /* 长度溢出：跳过拼接，仅使用 desc */
                    lv_free((void**)&coord_str);
                } else {
                    char *new_desc = lv_malloc(desc_len + coord_len + 2);
                    if (new_desc) {
                        /* 使用 snprintf 替代 sprintf，防止缓冲区溢出 */
                        snprintf(new_desc, desc_len + coord_len + 2, "%s:%s", desc, coord_str);
                        lv_free((void**)&desc);
                        desc = new_desc;
                    }
                    lv_free((void**)&coord_str);
                }
            }
        }
        gh->node_hashes[i] = fnv1a_hash(desc);
        lv_free((void**)&desc);
    }
    gh->hash = lv_FNV64_OFFSET_BASIS;
    for (int i = 0; i < graph->node_count; i++) {
        gh->hash ^= gh->node_hashes[i];
        gh->hash *= lv_FNV64_PRIME;
    }
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        char desc[NORM_CONSTRAINT_DESC_SIZE];
        snprintf(desc, NORM_CONSTRAINT_DESC_SIZE, "C:%d:%d", c->id, (int)c->type);
        uint64_t chash = fnv1a_hash(desc);
        gh->hash ^= chash;
        gh->hash *= lv_FNV64_PRIME;
    }
    return gh;
}

bool graph_hash_equal(const GraphHash *a, const GraphHash *b) {
    if (!a || !b) return false;
    if (a->hash != b->hash) return false;
    if (a->node_count != b->node_count) return false;
    for (int i = 0; i < a->node_count; i++) {
        if (a->node_hashes[i] != b->node_hashes[i]) return false;
    }
    return true;
}

void graph_hash_destroy(GraphHash *hash) {
    if (hash) {
        lv_free((void**)&hash->node_hashes);
        lv_free((void**)&hash);
    }
}

RewriteHistory *rewrite_history_create(int capacity) {
    RewriteHistory *rh = lv_calloc(1, sizeof(RewriteHistory));
    if (!rh) return NULL;
    rh->capacity = capacity;
    rh->count = 0;
    rh->history = lv_calloc((size_t)capacity , sizeof(GraphHash *));
    if (!rh->history) {
        lv_free((void**)&rh);
        return NULL;
    }
    for (int i = 0; i < capacity; i++) {
        rh->history[i] = NULL;
    }
    return rh;
}

void rewrite_history_destroy(RewriteHistory *history) {
    if (history) {
        for (int i = 0; i < history->count; i++) {
            graph_hash_destroy(history->history[i]);
        }
        lv_free((void**)&history->history);
        lv_free((void**)&history);
    }
}

bool rewrite_history_check_cycle(const RewriteHistory *history, const ConstraintGraph *graph) {
    GraphHash *current_hash = compute_complete_graph_hash(graph);
    if (!current_hash) return false;
    bool cycle = false;
    for (int i = 0; i < history->count; i++) {
        if (graph_hash_equal(history->history[i], current_hash)) {
            cycle = true;
            /* 流式事件：检测到重写循环 */
            if (normalization_stream_ctx) {
                stream_emit_simple(normalization_stream_ctx, STREAM_EVENT_WARNING,
                    "重写循环检测: 当前图哈希匹配历史条目", 0);
            }
            break;
        }
    }
    graph_hash_destroy(current_hash);
    return cycle;
}

void rewrite_history_add(RewriteHistory *history, ConstraintGraph *graph) {
    if (history->count >= history->capacity) {
        graph_hash_destroy(history->history[0]);
        for (int i = 1; i < history->capacity; i++) {
            history->history[i - 1] = history->history[i];
        }
        history->count--;
    }
    history->history[history->count++] = compute_complete_graph_hash(graph);
}

/* ------------------------------------------------------------------ */
/*  normalization_verify_idempotency                                   */
/* ------------------------------------------------------------------ */

/**
 * @brief 验证图规范化的幂等性。
 *
 * 对已规范化的图再次运行规范化，检查是否产生任何变化。
 * 根据 design_v2.9.md 第4.3节：
 * "规范化算法设计保证幂等：规范化后的图再次运行规范化不会产生任何变化"
 *
 * @param graph 约束图（预期已规范化）
 * @return true 如果幂等（无变化），false 否则
 */
bool normalization_verify_idempotency(ConstraintGraph *graph) {
    if (!graph) return false;

    /* 计算第二次规范化前的哈希值 */
    GraphHash *hash_before = compute_complete_graph_hash(graph);
    if (!hash_before) return false;

    /* 再次运行规范化 */
    NormalizationResult *result = graph_normalize(graph, false);
    
    /* 计算第二次规范化后的哈希值 */
    GraphHash *hash_after = compute_complete_graph_hash(graph);
    
    bool idempotent = false;
    if (hash_after) {
        idempotent = graph_hash_equal(hash_before, hash_after);
    }
    
    /* 同时检查：不应发生任何合并 */
    if (result && result->merged_count > 0) {
        idempotent = false;
    }
    
    /* 清理资源 */
    graph_hash_destroy(hash_before);
    if (hash_after) graph_hash_destroy(hash_after);
    if (result) normalization_result_destroy(result);
    
    /* 流式事件：幂等性验证结果 */
    if (normalization_stream_ctx) {
        if (idempotent) {
            stream_emit_simple(normalization_stream_ctx, STREAM_EVENT_INFO,
                "幂等性验证通过: 规范化结果稳定", 0);
        } else {
            stream_emit_simple(normalization_stream_ctx, STREAM_EVENT_WARNING,
                "幂等性验证失败: 二次规范化产生了变化", 0);
        }
    }

    return idempotent;
}
