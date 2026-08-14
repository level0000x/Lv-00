/**
 * @file geometry_transform_analysis.c
 * @brief 变换性质分析与特殊变换识别（由 geometry_transform.c 拆分子模块）
 *
 * @details 等距/保向判定、不动点、逆变换、相等比较、点对称性识别与
 *          变换序列化（字符串/JSON）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/lv_platform.h"
#include "lv/lv_internal.h"

#include "lv/geometry_transform.h"

#include "lv/constraint_graph.h"
#include "lv/lv_numeric.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_str_utils.h"
#include "lv/symbolic_coord.h"
#include "lv/lv_utils.h"
#include "lv/lv_xmacro.h" /* LV_DISPATCH_VOID */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============== 变换性质分析实现 ============== */

/* ── 2x2 伴随矩阵/行列式辅助（mpq 精确运算） ── */

/**
 * @brief 计算 2x2 矩阵的伴随矩阵与行列式（mpq 精确运算）
 *
 * 对 M = [[m00, m01], [m10, m11]]：
 *   det = m00*m11 - m01*m10
 *   adj = [[m11, -m01], [-m10, m00]]   （伴随矩阵，未除以 det）
 *
 * 供 lv_transform_find_fixed_point 与 lv_transform_inverse 共用，
 * 消除两处重复的 2x2 行列式/伴随计算。mpq 为精确有理数运算，
 * 提取前后数值语义完全一致。
 *
 * @param m00, m01, m10, m11      矩阵元素
 * @param det                     输出行列式
 * @param adj00, adj01, adj10, adj11 输出伴随矩阵元素
 */
static void lv_mpq_2x2_adj_det(const mpq_t m00, const mpq_t m01, const mpq_t m10, const mpq_t m11, mpq_t det,
                               mpq_t adj00, mpq_t adj01, mpq_t adj10, mpq_t adj11) {
    mpq_t temp;
    mpq_init(temp);
    mpq_mul(det, m00, m11);
    mpq_mul(temp, m01, m10);
    mpq_sub(det, det, temp);
    mpq_clear(temp);

    mpq_set(adj00, m11);
    mpq_neg(adj01, m01);
    mpq_neg(adj10, m10);
    mpq_set(adj11, m00);
}

bool lv_transform_is_isometry(const lvTransform *t) {
    return t ? t->is_isometry : false;
}

bool lv_transform_is_orientation_preserving(const lvTransform *t) {
    return t ? t->is_orientation_preserving : false;
}

bool lv_transform_find_fixed_point(const lvTransform *t, mpq_t out_x, mpq_t out_y) {
    if (!t || !t->matrix_valid) {
        return false;
    }

    /* 求解 (x, y) = M * (x, y) */
    /* x = a*x + b*y + tx => (1-a)*x - b*y = tx */
    /* y = c*x + d*y + ty => -c*x + (1-d)*y = ty */
    /* 系数矩阵 [[1-a, -b], [-c, 1-d]]，伴随/行列式由 lv_mpq_2x2_adj_det 统一计算 */

    mpq_t one_minus_a, one_minus_d, neg_b, neg_c;
    mpq_init(one_minus_a);
    mpq_init(one_minus_d);
    mpq_init(neg_b);
    mpq_init(neg_c);

    mpq_set_ui(one_minus_a, 1, 1);
    mpq_sub(one_minus_a, one_minus_a, t->matrix.a);

    mpq_set_ui(one_minus_d, 1, 1);
    mpq_sub(one_minus_d, one_minus_d, t->matrix.d);

    mpq_neg(neg_b, t->matrix.b);
    mpq_neg(neg_c, t->matrix.c);

    /* det = (1-a)*(1-d) - (-b)*(-c) = (1-a)*(1-d) - b*c */
    mpq_t det, adj00, adj01, adj10, adj11;
    mpq_init(det);
    mpq_init(adj00);
    mpq_init(adj01);
    mpq_init(adj10);
    mpq_init(adj11);
    lv_mpq_2x2_adj_det(one_minus_a, neg_b, neg_c, one_minus_d, det, adj00, adj01, adj10, adj11);

    if (mpq_cmp_ui(det, 0, 1) == 0) {
        /* 无唯一不动点 */
        mpq_clear(det);
        mpq_clear(adj00);
        mpq_clear(adj01);
        mpq_clear(adj10);
        mpq_clear(adj11);
        mpq_clear(one_minus_a);
        mpq_clear(one_minus_d);
        mpq_clear(neg_b);
        mpq_clear(neg_c);
        return false;
    }

    /* x = (tx*(1-d) + b*ty) / det = (tx*adj00 + ty*adj01) / det */
    mpq_mul(out_x, t->matrix.tx, adj00);
    mpq_t temp;
    mpq_init(temp);
    mpq_mul(temp, t->matrix.ty, adj01);
    mpq_add(out_x, out_x, temp);
    mpq_div(out_x, out_x, det);

    /* y = ((1-a)*ty + c*tx) / det = (tx*adj10 + ty*adj11) / det */
    mpq_mul(out_y, t->matrix.tx, adj10);
    mpq_mul(temp, t->matrix.ty, adj11);
    mpq_add(out_y, out_y, temp);
    mpq_div(out_y, out_y, det);

    mpq_clear(det);
    mpq_clear(adj00);
    mpq_clear(adj01);
    mpq_clear(adj10);
    mpq_clear(adj11);
    mpq_clear(one_minus_a);
    mpq_clear(one_minus_d);
    mpq_clear(neg_b);
    mpq_clear(neg_c);
    mpq_clear(temp);

    return true;
}

lvTransform *lv_transform_inverse(const lvTransform *t) {
    if (!t || !t->matrix_valid) {
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "lv_transform_inverse: NULL or invalid matrix");
    }

    lvTransform *inv = (lvTransform *) lv_calloc(1, sizeof(lvTransform));
    if (!inv) {
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_transform_inverse: calloc failed");
    }

    inv->type = t->type;
    inv->ref_count = 1;

    mpq_init(inv->matrix.a);
    mpq_init(inv->matrix.b);
    mpq_init(inv->matrix.tx);
    mpq_init(inv->matrix.c);
    mpq_init(inv->matrix.d);
    mpq_init(inv->matrix.ty);

    /* 计算行列式与伴随矩阵（mpq 精确运算，由 lv_mpq_2x2_adj_det 统一计算） */
    mpq_t det, adj00, adj01, adj10, adj11;
    mpq_init(det);
    mpq_init(adj00);
    mpq_init(adj01);
    mpq_init(adj10);
    mpq_init(adj11);
    lv_mpq_2x2_adj_det(t->matrix.a, t->matrix.b, t->matrix.c, t->matrix.d, det, adj00, adj01, adj10, adj11);

    if (mpq_cmp_ui(det, 0, 1) == 0) {
        /* 不可逆 */
        mpq_clear(det);
        mpq_clear(adj00);
        mpq_clear(adj01);
        mpq_clear(adj10);
        mpq_clear(adj11);
        lv_transform_destroy(inv);
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_STATE, "lv_transform_inverse: singular matrix, not invertible");
    }

    /* 逆矩阵 = 伴随矩阵 / det */
    mpq_div(inv->matrix.a, adj00, det);
    mpq_div(inv->matrix.b, adj01, det);
    mpq_div(inv->matrix.c, adj10, det);
    mpq_div(inv->matrix.d, adj11, det);

    /* tx' = -(a'*tx + b'*ty) */
    mpq_t temp;
    mpq_init(temp);
    mpq_mul(inv->matrix.tx, inv->matrix.a, t->matrix.tx);
    mpq_mul(temp, inv->matrix.b, t->matrix.ty);
    mpq_add(inv->matrix.tx, inv->matrix.tx, temp);
    mpq_neg(inv->matrix.tx, inv->matrix.tx);

    /* ty' = -(c'*tx + d'*ty) */
    mpq_mul(inv->matrix.ty, inv->matrix.c, t->matrix.tx);
    mpq_mul(temp, inv->matrix.d, t->matrix.ty);
    mpq_add(inv->matrix.ty, inv->matrix.ty, temp);
    mpq_neg(inv->matrix.ty, inv->matrix.ty);

    mpq_clear(det);
    mpq_clear(adj00);
    mpq_clear(adj01);
    mpq_clear(adj10);
    mpq_clear(adj11);
    mpq_clear(temp);

    inv->matrix_valid = true;
    inv->is_isometry = t->is_isometry;
    inv->is_orientation_preserving = t->is_orientation_preserving;

    return inv;
}

bool lv_transform_equal(const lvTransform *t1, const lvTransform *t2) {
    if (!t1 || !t2) {
        return false;
    }

    if (t1->type != t2->type) {
        return false;
    }

    if (!t1->matrix_valid || !t2->matrix_valid) {
        return false;
    }

    return (mpq_equal(t1->matrix.a, t2->matrix.a) && mpq_equal(t1->matrix.b, t2->matrix.b) &&
            mpq_equal(t1->matrix.c, t2->matrix.c) && mpq_equal(t1->matrix.d, t2->matrix.d) &&
            mpq_equal(t1->matrix.tx, t2->matrix.tx) && mpq_equal(t1->matrix.ty, t2->matrix.ty));
}

/* ============== 特殊变换识别实现 ============== */

bool lv_points_are_symmetric(const mpq_t px, const mpq_t py, const mpq_t qx, const mpq_t qy, const mpq_t ax,
                             const mpq_t ay, const mpq_t bx, const mpq_t by) {
    mpq_t rx, ry;
    mpq_init(rx);
    mpq_init(ry);

    lv_reflect_point(px, py, ax, ay, bx, by, rx, ry);

    bool result = (mpq_equal(rx, qx) && mpq_equal(ry, qy));

    mpq_clear(rx);
    mpq_clear(ry);

    return result;
}

bool lv_reflect_point(const mpq_t px, const mpq_t py, const mpq_t ax, const mpq_t ay, const mpq_t bx, const mpq_t by,
                      mpq_t out_x, mpq_t out_y) {
    /* 计算点关于直线的对称点 */
    /* 使用向量投影公式 */

    mpq_t dx, dy;
    mpq_init(dx);
    mpq_init(dy);

    mpq_sub(dx, bx, ax);
    mpq_sub(dy, by, ay);

    /* 向量 v = P - A */
    mpq_t vx, vy;
    mpq_init(vx);
    mpq_init(vy);

    mpq_sub(vx, px, ax);
    mpq_sub(vy, py, ay);

    /* 投影系数 t = (v · d) / (d · d) */
    mpq_t dot_vd, dot_dd, t;
    mpq_init(dot_vd);
    mpq_init(dot_dd);
    mpq_init(t);

    mpq_mul(dot_vd, vx, dx);
    mpq_mul(t, vy, dy);
    mpq_add(dot_vd, dot_vd, t);

    mpq_mul(dot_dd, dx, dx);
    mpq_mul(t, dy, dy);
    mpq_add(dot_dd, dot_dd, t);

    mpq_div(t, dot_vd, dot_dd);

    /* 投影点 H = A + t * d */
    mpq_t hx, hy;
    mpq_init(hx);
    mpq_init(hy);

    mpq_mul(hx, t, dx);
    mpq_add(hx, hx, ax);
    mpq_mul(hy, t, dy);
    mpq_add(hy, hy, ay);

    /* 对称点 R = 2*H - P */
    mpq_t two;
    mpq_init(two);
    mpq_set_ui(two, 2, 1); /* two = 2/1 = 2 */

    mpq_mul(out_x, hx, two);
    mpq_sub(out_x, out_x, px);

    mpq_mul(out_y, hy, two);
    mpq_sub(out_y, out_y, py);

    mpq_clear(two);

    mpq_clear(dx);
    mpq_clear(dy);
    mpq_clear(vx);
    mpq_clear(vy);
    mpq_clear(dot_vd);
    mpq_clear(dot_dd);
    mpq_clear(t);
    mpq_clear(hx);
    mpq_clear(hy);

    return true;
}

bool lv_rotate_point(const mpq_t px, const mpq_t py, const mpq_t cx, const mpq_t cy, int angle_num, int angle_denom,
                     mpq_t out_x, mpq_t out_y) {
    lvTransform *rot = lv_transform_rotation(cx, cy, angle_num, angle_denom);
    if (!rot) {
        return false;
    }

    mpq_set(out_x, px);
    mpq_set(out_y, py);

    bool result = lv_transform_apply_point(rot, out_x, out_y);

    lv_transform_destroy(rot);

    return result;
}

/* ── 变换类型名称映射表（替代 switch 语句） ── */

/** @brief 变换类型 → 显示名称 / JSON 标识符 */
static const struct {
    const char *display;
    const char *json;
} s_transform_type_names[] = {
    [TRANSFORM_IDENTITY]    = {"Identity",    "identity"},
    [TRANSFORM_TRANSLATION] = {"Translation", "translation"},
    [TRANSFORM_ROTATION]    = {"Rotation",    "rotation"},
    [TRANSFORM_SCALE]       = {"Scale",       "scale"},
    [TRANSFORM_SHEAR]       = {"Shear",       "shear"},
    [TRANSFORM_REFLECTION]  = {"Reflection",  "reflection"},
    [TRANSFORM_SCALING]     = {"Scaling",     "scaling"},
    [TRANSFORM_AFFINE]      = {"Affine",      "affine"},
    [TRANSFORM_PROJECTIVE]  = {"Projective",  "protective"},
    [TRANSFORM_GLUING]      = {"Composite",   "composite"},
    [TRANSFORM_COMPOSITE]   = {"Composite",   "composite"},
};

/* ============== 变换序列化实现 ============== */

char *lv_transform_to_string(const lvTransform *t) {
    if (!t) {
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_transform_to_string: NULL transform");
    }

    const char *type_str = lv_index_in_range(t->type, (int)(sizeof(s_transform_type_names)/sizeof(s_transform_type_names[0])))
                           ? s_transform_type_names[t->type].display : "Unknown";

    /* 用 lvStrBuf 统一构建，避免两遍 snprintf 重复格式串 */
    lvStrBuf sb = {0};
    lv_strbuf_printf(&sb, "%s: matrix=[%Qd %Qd %Qd; %Qd %Qd %Qd]", type_str, t->matrix.a, t->matrix.b,
                     t->matrix.tx, t->matrix.c, t->matrix.d, t->matrix.ty);
    return lv_strbuf_to_string(&sb);
}

char *lv_transform_to_json(const lvTransform *t) {
    if (!t) {
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_transform_to_json: NULL transform");
    }

    const char *type_str = lv_index_in_range(t->type, (int)(sizeof(s_transform_type_names)/sizeof(s_transform_type_names[0])))
                           ? s_transform_type_names[t->type].json : "unknown";

    /* 用 lvStrBuf 统一构建，避免两遍 snprintf 重复格式串 */
    lvStrBuf sb = {0};
    lv_strbuf_printf(&sb,
                     "{\"type\":\"%s\",\"matrix\":{\"a\":\"%Qd\",\"b\":\"%Qd\",\"tx\":\"%Qd\",\"c\":\"%Qd\",\"d\":"
                     "\"%Qd\",\"ty\":\"%Qd\"},"
                     "\"is_isometry\":%s,\"is_orientation_preserving\":%s}",
                     type_str, t->matrix.a, t->matrix.b, t->matrix.tx, t->matrix.c, t->matrix.d, t->matrix.ty,
                     t->is_isometry ? "true" : "false", t->is_orientation_preserving ? "true" : "false");
    return lv_strbuf_to_string(&sb);
}

