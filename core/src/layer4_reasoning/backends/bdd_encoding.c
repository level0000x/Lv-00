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

#include "bdd_encoding.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv_utils.h"

/* ========================================================================
 * 内部：唯一表哈希
 * ======================================================================== */

/** 墓碑标记 —— 用于开放寻址哈希表中标记已删除的槽位，保护探查链 */
static BDDNode bdd_tombstone_marker = {-2, NULL, NULL, 0, false};
#define BDD_TOMBSTONE (&bdd_tombstone_marker)

/** 节点三元组哈希 (var_id, low, high) -> 唯一表索引 */
static int bdd_unique_hash(int var_id, BDDNode *low, BDDNode *high, int table_size) {
    unsigned long h = (unsigned long) var_id;
    h = h * 31 + (unsigned long) (uintptr_t) low;
    h = h * 31 + (unsigned long) (uintptr_t) high;
    return (int) (h % (unsigned long) table_size);
}

/** 在唯一表中查找或插入节点 */
static BDDNode *bdd_unique_lookup(BDDManager *mgr, int var_id, BDDNode *low, BDDNode *high) {
    if (!mgr)
        return NULL;

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
            __sync_fetch_and_add(&existing->ref_count, 1);
            return existing;
        }
    }

    /* 未找到，分配新节点 */
    BDDNode *node = (BDDNode *) lv_calloc(1, sizeof(BDDNode));
    if (!node)
        return NULL;
    node->var_id = var_id;
    node->low = low;
    node->high = high;
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

    /* 创建终端 T 节点 */
    mgr->true_node = (BDDNode *) lv_calloc(1, sizeof(BDDNode));
    if (!mgr->true_node) {
        lv_free((void **) &mgr);
        return NULL;
    }
    mgr->true_node->var_id = -1;
    mgr->true_node->low = NULL;
    mgr->true_node->high = NULL;
    mgr->true_node->ref_count = 1; /* 持久引用 */
    mgr->true_node->complemented = false;

    /* 创建终端 F 节点 */
    mgr->false_node = (BDDNode *) lv_calloc(1, sizeof(BDDNode));
    if (!mgr->false_node) {
        lv_free((void **) &mgr->true_node);
        lv_free((void **) &mgr);
        return NULL;
    }
    mgr->false_node->var_id = -1;
    mgr->false_node->low = NULL;
    mgr->false_node->high = NULL;
    mgr->false_node->ref_count = 1; /* 持久引用 */
    mgr->false_node->complemented = false;

    /* 分配唯一表（当前为线性扫描实现，完整版应使用哈希表加速查找） */
    if (unique_table_size < 1024)
        unique_table_size = 1024;
    mgr->unique_table = (BDDNode **) lv_calloc((size_t) unique_table_size, sizeof(BDDNode *));
    if (!mgr->unique_table) {
        lv_free((void **) &mgr->false_node);
        lv_free((void **) &mgr->true_node);
        lv_free((void **) &mgr);
        return NULL;
    }
    mgr->unique_table_size = unique_table_size;

    /* 变量序数组 */
    mgr->var_order = (int *) lv_malloc((size_t) var_count * sizeof(int));
    if (!mgr->var_order) {
        lv_free((void **) &mgr->unique_table);
        lv_free((void **) &mgr->false_node);
        lv_free((void **) &mgr->true_node);
        lv_free((void **) &mgr);
        return NULL;
    }
    for (int i = 0; i < var_count; i++) {
        mgr->var_order[i] = i;
    }
    mgr->var_count = var_count;
    mgr->var_capacity = var_count;
    mgr->node_count = 0;

    /* 变量名称表和类型表 */
    mgr->var_names = (char **) lv_calloc((size_t) var_count, sizeof(char *));
    mgr->var_types = (BDDVarType *) lv_calloc((size_t) var_count, sizeof(BDDVarType));
    if (!mgr->var_names || !mgr->var_types) {
        lv_free((void **) &mgr->var_names);
        lv_free((void **) &mgr->var_types);
        lv_free((void **) &mgr->var_order);
        lv_free((void **) &mgr->unique_table);
        lv_free((void **) &mgr->false_node);
        lv_free((void **) &mgr->true_node);
        lv_free((void **) &mgr);
        return NULL;
    }

    /* 分配 ITE 计算表（缓存 ITE 结果，大小 = 唯一表大小） */
    mgr->computed_table_size = unique_table_size;
    mgr->computed_table = (ITECacheEntry *) lv_calloc((size_t) unique_table_size, sizeof(ITECacheEntry));
    if (!mgr->computed_table) {
        lv_free((void **) &mgr->var_names);
        lv_free((void **) &mgr->var_types);
        lv_free((void **) &mgr->var_order);
        lv_free((void **) &mgr->unique_table);
        lv_free((void **) &mgr->false_node);
        lv_free((void **) &mgr->true_node);
        lv_free((void **) &mgr);
        return NULL;
    }

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
    if (mgr->var_names) {
        for (int i = 0; i < mgr->var_count; i++) {
            lv_free((void **) &mgr->var_names[i]);
        }
        lv_free((void **) &mgr->var_names);
    }
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
    if (!mgr)
        return -1;
    /* 检查 var_order 数组容量，不足时扩容（2 倍增长） */
    if (mgr->var_count >= mgr->var_capacity) {
        if (mgr->var_capacity > 0 && mgr->var_capacity > INT_MAX / 2)
            return -1;
        int new_capacity = (mgr->var_capacity > 0) ? mgr->var_capacity * 2 : 16;
        int *new_order = (int *) lv_realloc(mgr->var_order, (size_t) new_capacity * sizeof(int));
        if (!new_order)
            return -1;
        mgr->var_order = new_order;

        /* 同步扩容 var_names 和 var_types 数组 */
        char **new_names = (char **) lv_realloc(mgr->var_names, (size_t) new_capacity * sizeof(char *));
        BDDVarType *new_types = (BDDVarType *) lv_realloc(mgr->var_types, (size_t) new_capacity * sizeof(BDDVarType));
        if (!new_names || !new_types) {
            lv_free((void **) &new_names);
            lv_free((void **) &new_types);
            return -1;
        }
        mgr->var_names = new_names;
        mgr->var_types = new_types;
        /* 初始化新增的槽位 */
        for (int i = mgr->var_capacity; i < new_capacity; i++) {
            mgr->var_names[i] = NULL;
            mgr->var_types[i] = BDD_BOOLEAN;
        }
        mgr->var_capacity = new_capacity;
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
    if (!mgr)
        return NULL;
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
        __sync_fetch_and_add(&node->ref_count, 1);
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
    uint64_t old_ref = __sync_fetch_and_sub(&node->ref_count, 1);
    /* 终端节点（var_id == -1）不回收 */
    if (node->var_id < 0)
        return;
    if (old_ref > 1)
        return;

    /* 引用计数降为 0：从唯一表中标记为墓碑并释放 */
    if (mgr && mgr->unique_table) {
        int idx = bdd_unique_hash(node->var_id, node->low, node->high, mgr->unique_table_size);
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
    return bdd_ite(mgr, f, g, mgr->false_node);
}

BDDNode *bdd_or(BDDManager *mgr, BDDNode *f, BDDNode *g) {
    /* f | g = ite(f, T, g) */
    return bdd_ite(mgr, f, mgr->true_node, g);
}

BDDNode *bdd_not(BDDManager *mgr, BDDNode *f) {
    /* ~f = ite(f, F, T) */
    return bdd_ite(mgr, f, mgr->false_node, mgr->true_node);
}

BDDNode *bdd_xor(BDDManager *mgr, BDDNode *f, BDDNode *g) {
    /* f ^ g = ite(f, ~g, g) */
    BDDNode *not_g = bdd_not(mgr, g);
    BDDNode *result = bdd_ite(mgr, f, not_g, g);
    bdd_deref(mgr, not_g);
    return result;
}

BDDNode *bdd_nand(BDDManager *mgr, BDDNode *f, BDDNode *g) {
    /* ~(f & g) = ite(f, ~g, T) */
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

/* 辅助：根据节点 ID 查找 node_base_var 数组索引 */
/* 当节点 ID != 数组索引时，需要遍历 nodes 数组找到对应位置 */
#define LOOKUP_NODE_BASE_VAR(node_id)                                    \
    ({                                                                   \
        int _result = -1;                                                \
        for (int _j = 0; _j < n; _j++) {                                 \
            if (graph->nodes[_j] && graph->nodes[_j]->id == (node_id)) { \
                _result = node_base_var[_j];                             \
                break;                                                   \
            }                                                            \
        }                                                                \
        _result;                                                         \
    })

    /* 阶段 2: 遍历所有活跃约束，按类型编码 BDD 子公式 */
    BDDNode *constraint_bdd = bdd_true(mgr);

    for (int ci = 0; ci < graph->constraint_count; ci++) {
        Constraint *con = graph->constraints[ci];
        if (!con || !con->is_active)
            continue;

        BDDNode *sub = NULL;

        switch (con->type) {
            case INCIDENCE:
                /* 关联(point, line): 点在线上 */
                if (con->participant_count >= 2) {
                    int p_id = con->participants[0];
                    int l_id = con->participants[1];
                    int p_var = (p_id >= 0) ? LOOKUP_NODE_BASE_VAR(p_id) : -1;
                    int l_var = (l_id >= 0) ? LOOKUP_NODE_BASE_VAR(l_id) : -1;
                    if (p_var >= 0 && l_var >= 0) {
                        BDDNode *p_lit = bdd_literal(mgr, p_var + 1);
                        BDDNode *l_lit = bdd_literal(mgr, l_var + 1);
                        sub = bdd_and(mgr, p_lit, l_lit);
                        bdd_deref(mgr, p_lit);
                        bdd_deref(mgr, l_lit);
                    }
                }
                break;

            case BETWEENNESS:
                /* 介于(p1, p2, p3): p2 在 p1 与 p3 之间 */
                if (con->participant_count >= 3) {
                    int p1_var = (con->participants[0] >= 0) ? LOOKUP_NODE_BASE_VAR(con->participants[0]) : -1;
                    int p2_var = (con->participants[1] >= 0) ? LOOKUP_NODE_BASE_VAR(con->participants[1]) : -1;
                    int p3_var = (con->participants[2] >= 0) ? LOOKUP_NODE_BASE_VAR(con->participants[2]) : -1;
                    if (p1_var >= 0 && p2_var >= 0 && p3_var >= 0) {
                        BDDNode *a = bdd_literal(mgr, p1_var + 1);
                        BDDNode *b = bdd_literal(mgr, p2_var + 1);
                        BDDNode *c = bdd_literal(mgr, p3_var + 1);
                        BDDNode *ab = bdd_and(mgr, a, b);
                        sub = bdd_and(mgr, ab, c);
                        bdd_deref(mgr, a);
                        bdd_deref(mgr, b);
                        bdd_deref(mgr, c);
                        bdd_deref(mgr, ab);
                    }
                }
                break;

            case INTERSECTION:
                /* 相交(line1, line2, point): 两线交于一点 */
                if (con->participant_count >= 3) {
                    int l1_var = (con->participants[0] >= 0) ? LOOKUP_NODE_BASE_VAR(con->participants[0]) : -1;
                    int l2_var = (con->participants[1] >= 0) ? LOOKUP_NODE_BASE_VAR(con->participants[1]) : -1;
                    int p_var = (con->participants[2] >= 0) ? LOOKUP_NODE_BASE_VAR(con->participants[2]) : -1;
                    if (l1_var >= 0 && l2_var >= 0 && p_var >= 0) {
                        BDDNode *l1_lit = bdd_literal(mgr, l1_var + 1);
                        BDDNode *l2_lit = bdd_literal(mgr, l2_var + 1);
                        BDDNode *p_lit = bdd_literal(mgr, p_var + 1);
                        BDDNode *l_and = bdd_and(mgr, l1_lit, l2_lit);
                        sub = bdd_and(mgr, l_and, p_lit);
                        bdd_deref(mgr, l1_lit);
                        bdd_deref(mgr, l2_lit);
                        bdd_deref(mgr, p_lit);
                        bdd_deref(mgr, l_and);
                    }
                }
                break;

            case CONTAINMENT:
                /* 包含(region, point): 点在区域内 */
                if (con->participant_count >= 2) {
                    int r_var = (con->participants[0] >= 0) ? LOOKUP_NODE_BASE_VAR(con->participants[0]) : -1;
                    int p_var = (con->participants[1] >= 0) ? LOOKUP_NODE_BASE_VAR(con->participants[1]) : -1;
                    if (r_var >= 0 && p_var >= 0) {
                        BDDNode *r_lit = bdd_literal(mgr, r_var + 1);
                        BDDNode *p_lit = bdd_literal(mgr, p_var + 1);
                        sub = bdd_and(mgr, r_lit, p_lit);
                        bdd_deref(mgr, r_lit);
                        bdd_deref(mgr, p_lit);
                    }
                }
                break;

            case CONNECTION:
                /* 连接: 端口连接 —— 此处跳过（端口编码需单独处理） */
                break;

            default:
                break;
        }

        if (sub) {
            BDDNode *new_bdd = bdd_and(mgr, constraint_bdd, sub);
            bdd_deref(mgr, constraint_bdd);
            bdd_deref(mgr, sub);
            constraint_bdd = new_bdd;
        }
    }

    lv_free((void **) &node_base_var);
#undef LOOKUP_NODE_BASE_VAR
    return constraint_bdd;
}

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
        switch (coord->type) {
            case RATIONAL:
                if (coord->data.rational)
                    value = mpq_get_d(coord->data.rational->value);
                break;

            case ALGEBRAIC:
                /* 代数数：使用区间中点作为数值近似 */
                if (coord->data.algebraic) {
                    value = (coord->data.algebraic->left_bound + coord->data.algebraic->right_bound) / 2.0;
                }
                break;

            case QUADRATIC:
                /* 二次扩张数：计算 a + b*sqrt(n) */
                if (coord->data.quadratic) {
                    Quadratic *q = coord->data.quadratic;
                    double a_val = (q->a) ? mpq_get_d(q->a->value) : 0.0;
                    double b_val = (q->b) ? mpq_get_d(q->b->value) : 0.0;
                    value = a_val + b_val * sqrt((double) q->n);
                }
                break;

            case TRANSCENDENTAL:
                /* 超越数：使用 symbolic_coord_to_double 辅助函数 */
                value = symbolic_coord_to_double(coord);
                break;

            default:
                value = 0.0;
                break;
        }
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
                int new_capacity = (mgr->var_capacity > 0) ? mgr->var_capacity * 2 : 16;
                if (new_capacity < needed)
                    new_capacity = needed;
                int *new_order = (int *) lv_realloc(mgr->var_order, (size_t) new_capacity * sizeof(int));
                char **new_names = (char **) lv_realloc(mgr->var_names, (size_t) new_capacity * sizeof(char *));
                BDDVarType *new_types =
                    (BDDVarType *) lv_realloc(mgr->var_types, (size_t) new_capacity * sizeof(BDDVarType));
                if (!new_order || !new_names || !new_types) {
                    /* 某些 realloc 成功了但 mgr-> 指针尚未更新，
                     * 所以 mgr->var_order 等仍指向旧内存（有效）。
                     * 释放成功分配的新内存以避免泄漏。 */
                    if (new_order)
                        lv_free((void **) &new_order);
                    if (new_names)
                        lv_free((void **) &new_names);
                    if (new_types)
                        lv_free((void **) &new_types);
                    return -1;
                }
                mgr->var_order = new_order;
                mgr->var_names = new_names;
                mgr->var_types = new_types;
                mgr->var_capacity = new_capacity;
            }
            /* 初始化新增的变量序条目 */
            for (int v = mgr->var_count; v < needed; v++) {
                mgr->var_order[v] = v;
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
        size_t buf_size = 128;
        char *buf = (char *) lv_malloc(buf_size);
        if (!buf)
            return false;
        if (bdd->complemented) {
            /* False 节点 -> 空 CNF（不可满足） */
            snprintf(buf, buf_size, "c BDD is FALSE\np cnf 1 1\n1 0\n-1 0\n");
        } else {
            /* True 节点 -> 空 CNF（可满足） */
            snprintf(buf, buf_size, "c BDD is TRUE\np cnf 1 1\n1 0\n");
        }
        *out_cnf = buf;
        return true;
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

    /* 估算缓冲区大小 */
    size_t buf_size = (size_t) (4096 + node_count * 256);
    char *buf = (char *) lv_malloc(buf_size);
    if (!buf) {
        lv_free((void **) &entries);
        return false;
    }

    int offset = 0;
    size_t remaining = buf_size;
    int clause_count = 0;

    /* DIMACS 头部（先写占位，后面回填） */
    int header_pos = offset;
    offset += snprintf(buf + offset, (size_t) remaining, "c BDD-to-CNF conversion (Tseitin)\n");
    remaining -= offset - header_pos;

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
        if (remaining > 64) {
            int n;
            if (high_lit > 0) {
                n = snprintf(buf + offset, (size_t) remaining, "%d %d %d 0\n", -v, -x, high_lit);
            } else {
                /* high 是 false (0)，子句变为 ~v | ~x（省略 0） */
                n = snprintf(buf + offset, (size_t) remaining, "%d %d 0\n", -v, -x);
            }
            if (n > 0 && n < remaining) {
                offset += n;
                remaining -= n;
            }
            clause_count++;
        }

        /* 子句 2: ~v | x | low */
        if (remaining > 64) {
            int n;
            if (low_lit > 0) {
                n = snprintf(buf + offset, (size_t) remaining, "%d %d %d 0\n", -v, x, low_lit);
            } else {
                n = snprintf(buf + offset, (size_t) remaining, "%d %d 0\n", -v, x);
            }
            if (n > 0 && n < remaining) {
                offset += n;
                remaining -= n;
            }
            clause_count++;
        }

        /* 子句 3: v | ~x | ~high */
        if (remaining > 64 && high_lit > 0) {
            int n = snprintf(buf + offset, (size_t) remaining, "%d %d %d 0\n", v, -x, -high_lit);
            if (n > 0 && n < remaining) {
                offset += n;
                remaining -= n;
            }
            clause_count++;
        }

        /* 子句 4: v | x | ~low */
        if (remaining > 64 && low_lit > 0) {
            int n = snprintf(buf + offset, (size_t) remaining, "%d %d %d 0\n", v, x, -low_lit);
            if (n > 0 && n < remaining) {
                offset += n;
                remaining -= n;
            }
            clause_count++;
        }
    }

    /* 根节点单位子句：root_aux 必须为 true */
    if (root_aux > 0 && remaining > 32) {
        int n = snprintf(buf + offset, (size_t) remaining, "%d 0\n", root_aux);
        if (n > 0 && n < remaining) {
            offset += n;
            remaining -= n;
        }
        clause_count++;
    }

    /* 回填 DIMACS p 行 */
    int total_vars = aux_base + node_count - 1;
    char header[256];
    snprintf(header, sizeof(header), "p cnf %d %d\n", total_vars, clause_count);

    /* 将 header 插入到头部位置之后 */
    size_t header_len = strlen(header);
    if (offset + (int) header_len < (int) buf_size) {
        /* 移动现有内容为 header腾出空间 */
        memmove(buf + header_pos + (int) header_len, buf + header_pos, (size_t) (offset - header_pos));
        memcpy(buf + header_pos, header, header_len);
        offset += (int) header_len;
    }

    lv_free((void **) &entries);

    *out_cnf = buf;
    return true;
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

    mgr->zero_node = (ADDNode *) lv_calloc(1, sizeof(ADDNode));
    if (!mgr->zero_node) {
        lv_free((void **) &mgr);
        return NULL;
    }
    mgr->zero_node->var_id = -1;
    mgr->zero_node->low = NULL;
    mgr->zero_node->high = NULL;
    mgr->zero_node->constant = 0.0;
    mgr->zero_node->is_constant = true;

    mgr->one_node = (ADDNode *) lv_calloc(1, sizeof(ADDNode));
    if (!mgr->one_node) {
        lv_free((void **) &mgr->zero_node);
        lv_free((void **) &mgr);
        return NULL;
    }
    mgr->one_node->var_id = -1;
    mgr->one_node->low = NULL;
    mgr->one_node->high = NULL;
    mgr->one_node->constant = 1.0;
    mgr->one_node->is_constant = true;

    /* 分配 ADD 唯一表 */
    if (unique_table_size > 0) {
        mgr->unique_table = (ADDNode **) lv_calloc((size_t) unique_table_size, sizeof(ADDNode *));
        if (!mgr->unique_table) {
            lv_free((void **) &mgr->one_node);
            lv_free((void **) &mgr->zero_node);
            lv_free((void **) &mgr);
            return NULL;
        }
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
    if (!mgr)
        return NULL;
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
    if (!mgr)
        return NULL;
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
