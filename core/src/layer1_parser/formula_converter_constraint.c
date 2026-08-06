/**
 * @file formula_converter_constraint.c
 * @brief 公式转换器实现 —— 约束转换：垂直/平行/中点/角度
 *
 * @details 由 formula_converter.c 按功能边界拆分而来，
 *          属于公式 AST 与约束图双向转换的一部分。
 *
 * @author Lv-00 Project
 * @version 3.0.1
 */

#include "lv/lv_platform.h"
#include "formula_converter.h"
#include "formula_converter_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "formula_renderer.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/lv_numeric.h"
#include "stream.h"
#include "stream_context_util.h"

/* ============================================================
 * 约束转换
 * ============================================================ */

bool formula_convert_perpendicular(const FormulaNode *constraint_node, ConstraintGraph *graph, int *out_constraint_id) {
    if (!constraint_node || constraint_node->type != NODE_CONSTRAINT_PERPENDICULAR || !graph || !out_constraint_id) {
        return false;
    }

    if (constraint_node->data.constraint.participant_count < 3) {
        return false;
    }

    /* 获取三个点 ID */
    int p1_id = -1, p2_id = -1, p3_id = -1;

    if (constraint_node->data.constraint.participants[0]->type == NODE_IDENTIFIER) {
        p1_id = formula_get_node_id(constraint_node->data.constraint.participants[0]->data.identifier.name);
    }
    if (constraint_node->data.constraint.participants[1]->type == NODE_IDENTIFIER) {
        p2_id = formula_get_node_id(constraint_node->data.constraint.participants[1]->data.identifier.name);
    }
    if (constraint_node->data.constraint.participants[2]->type == NODE_IDENTIFIER) {
        p3_id = formula_get_node_id(constraint_node->data.constraint.participants[2]->data.identifier.name);
    }

    if (p1_id < 0 || p2_id < 0 || p3_id < 0) {
        return false;
    }

    /* 添加 betweenness 约束（简化处理） */
    AddConstraintResult result = graph_add_betweenness(graph, p1_id, p2_id, p3_id);

    if (result != ADD_CONSTRAINT_OK) {
        return false;
    }

    *out_constraint_id = graph_get_constraint_count(graph) - 1;

    return true;
}

bool formula_convert_parallel(const FormulaNode *constraint_node, ConstraintGraph *graph, int *out_constraint_id) {
    if (!constraint_node || constraint_node->type != NODE_CONSTRAINT_PARALLEL || !graph || !out_constraint_id) {
        return false;
    }

    if (constraint_node->data.constraint.participant_count < 2) {
        return false;
    }

    /* 获取两条线 ID */
    int l1_id = -1, l2_id = -1;

    if (constraint_node->data.constraint.participants[0]->type == NODE_IDENTIFIER) {
        l1_id = formula_get_node_id(constraint_node->data.constraint.participants[0]->data.identifier.name);
    }
    if (constraint_node->data.constraint.participants[1]->type == NODE_IDENTIFIER) {
        l2_id = formula_get_node_id(constraint_node->data.constraint.participants[1]->data.identifier.name);
    }

    if (l1_id < 0 || l2_id < 0) {
        return false;
    }

    /* 平行约束目前使用 containment 简化表示 */
    /* 注意：实际实现可能需要扩展 ConstraintType */
    AddConstraintResult result = graph_add_containment(graph, l1_id, l2_id);

    if (result != ADD_CONSTRAINT_OK) {
        return false;
    }

    *out_constraint_id = graph_get_constraint_count(graph) - 1;

    return true;
}

bool formula_convert_midpoint(const FormulaNode *constraint_node, ConstraintGraph *graph, int *out_node_id) {
    if (!constraint_node || constraint_node->type != NODE_CONSTRAINT_MIDPOINT || !graph || !out_node_id) {
        return false;
    }

    if (constraint_node->data.constraint.participant_count < 3) {
        return false;
    }

    /* 获取中点 M 和端点 A, B */
    int m_id = -1, a_id = -1, b_id = -1;

    if (constraint_node->data.constraint.participants[0]->type == NODE_IDENTIFIER) {
        m_id = formula_get_node_id(constraint_node->data.constraint.participants[0]->data.identifier.name);
    }
    if (constraint_node->data.constraint.participants[1]->type == NODE_IDENTIFIER) {
        a_id = formula_get_node_id(constraint_node->data.constraint.participants[1]->data.identifier.name);
    }
    if (constraint_node->data.constraint.participants[2]->type == NODE_IDENTIFIER) {
        b_id = formula_get_node_id(constraint_node->data.constraint.participants[2]->data.identifier.name);
    }

    if (a_id < 0 || b_id < 0) {
        return false;
    }

    /* 获取 A 和 B 的坐标，计算中点 */
    GeomNode *node_a = graph_get_node(graph, a_id);
    GeomNode *node_b = graph_get_node(graph, b_id);

    if (!node_a || !node_b) {
        return false;
    }

    /* 计算中点坐标 */
    SymbolicCoord *mid_coords[2] = {NULL, NULL};

    if (node_a->symbolic_coords && node_b->symbolic_coords && node_a->coord_count >= 2 && node_b->coord_count >= 2) {
        mid_coords[0] = symbolic_coord_add(node_a->symbolic_coords[0], node_b->symbolic_coords[0]);
        mid_coords[1] = symbolic_coord_add(node_a->symbolic_coords[1], node_b->symbolic_coords[1]);

        /* 除以 2 */
        SymbolicCoord *half = symbolic_coord_create_rational(1, 2);
        SymbolicCoord *tmp;

        tmp = symbolic_coord_multiply(mid_coords[0], half);
        symbolic_coord_destroy(mid_coords[0]);
        mid_coords[0] = tmp;

        tmp = symbolic_coord_multiply(mid_coords[1], half);
        symbolic_coord_destroy(mid_coords[1]);
        mid_coords[1] = tmp;

        symbolic_coord_destroy(half);
    }

    if (!mid_coords[0] || !mid_coords[1]) {
        /* 使用默认坐标 */
        mid_coords[0] = symbolic_coord_create_rational(0, 1);
        mid_coords[1] = symbolic_coord_create_rational(0, 1);
    }

    /* 如果中点 M 已存在，更新其坐标 */
    if (m_id >= 0) {
        GeomNode *node_m = graph_get_node(graph, m_id);
        if (node_m && node_m->symbolic_coords) {
            int update_count = node_m->coord_count < 2 ? node_m->coord_count : 2;
            for (int i = 0; i < update_count; i++) {
                symbolic_coord_destroy(node_m->symbolic_coords[i]);
                node_m->symbolic_coords[i] = mid_coords[i];
            }
            /* 释放未使用的坐标，避免内存泄漏 */
            for (int i = update_count; i < 2; i++) {
                symbolic_coord_destroy(mid_coords[i]);
            }
        } else {
            /* 节点不存在或无坐标数组，释放所有 mid_coords */
            symbolic_coord_destroy(mid_coords[0]);
            symbolic_coord_destroy(mid_coords[1]);
        }
        *out_node_id = m_id;
    } else {
        /* 创建新的中点 */
        AddNodeResult result = graph_add_point(graph, mid_coords, 2);
        if (result == ADD_NODE_OK) {
            *out_node_id = graph_get_node_count(graph) - 1;
            GeomNode *new_node = graph_get_node(graph, *out_node_id);
            if (new_node) {
                *out_node_id = new_node->id;
            }

            /* 如果有中点名，记录映射 */
            if (constraint_node->data.constraint.participants[0]->type == NODE_IDENTIFIER) {
                formula_set_node_id(constraint_node->data.constraint.participants[0]->data.identifier.name,
                                    *out_node_id);
            }
        }

        symbolic_coord_destroy(mid_coords[0]);
        symbolic_coord_destroy(mid_coords[1]);
    }

    return true;
}

/**
 * @brief 转换角度约束到约束图
 *
 * 角度约束 ∠ABC = θ 可以转换为向量点积约束：
 *   向量 BA = A - B, 向量 BC = C - B
 *   BA · BC = |BA| * |BC| * cos(θ)
 *
 * 由于约束图当前不支持直接的代数方程约束，这里使用 betweenness 约束
 * 作为近似表示，并将角度信息存储在约束的附加数据中。
 * 后续求解器可以读取这些信息进行精确的角度约束求解。
 *
 * @param[in]  constraint_node 约束节点（类型须为 NODE_CONSTRAINT_ANGLE）
 * @param[in]  graph           目标图
 * @param[out] out_constraint_id 输出约束ID
 * @return 成功返回 true，失败返回 false
 */
bool formula_convert_angle(const FormulaNode *constraint_node, ConstraintGraph *graph, int *out_constraint_id) {
    if (!constraint_node || constraint_node->type != NODE_CONSTRAINT_ANGLE || !graph || !out_constraint_id) {
        return false;
    }

    /*
     * 角度约束参与者格式：
     *   participants[0] = 点 A（角的第一个端点）
     *   participants[1] = 点 B（角的顶点）
     *   participants[2] = 点 C（角的第二个端点）
     *   participants[3] = 角度值 θ（数字节点，可选，默认 90 度）
     */

    if (constraint_node->data.constraint.participant_count < 3) {
        return false;
    }

    /* 获取三个点 ID */
    int a_id = -1, b_id = -1, c_id = -1;

    if (constraint_node->data.constraint.participants[0]->type == NODE_IDENTIFIER) {
        a_id = formula_get_node_id(constraint_node->data.constraint.participants[0]->data.identifier.name);
    }
    if (constraint_node->data.constraint.participants[1]->type == NODE_IDENTIFIER) {
        b_id = formula_get_node_id(constraint_node->data.constraint.participants[1]->data.identifier.name);
    }
    if (constraint_node->data.constraint.participants[2]->type == NODE_IDENTIFIER) {
        c_id = formula_get_node_id(constraint_node->data.constraint.participants[2]->data.identifier.name);
    }

    if (a_id < 0 || b_id < 0 || c_id < 0) {
        return false;
    }

    /* 提取角度值（弧度） */
    double angle_rad = M_PI / 2.0; /* 默认 90 度 */

    if (constraint_node->data.constraint.participant_count >= 4) {
        const FormulaNode *angle_node = constraint_node->data.constraint.participants[3];
        if (angle_node && angle_node->type == NODE_NUMBER) {
            double angle_deg;
            if (angle_node->data.number.is_integer) {
                angle_deg = (double) angle_node->data.number.numerator;
            } else {
                angle_deg = (double) angle_node->data.number.numerator / (double) angle_node->data.number.denominator;
            }
            angle_rad = angle_deg * M_PI / 180.0;
        }
    }

    /*
     * 计算向量点积约束参数：
     *   BA · BC = |BA| * |BC| * cos(θ)
     *
     * 展开为坐标形式（设 A=(ax,ay), B=(bx,by), C=(cx,cy)）：
     *   (ax-bx)(cx-bx) + (ay-by)(cy-by) = |BA|*|BC|*cos(θ)
     *
     * 这是一个二次方程，约束图无法直接表示。
     * 使用 ANGLE 约束类型，将角度信息存储在 numeric_value 中（度）。
     */

    if (formula_converter_stream_ctx) {
        stream_emit_warning(formula_converter_stream_ctx, "角度约束转换为 ANGLE 约束（数值近似）", 0);
    }

    /* 将弧度转换为度 */
    double angle_deg = angle_rad * 180.0 / M_PI;

    /* 创建两条线段 AB 和 BC */
    AddNodeResult seg_ab = graph_add_line_segment(graph, a_id, b_id);
    if (seg_ab != ADD_NODE_OK) {
        return false;
    }
    int seg_ab_id = graph_get_last_added_node_id(graph);

    AddNodeResult seg_bc = graph_add_line_segment(graph, b_id, c_id);
    if (seg_bc != ADD_NODE_OK) {
        return false;
    }
    int seg_bc_id = graph_get_last_added_node_id(graph);

    AddConstraintResult result = graph_add_angle(graph, seg_ab_id, seg_bc_id, angle_deg);

    if (result != ADD_CONSTRAINT_OK) {
        return false;
    }

    *out_constraint_id = graph_get_constraint_count(graph) - 1;

    /* 将角度约束的详细信息存储到约束节点上 */
    Constraint *constraint = graph_get_constraint(graph, *out_constraint_id);
    if (constraint) {
        /*
         * 角度信息已存储在 constraint->numeric_value 中。
         * 创建一个辅助点节点存储 cos(θ) 和 sin(θ) 以备后续代数求解。
         */
    }

    /* 创建一个辅助节点来存储角度约束的代数信息 */
    {
        double cos_theta = cos(angle_rad);
        double sin_theta = sin(angle_rad);

        /* 使用两个坐标存储 cos(θ) 和 sin(θ) */
        SymbolicCoord *angle_coords[2];
        angle_coords[0] = symbolic_coord_from_double_scaled(cos_theta, lv_RATIONAL_SCALE_DEFAULT);
        angle_coords[1] = symbolic_coord_from_double_scaled(sin_theta, lv_RATIONAL_SCALE_DEFAULT);

        AddNodeResult add_result = graph_add_point(graph, angle_coords, 2);
        symbolic_coord_destroy(angle_coords[0]);
        symbolic_coord_destroy(angle_coords[1]);

        if (add_result == ADD_NODE_OK) {
            int aux_id = graph->next_node_id - 1;
            GeomNode *aux_node = graph_get_node(graph, aux_id);
            if (aux_node) {
                if (aux_node->numeric_assumption_declaration) {
                    lv_free((void **) &aux_node->numeric_assumption_declaration); /* 统一内存释放器 */
                    aux_node->numeric_assumption_declaration = NULL;
                }
                /*
                 * 格式: ANGLE_CONSTRAINT:A_id:B_id:C_id:angle_rad:cos:sin
                 * 求解器可解析此字符串获取完整的角度约束信息。
                 */
                char buf[FORMULA_BUF_SIZE];
                snprintf(buf, sizeof(buf), "ANGLE_CONSTRAINT:%d:%d:%d:%.10g:%.10g:%.10g", a_id, b_id, c_id, angle_rad,
                         cos_theta, sin_theta);
                aux_node->numeric_assumption_declaration = lv_strdup_safe(buf);
            }
        }
    }

    return true;
}
