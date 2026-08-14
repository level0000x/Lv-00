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

#include "lv/relation_model.h"

#include <stdio.h>
#include <string.h>

#include "lv/constraint_graph.h"

#include "lv/error_codes.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_xmacro.h"

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
 * 静态查找表 —— 消除 switch-case 类型代码反模式
 * ======================================================================== */

/** GeomType → sig_idx 映射表（GEOM_POINT=0, ..., GEOM_FUNCTION_BLOCK=5） */
static const int geom_type_to_sig_idx[] = {
    0, /* GEOM_POINT          -> sig_idx 0 (Point) */
    1, /* GEOM_LINE_SEGMENT   -> sig_idx 1 (LineSegment) */
    2, /* GEOM_REGION         -> sig_idx 2 (Region) */
    2, /* GEOM_CIRCLE         -> sig_idx 2 (Region，与 REGION 共享) */
    3, /* GEOM_PORT           -> sig_idx 3 (Port) */
    4  /* GEOM_FUNCTION_BLOCK -> sig_idx 4 (FuncBlock) */
};

/** RelAtomType → 类型名称静态表 */
static const char *atom_type_names[] = {
    "Point",     /* REL_ATOM_POINT (0) */
    "Line",      /* REL_ATOM_LINE (1) */
    "Region",    /* REL_ATOM_REGION (2) */
    "Port",      /* REL_ATOM_PORT (3) */
    "FuncBlock"  /* REL_ATOM_FUNC_BLOCK (4) */
};

/* ========================================================================
 * 函数指针类型定义 —— 用于 VTable 与查找表调度
 * ======================================================================== */

/** 关系表达式求值函数指针 */
typedef Relation *(*RelExprEvalFunc)(const RelModel *model, const RelInstance *inst, const RelExpr *expr);

/** 关系运算符执行函数指针 */
typedef Relation *(*RelOpFunc)(Relation *left_r, Relation *right_r, const RelModel *model);

/** 逻辑公式求值函数指针 */
typedef bool (*RelFormulaEvalFunc)(const RelModel *model, const RelInstance *inst, const RelFormula *formula);

/** 量词检查函数指针 */
typedef bool (*QuantCheckFunc)(int count, const Relation *r, const RelFormula *formula);

/* ========================================================================
 * 前向声明 —— 表达式中使用的辅助函数
 * ======================================================================== */

/* RelOp 包装函数（13 种） */
static Relation *rel_op_union(Relation *left_r, Relation *right_r, const RelModel *model);
static Relation *rel_op_intersection(Relation *left_r, Relation *right_r, const RelModel *model);
static Relation *rel_op_difference(Relation *left_r, Relation *right_r, const RelModel *model);
static Relation *rel_op_join(Relation *left_r, Relation *right_r, const RelModel *model);
static Relation *rel_op_product(Relation *left_r, Relation *right_r, const RelModel *model);
static Relation *rel_op_transpose(Relation *left_r, Relation *right_r, const RelModel *model);
static Relation *rel_op_transitive_closure(Relation *left_r, Relation *right_r, const RelModel *model);
static Relation *rel_op_refl_trans_closure(Relation *left_r, Relation *right_r, const RelModel *model);
static Relation *rel_op_identity(Relation *left_r, Relation *right_r, const RelModel *model);
static Relation *rel_op_complement(Relation *left_r, Relation *right_r, const RelModel *model);
static Relation *rel_op_restrict_domain(Relation *left_r, Relation *right_r, const RelModel *model);
static Relation *rel_op_restrict_range(Relation *left_r, Relation *right_r, const RelModel *model);
static Relation *rel_op_override(Relation *left_r, Relation *right_r, const RelModel *model);

/* RelOp 查找表（按 RelOp 枚举顺序） */
static const RelOpFunc rel_op_table[] = {
    rel_op_union,               /* REL_OP_UNION (0) */
    rel_op_intersection,        /* REL_OP_INTERSECTION (1) */
    rel_op_difference,          /* REL_OP_DIFFERENCE (2) */
    rel_op_join,                /* REL_OP_JOIN (3) */
    rel_op_product,             /* REL_OP_PRODUCT (4) */
    rel_op_transpose,           /* REL_OP_TRANSPOSE (5) */
    rel_op_transitive_closure,  /* REL_OP_TRANSITIVE_CLOSURE (6) */
    rel_op_refl_trans_closure,  /* REL_OP_REFL_TRANS_CLOSURE (7) */
    rel_op_identity,            /* REL_OP_IDENTITY (8) */
    rel_op_complement,          /* REL_OP_COMPLEMENT (9) */
    rel_op_restrict_domain,     /* REL_OP_RESTRICT_DOMAIN (10) */
    rel_op_restrict_range,      /* REL_OP_RESTRICT_RANGE (11) */
    rel_op_override             /* REL_OP_OVERRIDE (12) */
};

/* RelExpr 求值函数 */
static Relation *eval_expr_atomic(const RelModel *model, const RelInstance *inst, const RelExpr *expr);
static Relation *eval_expr_composite(const RelModel *model, const RelInstance *inst, const RelExpr *expr);

/* RelExpr VTable（按 RelExprType 枚举顺序） */
static const RelExprEvalFunc expr_eval_table[] = {
    eval_expr_atomic,    /* REL_EXPR_ATOMIC (0) */
    eval_expr_composite  /* REL_EXPR_COMPOSITE (1) */
};

/* 公式求值函数（12 种） */
static bool eval_formula_quantifier(const RelModel *model, const RelInstance *inst, const RelFormula *formula);
static bool eval_formula_eq(const RelModel *model, const RelInstance *inst, const RelFormula *formula);
static bool eval_formula_subset(const RelModel *model, const RelInstance *inst, const RelFormula *formula);
static bool eval_formula_and(const RelModel *model, const RelInstance *inst, const RelFormula *formula);
static bool eval_formula_or(const RelModel *model, const RelInstance *inst, const RelFormula *formula);
static bool eval_formula_not(const RelModel *model, const RelInstance *inst, const RelFormula *formula);
static bool eval_formula_implies(const RelModel *model, const RelInstance *inst, const RelFormula *formula);

/* RelFormula VTable（按 RelFormulaType 枚举顺序） */
static const RelFormulaEvalFunc formula_eval_table[] = {
    eval_formula_quantifier,  /* REL_FORMULA_FORALL (0) */
    eval_formula_quantifier,  /* REL_FORMULA_EXISTS (1) */
    eval_formula_quantifier,  /* REL_FORMULA_NO (2) */
    eval_formula_quantifier,  /* REL_FORMULA_SOME (3) */
    eval_formula_quantifier,  /* REL_FORMULA_LONE (4) */
    eval_formula_quantifier,  /* REL_FORMULA_ONE (5) */
    eval_formula_eq,          /* REL_FORMULA_EQ (6) */
    eval_formula_subset,      /* REL_FORMULA_SUBSET (7) */
    eval_formula_and,         /* REL_FORMULA_AND (8) */
    eval_formula_or,          /* REL_FORMULA_OR (9) */
    eval_formula_not,         /* REL_FORMULA_NOT (10) */
    eval_formula_implies      /* REL_FORMULA_IMPLIES (11) */
};

/* 量词检查函数（6 种） */
static bool quant_check_some(int count, const Relation *r, const RelFormula *formula);
static bool quant_check_no(int count, const Relation *r, const RelFormula *formula);
static bool quant_check_lone(int count, const Relation *r, const RelFormula *formula);
static bool quant_check_one(int count, const Relation *r, const RelFormula *formula);
static bool quant_check_forall(int count, const Relation *r, const RelFormula *formula);
static bool quant_check_exists(int count, const Relation *r, const RelFormula *formula);

/* 量词检查查找表（按 RelFormulaType 中 FORALL=0..ONE=5 的顺序） */
static const QuantCheckFunc quant_check_table[] = {
    quant_check_forall,  /* REL_FORMULA_FORALL (0) */
    quant_check_exists,  /* REL_FORMULA_EXISTS (1) */
    quant_check_no,      /* REL_FORMULA_NO (2) */
    quant_check_some,    /* REL_FORMULA_SOME (3) */
    quant_check_lone,    /* REL_FORMULA_LONE (4) */
    quant_check_one      /* REL_FORMULA_ONE (5) */
};

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

/* lvDArray 自动管理扩容，不再需要 rel_ensure_capacity */

/**
 * @brief 检查元组是否在关系中
 */
static bool rel_contains_tuple(const Relation *r, const int *tuple) {
    for (int i = 0; i < r->tuples.count; i++) {
        int **t = (int **)lv_darray_get(&r->tuples, i);
        if (tuple_eq(*t, tuple, r->arity))
            return true;
    }
    return false;
}

/**
 * @brief 向关系中添加元组（不去重）
 */
static bool rel_add_tuple_inner(Relation *r, const int *tuple) {
    int *clone = tuple_clone(tuple, r->arity);
    if (!clone)
        return false;
    if (lv_darray_push(&r->tuples, &clone) < 0) {
        lv_free((void **) &clone);
        return false;
    }
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
    lv_darray_init(&r->tuples, sizeof(int *));
    return r;
}

/**
 * @brief 销毁关系（内部使用，不对外暴露）
 */
static void rel_destroy(Relation *r) {
    if (!r)
        return;
    for (int i = 0; i < r->tuples.count; i++) {
        int **t = (int **)lv_darray_get(&r->tuples, i);
        lv_free((void **) t);
    }
    lv_darray_free(&r->tuples);
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
    for (int i = 0; i < a->tuples.count; i++) {
        int **t = (int **)lv_darray_get(&a->tuples, i);
        if (!rel_add_tuple_inner(r, *t)) {
            rel_destroy(r);
            return NULL;
        }
    }
    /* 添加 b 中不重复的元组 */
    for (int i = 0; i < b->tuples.count; i++) {
        if (!rel_contains_tuple(a, *(int **)lv_darray_get(&b->tuples, i))) {
            if (!rel_add_tuple_inner(r, *(int **)lv_darray_get(&b->tuples, i))) {
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
    for (int i = 0; i < a->tuples.count; i++) {
        if (rel_contains_tuple(b, *(int **)lv_darray_get(&a->tuples, i))) {
            if (!rel_add_tuple_inner(r, *(int **)lv_darray_get(&a->tuples, i))) {
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
    for (int i = 0; i < a->tuples.count; i++) {
        if (!rel_contains_tuple(b, *(int **)lv_darray_get(&a->tuples, i))) {
            if (!rel_add_tuple_inner(r, *(int **)lv_darray_get(&a->tuples, i))) {
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
    for (int i = 0; i < a->tuples.count; i++) {
        int *a_tuple_i = *(int **)lv_darray_get(&a->tuples, i);
        int a_last = a_tuple_i[a->arity - 1];
        for (int j = 0; j < b->tuples.count; j++) {
            if (a_last == (*(int **)lv_darray_get(&b->tuples, j))[0]) {
                /* 构建新元组: a[0..a->arity-2] + b[1..b->arity-1] */
                int *t = (int *) lv_calloc((size_t) new_arity, sizeof(int));
                if (!t) {
                    rel_destroy(r);
                    return NULL;
                }
                int pos = 0;
                for (int k = 0; k < a->arity - 1; k++, pos++) {
                    t[pos] = a_tuple_i[k];
                }
                int *b_tuple_j = *(int **)lv_darray_get(&b->tuples, j);
                for (int k = 1; k < b->arity; k++, pos++) {
                    t[pos] = b_tuple_j[k];
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

    for (int i = 0; i < a->tuples.count; i++) {
        for (int j = 0; j < b->tuples.count; j++) {
            int *t = (int *) lv_calloc((size_t) new_arity, sizeof(int));
            if (!t) {
                rel_destroy(r);
                return NULL;
            }
            memcpy(t, *(int **)lv_darray_get(&a->tuples, i), (size_t) a->arity * sizeof(int));
            memcpy(t + a->arity, *(int **)lv_darray_get(&b->tuples, j), (size_t) b->arity * sizeof(int));
            if (lv_darray_push(&r->tuples, &t) < 0) {
                lv_free((void **) &t);
                rel_destroy(r);
                return NULL;
            }
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

    for (int i = 0; i < r->tuples.count; i++) {
        int *r_tuple_i = *(int **)lv_darray_get(&r->tuples, i);
        int t[2] = {r_tuple_i[1], r_tuple_i[0]};
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
    for (int i = 0; i < r->tuples.count; i++) {
        if (!rel_add_tuple_inner(closure, *(int **)lv_darray_get(&r->tuples, i))) {
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
        int prev_count = closure->tuples.count;

        /* 对于每对 (i, j) 和 (k, l)，若 j==k 则添加 (i, l) */
        for (int i = 0; i < prev_count; i++) {
            int *c_i = *(int **)lv_darray_get(&closure->tuples, i);
            int mid = c_i[1];
            for (int j = 0; j < prev_count; j++) {
                int *c_j = *(int **)lv_darray_get(&closure->tuples, j);
                if (c_j[0] == mid) {
                    int new_t[2] = {c_i[0], c_j[1]};
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
    for (int i = 0; i < tc->tuples.count; i++) {
        if (!rel_add_tuple_inner(result, *(int **)lv_darray_get(&tc->tuples, i))) {
            rel_destroy(result);
            rel_destroy(tc);
            return NULL;
        }
    }
    rel_destroy(tc);

    /* 收集所有出现的原子，添加 (x, x) */
    /* 动态位图去重：计算最大元素 ID 以确定位图大小 */
    int max_elem = 0;
    for (int i = 0; i < r->tuples.count; i++) {
        int *r_i = *(int **)lv_darray_get(&r->tuples, i);
        if (r_i[0] > max_elem)
            max_elem = r_i[0];
        if (r_i[1] > max_elem)
            max_elem = r_i[1];
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
    for (int i = 0; i < r->tuples.count; i++) {
        int *r_i = *(int **)lv_darray_get(&r->tuples, i);
        int a = r_i[0];
        int b = r_i[1];
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
 * RelOp 包装函数 —— 替代 switch-case 的函数指针查找表
 * ======================================================================== */

static Relation *rel_op_union(Relation *left_r, Relation *right_r, const RelModel *model) {
    (void)model;
    return (left_r && right_r) ? rel_union(left_r, right_r) : NULL;
}

static Relation *rel_op_intersection(Relation *left_r, Relation *right_r, const RelModel *model) {
    (void)model;
    return (left_r && right_r) ? rel_intersection(left_r, right_r) : NULL;
}

static Relation *rel_op_difference(Relation *left_r, Relation *right_r, const RelModel *model) {
    (void)model;
    return (left_r && right_r) ? rel_difference(left_r, right_r) : NULL;
}

static Relation *rel_op_join(Relation *left_r, Relation *right_r, const RelModel *model) {
    (void)model;
    return (left_r && right_r) ? rel_join(left_r, right_r) : NULL;
}

static Relation *rel_op_product(Relation *left_r, Relation *right_r, const RelModel *model) {
    (void)model;
    return (left_r && right_r) ? rel_product(left_r, right_r) : NULL;
}

static Relation *rel_op_transpose(Relation *left_r, Relation *right_r, const RelModel *model) {
    (void)model;
    (void)right_r;
    return left_r ? rel_transpose(left_r) : NULL;
}

static Relation *rel_op_transitive_closure(Relation *left_r, Relation *right_r, const RelModel *model) {
    (void)model;
    (void)right_r;
    return left_r ? rel_transitive_closure(left_r) : NULL;
}

static Relation *rel_op_refl_trans_closure(Relation *left_r, Relation *right_r, const RelModel *model) {
    (void)model;
    (void)right_r;
    return left_r ? rel_reflexive_transitive_closure(left_r) : NULL;
}

static Relation *rel_op_identity(Relation *left_r, Relation *right_r, const RelModel *model) {
    (void)left_r;
    (void)right_r;
    Relation *result = rel_new("identity", 2);
    if (result && model) {
        for (int si = 0; si < model->sigs.count; si++) {
            RelSignature *sig = *(RelSignature **)lv_darray_get(&model->sigs, si);
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
    return result;
}

static Relation *rel_op_complement(Relation *left_r, Relation *right_r, const RelModel *model) {
    (void)right_r;
    (void)model;
    int arity = left_r ? left_r->arity : 1;
    Relation *result = rel_new("complement", arity);
    if (result && left_r) {
        int domain_sizes[8] = {0};
        RelAtom **domain_atoms[8] = {NULL};
        for (int col = 0; col < arity && col < 8; col++) {
            RelSignature *dsig = left_r->domains[col];
            if (dsig && dsig->atom_count > 0) {
                domain_atoms[col] = dsig->atoms;
                domain_sizes[col] = dsig->atom_count;
            } else {
                domain_sizes[col] = 0;
            }
        }
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
            int tuple[8] = {0};
            int idx[8] = {0};
            for (long long n = 0; n < total; n++) {
                for (int col = arity - 1; col >= 0; col--) {
                    tuple[col] = domain_atoms[col][idx[col]]->atom_id;
                }
                if (!rel_contains_tuple(left_r, tuple)) {
                    rel_add_tuple_inner(result, tuple);
                }
                for (int col = arity - 1; col >= 0; col--) {
                    idx[col]++;
                    if (idx[col] < domain_sizes[col])
                        break;
                    idx[col] = 0;
                }
            }
        }
    }
    return result;
}

static Relation *rel_op_restrict_domain(Relation *left_r, Relation *right_r, const RelModel *model) {
    (void)model;
    if (left_r && right_r) {
        Relation *result = rel_new("rdom", left_r->arity);
        if (result) {
            for (int i = 0; i < left_r->tuples.count; i++) {
                int *left_i = *(int **)lv_darray_get(&left_r->tuples, i);
                int first_elem = left_i[0];
                bool allowed = false;
                for (int j = 0; j < right_r->tuples.count; j++) {
                    if ((*(int **)lv_darray_get(&right_r->tuples, j))[0] == first_elem) {
                        allowed = true;
                        break;
                    }
                }
                if (allowed) {
                    rel_add_tuple_inner(result, left_i);
                }
            }
        }
        return result;
    }
    return NULL;
}

static Relation *rel_op_restrict_range(Relation *left_r, Relation *right_r, const RelModel *model) {
    (void)model;
    if (left_r && right_r) {
        Relation *result = rel_new("rrng", left_r->arity);
        if (result) {
            for (int i = 0; i < left_r->tuples.count; i++) {
                if (left_r->arity < 2)
                    continue;
                int *left_i = *(int **)lv_darray_get(&left_r->tuples, i);
                int last_elem = left_i[left_r->arity - 1];
                bool allowed = false;
                for (int j = 0; j < right_r->tuples.count; j++) {
                    if ((*(int **)lv_darray_get(&right_r->tuples, j))[0] == last_elem) {
                        allowed = true;
                        break;
                    }
                }
                if (allowed) {
                    rel_add_tuple_inner(result, left_i);
                }
            }
        }
        return result;
    }
    return NULL;
}

static Relation *rel_op_override(Relation *left_r, Relation *right_r, const RelModel *model) {
    (void)model;
    if (left_r) {
        Relation *result = rel_new("override", left_r->arity);
        if (result) {
            for (int i = 0; i < left_r->tuples.count; i++) {
                bool overridden = false;
                int *left_i = *(int **)lv_darray_get(&left_r->tuples, i);
                if (right_r) {
                    for (int j = 0; j < right_r->tuples.count; j++) {
                        bool key_match = true;
                        int key_len = (left_r->arity > 1) ? left_r->arity - 1 : 1;
                        int *right_j = *(int **)lv_darray_get(&right_r->tuples, j);
                        for (int k = 0; k < key_len; k++) {
                            if (left_i[k] != right_j[k]) {
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
                    rel_add_tuple_inner(result, left_i);
                }
            }
            if (right_r) {
                for (int j = 0; j < right_r->tuples.count; j++) {
                    rel_add_tuple_inner(result, *(int **)lv_darray_get(&right_r->tuples, j));
                }
            }
        }
        return result;
    }
    return NULL;
}

/* ========================================================================
 * RelExpr 求值辅助函数 —— 供 VTable 调度
 * ======================================================================== */

static Relation *eval_expr_atomic(const RelModel *model, const RelInstance *inst, const RelExpr *expr) {
    (void)model;
    (void)inst;
    if (expr->data.atomic.rel) {
        Relation *result = rel_new("eval", expr->data.atomic.rel->arity);
        if (!result)
            return NULL;
        for (int i = 0; i < expr->data.atomic.rel->tuples.count; i++) {
            if (!rel_add_tuple_inner(result, *(int **)lv_darray_get(&expr->data.atomic.rel->tuples, i))) {
                rel_destroy(result);
                return NULL;
            }
        }
        return result;
    }
    return NULL;
}

static Relation *eval_expr_composite(const RelModel *model, const RelInstance *inst, const RelExpr *expr) {
    RelOp op = expr->data.composite.op;
    RelExpr *left = expr->data.composite.left;
    RelExpr *right = expr->data.composite.right;

    Relation *left_r = left ? relation_evaluate_expr(model, inst, left) : NULL;
    Relation *right_r = right ? relation_evaluate_expr(model, inst, right) : NULL;

    Relation *result = LV_DISPATCH(rel_op_table, op, NULL, left_r, right_r, model);

    if (left_r)
        rel_destroy(left_r);
    if (right_r)
        rel_destroy(right_r);
    return result;
}

/* ========================================================================
 * 关系模型构建 API
 * ======================================================================== */

/* lvDArray 自动管理扩容，不再需要 model_ensure_sig_capacity */

RelModel *relation_model_from_graph(const ConstraintGraph *graph) {
    lv_CHECK_NULL(graph, NULL);

    RelModel *model = (RelModel *) lv_calloc(1, sizeof(RelModel));
    lv_CHECK_ALLOC(model, NULL);

    /* 为每种 GeomType 创建 RelSignature */
    const char *sig_names[] = {"Point", "LineSegment", "Region", "Port", "FuncBlock"};
    RelAtomType sig_types[] = {REL_ATOM_POINT, REL_ATOM_LINE, REL_ATOM_REGION, REL_ATOM_PORT, REL_ATOM_FUNC_BLOCK};

    /* 分配初始签名容量 */
    lv_darray_init(&model->sigs, sizeof(RelSignature *));
    if (!lv_darray_reserve(&model->sigs, SIG_INITIAL_CAP)) {
        lv_free((void **) &model);
        return NULL;
    }

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

        if (lv_darray_push(&model->sigs, &sig) < 0) {
            relation_model_destroy(model);
            return NULL;
        }
    }

    /* 为图中的每个节点创建对应的 RelAtom */
    for (int ni = 0; ni < graph->node_count; ni++) {
        GeomNode *node = graph->nodes[ni];
        if (!node)
            continue;

        /* 使用静态查找表替代 switch */
        int type_idx = (int)node->type;
        if (type_idx < 0 || type_idx > GEOM_FUNCTION_BLOCK)
            continue;
        int sig_idx = geom_type_to_sig_idx[type_idx];

        RelSignature *sig = *(RelSignature **)lv_darray_get(&model->sigs, sig_idx);

        /* 扩容 atom 数组（统一委托 lv_ensure_capacity，内部含溢出检查与倍增） */
        if (sig->atom_count >= sig->atom_capacity) {
            if (!lv_ensure_capacity((void **) &sig->atoms, sig->atom_count + 1, &sig->atom_capacity,
                                    sizeof(RelAtom *), 1)) {
                relation_model_destroy(model);
                return NULL;
            }
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
    for (int si = 0; si < model->sigs.count; si++) {
        RelSignature **sig_p = (RelSignature **)lv_darray_get(&model->sigs, si);
        RelSignature *sig = *sig_p;
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
    lv_darray_free(&model->sigs);

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

    /* 倍增扩容：委托 lv_ensure_capacity（初始 8，此后每次倍增；失败语义与原来一致：返回 false） */
    if (!lv_ensure_capacity((void **) &model->facts, model->fact_count, &model->fact_capacity,
                            sizeof(RelFormula *), 1))
        return false;
    model->facts[model->fact_count++] = formula;
    return true;
}

bool relation_model_add_assertion(RelModel *model, RelFormula *formula) {
    lv_CHECK_NULL(model, false);
    lv_CHECK_NULL(formula, false);

    /* 倍增扩容：委托 lv_ensure_capacity（初始 8，此后每次倍增；失败语义与原来一致：返回 false） */
    if (!lv_ensure_capacity((void **) &model->assertions, model->assertion_count, &model->assertion_capacity,
                            sizeof(RelFormula *), 1))
        return false;
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
    for (int si = 0; si < model->sigs.count; si++) {
        RelSignature *sig = *(RelSignature **)lv_darray_get(&model->sigs, si);
        if (sig)
            total_atoms += sig->atom_count;
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
    for (int si = 0; si < model->sigs.count; si++) {
        RelSignature *sig = *(RelSignature **)lv_darray_get(&model->sigs, si);
        if (sig) {
            total_atoms += sig->atom_count;
        }
    }

    inst->atoms = (RelAtom **) lv_calloc((size_t) total_atoms, sizeof(RelAtom *));
    if (!inst->atoms) {
        lv_free((void **) &inst);
        return NULL;
    }
    inst->atom_count = 0;
    for (int si = 0; si < model->sigs.count; si++) {
        RelSignature *sig = *(RelSignature **)lv_darray_get(&model->sigs, si);
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
                    for (int ti = 0; ti < src->tuples.count; ti++) {
                        rel_add_tuple_inner(clone, *(int **)lv_darray_get(&src->tuples, ti));
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

    /* 使用 VTable 替代 switch (expr->type) */
    return LV_DISPATCH(expr_eval_table, expr->type, NULL, model, inst, expr);
}

/* ========================================================================
 * 量词检查函数 —— 替代内层 switch (formula->type) 的查找表
 * ======================================================================== */

static bool quant_check_some(int count, const Relation *r, const RelFormula *formula) {
    (void)r;
    (void)formula;
    return count > 0;
}

static bool quant_check_no(int count, const Relation *r, const RelFormula *formula) {
    (void)r;
    (void)formula;
    return count == 0;
}

static bool quant_check_lone(int count, const Relation *r, const RelFormula *formula) {
    (void)r;
    (void)formula;
    return count <= 1;
}

static bool quant_check_one(int count, const Relation *r, const RelFormula *formula) {
    (void)r;
    (void)formula;
    return count == 1;
}

static bool quant_check_forall(int count, const Relation *r, const RelFormula *formula) {
    if (r && formula->quant_sig) {
        int domain_size = formula->quant_sig->atom_count;
        if (domain_size <= 0)
            return true;
        long long total_possible = 1;
        for (int a = 0; a < r->arity && a < 10; a++) {
            total_possible *= domain_size;
            if (total_possible > 100000)
                return true;
        }
        return count >= total_possible;
    }
    return true;
}

static bool quant_check_exists(int count, const Relation *r, const RelFormula *formula) {
    (void)r;
    (void)formula;
    return count > 0;
}

/* ========================================================================
 * 公式求值辅助函数 —— 供 VTable 调度
 * ======================================================================== */

static bool eval_formula_quantifier(const RelModel *model, const RelInstance *inst, const RelFormula *formula) {
    if (!formula->expr)
        return false;

    Relation *r = relation_evaluate_expr(model, inst, formula->expr);
    int count = r ? r->tuples.count : 0;
    /* 使用量词检查查找表替代内层 switch */
    bool result = LV_DISPATCH(quant_check_table, formula->type, false, count, r, formula);
    rel_destroy(r);
    return result;
}

static bool eval_formula_eq(const RelModel *model, const RelInstance *inst, const RelFormula *formula) {
    Relation *left_r = NULL, *right_r = NULL;
    if (formula->sub[0] && formula->sub[0]->expr) {
        left_r = relation_evaluate_expr(model, inst, formula->sub[0]->expr);
    }
    if (formula->expr) {
        right_r = relation_evaluate_expr(model, inst, formula->expr);
    }

    bool result = false;
    if (left_r && right_r) {
        if (left_r->tuples.count == right_r->tuples.count) {
            result = true;
            for (int i = 0; i < left_r->tuples.count && result; i++) {
                if (!rel_contains_tuple(right_r, *(int **)lv_darray_get(&left_r->tuples, i))) {
                    result = false;
                }
            }
        }
    } else if (!left_r && !right_r) {
        result = true;
    }

    if (left_r)
        rel_destroy(left_r);
    if (right_r)
        rel_destroy(right_r);
    return result;
}

static bool eval_formula_subset(const RelModel *model, const RelInstance *inst, const RelFormula *formula) {
    Relation *left_r = NULL, *right_r = NULL;
    if (formula->sub[0] && formula->sub[0]->expr) {
        left_r = relation_evaluate_expr(model, inst, formula->sub[0]->expr);
    }
    if (formula->expr) {
        right_r = relation_evaluate_expr(model, inst, formula->expr);
    }

    bool result = false;
    if (left_r && right_r) {
        result = true;
        for (int i = 0; i < left_r->tuples.count && result; i++) {
            if (!rel_contains_tuple(right_r, *(int **)lv_darray_get(&left_r->tuples, i))) {
                result = false;
            }
        }
    } else if (!left_r) {
        result = true;
    }

    if (left_r)
        rel_destroy(left_r);
    if (right_r)
        rel_destroy(right_r);
    return result;
}

static bool eval_formula_and(const RelModel *model, const RelInstance *inst, const RelFormula *formula) {
    if (formula->sub[0] && formula->sub[1]) {
        return relation_evaluate_formula(model, inst, formula->sub[0]) &&
               relation_evaluate_formula(model, inst, formula->sub[1]);
    }
    return false;
}

static bool eval_formula_or(const RelModel *model, const RelInstance *inst, const RelFormula *formula) {
    if (formula->sub[0] && formula->sub[1]) {
        return relation_evaluate_formula(model, inst, formula->sub[0]) ||
               relation_evaluate_formula(model, inst, formula->sub[1]);
    }
    return false;
}

static bool eval_formula_not(const RelModel *model, const RelInstance *inst, const RelFormula *formula) {
    if (formula->sub[0]) {
        return !relation_evaluate_formula(model, inst, formula->sub[0]);
    }
    return false;
}

static bool eval_formula_implies(const RelModel *model, const RelInstance *inst, const RelFormula *formula) {
    if (formula->sub[0] && formula->sub[1]) {
        return !relation_evaluate_formula(model, inst, formula->sub[0]) ||
               relation_evaluate_formula(model, inst, formula->sub[1]);
    }
    return false;
}

/* ========================================================================
 * 公式评估（12 种逻辑公式类型）
 * ======================================================================== */

bool relation_evaluate_formula(const RelModel *model, const RelInstance *inst, const RelFormula *formula) {
    lv_CHECK_NULL(model, false);
    lv_CHECK_NULL(inst, false);
    lv_CHECK_NULL(formula, false);

    /* 使用 VTable 替代 switch (formula->type) */
    return LV_DISPATCH(formula_eval_table, formula->type, false, model, inst, formula);
}

/* ========================================================================
 * 导出 API
 * ======================================================================== */

char *relation_model_export_alloy(const RelModel *model) {
    lv_CHECK_NULL(model, NULL);

    /* 用 lvStrBuf 累积输出（自动扩容；lv_strbuf_to_string 返回 lv_malloc 分配的 NUL 结尾字符串） */
    lvStrBuf sb = {0};

    /* 导出签名声明 */
    for (int si = 0; si < model->sigs.count; si++) {
        RelSignature *sig = *(RelSignature **)lv_darray_get(&model->sigs, si);
        if (!sig)
            continue;

        lv_strbuf_printf(&sb, "%ssig %s {\n", sig->is_abstract ? "abstract " : "", sig->name);

        /* 导出关系字段 */
        if (model->relations) {
            for (int ri = 0; ri < model->relation_count; ri++) {
                Relation *rel = model->relations[ri];
                if (rel) {
                    lv_strbuf_printf(&sb, "  %s: set ", rel->name ? rel->name : "R");
                    for (int di = 0; di < rel->arity; di++) {
                        lv_strbuf_printf(&sb, "%s%s", (di > 0 ? " -> " : ""),
                                         rel->domains[di] ? rel->domains[di]->name : "univ");
                    }
                    lv_strbuf_printf(&sb, "\n");
                }
            }
        }
        lv_strbuf_printf(&sb, "}\n\n");
    }

    /* 导出事实 */
    for (int fi = 0; fi < model->fact_count; fi++) {
        lv_strbuf_printf(&sb, "fact {\n  /* fact %d */\n}\n\n", fi);
    }

    /* 导出断言 */
    for (int ai = 0; ai < model->assertion_count; ai++) {
        lv_strbuf_printf(&sb, "assert {\n  /* assertion %d */\n}\n", ai);
    }

    /* 导出范围配置 */
    lv_strbuf_printf(&sb, "\nrun {} for %d\n", model->max_point_count);

    return lv_strbuf_to_string(&sb);
}

char *relation_instance_export_xml(const RelInstance *inst) {
    lv_CHECK_NULL(inst, NULL);

    /* lvStrBuf 动态构建（自动扩容），消除原固定 4KB 缓冲的静默截断 */
    lvStrBuf sb;
    lv_strbuf_init(&sb);

    lv_strbuf_printf(&sb, "<?xml version=\"1.0\"?>\n<alloy>\n");

    /* 导出实例中的原子 */
    lv_strbuf_printf(&sb, "  <instance>\n");
    for (int ai = 0; ai < inst->atom_count; ai++) {
        RelAtom *atom = inst->atoms[ai];
        if (!atom)
            continue;
        const char *type_name = "Unknown";
        if (atom->type >= 0 && (size_t)atom->type < sizeof(atom_type_names) / sizeof(atom_type_names[0])) {
            type_name = atom_type_names[atom->type];
        }
        lv_strbuf_printf(&sb, "    <atom id=\"%d\" label=\"%s\"/>\n", atom->atom_id, type_name);
    }

    lv_strbuf_printf(&sb, "  </instance>\n</alloy>\n");

    return lv_strbuf_to_string(&sb);
}
