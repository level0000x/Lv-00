/**
 * @file sat_encoding_geom.c
 * @brief 几何约束→SAT 编码（由 sat_encoding.c 拆分子模块）
 *
 * @details 共线/平行/垂直/距离相等/角度相等/包含等几何约束的 CNF 编码、
 *          约束类型分发表与约束图整体编码入口。
 * @author Lv-00 Project
 * @version 3.3.0
 */
#include "lv/sat_encoding.h"

#include <stdio.h>
#include <string.h>

#include "lv/constraint_graph.h"

#include "lv/error_codes.h"
#include "lv/lv.h"
#include "lv/lv_internal.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_utils.h"
#include "lv/solver_core.h"
#include "sat_encoding_internal.h"

/** 比特编码用的整数位宽（默认 8 位，原 sat_encoding.c 内部常量移入） */
#define DEFAULT_BITWIDTH 8
/* ========================================================================
 * 约束编码规则 —— 7 种几何约束的 CNF 编码
 *
 * 每类约束通过为相关的 (node_i, node_j) 对创建布尔变量，
 * 然后编码几何关系为 CNF 子句。
 * ======================================================================== */

/**
 * @brief 为两个节点对注册或查找 SAT 变量
 *
 * @param enc      编码上下文
 * @param n1_id    节点1
 * @param n2_id    节点2
 * @param out_var1 输出变量1
 * @param out_var2 输出变量2
 * @return 成功返回0
 */
static int register_pair_var(SatEncoding *enc, int n1_id, int n2_id, int *out_var) {
    int ids[2] = {n1_id, n2_id};
    int var = sat_encoding_register_var(enc, 2, ids);
    if (var < 1)
        return -1;
    *out_var = var;
    return 0;
}

/* ── 共线性约束编码 ── */

int sat_encode_collinearity(SatEncoding *enc, int p1_id, int p2_id, int p3_id) {
    lv_CHECK_NULL(enc, -1);

    int v12, v23, v13;
    if (register_pair_var(enc, p1_id, p2_id, &v12) < 0)
        return -1;
    if (register_pair_var(enc, p2_id, p3_id, &v23) < 0)
        return -1;
    if (register_pair_var(enc, p1_id, p3_id, &v13) < 0)
        return -1;

    int clause_count = 0;

    /* 共线意味着：若 p1-p2 共线和 p2-p3 共线，则 p1-p3 共线 */
    {
        SatLiteral c1[] = {-v12, -v23, v13};
        if (sat_encoding_add_clause(enc, c1, 3) >= 0)
            clause_count++;
    }
    /* 传递性反向 */
    {
        SatLiteral c2[] = {-v12, -v13, v23};
        if (sat_encoding_add_clause(enc, c2, 3) >= 0)
            clause_count++;
    }
    {
        SatLiteral c3[] = {-v23, -v13, v12};
        if (sat_encoding_add_clause(enc, c3, 3) >= 0)
            clause_count++;
    }

    return clause_count;
}

/* ── 平行性约束编码 ── */

int sat_encode_parallelism(SatEncoding *enc, int p1_id, int p2_id, int p3_id, int p4_id) {
    lv_CHECK_NULL(enc, -1);

    int v12, v34, v_parallel;
    if (register_pair_var(enc, p1_id, p2_id, &v12) < 0)
        return -1;
    if (register_pair_var(enc, p3_id, p4_id, &v34) < 0)
        return -1;

    int ids[2] = {v12, v34};
    v_parallel = sat_encoding_register_var(enc, 2, ids);
    if (v_parallel < 1)
        return -1;

    int clause_count = 0;

    /* parallel 变量等价于两个线段方向一致，编码为双向蕴含 */
    {
        SatLiteral c1[] = {-v_parallel, v12};
        if (sat_encoding_add_clause(enc, c1, 2) >= 0)
            clause_count++;
    }
    {
        SatLiteral c2[] = {-v_parallel, v34};
        if (sat_encoding_add_clause(enc, c2, 2) >= 0)
            clause_count++;
    }
    {
        SatLiteral c3[] = {v_parallel, -v12, -v34};
        if (sat_encoding_add_clause(enc, c3, 3) >= 0)
            clause_count++;
    }

    return clause_count;
}

/* ── 垂直性约束编码 ── */

int sat_encode_perpendicularity(SatEncoding *enc, int p1_id, int p2_id, int p3_id, int p4_id) {
    lv_CHECK_NULL(enc, -1);

    int v12, v34, v_perp;
    if (register_pair_var(enc, p1_id, p2_id, &v12) < 0)
        return -1;
    if (register_pair_var(enc, p3_id, p4_id, &v34) < 0)
        return -1;

    int ids[2] = {v12, v34};
    v_perp = sat_encoding_register_var(enc, 2, ids);
    if (v_perp < 1)
        return -1;

    int clause_count = 0;

    /* 垂直等价于内积为0，建立蕴含关系 */
    {
        SatLiteral c1[] = {-v_perp, v12};
        if (sat_encoding_add_clause(enc, c1, 2) >= 0)
            clause_count++;
    }
    {
        SatLiteral c2[] = {-v_perp, v34};
        if (sat_encoding_add_clause(enc, c2, 2) >= 0)
            clause_count++;
    }
    {
        SatLiteral c3[] = {v_perp, -v12, -v34};
        if (sat_encoding_add_clause(enc, c3, 3) >= 0)
            clause_count++;
    }

    /* 垂直性与平行性不能同时成立 */
    int ids_p[2] = {v12, v34};
    int v_no_overlap = sat_encoding_register_var(enc, 2, ids_p);
    if (v_no_overlap >= 1) {
        SatLiteral c4[] = {-v_perp, -v_no_overlap};
        if (sat_encoding_add_clause(enc, c4, 2) >= 0)
            clause_count++;
    }

    return clause_count;
}

/* ── 距离相等约束编码 ── */

int sat_encode_distance_eq(SatEncoding *enc, int p1_id, int p2_id, int p3_id, int p4_id) {
    lv_CHECK_NULL(enc, -1);

    int v12, v34, v_dist_eq;
    if (register_pair_var(enc, p1_id, p2_id, &v12) < 0)
        return -1;
    if (register_pair_var(enc, p3_id, p4_id, &v34) < 0)
        return -1;

    int ids[2] = {v12, v34};
    v_dist_eq = sat_encoding_register_var(enc, 2, ids);
    if (v_dist_eq < 1)
        return -1;

    int clause_count = 0;

    /* 距离相等 = 两对都存在且等价 */
    {
        SatLiteral c1[] = {-v_dist_eq, v12};
        if (sat_encoding_add_clause(enc, c1, 2) >= 0)
            clause_count++;
    }
    {
        SatLiteral c2[] = {-v_dist_eq, v34};
        if (sat_encoding_add_clause(enc, c2, 2) >= 0)
            clause_count++;
    }
    {
        SatLiteral c3[] = {v_dist_eq, -v12, -v34};
        if (sat_encoding_add_clause(enc, c3, 3) >= 0)
            clause_count++;
    }

    return clause_count;
}

/* ── 角度相等约束编码 ── */

int sat_encode_angle_eq(SatEncoding *enc, int p1_id, int p2_id, int p3_id, int p4_id, int p5_id, int p6_id) {
    lv_CHECK_NULL(enc, -1);

    int v12, v13, v45, v46, v_angle;
    if (register_pair_var(enc, p1_id, p2_id, &v12) < 0)
        return -1;
    if (register_pair_var(enc, p1_id, p3_id, &v13) < 0)
        return -1;
    if (register_pair_var(enc, p4_id, p5_id, &v45) < 0)
        return -1;
    if (register_pair_var(enc, p4_id, p6_id, &v46) < 0)
        return -1;

    int ids[2] = {v12, v13};
    v_angle = sat_encoding_register_var(enc, 2, ids);
    if (v_angle < 1)
        return -1;

    int clause_count = 0;

    /* 角度相等：两个角的两对边都存在且分别等价 */
    {
        SatLiteral c1[] = {-v_angle, v12, -v45};
        if (sat_encoding_add_clause(enc, c1, 3) >= 0)
            clause_count++;
    }
    {
        SatLiteral c2[] = {-v_angle, v13, -v46};
        if (sat_encoding_add_clause(enc, c2, 3) >= 0)
            clause_count++;
    }
    {
        SatLiteral c3[] = {v_angle, -v12};
        if (sat_encoding_add_clause(enc, c3, 2) >= 0)
            clause_count++;
    }
    {
        SatLiteral c4[] = {v_angle, -v13};
        if (sat_encoding_add_clause(enc, c4, 2) >= 0)
            clause_count++;
    }

    return clause_count;
}

/* ── 包含关系约束编码 ── */

int sat_encode_containment(SatEncoding *enc, int p_id, int r_id) {
    lv_CHECK_NULL(enc, -1);

    int var;
    if (register_pair_var(enc, p_id, r_id, &var) < 0)
        return -1;

    /* 包含关系编码为单元子句：该对必须为真 */
    SatLiteral c1[] = {var};
    int idx = sat_encoding_add_clause(enc, c1, 1);
    return (idx >= 0) ? 1 : -1;
}

/* ========================================================================
 * VTable 约束编码器 — 函数指针表替代 switch 分发
 * ======================================================================== */

/** 约束编码函数指针类型 */
typedef int (*ConstraintEncoderFn)(SatEncoding *enc, const Constraint *con);

/** VTable 条目：约束类型到编码函数的映射 */
typedef struct {
    ConstraintType type;
    ConstraintEncoderFn fn;
} ConstraintEncoderEntry;

/* ── 各约束类型的编码器实现 ── */

static int encode_betweenness(SatEncoding *enc, const Constraint *con) {
    if (con->participant_count >= 3)
        return sat_encode_collinearity(enc, con->participants[0], con->participants[1], con->participants[2]);
    return 0;
}

static int encode_incidence(SatEncoding *enc, const Constraint *con) {
    if (con->participant_count >= 2)
        return sat_encode_containment(enc, con->participants[0], con->participants[1]);
    return 0;
}

static int encode_intersection(SatEncoding *enc, const Constraint *con) {
    if (con->participant_count >= 4)
        return sat_encode_parallelism(enc, con->participants[0], con->participants[1],
                                      con->participants[2], con->participants[3]);
    return 0;
}

static int encode_containment(SatEncoding *enc, const Constraint *con) {
    if (con->participant_count >= 2)
        return sat_encode_containment(enc, con->participants[0], con->participants[1]);
    return 0;
}

static int encode_angle(SatEncoding *enc, const Constraint *con) {
    if (con->participant_count >= 2) {
        int line1_id = con->participants[0];
        int line2_id = con->participants[1];

        /* 角度关系存在域：注册 (line1, line2) 对变量 */
        int v_angle;
        if (register_pair_var(enc, line1_id, line2_id, &v_angle) < 0)
            return -1;

        /* 计算目标离散桶索引 = floor(角度值 / 桶宽) */
        int bucket_count = 1 << DEFAULT_BITWIDTH;
        double bucket_width = lv_HALF_CIRCLE_DEG / (double) bucket_count;
        int target_bucket = (int) (con->numeric_value / bucket_width);
        if (target_bucket < 0)
            target_bucket = 0;
        if (target_bucket >= bucket_count)
            target_bucket = bucket_count - 1;

        int clause_count = 0;

        /* 两条线段必须真实参与角度关系 */
        {
            SatLiteral c_pair[] = {v_angle};
            if (sat_encoding_add_clause(enc, c_pair, 1) >= 0)
                clause_count++;
        }

        /* 角度离散位：以 (v_angle, bit) 为键注册布尔变量，
         * 并用单元子句强制每一位等于目标桶索引的二进制位 */
        for (int bit = 0; bit < DEFAULT_BITWIDTH; bit++) {
            int ids[2] = {v_angle, bit};
            int bit_var = sat_encoding_register_var(enc, 2, ids);
            if (bit_var < 1)
                return -1;

            int bit_value = (target_bucket >> bit) & 1;
            SatLiteral lit = bit_value ? (SatLiteral) bit_var : (SatLiteral) -bit_var;
            if (sat_encoding_add_clause(enc, &lit, 1) >= 0)
                clause_count++;
        }

        return clause_count;
    }
    return 0;
}

static int encode_connection(SatEncoding *enc, const Constraint *con) {
    if (con->participant_count >= 2) {
        int var;
        if (register_pair_var(enc, con->participants[0], con->participants[1], &var) < 0)
            return -1;
        SatLiteral c1[] = {var};
        int idx = sat_encoding_add_clause(enc, c1, 1);
        return (idx >= 0) ? 1 : -1;
    }
    return 0;
}

/* ── VTable 映射表 ── */

static const ConstraintEncoderEntry constraint_encoders[] = {
    {BETWEENNESS,  encode_betweenness},
    {INCIDENCE,    encode_incidence},
    {INTERSECTION, encode_intersection},
    {CONTAINMENT,  encode_containment},
    {ANGLE,        encode_angle},
    {CONNECTION,   encode_connection},
};

/* ── 通用约束编码（根据约束类型分发）── */

int sat_encode_constraint(SatEncoding *enc, int constraint_id) {
    lv_CHECK_NULL(enc, -1);
    if (!enc->graph) {
        lv_set_error_ctx(lv_ERROR_INVALID_PARAM, __FILE__, __LINE__, __func__, "编码上下文中没有关联约束图");
        return -1;
    }

    const Constraint *con = graph_get_constraint(enc->graph, constraint_id);
    if (!con) {
        lv_set_error_ctx(lv_ERROR_NOT_FOUND, __FILE__, __LINE__, __func__, "约束 ID=%d 未找到", constraint_id);
        return -1;
    }

    /* VTable 查找：遍历映射表，找到匹配的约束类型并调用对应的编码函数 */
    for (int i = 0; i < (int)(sizeof(constraint_encoders) / sizeof(constraint_encoders[0])); i++) {
        if (constraint_encoders[i].type == con->type) {
            return constraint_encoders[i].fn(enc, con);
        }
    }

    lv_LOG_WARNING("Unknown constraint type %d in sat_encode_constraint", con->type);

    return 0;
}

/* ========================================================================
 * 约束图 → SAT 编码（主编码管道）
 * ======================================================================== */

SatResult constraint_graph_to_sat(const ConstraintGraph *graph, SatEncoding *enc) {
    lv_CHECK_NULL(graph, SAT_ERROR);
    lv_CHECK_NULL(enc, SAT_ERROR);

    enc->graph = (ConstraintGraph *) graph;

    /* 遍历所有约束，逐个编码 */
    for (int i = 0; i < graph->constraint_count; i++) {
        if (!graph->constraints[i])
            continue;
        int ret = sat_encode_constraint(enc, graph->constraints[i]->id);
        if (ret < 0) {
            lv_set_error_ctx(lv_ERROR_INTERNAL, __FILE__, __LINE__, __func__, "约束 ID=%d 编码失败",
                             graph->constraints[i]->id);
            return SAT_ERROR;
        }
    }

    return SAT_OK;
}

