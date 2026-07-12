/**
 * @file three_valued_logic.c
 * @brief 三值逻辑系统实现（子目录版本）
 *
 * 实现 Kleene 强三值逻辑的完整运算，扩展 Lv-00 原有的
 * TRUE/FALSE 二值系统。支持三种真值：
 *   - LV00_TRUE   (0)：已证真
 *   - LV00_FALSE  (1)：已证伪
 *   - LV00_UNKNOWN (2)：未确定
 *
 * 提供基本逻辑运算（AND/OR/NOT/IMPLIES/EQUIV）、
 * 批量归约操作和字符串转换功能。
 */

#include "lv00/three_valued_logic.h"

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
static const Lv00TruthValue tvl_and_table[3][3] = {
    { LV00_TRUE,    LV00_FALSE, LV00_UNKNOWN },  /* TRUE  AND ... */
    { LV00_FALSE,   LV00_FALSE, LV00_FALSE   },  /* FALSE AND ... */
    { LV00_UNKNOWN, LV00_FALSE, LV00_UNKNOWN }   /* UNK   AND ... */
};

/**
 * OR 真值表: tvl_or_table[a][b]
 *   OR       | TRUE  FALSE UNKNOWN
 *   ---------+---------------------
 *   TRUE     | TRUE  TRUE  TRUE
 *   FALSE    | TRUE  FALSE UNKNOWN
 *   UNKNOWN  | TRUE  UNKNOWN UNKNOWN
 */
static const Lv00TruthValue tvl_or_table[3][3] = {
    { LV00_TRUE,  LV00_TRUE,    LV00_TRUE    },  /* TRUE  OR ... */
    { LV00_TRUE,  LV00_FALSE,   LV00_UNKNOWN },  /* FALSE OR ... */
    { LV00_TRUE,  LV00_UNKNOWN, LV00_UNKNOWN }   /* UNK   OR ... */
};

/**
 * NOT 真值表: tvl_not_table[v]
 *   NOT(TRUE)    = FALSE
 *   NOT(FALSE)   = TRUE
 *   NOT(UNKNOWN) = UNKNOWN
 */
static const Lv00TruthValue tvl_not_table[3] = {
    LV00_FALSE,   /* NOT TRUE  = FALSE */
    LV00_TRUE,    /* NOT FALSE = TRUE  */
    LV00_UNKNOWN  /* NOT UNK   = UNK   */
};

/**
 * IMPLIES 真值表: tvl_implies_table[a][b]  (a -> b)
 *   IMPLIES  | TRUE  FALSE UNKNOWN
 *   ---------+---------------------
 *   TRUE     | TRUE  FALSE UNKNOWN
 *   FALSE    | TRUE  TRUE  TRUE
 *   UNKNOWN  | TRUE  UNKNOWN UNKNOWN
 */
static const Lv00TruthValue tvl_implies_table[3][3] = {
    { LV00_TRUE,    LV00_FALSE,   LV00_UNKNOWN },  /* TRUE  -> ... */
    { LV00_TRUE,    LV00_TRUE,    LV00_TRUE    },  /* FALSE -> ... */
    { LV00_TRUE,    LV00_UNKNOWN, LV00_UNKNOWN }   /* UNK   -> ... */
};

/* ================================================================
 *  内部辅助宏
 * ================================================================ */

/**
 * @brief 验证真值是否合法 (0, 1, 2)
 */
#define TVL_VALID(v) ((unsigned)(v) <= (unsigned)LV00_UNKNOWN)

/**
 * @brief 安全索引真值（非法值映射为 UNKNOWN）
 */
#define TVL_IDX(v) (TVL_VALID(v) ? (int)(v) : (int)LV00_UNKNOWN)

/* ================================================================
 *  基本逻辑运算
 * ================================================================ */

/**
 * @brief 三值与运算 AND
 */
Lv00TruthValue lv00_tvl_and(Lv00TruthValue a, Lv00TruthValue b)
{
    return tvl_and_table[TVL_IDX(a)][TVL_IDX(b)];
}

/**
 * @brief 三值或运算 OR
 */
Lv00TruthValue lv00_tvl_or(Lv00TruthValue a, Lv00TruthValue b)
{
    return tvl_or_table[TVL_IDX(a)][TVL_IDX(b)];
}

/**
 * @brief 三值非运算 NOT
 */
Lv00TruthValue lv00_tvl_not(Lv00TruthValue v)
{
    return tvl_not_table[TVL_IDX(v)];
}

/**
 * @brief 三值蕴涵运算 IMPLIES (A -> B)
 */
Lv00TruthValue lv00_tvl_implies(Lv00TruthValue a, Lv00TruthValue b)
{
    return tvl_implies_table[TVL_IDX(a)][TVL_IDX(b)];
}

/**
 * @brief 三值等价运算 EQUIV (A <-> B)
 *
 * 等价于 AND(IMPLIES(a, b), IMPLIES(b, a))
 */
Lv00TruthValue lv00_tvl_equiv(Lv00TruthValue a, Lv00TruthValue b)
{
    Lv00TruthValue ab = lv00_tvl_implies(a, b);
    Lv00TruthValue ba = lv00_tvl_implies(b, a);
    return lv00_tvl_and(ab, ba);
}

/* ================================================================
 *  批量操作
 * ================================================================ */

/**
 * @brief 三值与运算（数组形式，带短路）
 *
 * 遇到 FALSE 立即返回 FALSE。
 */
Lv00TruthValue lv00_tvl_and_all(const Lv00TruthValue *values, int count)
{
    Lv00TruthValue result;
    int i;

    if (!values || count <= 0) {
        return LV00_TRUE;  /* 空归约的 AND 单位元为 TRUE */
    }

    result = LV00_TRUE;
    for (i = 0; i < count; i++) {
        result = lv00_tvl_and(result, values[i]);
        if (result == LV00_FALSE) {
            return LV00_FALSE;  /* 短路 */
        }
    }
    return result;
}

/**
 * @brief 三值或运算（数组形式，带短路）
 *
 * 遇到 TRUE 立即返回 TRUE。
 */
Lv00TruthValue lv00_tvl_or_all(const Lv00TruthValue *values, int count)
{
    Lv00TruthValue result;
    int i;

    if (!values || count <= 0) {
        return LV00_FALSE;  /* 空归约的 OR 单位元为 FALSE */
    }

    result = LV00_FALSE;
    for (i = 0; i < count; i++) {
        result = lv00_tvl_or(result, values[i]);
        if (result == LV00_TRUE) {
            return LV00_TRUE;  /* 短路 */
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
const char *lv00_tvl_to_string(Lv00TruthValue v)
{
    switch (v) {
        case LV00_TRUE:    return "TRUE";
        case LV00_FALSE:   return "FALSE";
        case LV00_UNKNOWN: return "UNKNOWN";
        default:           return "INVALID";
    }
}

/**
 * @brief 将三值真值转换为中文字符串
 */
const char *lv00_tvl_to_string_zh(Lv00TruthValue v)
{
    switch (v) {
        case LV00_TRUE:    return "\xe7\x9c\x9f";           /* "真" */
        case LV00_FALSE:   return "\xe4\xbc\xaa";           /* "伪" */
        case LV00_UNKNOWN: return "\xe6\x9c\xaa\xe7\x9f\xa5"; /* "未知" */
        default:           return "\xe6\x97\xa0\xe6\x95\x88"; /* "无效" */
    }
}
