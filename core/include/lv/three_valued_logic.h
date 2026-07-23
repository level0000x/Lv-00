/* ========================================================================
 * 模块名称：三值逻辑系统 (three_valued_logic)
 * 功能概述：引入 Kleene 强三值逻辑，扩展 Lv-00 原有的 TRUE/FALSE 二值
 *          系统。支持三种真值：lv_TRUE（已证真）、lv_FALSE（已证伪）、
 *          lv_UNKNOWN（未确定）。提供完整的真值表运算（AND/OR/NOT/
 *          IMPLIES/EQUIV）、批量归约操作和二值/三值转换。
 *          应用于证明子目标未解决时的真值标注和无限域量词评估。
 *
 * 主要 API：
 *   - lv_tvl_and / or / not / implies / equiv  — 基本逻辑运算
 *   - lv_tvl_and_all / or_all                   — 批量归约
 *   - lv_tvl_is_known / is_true / is_false      — 判定辅助
 *   - lv_tvl_to_bool_conservative / optimistic  — 转布尔值
 *   - lv_tvl_to_string / to_string_zh           — 字符串转换
 *   - lv_tvl_from_bool                          — 布尔转三值
 *
 * 使用示例：
 *   lvTruthValue a = lv_TRUE, b = lv_UNKNOWN;
 *   lvTruthValue r = lv_tvl_and(a, b);  // lv_UNKNOWN
 *   bool known = lv_tvl_is_known(r);       // false
 *
 * @version 1.1.0
 * ======================================================================== */

/**
 * @file three_valued_logic.h
 * @brief 三值逻辑系统 —— 真、假、未知三态推理
 */

#ifndef lv_THREE_VALUED_LOGIC_H
#define lv_THREE_VALUED_LOGIC_H

#include <stdbool.h>
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============== 三值逻辑真值枚举 ============== */

/**
 * @brief 三值逻辑真值
 *
 * lv_TRUE   = 真（已证构造性）
 * lv_FALSE  = 伪（有反例/矛盾）
 * lv_UNKNOWN = 未知（未确定）
 */
typedef enum {
    lv_TRUE    = 0,
    lv_FALSE   = 1,
    lv_UNKNOWN = 2
} lvTruthValue;

/* ============== 真值表查找操作 ============== */

/**
 * @brief 三值与运算 AND
 *
 * Kleene 强三值逻辑真值表：
 *   AND      | TRUE  FALSE UNKNOWN
 *   ---------+---------------------
 *   TRUE     | TRUE  FALSE UNKNOWN
 *   FALSE    | FALSE FALSE FALSE
 *   UNKNOWN  | UNKNOWN FALSE UNKNOWN
 *
 * @param a  左操作数真值
 * @param b  右操作数真值
 * @return   a AND b 的真值
 */
lv_PUBLIC_API lvTruthValue lv_tvl_and(lvTruthValue a, lvTruthValue b);

/**
 * @brief 三值或运算 OR
 *
 * Kleene 强三值逻辑真值表：
 *   OR       | TRUE  FALSE UNKNOWN
 *   ---------+---------------------
 *   TRUE     | TRUE  TRUE  TRUE
 *   FALSE    | TRUE  FALSE UNKNOWN
 *   UNKNOWN  | TRUE  UNKNOWN UNKNOWN
 *
 * @param a  左操作数真值
 * @param b  右操作数真值
 * @return   a OR b 的真值
 */
lv_PUBLIC_API lvTruthValue lv_tvl_or(lvTruthValue a, lvTruthValue b);

/**
 * @brief 三值非运算 NOT
 *
 * Kleene 强三值逻辑真值表：
 *   NOT(TRUE)    = FALSE
 *   NOT(FALSE)   = TRUE
 *   NOT(UNKNOWN) = UNKNOWN
 *
 * @param v  操作数真值
 * @return   NOT v 的真值
 */
lv_PUBLIC_API lvTruthValue lv_tvl_not(lvTruthValue v);

/**
 * @brief 三值蕴涵运算 IMPLIES (A → B)
 *
 * 等价于 lv_tvl_or(lv_tvl_not(a), b)，真值表：
 *   IMPLIES  | TRUE  FALSE UNKNOWN
 *   ---------+---------------------
 *   TRUE     | TRUE  FALSE UNKNOWN
 *   FALSE    | TRUE  TRUE  TRUE
 *   UNKNOWN  | TRUE  UNKNOWN UNKNOWN
 *
 * 物理含义：
 * - 当前提已证真(a=TRUE)而结论已证伪(b=FALSE)时，蕴涵失败
 * - 当前提已证伪(a=FALSE)时，蕴涵平凡成立（爆炸原理）
 * - 当任一项为未知时，除非前提为假，否则结果未知
 *
 * @param a  前提真值
 * @param b  结论真值
 * @return   a → b 的真值
 */
lv_PUBLIC_API lvTruthValue lv_tvl_implies(lvTruthValue a, lvTruthValue b);

/**
 * @brief 三值等价运算 EQUIV (A ↔ B)
 *
 * 等价于 lv_tvl_and(lv_tvl_implies(a, b), lv_tvl_implies(b, a))
 *
 * @param a  左操作数真值
 * @param b  右操作数真值
 * @return   a ↔ b 的真值
 */
lv_PUBLIC_API lvTruthValue lv_tvl_equiv(lvTruthValue a, lvTruthValue b);

/* ============== 批量操作 ============== */

/**
 * @brief 三值与运算（数组形式）
 *
 * 对真值数组中的所有元素执行 AND 归约。
 * 短路语义：遇到任一 FALSE 立即返回 FALSE。
 *
 * @param values  真值数组
 * @param count   数组长度
 * @return        所有元素 AND 归约后的真值
 */
lv_PUBLIC_API lvTruthValue lv_tvl_and_all(const lvTruthValue *values, int count);

/**
 * @brief 三值或运算（数组形式）
 *
 * 对真值数组中的所有元素执行 OR 归约。
 * 短路语义：遇到任一 TRUE 立即返回 TRUE。
 *
 * @param values  真值数组
 * @param count   数组长度
 * @return        所有元素 OR 归约后的真值
 */
lv_PUBLIC_API lvTruthValue lv_tvl_or_all(const lvTruthValue *values, int count);

/* ============== 判定辅助函数 ============== */

/**
 * @brief 判断真值是否已确定（非 UNKNOWN）
 *
 * @param v  真值
 * @return   true 如果 v 是 TRUE 或 FALSE
 */
static inline bool lv_tvl_is_known(lvTruthValue v) {
    return v != lv_UNKNOWN;
}

/**
 * @brief 判断真值是否为 TRUE
 *
 * @param v  真值
 * @return   true 如果 v == lv_TRUE
 */
static inline bool lv_tvl_is_true(lvTruthValue v) {
    return v == lv_TRUE;
}

/**
 * @brief 判断真值是否为 FALSE
 *
 * @param v  真值
 * @return   true 如果 v == lv_FALSE
 */
static inline bool lv_tvl_is_false(lvTruthValue v) {
    return v == lv_FALSE;
}

/**
 * @brief 将三值真值转换为布尔值（保守策略）
 *
 * 只有已知为真才返回 true，FALSE 和 UNKNOWN 都返回 false。
 * 适用于需要保守判断的场景（如"是否可以确定使用此引理？"）。
 *
 * @param v  真值
 * @return   true 仅当 v == lv_TRUE
 */
static inline bool lv_tvl_to_bool_conservative(lvTruthValue v) {
    return v == lv_TRUE;
}

/**
 * @brief 将三值真值转换为布尔值（乐观策略）
 *
 * 只要未被证伪就返回 true，即 TRUE 和 UNKNOWN 都视为 true。
 * 适用于需要乐观判断的场景（如"此方向是否至少可能成功？"）。
 *
 * @param v  真值
 * @return   true 当 v != lv_FALSE
 */
static inline bool lv_tvl_to_bool_optimistic(lvTruthValue v) {
    return v != lv_FALSE;
}

/* ============== 字符串转换 ============== */

/**
 * @brief 将三值真值转换为人类可读字符串
 *
 * @param v  真值
 * @return   静态字符串（"TRUE" / "FALSE" / "UNKNOWN"），请勿释放
 */
lv_PUBLIC_API const char *lv_tvl_to_string(lvTruthValue v);

/**
 * @brief 将三值真值转换为中文可读字符串
 *
 * @param v  真值
 * @return   静态字符串（"真" / "伪" / "未知"），请勿释放
 */
lv_PUBLIC_API const char *lv_tvl_to_string_zh(lvTruthValue v);

/* ============== 二值/三值转换 ============== */

/**
 * @brief 将布尔值转为三值真值
 *
 * @param b  true → lv_TRUE, false → lv_FALSE
 * @return   对应的三值真值
 */
static inline lvTruthValue lv_tvl_from_bool(bool b) {
    return b ? lv_TRUE : lv_FALSE;
}

#ifdef __cplusplus
}
#endif

#endif /* lv_THREE_VALUED_LOGIC_H */
