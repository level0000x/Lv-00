/**
 * @file autodiff.c
 * @brief Implementation of the automatic differentiation engine.
 *
 * @details Implements forward mode and reverse mode automatic differentiation.
 *
 *          Forward mode:
 *          - Evaluates the expression while simultaneously propagating the
 *            tangent (derivative) value.
 *          - For each node, computes both the primal value and the tangent value.
 *          - Efficient for functions f: R^n -> R^m where n is small.
 *
 *          Reverse mode:
 *          - First pass: evaluates the expression top-down, storing primal values.
 *          - Second pass: propagates adjoints (gradients) bottom-up.
 *          - Efficient for functions f: R^n -> R where n is large.
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date   2026-05-25
 */

#include "autodiff.h"

#include "lv/lv_platform.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "lv_utils.h" /* lv_calloc / lv_malloc / lv_free */
#include "lv/autodiff_vtable.h"

/* ============================================================
 * Internal helpers
 * ============================================================ */

/**
 * @brief Allocate a new expression node with zero-initialized fields.
 *
 * @return Newly allocated node, or NULL on failure
 */
static lvADExpr *expr_alloc(lvADExprKind kind) {
    lvADExpr *expr = (lvADExpr *) lv_calloc(1, sizeof(lvADExpr));
    if (expr) {
        expr->kind = kind;
        expr->var_index = -1;
        expr->gradient = 0.0;
    }
    return expr;
}

/**
 * @brief Set children of a binary expression node.
 *
 * @param expr  The expression node
 * @param a     First child
 * @param b     Second child
 * @return true on success, false on failure
 */
static bool expr_set_binary_children(lvADExpr *expr, lvADExpr *a, lvADExpr *b) {
    expr->children = (lvADExpr **) lv_malloc(2 * sizeof(lvADExpr *));
    if (!expr->children)
        return false;
    expr->children[0] = a;
    expr->children[1] = b;
    expr->child_count = 2;
    return true;
}

/**
 * @brief Set a single child of a unary expression node.
 *
 * @param expr  The expression node
 * @param child The child node
 * @return true on success, false on failure
 */
static bool expr_set_unary_child(lvADExpr *expr, lvADExpr *child) {
    expr->children = (lvADExpr **) lv_malloc(sizeof(lvADExpr *));
    if (!expr->children)
        return false;
    expr->children[0] = child;
    expr->child_count = 1;
    return true;
}

/* ============================================================
 * Forward declarations of original dispatch functions
 * (vtable handler functions call these for recursion)
 * ============================================================ */
static ForwardResult forward_eval(const lvADExpr *expr, int var_index, double var_value);
static double reverse_forward_pass(const lvADExpr *expr, const double *var_values, size_t var_count);
static void reverse_backward_pass(lvADExpr *expr, double adjoint);
static void store_values(lvADExpr *expr, const double *var_values, size_t var_count);

/* ============================================================
 * Vtable handler functions (one set per ADKind)
 * ============================================================ */

/* ---- AD_CONST handlers ---- */
static ForwardResult forward_eval_const(const lvADExpr *expr, int var_index, double var_value) {
    (void)var_index;
    (void)var_value;
    ForwardResult result = {expr->value, 0.0};
    return result;
}
static double reverse_forward_const(const lvADExpr *expr, const double *var_values, size_t var_count) {
    (void)var_values;
    (void)var_count;
    return expr->value;
}
static void reverse_backward_const(lvADExpr *expr, double adjoint) {
    (void)expr;
    (void)adjoint;
    /* Leaf node: nothing to propagate */
}
static void store_values_const(lvADExpr *expr, const double *var_values, size_t var_count) {
    (void)expr;
    (void)var_values;
    (void)var_count;
    /* Value already set at creation */
}

/* ---- AD_VAR handlers ---- */
static ForwardResult forward_eval_var(const lvADExpr *expr, int var_index, double var_value) {
    ForwardResult result = {var_value, (expr->var_index == var_index) ? 1.0 : 0.0};
    return result;
}
static double reverse_forward_var(const lvADExpr *expr, const double *var_values, size_t var_count) {
    if (expr->var_index >= 0 && (size_t)expr->var_index < var_count) {
        return var_values[expr->var_index];
    }
    return 0.0;
}
static void reverse_backward_var(lvADExpr *expr, double adjoint) {
    (void)expr;
    (void)adjoint;
    /* Leaf node: gradient already accumulated in original function */
}
static void store_values_var(lvADExpr *expr, const double *var_values, size_t var_count) {
    if (expr->var_index >= 0 && (size_t)expr->var_index < var_count) {
        expr->value = var_values[expr->var_index];
    }
}

/* ---- AD_ADD handlers ---- */
static ForwardResult forward_eval_add(const lvADExpr *expr, int var_index, double var_value) {
    ForwardResult a = forward_eval(expr->children[0], var_index, var_value);
    ForwardResult b = forward_eval(expr->children[1], var_index, var_value);
    ForwardResult result = {a.value + b.value, a.tangent + b.tangent};
    return result;
}
static double reverse_forward_add(const lvADExpr *expr, const double *var_values, size_t var_count) {
    return reverse_forward_pass(expr->children[0], var_values, var_count) +
           reverse_forward_pass(expr->children[1], var_values, var_count);
}
static void reverse_backward_add(lvADExpr *expr, double adjoint) {
    reverse_backward_pass(expr->children[0], adjoint);
    reverse_backward_pass(expr->children[1], adjoint);
}
static void store_values_add(lvADExpr *expr, const double *var_values, size_t var_count) {
    store_values(expr->children[0], var_values, var_count);
    store_values(expr->children[1], var_values, var_count);
    expr->value = expr->children[0]->value + expr->children[1]->value;
}

/* ---- AD_MUL handlers ---- */
static ForwardResult forward_eval_mul(const lvADExpr *expr, int var_index, double var_value) {
    ForwardResult a = forward_eval(expr->children[0], var_index, var_value);
    ForwardResult b = forward_eval(expr->children[1], var_index, var_value);
    ForwardResult result = {a.value * b.value, a.tangent * b.value + a.value * b.tangent};
    return result;
}
static double reverse_forward_mul(const lvADExpr *expr, const double *var_values, size_t var_count) {
    return reverse_forward_pass(expr->children[0], var_values, var_count) *
           reverse_forward_pass(expr->children[1], var_values, var_count);
}
static void reverse_backward_mul(lvADExpr *expr, double adjoint) {
    double a_val = expr->children[0]->value;
    double b_val = expr->children[1]->value;
    reverse_backward_pass(expr->children[0], adjoint * b_val);
    reverse_backward_pass(expr->children[1], adjoint * a_val);
}
static void store_values_mul(lvADExpr *expr, const double *var_values, size_t var_count) {
    store_values(expr->children[0], var_values, var_count);
    store_values(expr->children[1], var_values, var_count);
    expr->value = expr->children[0]->value * expr->children[1]->value;
}

/* ---- AD_NEG handlers ---- */
static ForwardResult forward_eval_neg(const lvADExpr *expr, int var_index, double var_value) {
    ForwardResult a = forward_eval(expr->children[0], var_index, var_value);
    ForwardResult result = {-a.value, -a.tangent};
    return result;
}
static double reverse_forward_neg(const lvADExpr *expr, const double *var_values, size_t var_count) {
    return -reverse_forward_pass(expr->children[0], var_values, var_count);
}
static void reverse_backward_neg(lvADExpr *expr, double adjoint) {
    reverse_backward_pass(expr->children[0], -adjoint);
}
static void store_values_neg(lvADExpr *expr, const double *var_values, size_t var_count) {
    store_values(expr->children[0], var_values, var_count);
    expr->value = -expr->children[0]->value;
}

/* ---- AD_SIN handlers ---- */
static ForwardResult forward_eval_sin(const lvADExpr *expr, int var_index, double var_value) {
    ForwardResult a = forward_eval(expr->children[0], var_index, var_value);
    ForwardResult result = {sin(a.value), a.tangent * cos(a.value)};
    return result;
}
static double reverse_forward_sin(const lvADExpr *expr, const double *var_values, size_t var_count) {
    return sin(reverse_forward_pass(expr->children[0], var_values, var_count));
}
static void reverse_backward_sin(lvADExpr *expr, double adjoint) {
    double a_val = expr->children[0]->value;
    reverse_backward_pass(expr->children[0], adjoint * cos(a_val));
}
static void store_values_sin(lvADExpr *expr, const double *var_values, size_t var_count) {
    store_values(expr->children[0], var_values, var_count);
    expr->value = sin(expr->children[0]->value);
}

/* ---- AD_COS handlers ---- */
static ForwardResult forward_eval_cos(const lvADExpr *expr, int var_index, double var_value) {
    ForwardResult a = forward_eval(expr->children[0], var_index, var_value);
    ForwardResult result = {cos(a.value), -a.tangent * sin(a.value)};
    return result;
}
static double reverse_forward_cos(const lvADExpr *expr, const double *var_values, size_t var_count) {
    return cos(reverse_forward_pass(expr->children[0], var_values, var_count));
}
static void reverse_backward_cos(lvADExpr *expr, double adjoint) {
    double a_val = expr->children[0]->value;
    reverse_backward_pass(expr->children[0], -adjoint * sin(a_val));
}
static void store_values_cos(lvADExpr *expr, const double *var_values, size_t var_count) {
    store_values(expr->children[0], var_values, var_count);
    expr->value = cos(expr->children[0]->value);
}

/* ---- AD_POW handlers ---- */
static ForwardResult forward_eval_pow(const lvADExpr *expr, int var_index, double var_value) {
    ForwardResult base = forward_eval(expr->children[0], var_index, var_value);
    ForwardResult exp = forward_eval(expr->children[1], var_index, var_value);
    ForwardResult result = {0.0, 0.0};
    /* 保护：负底数的非整数幂在实数域无定义 */
    if (base.value < 0.0 && fabs(exp.value - round(exp.value)) > 1e-15) {
        return result;
    }
    result.value = pow(base.value, exp.value);
    /* d/dx (f^g) = f^g * (g' * ln(f) + g * f'/f) */
    if (base.value > 0.0) {
        result.tangent = result.value * (exp.tangent * log(base.value) + exp.value * base.tangent / base.value);
    } else if (fabs(exp.tangent) < 1e-15) {
        /* Exponent is effectively constant: d/dx (base^n) = n * base^(n-1) * dbase/dx */
        if (base.value < 0.0 && fabs(exp.value - 1.0 - round(exp.value - 1.0)) > 1e-15) {
            result.tangent = 0.0;
        } else {
            result.tangent = exp.value * pow(base.value, exp.value - 1.0) * base.tangent;
        }
    } else {
        result.tangent = 0.0;
    }
    return result;
}
static double reverse_forward_pow(const lvADExpr *expr, const double *var_values, size_t var_count) {
    return pow(reverse_forward_pass(expr->children[0], var_values, var_count),
               reverse_forward_pass(expr->children[1], var_values, var_count));
}
static void reverse_backward_pow(lvADExpr *expr, double adjoint) {
    double a_val = expr->children[0]->value;
    double b_val = expr->children[1]->value;
    double f_val = expr->value;
    reverse_backward_pass(expr->children[0], adjoint * b_val * pow(a_val, b_val - 1.0));
    if (a_val > 0.0) {
        reverse_backward_pass(expr->children[1], adjoint * f_val * log(a_val));
    }
}
static void store_values_pow(lvADExpr *expr, const double *var_values, size_t var_count) {
    store_values(expr->children[0], var_values, var_count);
    store_values(expr->children[1], var_values, var_count);
    expr->value = pow(expr->children[0]->value, expr->children[1]->value);
}

/* ============================================================
 * Vtable instances (one per ADKind)
 * ============================================================ */
static const lvADExprOps lv_ad_const_ops = {
    forward_eval_const,
    reverse_forward_const,
    reverse_backward_const,
    store_values_const
};
static const lvADExprOps lv_ad_var_ops = {
    forward_eval_var,
    reverse_forward_var,
    reverse_backward_var,
    store_values_var
};
static const lvADExprOps lv_ad_add_ops = {
    forward_eval_add,
    reverse_forward_add,
    reverse_backward_add,
    store_values_add
};
static const lvADExprOps lv_ad_mul_ops = {
    forward_eval_mul,
    reverse_forward_mul,
    reverse_backward_mul,
    store_values_mul
};
static const lvADExprOps lv_ad_neg_ops = {
    forward_eval_neg,
    reverse_forward_neg,
    reverse_backward_neg,
    store_values_neg
};
static const lvADExprOps lv_ad_sin_ops = {
    forward_eval_sin,
    reverse_forward_sin,
    reverse_backward_sin,
    store_values_sin
};
static const lvADExprOps lv_ad_cos_ops = {
    forward_eval_cos,
    reverse_forward_cos,
    reverse_backward_cos,
    store_values_cos
};
static const lvADExprOps lv_ad_pow_ops = {
    forward_eval_pow,
    reverse_forward_pow,
    reverse_backward_pow,
    store_values_pow
};

/* ============================================================
 * lv_ad_get_ops() — vtable lookup
 * ============================================================ */
const lvADExprOps *lv_ad_get_ops(lvADExprKind kind) {
    static const lvADExprOps *lookup[] = {
        [AD_CONST] = &lv_ad_const_ops,
        [AD_VAR]   = &lv_ad_var_ops,
        [AD_ADD]   = &lv_ad_add_ops,
        [AD_MUL]   = &lv_ad_mul_ops,
        [AD_NEG]   = &lv_ad_neg_ops,
        [AD_SIN]   = &lv_ad_sin_ops,
        [AD_COS]   = &lv_ad_cos_ops,
        [AD_POW]   = &lv_ad_pow_ops,
    };
    if (kind < AD_CONST || kind > AD_POW)
        return NULL;
    return lookup[kind];
}

/* ============================================================
 * Forward mode: evaluate value and derivative simultaneously
 * ============================================================ */

/**
 * @brief Recursively evaluate in forward mode.
 *
 * @param expr        The expression node
 * @param var_index   The variable we are differentiating with respect to
 * @param var_value   The value of that variable
 * @return (value, derivative) pair
 */
static ForwardResult forward_eval(const lvADExpr *expr, int var_index, double var_value) {
    const lvADExprOps *ops = lv_ad_get_ops(expr->kind);
    if (ops)
        return ops->forward_eval(expr, var_index, var_value);
    ForwardResult result = {0.0, 0.0};
    return result;
}

/* ============================================================
 * Reverse mode: forward evaluation + backward gradient propagation
 * ============================================================ */

/**
 * @brief Reset all gradient accumulators in the expression tree.
 */
static void reset_gradients(lvADExpr *expr) {
    if (!expr)
        return;
    expr->gradient = 0.0;
    for (size_t i = 0; i < expr->child_count; i++) {
        reset_gradients(expr->children[i]);
    }
}

/**
 * @brief Forward evaluation pass: compute primal values.
 *
 * @param expr        The expression node
 * @param var_values  Array of variable values
 * @param var_count   Number of variables
 * @return The primal value of this node
 */
static double reverse_forward_pass(const lvADExpr *expr, const double *var_values, size_t var_count) {
    const lvADExprOps *ops = lv_ad_get_ops(expr->kind);
    if (ops)
        return ops->reverse_forward(expr, var_values, var_count);
    return 0.0;
}

/**
 * @brief Backward pass: propagate gradients from output to inputs.
 *
 * @param expr  The expression node
 * @param adjoint  The incoming adjoint (gradient) value
 */
static void reverse_backward_pass(lvADExpr *expr, double adjoint) {
    if (!expr)
        return;

    /* Accumulate gradient (common to all node kinds) */
    expr->gradient += adjoint;

    const lvADExprOps *ops = lv_ad_get_ops(expr->kind);
    if (ops)
        ops->reverse_backward(expr, adjoint);
}

/**
 * @brief Store primal values in all nodes during forward pass.
 */
static void store_values(lvADExpr *expr, const double *var_values, size_t var_count) {
    if (!expr)
        return;

    const lvADExprOps *ops = lv_ad_get_ops(expr->kind);
    if (ops)
        ops->store_values(expr, var_values, var_count);
}

/* ============================================================
 * API implementation: Engine lifecycle
 * ============================================================ */

lvADEngine *lv_ad_engine_create(lvADMode mode) {
    lvADEngine *engine = (lvADEngine *) lv_malloc(sizeof(lvADEngine));
    if (engine) {
        engine->mode = mode;
    }
    return engine;
}

void lv_ad_engine_destroy(lvADEngine *engine) {
    lv_free((void **)&(engine));
}

/* ============================================================
 * API implementation: Expression construction
 * ============================================================ */

lvADExpr *lv_ad_expr_create_const(double value) {
    lvADExpr *expr = expr_alloc(AD_CONST);
    if (expr) {
        expr->value = value;
    }
    return expr;
}

lvADExpr *lv_ad_expr_create_var(int var_index) {
    lvADExpr *expr = expr_alloc(AD_VAR);
    if (expr) {
        expr->var_index = var_index;
    }
    return expr;
}

lvADExpr *lv_ad_expr_add(lvADExpr *a, lvADExpr *b) {
    if (!a || !b)
        return NULL;
    lvADExpr *expr = expr_alloc(AD_ADD);
    if (!expr)
        return NULL;
    if (!expr_set_binary_children(expr, a, b)) {
        lv_free((void **)&(expr));
        return NULL;
    }
    return expr;
}

lvADExpr *lv_ad_expr_mul(lvADExpr *a, lvADExpr *b) {
    if (!a || !b)
        return NULL;
    lvADExpr *expr = expr_alloc(AD_MUL);
    if (!expr)
        return NULL;
    if (!expr_set_binary_children(expr, a, b)) {
        lv_free((void **)&(expr));
        return NULL;
    }
    return expr;
}

lvADExpr *lv_ad_expr_sin(lvADExpr *x) {
    if (!x)
        return NULL;
    lvADExpr *expr = expr_alloc(AD_SIN);
    if (!expr)
        return NULL;
    if (!expr_set_unary_child(expr, x)) {
        lv_free((void **)&(expr));
        return NULL;
    }
    return expr;
}

lvADExpr *lv_ad_expr_cos(lvADExpr *x) {
    if (!x)
        return NULL;
    lvADExpr *expr = expr_alloc(AD_COS);
    if (!expr)
        return NULL;
    if (!expr_set_unary_child(expr, x)) {
        lv_free((void **)&(expr));
        return NULL;
    }
    return expr;
}

lvADExpr *lv_ad_expr_pow(lvADExpr *base, lvADExpr *exponent) {
    if (!base || !exponent)
        return NULL;
    lvADExpr *expr = expr_alloc(AD_POW);
    if (!expr)
        return NULL;
    if (!expr_set_binary_children(expr, base, exponent)) {
        lv_free((void **)&(expr));
        return NULL;
    }
    return expr;
}

void lv_ad_expr_destroy(lvADExpr *expr) {
    if (!expr)
        return;

    /* 保存子节点指针并清空 children，防止共享子节点（如 x*x）导致 double free */
    size_t count = expr->child_count;
    lvADExpr **saved = expr->children;
    expr->children = NULL;
    expr->child_count = 0;

    /* 销毁子节点：跳过重复指针，每个唯一指针只释放一次 */
    for (size_t i = 0; i < count; i++) {
        if (!saved[i])
            continue;
        /* 检查是否与后续子节点重复 */
        bool duplicate = false;
        for (size_t j = i + 1; j < count; j++) {
            if (saved[j] == saved[i]) {
                saved[j] = NULL; /* 标记为已处理，避免后续重复释放 */
                duplicate = true;
            }
        }
        if (!duplicate) {
            lv_ad_expr_destroy(saved[i]);
        }
    }

    lv_free((void **)&(saved));
    lv_free((void **)&(expr));
}

/* ============================================================
 * API implementation: Differentiation
 * ============================================================ */

bool lv_ad_forward_diff(lvADExpr *expr, int var_index, double var_value, double *value, double *derivative) {
    if (!expr || !value || !derivative)
        return false;

    ForwardResult result = forward_eval(expr, var_index, var_value);
    *value = result.value;
    *derivative = result.tangent;
    return true;
}

/**
 * @brief Recursively collect gradients from VAR nodes into the output array.
 *
 * Each VAR node's gradient field was accumulated during the backward pass.
 * Multiple nodes can reference the same var_index (e.g., x appears in both
 * children of x*x), so we sum their gradients into the output array.
 *
 * IMPORTANT: The expression graph is a DAG, not a tree — children may be
 * shared (e.g., x*x has the same VAR node as both children of MUL).
 * The backward pass already accumulated the correct total gradient on the
 * shared node. We must deduplicate sibling pointers to avoid adding the
 * same gradient multiple times.
 */
static void collect_gradients(const lvADExpr *expr, double *gradients, size_t var_count) {
    if (!expr)
        return;

    if (expr->kind == AD_VAR && expr->var_index >= 0 && (size_t) expr->var_index < var_count) {
        gradients[expr->var_index] += expr->gradient;
    }

    /* Deduplicate children: skip children that share a pointer with an
     * earlier sibling, preventing double-counting of shared subgraphs. */
    for (size_t i = 0; i < expr->child_count; i++) {
        bool already_visited = false;
        for (size_t j = 0; j < i; j++) {
            if (expr->children[j] == expr->children[i]) {
                already_visited = true;
                break;
            }
        }
        if (!already_visited) {
            collect_gradients(expr->children[i], gradients, var_count);
        }
    }
}

bool lv_ad_reverse_diff(lvADExpr *expr, const double *var_values, size_t var_count, double *value, double *gradients) {
    if (!expr || !var_values || !value || !gradients)
        return false;

    /* Reset all gradients */
    reset_gradients(expr);

    /* Forward pass: store primal values */
    store_values(expr, var_values, var_count);

    /* Get the output value */
    *value = expr->value;

    /* Backward pass: propagate gradients from output (adjoint = 1.0) */
    reverse_backward_pass(expr, 1.0);

    /* Collect accumulated gradients from VAR nodes into the output array */
    memset(gradients, 0, var_count * sizeof(double));
    collect_gradients(expr, gradients, var_count);

    return true;
}

/* ============================================================
 * API implementation: Evaluation and gradient query
 * ============================================================ */

bool lv_ad_eval(lvADExpr *expr, const double *var_values, size_t var_count, double *result) {
    if (!expr || !var_values || !result)
        return false;
    *result = reverse_forward_pass(expr, var_values, var_count);
    return true;
}

double lv_ad_grad(lvADExpr *expr, int var_index) {
    if (!expr)
        return 0.0;

    /* Sum gradients across all occurrences of var_index in the DAG.
     * A variable may appear multiple times (e.g., x in x*x), and each
     * occurrence accumulates its own gradient contribution.
     *
     * IMPORTANT: The expression graph is a DAG with shared children.
     * We must skip duplicate sibling pointers to avoid double-counting
     * the gradient of a shared node. */
    double total = 0.0;

    if (expr->kind == AD_VAR && expr->var_index == var_index) {
        total += expr->gradient;
    }

    for (size_t i = 0; i < expr->child_count; i++) {
        /* Skip if this child's pointer matches an earlier sibling */
        bool already_visited = false;
        for (size_t j = 0; j < i; j++) {
            if (expr->children[j] == expr->children[i]) {
                already_visited = true;
                break;
            }
        }
        if (!already_visited) {
            total += lv_ad_grad(expr->children[i], var_index);
        }
    }

    return total;
}
