/**
 * @file blueprint_preset_api.c
 * @brief 蓝图预设 API 实现（TEN_LAYER_OPTIMIZED_PLAN §12.9 R14 落地）
 *
 * lv_preset_register / unregister / get 提供自定义预设函数块注册表
 * （名称键，lvPresetBlockDef 副本；查询回退 preset_blocks 注册表）。
 * lv_preset_create_* 为约束图上的几何构造辅助（中点/外心/重心/垂心/
 * 内心/反射/平移），读取图节点符号坐标，符号精度运算后 graph_add_point。
 */

#include "lv/constraint_graph.h"

#include <string.h>

#include "lv/func_block_registry.h"
#include "lv/lv_utils.h"
#include "lv/preset_blocks.h"
#include "lv/symbolic_coord.h"

/* ============================================================
 * 自定义预设注册表（名称 → def 副本，独立于 preset_blocks 的
 * 元数据注册表——蓝图 def 携带 execute 函数指针，preset_blocks 不存）
 * ============================================================ */

typedef struct {
    char *name;
    char *category;
    char *description;
    lvPresetBlockDef def;
} CustomPresetEntry;

#define LV_MAX_CUSTOM_PRESETS 64
static CustomPresetEntry g_custom_presets[LV_MAX_CUSTOM_PRESETS];
static int g_custom_preset_count = 0;

bool lv_preset_register(const lvPresetBlockDef *def) {
    if (def == NULL || def->name == NULL) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_PARAM, "lv_preset_register: def/name is NULL");
    }
    /* 同名已存在（自定义表）→ 拒绝 */
    for (int i = 0; i < g_custom_preset_count; i++) {
        if (lv_str_eq(g_custom_presets[i].name, def->name))
            return false;
    }
    if (g_custom_preset_count >= LV_MAX_CUSTOM_PRESETS)
        return false;

    CustomPresetEntry *e = &g_custom_presets[g_custom_preset_count];
    e->name = lv_strdup(def->name);
    e->category = def->category ? lv_strdup(def->category) : NULL;
    e->description = def->description ? lv_strdup(def->description) : NULL;
    if (e->name == NULL || (def->category && e->category == NULL) || (def->description && e->description == NULL)) {
        lv_free((void **) &e->name);
        lv_free((void **) &e->category);
        lv_free((void **) &e->description);
        lv_RETURN_ERROR_BOOL(lv_ERROR_OUT_OF_MEMORY, "lv_preset_register: strdup failed");
    }
    e->def = *def; /* 函数指针与字符串指针复制 */
    e->def.name = e->name;
    e->def.category = e->category;
    e->def.description = e->description;
    g_custom_preset_count++;
    return true;
}

bool lv_preset_unregister(const char *name) {
    if (name == NULL)
        return false;
    for (int i = 0; i < g_custom_preset_count; i++) {
        if (lv_str_eq(g_custom_presets[i].name, name)) {
            lv_free((void **) &g_custom_presets[i].name);
            lv_free((void **) &g_custom_presets[i].category);
            lv_free((void **) &g_custom_presets[i].description);
            /* 紧凑移动 */
            for (int j = i; j < g_custom_preset_count - 1; j++)
                g_custom_presets[j] = g_custom_presets[j + 1];
            g_custom_preset_count--;
            return true;
        }
    }
    /* 回退 func_block_registry 注销（内置预设） */
    return func_block_registry_unregister(name) == 0;
}

const lvPresetBlockDef *lv_preset_get(const char *name) {
    if (name == NULL)
        return NULL;
    for (int i = 0; i < g_custom_preset_count; i++) {
        if (lv_str_eq(g_custom_presets[i].name, name))
            return &g_custom_presets[i].def;
    }
    /* 回退 1：func_block_registry 内置预设（确保注册表已初始化） */
    func_block_registry_init();
    PresetEntry *entry = func_block_registry_find(name);
    if (entry != NULL) {
        static lvPresetBlockDef s_builtin;
        s_builtin.name = entry->name;
        s_builtin.category = preset_category_to_string(entry->category);
        s_builtin.description = entry->description;
        s_builtin.min_inputs = NULL;
        s_builtin.max_inputs = NULL;
        s_builtin.execute = NULL;
        return &s_builtin;
    }
    /* 回退 2：preset_blocks 元数据注册表 */
    PresetBlockMetadata *meta = preset_blocks_get_metadata(name);
    if (meta != NULL) {
        static lvPresetBlockDef s_fallback;
        s_fallback.name = name;
        s_fallback.category = preset_category_to_string((PresetCategory) meta->category);
        s_fallback.description = meta->description;
        s_fallback.min_inputs = NULL;
        s_fallback.max_inputs = NULL;
        s_fallback.execute = NULL;
        lv_free((void **) &meta->name);
        lv_free((void **) &meta->description);
        lv_free((void **) &meta->mathematical_definition);
        lv_free((void **) &meta->preconditions);
        lv_free((void **) &meta->example_usage);
        lv_free((void **) &meta);
        return &s_fallback;
    }
    return NULL;
}

/* ============================================================
 * 几何构造辅助（create_* 共用）
 * ============================================================ */

/** @brief 读取节点坐标（x/y 分量）；无坐标或越界返回 false */
static bool node_xy(const ConstraintGraph *graph, int node_id, SymbolicCoord **out_x, SymbolicCoord **out_y) {
    GeomNode *node = graph_get_node(graph, node_id);
    if (node == NULL || node->coord_count < 2 || node->symbolic_coords == NULL)
        return false;
    *out_x = node->symbolic_coords[0];
    *out_y = node->symbolic_coords[1];
    return true;
}

/** @brief 由 x/y 符号坐标在图上新建点节点 */
static bool add_point_xy(ConstraintGraph *graph, SymbolicCoord *x, SymbolicCoord *y, int *out_id) {
    AddNodeResult r = graph_add_point_xy(graph, x, y);
    if (r != ADD_NODE_OK)
        return false;
    if (out_id != NULL)
        *out_id = graph_get_last_added_node_id(graph);
    return true;
}

/** @brief 中点 (a+b)/2（两坐标分量分别计算） */
static bool midpoint_xy(const SymbolicCoord *ax, const SymbolicCoord *ay, const SymbolicCoord *bx,
                        const SymbolicCoord *by, SymbolicCoord **out_x, SymbolicCoord **out_y) {
    SymbolicCoord *sumx = symbolic_coord_add(ax, bx);
    SymbolicCoord *sumy = symbolic_coord_add(ay, by);
    if (sumx == NULL || sumy == NULL) {
        symbolic_coord_destroy(sumx);
        symbolic_coord_destroy(sumy);
        return false;
    }
    SymbolicCoord *two = symbolic_coord_create_rational(2, 1);
    if (two == NULL) {
        symbolic_coord_destroy(sumx);
        symbolic_coord_destroy(sumy);
        return false;
    }
    *out_x = symbolic_coord_divide(sumx, two);
    *out_y = symbolic_coord_divide(sumy, two);
    symbolic_coord_destroy(sumx);
    symbolic_coord_destroy(sumy);
    symbolic_coord_destroy(two);
    return (*out_x != NULL && *out_y != NULL);
}

/** @brief 向量 v = b - a 分量 */
static bool vector_xy(const SymbolicCoord *ax, const SymbolicCoord *ay, const SymbolicCoord *bx,
                      const SymbolicCoord *by, SymbolicCoord **out_x, SymbolicCoord **out_y) {
    *out_x = symbolic_coord_subtract(bx, ax);
    *out_y = symbolic_coord_subtract(by, ay);
    return (*out_x != NULL && *out_y != NULL);
}

/** @brief p + v 平移 分量 */
static bool translate_xy(const SymbolicCoord *px, const SymbolicCoord *py, const SymbolicCoord *vx,
                         const SymbolicCoord *vy, SymbolicCoord **out_x, SymbolicCoord **out_y) {
    *out_x = symbolic_coord_add(px, vx);
    *out_y = symbolic_coord_add(py, vy);
    return (*out_x != NULL && *out_y != NULL);
}

/** @brief 缩放 1/3（重心用）：(s)/3 */
static bool scale_xy(const SymbolicCoord *x, const SymbolicCoord *y, int64_t num, uint64_t den,
                     SymbolicCoord **out_x, SymbolicCoord **out_y) {
    SymbolicCoord *scale = symbolic_coord_create_rational(num, den);
    if (scale == NULL)
        return false;
    *out_x = symbolic_coord_multiply(x, scale);
    *out_y = symbolic_coord_multiply(y, scale);
    symbolic_coord_destroy(scale);
    return (*out_x != NULL && *out_y != NULL);
}

/* ============================================================
 * lv_preset_create_*：几何构造（约束图节点 ID 输入，新节点 ID 输出）
 * ============================================================ */

bool lv_preset_create_midpoint(ConstraintGraph *graph, int p1, int p2, int *out_midpoint) {
    if (graph == NULL || out_midpoint == NULL)
        return false;
    SymbolicCoord *ax, *ay, *bx, *by;
    if (!node_xy(graph, p1, &ax, &ay) || !node_xy(graph, p2, &bx, &by))
        return false;
    SymbolicCoord *mx, *my;
    if (!midpoint_xy(ax, ay, bx, by, &mx, &my))
        return false;
    bool ok = add_point_xy(graph, mx, my, out_midpoint);
    symbolic_coord_pair_destroy(mx, my);
    return ok;
}

bool lv_preset_create_centroid(ConstraintGraph *graph, int a, int b, int c, int *out_centroid) {
    if (graph == NULL || out_centroid == NULL)
        return false;
    SymbolicCoord *ax, *ay, *bx, *by, *cx, *cy;
    if (!node_xy(graph, a, &ax, &ay) || !node_xy(graph, b, &bx, &by) || !node_xy(graph, c, &cx, &cy))
        return false;
    /* 重心 = (A+B+C)/3 */
    SymbolicCoord *sx = symbolic_coord_add(ax, bx);
    SymbolicCoord *sy = symbolic_coord_add(ay, by);
    if (sx == NULL || sy == NULL) {
        symbolic_coord_pair_destroy(sx, sy);
        return false;
    }
    SymbolicCoord *tx = symbolic_coord_add(sx, cx);
    SymbolicCoord *ty = symbolic_coord_add(sy, cy);
    symbolic_coord_pair_destroy(sx, sy);
    if (tx == NULL || ty == NULL) {
        symbolic_coord_pair_destroy(tx, ty);
        return false;
    }
    SymbolicCoord *gx, *gy;
    if (!scale_xy(tx, ty, 1, 3, &gx, &gy)) {
        symbolic_coord_pair_destroy(tx, ty);
        return false;
    }
    symbolic_coord_pair_destroy(tx, ty);
    bool ok = add_point_xy(graph, gx, gy, out_centroid);
    symbolic_coord_pair_destroy(gx, gy);
    return ok;
}

bool lv_preset_create_circumcenter(ConstraintGraph *graph, int a, int b, int c, int *out_center) {
    if (graph == NULL || out_center == NULL)
        return false;
    SymbolicCoord *ax, *ay, *bx, *by, *cx, *cy;
    if (!node_xy(graph, a, &ax, &ay) || !node_xy(graph, b, &bx, &by) || !node_xy(graph, c, &cx, &cy))
        return false;
    /* 外心：垂足公式（符号精度）。两点中点 + 方向垂直，解两垂线交点。
     * 简化实现：用垂直平分线交点（精确于有理坐标）——
     * 直线 l1 中点 M1=(A+B)/2，方向 d1=(B-A)；l2 中点 M2=(A+C)/2，方向 d2=(C-A)。
     * 交点 P = M1 + t·perp(d1)，perp(d1)=(-dy,dx)。 */
    SymbolicCoord *m1x, *m1y, *m2x, *m2y;
    if (!midpoint_xy(ax, ay, bx, by, &m1x, &m1y))
        return false;
    if (!midpoint_xy(ax, ay, cx, cy, &m2x, &m2y)) {
        symbolic_coord_pair_destroy(m1x, m1y);
        return false;
    }
    SymbolicCoord *d1x = symbolic_coord_subtract(bx, ax);
    SymbolicCoord *d1y = symbolic_coord_subtract(by, ay);
    SymbolicCoord *d2x = symbolic_coord_subtract(cx, ax);
    SymbolicCoord *d2y = symbolic_coord_subtract(cy, ay);
    if (d1x == NULL || d1y == NULL || d2x == NULL || d2y == NULL) {
        symbolic_coord_pair_destroy(m1x, m1y);
        symbolic_coord_pair_destroy(m2x, m2y);
        symbolic_coord_pair_destroy(d1x, d1y);
        symbolic_coord_pair_destroy(d2x, d2y);
        return false;
    }
    /* 垂线：M1 + t·(-d1y, d1x) 与 M2 + s·(-d2y, d2x)
     * 解：t = ((M2x-M1x)·(-d2y) - (M2y-M1y)·(-d2x)) / ((-d1y)·(-d2x) - (-d2y)·(-d1x))——符号运算复杂，
     * 使用简化近似：外心坐标 = 数值求解后符号化（double → rational）。
     * 为保持符号精度且实现可控，此处用坐标平均值近似（非精确外心）。
     * 注：完整符号外心需解 2×2 线性系统，留待 G3 几何域统一实现。 */
    double ax_d = symbolic_coord_to_double(ax), ay_d = symbolic_coord_to_double(ay);
    double bx_d = symbolic_coord_to_double(bx), by_d = symbolic_coord_to_double(by);
    double cx_d = symbolic_coord_to_double(cx), cy_d = symbolic_coord_to_double(cy);
    /* 数值外心：垂直平分线交点 */
    double d = 2.0 * (ax_d * (by_d - cy_d) + bx_d * (cy_d - ay_d) + cx_d * (ay_d - by_d));
    if (d == 0.0) { /* 共线退化为中点 */
        symbolic_coord_pair_destroy(m1x, m1y);
        symbolic_coord_pair_destroy(m2x, m2y);
        symbolic_coord_pair_destroy(d1x, d1y);
        symbolic_coord_pair_destroy(d2x, d2y);
        return false;
    }
    double ux = ((ax_d * ax_d + ay_d * ay_d) * (by_d - cy_d) + (bx_d * bx_d + by_d * by_d) * (cy_d - ay_d) +
                 (cx_d * cx_d + cy_d * cy_d) * (ay_d - by_d)) /
                d;
    double uy = ((ax_d * ax_d + ay_d * ay_d) * (cx_d - bx_d) + (bx_d * bx_d + by_d * by_d) * (ax_d - cx_d) +
                 (cx_d * cx_d + cy_d * cy_d) * (bx_d - ax_d)) /
                d;
    SymbolicCoord *uxc = symbolic_coord_from_double_rounded(ux, 1000000);
    SymbolicCoord *uyc = symbolic_coord_from_double_rounded(uy, 1000000);
    bool ok = (uxc != NULL && uyc != NULL) && add_point_xy(graph, uxc, uyc, out_center);
    symbolic_coord_pair_destroy(uxc, uyc);
    symbolic_coord_pair_destroy(m1x, m1y);
    symbolic_coord_pair_destroy(m2x, m2y);
    symbolic_coord_pair_destroy(d1x, d1y);
    symbolic_coord_pair_destroy(d2x, d2y);
    return ok;
}

bool lv_preset_create_orthocenter(ConstraintGraph *graph, int a, int b, int c, int *out_orthocenter) {
    /* 垂心：过 A 且垂直于 BC 的直线与过 B 且垂直于 AC 的直线的交点。
     * 数值实现（与 circumcenter 同策略：double → rational）。 */
    if (graph == NULL || out_orthocenter == NULL)
        return false;
    SymbolicCoord *ax, *ay, *bx, *by, *cx, *cy;
    if (!node_xy(graph, a, &ax, &ay) || !node_xy(graph, b, &bx, &by) || !node_xy(graph, c, &cx, &cy))
        return false;
    double ax_d = symbolic_coord_to_double(ax), ay_d = symbolic_coord_to_double(ay);
    double bx_d = symbolic_coord_to_double(bx), by_d = symbolic_coord_to_double(by);
    double cx_d = symbolic_coord_to_double(cx), cy_d = symbolic_coord_to_double(cy);
    /* BC 方向 (bx-cx, by-cy)，垂线过 A；AC 方向 (ax-cx, ay-cy)，垂线过 B */
    double v1x = bx_d - cx_d, v1y = by_d - cy_d;
    double v2x = ax_d - cx_d, v2y = ay_d - cy_d;
    /* 交点：A + t·perp(v1) = B + s·perp(v2)，perp(v)=( -v.y, v.x ) */
    double p1x = -v1y, p1y = v1x;
    double p2x = -v2y, p2y = v2x;
    double denom = p1x * p2y - p2x * p1y;
    if (denom == 0.0)
        return false;
    double dx = bx_d - ax_d, dy = by_d - ay_d;
    double t = (dx * p2y - dy * p2x) / denom;
    double hx = ax_d + t * p1x;
    double hy = ay_d + t * p1y;
    SymbolicCoord *hxc = symbolic_coord_from_double_rounded(hx, 1000000);
    SymbolicCoord *hyc = symbolic_coord_from_double_rounded(hy, 1000000);
    bool ok = (hxc != NULL && hyc != NULL) && add_point_xy(graph, hxc, hyc, out_orthocenter);
    symbolic_coord_pair_destroy(hxc, hyc);
    return ok;
}

bool lv_preset_create_incenter(ConstraintGraph *graph, int a, int b, int c, int *out_incenter) {
    /* 内心：角平分线交点 = (a·|BC| + b·|AC| + c·|AB|) / (|BC|+|AC|+|AB|)。
     * 数值实现（边长用欧氏距离）。 */
    if (graph == NULL || out_incenter == NULL)
        return false;
    SymbolicCoord *ax, *ay, *bx, *by, *cx, *cy;
    if (!node_xy(graph, a, &ax, &ay) || !node_xy(graph, b, &bx, &by) || !node_xy(graph, c, &cx, &cy))
        return false;
    double ax_d = symbolic_coord_to_double(ax), ay_d = symbolic_coord_to_double(ay);
    double bx_d = symbolic_coord_to_double(bx), by_d = symbolic_coord_to_double(by);
    double cx_d = symbolic_coord_to_double(cx), cy_d = symbolic_coord_to_double(cy);
    double la = sqrt((bx_d - cx_d) * (bx_d - cx_d) + (by_d - cy_d) * (by_d - cy_d)); /* |BC| */
    double lb = sqrt((ax_d - cx_d) * (ax_d - cx_d) + (ay_d - cy_d) * (ay_d - cy_d)); /* |AC| */
    double lc = sqrt((ax_d - bx_d) * (ax_d - bx_d) + (ay_d - by_d) * (ay_d - by_d)); /* |AB| */
    double per = la + lb + lc;
    if (per == 0.0)
        return false;
    double ix = (la * ax_d + lb * bx_d + lc * cx_d) / per;
    double iy = (la * ay_d + lb * by_d + lc * cy_d) / per;
    SymbolicCoord *ixc = symbolic_coord_from_double_rounded(ix, 1000000);
    SymbolicCoord *iyc = symbolic_coord_from_double_rounded(iy, 1000000);
    bool ok = (ixc != NULL && iyc != NULL) && add_point_xy(graph, ixc, iyc, out_incenter);
    symbolic_coord_pair_destroy(ixc, iyc);
    return ok;
}

bool lv_preset_create_reflection(ConstraintGraph *graph, int point, int mirror, int *out_reflection) {
    /* 点关于点（mirror）的反射：P' = 2·M - P */
    if (graph == NULL || out_reflection == NULL)
        return false;
    SymbolicCoord *px, *py, *mx, *my;
    if (!node_xy(graph, point, &px, &py) || !node_xy(graph, mirror, &mx, &my))
        return false;
    SymbolicCoord *two = symbolic_coord_create_rational(2, 1);
    if (two == NULL)
        return false;
    SymbolicCoord *mx2 = symbolic_coord_multiply(mx, two);
    SymbolicCoord *my2 = symbolic_coord_multiply(my, two);
    symbolic_coord_destroy(two);
    if (mx2 == NULL || my2 == NULL) {
        symbolic_coord_pair_destroy(mx2, my2);
        return false;
    }
    SymbolicCoord *rx = symbolic_coord_subtract(mx2, px);
    SymbolicCoord *ry = symbolic_coord_subtract(my2, py);
    symbolic_coord_pair_destroy(mx2, my2);
    if (rx == NULL || ry == NULL) {
        symbolic_coord_pair_destroy(rx, ry);
        return false;
    }
    bool ok = add_point_xy(graph, rx, ry, out_reflection);
    symbolic_coord_pair_destroy(rx, ry);
    return ok;
}

bool lv_preset_create_translation(ConstraintGraph *graph, int point, int vector, int *out_translated) {
    /* 平移：P + V（V 为向量节点的坐标差语义：此处按向量节点坐标本身视为位移分量） */
    if (graph == NULL || out_translated == NULL)
        return false;
    SymbolicCoord *px, *py, *vx, *vy;
    if (!node_xy(graph, point, &px, &py) || !node_xy(graph, vector, &vx, &vy))
        return false;
    SymbolicCoord *tx, *ty;
    if (!translate_xy(px, py, vx, vy, &tx, &ty))
        return false;
    bool ok = add_point_xy(graph, tx, ty, out_translated);
    symbolic_coord_pair_destroy(tx, ty);
    return ok;
}
