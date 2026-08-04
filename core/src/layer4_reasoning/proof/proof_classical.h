/**
 * @file proof_classical.h
 * @brief 经典不可构造问题（古典难题）关键词查找表
 *
 * 统一 proof_navigator_instantiate.c 与 proof_dependency.c 中
 * 对经典难题关键词（中/英双语）的 strstr 子串匹配分发，
 * 消除两处近乎逐字重复的 strstr 判断链。
 *
 * @author Lv-00 Project
 */

#ifndef lv_PROOF_CLASSICAL_H
#define lv_PROOF_CLASSICAL_H

#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 经典不可构造问题枚举 */
typedef enum {
    CLASSICAL_PROBLEM_NONE = 0,   /**< 未匹配到任何经典问题 */
    CLASSICAL_PROBLEM_TRISECTION, /**< 三等分角 */
    CLASSICAL_PROBLEM_DOUBLING,   /**< 倍立方体 */
    CLASSICAL_PROBLEM_SQUARING,   /**< 化圆为方 */
    CLASSICAL_PROBLEM_HEPTAGON,   /**< 正七边形 */
} ClassicalProblem;

/** @brief 经典难题关键词查找表条目（strstr 子串匹配） */
typedef struct {
    const char *keyword;       /**< 关键词（中/英文） */
    ClassicalProblem problem;  /**< 对应的经典难题枚举 */
} ClassicalProblemEntry;

/**
 * @brief 经典难题关键词查找表
 *
 * 含中英双语条目；条目顺序与原 if/else-if 分支一致
 * （trisection -> doubling -> squaring -> heptagon），
 * 保证首次命中语义与原判断链相同。
 */
static const ClassicalProblemEntry kClassicalProblemKeywords[] = {
    {"trisection", CLASSICAL_PROBLEM_TRISECTION},
    {"三等分", CLASSICAL_PROBLEM_TRISECTION},
    {"doubling", CLASSICAL_PROBLEM_DOUBLING},
    {"倍立方", CLASSICAL_PROBLEM_DOUBLING},
    {"squaring", CLASSICAL_PROBLEM_SQUARING},
    {"化圆为方", CLASSICAL_PROBLEM_SQUARING},
    {"heptagon", CLASSICAL_PROBLEM_HEPTAGON},
    {"七边形", CLASSICAL_PROBLEM_HEPTAGON},
};

/**
 * @brief 在文本中匹配经典难题关键词（strstr 子串匹配）
 *
 * 按表序返回首个命中的问题枚举；语义与原 if/else-if 链一致。
 *
 * @param text 待匹配文本（可为 NULL）
 * @return 命中的 ClassicalProblem；无匹配返回 CLASSICAL_PROBLEM_NONE
 */
static inline ClassicalProblem lv_classical_problem_match(const char *text) {
    if (!text)
        return CLASSICAL_PROBLEM_NONE;
    for (size_t i = 0; i < sizeof(kClassicalProblemKeywords) / sizeof(kClassicalProblemKeywords[0]); i++) {
        if (strstr(text, kClassicalProblemKeywords[i].keyword) != NULL)
            return kClassicalProblemKeywords[i].problem;
    }
    return CLASSICAL_PROBLEM_NONE;
}

#ifdef __cplusplus
}
#endif

#endif /* lv_PROOF_CLASSICAL_H */
