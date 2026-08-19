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
#include "lv/debug.h"

#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/lambda_to_graph.h"

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
        int n = lv_snprintf(buf + pos, size - pos,
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
 * 合一替换 → 约束图集成（真实实例化）
 *
 * λ-替换 {index ↦ term} 应用到约束图的语义：
 *   图中"顶层自由 λ-变量引用端口"（PORT_OUTPUT、parent_block_id == -1、
 *   非形式参数、非函数块内部节点、namespace_depth == index）代表自由
 *   变量 index 的一次出现；本实现把替换项编译进图并将这些出现实例化。
 *
 * 执行流程（事务式）：
 *   准备阶段（不改图）：预编译每个替换项（闭项才可编译）、匹配槽位端口、
 *     收集全部旧连接；不可应用的条目被跳过，全部不可应用返回 NOT_FOUND。
 *   提交阶段：编译替换子图并将新节点 namespace_depth 整体平移 index
 *     （保持连接深度规则 |Δdepth|≤1 与端口不变量）、把旧连接的消费者
 *     重连到替换子图输出端口、停用旧连接与旧槽位端口（保留审计数据）。
 *   提交阶段仅在追加节点/连接时可能失败（OOM），失败即回滚：恢复被覆盖
 *     的 connected_to 指针、移除全部新节点（其约束随之移除），保证失败
 *     时图与调用前完全一致。
 *
 * 返回：0 = 至少应用了一个替换；负值 = 失败（lv_ERROR_NOT_FOUND =
 *   无可用替换或无可实例化槽位；lv_ERROR_NULL_POINTER = 参数为空）。
 * ================================================================ */

/** @brief 连接重连的撤销记录：恢复旧连接目标端口的 connected_to 指针 */
typedef struct {
    int dst_id;
    GeomNode *old_connected_to;
} LambdaConnUndo;

/** @brief 单个可应用替换条目的提交计划 */
typedef struct {
    int index;                 /**< 替换的 De Bruijn 索引 */
    LvLambdaTerm *replacement; /**< 替换项（不拥有，指向 subs 链表节点） */
    int *slot_ids;             /**< 匹配的槽位端口 id 数组 */
    int slot_count;
    int *old_conn_indices;     /**< 槽位旧 CONNECTION 的约束数组索引（扁平） */
    int *old_conn_dst_ids;     /**< 与 old_conn_indices 平行：旧连接目标端口 id */
    int old_conn_count;
    int repl_out_id;           /**< 提交后：替换子图的输出端口 id */
} LambdaApplyEntry;

/**
 * @brief 判定端口节点是否为"顶层自由 λ-变量槽位"
 *
 * 槽位 = 自由变量的一次出现：PORT_OUTPUT、非形式参数、parent_block_id
 * 为 -1（排除函数块输出端口与 app_sink 端口对）、namespace_depth 等于
 * 替换索引、且不是任何函数块的内部节点（函数块内部的变量引用是受绑定
 * 出现，λ-替换只作用于自由出现）。
 */
static bool port_is_free_lambda_slot(const ConstraintGraph *graph, const GeomNode *node, int index) {
    if (!node || !node->is_active || node->type != GEOM_PORT)
        return false;
    Port *port = node->data.port;
    if (!port || port->type != PORT_OUTPUT || port->is_formal_param)
        return false;
    if (node->parent_block_id != -1)
        return false;
    if (node->namespace_depth != index)
        return false;
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *fb = graph->nodes[i];
        if (!fb || !fb->is_active || fb->type != GEOM_FUNCTION_BLOCK)
            continue;
        if (!fb->data.func_block.internal_nodes)
            continue;
        for (int j = 0; j < fb->data.func_block.internal_node_count; j++) {
            if (fb->data.func_block.internal_nodes[j] &&
                fb->data.func_block.internal_nodes[j]->id == node->id)
                return false;
        }
    }
    return true;
}

/** @brief 获取已编译节点的有效输出端口（PORT 节点为自身，函数块为第一个输出端口） */
static int lambda_compiled_output_port(const ConstraintGraph *graph, int node_id) {
    GeomNode *node = graph_get_node(graph, node_id);
    if (!node)
        return -1;
    if (node->type == GEOM_PORT)
        return node_id;
    if (node->type == GEOM_FUNCTION_BLOCK && node->data.func_block.output_count > 0 &&
        node->data.func_block.output_port_ids)
        return node->data.func_block.output_port_ids[0];
    return -1;
}

/** @brief 释放提交计划数组（元素可为 NULL，lv_free 安全） */
static void lambda_apply_entries_destroy(LambdaApplyEntry *entries, int count) {
    if (!entries)
        return;
    for (int i = 0; i < count; i++) {
        lv_free((void **) &entries[i].slot_ids);
        lv_free((void **) &entries[i].old_conn_indices);
        lv_free((void **) &entries[i].old_conn_dst_ids);
    }
    lv_free((void **) &entries);
}

int lambda_unify_apply_to_graph(struct ConstraintGraph *graph,
                                 LambdaSubstitution *subs,
                                 int binder_depth) {
    if (!graph || !subs) {
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "lambda_unify_apply_to_graph: 参数为空 (graph/subs)");
    }

    /* ── 收集候选条目：index >= binder_depth 且 replacement 非空 ── */
    int candidate_count = 0;
    for (LambdaSubstitution *s = subs; s; s = s->next) {
        if (s->index >= binder_depth && s->replacement)
            candidate_count++;
    }
    if (candidate_count == 0) {
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "lambda_unify_apply_to_graph: 没有可应用的替换项");
    }

    /* ── 准备阶段（不改图）：预编译替换 + 匹配槽位端口 ── */
    LambdaApplyEntry *entries = lv_calloc((size_t) candidate_count, sizeof(LambdaApplyEntry));
    if (!entries) {
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "lambda_unify_apply_to_graph: 分配提交计划失败");
    }

    int entry_count = 0;
    for (LambdaSubstitution *s = subs; s; s = s->next) {
        if (s->index < binder_depth || !s->replacement)
            continue;

        /* 预编译：验证替换项可编译（闭项）。编译失败（如含自由变量）→ 跳过该条目 */
        ConstraintGraph *probe = graph_create();
        if (!probe)
            goto prepare_fail;
        int probe_root = -1;
        bool compiles = lambda_to_graph(s->replacement, probe, &probe_root);
        graph_destroy(probe);
        if (!compiles) {
            LOG_WARN("lambda_unify", "合一替换 [%d↦λ-term] 无法编译（含自由变量），跳过", s->index);
            continue;
        }

        /* 第一遍：统计匹配槽位数与旧连接数 */
        int slot_count = 0, conn_count = 0;
        for (int i = 0; i < graph->node_count; i++) {
            GeomNode *node = graph->nodes[i];
            if (!port_is_free_lambda_slot(graph, node, s->index))
                continue;
            slot_count++;
            int con_indices[32];
            int n = graph_find_constraints_involving(graph, node->id, con_indices, 32);
            for (int c = 0; c < n; c++) {
                Constraint *con = graph->constraints[con_indices[c]];
                if (con && con->is_active && con->type == CONNECTION && con->participant_count == 2 &&
                    con->participants[0] == node->id)
                    conn_count++;
            }
        }
        if (slot_count == 0)
            continue; /* 无匹配槽位 → 该条目不产生任何图变化 */

        LambdaApplyEntry *e = &entries[entry_count];
        e->index = s->index;
        e->replacement = s->replacement;
        e->slot_ids = lv_calloc((size_t) slot_count, sizeof(int));
        if (!e->slot_ids)
            goto prepare_fail;
        if (conn_count > 0) {
            e->old_conn_indices = lv_calloc((size_t) conn_count, sizeof(int));
            e->old_conn_dst_ids = lv_calloc((size_t) conn_count, sizeof(int));
            if (!e->old_conn_indices || !e->old_conn_dst_ids)
                goto prepare_fail;
        }

        /* 第二遍：填充槽位与旧连接 */
        int si = 0, ci = 0;
        for (int i = 0; i < graph->node_count; i++) {
            GeomNode *node = graph->nodes[i];
            if (!port_is_free_lambda_slot(graph, node, s->index))
                continue;
            e->slot_ids[si++] = node->id;
            int con_indices[32];
            int n = graph_find_constraints_involving(graph, node->id, con_indices, 32);
            for (int c = 0; c < n; c++) {
                Constraint *con = graph->constraints[con_indices[c]];
                if (!con || !con->is_active || con->type != CONNECTION || con->participant_count != 2 ||
                    con->participants[0] != node->id)
                    continue;
                e->old_conn_indices[ci] = con_indices[c];
                e->old_conn_dst_ids[ci] = con->participants[1];
                ci++;
            }
        }
        e->slot_count = slot_count;
        e->old_conn_count = conn_count;
        entry_count++;
    }

    if (entry_count == 0) {
        lambda_apply_entries_destroy(entries, candidate_count);
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "lambda_unify_apply_to_graph: 没有可实例化的 λ-变量槽位");
    }

    /* ── 提交阶段 ── */
    int node_count0 = graph->node_count;

    /* 撤销记录：容量 = 全部条目的旧连接数（阶段 B 追加连接的 dst 指针恢复） */
    int undo_capacity = 0;
    for (int i = 0; i < entry_count; i++)
        undo_capacity += entries[i].old_conn_count;
    LambdaConnUndo *conn_undo = NULL;
    if (undo_capacity > 0) {
        conn_undo = lv_calloc((size_t) undo_capacity, sizeof(LambdaConnUndo));
        if (!conn_undo) {
            lambda_apply_entries_destroy(entries, candidate_count);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "lambda_unify_apply_to_graph: 分配撤销记录失败");
        }
    }
    int undo_count = 0;

    /* 阶段 A：编译替换子图并平移命名空间深度 */
    for (int i = 0; i < entry_count; i++) {
        LambdaApplyEntry *e = &entries[i];
        int before = graph->node_count;
        int root_id = -1;
        if (!lambda_to_graph(e->replacement, graph, &root_id))
            goto commit_fail;
        int repl_out = lambda_compiled_output_port(graph, root_id);
        if (repl_out < 0)
            goto commit_fail;
        e->repl_out_id = repl_out;
        /* 平移新节点深度：使替换子图位于槽位（索引 index）所在深度 */
        for (int nid = before; nid < graph->node_count; nid++) {
            GeomNode *node = graph->nodes[nid];
            if (!node)
                continue;
            node->namespace_depth += e->index;
            if (node->type == GEOM_PORT && node->data.port)
                node->data.port->namespace_depth += e->index;
        }
    }

    /* 阶段 B：将旧连接的消费者重连到替换子图输出端口（先记录撤销） */
    for (int i = 0; i < entry_count; i++) {
        LambdaApplyEntry *e = &entries[i];
        for (int c = 0; c < e->old_conn_count; c++) {
            int dst_id = e->old_conn_dst_ids[c];
            GeomNode *dst = graph_get_node(graph, dst_id);
            if (!dst || dst->type != GEOM_PORT || !dst->data.port)
                goto commit_fail; /* 准备阶段已验证，不会发生 */
            conn_undo[undo_count].dst_id = dst_id;
            conn_undo[undo_count].old_connected_to = dst->data.port->connected_to;
            undo_count++;
            /* 替换子图输出端口若为 ABS 输出端口，其 connected_to 记录的是
             * body 根；graph_add_connection 的双向覆盖会破坏该关联（影响
             * 后续 graph_to_lambda 反编译），连接后恢复原值。 */
            GeomNode *repl_out_node = graph_get_node(graph, e->repl_out_id);
            GeomNode *repl_ct_saved = (repl_out_node && repl_out_node->data.port)
                                          ? repl_out_node->data.port->connected_to
                                          : NULL;
            AddConstraintResult cr = graph_add_connection(graph, e->repl_out_id, dst_id);
            if (repl_ct_saved && repl_out_node && repl_out_node->data.port)
                repl_out_node->data.port->connected_to = repl_ct_saved;
            if (cr != ADD_CONSTRAINT_OK && cr != ADD_CONSTRAINT_DUPLICATE)
                goto commit_fail;
        }
    }

    /* 阶段 C：最终化（纯字段写入，不可失败） */
    for (int i = 0; i < entry_count; i++) {
        LambdaApplyEntry *e = &entries[i];
        GeomNode *repl_out_node = graph_get_node(graph, e->repl_out_id);

        /* C1：停用槽位的旧连接（保留约束数据用于审计） */
        for (int c = 0; c < e->old_conn_count; c++) {
            Constraint *con = graph->constraints[e->old_conn_indices[c]];
            if (con && con->is_active)
                graph_deactivate_constraint(graph, con->id);
        }

        /* C2：修复仍指向槽位的 connected_to 指针（如 ABS 输出端口以槽位为体根） */
        for (int s = 0; s < e->slot_count; s++) {
            GeomNode *slot_node = graph_get_node(graph, e->slot_ids[s]);
            if (!slot_node)
                continue;
            for (int nid = 0; nid < graph->node_count; nid++) {
                GeomNode *node = graph->nodes[nid];
                if (!node || node->type != GEOM_PORT || !node->data.port)
                    continue;
                if (node == slot_node)
                    continue;
                if (node->data.port->connected_to == slot_node)
                    node->data.port->connected_to = repl_out_node;
            }
            /* C3：停用槽位端口（保留节点数据）并断开其残留指针 */
            slot_node->is_active = false;
            slot_node->data.port->connected_to = NULL;
        }
    }

    graph_mark_dirty(graph);
    lambda_apply_entries_destroy(entries, candidate_count);
    lv_free((void **) &conn_undo);

    LOG_DEBUG("lambda_unify", "合一替换已实例化到约束图: %d 个条目", entry_count);
    return 0;

commit_fail:
    /* 回滚：先恢复被覆盖的 connected_to 指针，再移除全部新节点（其约束随之移除） */
    for (int i = undo_count - 1; i >= 0; i--) {
        GeomNode *dst = graph_get_node(graph, conn_undo[i].dst_id);
        if (dst && dst->type == GEOM_PORT && dst->data.port)
            dst->data.port->connected_to = conn_undo[i].old_connected_to;
    }
    while (graph->node_count > node_count0) {
        GeomNode *node = graph->nodes[node_count0];
        if (!node)
            break;
        graph_remove_node(graph, node->id);
    }
    lambda_apply_entries_destroy(entries, candidate_count);
    lv_free((void **) &conn_undo);
    lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "lambda_unify_apply_to_graph: 提交失败，已回滚，图保持不变");

prepare_fail:
    lambda_apply_entries_destroy(entries, candidate_count);
    lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "lambda_unify_apply_to_graph: 准备阶段失败，图保持不变");
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
 * @brief 柔性头应用信息（Miller 模式合一辅助）
 *
 * 柔性项：应用链的头是自由变量（元变量），且所有参数都是不同的
 * bound 变量（模式条件）。如 F x1 x2 ... xn。
 */
typedef struct {
    int head;                /* 柔性头自由变量的 De Bruijn 索引 */
    LvLambdaTerm *args[256]; /* 参数（bound 变量项，从左到右） */
    int arg_count;           /* 参数个数 */
} FlexAppInfo;

/**
 * @brief 收集应用链为柔性头应用（若满足模式条件）
 *
 * 拆解 App(App(...App(F, a1), a2)..., an) 应用链：
 * - 头必须是自由变量（index >= binder_count）
 * - 所有参数必须是 bound 变量（index < binder_count）且互不相同
 *
 * @param term         待检查项
 * @param binder_count 当前绑定层数
 * @param info         输出：柔性头信息（head/args 均为 term 的子节点，不拥有）
 * @return true 是合法的柔性头应用
 */
static bool collect_flex_app(LvLambdaTerm *term, int binder_count, FlexAppInfo *info) {
    if (!term || !info || term->type != LV_LAMBDA_APP)
        return false;

    LvLambdaTerm *args_buf[256];
    int count = 0;
    LvLambdaTerm *cur = term;
    while (cur && cur->type == LV_LAMBDA_APP) {
        if (count >= 256)
            return false;
        args_buf[count++] = cur->data.app.right;
        cur = cur->data.app.left;
    }

    if (!cur || cur->type != LV_LAMBDA_VAR)
        return false;
    if (cur->data.var.index < binder_count)
        return false; /* 头是 bound 变量 → 刚性项，非柔性 */

    /* 参数必须是 bound 变量且互不相同 */
    for (int i = 0; i < count; i++) {
        if (!args_buf[i] || args_buf[i]->type != LV_LAMBDA_VAR)
            return false;
        if (args_buf[i]->data.var.index >= binder_count)
            return false;
        for (int j = 0; j < i; j++) {
            if (args_buf[i]->data.var.index == args_buf[j]->data.var.index)
                return false;
        }
    }

    info->head = cur->data.var.index;
    info->arg_count = count;
    /* 应用链的 right 是从右到左收集的，反转得到从左到右的参数顺序 */
    for (int i = 0; i < count; i++) {
        info->args[i] = args_buf[count - 1 - i];
    }
    return true;
}

/**
 * @brief 用 n 层 λ-抽象包裹 body
 *
 * 构造 λx1.λx2.…λxn.body。包裹后 x_i（第 i+1 个参数，1-based）的
 * De Bruijn 索引为 n-1-i（x1 最外层 = n-1，xn 最内层 = 0）。
 */
static LvLambdaTerm *lambda_wrap(LvLambdaTerm *body, int n) {
    for (int i = 0; i < n; i++) {
        body = lv_lambda_create_abs(0, body);
    }
    return body;
}

/**
 * @brief 构造应用链：head 应用到 n 个"新 binder"参数
 *
 * 用于 Imitation 构造 F ↦ λx1..λxn.(head x1 x2 ... xn)。
 * head_index 必须已包含提升（处于 λx1..λxn 下时 head 为自由变量需 +n）；
 * 参数 x_i 使用新 binder 的索引 n-1-i（x1 = n-1，xn = 0）。
 */
static LvLambdaTerm *chain_new_binders(int head_index_lifted, int nargs) {
    LvLambdaTerm *result = lv_lambda_create_var(head_index_lifted);
    for (int k = 0; k < nargs; k++) {
        result = lv_lambda_create_app(result, lv_lambda_create_var(nargs - 1 - k));
    }
    return result;
}

/**
 * @brief 构造应用链：head 应用到"提升后的旧参数"
 *
 * 用于柔性-柔性绑定 F ↦ λx1..λxn.(G y1 y2 ... ym)。
 * head_index 已含提升；每个参数取其原 De Bruijn 索引 + lift。
 */
static LvLambdaTerm *chain_lifted_args(int head_index_lifted, const FlexAppInfo *info, int lift) {
    LvLambdaTerm *result = lv_lambda_create_var(head_index_lifted);
    for (int k = 0; k < info->arg_count; k++) {
        result = lv_lambda_create_app(result,
                                      lv_lambda_create_var(info->args[k]->data.var.index + lift));
    }
    return result;
}

/**
 * @brief 求 λ-项中最大的变量 De Bruijn 索引（用于分配 fresh 元变量）
 */
static int max_var_index_in_term(LvLambdaTerm *t, int cur) {
    if (!t)
        return cur;
    if (t->type == LV_LAMBDA_VAR) {
        return t->data.var.index > cur ? t->data.var.index : cur;
    }
    if (t->type == LV_LAMBDA_ABS) {
        return max_var_index_in_term(t->data.abs.body, cur);
    }
    if (t->type == LV_LAMBDA_APP) {
        int m = max_var_index_in_term(t->data.app.left, cur);
        return max_var_index_in_term(t->data.app.right, m);
    }
    return cur;
}

/**
 * @brief 计算 fresh 元变量起始索引（大于 t1/t2 与替换链中的所有索引）
 */
static int compute_fresh_start(LvLambdaTerm *t1, LvLambdaTerm *t2, LambdaSubstitution *subs) {
    int m = max_var_index_in_term(t1, -1);
    m = max_var_index_in_term(t2, m);
    for (LambdaSubstitution *s = subs; s; s = s->next) {
        m = max_var_index_in_term(s->replacement, m);
    }
    return m + 1;
}

/* 前向声明（互相递归） */
static LambdaUnifyStatus pattern_unify_rec(LvLambdaTerm *t1, LvLambdaTerm *t2,
                                            LambdaSubstitution **subs,
                                            int binder_count,
                                            int *fresh_counter,
                                            int depth, int max_depth);

/**
 * @brief 柔性头 vs 刚性项合一（Imitation / Projection 规则）
 *
 * 合一 F x1..xn 与刚性项 rigid（F 不在 rigid 中自由出现）：
 * - n == 0：直接绑定 F ↦ rigid（一阶情况）
 * - rigid 是 bound 变量且是参数之一：Projection，F ↦ λx1..λxn.x_{k+1}
 * - rigid 是自由常量 G：Imitation 常量，F ↦ λx1..λxn.G
 * - rigid 是抽象 λb.M：F ↦ λx1..λxn.λb.(F' x1..xn b)，递归合一 F' x1..xn b ≡ M'
 * - rigid 是应用 u v：F ↦ λx1..λxn.(F_u x1..xn)(F_v x1..xn)，递归合一两侧
 * - rigid 是柔性头 G y1..ym：F ↦ λx1..λxn.(G y1..ym)（参数提升）
 *
 * @param fv_idx        元变量索引（柔性头）
 * @param info          柔性头参数信息
 * @param rigid         刚性目标项
 * @param subs          替换链表
 * @param binder_count  当前绑定层数
 * @param fresh_counter 计数器：新元变量索引分配（调用方持有，单调递增）
 * @param depth, max_depth 递归深度控制
 */
static LambdaUnifyStatus solve_flex_rigid(int fv_idx, const FlexAppInfo *info,
                                          LvLambdaTerm *rigid,
                                          LambdaSubstitution **subs,
                                          int binder_count,
                                          int *fresh_counter,
                                          int depth, int max_depth) {
    if (depth >= max_depth) {
        lv_LOG_ERROR("lambda_unify", "模式合一（柔性-刚性）：最大递归深度 %d 超限", max_depth);
        return LAMBDA_UNIFY_ERROR;
    }
    if (!rigid)
        return LAMBDA_UNIFY_ERROR;

    int nargs = info->arg_count;

    /* Occurs check：F 不得出现在 rigid 中 */
    if (occurs_check_rec(fv_idx, rigid, *subs, binder_count)) {
        return LAMBDA_UNIFY_OCCURS_CHECK;
    }

    /* ── n == 0：裸元变量直接绑定（一阶情况） ── */
    if (nargs == 0) {
        LvLambdaTerm *copy = lv_lambda_copy(rigid);
        if (!copy)
            return LAMBDA_UNIFY_ERROR;
        if (!add_substitution_head(subs, fv_idx, copy)) {
            lv_lambda_destroy(copy);
            return LAMBDA_UNIFY_ERROR;
        }
        return LAMBDA_UNIFY_OK;
    }

    /* ── 柔性-柔性：F x1..xn ≡ G y1..ym ── */
    FlexAppInfo rigid_info;
    if (collect_flex_app(rigid, binder_count, &rigid_info)) {
        if (rigid_info.head == fv_idx) {
            /* 同一元变量：参数必须逐位一致 */
            if (rigid_info.arg_count != nargs)
                return LAMBDA_UNIFY_FAIL;
            for (int i = 0; i < nargs; i++) {
                if (rigid_info.args[i]->data.var.index != info->args[i]->data.var.index)
                    return LAMBDA_UNIFY_FAIL;
            }
            return LAMBDA_UNIFY_OK;
        }
        /* G 已有替换 → 展开 (r2 y1..ym) 后递归 */
        LvLambdaTerm *r2 = find_substitution(*subs, rigid_info.head);
        if (r2) {
            LvLambdaTerm *app_chain = lv_lambda_copy(r2);
            if (!app_chain)
                return LAMBDA_UNIFY_ERROR;
            for (int i = 0; i < rigid_info.arg_count; i++) {
                app_chain = lv_lambda_create_app(app_chain, lv_lambda_copy(rigid_info.args[i]));
                if (!app_chain)
                    return LAMBDA_UNIFY_ERROR;
            }
            return solve_flex_rigid(fv_idx, info, app_chain, subs, binder_count,
                                    fresh_counter, depth + 1, max_depth);
        }
        /* 直接绑定：F ↦ λx1..λxn.(G y1..ym)（G 与 y 均提升 n 层） */
        LvLambdaTerm *chain = chain_lifted_args(rigid_info.head + nargs, &rigid_info, nargs);
        if (!chain)
            return LAMBDA_UNIFY_ERROR;
        LvLambdaTerm *repl = lambda_wrap(chain, nargs);
        if (!repl)
            return LAMBDA_UNIFY_ERROR;
        if (!add_substitution_head(subs, fv_idx, repl)) {
            lv_lambda_destroy(repl);
            return LAMBDA_UNIFY_ERROR;
        }
        return LAMBDA_UNIFY_OK;
    }

    /* ── rigid 是 bound 变量：Projection ── */
    if (rigid->type == LV_LAMBDA_VAR && rigid->data.var.index < binder_count) {
        int k = -1;
        for (int i = 0; i < nargs; i++) {
            if (info->args[i]->data.var.index == rigid->data.var.index) {
                k = i;
                break;
            }
        }
        if (k < 0)
            return LAMBDA_UNIFY_FAIL; /* 目标不是参数之一 → 无解 */
        /* F ↦ λx1..λxn.x_{k+1}（x_{k+1} 的 De Bruijn 索引 = n-1-k） */
        LvLambdaTerm *body = lv_lambda_create_var(nargs - 1 - k);
        if (!body)
            return LAMBDA_UNIFY_ERROR;
        LvLambdaTerm *repl = lambda_wrap(body, nargs);
        if (!repl)
            return LAMBDA_UNIFY_ERROR;
        if (!add_substitution_head(subs, fv_idx, repl)) {
            lv_lambda_destroy(repl);
            return LAMBDA_UNIFY_ERROR;
        }
        return LAMBDA_UNIFY_OK;
    }

    /* ── rigid 是自由变量（常量）：Imitation 常量 ── */
    if (rigid->type == LV_LAMBDA_VAR) {
        /* F ↦ λx1..λxn.G（G 提升 n 层） */
        LvLambdaTerm *body = lv_lambda_create_var(rigid->data.var.index + nargs);
        if (!body)
            return LAMBDA_UNIFY_ERROR;
        LvLambdaTerm *repl = lambda_wrap(body, nargs);
        if (!repl)
            return LAMBDA_UNIFY_ERROR;
        if (!add_substitution_head(subs, fv_idx, repl)) {
            lv_lambda_destroy(repl);
            return LAMBDA_UNIFY_ERROR;
        }
        return LAMBDA_UNIFY_OK;
    }

    /* ── rigid 是抽象 λb.M：剥离一层抽象，递归合一 ── */
    if (rigid->type == LV_LAMBDA_ABS) {
        if (!rigid->data.abs.body)
            return LAMBDA_UNIFY_ERROR;
        int fp = (*fresh_counter)++;

        /* 递归合一 F' x1'..xn' b ≡ M'（binder_count+1 上下文）：
         * x_i' = x_i + 1（多一层 λb），b = Var(0)，M' = M 提升 nargs+1 层 */
        FlexAppInfo args_p1;
        args_p1.head = fp;
        args_p1.arg_count = nargs + 1;
        for (int i = 0; i < nargs; i++) {
            args_p1.args[i] = lv_lambda_create_var(info->args[i]->data.var.index + 1);
        }
        args_p1.args[nargs] = lv_lambda_create_var(0);

        LvLambdaTerm *lifted_body = lift_free_vars(rigid->data.abs.body, nargs + 1);
        if (!lifted_body) {
            for (int i = 0; i < nargs + 1; i++)
                lv_lambda_destroy(args_p1.args[i]);
            return LAMBDA_UNIFY_ERROR;
        }
        LambdaUnifyStatus s = solve_flex_rigid(fp, &args_p1, lifted_body, subs,
                                               binder_count + 1, fresh_counter,
                                               depth + 1, max_depth);
        for (int i = 0; i < nargs + 1; i++)
            lv_lambda_destroy(args_p1.args[i]);
        lv_lambda_destroy(lifted_body);
        if (s != LAMBDA_UNIFY_OK)
            return s;

        /* 构造绑定：F ↦ λx1..λxn.λb.(F' x1..xn b)
         * λx1..λxn.λb 下：F' 索引 = fp + nargs + 1，x_{k+1} = nargs - k，b = 0 */
        LvLambdaTerm *chain = lv_lambda_create_var(fp + nargs + 1);
        if (!chain)
            return LAMBDA_UNIFY_ERROR;
        for (int k = 0; k < nargs; k++) {
            chain = lv_lambda_create_app(chain, lv_lambda_create_var(nargs - k));
            if (!chain)
                return LAMBDA_UNIFY_ERROR;
        }
        chain = lv_lambda_create_app(chain, lv_lambda_create_var(0));
        if (!chain)
            return LAMBDA_UNIFY_ERROR;
        LvLambdaTerm *repl = lv_lambda_create_abs(0, chain); /* λb */
        if (!repl)
            return LAMBDA_UNIFY_ERROR;
        repl = lambda_wrap(repl, nargs); /* λx1..λxn.λb */
        if (!repl)
            return LAMBDA_UNIFY_ERROR;
        if (!add_substitution_head(subs, fv_idx, repl)) {
            lv_lambda_destroy(repl);
            return LAMBDA_UNIFY_ERROR;
        }
        return LAMBDA_UNIFY_OK;
    }

    /* ── rigid 是应用 u v：Imitation（对齐分解） ── */
    if (rigid->type == LV_LAMBDA_APP) {
        if (!rigid->data.app.left || !rigid->data.app.right)
            return LAMBDA_UNIFY_ERROR;
        int fu = (*fresh_counter)++;
        int fv2 = (*fresh_counter)++;

        /* 递归合一 F_u x1..xn ≡ u'、F_v x1..xn ≡ v'（u/v 提升 nargs 层） */
        FlexAppInfo args_n;
        args_n.head = fu;
        args_n.arg_count = nargs;
        for (int i = 0; i < nargs; i++)
            args_n.args[i] = info->args[i];

        LvLambdaTerm *lift_u = lift_free_vars(rigid->data.app.left, nargs);
        LvLambdaTerm *lift_v = lift_free_vars(rigid->data.app.right, nargs);
        if (!lift_u || !lift_v) {
            lv_lambda_destroy(lift_u);
            lv_lambda_destroy(lift_v);
            return LAMBDA_UNIFY_ERROR;
        }
        LambdaUnifyStatus s = solve_flex_rigid(fu, &args_n, lift_u, subs, binder_count,
                                               fresh_counter, depth + 1, max_depth);
        if (s != LAMBDA_UNIFY_OK) {
            lv_lambda_destroy(lift_u);
            lv_lambda_destroy(lift_v);
            return s;
        }
        args_n.head = fv2;
        s = solve_flex_rigid(fv2, &args_n, lift_v, subs, binder_count,
                             fresh_counter, depth + 1, max_depth);
        lv_lambda_destroy(lift_u);
        lv_lambda_destroy(lift_v);
        if (s != LAMBDA_UNIFY_OK)
            return s;

        /* 构造绑定：F ↦ λx1..λxn.(F_u x1..xn)(F_v x1..xn) */
        LvLambdaTerm *cu = chain_new_binders(fu + nargs, nargs);
        LvLambdaTerm *cv = chain_new_binders(fv2 + nargs, nargs);
        if (!cu || !cv) {
            lv_lambda_destroy(cu);
            lv_lambda_destroy(cv);
            return LAMBDA_UNIFY_ERROR;
        }
        LvLambdaTerm *body = lv_lambda_create_app(cu, cv);
        if (!body)
            return LAMBDA_UNIFY_ERROR;
        LvLambdaTerm *repl = lambda_wrap(body, nargs);
        if (!repl)
            return LAMBDA_UNIFY_ERROR;
        if (!add_substitution_head(subs, fv_idx, repl)) {
            lv_lambda_destroy(repl);
            return LAMBDA_UNIFY_ERROR;
        }
        return LAMBDA_UNIFY_OK;
    }

    return LAMBDA_UNIFY_FAIL;
}

/**
 * @brief 模式合一递归核心
 *
 * @param t1, t2    待合一项
 * @param binder_count  当前绑定的抽象层数
 * @param fresh_counter 新元变量索引计数器
 * @param depth     递归深度
 * @param max_depth 最大深度
 * @param subs      替换链表
 */
static LambdaUnifyStatus pattern_unify_rec(LvLambdaTerm *t1, LvLambdaTerm *t2,
                                            LambdaSubstitution **subs,
                                            int binder_count,
                                            int *fresh_counter,
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

    /* t1 是自由变量（高阶变量候选，裸元变量） */
    if (t1->type == LV_LAMBDA_VAR && t1->data.var.index >= binder_count) {
        int fv_idx = t1->data.var.index;

        /* 检查是否已有替换 */
        LvLambdaTerm *r1 = find_substitution(*subs, fv_idx);
        if (r1) {
            return pattern_unify_rec(r1, t2, subs, binder_count, fresh_counter, depth + 1, max_depth);
        }

        /* t2 也是自由变量 */
        if (t2->type == LV_LAMBDA_VAR && t2->data.var.index >= binder_count) {
            int fv2 = t2->data.var.index;
            LvLambdaTerm *r2 = find_substitution(*subs, fv2);
            if (r2) {
                return pattern_unify_rec(t1, r2, subs, binder_count, fresh_counter, depth + 1, max_depth);
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

        /* t2 是刚性项 → 直接绑定（一阶情况，occurs check 后） */
        if (!occurs_check_rec(fv_idx, t2, *subs, binder_count)) {
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
        return pattern_unify_rec(t2, t1, subs, binder_count, fresh_counter, depth + 1, max_depth);
    }

    /* t1 是柔性头应用（模式形式）：F x1..xn */
    if (t1->type == LV_LAMBDA_APP) {
        FlexAppInfo t1_flex;
        if (collect_flex_app(t1, binder_count, &t1_flex)) {
            int fv_idx = t1_flex.head;

            /* 柔性头已有替换 → 展开 (r1 x1..xn) 后递归 */
            LvLambdaTerm *r1 = find_substitution(*subs, fv_idx);
            if (r1) {
                LvLambdaTerm *app_chain = lv_lambda_copy(r1);
                if (!app_chain)
                    return LAMBDA_UNIFY_ERROR;
                for (int k = 0; k < t1_flex.arg_count; k++) {
                    app_chain = lv_lambda_create_app(app_chain,
                                                     lv_lambda_copy(t1_flex.args[k]));
                    if (!app_chain)
                        return LAMBDA_UNIFY_ERROR;
                }
                LambdaUnifyStatus s = pattern_unify_rec(app_chain, t2, subs, binder_count,
                                                        fresh_counter, depth + 1, max_depth);
                lv_lambda_destroy(app_chain);
                return s;
            }

            /* t2 也是柔性头应用 → 逐位对齐（交由 solve_flex_rigid 的柔性-柔性分支） */
            return solve_flex_rigid(fv_idx, &t1_flex, t2, subs, binder_count,
                                    fresh_counter, depth + 1, max_depth);
        }
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
                                 subs, binder_count + 1, fresh_counter, depth + 1, max_depth);
    }

    /* === 处理应用（非柔性头） === */
    if (t1->type == LV_LAMBDA_APP) {
        if (t2->type != LV_LAMBDA_APP) return LAMBDA_UNIFY_FAIL;

        LambdaUnifyStatus s = pattern_unify_rec(t1->data.app.left, t2->data.app.left,
                                                 subs, binder_count, fresh_counter, depth + 1, max_depth);
        if (s != LAMBDA_UNIFY_OK) return s;
        return pattern_unify_rec(t1->data.app.right, t2->data.app.right,
                                 subs, binder_count, fresh_counter, depth + 1, max_depth);
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

    /* fresh 元变量计数器：从所有已用索引之上开始 */
    int fresh_counter = compute_fresh_start(t1, t2, NULL);
    return pattern_unify_rec(t1, t2, out_subs, 0, &fresh_counter, 0, max_depth);
}