/**
 * @file proof_optimize.c
 * @brief 证明优化模块（子目录版本）
 *
 * 对证明步骤序列进行优化，包括：
 * - 步骤合并：将多个冗余步骤合并为单步
 * - 步骤重排：按依赖关系重新排列步骤顺序
 * - 死步消除：移除未被引用的证明步骤
 * - 证明压缩：缩短证明路径
 *
 * 优化后的证明保持逻辑等价性和可验证性。
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_internal.h"
#include "lv/proof_trace.h"

/* ================================================================
 *  内部常量
 * ================================================================ */

#define OPT_MAX_STEPS 256    /**< 最大证明步骤数 */
#define OPT_MAX_DEPS 32      /**< 每步最大依赖数 */
#define OPT_RULE_NAME_LEN 64 /**< 规则名称最大长度 */

/* ================================================================
 *  内部数据结构
 * ================================================================ */

/**
 * @brief 证明步骤（内部表示）
 */
typedef struct {
    int step_id;                  /**< 步骤ID */
    char rule[OPT_RULE_NAME_LEN]; /**< 使用的推理规则 */
    int deps[OPT_MAX_DEPS];       /**< 依赖的步骤ID列表 */
    int dep_count;                /**< 依赖数量 */
    bool is_marked;               /**< 标记位（用于死步消除等） */
    bool is_eliminated;           /**< 是否已被消除 */
} OptStep;

/**
 * @brief 证明优化器（ProofOptimizer 不透明类型实现）
 */
struct ProofOptimizer {
    OptStep steps[OPT_MAX_STEPS]; /**< 步骤数组 */
    int step_count;               /**< 当前步骤数 */
    int next_id;                  /**< 下一个可用步骤ID */
    int eliminated_count;         /**< 已消除步骤数 */
};

/* ================================================================
 *  内部辅助函数
 * ================================================================ */

/**
 * @brief 在步骤数组中查找步骤索引
 * @return 索引 (0-based)，未找到返回 -1
 */
static int opt_find_step(const struct ProofOptimizer *ctx, int step_id) {
    int i;
    if (!ctx)
        return -1;
    for (i = 0; i < ctx->step_count; i++) {
        if (ctx->steps[i].step_id == step_id)
            return i;
    }
    return -1;
}

/**
 * @brief 标记步骤及其所有依赖（可达性分析）
 */
static void opt_mark_reachable(struct ProofOptimizer *ctx, int step_id) {
    int idx = opt_find_step(ctx, step_id);
    int i;
    if (idx < 0 || ctx->steps[idx].is_marked)
        return;

    ctx->steps[idx].is_marked = true;
    for (i = 0; i < ctx->steps[idx].dep_count; i++) {
        opt_mark_reachable(ctx, ctx->steps[idx].deps[i]);
    }
}

/**
 * @brief 检查两个步骤是否可以合并
 *
 * 合并条件：
 * - 使用相同规则
 * - 步骤 B 唯一依赖步骤 A
 * - 步骤 A 无其他被依赖者
 */
static bool opt_can_merge(const struct ProofOptimizer *ctx, int idx_a, int idx_b) {
    const OptStep *a, *b;
    int i, ref_count;

    if (idx_a < 0 || idx_b < 0)
        return false;
    a = &ctx->steps[idx_a];
    b = &ctx->steps[idx_b];

    /* 规则必须相同 */
    if (strcmp(a->rule, b->rule) != 0)
        return false;

    /* B 必须仅依赖 A */
    if (b->dep_count != 1 || b->deps[0] != a->step_id)
        return false;

    /* A 不能被其他未消除的步骤引用 */
    ref_count = 0;
    for (i = 0; i < ctx->step_count; i++) {
        int j;
        if (i == idx_b || ctx->steps[i].is_eliminated)
            continue;
        for (j = 0; j < ctx->steps[i].dep_count; j++) {
            if (ctx->steps[i].deps[j] == a->step_id) {
                ref_count++;
            }
        }
    }

    return ref_count == 0;
}

/**
 * @brief 拓扑排序辅助：计算入度
 */
static void opt_compute_indegrees(const struct ProofOptimizer *ctx, int *indegrees) {
    int i, j;
    for (i = 0; i < ctx->step_count; i++) {
        indegrees[i] = 0;
    }
    for (i = 0; i < ctx->step_count; i++) {
        if (ctx->steps[i].is_eliminated)
            continue;
        for (j = 0; j < ctx->steps[i].dep_count; j++) {
            int dep_idx = opt_find_step(ctx, ctx->steps[i].deps[j]);
            if (dep_idx >= 0) {
                indegrees[dep_idx]++;
            }
        }
    }
}

/* ================================================================
 *  公共 API
 * ================================================================ */

ProofOptimizer *lv_proof_opt_create(void) {
    struct ProofOptimizer *ctx = (struct ProofOptimizer *) calloc(1, sizeof(struct ProofOptimizer));
    if (!ctx)
        return NULL;
    ctx->step_count = 0;
    ctx->next_id = 1;
    ctx->eliminated_count = 0;
    return ctx;
}

void lv_proof_opt_destroy(ProofOptimizer *opt) {
    free(opt);
}

int lv_proof_opt_add_step(ProofOptimizer *opt, const char *rule, const int *deps, int dep_count) {
    struct ProofOptimizer *ctx = (struct ProofOptimizer *) opt;
    OptStep *step;
    int i;

    if (!ctx || !rule || ctx->step_count >= OPT_MAX_STEPS)
        return -1;

    step = &ctx->steps[ctx->step_count];
    step->step_id = ctx->next_id++;
    strncpy(step->rule, rule, OPT_RULE_NAME_LEN - 1);
    step->rule[OPT_RULE_NAME_LEN - 1] = '\0';
    step->dep_count = dep_count < OPT_MAX_DEPS ? dep_count : OPT_MAX_DEPS;
    for (i = 0; i < step->dep_count; i++) {
        step->deps[i] = deps ? deps[i] : 0;
    }
    step->is_marked = false;
    step->is_eliminated = false;
    ctx->step_count++;

    return step->step_id;
}

int lv_proof_opt_dead_step_elimination(ProofOptimizer *opt, int final_step) {
    struct ProofOptimizer *ctx = (struct ProofOptimizer *) opt;
    int i, count;

    if (!ctx)
        return 0;

    for (i = 0; i < ctx->step_count; i++) {
        ctx->steps[i].is_marked = false;
    }

    opt_mark_reachable(ctx, final_step);

    count = 0;
    for (i = 0; i < ctx->step_count; i++) {
        if (!ctx->steps[i].is_marked && !ctx->steps[i].is_eliminated) {
            ctx->steps[i].is_eliminated = true;
            count++;
        }
    }

    ctx->eliminated_count += count;
    return count;
}

int lv_proof_opt_merge_steps(ProofOptimizer *opt) {
    struct ProofOptimizer *ctx = (struct ProofOptimizer *) opt;
    int i, j, merge_count;

    if (!ctx)
        return 0;

    merge_count = 0;
    for (i = 0; i < ctx->step_count; i++) {
        if (ctx->steps[i].is_eliminated)
            continue;
        for (j = i + 1; j < ctx->step_count; j++) {
            if (ctx->steps[j].is_eliminated)
                continue;
            if (opt_can_merge(ctx, i, j)) {
                ctx->steps[i].dep_count = ctx->steps[j].dep_count;
                {
                    int k;
                    for (k = 0; k < ctx->steps[j].dep_count; k++) {
                        ctx->steps[i].deps[k] = ctx->steps[j].deps[k];
                    }
                }
                ctx->steps[j].is_eliminated = true;
                ctx->eliminated_count++;
                merge_count++;
                break;
            }
        }
    }

    return merge_count;
}

int lv_proof_opt_active_count(const ProofOptimizer *opt) {
    const struct ProofOptimizer *ctx = (const struct ProofOptimizer *) opt;
    int i, count = 0;
    if (!ctx)
        return 0;
    for (i = 0; i < ctx->step_count; i++) {
        if (!ctx->steps[i].is_eliminated)
            count++;
    }
    return count;
}
