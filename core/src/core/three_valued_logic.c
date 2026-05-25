/**
 * @file three_valued_logic.c
 * @brief 三值逻辑系统实现 —— Kleene 强三值逻辑运算
 *
 * @details 本文件实现 three_valued_logic.h 中声明的所有非 inline 函数，
 *          遵循 Kleene 强三值逻辑语义：
 *          - AND:  任何 FALSE 导致 FALSE，全 TRUE 才 TRUE，否则 UNKNOWN
 *          - OR:   任何 TRUE 导致 TRUE，全 FALSE 才 FALSE，否则 UNKNOWN
 *          - NOT:  取反（UNKNOWN 的否仍是 UNKNOWN）
 *          - IMPLIES:  NOT(A) OR B 的 Kleene 扩展
 *          - EQUIV:   (A->B) AND (B->A) 的 Kleene 扩展
 *
 *          批量操作（and_all / or_all）实现了短路语义：
 *          - and_all: 遇到第一个 FALSE 立即返回
 *          - or_all:  遇到第一个 TRUE 立即返回
 *
 * @author Lv-00 Project
 * @version 3.3.0
 *
 * @dependencies
 *   - three_valued_logic.h : 三值逻辑公共接口定义
 *   - lv00_utils.h         : 统一内存分配器（lv00_malloc / lv00_free）
 */

#include "three_valued_logic.h"
#include "lv00_utils.h"

/* ============== 真值表查找操作 ============== */

/**
 * @brief 三值与运算 AND
 *
 * 根据 Kleene 强三值逻辑真值表实现：
 *   AND      | TRUE  FALSE UNKNOWN
 *   ---------+---------------------
 *   TRUE     | TRUE  FALSE UNKNOWN
 *   FALSE    | FALSE FALSE FALSE
 *   UNKNOWN  | UNKNOWN FALSE UNKNOWN
 *
 * 实现策略：FALSE 为吸收元，任何操作数含 FALSE 则结果为 FALSE；
 *           若无 FALSE 但含 UNKNOWN 则结果为 UNKNOWN；
 *           仅当两者均为 TRUE 时结果为 TRUE。
 *
 * @param a  左操作数真值
 * @param b  右操作数真值
 * @return   a AND b 的真值
 */
Lv00TruthValue lv00_tvl_and(Lv00TruthValue a, Lv00TruthValue b)
{
    /* FALSE 为 AND 的吸收元 */
    if (a == LV00_FALSE || b == LV00_FALSE) {
        return LV00_FALSE;
    }
    /* 若无 FALSE 但含 UNKNOWN，则结果为 UNKNOWN */
    if (a == LV00_UNKNOWN || b == LV00_UNKNOWN) {
        return LV00_UNKNOWN;
    }
    /* 两者均为 TRUE */
    return LV00_TRUE;
}

/**
 * @brief 三值或运算 OR
 *
 * 根据 Kleene 强三值逻辑真值表实现：
 *   OR       | TRUE  FALSE UNKNOWN
 *   ---------+---------------------
 *   TRUE     | TRUE  TRUE  TRUE
 *   FALSE    | TRUE  FALSE UNKNOWN
 *   UNKNOWN  | TRUE  UNKNOWN UNKNOWN
 *
 * 实现策略：TRUE 为吸收元，任何操作数含 TRUE 则结果为 TRUE；
 *           若无 TRUE 但含 UNKNOWN 则结果为 UNKNOWN；
 *           仅当两者均为 FALSE 时结果为 FALSE。
 *
 * @param a  左操作数真值
 * @param b  右操作数真值
 * @return   a OR b 的真值
 */
Lv00TruthValue lv00_tvl_or(Lv00TruthValue a, Lv00TruthValue b)
{
    /* TRUE 为 OR 的吸收元 */
    if (a == LV00_TRUE || b == LV00_TRUE) {
        return LV00_TRUE;
    }
    /* 若无 TRUE 但含 UNKNOWN，则结果为 UNKNOWN */
    if (a == LV00_UNKNOWN || b == LV00_UNKNOWN) {
        return LV00_UNKNOWN;
    }
    /* 两者均为 FALSE */
    return LV00_FALSE;
}

/**
 * @brief 三值非运算 NOT
 *
 * 根据 Kleene 强三值逻辑真值表实现：
 *   NOT(TRUE)    = FALSE
 *   NOT(FALSE)   = TRUE
 *   NOT(UNKNOWN) = UNKNOWN
 *
 * 在 Kleene 语义下，UNKNOWN 的否定仍为 UNKNOWN，
 * 这与经典二值逻辑中排中律（A OR NOT(A) = TRUE）不同：
 *   UNKNOWN OR NOT(UNKNOWN) = UNKNOWN OR UNKNOWN = UNKNOWN
 * 这反映了"未知命题的真假尚未确定"这一直觉。
 *
 * @param v  操作数真值
 * @return   NOT v 的真值
 */
Lv00TruthValue lv00_tvl_not(Lv00TruthValue v)
{
    switch (v) {
    case LV00_TRUE:
        return LV00_FALSE;
    case LV00_FALSE:
        return LV00_TRUE;
    case LV00_UNKNOWN:
    default:
        return LV00_UNKNOWN;
    }
}

/**
 * @brief 三值蕴涵运算 IMPLIES (A -> B)
 *
 * 等价于 lv00_tvl_or(lv00_tvl_not(a), b)，真值表：
 *   IMPLIES  | TRUE  FALSE UNKNOWN
 *   ---------+---------------------
 *   TRUE     | TRUE  FALSE UNKNOWN
 *   FALSE    | TRUE  TRUE  TRUE
 *   UNKNOWN  | TRUE  UNKNOWN UNKNOWN
 *
 * 物理含义：
 * - 当前提已证真(a=TRUE)而结论已证伪(b=FALSE)时，蕴涵失败（FALSE）
 * - 当前提已证伪(a=FALSE)时，蕴涵平凡成立（爆炸原理），结果为 TRUE
 * - 当结论已证真(b=TRUE)时，无论前提如何蕴涵均成立，结果为 TRUE
 * - 当前提为 UNKNOWN 且结论为 UNKNOWN 或 FALSE 时，结果为 UNKNOWN
 *
 * @param a  前提真值
 * @param b  结论真值
 * @return   a -> b 的真值
 */
Lv00TruthValue lv00_tvl_implies(Lv00TruthValue a, Lv00TruthValue b)
{
    /* 前提为假时，蕴涵平凡成立（爆炸原理） */
    if (a == LV00_FALSE) {
        return LV00_TRUE;
    }
    /* 结论为真时，蕴涵成立 */
    if (b == LV00_TRUE) {
        return LV00_TRUE;
    }
    /* 前提为真且结论为假：蕴涵失败 */
    if (a == LV00_TRUE && b == LV00_FALSE) {
        return LV00_FALSE;
    }
    /* 其余情况（含 UNKNOWN）：结果为 UNKNOWN */
    return LV00_UNKNOWN;
}

/**
 * @brief 三值等价运算 EQUIV (A <-> B)
 *
 * 等价于 lv00_tvl_and(lv00_tvl_implies(a, b), lv00_tvl_implies(b, a))，
 * 真值表：
 *   EQUIV    | TRUE  FALSE UNKNOWN
 *   ---------+---------------------
 *   TRUE     | TRUE  FALSE UNKNOWN
 *   FALSE    | FALSE TRUE  UNKNOWN
 *   UNKNOWN  | UNKNOWN UNKNOWN UNKNOWN
 *
 * 直觉含义：
 * - 两个已确定真值相同时，等价成立（TRUE）
 * - 两个已确定真值不同时，等价不成立（FALSE）
 * - 任一为 UNKNOWN 时，等价关系无法确定（UNKNOWN）
 *
 * @param a  左操作数真值
 * @param b  右操作数真值
 * @return   a <-> b 的真值
 */
Lv00TruthValue lv00_tvl_equiv(Lv00TruthValue a, Lv00TruthValue b)
{
    /* 两者均为 UNKNOWN 时，等价关系无法确定 */
    if (a == LV00_UNKNOWN || b == LV00_UNKNOWN) {
        return LV00_UNKNOWN;
    }
    /* 两者已确定且相等 */
    if (a == b) {
        return LV00_TRUE;
    }
    /* 两者已确定但不等 */
    return LV00_FALSE;
}

/* ============== 批量操作 ============== */

/**
 * @brief 三值与运算（数组形式）
 *
 * 对真值数组中的所有元素执行 AND 归约。
 * 短路语义：遇到任一 FALSE 立即返回 FALSE，无需遍历剩余元素。
 *
 * 算法流程：
 * 1. 若数组为空（count <= 0），返回 TRUE（空合取的恒等元）
 * 2. 遍历数组，遇到 FALSE 立即短路返回
 * 3. 若遍历完成且发现 UNKNOWN，返回 UNKNOWN
 * 4. 所有元素均为 TRUE，返回 TRUE
 *
 * @param values  真值数组（不可为 NULL，除非 count <= 0）
 * @param count   数组长度（若 <= 0 视为空数组）
 * @return        所有元素 AND 归约后的真值
 */
Lv00TruthValue lv00_tvl_and_all(const Lv00TruthValue *values, int count)
{
    Lv00TruthValue result = LV00_TRUE;
    int i;

    /* 空数组的 AND 归约为 TRUE（合取的恒等元） */
    if (count <= 0 || values == NULL) {
        return LV00_TRUE;
    }

    for (i = 0; i < count; i++) {
        /* 短路：遇到 FALSE 立即返回 */
        if (values[i] == LV00_FALSE) {
            return LV00_FALSE;
        }
        /* 记录是否出现 UNKNOWN */
        if (values[i] == LV00_UNKNOWN) {
            result = LV00_UNKNOWN;
        }
    }

    return result;
}

/**
 * @brief 三值或运算（数组形式）
 *
 * 对真值数组中的所有元素执行 OR 归约。
 * 短路语义：遇到任一 TRUE 立即返回 TRUE，无需遍历剩余元素。
 *
 * 算法流程：
 * 1. 若数组为空（count <= 0），返回 FALSE（空析取的恒等元）
 * 2. 遍历数组，遇到 TRUE 立即短路返回
 * 3. 若遍历完成且发现 UNKNOWN，返回 UNKNOWN
 * 4. 所有元素均为 FALSE，返回 FALSE
 *
 * @param values  真值数组（不可为 NULL，除非 count <= 0）
 * @param count   数组长度（若 <= 0 视为空数组）
 * @return        所有元素 OR 归约后的真值
 */
Lv00TruthValue lv00_tvl_or_all(const Lv00TruthValue *values, int count)
{
    Lv00TruthValue result = LV00_FALSE;
    int i;

    /* 空数组的 OR 归约为 FALSE（析取的恒等元） */
    if (count <= 0 || values == NULL) {
        return LV00_FALSE;
    }

    for (i = 0; i < count; i++) {
        /* 短路：遇到 TRUE 立即返回 */
        if (values[i] == LV00_TRUE) {
            return LV00_TRUE;
        }
        /* 记录是否出现 UNKNOWN */
        if (values[i] == LV00_UNKNOWN) {
            result = LV00_UNKNOWN;
        }
    }

    return result;
}

/* ============== 字符串转换 ============== */

/**
 * @brief 将三值真值转换为人类可读字符串（英文）
 *
 * 返回静态字符串常量，调用者无需（也不应）释放返回的指针。
 * 适用于日志输出、调试信息等场景。
 *
 * @param v  真值
 * @return   静态字符串 "TRUE" / "FALSE" / "UNKNOWN"
 */
const char *lv00_tvl_to_string(Lv00TruthValue v)
{
    switch (v) {
    case LV00_TRUE:
        return "TRUE";
    case LV00_FALSE:
        return "FALSE";
    case LV00_UNKNOWN:
    default:
        return "UNKNOWN";
    }
}

/**
 * @brief 将三值真值转换为中文可读字符串
 *
 * 返回静态字符串常量，调用者无需（也不应）释放返回的指针。
 * 适用于面向中文用户的日志输出、证明报告、错误提示等场景。
 *
 * 映射关系：
 * - LV00_TRUE    -> "真"   （已证构造性为真）
 * - LV00_FALSE   -> "伪"   （有反例或矛盾，已证伪）
 * - LV00_UNKNOWN -> "未知" （尚未确定真假）
 *
 * @param v  真值
 * @return   静态字符串 "真" / "伪" / "未知"
 */
const char *lv00_tvl_to_string_zh(Lv00TruthValue v)
{
    switch (v) {
    case LV00_TRUE:
        return "真";
    case LV00_FALSE:
        return "伪";
    case LV00_UNKNOWN:
    default:
        return "未知";
    }
}
