/**
 * @file expr_canon.c
 * @brief 代数表达式规范形式实现
 *
 * @details 实现 lvExprCanonical 的完整生命周期、规范化和算术运算。
 *          规范形式确保代数等价的表达式产生相同的序列化形式。
 *
 *          核心算法:
 *          - 合并同类项: O(n) 哈希分组（FNV-1a 指数数组哈希桶）后逐组合并
 *          - 排序: 按总次数降序 + 字典序（插入排序）
 *          - 符号归一化: 规范形式要求首项系数为正（由 lv_expr_is_canonical 校验）
 *
 * @author Lv-00 Project
 * @version v1.0.0
 * @date 2026-05-24
 */

#include "expr_canon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv_internal.h" /* lv_UNUSED */
#include "lv_utils.h"
#include "lv/lv_lifecycle.h"
#include "lv/lv_strbuf.h"

/* 默认初始容量 */
#define EXPR_CANON_DEFAULT_CAPACITY 16

/* ========================================================================
 * 内部辅助
 * ======================================================================== */

/** 计算项的哈希值 */
static uint64_t term_hash(const int *exponents, int var_count) {
    uint64_t h = lv_FNV64_OFFSET_BASIS;
    for (int i = 0; i < var_count; i++) {
        uint32_t e = (uint32_t) exponents[i];
        h = lv_fnv1a_update(h, &e, sizeof(e));
    }
    return h;
}

/** 判断两个指数数组是否相等 */
static bool exponents_equal(const int *a, const int *b, int var_count) {
    for (int i = 0; i < var_count; i++) {
        if (a[i] != b[i])
            return false;
    }
    return true;
}

/** 计算项的总次数 */
static int term_total_degree(const int *exponents, int var_count) {
    int total = 0;
    for (int i = 0; i < var_count; i++) {
        total += exponents[i];
    }
    return total;
}

/* ========================================================================
 * 排序规则
 * ======================================================================== */

int lv_canonical_compare_terms(const int *a, const int *b, int var_count) {
    if (!a || !b)
        return 0;

    /* 字典序从最后一个变量开始比较（grlex 偏序） */
    for (int i = var_count - 1; i >= 0; i--) {
        if (a[i] != b[i]) {
            return a[i] - b[i]; /* 正数表示 a 的此项指数更大 */
        }
    }
    return 0; /* 完全相同 */
}

/**
 * 哈希分组合并桶节点：指数数组 FNV-1a 哈希（term_hash 结果）→ merged 数组下标
 *
 * 【lv_hashtable 收敛评估结论（不收敛，保留本实现）】
 *   1. 键特殊性：桶键是 64 位 term_hash（uint64_t），超出 lv_hashtable int 键
 *      （32 位）范围，折叠成 32 位会引入碰撞，而开放寻址要求键唯一。
 *   2. 键等价性：同一哈希桶内还需 exponents_equal() 对指数数组做精确比较
 *      （term_hash 是分组哈希而非唯一键），lv_hashtable 的 int == 无法承载。
 *   3. 合并语义：合并归零的项需从桶链表中"动态摘除"（链表节点复用数组），
 *      与 lv_hashtable 的键值增删模型不同。
 *   4. 本桶是 lv_expr_canonicalize 内部的单次临时结构：一次性分配
 *      bucket_head + bucket_nodes 两个数组、无逐节点 malloc、用完即释放，
 *      是当前热路径上的最优形态；改用 lv_hashtable 将引入逐节点分配、
 *      扩容重哈希与键折叠，纯性能退化。
 * 二次收敛评估（2026-05-24 提议 lv_hashtable_i64 方案）：
 *   即使键升级为 uint64_t，理由 2（分组哈希 + 桶内精确比较，键唯一性由
 *   hash 与指数数组共同决定，lv_hashtable 键比较模型无法承载）与理由 3
 *   （动态摘除）依然成立，且理由 4 的一次性数组形态与逐节点分配模型相悖，
 *   故仍不采用。
 * 三次收敛评估（2026-08-08 提交 17f402d 起 lv_hashtable_i64 形态已存在，
 * 键为 int64_t + void* 值、开放寻址 + 自动扩容）：
 *   i64 形态就绪后重新核实，结论不变，仍不迁移：
 *     a. 键唯一性语义仍不匹配：term_hash 是分组哈希，同一 hash 下可挂多个
 *        指数数组不同的项（需 exponents_equal 精确区分）；lv_hashtable_i64
 *        以 int64 == 判键唯一，同 hash 第二项 insert 返回 false 即丢失合并。
 *     b. 合并归零动态摘除：i64 形态 remove 后探测链经 tombstone 维护，可删，
 *        但"合并归零后重新入桶"（相同 hash 重新插入新指数数组）仍需在表外
 *        区分"该 hash 已存在"，等价于保留桶链表。
 *     c. 热路径形态：本桶为 lv_expr_canonicalize 单次调用内一次性两个数组，
 *        无句柄/无扩容（容量 = old_count 取 2 的幂），与 i64 形态的负载因子
 *        自动扩容、逐槽 calloc 分配模型相悖，迁移为纯性能退化。
 * 故保留原实现，仅与 lv_hashtable 共享 FNV-1a 哈希族（lv_fnv1a_update）。
 */
typedef struct MergeBucketNode {
    uint64_t hash;       /**< 指数数组哈希值 */
    int merged_index;    /**< merged 数组中的项下标 */
    int next;            /**< 桶内链表下一节点，-1 表示链尾 */
} MergeBucketNode;

/** 计算不小于 min 的 2 的幂桶数（上限 2^30，防止 int 溢出） */
static int merge_bucket_count(int min) {
    int buckets = 1;
    while (buckets < min && buckets <= (1 << 29)) {
        buckets <<= 1;
    }
    return buckets;
}

/** 排序比较回调（lv_insertion_sort 语义：<0 表示 a 排在 b 前）：
 *  总次数降序；同次数按规范字典序降序（与旧冒泡排序结果逐位一致） */
static int cmp_canon_terms_sort(const void *a, const void *b, void *ctx) {
    int vc = *(const int *) ctx;
    const lvExprTerm *ta = (const lvExprTerm *) a;
    const lvExprTerm *tb = (const lvExprTerm *) b;
    int deg_a = term_total_degree(ta->exponents, ta->var_count);
    int deg_b = term_total_degree(tb->exponents, tb->var_count);
    if (deg_a != deg_b)
        return deg_a > deg_b ? -1 : 1;
    int cmp = lv_canonical_compare_terms(ta->exponents, tb->exponents, vc);
    if (cmp != 0)
        return cmp > 0 ? -1 : 1;
    return 0;
}

/* ========================================================================
 * 生命周期
 * ======================================================================== */

/* lvExprCanonical 部分构建守卫：任一成员分配失败时统一释放已分配成员与外壳，
 * 替代递增回滚样板 */
typedef struct {
    lvExprCanonical *expr;
} ExprCanonGuard;

static void expr_canon_guard_cleanup(void *p) {
    ExprCanonGuard *g = (ExprCanonGuard *) p;
    if (g->expr) {
        if (g->expr->var_names) {
            for (int i = 0; i < g->expr->var_count; i++)
                lv_free((void **) &g->expr->var_names[i]);
            lv_free((void **) &g->expr->var_names);
        }
        lv_free((void **) &g->expr->terms);
        lv_free((void **) &g->expr);
    }
}

lvExprCanonical *lv_expr_canonical_create(int var_count, const char **var_names) {
    if (var_count < 0)
        return NULL;

    lvExprCanonical *expr = (lvExprCanonical *) lv_calloc(1, sizeof(lvExprCanonical));
    if (!expr)
        return NULL;
    expr->term_capacity = EXPR_CANON_DEFAULT_CAPACITY;
    expr->var_count = var_count;
    expr->canonicalized = true;

    /* 部分构建守卫：后续任一分配失败自动释放已分配成员；成功路径 guard.expr = NULL 解除 */
    ExprCanonGuard guard = {expr};
    lv_DEFER(expr_canon_guard_cleanup, &guard);

    expr->terms = (lvExprTerm *) lv_malloc((size_t) expr->term_capacity * sizeof(lvExprTerm));
    if (!expr->terms)
        return NULL;

    /* 初始化所有项 */
    for (int i = 0; i < expr->term_capacity; i++) {
        expr->terms[i].coeff = NULL;
        expr->terms[i].exponents = NULL;
        expr->terms[i].var_count = 0;
    }

    /* 拷贝变量名 */
    if (var_names && var_count > 0) {
        expr->var_names = (char **) lv_malloc((size_t) var_count * sizeof(char *));
        if (!expr->var_names)
            return NULL;
        for (int i = 0; i < var_count; i++) {
            if (var_names[i]) {
                size_t name_len = strlen(var_names[i]) + 1;
                expr->var_names[i] = (char *) lv_malloc(name_len);
                if (expr->var_names[i])
                    memcpy(expr->var_names[i], var_names[i], name_len);
            } else {
                expr->var_names[i] = NULL;
            }
        }
    } else {
        expr->var_names = NULL;
    }

    guard.expr = NULL; /* 守卫解除：结果移交调用方 */
    return expr;
}

void lv_expr_canonical_destroy(lvExprCanonical **expr) {
    if (!expr || !*expr)
        return;

    lvExprCanonical *e = *expr;

    for (int i = 0; i < e->term_capacity; i++) {
        if (e->terms[i].coeff) {
            lv_rational_destroy(&e->terms[i].coeff);
        }
        lv_free((void **) &e->terms[i].exponents);
    }
    lv_free((void **) &e->terms);

    if (e->var_names) {
        for (int i = 0; i < e->var_count; i++) {
            lv_free((void **) &e->var_names[i]);
        }
        lv_free((void **) &e->var_names);
    }

    lv_free((void **) expr);
}

lvExprCanonical *lv_expr_canonical_clone(const lvExprCanonical *src) {
    if (!src)
        return NULL;

    const char **names = (const char **) src->var_names;
    lvExprCanonical *dst = lv_expr_canonical_create(src->var_count, names);
    if (!dst)
        return NULL;

    /* 逐个拷贝现有项 */
    for (int i = 0; i < src->term_count; i++) {
        if (!lv_expr_canonical_add_term(dst, src->terms[i].coeff, src->terms[i].exponents)) {
            lv_expr_canonical_destroy(&dst);
            return NULL;
        }
    }

    /* 继承源的规范化状态 */
    dst->canonicalized = src->canonicalized;

    return dst;
}

/* ========================================================================
 * 项操作
 * ======================================================================== */

/**
 * @brief 确保有空间容纳一个新项
 *
 * @return true 成功扩容或空间足够，false 分配失败
 */
static bool ensure_capacity(lvExprCanonical *expr, int needed) {
    if (expr->term_count + needed <= expr->term_capacity)
        return true;

    /* 记录旧容量，扩容后初始化新增槽位 */
    int old_cap = expr->term_capacity;

    /* 统一委托 lv_ensure_capacity（倍增策略/溢出检查/失败语义一致） */
    if (!lv_ensure_capacity((void **) &expr->terms, expr->term_count + needed, &expr->term_capacity,
                            sizeof(lvExprTerm), 1))
        return false;

    /* 初始化新槽位 */
    for (int i = old_cap; i < expr->term_capacity; i++) {
        expr->terms[i].coeff = NULL;
        expr->terms[i].exponents = NULL;
        expr->terms[i].var_count = 0;
    }
    return true;
}

bool lv_expr_canonical_add_term(lvExprCanonical *expr, const lvRational *coeff, const int *exponents) {
    if (!expr || !coeff || !exponents)
        return false;
    if (lv_rational_is_zero(coeff))
        return true; /* 零系数项直接跳过 */

    if (!ensure_capacity(expr, 1))
        return false;

    int idx = expr->term_count;

    /* 分配并拷贝系数 */
    expr->terms[idx].coeff = lv_rational_clone(coeff);
    if (!expr->terms[idx].coeff)
        return false;

    /* 分配并拷贝指数 */
    expr->terms[idx].exponents = (int *) lv_malloc((size_t) expr->var_count * sizeof(int));
    if (!expr->terms[idx].exponents) {
        lv_rational_destroy(&expr->terms[idx].coeff);
        return false;
    }
    memcpy(expr->terms[idx].exponents, exponents, (size_t) expr->var_count * sizeof(int));
    expr->terms[idx].var_count = expr->var_count;

    expr->term_count++;
    expr->canonicalized = false;
    return true;
}

/* ========================================================================
 * 规范化
 * ======================================================================== */

bool lv_expr_canonicalize(lvExprCanonical *expr) {
    if (!expr)
        return false;
    if (expr->term_count == 0) {
        expr->canonicalized = true;
        return true;
    }

    int old_count = expr->term_count;
    int vc = expr->var_count;
    lvExprTerm *merged = (lvExprTerm *) lv_malloc((size_t) expr->term_capacity * sizeof(lvExprTerm));
    if (!merged)
        return false;

    for (int i = 0; i < expr->term_capacity; i++) {
        merged[i].coeff = NULL;
        merged[i].exponents = NULL;
        merged[i].var_count = 0;
    }

    int merged_count = 0;
    bool ok = true;

    /* 深拷贝并合并同类项，避免浅拷贝导致指针所有权混乱。
     * 哈希分组：term_hash 建桶（FNV-1a），桶内精确比较指数数组，平均 O(n)；
     * 合并后归零的项就地销毁并从桶链表摘除，删除槽位推迟到合并结束后统一压缩。
     * 结果与线性扫描逐项合并逐位一致（lvRational 为精确算术，加法满足交换/结合律，
     * 且排序结果只由唯一元素决定，与合并顺序无关）。 */
    int bucket_count = merge_bucket_count(old_count);
    int *bucket_head = (int *) lv_malloc((size_t) bucket_count * sizeof(int));
    MergeBucketNode *bucket_nodes = (MergeBucketNode *) lv_malloc((size_t) old_count * sizeof(MergeBucketNode));
    if (!bucket_head || !bucket_nodes) {
        lv_free((void **) &bucket_head);
        lv_free((void **) &bucket_nodes);
        for (int i = 0; i < expr->term_capacity; i++) {
            if (merged[i].coeff)
                lv_rational_destroy(&merged[i].coeff);
            lv_free((void **) &merged[i].exponents);
        }
        lv_free((void **) &merged);
        return false;
    }
    for (int b = 0; b < bucket_count; b++)
        bucket_head[b] = -1;
    int node_count = 0;

    for (int i = 0; i < old_count && ok; i++) {
        if (!expr->terms[i].coeff || !expr->terms[i].exponents || lv_rational_is_zero(expr->terms[i].coeff))
            continue;

        uint64_t h = term_hash(expr->terms[i].exponents, vc);
        int b = (int) (h % (uint64_t) bucket_count);
        int found = -1;
        int found_node = -1;
        int prev = -1;
        for (int node = bucket_head[b]; node >= 0; node = bucket_nodes[node].next) {
            int midx = bucket_nodes[node].merged_index;
            if (!merged[midx].coeff || !merged[midx].exponents)
                continue; /* 已删除（合并后归零）的槽位 */
            if (bucket_nodes[node].hash == h &&
                exponents_equal(merged[midx].exponents, expr->terms[i].exponents, vc)) {
                found = midx;
                found_node = node;
                break;
            }
            prev = node;
        }

        if (found >= 0) {
            lv_rational_add_inplace(merged[found].coeff, expr->terms[i].coeff);
            if (lv_rational_is_zero(merged[found].coeff)) {
                lv_rational_destroy(&merged[found].coeff);
                lv_free((void **) &merged[found].exponents);
                merged[found].var_count = 0;
                /* 从桶链表中摘除该节点，后续同类项将作为新项重新加入 */
                if (prev < 0)
                    bucket_head[b] = bucket_nodes[found_node].next;
                else
                    bucket_nodes[prev].next = bucket_nodes[found_node].next;
            }
        } else {
            bucket_nodes[node_count].hash = h;
            bucket_nodes[node_count].merged_index = merged_count;
            bucket_nodes[node_count].next = bucket_head[b];
            bucket_head[b] = node_count;
            node_count++;

            merged[merged_count].coeff = lv_rational_clone(expr->terms[i].coeff);
            if (!merged[merged_count].coeff) {
                ok = false;
                break;
            }
            merged[merged_count].exponents = (int *) lv_malloc((size_t) vc * sizeof(int));
            if (!merged[merged_count].exponents) {
                lv_rational_destroy(&merged[merged_count].coeff);
                ok = false;
                break;
            }
            memcpy(merged[merged_count].exponents, expr->terms[i].exponents, (size_t) vc * sizeof(int));
            merged[merged_count].var_count = vc;
            merged_count++;
        }
    }

    lv_free((void **) &bucket_head);
    lv_free((void **) &bucket_nodes);

    if (!ok) {
        for (int i = 0; i < expr->term_capacity; i++) {
            if (merged[i].coeff)
                lv_rational_destroy(&merged[i].coeff);
            lv_free((void **) &merged[i].exponents);
        }
        lv_free((void **) &merged);
        return false;
    }

    /* 压缩合并阶段标记删除的槽位（浅拷贝移动后清空源槽位，防止析构时重复释放） */
    int compact = 0;
    for (int i = 0; i < merged_count; i++) {
        if (merged[i].coeff && merged[i].exponents) {
            if (compact != i) {
                merged[compact] = merged[i];
                merged[i].coeff = NULL;
                merged[i].exponents = NULL;
                merged[i].var_count = 0;
            }
            compact++;
        }
    }
    merged_count = compact;

    /* 排序：总次数降序，同次数按规范字典序（插入排序；合并后元素唯一，结果确定） */
    lv_insertion_sort(merged, (size_t) merged_count, sizeof(lvExprTerm), cmp_canon_terms_sort, &vc);

    /* 规范化必须保持多项式值，不对整体符号做归一化。 */

    /* 释放旧项后安装新项数组。 */
    for (int i = 0; i < expr->term_capacity; i++) {
        if (expr->terms[i].coeff)
            lv_rational_destroy(&expr->terms[i].coeff);
        lv_free((void **) &expr->terms[i].exponents);
    }
    lv_free((void **) &expr->terms);

    expr->terms = merged;
    expr->term_count = merged_count;
    expr->canonicalized = true;
    return true;
}

bool lv_expr_is_canonical(const lvExprCanonical *expr) {
    if (!expr)
        return false;
    if (!expr->canonicalized)
        return false;
    if (expr->term_count <= 1)
        return true;

    /* 检查排序和唯一性 */
    for (int i = 0; i < expr->term_count; i++) {
        /* 检查零系数 */
        if (expr->terms[i].coeff && lv_rational_is_zero(expr->terms[i].coeff))
            return false;

        /* 检查排序 */
        if (i + 1 < expr->term_count) {
            int deg_a = term_total_degree(expr->terms[i].exponents, expr->var_count);
            int deg_b = term_total_degree(expr->terms[i + 1].exponents, expr->var_count);
            if (deg_a < deg_b)
                return false;
            if (deg_a == deg_b) {
                int cmp =
                    lv_canonical_compare_terms(expr->terms[i].exponents, expr->terms[i + 1].exponents, expr->var_count);
                if (cmp == 0)
                    return false; /* 重复项 */
                if (cmp < 0)
                    return false; /* 排序错误 */
            }
        }
    }

    /* 检查首项系数符号 */
    if (expr->term_count > 0 && expr->terms[0].coeff) {
        if (lv_rational_sgn(expr->terms[0].coeff) < 0)
            return false;
    }

    return true;
}

/* ========================================================================
 * 算术操作
 * ======================================================================== */

lvExprCanonical *lv_expr_canonical_add(const lvExprCanonical *a, const lvExprCanonical *b) {
    if (!a || !b)
        return NULL;
    if (a->var_count != b->var_count)
        return NULL;

    const char **names = (const char **) a->var_names;
    lvExprCanonical *result = lv_expr_canonical_create(a->var_count, names);
    if (!result)
        return NULL;

    /* 添加 a 的所有项 */
    for (int i = 0; i < a->term_count; i++) {
        if (!lv_expr_canonical_add_term(result, a->terms[i].coeff, a->terms[i].exponents)) {
            lv_expr_canonical_destroy(&result);
            return NULL;
        }
    }

    /* 添加 b 的所有项 */
    for (int i = 0; i < b->term_count; i++) {
        if (!lv_expr_canonical_add_term(result, b->terms[i].coeff, b->terms[i].exponents)) {
            lv_expr_canonical_destroy(&result);
            return NULL;
        }
    }

    /* 规范化 */
    if (!lv_expr_canonicalize(result)) {
        lv_expr_canonical_destroy(&result);
        return NULL;
    }

    return result;
}

lvExprCanonical *lv_expr_canonical_sub(const lvExprCanonical *a, const lvExprCanonical *b) {
    if (!a || !b)
        return NULL;
    if (a->var_count != b->var_count)
        return NULL;

    /* a - b = a + (-b) */
    lvExprCanonical *neg_b = lv_expr_canonical_neg(b);
    if (!neg_b)
        return NULL;

    lvExprCanonical *result = lv_expr_canonical_add(a, neg_b);
    lv_expr_canonical_destroy(&neg_b);
    return result;
}

lvExprCanonical *lv_expr_canonical_mul(const lvExprCanonical *a, const lvExprCanonical *b) {
    if (!a || !b)
        return NULL;
    if (a->var_count != b->var_count)
        return NULL;

    const char **names = (const char **) a->var_names;
    lvExprCanonical *result = lv_expr_canonical_create(a->var_count, names);
    if (!result)
        return NULL;

    int vc = a->var_count;

    /* 逐项乘法 */
    for (int i = 0; i < a->term_count; i++) {
        for (int j = 0; j < b->term_count; j++) {
            lvRational *coeff = lv_rational_mul(a->terms[i].coeff, b->terms[j].coeff);
            if (!coeff) {
                lv_expr_canonical_destroy(&result);
                return NULL;
            }

            int *exp = (int *) lv_malloc((size_t) vc * sizeof(int));
            if (!exp) {
                lv_rational_destroy(&coeff);
                lv_expr_canonical_destroy(&result);
                return NULL;
            }

            for (int k = 0; k < vc; k++) {
                exp[k] = a->terms[i].exponents[k] + b->terms[j].exponents[k];
            }

            bool ok = lv_expr_canonical_add_term(result, coeff, exp);
            lv_rational_destroy(&coeff);
            lv_free((void **) &exp);

            if (!ok) {
                lv_expr_canonical_destroy(&result);
                return NULL;
            }
        }
    }

    if (!lv_expr_canonicalize(result)) {
        lv_expr_canonical_destroy(&result);
        return NULL;
    }

    return result;
}

lvExprCanonical *lv_expr_canonical_scale(const lvExprCanonical *a, const lvRational *coeff) {
    if (!a || !coeff)
        return NULL;

    lvExprCanonical *result = lv_expr_canonical_clone(a);
    if (!result)
        return NULL;

    for (int i = 0; i < result->term_count; i++) {
        lv_rational_mul_inplace(result->terms[i].coeff, coeff);
    }

    /* 重新规范化以合并可能产生的零项 */
    if (!lv_expr_canonicalize(result)) {
        lv_expr_canonical_destroy(&result);
        return NULL;
    }

    return result;
}

lvExprCanonical *lv_expr_canonical_neg(const lvExprCanonical *a) {
    if (!a)
        return NULL;

    lvExprCanonical *result = lv_expr_canonical_clone(a);
    if (!result)
        return NULL;

    for (int i = 0; i < result->term_count; i++) {
        lv_rational_neg_inplace(result->terms[i].coeff);
    }

    return result;
}

/* ========================================================================
 * 比较与查询
 * ======================================================================== */

bool lv_expr_canonical_equal(const lvExprCanonical *a, const lvExprCanonical *b) {
    if (!a || !b)
        return (a == b);
    if (a->var_count != b->var_count)
        return false;
    if (a->term_count != b->term_count)
        return false;

    for (int i = 0; i < a->term_count; i++) {
        if (!lv_rational_equal(a->terms[i].coeff, b->terms[i].coeff))
            return false;
        if (!exponents_equal(a->terms[i].exponents, b->terms[i].exponents, a->var_count))
            return false;
    }
    return true;
}

bool lv_expr_canonical_is_zero(const lvExprCanonical *a) {
    if (!a)
        return true;
    return a->term_count == 0;
}

int lv_expr_canonical_degree(const lvExprCanonical *expr) {
    if (!expr || expr->term_count == 0)
        return -1;

    /* 规范形式下，首项是最高次数 */
    return term_total_degree(expr->terms[0].exponents, expr->var_count);
}

int lv_expr_canonical_term_count(const lvExprCanonical *expr) {
    if (!expr)
        return 0;
    return expr->term_count;
}

/* ========================================================================
 * 字符串表示
 * ======================================================================== */

char *lv_expr_canonical_to_string(const lvExprCanonical *expr) {
    if (!expr)
        return NULL;
    if (expr->term_count == 0) {
        char *zero = (char *) lv_malloc(2);
        if (zero)
            memcpy(zero, "0", 2);
        return zero;
    }

    /* 用 lvStrBuf 累积输出（自动扩容；lv_strbuf_to_string 返回 lv_malloc 分配的 NUL 结尾字符串） */
    lvStrBuf sb = {0};

    bool first = true;

    for (int i = 0; i < expr->term_count; i++) {
        const lvRational *coeff = expr->terms[i].coeff;
        const int *exp = expr->terms[i].exponents;
        int sgn = lv_rational_sgn(coeff);

        if (sgn == 0)
            continue;

        /* 符号前缀 */
        if (first) {
            if (sgn < 0)
                lv_strbuf_printf(&sb, "-");
            first = false;
        } else {
            lv_strbuf_printf(&sb, sgn >= 0 ? " + " : " - ");
        }

        /* 判断是否常数项 */
        bool is_constant = true;
        for (int k = 0; k < expr->var_count; k++) {
            if (exp[k] > 0) {
                is_constant = false;
                break;
            }
        }

        if (is_constant) {
            lvRational *abs_coeff = lv_rational_abs(coeff);
            char *cs = lv_rational_to_string(abs_coeff);
            if (cs) {
                lv_strbuf_printf(&sb, "%s", cs);
                lv_free((void **)&(cs));
            }
            lv_rational_destroy(&abs_coeff);
        } else {
            /* 变量项 */
            lvRational *abs_coeff = lv_rational_abs(coeff);
            bool is_one = lv_rational_is_one(abs_coeff);

            if (!is_one) {
                char *cs = lv_rational_to_string(abs_coeff);
                if (cs) {
                    lv_strbuf_printf(&sb, "%s*", cs);
                    lv_free((void **)&(cs));
                }
            }
            lv_rational_destroy(&abs_coeff);

            /* 输出变量 */
            bool first_var = true;
            for (int k = 0; k < expr->var_count; k++) {
                if (exp[k] == 0)
                    continue;

                if (!first_var)
                    lv_strbuf_printf(&sb, "*");
                first_var = false;

                if (expr->var_names && expr->var_names[k]) {
                    lv_strbuf_printf(&sb, "%s", expr->var_names[k]);
                } else {
                    lv_strbuf_printf(&sb, "x%d", k);
                }

                if (exp[k] > 1) {
                    lv_strbuf_printf(&sb, "^%d", exp[k]);
                }
            }
        }
    }

    return lv_strbuf_to_string(&sb);
}

/* ========================================================================
 * 字符串解析（递归下降解析器）
 * ======================================================================== */

/** 跳过空白字符 */
static void skip_spaces(const char **pp) {
    while (**pp && (unsigned char)**pp <= ' ')
        (*pp)++;
}

/**
 * @brief 解析不含 '/' 的十进制数字字符串为有理数
 *
 * 支持 "42"（整数）和 "3.14"（小数）两种格式。
 * 小数通过去除小数点转为分数形式，再通过 lv_rational_from_string
 * 构造后化简。
 */
static lvRational *parse_decimal(const char *start, const char *end) {
    /* 检查是否有小数点 */
    const char *dot = NULL;
    for (const char *cp = start; cp < end; cp++) {
        if (*cp == '.') {
            dot = cp;
            break;
        }
    }

    if (!dot) {
        /* 纯整数 */
        long val;
        char *e = NULL;
        val = strtol(start, &e, 10);
        if (e == start) return NULL;
        return lv_rational_create_from_si(val, 1);
    }

    /* 小数：提取所有数字字符，构造 num/den 分数 */
    int digits_before_dot = (int)(dot - start);
    int digits_after_dot  = (int)(end - dot - 1);

    /* 限制小数位精度，避免分母过大 */
    if (digits_after_dot > 9) digits_after_dot = 9;

    /* 构造分母字符串 "1" + digits_after_dot 个 "0" */
    char den_buf[32];
    int den_idx = 0;
    den_buf[den_idx++] = '1';
    for (int i = 0; i < digits_after_dot && den_idx < 30; i++)
        den_buf[den_idx++] = '0';
    den_buf[den_idx] = '\0';

    /* 构造分子：去掉小数点后的数字字符串 */
    char num_buf[128];
    int num_idx = 0;

    /* 处理符号 */
    if (*start == '-') {
        num_buf[num_idx++] = '-';
        start++;
    } else if (*start == '+') {
        start++;
    }

    /* 复制小数点前的数字（不含 '.' 本身）*/
    for (int i = 0; i < digits_before_dot; i++) {
        num_buf[num_idx++] = start[i];
    }

    /* 复制小数点后的数字 */
    for (int i = 0; i < digits_after_dot; i++) {
        num_buf[num_idx++] = dot[1 + i];
    }
    num_buf[num_idx] = '\0';

    /* 处理边缘情况：纯小数如 ".5" → 分子 "5" */
    if (num_idx == 0 || (num_idx == 1 && num_buf[0] == '-')) {
        num_buf[num_idx++] = '0';
        num_buf[num_idx] = '\0';
    }

    /* 组合 "num/den" */
    char full[256];
    int n = snprintf(full, sizeof(full), "%s/%s", num_buf, den_buf);
    if (n < 0 || (size_t)n >= sizeof(full))
        return NULL;

    lvRational *r = lv_rational_from_string(full);
    if (r) lv_rational_simplify(r);
    return r;
}

/**
 * @brief 从字符串解析规范多项式表达式
 *
 * 支持语法：
 *   expr   → term (('+' | '-') term)*
 *   term   → [SIGN] [NUMBER] ['*'] factor ('*' factor)*
 *   factor → VARIABLE ['^' UINT]
 *
 * 示例: "3*x^2*y + 5*x - 2", "-x + y", "42", "x^2"
 *
 * 变量名必须在 var_names 数组中注册，否则解析失败。
 *
 * @param str       输入字符串
 * @param var_names 变量名数组
 * @param var_count 变量个数
 * @return 解析后的规范多项式，失败返回 NULL
 */
lvExprCanonical *lv_expr_canonical_from_string(const char *str,
                                               const char **var_names,
                                               int var_count) {
    if (!str)
        return NULL;

    lvExprCanonical *expr = lv_expr_canonical_create(var_count, var_names);
    if (!expr)
        return NULL;

    const char *p = str;
    skip_spaces(&p);

    if (*p == '\0')
        return expr; /* 空字符串 = 零多项式 */

    int sign = 1;
    bool first_item = true;

    while (*p) {
        skip_spaces(&p);
        if (*p == '\0')
            break;

        /* --- 处理 +/- 分隔符 --- */
        if (*p == '+' || *p == '-') {
            if (!first_item) {
                sign = (*p == '+') ? 1 : -1;
                p++;
                continue;
            } else {
                sign = (*p == '-') ? -1 : 1;
                p++;
                first_item = false;
            }
        } else if (first_item) {
            first_item = false;
            sign = 1;
        } else {
            /* 非首项未出现 + / -，视为终止 */
            break;
        }

        skip_spaces(&p);
        if (*p == '\0')
            break;

        /* --- 解析数字系数 --- */
        lvRational *coeff = NULL;
        const char *num_start = p;

        if (*p == '-' || *p == '+' || (*p >= '0' && *p <= '9') || *p == '.') {
            char *num_end = NULL;
            strtod(p, &num_end);
            if (num_end != p) {
                coeff = parse_decimal(num_start, num_end);
                p = num_end;
                /* 系数后可能有 '*' 分隔符 */
                skip_spaces(&p);
                if (*p == '*') {
                    p++;
                    skip_spaces(&p);
                }
            }
        }

        /* --- 解析变量因子 --- */
        int *exponents = (int *)lv_calloc((size_t)var_count, sizeof(int));
        if (!exponents) {
            if (coeff) lv_rational_destroy(&coeff);
            lv_expr_canonical_destroy(&expr);
            return NULL;
        }

        bool has_var_part = false;

        while (*p) {
            skip_spaces(&p);
            if (*p == '\0' || *p == '+' || *p == '-')
                break;

            /* 尝试匹配变量名 */
            int longest_match = 0;
            int var_idx = -1;

            for (int i = 0; i < var_count; i++) {
                if (!var_names[i]) continue;
                size_t vlen = strlen(var_names[i]);
                if ((int)vlen > longest_match &&
                    strncmp(p, var_names[i], vlen) == 0) {
                    /* 确保不是更长的标识符的一部分 */
                    if (p[vlen] == '\0' || p[vlen] == '*' ||
                        p[vlen] == '^' || p[vlen] == '+' ||
                        p[vlen] == '-' || (unsigned char)p[vlen] <= ' ') {
                        longest_match = (int)vlen;
                        var_idx = i;
                    }
                }
            }

            if (var_idx < 0)
                break; /* 不是变量，终止变量因子解析 */

            has_var_part = true;
            p += longest_match;

            /* 解析可选的指数 ^N */
            int exponent = 1;
            skip_spaces(&p);
            if (*p == '^') {
                p++;
                skip_spaces(&p);
                char *exp_end = NULL;
                long exp_val = strtol(p, &exp_end, 10);
                if (exp_end != p && exp_val > 0 && exp_val <= 65536) {
                    exponent = (int)exp_val;
                    p = exp_end;
                }
            }

            exponents[var_idx] += exponent;

            /* 跳过可选的 '*' */
            skip_spaces(&p);
            if (*p == '*') {
                p++;
                skip_spaces(&p);
            }
        }

        /* --- 创建项 --- */
        if (coeff == NULL) {
            /* 没有显式系数：若只有变量部分，系数为 1 */
            coeff = lv_rational_create_from_si(sign, 1);
        } else if (sign == -1) {
            lv_rational_neg_inplace(coeff);
        }

        if (!has_var_part && coeff) {
            /* 常数项，指数已全零 */
        }

        if (coeff && !lv_rational_is_zero(coeff)) {
            lv_expr_canonical_add_term(expr, coeff, exponents);
        }

        lv_rational_destroy(&coeff);
        lv_free((void **)&exponents);
    }

    /* 规范化并返回 */
    if (!lv_expr_canonicalize(expr)) {
        lv_expr_canonical_destroy(&expr);
        return NULL;
    }

    return expr;
}

/* ========================================================================
 * 旧接口兼容
 * ======================================================================== */

char *lv_expr_canon(const char *expr) {
    if (!expr)
        return NULL;

    /* 尝试从字符串解析为规范形式 */
    const char *var_names[] = {"x", "y", "z", "w", "u", "v"};
    int var_count = 6;
    lvExprCanonical *canon = lv_expr_canonical_from_string(expr, var_names, var_count);
    if (!canon) {
        /* 解析失败，回退：返回原始字符串的副本 */
        char *fallback = lv_strdup(expr);
        return fallback;
    }

    char *result = lv_expr_canonical_to_string(canon);
    lv_expr_canonical_destroy(&canon);
    return result;
}
