/**
 * @file lambda_type_check.c
 * @brief λ-项类型检查与类型推断实现
 *
 * 实现 Simply Typed Lambda Calculus (STLC) 的类型检查/推断规则，
 * 桥接 LvLambdaTerm（λ-项）与 TypeSystem（类型系统）。
 *
 * 当前实现使用类型变量（TYPE_KIND_VARIABLE）作为 λ-抽象 binder
 * 的类型，即 Γ, x:α ⊢ M:τ → Γ ⊢ λx.M:α→τ（α 为自由类型变量）。
 * 这等价于 System F 风格的隐式多态：λ-抽象的形参类型在应用时
 * 通过类型等价检查被具体类型实例化。
 */

#include "lv/lambda_type_check.h"

#include <stdlib.h>
#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"

/* 上下文栈初始容量 */
#define TYPING_STACK_INIT_CAP 16

/* ── 上下文管理 ── */

bool lambda_type_check_init(LambdaTypingContext *ctx, TypeSystem *ts) {
    if (!ctx || !ts)
        return false;

    ctx->ts = ts;
    ctx->stack_capacity = TYPING_STACK_INIT_CAP;
    ctx->type_stack = (TypeRegion **) lv_malloc((size_t) ctx->stack_capacity * sizeof(TypeRegion *));
    if (!ctx->type_stack) {
        ctx->stack_capacity = 0;
        return false;
    }
    ctx->stack_count = 0;
    return true;
}

void lambda_type_check_destroy(LambdaTypingContext *ctx) {
    if (!ctx)
        return;
    lv_free((void **) &ctx->type_stack);
    ctx->type_stack = NULL;
    ctx->stack_count = 0;
    ctx->stack_capacity = 0;
    /* ctx->ts 由调用者管理，不在此处销毁 */
}

bool lambda_type_check_push(LambdaTypingContext *ctx, TypeRegion *type) {
    if (!ctx || !type)
        return false;

    if (ctx->stack_count >= ctx->stack_capacity) {
        int new_cap = ctx->stack_capacity * 2;
        TypeRegion **new_stack =
            (TypeRegion **) lv_realloc(ctx->type_stack, (size_t) new_cap * sizeof(TypeRegion *));
        if (!new_stack)
            return false;
        ctx->type_stack = new_stack;
        ctx->stack_capacity = new_cap;
    }

    ctx->type_stack[ctx->stack_count++] = type;
    return true;
}

void lambda_type_check_pop(LambdaTypingContext *ctx) {
    if (!ctx || ctx->stack_count <= 0)
        return;
    ctx->stack_count--;
}

/* ── 核心类型推断 - 查找表 ── */

typedef TypeRegion *(*InferHandler)(LvLambdaTerm *term, LambdaTypingContext *ctx);

static TypeRegion *infer_var(LvLambdaTerm *term, LambdaTypingContext *ctx) {
    int idx = term->data.var.index;
    if (idx < 0 || idx >= ctx->stack_count)
        return NULL;
    return ctx->type_stack[ctx->stack_count - 1 - idx];
}

static TypeRegion *infer_abs(LvLambdaTerm *term, LambdaTypingContext *ctx) {
    if (!term->data.abs.body)
        return NULL;

    TypeRegion *binder_type = type_create_variable(ctx->ts, "_abs_binder");
    if (!binder_type)
        return NULL;

    if (!lambda_type_check_push(ctx, binder_type))
        return NULL;

    TypeRegion *body_type = lambda_type_infer(term->data.abs.body, ctx);
    lambda_type_check_pop(ctx);

    if (!body_type)
        return NULL;

    return type_create_function(ctx->ts, binder_type, body_type);
}

static TypeRegion *infer_app(LvLambdaTerm *term, LambdaTypingContext *ctx) {
    if (!term->data.app.left || !term->data.app.right)
        return NULL;

    TypeRegion *left_type = lambda_type_infer(term->data.app.left, ctx);
    if (!left_type)
        return NULL;

    TypeRegion *right_type = lambda_type_infer(term->data.app.right, ctx);
    if (!right_type)
        return NULL;

    if (left_type->kind != TYPE_KIND_FUNCTION)
        return NULL;

    TypeEquivResult equiv = type_check_equivalence(ctx->ts, left_type->input_type, right_type, false);
    if (equiv != TYPE_EQUIV_OK)
        return NULL;

    return left_type->output_type;
}

static const InferHandler infer_table[LV_LAMBDA_APP + 1] = {
    [LV_LAMBDA_VAR] = infer_var,
    [LV_LAMBDA_ABS] = infer_abs,
    [LV_LAMBDA_APP] = infer_app,
};

TypeRegion *lambda_type_infer(LvLambdaTerm *term, LambdaTypingContext *ctx) {
    if (!term || !ctx || !ctx->ts)
        return NULL;

    if (term->type >= 0 && term->type <= LV_LAMBDA_APP) {
        InferHandler handler = infer_table[term->type];
        if (handler)
            return handler(term, ctx);
    }

    return NULL;
}

/* ── 便捷函数 ── */

TypeRegion *lambda_type_check_and_infer(LvLambdaTerm *term) {
    if (!term)
        return NULL;

    TypeSystem *ts = type_system_create();
    if (!ts)
        return NULL;

    LambdaTypingContext ctx;
    if (!lambda_type_check_init(&ctx, ts)) {
        type_system_destroy(ts);
        return NULL;
    }

    TypeRegion *result = lambda_type_infer(term, &ctx);

    lambda_type_check_destroy(&ctx);
    type_system_destroy(ts);

    return result;
}
