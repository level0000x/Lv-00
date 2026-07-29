/**
 * @file lambda_type_check.h
 * @brief λ-项类型检查与类型推断（Public API）
 *
 * 将现有类型系统（TypeSystem）与 λ-演算（LvLambdaTerm）桥接，
 * 提供对 λ-项的简单类型检查（Simply Typed Lambda Calculus）和
 * 类型推断功能。
 *
 * 类型规则（STLC 风格）：
 *   Var:  x:σ ∈ Γ
 *        ---------
 *        Γ ⊢ x : σ
 *
 *   Abs:  Γ, x:σ ⊢ M : τ
 *        ----------------
 *        Γ ⊢ λx.M : σ → τ
 *
 *   App:  Γ ⊢ M : σ → τ    Γ ⊢ N : σ
 *        ----------------------------
 *                Γ ⊢ M N : τ
 */

#ifndef lv_LAMBDA_TYPE_CHECK_H
#define lv_LAMBDA_TYPE_CHECK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv/lambda_term.h"
#include "lv/type_system.h"

/**
 * @brief λ-项类型推断上下文
 *
 * 维护 De Bruijn 索引到 TypeRegion 的映射栈。
 * 每层栈帧对应一个外层 λ-抽象的绑定变量。
 */
typedef struct LambdaTypingContext {
    TypeSystem *ts;            /**< 类型系统实例 */
    TypeRegion **type_stack;   /**< 类型栈：索引 → 类型的映射 */
    int stack_count;           /**< 栈中条目数 */
    int stack_capacity;        /**< 栈容量 */
} LambdaTypingContext;

/**
 * @brief 初始化 λ-项类型推断上下文
 *
 * @param ctx  上下文指针（由调用者分配）
 * @param ts   类型系统实例
 * @return true 成功，false 失败
 */
lv_PUBLIC_API bool lambda_type_check_init(LambdaTypingContext *ctx, TypeSystem *ts);

/**
 * @brief 销毁 λ-项类型推断上下文
 *
 * 释放栈内存，但不销毁 TypeSystem（由调用者管理）。
 *
 * @param ctx 上下文指针
 */
lv_PUBLIC_API void lambda_type_check_destroy(LambdaTypingContext *ctx);

/**
 * @brief 将一个新类型压入上下文栈
 *
 * 对应 λ-抽象绑定：将 binder 的类型加入上下文。
 *
 * @param ctx  上下文
 * @param type 绑定变量的类型
 * @return true 成功，false 失败
 */
lv_PUBLIC_API bool lambda_type_check_push(LambdaTypingContext *ctx, TypeRegion *type);

/**
 * @brief 从上下文栈中弹出最外层类型
 */
lv_PUBLIC_API void lambda_type_check_pop(LambdaTypingContext *ctx);

/**
 * @brief 推断 λ-项的类型
 *
 * 递归遍历 λ-项，根据 typing rules 推断类型。
 * 失败时返回 NULL，错误信息通过 ctx->ts 的错误机制获取。
 *
 * @param term λ-项
 * @param ctx  类型推断上下文
 * @return 推断得到的 TypeRegion（由 TypeSystem 管理，不可单独销毁），
 *         失败返回 NULL
 */
lv_PUBLIC_API TypeRegion *lambda_type_infer(LvLambdaTerm *term, LambdaTypingContext *ctx);

/**
 * @brief 简化版：创建临时上下文并推断 λ-项类型
 *
 * 适用于不需要自定义 TypeSystem 配置的场景。
 * 自动创建 TypeSystem 并在使用后销毁。
 *
 * @param term λ-项
 * @return 推断得到的 TypeRegion（调用者负责通过 type_region_destroy 释放），
 *         失败返回 NULL
 */
lv_PUBLIC_API TypeRegion *lambda_type_check_and_infer(LvLambdaTerm *term);

#ifdef __cplusplus
}
#endif

#endif /* lv_LAMBDA_TYPE_CHECK_H */
