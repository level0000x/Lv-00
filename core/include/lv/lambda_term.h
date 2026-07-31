/**
 * @file lambda_term.h
 * @brief λ-项数据结构定义
 *
 * 定义了 λ-演算的抽象语法树（AST）数据结构，包括变量（Var）、
 * 抽象（Abs）和应用（App）三种节点类型。
 * 使用 tagged union 实现类型安全的变体表示。
 */

#ifndef lv_LAMBDA_TERM_H
#define lv_LAMBDA_TERM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifndef lv_PUBLIC_API
#define lv_PUBLIC_API
#endif

/**
 * @brief λ-项节点类型枚举
 */
typedef enum {
    LV_LAMBDA_VAR, /**< @brief 变量节点（De Bruijn 索引） */
    LV_LAMBDA_ABS, /**< @brief 抽象节点（λ 绑定） */
    LV_LAMBDA_APP  /**< @brief 应用节点 */
} LvLambdaTermType;

/**
 * @brief λ-项节点结构（tagged union）
 *
 * 使用 tagged union 表示 λ-演算的三种节点类型：
 * - Var:   变量，通过 De Bruijn 索引引用绑定变量
 * - Abs:   抽象（λ 绑定），包含绑定变量索引和体子项
 * - App:   应用，包含左右两个子项
 */
typedef struct LvLambdaTerm {
    LvLambdaTermType type; /**< 节点类型（决定 data 联合体的活跃成员） */
    union {
        /** @brief 变量数据 */
        struct {
            int index; /**< De Bruijn 索引 */
        } var;

        /** @brief 抽象数据 */
        struct {
            int binder;                /**< 绑定变量索引 */
            struct LvLambdaTerm *body; /**< 体子项（生命周期由本节点管理） */
        } abs;

        /** @brief 应用数据 */
        struct {
            struct LvLambdaTerm *left;  /**< 左子项（生命周期由本节点管理） */
            struct LvLambdaTerm *right; /**< 右子项（生命周期由本节点管理） */
        } app;
    } data;
} LvLambdaTerm;

/**
 * @brief 创建变量项
 *
 * @param index De Bruijn 索引
 * @return 新分配的变量项指针，失败返回 NULL
 */
lv_PUBLIC_API LvLambdaTerm *lv_lambda_create_var(int index);

/**
 * @brief 创建抽象项
 *
 * 接过 body 的所有权。调用后 body 由返回的项管理，调用者不应再直接操作 body。
 *
 * @param binder 绑定变量索引
 * @param body   体子项指针（函数接过所有权）
 * @return 新分配的抽象项指针，失败返回 NULL
 */
lv_PUBLIC_API LvLambdaTerm *lv_lambda_create_abs(int binder, LvLambdaTerm *body);

/**
 * @brief 创建应用项
 *
 * 接过 left 和 right 的所有权。调用后 left/right 由返回的项管理。
 *
 * @param left  左子项指针（函数接过所有权）
 * @param right 右子项指针（函数接过所有权）
 * @return 新分配的应用项指针，失败返回 NULL
 */
lv_PUBLIC_API LvLambdaTerm *lv_lambda_create_app(LvLambdaTerm *left, LvLambdaTerm *right);

/**
 * @brief 递归销毁 λ-项
 *
 * 递归释放所有子项，然后释放自身。
 * 传入 NULL 时安全无操作（no-op）。
 *
 * @param term 待销毁的 λ-项指针（可为 NULL）
 */
lv_PUBLIC_API void lv_lambda_destroy(LvLambdaTerm *term);

/**
 * @brief 深拷贝 λ-项
 *
 * 递归复制所有子项，返回完全独立的副本。
 *
 * @param term 待复制的 λ-项指针（不可为 NULL）
 * @return 深拷贝后的新项指针，失败返回 NULL
 */
lv_PUBLIC_API LvLambdaTerm *lv_lambda_copy(LvLambdaTerm *term);

/**
 * @brief 将 λ-项打印为字符串
 *
 * 生成 λ-表达式的字符串表示。返回的字符串由 lv_malloc 分配，
 * 调用者通过 lv_free 释放。
 *
 * @param term λ-项指针
 * @return 字符串表示（调用者负责释放），失败返回 NULL
 */
lv_PUBLIC_API char *lv_lambda_to_string(LvLambdaTerm *term);

#ifdef __cplusplus
}
#endif

#endif /* lv_LAMBDA_TERM_H */
