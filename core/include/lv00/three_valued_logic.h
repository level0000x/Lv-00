/* ========================================================================
 * 模块名称：三值逻辑系统 (three_valued_logic)
 * 功能概述：引入 Kleene 强三值逻辑，扩展 Lv-00 原有的 TRUE/FALSE 二值
 *          系统。支持三种真值：LV00_TRUE（已证真）、LV00_FALSE（已证伪）、
 *          LV00_UNKNOWN（未确定）。提供完整的真值表运算（AND/OR/NOT/
 *          IMPLIES/EQUIV）、批量归约操作和二值/三值转换。
 *          应用于证明子目标未解决时的真值标注和无限域量词评估。
 *
 * 主要 API：
 *   - lv00_tvl_and / or / not / implies / equiv  — 基本逻辑运算
 *   - lv00_tvl_and_all / or_all                   — 批量归约
 *   - lv00_tvl_is_known / is_true / is_false      — 判定辅助
 *   - lv00_tvl_to_bool_conservative / optimistic  — 转布尔值
 *   - lv00_tvl_to_string / to_string_zh           — 字符串转换
 *   - lv00_tvl_from_bool                          — 布尔转三值
 *
 * 使用示例：
 *   Lv00TruthValue a = LV00_TRUE, b = LV00_UNKNOWN;
 *   Lv00TruthValue r = lv00_tvl_and(a, b);  // LV00_UNKNOWN
 *   bool known = lv00_tvl_is_known(r);       // false
 *
 * @version 1.0.0
 * ======================================================================== */

/**
 * @file three_valued_logic.h
 * @brief 三值逻辑系统 —— 真、假、未知三态推理
 */

#ifndef LV00_THREE_VALUED_LOGIC_H
#define LV00_THREE_VALUED_LOGIC_H

#include <stdbool.h>
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============== 三值逻辑真值枚举 ============== */

/**
 * @brief 三值逻辑真值
 *
 * LV00_TRUE   = 真（已证构造性）
 * LV00_FALSE  = 伪（有反例/矛盾）
 * LV00_UNKNOWN = 未知（未确定）
 */
typedef enum {
    LV00_TRUE    = 0,
    LV00_FALSE   = 1,
    LV00_UNKNOWN = 2
} Lv00TruthValue;

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
LV00_PUBLIC_API Lv00TruthValue lv00_tvl_and(Lv00TruthValue a, Lv00TruthValue b);

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
LV00_PUBLIC_API Lv00TruthValue lv00_tvl_or(Lv00TruthValue a, Lv00TruthValue b);

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
LV00_PUBLIC_API Lv00TruthValue lv00_tvl_not(Lv00TruthValue v);

/**
 * @brief 三值蕴涵运算 IMPLIES (A → B)
 *
 * 等价于 lv00_tvl_or(lv00_tvl_not(a), b)，真值表：
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
LV00_PUBLIC_API Lv00TruthValue lv00_tvl_implies(Lv00TruthValue a, Lv00TruthValue b);

/**
 * @brief 三值等价运算 EQUIV (A ↔ B)
 *
 * 等价于 lv00_tvl_and(lv00_tvl_implies(a, b), lv00_tvl_implies(b, a))
 *
 * @param a  左操作数真值
 * @param b  右操作数真值
 * @return   a ↔ b 的真值
 */
LV00_PUBLIC_API Lv00TruthValue lv00_tvl_equiv(Lv00TruthValue a, Lv00TruthValue b);

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
LV00_PUBLIC_API Lv00TruthValue lv00_tvl_and_all(const Lv00TruthValue *values, int count);

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
LV00_PUBLIC_API Lv00TruthValue lv00_tvl_or_all(const Lv00TruthValue *values, int count);

/* ============== 判定辅助函数 ============== */

/**
 * @brief 判断真值是否已确定（非 UNKNOWN）
 *
 * @param v  真值
 * @return   true 如果 v 是 TRUE 或 FALSE
 */
static inline bool lv00_tvl_is_known(Lv00TruthValue v) {
    return v != LV00_UNKNOWN;
}

/**
 * @brief 判断真值是否为 TRUE
 *
 * @param v  真值
 * @return   true 如果 v == LV00_TRUE
 */
static inline bool lv00_tvl_is_true(Lv00TruthValue v) {
    return v == LV00_TRUE;
}

/**
 * @brief 判断真值是否为 FALSE
 *
 * @param v  真值
 * @return   true 如果 v == LV00_FALSE
 */
static inline bool lv00_tvl_is_false(Lv00TruthValue v) {
    return v == LV00_FALSE;
}

/**
 * @brief 将三值真值转换为布尔值（保守策略）
 *
 * 只有已知为真才返回 true，FALSE 和 UNKNOWN 都返回 false。
 * 适用于需要保守判断的场景（如"是否可以确定使用此引理？"）。
 *
 * @param v  真值
 * @return   true 仅当 v == LV00_TRUE
 */
static inline bool lv00_tvl_to_bool_conservative(Lv00TruthValue v) {
    return v == LV00_TRUE;
}

/**
 * @brief 将三值真值转换为布尔值（乐观策略）
 *
 * 只要未被证伪就返回 true，即 TRUE 和 UNKNOWN 都视为 true。
 * 适用于需要乐观判断的场景（如"此方向是否至少可能成功？"）。
 *
 * @param v  真值
 * @return   true 当 v != LV00_FALSE
 */
static inline bool lv00_tvl_to_bool_optimistic(Lv00TruthValue v) {
    return v != LV00_FALSE;
}

/* ============== 字符串转换 ============== */

/**
 * @brief 将三值真值转换为人类可读字符串
 *
 * @param v  真值
 * @return   静态字符串（"TRUE" / "FALSE" / "UNKNOWN"），请勿释放
 */
LV00_PUBLIC_API const char *lv00_tvl_to_string(Lv00TruthValue v);

/**
 * @brief 将三值真值转换为中文可读字符串
 *
 * @param v  真值
 * @return   静态字符串（"真" / "伪" / "未知"），请勿释放
 */
LV00_PUBLIC_API const char *lv00_tvl_to_string_zh(Lv00TruthValue v);

/* ============== 二值/三值转换 ============== */

/**
 * @brief 将布尔值转为三值真值
 *
 * @param b  true → LV00_TRUE, false → LV00_FALSE
 * @return   对应的三值真值
 */
static inline Lv00TruthValue lv00_tvl_from_bool(bool b) {
    return b ? LV00_TRUE : LV00_FALSE;
}

#ifdef __cplusplus
}
#endif

#endif /* LV00_THREE_VALUED_LOGIC_H */
