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

#define SCORE_THRESHOLD_A_PLUS 0.95 /**< A+ 等级阈值 */
#define SCORE_THRESHOLD_A 0.85      /**< A  等级阈值 */
#define SCORE_THRESHOLD_B 0.70      /**< B  等级阈值 */
#define SCORE_THRESHOLD_C 0.50      /**< C  等级阈值 */
#define SCORE_INVALID (-1.0)        /**< 无效分数 */

/* ================================================================
 *  评分维度权重
 * ================================================================ */

/** 完整性权重（证明步骤是否齐全） */
#define WEIGHT_COMPLETENESS 0.30

/** 正确性权重（逻辑推导是否无误） */
#define WEIGHT_CORRECTNESS 0.35

/** 效率权重（证明步数、推理时间） */
#define WEIGHT_EFFICIENCY 0.20

/** 简洁度权重（是否存在冗余步骤） */
#define WEIGHT_SIMPLICITY 0.15

/* ================================================================
 *  内部辅助函数
 * ================================================================ */

/**
 * @brief 将原始分数钳制到 [0.0, 1.0] 范围
 *
 * @param raw 原始分数
 * @return 钳制后的分数
 */
static double clamp_score(double raw) {
    if (raw < 0.0)
        return 0.0;
    if (raw > 1.0)
        return 1.0;
    return raw;
}

/**
 * @brief 根据分数返回等级标签
 *
 * @param score 已钳制到 [0.0, 1.0] 的分数
 * @return 等级字符串（静态存储，无需释放）
 */
static const char *score_to_grade(double score) {
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
double lv_proof_score_evaluate(int proof_id, void *engine) {
    double completeness;
    double correctness;
    double efficiency;
    double simplicity;
    double final_score;

    /* 无效 proof_id 检查 */
    if (proof_id < 0) {
        return 0.0;
    }

    /*
     * 真实评分：从引擎的约束图中获取证明状态数据，
     * 评估完整性、正确性、效率和简洁度四个维度。
     * 如果 engine 不可用或无法获取证明数据，降级使用确定性哈希模拟。
     */
    lvEngine *eng = (lvEngine *) engine;
    if (eng && eng->main_graph) {
        ConstraintGraph *graph = eng->main_graph;
        int active_node_count = 0;
        int green_node_count = 0;
        int active_constraint_count = 0;
        int conflict_node_count = 0;
        double total_satisfaction = 0.0;

        /* 遍历节点统计：活跃数、绿色（已证明）数、冲突数 */
        for (int i = 0; i < graph->node_count; i++) {
            GeomNode *node = graph->nodes[i];
            if (!node || !node->is_active)
                continue;
            active_node_count++;
            if (node->trust == TRUST_GREEN)
                green_node_count++;
            if (node->trust == TRUST_RED)
                conflict_node_count++;
        }

        /* 遍历约束统计：满意度累计 */
        for (int i = 0; i < graph->constraint_count; i++) {
            Constraint *con = graph->constraints[i];
            if (!con || !con->is_active)
                continue;
            active_constraint_count++;
            total_satisfaction += con->satisfaction;
        }

        /* ---- 完整性：绿色（已证明）节点占比 ---- */
        if (active_node_count > 0) {
            completeness = (double) green_node_count / (double) active_node_count;
            completeness = 0.3 + 0.7 * completeness; /* 确保基础分 */
        } else {
            completeness = 0.5;
        }

        /* ---- 正确性：约束满意度均值 ---- */
        if (active_constraint_count > 0) {
            correctness = total_satisfaction / (double) active_constraint_count;
        } else {
            correctness = 0.7;
        }

        /* ---- 效率：约束/节点比例越接近 1.0 效率越高 ---- */
        if (active_node_count > 0) {
            double ratio = (double) active_constraint_count / (double) active_node_count;
            if (ratio <= 0.5)
                efficiency = 0.6 + 0.4 * (ratio / 0.5);
            else if (ratio <= 1.5)
                efficiency = 1.0 - 0.2 * ((ratio - 0.5) / 1.0);
            else
                efficiency = 0.8 - 0.3 * ((ratio - 1.5) / (ratio + 0.5));
            efficiency = clamp_score(efficiency);
        } else {
            efficiency = 0.6;
        }

        /* ---- 简洁度：冲突惩罚 + 冗余检测 ---- */
        if (active_node_count > 0 && active_constraint_count > 0) {
            double conflict_penalty = (double) conflict_node_count / (double) active_node_count;
            simplicity = 0.9 - 0.5 * conflict_penalty;
            /* 检测冗余约束并惩罚 */
            int redundant_count = 0;
            int *redundant_ids = graph_detect_redundant_constraints(graph, &redundant_count);
            if (redundant_ids) {
                double redundancy_ratio = (double) redundant_count / (double) active_constraint_count;
                simplicity -= 0.3 * redundancy_ratio;
                free(redundant_ids);
            }
            simplicity = clamp_score(simplicity);
        } else {
            simplicity = 0.7;
        }
    } else {
        /* 降级：无可用引擎或图，使用确定性哈希模拟评分 */
        completeness = 0.6 + 0.4 * ((double) (proof_id % 11) / 10.0);
        correctness = 0.7 + 0.3 * ((double) (proof_id % 7) / 6.0);
        efficiency = 0.5 + 0.5 * ((double) (proof_id % 13) / 12.0);
        simplicity = 0.6 + 0.4 * ((double) (proof_id % 9) / 8.0);
    }

    /* 加权综合评分 */
    final_score = completeness * WEIGHT_COMPLETENESS + correctness * WEIGHT_CORRECTNESS +
                  efficiency * WEIGHT_EFFICIENCY + simplicity * WEIGHT_SIMPLICITY;

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
const char *lv_proof_score_grade(double score) {
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
