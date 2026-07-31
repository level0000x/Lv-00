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

/* 在记忆化表中查找 */
int memo_find(ProofContext *ctx, const PropFormula *goal, uint64_t phash) {
    for (int i = 0; i < ctx->memo_count; i++) {
        if (ctx->memo[i].goal == goal && ctx->memo[i].premises_hash == phash) {
            return i;
        }
    }
    return -1;
}

/* 添加记忆化条目 */
void memo_add(ProofContext *ctx, const PropFormula *goal, uint64_t phash, bool proven) {
    if (ctx->memo_count >= MAX_MEMO_ENTRIES)
        return;
    ctx->memo[ctx->memo_count].goal = goal;
    ctx->memo[ctx->memo_count].premises_hash = phash;
    ctx->memo[ctx->memo_count].proven = proven;
    ctx->memo[ctx->memo_count].searched = true;
    ctx->memo_count++;
}

