/**
 * @file three_valued_logic.c
 * @brief 三值逻辑系统实现（子目录版本）
 *
 * 实现 Kleene 强三值逻辑的完整运算，扩展 Lv-00 原有的
 * TRUE/FALSE 二值系统。支持三种真值：
 *   - lv_TRUE   (0)：已证真
 *   - lv_FALSE  (1)：已证伪
 *   - lv_UNKNOWN (2)：未确定
 *
 * 提供基本逻辑运算（AND/OR/NOT/IMPLIES/EQUIV）、
 * 批量归约操作和字符串转换功能。
 */

#include "lv/three_valued_logic.h"

#include <stddef.h>

/* ================================================================
 *  真值表查找表（Kleene 强三值逻辑）
 * ================================================================ */

/**
 * AND 真值表: tvl_and_table[a][b]
 *   AND      | TRUE  FALSE UNKNOWN
 *   ---------+---------------------
 *   TRUE     | TRUE  FALSE UNKNOWN
 *   FALSE    | FALSE FALSE FALSE
 *   UNKNOWN  | UNKNOWN FALSE UNKNOWN
 */
static const lvTruthValue tvl_and_table[3][3] = {
    { lv_TRUE,    lv_FALSE, lv_UNKNOWN },  /* TRUE  AND ... */
    { lv_FALSE,   lv_FALSE, lv_FALSE   },  /* FALSE AND ... */
    { lv_UNKNOWN, lv_FALSE, lv_UNKNOWN }   /* UNK   AND ... */
};

/**
 * OR 真值表: tvl_or_table[a][b]
 *   OR       | TRUE  FALSE UNKNOWN
 *   ---------+---------------------
 *   TRUE     | TRUE  TRUE  TRUE
 *   FALSE    | TRUE  FALSE UNKNOWN
 *   UNKNOWN  | TRUE  UNKNOWN UNKNOWN
 */
static const lvTruthValue tvl_or_table[3][3] = {
    { lv_TRUE,  lv_TRUE,    lv_TRUE    },  /* TRUE  OR ... */
    { lv_TRUE,  lv_FALSE,   lv_UNKNOWN },  /* FALSE OR ... */
    { lv_TRUE,  lv_UNKNOWN, lv_UNKNOWN }   /* UNK   OR ... */
};

/**
 * NOT 真值表: tvl_not_table[v]
 *   NOT(TRUE)    = FALSE
 *   NOT(FALSE)   = TRUE
 *   NOT(UNKNOWN) = UNKNOWN
 */
static const lvTruthValue tvl_not_table[3] = {
    lv_FALSE,   /* NOT TRUE  = FALSE */
    lv_TRUE,    /* NOT FALSE = TRUE  */
    lv_UNKNOWN  /* NOT UNK   = UNK   */
};

/**
 * IMPLIES 真值表: tvl_implies_table[a][b]  (a -> b)
 *   IMPLIES  | TRUE  FALSE UNKNOWN
 *   ---------+---------------------
 *   TRUE     | TRUE  FALSE UNKNOWN
 *   FALSE    | TRUE  TRUE  TRUE
 *   UNKNOWN  | TRUE  UNKNOWN UNKNOWN
 */
static const lvTruthValue tvl_implies_table[3][3] = {
    { lv_TRUE,    lv_FALSE,   lv_UNKNOWN },  /* TRUE  -> ... */
    { lv_TRUE,    lv_TRUE,    lv_TRUE    },  /* FALSE -> ... */
    { lv_TRUE,    lv_UNKNOWN, lv_UNKNOWN }   /* UNK   -> ... */
};

/* ================================================================
 *  内部辅助宏
 * ================================================================ */

/**
 * @brief 验证真值是否合法 (0, 1, 2)
 */
#define TVL_VALID(v) ((unsigned)(v) <= (unsigned)lv_UNKNOWN)

/**
 * @brief 安全索引真值（非法值映射为 UNKNOWN）
 */
#define TVL_IDX(v) (TVL_VALID(v) ? (int)(v) : (int)lv_UNKNOWN)

/* ================================================================
 *  基本逻辑运算
 * ================================================================ */

/**
 * @brief 三值与运算 AND
 */
lvTruthValue lv_tvl_and(lvTruthValue a, lvTruthValue b)
{
    return tvl_and_table[TVL_IDX(a)][TVL_IDX(b)];
}

/**
 * @brief 三值或运算 OR
 */
lvTruthValue lv_tvl_or(lvTruthValue a, lvTruthValue b)
{
    return tvl_or_table[TVL_IDX(a)][TVL_IDX(b)];
}

/**
 * @brief 三值非运算 NOT
 */
lvTruthValue lv_tvl_not(lvTruthValue v)
{
    return tvl_not_table[TVL_IDX(v)];
}

/**
 * @brief 三值蕴涵运算 IMPLIES (A -> B)
 */
lvTruthValue lv_tvl_implies(lvTruthValue a, lvTruthValue b)
{
    return tvl_implies_table[TVL_IDX(a)][TVL_IDX(b)];
}

/**
 * @brief 三值等价运算 EQUIV (A <-> B)
 *
 * 等价于 AND(IMPLIES(a, b), IMPLIES(b, a))
 */
lvTruthValue lv_tvl_equiv(lvTruthValue a, lvTruthValue b)
{
    lvTruthValue ab = lv_tvl_implies(a, b);
    lvTruthValue ba = lv_tvl_implies(b, a);
    return lv_tvl_and(ab, ba);
}

/* ================================================================
 *  批量操作
 * ================================================================ */

/**
 * @brief 三值与运算（数组形式，带短路）
 *
 * 遇到 FALSE 立即返回 FALSE。
 */
lvTruthValue lv_tvl_and_all(const lvTruthValue *values, int count)
{
    lvTruthValue result;
    int i;

    if (!values || count <= 0) {
        return lv_TRUE;  /* 空归约的 AND 单位元为 TRUE */
    }

    result = lv_TRUE;
    for (i = 0; i < count; i++) {
        result = lv_tvl_and(result, values[i]);
        if (result == lv_FALSE) {
            return lv_FALSE;  /* 短路 */
        }
    }
    return result;
}

/**
 * @brief 三值或运算（数组形式，带短路）
 *
 * 遇到 TRUE 立即返回 TRUE。
 */
lvTruthValue lv_tvl_or_all(const lvTruthValue *values, int count)
{
    lvTruthValue result;
    int i;

    if (!values || count <= 0) {
        return lv_FALSE;  /* 空归约的 OR 单位元为 FALSE */
    }

    result = lv_FALSE;
    for (i = 0; i < count; i++) {
        result = lv_tvl_or(result, values[i]);
        if (result == lv_TRUE) {
            return lv_TRUE;  /* 短路 */
        }
    }
    return result;
}

/* ================================================================
 *  字符串转换
 * ================================================================ */

/**
 * @brief 将三值真值转换为英文字符串
 */
const char *lv_tvl_to_string(lvTruthValue v)
{
    switch (v) {
        case lv_TRUE:    return "TRUE";
        case lv_FALSE:   return "FALSE";
        case lv_UNKNOWN: return "UNKNOWN";
        default:           return "INVALID";
    }
}

/**
 * @brief 将三值真值转换为中文字符串
 */
const char *lv_tvl_to_string_zh(lvTruthValue v)
{
    switch (v) {
        case lv_TRUE:    return "\xe7\x9c\x9f";           /* "真" */
        case lv_FALSE:   return "\xe4\xbc\xaa";           /* "伪" */
        case lv_UNKNOWN: return "\xe6\x9c\xaa\xe7\x9f\xa5"; /* "未知" */
        default:           return "\xe6\x97\xa0\xe6\x95\x88"; /* "无效" */
    }
}
