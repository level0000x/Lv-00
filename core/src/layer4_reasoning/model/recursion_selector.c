/**
 * @file recursion_selector.c
 * @brief selector block API
 * @details Split from recursion.c
 */

#include "lv/lv_platform.h"
#include "lv/recursion.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_numeric.h"
#include "lv/geo_utils.h"
#include "lv/lv_internal.h"
#include "lv/lv_xmacro.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"
#include "lv/stream.h"
#include "recursion_internal.h"

/* ============== 选择器块API ============== */

/* 辅助函数：检查点是否在线段上（符号计算） */
static bool point_on_segment_symbolic(SymbolicCoord *px, SymbolicCoord *py, SymbolicCoord *x1, SymbolicCoord *y1,
                                      SymbolicCoord *x2, SymbolicCoord *y2) {
    /* 计算向量 (P-A) 和 (B-A) */
    SymbolicCoord *pax = symbolic_coord_subtract(px, x1);
    SymbolicCoord *pay = symbolic_coord_subtract(py, y1);
    SymbolicCoord *bax = symbolic_coord_subtract(x2, x1);
    SymbolicCoord *bay = symbolic_coord_subtract(y2, y1);

    if (!pax || !pay || !bax || !bay) {
        symbolic_coord_destroy(pax);
        symbolic_coord_destroy(pay);
        symbolic_coord_destroy(bax);
        symbolic_coord_destroy(bay);
        return false;
    }

    /* 检查叉积是否为零（三点共线）：(P-A) × (B-A) = 0 */
    /* 叉积 = pax * bay - pay * bax */
    SymbolicCoord *cross1 = symbolic_coord_multiply(pax, bay);
    SymbolicCoord *cross2 = symbolic_coord_multiply(pay, bax);
    SymbolicCoord *cross = symbolic_coord_subtract(cross1, cross2);

    symbolic_coord_destroy(cross1);
    symbolic_coord_destroy(cross2);

    bool is_collinear = false;
    if (cross) {
        SymbolicCoord *zero = symbolic_coord_create_rational(0, 1);
        int cmp = symbolic_coord_compare(cross, zero);
        is_collinear = (cmp == 0);
        symbolic_coord_destroy(zero);
        symbolic_coord_destroy(cross);
    }

    if (!is_collinear) {
        symbolic_coord_destroy(pax);
        symbolic_coord_destroy(pay);
        symbolic_coord_destroy(bax);
        symbolic_coord_destroy(bay);
        return false;
    }

    /* 检查点是否在线段范围内：0 <= t <= 1，其中 P = A + t*(B-A) */
    /* 使用数值方法检查范围 */
    double px_val = symbolic_coord_to_double(px);
    double py_val = symbolic_coord_to_double(py);
    double x1_val = symbolic_coord_to_double(x1);
    double y1_val = symbolic_coord_to_double(y1);
    double x2_val = symbolic_coord_to_double(x2);
    double y2_val = symbolic_coord_to_double(y2);

    symbolic_coord_destroy(pax);
    symbolic_coord_destroy(pay);
    symbolic_coord_destroy(bax);
    symbolic_coord_destroy(bay);

    /* 检查点是否在边界框内 */
    const double epsilon = 1e-10;
    return geo_bbox_contains_2d(px_val, py_val, x1_val, y1_val, x2_val, y2_val, epsilon);
}

/* 辅助函数：计算从点到线段端点的有向角度 */
static double compute_angle(double px, double py, double x1, double y1, double x2, double y2) {
    double dx1 = x1 - px;
    double dy1 = y1 - py;
    double dx2 = x2 - px;
    double dy2 = y2 - py;

    /* 检查零向量：若参考点与端点重合，atan2(0,0) 返回 NaN */
    if ((dx1 == 0.0 && dy1 == 0.0) || (dx2 == 0.0 && dy2 == 0.0)) {
        return 0.0;
    }

    double angle1 = atan2(dy1, dx1);
    double angle2 = atan2(dy2, dx2);
    double diff = angle2 - angle1;

    /* 归一化到 [-π, π]（共享设施 lv_angle_diff_pi，端点 ±π 保持不变） */
    diff = lv_angle_diff_pi(diff);

    return diff;
}

/* 辅助函数：使用卷绕数算法判断点是否在区域内 */
static int compute_winding_number(double px, double py, GeomNode **segments, int seg_count) {
    double total_angle = 0.0;

    for (int i = 0; i < seg_count; i++) {
        GeomNode *seg = segments[i];
        if (!seg || seg->type != GEOM_LINE_SEGMENT || seg->coord_count < 4)
            continue;

        double x1, y1, x2, y2;
        if (!symbolic_coord_get_segment(seg->symbolic_coords, seg->coord_count, &x1, &y1, &x2, &y2))
            continue;

        total_angle += compute_angle(px, py, x1, y1, x2, y2);
    }

    /* 卷绕数 = total_angle / (2π) */
    /* 如果卷绕数不为零，点在内部 */
    return (int) round(total_angle / (2 * lv_PI));
}

/* 辅助函数：检查点是否在区域边界上 */
static bool point_on_region_boundary(GeomNode *point, GeomNode *region) {
    if (!point || !region || region->type != GEOM_REGION)
        return false;
    if (!region->data.region.boundary_segments || region->data.region.segment_count <= 0)
        return false;
    if (point->coord_count < 2)
        return false;

    SymbolicCoord *px = point->symbolic_coords[0];
    SymbolicCoord *py = point->symbolic_coords[1];

    for (int i = 0; i < region->data.region.segment_count; i++) {
        GeomNode *seg = region->data.region.boundary_segments[i];
        if (!seg || seg->type != GEOM_LINE_SEGMENT || seg->coord_count < 4)
            continue;

        if (point_on_segment_symbolic(px, py, seg->symbolic_coords[0], seg->symbolic_coords[1], seg->symbolic_coords[2],
                                      seg->symbolic_coords[3])) {
            return true;
        }
    }

    return false;
}

SelectorBlock *selector_block_create(int id, ConstraintGraph *graph) {
    SelectorBlock *sb = lv_calloc(1, sizeof(SelectorBlock));
    if (!sb)
        return NULL;

    sb->id = id;
    sb->graph = graph;
    sb->true_state = BRANCH_PENDING;
    sb->false_state = BRANCH_PENDING;

    /* 修改3：初始化分支子图字段 */
    sb->true_branch_node_ids = NULL;
    sb->true_branch_node_count = 0;
    sb->false_branch_node_ids = NULL;
    sb->false_branch_node_count = 0;

    return sb;
}

void selector_block_destroy(SelectorBlock *sb) {
    if (!sb)
        return;

    /* 修改3：释放分支子图节点数组 */
    lv_free((void **) &sb->true_branch_node_ids);
    lv_free((void **) &sb->false_branch_node_ids);

    lv_free((void **) &sb);
}

bool selector_block_set_condition(SelectorBlock *sb, int point_id, int region_id) {
    if (!sb)
        return false;

    sb->test_point_id = point_id;
    sb->test_region_id = region_id;
    return true;
}

bool selector_block_set_branches(SelectorBlock *sb, int true_root, int false_root) {
    if (!sb)
        return false;

    sb->true_branch_root_id = true_root;
    sb->false_branch_root_id = false_root;
    return true;
}

/* ============== 修改3：分支子图管理 ============== */

void selector_block_set_branch_nodes(SelectorBlock *sb, int *true_ids, int true_count, int *false_ids,
                                     int false_count) {
    if (!sb)
        return;

    /* 释放旧的真分支节点数组 */
    lv_free((void **) &sb->true_branch_node_ids);

    /* 复制新的真分支节点ID数组 */
    if (true_ids && true_count > 0) {
        sb->true_branch_node_ids = lv_calloc(true_count, sizeof(int));
        if (sb->true_branch_node_ids) {
            memcpy(sb->true_branch_node_ids, true_ids, true_count * sizeof(int));
            sb->true_branch_node_count = true_count;
        } else {
            sb->true_branch_node_count = 0;
        }
    } else {
        sb->true_branch_node_ids = NULL;
        sb->true_branch_node_count = 0;
    }

    /* 释放旧的假分支节点数组 */
    lv_free((void **) &sb->false_branch_node_ids);

    /* 复制新的假分支节点ID数组 */
    if (false_ids && false_count > 0) {
        sb->false_branch_node_ids = lv_calloc(false_count, sizeof(int));
        if (sb->false_branch_node_ids) {
            memcpy(sb->false_branch_node_ids, false_ids, false_count * sizeof(int));
            sb->false_branch_node_count = false_count;
        } else {
            sb->false_branch_node_count = 0;
        }
    } else {
        sb->false_branch_node_ids = NULL;
        sb->false_branch_node_count = 0;
    }
}

const int *selector_block_get_branch_nodes(SelectorBlock *sb, bool is_true_branch, int *out_count) {
    if (!sb || !out_count)
        return NULL;

    if (is_true_branch) {
        *out_count = sb->true_branch_node_count;
        return sb->true_branch_node_ids;
    } else {
        *out_count = sb->false_branch_node_count;
        return sb->false_branch_node_ids;
    }
}

bool selector_block_evaluate(SelectorBlock *sb, ConstraintGraph *graph) {
    if (!sb || !graph)
        return false;

    GeomNode *point = graph_get_node(graph, sb->test_point_id);
    GeomNode *region = graph_get_node(graph, sb->test_region_id);

    if (!point || !region || region->type != GEOM_REGION) {
        /* 无法判定，两个分支都保持待定（修改3：使用 BRANCH_PENDING） */
        sb->true_state = BRANCH_PENDING;
        sb->false_state = BRANCH_PENDING;
        return false;
    }

    /* 检查点是否在区域内 - 使用混合符号/数值方法 */

    /* 第一步：检查点是否在边界上（符号计算） */
    if (point_on_region_boundary(point, region)) {
        /* 点在边界上，严格来说不在区域内部 */
        /* 根据定义，边界点不算在内部 */
        /* 修改3：真分支变为虚影，假分支激活 */
        sb->true_state = BRANCH_SHADOWED;
        sb->false_state = BRANCH_ACTIVE;
        return true;
    }

    bool is_inside = false;

    if (region->data.region.boundary_segments && region->data.region.segment_count > 0) {
        double px, py;
        if (!symbolic_coord_get_xy(point->symbolic_coords, point->coord_count, &px, &py))
            return false;

        /* 第二步：使用卷绕数算法（更稳健） */
        int winding =
            compute_winding_number(px, py, region->data.region.boundary_segments, region->data.region.segment_count);

        if (winding != 0) {
            is_inside = true;
        } else {
            /* 第三步：使用射线法作为备用验证 */
            /* 这可以处理一些卷绕数算法可能遗漏的边缘情况 */
            is_inside = geo_point_in_region_segments(px, py, region->data.region.boundary_segments,
                                                     region->data.region.segment_count);
        }

        /* 第四步：根据信任颜色设置分支状态 */
        /* 如果点或区域有非绿色信任级别，标记为待定 */
        if (point->trust != TRUST_GREEN || region->trust != TRUST_GREEN) {
            /* 对于非完全构造的几何体，结果可能不可靠 */
            /* 但仍然给出判断，只是需要用户注意信任级别 */
            /* 这里我们仍然使用计算结果，但可以在日志中记录警告 */
        }
    }

    /* 流式事件：选择器块评估结果 */
    if (recursion_stream_ctx) {
        stream_emit_simple(recursion_stream_ctx, STREAM_EVENT_INFO,
                           is_inside ? "选择器块评估：真分支激活" : "选择器块评估：假分支激活", sb->id);
    }

    /* 修改3：评估后根据结果设置分支状态 */
    if (is_inside) {
        /* 真分支激活，假分支变为虚影 */
        sb->true_state = BRANCH_ACTIVE;
        sb->false_state = BRANCH_SHADOWED;
    } else {
        /* 假分支激活，真分支变为虚影 */
        sb->true_state = BRANCH_SHADOWED;
        sb->false_state = BRANCH_ACTIVE;
    }

    /* 根据 design_v2.9.md 第 9.5 节管理分支子图节点：
     * 活跃分支节点保持 TRUST_GREEN（完全构造）。
     * 虚影分支节点标记为 TRUST_BLUE_UNEXPLORED（幽灵/虚拟）。 */
    {
        int *active_ids = is_inside ? sb->true_branch_node_ids : sb->false_branch_node_ids;
        int active_count = is_inside ? sb->true_branch_node_count : sb->false_branch_node_count;
        int *shadowed_ids = is_inside ? sb->false_branch_node_ids : sb->true_branch_node_ids;
        int shadowed_count = is_inside ? sb->false_branch_node_count : sb->true_branch_node_count;

        for (int i = 0; i < active_count; i++) {
            if (active_ids[i] < 0)
                continue;
            GeomNode *node = graph_get_node(graph, active_ids[i]);
            if (node) {
                node->trust = TRUST_GREEN;
            }
        }
        for (int i = 0; i < shadowed_count; i++) {
            if (shadowed_ids[i] < 0)
                continue;
            GeomNode *node = graph_get_node(graph, shadowed_ids[i]);
            if (node) {
                node->trust = TRUST_BLUE_UNEXPLORED;
            }
        }
    }

    return true;
}

int selector_block_get_active_branch(SelectorBlock *sb) {
    if (!sb)
        return -1;

    if (sb->true_state == BRANCH_ACTIVE) {
        return sb->true_branch_root_id;
    } else if (sb->false_state == BRANCH_ACTIVE) {
        return sb->false_branch_root_id;
    }

    return -1; /* 无活跃分支 */
}

void selector_block_update_states(SelectorBlock *sb, BranchState true_state, BranchState false_state) {
    if (sb) {
        sb->true_state = true_state;
        sb->false_state = false_state;
    }
}

/* ============== 符号测度验证 ============== */

/* 测度比较结果 → 递归检查结果 静态查找表 */
static const RecursionCheckResult kCompareToCheckTable[] = {
    [MEASURE_LESS]    = RECURSION_OK,             /* 严格递减 → 有效 */
    [MEASURE_EQUAL]   = RECURSION_NOT_DECREASING, /* 相等 → 未递减 */
    [MEASURE_GREATER] = RECURSION_NOT_DECREASING, /* 增大 → 未递减 */
    [MEASURE_UNKNOWN] = RECURSION_ERROR,          /* 无法比较 → 出错 */
    [MEASURE_ERROR]   = RECURSION_ERROR,          /* 比较出错 → 出错 */
};

RecursionCheckResult recursion_validate_measure(const RecursionContext *ctx, const Measure *measure,
                                                const ConstraintGraph *graph, int node_id) {
    if (!ctx || !measure || !graph || node_id < 0)
        return RECURSION_ERROR;

    /* 获取目标节点（需要 const_cast，因为 graph_get_node 不接受 const） */
    GeomNode *node = graph_get_node((ConstraintGraph *) graph, node_id);
    if (!node)
        return RECURSION_ERROR;

    /* 计算当前节点的测度值（需要 const_cast，因为底层 API 不接受 const） */
    SymbolicCoord *current_value = measure_compute_value((Measure *) measure, node, (ConstraintGraph *) graph);
    if (!current_value) {
        /* 如果符号测度无法计算，尝试纯符号版本 */
        current_value = measure_compute_value_symbolic((Measure *) measure, node, (ConstraintGraph *) graph);
    }
    if (!current_value)
        return RECURSION_ERROR;

    /* 如果上下文中没有历史测度值，这是第一次调用，无法比较 */
    if (ctx->measure_value_count == 0) {
        symbolic_coord_destroy(current_value);
        return RECURSION_ERROR;
    }

    /* 获取上下文中的前一个测度值（最近一次记录的） */
    SymbolicCoord *prev_value = ctx->measure_values[ctx->measure_value_count - 1];

    /* 比较当前值与前一个值 */
    MeasureCompareResult cmp = measure_compare((Measure *) measure, current_value, prev_value);

    symbolic_coord_destroy(current_value);

    /* 流式事件：测度验证结果 */
    if (recursion_stream_ctx) {
        stream_emit_simple(recursion_stream_ctx, STREAM_EVENT_PROGRESS,
                           cmp == MEASURE_LESS ? "测度验证通过" : "测度验证失败", node_id);
    }

    /* 返回结果：查静态查找表，越界保守回退为出错 */
    if ((unsigned)cmp < sizeof(kCompareToCheckTable) / sizeof(kCompareToCheckTable[0]))
        return kCompareToCheckTable[cmp];
    return RECURSION_ERROR;
}

/* ============== 选择器块分支管理增强 ============== */

int selector_block_count_branch_nodes(const SelectorBlock *sb, int *out_true_count, int *out_false_count) {
    if (!sb || !out_true_count || !out_false_count)
        return -1;

    *out_true_count = sb->true_branch_node_count;
    *out_false_count = sb->false_branch_node_count;

    return 0;
}

bool selector_block_validate_branches(const SelectorBlock *sb) {
    if (!sb)
        return false;

    /* 如果任一分支没有节点，视为互斥（空集与任何集互斥） */
    if (sb->true_branch_node_count == 0 || sb->false_branch_node_count == 0) {
        return true;
    }

    /* 检查真分支和假分支的节点ID集合是否有交集 */
    /* 使用简单的双重循环检查 */
    for (int i = 0; i < sb->true_branch_node_count; i++) {
        int true_id = sb->true_branch_node_ids[i];
        for (int j = 0; j < sb->false_branch_node_count; j++) {
            if (true_id == sb->false_branch_node_ids[j]) {
                /* 发现公共节点，分支不互斥 */
                return false;
            }
        }
    }

    /* 无交集，分支互斥 */
    return true;
}
