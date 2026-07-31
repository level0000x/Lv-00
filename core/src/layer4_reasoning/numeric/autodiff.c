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
 * Forward mode: evaluate value and derivative simultaneously
 * ============================================================ */

/**
 * @brief Forward mode evaluation.
 *
 * Returns a pair (value, tangent) for each node.
 */
typedef struct {
    double value;   /**< Primal value */
    double tangent; /**< Tangent (derivative) value */
} ForwardResult;

/**
 * @brief Recursively evaluate in forward mode.
 *
 * @param expr        The expression node
 * @param var_index   The variable we are differentiating with respect to
 * @param var_value   The value of that variable
 * @return (value, derivative) pair
 */
static ForwardResult forward_eval(const lvADExpr *expr, int var_index, double var_value) {
    ForwardResult result = {0.0, 0.0};

    switch (expr->kind) {
        case AD_CONST:
            result.value = expr->value;
            result.tangent = 0.0;
            break;

        case AD_VAR:
            result.value = var_value;
            result.tangent = (expr->var_index == var_index) ? 1.0 : 0.0;
            break;

        case AD_ADD: {
            ForwardResult a = forward_eval(expr->children[0], var_index, var_value);
            ForwardResult b = forward_eval(expr->children[1], var_index, var_value);
            result.value = a.value + b.value;
            result.tangent = a.tangent + b.tangent;
            break;
        }

        case AD_MUL: {
            ForwardResult a = forward_eval(expr->children[0], var_index, var_value);
            ForwardResult b = forward_eval(expr->children[1], var_index, var_value);
            result.value = a.value * b.value;
            result.tangent = a.tangent * b.value + a.value * b.tangent;
            break;
        }

        case AD_NEG: {
            ForwardResult a = forward_eval(expr->children[0], var_index, var_value);
            result.value = -a.value;
            result.tangent = -a.tangent;
            break;
        }

        case AD_SIN: {
            ForwardResult a = forward_eval(expr->children[0], var_index, var_value);
            result.value = sin(a.value);
            result.tangent = a.tangent * cos(a.value);
            break;
        }

        case AD_COS: {
            ForwardResult a = forward_eval(expr->children[0], var_index, var_value);
            result.value = cos(a.value);
            result.tangent = -a.tangent * sin(a.value);
            break;
        }

        case AD_POW: {
            ForwardResult base = forward_eval(expr->children[0], var_index, var_value);
            ForwardResult exp = forward_eval(expr->children[1], var_index, var_value);
            /* 保护：负底数的非整数幂在实数域无定义 */
            if (base.value < 0.0 && fabs(exp.value - round(exp.value)) > 1e-15) {
                result.value = 0.0;
                result.tangent = 0.0;
                break;
            }
            result.value = pow(base.value, exp.value);
            /* d/dx (f^g) = f^g * (g' * ln(f) + g * f'/f)
             *
             * When base > 0, the general formula applies.
             * When base <= 0, the function x^y is only real-differentiable if
             * the exponent is constant (exp.tangent == 0); otherwise the
             * derivative is ill-defined in the real domain. */
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
                /* Base <= 0 with varying exponent: derivative undefined in reals.
                 * Return 0 to indicate non-differentiability. */
                result.tangent = 0.0;
            }
            break;
        }
    }

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
static double reverse_forward_pass(lvADExpr *expr, const double *var_values, size_t var_count) {
    switch (expr->kind) {
        case AD_CONST:
            return expr->value;

        case AD_VAR:
            if (expr->var_index >= 0 && (size_t) expr->var_index < var_count) {
                return var_values[expr->var_index];
            }
            return 0.0;

        case AD_ADD:
            return reverse_forward_pass(expr->children[0], var_values, var_count) +
                   reverse_forward_pass(expr->children[1], var_values, var_count);

        case AD_MUL:
            return reverse_forward_pass(expr->children[0], var_values, var_count) *
                   reverse_forward_pass(expr->children[1], var_values, var_count);

        case AD_NEG:
            return -reverse_forward_pass(expr->children[0], var_values, var_count);

        case AD_SIN:
            return sin(reverse_forward_pass(expr->children[0], var_values, var_count));

        case AD_COS:
            return cos(reverse_forward_pass(expr->children[0], var_values, var_count));

        case AD_POW:
            return pow(reverse_forward_pass(expr->children[0], var_values, var_count),
                       reverse_forward_pass(expr->children[1], var_values, var_count));
    }
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

    /* Accumulate gradient */
    expr->gradient += adjoint;

    switch (expr->kind) {
        case AD_CONST:
        case AD_VAR:
            /* Leaf nodes: no children to propagate to */
            break;

        case AD_ADD:
            /* d(a+b)/da = 1, d(a+b)/db = 1 */
            reverse_backward_pass(expr->children[0], adjoint);
            reverse_backward_pass(expr->children[1], adjoint);
            break;

        case AD_MUL: {
            /* d(a*b)/da = b, d(a*b)/db = a */
            double a_val = expr->children[0]->value;
            double b_val = expr->children[1]->value;
            reverse_backward_pass(expr->children[0], adjoint * b_val);
            reverse_backward_pass(expr->children[1], adjoint * a_val);
            break;
        }

        case AD_NEG:
            /* d(-a)/da = -1 */
            reverse_backward_pass(expr->children[0], -adjoint);
            break;

        case AD_SIN: {
            /* d(sin(a))/da = cos(a) */
            double a_val = expr->children[0]->value;
            reverse_backward_pass(expr->children[0], adjoint * cos(a_val));
            break;
        }

        case AD_COS: {
            /* d(cos(a))/da = -sin(a) */
            double a_val = expr->children[0]->value;
            reverse_backward_pass(expr->children[0], -adjoint * sin(a_val));
            break;
        }

        case AD_POW: {
            /* d(a^b)/da = b * a^(b-1) */
            /* d(a^b)/db = a^b * ln(a) */
            double a_val = expr->children[0]->value;
            double b_val = expr->children[1]->value;
            double f_val = expr->value;
            reverse_backward_pass(expr->children[0], adjoint * b_val * pow(a_val, b_val - 1.0));
            if (a_val > 0.0) {
                reverse_backward_pass(expr->children[1], adjoint * f_val * log(a_val));
            }
            break;
        }
    }
}

/**
 * @brief Store primal values in all nodes during forward pass.
 */
static void store_values(lvADExpr *expr, const double *var_values, size_t var_count) {
    if (!expr)
        return;

    switch (expr->kind) {
        case AD_CONST:
            /* value already set */
            break;
        case AD_VAR:
            if (expr->var_index >= 0 && (size_t) expr->var_index < var_count) {
                expr->value = var_values[expr->var_index];
            }
            break;
        case AD_ADD:
            store_values(expr->children[0], var_values, var_count);
            store_values(expr->children[1], var_values, var_count);
            expr->value = expr->children[0]->value + expr->children[1]->value;
            break;
        case AD_MUL:
            store_values(expr->children[0], var_values, var_count);
            store_values(expr->children[1], var_values, var_count);
            expr->value = expr->children[0]->value * expr->children[1]->value;
            break;
        case AD_NEG:
            store_values(expr->children[0], var_values, var_count);
            expr->value = -expr->children[0]->value;
            break;
        case AD_SIN:
            store_values(expr->children[0], var_values, var_count);
            expr->value = sin(expr->children[0]->value);
            break;
        case AD_COS:
            store_values(expr->children[0], var_values, var_count);
            expr->value = cos(expr->children[0]->value);
            break;
        case AD_POW:
            store_values(expr->children[0], var_values, var_count);
            store_values(expr->children[1], var_values, var_count);
            expr->value = pow(expr->children[0]->value, expr->children[1]->value);
            break;
    }
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
