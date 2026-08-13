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
#include "lv/geo_utils.h" /* geo_norm_2d（2D 向量模长统一工具） */
#include "lv/lv_json.h"
#include "lv/lv_parse_utils.h"
#include "lv/lv_numeric.h"

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
            static const int kConstraintRequiredDims[] = {
                1,  /* INCIDENCE */
                2,  /* BETWEENNESS */
                2,  /* INTERSECTION */
                2,  /* CONTAINMENT */
                1,  /* CONNECTION */
                2   /* ANGLE */
            };
            static const int kConstraintRequiredDimsCount =
                (int)(sizeof(kConstraintRequiredDims) / sizeof(kConstraintRequiredDims[0]));
            int required_dims = 1;
            if (c->type >= 0 && c->type < kConstraintRequiredDimsCount) {
                required_dims = kConstraintRequiredDims[(int)c->type];
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

    lv_snprintf(buffer, buffer_size,
                      "警告：当前投影'%s'的保真度为%.1f%%，低于推荐阈值%.0f%%。"
                      "建议切换到其他投影预设以获得更好的可视化效果。",
                      preset->name, block->fidelity_ratio * 100.0, lv_high_dim_default_fidelity_threshold() * 100.0);

    return lv_OK;
}
/* ==================== 增强版保真度计算：六项细粒度指标 ==================== */

/**
 * 约束类型敏感度权重表
 *
 * 每种约束类型在低维投影中"失真"的敏感程度不同：
 *   - INCIDENCE / CONNECTION 只需 1 个可见维度即可区分，敏感度低；
 *   - BETWEENNESS / INTERSECTION 需要 2 个可见维度表达空间关系；
 *   - CONTAINMENT / ANGLE 对维度保留要求最高（内外判定、角度度量），敏感度最高。
 * weight 用于约束可见性的加权统计（越大越敏感），required_dims 为可见性判定门槛。
 */
typedef struct {
    ConstraintType type;
    double weight;
    int required_dims;
} HighDimConstraintSensitivityEntry;

static const HighDimConstraintSensitivityEntry g_constraint_sensitivity_table[] = {
    { INCIDENCE,    1.0, 1 },
    { CONNECTION,   1.0, 1 },
    { BETWEENNESS,  1.2, 2 },
    { INTERSECTION, 1.2, 2 },
    { CONTAINMENT,  1.4, 2 },
    { ANGLE,        1.3, 2 },
};

static const int g_constraint_sensitivity_count =
    (int) (sizeof(g_constraint_sensitivity_table) / sizeof(g_constraint_sensitivity_table[0]));

/* 从权重表查询约束类型的敏感度权重与可见维度门槛 */
static void high_dim_sensitivity_lookup(ConstraintType type, double *out_weight, int *out_required_dims) {
    *out_weight = 1.0;
    *out_required_dims = 1;
    for (int i = 0; i < g_constraint_sensitivity_count; i++) {
        if (g_constraint_sensitivity_table[i].type == type) {
            *out_weight = g_constraint_sensitivity_table[i].weight;
            *out_required_dims = g_constraint_sensitivity_table[i].required_dims;
            return;
        }
    }
}

/* 计算当前预设可见的维度数（映射到 X/Y 轴的维度） */
static int high_dim_count_visible_dims(HighDimManager *manager, int block_id) {
    const HighDimProjectionPreset *preset = high_dim_get_current_preset(manager, block_id);
    if (!preset) {
        return 0;
    }
    int visible = 0;
    for (int i = 0; i < preset->mapping_count; i++) {
        if (preset->mappings[i].mapping_type == HIGH_DIM_MAP_TO_X ||
            preset->mappings[i].mapping_type == HIGH_DIM_MAP_TO_Y) {
            visible++;
        }
    }
    return visible;
}

/* ── 指标 1：约束类型敏感度 ──
 *
 * 以约束类型权重为口径统计约束保留率：类型越"敏感"（如包含、角度），
 * 权重越高，其丢失对保真度的影响越大。结果为 0~1 的加权保留率。
 */
static double high_dim_constraint_type_sensitivity(HighDimManager *manager, int block_id,
                                                   const ConstraintGraph *graph) {
    if (!graph || graph->constraint_count == 0) {
        return 1.0;
    }

    int visible_dims = high_dim_count_visible_dims(manager, block_id);

    double weighted_total = 0.0;
    double weighted_visible = 0.0;

    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (!c || !c->is_active) {
            continue;
        }
        double weight;
        int required_dims;
        high_dim_sensitivity_lookup(c->type, &weight, &required_dims);

        weighted_total += weight;
        if (visible_dims >= required_dims) {
            weighted_visible += weight;
        }
    }

    if (weighted_total <= 0.0) {
        return 1.0;
    }
    return weighted_visible / weighted_total;
}

/* ── 指标 2：几何失真度量（距离 + 角度） ──
 *
 * 将节点符号坐标分别投影到高维数值向量与低维投影坐标：
 *   - 距离失真：采样点对的高维距离/低维距离比值的变异系数
 *     （比值偏离均值越大，长度关系被扭曲越严重）
 *   - 角度失真：采样三点在投影前后的夹角余弦偏差（归一化到 0~1）
 * 最终失真 = 0.7*距离失真 + 0.3*角度失真。
 */
static double high_dim_geometric_distortion(HighDimManager *manager, int block_id,
                                            const ConstraintGraph *graph) {
    if (!graph || graph->node_count < 2) {
        return 0.0;
    }

    /* 采样可投影节点，保存高维数值向量与低维投影坐标 */
    enum { MAX_SAMPLE_NODES = 64 };
    double hi[MAX_SAMPLE_NODES][HIGH_DIM_MAX_DIMENSIONS];
    double lo[MAX_SAMPLE_NODES][2];
    int sample_count = 0;

    for (int i = 0; i < graph->node_count && sample_count < MAX_SAMPLE_NODES; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || !node->is_active || !node->symbolic_coords || node->coord_count == 0) {
            continue;
        }

        /* 使用当前投影预设将节点符号坐标投影到低维 */
        HighDimProjectedCoord pc;
        if (high_dim_project_coordinates(manager, block_id, (const SymbolicCoord **) node->symbolic_coords,
                                         node->coord_count, &pc) != lv_OK || !pc.is_valid) {
            continue;
        }

        int dims = node->coord_count;
        if (dims > HIGH_DIM_MAX_DIMENSIONS) {
            dims = HIGH_DIM_MAX_DIMENSIONS;
        }
        for (int d = 0; d < dims; d++) {
            hi[sample_count][d] = symbolic_coord_to_double(node->symbolic_coords[d]);
        }
        for (int d = dims; d < HIGH_DIM_MAX_DIMENSIONS; d++) {
            hi[sample_count][d] = 0.0;
        }
        lo[sample_count][0] = pc.x;
        lo[sample_count][1] = pc.y;
        sample_count++;
    }

    if (sample_count < 2) {
        return 0.0;
    }

    /* 距离失真：抽样点对，比较高维/低维距离比的一致性 */
    enum { MAX_DIST_SAMPLES = 128 };
    double ratios[MAX_DIST_SAMPLES];
    int ratio_count = 0;
    double sum_ratio = 0.0;

    for (int i = 0; i < sample_count - 1 && ratio_count < MAX_DIST_SAMPLES; i++) {
        for (int j = i + 1; j < sample_count && ratio_count < MAX_DIST_SAMPLES; j++) {
            double sum_sq = 0.0;
            for (int d = 0; d < HIGH_DIM_MAX_DIMENSIONS; d++) {
                double diff = hi[i][d] - hi[j][d];
                sum_sq += diff * diff;
            }
            double hi_dist = sqrt(sum_sq);
            if (hi_dist < lv_GEO_COLLINEAR_EPSILON) {
                continue; /* 高维退化点对，跳过 */
            }
            double dx = lo[i][0] - lo[j][0];
            double dy = lo[i][1] - lo[j][1];
            double lo_dist = geo_norm_2d(dx, dy);

            double ratio = lo_dist / hi_dist;
            ratios[ratio_count++] = ratio;
            sum_ratio += ratio;
        }
    }

    double distance_distortion = 0.0;
    if (ratio_count > 0) {
        double mean_ratio = sum_ratio / ratio_count;
        if (mean_ratio < lv_GEO_COLLINEAR_EPSILON) {
            distance_distortion = 1.0; /* 所有低维距离为零：完全坍缩 */
        } else {
            double var = 0.0;
            for (int i = 0; i < ratio_count; i++) {
                double dev = (ratios[i] - mean_ratio) / mean_ratio;
                var += dev * dev;
            }
            distance_distortion = sqrt(var / ratio_count);
            if (distance_distortion > 1.0) {
                distance_distortion = 1.0;
            }
        }
    }

    /* 角度失真：抽样三点，比较投影前后的夹角余弦偏差 */
    enum { MAX_ANGLE_SAMPLES = 128 };
    double angle_loss_sum = 0.0;
    int angle_count = 0;

    for (int i = 0; i < sample_count - 2 && angle_count < MAX_ANGLE_SAMPLES; i++) {
        for (int j = i + 1; j < sample_count - 1 && angle_count < MAX_ANGLE_SAMPLES; j++) {
            for (int k = j + 1; k < sample_count && angle_count < MAX_ANGLE_SAMPLES; k++) {
                /* 高维夹角余弦（顶点 j） */
                double cos_hi = 0.0;
                double n_hi_a = 0.0, n_hi_b = 0.0;
                for (int d = 0; d < HIGH_DIM_MAX_DIMENSIONS; d++) {
                    double a = hi[i][d] - hi[j][d];
                    double b = hi[k][d] - hi[j][d];
                    cos_hi += a * b;
                    n_hi_a += a * a;
                    n_hi_b += b * b;
                }
                double norm_hi = sqrt(n_hi_a) * sqrt(n_hi_b);
                if (norm_hi < lv_GEO_COLLINEAR_EPSILON) {
                    continue;
                }
                cos_hi /= norm_hi;
                cos_hi = lv_clamp(cos_hi, -1.0, 1.0);

                /* 低维夹角余弦（顶点 j） */
                double a_lo_x = lo[i][0] - lo[j][0];
                double a_lo_y = lo[i][1] - lo[j][1];
                double b_lo_x = lo[k][0] - lo[j][0];
                double b_lo_y = lo[k][1] - lo[j][1];
                double n_lo_a = geo_norm_2d(a_lo_x, a_lo_y);
                double n_lo_b = geo_norm_2d(b_lo_x, b_lo_y);
                if (n_lo_a < lv_GEO_COLLINEAR_EPSILON || n_lo_b < lv_GEO_COLLINEAR_EPSILON) {
                    continue;
                }
                double cos_lo = (a_lo_x * b_lo_x + a_lo_y * b_lo_y) / (n_lo_a * n_lo_b);
                cos_lo = lv_clamp(cos_lo, -1.0, 1.0);

                double diff = cos_hi - cos_lo;
                if (diff < 0.0) diff = -diff;
                angle_loss_sum += diff / 2.0; /* 余弦差/2 归一化到 0~1 */
                angle_count++;
            }
        }
    }

    double angle_distortion = (angle_count > 0) ? (angle_loss_sum / angle_count) : 0.0;

    double distortion = 0.7 * distance_distortion + 0.3 * angle_distortion;
    distortion = lv_clamp(distortion, 0.0, 1.0);
    return distortion;
}

/* ── 指标 3：局部保真度热图 ──
 *
 * 将低维投影空间按包围盒划分为 HIGH_DIM_HEATMAP_GRID × HIGH_DIM_HEATMAP_GRID
 * 的网格。每个约束按其参与者投影质心归入所在格子，统计该格内的
 * 类型加权可见率，得到局部保真度热图（0~1）。
 */
static void high_dim_local_fidelity_heatmap(HighDimManager *manager, int block_id,
                                            const ConstraintGraph *graph,
                                            double heatmap[HIGH_DIM_HEATMAP_GRID][HIGH_DIM_HEATMAP_GRID]) {
    for (int r = 0; r < HIGH_DIM_HEATMAP_GRID; r++) {
        for (int c = 0; c < HIGH_DIM_HEATMAP_GRID; c++) {
            heatmap[r][c] = 1.0;
        }
    }

    if (!graph || graph->node_count == 0 || graph->constraint_count == 0) {
        return;
    }

    int visible_dims = high_dim_count_visible_dims(manager, block_id);

    /* 包围盒：先投影所有节点，得到低维坐标范围 */
    enum { MAX_HM_NODES = 256 };
    double px[MAX_HM_NODES], py[MAX_HM_NODES];
    int pid[MAX_HM_NODES];
    int pcount = 0;
    double min_x = lv_NEAR_INFINITY_SENTINEL, min_y = lv_NEAR_INFINITY_SENTINEL;
    double max_x = -lv_NEAR_INFINITY_SENTINEL, max_y = -lv_NEAR_INFINITY_SENTINEL;

    for (int i = 0; i < graph->node_count && pcount < MAX_HM_NODES; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || !node->is_active || !node->symbolic_coords || node->coord_count == 0) {
            continue;
        }
        HighDimProjectedCoord pc;
        if (high_dim_project_coordinates(manager, block_id, (const SymbolicCoord **) node->symbolic_coords,
                                         node->coord_count, &pc) != lv_OK || !pc.is_valid) {
            continue;
        }
        px[pcount] = pc.x;
        py[pcount] = pc.y;
        pid[pcount] = node->id;
        if (pc.x < min_x) min_x = pc.x;
        if (pc.x > max_x) max_x = pc.x;
        if (pc.y < min_y) min_y = pc.y;
        if (pc.y > max_y) max_y = pc.y;
        pcount++;
    }

    if (pcount == 0) {
        return;
    }

    double span_x = max_x - min_x;
    double span_y = max_y - min_y;
    if (span_x < lv_GEO_COLLINEAR_EPSILON) span_x = 1.0;
    if (span_y < lv_GEO_COLLINEAR_EPSILON) span_y = 1.0;

    /* 每格累计权重（分母）与可见权重（分子） */
    double weight_total[HIGH_DIM_HEATMAP_GRID][HIGH_DIM_HEATMAP_GRID];
    double weight_visible[HIGH_DIM_HEATMAP_GRID][HIGH_DIM_HEATMAP_GRID];
    for (int r = 0; r < HIGH_DIM_HEATMAP_GRID; r++) {
        for (int c = 0; c < HIGH_DIM_HEATMAP_GRID; c++) {
            weight_total[r][c] = 0.0;
            weight_visible[r][c] = 0.0;
        }
    }

    /* 遍历约束：质心所在格决定归属，可见性决定该约束是否被保留 */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *con = graph->constraints[i];
        if (!con || !con->is_active) {
            continue;
        }

        double weight;
        int required_dims;
        high_dim_sensitivity_lookup(con->type, &weight, &required_dims);

        /* 参与者投影质心 */
        double cx = 0.0, cy = 0.0;
        int cnt = 0;
        for (int p = 0; p < con->participant_count; p++) {
            for (int q = 0; q < pcount; q++) {
                if (pid[q] == con->participants[p]) {
                    cx += px[q];
                    cy += py[q];
                    cnt++;
                    break;
                }
            }
        }
        if (cnt == 0) {
            continue;
        }
        cx /= cnt;
        cy /= cnt;

        /* 归一化到 [0,1] 并映射到格子 */
        double ux = (cx - min_x) / span_x;
        double uy = (cy - min_y) / span_y;
        if (ux < 0.0) ux = 0.0;
        if (ux >= 1.0) ux = 1.0 - lv_GEO_COLLINEAR_EPSILON;
        if (uy < 0.0) uy = 0.0;
        if (uy >= 1.0) uy = 1.0 - lv_GEO_COLLINEAR_EPSILON;
        int c = (int) (ux * HIGH_DIM_HEATMAP_GRID);
        int r = (int) (uy * HIGH_DIM_HEATMAP_GRID);

        weight_total[r][c] += weight;
        if (visible_dims >= required_dims) {
            weight_visible[r][c] += weight;
        }
    }

    /* 计算每格的局部保真度；无约束落入的格子保持 1.0 */
    for (int r = 0; r < HIGH_DIM_HEATMAP_GRID; r++) {
        for (int c = 0; c < HIGH_DIM_HEATMAP_GRID; c++) {
            if (weight_total[r][c] > 0.0) {
                heatmap[r][c] = weight_visible[r][c] / weight_total[r][c];
            }
        }
    }
}

/* ── 指标 4：动态精度调整 ──
 *
 * - dynamic_precision_factor：几何失真越大，建议降低缩放以放大局部细节，
 *   因子 = 1 - 0.5*失真，钳制在 [0.1, 2.0]；
 * - dynamic_rotation_angle：对低维投影点做 2D PCA，建议旋转到主成分方向，
 *   以最大化轴向方差、减少投影遮挡，角度 = 0.5*atan2(2*Cxy, Cxx - Cyy)。
 */
static void high_dim_dynamic_precision(HighDimManager *manager, int block_id, const ConstraintGraph *graph,
                                       double geometric_distortion, double *out_precision_factor,
                                       double *out_rotation_angle) {
    double factor = 1.0 - 0.5 * geometric_distortion;
    if (factor < 0.1) factor = 0.1;
    if (factor > 2.0) factor = 2.0;
    *out_precision_factor = factor;

    *out_rotation_angle = 0.0;
    if (!graph || graph->node_count < 2) {
        return;
    }

    enum { MAX_PCA_NODES = 128 };
    double xs[MAX_PCA_NODES];
    double ys[MAX_PCA_NODES];
    int count = 0;

    for (int i = 0; i < graph->node_count && count < MAX_PCA_NODES; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || !node->is_active || !node->symbolic_coords || node->coord_count == 0) {
            continue;
        }
        HighDimProjectedCoord pc;
        if (high_dim_project_coordinates(manager, block_id, (const SymbolicCoord **) node->symbolic_coords,
                                         node->coord_count, &pc) != lv_OK || !pc.is_valid) {
            continue;
        }
        xs[count] = pc.x;
        ys[count] = pc.y;
        count++;
    }
    if (count < 2) {
        return;
    }

    double mx = 0.0, my = 0.0;
    for (int i = 0; i < count; i++) {
        mx += xs[i];
        my += ys[i];
    }
    mx /= count;
    my /= count;

    double cxx = 0.0, cyy = 0.0, cxy = 0.0;
    for (int i = 0; i < count; i++) {
        double dx = xs[i] - mx;
        double dy = ys[i] - my;
        cxx += dx * dx;
        cyy += dy * dy;
        cxy += dx * dy;
    }

    /* 2x2 对称协方差矩阵主方向角 */
    *out_rotation_angle = 0.5 * atan2(2.0 * cxy, cxx - cyy);
}
/* ── 指标 5：MDS stress（Kruskal stress-1） ──
 *
 * 对采样点对计算高维欧氏距离与低维欧氏距离，用最小二乘过原点
 * 拟合得到最优缩放系数 b，再计算 Kruskal stress-1：
 *   stress = sqrt( Σ(d_lo - b*d_hi)^2 / Σ(d_lo^2) )
 * 0 = 完美保持，1 = 完全失真。
 */
static double high_dim_mds_stress(HighDimManager *manager, int block_id, const ConstraintGraph *graph) {
    if (!graph || graph->node_count < 2) {
        return 0.0;
    }

    enum { MAX_STRESS_NODES = 48 };
    double hi[MAX_STRESS_NODES][HIGH_DIM_MAX_DIMENSIONS];
    double lo[MAX_STRESS_NODES][2];
    int sample_count = 0;

    for (int i = 0; i < graph->node_count && sample_count < MAX_STRESS_NODES; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || !node->is_active || !node->symbolic_coords || node->coord_count == 0) {
            continue;
        }
        HighDimProjectedCoord pc;
        if (high_dim_project_coordinates(manager, block_id, (const SymbolicCoord **) node->symbolic_coords,
                                         node->coord_count, &pc) != lv_OK || !pc.is_valid) {
            continue;
        }
        int dims = node->coord_count;
        if (dims > HIGH_DIM_MAX_DIMENSIONS) {
            dims = HIGH_DIM_MAX_DIMENSIONS;
        }
        for (int d = 0; d < dims; d++) {
            hi[sample_count][d] = symbolic_coord_to_double(node->symbolic_coords[d]);
        }
        for (int d = dims; d < HIGH_DIM_MAX_DIMENSIONS; d++) {
            hi[sample_count][d] = 0.0;
        }
        lo[sample_count][0] = pc.x;
        lo[sample_count][1] = pc.y;
        sample_count++;
    }

    if (sample_count < 2) {
        return 0.0;
    }

    enum { MAX_STRESS_PAIRS = 1024 };
    double d_hi[MAX_STRESS_PAIRS];
    double d_lo[MAX_STRESS_PAIRS];
    int pair_count = 0;

    for (int i = 0; i < sample_count - 1 && pair_count < MAX_STRESS_PAIRS; i++) {
        for (int j = i + 1; j < sample_count && pair_count < MAX_STRESS_PAIRS; j++) {
            double sum_sq = 0.0;
            for (int d = 0; d < HIGH_DIM_MAX_DIMENSIONS; d++) {
                double diff = hi[i][d] - hi[j][d];
                sum_sq += diff * diff;
            }
            double hi_dist = sqrt(sum_sq);
            if (hi_dist < lv_GEO_COLLINEAR_EPSILON) {
                continue; /* 高维退化点对，不参与应力计算 */
            }
            double dx = lo[i][0] - lo[j][0];
            double dy = lo[i][1] - lo[j][1];
            d_hi[pair_count] = hi_dist;
            d_lo[pair_count] = geo_norm_2d(dx, dy);
            pair_count++;
        }
    }

    if (pair_count < 2) {
        return 0.0;
    }

    double sum_lo_hi = 0.0, sum_hi2 = 0.0, sum_lo2 = 0.0;
    for (int k = 0; k < pair_count; k++) {
        sum_lo_hi += d_lo[k] * d_hi[k];
        sum_hi2 += d_hi[k] * d_hi[k];
        sum_lo2 += d_lo[k] * d_lo[k];
    }

    if (sum_hi2 < lv_EPSILON_ULTRA) {
        return 0.0;
    }
    if (sum_lo2 < lv_EPSILON_ULTRA) {
        return 1.0; /* 所有低维距离为零：投影完全坍缩 */
    }

    double b = sum_lo_hi / sum_hi2;
    double num = 0.0;
    for (int k = 0; k < pair_count; k++) {
        double residual = d_lo[k] - b * d_hi[k];
        num += residual * residual;
    }

    double stress = sqrt(num / sum_lo2);
    stress = lv_clamp(stress, 0.0, 1.0);
    return stress;
}

/* ── 指标 6：拓扑保持度量（k 近邻重叠率） ──
 *
 * 对每个采样点分别在高维与低维中取 k 个最近邻（贪心选择），
 * 计算两个近邻集合的共有元素占比，再对所有点取平均，得到 0~1 的
 * 邻域保持率。k = min(5, n-1)。
 */
static double high_dim_topology_preservation(HighDimManager *manager, int block_id, const ConstraintGraph *graph) {
    if (!graph || graph->node_count < 3) {
        return 1.0;
    }

    enum { MAX_TOP_NODES = 48 };
    double hi[MAX_TOP_NODES][HIGH_DIM_MAX_DIMENSIONS];
    double lo[MAX_TOP_NODES][2];
    int sample_count = 0;

    for (int i = 0; i < graph->node_count && sample_count < MAX_TOP_NODES; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || !node->is_active || !node->symbolic_coords || node->coord_count == 0) {
            continue;
        }
        HighDimProjectedCoord pc;
        if (high_dim_project_coordinates(manager, block_id, (const SymbolicCoord **) node->symbolic_coords,
                                         node->coord_count, &pc) != lv_OK || !pc.is_valid) {
            continue;
        }
        int dims = node->coord_count;
        if (dims > HIGH_DIM_MAX_DIMENSIONS) {
            dims = HIGH_DIM_MAX_DIMENSIONS;
        }
        for (int d = 0; d < dims; d++) {
            hi[sample_count][d] = symbolic_coord_to_double(node->symbolic_coords[d]);
        }
        for (int d = dims; d < HIGH_DIM_MAX_DIMENSIONS; d++) {
            hi[sample_count][d] = 0.0;
        }
        lo[sample_count][0] = pc.x;
        lo[sample_count][1] = pc.y;
        sample_count++;
    }

    if (sample_count < 3) {
        return 1.0;
    }

    int k = 5;
    if (k >= sample_count) {
        k = sample_count - 1;
    }

    double total_overlap = 0.0;

    for (int p = 0; p < sample_count; p++) {
        /* 高维与低维距离数组（自身距离置为无穷大，避免自指） */
        double hi_dist[MAX_TOP_NODES];
        double lo_dist[MAX_TOP_NODES];
        for (int q = 0; q < sample_count; q++) {
            if (q == p) {
                hi_dist[q] = lv_NEAR_INFINITY_SENTINEL;
                lo_dist[q] = lv_NEAR_INFINITY_SENTINEL;
                continue;
            }
            double sum_sq = 0.0;
            for (int d = 0; d < HIGH_DIM_MAX_DIMENSIONS; d++) {
                double diff = hi[p][d] - hi[q][d];
                sum_sq += diff * diff;
            }
            hi_dist[q] = sqrt(sum_sq);
            double dx = lo[p][0] - lo[q][0];
            double dy = lo[p][1] - lo[q][1];
            lo_dist[q] = geo_norm_2d(dx, dy);
        }

        int hi_nn[MAX_TOP_NODES];
        int lo_nn[MAX_TOP_NODES];

        /* 贪心选择 k 个最近邻（每轮取当前最小距离并标记为无穷大） */
        for (int round = 0; round < k; round++) {
            int best_hi = -1;
            int best_lo = -1;
            double bd_hi = lv_NEAR_INFINITY_SENTINEL;
            double bd_lo = lv_NEAR_INFINITY_SENTINEL;
            for (int q = 0; q < sample_count; q++) {
                if (hi_dist[q] < bd_hi) {
                    bd_hi = hi_dist[q];
                    best_hi = q;
                }
                if (lo_dist[q] < bd_lo) {
                    bd_lo = lo_dist[q];
                    best_lo = q;
                }
            }
            hi_nn[round] = best_hi;
            lo_nn[round] = best_lo;
            if (best_hi >= 0) {
                hi_dist[best_hi] = lv_NEAR_INFINITY_SENTINEL;
            }
            if (best_lo >= 0) {
                lo_dist[best_lo] = lv_NEAR_INFINITY_SENTINEL;
            }
        }

        /* 计算两个 k 近邻集合的共有元素占比 */
        int overlap = 0;
        for (int a = 0; a < k; a++) {
            if (hi_nn[a] < 0) {
                continue;
            }
            for (int b = 0; b < k; b++) {
                if (hi_nn[a] == lo_nn[b]) {
                    overlap++;
                    break;
                }
            }
        }
        total_overlap += (double) overlap / (double) k;
    }

    double ratio = total_overlap / (double) sample_count;
    ratio = lv_clamp(ratio, 0.0, 1.0);
    return ratio;
}

/* 统计可投影节点的数量（拓扑/几何指标的采样基数） */
static int high_dim_count_valid_points(HighDimManager *manager, int block_id, const ConstraintGraph *graph) {
    if (!graph) {
        return 0;
    }
    int count = 0;
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || !node->is_active || !node->symbolic_coords || node->coord_count == 0) {
            continue;
        }
        HighDimProjectedCoord pc;
        if (high_dim_project_coordinates(manager, block_id, (const SymbolicCoord **) node->symbolic_coords,
                                         node->coord_count, &pc) != lv_OK || !pc.is_valid) {
            continue;
        }
        count++;
    }
    return count;
}
/* ==================== 增强版综合保真度 ==================== */

/**
 * @brief 计算高维投影的增强保真度（综合六项细粒度指标）
 *
 * 在传统维度可见性统计之上，引入：
 *   - 第二层：约束类型敏感度加权保留率
 *   - 第三层：几何保真（1 - 几何失真）
 *
 * 综合保真度 = 0.2*维度保真 + 0.5*约束保真 + 0.3*几何保真
 *
 * @param manager  管理器指针
 * @param block_id 块 ID
 * @param graph    约束图指针（可为 NULL，此时约束/几何指标退化为维度统计）
 * @param stats    输出参数，接收可见性统计
 * @return lv_OK 成功，错误码表示失败原因
 */
int high_dim_compute_fidelity(HighDimManager *manager, int block_id, const ConstraintGraph *graph,
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

    /* 第一层：维度可见性保真 */
    double dim_fidelity = 1.0;
    if (block->dimension_count > 0) {
        dim_fidelity = (double) high_dim_count_visible_dims(manager, block_id) / (double) block->dimension_count;
    }

    /* 第二层：约束类型敏感度加权保留率 */
    double constraint_fidelity = high_dim_constraint_type_sensitivity(manager, block_id, graph);

    /* 第三层：几何保真（1 - 几何失真） */
    double geometric_distortion = high_dim_geometric_distortion(manager, block_id, graph);
    double geometric_fidelity = 1.0 - geometric_distortion;

    /* 综合加权 */
    double combined = 0.2 * dim_fidelity + 0.5 * constraint_fidelity + 0.3 * geometric_fidelity;
    combined = lv_clamp(combined, 0.0, 1.0);

    /* 填充宏观统计 */
    stats->fidelity_ratio = combined;

    stats->total_elements = graph ? graph->node_count : 0;
    stats->visible_elements = high_dim_count_valid_points(manager, block_id, graph);

    if (graph && graph->constraint_count > 0) {
        stats->total_relations = graph->constraint_count;
        int visible_dims = high_dim_count_visible_dims(manager, block_id);
        int visible_relations = 0;
        for (int i = 0; i < graph->constraint_count; i++) {
            Constraint *c = graph->constraints[i];
            if (!c || !c->is_active) {
                continue;
            }
            double weight;
            int required_dims;
            high_dim_sensitivity_lookup(c->type, &weight, &required_dims);
            if (visible_dims >= required_dims) {
                visible_relations++;
            }
        }
        stats->visible_relations = visible_relations;
    } else {
        stats->total_relations = block->dimension_count;
        stats->visible_relations = high_dim_count_visible_dims(manager, block_id);
    }

    /* 遮挡率以 MDS stress 作为真实代理指标（嵌入越扭曲，遮挡越严重） */
    stats->occlusion_rate = high_dim_mds_stress(manager, block_id, graph);
    stats->is_below_threshold = (combined < lv_high_dim_default_fidelity_threshold()) ? true : false;

    block->fidelity_ratio = combined;

    return lv_OK;
}

/**
 * @brief 计算高维投影的详细保真度（六项细粒度指标逐项输出）
 *
 * 除宏观可见性统计外，还输出：
 *   - constraint_type_sensitivity：约束类型敏感度加权保留率
 *   - geometric_distortion：几何失真（距离/角度）
 *   - local_fidelity_heatmap：局部保真度热图（GRID×GRID）
 *   - dynamic_precision_factor / dynamic_rotation_angle：动态精度建议
 *   - mds_stress：Kruskal stress-1
 *   - topology_preservation：k 近邻拓扑保持率
 *
 * @param manager  管理器指针
 * @param block_id 块 ID
 * @param graph    约束图指针（可为 NULL）
 * @param stats    输出参数，宏观统计（可为 NULL，内部使用临时对象）
 * @param detail   输出参数，详细指标（不可为 NULL）
 * @return lv_OK 成功，错误码表示失败原因
 */
int high_dim_compute_fidelity_detailed(HighDimManager *manager, int block_id, const ConstraintGraph *graph,
                                       HighDimVisibilityStats *stats, HighDimFidelityDetail *detail) {
    if (!manager || !detail) {
        return lv_ERROR_INVALID_PARAM;
    }

    HighDimVisibilityStats local_stats;
    HighDimVisibilityStats *out_stats = stats ? stats : &local_stats;

    int rc = high_dim_compute_fidelity(manager, block_id, graph, out_stats);
    if (rc != lv_OK) {
        return rc;
    }

    /* 六项细粒度指标 */
    detail->constraint_type_sensitivity = high_dim_constraint_type_sensitivity(manager, block_id, graph);
    detail->geometric_distortion = high_dim_geometric_distortion(manager, block_id, graph);
    high_dim_local_fidelity_heatmap(manager, block_id, graph, detail->local_fidelity_heatmap);
    high_dim_dynamic_precision(manager, block_id, graph, detail->geometric_distortion,
                               &detail->dynamic_precision_factor, &detail->dynamic_rotation_angle);
    detail->mds_stress = high_dim_mds_stress(manager, block_id, graph);
    detail->topology_preservation = high_dim_topology_preservation(manager, block_id, graph);

    detail->node_count = graph ? graph->node_count : 0;
    detail->valid_point_count = high_dim_count_valid_points(manager, block_id, graph);

    return lv_OK;
}