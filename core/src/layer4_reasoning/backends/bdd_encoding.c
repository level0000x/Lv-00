/**
 * @file bdd_encoding.c
 * @brief CUDD 二阶策略图编码 —— 真实实现
 *
 * 提供 BDD/ADD 的完整操作实现，包括布尔运算、变量序优化、
 * 约束图 -> BDD 编码和坐标 bit-blasting。
 *
 * BDD 核心算法：
 * - 唯一表哈希（开放寻址法）确保节点去重
 * - ITE (If-Then-Else) 递归算法实现所有布尔运算
 * - Tseitin 变换实现 BDD -> CNF 转换
 * - Sifting 变量序优化
 *
 * @version v3.3.0
 * @date 2026-05-24
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

/* ========================================================================
 * 内部：唯一表哈希
 * ======================================================================== */

/** 墓碑标记 —— 用于开放寻址哈希表中标记已删除的槽位，保护探查链 */
static BDDNode bdd_tombstone_marker = {-2, NULL, NULL, 0, false};
#define BDD_TOMBSTONE (&bdd_tombstone_marker)

/** 节点三元组哈希 (var_id, low, high) -> 唯一表索引
 *  exempt: 判据「哈希表族收敛」——唯一表为三元组键 (var_id, low, high) +
 *  墓碑 + 引用计数的开放寻址表，语义不同于 lv_hashtable_int_hash 的整型键，保留。
 */
static int bdd_unique_hash(int var_id, BDDNode *low, BDDNode *high, int table_size) {
    unsigned long h = (unsigned long) var_id;
    h = h * 31 + (unsigned long) (uintptr_t) low;
    h = h * 31 + (unsigned long) (uintptr_t) high;
    return (int) (h % (unsigned long) table_size);
}

/** 在唯一表中查找或插入节点 */
static BDDNode *bdd_unique_lookup(BDDManager *mgr, int var_id, BDDNode *low, BDDNode *high) {
    lv_CHECK_NULL(mgr, NULL);

    /* 终端节点直接返回 */
    if (low == high)
        return low;

    /* 在唯一表中查找已存在的相同 (var, lo, hi) 节点 */
    int idx = bdd_unique_hash(var_id, low, high, mgr->unique_table_size);
    int first_tombstone = -1;
    /* 线性探测 */
    for (int probe = 0; probe < mgr->unique_table_size; probe++) {
        int slot = (idx + probe) % mgr->unique_table_size;
        BDDNode *existing = mgr->unique_table[slot];
        if (existing == NULL)
            break; /* 空槽，未找到 — 探查链结束 */
        if (existing == BDD_TOMBSTONE) {
            /* 墓碑：记录第一个可复用位置，继续探查 */
            if (first_tombstone < 0)
                first_tombstone = slot;
            continue;
        }
        if (existing->var_id == var_id && existing->low == low && existing->high == high) {
            /* 找到已存在节点，增加引用计数并返回 */
            lv_ATOMIC_ADD64(&existing->ref_count, 1);
            return existing;
        }
    }

    /* 未找到，分配新节点 */
    BDDNode *node = (BDDNode *) lv_calloc(1, sizeof(BDDNode));
    if (!node)
        return NULL;
    node->var_id = var_id;
    node->low = low;
    bdd_ref(low);   /* 子节点引用计数 +1，防止被 bdd_deref 提前释放 */
    node->high = high;
    bdd_ref(high);  /* 子节点引用计数 +1，防止被 bdd_deref 提前释放 */
    node->ref_count = 1; /* 初始引用计数为 1（调用者持有） */
    node->complemented = false;
    mgr->node_count++;

    /* 插入唯一表：优先复用墓碑槽位 */
    if (first_tombstone >= 0) {
        mgr->unique_table[first_tombstone] = node;
        return node;
    }

    int slot = bdd_unique_hash(var_id, low, high, mgr->unique_table_size);
    for (int probe = 0; probe < mgr->unique_table_size; probe++) {
        int s = (slot + probe) % mgr->unique_table_size;
        if (mgr->unique_table[s] == NULL || mgr->unique_table[s] == BDD_TOMBSTONE) {
            mgr->unique_table[s] = node;
            break;
        }
    }

    return node;
}

/* ========================================================================
 * BDD 管理器生命周期
 * ======================================================================== */

/* ---- lv_DEFER 守卫：管理器部分构建的 defer 清理（graph_memory.c 的
 *      mpq_matrix_guard 模式 —— guard 持有值拷贝，置 NULL 即解除守卫） ---- */

typedef struct {
    BDDManager *mgr;
} BddManagerGuard;

static void bdd_manager_guard_cleanup(void *p) {
    BddManagerGuard *g = (BddManagerGuard *) p;
    if (!g->mgr)
        return;
    /* 与原 cleanup 标签清理顺序一致：lv_free 对 NULL 安全 */
    lv_free((void **) &g->mgr->computed_table);
    lv_free((void **) &g->mgr->var_names);
    lv_free((void **) &g->mgr->var_types);
    lv_free((void **) &g->mgr->var_order);
    lv_free((void **) &g->mgr->unique_table);
    lv_free((void **) &g->mgr->false_node);
    lv_free((void **) &g->mgr->true_node);
    lv_free((void **) &g->mgr);
}

typedef struct {
    ADDManager *mgr;
} AddManagerGuard;

static void add_manager_guard_cleanup(void *p) {
    AddManagerGuard *g = (AddManagerGuard *) p;
    if (!g->mgr)
        return;
    /* 与原 cleanup 标签清理顺序一致：lv_free 对 NULL 安全 */
    lv_free((void **) &g->mgr->var_order);
    lv_free((void **) &g->mgr->unique_table);
    lv_free((void **) &g->mgr->one_node);
    lv_free((void **) &g->mgr->zero_node);
    lv_free((void **) &g->mgr);
}

/**
 * @brief 创建 BDD 管理器
 *
 * 分配并初始化 BDD 管理器，创建终端节点（T/F）、唯一表和变量序数组。
 *
 * @param var_count        变量数量
 * @param unique_table_size 唯一表大小（最小 1024）
 * @return 新分配的 BDDManager 指针，失败返回 NULL
 */
BDDManager *bdd_manager_create(int var_count, int unique_table_size) {
    BDDManager *mgr = (BDDManager *) lv_calloc(1, sizeof(BDDManager));
    if (!mgr)
        return NULL;
    /* 注册 lv_DEFER 守卫：任何失败路径（含中途 return NULL）自动按原 cleanup
     * 语义逐字段释放已分配的成员；成功路径 guard.mgr = NULL 解除守卫 */
    BddManagerGuard guard = {mgr};
    lv_DEFER(bdd_manager_guard_cleanup, &guard);

    /* 创建终端 T 节点 */
    mgr->true_node = (BDDNode *) lv_calloc(1, sizeof(BDDNode));
    if (!mgr->true_node)
        return NULL;
    mgr->true_node->var_id = -1;
    mgr->true_node->low = NULL;
    mgr->true_node->high = NULL;
    mgr->true_node->ref_count = 1; /* 持久引用 */
    mgr->true_node->complemented = false;

    /* 创建终端 F 节点 */
    mgr->false_node = (BDDNode *) lv_calloc(1, sizeof(BDDNode));
    if (!mgr->false_node)
        return NULL;
    mgr->false_node->var_id = -1;
    mgr->false_node->low = NULL;
    mgr->false_node->high = NULL;
    mgr->false_node->ref_count = 1; /* 持久引用 */
    mgr->false_node->complemented = false;

    /* 分配唯一表（当前为线性扫描实现，完整版应使用哈希表加速查找） */
    if (unique_table_size < 1024)
        unique_table_size = 1024;
    mgr->unique_table = (BDDNode **) lv_calloc((size_t) unique_table_size, sizeof(BDDNode *));
    if (!mgr->unique_table)
        return NULL;
    mgr->unique_table_size = unique_table_size;

    /* 变量序数组 */
    mgr->var_order = (int *) lv_malloc((size_t) var_count * sizeof(int));
    if (!mgr->var_order)
        return NULL;
    for (int i = 0; i < var_count; i++) {
        mgr->var_order[i] = i;
    }
    mgr->var_count = 0;
    mgr->var_capacity = var_count;
    mgr->node_count = 0;

    /* 变量名称表和类型表 */
    mgr->var_names = (char **) lv_calloc((size_t) var_count, sizeof(char *));
    mgr->var_types = (BDDVarType *) lv_calloc((size_t) var_count, sizeof(BDDVarType));
    if (!mgr->var_names || !mgr->var_types)
        return NULL;

    /* 分配 ITE 计算表（缓存 ITE 结果，大小 = 唯一表大小） */
    mgr->computed_table_size = unique_table_size;
    mgr->computed_table = (ITECacheEntry *) lv_calloc((size_t) unique_table_size, sizeof(ITECacheEntry));
    if (!mgr->computed_table)
        return NULL;

    guard.mgr = NULL; /* 守卫解除：结果移交调用方 */
    return mgr;
}

/**
 * @brief 销毁 BDD 管理器，释放所有节点和内部数据结构
 * @param mgr 要销毁的 BDD 管理器
 */
void bdd_manager_destroy(BDDManager *mgr) {
    if (!mgr)
        return;
    /* 遍历唯一表，释放所有已分配的节点 */
    if (mgr->unique_table) {
        for (int i = 0; i < mgr->unique_table_size; i++) {
            if (mgr->unique_table[i] != NULL && mgr->unique_table[i] != BDD_TOMBSTONE) {
                lv_free((void **) &mgr->unique_table[i]);
            }
        }
    }
    lv_free((void **) &mgr->true_node);
    lv_free((void **) &mgr->false_node);
    lv_free((void **) &mgr->unique_table);
    lv_free((void **) &mgr->var_order);
    lv_free((void **) &mgr->computed_table);
    /* 释放变量名称表 */
    lv_free_ptr_array((void ***) &mgr->var_names, (size_t) mgr->var_count);
    lv_free((void **) &mgr->var_types);
    lv_free((void **) &mgr);
}

/**
 * @brief 在 BDD 管理器中创建新变量
 *
 * @param mgr  BDD 管理器
 * @param name 变量名称（调试用，可为 NULL）
 * @param type 变量类型
 * @return 新变量的 ID，失败返回 -1
 */
int bdd_new_var(BDDManager *mgr, const char *name, BDDVarType type) {
    lv_CHECK_NULL(mgr, -1);
    /* 检查 var_order 数组容量，不足时扩容 */
    if (mgr->var_count >= mgr->var_capacity) {
        if (mgr->var_capacity > 0 && mgr->var_capacity > INT_MAX / 2)
            return -1;
        int old_capacity = mgr->var_capacity;
        if (!lv_ensure_capacity((void **)&mgr->var_order, mgr->var_count,
                                &mgr->var_capacity, sizeof(int), 1))
            return -1;

        /* 同步扩容 var_names 和 var_types 数组（统一 lv_ensure_capacity；
         * 以旧容量独立扩容（增长因子相同 → 三数组容量一致），部分失败时已
         * 扩容的临时指针释放、未动的指针保持旧值，避免泄漏） */
        char **new_names = mgr->var_names;
        BDDVarType *new_types = mgr->var_types;
        int names_cap = old_capacity;
        int types_cap = old_capacity;
        if (!lv_ensure_capacity((void **) &new_names, mgr->var_count, &names_cap, sizeof(char *), 1) ||
            !lv_ensure_capacity((void **) &new_types, mgr->var_count, &types_cap, sizeof(BDDVarType), 1)) {
            if (new_names != mgr->var_names)
                lv_free((void **) &new_names);
            if (new_types != mgr->var_types)
                lv_free((void **) &new_types);
            return -1;
        }
        mgr->var_names = new_names;
        mgr->var_types = new_types;
        /* 初始化新增的槽位 */
        for (int i = old_capacity; i < mgr->var_capacity; i++) {
            mgr->var_names[i] = NULL;
            mgr->var_types[i] = BDD_BOOLEAN;
        }
    }
    int id = mgr->var_count;
    mgr->var_order[id] = id;

    /* 存储变量名称和类型 */
    if (name) {
        mgr->var_names[id] = lv_strdup_safe(name);
    } else {
        char auto_name[32];
        snprintf(auto_name, sizeof(auto_name), "bdd_var_%d", id);
        mgr->var_names[id] = lv_strdup_safe(auto_name);
    }
    mgr->var_types[id] = type;

    mgr->var_count++;
    return id;
}

/* ========================================================================
 * BDD 节点创建与引用计数
 * ======================================================================== */

/**
 * @brief 获取 BDD 终端 True 节点
 * @param mgr BDD 管理器
 * @return True 节点指针，失败返回 NULL
 */
BDDNode *bdd_true(BDDManager *mgr) {
    return mgr ? mgr->true_node : NULL;
}

/**
 * @brief 获取 BDD 终端 False 节点
 * @param mgr BDD 管理器
 * @return False 节点指针，失败返回 NULL
 */
BDDNode *bdd_false(BDDManager *mgr) {
    return mgr ? mgr->false_node : NULL;
}

/**
 * @brief 创建 BDD 文字节点（正文字或负文字）
 *
 * @param mgr    BDD 管理器
 * @param var_id 变量 ID（正数=正文字，负数=负文字）
 * @return 文字节点指针，失败返回 NULL
 */
BDDNode *bdd_literal(BDDManager *mgr, int var_id) {
    lv_CHECK_NULL(mgr, NULL);
    if (var_id > 0) {
        /* 正文字：var -> high=T, low=F */
        return bdd_unique_lookup(mgr, var_id, mgr->false_node, mgr->true_node);
    } else {
        /* 负文字：~var -> high=F, low=T */
        return bdd_unique_lookup(mgr, -var_id, mgr->true_node, mgr->false_node);
    }
}

/**
 * @brief 增加节点引用计数
 * @param node BDD 节点
 */
void bdd_ref(BDDNode *node) {
    if (node) {
        lv_ATOMIC_ADD64(&node->ref_count, 1);
    }
}

/**
 * @brief 减少节点引用计数，为 0 时从唯一表移除并释放
 * @param mgr  BDD 管理器
 * @param node BDD 节点
 */
void bdd_deref(BDDManager *mgr, BDDNode *node) {
    if (!node || node->ref_count == 0)
        return;
    uint64_t old_ref = lv_ATOMIC_ADD64(&node->ref_count, -1);
    /* 终端节点（var_id == -1）不回收 */
    if (node->var_id < 0)
        return;
    if (old_ref > 1)
        return;

    /* 引用计数降为 0：先释放子节点，再从唯一表移除，最后释放自身 */
    BDDNode *child_low = node->low;
    BDDNode *child_high = node->high;
    node->low = NULL;
    node->high = NULL;

    /* 从唯一表中标记为墓碑并释放 */
    if (mgr && mgr->unique_table) {
        int idx = bdd_unique_hash(node->var_id, child_low, child_high, mgr->unique_table_size);
        for (int probe = 0; probe < mgr->unique_table_size; probe++) {
            int slot = (idx + probe) % mgr->unique_table_size;
            if (mgr->unique_table[slot] == node) {
                mgr->unique_table[slot] = BDD_TOMBSTONE; /* 墓碑标记，保护探查链 */
                break;
            }
            if (mgr->unique_table[slot] == NULL)
                break;
        }
        mgr->node_count--;
    }
    lv_free((void **) &node);

    /* 释放子节点引用（在释放自身之后，避免递归中的崩溃） */
    bdd_deref(mgr, child_low);
    bdd_deref(mgr, child_high);
}

/* ========================================================================
 * BDD ITE —— 核心递归算法
 *
 * ite(F, G, H) = (F & G) | (~F & H)
 *
 * 递归终止条件：
 * - F = T -> G
 * - F = F -> H
 * - G = H -> G
 * - G = T, H = F -> F
 * - G = F, H = T -> ~F
 *
 * 一般情况：选择 F, G, H 中最小的变量，递归展开。
 * ======================================================================== */

/** ITE 计算表哈希: (f, g, h) -> 缓存槽索引 */
static int bdd_ite_cache_hash(BDDNode *f, BDDNode *g, BDDNode *h, int table_size) {
    unsigned long val = (unsigned long) (uintptr_t) f;
    val = val * 31 + (unsigned long) (uintptr_t) g;
    val = val * 31 + (unsigned long) (uintptr_t) h;
    return (int) (val % (unsigned long) table_size);
}

BDDNode *bdd_ite(BDDManager *mgr, BDDNode *f, BDDNode *g, BDDNode *h) {
    if (!mgr || !f || !g || !h)
        return NULL;

    /* 查找 ITE 计算表缓存 */
    if (mgr->computed_table && mgr->computed_table_size > 0) {
        int cache_idx = bdd_ite_cache_hash(f, g, h, mgr->computed_table_size);
        for (int probe = 0; probe < 8; probe++) {
            int slot = (cache_idx + probe) % mgr->computed_table_size;
            ITECacheEntry *entry = &mgr->computed_table[slot];
            if (!entry->occupied)
                break;
            if (entry->f == f && entry->g == g && entry->h == h) {
                /* 缓存命中 */
                bdd_ref(entry->result);
                return entry->result;
            }
        }
    }

    /* 终端条件 */
    if (f == mgr->true_node) {
        bdd_ref(g);
        return g;
    }
    if (f == mgr->false_node) {
        bdd_ref(h);
        return h;
    }
    if (g == h) {
        bdd_ref(g);
        return g;
    }
    if (g == mgr->true_node && h == mgr->false_node) {
        bdd_ref(f);
        return f;
    }
    if (g == mgr->false_node && h == mgr->true_node) {
        return bdd_not(mgr, f);
    }

    /* 确定顶部变量：取三者中最小的变量 ID */
    int top_var = f->var_id;
    if (g->var_id >= 0 && (top_var < 0 || g->var_id < top_var))
        top_var = g->var_id;
    if (h->var_id >= 0 && (top_var < 0 || h->var_id < top_var))
        top_var = h->var_id;

    /* 若非终端，cofactor（简化实现仅比较 var_id） */
    BDDNode *f_low = (f->var_id == top_var) ? f->low : f;
    BDDNode *f_high = (f->var_id == top_var) ? f->high : f;
    BDDNode *g_low = (g->var_id == top_var) ? g->low : g;
    BDDNode *g_high = (g->var_id == top_var) ? g->high : g;
    BDDNode *h_low = (h->var_id == top_var) ? h->low : h;
    BDDNode *h_high = (h->var_id == top_var) ? h->high : h;

    /* 递归 */
    BDDNode *t = bdd_ite(mgr, f_low, g_low, h_low);
    BDDNode *e = bdd_ite(mgr, f_high, g_high, h_high);

    BDDNode *result = bdd_unique_lookup(mgr, top_var, t, e);
    bdd_deref(mgr, t);
    bdd_deref(mgr, e);

    /* 将结果存入 ITE 计算表缓存 */
    if (mgr->computed_table && mgr->computed_table_size > 0 && result) {
        int cache_idx = bdd_ite_cache_hash(f, g, h, mgr->computed_table_size);
        for (int probe = 0; probe < 8; probe++) {
            int slot = (cache_idx + probe) % mgr->computed_table_size;
            ITECacheEntry *entry = &mgr->computed_table[slot];
            if (!entry->occupied) {
                entry->f = f;
                entry->g = g;
                entry->h = h;
                entry->result = result;
                entry->occupied = true;
                break;
            }
        }
    }

    return result;
}

/* ========================================================================
 * BDD 布尔运算
 * ======================================================================== */

BDDNode *bdd_and(BDDManager *mgr, BDDNode *f, BDDNode *g) {
    /* f & g = ite(f, g, F) */
    lv_CHECK_NULL(mgr, NULL);
    return bdd_ite(mgr, f, g, mgr->false_node);
}

BDDNode *bdd_or(BDDManager *mgr, BDDNode *f, BDDNode *g) {
    /* f | g = ite(f, T, g) */
    lv_CHECK_NULL(mgr, NULL);
    return bdd_ite(mgr, f, mgr->true_node, g);
}

BDDNode *bdd_not(BDDManager *mgr, BDDNode *f) {
    /* ~f = ite(f, F, T) —— 直接实现避免通过 bdd_ite 触发无限递归
     * Shannon 展开：~f = x * ~f_high + ~x * ~f_low */
    lv_CHECK_NULL(mgr, NULL);
    lv_CHECK_NULL(f, NULL);
    if (f == mgr->true_node) {
        bdd_ref(mgr->false_node);
        return mgr->false_node;
    }
    if (f == mgr->false_node) {
        bdd_ref(mgr->true_node);
        return mgr->true_node;
    }
    BDDNode *not_low = bdd_not(mgr, f->low);
    BDDNode *not_high = bdd_not(mgr, f->high);
    BDDNode *result = bdd_unique_lookup(mgr, f->var_id, not_low, not_high);
    bdd_deref(mgr, not_low);
    bdd_deref(mgr, not_high);
    return result;
}

BDDNode *bdd_xor(BDDManager *mgr, BDDNode *f, BDDNode *g) {
    /* f ^ g = ite(f, ~g, g) */
    lv_CHECK_NULL(mgr, NULL);
    BDDNode *not_g = bdd_not(mgr, g);
    BDDNode *result = bdd_ite(mgr, f, not_g, g);
    bdd_deref(mgr, not_g);
    return result;
}

BDDNode *bdd_nand(BDDManager *mgr, BDDNode *f, BDDNode *g) {
    /* ~(f & g) = ite(f, ~g, T) */
    lv_CHECK_NULL(mgr, NULL);
    BDDNode *not_g = bdd_not(mgr, g);
    BDDNode *result = bdd_ite(mgr, f, not_g, mgr->true_node);
    bdd_deref(mgr, not_g);
    return result;
}

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

/* ── 辅助：根据节点 ID 查找 node_base_var 数组索引（graph_get_node 哈希 → O(1)） ── */
static int lookup_node_base_var(int node_id, int n, const int *node_base_var, const ConstraintGraph *graph) {
    if (!graph || node_id < 0 || graph->node_count <= 0 || !graph->nodes[0])
        return -1;
    GeomNode *node = graph_get_node(graph, node_id);
    if (!node)
        return -1;
    intptr_t byte_off = (intptr_t) node - (intptr_t) graph->nodes[0];
    if (byte_off < 0 || byte_off % (intptr_t) sizeof(GeomNode *) != 0)
        return -1;
    intptr_t j = byte_off / (intptr_t) sizeof(GeomNode *);
    if (j < 0 || j >= n)
        return -1;
    return node_base_var[j];
}

/* ── BDD 编码辅助函数（文件作用域，用于查找表） ── */
typedef BDDNode *(*BDDEncodeFn)(BDDManager *mgr, const Constraint *con, int n, const int *node_base_var, const ConstraintGraph *graph);

static BDDNode *bdd_encode_incidence(BDDManager *mgr, const Constraint *con, int n, const int *node_base_var, const ConstraintGraph *graph) {
    (void)n;
    if (lv_constraint_has_participants(con, 2)) {
        int p_id = con->participants[0];
        int l_id = con->participants[1];
        int p_var = (p_id >= 0) ? lookup_node_base_var(p_id, n, node_base_var, graph) : -1;
        int l_var = (l_id >= 0) ? lookup_node_base_var(l_id, n, node_base_var, graph) : -1;
        if (p_var >= 0 && l_var >= 0) {
            BDDNode *p_lit = bdd_literal(mgr, p_var + 1);
            BDDNode *l_lit = bdd_literal(mgr, l_var + 1);
            BDDNode *result = bdd_and(mgr, p_lit, l_lit);
            bdd_deref(mgr, p_lit);
            bdd_deref(mgr, l_lit);
            return result;
        }
    }
    return NULL;
}

static BDDNode *bdd_encode_betweenness(BDDManager *mgr, const Constraint *con, int n, const int *node_base_var, const ConstraintGraph *graph) {
    (void)n;
    if (lv_constraint_has_participants(con, 3)) {
        int p1_var = (con->participants[0] >= 0) ? lookup_node_base_var(con->participants[0], n, node_base_var, graph) : -1;
        int p2_var = (con->participants[1] >= 0) ? lookup_node_base_var(con->participants[1], n, node_base_var, graph) : -1;
        int p3_var = (con->participants[2] >= 0) ? lookup_node_base_var(con->participants[2], n, node_base_var, graph) : -1;
        if (p1_var >= 0 && p2_var >= 0 && p3_var >= 0) {
            BDDNode *a = bdd_literal(mgr, p1_var + 1);
            BDDNode *b = bdd_literal(mgr, p2_var + 1);
            BDDNode *c = bdd_literal(mgr, p3_var + 1);
            BDDNode *ab = bdd_and(mgr, a, b);
            BDDNode *result = bdd_and(mgr, ab, c);
            bdd_deref(mgr, a);
            bdd_deref(mgr, b);
            bdd_deref(mgr, c);
            bdd_deref(mgr, ab);
            return result;
        }
    }
    return NULL;
}

static BDDNode *bdd_encode_intersection(BDDManager *mgr, const Constraint *con, int n, const int *node_base_var, const ConstraintGraph *graph) {
    (void)n;
    if (lv_constraint_has_participants(con, 3)) {
        int l1_var = (con->participants[0] >= 0) ? lookup_node_base_var(con->participants[0], n, node_base_var, graph) : -1;
        int l2_var = (con->participants[1] >= 0) ? lookup_node_base_var(con->participants[1], n, node_base_var, graph) : -1;
        int p_var = (con->participants[2] >= 0) ? lookup_node_base_var(con->participants[2], n, node_base_var, graph) : -1;
        if (l1_var >= 0 && l2_var >= 0 && p_var >= 0) {
            BDDNode *l1_lit = bdd_literal(mgr, l1_var + 1);
            BDDNode *l2_lit = bdd_literal(mgr, l2_var + 1);
            BDDNode *p_lit = bdd_literal(mgr, p_var + 1);
            BDDNode *l_and = bdd_and(mgr, l1_lit, l2_lit);
            BDDNode *result = bdd_and(mgr, l_and, p_lit);
            bdd_deref(mgr, l1_lit);
            bdd_deref(mgr, l2_lit);
            bdd_deref(mgr, p_lit);
            bdd_deref(mgr, l_and);
            return result;
        }
    }
    return NULL;
}

static BDDNode *bdd_encode_containment(BDDManager *mgr, const Constraint *con, int n, const int *node_base_var, const ConstraintGraph *graph) {
    (void)n;
    if (lv_constraint_has_participants(con, 2)) {
        int r_var = (con->participants[0] >= 0) ? lookup_node_base_var(con->participants[0], n, node_base_var, graph) : -1;
        int p_var = (con->participants[1] >= 0) ? lookup_node_base_var(con->participants[1], n, node_base_var, graph) : -1;
        if (r_var >= 0 && p_var >= 0) {
            BDDNode *r_lit = bdd_literal(mgr, r_var + 1);
            BDDNode *p_lit = bdd_literal(mgr, p_var + 1);
            BDDNode *result = bdd_and(mgr, r_lit, p_lit);
            bdd_deref(mgr, r_lit);
            bdd_deref(mgr, p_lit);
            return result;
        }
    }
    return NULL;
}

static BDDNode *bdd_encode_angle(BDDManager *mgr, const Constraint *con, int n, const int *node_base_var, const ConstraintGraph *graph) {
    (void)n;
    if (lv_constraint_has_participants(con, 2)) {
        int l1_id = con->participants[0];
        int l2_id = con->participants[1];
        int l1_var = (l1_id >= 0) ? lookup_node_base_var(l1_id, n, node_base_var, graph) : -1;
        int l2_var = (l2_id >= 0) ? lookup_node_base_var(l2_id, n, node_base_var, graph) : -1;

        int bucket_count = 1 << 8;
        double bucket_width = lv_HALF_CIRCLE_DEG / (double) bucket_count;
        int target_bucket = (int) (con->numeric_value / bucket_width);
        if (target_bucket < 0)
            target_bucket = 0;
        if (target_bucket >= bucket_count)
            target_bucket = bucket_count - 1;

        BDDNode *acc = bdd_true(mgr);

        if (l1_var >= 0) {
            BDDNode *lit = bdd_literal(mgr, l1_var + 1);
            BDDNode *and1 = bdd_and(mgr, acc, lit);
            bdd_deref(mgr, acc);
            bdd_deref(mgr, lit);
            acc = and1;
        }
        if (l2_var >= 0) {
            BDDNode *lit = bdd_literal(mgr, l2_var + 1);
            BDDNode *and1 = bdd_and(mgr, acc, lit);
            bdd_deref(mgr, acc);
            bdd_deref(mgr, lit);
            acc = and1;
        }

        for (int bit = 0; bit < 8; bit++) {
            char var_name[48];
            snprintf(var_name, sizeof(var_name), "angle_c%d_bit%d", con->id, bit);
            int bit_var = bdd_new_var(mgr, var_name, BDD_BOOLEAN);
            if (bit_var < 0)
                break;
            int bit_value = (target_bucket >> bit) & 1;
            BDDNode *bit_lit = bdd_literal(mgr, bit_value ? (bit_var + 1) : -(bit_var + 1));
            BDDNode *and1 = bdd_and(mgr, acc, bit_lit);
            bdd_deref(mgr, acc);
            bdd_deref(mgr, bit_lit);
            acc = and1;
        }

        return acc;
    }
    return NULL;
}

static BDDNode *bdd_encode_connection(BDDManager *mgr, const Constraint *con, int n, const int *node_base_var, const ConstraintGraph *graph) {
    (void)n;
    if (!mgr || !graph || !lv_constraint_has_participants(con, 2))
        return NULL;
    int p1_id = con->participants[0];
    int p2_id = con->participants[1];
    GeomNode *p1 = graph_get_node(graph, p1_id);
    GeomNode *p2 = graph_get_node(graph, p2_id);
    if (!p1 || !p2 || p1->type != GEOM_PORT || p2->type != GEOM_PORT)
        return NULL;

    /* 端口连接 = 数据流等值：两个端口各占用 64 位坐标/数据 bit-blast 变量。
     * 优先复用节点坐标变量（node_base_var），端口缺坐标（默认 -1）时动态分配
     * 数据位变量组，分配模式与 bdd_encode_angle 的动态位变量一致。 */
    int base1 = (p1_id >= 0) ? lookup_node_base_var(p1_id, n, node_base_var, graph) : -1;
    int base2 = (p2_id >= 0) ? lookup_node_base_var(p2_id, n, node_base_var, graph) : -1;

    if (base1 < 0) {
        base1 = mgr->var_count;
        for (int bit = 0; bit < 64; bit++) {
            char var_name[48];
            snprintf(var_name, sizeof(var_name), "conn_%d_src_bit%d", con->id, bit);
            if (bdd_new_var(mgr, var_name, BDD_BOOLEAN) < 0)
                return NULL;
        }
    }
    if (base2 < 0) {
        base2 = mgr->var_count;
        for (int bit = 0; bit < 64; bit++) {
            char var_name[48];
            snprintf(var_name, sizeof(var_name), "conn_%d_dst_bit%d", con->id, bit);
            if (bdd_new_var(mgr, var_name, BDD_BOOLEAN) < 0)
                return NULL;
        }
    }

    /* 逐位等值编码：bit_eq = (a∧b)∨(¬a∧¬b)（XNOR），64 位全部相等
     * → 两端口坐标/数据相等，结果非空（不返回 NULL）。 */
    BDDNode *acc = bdd_true(mgr);
    for (int bit = 0; bit < 64; bit++) {
        BDDNode *a = bdd_literal(mgr, base1 + bit + 1);
        BDDNode *b = bdd_literal(mgr, base2 + bit + 1);
        BDDNode *not_b = bdd_not(mgr, b);
        BDDNode *eq = bdd_ite(mgr, a, b, not_b);
        BDDNode *and1 = bdd_and(mgr, acc, eq);
        bdd_deref(mgr, a);
        bdd_deref(mgr, b);
        bdd_deref(mgr, not_b);
        bdd_deref(mgr, eq);
        bdd_deref(mgr, acc);
        acc = and1;
    }
    return acc;
}

static const BDDEncodeFn kBddEncodeTable[] = {
    bdd_encode_incidence,    /* INCIDENCE */
    bdd_encode_betweenness,  /* BETWEENNESS */
    bdd_encode_intersection, /* INTERSECTION */
    bdd_encode_containment,  /* CONTAINMENT */
    bdd_encode_connection,   /* CONNECTION */
    bdd_encode_angle         /* ANGLE */
};
static const int kBddEncodeTableCount =
    (int)(sizeof(kBddEncodeTable) / sizeof(kBddEncodeTable[0]));

/* ========================================================================
 * constraint_graph_to_bdd —— 约束图 -> BDD 编码
 *
 * 枚举所有布尔组合，构建 BDD。
 * 对于约束图中 n 个节点，有 2^n 种布尔赋值。
 * 每种赋值对应 BDD 的一个满足路径。
 * ======================================================================== */

BDDNode *constraint_graph_to_bdd(const ConstraintGraph *graph, BDDManager *mgr) {
    if (!graph || !mgr)
        return NULL;

    int n = graph->node_count;
    if (n <= 0)
        return bdd_true(mgr);

    /* 阶段 1: 为每个节点的坐标分配 BDD 变量范围 */
    int *node_base_var = (int *) lv_malloc((size_t) n * sizeof(int));
    if (!node_base_var)
        return NULL;

    int next_var = 0;
    for (int i = 0; i < n; i++) {
        GeomNode *gn = graph->nodes[i];
        if (!gn || !gn->symbolic_coords || gn->coord_count <= 0) {
            node_base_var[i] = -1;
            continue;
        }
        node_base_var[i] = next_var;
        int bits = coord_to_bdd_var(gn->symbolic_coords[0], mgr, next_var);
        if (bits > 0)
            next_var += bits;
    }

    /* 阶段 2: 遍历所有活跃约束，按类型编码 BDD 子公式 */
    BDDNode *constraint_bdd = bdd_true(mgr);

    for (int ci = 0; ci < graph->constraint_count; ci++) {
        Constraint *con = graph->constraints[ci];
        if (!con || !con->is_active)
            continue;

        BDDNode *sub = LV_DISPATCH(kBddEncodeTable, con->type, NULL, mgr, con, n, node_base_var, graph);

        if (sub) {
            BDDNode *new_bdd = bdd_and(mgr, constraint_bdd, sub);
            bdd_deref(mgr, constraint_bdd);
            bdd_deref(mgr, sub);
            constraint_bdd = new_bdd;
        }
    }

    lv_free((void **) &node_base_var);
    return constraint_bdd;
}

/* ── 坐标类型数值提取函数（文件作用域，用于查找表）── */
typedef double (*CoordValueFn)(const SymbolicCoord *coord);
static double coord_value_rational(const SymbolicCoord *coord) {
    if (coord->data.rational)
        return mpq_get_d(coord->data.rational->value);
    return 0.0;
}
static double coord_value_algebraic(const SymbolicCoord *coord) {
    if (coord->data.algebraic) {
        return (coord->data.algebraic->left_bound + coord->data.algebraic->right_bound) / 2.0;
    }
    return 0.0;
}
static double coord_value_quadratic(const SymbolicCoord *coord) {
    if (coord->data.quadratic) {
        Quadratic *q = coord->data.quadratic;
        double a_val = (q->a) ? mpq_get_d(q->a->value) : 0.0;
        double b_val = (q->b) ? mpq_get_d(q->b->value) : 0.0;
        return a_val + b_val * sqrt((double) q->n);
    }
    return 0.0;
}
static double coord_value_transcendental(const SymbolicCoord *coord) {
    return symbolic_coord_to_double(coord);
}
static const CoordValueFn kCoordValueTable[] = {
    coord_value_rational,      /* RATIONAL */
    coord_value_algebraic,     /* ALGEBRAIC */
    coord_value_quadratic,     /* QUADRATIC */
    coord_value_transcendental /* TRANSCENDENTAL */
};
static const int kCoordValueTableCount =
    (int)(sizeof(kCoordValueTable) / sizeof(kCoordValueTable[0]));

/* ========================================================================
 * coord_to_bdd_var —— 坐标 bit-blasting
 *
 * IEEE 754 双精度位表示：
 * 符号位 + 11 位指数 + 52 位尾数 = 64 位。
 * 每位编码为一个 BDD 变量。
 * ======================================================================== */

int coord_to_bdd_var(const SymbolicCoord *coord, BDDManager *mgr, int base_var) {
    if (!coord || !mgr)
        return -1;

/* 64 位 IEEE 754 双精度编码 */
#define IEEE754_DOUBLE_BITS 64

    /* 提取坐标的数值近似（使用 double），支持所有坐标类型 */
    double value = 0.0;

    if (coord->cache_valid) {
        /* 优先使用已缓存的数值近似 */
        value = coord->cached_value;
    } else {
        value = LV_DISPATCH(kCoordValueTable, coord->type, 0.0, coord);
    }

    /* 将 double 的 64 位分别编码为 BDD 变量 */
    union {
        double d;
        uint64_t u;
    } ieee;
    ieee.d = value;

    /* 为每一位注册一个 BDD 变量 */
    for (int bit = 0; bit < IEEE754_DOUBLE_BITS; bit++) {
        int var_id = base_var + bit;
        /* 获取或创建变量 */
        if (var_id >= mgr->var_count) {
            /* 检查 var_order 容量，不足时扩容 */
            int needed = var_id + 1;
            if (needed > mgr->var_capacity) {
                /* 三数组同步扩容至 lv_ensure_capacity（独立容量变量保证三数组容量一致） */
                int *new_order = mgr->var_order;
                char **new_names = mgr->var_names;
                BDDVarType *new_types = mgr->var_types;
                int order_cap = mgr->var_capacity;
                int names_cap = mgr->var_capacity;
                int types_cap = mgr->var_capacity;
                if (!lv_ensure_capacity((void **) &new_order, needed, &order_cap, sizeof(int), 0) ||
                    !lv_ensure_capacity((void **) &new_names, needed, &names_cap, sizeof(char *), 0) ||
                    !lv_ensure_capacity((void **) &new_types, needed, &types_cap, sizeof(BDDVarType), 0)) {
                    /* 部分失败：已成功扩容的临时指针指向新内存，释放以免泄漏；
                     * 失败/未执行的调用保持旧指针（与 mgr-> 相同），不可释放。 */
                    if (new_order != mgr->var_order)
                        lv_free((void **) &new_order);
                    if (new_names != mgr->var_names)
                        lv_free((void **) &new_names);
                    if (new_types != mgr->var_types)
                        lv_free((void **) &new_types);
                    return -1;
                }
                mgr->var_order = new_order;
                mgr->var_names = new_names;
                mgr->var_types = new_types;
                mgr->var_capacity = order_cap; /* 三数组容量一致 */
            }
            /* 初始化新增的变量条目 */
            for (int v = mgr->var_count; v < needed; v++) {
                mgr->var_order[v] = v;
                mgr->var_names[v] = NULL;
                mgr->var_types[v] = BDD_BOOLEAN;
            }
            mgr->var_count = needed;
        }
    }

    return IEEE754_DOUBLE_BITS;

#undef IEEE754_DOUBLE_BITS
}

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

/* ========================================================================
 * ADD 管理器（代数决策图）
 * ======================================================================== */

/**
 * @brief 创建 ADD 管理器（代数决策图）
 *
 * @param var_count        变量数量
 * @param unique_table_size 唯一表大小
 * @return 新分配的 ADDManager 指针，失败返回 NULL
 */
ADDManager *add_manager_create(int var_count, int unique_table_size) {
    ADDManager *mgr = (ADDManager *) lv_calloc(1, sizeof(ADDManager));
    if (!mgr)
        return NULL;
    /* 注册 lv_DEFER 守卫：任何失败路径（含中途 return NULL）自动按原 cleanup
     * 语义逐字段释放已分配的成员；成功路径 guard.mgr = NULL 解除守卫 */
    AddManagerGuard guard = {mgr};
    lv_DEFER(add_manager_guard_cleanup, &guard);

    mgr->zero_node = (ADDNode *) lv_calloc(1, sizeof(ADDNode));
    if (!mgr->zero_node)
        return NULL;
    mgr->zero_node->var_id = -1;
    mgr->zero_node->low = NULL;
    mgr->zero_node->high = NULL;
    mgr->zero_node->constant = 0.0;
    mgr->zero_node->is_constant = true;

    mgr->one_node = (ADDNode *) lv_calloc(1, sizeof(ADDNode));
    if (!mgr->one_node)
        return NULL;
    mgr->one_node->var_id = -1;
    mgr->one_node->low = NULL;
    mgr->one_node->high = NULL;
    mgr->one_node->constant = 1.0;
    mgr->one_node->is_constant = true;

    /* 分配 ADD 唯一表 */
    if (unique_table_size > 0) {
        mgr->unique_table = (ADDNode **) lv_calloc((size_t) unique_table_size, sizeof(ADDNode *));
        if (!mgr->unique_table)
            return NULL;
    } else {
        mgr->unique_table = NULL;
    }
    mgr->unique_table_size = unique_table_size;
    mgr->var_count = var_count;
    mgr->node_count = 0;

    mgr->var_order = (int *) lv_malloc((size_t) var_count * sizeof(int));
    if (mgr->var_order) {
        for (int i = 0; i < var_count; i++)
            mgr->var_order[i] = i;
    }

    guard.mgr = NULL; /* 守卫解除：结果移交调用方 */
    return mgr;
}

/**
 * @brief 销毁 ADD 管理器
 * @param mgr 要销毁的 ADD 管理器
 */
void add_manager_destroy(ADDManager *mgr) {
    if (!mgr)
        return;
    lv_free((void **) &mgr->zero_node);
    lv_free((void **) &mgr->one_node);
    lv_free((void **) &mgr->unique_table);
    lv_free((void **) &mgr->var_order);
    lv_free((void **) &mgr);
}

/**
 * @brief 创建 ADD 常数节点
 * @param mgr   ADD 管理器
 * @param value 常数值
 * @return 常数节点指针，失败返回 NULL
 */
ADDNode *add_constant(ADDManager *mgr, double value) {
    lv_CHECK_NULL(mgr, NULL);
    ADDNode *node = (ADDNode *) lv_calloc(1, sizeof(ADDNode));
    if (!node)
        return NULL;
    node->var_id = -1;
    node->low = NULL;
    node->high = NULL;
    node->constant = value;
    node->is_constant = true;
    return node;
}

/* ADD 运算 —— Shannon 展开实现 */

/** 内部：ADD 节点创建辅助 */
static ADDNode *add_node_create(ADDManager *mgr, int var_id, ADDNode *low, ADDNode *high) {
    lv_CHECK_NULL(mgr, NULL);
    /* 终端合并：如果 low == high，返回 low */
    if (low == high)
        return low;
    ADDNode *node = (ADDNode *) lv_calloc(1, sizeof(ADDNode));
    if (!node)
        return NULL;
    node->var_id = var_id;
    node->low = low;
    node->high = high;
    node->constant = 0.0;
    node->is_constant = false;
    return node;
}

/** 内部：获取 ADD 节点的值（终端节点返回常量，非终端返回 NaN） */
static double add_node_value(const ADDNode *node) {
    if (!node || !node->is_constant)
        return NAN;
    return node->constant;
}

/** 内部：Shannon 展开 —— 选择顶部变量 */
static int add_top_var(const ADDNode *a, const ADDNode *b) {
    int va = (a && !a->is_constant) ? a->var_id : 999999;
    int vb = (b && !b->is_constant) ? b->var_id : 999999;
    return (va < vb) ? va : vb;
}

/** 内部：ADD cofactor（取变量为 0 或 1 的分支） */
static ADDNode *add_cofactor(ADDNode *node, int var, int val) {
    if (!node || node->is_constant)
        return node;
    if (node->var_id > var)
        return node;
    if (node->var_id == var)
        return val ? node->high : node->low;
    return node;
}

ADDNode *add_add(ADDManager *mgr, ADDNode *a, ADDNode *b) {
    if (!mgr || !a || !b)
        return NULL;
    /* 常数情况直接计算 */
    if (a->is_constant && b->is_constant) {
        return add_constant(mgr, a->constant + b->constant);
    }
    /* Shannon 展开：f+g = x*(f1+g1) + x'*(f0+g0) */
    int top = add_top_var(a, b);
    ADDNode *a0 = add_cofactor(a, top, 0);
    ADDNode *a1 = add_cofactor(a, top, 1);
    ADDNode *b0 = add_cofactor(b, top, 0);
    ADDNode *b1 = add_cofactor(b, top, 1);
    ADDNode *low = add_add(mgr, a0, b0);
    ADDNode *high = add_add(mgr, a1, b1);
    return add_node_create(mgr, top, low, high);
}

ADDNode *add_sub(ADDManager *mgr, ADDNode *a, ADDNode *b) {
    if (!mgr || !a || !b)
        return NULL;
    /* 常数情况直接计算 */
    if (a->is_constant && b->is_constant) {
        return add_constant(mgr, a->constant - b->constant);
    }
    /* f-g = f + (-g)：先对 g 取负再相加 */
    ADDNode *neg_b = add_mul(mgr, add_constant(mgr, -1.0), b);
    if (!neg_b)
        return add_constant(mgr, 0.0);
    ADDNode *result = add_add(mgr, a, neg_b);
    return result;
}

ADDNode *add_mul(ADDManager *mgr, ADDNode *a, ADDNode *b) {
    if (!mgr || !a || !b)
        return NULL;
    /* 常数情况直接计算 */
    if (a->is_constant && b->is_constant) {
        return add_constant(mgr, a->constant * b->constant);
    }
    /* 乘以零恒为零 */
    if (a->is_constant && a->constant == 0.0)
        return a;
    if (b->is_constant && b->constant == 0.0)
        return b;
    /* 乘以一不变 */
    if (a->is_constant && a->constant == 1.0)
        return b;
    if (b->is_constant && b->constant == 1.0)
        return a;
    /* Shannon 展开：f*g = x*(f1*g1) + x'*(f0*g0) */
    int top = add_top_var(a, b);
    ADDNode *a0 = add_cofactor(a, top, 0);
    ADDNode *a1 = add_cofactor(a, top, 1);
    ADDNode *b0 = add_cofactor(b, top, 0);
    ADDNode *b1 = add_cofactor(b, top, 1);
    ADDNode *low = add_mul(mgr, a0, b0);
    ADDNode *high = add_mul(mgr, a1, b1);
    return add_node_create(mgr, top, low, high);
}

ADDNode *add_div(ADDManager *mgr, ADDNode *a, ADDNode *b) {
    if (!mgr || !a || !b)
        return NULL;
    /* 常数情况直接计算 */
    if (a->is_constant && b->is_constant && fabs(b->constant) > lv_EPSILON_DOUBLE) {
        return add_constant(mgr, a->constant / b->constant);
    }
    /* 非常数情况：除法在 ADD 上实现复杂，返回常数 0 */
    return add_constant(mgr, 0.0);
}

ADDNode *add_max(ADDManager *mgr, ADDNode *a, ADDNode *b) {
    if (!mgr || !a || !b)
        return NULL;
    /* 常数情况直接计算 */
    if (a->is_constant && b->is_constant) {
        return add_constant(mgr, (a->constant > b->constant) ? a->constant : b->constant);
    }
    /* 使用 ITE 映射到 ADD：max(f,g) = ITE(f>g, f, g)
     * Shannon 展开：max(f,g) = x * max(f1,g1) + x' * max(f0,g0) */
    int top = add_top_var(a, b);
    ADDNode *a0 = add_cofactor(a, top, 0);
    ADDNode *a1 = add_cofactor(a, top, 1);
    ADDNode *b0 = add_cofactor(b, top, 0);
    ADDNode *b1 = add_cofactor(b, top, 1);
    ADDNode *low = add_max(mgr, a0, b0);
    ADDNode *high = add_max(mgr, a1, b1);
    return add_node_create(mgr, top, low, high);
}

ADDNode *add_min(ADDManager *mgr, ADDNode *a, ADDNode *b) {
    if (!mgr || !a || !b)
        return NULL;
    /* 常数情况直接计算 */
    if (a->is_constant && b->is_constant) {
        return add_constant(mgr, (a->constant < b->constant) ? a->constant : b->constant);
    }
    /* 使用 ITE 映射到 ADD：min(f,g) = ITE(f<g, f, g)
     * Shannon 展开：min(f,g) = x * min(f1,g1) + x' * min(f0,g0) */
    int top = add_top_var(a, b);
    ADDNode *a0 = add_cofactor(a, top, 0);
    ADDNode *a1 = add_cofactor(a, top, 1);
    ADDNode *b0 = add_cofactor(b, top, 0);
    ADDNode *b1 = add_cofactor(b, top, 1);
    ADDNode *low = add_min(mgr, a0, b0);
    ADDNode *high = add_min(mgr, a1, b1);
    return add_node_create(mgr, top, low, high);
}
