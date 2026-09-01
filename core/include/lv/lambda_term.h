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
 *
 * 编码约定（与 lambda_to_graph 的 scope_lookup 一致）：
 *   - Var.index 为标准 De Bruijn 相对索引：0 = 最近（最内层）binder。
 *   - Abs.binder 恒为 0 占位，不参与求值。
 *   - 开放项中的自由变量直接用超出当前作用域深度的 Var.index 表示。
 */
typedef struct LvLambdaTerm {
    LvLambdaTermType type; /**< 节点类型（决定 data 联合体的活跃成员） */
    union {
        /** @brief 变量数据 */
        struct {
            int index; /**< De Bruijn 索引（0 = 最近 binder） */
        } var;

        /** @brief 抽象数据 */
        struct {
            int binder;                /**< 绑定变量索引（恒为 0 占位） */
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
 * @brief 深拷贝 λ-项（[copy] 语义，memory-ownership.md K10/F39）
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
 * @return 字符串表示（[take] 调用者负责释放），失败返回 NULL
 */
lv_PUBLIC_API char *lv_lambda_to_string(LvLambdaTerm *term);

/* ===========================================================================
 * β-归约求值器
 *
 * 编码约定（与 lambda_to_graph 的 scope_lookup 一致）：
 *   - VAR.index 为标准 De Bruijn 相对索引：0 = 最近（最内层）binder。
 *   - ABS.binder 恒为 0 占位，不参与求值。
 *   - 开放项中的自由变量直接用超出当前作用域深度的 VAR.index 表示。
 *
 * 求值策略：规范序（最左最外）β-归约到规范形（normal form），基于标准
 * TAPL 的 De Bruijn shift/subst（替换时以 shift 重定位变量索引）+ 迭代
 * 单步 β-归约。返回新分配的项（调用者负责 lv_lambda_destroy），与原项
 * 完全独立（深拷贝式求值）。
 * =========================================================================== */

/**
 * @brief β-归约求值器的默认步数上限
 *
 * 规范序求值可对非终止项（如 Y 组合子）无限展开，求值器在 β 步数超过
 * 上限时安全终止并返回 NULL。默认 10000（与 test_lambda_church 的
 * Y 组合子 10000 步上限一致），可用 lv_lambda_eval_set_max_steps 调整。
 */
#define LV_LAMBDA_EVAL_DEFAULT_MAX_STEPS 10000

/**
 * @brief 规范序求值到规范形
 *
 * - 闭合项：返回其规范形（normal form）的深拷贝。
 * - 开放项：自由变量（无对应 binder 的 VAR）原样保留，不报错。
 * - 非终止项（如 Y 组合子应用到实参）：β 步数超过上限时安全终止，
 *   返回 NULL（调用者视为"超限/失败"）。
 *
 * 内存所有权：返回值是新分配的独立项，调用者负责 lv_lambda_destroy；
 * 输入 term 不被修改、不被释放。
 *
 * @param term 待求值的 λ-项（可为 NULL，返回 NULL）
 * @return 规范形的深拷贝；超限/内存不足返回 NULL
 */
lv_PUBLIC_API LvLambdaTerm *lv_lambda_eval(LvLambdaTerm *term);

/**
 * @brief 完整求值到规范形
 *
 * 当前求值策略下 lv_lambda_eval 已直接返回规范形，本函数是其语义别名
 * （对结果再做一次不动点求值，保证在任何未来求值策略调整下语义不变）。
 *
 * @param term 待求值的 λ-项（可为 NULL，返回 NULL）
 * @return 规范形的深拷贝；超限/内存不足返回 NULL
 */
lv_PUBLIC_API LvLambdaTerm *lv_lambda_eval_full(LvLambdaTerm *term);

/**
 * @brief 返回对 term 完整求值消耗的 β-归约步数
 *
 * 内部执行一次完整求值（与 lv_lambda_eval 相同的策略与上限），丢弃结果，
 * 仅返回统计的 β 步数。超限时返回达到的步数（即当前上限值）。
 *
 * @param term 待统计的 λ-项（可为 NULL，返回 0）
 * @return β-归约步数（≥ 0）
 */
lv_PUBLIC_API int lv_lambda_eval_steps(LvLambdaTerm *term);

/**
 * @brief 设置 β-归约求值器的步数上限
 *
 * @param max_steps 新上限；小于等于 0 时恢复默认值
 *                  LV_LAMBDA_EVAL_DEFAULT_MAX_STEPS（10000）
 */
lv_PUBLIC_API void lv_lambda_eval_set_max_steps(int max_steps);

#ifdef __cplusplus
}
#endif

#endif /* lv_LAMBDA_TERM_H */
