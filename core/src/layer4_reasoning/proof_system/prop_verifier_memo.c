/*
 * @file prop_verifier_memo.c
 * @brief Proposition verifier module - memoization
 * @details Split from prop_verifier.c
 */

#include "lv/prop_verifier.h"
#include "prop_verifier_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"
#include "lv/stream_context_util.h"

/* ============================================================
 * 记忆化操作
 * ============================================================ */

/* 在记忆化表中查找：组合键 = goal 指针 ^ premises_hash，哈希表索引命中后二次校验防碰撞 */
int memo_find(ProofContext *ctx, const PropFormula *goal, uint64_t phash) {
    if (!ctx->memo_index)
        return -1;
    uint64_t key = ((uint64_t) (uintptr_t) goal) ^ phash;
    void *boxed = lv_hashtable_i64_get(ctx->memo_index, (int64_t) key);
    if (!boxed)
        return -1;
    int idx = (int) (intptr_t) boxed - 1;
    if (idx < 0 || idx >= ctx->memo_count)
        return -1;
    if (ctx->memo[idx].goal == goal && ctx->memo[idx].premises_hash == phash)
        return idx;
    return -1;
}

/* 添加记忆化条目：幂等，键已存在则跳过（与调用点"先查后写"语义一致） */
void memo_add(ProofContext *ctx, const PropFormula *goal, uint64_t phash, bool proven) {
    if (!ctx->memo_index)
        ctx->memo_index = lv_hashtable_i64_create(0);
    if (!ctx->memo_index)
        return;
    uint64_t key = ((uint64_t) (uintptr_t) goal) ^ phash;
    if (lv_hashtable_i64_contains(ctx->memo_index, (int64_t) key))
        return;
    if (!lv_ensure_capacity((void **) &ctx->memo, ctx->memo_count, &ctx->memo_capacity,
                            sizeof(MemoEntry), 0))
        return;
    int idx = ctx->memo_count++;
    ctx->memo[idx].goal = goal;
    ctx->memo[idx].premises_hash = phash;
    ctx->memo[idx].proven = proven;
    ctx->memo[idx].searched = true;
    lv_hashtable_i64_insert(ctx->memo_index, (int64_t) key, (void *) (intptr_t) (idx + 1));
}

/* 释放记忆化表（在 prove 最外层退出时调用） */
void memo_destroy(ProofContext *ctx) {
    if (!ctx)
        return;
    if (ctx->memo_index) {
        lv_hashtable_i64_destroy(ctx->memo_index);
        ctx->memo_index = NULL;
    }
    lv_free((void **) &ctx->memo);
    ctx->memo_count = 0;
    ctx->memo_capacity = 0;
}

