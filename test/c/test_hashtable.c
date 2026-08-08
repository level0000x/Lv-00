/**
 * @file test_hashtable.c
 * @brief lv_hashtable 哈希表设施测试（重点覆盖新增的 int64 键形态）
 *
 * 覆盖：
 *   (a) i64 插入/查找/包含/删除/计数 基本流程；
 *   (b) 键已存在时 insert 返回 false 且不覆盖；
 *   (c) 边界键值（0、±1、INT64_MIN/MAX、高位置位的大 uint64 位模式）；
 *   (d) 10000 个连续键插入后全部可查（验证扩容重哈希与哈希分布）；
 *   (e) 碰撞场景：同桶键对 + tombstone 删除后探测链完整、墓碑复用；
 *   (f) foreach 遍历访问全部键值且每个键恰好一次；
 *   (g) int / string 形态回归（宏模板化迁移后行为不变）；
 *   (h) destroy 后无新增内存泄漏（对照 lv_memory_leak_report 基线）。
 */

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv.h"
#include "lv/lv_hashtable.h"
#include "lv/lv_utils.h" /* lv_memory_leak_report */

/* ---------- (a) 基本流程 ---------- */

static void test_i64_basic(void) {
    printf("Testing i64 basic insert/get/contains/remove/count...\n");

    lvHashtable *ht = lv_hashtable_i64_create(0);
    assert(ht != NULL);
    assert(lv_hashtable_i64_count(ht) == 0);

    /* 未找到返回 NULL */
    assert(lv_hashtable_i64_get(ht, 42) == NULL);
    assert(!lv_hashtable_i64_contains(ht, 42));
    assert(!lv_hashtable_i64_remove(ht, 42)); /* 不存在删除失败 */

    /* 插入与查找 */
    assert(lv_hashtable_i64_insert(ht, 42, (void *) (intptr_t) 1000));
    assert(lv_hashtable_i64_insert(ht, -7, (void *) (intptr_t) 2000));
    assert(lv_hashtable_i64_insert(ht, 0, (void *) (intptr_t) 3000));
    assert(lv_hashtable_i64_count(ht) == 3);
    assert(lv_hashtable_i64_get(ht, 42) == (void *) (intptr_t) 1000);
    assert(lv_hashtable_i64_get(ht, -7) == (void *) (intptr_t) 2000);
    assert(lv_hashtable_i64_get(ht, 0) == (void *) (intptr_t) 3000);
    assert(lv_hashtable_i64_contains(ht, 42));
    assert(lv_hashtable_i64_contains(ht, -7));
    assert(lv_hashtable_i64_contains(ht, 0));
    assert(!lv_hashtable_i64_contains(ht, 43));

    /* 删除 */
    assert(lv_hashtable_i64_remove(ht, 42));
    assert(!lv_hashtable_i64_contains(ht, 42));
    assert(lv_hashtable_i64_get(ht, 42) == NULL);
    assert(lv_hashtable_i64_count(ht) == 2);
    assert(!lv_hashtable_i64_remove(ht, 42)); /* 重复删除失败 */

    /* 删除后其余键仍可查 */
    assert(lv_hashtable_i64_get(ht, -7) == (void *) (intptr_t) 2000);
    assert(lv_hashtable_i64_get(ht, 0) == (void *) (intptr_t) 3000);

    lv_hashtable_i64_destroy(ht);
    printf("  PASSED\n");
}

/* ---------- (b) 键已存在时不覆盖 ---------- */

static void test_i64_overwrite_rejected(void) {
    printf("Testing i64 duplicate insert rejected...\n");

    lvHashtable *ht = lv_hashtable_i64_create(8);
    assert(ht != NULL);
    assert(lv_hashtable_i64_insert(ht, 5, (void *) (intptr_t) 111));
    assert(!lv_hashtable_i64_insert(ht, 5, (void *) (intptr_t) 222)); /* 返回 false 不覆盖 */
    assert(lv_hashtable_i64_get(ht, 5) == (void *) (intptr_t) 111);
    assert(lv_hashtable_i64_count(ht) == 1);

    /* 负键与大键同样适用 */
    assert(lv_hashtable_i64_insert(ht, -1234567890123LL, (void *) (intptr_t) 1));
    assert(!lv_hashtable_i64_insert(ht, -1234567890123LL, (void *) (intptr_t) 2));
    assert(lv_hashtable_i64_get(ht, -1234567890123LL) == (void *) (intptr_t) 1);

    lv_hashtable_i64_destroy(ht);
    printf("  PASSED\n");
}

/* ---------- (c) 边界键值 ---------- */

static void test_i64_boundary_values(void) {
    printf("Testing i64 boundary keys...\n");

    lvHashtable *ht = lv_hashtable_i64_create(16);
    assert(ht != NULL);

    /* 0 / ±1 / 极端有符号值 */
    assert(lv_hashtable_i64_insert(ht, 0, (void *) (intptr_t) 0xA));
    assert(lv_hashtable_i64_insert(ht, 1, (void *) (intptr_t) 0xB));
    assert(lv_hashtable_i64_insert(ht, -1, (void *) (intptr_t) 0xC));
    assert(lv_hashtable_i64_insert(ht, INT64_MAX, (void *) (intptr_t) 0xD));
    assert(lv_hashtable_i64_insert(ht, INT64_MIN, (void *) (intptr_t) 0xE));

    /* 高位置位的大 uint64 位模式（按位模式存取，可覆盖 uint64 键） */
    const int64_t k_big1 = (int64_t) 0xDEADBEEFCAFEBABEULL;
    const int64_t k_big2 = (int64_t) 0x8000000000000001ULL;
    const int64_t k_big3 = (int64_t) 0xFFFFFFFFFFFFFFFFULL; /* == -1 位模式 */
    assert(k_big3 == -1);
    assert(lv_hashtable_i64_insert(ht, k_big1, (void *) (intptr_t) 0xF0));
    assert(lv_hashtable_i64_insert(ht, k_big2, (void *) (intptr_t) 0xF1));
    /* k_big3 与 -1 位模式相同 → 已存在，不得重复插入 */
    assert(!lv_hashtable_i64_insert(ht, k_big3, (void *) (intptr_t) 0xF2));

    assert(lv_hashtable_i64_get(ht, 0) == (void *) (intptr_t) 0xA);
    assert(lv_hashtable_i64_get(ht, 1) == (void *) (intptr_t) 0xB);
    assert(lv_hashtable_i64_get(ht, -1) == (void *) (intptr_t) 0xC);
    assert(lv_hashtable_i64_get(ht, INT64_MAX) == (void *) (intptr_t) 0xD);
    assert(lv_hashtable_i64_get(ht, INT64_MIN) == (void *) (intptr_t) 0xE);
    assert(lv_hashtable_i64_get(ht, k_big1) == (void *) (intptr_t) 0xF0);
    assert(lv_hashtable_i64_get(ht, k_big2) == (void *) (intptr_t) 0xF1);
    assert(lv_hashtable_i64_contains(ht, k_big3));
    assert(lv_hashtable_i64_count(ht) == 7);

    /* 删除边界键后再查 */
    assert(lv_hashtable_i64_remove(ht, INT64_MIN));
    assert(!lv_hashtable_i64_contains(ht, INT64_MIN));
    assert(lv_hashtable_i64_remove(ht, k_big1));
    assert(!lv_hashtable_i64_contains(ht, k_big1));

    lv_hashtable_i64_destroy(ht);
    printf("  PASSED\n");
}

/* ---------- (d) 大量连续键：扩容重哈希 + 哈希分布 ---------- */

#define MASSIVE_N 10000

static void test_i64_massive(void) {
    printf("Testing i64 massive insert (10000 consecutive keys)...\n");

    /* 极小初始容量，强制多次扩容重哈希 */
    lvHashtable *ht = lv_hashtable_i64_create(4);
    assert(ht != NULL);

    const int64_t base = 100000; /* 从大数起步的连续区间 */
    for (int64_t i = 0; i < MASSIVE_N; i++) {
        int64_t k = base + i;
        assert(lv_hashtable_i64_insert(ht, k, (void *) (intptr_t) (k * 2)));
    }
    assert(lv_hashtable_i64_count(ht) == MASSIVE_N);

    /* 全部可查（逐一遍历也验证哈希分布下探测正确） */
    for (int64_t i = 0; i < MASSIVE_N; i++) {
        int64_t k = base + i;
        assert(lv_hashtable_i64_contains(ht, k));
        assert(lv_hashtable_i64_get(ht, k) == (void *) (intptr_t) (k * 2));
    }
    /* 区间外键不存在 */
    assert(!lv_hashtable_i64_contains(ht, base - 1));
    assert(!lv_hashtable_i64_contains(ht, base + MASSIVE_N));

    /* 删除一半后：剩余键仍全部可查（tombstone 链完整） */
    for (int64_t i = 0; i < MASSIVE_N; i += 2) {
        assert(lv_hashtable_i64_remove(ht, base + i));
    }
    assert(lv_hashtable_i64_count(ht) == MASSIVE_N / 2);
    for (int64_t i = 1; i < MASSIVE_N; i += 2) {
        int64_t k = base + i;
        assert(lv_hashtable_i64_contains(ht, k));
        assert(lv_hashtable_i64_get(ht, k) == (void *) (intptr_t) (k * 2));
    }
    for (int64_t i = 0; i < MASSIVE_N; i += 2) {
        assert(!lv_hashtable_i64_contains(ht, base + i));
    }

    /* 在墓碑之上继续插入新键（墓碑复用 + 触发扩容），全部可查 */
    for (int64_t i = 0; i < MASSIVE_N; i++) {
        int64_t k = base + MASSIVE_N + i; /* 新区间 */
        assert(lv_hashtable_i64_insert(ht, k, (void *) (intptr_t) k));
    }
    for (int64_t i = 0; i < MASSIVE_N; i++) {
        assert(lv_hashtable_i64_contains(ht, base + MASSIVE_N + i));
        assert(lv_hashtable_i64_get(ht, base + MASSIVE_N + i) == (void *) (intptr_t) (base + MASSIVE_N + i));
    }
    assert(lv_hashtable_i64_count(ht) == MASSIVE_N / 2 + MASSIVE_N);

    lv_hashtable_i64_destroy(ht);
    printf("  PASSED\n");
}

/* ---------- (e) 碰撞场景：同桶键对 + tombstone 探测链 ---------- */

static void test_i64_collision_probe_chain(void) {
    printf("Testing i64 collision probe chain...\n");

    lvHashtable *ht = lv_hashtable_i64_create(8); /* 实际容量 8，便于制造冲突 */
    assert(ht != NULL);

    /* 暴力找出与 k1 同桶（哈希一致）的第二个键 */
    const int64_t k1 = 0x1234567890ABCDEFLL;
    unsigned b1 = lv_hashtable_i64_hash(k1, 8);
    int64_t k2 = 0;
    for (int64_t cand = 1; cand < 200000; cand++) {
        if (lv_hashtable_i64_hash(cand, 8) == b1) {
            k2 = cand;
            break;
        }
    }
    assert(k2 != 0); /* 8 桶碰撞概率 1/8，必定能找到 */

    assert(lv_hashtable_i64_insert(ht, k1, (void *) (intptr_t) 0x11));
    assert(lv_hashtable_i64_insert(ht, k2, (void *) (intptr_t) 0x22)); /* 与 k1 同桶 → 线性探测后移 */
    assert(lv_hashtable_i64_get(ht, k1) == (void *) (intptr_t) 0x11);
    assert(lv_hashtable_i64_get(ht, k2) == (void *) (intptr_t) 0x22);

    /* 删除 k1（置墓碑）后，k2 的探测链必须仍完整可查 */
    assert(lv_hashtable_i64_remove(ht, k1));
    assert(!lv_hashtable_i64_contains(ht, k1));
    assert(lv_hashtable_i64_get(ht, k2) == (void *) (intptr_t) 0x22);

    /* 同桶插入新键 k3：应复用 k1 留下的墓碑槽位 */
    int64_t k3 = 0;
    for (int64_t cand = 1; cand < 200000; cand++) {
        if (lv_hashtable_i64_hash(cand, 8) == b1 && cand != k2) {
            k3 = cand;
            break;
        }
    }
    assert(k3 != 0 && k3 != k2);
    assert(lv_hashtable_i64_insert(ht, k3, (void *) (intptr_t) 0x33));
    assert(lv_hashtable_i64_get(ht, k3) == (void *) (intptr_t) 0x33);
    assert(lv_hashtable_i64_get(ht, k2) == (void *) (intptr_t) 0x22);
    assert(lv_hashtable_i64_count(ht) == 2);

    /* 固定间隔键（低 12 位相同的高冲突模式）也能完整存取 */
    lvHashtable *ht2 = lv_hashtable_i64_create(0);
    assert(ht2 != NULL);
    for (int m = 1; m <= 3000; m++) {
        int64_t k = (int64_t) m * 4096;
        assert(lv_hashtable_i64_insert(ht2, k, (void *) (intptr_t) m));
    }
    for (int m = 1; m <= 3000; m++) {
        int64_t k = (int64_t) m * 4096;
        assert(lv_hashtable_i64_contains(ht2, k));
        assert(lv_hashtable_i64_get(ht2, k) == (void *) (intptr_t) m);
    }
    lv_hashtable_i64_destroy(ht2);

    lv_hashtable_i64_destroy(ht);
    printf("  PASSED\n");
}

/* ---------- (f) foreach 遍历 ---------- */

typedef struct {
    int64_t keys[MASSIVE_N];
    int seen;
} I64VisitCtx;

static void visit_i64(int64_t key, void *value, void *ctx) {
    I64VisitCtx *c = (I64VisitCtx *) ctx;
    assert(c->seen < MASSIVE_N);
    assert((int64_t) (intptr_t) value == key + 1); /* 值应与键关联 */
    c->keys[c->seen++] = key;
}

static void test_i64_foreach(void) {
    printf("Testing i64 foreach visits all entries exactly once...\n");

    lvHashtable *ht = lv_hashtable_i64_create(16);
    assert(ht != NULL);

    I64VisitCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    const int n = 2000;
    for (int64_t i = 0; i < n; i++) {
        int64_t k = i * 3 + 7;
        assert(lv_hashtable_i64_insert(ht, k, (void *) (intptr_t) (k + 1)));
    }

    lv_hashtable_i64_foreach(ht, visit_i64, &ctx);
    assert(ctx.seen == n);

    /* 每个键恰好访问一次 */
    for (int a = 0; a < ctx.seen; a++) {
        for (int b = a + 1; b < ctx.seen; b++) {
            assert(ctx.keys[a] != ctx.keys[b]);
        }
    }
    /* 所有键都在预期集合内 */
    for (int a = 0; a < ctx.seen; a++) {
        assert(ctx.keys[a] % 3 == 1); /* 7 = 2*3+1，k = i*3+7 ≡ 1 (mod 3) */
        assert(ctx.keys[a] >= 7 && ctx.keys[a] < (int64_t) n * 3 + 7);
    }

    lv_hashtable_i64_destroy(ht);
    printf("  PASSED\n");
}

/* ---------- (g) int / string 形态回归 ---------- */

static void test_int_kind_regression(void) {
    printf("Testing int-kind regression...\n");

    lvHashtable *ht = lv_hashtable_int_create(0);
    assert(ht != NULL);
    for (int i = 0; i < 10000; i++) {
        assert(lv_hashtable_int_insert(ht, i, (void *) (intptr_t) (i + 1)));
    }
    assert(lv_hashtable_int_count(ht) == 10000);
    for (int i = 0; i < 10000; i++) {
        assert(lv_hashtable_int_get(ht, i) == (void *) (intptr_t) (i + 1));
    }
    assert(!lv_hashtable_int_insert(ht, 0, (void *) (intptr_t) 999)); /* 不覆盖 */
    assert(lv_hashtable_int_get(ht, 0) == (void *) (intptr_t) 1);
    assert(lv_hashtable_int_remove(ht, 5000));
    assert(!lv_hashtable_int_contains(ht, 5000));
    assert(lv_hashtable_int_get(ht, 5001) == (void *) (intptr_t) 5002);
    lv_hashtable_int_destroy(ht);
    printf("  PASSED\n");
}

static void test_str_kind_regression(void) {
    printf("Testing str-kind regression...\n");

    lvHashtable *ht = lv_hashtable_str_create(0);
    assert(ht != NULL);
    assert(lv_hashtable_str_insert(ht, "alpha", (void *) (intptr_t) 1));
    assert(lv_hashtable_str_insert(ht, "beta", (void *) (intptr_t) 2));
    assert(lv_hashtable_str_insert(ht, "gamma", (void *) (intptr_t) 3));
    assert(!lv_hashtable_str_insert(ht, "alpha", (void *) (intptr_t) 9)); /* 不覆盖 */
    assert(lv_hashtable_str_get(ht, "alpha") == (void *) (intptr_t) 1);
    assert(lv_hashtable_str_contains(ht, "beta"));
    assert(lv_hashtable_str_count(ht) == 3);
    assert(lv_hashtable_str_remove(ht, "beta"));
    assert(!lv_hashtable_str_contains(ht, "beta"));
    assert(lv_hashtable_str_count(ht) == 2);
    lv_hashtable_str_destroy(ht);
    printf("  PASSED\n");
}

/* ---------- (h) destroy 后无新增泄漏 ---------- */

static void test_i64_destroy_no_leak(void) {
    printf("Testing i64 destroy (no leak)...\n");

    /* lv_init 可能持有少量全局分配：以操作前报告数为基线 */
    int base = lv_memory_leak_report(NULL);
    for (int round = 0; round < 5; round++) {
        lvHashtable *ht = lv_hashtable_i64_create(4);
        assert(ht != NULL);
        for (int i = 0; i < 2000; i++) {
            int64_t k = (int64_t) i + (int64_t) round * 100000;
            assert(lv_hashtable_i64_insert(ht, k, (void *) (intptr_t) (k + 1)));
            assert(lv_hashtable_i64_remove(ht, k)); /* 插删混合，制造墓碑与扩容 */
        }
        lv_hashtable_i64_destroy(ht);
    }
    int after = lv_memory_leak_report(NULL);
    assert(after <= base);
    printf("  PASSED (leak baseline=%d after=%d)\n", base, after);
}

/* ---------- 主函数 ---------- */

int main(void) {
    printf("=== Lv-00 Hashtable Test Suite ===\n\n");

    if (!lv_init()) {
        fprintf(stderr, "Failed to initialize Lv-00 system\n");
        return 1;
    }

    test_i64_basic();
    test_i64_overwrite_rejected();
    test_i64_boundary_values();
    test_i64_massive();
    test_i64_collision_probe_chain();
    test_i64_foreach();
    test_int_kind_regression();
    test_str_kind_regression();
    test_i64_destroy_no_leak();

    printf("\nAll hashtable tests passed.\n");
    return 0;
}
