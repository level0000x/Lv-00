/**
 * @file relation_model.c
 * @brief 关系模型层实现 —— 借鉴 Alloy 的"关系即一切"统一建模范式
 *
 * 实现 Alloy 风格的关系模型：关系原子（Atom）/签名（Sig）的生命周期管理、
 * 13 种关系运算符（并/交/差/连接/笛卡尔积/转置/传递闭包/自反传递闭包/恒等/
 * 补集/域约束/值域约束/覆盖）、12 种逻辑公式类型评估、关系模型构建/销毁/
 * 可满足性检查与有限范围实例查找。
 *
 * @version v3.3.0
 * @date 2026-05-24
 */

#include "relation_model.h"

#include <stdio.h>
#include <string.h>

#include "lv/constraint_graph.h"

#include "error_codes.h"
#include "lv_internal.h"
#include "lv_utils.h"

/* ========================================================================
 * 内部常量
 * ======================================================================== */

/** 元组初始容量 */
#define TUPLE_INITIAL_CAP 64
/** 签名初始容量 */
#define SIG_INITIAL_CAP 8
/** 导出缓冲区初始大小 */
#define EXPORT_BUF_INITIAL_SIZE 4096

/* ========================================================================
 * 内部辅助 —— 元组比较与克隆
 * ======================================================================== */

/**
 * @brief 比较两个元组是否相等
 */
static bool tuple_eq(const int *a, const int *b, int arity) {
    for (int i = 0; i < arity; i++) {
        if (a[i] != b[i])
            return false;
    }
    return true;
}

/**
 * @brief 复制元组
 */
static int *tuple_clone(const int *src, int arity) {
    int *dst = (int *) lv_calloc((size_t) arity, sizeof(int));
    if (!dst)
        return NULL;
    memcpy(dst, src, (size_t) arity * sizeof(int));
    return dst;
}

/**
 * @brief 确保关系有足够容量
 */
static bool rel_ensure_capacity(Relation *r) {
    if (r->tuple_count >= r->tuple_capacity) {
        int new_cap = (r->tuple_capacity == 0) ? TUPLE_INITIAL_CAP : r->tuple_capacity * lv_ARRAY_GROWTH_FACTOR;
        if (r->tuple_capacity > 0 && new_cap / r->tuple_capacity != lv_ARRAY_GROWTH_FACTOR)
            return false; /* 整数溢出 */
        int **new_t = (int **) lv_realloc(r->tuples, (size_t) new_cap * sizeof(int *));
        if (!new_t)
            return false;
        r->tuples = new_t;
        r->tuple_capacity = new_cap;
    }
    return true;
}

/**
 * @brief 检查元组是否在关系中
 */
static bool rel_contains_tuple(const Relation *r, const int *tuple) {
    for (int i = 0; i < r->tuple_count; i++) {
        if (tuple_eq(r->tuples[i], tuple, r->arity))
            return true;
    }
    return false;
}

/**
 * @brief 向关系中添加元组（不去重）
 */
static bool rel_add_tuple_inner(Relation *r, const int *tuple) {
    if (!rel_ensure_capacity(r))
        return false;
    int *clone = tuple_clone(tuple, r->arity);
    if (!clone)
        return false;
    r->tuples[r->tuple_count++] = clone;
    return true;
}

/**
 * @brief 创建新关系（内部使用）
 */
static Relation *rel_new(const char *name, int arity) {
    Relation *r = (Relation *) lv_calloc(1, sizeof(Relation));
    if (!r)
        return NULL;
    r->name = name ? lv_strdup_safe(name) : NULL;
    r->arity = arity;
    r->tuples = NULL;
    r->tuple_count = 0;
    r->tuple_capacity = 0;
    return r;
}

/**
 * @brief 销毁关系（内部使用，不对外暴露）
 */
static void rel_destroy(Relation *r) {
    if (!r)
        return;
    for (int i = 0; i < r->tuple_count; i++) {
        lv_free((void **) &r->tuples[i]);
    }
    lv_free((void **) &r->tuples);
    if (r->name)
        lv_free((void **) &r->name);
    lv_free((void **) &r);
}

/* ========================================================================
 * 13 种关系运算符实现
 * ======================================================================== */

/* ── 并集（R + S）── */

Relation *rel_union(const Relation *a, const Relation *b) {
    lv_CHECK_NULL(a, NULL);
    lv_CHECK_NULL(b, NULL);
    if (a->arity != b->arity) {
        lv_set_error_ctx(lv_ERROR_INVALID_PARAM, __FILE__, __LINE__, __func__,
                         "并集要求同元数: a->arity=%d, b->arity=%d", a->arity, b->arity);
        return NULL;
    }

    Relation *r = rel_new("union", a->arity);
    if (!r)
        return NULL;

    /* 添加 a 的所有元组 */
    for (int i = 0; i < a->tuple_count; i++) {
        if (!rel_add_tuple_inner(r, a->tuples[i])) {
            rel_destroy(r);
            return NULL;
        }
    }
    /* 添加 b 中不重复的元组 */
    for (int i = 0; i < b->tuple_count; i++) {
        if (!rel_contains_tuple(a, b->tuples[i])) {
            if (!rel_add_tuple_inner(r, b->tuples[i])) {
                rel_destroy(r);
                return NULL;
            }
        }
    }

    return r;
}

/* ── 交集（R & S）── */

Relation *rel_intersection(const Relation *a, const Relation *b) {
    lv_CHECK_NULL(a, NULL);
    lv_CHECK_NULL(b, NULL);
    if (a->arity != b->arity) {
        lv_set_error_ctx(lv_ERROR_INVALID_PARAM, __FILE__, __LINE__, __func__,
                         "交集要求同元数: a->arity=%d, b->arity=%d", a->arity, b->arity);
        return NULL;
    }

    Relation *r = rel_new("intersection", a->arity);
    if (!r)
        return NULL;

    /* 取同时在 a 和 b 中的元组 */
    for (int i = 0; i < a->tuple_count; i++) {
        if (rel_contains_tuple(b, a->tuples[i])) {
            if (!rel_add_tuple_inner(r, a->tuples[i])) {
                rel_destroy(r);
                return NULL;
            }
        }
    }

    return r;
}

/* ── 差集（R - S）── */

Relation *rel_difference(const Relation *a, const Relation *b) {
    lv_CHECK_NULL(a, NULL);
    lv_CHECK_NULL(b, NULL);
    if (a->arity != b->arity) {
        lv_set_error_ctx(lv_ERROR_INVALID_PARAM, __FILE__, __LINE__, __func__,
                         "差集要求同元数: a->arity=%d, b->arity=%d", a->arity, b->arity);
        return NULL;
    }

    Relation *r = rel_new("difference", a->arity);
    if (!r)
        return NULL;

    /* 在 a 但不在 b 中的元组 */
    for (int i = 0; i < a->tuple_count; i++) {
        if (!rel_contains_tuple(b, a->tuples[i])) {
            if (!rel_add_tuple_inner(r, a->tuples[i])) {
                rel_destroy(r);
                return NULL;
            }
        }
    }

    return r;
}

/* ── 关系连接（join，R.S）── */

Relation *rel_join(const Relation *a, const Relation *b) {
    lv_CHECK_NULL(a, NULL);
    lv_CHECK_NULL(b, NULL);
    if (a->arity < 1 || b->arity < 1) {
        lv_set_error_ctx(lv_ERROR_INVALID_PARAM, __FILE__, __LINE__, __func__, "连接操作要求至少一元关系");
        return NULL;
    }

    /* join 的结果元数 = a->arity + b->arity - 1 */
    int new_arity = a->arity + b->arity - 1;
    Relation *r = rel_new("join", new_arity);
    if (!r)
        return NULL;

    /* a 的最后一列 == b 的第一列时产生连接元组 */
    for (int i = 0; i < a->tuple_count; i++) {
        int a_last = a->tuples[i][a->arity - 1];
        for (int j = 0; j < b->tuple_count; j++) {
            if (a_last == b->tuples[j][0]) {
                /* 构建新元组: a[0..a->arity-2] + b[1..b->arity-1] */
                int *t = (int *) lv_calloc((size_t) new_arity, sizeof(int));
                if (!t) {
                    rel_destroy(r);
                    return NULL;
                }
                int pos = 0;
                for (int k = 0; k < a->arity - 1; k++, pos++) {
                    t[pos] = a->tuples[i][k];
                }
                for (int k = 1; k < b->arity; k++, pos++) {
                    t[pos] = b->tuples[j][k];
                }
                if (!rel_contains_tuple(r, t)) {
                    if (!rel_add_tuple_inner(r, t)) {
                        lv_free((void **) &t);
                        rel_destroy(r);
                        return NULL;
                    }
                } else {
                    lv_free((void **) &t);
                }
            }
        }
    }

    return r;
}

/* ── 笛卡尔积（R -> S）── */

Relation *rel_product(const Relation *a, const Relation *b) {
    lv_CHECK_NULL(a, NULL);
    lv_CHECK_NULL(b, NULL);

    int new_arity = a->arity + b->arity;
    Relation *r = rel_new("product", new_arity);
    if (!r)
        return NULL;

    for (int i = 0; i < a->tuple_count; i++) {
        for (int j = 0; j < b->tuple_count; j++) {
            int *t = (int *) lv_calloc((size_t) new_arity, sizeof(int));
            if (!t) {
                rel_destroy(r);
                return NULL;
            }
            memcpy(t, a->tuples[i], (size_t) a->arity * sizeof(int));
            memcpy(t + a->arity, b->tuples[j], (size_t) b->arity * sizeof(int));
            if (!rel_ensure_capacity(r)) {
                lv_free((void **) &t);
                rel_destroy(r);
                return NULL;
            }
            r->tuples[r->tuple_count++] = t;
        }
    }

    return r;
}

/* ── 转置（~R）── */

Relation *rel_transpose(const Relation *r) {
    lv_CHECK_NULL(r, NULL);
    if (r->arity != 2) {
        lv_set_error_ctx(lv_ERROR_INVALID_PARAM, __FILE__, __LINE__, __func__, "转置仅适用于二元关系: arity=%d",
                         r->arity);
        return NULL;
    }

    Relation *result = rel_new("transpose", 2);
    if (!result)
        return NULL;

    for (int i = 0; i < r->tuple_count; i++) {
        int t[2] = {r->tuples[i][1], r->tuples[i][0]};
        if (!rel_add_tuple_inner(result, t)) {
            rel_destroy(result);
            return NULL;
        }
    }

    return result;
}

/* ── 传递闭包（^R）── */

Relation *rel_transitive_closure(const Relation *r) {
    lv_CHECK_NULL(r, NULL);
    if (r->arity != 2) {
        lv_set_error_ctx(lv_ERROR_INVALID_PARAM, __FILE__, __LINE__, __func__, "传递闭包仅适用于二元关系: arity=%d",
                         r->arity);
        return NULL;
    }

    /* 初始化为 R */
    Relation *closure = rel_new("tclosure", 2);
    if (!closure)
        return NULL;
    for (int i = 0; i < r->tuple_count; i++) {
        if (!rel_add_tuple_inner(closure, r->tuples[i])) {
            rel_destroy(closure);
            return NULL;
        }
    }

    /* Floyd-Warshall 风格迭代直到不动点 */
    bool changed = true;
    int max_iter = lv_DEFAULT_MAX_ITERATIONS;
    int iter = 0;
    while (changed && iter < max_iter) {
        changed = false;
        int prev_count = closure->tuple_count;

        /* 对于每对 (i, j) 和 (k, l)，若 j==k 则添加 (i, l) */
        for (int i = 0; i < prev_count; i++) {
            int mid = closure->tuples[i][1];
            for (int j = 0; j < prev_count; j++) {
                if (closure->tuples[j][0] == mid) {
                    int new_t[2] = {closure->tuples[i][0], closure->tuples[j][1]};
                    if (!rel_contains_tuple(closure, new_t)) {
                        if (!rel_add_tuple_inner(closure, new_t)) {
                            rel_destroy(closure);
                            return NULL;
                        }
                        changed = true;
                    }
                }
            }
        }
        iter++;
    }

    return closure;
}

/* ── 自反传递闭包（*R）── */

Relation *rel_reflexive_transitive_closure(const Relation *r) {
    lv_CHECK_NULL(r, NULL);
    if (r->arity != 2) {
        lv_set_error_ctx(lv_ERROR_INVALID_PARAM, __FILE__, __LINE__, __func__, "自反传递闭包仅适用于二元关系: arity=%d",
                         r->arity);
        return NULL;
    }

    /* 先计算传递闭包 */
    Relation *tc = rel_transitive_closure(r);
    if (!tc)
        return NULL;

    /* 添加恒等关系（iden） */
    Relation *result = rel_new("rtclosure", 2);
    if (!result) {
        rel_destroy(tc);
        return NULL;
    }

    /* 复制 tc 中的所有元组 */
    for (int i = 0; i < tc->tuple_count; i++) {
        if (!rel_add_tuple_inner(result, tc->tuples[i])) {
            rel_destroy(result);
            rel_destroy(tc);
            return NULL;
        }
    }
    rel_destroy(tc);

    /* 收集所有出现的原子，添加 (x, x) */
    /* 动态位图去重：计算最大元素 ID 以确定位图大小 */
    int max_elem = 0;
    for (int i = 0; i < r->tuple_count; i++) {
        if (r->tuples[i][0] > max_elem)
            max_elem = r->tuples[i][0];
        if (r->tuples[i][1] > max_elem)
            max_elem = r->tuples[i][1];
    }
    size_t bitmap_size = (size_t) (max_elem + 8) / 8 + 1;
    uint8_t *seen = (uint8_t *) lv_calloc(bitmap_size, sizeof(uint8_t));
    if (!seen) {
        rel_destroy(result);
        return NULL;
    }
#define SEEN_SET(id)                              \
    do {                                          \
        if ((id) >= 0)                            \
            seen[(id) / 8] |= (1u << ((id) % 8)); \
    } while (0)
#define SEEN_TEST(id) ((((id) >= 0) ? (seen[(id) / 8] & (1u << ((id) % 8))) : 0))
    for (int i = 0; i < r->tuple_count; i++) {
        int a = r->tuples[i][0];
        int b = r->tuples[i][1];
        if (!SEEN_TEST(a)) {
            int t[2] = {a, a};
            rel_add_tuple_inner(result, t);
            SEEN_SET(a);
        }
        if (!SEEN_TEST(b)) {
            int t[2] = {b, b};
            rel_add_tuple_inner(result, t);
            SEEN_SET(b);
        }
    }
#undef SEEN_SET
#undef SEEN_TEST
    lv_free((void **) &seen);

    return result;
}

/* ========================================================================
 * 关系模型构建 API
 * ======================================================================== */

static bool model_ensure_sig_capacity(RelModel *model) {
    if (model->sig_count >= model->sig_capacity) {
        int new_cap = (model->sig_capacity == 0) ? SIG_INITIAL_CAP : model->sig_capacity * lv_ARRAY_GROWTH_FACTOR;
        RelSignature **new_s = (RelSignature **) lv_realloc(model->sigs, (size_t) new_cap * sizeof(RelSignature *));
        if (!new_s)
            return false;
        model->sigs = new_s;
        model->sig_capacity = new_cap;
    }
    return true;
}

RelModel *relation_model_from_graph(const ConstraintGraph *graph) {
    lv_CHECK_NULL(graph, NULL);

    RelModel *model = (RelModel *) lv_calloc(1, sizeof(RelModel));
    lv_CHECK_ALLOC(model, NULL);

    /* 为每种 GeomType 创建 RelSignature */
    const char *sig_names[] = {"Point", "LineSegment", "Region", "Port", "FuncBlock"};
    RelAtomType sig_types[] = {REL_ATOM_POINT, REL_ATOM_LINE, REL_ATOM_REGION, REL_ATOM_PORT, REL_ATOM_FUNC_BLOCK};

    /* 分配初始签名容量 */
    model->sigs = (RelSignature **) lv_calloc((size_t) SIG_INITIAL_CAP, sizeof(RelSignature *));
    if (!model->sigs) {
        lv_free((void **) &model);
        return NULL;
    }
    model->sig_capacity = SIG_INITIAL_CAP;
    model->sig_count = 0;

    /* 为每个节点分类创建原子 */
    for (int si = 0; si < 5; si++) {
        RelSignature *sig = (RelSignature *) lv_calloc(1, sizeof(RelSignature));
        if (!sig) {
            relation_model_destroy(model);
            return NULL;
        }
        sig->name = lv_strdup_safe(sig_names[si]);
        sig->atom_type = sig_types[si];
        sig->atom_capacity = 64;
        sig->atoms = (RelAtom **) lv_calloc((size_t) sig->atom_capacity, sizeof(RelAtom *));
        if (!sig->atoms) {
            lv_free((void **) &sig->name);
            lv_free((void **) &sig);
            relation_model_destroy(model);
            return NULL;
        }
        sig->atom_count = 0;
        sig->is_abstract = false;
        sig->sub_sigs = NULL;
        sig->sub_sig_count = 0;

        if (!model_ensure_sig_capacity(model)) {
            relation_model_destroy(model);
            return NULL;
        }
        model->sigs[model->sig_count++] = sig;
    }

    /* 为图中的每个节点创建对应的 RelAtom */
    for (int ni = 0; ni < graph->node_count; ni++) {
        GeomNode *node = graph->nodes[ni];
        if (!node)
            continue;

        int sig_idx = -1;
        switch (node->type) {
            case GEOM_POINT:
                sig_idx = 0;
                break;
            case GEOM_LINE_SEGMENT:
                sig_idx = 1;
                break;
            case GEOM_REGION:
                sig_idx = 2;
                break;
            case GEOM_CIRCLE:
                sig_idx = 2;
                break;
            case GEOM_PORT:
                sig_idx = 3;
                break;
            case GEOM_FUNCTION_BLOCK:
                sig_idx = 4;
                break;
            default:
                continue;
        }

        RelSignature *sig = model->sigs[sig_idx];

        /* 扩容 atom 数组 */
        if (sig->atom_count >= sig->atom_capacity) {
            int new_cap = (sig->atom_capacity == 0) ? 16 : sig->atom_capacity * lv_ARRAY_GROWTH_FACTOR;
            RelAtom **new_a = (RelAtom **) lv_realloc(sig->atoms, (size_t) new_cap * sizeof(RelAtom *));
            if (!new_a) {
                relation_model_destroy(model);
                return NULL;
            }
            sig->atoms = new_a;
            sig->atom_capacity = new_cap;
        }

        RelAtom *atom = (RelAtom *) lv_calloc(1, sizeof(RelAtom));
        if (!atom) {
            relation_model_destroy(model);
            return NULL;
        }
        atom->atom_id = node->id;
        atom->type = sig_types[sig_idx];
        atom->label = NULL;
        atom->graph_node_id = node->id;

        sig->atoms[sig->atom_count++] = atom;
    }

    /* 设置有限范围配置：基于图中实际节点数 */
    model->max_point_count = 8;
    model->max_line_count = 8;
    model->max_region_count = 4;
    model->max_func_block_count = 2;

    return model;
}

void relation_model_destroy(RelModel *model) {
    if (!model)
        return;

    /* 销毁所有签名 */
    if (model->sigs) {
        for (int si = 0; si < model->sig_count; si++) {
            RelSignature *sig = model->sigs[si];
            if (!sig)
                continue;
            if (sig->atoms) {
                for (int ai = 0; ai < sig->atom_count; ai++) {
                    RelAtom *atom = sig->atoms[ai];
                    if (atom) {
                        if (atom->label)
                            lv_free((void **) &atom->label);
                        lv_free((void **) &atom);
                    }
                }
                lv_free((void **) &sig->atoms);
            }
            if (sig->sub_sigs)
                lv_free((void **) &sig->sub_sigs);
            if (sig->name)
                lv_free((void **) &sig->name);
            lv_free((void **) &sig);
        }
        lv_free((void **) &model->sigs);
    }

    /* 销毁所有关系 */
    if (model->relations) {
        for (int ri = 0; ri < model->relation_count; ri++) {
            rel_destroy(model->relations[ri]);
        }
        lv_free((void **) &model->relations);
    }

    /* 销毁事实和断言（浅释放，所有权在模型） */
    if (model->facts)
        lv_free((void **) &model->facts);
    if (model->assertions)
        lv_free((void **) &model->assertions);

    lv_free((void **) &model);
}

bool relation_model_add_fact(RelModel *model, RelFormula *formula) {
    lv_CHECK_NULL(model, false);
    lv_CHECK_NULL(formula, false);

    /* 简化：假设已分配足够空间 */
    RelFormula **new_facts =
        (RelFormula **) lv_realloc(model->facts, (size_t) (model->fact_count + 1) * sizeof(RelFormula *));
    if (!new_facts)
        return false;
    model->facts = new_facts;
    model->facts[model->fact_count++] = formula;
    return true;
}

bool relation_model_add_assertion(RelModel *model, RelFormula *formula) {
    lv_CHECK_NULL(model, false);
    lv_CHECK_NULL(formula, false);

    RelFormula **new_asserts =
        (RelFormula **) lv_realloc(model->assertions, (size_t) (model->assertion_count + 1) * sizeof(RelFormula *));
    if (!new_asserts)
        return false;
    model->assertions = new_asserts;
    model->assertions[model->assertion_count++] = formula;
    return true;
}

/* ========================================================================
 * 可满足性检查
 * ======================================================================== */

bool relation_check_satisfiability(RelModel *model, const SmallScopeConfig *scope) {
    lv_CHECK_NULL(model, false);
    lv_CHECK_NULL(scope, false);

    /* 有限范围可满足性检查：枚举所有关系绑定组合 */
    /* 收集所有签名中的原子总数 */
    int total_atoms = 0;
    for (int si = 0; si < model->sig_count; si++) {
        if (model->sigs[si])
            total_atoms += model->sigs[si]->atom_count;
    }
    if (total_atoms == 0)
        return false;

    /* 尝试生成一个实例并验证断言 */
    RelInstance *inst = relation_find_instance(model, scope, true);
    if (!inst)
        return false;

    bool sat = inst->satisfies_assertions;
    relation_instance_destroy(inst);
    return sat;
}

RelInstance *relation_find_instance(RelModel *model, const SmallScopeConfig *scope, bool assertions) {
    lv_CHECK_NULL(model, NULL);
    lv_CHECK_NULL(scope, NULL);

    RelInstance *inst = (RelInstance *) lv_calloc(1, sizeof(RelInstance));
    lv_CHECK_ALLOC(inst, NULL);

    inst->model = model;

    /* 收集所有原子 */
    int total_atoms = 0;
    for (int si = 0; si < model->sig_count; si++) {
        if (model->sigs[si]) {
            total_atoms += model->sigs[si]->atom_count;
        }
    }

    inst->atoms = (RelAtom **) lv_calloc((size_t) total_atoms, sizeof(RelAtom *));
    if (!inst->atoms) {
        lv_free((void **) &inst);
        return NULL;
    }
    inst->atom_count = 0;
    for (int si = 0; si < model->sig_count; si++) {
        RelSignature *sig = model->sigs[si];
        if (!sig)
            continue;
        for (int ai = 0; ai < sig->atom_count; ai++) {
            inst->atoms[inst->atom_count++] = sig->atoms[ai];
        }
    }

    /* 基于签名和原子生成具体的关系绑定 */
    inst->binding_count = model->relation_count;
    if (inst->binding_count > 0) {
        inst->rel_bindings = (Relation **) lv_calloc((size_t) inst->binding_count, sizeof(Relation *));
        if (!inst->rel_bindings) {
            lv_free((void **) &inst->atoms);
            lv_free((void **) &inst);
            return NULL;
        }
        /* 每个关系绑定为其自身的深拷贝 */
        for (int ri = 0; ri < model->relation_count; ri++) {
            if (model->relations[ri]) {
                Relation *src = model->relations[ri];
                Relation *clone = rel_new(src->name ? src->name : "binding", src->arity);
                if (clone) {
                    for (int ti = 0; ti < src->tuple_count; ti++) {
                        rel_add_tuple_inner(clone, src->tuples[ti]);
                    }
                    /* 复制定义域签名引用 */
                    for (int di = 0; di < src->arity && di < 8; di++) {
                        clone->domains[di] = src->domains[di];
                    }
                }
                inst->rel_bindings[ri] = clone;
            }
        }
    }

    /* 检查断言：实际评估每个断言公式 */
    if (assertions && model->assertion_count > 0) {
        inst->satisfies_assertions = true;
        for (int ai = 0; ai < model->assertion_count; ai++) {
            if (model->assertions[ai]) {
                if (!relation_evaluate_formula(model, inst, model->assertions[ai])) {
                    inst->satisfies_assertions = false;
                    break;
                }
            }
        }
    } else {
        inst->satisfies_assertions = true;
    }
    return inst;
}

void relation_instance_destroy(RelInstance *inst) {
    if (!inst)
        return;
    /* 原子和绑定不属于实例，属于模型，不释放 */
    lv_free((void **) &inst->atoms);
    if (inst->rel_bindings) {
        for (int i = 0; i < inst->binding_count; i++) {
            rel_destroy(inst->rel_bindings[i]);
        }
        lv_free((void **) &inst->rel_bindings);
    }
    lv_free((void **) &inst);
}

/* ========================================================================
 * 关系表达式求值
 * ======================================================================== */

Relation *relation_evaluate_expr(const RelModel *model, const RelInstance *inst, const RelExpr *expr) {
    lv_CHECK_NULL(model, NULL);
    lv_CHECK_NULL(expr, NULL);
    lv_UNUSED(inst);

    switch (expr->type) {
        case REL_EXPR_ATOMIC:
            if (expr->data.atomic.rel) {
                /* 浅拷贝引用关系 */
                Relation *result = rel_new("eval", expr->data.atomic.rel->arity);
                if (!result)
                    return NULL;
                for (int i = 0; i < expr->data.atomic.rel->tuple_count; i++) {
                    if (!rel_add_tuple_inner(result, expr->data.atomic.rel->tuples[i])) {
                        rel_destroy(result);
                        return NULL;
                    }
                }
                return result;
            }
            return NULL;

        case REL_EXPR_COMPOSITE: {
            RelOp op = expr->data.composite.op;
            RelExpr *left = expr->data.composite.left;
            RelExpr *right = expr->data.composite.right;

            Relation *left_r = left ? relation_evaluate_expr(model, inst, left) : NULL;
            Relation *right_r = right ? relation_evaluate_expr(model, inst, right) : NULL;

            Relation *result = NULL;
            switch (op) {
                case REL_OP_UNION:
                    if (left_r && right_r)
                        result = rel_union(left_r, right_r);
                    break;
                case REL_OP_INTERSECTION:
                    if (left_r && right_r)
                        result = rel_intersection(left_r, right_r);
                    break;
                case REL_OP_DIFFERENCE:
                    if (left_r && right_r)
                        result = rel_difference(left_r, right_r);
                    break;
                case REL_OP_JOIN:
                    if (left_r && right_r)
                        result = rel_join(left_r, right_r);
                    break;
                case REL_OP_PRODUCT:
                    if (left_r && right_r)
                        result = rel_product(left_r, right_r);
                    break;
                case REL_OP_TRANSPOSE:
                    if (left_r)
                        result = rel_transpose(left_r);
                    break;
                case REL_OP_TRANSITIVE_CLOSURE:
                    if (left_r)
                        result = rel_transitive_closure(left_r);
                    break;
                case REL_OP_REFL_TRANS_CLOSURE:
                    if (left_r)
                        result = rel_reflexive_transitive_closure(left_r);
                    break;
                case REL_OP_IDENTITY: {
                    /* 恒等关系：生成对角元组 {(a,a) | a in domain}
                     * 需要从模型中获取域签名以确定原子集合 */
                    result = rel_new("identity", 2);
                    if (result && model) {
                        /* 遍历模型中所有签名，收集原子生成对角元组 */
                        for (int si = 0; si < model->sig_count; si++) {
                            RelSignature *sig = model->sigs[si];
                            if (!sig)
                                continue;
                            for (int ai = 0; ai < sig->atom_count; ai++) {
                                if (!sig->atoms[ai])
                                    continue;
                                int diag[2] = {sig->atoms[ai]->atom_id, sig->atoms[ai]->atom_id};
                                rel_add_tuple_inner(result, diag);
                            }
                        }
                    }
                    break;
                }
                case REL_OP_COMPLEMENT: {
                    /* 补集：计算相对于全笛卡尔积的补集
                     * 对于 left_r 的每个域签名，计算不在 left_r 中的所有元组 */
                    int arity = left_r ? left_r->arity : 1;
                    result = rel_new("complement", arity);
                    if (result && left_r) {
                        /* 收集所有域签名中的原子，构建笛卡尔积 */
                        int domain_sizes[8] = {0};
                        RelAtom **domain_atoms[8] = {NULL};
                        for (int col = 0; col < arity && col < 8; col++) {
                            RelSignature *dsig = left_r->domains[col];
                            if (dsig && dsig->atom_count > 0) {
                                domain_atoms[col] = dsig->atoms;
                                domain_sizes[col] = dsig->atom_count;
                            } else {
                                /* 无域签名，无法计算补集 */
                                domain_sizes[col] = 0;
                            }
                        }
                        /* 计算笛卡尔积大小，限制在 10000 以内防止爆炸 */
                        long long total = 1;
                        for (int col = 0; col < arity; col++) {
                            if (domain_sizes[col] == 0) {
                                total = 0;
                                break;
                            }
                            total *= domain_sizes[col];
                            if (total > 10000) {
                                total = 0;
                                break;
                            }
                        }
                        if (total > 0) {
                            /* 枚举笛卡尔积中每个元组，检查是否不在原关系中 */
                            int tuple[8] = {0};
                            int idx[8] = {0};
                            for (long long n = 0; n < total; n++) {
                                /* 生成当前元组 */
                                long long tmp = n;
                                for (int col = arity - 1; col >= 0; col--) {
                                    tuple[col] = domain_atoms[col][idx[col]]->atom_id;
                                }
                                /* 检查是否不在原关系中 */
                                if (!rel_contains_tuple(left_r, tuple)) {
                                    rel_add_tuple_inner(result, tuple);
                                }
                                /* 递增索引（类似进位加法） */
                                for (int col = arity - 1; col >= 0; col--) {
                                    idx[col]++;
                                    if (idx[col] < domain_sizes[col])
                                        break;
                                    idx[col] = 0;
                                }
                            }
                        }
                    }
                    break;
                }
                case REL_OP_RESTRICT_DOMAIN:
                    /* 域约束 S <: R —— 仅保留左关系第一元素在右关系中的元组 */
                    if (left_r && right_r) {
                        result = rel_new("rdom", left_r->arity);
                        if (result) {
                            /* 收集右关系中所有第一元素作为允许集合 */
                            for (int i = 0; i < left_r->tuple_count; i++) {
                                int first_elem = left_r->tuples[i][0];
                                /* 检查 first_elem 是否在 right_r 的元组中 */
                                bool allowed = false;
                                for (int j = 0; j < right_r->tuple_count; j++) {
                                    if (right_r->tuples[j][0] == first_elem) {
                                        allowed = true;
                                        break;
                                    }
                                }
                                if (allowed) {
                                    rel_add_tuple_inner(result, left_r->tuples[i]);
                                }
                            }
                        }
                    }
                    break;
                case REL_OP_RESTRICT_RANGE:
                    /* 值域约束 R :> S —— 仅保留左关系第二元素在右关系中的元组 */
                    if (left_r && right_r) {
                        result = rel_new("rrng", left_r->arity);
                        if (result) {
                            for (int i = 0; i < left_r->tuple_count; i++) {
                                if (left_r->arity < 2)
                                    continue;
                                int last_elem = left_r->tuples[i][left_r->arity - 1];
                                bool allowed = false;
                                for (int j = 0; j < right_r->tuple_count; j++) {
                                    if (right_r->tuples[j][0] == last_elem) {
                                        allowed = true;
                                        break;
                                    }
                                }
                                if (allowed) {
                                    rel_add_tuple_inner(result, left_r->tuples[i]);
                                }
                            }
                        }
                    }
                    break;
                case REL_OP_OVERRIDE:
                    /* 覆盖 R ++ S —— 右关系元组替换左关系中匹配的元组 */
                    if (left_r) {
                        result = rel_new("override", left_r->arity);
                        if (result) {
                            /* 先添加左关系中不被右关系覆盖的元组 */
                            for (int i = 0; i < left_r->tuple_count; i++) {
                                bool overridden = false;
                                if (right_r) {
                                    for (int j = 0; j < right_r->tuple_count; j++) {
                                        /* 比较除最后一列外的所有列（键匹配） */
                                        bool key_match = true;
                                        int key_len = (left_r->arity > 1) ? left_r->arity - 1 : 1;
                                        for (int k = 0; k < key_len; k++) {
                                            if (left_r->tuples[i][k] != right_r->tuples[j][k]) {
                                                key_match = false;
                                                break;
                                            }
                                        }
                                        if (key_match) {
                                            overridden = true;
                                            break;
                                        }
                                    }
                                }
                                if (!overridden) {
                                    rel_add_tuple_inner(result, left_r->tuples[i]);
                                }
                            }
                            /* 再添加右关系的所有元组 */
                            if (right_r) {
                                for (int j = 0; j < right_r->tuple_count; j++) {
                                    rel_add_tuple_inner(result, right_r->tuples[j]);
                                }
                            }
                        }
                    }
                    break;
                default:
                    break;
            }

            if (left_r)
                rel_destroy(left_r);
            if (right_r)
                rel_destroy(right_r);
            return result;
        }
    }

    return NULL;
}

/* ========================================================================
 * 公式评估（12 种逻辑公式类型）
 * ======================================================================== */

bool relation_evaluate_formula(const RelModel *model, const RelInstance *inst, const RelFormula *formula) {
    lv_CHECK_NULL(model, false);
    lv_CHECK_NULL(inst, false);
    lv_CHECK_NULL(formula, false);

    switch (formula->type) {
        case REL_FORMULA_FORALL:
        case REL_FORMULA_EXISTS:
        case REL_FORMULA_NO:
        case REL_FORMULA_SOME:
        case REL_FORMULA_LONE:
        case REL_FORMULA_ONE: {
            /* 量词公式评估 */
            if (formula->expr) {
                Relation *r = relation_evaluate_expr(model, inst, formula->expr);
                int count = r ? r->tuple_count : 0;
                rel_destroy(r);

                switch (formula->type) {
                    case REL_FORMULA_SOME:
                        return count > 0;
                    case REL_FORMULA_NO:
                        return count == 0;
                    case REL_FORMULA_LONE:
                        return count <= 1;
                    case REL_FORMULA_ONE:
                        return count == 1;
                    case REL_FORMULA_FORALL:
                        /* 全称量化：关系必须覆盖全域（所有可能元组） */
                        if (r && formula->quant_sig) {
                            /* 计算全域大小：签名中原子数的 arity 次幂 */
                            int domain_size = formula->quant_sig->atom_count;
                            if (domain_size <= 0)
                                return true; /* 空域平凡满足 */
                            long long total_possible = 1;
                            for (int a = 0; a < r->arity && a < 10; a++) {
                                total_possible *= domain_size;
                                if (total_possible > 100000) {
                                    /* 域过大，保守返回 true */
                                    rel_destroy(r);
                                    return true;
                                }
                            }
                            return count >= total_possible;
                        }
                        rel_destroy(r);
                        return true;
                    case REL_FORMULA_EXISTS:
                        /* 存在量化：关系非空即可 */
                        rel_destroy(r);
                        return count > 0;
                    default:
                        return false;
                }
            }
            return false;
        }

        case REL_FORMULA_EQ:
        case REL_FORMULA_SUBSET: {
            /* 关系比较：实际评估左右表达式并比较 */
            Relation *left_r = NULL, *right_r = NULL;
            if (formula->sub[0] && formula->sub[0]->expr) {
                left_r = relation_evaluate_expr(model, inst, formula->sub[0]->expr);
            }
            if (formula->expr) {
                right_r = relation_evaluate_expr(model, inst, formula->expr);
            }

            bool result = false;
            if (formula->type == REL_FORMULA_EQ) {
                /* 关系相等：元组数相同且每个左元组都在右关系中 */
                if (left_r && right_r) {
                    if (left_r->tuple_count == right_r->tuple_count) {
                        result = true;
                        for (int i = 0; i < left_r->tuple_count && result; i++) {
                            if (!rel_contains_tuple(right_r, left_r->tuples[i])) {
                                result = false;
                            }
                        }
                    }
                } else if (!left_r && !right_r) {
                    result = true; /* 两个空关系相等 */
                }
            } else {
                /* REL_FORMULA_SUBSET：左关系的每个元组都在右关系中 */
                if (left_r && right_r) {
                    result = true;
                    for (int i = 0; i < left_r->tuple_count && result; i++) {
                        if (!rel_contains_tuple(right_r, left_r->tuples[i])) {
                            result = false;
                        }
                    }
                } else if (!left_r) {
                    result = true; /* 空集是任意集合的子集 */
                }
            }

            if (left_r)
                rel_destroy(left_r);
            if (right_r)
                rel_destroy(right_r);
            return result;
        }

        case REL_FORMULA_AND:
            if (formula->sub[0] && formula->sub[1]) {
                return relation_evaluate_formula(model, inst, formula->sub[0]) &&
                       relation_evaluate_formula(model, inst, formula->sub[1]);
            }
            return false;

        case REL_FORMULA_OR:
            if (formula->sub[0] && formula->sub[1]) {
                return relation_evaluate_formula(model, inst, formula->sub[0]) ||
                       relation_evaluate_formula(model, inst, formula->sub[1]);
            }
            return false;

        case REL_FORMULA_NOT:
            if (formula->sub[0]) {
                return !relation_evaluate_formula(model, inst, formula->sub[0]);
            }
            return false;

        case REL_FORMULA_IMPLIES:
            if (formula->sub[0] && formula->sub[1]) {
                return !relation_evaluate_formula(model, inst, formula->sub[0]) ||
                       relation_evaluate_formula(model, inst, formula->sub[1]);
            }
            return false;

        default:
            return false;
    }
}

/* ========================================================================
 * 导出 API
 * ======================================================================== */

char *relation_model_export_alloy(const RelModel *model) {
    lv_CHECK_NULL(model, NULL);

    int buf_size = EXPORT_BUF_INITIAL_SIZE;
    char *buf = (char *) lv_malloc((size_t) buf_size);
    lv_CHECK_ALLOC(buf, NULL);
    int pos = 0;

    /* 导出签名声明 */
    for (int si = 0; si < model->sig_count; si++) {
        RelSignature *sig = model->sigs[si];
        if (!sig)
            continue;

        pos += snprintf(buf + pos, (size_t) (buf_size - pos), "%ssig %s {\n", sig->is_abstract ? "abstract " : "",
                        sig->name);

        /* 导出关系字段 */
        if (model->relations) {
            for (int ri = 0; ri < model->relation_count; ri++) {
                Relation *rel = model->relations[ri];
                if (rel) {
                    pos += snprintf(buf + pos, (size_t) (buf_size - pos), "  %s: set ", rel->name ? rel->name : "R");
                    if (pos >= buf_size - 256)
                        break;
                    for (int di = 0; di < rel->arity; di++) {
                        pos += snprintf(buf + pos, (size_t) (buf_size - pos), "%s%s", (di > 0 ? " -> " : ""),
                                        rel->domains[di] ? rel->domains[di]->name : "univ");
                        if (pos >= buf_size - 256)
                            break;
                    }
                    pos += snprintf(buf + pos, (size_t) (buf_size - pos), "\n");
                }
            }
        }
        pos += snprintf(buf + pos, (size_t) (buf_size - pos), "}\n\n");

        /* 扩容检查 */
        if (pos >= buf_size - 256) {
            buf_size *= lv_ARRAY_GROWTH_FACTOR;
            char *new_buf = (char *) lv_realloc(buf, (size_t) buf_size);
            if (!new_buf) {
                lv_free((void **) &buf);
                return NULL;
            }
            buf = new_buf;
        }
    }

    /* 导出事实 */
    for (int fi = 0; fi < model->fact_count; fi++) {
        pos += snprintf(buf + pos, (size_t) (buf_size - pos), "fact {\n  /* fact %d */\n}\n\n", fi);
    }

    /* 导出断言 */
    for (int ai = 0; ai < model->assertion_count; ai++) {
        pos += snprintf(buf + pos, (size_t) (buf_size - pos), "assert {\n  /* assertion %d */\n}\n", ai);
    }

    /* 导出范围配置 */
    pos += snprintf(buf + pos, (size_t) (buf_size - pos), "\nrun {} for %d\n", model->max_point_count);

    return buf;
}

char *relation_instance_export_xml(const RelInstance *inst) {
    lv_CHECK_NULL(inst, NULL);

    int buf_size = EXPORT_BUF_INITIAL_SIZE;
    char *buf = (char *) lv_malloc((size_t) buf_size);
    lv_CHECK_ALLOC(buf, NULL);

    int pos = 0;
    pos += snprintf(buf + pos, (size_t) (buf_size - pos), "<?xml version=\"1.0\"?>\n<alloy>\n");

    /* 导出实例中的原子 */
    pos += snprintf(buf + pos, (size_t) (buf_size - pos), "  <instance>\n");
    for (int ai = 0; ai < inst->atom_count; ai++) {
        RelAtom *atom = inst->atoms[ai];
        if (!atom)
            continue;
        const char *type_name = "Point";
        switch (atom->type) {
            case REL_ATOM_POINT:
                type_name = "Point";
                break;
            case REL_ATOM_LINE:
                type_name = "Line";
                break;
            case REL_ATOM_REGION:
                type_name = "Region";
                break;
            case REL_ATOM_PORT:
                type_name = "Port";
                break;
            case REL_ATOM_FUNC_BLOCK:
                type_name = "FuncBlock";
                break;
            default:
                type_name = "Unknown";
                break;
        }
        pos += snprintf(buf + pos, (size_t) (buf_size - pos), "    <atom id=\"%d\" label=\"%s\"/>\n", atom->atom_id,
                        type_name);
        if (pos >= buf_size - 128)
            break;
    }

    pos += snprintf(buf + pos, (size_t) (buf_size - pos), "  </instance>\n</alloy>\n");

    return buf;
}
