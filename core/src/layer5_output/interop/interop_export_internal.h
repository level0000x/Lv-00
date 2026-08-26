/**
 * @file interop_export_internal.h
 * @brief 互操作导出内部共享声明（SVG/TikZ/PDF 导出器共用）
 *
 * 集中定义曾经在三个导出器中各自维护、靠人肉同步的公共数据：
 *   1. 约束类型 → 绘图参数核心表（kConstraintVisuals），覆盖全部 6 种约束类型；
 *   2. 信任颜色 → 各导出器颜色字符串全字段表（kTrustColorEntries）；
 *   3. 公共几何工具函数：compute_bezier_control_points / segment_intersection。
 * 三个导出器只保留"核心表 → 本语法"的窄适配，颜色/线宽以核心表为唯一来源。
 */

#ifndef lv_INTEROP_EXPORT_INTERNAL_H
#define lv_INTEROP_EXPORT_INTERNAL_H

#include <stdio.h>

#include "lv/constraint_graph.h"
#include "lv/lv_strbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 计算约束图的包围盒（SVG 与 PDF 导出共用） */
void compute_bounding_box(const ConstraintGraph *graph, double *min_x, double *min_y, double *max_x,
                          double *max_y);

/* ==================== 约束类型 → 绘图参数核心表 ==================== */

/** @brief 语义线型枚举（各导出器再转换为自己的语法） */
typedef enum {
    CONSTRAINT_LINE_SOLID = 0,      /**< 实线 */
    CONSTRAINT_LINE_DASHED,         /**< 虚线 */
    CONSTRAINT_LINE_DOTTED,         /**< 点线 */
    CONSTRAINT_LINE_DASH_DOTTED     /**< 虚点线 */
} ConstraintLineStyle;

/** @brief 约束可视化参数（跨导出器统一的绘图参数核心条目） */
typedef struct {
    ConstraintType type;            /**< 约束类型 */
    uint8_t rgb[3];                 /**< 权威 RGB 颜色（0-255，唯一颜色来源） */
    ConstraintLineStyle line_style; /**< 语义线型 */
    float line_width;               /**< 权威线宽 */
} ConstraintVisual;

/**
 * @brief 约束类型 → 绘图参数核心表（覆盖全部 6 种约束类型）
 * @note 三个导出器（SVG/TikZ/PDF）均从此表取颜色与线宽，
 *       再经各自的窄适配函数转换为本语法输出，杜绝颜色人肉同步漂移。
 */
static const ConstraintVisual kConstraintVisuals[] = {
    [INCIDENCE]    = { INCIDENCE,    {107, 114, 128}, CONSTRAINT_LINE_DASHED,      1.0f },
    [BETWEENNESS]  = { BETWEENNESS,  { 99, 102, 241}, CONSTRAINT_LINE_DASHED,      1.0f },
    [INTERSECTION] = { INTERSECTION, {168,  85, 247}, CONSTRAINT_LINE_SOLID,       1.0f },
    [CONTAINMENT]  = { CONTAINMENT,  { 20, 184, 166}, CONSTRAINT_LINE_DOTTED,      1.0f },
    [CONNECTION]   = { CONNECTION,   {245, 158,  11}, CONSTRAINT_LINE_SOLID,       1.5f },
    [ANGLE]        = { ANGLE,        {168,  85, 247}, CONSTRAINT_LINE_DASH_DOTTED, 1.0f },
    [PARALLEL]     = { PARALLEL,     { 96, 165, 250}, CONSTRAINT_LINE_DASHED,      1.0f },
};

/**
 * @brief 按约束类型查找核心表条目（O(1) 下标直查；未命中返回 NULL）
 * @note 表按 ConstraintType 连续枚举（INCIDENCE..ANGLE）组织，type 即下标；
 *       仍校验条目 type 字段，防止未来枚举出现空隙时误命中。
 */
static inline const ConstraintVisual *constraint_visual_find(ConstraintType type) {
    if ((unsigned) type < (unsigned) (sizeof(kConstraintVisuals) / sizeof(kConstraintVisuals[0])) &&
        kConstraintVisuals[type].type == type)
        return &kConstraintVisuals[type];
    return NULL;
}

/* ==================== 信任颜色 → 各导出器颜色字符串全字段表 ==================== */

/** @brief 信任颜色映射条目（SVG/TikZ/PDF 三套输出统一维护） */
typedef struct {
    TrustColor trust;      /**< 信任颜色枚举 */
    const char *svg_hex;   /**< SVG 十六进制颜色（如 "#22c55e"） */
    const char *tikz_expr; /**< TikZ/LaTeX 颜色表达式（如 "green!70!black"） */
    const char *pdf_rgba;  /**< PDF RG/rg 颜色三元组（如 "0.13 0.76 0.29"） */
} TrustColorEntry;

/**
 * @brief 信任颜色全字段表（按枚举值升序，10 项）
 * @note SVG/TikZ 输出与历史行为完全一致；PDF 侧修复了 TRUST_AMBER 误输出红色
 *       （0.94 0.27 0.27）的问题，现与 SVG 语义一致输出橙色（0.96 0.62 0.04）。
 *       非绿/非琥珀色在 PDF 侧维持历史灰色（0.61 0.64 0.69），保证输出不变。
 */
static const TrustColorEntry kTrustColorEntries[] = {
    [TRUST_GREEN]               = { TRUST_GREEN,               "#22c55e", "green!70!black",  "0.13 0.76 0.29" },
    [TRUST_BLUE_UNEXPLORED]     = { TRUST_BLUE_UNEXPLORED,     "#3b82f6", "blue!70!black",   "0.61 0.64 0.69" },
    [TRUST_BLUE_EXCEEDED]       = { TRUST_BLUE_EXCEEDED,       "#6366f1", "blue!50!black",   "0.61 0.64 0.69" },
    [TRUST_BLUE_OUT_OF_SCOPE]   = { TRUST_BLUE_OUT_OF_SCOPE,   "#93c5fd", "blue!30!black",   "0.61 0.64 0.69" },
    [TRUST_YELLOW]              = { TRUST_YELLOW,              "#eab308", "yellow!70!black", "0.61 0.64 0.69" },
    [TRUST_LIGHT_ORANGE_ORACLE] = { TRUST_LIGHT_ORANGE_ORACLE, "#fb923c", "orange!40!black", "0.61 0.64 0.69" },
    [TRUST_LIGHT_ORANGE_EXPLOSION] = { TRUST_LIGHT_ORANGE_EXPLOSION, "#f97316", "orange!60!black", "0.61 0.64 0.69" },
    [TRUST_AMBER]               = { TRUST_AMBER,               "#f59e0b", "orange!80!black", "0.96 0.62 0.04" },
    [TRUST_DEEP_ORANGE]         = { TRUST_DEEP_ORANGE,         "#ea580c", "red!70!black",    "0.61 0.64 0.69" },
    [TRUST_RED]                 = { TRUST_RED,                 "#ef4444", "red!80!black",    "0.61 0.64 0.69" },
};

/** @brief 按信任颜色查找全字段表条目（未命中返回 NULL） */
static inline const TrustColorEntry *interop_trust_color_find(TrustColor trust) {
    for (size_t i = 0; i < sizeof(kTrustColorEntries) / sizeof(kTrustColorEntries[0]); i++) {
        if (kTrustColorEntries[i].trust == trust)
            return &kTrustColorEntries[i];
    }
    return NULL;
}

/* ==================== 公共几何工具函数 ==================== */

/**
 * @brief 计算贝塞尔曲线的两个控制点（SVG cubic 与 PDF c 操作符共用）
 * @details 控制点公式：
 *          CP1 = P1 + 0.3*(P2-P1) + 垂直法向偏移
 *          CP2 = P2 - 0.3*(P2-P1) + 垂直法向偏移
 *          其中垂直偏移 offset = 0.15*|P2-P1|（过短时固定为 5.0），
 *          法向 n = (-dy, dx) / (|P2-P1| + 0.001)。
 * @param p1x,p1y 起点坐标
 * @param p2x,p2y 终点坐标
 * @param cp1x,cp1y [out] 第一控制点
 * @param cp2x,cp2y [out] 第二控制点
 */
void compute_bezier_control_points(double p1x, double p1y, double p2x, double p2y,
                                   double *cp1x, double *cp1y, double *cp2x, double *cp2y);

/**
 * @brief 计算两条线段的交点（SVG 相交约束渲染共用）
 * @details 解线段参数方程：P1 + t*(P2-P1) = Q1 + s*(Q2-Q1)。
 *          平行/共线（|cross|<=1e-10）或交点超出 t∈[-0.05, 1.05] 范围时返回 false，
 *          此时不写 ix/iy。
 * @return true 交点有效；false 无有效交点
 */
bool segment_intersection(double p1x, double p1y, double p2x, double p2y,
                          double q1x, double q1y, double q2x, double q2y,
                          double *ix, double *iy);

/* ==================== 约束渲染分发表（SVG/TikZ/PDF 共用） ==================== */

/**
 * @brief 约束渲染上下文
 *
 * 承载约束、前两个参与者坐标与输出介质。三个导出器按需取用介质：
 * SVG 使用 fp（FILE*），TikZ/PDF 使用 sb（lvStrBuf*）；
 * pdf_margin/pdf_ox/pdf_oy 为 PDF 图形空间 → 页面空间变换参数（仅 PDF 设置）。
 */
typedef struct {
    const ConstraintGraph *graph; /**< 约束图（参与者节点解析用） */
    const Constraint *c;          /**< 约束（非 NULL） */
    const GeomNode *p0;           /**< 第一参与者节点（坐标已解析） */
    const GeomNode *p1;           /**< 第二参与者节点（坐标已解析） */
    double x0, y0;                /**< 第一参与者坐标 */
    double x1, y1;                /**< 第二参与者坐标 */
    FILE *fp;                     /**< SVG 导出输出流（SVG 渲染函数使用） */
    lvStrBuf *sb;                 /**< TikZ/PDF 导出输出缓冲（TikZ/PDF 渲染函数使用） */
    double pdf_margin;            /**< PDF 页边距（PDF 专用；tx = margin + (x - min_x)） */
    double pdf_ox;                /**< PDF 图形空间原点 min_x（PDF 专用） */
    double pdf_oy;                /**< PDF 图形空间原点 min_y（PDF 专用） */
} ConstraintRenderCtx;

/**
 * @brief 约束渲染操作分发表（每导出器注册一个实例）
 *
 * BETWEENNESS/INTERSECTION 为特判渲染，其余类型走 default（由核心视觉表
 * kConstraintVisuals 驱动本语法窄适配）。三个导出器的约束渲染循环不再各自
 * 维护 switch，统一经 constraint_render_dispatch 分发。
 */
typedef struct {
    bool (*render_betweenness)(const ConstraintRenderCtx *ctx); /**< BETWEENNESS 特判渲染 */
    bool (*render_intersection)(const ConstraintRenderCtx *ctx); /**< INTERSECTION 特判渲染 */
    bool (*render_default)(const ConstraintRenderCtx *ctx);      /**< 核心表驱动的默认渲染 */
} ConstraintRenderOps;

/** @brief 按约束类型分发约束渲染（BETWEENNESS/INTERSECTION 特判 + default 核心表驱动） */
static inline bool constraint_render_dispatch(const ConstraintRenderOps *ops, const ConstraintRenderCtx *ctx,
                                              ConstraintType type) {
    if (!ops || !ctx)
        return false;
    switch (type) {
        case BETWEENNESS:
            return ops->render_betweenness(ctx);
        case INTERSECTION:
            return ops->render_intersection(ctx);
        default:
            return ops->render_default(ctx);
    }
}

/**
 * @brief 解析节点代表坐标（约束渲染用）
 * @details GEOM_POINT / GEOM_LINE_SEGMENT / GEOM_PORT / GEOM_FUNCTION_BLOCK
 *          取前两个符号坐标；GEOM_CIRCLE 无自身坐标（coord_count=0），
 *          经 data.circle.center_node_id 解析圆心作为代表坐标（与
 *          meta_proof.c / interop_export_geojson.c 的圆解析语义一致）。
 * @return true 已写出代表坐标；false 节点无效/坐标不足
 */
static inline bool constraint_render_node_coords(const ConstraintGraph *graph, const GeomNode *node,
                                                 double *x, double *y) {
    if (!node)
        return false;
    if (node->type == GEOM_CIRCLE) {
        const GeomNode *center = graph_get_node(graph, node->data.circle.center_node_id);
        if (!center || center->coord_count < 2 || !center->symbolic_coords ||
            !center->symbolic_coords[0] || !center->symbolic_coords[1])
            return false;
        *x = symbolic_coord_to_double(center->symbolic_coords[0]);
        *y = symbolic_coord_to_double(center->symbolic_coords[1]);
        return true;
    }
    if (node->coord_count < 2 || !node->symbolic_coords || !node->symbolic_coords[0] ||
        !node->symbolic_coords[1])
        return false;
    *x = symbolic_coord_to_double(node->symbolic_coords[0]);
    *y = symbolic_coord_to_double(node->symbolic_coords[1]);
    return true;
}

/**
 * @brief 准备约束渲染：解析前两个参与者节点与代表坐标（圆→圆心）
 * @return 参与者有效（均已解析出代表坐标）返回 true；节点缺失/坐标不足返回 false
 */
static inline bool constraint_render_prepare(const ConstraintGraph *graph, const Constraint *c,
                                             const GeomNode **p0, const GeomNode **p1,
                                             double *x0, double *y0, double *x1, double *y1) {
    *p0 = graph_get_node(graph, c->participants[0]);
    *p1 = graph_get_node(graph, c->participants[1]);
    if (!*p0 || !*p1)
        return false;
    if (!constraint_render_node_coords(graph, *p0, x0, y0))
        return false;
    if (!constraint_render_node_coords(graph, *p1, x1, y1))
        return false;
    return true;
}

/**
 * @brief 计算两参与者几何对象的交点（INTERSECTION 特判渲染共用）
 * @details 按参与者类型组合求解精确交点：
 *          - 两线段：segment_intersection 直线参数方程解；
 *          - 线段+圆（任一顺序）：线段参数方程代入圆方程解二次方程，取
 *            t∈[0,1] 的首个有效根；
 *          - 两圆：标准两圆方程解（相交时取 +h 分支交点）；
 *          - 其余组合/无有效交点：回退默认点 (dflt_x, dflt_y)。
 *          圆的圆心/半径经 data.circle.center_node_id / radius_node_id
 *          解析（graph 参数），与 geojson/meta_proof 圆解析语义一致。
 * @param graph 约束图（圆节点解析圆心/半径用）
 * @param p0,p1 参与者节点
 * @param dflt_x,dflt_y 无法求解时的回退交点
 * @param ix,iy [out] 交点坐标
 * @note 实现位于 interop_export_svg.c（当前唯一调用方）。
 */
void constraint_intersection_point(const ConstraintGraph *graph, const GeomNode *p0, const GeomNode *p1,
                                   double dflt_x, double dflt_y, double *ix, double *iy);

#ifdef __cplusplus
}
#endif

#endif /* lv_INTEROP_EXPORT_INTERNAL_H */
