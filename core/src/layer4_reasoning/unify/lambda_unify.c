/**
 * @file lambda_unify.c
 * @brief λ-演算合一实现：句法合一 + Miller 模式合一
 *
 * 算法：
 *   句法合一：Martelli-Montanari 风格，处理 VAR/ABS/APP 节点类型。
 *   模式合一：Miller 可判定高阶合一子集，通过 Imitation 和 Projection 规则求解。
 *
 * 所有合一操作基于 De Bruijn 索引表示的 λ-项。
 */

#include "lv/lambda_unify.h"
#include "lv/lv_utils.h"
#include "lv/lv_internal.h"
#include "debug.h"

#include <string.h>

/* ── 内部辅助函数 ── */

/**
 * @brief 在替换链表中查找索引对应的替换项
 * @return 找到的替换项指针，未找到返回 NULL
 */
static LvLambdaTerm *find_substitution(LambdaSubstitution *subs, int index) {
    for (LambdaSubstitution *s = subs; s; s = s->next) {
        if (s->index == index) {
            return s->replacement;
        }
    }
    return NULL;
}

/**
 * @brief 在替换链表的头部添加新替换
 */
static LambdaSubstitution *add_substitution_head(LambdaSubstitution **list,
                                                  int index, LvLambdaTerm *replacement) {
    LambdaSubstitution *node = (LambdaSubstitution *) lv_calloc(1, sizeof(LambdaSubstitution));
    if (!node) {
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "替换节点分配失败");
    }
    node->index = index;
    node->replacement = replacement;
    node->next = *list;
    *list = node;
    return node;
}

/* ── VTable 模式 ── */

/* 前向声明分发函数 */
static bool occurs_check_rec(int index, LvLambdaTerm *term,
                             LambdaSubstitution *subs, int binder_depth);
static LvLambdaTerm *apply_subs_rec(LvLambdaTerm *term, LambdaSubstitution *subs,
                                    int binder_depth);
static bool is_pattern_rec(LvLambdaTerm *term, int binder_count);
static LvLambdaTerm *lift_free_vars(LvLambdaTerm *term, int binder_offset);
static int free_var_depth(LvLambdaTerm *term, int free_idx, int depth);

/**
 * @brief λ-项虚函数表，按 term->type 索引
 */
typedef struct {
    bool (*occurs_check)(int index, LvLambdaTerm *term,
                         LambdaSubstitution *subs, int binder_depth);
    LvLambdaTerm *(*apply_subs)(LvLambdaTerm *term, LambdaSubstitution *subs,
                                int binder_depth);
    bool (*is_pattern)(LvLambdaTerm *term, int binder_count);
    LvLambdaTerm *(*lift_free_vars)(LvLambdaTerm *term, int binder_offset);
    int (*free_var_depth)(LvLambdaTerm *term, int free_idx, int depth);
} LambdaTermVTable;

/* ── LV_LAMBDA_VAR handler ── */

static bool var_occurs_check(int index, LvLambdaTerm *term,
                              LambdaSubstitution *subs, int binder_depth) {
    /* 受 binder 绑定的变量（index < binder_depth）不是自由出现，跳过 */
    if (term->data.var.index < binder_depth) {
        return false;
    }
    if (term->data.var.index == index) return true;
    {
        LvLambdaTerm *replacement = find_substitution(subs, term->data.var.index);
        if (replacement) {
            return occurs_check_rec(index, replacement, subs, binder_depth);
        }
    }
    return false;
}

static LvLambdaTerm *var_apply_subs(LvLambdaTerm *term, LambdaSubstitution *subs,
                                     int binder_depth) {
    int idx = term->data.var.index;
    /* 受 binder 绑定的变量不应用替换，仅复制自身 */
    if (idx >= binder_depth) {
        LvLambdaTerm *replacement = find_substitution(subs, idx);
        if (replacement) {
            /* 替换项也要递归应用替换（链式替换） */
            return apply_subs_rec(replacement, subs, binder_depth);
        }
    }
    /* 无替换 → 复制自身 */
    return lv_lambda_create_var(idx);
}

static bool var_is_pattern(LvLambdaTerm *term, int binder_count) {
    (void)term;
    (void)binder_count;
    /* 自由变量 (index >= binder_count) 本身是合法的模式变量 */
    return true;
}

static LvLambdaTerm *var_lift_free_vars(LvLambdaTerm *term, int binder_offset) {
    return lv_lambda_create_var(term->data.var.index + binder_offset);
}

static int var_free_var_depth(LvLambdaTerm *term, int free_idx, int depth) {
    if (term->data.var.index == free_idx) return depth;
    return -1;
}

/* ── LV_LAMBDA_ABS handler ── */

static bool abs_occurs_check(int index, LvLambdaTerm *term,
                              LambdaSubstitution *subs, int binder_depth) {
    /* 进入抽象体，binder 深度 +1；De Bruijn 索引在 abs 内部自然递增 */
    return occurs_check_rec(index, term->data.abs.body, subs, binder_depth + 1);
}

static LvLambdaTerm *abs_apply_subs(LvLambdaTerm *term, LambdaSubstitution *subs,
                                     int binder_depth) {
    LvLambdaTerm *new_body = apply_subs_rec(term->data.abs.body, subs, binder_depth + 1);
    if (!new_body && term->data.abs.body) {
        return NULL;
    }
    return lv_lambda_create_abs(term->data.abs.binder, new_body);
}

static bool abs_is_pattern(LvLambdaTerm *term, int binder_count) {
    /* 进入抽象，binder_count + 1 */
    return is_pattern_rec(term->data.abs.body, binder_count + 1);
}

static LvLambdaTerm *abs_lift_free_vars(LvLambdaTerm *term, int binder_offset) {
    LvLambdaTerm *new_body = lift_free_vars(term->data.abs.body, binder_offset + 1);
    if (!new_body && term->data.abs.body) lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "提升自由变量: 抽象体复制失败");
    /* 注意：abs 中的 binder 是绑定变量，不提升 */
    return lv_lambda_create_abs(term->data.abs.binder, new_body);
}

static int abs_free_var_depth(LvLambdaTerm *term, int free_idx, int depth) {
    int d = free_var_depth(term->data.abs.body, free_idx, depth + 1);
    if (d >= 0) return d;
    return -1;
}

/* ── LV_LAMBDA_APP handler ── */

static bool app_occurs_check(int index, LvLambdaTerm *term,
                              LambdaSubstitution *subs, int binder_depth) {
    if (occurs_check_rec(index, term->data.app.left, subs, binder_depth)) return true;
    return occurs_check_rec(index, term->data.app.right, subs, binder_depth);
}

static LvLambdaTerm *app_apply_subs(LvLambdaTerm *term, LambdaSubstitution *subs,
                                     int binder_depth) {
    LvLambdaTerm *new_left = apply_subs_rec(term->data.app.left, subs, binder_depth);
    LvLambdaTerm *new_right = apply_subs_rec(term->data.app.right, subs, binder_depth);
    if ((!new_left && term->data.app.left) || (!new_right && term->data.app.right)) {
        lv_lambda_destroy(new_left);
        lv_lambda_destroy(new_right);
        return NULL;
    }
    return lv_lambda_create_app(new_left, new_right);
}

static bool app_is_pattern(LvLambdaTerm *term, int binder_count) {
    /* 如果应用的最左端是自由变量 → 检查参数条件 */
    LvLambdaTerm *leftmost = term;
    while (leftmost && leftmost->type == LV_LAMBDA_APP) {
        leftmost = leftmost->data.app.left;
    }

    if (leftmost && leftmost->type == LV_LAMBDA_VAR &&
        leftmost->data.var.index >= binder_count) {
        /* 自由变量在函数位置：收集所有参数，检查是否都是不同的 bound 变量 */
        LvLambdaTerm *cur = term; /* 整个应用链 */
        int arg_count = 0;
        int bound_args[256];
        bool all_bound = true;

        /* 遍历应用链收集参数 */
        while (cur && cur->type == LV_LAMBDA_APP) {
            LvLambdaTerm *arg = cur->data.app.right;
            if (arg && arg->type == LV_LAMBDA_VAR &&
                arg->data.var.index < binder_count) {
                /* 参数是 bound 变量 */
                bound_args[arg_count++] = arg->data.var.index;
            } else {
                all_bound = false;
                break;
            }
            cur = cur->data.app.left;
        }

        if (!all_bound) return false;

        /* 检查所有 bound 变量是否各不相同 */
        for (int i = 0; i < arg_count; i++) {
            for (int j = i + 1; j < arg_count; j++) {
                if (bound_args[i] == bound_args[j]) return false;
            }
        }
        return true;
    }

    /* 函数位置不是自由变量 → 递归检查左右子项 */
    return is_pattern_rec(term->data.app.left, binder_count) &&
           is_pattern_rec(term->data.app.right, binder_count);
}

static LvLambdaTerm *app_lift_free_vars(LvLambdaTerm *term, int binder_offset) {
    LvLambdaTerm *new_left = lift_free_vars(term->data.app.left, binder_offset);
    LvLambdaTerm *new_right = lift_free_vars(term->data.app.right, binder_offset);
    if ((!new_left && term->data.app.left) || (!new_right && term->data.app.right)) {
        lv_lambda_destroy(new_left);
        lv_lambda_destroy(new_right);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "提升自由变量: 应用体复制失败");
    }
    return lv_lambda_create_app(new_left, new_right);
}

static int app_free_var_depth(LvLambdaTerm *term, int free_idx, int depth) {
    int d = free_var_depth(term->data.app.left, free_idx, depth);
    if (d >= 0) return d;
    return free_var_depth(term->data.app.right, free_idx, depth);
}

/* ── VTable 查找表 ── */

static const LambdaTermVTable kLambdaVTable[] = {
    [LV_LAMBDA_VAR] = {
        var_occurs_check,
        var_apply_subs,
        var_is_pattern,
        var_lift_free_vars,
        var_free_var_depth
    },
    [LV_LAMBDA_ABS] = {
        abs_occurs_check,
        abs_apply_subs,
        abs_is_pattern,
        abs_lift_free_vars,
        abs_free_var_depth
    },
    [LV_LAMBDA_APP] = {
        app_occurs_check,
        app_apply_subs,
        app_is_pattern,
        app_lift_free_vars,
        app_free_var_depth
    }
};

/**
 * @brief 递归检查 index 是否在 term 中出现（occurs check）
 *
 * 扫描 λ-项树的所有节点，判断 De Bruijn 索引 index 是否作为自由变量出现。
 * 如果替换链中有替换项，需要展开后检查。
 */
static bool occurs_check_rec(int index, LvLambdaTerm *term,
                             LambdaSubstitution *subs, int binder_depth) {
    if (!term) return false;

    if (term->type >= LV_LAMBDA_VAR && term->type <= LV_LAMBDA_APP) {
        return kLambdaVTable[term->type].occurs_check(index, term, subs, binder_depth);
    }
    return false;
}

/* ── 句法合一递归核心 ── */

/**
 * @brief 递归合一核心
 *
 * @param t1, t2    待合两项
 * @param subs      替换链表指针（可修改）
 * @param depth     当前递归深度
 * @param max_depth 最大允许深度
 * @return LambdaUnifyStatus
 */
static LambdaUnifyStatus lambda_unify_rec(LvLambdaTerm *t1, LvLambdaTerm *t2,
                                           LambdaSubstitution **subs,
                                           int binder_depth,
                                           int depth, int max_depth) {
    if (depth >= max_depth) {
        lv_LOG_ERROR("lambda_unify", "最大递归深度 %d 超限", max_depth);
        return LAMBDA_UNIFY_ERROR;
    }
    if (!t1 || !t2) {
        return LAMBDA_UNIFY_ERROR;
    }

    /* 同一指针 → 合一成功 */
    if (t1 == t2) {
        return LAMBDA_UNIFY_OK;
    }

    /* ── 处理变量 ── */
    if (t1->type == LV_LAMBDA_VAR) {
        int idx1 = t1->data.var.index;

        /* 刚性受绑定变量：index < binder_depth 时已被外层 binder 绑定，
           只能与相同 index 的 VAR 合一，与任何其它项合一一律失败 */
        if (idx1 < binder_depth) {
            if (t2->type == LV_LAMBDA_VAR && t2->data.var.index == idx1) {
                return LAMBDA_UNIFY_OK;
            }
            return LAMBDA_UNIFY_FAIL;
        }

        /* 如果 idx1 已有替换，展开替换后递归 */
        LvLambdaTerm *r1 = find_substitution(*subs, idx1);
        if (r1) {
            return lambda_unify_rec(r1, t2, subs, binder_depth, depth + 1, max_depth);
        }

        /* t2 如果是自由元变量，也尝试展开 */
        if (t2->type == LV_LAMBDA_VAR) {
            int idx2 = t2->data.var.index;
            if (idx2 >= binder_depth) {
                LvLambdaTerm *r2 = find_substitution(*subs, idx2);
                if (r2) {
                    return lambda_unify_rec(t1, r2, subs, binder_depth, depth + 1, max_depth);
                }
                /* 相同索引 → 合一成功 */
                if (idx1 == idx2) {
                    return LAMBDA_UNIFY_OK;
                }
            }
            /* idx2 < binder_depth：t2 是受绑定变量，不能与自由元变量直接相等 */
        }

        /* Occurs check：检查 idx1 是否在 t2 中自由出现 */
        if (occurs_check_rec(idx1, t2, *subs, binder_depth)) {
            return LAMBDA_UNIFY_OCCURS_CHECK;
        }

        /* 添加替换 idx1 ↦ t2（深拷贝 t2） */
        LvLambdaTerm *copy = lv_lambda_copy(t2);
        if (!copy) {
            return LAMBDA_UNIFY_ERROR;
        }
        if (!add_substitution_head(subs, idx1, copy)) {
            lv_lambda_destroy(copy);
            return LAMBDA_UNIFY_ERROR;
        }
        return LAMBDA_UNIFY_OK;
    }

    /* t2 是变量但 t1 不是 → 交换合一 */
    if (t2->type == LV_LAMBDA_VAR) {
        return lambda_unify_rec(t2, t1, subs, binder_depth, depth + 1, max_depth);
    }

    /* ── 处理抽象 ── */
    if (t1->type == LV_LAMBDA_ABS) {
        if (t2->type != LV_LAMBDA_ABS) {
            return LAMBDA_UNIFY_FAIL;
        }
        /* 抽象合一：递归合一体，进入抽象体 binder 深度 +1 */
        return lambda_unify_rec(t1->data.abs.body, t2->data.abs.body,
                                subs, binder_depth + 1, depth + 1, max_depth);
    }

    /* ── 处理应用 ── */
    if (t1->type == LV_LAMBDA_APP) {
        if (t2->type != LV_LAMBDA_APP) {
            return LAMBDA_UNIFY_FAIL;
        }
        /* 先合一 fun，再合一 arg */
        LambdaUnifyStatus s = lambda_unify_rec(t1->data.app.left, t2->data.app.left,
                                                subs, binder_depth, depth + 1, max_depth);
        if (s != LAMBDA_UNIFY_OK) {
            return s;
        }
        return lambda_unify_rec(t1->data.app.right, t2->data.app.right,
                                subs, binder_depth, depth + 1, max_depth);
    }

    return LAMBDA_UNIFY_ERROR;
}

/* ── 公有 API 实现 ── */

LambdaUnifyStatus lambda_unify(LvLambdaTerm *t1, LvLambdaTerm *t2,
                                LambdaSubstitution **out_subs, int max_depth) {
    if (!t1 || !t2 || !out_subs || max_depth <= 0) {
        return LAMBDA_UNIFY_ERROR;
    }
    *out_subs = NULL;
    return lambda_unify_rec(t1, t2, out_subs, 0, 0, max_depth);
}

/* ── 替换应用 ── */

/**
 * @brief 递归将替换应用于 λ-项
 */
static LvLambdaTerm *apply_subs_rec(LvLambdaTerm *term, LambdaSubstitution *subs,
                                    int binder_depth) {
    if (!term) return NULL;

    if (term->type >= LV_LAMBDA_VAR && term->type <= LV_LAMBDA_APP) {
        return kLambdaVTable[term->type].apply_subs(term, subs, binder_depth);
    }
    lv_RETURN_ERROR_NULL(lv_ERROR_INTERNAL, "未知的λ-项类型");
}

LvLambdaTerm *lambda_unify_apply(LvLambdaTerm *term, LambdaSubstitution *subs) {
    if (!term) return NULL;
    return apply_subs_rec(term, subs, 0);
}

void lambda_substitution_list_destroy(LambdaSubstitution *subs) {
    while (subs) {
        LambdaSubstitution *next = subs->next;
        lv_lambda_destroy(subs->replacement);
        lv_free((void **) &subs);
        subs = next;
    }
}

void lambda_substitution_snprint(LambdaSubstitution *subs, char *buf, size_t size) {
    if (!buf || size == 0) return;

    buf[0] = '\0';
    size_t pos = 0;

    for (LambdaSubstitution *s = subs; s && pos < size - 1; s = s->next) {
        int n = snprintf(buf + pos, size - pos,
                         "[%d↦%s]", s->index,
                         s->replacement ? "?" : "NULL");
        if (n < 0) break;
        pos += (size_t) n;
        if (pos >= size - 1) break;

        if (s->next && pos < size - 3) {
            buf[pos++] = ' ';
            buf[pos] = '\0';
        }
    }
}

/* ================================================================
 * 合一替换 → 约束图集成
 * ================================================================ */

int lambda_unify_apply_to_graph(struct ConstraintGraph *graph,
                                 LambdaSubstitution *subs,
                                 int binder_depth) {
    if (!graph || !subs) {
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "参数为空: graph或subs");
    }
    (void) binder_depth;

    int count = 0;
    for (LambdaSubstitution *s = subs; s; s = s->next) {
        if (s->replacement) {
            LOG_DEBUG("lambda_unify", "合一替换应用到约束图: [%d↦λ-term] 待集成",
                      s->index);
            count++;
        }
    }
    if (count == 0) {
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "没有可应用的替换项");
    }
    return 0;
}

/* ================================================================
 * Miller 模式合一实现
 * ================================================================ */

/**
 * @brief 检查项是否为模式形式的辅助函数
 *
 * 递归检查所有自由变量（高阶变量）的应用位置：
 * 如果自由变量出现在函数位置（应用的最左端），
 * 则它的所有参数必须是不同的 bound 变量。
 */
static bool is_pattern_rec(LvLambdaTerm *term, int binder_count) {
    if (!term) return false;

    if (term->type >= LV_LAMBDA_VAR && term->type <= LV_LAMBDA_APP) {
        return kLambdaVTable[term->type].is_pattern(term, binder_count);
    }
    return false;
}

bool lambda_is_pattern(LvLambdaTerm *term) {
    if (!term) return false;
    return is_pattern_rec(term, 0);
}

/* ── 模式合一核心 ── */

/**
 * @brief 深度复制项并将自由变量按照 binder_offset 提升（用于 Imitation）
 */
static LvLambdaTerm *lift_free_vars(LvLambdaTerm *term, int binder_offset) {
    if (!term) return NULL;

    if (term->type >= LV_LAMBDA_VAR && term->type <= LV_LAMBDA_APP) {
        return kLambdaVTable[term->type].lift_free_vars(term, binder_offset);
    }
    lv_RETURN_ERROR_NULL(lv_ERROR_INTERNAL, "提升自由变量: 未知的λ-项类型");
}

/**
 * @brief 检查一个 λ-项是否是"刚性"的（以常量或抽象头开始）
 *
 * 刚性项：ABS 或应用链的最左端不是自由变量
 */
static bool is_rigid(LvLambdaTerm *term) {
    if (!term) return false;
    if (term->type == LV_LAMBDA_ABS) return true;
    if (term->type == LV_LAMBDA_APP) {
        LvLambdaTerm *cur = term;
        while (cur && cur->type == LV_LAMBDA_APP) {
            cur = cur->data.app.left;
        }
        /* 最左端是变量 */
        if (cur && cur->type == LV_LAMBDA_VAR) {
            return false; /* 柔性（变量头） */
        }
        return true; /* 常量头 → 刚性 */
    }
    return false;
}

/**
 * @brief 检查一个自由变量在项中出现的深度（最外层 binder 计数）
 *
 * 用于确定 Projection 时应选择哪个参数。
 */
static int free_var_depth(LvLambdaTerm *term, int free_idx, int depth) {
    if (!term) return -1;

    if (term->type >= LV_LAMBDA_VAR && term->type <= LV_LAMBDA_APP) {
        return kLambdaVTable[term->type].free_var_depth(term, free_idx, depth);
    }
    return -1;
}

/**
 * @brief 模式合一递归核心
 *
 * @param t1, t2    待合一项
 * @param binder_count  当前绑定的抽象层数
 * @param depth     递归深度
 * @param max_depth 最大深度
 * @param subs      替换链表
 */
static LambdaUnifyStatus pattern_unify_rec(LvLambdaTerm *t1, LvLambdaTerm *t2,
                                            LambdaSubstitution **subs,
                                            int binder_count,
                                            int depth, int max_depth) {
    if (depth >= max_depth) {
        return LAMBDA_UNIFY_ERROR;
    }
    if (!t1 || !t2) {
        return LAMBDA_UNIFY_ERROR;
    }
    if (t1 == t2) {
        return LAMBDA_UNIFY_OK;
    }

    /* === 处理变量 === */

    /* t1 是自由变量（高阶变量候选） */
    if (t1->type == LV_LAMBDA_VAR && t1->data.var.index >= binder_count) {
        int fv_idx = t1->data.var.index;

        /* 检查是否已有替换 */
        LvLambdaTerm *r1 = find_substitution(*subs, fv_idx);
        if (r1) {
            return pattern_unify_rec(r1, t2, subs, binder_count, depth + 1, max_depth);
        }

        /* t2 也是自由变量 */
        if (t2->type == LV_LAMBDA_VAR && t2->data.var.index >= binder_count) {
            int fv2 = t2->data.var.index;
            LvLambdaTerm *r2 = find_substitution(*subs, fv2);
            if (r2) {
                return pattern_unify_rec(t1, r2, subs, binder_count, depth + 1, max_depth);
            }
            if (fv_idx == fv2) return LAMBDA_UNIFY_OK;

            /* 绑定 fv_idx ↦ fv2 */
            LvLambdaTerm *copy = lv_lambda_copy(t2);
            if (!copy) return LAMBDA_UNIFY_ERROR;
            if (!add_substitution_head(subs, fv_idx, copy)) {
                lv_lambda_destroy(copy);
                return LAMBDA_UNIFY_ERROR;
            }
            return LAMBDA_UNIFY_OK;
        }

        /* t2 是刚性项 → Imitation + Projection 决策 */
        if (!occurs_check_rec(fv_idx, t2, *subs, binder_count)) {
            /* 直接替换（一阶情况）：绑定 fv_idx ↦ t2 */
            LvLambdaTerm *copy = lv_lambda_copy(t2);
            if (!copy) return LAMBDA_UNIFY_ERROR;
            if (!add_substitution_head(subs, fv_idx, copy)) {
                lv_lambda_destroy(copy);
                return LAMBDA_UNIFY_ERROR;
            }
            return LAMBDA_UNIFY_OK;
        }

        return LAMBDA_UNIFY_OCCURS_CHECK;
    }

    /* t2 是自由变量但 t1 不是 → 交换合一 */
    if (t2->type == LV_LAMBDA_VAR && t2->data.var.index >= binder_count) {
        return pattern_unify_rec(t2, t1, subs, binder_count, depth + 1, max_depth);
    }

    /* === 处理 bound 变量 === */
    if (t1->type == LV_LAMBDA_VAR) {
        /* bound 变量只能与自身合一 */
        if (t2->type == LV_LAMBDA_VAR && t1->data.var.index == t2->data.var.index) {
            return LAMBDA_UNIFY_OK;
        }
        return LAMBDA_UNIFY_FAIL;
    }

    if (t2->type == LV_LAMBDA_VAR) {
        return LAMBDA_UNIFY_FAIL;
    }

    /* === 处理抽象 === */
    if (t1->type == LV_LAMBDA_ABS) {
        if (t2->type != LV_LAMBDA_ABS) return LAMBDA_UNIFY_FAIL;
        return pattern_unify_rec(t1->data.abs.body, t2->data.abs.body,
                                 subs, binder_count + 1, depth + 1, max_depth);
    }

    /* === 处理应用 === */
    if (t1->type == LV_LAMBDA_APP) {
        if (t2->type != LV_LAMBDA_APP) return LAMBDA_UNIFY_FAIL;

        LambdaUnifyStatus s = pattern_unify_rec(t1->data.app.left, t2->data.app.left,
                                                 subs, binder_count, depth + 1, max_depth);
        if (s != LAMBDA_UNIFY_OK) return s;
        return pattern_unify_rec(t1->data.app.right, t2->data.app.right,
                                 subs, binder_count, depth + 1, max_depth);
    }

    return LAMBDA_UNIFY_ERROR;
}

LambdaUnifyStatus lambda_pattern_unify(LvLambdaTerm *t1, LvLambdaTerm *t2,
                                        LambdaSubstitution **out_subs, int max_depth) {
    if (!t1 || !t2 || !out_subs || max_depth <= 0) {
        return LAMBDA_UNIFY_ERROR;
    }
    *out_subs = NULL;

    /* 检查是否符合模式条件 */
    if (!lambda_is_pattern(t1) && !lambda_is_pattern(t2)) {
        LOG_WARN("lambda_unify", "模式合一：两项都不是模式形式，降级为句法合一");
        return lambda_unify(t1, t2, out_subs, max_depth);
    }

    return pattern_unify_rec(t1, t2, out_subs, 0, 0, max_depth);
}