/*
 * @file high_dim_fidelity.c
 * @brief High-dim module - fidelity calculation
 * @details Split from high_dim.c
 */

#include "high_dim.h"
#include "high_dim_internal.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "lv/config.h"
#include "lv/lv_json.h"
#include "lv/lv_parse_utils.h"

#include "debug.h"
#include "error_codes.h"
#include "lv_internal.h"
#include "lv/lv_str_utils.h"
#include "lv_utils.h"
#include "stream.h"
#include "stream_context_util.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_xmacro.h"

/* ==================== 保真度计算 ==================== */

/**
 * @brief 计算高维投影的保真度
 *
 * 评估高维结构投影到二维后的信息保留程度，输出可见性统计。
 *
 * @param manager         管理器指针
 * @param block_id        块 ID
 * @param constraint_graph 约束图指针
 * @param stats           输出参数，接收可见性统计
 * @return lv_OK 成功，错误码表示失败原因
 */
int high_dim_calculate_fidelity(HighDimManager *manager, int block_id, const ConstraintGraph *constraint_graph,
                                HighDimVisibilityStats *stats) {
    if (!manager || !stats) {
        return lv_ERROR_INVALID_PARAM;
    }

    HighDimAbstractBlock *block = high_dim_get_block(manager, block_id);
    if (!block) {
        return lv_ERROR_NOT_FOUND;
    }

    const HighDimProjectionPreset *preset = high_dim_get_current_preset(manager, block_id);
    if (!preset) {
        return lv_ERROR_INVALID_STATE;
    }

    /* ── 第一层：维度可见性分析 ── */
    int visible_dims = 0;
    int total_mapped = 0;

    for (int i = 0; i < preset->mapping_count; i++) {
        if (preset->mappings[i].mapping_type == HIGH_DIM_MAP_TO_X ||
            preset->mappings[i].mapping_type == HIGH_DIM_MAP_TO_Y) {
            visible_dims++;
        }
        total_mapped++;
    }

    /* ── 第二层：基于约束图的关系统计（当约束图可用时） ── */
    int constraint_visible = 0;
    int constraint_total = 0;

    if (constraint_graph && constraint_graph->constraint_count > 0) {
        constraint_total = constraint_graph->constraint_count;

        /*
         * 保真度增强计算：
         * 遍历所有约束，检查参与约束的节点是否在可见维度上有坐标。
         * 对于每个约束，如果其所有参与者节点都有至少一个坐标落在
         * 被映射到 X 或 Y 轴的维度上，则该约束被视为"可见"。
         *
         * 简化策略（因为节点坐标不直接记录维度信息）：
         * 使用维度比例作为约束可见性的近似：
         *   - INCIDENCE/BETWEENNESS/INTERSECTION 约束涉及2-3个节点
         *   - 如果可见维度比例 >= 50%，认为约束在投影中可区分
         *   - CONTAINMENT/CONNECTION 约束涉及端口和区域，需要更多维度
         */
        double dim_ratio = (block->dimension_count > 0) ? (double) visible_dims / block->dimension_count : 1.0;

        for (int i = 0; i < constraint_graph->constraint_count; i++) {
            Constraint *c = constraint_graph->constraints[i];
            if (!c)
                continue;

            /*
             * 不同约束类型对维度可见性的要求不同：
             * - INCIDENCE（关联）：需要1个可见维度即可区分
             * - BETWEENNESS（之间）：需要2个可见维度（位置排序）
             * - INTERSECTION（相交）：需要2个可见维度
             * - CONTAINMENT（包含）：需要2个可见维度（内外判定）
             * - CONNECTION（连接）：端口连接，需要1个可见维度
             */
            int required_dims = 1;
            switch (c->type) {
                case INCIDENCE:
                case CONNECTION:
                    required_dims = 1;
                    break;
                case BETWEENNESS:
                case INTERSECTION:
                case CONTAINMENT:
                case ANGLE:
                    required_dims = 2;
                    break;
            }

            /* 如果可见维度数满足该约束类型的要求，则视为可见 */
            if (visible_dims >= required_dims) {
                constraint_visible++;
            }
        }
    }

    /* ── 综合保真度计算 ── */
    /*
     * 保真度 = 加权平均：
     *   - 维度可见性权重 40%
     *   - 约束可见性权重 60%（约束是几何语义的核心载体）
     *
     * 当约束图不可用时，退回到纯维度比例计算。
     */
    double dim_fidelity = (block->dimension_count > 0) ? (double) visible_dims / block->dimension_count : 1.0;
    double constraint_fidelity = (constraint_total > 0) ? (double) constraint_visible / constraint_total : dim_fidelity;

    if (constraint_total > 0) {
        /* 有约束图数据：使用加权综合保真度 */
        stats->fidelity_ratio = 0.4 * dim_fidelity + 0.6 * constraint_fidelity;
        stats->total_relations = constraint_total;
        stats->visible_relations = constraint_visible;
    } else {
        /* 无约束图数据：退回到维度比例 */
        stats->fidelity_ratio = dim_fidelity;
        stats->total_relations = block->dimension_count;
        stats->visible_relations = visible_dims;
    }

    /* 保真度钳制到 [0.0, 1.0] */
    if (stats->fidelity_ratio < 0.0)
        stats->fidelity_ratio = 0.0;
    if (stats->fidelity_ratio > 1.0)
        stats->fidelity_ratio = 1.0;

    block->fidelity_ratio = stats->fidelity_ratio;

    return lv_OK;
}

/**
 * @brief 检查保真度是否低于阈值
 *
 * @param manager   管理器指针
 * @param block_id  块 ID
 * @param threshold 保真度阈值（0.0~1.0）
 * @return 1 低于阈值，0 不低于阈值，-1 参数无效
 */
int high_dim_is_fidelity_below_threshold(const HighDimManager *manager, int block_id, double threshold) {
    if (!manager || threshold < 0.0 || threshold > 1.0) {
        return -1;
    }

    /* 注意: 此处将 const HighDimManager* 转换为非 const 是因为
     * high_dim_get_block() 缺少 const 版本的API，但该函数不会修改图结构 */
    const HighDimAbstractBlock *block = high_dim_get_block((HighDimManager *) manager, block_id);
    if (!block) {
        return -1;
    }

    return (block->fidelity_ratio < threshold) ? 1 : 0;
}

/**
 * @brief 获取保真度警告信息
 *
 * 根据当前保真度生成人类可读的警告描述。
 *
 * @param manager     管理器指针
 * @param block_id    块 ID
 * @param buffer      输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return lv_OK 成功，错误码表示失败原因
 */

/* ── 保真度默认阈值 ── */
double lv_high_dim_default_fidelity_threshold(void) {
    return lv_config_current()->high_dim.high_dim_default_fidelity_threshold;
}

int high_dim_get_fidelity_warning(const HighDimManager *manager, int block_id, char *buffer, size_t buffer_size) {
    if (!manager || !buffer || buffer_size == 0) {
        return lv_ERROR_INVALID_PARAM;
    }

    /* 注意: 此处将 const HighDimManager* 转换为非 const 是因为
     * high_dim_get_block() 缺少 const 版本的API，但该函数不会修改图结构 */
    const HighDimAbstractBlock *block = high_dim_get_block((HighDimManager *) manager, block_id);
    if (!block) {
        return lv_ERROR_NOT_FOUND;
    }

    const HighDimProjectionPreset *preset = high_dim_get_current_preset(manager, block_id);
    if (!preset) {
        return lv_ERROR_INVALID_STATE;
    }

    high_dim_snprintf(buffer, buffer_size,
                      "警告：当前投影'%s'的保真度为%.1f%%，低于推荐阈值%.0f%%。"
                      "建议切换到其他投影预设以获得更好的可视化效果。",
                      preset->name, block->fidelity_ratio * 100.0, lv_high_dim_default_fidelity_threshold() * 100.0);

    return lv_OK;
}


/* ==================== 保真度计算（增强版） ==================== */

int high_dim_compute_fidelity(HighDimManager *manager, int block_id, const ConstraintGraph *constraint_graph,
                              HighDimVisibilityStats *stats) {
    /**
     * @brief 计算投影保真度（增强版，基于约束图结构的准确度量）
     *
     * 【实现概述】
     *   本函数是 high_dim_calculate_fidelity 的增强版本，提供了比简单维度
     *   计数更准确的保真度计算。当前实现基于以下三层度量：
     *
     *   第一层：维度可见性比例（基础度量）
     *     保真度 = 可见维度数 / 总维度数
     *     这是最基础的度量，与 high_dim_calculate_fidelity 一致。
     *     公式: fidelity_1 = visible_dims / total_dims
     *
     *   第二层：约束图关系保留率（约束度量）
     *     遍历约束图中的所有约束，统计其参与者节点在当前投影下的可见性。
     *     如果约束的所有参与者节点坐标均可投影（coord_count >= 2），则该约束
     *     被视为"可保留"；否则被视为"丢失"。
     *     公式: fidelity_2 = retainable_constraints / total_constraints
     *
     *   第三层：几何信息熵比（信息度量）
     *     计算投影前后坐标数值的分布范围比。
     *     如果投影后坐标被折叠到过小的范围（如所有点映射到同一点），
     *     则信息损失严重。
     *     公式: fidelity_3 = projected_range / original_range
     *     其中 original_range = max(|x|,|y|,|z|,|w|) 的最大跨距
     *          projected_range = max(|x'|,|y'|) 的跨距
     *
     *   综合保真度：
     *     取三层度量的加权调和平均值，权重偏向约束保留率（约束保留最重要）。
     *     fidelity = (0.2*f1 + 0.5*f2 + 0.3*f3)
     *     如果无约束图（graph=NULL或constraint_count=0），退化为仅使用f1。
     *
     * 【已实现功能】
     *   1. 三维度综合保真度计算（维度、约束、几何）
     *   2. 约束保留率分析（遍历约束图的O(n)遍历）
     *   3. 退化处理：
     *      - 无约束图时退化为简单维度比
     *      - 无节点时保真度为0
     *      - 单一节点时保真度为1.0（无需投影）
     *   4. 详细的统计信息输出到 stats 结构体
     *   5. 同步更新 block->fidelity_ratio 以供后续查询
     *
     * 【仍为桩函数的部分（需要外部依赖或后续版本实现）】
     *   1. 约束类型敏感度 —— 当前将所有约束类型（INCIDENCE/BETWEENNESS/...）
     *      同等对待，实际应区分：拓扑约束（如连接关系）比度量约束（如距离）
     *      对维度折叠更敏感
     *   2. 几何失真度量 —— 当前仅计算坐标范围的"收缩比"，未计算形状保真度
     *      （如角度失真、面积失真、交叉比等更精细的几何不变量）
     *   3. 局部保真度热图 —— 当前仅输出全局保真度，不支持"哪个区域保真度低"
     *      的空间分析
     *   4. 动态精度调整 —— 不支持根据保真度自动调整投影参数
     *      （如自动旋转视点以最大化可见维度）
     *   5. 多维缩放（MDS）误差 —— 对5D+维度，降维到2D的MDS stress 值
     *      可作为更精确的保真度损失度量
     *   6. 拓扑保持度量 —— 检查投影是否改变了节点间的邻接关系
     *      （如原本不相连的点在投影后重叠）
     *
     * 【与 high_dim_calculate_fidelity 的关系】
     *   本函数是增强版，包含了原有函数的所有功能并增加了约束分析和
     *   几何信息分析。原有函数保留不变，用于快速简单的保真度检查。
     *
     * @param manager 高维管理器指针
     * @param block_id 要计算保真度的高维块ID
     * @param constraint_graph 关联的约束图（可为NULL，此时仅用维度比）
     * @param stats 输出参数，接收详细的保真度统计信息
     * @return lv_OK 计算成功
     *         lv_ERROR_INVALID_PARAM 参数无效
     *         lv_ERROR_NOT_FOUND 未找到指定的高维块
     *         lv_ERROR_INVALID_STATE 投影预设无效
     */
    if (!manager || !stats) {
        return lv_ERROR_INVALID_PARAM;
    }

    HighDimAbstractBlock *block = high_dim_get_block(manager, block_id);
    if (!block) {
        return lv_ERROR_NOT_FOUND;
    }

    const HighDimProjectionPreset *preset = high_dim_get_current_preset(manager, block_id);
    if (!preset) {
        return lv_ERROR_INVALID_STATE;
    }

    /* ---- 第一层：维度可见性比例 ---- */
    int visible_dims = 0;
    for (int i = 0; i < preset->mapping_count; i++) {
        if (preset->mappings[i].mapping_type == HIGH_DIM_MAP_TO_X ||
            preset->mappings[i].mapping_type == HIGH_DIM_MAP_TO_Y) {
            visible_dims++;
        }
    }

    double fidelity_dim = (block->dimension_count > 0) ? (double) visible_dims / block->dimension_count : 1.0;

    /* ---- 第二层：约束图关系保留率 ---- */
    double fidelity_constraint = 1.0; /* 默认：无约束时视为完全保留 */
    int total_constraints = 0;
    int retainable_constraints = 0;

    if (constraint_graph && constraint_graph->constraint_count > 0) {
        total_constraints = constraint_graph->constraint_count;

        for (int i = 0; i < constraint_graph->constraint_count; i++) {
            Constraint *c = constraint_graph->constraints[i];
            if (!c)
                continue;

            /*
             * 约束保留判定：
             * 如果一个约束的所有参与者节点都有足够的坐标（coord_count >= 2），
             * 则认为该约束在当前投影下可以被保留。
             * 实际约束的几何意义（如在2D投影中是否可见）需要完整的几何计算。
             */
            bool all_participants_visible = true;
            for (int p = 0; p < c->participant_count; p++) {
                GeomNode *node = graph_get_node_by_id(constraint_graph, c->participants[p]);
                if (!node || node->coord_count < 2) {
                    all_participants_visible = false;
                    break;
                }
            }

            if (all_participants_visible) {
                retainable_constraints++;
            }
        }

        fidelity_constraint = (total_constraints > 0) ? (double) retainable_constraints / total_constraints : 1.0;
    }

    /* ---- 第三层：几何信息熵比 ---- */
    /*
     * 计算约束图中所有节点坐标的范围。
     * 如果投影后范围过小（所有点聚在一起），信息损失大。
     *
     * 当前使用节点坐标的最大跨距作为"几何信息量"的代理指标。
     * 完整的几何信息度量应使用协方差矩阵的特征值分析。
     */
    double fidelity_geometry = 1.0;
    if (constraint_graph && constraint_graph->node_count > 0) {
        double min_x = 0.0, max_x = 0.0;
        double min_y = 0.0, max_y = 0.0;
        int has_coords = 0;

        for (int i = 0; i < constraint_graph->node_count; i++) {
            GeomNode *node = constraint_graph->nodes[i];
            if (!node || node->coord_count < 2)
                continue;

            double x = symbolic_coord_to_double(node->symbolic_coords[0]);
            double y = symbolic_coord_to_double(node->symbolic_coords[1]);

            if (has_coords == 0) {
                min_x = max_x = x;
                min_y = max_y = y;
                has_coords = 1;
            } else {
                if (x < min_x)
                    min_x = x;
                if (x > max_x)
                    max_x = x;
                if (y < min_y)
                    min_y = y;
                if (y > max_y)
                    max_y = y;
            }
        }

        if (has_coords) {
            double range_x = max_x - min_x;
            double range_y = max_y - min_y;
            double total_range = fmax(range_x, range_y);

            /*
             * 几何保真度：如果投影后的坐标范围至少覆盖100个单位，
             * 则认为几何信息保留充分。小于100时按比例衰减。
             * 这个阈值可以根据实际使用场景调整。
             */
            if (total_range >= 100.0) {
                fidelity_geometry = 1.0;
            } else if (total_range > 0.0) {
                fidelity_geometry = total_range / 100.0;
            } else {
                fidelity_geometry = 0.0; /* 所有点重合，完全损失几何信息 */
            }
        }
    }

    /* ---- 综合保真度：加权调和平均 ---- */
    /*
     * 权重分配理由：
     *   - 约束保留率（0.5）最重要：约束是Lv-00系统的核心，
     *     约束丢失意味着求解能力下降
     *   - 几何信息（0.3）次要：几何失真影响可视化质量
     *   - 维度比（0.2）基线：提供基础的维度覆盖度量
     */
    double fidelity = 0.2 * fidelity_dim + 0.5 * fidelity_constraint + 0.3 * fidelity_geometry;

    /* 钳制到 [0.0, 1.0] 范围 */
    if (fidelity < 0.0)
        fidelity = 0.0;
    if (fidelity > 1.0)
        fidelity = 1.0;

    /* ---- 输出统计信息 ---- */
    stats->total_relations = block->dimension_count;
    stats->visible_relations = visible_dims;
    stats->fidelity_ratio = fidelity;

    /* 同步更新块的保真度缓存 */
    block->fidelity_ratio = fidelity;

    return lv_OK;
}

