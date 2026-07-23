/**
 * @file proof_score.c
 * @brief 证明评分模块 —— Layer2 资源管理层
 *
 * 提供证明质量的评估与等级划分功能。
 * 基于证明的完整性、正确性、效率和简洁度等维度进行综合评分，
 * 并将评分映射为人类可读的等级标签。
 *
 * 评分范围：0.0（最差）~ 1.0（完美）
 * 等级划分：
 *   A+  [0.95, 1.00]  优秀
 *   A   [0.85, 0.95)  良好
 *   B   [0.70, 0.85)  合格
 *   C   [0.50, 0.70)  及格
 *   D   [0.00, 0.50)  不及格
 *   F   < 0.0         无效
 *
 * @version 1.0.0
 */

#include "lv/proof_score.h"

#include <string.h>

/* ================================================================
 *  评分阈值常量
 * ================================================================ */

#define SCORE_THRESHOLD_A_PLUS  0.95    /**< A+ 等级阈值 */
#define SCORE_THRESHOLD_A       0.85    /**< A  等级阈值 */
#define SCORE_THRESHOLD_B       0.70    /**< B  等级阈值 */
#define SCORE_THRESHOLD_C       0.50    /**< C  等级阈值 */
#define SCORE_INVALID           (-1.0)  /**< 无效分数 */

/* ================================================================
 *  评分维度权重
 * ================================================================ */

/** 完整性权重（证明步骤是否齐全） */
#define WEIGHT_COMPLETENESS     0.30

/** 正确性权重（逻辑推导是否无误） */
#define WEIGHT_CORRECTNESS      0.35

/** 效率权重（证明步数、推理时间） */
#define WEIGHT_EFFICIENCY       0.20

/** 简洁度权重（是否存在冗余步骤） */
#define WEIGHT_SIMPLICITY       0.15

/* ================================================================
 *  内部辅助函数
 * ================================================================ */

/**
 * @brief 将原始分数钳制到 [0.0, 1.0] 范围
 *
 * @param raw 原始分数
 * @return 钳制后的分数
 */
static double clamp_score(double raw)
{
    if (raw < 0.0) return 0.0;
    if (raw > 1.0) return 1.0;
    return raw;
}

/**
 * @brief 根据分数返回等级标签
 *
 * @param score 已钳制到 [0.0, 1.0] 的分数
 * @return 等级字符串（静态存储，无需释放）
 */
static const char *score_to_grade(double score)
{
    if (score >= SCORE_THRESHOLD_A_PLUS) {
        return "A+";
    }
    if (score >= SCORE_THRESHOLD_A) {
        return "A";
    }
    if (score >= SCORE_THRESHOLD_B) {
        return "B";
    }
    if (score >= SCORE_THRESHOLD_C) {
        return "C";
    }
    return "D";
}

/* ================================================================
 *  公共 API 实现
 * ================================================================ */

/**
 * @brief 评估证明的综合质量评分
 *
 * 对指定证明进行多维度评估，返回 [0.0, 1.0] 范围的综合评分。
 * 评估维度包括完整性、正确性、效率和简洁度。
 *
 * 当前实现为基础版本：使用 proof_id 的哈希映射作为模拟评分。
 * 完整版将接入证明引擎进行实际评估。
 *
 * @param proof_id 证明标识符（>= 0 有效）
 * @param engine   证明引擎句柄（当前预留接口，传 NULL 有效）
 * @return 综合评分 [0.0, 1.0]，无效输入返回 0.0
 */
double lv_proof_score_evaluate(int proof_id, void *engine)
{
    double base_score;
    double completeness;
    double correctness;
    double efficiency;
    double simplicity;
    double final_score;

    (void)engine;  /* 预留：未来接入证明引擎 */

    /* 无效 proof_id 检查 */
    if (proof_id < 0) {
        return 0.0;
    }

    /*
     * 基于 proof_id 的确定性模拟评分。
     * 使用简单哈希确保同一 proof_id 每次返回相同分数。
     * 完整实现应遍历证明树节点，统计：
     *   - 完整性：已证明子目标 / 总子目标
     *   - 正确性：正确推理步 / 总推理步
     *   - 效率：   基于步数的反比评分
     *   - 简洁度：1.0 - (冗余步 / 总步)
     */

    /* 模拟各维度评分（基于 proof_id 的确定性计算） */
    completeness = 0.6 + 0.4 * ((double)(proof_id % 11) / 10.0);
    correctness  = 0.7 + 0.3 * ((double)(proof_id % 7) / 6.0);
    efficiency   = 0.5 + 0.5 * ((double)(proof_id % 13) / 12.0);
    simplicity   = 0.6 + 0.4 * ((double)(proof_id % 9) / 8.0);

    /* 加权综合评分 */
    final_score = completeness * WEIGHT_COMPLETENESS +
                  correctness  * WEIGHT_CORRECTNESS  +
                  efficiency   * WEIGHT_EFFICIENCY    +
                  simplicity   * WEIGHT_SIMPLICITY;

    /* 钳制到有效范围 */
    final_score = clamp_score(final_score);

    return final_score;
}

/**
 * @brief 将评分转换为人类可读的等级标签
 *
 * @param score 综合评分（通常由 lv_proof_score_evaluate 返回）
 * @return 等级字符串（静态存储，无需释放）
 *         无效分数返回 "F"
 */
const char *lv_proof_score_grade(double score)
{
    /* 无效分数检查 */
    if (score < 0.0) {
        return "F";
    }

    /* 超出范围的分数也标记为无效 */
    if (score > 1.0) {
        return "F";
    }

    return score_to_grade(score);
}
