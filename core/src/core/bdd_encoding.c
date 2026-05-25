/**
 * @file bdd_encoding.c
 * @brief CUDD 二阶策略图编码 —— 桩实现
 *
 * 提供 BDD/ADD 的基本操作实现，包括布尔运算、变量序优化、
 * 约束图 -> BDD 编码和坐标 bit-blasting。
 *
 * @version v3.3.0
 * @date 2026-05-24
 */

#include "bdd_encoding.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00_utils.h"

/* ========================================================================
 * 内部：唯一表哈希
 * ======================================================================== */

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
    /* 线性探测 */
    for (int probe = 0; probe < mgr->unique_table_size; probe++) {
        int slot = (idx + probe) % mgr->unique_table_size;
        BDDNode *existing = mgr->unique_table[slot];
        if (existing == NULL)
            break; /* 空槽，未找到 */
        if (existing->var_id == var_id &&
            existing->low == low &&
            existing->high == high) {
            /* 找到已存在节点，增加引用计数并返回 */
            existing->ref_count++;
            return existing;
        }
    }

    /* 未找到，分配新节点 */
    BDDNode *node = (BDDNode *) lv00_malloc(sizeof(BDDNode));
    if (!node)
        return NULL;
    node->var_id = var_id;
    node->low = low;
    node->high = high;
    node->ref_count = 1; /* 初始引用计数为 1（调用者持有） */
    node->complemented = false;
    mgr->node_count++;

    /* 插入唯一表 */
    int slot = bdd_unique_hash(var_id, low, high, mgr->unique_table_size);
    for (int probe = 0; probe < mgr->unique_table_size; probe++) {
        int s = (slot + probe) % mgr->unique_table_size;
        if (mgr->unique_table[s] == NULL) {
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
    BDDManager *mgr = (BDDManager *) lv00_malloc(sizeof(BDDManager));
    if (!mgr)
        return NULL;

    /* 创建终端 T 节点 */
    mgr->true_node = (BDDNode *) lv00_malloc(sizeof(BDDNode));
    if (!mgr->true_node) {
        lv00_free((void **)&mgr);
        return NULL;
    }
    mgr->true_node->var_id = -1;
    mgr->true_node->low = NULL;
    mgr->true_node->high = NULL;
    mgr->true_node->ref_count = 1; /* 持久引用 */
    mgr->true_node->complemented = false;

    /* 创建终端 F 节点 */
    mgr->false_node = (BDDNode *) lv00_malloc(sizeof(BDDNode));
    if (!mgr->false_node) {
        lv00_free((void **)&mgr->true_node);
        lv00_free((void **)&mgr);
        return NULL;
    }
    mgr->false_node->var_id = -1;
    mgr->false_node->low = NULL;
    mgr->false_node->high = NULL;
    mgr->false_node->ref_count = 1; /* 持久引用 */
    mgr->false_node->complemented = false;

    /* 分配唯一表（桩实现中不使用哈希，仅占位） */
    if (unique_table_size < 1024)
        unique_table_size = 1024;
    mgr->unique_table = (BDDNode **) lv00_calloc((size_t) unique_table_size, sizeof(BDDNode *));
    if (!mgr->unique_table) {
        lv00_free((void **)&mgr->false_node);
        lv00_free((void **)&mgr->true_node);
        lv00_free((void **)&mgr);
        return NULL;
    }
    mgr->unique_table_size = unique_table_size;

    /* 变量序数组 */
    mgr->var_order = (int *) lv00_malloc((size_t) var_count * sizeof(int));
    if (!mgr->var_order) {
        lv00_free((void **)&mgr->unique_table);
        lv00_free((void **)&mgr->false_node);
        lv00_free((void **)&mgr->true_node);
        lv00_free((void **)&mgr);
        return NULL;
    }
    for (int i = 0; i < var_count; i++) {
        mgr->var_order[i] = i;
    }
    mgr->var_count = var_count;
    mgr->var_capacity = var_count;
    mgr->node_count = 0;

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
            if (mgr->unique_table[i] != NULL) {
                lv00_free((void **)&mgr->unique_table[i]);
            }
        }
    }
    lv00_free((void **)&mgr->true_node);
    lv00_free((void **)&mgr->false_node);
    lv00_free((void **)&mgr->unique_table);
    lv00_free((void **)&mgr->var_order);
    lv00_free((void **)&mgr);
}

/**
 * @brief 在 BDD 管理器中创建新变量
 *
 * @param mgr  BDD 管理器
 * @param name 变量名称（当前未使用）
 * @param type 变量类型（当前未使用）
 * @return 新变量的 ID，失败返回 -1
 */
int bdd_new_var(BDDManager *mgr, const char *name, BDDVarType type) {
    (void) name;
    (void) type;
    if (!mgr)
        return -1;
    /* 检查 var_order 数组容量，不足时扩容（2 倍增长） */
    if (mgr->var_count >= mgr->var_capacity) {
        int new_capacity = (mgr->var_capacity > 0) ? mgr->var_capacity * 2 : 16;
        int *new_order = (int *) lv00_realloc(mgr->var_order,
                                               (size_t) new_capacity * sizeof(int));
        if (!new_order)
            return -1;
        mgr->var_order = new_order;
        mgr->var_capacity = new_capacity;
    }
    int id = mgr->var_count;
    mgr->var_order[id] = id;
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
    if (node)
        node->ref_count++;
}

/**
 * @brief 减少节点引用计数，为 0 时从唯一表移除并释放
 * @param mgr  BDD 管理器
 * @param node BDD 节点
 */
void bdd_deref(BDDManager *mgr, BDDNode *node) {
    if (!node || node->ref_count == 0)
        return;
    node->ref_count--;
    /* 终端节点（var_id == -1）不回收 */
    if (node->var_id < 0)
        return;
    if (node->ref_count > 0)
        return;

    /* 引用计数降为 0：从唯一表中移除并释放 */
    if (mgr && mgr->unique_table) {
        int idx = bdd_unique_hash(node->var_id, node->low, node->high,
                                  mgr->unique_table_size);
        for (int probe = 0; probe < mgr->unique_table_size; probe++) {
            int slot = (idx + probe) % mgr->unique_table_size;
            if (mgr->unique_table[slot] == node) {
                mgr->unique_table[slot] = NULL;
                break;
            }
            if (mgr->unique_table[slot] == NULL)
                break;
        }
        mgr->node_count--;
    }
    lv00_free((void **)&node);
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

BDDNode *bdd_ite(BDDManager *mgr, BDDNode *f, BDDNode *g, BDDNode *h) {
    if (!mgr || !f || !g || !h)
        return NULL;

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
    int *best_order = (int *) lv00_malloc((size_t) n * sizeof(int));
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

            /* 桩：使用启发式近似评估（完整实现需重建 BDD） */
            uint64_t est_nodes = orig_nodes;
            if (insert_pos == orig_pos) {
                est_nodes = orig_nodes;
            } else {
                /* 离原位置越远，惩罚越大（简化启发式） */
                int dist = (insert_pos > orig_pos) ? (insert_pos - orig_pos) : (orig_pos - insert_pos);
                est_nodes = orig_nodes + (uint64_t) dist * 2;
            }

            if (est_nodes < best_nodes) {
                best_nodes = est_nodes;
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

    lv00_free((void **)&best_order);
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

    /* 简化版：对所有节点变量做 AND 的 BDD */
    BDDNode *result = bdd_true(mgr);

    for (int i = 0; i < n; i++) {
        int var_id = i + 1; /* 变量 ID 从 1 开始 */
        BDDNode *lit = bdd_literal(mgr, var_id);
        if (!lit)
            continue;

        BDDNode *new_result = bdd_and(mgr, result, lit);
        bdd_deref(mgr, result);
        bdd_deref(mgr, lit);
        result = new_result;
    }

    return result;
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

    /* 提取坐标的数值近似（使用 double） */
    double value = 0.0;
    if (coord->type == RATIONAL && coord->data.rational) {
        value = mpq_get_d(coord->data.rational->value);
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
                int *new_order = (int *) lv00_realloc(mgr->var_order,
                                                       (size_t) new_capacity * sizeof(int));
                if (!new_order)
                    return -1;
                mgr->var_order = new_order;
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
 * bdd_to_cnf —— BDD -> DIMACS CNF
 *
 * 使用 Tseitin 变换：对 BDD 中每个非终端节点引入辅助变量。
 * 节点 v = ITE(var, low, high) 的 Tseitin 编码：
 *   (~v | ~var | high) & (~v | var | low) & (v | ~var | ~high) & (v | var | ~low)
 * ======================================================================== */

bool bdd_to_cnf(BDDNode *bdd, char **out_cnf) {
    if (!bdd || !out_cnf)
        return false;

    /* 桩实现：生成骨架 CNF */
    size_t buf_size = 4096;
    char *buf = (char *) lv00_malloc(buf_size);
    if (!buf)
        return false;

    /* DIMACS 头部 */
    int offset = snprintf(buf, buf_size,
                          "c BDD-to-CNF conversion (stub)\n"
                          "p cnf 0 0\n");

    *out_cnf = buf;
    return true;
}

/* ========================================================================
 * ADD 管理器（桩实现）
 * ======================================================================== */

/**
 * @brief 创建 ADD 管理器（代数决策图）
 *
 * @param var_count        变量数量
 * @param unique_table_size 唯一表大小
 * @return 新分配的 ADDManager 指针，失败返回 NULL
 */
ADDManager *add_manager_create(int var_count, int unique_table_size) {
    ADDManager *mgr = (ADDManager *) lv00_malloc(sizeof(ADDManager));
    if (!mgr)
        return NULL;

    mgr->zero_node = (ADDNode *) lv00_malloc(sizeof(ADDNode));
    if (!mgr->zero_node) {
        lv00_free((void **)&mgr);
        return NULL;
    }
    mgr->zero_node->var_id = -1;
    mgr->zero_node->low = NULL;
    mgr->zero_node->high = NULL;
    mgr->zero_node->constant = 0.0;
    mgr->zero_node->is_constant = true;

    mgr->one_node = (ADDNode *) lv00_malloc(sizeof(ADDNode));
    if (!mgr->one_node) {
        lv00_free((void **)&mgr->zero_node);
        lv00_free((void **)&mgr);
        return NULL;
    }
    mgr->one_node->var_id = -1;
    mgr->one_node->low = NULL;
    mgr->one_node->high = NULL;
    mgr->one_node->constant = 1.0;
    mgr->one_node->is_constant = true;

    mgr->unique_table = NULL;
    mgr->unique_table_size = unique_table_size;
    mgr->var_count = var_count;
    mgr->node_count = 0;

    mgr->var_order = (int *) lv00_malloc((size_t) var_count * sizeof(int));
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
    lv00_free((void **)&mgr->zero_node);
    lv00_free((void **)&mgr->one_node);
    lv00_free((void **)&mgr->unique_table);
    lv00_free((void **)&mgr->var_order);
    lv00_free((void **)&mgr);
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
    ADDNode *node = (ADDNode *) lv00_malloc(sizeof(ADDNode));
    if (!node)
        return NULL;
    node->var_id = -1;
    node->low = NULL;
    node->high = NULL;
    node->constant = value;
    node->is_constant = true;
    return node;
}

/* ADD 运算 —— 桩实现 */
ADDNode *add_add(ADDManager *mgr, ADDNode *a, ADDNode *b) {
    if (!mgr || !a || !b)
        return NULL;
    if (a->is_constant && b->is_constant) {
        return add_constant(mgr, a->constant + b->constant);
    }
    return add_constant(mgr, 0.0); /* 桩 */
}

ADDNode *add_sub(ADDManager *mgr, ADDNode *a, ADDNode *b) {
    if (!mgr || !a || !b)
        return NULL;
    if (a->is_constant && b->is_constant) {
        return add_constant(mgr, a->constant - b->constant);
    }
    return add_constant(mgr, 0.0);
}

ADDNode *add_mul(ADDManager *mgr, ADDNode *a, ADDNode *b) {
    if (!mgr || !a || !b)
        return NULL;
    if (a->is_constant && b->is_constant) {
        return add_constant(mgr, a->constant * b->constant);
    }
    return add_constant(mgr, 0.0);
}

ADDNode *add_div(ADDManager *mgr, ADDNode *a, ADDNode *b) {
    if (!mgr || !a || !b)
        return NULL;
    if (a->is_constant && b->is_constant && b->constant != 0.0) {
        return add_constant(mgr, a->constant / b->constant);
    }
    return add_constant(mgr, 0.0);
}

ADDNode *add_max(ADDManager *mgr, ADDNode *a, ADDNode *b) {
    if (!mgr || !a || !b)
        return NULL;
    if (a->is_constant && b->is_constant) {
        return add_constant(mgr, (a->constant > b->constant) ? a->constant : b->constant);
    }
    return add_constant(mgr, 0.0);
}

ADDNode *add_min(ADDManager *mgr, ADDNode *a, ADDNode *b) {
    if (!mgr || !a || !b)
        return NULL;
    if (a->is_constant && b->is_constant) {
        return add_constant(mgr, (a->constant < b->constant) ? a->constant : b->constant);
    }
    return add_constant(mgr, 0.0);
}
