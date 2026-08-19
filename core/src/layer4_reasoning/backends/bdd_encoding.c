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
#include "bdd_encoding_internal.h"

/* ========================================================================
 * 内部：唯一表哈希
 * ======================================================================== */

/** 墓碑标记 —— 用于开放寻址哈希表中标记已删除的槽位，保护探查链 */
BDDNode bdd_tombstone_marker = {-2, NULL, NULL, 0, false};
#define BDD_TOMBSTONE (&bdd_tombstone_marker)

/** 节点三元组哈希 (var_id, low, high) -> 唯一表索引
 *  exempt: 判据「哈希表族收敛」——唯一表为三元组键 (var_id, low, high) +
 *  墓碑 + 引用计数的开放寻址表，语义不同于 lv_hashtable_int_hash 的整型键，保留。
 */
int bdd_unique_hash(int var_id, BDDNode *low, BDDNode *high, int table_size) {
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
        lv_snprintf(auto_name, sizeof(auto_name), "bdd_var_%d", id);
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

    /* 非终端：按 top_var 做 Shannon 展开（var_id 匹配的节点取对应分支，否则整节点透传） */
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

